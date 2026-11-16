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
#include <iomanip>
#include <memory>
#include <algorithm>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstring>
#include <unistd.h>
#include <type_traits>

#include <msgpack.hpp>

#include "bin.h"

#include "aescrypto.h"
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
    return uri.substr(p != std::string::npos ? p+DUP.length() : 0, code::MAX_LEN);
}

std::optional<code::Info>
Bin::parse_code_info(const std::string& code) {
    /* Every format starts with the location pin, an hexadecimal number which
     * is not zero-padded in releases before 0.4.0. */
    uint32_t pin {};
    const auto pin_end    = code.data() + std::min<size_t>(code.size(), code::DPASTE_PIN_LEN);
    const auto parsed_pin = std::from_chars(code.data(), pin_end, pin, 16);

    if (parsed_pin.ec != std::errc{} or parsed_pin.ptr != pin_end)
        return std::nullopt;

    /* Releases up to 0.5.0 do not carry a packet count field. */
    if (code.size() <= code::DPASTE_PIN_LEN)
        return code::Info { code, pin, 1, 1, "", code::Format::LEGACY_PIN };

    const auto lcode = code.substr(0, code::DPASTE_PIN_LEN);
    if (code.size() == code::LEGACY_PIN_WITH_PASS_LEN)
        return code::Info { lcode
                          , pin
                          , 1
                          , 1
                          , code.substr(code::DPASTE_PIN_LEN, code::PASSWORD_LEN)
                          , code::Format::LEGACY_PIN_PWD
                          };

    if (code.size() != code::DPASTE_PIN_LEN + code::DPASTE_NPACKETS_LEN
            and code.size() != code::PIN_WITH_PASS_LEN
            and code.size() != code::PIN_NPACKETS_VPK_LEN
            and code.size() != code::PIN_NPACKETS_VPK_PWD_LEN)
        return std::nullopt;

    uint32_t npackets {};
    const auto* first          = code.data() + code::DPASTE_PIN_LEN;
    const auto* last           = first + code::DPASTE_NPACKETS_LEN;
    const auto parsed_npackets = std::from_chars(first, last, npackets, 16);

    if (parsed_npackets.ec != std::errc{} or parsed_npackets.ptr != last or npackets == 0)
        return std::nullopt;

    const bool with_vpk = code.size() == code::PIN_NPACKETS_VPK_LEN
                       or code.size() == code::PIN_NPACKETS_VPK_PWD_LEN;
    const bool with_pwd = code.size() == code::PIN_WITH_PASS_LEN
                       or code.size() == code::PIN_NPACKETS_VPK_PWD_LEN;

    uint8_t vpk {1};
    if (with_vpk) {
        uint32_t parsed_vpk {};
        const auto* vpk_first = code.data() + code::DPASTE_PIN_LEN + code::DPASTE_NPACKETS_LEN;
        const auto* vpk_last  = vpk_first + code::DPASTE_VPK_LEN;
        const auto parsed     = std::from_chars(vpk_first, vpk_last, parsed_vpk, 16);
        if (parsed.ec != std::errc{} or parsed.ptr != vpk_last or parsed_vpk == 0)
            return std::nullopt;
        vpk = static_cast<uint8_t>(parsed_vpk);
    }

    const auto pwd_offset = code::DPASTE_PIN_LEN + code::DPASTE_NPACKETS_LEN
                          + (with_vpk ? code::DPASTE_VPK_LEN : 0);
    const auto format = with_vpk
                      ? (with_pwd ? code::Format::PIN_NPACKETS_VPK_PWD : code::Format::PIN_NPACKETS_VPK)
                      : (with_pwd ? code::Format::PIN_NPACKETS_PWD : code::Format::PIN_NPACKETS);
    return code::Info {
        lcode, pin, npackets, vpk,
        with_pwd ? code.substr(pwd_offset, code::PASSWORD_LEN) : std::string {},
        format };
}

