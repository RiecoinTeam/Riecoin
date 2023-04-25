// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Copyright (c) 2013-2023 The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/walletdb.h>

#include <fs.h>
#include <key_io.h>
#include <protocol.h>
#include <serialize.h>
#include <sync.h>
#include <util/bip32.h>
#include <util/system.h>
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

class CWalletScanState {
public:
    unsigned int nKeys{0};
    unsigned int nCKeys{0};
    unsigned int nWatchKeys{0};
    unsigned int nKeyMeta{0};
    unsigned int m_unknown_records{0};
    bool fIsEncrypted{false};
    bool fAnyUnordered{false};
    std::vector<uint256> vWalletUpgrade;
    std::map<OutputType, uint256> m_active_external_spks;
    std::map<OutputType, uint256> m_active_internal_spks;
    std::map<uint256, DescriptorCache> m_descriptor_caches;
    std::map<std::pair<uint256, CKeyID>, CKey> m_descriptor_keys;
    std::map<std::pair<uint256, CKeyID>, std::pair<CPubKey, std::vector<unsigned char>>> m_descriptor_crypt_keys;
    bool tx_corrupt{false};
    bool descriptor_unknown{false};

    CWalletScanState() = default;
};

static bool
ReadKeyValue(CWallet* pwallet, CDataStream& ssKey, CDataStream& ssValue,
             CWalletScanState &wss, std::string& strType, std::string& strErr, const KeyFilterFn& filter_fn = nullptr) EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet)
{
    try {
        // Unserialize
        // Taking advantage of the fact that pair serialization
        // is just the two items serialized one after the other
        ssKey >> strType;
        // If we have a filter, check if this matches the filter
        if (filter_fn && !filter_fn(strType)) {
            return true;
        }
        if (strType == DBKeys::NAME) {
            std::string strAddress;
            ssKey >> strAddress;
            std::string label;
            ssValue >> label;
            pwallet->m_address_book[DecodeDestination(strAddress)].SetLabel(label);
        } else if (strType == DBKeys::PURPOSE) {
            std::string strAddress;
            ssKey >> strAddress;
            ssValue >> pwallet->m_address_book[DecodeDestination(strAddress)].purpose;
        } else if (strType == DBKeys::TX) {
            uint256 hash;
            ssKey >> hash;
            // LoadToWallet call below creates a new CWalletTx that fill_wtx
            // callback fills with transaction metadata.
            auto fill_wtx = [&](CWalletTx& wtx, bool new_tx) {
                if(!new_tx) {
                    // There's some corruption here since the tx we just tried to load was already in the wallet.
                    // We don't consider this type of corruption critical, and can fix it by removing tx data and
                    // rescanning.
                    wss.tx_corrupt = true;
                    return false;
                }
                ssValue >> wtx;
                if (wtx.GetHash() != hash)
                    return false;

                // Undo serialize changes in 31600
                if (31404 <= wtx.fTimeReceivedIsTxTime && wtx.fTimeReceivedIsTxTime <= 31703)
                {
                    if (!ssValue.empty())
                    {
                        uint8_t fTmp;
                        uint8_t fUnused;
                        std::string unused_string;
                        ssValue >> fTmp >> fUnused >> unused_string;
                        strErr = strprintf("LoadWallet() upgrading tx ver=%d %d %s",
                                           wtx.fTimeReceivedIsTxTime, fTmp, hash.ToString());
                        wtx.fTimeReceivedIsTxTime = fTmp;
                    }
                    else
                    {
                        strErr = strprintf("LoadWallet() repairing tx ver=%d %s", wtx.fTimeReceivedIsTxTime, hash.ToString());
                        wtx.fTimeReceivedIsTxTime = 0;
                    }
                    wss.vWalletUpgrade.push_back(hash);
                }

                if (wtx.nOrderPos == -1)
                    wss.fAnyUnordered = true;

                return true;
            };
            if (!pwallet->LoadToWallet(hash, fill_wtx)) {
                return false;
            }
        } else if (strType == DBKeys::MASTER_KEY) {
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
        } else if (strType == DBKeys::ORDERPOSNEXT) {
            ssValue >> pwallet->nOrderPosNext;
        } else if (strType == DBKeys::DESTDATA) {
            std::string strAddress, strKey, strValue;
            ssKey >> strAddress;
            ssKey >> strKey;
            ssValue >> strValue;
            pwallet->LoadDestData(DecodeDestination(strAddress), strKey, strValue);
        } else if (strType == DBKeys::ACTIVEEXTERNALSPK || strType == DBKeys::ACTIVEINTERNALSPK) {
            uint8_t type;
            ssKey >> type;
            uint256 id;
            ssValue >> id;

            bool internal = strType == DBKeys::ACTIVEINTERNALSPK;
            auto& spk_mans = internal ? wss.m_active_internal_spks : wss.m_active_external_spks;
            if (spk_mans.count(static_cast<OutputType>(type)) > 0) {
                strErr = "Multiple ScriptPubKeyMans specified for a single type";
                return false;
            }
            spk_mans[static_cast<OutputType>(type)] = id;
        } else if (strType == DBKeys::WALLETDESCRIPTOR) {
            uint256 id;
            ssKey >> id;
            WalletDescriptor desc;
            try {
                ssValue >> desc;
            } catch (const std::ios_base::failure& e) {
                strErr = e.what();
                wss.descriptor_unknown = true;
                return false;
            }
            if (wss.m_descriptor_caches.count(id) == 0) {
                wss.m_descriptor_caches[id] = DescriptorCache();
            }
            pwallet->LoadDescriptorScriptPubKeyMan(id, desc);
        } else if (strType == DBKeys::WALLETDESCRIPTORCACHE) {
            bool parent = true;
            uint256 desc_id;
            uint32_t key_exp_index;
            uint32_t der_index;
            ssKey >> desc_id;
            ssKey >> key_exp_index;

            // if the der_index exists, it's a derived xpub
            try
            {
                ssKey >> der_index;
                parent = false;
            }
            catch (...) {}

            std::vector<unsigned char> ser_xpub(BIP32_EXTKEY_SIZE);
            ssValue >> ser_xpub;
            CExtPubKey xpub;
            xpub.Decode(ser_xpub.data());
            if (parent) {
                wss.m_descriptor_caches[desc_id].CacheParentExtPubKey(key_exp_index, xpub);
            } else {
                wss.m_descriptor_caches[desc_id].CacheDerivedExtPubKey(key_exp_index, der_index, xpub);
            }
        } else if (strType == DBKeys::WALLETDESCRIPTORLHCACHE) {
            uint256 desc_id;
            uint32_t key_exp_index;
            ssKey >> desc_id;
            ssKey >> key_exp_index;

            std::vector<unsigned char> ser_xpub(BIP32_EXTKEY_SIZE);
            ssValue >> ser_xpub;
            CExtPubKey xpub;
            xpub.Decode(ser_xpub.data());
            wss.m_descriptor_caches[desc_id].CacheLastHardenedExtPubKey(key_exp_index, xpub);
        } else if (strType == DBKeys::WALLETDESCRIPTORKEY) {
            uint256 desc_id;
            CPubKey pubkey;
            ssKey >> desc_id;
            ssKey >> pubkey;
            if (!pubkey.IsValid())
            {
                strErr = "Error reading wallet database: CPubKey corrupt";
                return false;
            }
            CKey key;
            CPrivKey pkey;
            uint256 hash;

            wss.nKeys++;
            ssValue >> pkey;
            ssValue >> hash;

            // hash pubkey/privkey to accelerate wallet load
            std::vector<unsigned char> to_hash;
            to_hash.reserve(pubkey.size() + pkey.size());
            to_hash.insert(to_hash.end(), pubkey.begin(), pubkey.end());
            to_hash.insert(to_hash.end(), pkey.begin(), pkey.end());

            if (Hash(to_hash) != hash)
            {
                strErr = "Error reading wallet database: CPubKey/CPrivKey corrupt";
                return false;
            }

            if (!key.Load(pkey, pubkey, true))
            {
                strErr = "Error reading wallet database: CPrivKey corrupt";
                return false;
            }
            wss.m_descriptor_keys.insert(std::make_pair(std::make_pair(desc_id, pubkey.GetID()), key));
        } else if (strType == DBKeys::WALLETDESCRIPTORCKEY) {
            uint256 desc_id;
            CPubKey pubkey;
            ssKey >> desc_id;
            ssKey >> pubkey;
            if (!pubkey.IsValid())
            {
                strErr = "Error reading wallet database: CPubKey corrupt";
                return false;
            }
            std::vector<unsigned char> privkey;
            ssValue >> privkey;
            wss.nCKeys++;

            wss.m_descriptor_crypt_keys.insert(std::make_pair(std::make_pair(desc_id, pubkey.GetID()), std::make_pair(pubkey, privkey)));
            wss.fIsEncrypted = true;
        } else if (strType == DBKeys::LOCKED_UTXO) {
            uint256 hash;
            uint32_t n;
            ssKey >> hash;
            ssKey >> n;
            pwallet->LockCoin(COutPoint(hash, n));
        } else if (strType != DBKeys::BESTBLOCK && strType != DBKeys::BESTBLOCK_NOMERKLE &&
                   strType != DBKeys::MINVERSION && strType != DBKeys::ACENTRY &&
                   strType != DBKeys::VERSION && strType != DBKeys::SETTINGS &&
                   strType != DBKeys::FLAGS) {
            wss.m_unknown_records++;
        }
    } catch (const std::exception& e) {
        if (strErr.empty()) {
            strErr = e.what();
        }
        return false;
    } catch (...) {
        if (strErr.empty()) {
            strErr = "Caught unknown exception in ReadKeyValue";
        }
        return false;
    }
    return true;
}

