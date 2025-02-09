// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2013-present The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <logging.h>
#include <primitives/block.h>
#include <uint256.h>
#include <util/check.h>

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    if (pindexLast->nHeight + 1 >= params.fork2Height) {
        uint32_t nBits;
        if (pindexLast->nHeight + 1 == params.fork2Height) { // Take previous Difficulty/1.5, which is arbitrary, but approximates well enough the corresponding Difficulty for the transition from k to k + 1 tuples.
            uint32_t oldDifficulty((pindexLast->nBits & 0x007FFFFFU) >> 8U);
            nBits = oldDifficulty*171; // In the new format, the nBits is Difficulty/256, and 2*256/3 = ~171
            if (nBits < params.nBitsMin) nBits = params.nBitsMin;
        }
        else {
            if (pindexLast->nHeight == 0)
                return pindexLast->nBits;
            const CBlockIndex* pindexPrev(pindexLast->pprev);
            assert(pindexPrev);
            return CalculateNextWorkRequired(pindexLast, pindexPrev->GetBlockTime(), params);
        }
        return nBits;
    }
    else { // Before second Fork
        // Only change once per difficulty adjustment interval
        if ((pindexLast->nHeight + 1) % 288 != 0)
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
            return pindexLast->nBits;
        }

        // Go back by what we want to be nTargetTimespan worth of blocks
        int nHeightFirst = pindexLast->nHeight - 287;
        assert(nHeightFirst >= 0);
        if (nHeightFirst == 0)
            nHeightFirst++;
        const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
        assert(pindexFirst);

        return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
    }
}

unsigned int asert(const uint64_t nBits, int64_t previousSolveTime, int64_t nextHeight, const Consensus::Params& params) {
    const int64_t N(64), // Smoothing Value
                  cp(10*params.GetPowAcceptedPatternsAtHeight(nextHeight)[0].size() + 23), // Constellation Power * 10
                  previousDifficulty(nBits); // With the fixed point format, calculations can directly be done on nBits (int64 is used to avoid overflows)
    if (previousSolveTime < -TIMESTAMP_WINDOW)
        previousSolveTime = -TIMESTAMP_WINDOW;
    if (previousSolveTime > 12*params.nPowTargetSpacing)
        previousSolveTime = 12*params.nPowTargetSpacing;
    // Approximation of the ASERT Difficulty Adjustment Algorithm, see https://riecoin.dev/en/Protocol/Difficulty_Adjustment_Algorithm
    int64_t difficulty((previousDifficulty*(65536LL + 10LL*(65536LL - 65536LL*previousSolveTime/params.nPowTargetSpacing)/(N*cp)))/65536LL);
    if (difficulty < params.nBitsMin) difficulty = params.nBitsMin;
    else if (difficulty > 4294967295LL) difficulty = 4294967295LL;
    return static_cast<uint32_t>(difficulty);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting) // RegTest Only
        return pindexLast->nBits;

    if (pindexLast->nHeight + 1 >= params.fork2Height)
        return asert(pindexLast->nBits, pindexLast->GetBlockTime() - nFirstBlockTime, pindexLast->nHeight + 1, params);
    else { // MainNet Only, before Fork 2
        // Limit adjustment step
        int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
        if (pindexLast->nHeight != 287) { // But not for the first adjustement.
            if (nActualTimespan < 10800)
                nActualTimespan = 10800;
            if (nActualTimespan > 172800)
                nActualTimespan = 172800;
        }

        // Retarget
        mpz_class difficulty, newLinDifficulty, newDifficulty;
        arith_uint256 difficultyU256;
        difficultyU256.SetCompact(pindexLast->nBits);
        mpz_import(difficulty.get_mpz_t(), 8, -1, sizeof(uint32_t), 0, 0, ArithToUint256(difficultyU256).begin());

        // Approximately linearize difficulty by raising to the power 3 + Constellation Size
        mpz_pow_ui(newLinDifficulty.get_mpz_t(), difficulty.get_mpz_t(), 9);
        newLinDifficulty *= 43200U;
        newLinDifficulty /= (uint32_t) nActualTimespan; // Gmp does not support 64 bits in some operating systems :| (compiler "use of overloaded operator is ambiguous" errors)

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

        mpz_root(newDifficulty.get_mpz_t(), newLinDifficulty.get_mpz_t(), 9);
        uint32_t minDifficulty(304);
        if (newDifficulty < minDifficulty)
            newDifficulty = minDifficulty;

        std::string newDifficultyStr(newDifficulty.get_str(16));
        newDifficultyStr = std::string(64U - newDifficultyStr.length(), '0') + newDifficultyStr;
        arith_uint256 newDifficultyU256(UintToArith256(uint256::FromHex(newDifficultyStr).value()));
        return newDifficultyU256.GetCompact();
    }
}

