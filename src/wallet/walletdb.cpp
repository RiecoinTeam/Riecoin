// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2013-present The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bitcoin-build-config.h> // IWYU pragma: keep

#include <wallet/walletdb.h>

#include <common/system.h>
#include <key_io.h>
#include <protocol.h>
#include <script/script.h>
#include <serialize.h>
#include <sync.h>
#include <util/bip32.h>
#include <util/check.h>
#include <util/fs.h>
#include <util/time.h>
#include <util/translation.h>
#ifdef USE_SQLITE
#include <wallet/sqlite.h>
#endif
#include <wallet/wallet.h>

#include <atomic>
#include <optional>
#include <string>

namespace wallet {
namespace DBKeys {
const std::string ACENTRY{"acentry"};
const std::string ACTIVEEXTERNALSPK{"activeexternalspk"};
const std::string ACTIVEINTERNALSPK{"activeinternalspk"};
const std::string BESTBLOCK_NOMERKLE{"bestblock_nomerkle"};
const std::string BESTBLOCK{"bestblock"};
const std::string DESTDATA{"destdata"};
const std::string FLAGS{"flags"};
const std::string LOCKED_UTXO{"lockedutxo"};
const std::string MASTER_KEY{"mkey"};
const std::string MINVERSION{"minversion"};
const std::string NAME{"name"};
const std::string ORDERPOSNEXT{"orderposnext"};
const std::string PURPOSE{"purpose"};
const std::string SETTINGS{"settings"};
const std::string TX{"tx"};
const std::string VERSION{"version"};
const std::string WALLETDESCRIPTOR{"walletdescriptor"};
const std::string WALLETDESCRIPTORCACHE{"walletdescriptorcache"};
const std::string WALLETDESCRIPTORLHCACHE{"walletdescriptorlhcache"};
const std::string WALLETDESCRIPTORCKEY{"walletdescriptorckey"};
const std::string WALLETDESCRIPTORKEY{"walletdescriptorkey"};
} // namespace DBKeys

//
// WalletBatch
//

bool WalletBatch::WriteName(const std::string& strAddress, const std::string& strName)
{
    return WriteIC(std::make_pair(DBKeys::NAME, strAddress), strName);
}

bool WalletBatch::EraseName(const std::string& strAddress)
{
    // This should only be used for sending addresses, never for receiving addresses,
    // receiving addresses must always have an address book entry if they're not change return.
    return EraseIC(std::make_pair(DBKeys::NAME, strAddress));
}

bool WalletBatch::WritePurpose(const std::string& strAddress, const std::string& strPurpose)
{
    return WriteIC(std::make_pair(DBKeys::PURPOSE, strAddress), strPurpose);
}

bool WalletBatch::ErasePurpose(const std::string& strAddress)
{
    return EraseIC(std::make_pair(DBKeys::PURPOSE, strAddress));
}

bool WalletBatch::WriteTx(const CWalletTx& wtx)
{
    return WriteIC(std::make_pair(DBKeys::TX, wtx.GetHash()), wtx);
}

bool WalletBatch::EraseTx(uint256 hash)
{
    return EraseIC(std::make_pair(DBKeys::TX, hash));
}

bool WalletBatch::WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey)
{
    return WriteIC(std::make_pair(DBKeys::MASTER_KEY, nID), kMasterKey, true);
}

bool WalletBatch::WriteBestBlock(const CBlockLocator& locator)
{
    WriteIC(DBKeys::BESTBLOCK, CBlockLocator()); // Write empty block locator so versions that require a merkle branch automatically rescan
    return WriteIC(DBKeys::BESTBLOCK_NOMERKLE, locator);
}

bool WalletBatch::ReadBestBlock(CBlockLocator& locator)
{
    if (m_batch->Read(DBKeys::BESTBLOCK, locator) && !locator.vHave.empty()) return true;
    return m_batch->Read(DBKeys::BESTBLOCK_NOMERKLE, locator);
}

bool WalletBatch::WriteOrderPosNext(int64_t nOrderPosNext)
{
    return WriteIC(DBKeys::ORDERPOSNEXT, nOrderPosNext);
}

bool WalletBatch::WriteMinVersion(int nVersion)
{
    return WriteIC(DBKeys::MINVERSION, nVersion);
}

bool WalletBatch::WriteActiveScriptPubKeyMan(uint8_t type, const uint256& id, bool internal)
{
    std::string key = internal ? DBKeys::ACTIVEINTERNALSPK : DBKeys::ACTIVEEXTERNALSPK;
    return WriteIC(make_pair(key, type), id);
}

