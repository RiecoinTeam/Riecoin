// Copyright (c) 2019-2021 The Bitcoin Core developers
// Copyright (c) 2013-2023 The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key_io.h>
#include <logging.h>
#include <outputtype.h>
#include <script/descriptor.h>
#include <script/sign.h>
#include <util/bip32.h>
#include <util/strencodings.h>
#include <util/string.h>
#include <util/system.h>
#include <util/time.h>
#include <util/translation.h>
#include <wallet/scriptpubkeyman.h>

#include <optional>

namespace wallet {
//! Value for the first BIP 32 hardened derivation. Can be used as a bit mask and as a value. See BIP 32 for more details.
const uint32_t BIP32_HARDENED_KEY_LIMIT = 0x80000000;

typedef std::vector<unsigned char> valtype;

util::Result<CTxDestination> DescriptorScriptPubKeyMan::GetNewDestination(const OutputType type)
{
    // Returns true if this descriptor supports getting new addresses. Conditions where we may be unable to fetch them (e.g. locked) are caught later
    if (!CanGetAddresses()) {
        return util::Error{_("No addresses available")};
    }
    {
        LOCK(cs_desc_man);
        assert(m_wallet_descriptor.descriptor->IsSingleType()); // This is a combo descriptor which should not be an active descriptor
        std::optional<OutputType> desc_addr_type = m_wallet_descriptor.descriptor->GetOutputType();
        assert(desc_addr_type);
        if (type != *desc_addr_type) {
            throw std::runtime_error(std::string(__func__) + ": Types are inconsistent");
        }

        TopUp();

        // Get the scriptPubKey from the descriptor
        FlatSigningProvider out_keys;
        std::vector<CScript> scripts_temp;
        if (m_wallet_descriptor.range_end <= m_max_cached_index && !TopUp(1)) {
            // We can't generate anymore keys
            return util::Error{_("Error: Keypool ran out, please call keypoolrefill first")};
        }
        if (!m_wallet_descriptor.descriptor->ExpandFromCache(m_wallet_descriptor.next_index, m_wallet_descriptor.cache, scripts_temp, out_keys)) {
            // We can't generate anymore keys
            return util::Error{_("Error: Keypool ran out, please call keypoolrefill first")};
        }

        CTxDestination dest;
        std::optional<OutputType> out_script_type = m_wallet_descriptor.descriptor->GetOutputType();
        if (out_script_type && out_script_type == type) {
            ExtractDestination(scripts_temp[0], dest);
        } else {
            throw std::runtime_error(std::string(__func__) + ": Types are inconsistent. Stored type does not match type of newly generated address");
        }
        m_wallet_descriptor.next_index++;
        WalletBatch(m_storage.GetDatabase()).WriteDescriptor(GetID(), m_wallet_descriptor);
        return dest;
    }
}

isminetype DescriptorScriptPubKeyMan::IsMine(const CScript& script) const
{
    LOCK(cs_desc_man);
    if (m_map_script_pub_keys.count(script) > 0) {
        return ISMINE_SPENDABLE;
    }
    return ISMINE_NO;
}

bool DescriptorScriptPubKeyMan::CheckDecryptionKey(const CKeyingMaterial& master_key, bool accept_no_keys)
{
    LOCK(cs_desc_man);
    if (!m_map_keys.empty()) {
        return false;
    }

    bool keyPass = m_map_crypted_keys.empty(); // Always pass when there are no encrypted keys
    bool keyFail = false;
    for (const auto& mi : m_map_crypted_keys) {
        const CPubKey &pubkey = mi.second.first;
        const std::vector<unsigned char> &crypted_secret = mi.second.second;
        CKey key;
        if (!DecryptKey(master_key, crypted_secret, pubkey, key)) {
            keyFail = true;
            break;
        }
        keyPass = true;
        if (m_decryption_thoroughly_checked)
            break;
    }
    if (keyPass && keyFail) {
        LogPrintf("The wallet is probably corrupted: Some keys decrypt but not all.\n");
        throw std::runtime_error("Error unlocking wallet: some keys decrypt but not all. Your wallet file may be corrupt.");
    }
    if (keyFail || (!keyPass && !accept_no_keys)) {
        return false;
    }
    m_decryption_thoroughly_checked = true;
    return true;
}

bool DescriptorScriptPubKeyMan::Encrypt(const CKeyingMaterial& master_key, WalletBatch* batch)
{
    LOCK(cs_desc_man);
    if (!m_map_crypted_keys.empty()) {
        return false;
    }

    for (const KeyMap::value_type& key_in : m_map_keys)
    {
        const CKey &key = key_in.second;
        CPubKey pubkey = key.GetPubKey();
        CKeyingMaterial secret(key.begin(), key.end());
        std::vector<unsigned char> crypted_secret;
        if (!EncryptSecret(master_key, secret, pubkey.GetHash(), crypted_secret)) {
            return false;
        }
        m_map_crypted_keys[pubkey.GetID()] = make_pair(pubkey, crypted_secret);
        batch->WriteCryptedDescriptorKey(GetID(), pubkey, crypted_secret);
    }
    m_map_keys.clear();
    return true;
}

util::Result<CTxDestination> DescriptorScriptPubKeyMan::GetReservedDestination(const OutputType type, bool internal, int64_t& index, CKeyPool& keypool)
{
    LOCK(cs_desc_man);
    auto op_dest = GetNewDestination(type);
    index = m_wallet_descriptor.next_index - 1;
    return op_dest;
}

void DescriptorScriptPubKeyMan::ReturnDestination(int64_t index, bool internal, const CTxDestination& addr)
{
    LOCK(cs_desc_man);
    // Only return when the index was the most recent
    if (m_wallet_descriptor.next_index - 1 == index) {
        m_wallet_descriptor.next_index--;
    }
    WalletBatch(m_storage.GetDatabase()).WriteDescriptor(GetID(), m_wallet_descriptor);
    NotifyCanGetAddressesChanged();
}

std::map<CKeyID, CKey> DescriptorScriptPubKeyMan::GetKeys() const
{
    AssertLockHeld(cs_desc_man);
    if (m_storage.HasEncryptionKeys() && !m_storage.IsLocked()) {
        KeyMap keys;
        for (const auto& key_pair : m_map_crypted_keys) {
            const CPubKey& pubkey = key_pair.second.first;
            const std::vector<unsigned char>& crypted_secret = key_pair.second.second;
            CKey key;
            DecryptKey(m_storage.GetEncryptionKey(), crypted_secret, pubkey, key);
            keys[pubkey.GetID()] = key;
        }
        return keys;
    }
    return m_map_keys;
}

bool DescriptorScriptPubKeyMan::TopUp(unsigned int size)
{
    LOCK(cs_desc_man);
    unsigned int target_size;
    if (size > 0) {
        target_size = size;
    } else {
        target_size = std::max(gArgs.GetIntArg("-keypool", DEFAULT_KEYPOOL_SIZE), (int64_t) 1);
    }

    // Calculate the new range_end
    int32_t new_range_end = std::max(m_wallet_descriptor.next_index + (int32_t)target_size, m_wallet_descriptor.range_end);

    // If the descriptor is not ranged, we actually just want to fill the first cache item
    if (!m_wallet_descriptor.descriptor->IsRange()) {
        new_range_end = 1;
        m_wallet_descriptor.range_end = 1;
        m_wallet_descriptor.range_start = 0;
    }

    FlatSigningProvider provider;
    provider.keys = GetKeys();

    WalletBatch batch(m_storage.GetDatabase());
    uint256 id = GetID();
    for (int32_t i = m_max_cached_index + 1; i < new_range_end; ++i) {
        FlatSigningProvider out_keys;
        std::vector<CScript> scripts_temp;
        DescriptorCache temp_cache;
        // Maybe we have a cached xpub and we can expand from the cache first
        if (!m_wallet_descriptor.descriptor->ExpandFromCache(i, m_wallet_descriptor.cache, scripts_temp, out_keys)) {
            if (!m_wallet_descriptor.descriptor->Expand(i, provider, scripts_temp, out_keys, &temp_cache)) return false;
        }
        // Add all of the scriptPubKeys to the scriptPubKey set
        for (const CScript& script : scripts_temp) {
            m_map_script_pub_keys[script] = i;
        }
        for (const auto& pk_pair : out_keys.pubkeys) {
            const CPubKey& pubkey = pk_pair.second;
            if (m_map_pubkeys.count(pubkey) != 0) {
                // We don't need to give an error here.
                // It doesn't matter which of many valid indexes the pubkey has, we just need an index where we can derive it and it's private key
                continue;
            }
            m_map_pubkeys[pubkey] = i;
        }
        // Merge and write the cache
        DescriptorCache new_items = m_wallet_descriptor.cache.MergeAndDiff(temp_cache);
        if (!batch.WriteDescriptorCacheItems(id, new_items)) {
            throw std::runtime_error(std::string(__func__) + ": writing cache items failed");
        }
        m_max_cached_index++;
    }
    m_wallet_descriptor.range_end = new_range_end;
    batch.WriteDescriptor(GetID(), m_wallet_descriptor);

    // By this point, the cache size should be the size of the entire range
    assert(m_wallet_descriptor.range_end - 1 == m_max_cached_index);

    NotifyCanGetAddressesChanged();
    return true;
}

std::vector<WalletDestination> DescriptorScriptPubKeyMan::MarkUnusedAddresses(const CScript& script)
{
    LOCK(cs_desc_man);
    std::vector<WalletDestination> result;
    if (IsMine(script)) {
        int32_t index = m_map_script_pub_keys[script];
        if (index >= m_wallet_descriptor.next_index) {
            WalletLogPrintf("%s: Detected a used keypool item at index %d, mark all keypool items up to this item as used\n", __func__, index);
            auto out_keys = std::make_unique<FlatSigningProvider>();
            std::vector<CScript> scripts_temp;
            while (index >= m_wallet_descriptor.next_index) {
                if (!m_wallet_descriptor.descriptor->ExpandFromCache(m_wallet_descriptor.next_index, m_wallet_descriptor.cache, scripts_temp, *out_keys)) {
                    throw std::runtime_error(std::string(__func__) + ": Unable to expand descriptor from cache");
                }
                CTxDestination dest;
                ExtractDestination(scripts_temp[0], dest);
                result.push_back({dest, std::nullopt});
                m_wallet_descriptor.next_index++;
            }
        }
        if (!TopUp()) {
            WalletLogPrintf("%s: Topping up keypool failed (locked wallet)\n", __func__);
        }
    }

    return result;
}

void DescriptorScriptPubKeyMan::AddDescriptorKey(const CKey& key, const CPubKey &pubkey)
{
    LOCK(cs_desc_man);
    WalletBatch batch(m_storage.GetDatabase());
    if (!AddDescriptorKeyWithDB(batch, key, pubkey)) {
        throw std::runtime_error(std::string(__func__) + ": writing descriptor private key failed");
    }
}

bool DescriptorScriptPubKeyMan::AddDescriptorKeyWithDB(WalletBatch& batch, const CKey& key, const CPubKey &pubkey)
{
    AssertLockHeld(cs_desc_man);
    assert(!m_storage.IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS));