// Check that on difficulty adjustments, the new difficulty does not increase
// or decrease beyond the permitted limits.
bool PermittedDifficultyTransition(const Consensus::Params& params, int64_t height, uint32_t old_nbits, uint32_t new_nbits)
{
    if (height >= params.fork2Height) {
        if (height == params.fork2Height) { // Transition Fork 1 -> Fork 2
            uint32_t oldDifficulty((old_nbits & 0x007FFFFFU) >> 8U);
            uint32_t expectedNBits(oldDifficulty*171);
            if (expectedNBits < params.nBitsMin) expectedNBits = params.nBitsMin;
            return new_nbits == expectedNBits;
        }
        else {
            int64_t largest_difficulty_target(asert(old_nbits, -TIMESTAMP_WINDOW, height, params));
            int64_t smallest_difficulty_target(asert(old_nbits, 12*params.nPowTargetSpacing, height, params));
            if (new_nbits < smallest_difficulty_target) return false;
            if (new_nbits > largest_difficulty_target) return false;
        }
    }
    else if (new_nbits < 33632256 || new_nbits > 34210816) // MainNet Only, before second Fork. Just enforce the lower (304) and upper (2564) bounds for simplicity.
        return false;
    return true;
}

std::optional<uint32_t> DeriveTrailingZeros(unsigned int nBits, const int32_t powVersion, const uint32_t nBitsMin)
{
    if (nBits < nBitsMin)
        return {};
    uint32_t trailingZeros;
    if (powVersion == -1)
        trailingZeros = (nBits & 0x007FFFFFU) >> 8U;
    else if (powVersion == 1)
        trailingZeros = (nBits >> 8U) + 1;
    else
        return {};

    const unsigned int significativeDigits(265); // 1 + 8 + 256
    if (trailingZeros < significativeDigits)
        return {};
    trailingZeros -= significativeDigits;
    return trailingZeros;
}

