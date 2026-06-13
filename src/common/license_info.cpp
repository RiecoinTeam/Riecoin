// Copyright (c) The Bitcoin Core developers
// Copyright (c) The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <riecoin-build-config.h> // IWYU pragma: keep

#include <common/license_info.h>

#include <tinyformat.h>
#include <util/translation.h>

#include <string>

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
           "\n Riecoin Core (Dev) is based on Bitcoin Core (Master).\n" +
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
           strprintf(_("Distributed under the GPLv3 license, see %s"), "<https://www.gnu.org/licenses/gpl-3.0.en.html>").translated +
           "\n";
}