    // Check if provided key already exists
    if (m_map_keys.find(pubkey.GetID()) != m_map_keys.end() ||
        m_map_crypted_keys.find(pubkey.GetID()) != m_map_crypted_keys.end()) {
        return true;
    }

    if (m_storage.HasEncryptionKeys()) {
        if (m_storage.IsLocked()) {
            return false;
        }

        std::vector<unsigned char> crypted_secret;
        CKeyingMaterial secret(key.begin(), key.end());
        if (!EncryptSecret(m_storage.GetEncryptionKey(), secret, pubkey.GetHash(), crypted_secret)) {
            return false;
        }

        m_map_crypted_keys[pubkey.GetID()] = make_pair(pubkey, crypted_secret);
        return batch.WriteCryptedDescriptorKey(GetID(), pubkey, crypted_secret);
    } else {
        m_map_keys[pubkey.GetID()] = key;
        return batch.WriteDescriptorKey(GetID(), pubkey, key.GetPrivKey());
    }
}

bool DescriptorScriptPubKeyMan::SetupDescriptorGeneration(const CExtKey& master_key, OutputType addr_type, bool internal)
{
    LOCK(cs_desc_man);
    assert(m_storage.IsWalletFlagSet(WALLET_FLAG_DESCRIPTORS));

    // Ignore when there is already a descriptor
    if (m_wallet_descriptor.descriptor) {
        return false;
    }

    int64_t creation_time = GetTime();

    std::string xpub = EncodeExtPubKey(master_key.Neuter());

    // Build descriptor string
    std::string desc_prefix;
    std::string desc_suffix = "/*)";
    switch (addr_type) {
    case OutputType::LEGACY: {
        desc_prefix = "pkh(" + xpub + "/44'";
        break;
    }
    case OutputType::P2SH_SEGWIT: {
        desc_prefix = "sh(wpkh(" + xpub + "/49'";
        desc_suffix += ")";
        break;
    }
    case OutputType::BECH32: {
        desc_prefix = "wpkh(" + xpub + "/84'";
        break;
    }
    case OutputType::BECH32M: {
        desc_prefix = "tr(" + xpub  + "/86'";
        break;
    }
    case OutputType::UNKNOWN: {
        // We should never have a DescriptorScriptPubKeyMan for an UNKNOWN OutputType,
        // so if we get to this point something is wrong
        assert(false);
    }
    } // no default case, so the compiler can warn about missing cases
    assert(!desc_prefix.empty());

    // Mainnet derives at 0', testnet and regtest derive at 1'
    if (Params().IsTestChain()) {
        desc_prefix += "/1'";
    } else {
        desc_prefix += "/0'";
    }

    std::string internal_path = internal ? "/1" : "/0";
    std::string desc_str = desc_prefix + "/0'" + internal_path + desc_suffix;

    // Make the descriptor
    FlatSigningProvider keys;
    std::string error;
    std::unique_ptr<Descriptor> desc = Parse(desc_str, keys, error, false);
    WalletDescriptor w_desc(std::move(desc), creation_time, 0, 0, 0);
    m_wallet_descriptor = w_desc;

    // Store the master private key, and descriptor
    WalletBatch batch(m_storage.GetDatabase());
    if (!AddDescriptorKeyWithDB(batch, master_key.key, master_key.key.GetPubKey())) {
        throw std::runtime_error(std::string(__func__) + ": writing descriptor master private key failed");
    }
    if (!batch.WriteDescriptor(GetID(), m_wallet_descriptor)) {
        throw std::runtime_error(std::string(__func__) + ": writing descriptor failed");
    }

    // TopUp
    TopUp();

    m_storage.UnsetBlankWalletFlag(batch);
    return true;
}

