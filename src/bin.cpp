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

Bin::Bin(std::string&& code,
        std::stringstream&& data_stream,
        std::string&& recipient,
        bool sign,
        bool no_decrypt,
        bool self_recipient) :
    code_(code), recipients_({recipient}), sign_(sign), no_decrypt_(no_decrypt)
{
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

    /* we include self as recipient if there's at least one other recipient */
    if (not recipients_.empty() and self_recipient and not keyid_.empty())
        recipients_.emplace_back(keyid_);

    if (code_.empty()) { /* operation is put */
        std::array<uint8_t, dht::MAX_VALUE_SIZE> buf;
        data_stream.read(reinterpret_cast<char*>(buf.data()), dht::MAX_VALUE_SIZE);
        buffer_.insert(buffer_.end(), buf.begin(), buf.begin()+data_stream.gcount());
    } else { /* otherwise, it's a get */
        const auto p = code_.find_first_of(DPASTE_CODE_PREFIX);
        code_ = code_.substr(p != std::string::npos ? sizeof(DPASTE_CODE_PREFIX)-1 : p);
    }
}

void comment_on_signature(const GpgME::Signature& sig) {
    const auto& s = sig.summary();
    if (s & GpgME::Signature::Valid)
        DPASTE_MSG("Valid signature from key with ID %s", sig.fingerprint());
}

int Bin::get() {
    /* first try http server */
    auto data_str = http_client_->get(code_);
    std::vector<uint8_t> data {data_str.begin(), data_str.end()};

    /* if fail, then perform request from local node */
    if (data.empty()) {

        /* get a pasted blob */
        auto values = node.get(code_);
        if (not values.empty())
            data = values.front();
        node.stop();
    }

    if (not data.empty()) {
        Packet p;
        p.deserialize(data);
        data.clear();
        try {
            if (gpg->isGPGencrypted(p.data)) {
                DPASTE_MSG("Data is GPG encrypted.");
                if (not no_decrypt_) {
                    DPASTE_MSG("Decrypting...");
                    auto res = gpg->decryptAndVerify(p.data);
                    DPASTE_MSG("Success!");

                    data = std::get<0>(res);
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
            return -1;
        }

        { /* print the buffer from data without copying */
            std::stringstream ss;
            ss.rdbuf()->pubsetbuf(reinterpret_cast<char*>(&data[0]), data.size());
            std::cout << ss.rdbuf() << std::endl;
        }
    }
    return 0;
}

int Bin::paste() const {
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
    auto to_sign = sign_ and not keyid_.empty();
    if (not recipients_.empty()) {
        DPASTE_MSG("Encrypting data...");
        auto res = gpg->encrypt(recipients_, buffer_, to_sign);
        p.data = std::get<0>(res);
    } else {
        if (to_sign) {
            auto res = gpg->sign(p.data);
            p.signature = res.first;
        }
        p.data.insert(p.data.end(), buffer_.begin(), buffer_.end());
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

    if (success) {
        std::cout << DPASTE_CODE_PREFIX << code << std::endl;
        return 0;
    } else
        return 1;
}

std::vector<uint8_t> Bin::Packet::serialize() const {
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);

    pk.pack_map(3);
    pk.pack("v");    pk.pack(PROTO_VERSION);
    pk.pack("data"); //pk.pack_array(1);
                     pk.pack(data);
    pk.pack("signature"); //pk.pack_array(1);
                          pk.pack(signature);
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

