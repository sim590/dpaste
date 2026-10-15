/*
 * Copyright © 2017-2020 Simon Désaulniers
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
#include <iomanip>
#include <memory>
#include <algorithm>
#include <charconv>
#include <unistd.h>
#include <type_traits>

#include <msgpack.hpp>

#include "bin.h"

#include "cipher.h"
#include "code.h"
#include "conf.h"
#include "log.h"
#include "gpgcrypto.h"

namespace dpaste {

const constexpr uint8_t Bin::PROTO_VERSION;

Bin::Bin() {
    /* load dpaste config */
    conf::ConfigurationFile config_file {};
    config_file.load();
    conf_ = config_file.getConfiguration();

    long port;
    {
        std::istringstream conv(conf_.at("port"));
        conv >> port;
    }

    node.run();
    http_client_ = std::make_unique<HttpClient>(conf_.at("host"), port);
}

std::string Bin::code_from_dpaste_uri(const std::string& uri) {
    static const std::string DUP {DPASTE_URI_PREFIX};
    const auto p = uri.find(DUP);
    return uri.substr(p != std::string::npos ? p+DUP.length() : 0, code::PIN_WITH_PASS_LEN);
}

std::optional<code::Info>
Bin::parse_code_info(const std::string& code) {
    /* Every format starts with the location pin, an hexadecimal number which
     * is not zero-padded in releases before 0.4.0. */
    const auto pin_end = code.data() + std::min<size_t>(code.size(), code::DPASTE_PIN_LEN);
    uint32_t pin {};
    const auto parsed_pin = std::from_chars(code.data(), pin_end, pin, 16);
    if (parsed_pin.ec != std::errc{} or parsed_pin.ptr != pin_end)
        return std::nullopt;

    /* Releases up to 0.5.0 do not carry a packet count field. */
    if (code.size() <= code::DPASTE_PIN_LEN)
        return code::Info { code, pin, 1, "", code::Format::LEGACY_PIN };

    const auto lcode = code.substr(0, code::DPASTE_PIN_LEN);
    if (code.size() == code::LEGACY_PIN_WITH_PASS_LEN)
        return code::Info { lcode, pin, 1,
                            code.substr(code::DPASTE_PIN_LEN, code::PASSWORD_LEN),
                            code::Format::LEGACY_PIN_PWD };

    if (code.size() != code::DPASTE_PIN_LEN + code::DPASTE_NPACKETS_LEN
            and code.size() != code::PIN_WITH_PASS_LEN)
        return std::nullopt;

    uint32_t npackets {};
    const auto* first = code.data() + code::DPASTE_PIN_LEN;
    const auto* last  = first + code::DPASTE_NPACKETS_LEN;
    const auto parsed_npackets = std::from_chars(first, last, npackets, 16);
    if (parsed_npackets.ec != std::errc{} or parsed_npackets.ptr != last or npackets == 0)
        return std::nullopt;

    const bool with_pwd = code.size() == code::PIN_WITH_PASS_LEN;
    return code::Info {
        lcode, pin, npackets,
        with_pwd ? code.substr(code::DPASTE_PIN_LEN + code::DPASTE_NPACKETS_LEN, code::PASSWORD_LEN)
                 : std::string {},
        with_pwd ? code::Format::PIN_NPACKETS_PWD : code::Format::PIN_NPACKETS };
}