bool DescriptorScriptPubKeyMan::IsHDEnabled() const
{
    LOCK(cs_desc_man);
    return m_wallet_descriptor.descriptor->IsRange();
}

bool DescriptorScriptPubKeyMan::CanGetAddresses(bool internal) const
{
    // We can only give out addresses from descriptors that are single type (not combo), ranged,
    // and either have cached keys or can generate more keys (ignoring encryption)
    LOCK(cs_desc_man);
    return m_wallet_descriptor.descriptor->IsSingleType() &&
           m_wallet_descriptor.descriptor->IsRange() &&
           (HavePrivateKeys() || m_wallet_descriptor.next_index < m_wallet_descriptor.range_end);
}

bool DescriptorScriptPubKeyMan::HavePrivateKeys() const
{
    LOCK(cs_desc_man);
    return m_map_keys.size() > 0 || m_map_crypted_keys.size() > 0;
}


unsigned int DescriptorScriptPubKeyMan::GetKeyPoolSize() const
{
    LOCK(cs_desc_man);
    return m_wallet_descriptor.range_end - m_wallet_descriptor.next_index;
}

int64_t DescriptorScriptPubKeyMan::GetTimeFirstKey() const
{
    LOCK(cs_desc_man);
    return m_wallet_descriptor.creation_time;
}

