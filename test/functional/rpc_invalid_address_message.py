#!/usr/bin/env python3
# Copyright (c) 2020-2021 The Bitcoin Core developers
# Copyright (c) 2013-2023 The Riecoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test error messages for 'getaddressinfo' and 'validateaddress' RPC commands."""

from test_framework.test_framework import BitcoinTestFramework

from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)

BECH32_VALID = 'rric1qtmp74ayg7p24uslctssvjm06q5phz4yrkrkxpr'
BECH32_VALID_CAPITALS = 'RRIC1QPLMTZKC2XHARPPZDLNPAQL78RSHJ68U3PUNEVV'
BECH32_VALID_MULTISIG = 'rric1qdg3myrgvzw7ml9q0ejxhlkyxm7vl9r56yzkfgvzclrf4hkpx9yfqna847z'

BECH32_INVALID_BECH32 = 'rric1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqf807s2'
BECH32_INVALID_BECH32M = 'rric1qw508d6qejxtdg4y5r3zarvary0c5xw7kpt4dsz'
BECH32_INVALID_VERSION = 'rric130xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqq09sqy'
BECH32_INVALID_SIZE = 'rric1s0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7v8n0nx0muaewav25lfmgrc'
BECH32_INVALID_V0_SIZE = 'rric1qw508d6qejxtdg4y5r3zarvary0c5xw7kqqzrey95'
BECH32_INVALID_PREFIX = 'ric1pw508d6qejxtdg4y5r3zarvary0c5xw7kw508d6qejxtdg4y5r3zarvary0c5xw7ke8662q'
BECH32_TOO_LONG = 'rric1q049edschfnwystcqnsvyfpj23mpsg3jcedq9xv049edschfnwystcqnsvyfpj23mpsg3jcedq9xv049edschfnwystcqnsvy6pdukm'
BECH32_ONE_ERROR = 'rric1q049edschfnwystcqnsvyfpj23mpsg3jcfjwt5r'
BECH32_ONE_ERROR_CAPITALS = 'RRIC1QPLMTZKC2XHARPPZDLNPAQL78RSHJ68U3QUNEVV'
BECH32_TWO_ERRORS = 'rric1qax9suht3qv95sw33xavx8crpxduefdrsxh7cdf' # should be rric1qax9suht3qv95sw33wavx8crpxduefdrsuh7cdf
BECH32_NO_SEPARATOR = 'rricq049ldschfnwystcqnsvyfpj23mpsg3jcfjwt5r'
BECH32_INVALID_CHAR = 'rric1q04oldschfnwystcqnsvyfpj23mpsg3jcfjwt5r'
BECH32_MULTISIG_TWO_ERRORS = 'rric1qdg3myrgvzw7ml8q0ejxhlkyxn7vl9r56yzkfgvzclrf4hkpx9yfqna847z'
BECH32_WRONG_VERSION = 'rric1ptmp74ayg7p24uslctssvjm06q5phz4yrkrkxpr'

BASE58_VALID = 'rNXS6ptDbskGGiJSnLZUYBsSqJDAdL3nxL'
BASE58_INVALID_PREFIX = 'REcZ5EvpB51RVqnrobzTqzK1gcS3JDycpj'
BASE58_INVALID_CHECKSUM = 'rNXS6ptDbskGGiJSnLZUYBsSqJDAdLAnxL'
BASE58_INVALID_LENGTH = 'tu45HDTX8ridnq7m5Y7BXw92iAQPSQ9zC1CRtjWaDVhohPHyGy'

INVALID_ADDRESS = 'asfah14i8fajz0123f'
INVALID_ADDRESS_2 = '1q049ldschfnwystcqnsvyfpj23mpsg3jcedq9xv'

class InvalidAddressErrorMessageTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def check_valid(self, addr):
        info = self.nodes[0].validateaddress(addr)
        assert info['isvalid']
        assert 'error' not in info
        assert 'error_locations' not in info

    def check_invalid(self, addr, error_str, error_locations=None):
        res = self.nodes[0].validateaddress(addr)
        assert not res['isvalid']
        assert_equal(res['error'], error_str)
        if error_locations:
            assert_equal(res['error_locations'], error_locations)
        else:
            assert_equal(res['error_locations'], [])

    def test_validateaddress(self):
        # Invalid Bech32
        self.check_invalid(BECH32_INVALID_SIZE, 'Invalid Bech32 address data size')
        self.check_invalid(BECH32_INVALID_PREFIX, 'Not a valid Bech32 or Base58 encoding')
        self.check_invalid(BECH32_INVALID_BECH32, 'Version 1+ witness address must use Bech32m checksum')
        self.check_invalid(BECH32_INVALID_BECH32M, 'Version 0 witness address must use Bech32 checksum')
        self.check_invalid(BECH32_INVALID_VERSION, 'Invalid Bech32 address witness version')
        self.check_invalid(BECH32_INVALID_V0_SIZE, 'Invalid Bech32 v0 address data size')
        self.check_invalid(BECH32_TOO_LONG, 'Bech32 string too long', list(range(90, 108)))
        self.check_invalid(BECH32_ONE_ERROR, 'Invalid Bech32 checksum', [9])
        self.check_invalid(BECH32_TWO_ERRORS, 'Invalid Bech32 checksum', [22, 38])
        self.check_invalid(BECH32_ONE_ERROR_CAPITALS, 'Invalid Bech32 checksum', [38])
        self.check_invalid(BECH32_NO_SEPARATOR, 'Missing separator')
        self.check_invalid(BECH32_INVALID_CHAR, 'Invalid Base 32 character', [8])
        self.check_invalid(BECH32_MULTISIG_TWO_ERRORS, 'Invalid Bech32 checksum', [19, 30])
        self.check_invalid(BECH32_WRONG_VERSION, 'Invalid Bech32 checksum', [5])

        # Valid Bech32
        self.check_valid(BECH32_VALID)
        self.check_valid(BECH32_VALID_CAPITALS)
        self.check_valid(BECH32_VALID_MULTISIG)

        # Invalid Base58
        self.check_invalid(BASE58_INVALID_PREFIX, 'Invalid prefix for Base58-encoded address')
        self.check_invalid(BASE58_INVALID_CHECKSUM, 'Invalid checksum or length of Base58 address')
        self.check_invalid(BASE58_INVALID_LENGTH, 'Invalid checksum or length of Base58 address')

        # Valid Base58
        self.check_valid(BASE58_VALID)

        # Invalid address format
        self.check_invalid(INVALID_ADDRESS, 'Not a valid Bech32 or Base58 encoding')
        self.check_invalid(INVALID_ADDRESS_2, 'Not a valid Bech32 or Base58 encoding')

    def test_getaddressinfo(self):
        node = self.nodes[0]

        assert_raises_rpc_error(-5, "Invalid Bech32 address data size", node.getaddressinfo, BECH32_INVALID_SIZE)

        assert_raises_rpc_error(-5, "Not a valid Bech32 or Base58 encoding", node.getaddressinfo, BECH32_INVALID_PREFIX)

        assert_raises_rpc_error(-5, "Invalid prefix for Base58-encoded address", node.getaddressinfo, BASE58_INVALID_PREFIX)

        assert_raises_rpc_error(-5, "Not a valid Bech32 or Base58 encoding", node.getaddressinfo, INVALID_ADDRESS)

    def run_test(self):
        self.test_validateaddress()

        if self.is_wallet_compiled():
            self.init_wallet(node=0)
            self.test_getaddressinfo()


if __name__ == '__main__':
    InvalidAddressErrorMessageTest().main()
