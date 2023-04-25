// Copyright (c) 2019 The Bitcoin Core developers
// Copyright (c) 2013-2023 The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TEST_UTIL_WALLET_H
#define BITCOIN_TEST_UTIL_WALLET_H

#include <string>

namespace wallet {
class CWallet;
} // namespace wallet

// Constants //

extern const std::string ADDRESS_BCRT1_UNSPENDABLE;

// RPC-like //

/** Returns a new address from the wallet */
std::string getnewaddress(wallet::CWallet& w);


#endif // BITCOIN_TEST_UTIL_WALLET_H
