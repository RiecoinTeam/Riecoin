// Copyright (c) 2019-2021 The Bitcoin Core developers
// Copyright (c) 2013-2023 The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_SCRIPTPUBKEYMAN_H
#define BITCOIN_WALLET_SCRIPTPUBKEYMAN_H

#include <psbt.h>
#include <script/descriptor.h>
#include <script/signingprovider.h>
#include <script/standard.h>
#include <util/error.h>
#include <util/message.h>
#include <util/result.h>
#include <util/time.h>
#include <wallet/crypter.h>
#include <wallet/ismine.h>
#include <wallet/walletdb.h>
#include <wallet/walletutil.h>

#include <boost/signals2/signal.hpp>

#include <optional>
#include <unordered_map>

enum class OutputType;
struct bilingual_str;

namespace wallet {
// Wallet storage things that ScriptPubKeyMans need in order to be able to store things to the wallet database.
// It provides access to things that are part of the entire wallet and not specific to a ScriptPubKeyMan such as
// wallet flags, wallet version, encryption keys, encryption status, and the database itself. This allows a
// ScriptPubKeyMan to have callbacks into CWallet without causing a circular dependency.
// WalletStorage should be the same for all ScriptPubKeyMans of a wallet.
class WalletStorage
{
public:
    virtual ~WalletStorage() = default;
    virtual const std::string GetDisplayName() const = 0;
    virtual WalletDatabase& GetDatabase() const = 0;
    virtual bool IsWalletFlagSet(uint64_t) const = 0;
    virtual void UnsetBlankWalletFlag(WalletBatch&) = 0;
    virtual bool CanSupportFeature(enum WalletFeature) const = 0;
    virtual void SetMinVersion(enum WalletFeature, WalletBatch* = nullptr) = 0;
    virtual const CKeyingMaterial& GetEncryptionKey() const = 0;
    virtual bool HasEncryptionKeys() const = 0;
    virtual bool IsLocked() const = 0;
};

//! Default for -keypool
static const unsigned int DEFAULT_KEYPOOL_SIZE = 1000;

std::vector<CKeyID> GetAffectedKeys(const CScript& spk, const SigningProvider& provider);

/** A key from a CWallet's keypool
 *
 * The wallet holds one (for pre HD-split wallets) or several keypools. These
 * are sets of keys that have not yet been used to provide addresses or receive
 * change.
 *
 * The Bitcoin Core wallet was originally a collection of unrelated private
 * keys with their associated addresses. If a non-HD wallet generated a
 * key/address, gave that address out and then restored a backup from before
 * that key's generation, then any funds sent to that address would be
 * lost definitively.
 *
 * The keypool was implemented to avoid this scenario (commit: 10384941). The
 * wallet would generate a set of keys (100 by default). When a new public key
 * was required, either to give out as an address or to use in a change output,
 * it would be drawn from the keypool. The keypool would then be topped up to
 * maintain 100 keys. This ensured that as long as the wallet hadn't used more
 * than 100 keys since the previous backup, all funds would be safe, since a
 * restored wallet would be able to scan for all owned addresses.
 *
 * A keypool also allowed encrypted wallets to give out addresses without
 * having to be decrypted to generate a new private key.
 *
 * With the introduction of HD wallets (commit: f1902510), the keypool
 * essentially became an address look-ahead pool. Restoring old backups can no
 * longer definitively lose funds as long as the addresses used were from the
 * wallet's HD seed (since all private keys can be rederived from the seed).
 * However, if many addresses were used since the backup, then the wallet may
 * not know how far ahead in the HD chain to look for its addresses. The
 * keypool is used to implement a 'gap limit'. The keypool maintains a set of
 * keys (by default 1000) ahead of the last used key and scans for the
 * addresses of those keys.  This avoids the risk of not seeing transactions
 * involving the wallet's addresses, or of re-using the same address.
 * In the unlikely case where none of the addresses in the `gap limit` are
 * used on-chain, the look-ahead will not be incremented to keep
 * a constant size and addresses beyond this range will not be detected by an
 * old backup. For this reason, it is not recommended to decrease keypool size
 * lower than default value.
 *
 * The HD-split wallet feature added a second keypool (commit: 02592f4c). There
 * is an external keypool (for addresses to hand out) and an internal keypool
 * (for change addresses).
 *
 * Keypool keys are stored in the wallet/keystore's keymap. The keypool data is
 * stored as sets of indexes in the wallet (setInternalKeyPool,
 * setExternalKeyPool and set_pre_split_keypool), and a map from the key to the
 * index (m_pool_key_to_index). The CKeyPool object is used to
 * serialize/deserialize the pool data to/from the database.
 */
class CKeyPool
{
public:
    //! The time at which the key was generated. Set in AddKeypoolPubKeyWithDB
    int64_t nTime;
    //! The public key
    CPubKey vchPubKey;
    //! Whether this keypool entry is in the internal keypool (for change outputs)
    bool fInternal;
    //! Whether this key was generated for a keypool before the wallet was upgraded to HD-split
    bool m_pre_split;