std::unique_ptr<FlatSigningProvider> DescriptorScriptPubKeyMan::GetSigningProvider(const CScript& script, bool include_private) const
{
    LOCK(cs_desc_man);

    // Find the index of the script
    auto it = m_map_script_pub_keys.find(script);
    if (it == m_map_script_pub_keys.end()) {
        return nullptr;
    }
    int32_t index = it->second;

    return GetSigningProvider(index, include_private);
}

std::unique_ptr<FlatSigningProvider> DescriptorScriptPubKeyMan::GetSigningProvider(const CPubKey& pubkey) const
{
    LOCK(cs_desc_man);

    // Find index of the pubkey
    auto it = m_map_pubkeys.find(pubkey);
    if (it == m_map_pubkeys.end()) {
        return nullptr;
    }
    int32_t index = it->second;

    // Always try to get the signing provider with private keys. This function should only be called during signing anyways
    return GetSigningProvider(index, true);
}

std::unique_ptr<FlatSigningProvider> DescriptorScriptPubKeyMan::GetSigningProvider(int32_t index, bool include_private) const
{
    AssertLockHeld(cs_desc_man);

    std::unique_ptr<FlatSigningProvider> out_keys = std::make_unique<FlatSigningProvider>();

    // Fetch SigningProvider from cache to avoid re-deriving
    auto it = m_map_signing_providers.find(index);
    if (it != m_map_signing_providers.end()) {
        out_keys->Merge(FlatSigningProvider{it->second});
    } else {
        // Get the scripts, keys, and key origins for this script
        std::vector<CScript> scripts_temp;
        if (!m_wallet_descriptor.descriptor->ExpandFromCache(index, m_wallet_descriptor.cache, scripts_temp, *out_keys)) return nullptr;

        // Cache SigningProvider so we don't need to re-derive if we need this SigningProvider again
        m_map_signing_providers[index] = *out_keys;
    }

    if (HavePrivateKeys() && include_private) {
        FlatSigningProvider master_provider;
        master_provider.keys = GetKeys();
        m_wallet_descriptor.descriptor->ExpandPrivate(index, master_provider, *out_keys);
    }

    return out_keys;
}

