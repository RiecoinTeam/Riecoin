// Copyright (c) 2017-2020 The Bitcoin Core developers
// Copyright (c) 2013-2021 The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <stdlib.h>

#include <chain.h>
#include <rpc/blockchain.h>
#include <test/util/setup_common.h>
#include <util/string.h>

/* Equality between doubles is imprecise. Comparison should be done
 * with a small threshold of tolerance, rather than exact equality.
 */
static bool DoubleEquals(double a, double b, double epsilon)
{
    return std::abs(a - b) < epsilon;
}

static CBlockIndex* CreateBlockIndexWithNbits(uint32_t nbits)
{
    CBlockIndex* block_index = new CBlockIndex();
    block_index->nHeight = 46367;
    block_index->nTime = 1269211443;
    block_index->nBits = nbits;
    return block_index;
}

static void RejectDifficultyMismatch(double difficulty, double expected_difficulty) {
     BOOST_CHECK_MESSAGE(
        DoubleEquals(difficulty, expected_difficulty, 0.00001),
        "Difficulty was " + ToString(difficulty)
            + " but was expected to be " + ToString(expected_difficulty));
}

/* Given a BlockIndex with the provided nbits,
 * verify that the expected difficulty results.
 */
static void TestDifficulty(uint32_t nbits, double expected_difficulty, int32_t powVersion)
{
    CBlockIndex* block_index = CreateBlockIndexWithNbits(nbits);
    double difficulty = GetDifficulty(block_index, powVersion);
    delete block_index;

    RejectDifficultyMismatch(difficulty, expected_difficulty);
}

BOOST_FIXTURE_TEST_SUITE(blockchain_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(get_difficulty_for_very_low_target)
{
    TestDifficulty(0x02019000, 400., -1); // 2^(8*(2 - 2))*400 or 2^(8*(2 - 3))*102400
}

BOOST_AUTO_TEST_CASE(get_difficulty_for_low_target)
{
    TestDifficulty(0x02064000, 1600., -1); // 2^(8*(2 - 2))*1600
}

BOOST_AUTO_TEST_CASE(get_difficulty_for_mid_target)
{
    TestDifficulty(316049, 1234.56640625, 1); // 316049/256
}

BOOST_AUTO_TEST_CASE(get_difficulty_for_high_target)
{
    TestDifficulty(0x02064000, 132672, 1); // 33964032/256
}

BOOST_AUTO_TEST_CASE(get_difficulty_for_very_high_target)
{
    TestDifficulty(0xffffffff, 16777215.99609375, 1); // (2^32 - 1)/256
}

BOOST_AUTO_TEST_SUITE_END()