    CKeyPool();
    CKeyPool(const CPubKey& vchPubKeyIn, bool internalIn);

    template<typename Stream>
    void Serialize(Stream& s) const
    {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH)) {
            s << nVersion;
        }
        s << nTime << vchPubKey << fInternal << m_pre_split;
    }

    template<typename Stream>
    void Unserialize(Stream& s)
    {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH)) {
            s >> nVersion;
        }
        s >> nTime >> vchPubKey;
        try {
            s >> fInternal;
        } catch (std::ios_base::failure&) {
            /* flag as external address if we can't read the internal boolean
               (this will be the case for any wallet before the HD chain split version) */
            fInternal = false;
        }
        try {
            s >> m_pre_split;
        } catch (std::ios_base::failure&) {
            /* flag as postsplit address if we can't read the m_pre_split boolean
               (this will be the case for any wallet that upgrades to HD chain split) */
            m_pre_split = false;
        }
    }
};

struct WalletDestination
{
    CTxDestination dest;
    std::optional<bool> internal;
};

/*
 * A class implementing ScriptPubKeyMan manages some (or all) scriptPubKeys used in a wallet.
 * It contains the scripts and keys related to the scriptPubKeys it manages.
 * A ScriptPubKeyMan will be able to give out scriptPubKeys to be used, as well as marking
 * when a scriptPubKey has been used. It also handles when and how to store a scriptPubKey
 * and its related scripts and keys, including encryption.
 */
class ScriptPubKeyMan
{
protected:
    WalletStorage& m_storage;

    SigningResult SignMessageBIP322(MessageSignatureFormat format, const SigningProvider* keystore, const std::string& message, const CTxDestination& address, std::string& str_sig) const;

public:
    explicit ScriptPubKeyMan(WalletStorage& storage) : m_storage(storage) {}
    virtual ~ScriptPubKeyMan() {};
    virtual util::Result<CTxDestination> GetNewDestination(const OutputType type) { return util::Error{Untranslated("Not supported")}; }
    virtual isminetype IsMine(const CScript& script) const { return ISMINE_NO; }

    //! Check that the given decryption key is valid for this ScriptPubKeyMan, i.e. it decrypts all of the keys handled by it.
    virtual bool CheckDecryptionKey(const CKeyingMaterial& master_key, bool accept_no_keys = false) { return false; }
    virtual bool Encrypt(const CKeyingMaterial& master_key, WalletBatch* batch) { return false; }

    virtual util::Result<CTxDestination> GetReservedDestination(const OutputType type, bool internal, int64_t& index, CKeyPool& keypool) { return util::Error{Untranslated("Not supported")}; }
    virtual void KeepDestination(int64_t index, const OutputType& type) {}
    virtual void ReturnDestination(int64_t index, bool internal, const CTxDestination& addr) {}

    /** Fills internal address pool. Use within ScriptPubKeyMan implementations should be used sparingly and only
      * when something from the address pool is removed, excluding GetNewDestination and GetReservedDestination.
      * External wallet code is primarily responsible for topping up prior to fetching new addresses
      */
    virtual bool TopUp(unsigned int size = 0) { return false; }

    /** Mark unused addresses as being used
     * Affects all keys up to and including the one determined by provided script.
     *
     * @param script determines the last key to mark as used
     *
     * @return All of the addresses affected
     */
    virtual std::vector<WalletDestination> MarkUnusedAddresses(const CScript& script) { return {}; }

    /** Sets up the key generation stuff, i.e. generates new HD seeds and sets them as active.
      * Returns false if already setup or setup fails, true if setup is successful
      * Set force=true to make it re-setup if already setup, used for upgrades
      */
    virtual bool SetupGeneration(bool force = false) { return false; }

    /* Returns true if HD is enabled */
    virtual bool IsHDEnabled() const { return false; }

