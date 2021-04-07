// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2013-2021 The Riecoin developers
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

bool isInSuperblockInterval(int nHeight, const Consensus::Params& params);
bool isSuperblock(int nHeight, const Consensus::Params& params);

uint32_t GenerateTarget(mpz_class &gmpTarget, uint256 hash, uint32_t compactBits, const int32_t powVersion);
unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params&);
unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params&);

extern const std::vector<uint64_t> primeTable;
/** Check whether an offset satisfies the proof-of-work requirement */
bool CheckProofOfWork(uint256 hash, unsigned int nBits, uint256 nOffset, const Consensus::Params&);

#endif // BITCOIN_POW_H
