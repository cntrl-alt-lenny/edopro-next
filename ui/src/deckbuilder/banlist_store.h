// SPDX-License-Identifier: AGPL-3.0-or-later
//
// In-memory store for limitation/forbidden card lists (LFLists/banlists).
//
// Upstream's deck editor manages banlists through cbDBLFList
// (gframe/deck_con.cpp:74, game.cpp:2430-2449), loaded at startup.
//
// Key invariant:
// Entry 0 is ALWAYS "No banlist", mapping to std::nullopt. This is distinct
// from a concrete "N/A" banlist (which has an empty content map and
// whitelist=false):
// - std::nullopt skips banlist limitation checks, copy-count >3 checks, and
//   scope checks (CheckDeckContent returns early in upstream
//   gframe/deck_manager.cpp:217-218).
// - Concrete "N/A" still runs CheckCards, enforcing <=3 copies and card scope
//   (docs/architecture/deck-legality.md §5).

#pragma once

#include <QString>
#include <QStringList>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include "edopro_next/policy/lf_list.h"

namespace edopro_next::ui {

class BanlistStore {
public:
    struct Entry {
        QString name;
        std::optional<policy::LfList> list;
    };

    BanlistStore();

    // Clears all loaded lists, preserving Entry 0 ("No banlist", std::nullopt).
    void clear();

    // Loads banlists from a file path using policy::load_lflist().
    // Returns true if the file was successfully read and parsed.
    bool loadFromFile(const std::filesystem::path& path);

    // Parses banlists from an in-memory string using policy::parse_lflist().
    void loadFromText(std::string_view text);

    // List of display names for UI selectors. Entry 0 is "No banlist".
    QStringList names() const;

    // Returns the LFList at index, or std::nullopt if index is 0 or out of bounds.
    const std::optional<policy::LfList>& listAt(int index) const;

    // Total number of entries (always >= 1).
    int count() const { return static_cast<int>(entries_.size()); }

    const std::vector<Entry>& entries() const { return entries_; }

private:
    void addList(policy::LfList list);

    std::vector<Entry> entries_;
};

} // namespace edopro_next::ui
