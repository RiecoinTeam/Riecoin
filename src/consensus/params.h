// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Copyright (c) 2013-present The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include <uint256.h>

#include <chrono>
#include <limits>
#include <map>
#include <vector>

namespace Consensus {

enum DeploymentPos : uint16_t {
    DEPLOYMENT_TESTDUMMY,
    // NOTE: Also add new deployments to VersionBitsDeploymentInfo in deploymentinfo.cpp
    MAX_VERSION_BITS_DEPLOYMENTS
};
constexpr bool ValidDeployment(DeploymentPos dep) { return dep < MAX_VERSION_BITS_DEPLOYMENTS; }

/** Struct for each individual consensus rule change using BIP9. */
struct BIP9Deployment {
    /** Bit position to select the particular bit in nVersion. */
    int bit{28};
    /** Start MedianTime for version bits miner confirmation. Can be a date in the past */
    int64_t nStartTime{NEVER_ACTIVE};
    /** Timeout/expiry MedianTime for the deployment attempt. */
    int64_t nTimeout{NEVER_ACTIVE};
    /** If lock in occurs, delay activation until at least this block
     *  height.  Note that activation will only occur on a retarget
     *  boundary.
     */
    int min_activation_height{0};

    /** Constant for nTimeout very far in the future. */
    static constexpr int64_t NO_TIMEOUT = std::numeric_limits<int64_t>::max();

    /** Special value for nStartTime indicating that the deployment is always active.
     *  This is useful for testing, as it means tests don't need to deal with the activation
     *  process (which takes at least 3 BIP9 intervals). Only tests that specifically test the
     *  behaviour during activation cannot use this. */
    static constexpr int64_t ALWAYS_ACTIVE = -1;

    /** Special value for nStartTime indicating that the deployment is never active.
     *  This is useful for integrating the code changes for a new feature
     *  prior to deploying it on some or all networks. */
    static constexpr int64_t NEVER_ACTIVE = -2;
};

/** Parameters that influence chain consensus. */
struct Params {
    uint256 hashGenesisBlock;
    uint256 hashGenesisBlockForPoW;
    int nSubsidyHalvingInterval;
    /** Don't warn about unknown BIP 9 activations below this height. This prevents us from warning about the CSV and segwit activations. */
    int MinBIP9WarningHeight;
    int fork1Height;
    int fork2Height;
    /** Window and Threshold for BIP9 deployments. */
    uint32_t nRuleChangeActivationThreshold;
    uint32_t nMinerConfirmationWindow;
    BIP9Deployment vDeployments[MAX_VERSION_BITS_DEPLOYMENTS];
    /** Proof of work parameters */
    int32_t GetPoWVersionAtHeight(int32_t height) const {return height < fork2Height ? -1 : 1;}
    std::vector<std::vector<int32_t>> powAcceptedPatterns;
    std::vector<std::vector<int32_t>> GetPowAcceptedPatternsAtHeight(int height) const {return height >= fork2Height ? powAcceptedPatterns : std::vector<std::vector<int32_t>>{{0, 4, 2, 4, 2, 4}};} // MainNet Only: Prime Sextuplets prior Fork 2
    uint32_t nBitsMin;
    bool fPowNoRetargeting;
    int64_t nPowTargetSpacing;
    std::chrono::seconds PowTargetSpacing() const {return std::chrono::seconds{nPowTargetSpacing};}
    /** The best chain should have at least this much work */
    uint256 nMinimumChainWork;
};

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