bool WalletBatch::EraseActiveScriptPubKeyMan(uint8_t type, bool internal)
{
    const std::string key{internal ? DBKeys::ACTIVEINTERNALSPK : DBKeys::ACTIVEEXTERNALSPK};
    return EraseIC(make_pair(key, type));
}

bool WalletBatch::WriteDescriptorKey(const uint256& desc_id, const CPubKey& pubkey, const CPrivKey& privkey)
{
    // hash pubkey/privkey to accelerate wallet load
    std::vector<unsigned char> key;
    key.reserve(pubkey.size() + privkey.size());
    key.insert(key.end(), pubkey.begin(), pubkey.end());
    key.insert(key.end(), privkey.begin(), privkey.end());

    return WriteIC(std::make_pair(DBKeys::WALLETDESCRIPTORKEY, std::make_pair(desc_id, pubkey)), std::make_pair(privkey, Hash(key)), false);
}

bool WalletBatch::WriteCryptedDescriptorKey(const uint256& desc_id, const CPubKey& pubkey, const std::vector<unsigned char>& secret)
{
    if (!WriteIC(std::make_pair(DBKeys::WALLETDESCRIPTORCKEY, std::make_pair(desc_id, pubkey)), secret, false)) {
        return false;
    }
    EraseIC(std::make_pair(DBKeys::WALLETDESCRIPTORKEY, std::make_pair(desc_id, pubkey)));
    return true;
}

bool WalletBatch::WriteDescriptor(const uint256& desc_id, const WalletDescriptor& descriptor)
{
    return WriteIC(make_pair(DBKeys::WALLETDESCRIPTOR, desc_id), descriptor);
}

bool WalletBatch::WriteDescriptorDerivedCache(const CExtPubKey& xpub, const uint256& desc_id, uint32_t key_exp_index, uint32_t der_index)
{
    std::vector<unsigned char> ser_xpub(BIP32_EXTKEY_SIZE);
    xpub.Encode(ser_xpub.data());
    return WriteIC(std::make_pair(std::make_pair(DBKeys::WALLETDESCRIPTORCACHE, desc_id), std::make_pair(key_exp_index, der_index)), ser_xpub);
}

bool WalletBatch::WriteDescriptorParentCache(const CExtPubKey& xpub, const uint256& desc_id, uint32_t key_exp_index)
{
    std::vector<unsigned char> ser_xpub(BIP32_EXTKEY_SIZE);
    xpub.Encode(ser_xpub.data());
    return WriteIC(std::make_pair(std::make_pair(DBKeys::WALLETDESCRIPTORCACHE, desc_id), key_exp_index), ser_xpub);
}

bool WalletBatch::WriteDescriptorLastHardenedCache(const CExtPubKey& xpub, const uint256& desc_id, uint32_t key_exp_index)
{
    std::vector<unsigned char> ser_xpub(BIP32_EXTKEY_SIZE);
    xpub.Encode(ser_xpub.data());
    return WriteIC(std::make_pair(std::make_pair(DBKeys::WALLETDESCRIPTORLHCACHE, desc_id), key_exp_index), ser_xpub);
}

bool WalletBatch::WriteDescriptorCacheItems(const uint256& desc_id, const DescriptorCache& cache)
{
    for (const auto& parent_xpub_pair : cache.GetCachedParentExtPubKeys()) {
        if (!WriteDescriptorParentCache(parent_xpub_pair.second, desc_id, parent_xpub_pair.first)) {
            return false;
        }
    }
    for (const auto& derived_xpub_map_pair : cache.GetCachedDerivedExtPubKeys()) {
        for (const auto& derived_xpub_pair : derived_xpub_map_pair.second) {
            if (!WriteDescriptorDerivedCache(derived_xpub_pair.second, desc_id, derived_xpub_map_pair.first, derived_xpub_pair.first)) {
                return false;
            }
        }
    }
    for (const auto& lh_xpub_pair : cache.GetCachedLastHardenedExtPubKeys()) {
        if (!WriteDescriptorLastHardenedCache(lh_xpub_pair.second, desc_id, lh_xpub_pair.first)) {
            return false;
        }
    }
    return true;
}

bool WalletBatch::WriteLockedUTXO(const COutPoint& output)
{
    return WriteIC(std::make_pair(DBKeys::LOCKED_UTXO, std::make_pair(output.hash, output.n)), uint8_t{'1'});
}

bool WalletBatch::EraseLockedUTXO(const COutPoint& output)
{
    return EraseIC(std::make_pair(DBKeys::LOCKED_UTXO, std::make_pair(output.hash, output.n)));
}