bool ReadKeyValue(CWallet* pwallet, CDataStream& ssKey, CDataStream& ssValue, std::string& strType, std::string& strErr, const KeyFilterFn& filter_fn)
{
    CWalletScanState dummy_wss;
    LOCK(pwallet->cs_wallet);
    return ReadKeyValue(pwallet, ssKey, ssValue, dummy_wss, strType, strErr, filter_fn);
}

bool WalletBatch::IsKeyType(const std::string& strType)
{
    return strType == DBKeys::MASTER_KEY;
}

DBErrors WalletBatch::LoadWallet(CWallet* pwallet)
{
    CWalletScanState wss;
    bool fNoncriticalErrors = false;
    bool rescan_required = false;
    DBErrors result = DBErrors::LOAD_OK;

    LOCK(pwallet->cs_wallet);

    // Last client version to open this wallet
    int last_client = CLIENT_VERSION;
    bool has_last_client = m_batch->Read(DBKeys::VERSION, last_client);
    pwallet->WalletLogPrintf("Wallet file version = %d, last client version = %d\n", pwallet->GetVersion(), last_client);

    try {
        int nMinVersion = 0;
        if (m_batch->Read(DBKeys::MINVERSION, nMinVersion)) {
            if (nMinVersion > FEATURE_LATEST)
                return DBErrors::TOO_NEW;
            pwallet->LoadMinVersion(nMinVersion);
        }

        // Load wallet flags, so they are known when processing other records.
        // The FLAGS key is absent during wallet creation.
        uint64_t flags;
        if (m_batch->Read(DBKeys::FLAGS, flags)) {
            if (!pwallet->LoadWalletFlags(flags)) {
                pwallet->WalletLogPrintf("Error reading wallet database: Unknown non-tolerable wallet flags found\n");
                return DBErrors::CORRUPT;
            }
        }

#ifndef ENABLE_EXTERNAL_SIGNER
        if (pwallet->IsWalletFlagSet(WALLET_FLAG_EXTERNAL_SIGNER)) {
            pwallet->WalletLogPrintf("Error: External signer wallet being loaded without external signer support compiled\n");
            return DBErrors::EXTERNAL_SIGNER_SUPPORT_REQUIRED;
        }
#endif

        // Get cursor
        if (!m_batch->StartCursor())
        {
            pwallet->WalletLogPrintf("Error getting wallet database cursor\n");
            return DBErrors::CORRUPT;
        }

        while (true)
        {
            // Read next record
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            bool complete;
            bool ret = m_batch->ReadAtCursor(ssKey, ssValue, complete);
            if (complete) {
                break;
            }
            else if (!ret)
            {
                m_batch->CloseCursor();
                pwallet->WalletLogPrintf("Error reading next record from wallet database\n");
                return DBErrors::CORRUPT;
            }

            // Try to be tolerant of single corrupt records:
            std::string strType, strErr;
            if (!ReadKeyValue(pwallet, ssKey, ssValue, wss, strType, strErr))
            {
                // losing keys is considered a catastrophic error, anything else
                // we assume the user can live with:
                if (IsKeyType(strType)) {
                    result = DBErrors::CORRUPT;
                } else if (strType == DBKeys::FLAGS) {
                    // reading the wallet flags can only fail if unknown flags are present
                    result = DBErrors::TOO_NEW;
                } else if (wss.tx_corrupt) {
                    pwallet->WalletLogPrintf("Error: Corrupt transaction found. This can be fixed by removing transactions from wallet and rescanning.\n");
                    // Set tx_corrupt back to false so that the error is only printed once (per corrupt tx)
                    wss.tx_corrupt = false;
                    result = DBErrors::CORRUPT;
                } else if (wss.descriptor_unknown) {
                    strErr = strprintf("Error: Unrecognized descriptor found in wallet %s. ", pwallet->GetName());
                    strErr += (last_client > CLIENT_VERSION) ? "The wallet might had been created on a newer version. " :
                            "The database might be corrupted or the software version is not compatible with one of your wallet descriptors. ";
                    strErr += "Please try running the latest software version";
                    pwallet->WalletLogPrintf("%s\n", strErr);
                    return DBErrors::UNKNOWN_DESCRIPTOR;
                } else {
                    // Leave other errors alone, if we try to fix them we might make things worse.
                    fNoncriticalErrors = true; // ... but do warn the user there is something wrong.
                    if (strType == DBKeys::TX)
                        // Rescan if there is a bad transaction record:
                        rescan_required = true;
                }
            }
            if (!strErr.empty())
                pwallet->WalletLogPrintf("%s\n", strErr);
        }
    } catch (...) {
        result = DBErrors::CORRUPT;
    }
    m_batch->CloseCursor();

    // Set the active ScriptPubKeyMans
    for (auto spk_man_pair : wss.m_active_external_spks) {
        pwallet->LoadActiveScriptPubKeyMan(spk_man_pair.second, spk_man_pair.first, /*internal=*/false);
    }
    for (auto spk_man_pair : wss.m_active_internal_spks) {
        pwallet->LoadActiveScriptPubKeyMan(spk_man_pair.second, spk_man_pair.first, /*internal=*/true);
    }

    // Set the descriptor caches
    for (const auto& desc_cache_pair : wss.m_descriptor_caches) {
        auto spk_man = pwallet->GetScriptPubKeyMan(desc_cache_pair.first);
        assert(spk_man);
        ((DescriptorScriptPubKeyMan*)spk_man)->SetCache(desc_cache_pair.second);
    }

    // Set the descriptor keys
    for (const auto& desc_key_pair : wss.m_descriptor_keys) {
        auto spk_man = pwallet->GetScriptPubKeyMan(desc_key_pair.first.first);
        ((DescriptorScriptPubKeyMan*)spk_man)->AddKey(desc_key_pair.first.second, desc_key_pair.second);
    }
    for (const auto& desc_key_pair : wss.m_descriptor_crypt_keys) {
        auto spk_man = pwallet->GetScriptPubKeyMan(desc_key_pair.first.first);
        ((DescriptorScriptPubKeyMan*)spk_man)->AddCryptedKey(desc_key_pair.first.second, desc_key_pair.second.first, desc_key_pair.second.second);
    }

    if (rescan_required && result == DBErrors::LOAD_OK) {
        result = DBErrors::NEED_RESCAN;
    } else if (fNoncriticalErrors && result == DBErrors::LOAD_OK) {
        result = DBErrors::NONCRITICAL_ERROR;
    }

    // Any wallet corruption at all: skip any rewriting or
    // upgrading, we don't want to make it worse.
    if (result != DBErrors::LOAD_OK)
        return result;

    pwallet->WalletLogPrintf("Keys: %u plaintext, %u encrypted, %u w/ metadata, %u total. Unknown wallet records: %u\n",
           wss.nKeys, wss.nCKeys, wss.nKeyMeta, wss.nKeys + wss.nCKeys, wss.m_unknown_records);

    for (const uint256& hash : wss.vWalletUpgrade)
        WriteTx(pwallet->mapWallet.at(hash));

    if (!has_last_client || last_client != CLIENT_VERSION) // Update
        m_batch->Write(DBKeys::VERSION, CLIENT_VERSION);

    if (wss.fAnyUnordered)
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

DBErrors WalletBatch::FindWalletTx(std::vector<uint256>& vTxHash, std::list<CWalletTx>& vWtx)
{
    DBErrors result = DBErrors::LOAD_OK;

    try {
        int nMinVersion = 0;
        if (m_batch->Read(DBKeys::MINVERSION, nMinVersion)) {
            if (nMinVersion > FEATURE_LATEST)
                return DBErrors::TOO_NEW;
        }

        // Get cursor
        if (!m_batch->StartCursor())
        {
            LogPrintf("Error getting wallet database cursor\n");
            return DBErrors::CORRUPT;
        }

        while (true)
        {
            // Read next record
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            bool complete;
            bool ret = m_batch->ReadAtCursor(ssKey, ssValue, complete);
            if (complete) {
                break;
            } else if (!ret) {
                m_batch->CloseCursor();
                LogPrintf("Error reading next record from wallet database\n");
                return DBErrors::CORRUPT;
            }

            std::string strType;
            ssKey >> strType;
            if (strType == DBKeys::TX) {
                uint256 hash;
                ssKey >> hash;
                vTxHash.push_back(hash);
                vWtx.emplace_back(/*tx=*/nullptr, TxStateInactive{});
                ssValue >> vWtx.back();
            }
        }
    } catch (...) {
        result = DBErrors::CORRUPT;
    }
    m_batch->CloseCursor();

    return result;
}

DBErrors WalletBatch::ZapSelectTx(std::vector<uint256>& vTxHashIn, std::vector<uint256>& vTxHashOut)
{
    // build list of wallet TXs and hashes
    std::vector<uint256> vTxHash;
    std::list<CWalletTx> vWtx;
    DBErrors err = FindWalletTx(vTxHash, vWtx);
    if (err != DBErrors::LOAD_OK) {
        return err;
    }

    std::sort(vTxHash.begin(), vTxHash.end());
    std::sort(vTxHashIn.begin(), vTxHashIn.end());

    // erase each matching wallet TX
    bool delerror = false;
    std::vector<uint256>::iterator it = vTxHashIn.begin();
    for (const uint256& hash : vTxHash) {
        while (it < vTxHashIn.end() && (*it) < hash) {
            it++;
        }
        if (it == vTxHashIn.end()) {
            break;
        }
        else if ((*it) == hash) {
            if(!EraseTx(hash)) {
                LogPrint(BCLog::WALLETDB, "Transaction was found for deletion but returned database error: %s\n", hash.GetHex());
                delerror = true;
            }
            vTxHashOut.push_back(hash);
        }
    }

    if (delerror) {
        return DBErrors::CORRUPT;
    }
    return DBErrors::LOAD_OK;
}

void MaybeCompactWalletDB(WalletContext& context)
{
    static std::atomic<bool> fOneThread(false);
    if (fOneThread.exchange(true)) {
        return;
    }

    for (const std::shared_ptr<CWallet>& pwallet : GetWallets(context)) {
        WalletDatabase& dbh = pwallet->GetDatabase();

        unsigned int nUpdateCounter = dbh.nUpdateCounter;

        if (dbh.nLastSeen != nUpdateCounter) {
            dbh.nLastSeen = nUpdateCounter;
            dbh.nLastWalletUpdate = GetTime();
        }

        if (dbh.nLastFlushed != nUpdateCounter && GetTime() - dbh.nLastWalletUpdate >= 2) {
            if (dbh.PeriodicFlush()) {
                dbh.nLastFlushed = nUpdateCounter;
            }
        }
    }

    fOneThread = false;
}

bool WalletBatch::WriteDestData(const std::string &address, const std::string &key, const std::string &value)
{
    return WriteIC(std::make_pair(DBKeys::DESTDATA, std::make_pair(address, key)), value);
}

bool WalletBatch::EraseDestData(const std::string &address, const std::string &key)
{
    return EraseIC(std::make_pair(DBKeys::DESTDATA, std::make_pair(address, key)));
}

bool WalletBatch::WriteWalletFlags(const uint64_t flags)
{
    return WriteIC(DBKeys::FLAGS, flags);
}

bool WalletBatch::EraseRecords(const std::unordered_set<std::string>& types)
{
    // Get cursor
    if (!m_batch->StartCursor())
    {
        return false;
    }

    // Iterate the DB and look for any records that have the type prefixes
    while (true)
    {
        // Read next record
        CDataStream key(SER_DISK, CLIENT_VERSION);
        CDataStream value(SER_DISK, CLIENT_VERSION);
        bool complete;
        bool ret = m_batch->ReadAtCursor(key, value, complete);
        if (complete) {
            break;
        }
        else if (!ret)
        {
            m_batch->CloseCursor();
            return false;
        }

        // Make a copy of key to avoid data being deleted by the following read of the type
        Span<const unsigned char> key_data = MakeUCharSpan(key);

        std::string type;
        key >> type;

        if (types.count(type) > 0) {
            m_batch->Erase(key_data);
        }
    }
    m_batch->CloseCursor();
    return true;
}

bool WalletBatch::TxnBegin()
{
    return m_batch->TxnBegin();
}

bool WalletBatch::TxnCommit()
{
    return m_batch->TxnCommit();
}

bool WalletBatch::TxnAbort()
{
    return m_batch->TxnAbort();
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
#endif
    error = Untranslated(strprintf("Failed to open database path '%s'. Build does not support SQLite database format.", fs::PathToString(path)));
    status = DatabaseStatus::FAILED_BAD_FORMAT;
    return nullptr;
}

/** Return object for accessing dummy database with no read/write capabilities. */
std::unique_ptr<WalletDatabase> CreateDummyWalletDatabase()
{
    return std::make_unique<DummyDatabase>();
}

/** Return object for accessing temporary in-memory database. */
std::unique_ptr<WalletDatabase> CreateMockWalletDatabase(DatabaseOptions& options)
{
#ifdef USE_SQLITE
    return std::make_unique<SQLiteDatabase>(":memory:", "", options, true);
#endif
    assert(false);
}

std::unique_ptr<WalletDatabase> CreateMockWalletDatabase()
{
    DatabaseOptions options;
    options.create_flags = WALLET_FLAG_DESCRIPTORS;
    return CreateMockWalletDatabase(options);
}
} // namespace wallet
