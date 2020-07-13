// Copyright (c) 2014-2019 The Bitcoin Core developers
// Copyright (c) 2013-2020 The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <net.h>
#include <validation.h>

#include <test/util/setup_common.h>

#include <boost/signals2/signal.hpp>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(validation_tests, TestingSetup)

static void TestBlockSubsidyHalvings(const Consensus::Params& consensusParams)
{
    int maxHalvings = 64;
    CAmount nInitialSubsidy = 50 * COIN;

    CAmount nPreviousSubsidy = nInitialSubsidy * 2; // for height == 0
    BOOST_CHECK_EQUAL(nPreviousSubsidy, nInitialSubsidy * 2);
    for (int nHalvings = 0; nHalvings < maxHalvings; nHalvings++) {
        int nHeight = nHalvings * consensusParams.nSubsidyHalvingInterval;
        CAmount nSubsidy = GetBlockSubsidy(nHeight, consensusParams);
        BOOST_CHECK(nSubsidy <= nInitialSubsidy);
        BOOST_CHECK_EQUAL(nSubsidy, nPreviousSubsidy / 2);
        nPreviousSubsidy = nSubsidy;
    }
    BOOST_CHECK_EQUAL(GetBlockSubsidy(maxHalvings * consensusParams.nSubsidyHalvingInterval, consensusParams), 0);
}

static void TestBlockSubsidyHalvings(int nSubsidyHalvingInterval)
{
    Consensus::Params consensusParams;
    consensusParams.nSubsidyHalvingInterval = nSubsidyHalvingInterval;
    consensusParams.hasFairLaunch = false; // Disable Fair Launch Subsidies
    consensusParams.fork1Height = 2147483647; // Disable SuperBlocks
    TestBlockSubsidyHalvings(consensusParams);
}

BOOST_AUTO_TEST_CASE(block_subsidy_test)
{
    Consensus::Params consensusParams(CreateChainParams(CBaseChainParams::MAIN)->GetConsensus());
    consensusParams.hasFairLaunch = false; // Disable Fair Launch Subsidies
    consensusParams.fork1Height = 2147483647; // Disable SuperBlocks
    TestBlockSubsidyHalvings(consensusParams); // As in main
    TestBlockSubsidyHalvings(150); // As in regtest
    TestBlockSubsidyHalvings(1000); // Just another interval
}

BOOST_AUTO_TEST_CASE(subsidy_limit_test)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    CAmount nSum = 0;
    // No Subsidy for Blocks 0-576
    for (int nHeight(0) ; nHeight < 577 ; nHeight++) {
        CAmount nSubsidy(GetBlockSubsidy(nHeight, chainParams->GetConsensus()));
        nSum += nSubsidy;
        BOOST_CHECK(MoneyRange(nSum));
    }
    BOOST_CHECK_EQUAL(nSum, CAmount{0}); // At Block 576
    // Blocks 577-1151 with linearly increasing Subsidy
    for (int nHeight(577) ; nHeight < 1152 ; nHeight++) {
        CAmount nSubsidy(GetBlockSubsidy(nHeight, chainParams->GetConsensus()));
        nSum += nSubsidy;
        BOOST_CHECK(MoneyRange(nSum));
    }
    BOOST_CHECK_EQUAL(nSum, CAmount{1437499999744}); // At Block 1151
    // Starting from Block 1152, we have 39 cycles of 4032 Blocks until the first SuperBlock
    for (int nHeight(1152) ; nHeight < 158400 ; nHeight += 4032) {
        CAmount nSubsidy(GetBlockSubsidy(nHeight, chainParams->GetConsensus()));
        nSum += 4032*nSubsidy;
        BOOST_CHECK(MoneyRange(nSum));
        BOOST_CHECK_EQUAL(nSum, CAmount{1437499999744 + (nHeight - 1152 + 4032)*50*COIN});
    }
    BOOST_CHECK_EQUAL(nSum, CAmount{787677499999744}); // 1437499999744 + 39*4032*50*COIN at Block 158399
    // SuperBlocks now active
    for (int nHeight(158400) ; nHeight < 839808 ; nHeight += 4032) { // Go until first halving (stopping at 839808 after 169 cycles)
        CAmount nSubsidyNormal(GetBlockSubsidy(nHeight + 2592, chainParams->GetConsensus())), nSubsidySuperBlock(GetBlockSubsidy(nHeight + 2448, chainParams->GetConsensus())), nSubsidySuperBlockCompensation(GetBlockSubsidy(nHeight + 2304, chainParams->GetConsensus()));
        nSum += nSubsidySuperBlock + 287*nSubsidySuperBlockCompensation + 3744*nSubsidyNormal;
        BOOST_CHECK(MoneyRange(nSum));
        BOOST_CHECK_EQUAL(nSum, CAmount{787677499999744 + (nHeight - 158400 + 4032)/4032*20159999999904});
    }
    BOOST_CHECK_EQUAL(nSum, CAmount{4194717499983520}); // 787677499999744 + 169*(1389.33333333 + 287*45.33333333 + 3744*50)*COIN at Block 839807
    // Add Subsidies of Blocks 839808-839999
    for (int nHeight(839808) ; nHeight < 840000 ; nHeight++) {
        CAmount nSubsidy(GetBlockSubsidy(nHeight, chainParams->GetConsensus()));
        nSum += nSubsidy;
        BOOST_CHECK(MoneyRange(nSum));
    }
    BOOST_CHECK_EQUAL(nSum, CAmount{4195677499983520}); // 4194717499983520 + 192*50*COIN at Block 839999
    // Add Subsidies of Blocks 840000-1678655
    for (int nHeight(840000) ; nHeight < 1678656 ; nHeight += 4032) { // Go until second halving (stopping at 839808 after 208 cycles)
        CAmount nSubsidyNormal(GetBlockSubsidy(nHeight + 2400, chainParams->GetConsensus())), nSubsidySuperBlock(GetBlockSubsidy(nHeight + 2256, chainParams->GetConsensus())), nSubsidySuperBlockCompensation(GetBlockSubsidy(nHeight + 2112, chainParams->GetConsensus()));
        nSum += nSubsidySuperBlock + 287*nSubsidySuperBlockCompensation + 3744*nSubsidyNormal;
        BOOST_CHECK(MoneyRange(nSum));
        BOOST_CHECK_EQUAL(nSum, CAmount{4195677499983520 + (nHeight - 840000 + 4032)/4032*10079999999808});
    }
    BOOST_CHECK_EQUAL(nSum, CAmount{6292317499943584}); // 4195677499983520 + 208(694.66666666 + 287*22.66666666 + 3744*25)*COIN at Block 1678655
    // Todo: handle SuperBlock removal and blocks beyond 2nd halving
}

static bool ReturnFalse() { return false; }
static bool ReturnTrue() { return true; }

BOOST_AUTO_TEST_CASE(test_combiner_all)
{
    boost::signals2::signal<bool (), CombinerAll> Test;
    BOOST_CHECK(Test());
    Test.connect(&ReturnFalse);
    BOOST_CHECK(!Test());
    Test.connect(&ReturnTrue);
    BOOST_CHECK(!Test());
    Test.disconnect(&ReturnFalse);
    BOOST_CHECK(Test());
    Test.disconnect(&ReturnTrue);
    BOOST_CHECK(Test());
}
BOOST_AUTO_TEST_SUITE_END()
