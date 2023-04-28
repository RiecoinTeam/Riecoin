// Copyright (c) 2011-2021 The Bitcoin Core developers
// Copyright (c) 2013-2023 The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/setup_common.h>

#include <kernel/validation_cache_sizes.h>

#include <addrman.h>
#include <banman.h>
#include <chainparams.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/validation.h>
#include <crypto/sha256.h>
#include <init.h>
#include <init/common.h>
#include <interfaces/chain.h>
#include <net.h>
#include <net_processing.h>
#include <node/blockstorage.h>
#include <node/chainstate.h>
#include <node/context.h>
#include <node/mempool_args.h>
#include <node/miner.h>
#include <node/validation_cache_args.h>
#include <noui.h>
#include <policy/fees.h>
#include <policy/fees_args.h>
#include <pow.h>
#include <rpc/blockchain.h>
#include <rpc/register.h>
#include <rpc/server.h>
#include <scheduler.h>
#include <script/sigcache.h>
#include <shutdown.h>
#include <streams.h>
#include <test/util/net.h>
#include <txdb.h>
#include <txmempool.h>
#include <util/strencodings.h>
#include <util/string.h>
#include <util/thread.h>
#include <util/threadnames.h>
#include <util/time.h>
#include <util/translation.h>
#include <util/url.h>
#include <util/vector.h>
#include <validation.h>
#include <validationinterface.h>
#include <walletinitinterface.h>

#include <algorithm>
#include <functional>
#include <stdexcept>

using kernel::ValidationCacheSizes;
using node::ApplyArgsManOptions;
using node::BlockAssembler;
using node::CalculateCacheSizes;
using node::LoadChainstate;
using node::NodeContext;
using node::RegenerateCommitments;
using node::VerifyLoadedChainstate;

const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;
UrlDecodeFn* const URL_DECODE = nullptr;

FastRandomContext g_insecure_rand_ctx;
/** Random context to get unique temp data dirs. Separate from g_insecure_rand_ctx, which can be seeded from a const env var */
static FastRandomContext g_insecure_rand_ctx_temp_path;

/** Return the unsigned from the environment var if available, otherwise 0 */
static uint256 GetUintFromEnv(const std::string& env_name)
{
    const char* num = std::getenv(env_name.c_str());
    if (!num) return {};
    return uint256S(num);
}

void Seed(FastRandomContext& ctx)
{
    // Should be enough to get the seed once for the process
    static uint256 seed{};
    static const std::string RANDOM_CTX_SEED{"RANDOM_CTX_SEED"};
    if (seed.IsNull()) seed = GetUintFromEnv(RANDOM_CTX_SEED);
    if (seed.IsNull()) seed = GetRandHash();
    LogPrintf("%s: Setting random seed for current tests to %s=%s\n", __func__, RANDOM_CTX_SEED, seed.GetHex());
    ctx = FastRandomContext(seed);
}

std::ostream& operator<<(std::ostream& os, const uint256& num)
{
    os << num.ToString();
    return os;
}

BasicTestingSetup::BasicTestingSetup(const std::string& chainName, const std::vector<const char*>& extra_args)
    : m_path_root{fs::temp_directory_path() / "test_common_" PACKAGE_NAME / g_insecure_rand_ctx_temp_path.rand256().ToString()},
      m_args{}
{
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
        extra_args);
    if (G_TEST_COMMAND_LINE_ARGUMENTS) {
        arguments = Cat(arguments, G_TEST_COMMAND_LINE_ARGUMENTS());
    }
    util::ThreadRename("test");
    fs::create_directories(m_path_root);
    m_args.ForceSetArg("-datadir", fs::PathToString(m_path_root));
    gArgs.ForceSetArg("-datadir", fs::PathToString(m_path_root));
    gArgs.ClearPathCache();
    {
        SetupServerArgs(*m_node.args);
        std::string error;
        if (!m_node.args->ParseParameters(arguments.size(), arguments.data(), error)) {
            m_node.args->ClearArgs();
            throw std::runtime_error{error};
        }
    }
    SelectParams(chainName);
    SeedInsecureRand();
    if (G_TEST_LOG_FUN) LogInstance().PushBackCallback(G_TEST_LOG_FUN);
    InitLogging(*m_node.args);
    AppInitParameterInteraction(*m_node.args);
    LogInstance().StartLogging();
    m_node.kernel = std::make_unique<kernel::Context>();
    SetupEnvironment();
    SetupNetworking();

    ValidationCacheSizes validation_cache_sizes{};
    ApplyArgsManOptions(*m_node.args, validation_cache_sizes);
    Assert(InitSignatureCache(validation_cache_sizes.signature_cache_bytes));
    Assert(InitScriptExecutionCache(validation_cache_sizes.script_execution_cache_bytes));

    m_node.chain = interfaces::MakeChain(m_node);
    fCheckBlockIndex = true;
    static bool noui_connected = false;
    if (!noui_connected) {
        noui_connect();
        noui_connected = true;
    }
}

