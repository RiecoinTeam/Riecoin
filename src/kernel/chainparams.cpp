// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Copyright (c) 2013-present The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <kernel/chainparams.h>

#include <chainparamsseeds.h>
#include <consensus/amount.h>
#include <consensus/merkle.h>
#include <consensus/params.h>
#include <hash.h>
#include <kernel/checkpointdata.h>
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

using namespace util::hex_literals;

/** Build the genesis block. Note that the output of its generation transaction cannot be spent since it did not originally exist in the database. */
static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint64_t nTime, arith_uint256 nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.version = 1;
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

        consensus.nMinimumChainWork = uint256{"000000000000000000000000000000000000d3b6857eeeada253b65f96200000"}; // 2325905

        /** The message start string is designed to be unlikely to occur in normal data. The characters are rarely used upper ASCII, not valid as UTF-8, and produce a large 32-bit integer with any alignment. */
        pchMessageStart[0] = 0xfc;
        pchMessageStart[1] = 0xbc;
        pchMessageStart[2] = 0xb2;
        pchMessageStart[3] = 0xdb;
        nDefaultPort = 28333;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 3;
        m_assumed_chain_state_size = 1;

        const CScript genesisOutputScript(CScript() << "04ff3c7ec6f2ed535b6d0d373aaff271c3e6a173cd2830fd224512dea3398d7b90a64173d9f112ec9fa8488eb56232f29f388f0aaf619bdd7ad786e731034eadf8"_hex << OP_CHECKSIG);
        genesis = CreateGenesisBlock("The Times 10/Feb/2014 Thousands of bankers sacked since crisis", genesisOutputScript, 1392079741, UintToArith256(uint256{"0000000000000000000000000000000000000000000000000000000000000000"}), 33632256, 1, 0);
        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.hashGenesisBlockForPoW = genesis.GetHashForPoW();
        assert(consensus.hashGenesisBlock == uint256{"e1ea18d0676ef9899fbc78ef428d1d26a2416d0f0441d46668d33bcb41275740"});
        assert(consensus.hashGenesisBlockForPoW == uint256{"26d0466d5a0eab0ebf171eacb98146b26143d143463514f26b28d3cded81c1bb"});
        assert(genesis.hashMerkleRoot == uint256{"d59afe19bb9e6126be90b2c8c18a8bee08c3c50ad3b3cca2b91c09683aa48118"});

        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as an addrfetch if they don't support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.
        // Todo: make/port Seeder for Riecoin and add Seeders here

        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "ric"; // https://github.com/satoshilabs/slips/blob/master/slip-0173.md

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_main), std::end(chainparams_seed_main));

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        checkpointData = mainCheckpointData;

        m_assumeutxo_data = {
            {
                .height = 2322000,
                .hash_serialized = AssumeutxoHash{uint256{"09c01ba89fe47314da250b7dccaeaee90a64b7303cf62d3f43e505677be5585b"}},
                .m_chain_tx_count = 4566965,
                .blockhash = uint256{"40a3c0aaa790165a9b3d8764e867aec7a0b817d4edfd1a248a04242d0506a7ee"}
            }
        };

        chainTxData = ChainTxData{
            // Data from RPC: getchaintxstats 65536 40a3c0aaa790165a9b3d8764e867aec7a0b817d4edfd1a248a04242d0506a7ee
            .nTime    = 1742504513,
            .tx_count = 4566965,
            .dTxRate  = 0.008461742683110736,
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

        consensus.nMinimumChainWork = uint256{"000000000000000000000000000000000000000000075f8e74698244438e6000"}; // 115939

        pchMessageStart[0] = 0x0e;
        pchMessageStart[1] = 0x09;
        pchMessageStart[2] = 0x11;
        pchMessageStart[3] = 0x05;
        nDefaultPort = 38333;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 1;
        m_assumed_chain_state_size = 1;

        genesis = CreateGenesisBlock("Happy Birthday, Stella!", CScript(OP_RETURN), 1707684554, UintToArith256(uint256{"00000000000000000000000000000000000000000000002990adb3a701960002"}), consensus.nBitsMin, 536870912, 50*COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.hashGenesisBlockForPoW = genesis.GetHashForPoW();
        assert(consensus.hashGenesisBlock == uint256{"753b93f5e3938f69d2b33c8c7572b019b12fa877e78581eebd65d349ca8645da"});
        assert(consensus.hashGenesisBlockForPoW == uint256{"d38d558bf81079c5c1662f6645dfa9856bcda0f54c93c5ca3788a59c7cfcc734"});
        assert(genesis.hashMerkleRoot == uint256{"495297a63256ff66e6bb810adc1660eee7a98eb55dbfeae8e25b1365b8bacca6"});

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        // Todo: make/port Seeder for Riecoin and add Seeders here

        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tric"; // https://github.com/satoshilabs/slips/blob/master/slip-0173.md

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_test), std::end(chainparams_seed_test));

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        checkpointData = testCheckpointData;

        m_assumeutxo_data = {
            {
                .height = 112000,
                .hash_serialized = AssumeutxoHash{uint256{"5c21e6d9c581a15a61fdeb905ddfa5ad13d93335dc08e23dbce589c23210df22"}},
                .m_chain_tx_count = 112014,
                .blockhash = uint256{"2e8b0c351e6ed2019ebdf567f7396ec034d721029b945a8e9659c721e06a7772"}
            }
        };

        chainTxData = ChainTxData{
            // Data from RPC: getchaintxstats 16384 2e8b0c351e6ed2019ebdf567f7396ec034d721029b945a8e9659c721e06a7772
            .nTime    = 1741902653,
            .tx_count = 112014,
            .dTxRate  = 0.003345395783715304,
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

        genesis = CreateGenesisBlock("Happy Birthday, Stella!", CScript(OP_RETURN), 1707684554, UintToArith256(uint256{"00000000000000000000000000000000000000000000000000000000001a0002"}), consensus.nBitsMin, 536870912, 50*COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.hashGenesisBlockForPoW = genesis.GetHashForPoW();
        assert(consensus.hashGenesisBlock == uint256{"08982e71e300f2c7f5b967df5e9b40788942abd4bc62edaeabd27d351f953b68"});
        assert(consensus.hashGenesisBlockForPoW == uint256{"e450cfcfbf053cbba2c70088cbe95a5bb4133665126028dd916a553dbf49d94a"});
        assert(genesis.hashMerkleRoot == uint256{"495297a63256ff66e6bb810adc1660eee7a98eb55dbfeae8e25b1365b8bacca6"});

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();
        vSeeds.emplace_back("dummySeed.invalid.");

        fDefaultConsistencyChecks = true;
        m_is_mockable_chain = true;

        checkpointData = {
            {},
            uint256{}, 0 // Assumed Valid Block
        };

        m_assumeutxo_data = {
            {   // For use by unit tests
                .height = 110,
                .hash_serialized = AssumeutxoHash{uint256{"6657b736d4fe4db0cbc796789e812d5dba7f5c143764b1b6905612f1830609d1"}},
                .m_chain_tx_count = 111,
                .blockhash = uint256{"91ed22a65c353d14bd238945e6ceefdcdb1193fef602dc61413a9c4c9b2bf998"}
            },
            {
                // For use by fuzz target src/test/fuzz/utxo_snapshot.cpp
                .height = 200,
                .hash_serialized = AssumeutxoHash{uint256{"7e3b7780fbd2fa479a01f66950dc8f728dc1b11f03d06d5bf223168520df3a48"}},
                .m_chain_tx_count = 201,
                .blockhash = uint256{"5e93653318f294fb5aa339d00bbf8cf1c3515488ad99412c37608b139ea63b27"}
            },
            {
                // For use by test/functional/feature_assumeutxo.py
                .height = 299,
                .hash_serialized = AssumeutxoHash{uint256{"030663cfbd01e69df8bd572086b45c7e242212a6c36b3386bd39f3d40a8dfb3b"}},
                .m_chain_tx_count = 334,
                .blockhash = uint256{"3e7998064a7c6cc4d980f5d1405d63566872ea2b23d1b1c9f068a4d3a98854bc"}
            },
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

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

std::vector<int> CChainParams::GetAvailableSnapshotHeights() const
{
    std::vector<int> heights;
    heights.reserve(m_assumeutxo_data.size());

    for (const auto& data : m_assumeutxo_data) {
        heights.emplace_back(data.height);
    }
    return heights;
}

std::optional<ChainType> GetNetworkForMagic(const MessageStartChars& message)
{
    const auto mainnet_msg = CChainParams::Main()->MessageStart();
    const auto testnet_msg = CChainParams::TestNet()->MessageStart();
    const auto regtest_msg = CChainParams::RegTest({})->MessageStart();

    if (std::ranges::equal(message, mainnet_msg)) {
        return ChainType::MAIN;
    } else if (std::ranges::equal(message, testnet_msg)) {
        return ChainType::TESTNET;
    } else if (std::ranges::equal(message, regtest_msg)) {
        return ChainType::REGTEST;
    }
    return std::nullopt;
}
