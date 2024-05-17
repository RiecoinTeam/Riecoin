// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Copyright (c) 2013-present The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <kernel/chainparams.h>

#include <chainparamsseeds.h>
#include <consensus/amount.h>
#include <consensus/merkle.h>
#include <consensus/params.h>
#include <hash.h>
#include <kernel/messagestartchars.h>
#include <logging.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <uint256.h>
#include <util/chaintype.h>
#include <util/strencodings.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <type_traits>

/** Build the genesis block. Note that the output of its generation transaction cannot be spent since it did not originally exist in the database. */
static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint64_t nTime, arith_uint256 nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
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
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/** Main network on which people trade goods and services. */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        m_chain_type = ChainType::MAIN;
        consensus.nSubsidyHalvingInterval = 840000;
        consensus.fork1Height = 157248;
        consensus.fork2Height = 1482768;
        consensus.MinBIP9WarningHeight = 1520064 + 4032; // Taproot activation height + miner confirmation window
        consensus.powAcceptedPatterns = {{0, 2, 4, 2, 4, 6, 2}, {0, 2, 6, 4, 2, 4, 2}}; // Prime septuplets, starting from fork2Height
        consensus.nBitsMin = 600*256; // Difficulty 600, starting from fork2Height
        consensus.nPowTargetSpacing = 150; // 2.5 min
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 3024; // 75%
        consensus.nMinerConfirmationWindow = 4032; // 7 days
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000a10894d0382f90c393ff96200000"); // 2144042
        consensus.defaultAssumeValid = uint256S("0x95e0d3078a9b75982b4967591e05b2886298f516220845118284a6a7fcd28be6"); // 2144042

        /** The message start string is designed to be unlikely to occur in normal data. The characters are rarely used upper ASCII, not valid as UTF-8, and produce a large 32-bit integer with any alignment. */
        pchMessageStart[0] = 0xfc;
        pchMessageStart[1] = 0xbc;
        pchMessageStart[2] = 0xb2;
        pchMessageStart[3] = 0xdb;
        nDefaultPort = 28333;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 3;
        m_assumed_chain_state_size = 1;

        const CScript genesisOutputScript(CScript() << ParseHex("04ff3c7ec6f2ed535b6d0d373aaff271c3e6a173cd2830fd224512dea3398d7b90a64173d9f112ec9fa8488eb56232f29f388f0aaf619bdd7ad786e731034eadf8") << OP_CHECKSIG);
        genesis = CreateGenesisBlock("The Times 10/Feb/2014 Thousands of bankers sacked since crisis", genesisOutputScript, 1392079741, UintToArith256(uint256S("0x0000000000000000000000000000000000000000000000000000000000000000")), 33632256, 1, 0);
        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.hashGenesisBlockForPoW = genesis.GetHashForPoW();
        assert(consensus.hashGenesisBlock == uint256S("0xe1ea18d0676ef9899fbc78ef428d1d26a2416d0f0441d46668d33bcb41275740"));
        assert(consensus.hashGenesisBlockForPoW == uint256S("0x26d0466d5a0eab0ebf171eacb98146b26143d143463514f26b28d3cded81c1bb"));
        assert(genesis.hashMerkleRoot == uint256S("0xd59afe19bb9e6126be90b2c8c18a8bee08c3c50ad3b3cca2b91c09683aa48118"));

        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as an addrfetch if they don't support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.
        // Todo: make/port Seeder for Riecoin and add Seeders here

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 60); // R
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 65); // R + 2 = T
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "ric"; // https://github.com/satoshilabs/slips/blob/master/slip-0173.md

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_main), std::end(chainparams_seed_main));

        fDefaultConsistencyChecks = false;
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
                {1486806, uint256S("0x0531ac83b4ec8ee5699fe8cbd591ffbdaf369187fb75227449bc640a9e19dd1a")},
                {1594496, uint256S("0x1d4e6dfe1ff598a0c69f5e81db9eaf8bbc1a9923b11c190da1ff4831850f496b")},
                {1921653, uint256S("0x1076d2f76cd20aedcd867b1d5ba058d90a55c74ce00dcac04c489ab64711a7f8")},
                {2122166, uint256S("0x973ba054bc7371c5e406c170b40c34f9d7c15fc81e5d286bed89bda2d8d58e12")},
                {2144042, uint256S("0x95e0d3078a9b75982b4967591e05b2886298f516220845118284a6a7fcd28be6")},
            }
        };

        m_assumeutxo_data = {
            {
                .height = 2100000,
                .hash_serialized = AssumeutxoHash{uint256S("0xae41c6b39cc38167843f7f2f3330f334313b1b987b7945d0b292450661646a62")},
                .nChainTx = 4282088,
                .blockhash = uint256S("0x0557f4bc11b810d1c398d61c66da3d213af4cb3b0573858d5ec9100892810d44")
            }
        };

        chainTxData = ChainTxData{
            // Data from RPC: getchaintxstats 65536 95e0d3078a9b75982b4967591e05b2886298f516220845118284a6a7fcd28be6
            .nTime    = 1715908327,
            .nTxCount = 4338668,
            .dTxRate  = 0.008264728090904366,
        };
    }
};

