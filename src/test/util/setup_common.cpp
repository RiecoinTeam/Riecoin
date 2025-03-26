// Copyright (c) 2011-present The Bitcoin Core developers
// Copyright (c) 2013-present The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/setup_common.h>

#include <addrman.h>
#include <banman.h>
#include <chainparams.h>
#include <common/system.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/validation.h>
#include <crypto/sha256.h>
#include <init.h>
#include <init/common.h>
#include <interfaces/chain.h>
#include <kernel/mempool_entry.h>
#include <logging.h>
#include <net.h>
#include <net_processing.h>
#include <node/blockstorage.h>
#include <node/chainstate.h>
#include <node/context.h>
#include <node/kernel_notifications.h>
#include <node/mempool_args.h>
#include <node/miner.h>
#include <node/peerman_args.h>
#include <node/warnings.h>
#include <noui.h>
#include <policy/fees.h>
#include <pow.h>
#include <random.h>
#include <rpc/blockchain.h>
#include <rpc/register.h>
#include <rpc/server.h>
#include <scheduler.h>
#include <script/sigcache.h>
#include <streams.h>
#include <test/util/coverage.h>
#include <test/util/net.h>
#include <test/util/random.h>
#include <test/util/txmempool.h>
#include <txdb.h>
#include <txmempool.h>
#include <util/chaintype.h>
#include <util/check.h>
#include <util/fs_helpers.h>
#include <util/rbf.h>
#include <util/strencodings.h>
#include <util/string.h>
#include <util/task_runner.h>
#include <util/thread.h>
#include <util/threadnames.h>
#include <util/time.h>
#include <util/translation.h>
#include <util/vector.h>
#include <validation.h>
#include <validationinterface.h>
#include <walletinitinterface.h>

#include <algorithm>
#include <functional>
#include <stdexcept>

using namespace util::hex_literals;
using node::ApplyArgsManOptions;
using node::BlockAssembler;
using node::BlockManager;
using node::KernelNotifications;
using node::LoadChainstate;
using node::RegenerateCommitments;
using node::VerifyLoadedChainstate;

const TranslateFn G_TRANSLATION_FUN{nullptr};

constexpr inline auto TEST_DIR_PATH_ELEMENT{"test_common bitcoin"}; // Includes a space to catch possible path escape issues.
/** Random context to get unique temp data dirs. Separate from m_rng, which can be seeded from a const env var */
static FastRandomContext g_rng_temp_path;
static const bool g_rng_temp_path_init{[] {
    // Must be initialized before any SeedRandomForTest
    Assert(!g_used_g_prng);
    (void)g_rng_temp_path.rand64();
    g_used_g_prng = false;
    ResetCoverageCounters(); // The seed strengthen in SeedStartup is not deterministic, so exclude it from coverage counts
    return true;
}()};

struct NetworkSetup
{
    NetworkSetup()
    {
        Assert(SetupNetworking());
    }
};
static NetworkSetup g_networksetup_instance;