std::vector<uint8_t>
Bin::parse_data(std::vector<uint8_t>&& value,
                std::string pwd,
                bool no_decrypt) const
{
    try {
        Packet p;
        p.deserialize(value);
        std::vector<uint8_t> data;

        auto cipher = crypto::Cipher::get(p.data_scheme);
        if (cipher and not no_decrypt) {
            std::shared_ptr<crypto::Parameters> params;
            if (p.data_scheme == crypto::Cipher::Scheme::AES) {
                params = std::make_shared<crypto::Parameters>();
                params->emplace<crypto::AESParameters>(pwd);
            }
            data = cipher->processCipherText(p.data, std::move(params));
        } else
            data = std::move(p.data);

        if (not (cipher or p.signature.empty())) {
            auto gc = std::dynamic_pointer_cast<crypto::GPG>(crypto::Cipher::get(crypto::Cipher::Scheme::GPG, {}));
            DPASTE_MSG("Data is GPG signed. Verifying...");
            auto res = gc->verify(p.signature, data);
            if (res.numSignatures() > 0)
                gc->comment_on_signature(res.signature(0));
        }

        return data;
    } catch (const GpgME::Exception& e) {
        DPASTE_MSG("%s", e.what());
        return {};
    } catch (const dht::crypto::DecryptError& e) {
        DPASTE_MSG("%s", e.what());
        return {};
    } catch (msgpack::insufficient_bytes& e) {
        DPASTE_MSG("%s", e.what());
        DPASTE_MSG("Insufficient bytes to unpack the data. The data may be corrupted or missing.");
        return {};
    } catch (msgpack::type_error& e) { } /* backward compatibility with <=0.3.3 */

    return {};
}

std::pair<bool, std::string> Bin::get(std::string&& code, bool no_decrypt, bool no_proxy) {
    code = code_from_dpaste_uri(code);
    const auto code_info = parse_code_info(code);
    if (not code_info) {
        DPASTE_MSG("Invalid code: %s", code.c_str());
        return {false, {}};
    }
    const auto& lcode    = code_info->lcode;
    const auto& npackets = code_info->npackets;
    const auto& pwd      = code_info->pwd;

    auto available = http_client_->isAvailable();
    auto get_method = [this,&available,&no_proxy](const auto& c) {
        /* if available, try http server */
        if (available and not no_proxy) {
            auto data = http_client_->get(c);
            std::vector<uint8_t> blob;
            std::move(data.begin(), data.end(), std::back_inserter(blob));
            return blob;
        /* if fail, then perform request from local node */
        } else {
            auto values = node.get(c);
            if (not values.empty())
                return values.front();
        }
        return std::vector<uint8_t> {};
    };

    std::vector<uint8_t> whole_data;
    for (uint8_t s = 0; s < npackets; ++s) {
        /* The first packet is looked up with the raw location code so that
         * unpadded pins from releases before 0.4.0 keep their original key. */
        const std::string target = s == 0 ? lcode : code_from_pin(code_info->pin + s);
        auto value               = get_method(target);
        auto data          = parse_data(std::move(value), pwd, no_decrypt);
        if (data.empty())
            return {false, {}};
        std::move(data.begin(), data.end(), std::back_inserter(whole_data));
    }

    std::string ret;
    std::move(whole_data.begin(), whole_data.end(), std::back_inserter(ret));
    return {true, std::move(ret)};
}

std::vector<uint8_t> Bin::data_from_stream(std::stringstream& input_stream, const size_t& count) {
    std::vector<uint8_t> buffer;
    buffer.resize(count);
    input_stream.read(reinterpret_cast<char*>(buffer.data()), count);
    buffer.resize(input_stream.gcount());
    return buffer;
}

std::string Bin::code_from_pin(uint32_t pin) {
    auto pin_s = hexStrFromInt(pin, code::DPASTE_PIN_LEN);
    std::transform(pin_s.begin(), pin_s.end(), pin_s.begin(), ::toupper);
    return pin_s;
}

std::string Bin::hexStrFromInt(uint32_t i, size_t len) {
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(len) << std::hex << i;
    return ss.str();
}