std::optional<Bin::ParsedPacket>
Bin::parse_data(std::vector<uint8_t>&& value,
                std::string pwd,
                bool no_decrypt) const
{
    Packet p;
    bool guessed_gpg {false}; /* Whether the OpenPGP cipher was guessed by the legacy detection. If it
                               * was and decryption fails, the raw value is returned (see below).
                               */
    try {
        p.deserialize(value);

        std::shared_ptr<crypto::Cipher> cipher;
        if (p.data_scheme) {
            /* Packets written by protocol version 1 and later store the data
             * scheme, so the cipher is selected without guessing. */
            switch (*p.data_scheme) {
                case crypto::Cipher::Scheme::NONE:
                    break;
                case crypto::Cipher::Scheme::AES:
                case crypto::Cipher::Scheme::GPG:
                    if (not no_decrypt)
                        cipher = crypto::Cipher::get(*p.data_scheme);
                    break;
                default:
                    DPASTE_MSG( "Unknown data scheme (%u). Discarding the value."
                              , static_cast<unsigned>(*p.data_scheme)
                              );
                    return std::nullopt;
            }
        } else if (not no_decrypt) {
            /* Packets written by releases up to 0.5.0 do not carry a scheme.
             * Reproduce the detection used by those releases. It is a guess:
             * remember whether it selected OpenPGP so that a failed
             * decryption can fall back to the raw value. */
            cipher      = crypto::Cipher::getLegacy(p.data, pwd);
            guessed_gpg = std::dynamic_pointer_cast<crypto::GPG>(cipher) != nullptr;
        }

        std::vector<uint8_t> data;
        if (cipher) {
            std::shared_ptr<crypto::Parameters> params;
            if (std::dynamic_pointer_cast<crypto::AES>(cipher)) {
                params = std::make_shared<crypto::Parameters>();
                params->emplace<crypto::AESParameters>(pwd);
            }
            data = cipher->processCipherText(p.data, std::move(params));
            /* GPG::encrypt appends a NUL delimiter to the plaintext of every
             * packet; decryption returns it unchanged. Strip exactly that
             * delimiter so that reassembled pastes do not contain stray NUL
             * bytes. Legacy values (guessed OpenPGP) and --no-decrypt keep
             * their current behavior. */
            if (p.data_scheme and *p.data_scheme == crypto::Cipher::Scheme::GPG
                and not data.empty() and data.back() == '\0')
                data.pop_back();
        } else
            data = std::move(p.data);

        if (not (cipher or p.signature.empty())) {
            DPASTE_MSG("Data is GPG signed. Verifying...");
            auto gc  = std::dynamic_pointer_cast<crypto::GPG>(crypto::Cipher::get(crypto::Cipher::Scheme::GPG, {}));
            auto res = gc->verify(p.signature, data);
            if (res.numSignatures() > 0)
                gc->comment_on_signature(res.signature(0));
        }

        return ParsedPacket { p.index, std::move(data) };
    } catch (const GpgME::Exception& e) {
        DPASTE_MSG("%s", e.what());
        if (guessed_gpg) {
            /* The OpenPGP detection may have misfired, or the value cannot
             * be decrypted here. Return it as is rather than losing it. */
            DPASTE_MSG("The value was detected as OpenPGP encrypted, but decryption failed. Returning the raw value.");
            return ParsedPacket { p.index, std::move(p.data) };
        }
        return std::nullopt;
    } catch (const dht::crypto::DecryptError& e) {
        DPASTE_MSG("%s", e.what());
        return std::nullopt;
    } catch (msgpack::unpack_error& e) {
        /* The value cannot be unpacked as a MessagePack packet: the header
         * is truncated or malformed. Releases up to 0.3.3 stored raw values,
         * so return it unchanged as an unindexed packet. Grouped packets are
         * still rejected by the missing-index check in get(). */
        DPASTE_MSG("%s", e.what());
        DPASTE_MSG("The value is not a MessagePack packet. Returning the raw value.");
        return ParsedPacket { std::nullopt, std::move(value) };
    } catch (msgpack::type_error& e) {
        /* Releases up to 0.3.3 stored raw values which are not MessagePack
         * maps. Return them as unindexed packets. */
        return ParsedPacket { std::nullopt, std::move(value) };
    }

    return std::nullopt;
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
    const auto& vpk      = code_info->vpk;
    const auto& pwd      = code_info->pwd;
    /* Codes from before the values-per-key field store one packet per key and
     * do not carry the packet index. */
    const bool one_per_key = code_info->format != code::Format::PIN_NPACKETS_VPK
                         and code_info->format != code::Format::PIN_NPACKETS_VPK_PWD;

    auto available = not no_proxy and http_client_->isAvailable();

    const uint32_t nkeys = static_cast<uint32_t>(std::ceil(static_cast<double>(npackets) / vpk));

    /* The first key is looked up with the raw location code so that unpadded
     * pins from releases before 0.4.0 keep their original key. */
    std::vector<std::string> targets(nkeys);
    for (uint32_t k = 0; k < nkeys; ++k)
        targets[k] = k == 0 ? lcode : code_from_pin(code_info->pin + k);

    /* Fetch every key in one batch. */
    std::vector<std::vector<dht::Blob>> key_values(nkeys);
    std::vector<size_t> missing;
    if (available) {
        const auto responses = http_client_->getAll(targets);
        for (size_t k = 0; k < nkeys; ++k) {
            for (auto& data : responses[k])
                key_values[k].emplace_back(data.begin(), data.end());
            /* A successful probe does not guarantee that a request succeeds:
             * fall back to the local node for the missing ones. */
            if (key_values[k].empty())
                missing.push_back(k);
        }
    } else {
        for (size_t k = 0; k < nkeys; ++k)
            missing.push_back(k);
    }
    if (not missing.empty()) {
        std::vector<std::string> retry;
        retry.reserve(missing.size());
        for (auto k : missing)
            retry.push_back(targets[k]);
        const auto node_values = node.getAll(retry);
        for (size_t j = 0; j < missing.size(); ++j)
            key_values[missing[j]] = node_values[j];
    }

    std::vector<std::vector<uint8_t>> chunks(npackets);
    std::vector<bool> found(npackets, false);

    for (uint32_t k = 0; k < nkeys; ++k) {
        for (auto& value : key_values[k]) {
            auto parsed = parse_data(std::move(value), pwd, no_decrypt);
            if (not parsed or parsed->data.empty())
                return {false, {}};

            uint32_t idx {};
            if (one_per_key)
                idx = k;
            else if (parsed->index)
                idx = *parsed->index;
            else
                return {false, {}};

            /* Every packet must be present exactly once, under the key it was
             * published under. */
            if (idx >= npackets or idx / vpk != k or found[idx])
                return {false, {}};

            chunks[idx] = std::move(parsed->data);
            found[idx]  = true;
        }
    }

    if (std::find(found.begin(), found.end(), false) != found.end())
        return {false, {}};

    std::vector<uint8_t> whole_data;
    for (auto& chunk : chunks)
        std::move(chunk.begin(), chunk.end(), std::back_inserter(whole_data));

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
    const size_t SIZE_PER_PACKET = scheme == crypto::Cipher::Scheme::GPG
                                 ? Bin::GPG_SIZE_PER_PACKET
                                 : dht::MAX_VALUE_SIZE - Packet::EXTRA_SERIALIZATION_BYTES;
    std::vector<Packet> packets;
    packets.reserve(std::ceil(((double)Bin::DPASTE_MAX_SIZE) / SIZE_PER_PACKET));
    std::vector<uint8_t> data;
    while (rsiz < Bin::DPASTE_MAX_SIZE
           and (data = data_from_stream(input_stream,
                                        std::min(SIZE_PER_PACKET, Bin::DPASTE_MAX_SIZE - rsiz))).size() > 0)
    {
        rsiz += data.size();

        Packet p;
        p.data_scheme = scheme;
        p.index       = static_cast<uint16_t>(packets.size());
        if (cipher) {
            auto cipher_text = cipher->processPlainText(data, std::move(sparams));
            if (cipher_text.empty()) {
                /* The data was not encrypted (sign-only paste): mark it as
                 * plaintext so that retrieval verifies the signature instead
                 * of trying to decrypt the data. */
                p.data_scheme = crypto::Cipher::Scheme::NONE;
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
    auto pin = random_.integer();

    auto pp = prepare_data(std::forward<std::stringstream>(input_stream),
                                std::forward<std::unique_ptr<crypto::Parameters>>(params));
    auto& packets = pp.first;
    auto& pwd     = pp.second;

    if (packets.empty()) {
        DPASTE_MSG("Empty input. Nothing to paste.");
        return {};
    }

    /* Build the request batch: packets are spread contiguously,
     * VALUES_PER_KEY packets per key. */
    std::vector<std::pair<std::string, std::string>> items;
    items.reserve(packets.size());
    for (size_t i = 0; i < packets.size(); ++i) {
        const uint32_t key_index = static_cast<uint32_t>(i / Bin::VALUES_PER_KEY);
        auto code                = code_from_pin(pin + key_index);
        auto bin_packet          = packets[i].serialize();
        /* OpenDHT drops values bigger than MAX_VALUE_SIZE, which would make
         * the whole paste fail silently at the proxy. */
        if (bin_packet.size() + std::strlen(Node::DPASTE_USER_TYPE) > dht::MAX_VALUE_SIZE) {
            DPASTE_MSG("Packet is too large for the DHT (%zu bytes). Not publishing.", bin_packet.size());
            return {};
        }
        items.emplace_back(std::move(code), std::string(bin_packet.begin(), bin_packet.end()));
    }

    DPASTE_MSG("Pasting data...");
    std::vector<bool> results;
    const auto available = not no_proxy and http_client_->isAvailable();
    if (available) {
        results = http_client_->putAll(items);
        /* A proxy request may fail even if the stats probe succeeded: retry
         * the failed ones on the local node. */
        std::vector<size_t> failed;
        for (size_t i = 0; i < results.size(); ++i)
            if (not results[i])
                failed.push_back(i);
        if (not failed.empty()) {
            std::vector<std::pair<std::string, dht::Blob>> retry;
            retry.reserve(failed.size());
            for (auto i : failed)
                retry.emplace_back(items[i].first, dht::Blob(items[i].second.begin(), items[i].second.end()));
            const auto retry_results = node.pasteAll(std::move(retry));
            for (size_t j = 0; j < failed.size(); ++j)
                results[failed[j]] = retry_results[j];
        }
    } else {
        std::vector<std::pair<std::string, dht::Blob>> node_items;
        node_items.reserve(items.size());
        for (auto& item : items)
            node_items.emplace_back(item.first, dht::Blob(item.second.begin(), item.second.end()));
        results = node.pasteAll(std::move(node_items));
    }

    if (std::find(results.begin(), results.end(), false) != results.end())
        return {};

    return DPASTE_URI_PREFIX
         + code_from_pin(pin)
         + hexStrFromInt(packets.size(), code::DPASTE_NPACKETS_LEN)
         + hexStrFromInt(Bin::VALUES_PER_KEY, code::DPASTE_VPK_LEN)
         + pwd;
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
    /* Packets are always created with a data scheme and an index (see
     * prepare_data); a missing one is a bug. Fail hard rather than storing
     * Scheme::NONE, which would make retrieval mistake encrypted data for
     * plain data. */
    assert(data_scheme.has_value() and index.has_value());

    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);

    pk.pack_map(5);
    pk.pack("v");         pk.pack(PROTO_VERSION);
    pk.pack("scheme");    pk.pack(static_cast<std::underlying_type_t<crypto::Cipher::Scheme>>(data_scheme.value()));
    pk.pack("index");     pk.pack(index.value());
    pk.pack("data");      pk.pack(data);
    pk.pack("signature"); pk.pack(signature);

    return {buffer.data(), buffer.data()+buffer.size()};
}

void Bin::Packet::deserialize(const std::vector<uint8_t>& pbuffer) {
    msgpack::unpacked unpacked = msgpack::unpack(reinterpret_cast<const char*>(pbuffer.data()), pbuffer.size());
    auto msgpack_object = unpacked.get();

    data_scheme.reset();
    if (auto d = findMapValue(msgpack_object, "scheme")) {
        std::underlying_type_t<crypto::Cipher::Scheme> ds {};
        d->convert(ds);
        data_scheme = static_cast<crypto::Cipher::Scheme>(ds);
    }
    index.reset();
    if (auto i = findMapValue(msgpack_object, "index")) {
        uint16_t idx {};
        i->convert(idx);
        index = idx;
    }
    data.clear();
    if (auto d = findMapValue(msgpack_object, "data"))
        d->convert(data);
    signature.clear();
    if (auto s = findMapValue(msgpack_object, "signature"))
        s->convert(signature);
}

} /* dpaste  */

/* vim:set et sw=4 ts=4 tw=120: */

