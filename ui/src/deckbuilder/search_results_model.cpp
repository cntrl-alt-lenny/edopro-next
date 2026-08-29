// SPDX-License-Identifier: AGPL-3.0-or-later

#include "search_results_model.h"

#include "card_catalog.h"
#include "card_entry.h"
#include "edopro_next/data/card_code.h"
#include "edopro_next/data/search_query.h"

namespace {

// A short, presentation-only line for the second row of a search result -
// not a rules statement. Deliberately says nothing for a non-monster: this
// project's `data/` layer treats `type` as opaque, and a spell/trap's
// summary line has no numeric stat worth surfacing here.
QString build_summary(const CardEntry& entry) {
    if (!entry.known)
        return QStringLiteral("Unknown card");
    if (!entry.isMonster)
        return QString();
    if (entry.isLink)
        return QStringLiteral("ATK %1").arg(entry.attack);
    return QStringLiteral("ATK %1 / DEF %2").arg(entry.attack).arg(entry.defense);
}

} // namespace

SearchResultsModel::SearchResultsModel(QObject* parent) : QAbstractListModel(parent) {}

int SearchResultsModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return static_cast<int>(results_.size());
}

QVariant SearchResultsModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 ||
        static_cast<std::size_t>(index.row()) >= results_.size())
        return {};
    const auto& result = results_[static_cast<std::size_t>(index.row())];
    if (role == MatchKindRole)
        return static_cast<int>(result.match);
    if (role == CardCodeRole)
        return QVariant::fromValue(edopro_next::data::to_number(result.code));

    const CardEntry entry =
        catalog_ ? catalog_->cardDetails(edopro_next::data::to_number(result.code)) : CardEntry{};
    switch (role) {
    case NameRole:
        return entry.known ? entry.name : QStringLiteral("Unknown card");
    case SummaryRole:
        return build_summary(entry);
    default:
        return {};
    }
}

QHash<int, QByteArray> SearchResultsModel::roleNames() const {
    return {
        {CardCodeRole, "cardCode"},
        {NameRole, "name"},
        {SummaryRole, "summary"},
        {MatchKindRole, "matchKind"},
    };
}

CardCatalog* SearchResultsModel::catalog() const { return catalog_; }

void SearchResultsModel::setCatalog(CardCatalog* catalog) {
    if (catalog_ == catalog)
        return;
    if (catalog_)
        disconnect(catalog_, &CardCatalog::loadedChanged, this, &SearchResultsModel::refresh);
    catalog_ = catalog;
    if (catalog_)
        connect(catalog_, &CardCatalog::loadedChanged, this, &SearchResultsModel::refresh);
    emit catalogChanged();
    refresh();
}

QString SearchResultsModel::queryText() const { return queryText_; }

void SearchResultsModel::setQueryText(const QString& text) {
    if (queryText_ == text)
        return;
    queryText_ = text;
    emit queryTextChanged();
    refresh();
}

int SearchResultsModel::resultCount() const { return static_cast<int>(results_.size()); }

quint32 SearchResultsModel::cardCodeAt(int row) const {
    if (row < 0 || static_cast<std::size_t>(row) >= results_.size())
        return 0;
    return edopro_next::data::to_number(results_[static_cast<std::size_t>(row)].code);
}

void SearchResultsModel::refresh() {
    beginResetModel();
    results_.clear();
    if (catalog_) {
        edopro_next::data::SearchQuery query;
        query.text = queryText_.toStdString();
        // A sane cap for a live-typing search box - ranking (highest
        // priority first, docs/architecture/card-search.md#ranking) means
        // truncating here never hides an exact/prefix match behind a
        // flood of weaker ones.
        query.limit = 200;
        results_ = catalog_->searchIndex().search(query);
    }
    endResetModel();
    emit resultsChanged();
}