void SetupCommonTestArgs(ArgsManager& argsman)
{
    argsman.AddArg("-testdatadir", strprintf("Custom data directory (default: %s<random_string>)", fs::PathToString(fs::temp_directory_path() / TEST_DIR_PATH_ELEMENT / "")),
                   ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
}

/** Test setup failure */
static void ExitFailure(std::string_view str_err)
{
    std::cerr << str_err << std::endl;
    exit(EXIT_FAILURE);
}

BasicTestingSetup::BasicTestingSetup(const ChainType chainType, TestOpts opts)
    : m_args{}
{
    if constexpr (!G_FUZZING) {
        SeedRandomForTest(SeedRand::FIXED_SEED);
    }
    m_node.shutdown_signal = &m_interrupt;
    m_node.shutdown_request = [this]{ return m_interrupt(); };
    m_node.args = &gArgs;
    std::vector<const char*> arguments = Cat(
        {
            "dummy",
            "-printtoconsole=0",
            "-logsourcelocations",
            "-logtimemicros",
            "-logthreadnames",
            "-loglevel=trace",
            "-debug",
            "-debugexclude=libevent",
            "-debugexclude=leveldb",
        },
        opts.extra_args);
    if (G_TEST_COMMAND_LINE_ARGUMENTS) {
        arguments = Cat(arguments, G_TEST_COMMAND_LINE_ARGUMENTS());
    }
    util::ThreadRename("test");
    gArgs.ClearPathCache();
    {
        SetupServerArgs(*m_node.args);
        SetupCommonTestArgs(*m_node.args);
        std::string error;
        if (!m_node.args->ParseParameters(arguments.size(), arguments.data(), error)) {
            m_node.args->ClearArgs();
            throw std::runtime_error{error};
        }
    }

    const std::string test_name{G_TEST_GET_FULL_NAME ? G_TEST_GET_FULL_NAME() : ""};
    if (!m_node.args->IsArgSet("-testdatadir")) {
        // To avoid colliding with a leftover prior datadir, and to allow
        // tests, such as the fuzz tests to run in several processes at the
        // same time, add a random element to the path. Keep it small enough to
        // avoid a MAX_PATH violation on Windows.
        const auto rand{HexStr(g_rng_temp_path.randbytes(10))};
        m_path_root = fs::temp_directory_path() / TEST_DIR_PATH_ELEMENT / test_name / rand;
        TryCreateDirectories(m_path_root);
    } else {
        // Custom data directory
        m_has_custom_datadir = true;
        fs::path root_dir{m_node.args->GetPathArg("-testdatadir")};
        if (root_dir.empty()) ExitFailure("-testdatadir argument is empty, please specify a path");

        root_dir = fs::absolute(root_dir);
        m_path_lock = root_dir / TEST_DIR_PATH_ELEMENT / fs::PathFromString(test_name);
        m_path_root = m_path_lock / "datadir";

        // Try to obtain the lock; if unsuccessful don't disturb the existing test.
        TryCreateDirectories(m_path_lock);
        if (util::LockDirectory(m_path_lock, ".lock", /*probe_only=*/false) != util::LockResult::Success) {
            ExitFailure("Cannot obtain a lock on test data lock directory " + fs::PathToString(m_path_lock) + '\n' + "The test executable is probably already running.");
        }

        // Always start with a fresh data directory; this doesn't delete the .lock file located one level above.
        fs::remove_all(m_path_root);
        if (!TryCreateDirectories(m_path_root)) ExitFailure("Cannot create test data directory");

        // Print the test directory name if custom.
        std::cout << "Test directory (will not be deleted): " << m_path_root << std::endl;
    }
    m_args.ForceSetArg("-datadir", fs::PathToString(m_path_root));
    gArgs.ForceSetArg("-datadir", fs::PathToString(m_path_root));

    SelectParams(chainType);
    if (G_TEST_LOG_FUN) LogInstance().PushBackCallback(G_TEST_LOG_FUN);
    InitLogging(*m_node.args);
    AppInitParameterInteraction(*m_node.args);
    LogInstance().StartLogging();
    m_node.warnings = std::make_unique<node::Warnings>();
    m_node.kernel = std::make_unique<kernel::Context>();
    m_node.ecc_context = std::make_unique<ECC_Context>();
    SetupEnvironment();

    m_node.chain = interfaces::MakeChain(m_node);
    static bool noui_connected = false;
    if (!noui_connected) {
        noui_connect();
        noui_connected = true;
    }
}

BasicTestingSetup::~BasicTestingSetup()
{
    m_node.ecc_context.reset();
    m_node.kernel.reset();
    if constexpr (!G_FUZZING) {
        SetMockTime(0s); // Reset mocktime for following tests
    }
    LogInstance().DisconnectTestLogger();
    if (m_has_custom_datadir) {
        // Only remove the lock file, preserve the data directory.
        UnlockDirectory(m_path_lock, ".lock");
        fs::remove(m_path_lock / ".lock");
    } else {
        fs::remove_all(m_path_root);
    }
    gArgs.ClearArgs();
}

ChainTestingSetup::ChainTestingSetup(const ChainType chainType, TestOpts opts)
    : BasicTestingSetup(chainType, opts)
{
    const CChainParams& chainparams = Params();

    // A task runner is required to prevent ActivateBestChain
    // from blocking due to queue overrun.
    if (opts.setup_validation_interface) {
        m_node.scheduler = std::make_unique<CScheduler>();
        m_node.scheduler->m_service_thread = std::thread(util::TraceThread, "scheduler", [&] { m_node.scheduler->serviceQueue(); });
        m_node.validation_signals =
            // Use synchronous task runner while fuzzing to avoid non-determinism
            G_FUZZING ? std::make_unique<ValidationSignals>(std::make_unique<util::ImmediateTaskRunner>()) :
                        std::make_unique<ValidationSignals>(std::make_unique<SerialTaskRunner>(*m_node.scheduler));
    }

    bilingual_str error{};
    m_node.mempool = std::make_unique<CTxMemPool>(MemPoolOptionsForTest(m_node), error);
    Assert(error.empty());
    m_node.warnings = std::make_unique<node::Warnings>();

    m_node.notifications = std::make_unique<KernelNotifications>(Assert(m_node.shutdown_request), m_node.exit_status, *Assert(m_node.warnings));

    m_make_chainman = [this, &chainparams, opts] {
        Assert(!m_node.chainman);
        ChainstateManager::Options chainman_opts{
            .chainparams = chainparams,
            .datadir = m_args.GetDataDirNet(),
            .check_block_index = 1,
            .notifications = *m_node.notifications,
            .signals = m_node.validation_signals.get(),
            .worker_threads_num = 2,
        };
        if (opts.min_validation_cache) {
            chainman_opts.script_execution_cache_bytes = 0;
            chainman_opts.signature_cache_bytes = 0;
        }
        const BlockManager::Options blockman_opts{
            .chainparams = chainman_opts.chainparams,
            .blocks_dir = m_args.GetBlocksDirPath(),
            .notifications = chainman_opts.notifications,
            .block_tree_db_params = DBParams{
                .path = m_args.GetDataDirNet() / "blocks" / "index",
                .cache_bytes = m_kernel_cache_sizes.block_tree_db,
                .memory_only = opts.block_tree_db_in_memory,
                .wipe_data = m_args.GetBoolArg("-reindex", false),
            },
        };
        m_node.chainman = std::make_unique<ChainstateManager>(*Assert(m_node.shutdown_signal), chainman_opts, blockman_opts);
    };
    m_make_chainman();
}

ChainTestingSetup::~ChainTestingSetup()
{
    if (m_node.scheduler) m_node.scheduler->stop();
    if (m_node.validation_signals) m_node.validation_signals->FlushBackgroundCallbacks();
    m_node.connman.reset();
    m_node.banman.reset();
    m_node.addrman.reset();
    m_node.netgroupman.reset();
    m_node.args = nullptr;
    m_node.mempool.reset();
    Assert(!m_node.fee_estimator); // Each test must create a local object, if they wish to use the fee_estimator
    m_node.chainman.reset();
    m_node.validation_signals.reset();
    m_node.scheduler.reset();
}

void ChainTestingSetup::LoadVerifyActivateChainstate()
{
    auto& chainman{*Assert(m_node.chainman)};
    node::ChainstateLoadOptions options;
    options.mempool = Assert(m_node.mempool.get());
    options.coins_db_in_memory = m_coins_db_in_memory;
    options.wipe_chainstate_db = m_args.GetBoolArg("-reindex", false) || m_args.GetBoolArg("-reindex-chainstate", false);
    options.prune = chainman.m_blockman.IsPruneMode();
    options.check_blocks = m_args.GetIntArg("-checkblocks", DEFAULT_CHECKBLOCKS);
    options.check_level = m_args.GetIntArg("-checklevel", DEFAULT_CHECKLEVEL);
    options.require_full_verification = m_args.IsArgSet("-checkblocks") || m_args.IsArgSet("-checklevel");
    auto [status, error] = LoadChainstate(chainman, m_kernel_cache_sizes, options);
    assert(status == node::ChainstateLoadStatus::SUCCESS);

    std::tie(status, error) = VerifyLoadedChainstate(chainman, options);
    assert(status == node::ChainstateLoadStatus::SUCCESS);

    BlockValidationState state;
    if (!chainman.ActiveChainstate().ActivateBestChain(state)) {
        throw std::runtime_error(strprintf("ActivateBestChain failed. (%s)", state.ToString()));
    }
}

TestingSetup::TestingSetup(
    const ChainType chainType,
    TestOpts opts)
    : ChainTestingSetup(chainType, opts)
{
    m_coins_db_in_memory = opts.coins_db_in_memory;
    m_block_tree_db_in_memory = opts.block_tree_db_in_memory;
    // Ideally we'd move all the RPC tests to the functional testing framework
    // instead of unit tests, but for now we need these here.
    RegisterAllCoreRPCCommands(tableRPC);

    LoadVerifyActivateChainstate();

    if (!opts.setup_net) return;

    m_node.netgroupman = std::make_unique<NetGroupManager>(/*asmap=*/std::vector<bool>());
    m_node.addrman = std::make_unique<AddrMan>(*m_node.netgroupman,
                                               /*deterministic=*/false,
                                               m_node.args->GetIntArg("-checkaddrman", 0));
    m_node.banman = std::make_unique<BanMan>(m_args.GetDataDirBase() / "banlist", nullptr, DEFAULT_MISBEHAVING_BANTIME);
    m_node.connman = std::make_unique<ConnmanTestMsg>(0x1337, 0x1337, *m_node.addrman, *m_node.netgroupman, Params()); // Deterministic randomness for tests.
    PeerManager::Options peerman_opts;
    ApplyArgsManOptions(*m_node.args, peerman_opts);
    peerman_opts.deterministic_rng = true;
    m_node.peerman = PeerManager::make(*m_node.connman, *m_node.addrman,
                                       m_node.banman.get(), *m_node.chainman,
                                       *m_node.mempool, *m_node.warnings,
                                       peerman_opts);

    {
        CConnman::Options options;
        options.m_msgproc = m_node.peerman.get();
        m_node.connman->Init(options);
    }
}

TestChain100Setup::TestChain100Setup(
    const ChainType chain_type,
    TestOpts opts)
    : TestingSetup{ChainType::REGTEST, opts}
{
    SetMockTime(1710190154);
    constexpr std::array<unsigned char, 32> vchKey = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}};
    coinbaseKey.Set(vchKey.begin(), vchKey.end(), true);

    // Generate a 100-block chain:
    this->mineBlocks(COINBASE_MATURITY);

    {
        LOCK(::cs_main);
        assert(
            m_node.chainman->ActiveChain().Tip()->GetBlockHash().ToString() == "6dcdbb069a598de55640f1034918f019a9c865ee8df0cc1d53557ad53d6bebf5");
    }
}

