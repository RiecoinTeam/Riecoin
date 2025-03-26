// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Copyright (c) 2013-present The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <common/args.h>
#include <logging.h>
#include <util/fs.h>
#include <wallet/db.h>

#include <algorithm>
#include <exception>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace wallet {
bool operator<(BytePrefix a, std::span<const std::byte> b) { return std::ranges::lexicographical_compare(a.prefix, b.subspan(0, std::min(a.prefix.size(), b.size()))); }
bool operator<(std::span<const std::byte> a, BytePrefix b) { return std::ranges::lexicographical_compare(a.subspan(0, std::min(a.size(), b.prefix.size())), b.prefix); }

std::vector<std::pair<fs::path, std::string>> ListDatabases(const fs::path& wallet_dir)
{
    std::vector<std::pair<fs::path, std::string>> paths;
    std::error_code ec;

    for (auto it = fs::recursive_directory_iterator(wallet_dir, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
            if (fs::is_directory(*it)) {
                it.disable_recursion_pending();
                LogPrintf("%s: %s %s -- skipping.\n", __func__, ec.message(), fs::PathToString(it->path()));
            } else {
                LogPrintf("%s: %s %s\n", __func__, ec.message(), fs::PathToString(it->path()));
            }
            continue;
        }

        try {
            const fs::path path{it->path().lexically_relative(wallet_dir)};
            if (it->status().type() == fs::file_type::directory) {
                if (IsSQLiteFile(SQLiteDataFile(it->path()))) {
                    // Found a directory which contains wallet.dat sqlite file, add it as a wallet with SQLITE format.
                    paths.emplace_back(path, "sqlite");
                }
            }
        } catch (const std::exception& e) {
            LogPrintf("%s: Error scanning %s: %s\n", __func__, fs::PathToString(it->path()), e.what());
            it.disable_recursion_pending();
        }
    }

    return paths;
}

fs::path SQLiteDataFile(const fs::path& path)
{
    return path / "wallet.dat";
}

bool IsSQLiteFile(const fs::path& path)
{
    if (!fs::exists(path)) return false;

    // A SQLite Database file is at least 512 bytes.
    std::error_code ec;
    auto size = fs::file_size(path, ec);
    if (ec) LogPrintf("%s: %s %s\n", __func__, ec.message(), fs::PathToString(path));
    if (size < 512) return false;

    std::ifstream file{path, std::ios::binary};
    if (!file.is_open()) return false;

    // Magic is at beginning and is 16 bytes long
    char magic[16];
    file.read(magic, 16);

    // Application id is at offset 68 and 4 bytes long
    file.seekg(68, std::ios::beg);
    char app_id[4];
    file.read(app_id, 4);

    file.close();

    // Check the magic, see https://sqlite.org/fileformat.html
    std::string magic_str(magic, 16);
    if (magic_str != std::string{"SQLite format 3\000", 16}) {
        return false;
    }

    // Check the application id matches our network magic
    return memcmp(Params().MessageStart().data(), app_id, 4) == 0;
}

void ReadDatabaseArgs(const ArgsManager& args, DatabaseOptions& options)
{
    // Override current options with args values, if any were specified
    options.use_unsafe_sync = args.GetBoolArg("-unsafesqlitesync", options.use_unsafe_sync);
    options.use_shared_memory = !args.GetBoolArg("-privdb", !options.use_shared_memory);
    options.max_log_mb = args.GetIntArg("-dblogsize", options.max_log_mb);
}

} // namespace wallet
