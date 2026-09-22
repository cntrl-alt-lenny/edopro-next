// SPDX-License-Identifier: AGPL-3.0-or-later

#include "banlist_store.h"

namespace edopro_next::ui {

BanlistStore::BanlistStore() {
    clear();
}

void BanlistStore::clear() {
    entries_.clear();
    // Entry 0 is always "No banlist" mapping to std::nullopt.
    entries_.push_back(Entry{QStringLiteral("No banlist"), std::nullopt});
}

void BanlistStore::addList(policy::LfList list) {
    QString name = QString::fromUtf8(list.name.data(), static_cast<qsizetype>(list.name.size()));
    entries_.push_back(Entry{std::move(name), std::move(list)});
}

bool BanlistStore::loadFromFile(const std::filesystem::path& path) {
    auto result = policy::load_lflist(path);
    if (!result.ok) {
        return false;
    }
    for (auto& list : result.lists) {
        addList(std::move(list));
    }
    return true;
}

void BanlistStore::loadFromText(std::string_view text) {
    auto result = policy::parse_lflist(text);
    for (auto& list : result.lists) {
        addList(std::move(list));
    }
}

QStringList BanlistStore::names() const {
    QStringList result;
    result.reserve(static_cast<qsizetype>(entries_.size()));
    for (const auto& entry : entries_) {
        result.append(entry.name);
    }
    return result;
}

const std::optional<policy::LfList>& BanlistStore::listAt(int index) const {
    if (index <= 0 || static_cast<std::size_t>(index) >= entries_.size()) {
        static const std::optional<policy::LfList> kNullList = std::nullopt;
        return kNullList;
    }
    return entries_[static_cast<std::size_t>(index)].list;
}

} // namespace edopro_next::ui