bool LoadEncryptionKey(CWallet* pwallet, DataStream& ssKey, DataStream& ssValue, std::string& strErr)
{
    LOCK(pwallet->cs_wallet);
    try {
        // Master encryption key is loaded into only the wallet and not any of the ScriptPubKeyMans.
        unsigned int nID;
        ssKey >> nID;
        CMasterKey kMasterKey;
        ssValue >> kMasterKey;
        if(pwallet->mapMasterKeys.count(nID) != 0)
        {
            strErr = strprintf("Error reading wallet database: duplicate CMasterKey id %u", nID);
            return false;
        }
        pwallet->mapMasterKeys[nID] = kMasterKey;
        if (pwallet->nMasterKeyMaxID < nID)
            pwallet->nMasterKeyMaxID = nID;

    } catch (const std::exception& e) {
        if (strErr.empty()) {
            strErr = e.what();
        }
        return false;
    }
    return true;
}

static DBErrors LoadMinVersion(CWallet* pwallet, DatabaseBatch& batch) EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet)
{
    AssertLockHeld(pwallet->cs_wallet);
    int nMinVersion = 0;
    if (batch.Read(DBKeys::MINVERSION, nMinVersion)) {
        if (nMinVersion > FEATURE_LATEST)
            return DBErrors::TOO_NEW;
        pwallet->LoadMinVersion(nMinVersion);
    }
    return DBErrors::LOAD_OK;
}

static DBErrors LoadWalletFlags(CWallet* pwallet, DatabaseBatch& batch) EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet)
{
    AssertLockHeld(pwallet->cs_wallet);
    uint64_t flags;
    if (batch.Read(DBKeys::FLAGS, flags)) {
        if (!pwallet->LoadWalletFlags(flags)) {
            pwallet->WalletLogPrintf("Error reading wallet database: Unknown non-tolerable wallet flags found\n");
            return DBErrors::TOO_NEW;
        }
    }
    return DBErrors::LOAD_OK;
}

struct LoadResult
{
    DBErrors m_result{DBErrors::LOAD_OK};
    int m_records{0};
};

using LoadFunc = std::function<DBErrors(CWallet* pwallet, DataStream& key, DataStream& value, std::string& err)>;
static LoadResult LoadRecords(CWallet* pwallet, DatabaseBatch& batch, const std::string& key, DataStream& prefix, LoadFunc load_func)
{
    LoadResult result;
    DataStream ssKey;
    DataStream ssValue{};

    Assume(!prefix.empty());
    std::unique_ptr<DatabaseCursor> cursor = batch.GetNewPrefixCursor(prefix);
    if (!cursor) {
        pwallet->WalletLogPrintf("Error getting database cursor for '%s' records\n", key);
        result.m_result = DBErrors::CORRUPT;
        return result;
    }

    while (true) {
        DatabaseCursor::Status status = cursor->Next(ssKey, ssValue);
        if (status == DatabaseCursor::Status::DONE) {
            break;
        } else if (status == DatabaseCursor::Status::FAIL) {
            pwallet->WalletLogPrintf("Error reading next '%s' record for wallet database\n", key);
            result.m_result = DBErrors::CORRUPT;
            return result;
        }
        std::string type;
        ssKey >> type;
        assert(type == key);
        std::string error;
        DBErrors record_res = load_func(pwallet, ssKey, ssValue, error);
        if (record_res != DBErrors::LOAD_OK) {
            pwallet->WalletLogPrintf("%s\n", error);
        }
        result.m_result = std::max(result.m_result, record_res);
        ++result.m_records;
    }
    return result;
}

static LoadResult LoadRecords(CWallet* pwallet, DatabaseBatch& batch, const std::string& key, LoadFunc load_func)
{
    DataStream prefix;
    prefix << key;
    return LoadRecords(pwallet, batch, key, prefix, load_func);
}

template<typename... Args>
static DataStream PrefixStream(const Args&... args)
{
    DataStream prefix;
    SerializeMany(prefix, args...);
    return prefix;
}