    /* Returns true if the wallet can give out new addresses. This means it has keys in the keypool or can generate new keys */
    virtual bool CanGetAddresses(bool internal = false) const { return false; }

    /** Upgrades the wallet to the specified version */
    virtual bool Upgrade(int prev_version, int new_version, bilingual_str& error) { return true; }

    virtual bool HavePrivateKeys() const { return false; }

    //! The action to do when the DB needs rewrite
    virtual void RewriteDB() {}

    virtual unsigned int GetKeyPoolSize() const { return 0; }

    virtual int64_t GetTimeFirstKey() const { return 0; }

    virtual std::unique_ptr<CKeyMetadata> GetMetadata(const CTxDestination& dest) const { return nullptr; }

    virtual std::unique_ptr<SigningProvider> GetSolvingProvider(const CScript& script) const { return nullptr; }

    /** Whether this ScriptPubKeyMan can provide a SigningProvider (via GetSolvingProvider) that, combined with
      * sigdata, can produce solving data.
      */
    virtual bool CanProvide(const CScript& script, SignatureData& sigdata) { return false; }

    /** Creates new signatures and adds them to the transaction. Returns whether all inputs were signed */
    virtual bool SignTransaction(CMutableTransaction& tx, const std::map<COutPoint, Coin>& coins, int sighash, std::map<int, bilingual_str>& input_errors) const { return false; }
    /** Sign a message with the given script */
    virtual SigningResult SignMessage(const MessageSignatureFormat format, const std::string& message, const CTxDestination& address, std::string& str_sig) const { return SigningResult::SIGNING_FAILED; };
    /** Adds script and derivation path information to a PSBT, and optionally signs it. */
    virtual TransactionError FillPSBT(PartiallySignedTransaction& psbt, const PrecomputedTransactionData& txdata, int sighash_type = SIGHASH_DEFAULT, bool sign = true, bool bip32derivs = false, int* n_signed = nullptr, bool finalize = true) const { return TransactionError::INVALID_PSBT; }

    virtual uint256 GetID() const { return uint256(); }

    /** Returns a set of all the scriptPubKeys that this ScriptPubKeyMan watches */
    virtual const std::unordered_set<CScript, SaltedSipHasher> GetScriptPubKeys() const { return {}; };

    /** Prepends the wallet name in logging output to ease debugging in multi-wallet use cases */
    template<typename... Params>
    void WalletLogPrintf(std::string fmt, Params... parameters) const {
        LogPrintf(("%s " + fmt).c_str(), m_storage.GetDisplayName(), parameters...);
    };

    /** Keypool has new keys */
    boost::signals2::signal<void ()> NotifyCanGetAddressesChanged;
};

class DescriptorScriptPubKeyMan : public ScriptPubKeyMan
{
private:
    using ScriptPubKeyMap = std::map<CScript, int32_t>; // Map of scripts to descriptor range index
    using PubKeyMap = std::map<CPubKey, int32_t>; // Map of pubkeys involved in scripts to descriptor range index
    using CryptedKeyMap = std::map<CKeyID, std::pair<CPubKey, std::vector<unsigned char>>>;
    using KeyMap = std::map<CKeyID, CKey>;

    ScriptPubKeyMap m_map_script_pub_keys GUARDED_BY(cs_desc_man);
    PubKeyMap m_map_pubkeys GUARDED_BY(cs_desc_man);
    int32_t m_max_cached_index = -1;

    KeyMap m_map_keys GUARDED_BY(cs_desc_man);
    CryptedKeyMap m_map_crypted_keys GUARDED_BY(cs_desc_man);

    //! keeps track of whether Unlock has run a thorough check before
    bool m_decryption_thoroughly_checked = false;

    bool AddDescriptorKeyWithDB(WalletBatch& batch, const CKey& key, const CPubKey &pubkey) EXCLUSIVE_LOCKS_REQUIRED(cs_desc_man);

    KeyMap GetKeys() const EXCLUSIVE_LOCKS_REQUIRED(cs_desc_man);

