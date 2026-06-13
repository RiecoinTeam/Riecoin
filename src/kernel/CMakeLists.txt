# Copyright (c) 2024-present The Bitcoin Core developers
# Copyright (c) 2024-present The Riecoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

include(GNUInstallDirs)

# TODO: libriecoinkernel is a work in progress consensus engine
#       library, as more and more modules are decoupled from the
#       consensus engine, this list will shrink to only those
#       which are absolutely necessary.
#       The Library has not been tested beyond being compilable.
add_library(riecoinkernel
  riecoinkernel.cpp
  chain.cpp
  checks.cpp
  chainparams.cpp
  coinstats.cpp
  context.cpp
  cs_main.cpp
  disconnected_transactions.cpp
  mempool_removal_reason.cpp
  ../arith_uint256.cpp
  ../chain.cpp
  ../chainparams.cpp
  ../chainparamsbase.cpp
  ../common/args.cpp
  ../common/settings.cpp
  ../coins.cpp
  ../compressor.cpp
  ../consensus/merkle.cpp
  ../consensus/tx_check.cpp
  ../consensus/tx_verify.cpp
  ../dbwrapper.cpp
  ../deploymentinfo.cpp
  ../deploymentstatus.cpp
  ../flatfile.cpp
  ../hash.cpp
  ../logging.cpp
  ../node/blockstorage.cpp
  ../node/chainstate.cpp
  ../node/utxo_snapshot.cpp
  ../policy/ephemeral_policy.cpp
  ../policy/feerate.cpp
  ../policy/packages.cpp
  ../policy/policy.cpp
  ../policy/rbf.cpp
  ../policy/settings.cpp
  ../policy/truc_policy.cpp
  ../pow.cpp
  ../primitives/block.cpp
  ../primitives/transaction.cpp
  ../pubkey.cpp
  ../random.cpp
  ../randomenv.cpp
  ../script/interpreter.cpp
  ../script/script.cpp
  ../script/script_error.cpp
  ../script/sigcache.cpp
  ../script/solver.cpp
  ../streams.cpp
  ../sync.cpp
  ../txdb.cpp
  ../txgraph.cpp
  ../txmempool.cpp
  ../uint256.cpp
  ../util/chaintype.cpp
  ../util/check.cpp
  ../util/expected.cpp
  ../util/feefrac.cpp
  ../util/fs.cpp
  ../util/fs_helpers.cpp
  ../util/hasher.cpp
  ../util/moneystr.cpp
  ../util/rbf.cpp
  ../util/signalinterrupt.cpp
  ../util/strencodings.cpp
  ../util/syserror.cpp
  ../util/threadnames.cpp
  ../util/time.cpp
  ../util/tokenpipe.cpp
  ../validation.cpp
  ../validationinterface.cpp
  ../versionbits.cpp
  $<TARGET_OBJECTS:riecoin_clientversion>
  $<TARGET_OBJECTS:bitcoin_crypto>
  $<TARGET_OBJECTS:leveldb>
  $<TARGET_OBJECTS:crc32c>
)

# Compiler warnings that apply only to the kernel and its dependencies.
# These can be more strict and/or warnings that only apply to shared
# libs.
add_library(kernel_warn_interface INTERFACE)
if(NOT MSVC)
  try_append_cxx_flags("-Wunique-object-duplication" TARGET kernel_warn_interface SKIP_LINK)
endif()

# Also manually apply the warnings to the kernel's internal dependencies
target_link_libraries(riecoin_clientversion PRIVATE kernel_warn_interface)
target_link_libraries(bitcoin_crypto PRIVATE kernel_warn_interface)
target_link_libraries(leveldb PRIVATE kernel_warn_interface)
target_link_libraries(crc32c PRIVATE kernel_warn_interface)

target_link_libraries(riecoinkernel
  PRIVATE
    core_interface
    kernel_warn_interface
    secp256k1_objs
    univalue
    $<$<PLATFORM_ID:Windows>:bcrypt>
    $<TARGET_NAME_IF_EXISTS:GMP::GMP>
    $<TARGET_NAME_IF_EXISTS:USDT::headers>
    Boost::headers
)

target_include_directories(riecoinkernel PRIVATE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/leveldb/include>)

# Add a convenience libriecoinkernel target as a synonym for riecoinkernel.
add_custom_target(libriecoinkernel)
add_dependencies(libriecoinkernel riecoinkernel)

get_target_property(riecoinkernel_type riecoinkernel TYPE)
if(riecoinkernel_type STREQUAL "STATIC_LIBRARY")
  target_compile_definitions(riecoinkernel PUBLIC RIECOINKERNEL_STATIC)
endif()

configure_file(${PROJECT_SOURCE_DIR}/libriecoinkernel.pc.in ${PROJECT_BINARY_DIR}/libriecoinkernel.pc @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/libriecoinkernel.pc DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig" COMPONENT libriecoinkernel)

install(TARGETS riecoinkernel
  RUNTIME
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT libriecoinkernel
  LIBRARY
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT libriecoinkernel
  ARCHIVE
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT libriecoinkernel
)

install(FILES riecoinkernel.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT libriecoinkernel)
