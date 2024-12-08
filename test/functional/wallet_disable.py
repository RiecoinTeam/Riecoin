#!/usr/bin/env python3
# Copyright (c) 2015-present The Bitcoin Core developers
# Copyright (c) 2015-present The Riecoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test a node with the -disablewallet option.

- Test that validateaddress RPC works when running with -disablewallet
- Test that it is not possible to mine to an invalid address.
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_raises_rpc_error

class DisableWalletTest (BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [["-disablewallet"]]
        self.wallet_names = []

    def run_test (self):
        # Make sure wallet is really disabled
        assert_raises_rpc_error(-32601, 'Method not found', self.nodes[0].getwalletinfo)
        x = self.nodes[0].validateaddress('ric1pstellap55ue6keg3ta2qwlxr0h58g66fd7y4ea78hzkj3r4lstrsk4clvn')
        assert x['isvalid'] == False
        x = self.nodes[0].validateaddress('rric1pstellap55ue6keg3ta2qwlxr0h58g66fd7y4ea78hzkj3r4lstrs8rqekw')
        assert x['isvalid'] == True


if __name__ == '__main__':
    DisableWalletTest(__file__).main()