std::unique_ptr<SigningProvider> DescriptorScriptPubKeyMan::GetSolvingProvider(const CScript& script) const
{
    return GetSigningProvider(script, false);
}

bool DescriptorScriptPubKeyMan::CanProvide(const CScript& script, SignatureData& sigdata)
{
    return IsMine(script);
}

bool DescriptorScriptPubKeyMan::SignTransaction(CMutableTransaction& tx, const std::map<COutPoint, Coin>& coins, int sighash, std::map<int, bilingual_str>& input_errors) const
{
    std::unique_ptr<FlatSigningProvider> keys = std::make_unique<FlatSigningProvider>();
    for (const auto& coin_pair : coins) {
        std::unique_ptr<FlatSigningProvider> coin_keys = GetSigningProvider(coin_pair.second.out.scriptPubKey, true);
        if (!coin_keys) {
            continue;
        }
        keys->Merge(std::move(*coin_keys));
    }

    return ::SignTransaction(tx, keys.get(), coins, sighash, input_errors);
}

SigningResult DescriptorScriptPubKeyMan::SignMessage(const std::string& message, const PKHash& pkhash, std::string& str_sig) const
{
    std::unique_ptr<FlatSigningProvider> keys = GetSigningProvider(GetScriptForDestination(pkhash), true);
    if (!keys) {
        return SigningResult::PRIVATE_KEY_NOT_AVAILABLE;
    }

    CKey key;
    if (!keys->GetKey(ToKeyID(pkhash), key)) {
        return SigningResult::PRIVATE_KEY_NOT_AVAILABLE;
    }

    if (!MessageSign(key, message, str_sig)) {
        return SigningResult::SIGNING_FAILED;
    }
    return SigningResult::OK;
}

