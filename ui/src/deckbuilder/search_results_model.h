// SPDX-License-Identifier: AGPL-3.0-or-later
//
// A QML-facing list of CardSearchIndex results for the current query text
// and search filters. Owns no card data itself - every row is resolved from
// CardCatalog at display time, so a catalog reload is reflected
// automatically (see setCatalog()).
//
// The filters (round 022, ADR 0012) are upstream's deck-builder filter
// window: card type and sub-type, attribute, race, ATK/DEF/Level/Scale text,
// effect categories, link markers, the limit list and the "show
// non-official cards" switch. This class holds the choices and offers the
// labels; search_filters.h turns them into a data::SearchQuery, and
// policy::deck_search_admits() applies the banlist-dependent part, against
// the banlist `deckController` has selected. QML sets indexes and text and
// renders labels, nothing more.

#pragma once

#include <QAbstractListModel>
#include <QString>
#include <qqmlintegration.h>
#include <vector>

#include "edopro_next/data/search_result.h"
#include "search_filters.h"

class CardCatalog;
class DeckController;

class SearchResultsModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(CardCatalog* catalog READ catalog WRITE setCatalog NOTIFY catalogChanged)
    Q_PROPERTY(QString queryText READ queryText WRITE setQueryText NOTIFY queryTextChanged)
    Q_PROPERTY(int resultCount READ resultCount NOTIFY resultsChanged)
    // The banlist the limit list counts against, and whose whitelist decides
    // what the search shows: deckController's selected one.
    Q_PROPERTY(DeckController* deckController READ deckController WRITE setDeckController NOTIFY deckControllerChanged)

    // ---- Filters. Every setter re-runs the search. ----
    Q_PROPERTY(QStringList cardTypeNames READ cardTypeNames CONSTANT)
    Q_PROPERTY(int cardType READ cardType WRITE setCardType NOTIFY filtersChanged)
    Q_PROPERTY(QStringList subTypeNames READ subTypeNames NOTIFY filtersChanged)
    Q_PROPERTY(bool subTypeEnabled READ subTypeEnabled NOTIFY filtersChanged)
    Q_PROPERTY(int subType READ subType WRITE setSubType NOTIFY filtersChanged)
    Q_PROPERTY(bool monsterFiltersEnabled READ monsterFiltersEnabled NOTIFY filtersChanged)
    Q_PROPERTY(bool defenseEnabled READ defenseEnabled NOTIFY filtersChanged)
    Q_PROPERTY(QStringList attributeNames READ attributeNames CONSTANT)
    Q_PROPERTY(int attribute READ attribute WRITE setAttribute NOTIFY filtersChanged)
    Q_PROPERTY(QStringList raceNames READ raceNames CONSTANT)
    Q_PROPERTY(int race READ race WRITE setRace NOTIFY filtersChanged)
    Q_PROPERTY(QString attackText READ attackText WRITE setAttackText NOTIFY filtersChanged)
    Q_PROPERTY(QString defenseText READ defenseText WRITE setDefenseText NOTIFY filtersChanged)
    Q_PROPERTY(QString levelText READ levelText WRITE setLevelText NOTIFY filtersChanged)
    Q_PROPERTY(QString scaleText READ scaleText WRITE setScaleText NOTIFY filtersChanged)
    Q_PROPERTY(QStringList categoryNames READ categoryNames CONSTANT)
    Q_PROPERTY(int selectedCategoryCount READ selectedCategoryCount NOTIFY filtersChanged)
    Q_PROPERTY(QStringList linkMarkerGlyphs READ linkMarkerGlyphs CONSTANT)
    Q_PROPERTY(QStringList limitationNames READ limitationNames NOTIFY filtersChanged)
    Q_PROPERTY(int limitation READ limitation WRITE setLimitation NOTIFY filtersChanged)
    Q_PROPERTY(bool showNonOfficial READ showNonOfficial WRITE setShowNonOfficial NOTIFY filtersChanged)
    Q_PROPERTY(bool nonOfficialSwitchEnabled READ nonOfficialSwitchEnabled NOTIFY filtersChanged)
    Q_PROPERTY(bool filtersActive READ filtersActive NOTIFY filtersChanged)

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

    DeckController* deckController() const { return deckController_; }
    void setDeckController(DeckController* controller);

    QStringList cardTypeNames() const;
    int cardType() const { return static_cast<int>(filters_.cardType); }
    // Also resets the sub-type, attribute, race and the four number boxes,
    // as upstream does when the card type changes (deck_con.cpp:525-532).
    void setCardType(int index);
    QStringList subTypeNames() const;
    bool subTypeEnabled() const;
    int subType() const { return filters_.subType; }
    // Choosing the Monster sub-type Link also clears the DEF box
    // (deck_con.cpp:577-583).
    void setSubType(int index);
    bool monsterFiltersEnabled() const;
    bool defenseEnabled() const;
    QStringList attributeNames() const;
    int attribute() const { return filters_.attribute; }
    void setAttribute(int index);
    QStringList raceNames() const;
    int race() const { return filters_.race; }
    void setRace(int index);
    QString attackText() const { return filters_.attackText; }
    void setAttackText(const QString& text);
    QString defenseText() const { return filters_.defenseText; }
    void setDefenseText(const QString& text);
    QString levelText() const { return filters_.levelText; }
    void setLevelText(const QString& text);
    QString scaleText() const { return filters_.scaleText; }
    void setScaleText(const QString& text);
    QStringList categoryNames() const;
    int selectedCategoryCount() const;
    Q_INVOKABLE bool categorySelected(int index) const;
    Q_INVOKABLE void setCategorySelected(int index, bool selected);
    QStringList linkMarkerGlyphs() const;
    Q_INVOKABLE bool linkMarkerSelected(int index) const;
    Q_INVOKABLE void setLinkMarkerSelected(int index, bool selected);
    QStringList limitationNames() const;
    // An index into limitationNames(), which changes with the banlist and
    // the non-official switch; a choice that is no longer offered falls back
    // to index 0.
    int limitation() const;
    void setLimitation(int index);
    bool showNonOfficial() const { return filters_.showNonOfficial; }
    void setShowNonOfficial(bool show);
    bool nonOfficialSwitchEnabled() const;
    // True when any filter other than the search text narrows the results.
    bool filtersActive() const;
    // Upstream's Clear button (deck_con.cpp:1363-1397): every filter back to
    // its default, except the non-official switch, which ClearFilter leaves
    // alone. The search text is QML's own field and is cleared there.
    Q_INVOKABLE void clearFilters();

    // C++-only, for tests.
    const edopro_next::ui::SearchFilterState& filterState() const { return filters_; }

signals:
    void catalogChanged();
    void queryTextChanged();
    void resultsChanged();
    void deckControllerChanged();
    void filtersChanged();

private:
    void refresh();
    void setFilters(const edopro_next::ui::SearchFilterState& next);
    const edopro_next::policy::LfList* selectedList() const;
    std::vector<edopro_next::policy::LimitationFilter> limitationChoices() const;
    void onBanlistChanged();

    CardCatalog* catalog_ = nullptr;
    DeckController* deckController_ = nullptr;
    QString queryText_;
    edopro_next::ui::SearchFilterState filters_;
    std::vector<edopro_next::data::SearchResult> results_;
};