static DBErrors LoadDescriptorWalletRecords(CWallet* pwallet, DatabaseBatch& batch, int last_client) EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet)
{
    AssertLockHeld(pwallet->cs_wallet);

    // Load descriptor record
    int num_keys = 0;
    int num_ckeys= 0;
    LoadResult desc_res = LoadRecords(pwallet, batch, DBKeys::WALLETDESCRIPTOR,
        [&batch, &num_keys, &num_ckeys, &last_client] (CWallet* pwallet, DataStream& key, DataStream& value, std::string& strErr) {
        DBErrors result = DBErrors::LOAD_OK;

        uint256 id;
        key >> id;
        WalletDescriptor desc;
        try {
            value >> desc;
        } catch (const std::ios_base::failure& e) {
            strErr = strprintf("Error: Unrecognized descriptor found in wallet %s. ", pwallet->GetName());
            strErr += (last_client > CLIENT_VERSION) ? "The wallet might had been created on a newer version. " :
                    "The database might be corrupted or the software version is not compatible with one of your wallet descriptors. ";
            strErr += "Please try running the latest software version";
            // Also include error details
            strErr = strprintf("%s\nDetails: %s", strErr, e.what());
            return DBErrors::UNKNOWN_DESCRIPTOR;
        }
        DescriptorScriptPubKeyMan& spkm = pwallet->LoadDescriptorScriptPubKeyMan(id, desc);

        // Prior to doing anything with this spkm, verify ID compatibility
        if (id != spkm.GetID()) {
            strErr = "The descriptor ID calculated by the wallet differs from the one in DB";
            return DBErrors::CORRUPT;
        }

        DescriptorCache cache;

        // Get key cache for this descriptor
        DataStream prefix = PrefixStream(DBKeys::WALLETDESCRIPTORCACHE, id);
        LoadResult key_cache_res = LoadRecords(pwallet, batch, DBKeys::WALLETDESCRIPTORCACHE, prefix,
            [&id, &cache] (CWallet* pwallet, DataStream& key, DataStream& value, std::string& err) {
            bool parent = true;
            uint256 desc_id;
            uint32_t key_exp_index;
            uint32_t der_index;
            key >> desc_id;
            assert(desc_id == id);
            key >> key_exp_index;

            // if the der_index exists, it's a derived xpub
            try
            {
                key >> der_index;
                parent = false;
            }
            catch (...) {}

            std::vector<unsigned char> ser_xpub(BIP32_EXTKEY_SIZE);
            value >> ser_xpub;
            CExtPubKey xpub;
            xpub.Decode(ser_xpub.data());
            if (parent) {
                cache.CacheParentExtPubKey(key_exp_index, xpub);
            } else {
                cache.CacheDerivedExtPubKey(key_exp_index, der_index, xpub);
            }
            return DBErrors::LOAD_OK;
        });
        result = std::max(result, key_cache_res.m_result);

        // Get last hardened cache for this descriptor
        prefix = PrefixStream(DBKeys::WALLETDESCRIPTORLHCACHE, id);
        LoadResult lh_cache_res = LoadRecords(pwallet, batch, DBKeys::WALLETDESCRIPTORLHCACHE, prefix,
            [&id, &cache] (CWallet* pwallet, DataStream& key, DataStream& value, std::string& err) {
            uint256 desc_id;
            uint32_t key_exp_index;
            key >> desc_id;
            assert(desc_id == id);
            key >> key_exp_index;

            std::vector<unsigned char> ser_xpub(BIP32_EXTKEY_SIZE);
            value >> ser_xpub;
            CExtPubKey xpub;
            xpub.Decode(ser_xpub.data());
            cache.CacheLastHardenedExtPubKey(key_exp_index, xpub);
            return DBErrors::LOAD_OK;
        });
        result = std::max(result, lh_cache_res.m_result);

        // Set the cache for this descriptor
        auto spk_man = (DescriptorScriptPubKeyMan*)pwallet->GetScriptPubKeyMan(id);
        assert(spk_man);
        spk_man->SetCache(cache);

        // Get unencrypted keys
        prefix = PrefixStream(DBKeys::WALLETDESCRIPTORKEY, id);
        LoadResult key_res = LoadRecords(pwallet, batch, DBKeys::WALLETDESCRIPTORKEY, prefix,
            [&id, &spk_man] (CWallet* pwallet, DataStream& key, DataStream& value, std::string& strErr) {
            uint256 desc_id;
            CPubKey pubkey;
            key >> desc_id;
            assert(desc_id == id);
            key >> pubkey;
            if (!pubkey.IsValid())
            {
                strErr = "Error reading wallet database: descriptor unencrypted key CPubKey corrupt";
                return DBErrors::CORRUPT;
            }
            CKey privkey;
            CPrivKey pkey;
            uint256 hash;

            value >> pkey;
            value >> hash;

            // hash pubkey/privkey to accelerate wallet load
            std::vector<unsigned char> to_hash;
            to_hash.reserve(pubkey.size() + pkey.size());
            to_hash.insert(to_hash.end(), pubkey.begin(), pubkey.end());
            to_hash.insert(to_hash.end(), pkey.begin(), pkey.end());

            if (Hash(to_hash) != hash)
            {
                strErr = "Error reading wallet database: descriptor unencrypted key CPubKey/CPrivKey corrupt";
                return DBErrors::CORRUPT;
            }

            if (!privkey.Load(pkey, pubkey, true))
            {
                strErr = "Error reading wallet database: descriptor unencrypted key CPrivKey corrupt";
                return DBErrors::CORRUPT;
            }
            spk_man->AddKey(pubkey.GetID(), privkey);
            return DBErrors::LOAD_OK;
        });
        result = std::max(result, key_res.m_result);
        num_keys = key_res.m_records;

        // Get encrypted keys
        prefix = PrefixStream(DBKeys::WALLETDESCRIPTORCKEY, id);
        LoadResult ckey_res = LoadRecords(pwallet, batch, DBKeys::WALLETDESCRIPTORCKEY, prefix,
            [&id, &spk_man] (CWallet* pwallet, DataStream& key, DataStream& value, std::string& err) {
            uint256 desc_id;
            CPubKey pubkey;
            key >> desc_id;
            assert(desc_id == id);
            key >> pubkey;
            if (!pubkey.IsValid())
            {
                err = "Error reading wallet database: descriptor encrypted key CPubKey corrupt";
                return DBErrors::CORRUPT;
            }
            std::vector<unsigned char> privkey;
            value >> privkey;

            spk_man->AddCryptedKey(pubkey.GetID(), pubkey, privkey);
            return DBErrors::LOAD_OK;
        });
        result = std::max(result, ckey_res.m_result);
        num_ckeys = ckey_res.m_records;

        return result;
    });

    if (desc_res.m_result <= DBErrors::NONCRITICAL_ERROR) {
        // Only log if there are no critical errors
        pwallet->WalletLogPrintf("Descriptors: %u, Descriptor Keys: %u plaintext, %u encrypted, %u total.\n",
               desc_res.m_records, num_keys, num_ckeys, num_keys + num_ckeys);
    }

    return desc_res.m_result;
}