void TestChain100Setup::mineBlocks(int num_blocks)
{
    CScript scriptPubKey = CScript() << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    for (int i = 0; i < num_blocks; i++) {
        std::vector<CMutableTransaction> noTxns;
        CBlock b = CreateAndProcessBlock(noTxns, scriptPubKey);
        SetMockTime(GetTime() + 1);
        m_coinbase_txns.push_back(b.vtx[0]);
    }
}

CBlock TestChain100Setup::CreateBlock(
    const std::vector<CMutableTransaction>& txns,
    const CScript& scriptPubKey,
    Chainstate& chainstate)
{
    BlockAssembler::Options options;
    options.coinbase_output_script = scriptPubKey;
    CBlock block = BlockAssembler{chainstate, nullptr, options}.CreateNewBlock()->block;

    Assert(block.vtx.size() == 1);
    for (const CMutableTransaction& tx : txns) {
        block.vtx.push_back(MakeTransactionRef(tx));
    }
    RegenerateCommitments(block, *Assert(m_node.chainman));

    block.nNonce = UintToArith256(uint256{"0000000000000000000000000000000000000000000000000000000000000002"});
    while (!CheckProofOfWork(block.GetHashForPoW(), block.nBits, ArithToUint256(block.nNonce), m_node.chainman->GetConsensus())) block.nNonce += 131072;

    return block;
}

