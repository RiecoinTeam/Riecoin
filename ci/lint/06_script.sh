#!/usr/bin/env bash
#
# Copyright (c) 2018-2021 The Bitcoin Core developers
# Copyright (c) 2013-2023 The Riecoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C

GIT_HEAD=$(git rev-parse HEAD)
if [ -n "$CIRRUS_PR" ]; then
  COMMIT_RANGE="${CIRRUS_BASE_SHA}..$GIT_HEAD"
  echo
  git log --no-merges --oneline "$COMMIT_RANGE"
  echo
  test/lint/commit-script-check.sh "$COMMIT_RANGE"
else
  COMMIT_RANGE="SKIP_EMPTY_NOT_A_PR"
fi
export COMMIT_RANGE

# This only checks that the trees are pure subtrees, it is not doing a full
# check with -r to not have to fetch all the remotes.
test/lint/git-subtree-check.sh src/crypto/ctaes
test/lint/git-subtree-check.sh src/secp256k1
test/lint/git-subtree-check.sh src/minisketch
test/lint/git-subtree-check.sh src/leveldb
test/lint/git-subtree-check.sh src/crc32c
test/lint/check-doc.py
test/lint/all-lint.py
