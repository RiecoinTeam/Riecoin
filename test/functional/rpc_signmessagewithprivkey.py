#!/usr/bin/env python3
# Copyright (c) 2016-present The Bitcoin Core developers
# Copyright (c) 2016-present The Riecoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test RPC commands for signing messages with private key."""

from test_framework.descriptors import (
    descsum_create,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)


class SignMessagesWithPrivTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def addresses_from_privkey(self, priv_key):
        '''Return addresses for a given WIF private key in legacy (P2PKH),
           nested segwit (P2SH-P2WPKH) and native segwit (P2WPKH) formats.'''
        descriptors = f'pkh({priv_key})', f'sh(wpkh({priv_key}))', f'wpkh({priv_key})'
        return [self.nodes[0].deriveaddresses(descsum_create(desc))[0] for desc in descriptors]

    def run_test(self):
        message = 'This is just a test message'

        self.log.info('test signing with priv_key')
        priv_key = 'prvd2b8a0116d641fe7d3036f8464628fb595b480414c13a301b3d4038c811c28b0'
        expected_signature = 'ILo9wUJXRheXNKGSbXKcKgXKMnsGbcmKUQK8aqQILG84BbrQKTYiwynQWURmc+0itT7NgXfRBUbZqiYB6gLUlWU='
        signature = self.nodes[0].signmessagewithprivkey(priv_key, message)
        assert_equal(expected_signature, signature)

        self.log.info('test that verifying with P2PKH address succeeds')
        addresses = self.addresses_from_privkey(priv_key)
        assert_equal(addresses[2], 'rric1qvza2pay5kwxw8j2qm6n87wqym3fdr7u5aeta6x')
        assert self.nodes[0].verifymessage(addresses[2], signature, message)

        self.log.info('test that verifying with non-P2PKH addresses throws error')
        for non_p2pkh_address in addresses[0:2]:
            assert_raises_rpc_error(-3, "Address does not refer to key", self.nodes[0].verifymessage, non_p2pkh_address, signature, message)

        self.log.info('test parameter validity and error codes')
        # signmessagewithprivkey has two required parameters
        for num_params in [0, 1, 3, 4, 5]:
            param_list = ["dummy"]*num_params
            assert_raises_rpc_error(-1, "signmessagewithprivkey", self.nodes[0].signmessagewithprivkey, *param_list)
        # verifymessage has three required parameters
        for num_params in [0, 1, 2, 4, 5]:
            param_list = ["dummy"]*num_params
            assert_raises_rpc_error(-1, "verifymessage", self.nodes[0].verifymessage, *param_list)
        # invalid key or address provided
        assert_raises_rpc_error(-5, "Invalid private key", self.nodes[0].signmessagewithprivkey, "invalid_key", message)
        assert_raises_rpc_error(-5, "Invalid address", self.nodes[0].verifymessage, "invalid_addr", signature, message)
        # malformed signature provided
        assert_raises_rpc_error(-3, "Malformed base64 encoding", self.nodes[0].verifymessage, 'rric1qvza2pay5kwxw8j2qm6n87wqym3fdr7u5aeta6x', "invalid_sig", message)


if __name__ == '__main__':
    SignMessagesWithPrivTest(__file__).main()
