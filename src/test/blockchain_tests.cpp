// Copyright (c) 2017-2019 The Bitcoin Core developers
// Copyright (c) 2013-2020 The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <stdlib.h>

#include <chain.h>
#include <pow.h>
#include <rpc/blockchain.h>
#include <util/string.h>
#include <test/util/setup_common.h>

static CBlockIndex* CreateBlockIndexWithNbits(uint32_t nbits)
{
    CBlockIndex* block_index = new CBlockIndex();
    block_index->nHeight = 46367;
    block_index->nTime = 1269211443;
    block_index->nBits = nbits;
    return block_index;
}

static void RejectDifficultyMismatch(mpz_class difficulty, mpz_class expected_difficulty) {
     BOOST_CHECK_MESSAGE(
        difficulty == expected_difficulty,
        "Difficulty was " + difficulty.get_str()
            + " but was expected to be " + expected_difficulty.get_str());
}

/* Given a BlockIndex with the provided nbits,
 * verify that the expected difficulty results.
 */
static void TestDifficulty(uint32_t nbits, mpz_class expected_difficulty)
{
    CBlockIndex* block_index = CreateBlockIndexWithNbits(nbits);
    mpz_class difficulty = GetDifficulty(block_index->nBits, -1);
    delete block_index;

    RejectDifficultyMismatch(difficulty, expected_difficulty);
}

BOOST_FIXTURE_TEST_SUITE(blockchain_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(get_difficulty_for_very_low_target)
{
    TestDifficulty(0x02019000, 400); // 2^(8*(2 - 2))*400 or 2^(8*(2 - 3))*102400
}

BOOST_AUTO_TEST_CASE(get_difficulty_for_low_target)
{
    TestDifficulty(0x02064000, 1600); // 2^(8*(2 - 2))*1600
}

BOOST_AUTO_TEST_CASE(get_difficulty_for_mid_target)
{
    TestDifficulty(0x037fff80, 8388480); // 2^(8*(3 - 2))*(32767 + 128/256)
}

BOOST_AUTO_TEST_CASE(get_difficulty_for_high_target)
{
    TestDifficulty(0x05123456, mpz_class("78187462656")); // 2^(8*(5 - 2))*(4660 + 86/256)
}

BOOST_AUTO_TEST_CASE(get_difficulty_for_very_high_target)
{
    TestDifficulty(0x207fffff, mpz_class("57896037716911750921221705069588091649609539881711309849342236841432341020672")); // 2^(8*(32 - 2))*(32767 + 255/256)
}

BOOST_AUTO_TEST_SUITE_END()
