// SPDX-License-Identifier: AGPL-3.0-or-later
//
// A read-only QML-facing view over one section (main/extra/side) of a
// DeckController's single canonical edopro_next::data::Deck. This model
// owns no card data of its own - it holds a pointer straight at the
// section vector living inside DeckController, so there is exactly one
// copy of "what is in this deck" anywhere in the process. DeckController
// is the only thing that ever mutates that vector, and it does so through
// notifyInserted()/notifyRemoved()/notifyReset() immediately afterward, so
// this model's row count is never allowed to disagree with the vector it
// reflects.

#pragma once

#include <QAbstractListModel>
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
    void bind(const std::vector<edopro_next::data::CardCode>* section,
              const CardCatalog* catalog);
    void notifyInserted(int index);
    void notifyRemoved(int index);
    void notifyReset();

private:
    const std::vector<edopro_next::data::CardCode>* section_ = nullptr;
    const CardCatalog* catalog_ = nullptr;
};