/** Testnet: public test network which is reset from time to time (lastly with 24.04). */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        m_chain_type = ChainType::TESTNET;
        consensus.nSubsidyHalvingInterval = 840000;
        consensus.fork1Height = 2147483647; // No SuperBlocks
        consensus.fork2Height = 0; // Start Chain already with Fork 2 Rules
        consensus.MinBIP9WarningHeight = 0;
        consensus.powAcceptedPatterns = {{0, 4, 2, 4, 2}, {0, 2, 4, 2, 4}}; // Prime quintuplets for TestNet
        consensus.nBitsMin = 512*256; // Difficulty 512
        consensus.nPowTargetSpacing = 300; // 5 min, 2x less blocks to download for TestNet
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75%
        consensus.nMinerConfirmationWindow = 2016;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        consensus.nMinimumChainWork = uint256S("0x00000000000000000000000000000000000000000001844f9fdaadaf10b04000"); // 27214
        consensus.defaultAssumeValid = uint256S("0x83af0a19444005fb05c1b51d08baff26467ab1911334f470ae386e60d7e6cbce"); // 27214

        pchMessageStart[0] = 0x0e;
        pchMessageStart[1] = 0x09;
        pchMessageStart[2] = 0x11;
        pchMessageStart[3] = 0x05;
        nDefaultPort = 38333;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 1;
        m_assumed_chain_state_size = 1;

        genesis = CreateGenesisBlock("Happy Birthday, Stella!", CScript(OP_RETURN), 1707684554, UintToArith256(uint256S("0x00000000000000000000000000000000000000000000002990adb3a701960002")), consensus.nBitsMin, 536870912, 50*COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.hashGenesisBlockForPoW = genesis.GetHashForPoW();
        assert(consensus.hashGenesisBlock == uint256S("0x753b93f5e3938f69d2b33c8c7572b019b12fa877e78581eebd65d349ca8645da"));
        assert(consensus.hashGenesisBlockForPoW == uint256S("0xd38d558bf81079c5c1662f6645dfa9856bcda0f54c93c5ca3788a59c7cfcc734"));
        assert(genesis.hashMerkleRoot == uint256S("0x495297a63256ff66e6bb810adc1660eee7a98eb55dbfeae8e25b1365b8bacca6"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        // Todo: make/port Seeder for Riecoin and add Seeders here

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,122); // r
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,127); // r + 2 = t
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tric"; // https://github.com/satoshilabs/slips/blob/master/slip-0173.md

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_test), std::end(chainparams_seed_test));

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        checkpointData = {
            {
                {546, uint256S("0xde0475d73da731abe2763210994fb17532589949bd9966e8e31c814d9f4242e1")},
            }
        };

        m_assumeutxo_data = {
            {
                .height = 16383,
                .hash_serialized = AssumeutxoHash{uint256S("0x128b4616c1bcbebebe2c6e877065afabfbb6a029eae751ffeacb313ec4602291")},
                .nChainTx = 16384,
                .blockhash = uint256S("0xe834f891c59fb5d63685d2fa91e20dc840e00d4079123cc3b78a6e30b68097a3")
            }
        };

        chainTxData = ChainTxData{
            // Data from RPC: getchaintxstats 16384 83af0a19444005fb05c1b51d08baff26467ab1911334f470ae386e60d7e6cbce
            .nTime    = 1715908063,
            .nTxCount = 27216,
            .dTxRate  = 0.003294648112077153,
        };
    }
};

