// Copyright (c) 2012-present The Bitcoin Core developers
// Copyright (c) 2013-present The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bitcoin-build-config.h> // IWYU pragma: keep

#include <clientversion.h>
#include <util/string.h>
#include <util/translation.h>

#include <tinyformat.h>

#include <string>
#include <vector>

using util::Join;

/**
 * Name of client reported in the 'version' message. Report the same name
 * for both bitcoind and bitcoin-qt, to make it harder for attackers to
 * target servers or GUI users specifically.
 */
const std::string UA_NAME("Dev");


#include <bitcoin-build-info.h>
// The <bitcoin-build-info.h>, which is generated by the build environment (cmake/script/GenerateBuildInfo.cmake),
// could contain only one line of the following:
//   - "#define BUILD_GIT_TAG ...", if the top commit is tagged
//   - "#define BUILD_GIT_COMMIT ...", if the top commit is not tagged
//   - "// No build information available", if proper git information is not available

//! git will put "#define GIT_COMMIT_ID ..." on the next line inside archives. $Format:%n#define GIT_COMMIT_ID "%H"$

#ifdef BUILD_GIT_TAG
    #define BUILD_DESC BUILD_GIT_TAG
    #define BUILD_SUFFIX ""
#else
    #define BUILD_DESC CLIENT_VERSION_STRING
    #if CLIENT_VERSION_IS_RELEASE
        #define BUILD_SUFFIX ""
    #elif defined(BUILD_GIT_COMMIT)
        #define BUILD_SUFFIX "-" BUILD_GIT_COMMIT
    #elif defined(GIT_COMMIT_ID)
        #define BUILD_SUFFIX "-g" GIT_COMMIT_ID
    #else
        #define BUILD_SUFFIX "-unk"
    #endif
#endif

static std::string FormatVersion(int nVersion)
{
    return nVersion % 100 == 0 ? strprintf("%d", nVersion/100) : strprintf("%d.%d", nVersion/100, nVersion % 100);
}

std::string FormatFullVersion()
{
    static const std::string CLIENT_BUILD(BUILD_DESC BUILD_SUFFIX);
    return CLIENT_BUILD;
}

/**
 * Format the subversion field according to BIP 14 spec (https://github.com/bitcoin/bips/blob/master/bip-0014.mediawiki)
 */
std::string FormatSubVersion(const std::string& name, int nClientVersion, const std::vector<std::string>& comments)
{
    std::string comments_str;
    if (!comments.empty()) comments_str = strprintf("(%s)", Join(comments, "; "));
    return strprintf("/%s:%s%s/", name, FormatVersion(nClientVersion), comments_str);
}

std::string CopyrightHolders(const std::string& strPrefix)
{
    std::string strCopyrightHolders = strPrefix + " 2009-" + std::to_string(COPYRIGHT_YEAR) + " The Bitcoin Core developers";
    strCopyrightHolders += "\n" + strPrefix + " 2013-" + std::to_string(COPYRIGHT_YEAR) + " The Riecoin developers";
    return strCopyrightHolders;
}

std::string LicenseInfo()
{
    const std::string URL_SOURCE_CODE = "<https://github.com/RiecoinTeam/Riecoin>";

    return CopyrightHolders("(C) ") + "" +
           "\n Riecoin Core 2505 (Dev) is based on Bitcoin Core (Master).\n" +
           strprintf(_("Please contribute if you find %s useful. "
                       "Visit %s for further information about the software."),
                     CLIENT_NAME, "<" CLIENT_URL ">")
               .translated +
           "\n" +
           strprintf(_("The source code is currently available from %s."), URL_SOURCE_CODE).translated +
           "\n\n Whitepaper: <https://riecoin.xyz/Whitepaper>" +
           "\n Riecoin Forum: <https://riecoin.xyz/Forum>" +
           "\n Discord: <https://discord.gg/2sJEayC>" +
           "\n Mastodon: <https://steloj.xyz/@Riecoin>\n\n" +
           _("This is experimental software.") + "\n" +
           strprintf(_("Distributed under the MIT software license, see the accompanying file %s or %s"), "COPYING", "<https://opensource.org/licenses/MIT>").translated +
           "\n";
}
