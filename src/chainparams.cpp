// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Copyright (c) 2013-2020 The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>

#include <chainparamsseeds.h>
#include <consensus/merkle.h>
#include <tinyformat.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <versionbitsinfo.h>

#include <assert.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint64_t nTime, arith_uint256 nOffset, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nOffset  = nOffset;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 */
static CBlock CreateGenesisBlock(uint64_t nTime, arith_uint256 nOffset, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "The Times 10/Feb/2014 Thousands of bankers sacked since crisis";
    const CScript genesisOutputScript = CScript() << ParseHex("04ff3c7ec6f2ed535b6d0d373aaff271c3e6a173cd2830fd224512dea3398d7b90a64173d9f112ec9fa8488eb56232f29f388f0aaf619bdd7ad786e731034eadf8") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nOffset, nBits, nVersion, genesisReward);
}

/**
 * Main network
 */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = CBaseChainParams::MAIN;
        consensus.hasFairLaunch = true;
        consensus.nSubsidyHalvingInterval = 840000;
        consensus.BIP34Height = 1; // Always active for Riecoin (except for the Genesis Block)
        consensus.BIP65Height = 1096704; // Activated with CSV
        consensus.BIP66Height = 1096704; // Activated with CSV
        consensus.CSVHeight = 1096704;
        consensus.SegwitHeight = 1096704; // Activated with CSV
        consensus.fork1Height = 157248;
        consensus.fork2Height = 2147483647; // TODO: set second Hard Fork height
        consensus.MinBIP9WarningHeight = 1100736; // SegwitHeight + miner confirmation window
        consensus.powAcceptedPatterns1 = {{0, 4, 2, 4, 2, 4}}; // Prime sextuplets, before fork2Height
        consensus.powAcceptedPatterns2 = {{0, 2, 4, 2, 4, 6, 2}, {0, 2, 6, 4, 2, 4, 2}}; // Prime septuplets, starting from fork2Height
        consensus.powLimit = ArithToUint256(304); // Primes of size 1 + 8 + 256 (hash) + 39 = 304 bits, before fork2Height
        consensus.powLimit2 = 600*256; // nBits value for Difficulty 600, starting from fork2Height
        consensus.nPowTargetTimespan = 12*3600; // 12 h
        consensus.nPowTargetSpacing = 150; // 2.5 min
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 3024; // 75%
        consensus.nMinerConfirmationWindow = 4032; // 7 days
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008
        // Bit 5 was used for the SegWit SoftFork, avoid "New Unknown Rules" messages
        consensus.vDeployments[Consensus::LOWER_THRESHOLD].bit = 5;
        consensus.vDeployments[Consensus::LOWER_THRESHOLD].nStartTime = 1546300800; // January 1st, 2019.
        consensus.vDeployments[Consensus::LOWER_THRESHOLD].nTimeout = 1577836800; // January 1st, 2020.



        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000eccc0e88f3963748afe16200000"); // 1330344

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0xb055f0cc42580d73d429105e92cdcb7157b8c7f68654eb9dc8a3794985ea379f"); // 1330344

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xfc;
        pchMessageStart[1] = 0xbc;
        pchMessageStart[2] = 0xb2;
        pchMessageStart[3] = 0xdb;
        nDefaultPort = 28333;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 320;
        m_assumed_chain_state_size = 4;

        genesis = CreateGenesisBlock(1392079741, 0, UintToArith256(consensus.powLimit).GetCompact(), 1, 0);
        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.hashGenesisBlockForPoW = genesis.GetHashForPoW();
        assert(consensus.hashGenesisBlock == uint256S("0xe1ea18d0676ef9899fbc78ef428d1d26a2416d0f0441d46668d33bcb41275740"));
        assert(consensus.hashGenesisBlockForPoW == uint256S("0x26d0466d5a0eab0ebf171eacb98146b26143d143463514f26b28d3cded81c1bb"));
        assert(genesis.hashMerkleRoot == uint256S("0xd59afe19bb9e6126be90b2c8c18a8bee08c3c50ad3b3cca2b91c09683aa48118"));

        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as a oneshot if they don't support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.
        // TODO: add seeds here

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 60); // R
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 65); // R + 2 = T
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "ric"; // https://github.com/satoshilabs/slips/blob/master/slip-0173.md

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        m_is_test_chain = false;
        m_is_mockable_chain = false;

        checkpointData = {
            {
                {  4000,  uint256S("0x1c0cbd04b20aa0df11ef7194d4117696a4d761f31882ee098432cffe222869f8")},
                { 33400,  uint256S("0x8d1f31eb883c1bee51f02335594b14f1cf79772eae42dc7e81e5fd569edff1cc")},
                { 50300,  uint256S("0x9640513f592d30940d4cf0d139c0106b46eb3f08d267043eae3e0cc6113aae19")},
                { 76499,  uint256S("0x4f1a629015a269b37c840c8450903bcac801fb99a0ae0d1d5ce86b2bcf8fd692")},
                { 150550, uint256S("0x373ca9ff9f0b68355bff755f78c5511d635be535a0ecf3f8f32b1ee7bcd07939")},
                { 931912, uint256S("0x4b6a2102c6c3e5ac094cecdedecc7ab1b6b26b05cef4bacda69f55643f114655")},
                {1330344, uint256S("0xb055f0cc42580d73d429105e92cdcb7157b8c7f68654eb9dc8a3794985ea379f")},
            }
        };

        chainTxData = ChainTxData{
            // Data from RPC: getchaintxstats 4096 4b6a2102c6c3e5ac094cecdedecc7ab1b6b26b05cef4bacda69f55643f114655
            /* nTime    */ 1594054226,
            /* nTxCount */ 2701660,
            /* dTxRate  */ 0.008215703391365934,
        };
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = CBaseChainParams::TESTNET;
        consensus.hasFairLaunch = false;
        consensus.nSubsidyHalvingInterval = 840000;
        consensus.BIP34Height = 1; // Always active for Riecoin (except for the Genesis Block)
        consensus.BIP65Height = 0; // Always active in TestNet
        consensus.BIP66Height = 0; // Always active in TestNet
        consensus.CSVHeight = 0; // Always active in TestNet
        consensus.SegwitHeight = 0;  // Always active in TestNet
        consensus.fork1Height = 4032;
        consensus.fork2Height = 2147483647; // TODO: set second Hard Fork height
        consensus.MinBIP9WarningHeight = 0;
        consensus.powAcceptedPatterns1 = {{0, 2, 4, 2}}; // Prime quadruplets, before fork2Height
        consensus.powAcceptedPatterns2 = {{0, 4, 2, 4, 2}, {0, 2, 4, 2, 4}}; // Prime quintuplets, starting from fork2Height
        consensus.powLimit = ArithToUint256(600); // Before fork2Height
        consensus.powLimit2 = 600*256; // nBits value for Difficulty 600, starting from fork2Height
        consensus.nPowTargetTimespan = 12*3600; // 12 h
        consensus.nPowTargetSpacing = 150; // 2.5 min
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 216; // 75 %
        consensus.nMinerConfirmationWindow = 288; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000001718f2c9fe3041b9100");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x3d2ccbefa91ec142c64c7fd076e1393d2ff6cd90d51d53d1b9ffcb8fea43f83c"); // Block 6000

        pchMessageStart[0] = 0x0d;
        pchMessageStart[1] = 0x09;
        pchMessageStart[2] = 0x11;
        pchMessageStart[3] = 0x05;
        nDefaultPort = 38333;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 40;
        m_assumed_chain_state_size = 2;

        genesis = CreateGenesisBlock(1593561600, 0, UintToArith256(consensus.powLimit).GetCompact(), 1, 50*COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.hashGenesisBlockForPoW = genesis.GetHashForPoW();
        assert(consensus.hashGenesisBlock == uint256S("0xa437561f7e97ee05f336c12b36900ccf3ef19851a08805c1452d69a4efcbe701"));
        assert(consensus.hashGenesisBlockForPoW == uint256S("0xd325c614094bec9a6901d31bd84f3928e21c2f104a166aa56717f4bf38fb9e60"));
        assert(genesis.hashMerkleRoot == uint256S("0x86fb307a0d0caaae0baa4bff6beb3209d848f263bc6bbed5f12f6071a0e747a1"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        // TODO: add seeds here

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,122); // r
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,127); // r + 2 = t
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tric"; // https://github.com/satoshilabs/slips/blob/master/slip-0173.md

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        m_is_test_chain = true;
        m_is_mockable_chain = false;

        checkpointData = {
            {
                {6000, uint256S("0x3d2ccbefa91ec142c64c7fd076e1393d2ff6cd90d51d53d1b9ffcb8fea43f83c")},
            }
        };

        chainTxData = ChainTxData{
            // Data from RPC: getchaintxstats 4096 3d2ccbefa91ec142c64c7fd076e1393d2ff6cd90d51d53d1b9ffcb8fea43f83c
            /* nTime    */ 1594461600,
            /* nTxCount */ 6011,
            /* dTxRate  */ 0.006106790620064956,
        };
    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    explicit CRegTestParams(const ArgsManager& args) {
        strNetworkID =  CBaseChainParams::REGTEST;
        consensus.hasFairLaunch = false;
        consensus.nSubsidyHalvingInterval = 150;
        consensus.BIP34Height = 500; // BIP34 activated on regtest (Used in functional tests)
        consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in functional tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in functional tests)
        consensus.CSVHeight = 432; // CSV activated on regtest (Used in rpc activation tests)
        consensus.SegwitHeight = 0; // SEGWIT is always activated on regtest unless overridden
        consensus.fork1Height  = 2147483647; // No SuperBlocks
        consensus.fork2Height  = 2147483647; // Use original PoW/consensus, though it would be nice to rewrite the tests for the current ones in future versions
        consensus.MinBIP9WarningHeight = 0;
        consensus.powAcceptedPatterns1 = {{0}}; // Just prime numbers for RegTest
        consensus.powAcceptedPatterns2 = {{0}};
        consensus.powLimit = ArithToUint256(304);
        consensus.powLimit2 = 304*256;
        consensus.nPowTargetTimespan = 12*3600; // 12 h
        consensus.nPowTargetSpacing = 150; // 2.5 min
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 38444;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        UpdateActivationParametersFromArgs(args);

        genesis = CreateGenesisBlock(1577836800, 0, UintToArith256(consensus.powLimit).GetCompact(), 1, 50*COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.hashGenesisBlockForPoW = genesis.GetHashForPoW();
        assert(consensus.hashGenesisBlock == uint256S("0xcc673f6ea26e6477ab71b67c47149a4206b2098be8612f7e8357aeb1523ee01b"));
        assert(consensus.hashGenesisBlockForPoW == uint256S("0x78d63e39b5722379645e232a957eafcfa3d548e1aa147054cee225584012b873"));
        assert(genesis.hashMerkleRoot == uint256S("0x86fb307a0d0caaae0baa4bff6beb3209d848f263bc6bbed5f12f6071a0e747a1"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = true;
        m_is_test_chain = true;
        m_is_mockable_chain = true;

        checkpointData = {
            {
                {0, uint256S("0xcc673f6ea26e6477ab71b67c47149a4206b2098be8612f7e8357aeb1523ee01b")},
            }
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,122); // r
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,127); // r + 2 = t
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "rric"; // https://github.com/satoshilabs/slips/blob/master/slip-0173.md
    }

    /**
     * Allows modifying the Version Bits regtest parameters.
     */
    void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
    {
        consensus.vDeployments[d].nStartTime = nStartTime;
        consensus.vDeployments[d].nTimeout = nTimeout;
    }
    void UpdateActivationParametersFromArgs(const ArgsManager& args);
};

void CRegTestParams::UpdateActivationParametersFromArgs(const ArgsManager& args)
{
    if (gArgs.IsArgSet("-segwitheight")) {
        int64_t height = gArgs.GetArg("-segwitheight", consensus.SegwitHeight);
        if (height < -1 || height >= std::numeric_limits<int>::max()) {
            throw std::runtime_error(strprintf("Activation height %ld for segwit is out of valid range. Use -1 to disable segwit.", height));
        } else if (height == -1) {
            LogPrintf("Segwit disabled for testing\n");
            height = std::numeric_limits<int>::max();
        }
        consensus.SegwitHeight = static_cast<int>(height);
    }

    if (!args.IsArgSet("-vbparams")) return;

    for (const std::string& strDeployment : args.GetArgs("-vbparams")) {
        std::vector<std::string> vDeploymentParams;
        boost::split(vDeploymentParams, strDeployment, boost::is_any_of(":"));
        if (vDeploymentParams.size() != 3) {
            throw std::runtime_error("Version bits parameters malformed, expecting deployment:start:end");
        }
        int64_t nStartTime, nTimeout;
        if (!ParseInt64(vDeploymentParams[1], &nStartTime)) {
            throw std::runtime_error(strprintf("Invalid nStartTime (%s)", vDeploymentParams[1]));
        }
        if (!ParseInt64(vDeploymentParams[2], &nTimeout)) {
            throw std::runtime_error(strprintf("Invalid nTimeout (%s)", vDeploymentParams[2]));
        }
        bool found = false;
        for (int j=0; j < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; ++j) {
            if (vDeploymentParams[0] == VersionBitsDeploymentInfo[j].name) {
                UpdateVersionBitsParameters(Consensus::DeploymentPos(j), nStartTime, nTimeout);
                found = true;
                LogPrintf("Setting version bits activation parameters for %s to start=%ld, timeout=%ld\n", vDeploymentParams[0], nStartTime, nTimeout);
                break;
            }
        }
        if (!found) {
            throw std::runtime_error(strprintf("Invalid deployment (%s)", vDeploymentParams[0]));
        }
    }
}

static std::unique_ptr<const CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<const CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams(gArgs));
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}