CBlock TestChain100Setup::CreateAndProcessBlock(
    const std::vector<CMutableTransaction>& txns,
    const CScript& scriptPubKey,
    Chainstate* chainstate)
{
    if (!chainstate) {
        chainstate = &Assert(m_node.chainman)->ActiveChainstate();
    }

    CBlock block = this->CreateBlock(txns, scriptPubKey, *chainstate);
    std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(block);
    Assert(m_node.chainman)->ProcessNewBlock(shared_pblock, true, true, nullptr);

    return block;
}

std::pair<CMutableTransaction, CAmount> TestChain100Setup::CreateValidTransaction(const std::vector<CTransactionRef>& input_transactions,
                                                                                  const std::vector<COutPoint>& inputs,
                                                                                  int input_height,
                                                                                  const std::vector<CKey>& input_signing_keys,
                                                                                  const std::vector<CTxOut>& outputs,
                                                                                  const std::optional<CFeeRate>& feerate,
                                                                                  const std::optional<uint32_t>& fee_output)
{
    CMutableTransaction mempool_txn;
    mempool_txn.vin.reserve(inputs.size());
    mempool_txn.vout.reserve(outputs.size());

    for (const auto& outpoint : inputs) {
        mempool_txn.vin.emplace_back(outpoint, CScript(), MAX_BIP125_RBF_SEQUENCE);
    }
    mempool_txn.vout = outputs;

    // - Add the signing key to a keystore
    FillableSigningProvider keystore;
    for (const auto& input_signing_key : input_signing_keys) {
        keystore.AddKey(input_signing_key);
    }
    // - Populate a CoinsViewCache with the unspent output
    CCoinsView coins_view;
    CCoinsViewCache coins_cache(&coins_view);
    for (const auto& input_transaction : input_transactions) {
        AddCoins(coins_cache, *input_transaction.get(), input_height);
    }
    // Build Outpoint to Coin map for SignTransaction
    std::map<COutPoint, Coin> input_coins;
    CAmount inputs_amount{0};
    for (const auto& outpoint_to_spend : inputs) {
        // Use GetCoin to properly populate utxo_to_spend
        auto utxo_to_spend{coins_cache.GetCoin(outpoint_to_spend).value()};
        input_coins.insert({outpoint_to_spend, utxo_to_spend});
        inputs_amount += utxo_to_spend.out.nValue;
    }
    // - Default signature hashing type
    int nHashType = SIGHASH_ALL;
    std::map<int, bilingual_str> input_errors;
    assert(SignTransaction(mempool_txn, &keystore, input_coins, nHashType, input_errors));
    CAmount current_fee = inputs_amount - std::accumulate(outputs.begin(), outputs.end(), CAmount(0),
        [](const CAmount& acc, const CTxOut& out) {
        return acc + out.nValue;
    });
    // Deduct fees from fee_output to meet feerate if set
    if (feerate.has_value()) {
        assert(fee_output.has_value());
        assert(fee_output.value() < mempool_txn.vout.size());
        CAmount target_fee = feerate.value().GetFee(GetVirtualTransactionSize(CTransaction{mempool_txn}));
        CAmount deduction = target_fee - current_fee;
        if (deduction > 0) {
            // Only deduct fee if there's anything to deduct. If the caller has put more fees than
            // the target feerate, don't change the fee.
            mempool_txn.vout[fee_output.value()].nValue -= deduction;
            // Re-sign since an output has changed
            input_errors.clear();
            assert(SignTransaction(mempool_txn, &keystore, input_coins, nHashType, input_errors));
            current_fee = target_fee;
        }
    }
    return {mempool_txn, current_fee};
}

