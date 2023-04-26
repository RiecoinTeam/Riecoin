// Copyright (c) 2011-2020 The Bitcoin Core developers
// Copyright (c) 2013-2023 The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <policy/fees.h>
#include <policy/policy.h>
#include <txmempool.h>
#include <uint256.h>
#include <util/time.h>

#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(policyestimator_tests, ChainTestingSetup)

BOOST_AUTO_TEST_CASE(BlockPolicyEstimates)
{
    return; // Bitcoin Developers might eventually rewrite this Test for EstimateRawFee rather than EstimateFee, so keep this file for now.
}

BOOST_AUTO_TEST_SUITE_END()
