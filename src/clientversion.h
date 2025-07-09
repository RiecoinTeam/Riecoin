// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2013-present The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CLIENTVERSION_H
#define BITCOIN_CLIENTVERSION_H

#include <util/macros.h>

#include <riecoin-build-config.h> // IWYU pragma: keep

// Check that required client information is defined
#if !defined(CLIENT_VERSION_MONTH) || !defined(CLIENT_VERSION_REVISION) || !defined(CLIENT_VERSION_IS_RELEASE) || !defined(COPYRIGHT_YEAR)
#error Client version information missing: version is not defined by riecoin-build-config.h or in any other way
#endif

//! Copyright string used in Windows .rc files
#define COPYRIGHT_STR "2009-" STRINGIZE(COPYRIGHT_YEAR) " " COPYRIGHT_HOLDERS_FINAL

// Windows .rc files include this header, but they cannot cope with real C++ code.
#if !defined(RC_INVOKED)

#include <string>
#include <vector>

static const int CLIENT_VERSION =
                               100 * CLIENT_VERSION_MONTH
                         +       1 * CLIENT_VERSION_REVISION;

extern const std::string UA_NAME;


std::string FormatFullVersion();
std::string FormatSubVersion(const std::string& name, int nClientVersion, const std::vector<std::string>& comments);

std::string CopyrightHolders(const std::string& strPrefix);

/** Returns licensing information (for -version) */
std::string LicenseInfo();

#endif // RC_INVOKED

#endif // BITCOIN_CLIENTVERSION_H
