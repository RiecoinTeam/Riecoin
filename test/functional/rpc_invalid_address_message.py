#!/usr/bin/env python3
# Copyright (c) 2020-present The Bitcoin Core developers
# Copyright (c) 2020-present The Riecoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test error messages for 'getaddressinfo' and 'validateaddress' RPC commands."""

from test_framework.test_framework import BitcoinTestFramework

from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)

BECH32_VALID = 'rric1qtmp74ayg7p24uslctssvjm06q5phz4yrrlx2yp'
BECH32_VALID_UNKNOWN_WITNESS = 'rric1p424q0sfn4y'
BECH32_VALID_CAPITALS = 'RRIC1QPLMTZKC2XHARPPZDLNPAQL78RSHJ68U35QR4FW'
BECH32_VALID_MULTISIG = 'rric1qdg3myrgvzw7ml9q0ejxhlkyxm7vl9r56yzkfgvzclrf4hkpx9yfqxphemq'

BECH32_INVALID_BECH32 = 'rric1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqf807s2'
BECH32_INVALID_VERSION = 'rric130xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqq09sqy'
BECH32_INVALID_SIZE = 'rric1s0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7v8n0nx0muaewav25lfmgrc'
BECH32_INVALID_V0_SIZE = 'rric1qw508d6qejxtdg4y5r3zarvary0c5xw7kqqhlfgqk'
BECH32_INVALID_PREFIX = 'ric1pw508d6qejxtdg4y5r3zarvary0c5xw7kw508d6qejxtdg4y5r3zarvary0c5xw7ke8662q'
BECH32_TOO_LONG = 'rric1q049edschfnwystcqnsvyfpj23mpsg3jcedq9xv049edschfnwystcqnsvyfpj23mpsg3jcedq9xv049edschfnwystcqnsvy6pdukm'
BECH32_ONE_ERROR = 'rric1q049edschfnwystcqnsvyfpj23mpsg3jcuw783p'
BECH32_ONE_ERROR_CAPITALS = 'RRIC1QPLMTZKC2XHARPPZDLNPAQL78RSHJ68U36QR4FW'
BECH32_TWO_ERRORS = 'rric1qax9suht3qv95sw33xavx8crpxduefdrsgtw5gt' # should be rric1qax9suht3qv95sw33wavx8crpxduefdrsuh7cdf
BECH32_NO_SEPARATOR = 'rricq049ldschfnwystcqnsvyfpj23mpsg3jcfjwt5r'
BECH32_INVALID_CHAR = 'rric1q04oldschfnwystcqnsvyfpj23mpsg3jcfjwt5r'
BECH32_MULTISIG_TWO_ERRORS = 'rric1qdg3myrgvzw7ml8q0ejxhlkyxn7vl9r56yzkfgvzclrf4hkpx9yfqxphemq'
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
        self.check_invalid(BECH32_INVALID_SIZE, "Invalid Bech32 address program size (41 bytes)")
        self.check_invalid(BECH32_INVALID_PREFIX, 'Invalid or unsupported Segwit (Bech32) or Base58 encoding.')
        self.check_invalid(BECH32_INVALID_BECH32, 'Version 1+ witness address must use Bech32m checksum')
        self.check_invalid(BECH32_INVALID_VERSION, 'Invalid Bech32 address witness version')
        self.check_invalid(BECH32_INVALID_V0_SIZE, "Invalid Bech32 v0 address program size (21 bytes), per BIP141")
        self.check_invalid(BECH32_TOO_LONG, 'Bech32 string too long', list(range(90, 108)))
        self.check_invalid(BECH32_ONE_ERROR, 'Invalid Bech32m checksum', [9])
        self.check_invalid(BECH32_TWO_ERRORS, 'Invalid Bech32m checksum', [22, 38])
        self.check_invalid(BECH32_ONE_ERROR_CAPITALS, 'Invalid Bech32m checksum', [38])
        self.check_invalid(BECH32_NO_SEPARATOR, 'Missing separator')
        self.check_invalid(BECH32_INVALID_CHAR, 'Invalid Base 32 character', [8])
        self.check_invalid(BECH32_MULTISIG_TWO_ERRORS, 'Invalid Bech32m checksum', [19, 30])
        self.check_invalid(BECH32_WRONG_VERSION, 'Invalid checksum')

        # Valid Bech32
        self.check_valid(BECH32_VALID)
        self.check_valid(BECH32_VALID_UNKNOWN_WITNESS)
        self.check_valid(BECH32_VALID_CAPITALS)
        self.check_valid(BECH32_VALID_MULTISIG)

        # Invalid Base58
        self.check_invalid(BASE58_INVALID_PREFIX, 'Invalid or unsupported Base58-encoded address.')
        self.check_invalid(BASE58_INVALID_CHECKSUM, 'Invalid checksum or length of Base58 address (P2PKH or P2SH)')
        self.check_invalid(BASE58_INVALID_LENGTH, 'Invalid checksum or length of Base58 address (P2PKH or P2SH)')

        # Valid Base58
        self.check_valid(BASE58_VALID)

        # Invalid address format
        self.check_invalid(INVALID_ADDRESS, 'Invalid or unsupported Segwit (Bech32) or Base58 encoding.')
        self.check_invalid(INVALID_ADDRESS_2, 'Invalid or unsupported Segwit (Bech32) or Base58 encoding.')

        node = self.nodes[0]

        # Missing arg returns the help text
        assert_raises_rpc_error(-1, "Return information about the given Riecoin address.", node.validateaddress)
        # Explicit None is not allowed for required parameters
        assert_raises_rpc_error(-3, "JSON value of type null is not of expected type string", node.validateaddress, None)

    def test_getaddressinfo(self):
        node = self.nodes[0]

        assert_raises_rpc_error(-5, "Invalid Bech32 address program size (41 bytes)", node.getaddressinfo, BECH32_INVALID_SIZE)
        assert_raises_rpc_error(-5, "Invalid or unsupported Segwit (Bech32) or Base58 encoding.", node.getaddressinfo, BECH32_INVALID_PREFIX)
        assert_raises_rpc_error(-5, "Invalid or unsupported Base58-encoded address.", node.getaddressinfo, BASE58_INVALID_PREFIX)
        assert_raises_rpc_error(-5, "Invalid or unsupported Segwit (Bech32) or Base58 encoding.", node.getaddressinfo, INVALID_ADDRESS)
        assert "isscript" not in node.getaddressinfo(BECH32_VALID_UNKNOWN_WITNESS)

    def run_test(self):
        self.test_validateaddress()

        if self.is_wallet_compiled():
            self.init_wallet(node=0)
            self.test_getaddressinfo()


if __name__ == '__main__':
    InvalidAddressErrorMessageTest(__file__).main()
