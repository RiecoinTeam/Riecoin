// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bitcoin-build-config.h> // IWYU pragma: keep

#include <node/warnings.h>

#include <common/system.h>
#include <node/interface_ui.h>
#include <sync.h>
#include <univalue.h>
#include <util/translation.h>

#include <utility>
#include <vector>

namespace node {
Warnings::Warnings()
{
    // Pre-release build warning
    if (!CLIENT_VERSION_IS_RELEASE) {
        m_warnings.insert(
            {Warning::PRE_RELEASE_TEST_BUILD,
             _("This is a development version, be sure to regularly follow the latest Riecoin news to be aware of possible bugs and updates!")});
    }
}
bool Warnings::Set(warning_type id, bilingual_str message)
{
    const auto& [_, inserted]{WITH_LOCK(m_mutex, return m_warnings.insert({id, std::move(message)}))};
    if (inserted) uiInterface.NotifyAlertChanged();
    return inserted;
}

bool Warnings::Unset(warning_type id)
{
    auto success{WITH_LOCK(m_mutex, return m_warnings.erase(id))};
    if (success) uiInterface.NotifyAlertChanged();
    return success;
}

std::vector<bilingual_str> Warnings::GetMessages() const
{
    LOCK(m_mutex);
    std::vector<bilingual_str> messages;
    messages.reserve(m_warnings.size());
    for (const auto& [id, msg] : m_warnings) {
        messages.push_back(msg);
    }
    return messages;
}

UniValue GetWarningsForRpc(const Warnings& warnings)
{
    UniValue messages{UniValue::VARR};
    for (auto&& message : warnings.GetMessages()) {
        messages.push_back(std::move(message.original));
    }
    return messages;
}
} // namespace node
