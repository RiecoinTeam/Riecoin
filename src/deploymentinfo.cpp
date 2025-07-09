// Copyright (c) 2016-2021 The Bitcoin Core developers
// Copyright (c) 2013-present The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <deploymentinfo.h>

#include <consensus/params.h>

#include <string_view>

const std::array<VBDeploymentInfo,Consensus::MAX_VERSION_BITS_DEPLOYMENTS> VersionBitsDeploymentInfo{
    VBDeploymentInfo{
        .name = "testdummy",
        .gbt_optional_rule = true,
    },
};
