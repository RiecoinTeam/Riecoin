// Copyright (c) 2014-present The Bitcoin Core developers
// Copyright (c) 2014-present The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key_io.h>

#include <base58.h>
#include <bech32.h>
#include <script/interpreter.h>
#include <script/solver.h>
#include <tinyformat.h>
#include <util/strencodings.h>

#include <algorithm>
#include <cassert>
#include <cstring>

/// Maximum witness length for Bech32 addresses.
static constexpr std::size_t BECH32_WITNESS_PROG_MAX_LEN = 40;

namespace {
class DestinationEncoder
{
private:
    const CChainParams& m_params;

public:
    explicit DestinationEncoder(const CChainParams& params) : m_params(params) {}

    std::string operator()(const WitnessV0KeyHash& id) const
    {
        std::vector<unsigned char> data = {0};
        data.reserve(33);
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, id.begin(), id.end());
        return bech32::Encode(bech32::Encoding::BECH32M, m_params.Bech32HRP(), data);
    }

    std::string operator()(const WitnessV0ScriptHash& id) const
    {
        std::vector<unsigned char> data = {0};
        data.reserve(53);
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, id.begin(), id.end());
        return bech32::Encode(bech32::Encoding::BECH32M, m_params.Bech32HRP(), data);
    }

    std::string operator()(const WitnessV1Taproot& tap) const
    {
        std::vector<unsigned char> data = {1};
        data.reserve(53);
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, tap.begin(), tap.end());
        return bech32::Encode(bech32::Encoding::BECH32M, m_params.Bech32HRP(), data);
    }

    std::string operator()(const WitnessUnknown& id) const
    {
        const std::vector<unsigned char>& program = id.GetWitnessProgram();
        if (id.GetWitnessVersion() < 1 || id.GetWitnessVersion() > 16 || program.size() < 2 || program.size() > 40) {
            return {};
        }
        std::vector<unsigned char> data = {(unsigned char)id.GetWitnessVersion()};
        data.reserve(1 + (program.size() * 8 + 4) / 5);
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, program.begin(), program.end());
        return bech32::Encode(bech32::Encoding::BECH32M, m_params.Bech32HRP(), data);
    }

    std::string operator()(const CNoDestination& no) const { return {}; }
    std::string operator()(const PubKeyDestination& pk) const { return {}; }

    std::string operator()(const PKHash& id) const
    {
        std::vector<unsigned char> data;
        data.insert(data.end(), id.begin(), id.end());
        return std::string("76a914") + HexStr(data) + std::string("88ac");
    }

    std::string operator()(const ScriptHash& id) const
    {
        std::vector<unsigned char> data;
        data.insert(data.end(), id.begin(), id.end());
        return std::string("a914") + HexStr(data) + std::string("87");
    }
};

CTxDestination DecodeDestination(const std::string& str, const CChainParams& params, std::string& error_str, std::vector<int>* error_locations)
{
    std::vector<unsigned char> data;
    uint160 hash;
    error_str = "";
    // Note this will be false if it is a valid Bech32 address for a different network
    bool is_bech32 = (ToLower(str.substr(0, params.Bech32HRP().size())) == params.Bech32HRP());

    if (!is_bech32) {
        std::vector<unsigned char> data;
        if (str.size() == 50) {
            if (str.substr(0, 6) == "76a914" && str.substr(46, 4) == "88ac") {
                data = ParseHex(str.substr(6, 40));
                if (data.size() == 20) {
                    std::copy(data.begin(), data.end(), hash.begin());
                    return PKHash(hash);
                }
            }
        }
        else if (str.size() == 46) {
            if (str.substr(0, 4) == "a914" && str.substr(44, 2) == "87") {
                data = ParseHex(str.substr(4, 40));
                if (data.size() == 20) {
                    std::copy(data.begin(), data.end(), hash.begin());
                    return ScriptHash(hash);
                }
            }
        }
        error_str = "Invalid or unsupported Segwit (Bech32) encoding or Script.";
        return CNoDestination();
    }

    data.clear();
    const auto dec = bech32::Decode(str);
    if (dec.encoding == bech32::Encoding::BECH32 || dec.encoding == bech32::Encoding::BECH32M) {
        if (dec.data.empty()) {
            error_str = "Empty Bech32 data section";
            return CNoDestination();
        }
        // Bech32 decoding
        if (dec.hrp != params.Bech32HRP()) {
            error_str = strprintf("Invalid or unsupported prefix for Segwit (Bech32) address (expected %s, got %s).", params.Bech32HRP(), dec.hrp);
            return CNoDestination();
        }
        int version = dec.data[0]; // The first 5 bit symbol is the witness version (0-16)
        if (version == 0 && dec.encoding != bech32::Encoding::BECH32 && dec.encoding != bech32::Encoding::BECH32M) {
            error_str = "Version 0 witness address must use Bech32 or Bech32m checksum"; // Short term compatibility purpose, to be refactored into a single Bech32M check later.
            return CNoDestination();
        }
        if (version != 0 && dec.encoding != bech32::Encoding::BECH32M) {
            error_str = "Version 1+ witness address must use Bech32m checksum";
            return CNoDestination();
        }
        // The rest of the symbols are converted witness program bytes.
        data.reserve(((dec.data.size() - 1) * 5) / 8);
        if (ConvertBits<5, 8, false>([&](unsigned char c) { data.push_back(c); }, dec.data.begin() + 1, dec.data.end())) {

            std::string_view byte_str{data.size() == 1 ? "byte" : "bytes"};

            if (version == 0) {
                {
                    WitnessV0KeyHash keyid;
                    if (data.size() == keyid.size()) {
                        std::copy(data.begin(), data.end(), keyid.begin());
                        return keyid;
                    }
                }
                {
                    WitnessV0ScriptHash scriptid;
                    if (data.size() == scriptid.size()) {
                        std::copy(data.begin(), data.end(), scriptid.begin());
                        return scriptid;
                    }
                }

                error_str = strprintf("Invalid Bech32 v0 address program size (%d %s), per BIP141", data.size(), byte_str);
                return CNoDestination();
            }

            if (version == 1 && data.size() == WITNESS_V1_TAPROOT_SIZE) {
                static_assert(WITNESS_V1_TAPROOT_SIZE == WitnessV1Taproot::size());
                WitnessV1Taproot tap;
                std::copy(data.begin(), data.end(), tap.begin());
                return tap;
            }

            if (CScript::IsPayToAnchor(version, data)) {
                return PayToAnchor();
            }

            if (version > 16) {
                error_str = "Invalid Bech32 address witness version";
                return CNoDestination();
            }

            if (data.size() < 2 || data.size() > BECH32_WITNESS_PROG_MAX_LEN) {
                error_str = strprintf("Invalid Bech32 address program size (%d %s)", data.size(), byte_str);
                return CNoDestination();
            }

            return WitnessUnknown{version, data};
        } else {
            error_str = strprintf("Invalid padding in Bech32 data section");
            return CNoDestination();
        }
    }

    // Perform Bech32 error location
    auto res = bech32::LocateErrors(str);
    error_str = res.first;
    if (error_locations) *error_locations = std::move(res.second);
    return CNoDestination();
}
} // namespace