CMutableTransaction TestChain100Setup::CreateValidMempoolTransaction(const std::vector<CTransactionRef>& input_transactions,
                                                                     const std::vector<COutPoint>& inputs,
                                                                     int input_height,
                                                                     const std::vector<CKey>& input_signing_keys,
                                                                     const std::vector<CTxOut>& outputs,
                                                                     bool submit)
{
    CMutableTransaction mempool_txn = CreateValidTransaction(input_transactions, inputs, input_height, input_signing_keys, outputs, std::nullopt, std::nullopt).first;
    // If submit=true, add transaction to the mempool.
    if (submit) {
        LOCK(cs_main);
        const MempoolAcceptResult result = m_node.chainman->ProcessTransaction(MakeTransactionRef(mempool_txn));
        assert(result.m_result_type == MempoolAcceptResult::ResultType::VALID);
    }
    return mempool_txn;
}

CMutableTransaction TestChain100Setup::CreateValidMempoolTransaction(CTransactionRef input_transaction,
                                                                     uint32_t input_vout,
                                                                     int input_height,
                                                                     CKey input_signing_key,
                                                                     CScript output_destination,
                                                                     CAmount output_amount,
                                                                     bool submit)
{
    COutPoint input{input_transaction->GetHash(), input_vout};
    CTxOut output{output_amount, output_destination};
    return CreateValidMempoolTransaction(/*input_transactions=*/{input_transaction},
                                         /*inputs=*/{input},
                                         /*input_height=*/input_height,
                                         /*input_signing_keys=*/{input_signing_key},
                                         /*outputs=*/{output},
                                         /*submit=*/submit);
}