/** Regression test: intended for private networks only. Has minimal difficulty to ensure that blocks can be found instantly. */
class CRegTestParams : public CChainParams
{
public:
    explicit CRegTestParams(const RegTestOptions& opts)
    {
        m_chain_type = ChainType::REGTEST;
        consensus.nSubsidyHalvingInterval = 150;
        consensus.fork1Height = 2147483647; // No SuperBlocks
        consensus.fork2Height = 0; // Start Chain already with Fork 2 Rules
        consensus.MinBIP9WarningHeight = 0;
        consensus.powAcceptedPatterns = {{0}}; // Just prime numbers for RegTest
        consensus.nBitsMin = 288*256; // 288
        consensus.nPowTargetSpacing = 150; // 2.5 min
        consensus.fPowNoRetargeting = true; // No Difficulty Adjustment
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 18444;
        nPruneAfterHeight = opts.fastprune ? 100 : 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        for (const auto& [deployment_pos, version_bits_params] : opts.version_bits_parameters) {
            consensus.vDeployments[deployment_pos].nStartTime = version_bits_params.start_time;
            consensus.vDeployments[deployment_pos].nTimeout = version_bits_params.timeout;
            consensus.vDeployments[deployment_pos].min_activation_height = version_bits_params.min_activation_height;
        }

        genesis = CreateGenesisBlock("Happy Birthday, Stella!", CScript(OP_RETURN), 1707684554, UintToArith256(uint256S("0x00000000000000000000000000000000000000000000000000000000001a0002")), consensus.nBitsMin, 536870912, 50*COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.hashGenesisBlockForPoW = genesis.GetHashForPoW();
        assert(consensus.hashGenesisBlock == uint256S("0x08982e71e300f2c7f5b967df5e9b40788942abd4bc62edaeabd27d351f953b68"));
        assert(consensus.hashGenesisBlockForPoW == uint256S("0xe450cfcfbf053cbba2c70088cbe95a5bb4133665126028dd916a553dbf49d94a"));
        assert(genesis.hashMerkleRoot == uint256S("0x495297a63256ff66e6bb810adc1660eee7a98eb55dbfeae8e25b1365b8bacca6"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();
        vSeeds.emplace_back("dummySeed.invalid.");

        fDefaultConsistencyChecks = true;
        m_is_mockable_chain = true;

        checkpointData = {
            {
                {0, uint256S("0x08982e71e300f2c7f5b967df5e9b40788942abd4bc62edaeabd27d351f953b68")},
            }
        };

        m_assumeutxo_data = {
            {
                .height = 110,
                .hash_serialized = AssumeutxoHash{uint256S("0x6657b736d4fe4db0cbc796789e812d5dba7f5c143764b1b6905612f1830609d1")},
                .nChainTx = 111,
                .blockhash = uint256S("0x91ed22a65c353d14bd238945e6ceefdcdb1193fef602dc61413a9c4c9b2bf998")
            },
            {
                // For use by test/functional/feature_assumeutxo.py
                .height = 299,
                .hash_serialized = AssumeutxoHash{uint256S("0x030663cfbd01e69df8bd572086b45c7e242212a6c36b3386bd39f3d40a8dfb3b")},
                .nChainTx = 334,
                .blockhash = uint256S("0x3e7998064a7c6cc4d980f5d1405d63566872ea2b23d1b1c9f068a4d3a98854bc")
            },
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,122); // r
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,127); // r + 2 = t
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "rric"; // https://github.com/satoshilabs/slips/blob/master/slip-0173.md
    }
};

std::unique_ptr<const CChainParams> CChainParams::RegTest(const RegTestOptions& options)
{
    return std::make_unique<const CRegTestParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::Main()
{
    return std::make_unique<const CMainParams>();
}

std::unique_ptr<const CChainParams> CChainParams::TestNet()
{
    return std::make_unique<const CTestNetParams>();
}