BasicTestingSetup::~BasicTestingSetup()
{
    SetMockTime(0s); // Reset mocktime for following tests
    LogInstance().DisconnectTestLogger();
    fs::remove_all(m_path_root);
    gArgs.ClearArgs();
}

CTxMemPool::Options MemPoolOptionsForTest(const NodeContext& node)
{
    CTxMemPool::Options mempool_opts{
        .estimator = node.fee_estimator.get(),
        // Default to always checking mempool regardless of
        // chainparams.DefaultConsistencyChecks for tests
        .check_ratio = 1,
    };
    const auto err{ApplyArgsManOptions(*node.args, ::Params(), mempool_opts)};
    Assert(!err);
    return mempool_opts;
}

ChainTestingSetup::ChainTestingSetup(const std::string& chainName, const std::vector<const char*>& extra_args)
    : BasicTestingSetup(chainName, extra_args)
{
    const CChainParams& chainparams = Params();

    // We have to run a scheduler thread to prevent ActivateBestChain
    // from blocking due to queue overrun.
    m_node.scheduler = std::make_unique<CScheduler>();
    m_node.scheduler->m_service_thread = std::thread(util::TraceThread, "scheduler", [&] { m_node.scheduler->serviceQueue(); });
    GetMainSignals().RegisterBackgroundSignalScheduler(*m_node.scheduler);

    m_node.fee_estimator = std::make_unique<CBlockPolicyEstimator>(FeeestPath(*m_node.args));
    m_node.mempool = std::make_unique<CTxMemPool>(MemPoolOptionsForTest(m_node));

    m_cache_sizes = CalculateCacheSizes(m_args);

    const ChainstateManager::Options chainman_opts{
        .chainparams = chainparams,
        .adjusted_time_callback = Now<NodeSeconds>,
    };
    m_node.chainman = std::make_unique<ChainstateManager>(chainman_opts);
    m_node.chainman->m_blockman.m_block_tree_db = std::make_unique<CBlockTreeDB>(m_cache_sizes.block_tree_db, true);

    // Start script-checking threads. Set g_parallel_script_checks to true so they are used.
    constexpr int script_check_threads = 2;
    StartScriptCheckWorkerThreads(script_check_threads);
    g_parallel_script_checks = true;
}

ChainTestingSetup::~ChainTestingSetup()
{
    if (m_node.scheduler) m_node.scheduler->stop();
    StopScriptCheckWorkerThreads();
    GetMainSignals().FlushBackgroundCallbacks();
    GetMainSignals().UnregisterBackgroundSignalScheduler();
    m_node.connman.reset();
    m_node.banman.reset();
    m_node.addrman.reset();
    m_node.netgroupman.reset();
    m_node.args = nullptr;
    m_node.mempool.reset();
    m_node.scheduler.reset();
    m_node.chainman.reset();
}