std::vector<CTransactionRef> TestChain100Setup::PopulateMempool(FastRandomContext& det_rand, size_t num_transactions, bool submit)
{
    std::vector<CTransactionRef> mempool_transactions;
    std::deque<std::pair<COutPoint, CAmount>> unspent_prevouts;
    std::transform(m_coinbase_txns.begin(), m_coinbase_txns.end(), std::back_inserter(unspent_prevouts),
        [](const auto& tx){ return std::make_pair(COutPoint(tx->GetHash(), 0), tx->vout[0].nValue); });
    while (num_transactions > 0 && !unspent_prevouts.empty()) {
        // The number of inputs and outputs are random, between 1 and 24.
        CMutableTransaction mtx = CMutableTransaction();
        const size_t num_inputs = det_rand.randrange(24) + 1;
        CAmount total_in{0};
        for (size_t n{0}; n < num_inputs; ++n) {
            if (unspent_prevouts.empty()) break;
            const auto& [prevout, amount] = unspent_prevouts.front();
            mtx.vin.emplace_back(prevout, CScript());
            total_in += amount;
            unspent_prevouts.pop_front();
        }
        const size_t num_outputs = det_rand.randrange(24) + 1;
        const CAmount fee = 100 * det_rand.randrange(30);
        const CAmount amount_per_output = (total_in - fee) / num_outputs;
        for (size_t n{0}; n < num_outputs; ++n) {
            CScript spk = CScript() << CScriptNum(num_transactions + n);
            mtx.vout.emplace_back(amount_per_output, spk);
        }
        CTransactionRef ptx = MakeTransactionRef(mtx);
        mempool_transactions.push_back(ptx);
        if (amount_per_output > 3000) {
            // If the value is high enough to fund another transaction + fees, keep track of it so
            // it can be used to build a more complex transaction graph. Insert randomly into
            // unspent_prevouts for extra randomness in the resulting structures.
            for (size_t n{0}; n < num_outputs; ++n) {
                unspent_prevouts.emplace_back(COutPoint(ptx->GetHash(), n), amount_per_output);
                std::swap(unspent_prevouts.back(), unspent_prevouts[det_rand.randrange(unspent_prevouts.size())]);
            }
        }
        if (submit) {
            LOCK2(cs_main, m_node.mempool->cs);
            LockPoints lp;
            auto changeset = m_node.mempool->GetChangeSet();
            changeset->StageAddition(ptx, /*fee=*/(total_in - num_outputs * amount_per_output),
                    /*time=*/0, /*entry_height=*/1, /*entry_sequence=*/0,
                    /*spends_coinbase=*/false, /*sigops_cost=*/4, lp);
            changeset->Apply();
        }
        --num_transactions;
    }
    return mempool_transactions;
}