static DBErrors LoadAddressBookRecords(CWallet* pwallet, DatabaseBatch& batch) EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet)
{
    AssertLockHeld(pwallet->cs_wallet);
    DBErrors result = DBErrors::LOAD_OK;

    // Load name record
    LoadResult name_res = LoadRecords(pwallet, batch, DBKeys::NAME,
        [] (CWallet* pwallet, DataStream& key, DataStream& value, std::string& err) EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet) {
        std::string strAddress;
        key >> strAddress;
        std::string label;
        value >> label;
        pwallet->m_address_book[DecodeDestination(strAddress)].SetLabel(label);
        return DBErrors::LOAD_OK;
    });
    result = std::max(result, name_res.m_result);

    // Load purpose record
    LoadResult purpose_res = LoadRecords(pwallet, batch, DBKeys::PURPOSE,
        [] (CWallet* pwallet, DataStream& key, DataStream& value, std::string& err) EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet) {
        std::string strAddress;
        key >> strAddress;
        std::string purpose_str;
        value >> purpose_str;
        std::optional<AddressPurpose> purpose{PurposeFromString(purpose_str)};
        if (!purpose) {
            pwallet->WalletLogPrintf("Warning: nonstandard purpose string '%s' for address '%s'\n", purpose_str, strAddress);
        }
        pwallet->m_address_book[DecodeDestination(strAddress)].purpose = purpose;
        return DBErrors::LOAD_OK;
    });
    result = std::max(result, purpose_res.m_result);

    // Load destination data record
    LoadResult dest_res = LoadRecords(pwallet, batch, DBKeys::DESTDATA,
        [] (CWallet* pwallet, DataStream& key, DataStream& value, std::string& err) EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet) {
        std::string strAddress, strKey, strValue;
        key >> strAddress;
        key >> strKey;
        value >> strValue;
        const CTxDestination& dest{DecodeDestination(strAddress)};
        if (strKey.compare("used") == 0) {
            // Load "used" key indicating if an IsMine address has
            // previously been spent from with avoid_reuse option enabled.
            // The strValue is not used for anything currently, but could
            // hold more information in the future. Current values are just
            // "1" or "p" for present (which was written prior to
            // f5ba424cd44619d9b9be88b8593d69a7ba96db26).
            pwallet->LoadAddressPreviouslySpent(dest);
        } else if (strKey.compare(0, 2, "rr") == 0) {
            // Load "rr##" keys where ## is a decimal number, and strValue
            // is a serialized RecentRequestEntry object.
            pwallet->LoadAddressReceiveRequest(dest, strKey.substr(2), strValue);
        }
        return DBErrors::LOAD_OK;
    });
    result = std::max(result, dest_res.m_result);

    return result;
}