TransactionError DescriptorScriptPubKeyMan::FillPSBT(PartiallySignedTransaction& psbtx, const PrecomputedTransactionData& txdata, int sighash_type, bool sign, bool bip32derivs, int* n_signed, bool finalize) const
{
    if (n_signed) {
        *n_signed = 0;
    }
    for (unsigned int i = 0; i < psbtx.tx->vin.size(); ++i) {
        const CTxIn& txin = psbtx.tx->vin[i];
        PSBTInput& input = psbtx.inputs.at(i);

        if (PSBTInputSigned(input)) {
            continue;
        }

        // Get the Sighash type
        if (sign && input.sighash_type != std::nullopt && *input.sighash_type != sighash_type) {
            return TransactionError::SIGHASH_MISMATCH;
        }

        // Get the scriptPubKey to know which SigningProvider to use
        CScript script;
        if (!input.witness_utxo.IsNull()) {
            script = input.witness_utxo.scriptPubKey;
        } else if (input.non_witness_utxo) {
            if (txin.prevout.n >= input.non_witness_utxo->vout.size()) {
                return TransactionError::MISSING_INPUTS;
            }
            script = input.non_witness_utxo->vout[txin.prevout.n].scriptPubKey;
        } else {
            // There's no UTXO so we can just skip this now
            continue;
        }
        SignatureData sigdata;
        input.FillSignatureData(sigdata);

        std::unique_ptr<FlatSigningProvider> keys = std::make_unique<FlatSigningProvider>();
        std::unique_ptr<FlatSigningProvider> script_keys = GetSigningProvider(script, sign);
        if (script_keys) {
            keys->Merge(std::move(*script_keys));
        } else {
            // Maybe there are pubkeys listed that we can sign for
            std::vector<CPubKey> pubkeys;

            // ECDSA Pubkeys
            for (const auto& [pk, _] : input.hd_keypaths) {
                pubkeys.push_back(pk);
            }

            // Taproot output pubkey
            std::vector<std::vector<unsigned char>> sols;
            if (Solver(script, sols) == TxoutType::WITNESS_V1_TAPROOT) {
                sols[0].insert(sols[0].begin(), 0x02);
                pubkeys.emplace_back(sols[0]);
                sols[0][0] = 0x03;
                pubkeys.emplace_back(sols[0]);
            }

            // Taproot pubkeys
            for (const auto& pk_pair : input.m_tap_bip32_paths) {
                const XOnlyPubKey& pubkey = pk_pair.first;
                for (unsigned char prefix : {0x02, 0x03}) {
                    unsigned char b[33] = {prefix};
                    std::copy(pubkey.begin(), pubkey.end(), b + 1);
                    CPubKey fullpubkey;
                    fullpubkey.Set(b, b + 33);
                    pubkeys.push_back(fullpubkey);
                }
            }

            for (const auto& pubkey : pubkeys) {
                std::unique_ptr<FlatSigningProvider> pk_keys = GetSigningProvider(pubkey);
                if (pk_keys) {
                    keys->Merge(std::move(*pk_keys));
                }
            }
        }

        SignPSBTInput(HidingSigningProvider(keys.get(), !sign, !bip32derivs), psbtx, i, &txdata, sighash_type, nullptr, finalize);

        bool signed_one = PSBTInputSigned(input);
        if (n_signed && (signed_one || !sign)) {
            // If sign is false, we assume that we _could_ sign if we get here. This
            // will never have false negatives; it is hard to tell under what i
            // circumstances it could have false positives.
            (*n_signed)++;
        }
    }

    // Fill in the bip32 keypaths and redeemscripts for the outputs so that hardware wallets can identify change
    for (unsigned int i = 0; i < psbtx.tx->vout.size(); ++i) {
        std::unique_ptr<SigningProvider> keys = GetSolvingProvider(psbtx.tx->vout.at(i).scriptPubKey);
        if (!keys) {
            continue;
        }
        UpdatePSBTOutput(HidingSigningProvider(keys.get(), true, !bip32derivs), psbtx, i);
    }

    return TransactionError::OK;
}

