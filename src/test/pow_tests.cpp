// Copyright (c) 2015-2019 The Bitcoin Core developers
// Copyright (c) 2013-2021 The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <pow.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(pow_tests, BasicTestingSetup)

/* Test calculation of next difficulty target with no constraints applying */
BOOST_AUTO_TEST_CASE(get_next_work)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    int64_t nLastRetargetTime = 1435639430; // Block #287712
    CBlockIndex pindexLast;
    pindexLast.nHeight = 287999;
    pindexLast.nTime = 1435676461;  // Block #287999
    pindexLast.nBits = 0x0205d900; // 1497
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x0205f200); // 1522
}

/* Test the constraint on the upper bound for next work */
BOOST_AUTO_TEST_CASE(get_next_work_pow_limit)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    int64_t nLastRetargetTime = 1577836800;
    CBlockIndex pindexLast;
    pindexLast.nHeight = 287;
    pindexLast.nTime = nLastRetargetTime + 288*150*2;
    pindexLast.nBits = 0x02013000; // 304
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x02013000); // 304
}

/* Test the constraint on the lower bound for actual time taken */
BOOST_AUTO_TEST_CASE(get_next_work_lower_limit_actual)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    int64_t nLastRetargetTime = 1577836800;
    CBlockIndex pindexLast;
    pindexLast.nHeight = 1151; // Note that the bound is not applied for the 3 first adjustments
    pindexLast.nTime = nLastRetargetTime + 288*150/5; // Limited to >= 288*150/4
    pindexLast.nBits = 0x02064000; // 1600
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x02074a00); // 1866
}

/* Test the constraint on the upper bound for actual time taken */
BOOST_AUTO_TEST_CASE(get_next_work_upper_limit_actual)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    int64_t nLastRetargetTime = 1577836800;
    CBlockIndex pindexLast;
    pindexLast.nHeight = 1151; // Note that the bound is not applied for the 3 first adjustments
    pindexLast.nTime = nLastRetargetTime + 5*288*150; // Limited to <= 4*288*150
    pindexLast.nBits = 0x02064000; // 1600
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x02055b00); // 1371
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_negative_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::REGTEST)->GetConsensus();
    uint256 hash, offset;
    unsigned int nBits(arith_uint256{304}.GetCompact(true));
    hash.SetHex("0x0");
    offset.SetHex("0x65"); // 2^303 + 101 is prime
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, offset, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_overflow_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::REGTEST)->GetConsensus();
    uint256 hash, offset;
    unsigned int nBits = ~0x00800000;
    hash.SetHex("0x0");
    offset.SetHex("0xaf"); // 2^264 + 175 is prime
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, offset, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_too_easy_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::REGTEST)->GetConsensus();
    uint256 hash, offset;
    unsigned int nBits(33632000); // 303
    hash.SetHex("0x0");
    offset.SetHex("0x133"); // 2^302 + 307 is prime
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, offset, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_biger_hash_than_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    uint256 hash, offset;
    unsigned int nBits(33632256); // 304
    hash.SetHex("0x0");
    offset.SetHex("0x0b770d4f166f50f63d6001df19f113cf68f79133439a90dc59c99b22a69dd8c3"); // 2^303 + offset is a prime sextuplet, but offset >= 2^39
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, offset, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_zero_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::REGTEST)->GetConsensus();
    uint256 hash, offset;
    unsigned int nBits;
    arith_uint256 hash_arith{0};
    nBits = hash_arith.GetCompact();
    hash = ArithToUint256(hash_arith);
    offset.SetHex("0xaf"); // 2^264 + 175 is prime
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, offset, consensus));
}

BOOST_AUTO_TEST_CASE(GetBlockProofEquivalentTime_test)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    std::vector<CBlockIndex> blocks(10000);
    for (int i = 0; i < 10000; i++) {
        blocks[i].pprev = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight = i;
        blocks[i].nTime = 1577836800 + i * chainParams->GetConsensus().nPowTargetSpacing;
        blocks[i].nBits = 0x02013000;
        blocks[i].nChainWork = i ? blocks[i - 1].nChainWork + GetBlockProof(blocks[i - 1]) : arith_uint256(0);
    }

    for (int j = 0; j < 1000; j++) {
        CBlockIndex *p1 = &blocks[InsecureRandRange(10000)];
        CBlockIndex *p2 = &blocks[InsecureRandRange(10000)];
        CBlockIndex *p3 = &blocks[InsecureRandRange(10000)];

        int64_t tdiff = GetBlockProofEquivalentTime(*p1, *p2, *p3, chainParams->GetConsensus());
        BOOST_CHECK_EQUAL(tdiff, p1->GetBlockTime() - p2->GetBlockTime());
    }
}

void sanity_check_chainparams(const ArgsManager& args, std::string chainName)
{
    const auto chainParams = CreateChainParams(args, chainName);
    const auto consensus = chainParams->GetConsensus();

    // hash genesis is correct
    BOOST_CHECK_EQUAL(consensus.hashGenesisBlock, chainParams->GenesisBlock().GetHash());

    // target timespan is an even multiple of spacing
    BOOST_CHECK_EQUAL(consensus.nPowTargetTimespan % consensus.nPowTargetSpacing, 0);
}

BOOST_AUTO_TEST_CASE(ChainParams_MAIN_sanity)
{
    sanity_check_chainparams(*m_node.args, CBaseChainParams::MAIN);
}

BOOST_AUTO_TEST_CASE(ChainParams_REGTEST_sanity)
{
    sanity_check_chainparams(*m_node.args, CBaseChainParams::REGTEST);
}

BOOST_AUTO_TEST_CASE(ChainParams_TESTNET_sanity)
{
    sanity_check_chainparams(*m_node.args, CBaseChainParams::TESTNET);
}

BOOST_AUTO_TEST_CASE(ChainParams_SIGNET_sanity)
{
    sanity_check_chainparams(*m_node.args, CBaseChainParams::SIGNET);
}

BOOST_AUTO_TEST_SUITE_END()