static DBErrors LoadTxRecords(CWallet* pwallet, DatabaseBatch& batch, std::vector<uint256>& upgraded_txs, bool& any_unordered) EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet)
{
    AssertLockHeld(pwallet->cs_wallet);
    DBErrors result = DBErrors::LOAD_OK;

    // Load tx record
    any_unordered = false;
    LoadResult tx_res = LoadRecords(pwallet, batch, DBKeys::TX,
        [&any_unordered, &upgraded_txs] (CWallet* pwallet, DataStream& key, DataStream& value, std::string& err) EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet) {
        DBErrors result = DBErrors::LOAD_OK;
        uint256 hash;
        key >> hash;
        // LoadToWallet call below creates a new CWalletTx that fill_wtx
        // callback fills with transaction metadata.
        auto fill_wtx = [&](CWalletTx& wtx, bool new_tx) {
            if(!new_tx) {
                // There's some corruption here since the tx we just tried to load was already in the wallet.
                err = "Error: Corrupt transaction found. This can be fixed by removing transactions from wallet and rescanning.";
                result = DBErrors::CORRUPT;
                return false;
            }
            value >> wtx;
            if (wtx.GetHash() != hash)
                return false;

            // Undo serialize changes in 31600
            if (31404 <= wtx.fTimeReceivedIsTxTime && wtx.fTimeReceivedIsTxTime <= 31703)
            {
                if (!value.empty())
                {
                    uint8_t fTmp;
                    uint8_t fUnused;
                    std::string unused_string;
                    value >> fTmp >> fUnused >> unused_string;
                    pwallet->WalletLogPrintf("LoadWallet() upgrading tx ver=%d %d %s\n",
                                       wtx.fTimeReceivedIsTxTime, fTmp, hash.ToString());
                    wtx.fTimeReceivedIsTxTime = fTmp;
                }
                else
                {
                    pwallet->WalletLogPrintf("LoadWallet() repairing tx ver=%d %s\n", wtx.fTimeReceivedIsTxTime, hash.ToString());
                    wtx.fTimeReceivedIsTxTime = 0;
                }
                upgraded_txs.push_back(hash);
            }

            if (wtx.nOrderPos == -1)
                any_unordered = true;

            return true;
        };
        if (!pwallet->LoadToWallet(hash, fill_wtx)) {
            // Use std::max as fill_wtx may have already set result to CORRUPT
            result = std::max(result, DBErrors::NEED_RESCAN);
        }
        return result;
    });
    result = std::max(result, tx_res.m_result);

    // Load locked utxo record
    LoadResult locked_utxo_res = LoadRecords(pwallet, batch, DBKeys::LOCKED_UTXO,
        [] (CWallet* pwallet, DataStream& key, DataStream& value, std::string& err) EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet) {
        Txid hash;
        uint32_t n;
        key >> hash;
        key >> n;
        pwallet->LockCoin(COutPoint(hash, n));
        return DBErrors::LOAD_OK;
    });
    result = std::max(result, locked_utxo_res.m_result);

    // Load orderposnext record
    // Note: There should only be one ORDERPOSNEXT record with nothing trailing the type
    LoadResult order_pos_res = LoadRecords(pwallet, batch, DBKeys::ORDERPOSNEXT,
        [] (CWallet* pwallet, DataStream& key, DataStream& value, std::string& err) EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet) {
        try {
            value >> pwallet->nOrderPosNext;
        } catch (const std::exception& e) {
            err = e.what();
            return DBErrors::NONCRITICAL_ERROR;
        }
        return DBErrors::LOAD_OK;
    });
    result = std::max(result, order_pos_res.m_result);

    return result;
}

static DBErrors LoadActiveSPKMs(CWallet* pwallet, DatabaseBatch& batch) EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet)
{
    AssertLockHeld(pwallet->cs_wallet);
    DBErrors result = DBErrors::LOAD_OK;

    // Load spk records
    std::set<std::pair<OutputType, bool>> seen_spks;
    for (const auto& spk_key : {DBKeys::ACTIVEEXTERNALSPK, DBKeys::ACTIVEINTERNALSPK}) {
        LoadResult spkm_res = LoadRecords(pwallet, batch, spk_key,
            [&seen_spks, &spk_key] (CWallet* pwallet, DataStream& key, DataStream& value, std::string& strErr) {
            uint8_t output_type;
            key >> output_type;
            uint256 id;
            value >> id;

            bool internal = spk_key == DBKeys::ACTIVEINTERNALSPK;
            auto [it, insert] = seen_spks.emplace(static_cast<OutputType>(output_type), internal);
            if (!insert) {
                strErr = "Multiple ScriptpubKeyMans specified for a single type";
                return DBErrors::CORRUPT;
            }
            pwallet->LoadActiveScriptPubKeyMan(id, static_cast<OutputType>(output_type), /*internal=*/internal);
            return DBErrors::LOAD_OK;
        });
        result = std::max(result, spkm_res.m_result);
    }
    return result;
}

