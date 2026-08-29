// SPDX-License-Identifier: AGPL-3.0-or-later
//
// A read-only QML-facing view over one section (main/extra/side) of a
// DeckController's single canonical edopro_next::data::Deck. This model
// owns no card data of its own - it holds a pointer straight at the
// section vector living inside DeckController, so there is exactly one
// copy of "what is in this deck" anywhere in the process. DeckController
// is the only thing that ever mutates that vector; it brackets each
// mutation between this model's notifyAboutToInsert()/notifyInserted(),
// notifyAboutToRemove()/notifyRemoved(), or notifyAboutToReset()/
// notifyReset() pair - the begin-side call before the vector actually
// changes, the end-side call immediately after - matching
// QAbstractItemModel's own contract (a single combined call made *after*
// mutating was a real bug external review found: views would observe the
// about-to-change signal when rowCount() already reflected the new state).
// This model's row count is never allowed to disagree with the vector it
// reflects.
//
// NameRole/KnownRole are resolved from CardCatalog at display time, not
// stored - so this model also listens for CardCatalog::loadedChanged and
// re-announces every row's display roles when the bound catalog reloads;
// see bind()/refreshDisplayRoles() in deck_section_model.cpp.

#pragma once

#include <QAbstractListModel>
#include <QMetaObject>
#include <qqmlintegration.h>
#include <vector>

#include "edopro_next/data/card_code.h"

class CardCatalog;

class DeckSectionModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Exposed by DeckController; never constructed from QML")

public:
    enum Role {
        CardCodeRole = Qt::UserRole + 1,
        NameRole,
        KnownRole,
    };
    Q_ENUM(Role)

    explicit DeckSectionModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // C++-only wiring, called only by DeckController - not part of the
    // QML-facing surface (neither parameter type is a Qt/QML type).
    //
    // Each operation is a begin/end pair, matching QAbstractItemModel's own
    // contract: begin*() must be called *before* the backing vector is
    // mutated, end*()/notify*() immediately after - not both together once
    // the mutation has already happened. DeckController is expected to
    // bracket its own vector mutation between the two calls of each pair.
    void bind(const std::vector<edopro_next::data::CardCode>* section,
              const CardCatalog* catalog);
    void notifyAboutToInsert(int index);
    void notifyInserted();
    void notifyAboutToRemove(int index);
    void notifyRemoved();
    void notifyAboutToReset();
    void notifyReset();

private:
    void refreshDisplayRoles();

    const std::vector<edopro_next::data::CardCode>* section_ = nullptr;
    const CardCatalog* catalog_ = nullptr;
    QMetaObject::Connection catalogConnection_;
};