std::unique_ptr<CKeyMetadata> DescriptorScriptPubKeyMan::GetMetadata(const CTxDestination& dest) const
{
    std::unique_ptr<SigningProvider> provider = GetSigningProvider(GetScriptForDestination(dest));
    if (provider) {
        KeyOriginInfo orig;
        CKeyID key_id = GetKeyForDestination(*provider, dest);
        if (provider->GetKeyOrigin(key_id, orig)) {
            LOCK(cs_desc_man);
            std::unique_ptr<CKeyMetadata> meta = std::make_unique<CKeyMetadata>();
            meta->key_origin = orig;
            meta->has_key_origin = true;
            meta->nCreateTime = m_wallet_descriptor.creation_time;
            return meta;
        }
    }
    return nullptr;
}

uint256 DescriptorScriptPubKeyMan::GetID() const
{
    LOCK(cs_desc_man);
    std::string desc_str = m_wallet_descriptor.descriptor->ToString();
    uint256 id;
    CSHA256().Write((unsigned char*)desc_str.data(), desc_str.size()).Finalize(id.begin());
    return id;
}

void DescriptorScriptPubKeyMan::SetCache(const DescriptorCache& cache)
{
    LOCK(cs_desc_man);
    m_wallet_descriptor.cache = cache;
    for (int32_t i = m_wallet_descriptor.range_start; i < m_wallet_descriptor.range_end; ++i) {
        FlatSigningProvider out_keys;
        std::vector<CScript> scripts_temp;
        if (!m_wallet_descriptor.descriptor->ExpandFromCache(i, m_wallet_descriptor.cache, scripts_temp, out_keys)) {
            throw std::runtime_error("Error: Unable to expand wallet descriptor from cache");
        }
        // Add all of the scriptPubKeys to the scriptPubKey set
        for (const CScript& script : scripts_temp) {
            if (m_map_script_pub_keys.count(script) != 0) {
                throw std::runtime_error(strprintf("Error: Already loaded script at index %d as being at index %d", i, m_map_script_pub_keys[script]));
            }
            m_map_script_pub_keys[script] = i;
        }
        for (const auto& pk_pair : out_keys.pubkeys) {
            const CPubKey& pubkey = pk_pair.second;
            if (m_map_pubkeys.count(pubkey) != 0) {
                // We don't need to give an error here.
                // It doesn't matter which of many valid indexes the pubkey has, we just need an index where we can derive it and it's private key
                continue;
            }
            m_map_pubkeys[pubkey] = i;
        }
        m_max_cached_index++;
    }
}

bool DescriptorScriptPubKeyMan::AddKey(const CKeyID& key_id, const CKey& key)
{
    LOCK(cs_desc_man);
    m_map_keys[key_id] = key;
    return true;
}

bool DescriptorScriptPubKeyMan::AddCryptedKey(const CKeyID& key_id, const CPubKey& pubkey, const std::vector<unsigned char>& crypted_key)
{
    LOCK(cs_desc_man);
    if (!m_map_keys.empty()) {
        return false;
    }

    m_map_crypted_keys[key_id] = make_pair(pubkey, crypted_key);
    return true;
}

bool DescriptorScriptPubKeyMan::HasWalletDescriptor(const WalletDescriptor& desc) const
{
    LOCK(cs_desc_man);
    return m_wallet_descriptor.descriptor != nullptr && desc.descriptor != nullptr && m_wallet_descriptor.descriptor->ToString() == desc.descriptor->ToString();
}

