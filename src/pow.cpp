// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2013-2020 The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>
#include <util/system.h> // Logging

bool isInSuperblockInterval(int nHeight, const Consensus::Params& params)
{
    return ((nHeight/params.DifficultyAdjustmentInterval()) % 14) == 12; // once per week
}

bool isSuperblock(int nHeight, const Consensus::Params& params)
{
    return ((nHeight % params.DifficultyAdjustmentInterval()) == 144) && isInSuperblockInterval(nHeight, params);
}

mpz_class GetDifficulty(uint32_t nBits)
{
    arith_uint256 difficultyAu256;
    difficultyAu256.SetCompact(nBits);
    mpz_class difficulty;
    mpz_import(difficulty.get_mpz_t(), 8, -1, sizeof(uint32_t), 0, 0, ArithToUint256(difficultyAu256).begin());
    return difficulty;
}

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    {
        if (pindexLast->nHeight + 1 >= params.fork1Height && pindexLast->nHeight + 1 < params.fork2Height) // Superblocks
        {
            if (isSuperblock(pindexLast->nHeight + 1, params))
            {
                arith_uint256 newDifficulty;
                newDifficulty.SetCompact(pindexLast->nBits);
                newDifficulty *= 95859; // superblock is 4168/136 times more difficult
                newDifficulty >>= 16;   // 95859/65536 ~= (4168/136)^1/9
                return newDifficulty.GetCompact();
            }
            else if (isSuperblock(pindexLast->nHeight, params)) // Right after superblock, go back to previous diff
                return pindexLast->pprev->nBits;
        }

        if (params.fPowAllowMinDifficultyBlocks) // Testnet
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 4*2.5 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*4)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be nTargetTimespan worth of blocks
    int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval()-1);
    assert(nHeightFirst >= 0);
    if (nHeightFirst == 0)
        nHeightFirst++;
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (pindexLast->nHeight + 1 >= params.DifficultyAdjustmentInterval()*2) { // But not during the very first blocks
        if (nActualTimespan < params.nPowTargetTimespan/4)
            nActualTimespan = params.nPowTargetTimespan/4;
        if (nActualTimespan > params.nPowTargetTimespan*4)
            nActualTimespan = params.nPowTargetTimespan*4;
    }

    // Retarget
    mpz_class minDifficulty, difficulty, newLinDifficulty, newDifficulty;
    mpz_import(minDifficulty.get_mpz_t(), 8, -1, sizeof(uint32_t), 0, 0, params.powLimit.begin());
    arith_uint256 difficultyU256;
    difficultyU256.SetCompact(pindexLast->nBits);
    mpz_import(difficulty.get_mpz_t(), 8, -1, sizeof(uint32_t), 0, 0, ArithToUint256(difficultyU256).begin());

    // Approximately linearize difficulty by raising to the power 3 + Constellation Size
    mpz_pow_ui(newLinDifficulty.get_mpz_t(), difficulty.get_mpz_t(), 3 + params.powAcceptedConstellations1[0].size());
    newLinDifficulty *= (uint32_t) params.nPowTargetTimespan; // Gmp does not support 64 bits in some operating systems :| (compiler "use of overloaded operator is ambiguous" errors)
    newLinDifficulty /= (uint32_t) nActualTimespan;

    if (pindexLast->nHeight + 1 >= params.fork1Height && pindexLast->nHeight + 1 < params.fork2Height)
    {
        if (isInSuperblockInterval(pindexLast->nHeight + 1, params)) // Once per week, our interval contains a superblock
        { // *136/150 to compensate for difficult superblock
            newLinDifficulty *= 68;
            newLinDifficulty /= 75;
        }
        else if (isInSuperblockInterval(pindexLast->nHeight, params))
        { // *150/136 to compensate for previous adjustment
            newLinDifficulty *= 75;
            newLinDifficulty /= 68;
        }
    }

    mpz_root(newDifficulty.get_mpz_t(), newLinDifficulty.get_mpz_t(), 3 + params.powAcceptedConstellations1[0].size());
    if (newDifficulty < minDifficulty)
        newDifficulty = minDifficulty;

    arith_uint256 newDifficultyU256;
    newDifficultyU256.SetHex(newDifficulty.get_str(16));
    return newDifficultyU256.GetCompact();
}

