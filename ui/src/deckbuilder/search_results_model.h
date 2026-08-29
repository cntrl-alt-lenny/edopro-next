// SPDX-License-Identifier: AGPL-3.0-or-later
//
// A QML-facing list of CardSearchIndex results for the current query text.
// Owns no card data itself - every row is resolved from CardCatalog at
// display time, so a catalog reload is reflected automatically (see
// setCatalog()).

#pragma once

#include <QAbstractListModel>
#include <QString>
#include <qqmlintegration.h>
#include <vector>

#include "edopro_next/data/search_result.h"

class CardCatalog;

class SearchResultsModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(CardCatalog* catalog READ catalog WRITE setCatalog NOTIFY catalogChanged)
    Q_PROPERTY(QString queryText READ queryText WRITE setQueryText NOTIFY queryTextChanged)
    Q_PROPERTY(int resultCount READ resultCount NOTIFY resultsChanged)

public:
    enum Role {
        CardCodeRole = Qt::UserRole + 1,
        NameRole,
        SummaryRole,
        MatchKindRole,
    };
    Q_ENUM(Role)

    explicit SearchResultsModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    CardCatalog* catalog() const;
    void setCatalog(CardCatalog* catalog);

    QString queryText() const;
    void setQueryText(const QString& text);

    int resultCount() const;

    // Convenience for QML's onClicked-style handlers, which otherwise have
    // no clean way to pull a role value for "the row that was activated"
    // out of a plain list view without also wiring a delegate model role.
    Q_INVOKABLE quint32 cardCodeAt(int row) const;

signals:
    void catalogChanged();
    void queryTextChanged();
    void resultsChanged();

private:
    void refresh();

    CardCatalog* catalog_ = nullptr;
    QString queryText_;
    std::vector<edopro_next::data::SearchResult> results_;
};