    // Cached FlatSigningProviders to avoid regenerating them each time they are needed.
    mutable std::map<int32_t, FlatSigningProvider> m_map_signing_providers;
    // Fetch the SigningProvider for the given script and optionally include private keys
    std::unique_ptr<FlatSigningProvider> GetSigningProvider(const CScript& script, bool include_private = false) const;
    // Fetch the SigningProvider for the given pubkey and always include private keys. This should only be called by signing code.
    std::unique_ptr<FlatSigningProvider> GetSigningProvider(const CPubKey& pubkey) const;
    // Fetch the SigningProvider for a given index and optionally include private keys. Called by the above functions.
    std::unique_ptr<FlatSigningProvider> GetSigningProvider(int32_t index, bool include_private = false) const EXCLUSIVE_LOCKS_REQUIRED(cs_desc_man);

protected:
  WalletDescriptor m_wallet_descriptor GUARDED_BY(cs_desc_man);

public:
    DescriptorScriptPubKeyMan(WalletStorage& storage, WalletDescriptor& descriptor)
        :   ScriptPubKeyMan(storage),
            m_wallet_descriptor(descriptor)
        {}
    DescriptorScriptPubKeyMan(WalletStorage& storage)
        :   ScriptPubKeyMan(storage)
        {}

    mutable RecursiveMutex cs_desc_man;

    util::Result<CTxDestination> GetNewDestination(const OutputType type) override;
    isminetype IsMine(const CScript& script) const override;

    bool CheckDecryptionKey(const CKeyingMaterial& master_key, bool accept_no_keys = false) override;
    bool Encrypt(const CKeyingMaterial& master_key, WalletBatch* batch) override;

    util::Result<CTxDestination> GetReservedDestination(const OutputType type, bool internal, int64_t& index, CKeyPool& keypool) override;
    void ReturnDestination(int64_t index, bool internal, const CTxDestination& addr) override;

    // Tops up the descriptor cache and m_map_script_pub_keys. The cache is stored in the wallet file
    // and is used to expand the descriptor in GetNewDestination. DescriptorScriptPubKeyMan relies
    // more on ephemeral data. For wallets using unhardened derivation (with or without private keys), the "keypool" is a single xpub.
    bool TopUp(unsigned int size = 0) override;

    std::vector<WalletDestination> MarkUnusedAddresses(const CScript& script) override;

    bool IsHDEnabled() const override;

    //! Setup descriptors based on the given CExtkey
    bool SetupDescriptorGeneration(const CExtKey& master_key, OutputType addr_type, bool internal);

    /** Provide a descriptor at setup time
    * Returns false if already setup or setup fails, true if setup is successful
    */
    bool SetupDescriptor(std::unique_ptr<Descriptor>desc);

    bool HavePrivateKeys() const override;

    unsigned int GetKeyPoolSize() const override;

    int64_t GetTimeFirstKey() const override;

    std::unique_ptr<CKeyMetadata> GetMetadata(const CTxDestination& dest) const override;

    bool CanGetAddresses(bool internal = false) const override;

    std::unique_ptr<SigningProvider> GetSolvingProvider(const CScript& script) const override;

    bool CanProvide(const CScript& script, SignatureData& sigdata) override;

    bool SignTransaction(CMutableTransaction& tx, const std::map<COutPoint, Coin>& coins, int sighash, std::map<int, bilingual_str>& input_errors) const override;
    SigningResult SignMessage(const MessageSignatureFormat format, const std::string& message, const CTxDestination& address, std::string& str_sig) const override;
    TransactionError FillPSBT(PartiallySignedTransaction& psbt, const PrecomputedTransactionData& txdata, int sighash_type = SIGHASH_DEFAULT, bool sign = true, bool bip32derivs = false, int* n_signed = nullptr, bool finalize = true) const override;

    uint256 GetID() const override;

    void SetCache(const DescriptorCache& cache);

    bool AddKey(const CKeyID& key_id, const CKey& key);
    bool AddCryptedKey(const CKeyID& key_id, const CPubKey& pubkey, const std::vector<unsigned char>& crypted_key);

    bool HasWalletDescriptor(const WalletDescriptor& desc) const;
    void UpdateWalletDescriptor(WalletDescriptor& descriptor);
    bool CanUpdateToWalletDescriptor(const WalletDescriptor& descriptor, std::string& error);
    void AddDescriptorKey(const CKey& key, const CPubKey &pubkey);
    void WriteDescriptor();

    const WalletDescriptor GetWalletDescriptor() const EXCLUSIVE_LOCKS_REQUIRED(cs_desc_man);
    const std::unordered_set<CScript, SaltedSipHasher> GetScriptPubKeys() const override;

    bool GetDescriptorString(std::string& out, const bool priv) const;

    void UpgradeDescriptorCache();
};
} // namespace wallet

#endif // BITCOIN_WALLET_SCRIPTPUBKEYMAN_H