void operator<<=(mpz_class& a, mpz_class b)
{
    a <<= mpz_get_ui(b.get_mpz_t()); // mpz_get_ui returns an unsigned long containing the lowest bits.
    if (b > ULONG_MAX) // ULONG_MAX = 2^32 - 1
    {
        // b can be written in the form x*2^32 + y and we already shifted by y. Now shifting by 2^32 x times.
        b -= mpz_get_ui(b.get_mpz_t()); // b <- x*2^32
        b >>= sizeof(unsigned long); // b <- x
        for (mpz_class i(0) ; i < b ; i++)
        {
            a <<= ULONG_MAX;
            a <<= 1;
        }
    }
}

mpz_class GenerateTarget(mpz_class &target, uint256 hash, unsigned int nBits)
{
    // Target <- 1 . 00...0 (ZEROS_BEFORE_HASH zeros) . hash (base 2) = 2^(1 + ZEROS_BEFORE_HASH + 256) + hash
    target = 1;
    target <<= ZEROS_BEFORE_HASH;
    for (int i(0) ; i < 256 ; i++)
    {
        target <<= 1;
        target += ((hash.begin()[i/8] >> (i % 8)) & 1);
    }

    // Now padding Target with zeros such that its size is the Difficulty
    arith_uint256 difficulty;
    difficulty.SetCompact(nBits);
    mpz_class trailingZeros;
    mpz_import(trailingZeros.get_mpz_t(), 8, -1, sizeof(uint32_t), 0, 0, ArithToUint256(difficulty).begin());

    const unsigned int significativeDigits(1 + ZEROS_BEFORE_HASH + 256);
    if (trailingZeros < significativeDigits)
        return 0;

    trailingZeros -= significativeDigits;
    target <<= trailingZeros;
    return trailingZeros;
}

uint32_t CheckConstellation(mpz_class n, std::vector<int32_t> offsets, uint32_t iterations)
{
    uint32_t tupleLength(0);
    for (const auto &offset : offsets)
    {
        n += offset;
        if (mpz_probab_prime_p(n.get_mpz_t(), iterations) == 0)
            break;
        tupleLength++;
    }
    return tupleLength;
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, uint256 nOffset, const Consensus::Params& params)
{
    if (hash == params.hashGenesisBlockForPoW)
        return true;

    // Reject weird Compacts
    bool fNegative;
    bool fOverflow;
    arith_uint256 dummy;
    dummy.SetCompact(nBits, &fNegative, &fOverflow);
    if (fNegative || dummy < UintToArith256(params.powLimit) || fOverflow)
        return false;

    // Calculate the PoW result
    mpz_class target, offset, trailingZeros;
    trailingZeros = GenerateTarget(target, hash, nBits);
    mpz_import(offset.get_mpz_t(), 8, -1, sizeof(uint32_t), 0, 0, nOffset.begin());
    if (trailingZeros < 256)
    {
        mpz_class offsetLimit(1);
        offsetLimit <<= trailingZeros;
        if (offset >= offsetLimit)
            return error("CheckProofOfWork() : offset %s larger than allowed 2^%s", offset.get_str().c_str(), trailingZeros.get_str().c_str());
    }
    mpz_class result(target + offset);

    // Check PoW result
    std::vector<uint32_t> tupleLengths;
    std::vector<std::vector<int32_t>> acceptedConstellations(params.powAcceptedConstellations1); // TODO: handle new constellations starting from fork2Height
    for (const auto &constellation : acceptedConstellations)
    {
        tupleLengths.push_back(CheckConstellation(result, constellation, 1)); // Quick single iteration test first
        if (tupleLengths.back() != constellation.size())
            continue;
        tupleLengths.back() = CheckConstellation(result, constellation, 31);
        if (tupleLengths.back() == constellation.size())
            return true;
    }
    return false;
}