void DescriptorScriptPubKeyMan::WriteDescriptor()
{
    LOCK(cs_desc_man);
    WalletBatch batch(m_storage.GetDatabase());
    if (!batch.WriteDescriptor(GetID(), m_wallet_descriptor)) {
        throw std::runtime_error(std::string(__func__) + ": writing descriptor failed");
    }
}

const WalletDescriptor DescriptorScriptPubKeyMan::GetWalletDescriptor() const
{
    return m_wallet_descriptor;
}

const std::unordered_set<CScript, SaltedSipHasher> DescriptorScriptPubKeyMan::GetScriptPubKeys() const
{
    LOCK(cs_desc_man);
    std::unordered_set<CScript, SaltedSipHasher> script_pub_keys;
    script_pub_keys.reserve(m_map_script_pub_keys.size());

    for (auto const& script_pub_key: m_map_script_pub_keys) {
        script_pub_keys.insert(script_pub_key.first);
    }
    return script_pub_keys;
}

bool DescriptorScriptPubKeyMan::GetDescriptorString(std::string& out, const bool priv) const
{
    LOCK(cs_desc_man);

    FlatSigningProvider provider;
    provider.keys = GetKeys();

    if (priv) {
        // For the private version, always return the master key to avoid
        // exposing child private keys. The risk implications of exposing child
        // private keys together with the parent xpub may be non-obvious for users.
        return m_wallet_descriptor.descriptor->ToPrivateString(provider, out);
    }

    return m_wallet_descriptor.descriptor->ToNormalizedString(provider, out, &m_wallet_descriptor.cache);
}

void DescriptorScriptPubKeyMan::UpgradeDescriptorCache()
{
    LOCK(cs_desc_man);
    if (m_storage.IsLocked() || m_storage.IsWalletFlagSet(WALLET_FLAG_LAST_HARDENED_XPUB_CACHED)) {
        return;
    }

    // Skip if we have the last hardened xpub cache
    if (m_wallet_descriptor.cache.GetCachedLastHardenedExtPubKeys().size() > 0) {
        return;
    }

    // Expand the descriptor
    FlatSigningProvider provider;
    provider.keys = GetKeys();
    FlatSigningProvider out_keys;
    std::vector<CScript> scripts_temp;
    DescriptorCache temp_cache;
    if (!m_wallet_descriptor.descriptor->Expand(0, provider, scripts_temp, out_keys, &temp_cache)){
        throw std::runtime_error("Unable to expand descriptor");
    }

    // Cache the last hardened xpubs
    DescriptorCache diff = m_wallet_descriptor.cache.MergeAndDiff(temp_cache);
    if (!WalletBatch(m_storage.GetDatabase()).WriteDescriptorCacheItems(GetID(), diff)) {
        throw std::runtime_error(std::string(__func__) + ": writing cache items failed");
    }
}

void DescriptorScriptPubKeyMan::UpdateWalletDescriptor(WalletDescriptor& descriptor)
{
    LOCK(cs_desc_man);
    std::string error;
    if (!CanUpdateToWalletDescriptor(descriptor, error)) {
        throw std::runtime_error(std::string(__func__) + ": " + error);
    }

    m_map_pubkeys.clear();
    m_map_script_pub_keys.clear();
    m_max_cached_index = -1;
    m_wallet_descriptor = descriptor;
}

bool DescriptorScriptPubKeyMan::CanUpdateToWalletDescriptor(const WalletDescriptor& descriptor, std::string& error)
{
    LOCK(cs_desc_man);
    if (!HasWalletDescriptor(descriptor)) {
        error = "can only update matching descriptor";
        return false;
    }

    if (descriptor.range_start > m_wallet_descriptor.range_start ||
        descriptor.range_end < m_wallet_descriptor.range_end) {
        // Use inclusive range for error
        error = strprintf("new range must include current range = [%d,%d]",
                          m_wallet_descriptor.range_start,
                          m_wallet_descriptor.range_end - 1);
        return false;
    }

    return true;
}
} // namespace wallet
