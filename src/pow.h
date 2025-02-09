// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2013-present The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POW_H
#define BITCOIN_POW_H

#include <consensus/params.h>
#include <gmp.h>
#include <gmpxx.h>

#include <stdint.h>

class CBlockHeader;
class CBlockIndex;
class uint256;
class arith_uint256;

/**
 * Convert nBits value to target.
 *
 * @param[in] hash       hash the target depends on
 * @param[in] nBits      integer representation of the target
 * @param[in] powVersion PoW Version to use for the derivation
 * @param[in] nBitsMin   PoW limit (consensus parameter)
 *
 * @return               the proof-of-work target or nullopt if nBits or powVersion is invalid
 */
std::optional<mpz_class> DeriveTarget(uint256 hash, unsigned int nBits, const int32_t powVersion, const uint32_t nBitsMin);

// MainNet Only, Pre Fork 2 SuperBlocks
inline bool isInSuperblockInterval(int nHeight, const Consensus::Params& params) {return ((nHeight/288) % 14) == 12;} // once per week
inline bool isSuperblock(int nHeight, const Consensus::Params& params) {return ((nHeight % 288) == 144) && isInSuperblockInterval(nHeight, params);}

uint32_t GenerateTarget(mpz_class &gmpTarget, uint256 hash, uint32_t compactBits, const int32_t powVersion);
unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const Consensus::Params&);
unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params&);

extern const std::vector<uint64_t> primeTable;
/** Check whether a Nonce satisfies the proof-of-work requirement */
bool CheckProofOfWork(uint256 hash, unsigned int nBits, uint256 nNonce, const Consensus::Params&);
bool CheckProofOfWorkImpl(uint256 hash, unsigned int nBits, uint256 nNonce, const Consensus::Params&);

/**
 * Return false if the proof-of-work requirement specified by new_nbits at a
 * given height is not possible, given the proof-of-work on the prior block as
 * specified by old_nbits.
 *
 * This function only checks that the new value is within a factor of 4 of the
 * old value for blocks at the difficulty adjustment interval, and otherwise
 * requires the values to be the same.
 *
 * Always returns true on networks where min difficulty blocks are allowed,
 * such as regtest/testnet.
 */
bool PermittedDifficultyTransition(const Consensus::Params& params, int64_t height, uint32_t old_nbits, uint32_t new_nbits);

#endif // BITCOIN_POW_H
