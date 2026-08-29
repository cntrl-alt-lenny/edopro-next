// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Owns this process's one edopro_next::data::CardDatabase and its matching
// CardSearchIndex - the Qt-facing seam onto the reviewed, Qt-free M3A/M3C
// data APIs. See docs/architecture/deck-builder-ui.md for the full
// bootstrap/rebuild lifecycle this implements.

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <qqmlintegration.h>

#include "card_entry.h"
#include "edopro_next/data/card_database.h"
#include "edopro_next/data/card_search_index.h"

class CardCatalog : public QObject {
    Q_OBJECT
    QML_ELEMENT

    // True once at least one database file has loaded successfully. False
    // both before any load attempt and after every supplied path failed -
    // there is no "silently keep whatever was there before" state, because
    // there is nothing to fall back to: this module never bundles or
    // fabricates card data (CLAUDE.md; docs/architecture/deck-builder-ui.md
    // #bootstrap).
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(int cardCount READ cardCount NOTIFY loadedChanged)
    // Empty when the most recent loadDatabases() call had no failures at
    // all (including "loaded nothing because no paths were given", which
    // is reported through `loaded`/`cardCount`, not as an error).
    Q_PROPERTY(QString lastError READ lastError NOTIFY loadedChanged)

public:
    explicit CardCatalog(QObject* parent = nullptr);

    // Loads every path in order via CardDatabase::load_database(), matching
    // the same base-then-overlay, last-file-wins precedence upstream's own
    // DataHandler::LoadDatabases() uses for "./cards.cdb" then
    // "./expansions/*.cdb" (gframe/data_handler.cpp:28-38) - just supplied
    // explicitly here rather than discovered from a fixed relative
    // convention (see docs/architecture/deck-builder-ui.md#bootstrap for
    // why). A path that fails to load does not stop the remaining paths
    // from being attempted, matching upstream's own per-file resilience -
    // but every failure is collected into `lastError`, never silently
    // dropped. The search index is rebuilt unconditionally at the end
    // against whatever the database actually now contains, so it can never
    // reflect data that failed to load. Returns true iff at least one path
    // loaded successfully and none failed.
    Q_INVOKABLE bool loadDatabases(const QStringList& paths);

    // Never returns a CardRecord* - the value type this UI layer's QML
    // boundary is built on (card_entry.h).
    Q_INVOKABLE CardEntry cardDetails(quint32 code) const;

    bool loaded() const;
    int cardCount() const;
    QString lastError() const;

    // C++-only accessors - not exposed to QML's meta-object system, since
    // neither type is a Qt/QML type. Used by SearchResultsModel/
    // DeckSectionModel/DeckController, all of which are plain C++ callers.
    const edopro_next::data::CardDatabase& database() const { return database_; }
    const edopro_next::data::CardSearchIndex& searchIndex() const { return searchIndex_; }

signals:
    // Fired after every loadDatabases() call, success or failure -
    // observers (SearchResultsModel) re-run against the now-current index
    // unconditionally, rather than needing to guess whether anything
    // actually changed.
    void loadedChanged();

private:
    edopro_next::data::CardDatabase database_;
    edopro_next::data::CardSearchIndex searchIndex_;
    QString lastError_;
};