std::pair<std::vector<Bin::Packet>, std::string>
Bin::prepare_data(std::stringstream&& input_stream,
                  std::unique_ptr<crypto::Parameters>&& params)
{
    std::string pwd = "";
    std::shared_ptr<crypto::Parameters> sparams(std::move(params));
    std::shared_ptr<crypto::Parameters> init_params;
    crypto::Cipher::Scheme scheme {crypto::Cipher::Scheme::NONE};

    bool to_sign {false};
    if (auto gp = std::get_if<crypto::GPGParameters>(sparams.get())) {
        auto& keyid = conf_.at("pgp_key_id");
        to_sign     = gp->sign and not keyid.empty();
        scheme      = gp->scheme;
        init_params = std::make_shared<crypto::Parameters>();
        init_params->emplace<crypto::GPGParameters>(keyid);
    } else if (auto aesp = std::get_if<crypto::AESParameters>(sparams.get())) {
        scheme         = aesp->scheme;
        pwd            = random_pin();
        aesp->password = pwd;
    }

    auto cipher = crypto::Cipher::get(scheme, std::move(init_params));

    uint32_t rsiz = 0;
    const size_t SIZE_PER_PACKET = dht::MAX_VALUE_SIZE - Packet::EXTRA_SERIALIZATION_BYTES;
    std::vector<Packet> packets;
    packets.reserve(std::ceil(((double)Bin::DPASTE_MAX_SIZE) / SIZE_PER_PACKET));
    std::vector<uint8_t> data;
    while (rsiz < Bin::DPASTE_MAX_SIZE
           and (data = data_from_stream(input_stream, SIZE_PER_PACKET)).size() > 0)
    {
        rsiz += data.size();

        Packet p { .data_scheme = scheme };
        if (cipher) {
            auto cipher_text = cipher->processPlainText(data, std::move(sparams));
            if (cipher_text.empty()) {
                p.data.insert(p.data.end(), data.begin(), data.end());
                if (to_sign) {
                    DPASTE_MSG("Signing data...");
                    auto res = std::dynamic_pointer_cast<crypto::GPG>(cipher)->sign(p.data);
                    p.signature = std::move(res.first);
                }
            } else
                p.data = std::move(cipher_text);
        } else
            p.data = std::move(data);

        packets.push_back(std::move(p));
    }

    packets.shrink_to_fit();
    return {packets, std::move(pwd)};
}

std::string
Bin::paste(std::stringstream&& input_stream, std::unique_ptr<crypto::Parameters>&& params, bool no_proxy) {
    auto pin          = random_.integer();
    auto available    = http_client_->isAvailable();
    auto paste_method = [this,&available,&no_proxy](const auto& c, auto&& d) {
        if (available and not no_proxy) return http_client_->put(c, {d.begin(), d.end()});
        else                            return node.paste(c, std::forward<std::vector<uint8_t>>(d));
    };

    auto pp = prepare_data(std::forward<std::stringstream>(input_stream),
                                std::forward<std::unique_ptr<crypto::Parameters>>(params));
    auto& packets = pp.first;
    auto& pwd     = pp.second;

    DPASTE_MSG("Pasting data...");
    size_t shift = 0;
    for (const auto& p : packets) {
        auto code       = code_from_pin(pin + shift++);
        auto bin_packet = p.serialize();
        bool success    = paste_method(code, std::move(bin_packet));
        if (not success)
            return {};
    }

    return DPASTE_URI_PREFIX+code_from_pin(pin)+hexStrFromInt(shift, code::DPASTE_NPACKETS_LEN)+pwd;
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

    pk.pack_map(4);
    pk.pack("v");         pk.pack(PROTO_VERSION);
    pk.pack("scheme");    pk.pack(static_cast<std::underlying_type_t<crypto::Cipher::Scheme>>(data_scheme));
    pk.pack("data");      pk.pack(data);
    pk.pack("signature"); pk.pack(signature);

    return {buffer.data(), buffer.data()+buffer.size()};
}

void Bin::Packet::deserialize(const std::vector<uint8_t>& pbuffer) {
    msgpack::unpacked unpacked = msgpack::unpack(reinterpret_cast<const char*>(pbuffer.data()), pbuffer.size());
    auto msgpack_object = unpacked.get();

    data.clear();
    if (auto d = findMapValue(msgpack_object, "scheme")) {
        std::underlying_type_t<crypto::Cipher::Scheme> ds {};
        d->convert(ds);
        data_scheme = static_cast<crypto::Cipher::Scheme>(ds);
    }
    if (auto d = findMapValue(msgpack_object, "data"))
        d->convert(data);
    signature.clear();
    if (auto s = findMapValue(msgpack_object, "signature"))
        s->convert(signature);
}

} /* dpaste  */

/* vim:set et sw=4 ts=4 tw=120: */

