// SPDX-License-Identifier: AGPL-3.0-or-later

#include "deck_section_model.h"

#include "card_catalog.h"
#include "card_entry.h"

DeckSectionModel::DeckSectionModel(QObject* parent) : QAbstractListModel(parent) {}

int DeckSectionModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid() || !section_)
        return 0;
    return static_cast<int>(section_->size());
}

QVariant DeckSectionModel::data(const QModelIndex& index, int role) const {
    if (!section_ || index.row() < 0 ||
        static_cast<std::size_t>(index.row()) >= section_->size())
        return {};
    const auto code = (*section_)[static_cast<std::size_t>(index.row())];
    if (role == CardCodeRole)
        return QVariant::fromValue(edopro_next::data::to_number(code));

    const CardEntry entry = catalog_ ? catalog_->cardDetails(edopro_next::data::to_number(code))
                                      : CardEntry{};
    switch (role) {
    case NameRole:
        return entry.known ? entry.name : QStringLiteral("Unknown card");
    case KnownRole:
        return entry.known;
    default:
        return {};
    }
}

QHash<int, QByteArray> DeckSectionModel::roleNames() const {
    return {
        {CardCodeRole, "cardCode"},
        {NameRole, "name"},
        {KnownRole, "known"},
    };
}

void DeckSectionModel::bind(const std::vector<edopro_next::data::CardCode>* section,
                             const CardCatalog* catalog) {
    beginResetModel();
    section_ = section;
    catalog_ = catalog;
    endResetModel();
}

void DeckSectionModel::notifyAboutToInsert(int index) { beginInsertRows(QModelIndex(), index, index); }
void DeckSectionModel::notifyInserted() { endInsertRows(); }

void DeckSectionModel::notifyAboutToRemove(int index) { beginRemoveRows(QModelIndex(), index, index); }
void DeckSectionModel::notifyRemoved() { endRemoveRows(); }

void DeckSectionModel::notifyAboutToReset() { beginResetModel(); }
void DeckSectionModel::notifyReset() { endResetModel(); }