CKey DecodeSecret(const std::string& str)
{
    CKey key;
    std::vector<unsigned char> data;
    if (str.size() == 67) {
        if (str.substr(0, 3) == "prv") { // Compressed: prv . 64 hex
            data = ParseHex(str.substr(3, 64));
            if (data.size() == 32)
                key.Set(data.begin(), data.begin() + 32, true);
        }
    }
    if (!data.empty()) {
        memory_cleanse(data.data(), data.size());
    }
    return key;
}

std::string EncodeSecret(const CKey& key)
{
    assert(key.IsValid());
    std::vector<unsigned char> data;
    data.insert(data.end(), UCharCast(key.begin()), UCharCast(key.end()));
    std::string ret("prv" + HexStr(data));
    memory_cleanse(data.data(), data.size());
    return ret;
}

CExtPubKey DecodeExtPubKey(const std::string& str)
{
    CExtPubKey key;
    std::vector<unsigned char> data;
    if (DecodeBase58Check(str, data, 78)) {
        const std::vector<unsigned char>& prefix = Params().Base58Prefix(CChainParams::EXT_PUBLIC_KEY);
        if (data.size() == BIP32_EXTKEY_SIZE + prefix.size() && std::equal(prefix.begin(), prefix.end(), data.begin())) {
            key.Decode(data.data() + prefix.size());
        }
    }
    return key;
}

std::string EncodeExtPubKey(const CExtPubKey& key)
{
    std::vector<unsigned char> data = Params().Base58Prefix(CChainParams::EXT_PUBLIC_KEY);
    size_t size = data.size();
    data.resize(size + BIP32_EXTKEY_SIZE);
    key.Encode(data.data() + size);
    std::string ret = EncodeBase58Check(data);
    return ret;
}

CExtKey DecodeExtKey(const std::string& str)
{
    CExtKey key;
    std::vector<unsigned char> data;
    if (DecodeBase58Check(str, data, 78)) {
        const std::vector<unsigned char>& prefix = Params().Base58Prefix(CChainParams::EXT_SECRET_KEY);
        if (data.size() == BIP32_EXTKEY_SIZE + prefix.size() && std::equal(prefix.begin(), prefix.end(), data.begin())) {
            key.Decode(data.data() + prefix.size());
        }
    }
    if (!data.empty()) {
        memory_cleanse(data.data(), data.size());
    }
    return key;
}

std::string EncodeExtKey(const CExtKey& key)
{
    std::vector<unsigned char> data = Params().Base58Prefix(CChainParams::EXT_SECRET_KEY);
    size_t size = data.size();
    data.resize(size + BIP32_EXTKEY_SIZE);
    key.Encode(data.data() + size);
    std::string ret = EncodeBase58Check(data);
    memory_cleanse(data.data(), data.size());
    return ret;
}

std::string EncodeDestination(const CTxDestination& dest)
{
    return std::visit(DestinationEncoder(Params()), dest);
}

CTxDestination DecodeDestination(const std::string& str, std::string& error_msg, std::vector<int>* error_locations)
{
    return DecodeDestination(str, Params(), error_msg, error_locations);
}

CTxDestination DecodeDestination(const std::string& str)
{
    std::string error_msg;
    return DecodeDestination(str, error_msg);
}

bool IsValidDestinationString(const std::string& str, const CChainParams& params)
{
    std::string error_msg;
    return IsValidDestination(DecodeDestination(str, params, error_msg, nullptr));
}

bool IsValidDestinationString(const std::string& str)
{
    return IsValidDestinationString(str, Params());
}
