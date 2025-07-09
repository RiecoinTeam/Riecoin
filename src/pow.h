// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Copyright (c) 2013-present The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POW_H
#define BITCOIN_POW_H

#include <consensus/params.h>
#include <gmp.h>
#include <gmpxx.h>

#include <cstdint>

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

#endif // BITCOIN_POW_H
