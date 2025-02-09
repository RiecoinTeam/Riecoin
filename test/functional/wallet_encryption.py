#!/usr/bin/env python3
# Copyright (c) 2016-2022 The Bitcoin Core developers
# Copyright (c) 2013-present The Riecoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Wallet encryption"""

import time
import subprocess

from test_framework.messages import hash256
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_raises_rpc_error,
    assert_equal,
)
from test_framework.wallet_util import WalletUnlock


class WalletEncryptionTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        passphrase = "WalletPassphrase"
        passphrase2 = "SecondWalletPassphrase"

        # Make sure the wallet isn't encrypted first
        msg = "test message"
        address = self.nodes[0].getnewaddress()
        sig = self.nodes[0].signmessage(address, msg)
        assert self.nodes[0].verifymessage(address, sig, msg)
        assert_raises_rpc_error(-15, "Error: running with an unencrypted wallet, but walletpassphrase was called", self.nodes[0].walletpassphrase, 'ff', 1)
        assert_raises_rpc_error(-15, "Error: running with an unencrypted wallet, but walletpassphrasechange was called.", self.nodes[0].walletpassphrasechange, 'ff', 'ff')

        # Encrypt the wallet
        assert_raises_rpc_error(-8, "passphrase cannot be empty", self.nodes[0].encryptwallet, '')
        self.nodes[0].encryptwallet(passphrase)

        # Test that the wallet is encrypted
        assert_raises_rpc_error(-13, "Please enter the wallet passphrase with walletpassphrase first", self.nodes[0].signmessage, address, msg)
        assert_raises_rpc_error(-15, "Error: running with an encrypted wallet, but encryptwallet was called.", self.nodes[0].encryptwallet, 'ff')
        assert_raises_rpc_error(-8, "passphrase cannot be empty", self.nodes[0].walletpassphrase, '', 1)
        assert_raises_rpc_error(-8, "passphrase cannot be empty", self.nodes[0].walletpassphrasechange, '', 'ff')

        # Check that walletpassphrase works
        self.nodes[0].walletpassphrase(passphrase, 2)
        sig = self.nodes[0].signmessage(address, msg)
        assert self.nodes[0].verifymessage(address, sig, msg)

        # Check that the timeout is right
        time.sleep(3)
        assert_raises_rpc_error(-13, "Please enter the wallet passphrase with walletpassphrase first", self.nodes[0].signmessage, address, msg)

        # Test wrong passphrase
        assert_raises_rpc_error(-14, "wallet passphrase entered was incorrect", self.nodes[0].walletpassphrase, passphrase + "wrong", 10)

        # Test walletlock
        with WalletUnlock(self.nodes[0], passphrase):
            sig = self.nodes[0].signmessage(address, msg)
            assert self.nodes[0].verifymessage(address, sig, msg)
        assert_raises_rpc_error(-13, "Please enter the wallet passphrase with walletpassphrase first", self.nodes[0].signmessage, address, msg)

        # Test passphrase changes
        self.nodes[0].walletpassphrasechange(passphrase, passphrase2)
        assert_raises_rpc_error(-14, "wallet passphrase entered was incorrect", self.nodes[0].walletpassphrase, passphrase, 10)
        with WalletUnlock(self.nodes[0], passphrase2):
            sig = self.nodes[0].signmessage(address, msg)
            assert self.nodes[0].verifymessage(address, sig, msg)

        # Test timeout bounds
        assert_raises_rpc_error(-8, "Timeout cannot be negative.", self.nodes[0].walletpassphrase, passphrase2, -10)

        self.log.info('Check a timeout less than the limit')
        MAX_VALUE = 100000000
        now = int(time.time())
        self.nodes[0].setmocktime(now)
        expected_time = now + MAX_VALUE - 600
        self.nodes[0].walletpassphrase(passphrase2, MAX_VALUE - 600)
        actual_time = self.nodes[0].getwalletinfo()['unlocked_until']
        assert_equal(actual_time, expected_time)

        self.log.info('Check a timeout greater than the limit')
        expected_time = now + MAX_VALUE
        self.nodes[0].walletpassphrase(passphrase2, MAX_VALUE + 1000)
        actual_time = self.nodes[0].getwalletinfo()['unlocked_until']
        assert_equal(actual_time, expected_time)
        self.nodes[0].walletlock()

        # Test passphrase with null characters
        passphrase_with_nulls = "Phrase\0With\0Nulls"
        self.nodes[0].walletpassphrasechange(passphrase2, passphrase_with_nulls)
        # walletpassphrasechange should not stop at null characters
        assert_raises_rpc_error(-14, "wallet passphrase entered was incorrect", self.nodes[0].walletpassphrase, passphrase_with_nulls.partition("\0")[0], 10)
        with WalletUnlock(self.nodes[0], passphrase_with_nulls):
            sig = self.nodes[0].signmessage(address, msg)
            assert self.nodes[0].verifymessage(address, sig, msg)

        self.log.info("Test that wallets without private keys cannot be encrypted")
        self.nodes[0].createwallet(wallet_name="noprivs", disable_private_keys=True)
        noprivs_wallet = self.nodes[0].get_wallet_rpc("noprivs")
        assert_raises_rpc_error(-16, "Error: wallet does not contain private keys, nothing to encrypt.", noprivs_wallet.encryptwallet, "pass")

        if self.is_wallet_tool_compiled():
            self.log.info("Test that encryption keys in wallets without privkeys are removed")

            def do_wallet_tool(*args):
                proc = subprocess.Popen(
                    [self.options.bitcoinwallet, f"-datadir={self.nodes[0].datadir_path}", f"-chain={self.chain}"] + list(args),
                    stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True
                )
                stdout, stderr = proc.communicate()
                assert_equal(proc.poll(), 0)
                assert_equal(stderr, "")

if __name__ == '__main__':
    WalletEncryptionTest(__file__).main()
