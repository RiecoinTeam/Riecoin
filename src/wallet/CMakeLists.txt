# Copyright (c) 2023-present The Bitcoin Core developers
# Copyright (c) 2024-present The Riecoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

# Wallet functionality used by riecoind and riecoin-wallet executables.
add_library(riecoin_wallet STATIC EXCLUDE_FROM_ALL
  coincontrol.cpp
  coinselection.cpp
  context.cpp
  crypter.cpp
  db.cpp
  dump.cpp
  external_signer_scriptpubkeyman.cpp
  feebumper.cpp
  fees.cpp
  interfaces.cpp
  load.cpp
  receive.cpp
  rpc/addresses.cpp
  rpc/backup.cpp
  rpc/coins.cpp
  rpc/encrypt.cpp
  rpc/signmessage.cpp
  rpc/spend.cpp
  rpc/transactions.cpp
  rpc/util.cpp
  rpc/wallet.cpp
  scriptpubkeyman.cpp
  spend.cpp
  transaction.cpp
  wallet.cpp
  walletdb.cpp
  walletutil.cpp
)
target_link_libraries(riecoin_wallet
  PRIVATE
    core_interface
    riecoin_common
    univalue
    Boost::headers
    $<TARGET_NAME_IF_EXISTS:USDT::headers>
)

if(NOT USE_SQLITE)
  message(FATAL_ERROR "Wallet functionality requested but no SQLite support available.")
endif()
if(USE_SQLITE)
  target_sources(riecoin_wallet PRIVATE sqlite.cpp)
  target_link_libraries(riecoin_wallet
    PRIVATE
      $<TARGET_NAME_IF_EXISTS:unofficial::sqlite3::sqlite3>
      $<TARGET_NAME_IF_EXISTS:SQLite::SQLite3>
  )
endif()
