// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Copyright (c) 2013-2023 The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_ISMINE_H
#define BITCOIN_WALLET_ISMINE_H

#include <script/standard.h>

#include <bitset>
#include <cstdint>
#include <type_traits>

class CScript;

namespace wallet {
class CWallet;

/**
 * IsMine() return codes, which depend on ScriptPubKeyMan implementation.
 * Not every ScriptPubKeyMan covers all types, please refer to
 * https://github.com/bitcoin/bitcoin/blob/master/doc/release-notes/release-notes-0.21.0.md#ismine-semantics
 * for better understanding.
 *
 * For DescriptorScriptPubKeyMan and future ScriptPubKeyMan,
 * ISMINE_NO: the scriptPubKey is not in the wallet;
 * ISMINE_SPENDABLE: the scriptPubKey matches a scriptPubKey in the wallet.
 * ISMINE_USED: the scriptPubKey corresponds to a used address owned by the wallet user.
 * ISMINE_ENUM_ELEMENTS: the number of isminetype enum elements.
 *
 */
enum isminetype : unsigned int {
    ISMINE_NO         = 0,
    ISMINE_SPENDABLE  = 1 << 0,
    ISMINE_USED       = 1 << 1,
    ISMINE_ENUM_ELEMENTS = 4,
};
/** used for bitflags of isminetype */
using isminefilter = std::underlying_type<isminetype>::type;

/**
 * Cachable amount subdivided into watchonly and spendable parts.
 */
struct CachableAmount
{
    // NO is never (supposed to be) cached
    std::bitset<ISMINE_ENUM_ELEMENTS> m_cached;
    CAmount m_value[ISMINE_ENUM_ELEMENTS];
    inline void Reset()
    {
        m_cached.reset();
    }
    void Set(isminefilter filter, CAmount value)
    {
        m_cached.set(filter);
        m_value[filter] = value;
    }
};
} // namespace wallet

#endif // BITCOIN_WALLET_ISMINE_H
