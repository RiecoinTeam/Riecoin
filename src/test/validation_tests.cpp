// Copyright (c) 2014-2019 The Bitcoin Core developers
// Copyright (c) 2013-2020 The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <net.h>
#include <signet.h>
#include <validation.h>

#include <test/util/setup_common.h>

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
    Consensus::Params consensusParams(CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus());
    consensusParams.hasFairLaunch = false; // Disable Fair Launch Subsidies
    consensusParams.fork1Height = 2147483647; // Disable SuperBlocks
    TestBlockSubsidyHalvings(consensusParams); // As in main
    TestBlockSubsidyHalvings(150); // As in regtest
    TestBlockSubsidyHalvings(1000); // Just another interval
}

BOOST_AUTO_TEST_CASE(subsidy_limit_test)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
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
    // Add Subsidies of Blocks 840000-1481088
    for (int nHeight(840000) ; nHeight < 1481088 ; nHeight += 4032) { // Go until second fork (stopping at 1481088 after 159 cycles), fork is at 1482768
        CAmount nSubsidyNormal(GetBlockSubsidy(nHeight + 2400, chainParams->GetConsensus())), nSubsidySuperBlock(GetBlockSubsidy(nHeight + 2256, chainParams->GetConsensus())), nSubsidySuperBlockCompensation(GetBlockSubsidy(nHeight + 2112, chainParams->GetConsensus()));
        nSum += nSubsidySuperBlock + 287*nSubsidySuperBlockCompensation + 3744*nSubsidyNormal;
        BOOST_CHECK(MoneyRange(nSum));
        BOOST_CHECK_EQUAL(nSum, CAmount{4195677499983520 + (nHeight - 840000 + 4032)/4032*10079999999808});
    }
    BOOST_CHECK_EQUAL(nSum, CAmount{5798397499952992}); // 4195677499983520 + 159(694.66666666 + 287*22.66666666 + 3744*25)*COIN at Block 1481087
    nSum += 198911*GetBlockSubsidy(1679999, chainParams->GetConsensus());
    BOOST_CHECK_EQUAL(nSum, CAmount{6295674999952992}); // 5798397499952992 + 198911*25*COIN at Block 1679999
    for (int nHeight(1680000) ; nHeight < 6*840000 ; nHeight += 1000) { // Test several Halvings
        CAmount nSubsidy = GetBlockSubsidy(nHeight, chainParams->GetConsensus());
        BOOST_CHECK(nSubsidy <= 50*COIN);
        nSum += nSubsidy*1000;
        BOOST_CHECK(MoneyRange(nSum));
    }
    BOOST_CHECK_EQUAL(nSum, CAmount{8264424999952992}); // 6295674999952992 + 840000*(12.5 + 6.25 + 3.125 + 1.5625)*COIN at Block 5039999 (around 2038)
}

BOOST_AUTO_TEST_CASE(signet_parse_tests)
{
    ArgsManager signet_argsman;
    signet_argsman.ForceSetArg("-signetchallenge", "51"); // set challenge to OP_TRUE
    const auto signet_params = CreateChainParams(signet_argsman, CBaseChainParams::SIGNET);
    CBlock block;
    BOOST_CHECK(signet_params->GetConsensus().signet_challenge == std::vector<uint8_t>{OP_TRUE});
    CScript challenge{OP_TRUE};

    // empty block is invalid
    BOOST_CHECK(!SignetTxs::Create(block, challenge));
    BOOST_CHECK(!CheckSignetBlockSolution(block, signet_params->GetConsensus()));

    // no witness commitment
    CMutableTransaction cb;
    cb.vout.emplace_back(0, CScript{});
    block.vtx.push_back(MakeTransactionRef(cb));
    block.vtx.push_back(MakeTransactionRef(cb)); // Add dummy tx to excercise merkle root code
    BOOST_CHECK(!SignetTxs::Create(block, challenge));
    BOOST_CHECK(!CheckSignetBlockSolution(block, signet_params->GetConsensus()));

    // no header is treated valid
    std::vector<uint8_t> witness_commitment_section_141{0xaa, 0x21, 0xa9, 0xed};
    for (int i = 0; i < 32; ++i) {
        witness_commitment_section_141.push_back(0xff);
    }
    cb.vout.at(0).scriptPubKey = CScript{} << OP_RETURN << witness_commitment_section_141;
    block.vtx.at(0) = MakeTransactionRef(cb);
    BOOST_CHECK(SignetTxs::Create(block, challenge));
    BOOST_CHECK(CheckSignetBlockSolution(block, signet_params->GetConsensus()));

    // no data after header, valid
    std::vector<uint8_t> witness_commitment_section_325{0xec, 0xc7, 0xda, 0xa2};
    cb.vout.at(0).scriptPubKey = CScript{} << OP_RETURN << witness_commitment_section_141 << witness_commitment_section_325;
    block.vtx.at(0) = MakeTransactionRef(cb);
    BOOST_CHECK(SignetTxs::Create(block, challenge));
    BOOST_CHECK(CheckSignetBlockSolution(block, signet_params->GetConsensus()));

    // Premature end of data, invalid
    witness_commitment_section_325.push_back(0x01);
    witness_commitment_section_325.push_back(0x51);
    cb.vout.at(0).scriptPubKey = CScript{} << OP_RETURN << witness_commitment_section_141 << witness_commitment_section_325;
    block.vtx.at(0) = MakeTransactionRef(cb);
    BOOST_CHECK(!SignetTxs::Create(block, challenge));
    BOOST_CHECK(!CheckSignetBlockSolution(block, signet_params->GetConsensus()));

    // has data, valid
    witness_commitment_section_325.push_back(0x00);
    cb.vout.at(0).scriptPubKey = CScript{} << OP_RETURN << witness_commitment_section_141 << witness_commitment_section_325;
    block.vtx.at(0) = MakeTransactionRef(cb);
    BOOST_CHECK(SignetTxs::Create(block, challenge));
    BOOST_CHECK(CheckSignetBlockSolution(block, signet_params->GetConsensus()));

    // Extraneous data, invalid
    witness_commitment_section_325.push_back(0x00);
    cb.vout.at(0).scriptPubKey = CScript{} << OP_RETURN << witness_commitment_section_141 << witness_commitment_section_325;
    block.vtx.at(0) = MakeTransactionRef(cb);
    BOOST_CHECK(!SignetTxs::Create(block, challenge));
    BOOST_CHECK(!CheckSignetBlockSolution(block, signet_params->GetConsensus()));
}

BOOST_AUTO_TEST_SUITE_END()