void TestChain100Setup::MockMempoolMinFee(const CFeeRate& target_feerate)
{
    LOCK2(cs_main, m_node.mempool->cs);
    // Transactions in the mempool will affect the new minimum feerate.
    assert(m_node.mempool->size() == 0);
    // The target feerate cannot be too low...
    // ...otherwise the transaction's feerate will need to be negative.
    assert(target_feerate > m_node.mempool->m_opts.incremental_relay_feerate);
    // ...otherwise this is not meaningful. The feerate policy uses the maximum of both feerates.
    assert(target_feerate > m_node.mempool->m_opts.min_relay_feerate);

    // Manually create an invalid transaction. Manually set the fee in the CTxMemPoolEntry to
    // achieve the exact target feerate.
    CMutableTransaction mtx = CMutableTransaction();
    mtx.vin.emplace_back(COutPoint{Txid::FromUint256(m_rng.rand256()), 0});
    mtx.vout.emplace_back(1 * COIN, GetScriptForDestination(WitnessV0ScriptHash(CScript() << OP_TRUE)));
    const auto tx{MakeTransactionRef(mtx)};
    LockPoints lp;
    // The new mempool min feerate is equal to the removed package's feerate + incremental feerate.
    const auto tx_fee = target_feerate.GetFee(GetVirtualTransactionSize(*tx)) -
        m_node.mempool->m_opts.incremental_relay_feerate.GetFee(GetVirtualTransactionSize(*tx));
    {
        auto changeset = m_node.mempool->GetChangeSet();
        changeset->StageAddition(tx, /*fee=*/tx_fee,
                /*time=*/0, /*entry_height=*/1, /*entry_sequence=*/0,
                /*spends_coinbase=*/true, /*sigops_cost=*/1, lp);
        changeset->Apply();
    }
    m_node.mempool->TrimToSize(0);
    assert(m_node.mempool->GetMinFee() == target_feerate);
}
/**
 * @returns a real block (5564fe1673f46378ea6417d8a9c04ea4898d1f470e979da1ebad562c38f7d571, Height 1323958)
 *      with 6 txs.
 */
