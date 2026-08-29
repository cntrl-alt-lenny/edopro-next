// SPDX-License-Identifier: AGPL-3.0-or-later

#include "card_catalog.h"

#include "qt_path.h"

CardCatalog::CardCatalog(QObject* parent) : QObject(parent) {}

bool CardCatalog::loadDatabases(const QStringList& paths) {
    // Built into a fresh CardDatabase and only swapped into database_ once
    // every path has been attempted - not loaded into the existing member
    // in place. load_database()'s own per-code overlay ("last file wins")
    // is about combining multiple paths within *one* call, not about what
    // a second loadDatabases() call should do to state left over from a
    // previous one: loading in place would leave codes from a prior call
    // searchable forever, and an all-failed call would leave loaded()
    // reporting the previous call's data as if it were still current.
    edopro_next::data::CardDatabase database;
    QStringList errors;
    bool anySucceeded = false;
    for (const QString& path : paths) {
        const auto result = database.load_database(to_fs_path(path));
        if (result.ok)
            anySucceeded = true;
        else
            errors << QStringLiteral("%1: %2").arg(path, QString::fromStdString(result.error));
    }
    database_ = std::move(database);
    // Rebuilt unconditionally, against whatever database_ actually holds
    // right now - including "nothing at all" if every path failed, or if
    // no path was ever supplied. There is no code path where searchIndex_
    // can describe a database state that never actually loaded.
    searchIndex_.rebuild(database_);
    lastError_ = errors.join(QStringLiteral("\n"));
    // Tracked from each call's own `ok`, not database_.size() > 0 - a
    // schema-valid, legitimately empty .cdb still counts as a successful
    // load (see loaded()'s own doc comment).
    loaded_ = anySucceeded;
    emit loadedChanged();
    return errors.isEmpty() && !paths.isEmpty();
}

CardEntry CardCatalog::cardDetails(quint32 code) const {
    return make_card_entry(static_cast<edopro_next::data::CardCode>(code), database_);
}

bool CardCatalog::loaded() const { return loaded_; }

int CardCatalog::cardCount() const { return static_cast<int>(database_.size()); }

QString CardCatalog::lastError() const { return lastError_; }
