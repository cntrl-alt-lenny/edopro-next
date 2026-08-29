// SPDX-License-Identifier: AGPL-3.0-or-later

#include "card_catalog.h"

#include "qt_path.h"

CardCatalog::CardCatalog(QObject* parent) : QObject(parent) {}

bool CardCatalog::loadDatabases(const QStringList& paths) {
    QStringList errors;
    for (const QString& path : paths) {
        const auto result = database_.load_database(to_fs_path(path));
        if (!result.ok)
            errors << QStringLiteral("%1: %2").arg(path, QString::fromStdString(result.error));
    }
    // Rebuilt unconditionally, against whatever database_ actually holds
    // right now - including "nothing at all" if every path failed, or if
    // no path was ever supplied. There is no code path where searchIndex_
    // can describe a database state that never actually loaded.
    searchIndex_.rebuild(database_);
    lastError_ = errors.join(QStringLiteral("\n"));
    emit loadedChanged();
    return errors.isEmpty() && !paths.isEmpty();
}

CardEntry CardCatalog::cardDetails(quint32 code) const {
    return make_card_entry(static_cast<edopro_next::data::CardCode>(code), database_);
}

bool CardCatalog::loaded() const { return database_.size() > 0; }

int CardCatalog::cardCount() const { return static_cast<int>(database_.size()); }

QString CardCatalog::lastError() const { return lastError_; }