CBlock getBlock5564f()
{
    CBlock block;
    DataStream stream{
        "000000208faded5983fd5bb5d23ca0b039b2cf91d4ca1d4db5a3b02c01b5c08a0d410a980bf3ee62c22d90bc2299352d1862f320c7e05cccabd6c643c3675664bdef8970d2c2f45e00000000009f0402059c1b2530726d8247879274fada96b3f3b9834edfb0f3d8805a5a7546638d7d06010000000001010000000000000000000000000000000000000000000000000000000000000000ffffffff0c03b633142f724d2f8617407affffffff02a7270395000000001600140ad73d094eca6d83cbcb7f921c6d0b70d36cbd720000000000000000266a24aa21a9ed6ef1aad21082ac1fb190c61e06f019640e7be548ec228625b3b782b2b51bb1ed01200000000000000000000000000000000000000000000000000000000000000000000000000100000001079abb2b2723515ff6cdb74f292a5c498332e99c2a86a5a0150563f4da5334b2000000006a47304402204b6db43519c851f422a73a89cd4ee94495b93c17caac1bacbba87a4dcc943bfe02201b645409a1d69034d2f56973b574114731b5bbdb3b9ffd3a08042785486ee54401210399f145c396c4940e346a11db221fedbf0228ee37242af39f2f60757a5f8510f1feffffff02abdc4f2f000000001976a914b54bfe1a783c71c02f3441d2e5eb102c60532db888ac7c67f741000000001976a914e4781b5c9370fe713fede1d89792b022608ed83288acb533140001000000010efb6b79553bb6fe3b51f7219ae600f9865213e3f06f4e23b30f225e5b9f4f49010000006a47304402203da145cbe6891a2244a7b8eff513d3311d0dd1db1b5d6bda8d70b323a9b9939402207e2354226e3823171d08cbb4f3ceb1eaaaca8eab7ceb3620ea11090c8bd50c77012102902c7cd4a14daeac63b9c075c4f43913b823f49350d15374017e0c283161c3defeffffff02d4b3d834000000001976a914081b1e2c58cb246341f73c15fbc2451022d96b8a88ace8d6230d000000001976a9144ac409c80bca26cdec87817dc0c6956f123eea0588acb53314000100000001c402acc13df1f4331f70434ff32da501f0d7f6c53fe823c081cc594953abd035010000006a473044022051f1ff30170a75d9b1cc70ad5eb097fbf8d88d247992d67f97ba0c7451003c320220705537f41fb4ce6e031ad1a0f7710fdc793e6faf127891e75252ec8cf9c0ad6a012103a4ffadecf271cf62ebf6cbc1ec9fa1dfb36146d62dbc88ddd7a8075d44b8ceccfeffffff0256fa947e000000001976a914e1b9572d307a8c1158c507589dd21328e562142788ac6af66d16000000001976a914bced4c4a28579dfd2edba27995954fc6e7f492b588acb53314000100000002436785bb90c8463ef78048311c0c25533e837c31c673b87b3f971fdf95980553010000006a47304402203bb68e43eb881e9953d1b49cfa93b3376f6498913b541f7996e8adf47f9e045402204335ca43f9ad165b59e1b037f9cfd602827f746bbfd703459076605f06cb1e4501210329da5b40a0fa877f2e1a3fd2296144b6f6b74bb636feef242fc5a20116b7ac44feffffffc9c80505b16ef9da1751b5bcf9bfaeee9128622cd452e28ac4291cfb7ea41a0d000000006b483045022100af48d711e4efab9e1f52df17281664cdca5e7b9f43b77f827951e560112b8ced02203f4931f94f922fc3ae4f02fa368f9d2eeb0b612744af66d0cef8911981bf7823012102157f5deb06c50045e9fa0724e4bb5c05a696fb3ba446fdf353c28d47cbaf70d8feffffff020fc3375e000000001976a91475f87a6fc2562cf6096313030b170da38f8c635588acf3031f00000000001976a9148633750417127ce58ff40bfe0e966cb82b07f48988acb53314000100000001ce66ebc2beeb5a4c6feb40bf375ab644a0576695cdbde31a4e974d0d16794e69010000006b483045022100933749d80ed779aea9bdd0857f885b65c66cf3ee2a71ea90aac02f71f6c543e80220659a4b864696ddb7c5da9201c22518605ea2f71d2faaf2e8c8d02100e3cff1600121035be414af5ea7081e8fc313ce8a7c42247ba5e4c659dd16af9ee71d496f2dff81feffffff02a4bbbc1e000000001976a9148ddfb0eb2aaa5aec31a501ecd68d9748cf87cbd988ac82f65d6b000000001976a9147f2171e3d70b1227823eb9453db1807be7e304c388acb5331400"_hex,
    };
    stream >> TX_WITH_WITNESS(block);
    return block;
}

std::ostream& operator<<(std::ostream& os, const arith_uint256& num)
{
    return os << num.ToString();
}

std::ostream& operator<<(std::ostream& os, const uint160& num)
{
    return os << num.ToString();
}

std::ostream& operator<<(std::ostream& os, const uint256& num)
{
    return os << num.ToString();
}