std::optional<mpz_class> DeriveTarget(uint256 hash, unsigned int nBits, const int32_t powVersion, const uint32_t nBitsMin)
{
    mpz_class target(256);
    if (powVersion == -1) { // Target = 1 . 00000000 . hash . 00...0 = 2^(D - 1) + H*2^(D – 265)
        for (int i(0) ; i < 256 ; i++) { // Inverts endianness and bit order inside bytes
            target <<= 1;
            target += ((hash.begin()[i/8] >> (i % 8)) & 1);
        }
    }
    else if (powVersion == 1) { // Here, rather than using 8 zeros, we fill this field with L = round(2^(8 + Df/2^8) - 2^8)
        uint32_t df(nBits & 255U);
        target += (10U*df*df*df + 7383U*df*df + 5840720U*df + 3997440U) >> 23U; // Gives the same results as L using only integers
        target <<= 256;
        mpz_class hashGmp;
        mpz_import(hashGmp.get_mpz_t(), 8, -1, sizeof(uint32_t), 0, 0, hash.begin());
        target += hashGmp;
    }
    else // Check must be done before calling DeriveTarget
        return {};

    // Now padding Target with zeros such that its size is the Difficulty (PoW Version -1) or such that Target = ~2^Difficulty (else)
    const auto trailingZeros(DeriveTrailingZeros(nBits, powVersion, nBitsMin));
    if (!trailingZeros)
        return {};
    target <<= *trailingZeros;
    return target;
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

static std::vector<uint64_t> GeneratePrimeTable(const uint64_t limit) // Using Sieve of Eratosthenes
{
    if (limit < 2) return {};
    std::vector<uint64_t> compositeTable((limit + 127ULL)/128ULL, 0ULL);
    for (uint64_t f(3ULL) ; f*f <= limit ; f += 2ULL) {
        if (compositeTable[f >> 7ULL] & (1ULL << ((f >> 1ULL) & 63ULL))) continue;
        for (uint64_t m((f*f) >> 1ULL) ; m <= (limit >> 1ULL) ; m += f)
            compositeTable[m >> 6ULL] |= 1ULL << (m & 63ULL);
    }
    std::vector<uint64_t> primeTable(1, 2);
    for (uint64_t i(1ULL) ; (i << 1ULL) + 1ULL <= limit ; i++) {
        if (!(compositeTable[i >> 6ULL] & (1ULL << (i & 63ULL))))
            primeTable.push_back((i << 1ULL) + 1ULL);
    }
    if (limit == 821641) {
        assert(primeTable.size() == 65536);
        assert(primeTable[0] == 2);
        assert(primeTable[32767] == 386093);
        assert(primeTable[65535] == 821641);
    }
    return primeTable;
}
const std::vector<uint64_t> primeTable(GeneratePrimeTable(821641)); // Used to calculate the Primorial when checking

// Bypasses the actual proof of work check during fuzz testing .
bool CheckProofOfWork(uint256 hash, unsigned int nBits, uint256 nOnce, const Consensus::Params& params)
{
    if constexpr (G_FUZZING) return true;
    return CheckProofOfWorkImpl(hash, nBits, nOnce, params);
}

bool CheckProofOfWorkImpl(uint256 hash, unsigned int nBits, uint256 nOnce, const Consensus::Params& params)
{
    if (hash == params.hashGenesisBlockForPoW)
        return true;

    int32_t powVersion;
    if ((nOnce.GetUint64(0) & 1) == 1)
    {
        // Now that we forked, we can have simple Sanity Checks. They also eliminate cases like negative numbers or overflows.
        if (nBits < 33632256 || nBits > 34210816) // All Difficulties before Fork 2 were between 304 and 2564.
            return false;
        powVersion = -1;
    }
    else if ((nOnce.GetUint64(0) & 65535) == 2) {
        if (nBits < params.nBitsMin)
            return false;
        powVersion = 1;
    }
    else
        return false;

    const auto trailingZeros(DeriveTrailingZeros(nBits, powVersion, params.nBitsMin));
    if (!trailingZeros)
        return false;
    const std::optional<mpz_class> target(DeriveTarget(hash, nBits, powVersion, params.nBitsMin));
    if (!target)
        return false;
    mpz_class offset, offsetLimit(1);
    offsetLimit <<= *trailingZeros;
    // Calculate the PoW result
    if (powVersion == -1)
        mpz_import(offset.get_mpz_t(), 8, -1, sizeof(uint32_t), 0, 0, nOnce.begin()); // [31-0 Offset]
    else if (powVersion == 1)
    {
        const uint8_t* rawOffset(nOnce.begin()); // [31-30 Primorial Number|29-14 Primorial Factor|13-2 Primorial Offset|1-0 Reserved/Version]
        const uint16_t primorialNumber(reinterpret_cast<const uint16_t*>(&rawOffset[30])[0]);
        mpz_class primorial(1), primorialFactor, primorialOffset;
        for (uint16_t i(0) ; i < primorialNumber ; i++)
        {
            mpz_mul_ui(primorial.get_mpz_t(), primorial.get_mpz_t(), primeTable[i]);
            if (primorial > offsetLimit) {
                LogError("CheckProofOfWork(): too large Primorial Number %s\n", primorialNumber);
                return false;
            }
        }
        mpz_import(primorialFactor.get_mpz_t(), 16, -1, sizeof(uint8_t), 0, 0, &rawOffset[14]);
        mpz_import(primorialOffset.get_mpz_t(), 12, -1, sizeof(uint8_t), 0, 0, &rawOffset[2]);
        offset = primorial - (*target % primorial) + primorialFactor*primorial + primorialOffset;
    }
    if (offset >= offsetLimit) {
        LogError("CheckProofOfWork(): offset %s larger than allowed 2^%d\n", offset.get_str().c_str(), *trailingZeros);
        return false;
    }
    const mpz_class result(*target + offset);

    // Check PoW result
    std::vector<uint32_t> tupleLengths;
    std::vector<std::vector<int32_t>> acceptedPatterns;
    if (powVersion == -1)
        acceptedPatterns = {{0, 4, 2, 4, 2, 4}};
    else if (powVersion == 1)
        acceptedPatterns = params.powAcceptedPatterns;
    for (const auto &pattern : acceptedPatterns)
    {
        tupleLengths.push_back(CheckConstellation(result, pattern, 1)); // Quick single iteration test first
        if (tupleLengths.back() != pattern.size())
            continue;
        tupleLengths.back() = CheckConstellation(result, pattern, 31);
        if (tupleLengths.back() == pattern.size())
            return true;
    }
    return false;
}
