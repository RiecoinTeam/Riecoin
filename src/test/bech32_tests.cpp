// Copyright (c) 2017 Pieter Wuille
// Copyright (c) 2021-present The Bitcoin Core developers
// Copyright (c) 2024-present The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bech32.h>
#include <test/util/str.h>

#include <boost/test/unit_test.hpp>

#include <string>

BOOST_AUTO_TEST_SUITE(bech32_tests)

BOOST_AUTO_TEST_CASE(bech32m_testvectors_valid)
{
    static const std::string CASES[] = {
        "A1LQFN3A",
        "a1lqfn3a",
        "an83characterlonghumanreadablepartthatcontainsthetheexcludedcharactersbioandnumber11sg7hg6",
        "abcdef1l7aum6echk45nj3s0wdvt2fg8x9yrzpqzd3ryx",
        "11llllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllludsr8",
        "split1checkupstagehandshakeupstreamerranterredcaperredlc445v",
        "?1v759aa"
    };
    for (const std::string& str : CASES) {
        const auto dec = bech32::Decode(str);
        BOOST_CHECK(dec.encoding == bech32::Encoding::BECH32M);
        std::string recode = bech32::Encode(bech32::Encoding::BECH32M, dec.hrp, dec.data);
        BOOST_CHECK(!recode.empty());
        BOOST_CHECK(CaseInsensitiveEqual(str, recode));
    }
}

BOOST_AUTO_TEST_CASE(bech32m_testvectors_invalid)
{
    static const std::string CASES[] = {
        " 1xj0phk",
        "\x7f""1g6xzxy",
        "\x80""1vctc34",
        "an84characterslonghumanreadablepartthatcontainsthetheexcludedcharactersbioandnumber11d6pts4",
        "qyrz8wqd2c9m",
        "1qyrz8wqd2c9m",
        "y1b0jsk6g",
        "lt1igcx5c0",
        "in1muywd",
        "mm1crxm3i",
        "au1s5cgom",
        "M1VUXWEZ",
        "16plkw9",
        "1p2gdwpf",
        "abcdef1l7aum6echk45nj2s0wdvt2fg8x9yrzpqzd3ryx",
        "test1zg69v7y60n00qy352euf40x77qcusag6",
    };
    static const std::pair<std::string, std::vector<int>> ERRORS[] = {
        {"Invalid character or mixed case", {0}},
        {"Invalid character or mixed case", {0}},
        {"Invalid character or mixed case", {0}},
        {"Bech32 string too long", {90}},
        {"Missing separator", {}},
        {"Invalid separator position", {0}},
        {"Invalid Base 32 character", {2}},
        {"Invalid Base 32 character", {3}},
        {"Invalid separator position", {2}},
        {"Invalid Base 32 character", {8}},
        {"Invalid Base 32 character", {7}},
        {"Invalid checksum", {}},
        {"Invalid separator position", {0}},
        {"Invalid separator position", {0}},
        {"Invalid Bech32m checksum", {21}},
        {"Invalid Bech32m checksum", {13, 32}},
    };
    static_assert(std::size(CASES) == std::size(ERRORS), "Bech32m CASES and ERRORS should have the same length");

    int i = 0;
    for (const std::string& str : CASES) {
        const auto& err = ERRORS[i];
        const auto dec = bech32::Decode(str);
        BOOST_CHECK(dec.encoding == bech32::Encoding::INVALID);
        auto [error, error_locations] = bech32::LocateErrors(str);
        BOOST_CHECK_EQUAL(err.first, error);
        BOOST_CHECK(err.second == error_locations);
        i++;
    }
}

BOOST_AUTO_TEST_SUITE_END()