TestingSetup::TestingSetup(const std::string& chainName, const std::vector<const char*>& extra_args)
    : ChainTestingSetup(chainName, extra_args)
{
    // Ideally we'd move all the RPC tests to the functional testing framework
    // instead of unit tests, but for now we need these here.
    RegisterAllCoreRPCCommands(tableRPC);

    node::ChainstateLoadOptions options;
    options.mempool = Assert(m_node.mempool.get());
    options.block_tree_db_in_memory = true;
    options.coins_db_in_memory = true;
    options.reindex = node::fReindex;
    options.reindex_chainstate = m_args.GetBoolArg("-reindex-chainstate", false);
    options.prune = node::fPruneMode;
    options.check_blocks = m_args.GetIntArg("-checkblocks", DEFAULT_CHECKBLOCKS);
    options.check_level = m_args.GetIntArg("-checklevel", DEFAULT_CHECKLEVEL);
    auto [status, error] = LoadChainstate(*Assert(m_node.chainman), m_cache_sizes, options);
    assert(status == node::ChainstateLoadStatus::SUCCESS);

    std::tie(status, error) = VerifyLoadedChainstate(*Assert(m_node.chainman), options);
    assert(status == node::ChainstateLoadStatus::SUCCESS);

    BlockValidationState state;
    if (!m_node.chainman->ActiveChainstate().ActivateBestChain(state)) {
        throw std::runtime_error(strprintf("ActivateBestChain failed. (%s)", state.ToString()));
    }

    m_node.netgroupman = std::make_unique<NetGroupManager>(/*asmap=*/std::vector<bool>());
    m_node.addrman = std::make_unique<AddrMan>(*m_node.netgroupman,
                                               /*deterministic=*/false,
                                               m_node.args->GetIntArg("-checkaddrman", 0));
    m_node.banman = std::make_unique<BanMan>(m_args.GetDataDirBase() / "banlist", nullptr, DEFAULT_MISBEHAVING_BANTIME);
    m_node.connman = std::make_unique<ConnmanTestMsg>(0x1337, 0x1337, *m_node.addrman, *m_node.netgroupman); // Deterministic randomness for tests.
    m_node.peerman = PeerManager::make(*m_node.connman, *m_node.addrman,
                                       m_node.banman.get(), *m_node.chainman,
                                       *m_node.mempool, false);
    {
        CConnman::Options options;
        options.m_msgproc = m_node.peerman.get();
        m_node.connman->Init(options);
    }
}

TestChain100Setup::TestChain100Setup(const std::string& chain_name, const std::vector<const char*>& extra_args)
    : TestingSetup{chain_name, extra_args}
{
    SetMockTime(1598887952);
    constexpr std::array<unsigned char, 32> vchKey = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}};
    coinbaseKey.Set(vchKey.begin(), vchKey.end(), true);

    // Generate a 100-block chain:
    this->mineBlocks(COINBASE_MATURITY);

    {
        LOCK(::cs_main);
        assert(
            m_node.chainman->ActiveChain().Tip()->GetBlockHash().ToString() == "a3b5ff036d33717084c0b26d85bf2061306c700c335452a66fec7143ba8f991e");
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
    CBlock block = BlockAssembler{chainstate, nullptr}.CreateNewBlock(scriptPubKey)->block;

    Assert(block.vtx.size() == 1);
    for (const CMutableTransaction& tx : txns) {
        block.vtx.push_back(MakeTransactionRef(tx));
    }
    RegenerateCommitments(block, *Assert(m_node.chainman));

    block.nNonce = 1;
    while (!CheckProofOfWork(block.GetHashForPoW(), block.nBits, ArithToUint256(block.nNonce), m_node.chainman->GetConsensus())) block.nNonce += 2;

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

    const CBlock block = this->CreateBlock(txns, scriptPubKey, *chainstate);
    std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(block);
    Assert(m_node.chainman)->ProcessNewBlock(shared_pblock, true, true, nullptr);

    return block;
}


