#!/usr/bin/env python3
# Copyright (c) 2023-present The Bitcoin Core developers
# Copyright (c) 2023-present The Riecoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test validateaddress for main chain"""

from test_framework.test_framework import BitcoinTestFramework

from test_framework.util import assert_equal

INVALID_DATA = [
    # BIP 173
    (
        "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kemeawh",
        "Invalid or unsupported Segwit (Bech32) encoding or Script.",  # Invalid hrp
        [],
    ),
    ("ric1qw508d6qejxtdg4y5r3zarvary0c5xw7kn4h7nl", "Invalid Bech32m checksum", [42]),
    (
        "RIC13W508D6QEJXTDG4Y5R3ZARVARY0C5XW7KEMPD5T",
        "Version 1+ witness address must use Bech32m checksum",
        [],
    ),
    (
        "ric1rw5cmwppx",
        "Version 1+ witness address must use Bech32m checksum",  # Invalid program length
        [],
    ),
    (
        "ric10w508d6qejxtdg4y5r3zarvary0c5xw7kw508d6qejxtdg4y5r3zarvary0c5xw7kw5pgs7z9",
        "Version 1+ witness address must use Bech32m checksum",  # Invalid program length
        [],
    ),
    (
        "RIC1QR508D6QEJXTDG4Y5R3ZARVARYVVKV8TQ",
        "Invalid Bech32 v0 address program size (16 bytes), per BIP141",
        [],
    ),
    (
        "btc1qrp33g0q5c5txsp9arysrx4k6zdkfs4nce4xj0gdcccefvpysxf3q8074Mu",
        "Invalid or unsupported Segwit (Bech32) encoding or Script.",  # btc1, Mixed case
        [],
    ),
    (
        "ric1qw508d6qejxtdg4y5r3zarvary0c5xw7kn4h7Nk",
        "Invalid character or mixed case",  # ric1, Mixed case, not in BIP 173 test vectors
        [41],
    ),
    (
        "ric1zw508d6qejxtdg4y5r3zarvaryvqdf7g2g",
        "Version 1+ witness address must use Bech32m checksum",  # Wrong padding
        [],
    ),
    (
        "tb1qrp33g0q5c5txsp9arysrx4k6zdkfs4nce4xj0gdcccefvpysxf3pjxtptv",
        "Invalid or unsupported Segwit (Bech32) encoding or Script.",  # tb1, Non-zero padding in 8-to-5 conversion
        [],
    ),
    ("ric1tpvwza", "Empty Bech32 data section", []),
    # BIP 350
    (
        "tc1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vq5zuyut",
        "Invalid or unsupported Segwit (Bech32) encoding or Script.",  # Invalid human-readable part
        [],
    ),
    (
        "ric1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqc3hc2h",
        "Version 1+ witness address must use Bech32m checksum",  # Invalid checksum (Bech32 instead of Bech32m)
        [],
    ),
    (
        "tb1z0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqglt7rf",
        "Invalid or unsupported Segwit (Bech32) encoding or Script.",  # tb1, Invalid checksum (Bech32 instead of Bech32m)
        [],
    ),
    (
        "RIC1S0XLXVLHEMJA6C4DQV22UAPCTQUPFHLXM9H8Z3K2E72Q4K9HCZ7VQMWALZ9",
        "Version 1+ witness address must use Bech32m checksum",  # Invalid checksum (Bech32 instead of Bech32m)
        [],
    ),
    (
        "ric1p38j9r5y49hruaue7wxjce0updqjuyyx0kh56v8s25huc6995vvpqynocl5",
        "Invalid Base 32 character",  # Invalid character in checksum
        [59],
    ),
    (
        "RIC130XLXVLHEMJA6C4DQV22UAPCTQUPFHLXM9H8Z3K2E72Q4K9HCZ7VQ3EAK6E",
        "Invalid Bech32 address witness version",
        [],
    ),
    ("ric1pw5frv2la", "Invalid Bech32 address program size (1 byte)", []),
    (
        "ric1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7v8n0nx0muaewav25n42h6x",
        "Invalid Bech32 address program size (41 bytes)",
        [],
    ),
    (
        "RIC1QR508D6QEJXTDG4Y5R3ZARVARYVVKV8TQ",
        "Invalid Bech32 v0 address program size (16 bytes), per BIP141",
        [],
    ),
    (
        "tb1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vq47Zagq",
        "Invalid or unsupported Segwit (Bech32) encoding or Script.",  # tb1, Mixed case
        [],
    ),
    (
        "ric1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7v07ql3pm5n",
        "Invalid padding in Bech32 data section",  # zero padding of more than 4 bits
        [],
    ),
    (
        "tb1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vpggkg4j",
        "Invalid or unsupported Segwit (Bech32) encoding or Script.",  # tb1, Non-zero padding in 8-to-5 conversion
        [],
    ),
    ("ric1tpvwza", "Empty Bech32 data section", []),
]
VALID_DATA = [
    # BIP 350
    (
        "RIC1QW508D6QEJXTDG4Y5R3ZARVARY0C5XW7KN4H7NK",
        "0014751e76e8199196d454941c45d1b3a323f1433bd6",
    ),
    # (
    #   "tb1qrp33g0q5c5txsp9arysrx4k6zdkfs4nce4xj0gdcccefvpysxf3q0sl5k7",
    #   "00201863143c14c5166804bd19203356da136c985678cd4d27a1b8c6329604903262",
    # ),
    (
        "ric1qrp33g0q5c5txsp9arysrx4k6zdkfs4nce4xj0gdcccefvpysxf3qzl235f",
        "00201863143c14c5166804bd19203356da136c985678cd4d27a1b8c6329604903262",
    ),
    (
        "ric1pw508d6qejxtdg4y5r3zarvary0c5xw7kw508d6qejxtdg4y5r3zarvary0c5xw7kvm2k0z",
        "5128751e76e8199196d454941c45d1b3a323f1433bd6751e76e8199196d454941c45d1b3a323f1433bd6",
    ),
    ("RIC1SW50QZUMJPG", "6002751e"),
    ("ric1zw508d6qejxtdg4y5r3zarvaryvptjcnn", "5210751e76e8199196d454941c45d1b3a323"),
    # (
    #   "tb1qqqqqp399et2xygdj5xreqhjjvcmzhxw4aywxecjdzew6hylgvsesrxh6hy",
    #   "0020000000c4a5cad46221b2a187905e5266362b99d5e91c6ce24d165dab93e86433",
    # ),
    (
        "ric1qqqqqp399et2xygdj5xreqhjjvcmzhxw4aywxecjdzew6hylgvseswfzl4n",
        "0020000000c4a5cad46221b2a187905e5266362b99d5e91c6ce24d165dab93e86433",
    ),
    # (
    #   "tb1pqqqqp399et2xygdj5xreqhjjvcmzhxw4aywxecjdzew6hylgvsesf3hn0c",
    #   "5120000000c4a5cad46221b2a187905e5266362b99d5e91c6ce24d165dab93e86433",
    # ),
    (
        "ric1pqqqqp399et2xygdj5xreqhjjvcmzhxw4aywxecjdzew6hylgvses3zj6gd",
        "5120000000c4a5cad46221b2a187905e5266362b99d5e91c6ce24d165dab93e86433",
    ),
    (
        "ric1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqdd8504",
        "512079be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798",
    ),
    # PayToAnchor(P2A)
    (
        "ric1pfees6jykan",
        "51024e73",
    ),
]


class ValidateAddressMainTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.chain = ""  # main
        self.num_nodes = 1
        self.extra_args = [["-prune=899"]] * self.num_nodes

    def check_valid(self, addr, spk):
        info = self.nodes[0].validateaddress(addr)
        assert_equal(info["isvalid"], True)
        assert_equal(info["scriptPubKey"], spk)
        assert "error" not in info
        assert "error_locations" not in info

    def check_invalid(self, addr, error_str, error_locations):
        res = self.nodes[0].validateaddress(addr)
        assert_equal(res["isvalid"], False)
        assert_equal(res["error"], error_str)
        assert_equal(res["error_locations"], error_locations)

    def test_validateaddress(self):
        for (addr, error, locs) in INVALID_DATA:
            self.check_invalid(addr, error, locs)
        for (addr, spk) in VALID_DATA:
            self.check_valid(addr, spk)

    def run_test(self):
        self.test_validateaddress()


if __name__ == "__main__":
    ValidateAddressMainTest(__file__).main()