static DBErrors LoadDecryptionKeys(CWallet* pwallet, DatabaseBatch& batch) EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet)
{
    AssertLockHeld(pwallet->cs_wallet);

    // Load decryption key (mkey) records
    LoadResult mkey_res = LoadRecords(pwallet, batch, DBKeys::MASTER_KEY,
        [] (CWallet* pwallet, DataStream& key, DataStream& value, std::string& err) {
        if (!LoadEncryptionKey(pwallet, key, value, err)) {
            return DBErrors::CORRUPT;
        }
        return DBErrors::LOAD_OK;
    });
    return mkey_res.m_result;
}

DBErrors WalletBatch::LoadWallet(CWallet* pwallet)
{
    DBErrors result = DBErrors::LOAD_OK;
    bool any_unordered = false;
    std::vector<uint256> upgraded_txs;

    LOCK(pwallet->cs_wallet);

    // Last client version to open this wallet
    int last_client = CLIENT_VERSION;
    bool has_last_client = m_batch->Read(DBKeys::VERSION, last_client);
    pwallet->WalletLogPrintf("Wallet file version = %d, last client version = %d\n", pwallet->GetVersion(), last_client);

    try {
        if ((result = LoadMinVersion(pwallet, *m_batch)) != DBErrors::LOAD_OK) return result;

        // Load wallet flags, so they are known when processing other records.
        // The FLAGS key is absent during wallet creation.
        if ((result = LoadWalletFlags(pwallet, *m_batch)) != DBErrors::LOAD_OK) return result;

#ifndef ENABLE_EXTERNAL_SIGNER
        if (pwallet->IsWalletFlagSet(WALLET_FLAG_EXTERNAL_SIGNER)) {
            pwallet->WalletLogPrintf("Error: External signer wallet being loaded without external signer support compiled\n");
            return DBErrors::EXTERNAL_SIGNER_SUPPORT_REQUIRED;
        }
#endif

        // Load descriptors
        result = std::max(LoadDescriptorWalletRecords(pwallet, *m_batch, last_client), result);
        // Early return if there are unknown descriptors. Later loading of ACTIVEINTERNALSPK and ACTIVEEXTERNALEXPK
        // may reference the unknown descriptor's ID which can result in a misleading corruption error
        // when in reality the wallet is simply too new.
        if (result == DBErrors::UNKNOWN_DESCRIPTOR) return result;

        // Load address book
        result = std::max(LoadAddressBookRecords(pwallet, *m_batch), result);

        // Load tx records
        result = std::max(LoadTxRecords(pwallet, *m_batch, upgraded_txs, any_unordered), result);

        // Load SPKMs
        result = std::max(LoadActiveSPKMs(pwallet, *m_batch), result);

        // Load decryption keys
        result = std::max(LoadDecryptionKeys(pwallet, *m_batch), result);
    } catch (...) {
        // Exceptions that can be ignored or treated as non-critical are handled by the individual loading functions.
        // Any uncaught exceptions will be caught here and treated as critical.
        result = DBErrors::CORRUPT;
    }

    // Any wallet corruption at all: skip any rewriting or
    // upgrading, we don't want to make it worse.
    if (result != DBErrors::LOAD_OK)
        return result;

    for (const uint256& hash : upgraded_txs)
        WriteTx(pwallet->mapWallet.at(hash));

    if (!has_last_client || last_client != CLIENT_VERSION) // Update
        m_batch->Write(DBKeys::VERSION, CLIENT_VERSION);

    if (any_unordered)
        result = pwallet->ReorderTransactions();

    // Upgrade all of the descriptor caches to cache the last hardened xpub
    // This operation is not atomic, but if it fails, only new entries are added so it is backwards compatible
    try {
        pwallet->UpgradeDescriptorCache();
    } catch (...) {
        result = DBErrors::CORRUPT;
    }

    return result;
}

