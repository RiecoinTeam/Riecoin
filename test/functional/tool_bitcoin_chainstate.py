#!/usr/bin/env python3
# Copyright (c) 2022-present The Bitcoin Core developers
# Copyright (c) 2025-present The Riecoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import subprocess

from test_framework.test_framework import BitcoinTestFramework

class BitcoinChainstateTest(BitcoinTestFramework):
    def skip_test_if_missing_module(self):
        self.skip_if_no_bitcoin_chainstate()

    def set_test_params(self):
        self.setup_clean_chain = True
        self.chain = ""
        self.num_nodes = 1
        # Set prune to avoid disk space warning.
        self.extra_args = [["-prune=550"]]

    def add_block(self, datadir, input, expected_stderr):
        proc = subprocess.Popen(
            self.get_binaries().chainstate_argv() + [datadir],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        stdout, stderr = proc.communicate(input=input + "\n", timeout=5)
        self.log.debug("STDOUT: {0}".format(stdout.strip("\n")))
        self.log.info("STDERR: {0}".format(stderr.strip("\n")))

        if expected_stderr not in stderr:
            raise AssertionError(f"Expected stderr output {expected_stderr} does not partially match stderr:\n{stderr}")

    def run_test(self):
        node = self.nodes[0]
        datadir = node.cli.datadir
        node.stop_node()

        self.log.info(f"Testing bitcoin-chainstate {self.get_binaries().chainstate_argv()} with datadir: {datadir}")
        block_one = "0200000040572741cb3bd36866d441040f6d41a2261d8d42ef78bc9f89f96e67d018eae1539dc9674036fac25f154a90c168f2d7c2ffcc41447718accf0952f5ae497661ca8cfa52000000000030010221cf5828000000000000000000000000000000000000000000000000000000000101000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0b5102270f062f503253482fffffffff01000000000000000023210319c9841fe6e507df5cf7fa14f1d1af43605424d71bae2effb6ab300f50a9cdc4ac00000000"
        self.add_block(datadir, block_one, "Block has not yet been rejected")
        self.add_block(datadir, block_one, "duplicate")
        self.add_block(datadir, "00", "Block decode failed")
        self.add_block(datadir, "", "Empty line found")

if __name__ == "__main__":
    BitcoinChainstateTest(__file__).main()
