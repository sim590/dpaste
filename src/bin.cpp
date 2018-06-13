/*
 * Copyright © 2017 Simon Désaulniers
 * Author: Simon Désaulniers <sim.desaulniers@gmail.com>
 *
 * This file is part of dpaste.
 *
 * dpaste is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dpaste is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with dpaste.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sstream>
#include <iostream>
#include <array>

#include <msgpack.hpp>

#include "bin.h"
#include "conf.h"
#include "log.h"

namespace dpaste {

const constexpr uint8_t Bin::PROTO_VERSION;

Bin::Bin() {
    /* load dpaste config */
    auto config_file = conf::ConfigurationFile();
    config_file.load();
    conf = config_file.getConfiguration();

    long port;
    {
        std::istringstream conv(conf.at("port"));
        conv >> port;
    }

    node.run();
    http_client_ = std::make_unique<HttpClient>(conf.at("host"), port);

    keyid_ = conf.at("pgp_key_id");
    gpg = std::make_unique<GPGCrypto>(keyid_);
}

void comment_on_signature(const GpgME::Signature& sig) {
    const auto& s = sig.summary();
    if (s & GpgME::Signature::Valid)
        DPASTE_MSG("Valid signature from key with ID %s", sig.fingerprint());
}

std::string Bin::code_from_dpaste_uri(const std::string& uri) {
    static const std::string DUP {DPASTE_URI_PREFIX};
    const auto p = uri.find(DUP);
    return uri.substr(p != std::string::npos ? p+DUP.length() : 0);
}

std::pair<bool, std::string> Bin::get(std::string&& code, bool no_decrypt) {
    code = code_from_dpaste_uri(code);

    /* first try http server */
    auto data_str = http_client_->get(code);
    std::vector<uint8_t> data {data_str.begin(), data_str.end()};

    /* if fail, then perform request from local node */
    if (data.empty()) {
        /* get a pasted blob */
        auto values = node.get(code);
        if (not values.empty())
            data = values.front();
        node.stop();
    }

    if (not data.empty()) {
        Packet p;
        try {
            p.deserialize(data);
            if (gpg->isGPGencrypted(p.data)) {
                DPASTE_MSG("Data is GPG encrypted.");
                if (not no_decrypt) {
                    DPASTE_MSG("Decrypting...");
                    auto res = gpg->decryptAndVerify(p.data);
                    DPASTE_MSG("Success!");

                    data = std::move(std::get<0>(res));
                    auto& verif_res = std::get<2>(res);
                    if (verif_res.numSignatures() > 0)
                        comment_on_signature(verif_res.signature(0));
                } else
                    data = std::move(p.data);
            } else {
                data = std::move(p.data);
                if (not p.signature.empty()) {
                    DPASTE_MSG("Data is GPG signed. Verifying...");
                    auto res = gpg->verify(p.signature, data);
                    if (res.numSignatures() > 0)
                        comment_on_signature(res.signature(0));
                }
            }
        } catch (const GpgME::Exception& e) {
            DPASTE_MSG("%s", e.what());
            return {false, ""};
        } catch (msgpack::type_error& e) { } /* backward compatibility with <=0.3.3 */

    }
    return {true, {data.begin(), data.end()}};
}

std::vector<uint8_t> Bin::data_from_stream(std::stringstream&& input_stream) {
    std::vector<uint8_t> buffer;
    buffer.resize(dht::MAX_VALUE_SIZE);
    input_stream.read(reinterpret_cast<char*>(buffer.data()), dht::MAX_VALUE_SIZE);
    buffer.resize(input_stream.gcount());
    return buffer;
}

std::string Bin::paste(std::vector<uint8_t>&& data,
        std::string&& recipient,
        bool sign,
        bool self_recipient) const
{
    /* we include self as recipient if there's at least one other recipient */
    std::vector<std::string> recipients {};
    if (not recipient.empty()) {
        recipients.emplace_back(std::move(recipient));
        if (self_recipient and not keyid_.empty())
            recipients.emplace_back(keyid_);
    }

    /* paste a blob on the DHT */
    std::uniform_int_distribution<uint32_t> codeDist_;
    std::mt19937_64 rand_;
    dht::crypto::random_device rdev;
    std::seed_seq seed {rdev(), rdev()};
    rand_.seed(seed);

    auto code_n = codeDist_(rand_);
    std::stringstream ss;
    ss << std::hex << code_n;
    auto code = ss.str();
    std::transform(code.begin(), code.end(), code.begin(), ::toupper);

    Packet p;
    auto to_sign = sign and not keyid_.empty();
    if (not recipients.empty()) {
        DPASTE_MSG("Encrypting %sdata...", to_sign ? "and signing " : "");
        auto res = gpg->encrypt(recipients, data, to_sign);
        p.data = std::get<0>(res);
    } else {
        if (to_sign) {
            DPASTE_MSG("Signing data...");
            auto res = gpg->sign(p.data);
            p.signature = res.first;
        }
        p.data.insert(p.data.end(), data.begin(), data.end());
    }

    DPASTE_MSG("Pasting data...");
    auto bin_packet = p.serialize();
    auto success = http_client_->put(code, {bin_packet.begin(), bin_packet.end()});
    if (not success) {
        Node node;
        node.run();

        success = node.paste(code, std::move(bin_packet));
        node.stop();
    }

    return success ? DPASTE_URI_PREFIX+code : "";
}

msgpack::object*
findMapValue(msgpack::object& map, const std::string& key) {
    if (map.type != msgpack::type::MAP) throw msgpack::type_error();
    for (unsigned i = 0; i < map.via.map.size; i++) {
        auto& o = map.via.map.ptr[i];
        if (o.key.type == msgpack::type::STR && o.key.as<std::string>() == key)
            return &o.val;
    }
    return nullptr;
}

std::vector<uint8_t> Bin::Packet::serialize() const {
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);

    pk.pack_map(3);
    pk.pack("v");    pk.pack(PROTO_VERSION);
    pk.pack("data"); pk.pack(data);
    pk.pack("signature"); pk.pack(signature);
    return {buffer.data(), buffer.data()+buffer.size()};
}

void Bin::Packet::deserialize(const std::vector<uint8_t>& pbuffer) {
    msgpack::unpacked unpacked = msgpack::unpack(reinterpret_cast<const char*>(pbuffer.data()), pbuffer.size());
    auto msgpack_object = unpacked.get();

    data.clear();
    if (auto d = findMapValue(msgpack_object, "data"))
        d->convert(data);
    signature.clear();
    if (auto s = findMapValue(msgpack_object, "signature"))
        s->convert(signature);
}

} /* dpaste  */

/* vim:set et sw=4 ts=4 tw=120: */