static bool RunWithinTxn(WalletBatch& batch, std::string_view process_desc, const std::function<bool(WalletBatch&)>& func)
{
    if (!batch.TxnBegin()) {
        LogDebug(BCLog::WALLETDB, "Error: cannot create db txn for %s\n", process_desc);
        return false;
    }

    // Run procedure
    if (!func(batch)) {
        LogDebug(BCLog::WALLETDB, "Error: %s failed\n", process_desc);
        batch.TxnAbort();
        return false;
    }

    if (!batch.TxnCommit()) {
        LogDebug(BCLog::WALLETDB, "Error: cannot commit db txn for %s\n", process_desc);
        return false;
    }

    // All good
    return true;
}

bool RunWithinTxn(WalletDatabase& database, std::string_view process_desc, const std::function<bool(WalletBatch&)>& func)
{
    WalletBatch batch(database);
    return RunWithinTxn(batch, process_desc, func);
}

bool WalletBatch::WriteAddressPreviouslySpent(const CTxDestination& dest, bool previously_spent)
{
    auto key{std::make_pair(DBKeys::DESTDATA, std::make_pair(EncodeDestination(dest), std::string("used")))};
    return previously_spent ? WriteIC(key, std::string("1")) : EraseIC(key);
}

bool WalletBatch::WriteAddressReceiveRequest(const CTxDestination& dest, const std::string& id, const std::string& receive_request)
{
    return WriteIC(std::make_pair(DBKeys::DESTDATA, std::make_pair(EncodeDestination(dest), "rr" + id)), receive_request);
}

bool WalletBatch::EraseAddressReceiveRequest(const CTxDestination& dest, const std::string& id)
{
    return EraseIC(std::make_pair(DBKeys::DESTDATA, std::make_pair(EncodeDestination(dest), "rr" + id)));
}

bool WalletBatch::EraseAddressData(const CTxDestination& dest)
{
    DataStream prefix;
    prefix << DBKeys::DESTDATA << EncodeDestination(dest);
    return m_batch->ErasePrefix(prefix);
}

bool WalletBatch::WriteWalletFlags(const uint64_t flags)
{
    return WriteIC(DBKeys::FLAGS, flags);
}

bool WalletBatch::TxnBegin()
{
    return m_batch->TxnBegin();
}

bool WalletBatch::TxnCommit()
{
    bool res = m_batch->TxnCommit();
    if (res) {
        for (const auto& listener : m_txn_listeners) {
            listener.on_commit();
        }
        // txn finished, clear listeners
        m_txn_listeners.clear();
    }
    return res;
}

bool WalletBatch::TxnAbort()
{
    bool res = m_batch->TxnAbort();
    if (res) {
        for (const auto& listener : m_txn_listeners) {
            listener.on_abort();
        }
        // txn finished, clear listeners
        m_txn_listeners.clear();
    }
    return res;
}

void WalletBatch::RegisterTxnListener(const DbTxnListener& l)
{
    assert(m_batch->HasActiveTxn());
    m_txn_listeners.emplace_back(l);
}

std::unique_ptr<WalletDatabase> MakeDatabase(const fs::path& path, const DatabaseOptions& options, DatabaseStatus& status, bilingual_str& error)
{
    bool exists, isSqliteFile(false);
    try {
        exists = fs::symlink_status(path).type() != fs::file_type::not_found;
    }
    catch (const fs::filesystem_error& e) {
        error = Untranslated(strprintf("Failed to access database path '%s': %s", fs::PathToString(path), fsbridge::get_filesystem_error_message(e)));
        status = DatabaseStatus::FAILED_BAD_PATH;
        return nullptr;
    }

    if (exists) {
        if (IsSQLiteFile(SQLiteDataFile(path)))
            isSqliteFile = true;
    }
    else if (options.require_existing) {
        error = Untranslated(strprintf("Failed to load database path '%s'. Path does not exist.", fs::PathToString(path)));
        status = DatabaseStatus::FAILED_NOT_FOUND;
        return nullptr;
    }

    if (!isSqliteFile && options.require_existing) {
        error = Untranslated(strprintf("Failed to load database path '%s'. Data is not in recognized format.", fs::PathToString(path)));
        status = DatabaseStatus::FAILED_BAD_FORMAT;
        return nullptr;
    }

    if (isSqliteFile && options.require_create) {
        error = Untranslated(strprintf("Failed to create database path '%s'. Database already exists.", fs::PathToString(path)));
        status = DatabaseStatus::FAILED_ALREADY_EXISTS;
        return nullptr;
    }

#ifdef USE_SQLITE
    return MakeSQLiteDatabase(path, options, status, error);
#else
    error = Untranslated(strprintf("Failed to open database path '%s'. Build does not support SQLite database format.", fs::PathToString(path)));
    status = DatabaseStatus::FAILED_BAD_FORMAT;
    return nullptr;
#endif
}
} // namespace wallet
