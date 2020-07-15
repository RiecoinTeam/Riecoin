// Copyright (c) 2014-2020 The Bitcoin Core developers
// Copyright (c) 2013-2020 The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <timedata.h>
#include <sync.h>

static RecursiveMutex cs_nTimeOffset;
static int64_t nTimeOffset GUARDED_BY(cs_nTimeOffset) = 0;

int64_t GetTimeOffset()
{
    LOCK(cs_nTimeOffset);
    return nTimeOffset;
}
