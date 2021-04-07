// Copyright (c) 2011-2020 The Bitcoin Core developers
// Copyright (c) 2013-2020 The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/setup_common.h>

#include <banman.h>
#include <chainparams.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/validation.h>
#include <crypto/sha256.h>
#include <init.h>
#include <interfaces/chain.h>
#include <miner.h>
#include <net.h>
#include <net_processing.h>
#include <noui.h>
#include <pow.h>
#include <rpc/blockchain.h>
#include <rpc/register.h>
#include <rpc/server.h>
#include <scheduler.h>
#include <script/sigcache.h>
#include <streams.h>
#include <txdb.h>
#include <util/memory.h>
#include <util/strencodings.h>
#include <util/string.h>
#include <util/time.h>
#include <util/translation.h>
#include <util/url.h>
#include <util/vector.h>
#include <validation.h>
#include <validationinterface.h>
#include <walletinitinterface.h>

#include <functional>

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
    : m_path_root{fs::temp_directory_path() / "test_common_" PACKAGE_NAME / g_insecure_rand_ctx_temp_path.rand256().ToString()}
{
    const std::vector<const char*> arguments = Cat(
        {
            "dummy",
            "-printtoconsole=0",
            "-logtimemicros",
            "-logthreadnames",
            "-debug",
            "-debugexclude=libevent",
            "-debugexclude=leveldb",
        },
        extra_args);
    util::ThreadRename("test");
    fs::create_directories(m_path_root);
    gArgs.ForceSetArg("-datadir", m_path_root.string());
    ClearDatadirCache();
    {
        SetupServerArgs(m_node);
        std::string error;
        const bool success{m_node.args->ParseParameters(arguments.size(), arguments.data(), error)};
        assert(success);
        assert(error.empty());
    }
    SelectParams(chainName);
    SeedInsecureRand();
    if (G_TEST_LOG_FUN) LogInstance().PushBackCallback(G_TEST_LOG_FUN);
    InitLogging(*m_node.args);
    AppInitParameterInteraction(*m_node.args);
    LogInstance().StartLogging();
    SHA256AutoDetect();
    ECC_Start();
    SetupEnvironment();
    SetupNetworking();
    InitSignatureCache();
    InitScriptExecutionCache();
    m_node.chain = interfaces::MakeChain(m_node);
    g_wallet_init_interface.Construct(m_node);
    fCheckBlockIndex = true;
    static bool noui_connected = false;
    if (!noui_connected) {
        noui_connect();
        noui_connected = true;
    }
}

BasicTestingSetup::~BasicTestingSetup()
{
    LogInstance().DisconnectTestLogger();
    fs::remove_all(m_path_root);
    gArgs.ClearArgs();
    ECC_Stop();
}

TestingSetup::TestingSetup(const std::string& chainName, const std::vector<const char*>& extra_args)
    : BasicTestingSetup(chainName, extra_args)
{
    const CChainParams& chainparams = Params();
    // Ideally we'd move all the RPC tests to the functional testing framework
    // instead of unit tests, but for now we need these here.
    RegisterAllCoreRPCCommands(tableRPC);

    m_node.scheduler = MakeUnique<CScheduler>();

    // We have to run a scheduler thread to prevent ActivateBestChain
    // from blocking due to queue overrun.
    threadGroup.create_thread([&] { TraceThread("scheduler", [&] { m_node.scheduler->serviceQueue(); }); });
    GetMainSignals().RegisterBackgroundSignalScheduler(*m_node.scheduler);

    pblocktree.reset(new CBlockTreeDB(1 << 20, true));

    m_node.mempool = MakeUnique<CTxMemPool>(&::feeEstimator);
    m_node.mempool->setSanityCheck(1.0);

    m_node.chainman = &::g_chainman;
    m_node.chainman->InitializeChainstate(*m_node.mempool);
    ::ChainstateActive().InitCoinsDB(
        /* cache_size_bytes */ 1 << 23, /* in_memory */ true, /* should_wipe */ false);
    assert(!::ChainstateActive().CanFlushToDisk());
    ::ChainstateActive().InitCoinsCache(1 << 23);
    assert(::ChainstateActive().CanFlushToDisk());
    if (!LoadGenesisBlock(chainparams)) {
        throw std::runtime_error("LoadGenesisBlock failed.");
    }

    BlockValidationState state;
    if (!ActivateBestChain(state, chainparams)) {
        throw std::runtime_error(strprintf("ActivateBestChain failed. (%s)", state.ToString()));
    }

    // Start script-checking threads. Set g_parallel_script_checks to true so they are used.
    constexpr int script_check_threads = 2;
    for (int i = 0; i < script_check_threads; ++i) {
        threadGroup.create_thread([i]() { return ThreadScriptCheck(i); });
    }
    g_parallel_script_checks = true;

    m_node.banman = MakeUnique<BanMan>(GetDataDir() / "banlist.dat", nullptr, DEFAULT_MISBEHAVING_BANTIME);
    m_node.connman = MakeUnique<CConnman>(0x1337, 0x1337); // Deterministic randomness for tests.
    m_node.peerman = MakeUnique<PeerManager>(chainparams, *m_node.connman, m_node.banman.get(), *m_node.scheduler, *m_node.chainman, *m_node.mempool);
    {
        CConnman::Options options;
        options.m_msgproc = m_node.peerman.get();
        m_node.connman->Init(options);
    }
}

TestingSetup::~TestingSetup()
{
    if (m_node.scheduler) m_node.scheduler->stop();
    threadGroup.interrupt_all();
    threadGroup.join_all();
    GetMainSignals().FlushBackgroundCallbacks();
    GetMainSignals().UnregisterBackgroundSignalScheduler();
    m_node.connman.reset();
    m_node.banman.reset();
    m_node.args = nullptr;
    UnloadBlockIndex(m_node.mempool.get(), *m_node.chainman);
    m_node.mempool.reset();
    m_node.scheduler.reset();
    m_node.chainman->Reset();
    m_node.chainman = nullptr;
    pblocktree.reset();
}

TestChain100Setup::TestChain100Setup()
{
    // Generate a 100-block chain:
    coinbaseKey.MakeNewKey(true);
    CScript scriptPubKey = CScript() << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    for (int i = 0; i < COINBASE_MATURITY; i++) {
        std::vector<CMutableTransaction> noTxns;
        CBlock b = CreateAndProcessBlock(noTxns, scriptPubKey);
        m_coinbase_txns.push_back(b.vtx[0]);
    }
}

CBlock TestChain100Setup::CreateAndProcessBlock(const std::vector<CMutableTransaction>& txns, const CScript& scriptPubKey)
{
    const CChainParams& chainparams = Params();
    CTxMemPool empty_pool;
    CBlock block = BlockAssembler(empty_pool, chainparams).CreateNewBlock(scriptPubKey)->block;

    Assert(block.vtx.size() == 1);
    for (const CMutableTransaction& tx : txns) {
        block.vtx.push_back(MakeTransactionRef(tx));
    }
    RegenerateCommitments(block);

    while (!CheckProofOfWork(block.GetHashForPoW(), block.nBits, ArithToUint256(block.nOffset), chainparams.GetConsensus())) block.nOffset += 2;

    std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(block);
    Assert(m_node.chainman)->ProcessNewBlock(chainparams, shared_pblock, true, nullptr);

    return block;
}

TestChain100Setup::~TestChain100Setup()
{
    gArgs.ForceSetArg("-segwitheight", "0");
}

CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(const CMutableTransaction& tx)
{
    return FromTx(MakeTransactionRef(tx));
}

CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(const CTransactionRef& tx)
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