CMutableTransaction TestChain100Setup::CreateValidMempoolTransaction(CTransactionRef input_transaction,
                                                                     int input_vout,
                                                                     int input_height,
                                                                     CKey input_signing_key,
                                                                     CScript output_destination,
                                                                     CAmount output_amount,
                                                                     bool submit)
{
    // Transaction we will submit to the mempool
    CMutableTransaction mempool_txn;

    // Create an input
    COutPoint outpoint_to_spend(input_transaction->GetHash(), input_vout);
    CTxIn input(outpoint_to_spend);
    mempool_txn.vin.push_back(input);

    // Create an output
    CTxOut output(output_amount, output_destination);
    mempool_txn.vout.push_back(output);

    // Sign the transaction
    // - Add the signing key to a keystore
    FillableSigningProvider keystore;
    keystore.AddKey(input_signing_key);
    // - Populate a CoinsViewCache with the unspent output
    CCoinsView coins_view;
    CCoinsViewCache coins_cache(&coins_view);
    AddCoins(coins_cache, *input_transaction.get(), input_height);
    // - Use GetCoin to properly populate utxo_to_spend,
    Coin utxo_to_spend;
    assert(coins_cache.GetCoin(outpoint_to_spend, utxo_to_spend));
    // - Then add it to a map to pass in to SignTransaction
    std::map<COutPoint, Coin> input_coins;
    input_coins.insert({outpoint_to_spend, utxo_to_spend});
    // - Default signature hashing type
    int nHashType = SIGHASH_ALL;
    std::map<int, bilingual_str> input_errors;
    assert(SignTransaction(mempool_txn, &keystore, input_coins, nHashType, input_errors));

    // If submit=true, add transaction to the mempool.
    if (submit) {
        LOCK(cs_main);
        const MempoolAcceptResult result = m_node.chainman->ProcessTransaction(MakeTransactionRef(mempool_txn));
        assert(result.m_result_type == MempoolAcceptResult::ResultType::VALID);
    }

    return mempool_txn;
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
            mtx.vin.push_back(CTxIn(prevout, CScript()));
            total_in += amount;
            unspent_prevouts.pop_front();
        }
        const size_t num_outputs = det_rand.randrange(24) + 1;
        // Approximately 1000sat "fee," equal output amounts.
        const CAmount amount_per_output = (total_in - 1000) / num_outputs;
        for (size_t n{0}; n < num_outputs; ++n) {
            CScript spk = CScript() << CScriptNum(num_transactions + n);
            mtx.vout.push_back(CTxOut(amount_per_output, spk));
        }
        CTransactionRef ptx = MakeTransactionRef(mtx);
        mempool_transactions.push_back(ptx);
        if (amount_per_output > 2000) {
            // If the value is high enough to fund another transaction + fees, keep track of it so
            // it can be used to build a more complex transaction graph. Insert randomly into
            // unspent_prevouts for extra randomness in the resulting structures.
            for (size_t n{0}; n < num_outputs; ++n) {
                unspent_prevouts.push_back(std::make_pair(COutPoint(ptx->GetHash(), n), amount_per_output));
                std::swap(unspent_prevouts.back(), unspent_prevouts[det_rand.randrange(unspent_prevouts.size())]);
            }
        }
        if (submit) {
            LOCK2(m_node.mempool->cs, cs_main);
            LockPoints lp;
            m_node.mempool->addUnchecked(CTxMemPoolEntry(ptx, 1000, 0, 1, false, 4, lp));
        }
        --num_transactions;
    }
    return mempool_transactions;
}

CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(const CMutableTransaction& tx) const
{
    return FromTx(MakeTransactionRef(tx));
}

CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(const CTransactionRef& tx) const
{
    return CTxMemPoolEntry(tx, nFee, nTime, nHeight,
                           spendsCoinbase, sigOpCost, lp);
}

/**
 * @returns a real block (5564fe1673f46378ea6417d8a9c04ea4898d1f470e979da1ebad562c38f7d571, Height 1323958)
 *      with 6 txs.
 */
CBlock getBlock5564f()
{
    CBlock block;
    CDataStream stream(ParseHex("000000208faded5983fd5bb5d23ca0b039b2cf91d4ca1d4db5a3b02c01b5c08a0d410a980bf3ee62c22d90bc2299352d1862f320c7e05cccabd6c643c3675664bdef8970d2c2f45e00000000009f0402059c1b2530726d8247879274fada96b3f3b9834edfb0f3d8805a5a7546638d7d06010000000001010000000000000000000000000000000000000000000000000000000000000000ffffffff0c03b633142f724d2f8617407affffffff02a7270395000000001600140ad73d094eca6d83cbcb7f921c6d0b70d36cbd720000000000000000266a24aa21a9ed6ef1aad21082ac1fb190c61e06f019640e7be548ec228625b3b782b2b51bb1ed01200000000000000000000000000000000000000000000000000000000000000000000000000100000001079abb2b2723515ff6cdb74f292a5c498332e99c2a86a5a0150563f4da5334b2000000006a47304402204b6db43519c851f422a73a89cd4ee94495b93c17caac1bacbba87a4dcc943bfe02201b645409a1d69034d2f56973b574114731b5bbdb3b9ffd3a08042785486ee54401210399f145c396c4940e346a11db221fedbf0228ee37242af39f2f60757a5f8510f1feffffff02abdc4f2f000000001976a914b54bfe1a783c71c02f3441d2e5eb102c60532db888ac7c67f741000000001976a914e4781b5c9370fe713fede1d89792b022608ed83288acb533140001000000010efb6b79553bb6fe3b51f7219ae600f9865213e3f06f4e23b30f225e5b9f4f49010000006a47304402203da145cbe6891a2244a7b8eff513d3311d0dd1db1b5d6bda8d70b323a9b9939402207e2354226e3823171d08cbb4f3ceb1eaaaca8eab7ceb3620ea11090c8bd50c77012102902c7cd4a14daeac63b9c075c4f43913b823f49350d15374017e0c283161c3defeffffff02d4b3d834000000001976a914081b1e2c58cb246341f73c15fbc2451022d96b8a88ace8d6230d000000001976a9144ac409c80bca26cdec87817dc0c6956f123eea0588acb53314000100000001c402acc13df1f4331f70434ff32da501f0d7f6c53fe823c081cc594953abd035010000006a473044022051f1ff30170a75d9b1cc70ad5eb097fbf8d88d247992d67f97ba0c7451003c320220705537f41fb4ce6e031ad1a0f7710fdc793e6faf127891e75252ec8cf9c0ad6a012103a4ffadecf271cf62ebf6cbc1ec9fa1dfb36146d62dbc88ddd7a8075d44b8ceccfeffffff0256fa947e000000001976a914e1b9572d307a8c1158c507589dd21328e562142788ac6af66d16000000001976a914bced4c4a28579dfd2edba27995954fc6e7f492b588acb53314000100000002436785bb90c8463ef78048311c0c25533e837c31c673b87b3f971fdf95980553010000006a47304402203bb68e43eb881e9953d1b49cfa93b3376f6498913b541f7996e8adf47f9e045402204335ca43f9ad165b59e1b037f9cfd602827f746bbfd703459076605f06cb1e4501210329da5b40a0fa877f2e1a3fd2296144b6f6b74bb636feef242fc5a20116b7ac44feffffffc9c80505b16ef9da1751b5bcf9bfaeee9128622cd452e28ac4291cfb7ea41a0d000000006b483045022100af48d711e4efab9e1f52df17281664cdca5e7b9f43b77f827951e560112b8ced02203f4931f94f922fc3ae4f02fa368f9d2eeb0b612744af66d0cef8911981bf7823012102157f5deb06c50045e9fa0724e4bb5c05a696fb3ba446fdf353c28d47cbaf70d8feffffff020fc3375e000000001976a91475f87a6fc2562cf6096313030b170da38f8c635588acf3031f00000000001976a9148633750417127ce58ff40bfe0e966cb82b07f48988acb53314000100000001ce66ebc2beeb5a4c6feb40bf375ab644a0576695cdbde31a4e974d0d16794e69010000006b483045022100933749d80ed779aea9bdd0857f885b65c66cf3ee2a71ea90aac02f71f6c543e80220659a4b864696ddb7c5da9201c22518605ea2f71d2faaf2e8c8d02100e3cff1600121035be414af5ea7081e8fc313ce8a7c42247ba5e4c659dd16af9ee71d496f2dff81feffffff02a4bbbc1e000000001976a9148ddfb0eb2aaa5aec31a501ecd68d9748cf87cbd988ac82f65d6b000000001976a9147f2171e3d70b1227823eb9453db1807be7e304c388acb5331400"), SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;
    return block;
}
