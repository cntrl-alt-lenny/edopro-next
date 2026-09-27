// SPDX-License-Identifier: AGPL-3.0-or-later

#include "search_results_model.h"

#include "card_catalog.h"
#include "card_entry.h"
#include "deck_controller.h"
#include "edopro_next/data/card_code.h"
#include "edopro_next/data/search_query.h"
#include "edopro_next/policy/deck_search_filter.h"

#include <algorithm>

using edopro_next::policy::LimitationFilter;
using edopro_next::ui::CardTypeChoice;
using edopro_next::ui::SearchFilterState;

namespace {

// A short, presentation-only line for the second row of a search result -
// not a rules statement. Deliberately says nothing for a non-monster: this
// project's `data/` layer treats `type` as opaque, and a spell/trap's
// summary line has no numeric stat worth surfacing here.
//
// Reads attackDisplay/defenseDisplay, never attack/defense directly -
// external review, third pass: a negative stored value is a real "varies"
// sentinel (CardRecord's own doc comment), and upstream's own card-info
// panel (gframe/game.cpp) renders it as "?", never the negative number
// itself. attackDisplay/defenseDisplay already apply that rule once
// (card_entry.cpp), so CardPreview.qml and this summary line can never
// disagree with each other about it.
QString build_summary(const CardEntry& entry) {
    if (!entry.known)
        return QStringLiteral("Unknown card");
    if (!entry.isMonster)
        return QString();
    if (entry.isLink)
        return QStringLiteral("ATK %1").arg(entry.attackDisplay);
    return QStringLiteral("ATK %1 / DEF %2").arg(entry.attackDisplay, entry.defenseDisplay);
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

namespace {

// A sane cap for a live-typing search box - ranking (highest priority
// first, docs/architecture/card-search.md#ranking) means truncating never
// hides an exact/prefix match behind a flood of weaker ones. Applied after
// policy::deck_search_admits(), not through SearchQuery::limit, so a card
// the banlist-dependent part drops never takes a place in the 200.
constexpr std::size_t kResultCap = 200;

} // namespace

void SearchResultsModel::refresh() {
    beginResetModel();
    results_.clear();
    if (catalog_) {
        const auto query = edopro_next::ui::buildSearchQuery(filters_, queryText_);
        const auto* list = selectedList();
        const auto choices = limitationChoices();
        const auto limit = choices[static_cast<std::size_t>(limitation())];
        const auto& database = catalog_->database();
        for (const auto& result : catalog_->searchIndex().search(query)) {
            const auto* record = database.find(result.code);
            if (!record ||
                !edopro_next::policy::deck_search_admits(*record, list, filters_.showNonOfficial, limit))
                continue;
            results_.push_back(result);
            if (results_.size() >= kResultCap)
                break;
        }
    }
    endResetModel();
    emit resultsChanged();
}

void SearchResultsModel::setDeckController(DeckController* controller) {
    if (deckController_ == controller)
        return;
    if (deckController_)
        disconnect(deckController_, nullptr, this, nullptr);
    deckController_ = controller;
    if (deckController_) {
        connect(deckController_, &DeckController::selectedBanlistChanged, this,
                &SearchResultsModel::onBanlistChanged);
        connect(deckController_, &DeckController::banlistsChanged, this, &SearchResultsModel::onBanlistChanged);
    }
    emit deckControllerChanged();
    onBanlistChanged();
}

// upstream, deck_con.cpp:512-516: a new list reloads the limit choices
// and re-runs the search.
void SearchResultsModel::onBanlistChanged() {
    // The stored choice is a LimitationFilter, so it survives the list of
    // choices changing; limitation() maps it back to an index, or 0.
    emit filtersChanged();
    refresh();
}

const edopro_next::policy::LfList* SearchResultsModel::selectedList() const {
    if (!deckController_)
        return nullptr;
    const auto& list = deckController_->banlistStore().listAt(deckController_->selectedBanlistIndex());
    return list ? &*list : nullptr;
}

std::vector<LimitationFilter> SearchResultsModel::limitationChoices() const {
    return edopro_next::policy::limitation_filter_choices(selectedList(), filters_.showNonOfficial);
}

void SearchResultsModel::setFilters(const SearchFilterState& next) {
    if (filters_ == next)
        return;
    filters_ = next;
    emit filtersChanged();
    refresh();
}

QStringList SearchResultsModel::cardTypeNames() const { return edopro_next::ui::cardTypeLabels(); }

void SearchResultsModel::setCardType(int index) {
    const auto labels = cardTypeNames();
    if (index < 0 || index >= labels.size() || index == cardType())
        return;
    auto next = filters_;
    next.cardType = static_cast<CardTypeChoice>(index);
    next.subType = 0;
    next.attribute = 0;
    next.race = 0;
    next.attackText.clear();
    next.defenseText.clear();
    next.levelText.clear();
    next.scaleText.clear();
    setFilters(next);
}

QStringList SearchResultsModel::subTypeNames() const {
    QStringList out;
    for (const auto& choice : edopro_next::ui::subTypeChoices(filters_.cardType))
        out.push_back(choice.label);
    return out;
}

bool SearchResultsModel::subTypeEnabled() const {
    // game.cpp:3367-3371: disabled for All and Skill.
    return filters_.cardType != CardTypeChoice::All && filters_.cardType != CardTypeChoice::Skill;
}

void SearchResultsModel::setSubType(int index) {
    const auto size = static_cast<int>(edopro_next::ui::subTypeChoices(filters_.cardType).size());
    if (index < 0 || index >= size)
        return;
    auto next = filters_;
    next.subType = index;
    if (!edopro_next::ui::defenseFilterApplies(next))
        next.defenseText.clear();
    setFilters(next);
}

bool SearchResultsModel::monsterFiltersEnabled() const { return edopro_next::ui::monsterFiltersApply(filters_); }
bool SearchResultsModel::defenseEnabled() const { return edopro_next::ui::defenseFilterApplies(filters_); }

namespace {
QStringList labelsOf(const std::vector<edopro_next::ui::LabelledValue>& choices) {
    QStringList out;
    for (const auto& choice : choices)
        out.push_back(choice.label);
    return out;
}
} // namespace

QStringList SearchResultsModel::attributeNames() const { return labelsOf(edopro_next::ui::attributeChoices()); }

void SearchResultsModel::setAttribute(int index) {
    if (index < 0 || index >= static_cast<int>(edopro_next::ui::attributeChoices().size()))
        return;
    auto next = filters_;
    next.attribute = index;
    setFilters(next);
}

QStringList SearchResultsModel::raceNames() const { return labelsOf(edopro_next::ui::raceChoices()); }

void SearchResultsModel::setRace(int index) {
    if (index < 0 || index >= static_cast<int>(edopro_next::ui::raceChoices().size()))
        return;
    auto next = filters_;
    next.race = index;
    setFilters(next);
}

void SearchResultsModel::setAttackText(const QString& text) {
    auto next = filters_;
    next.attackText = text;
    setFilters(next);
}

void SearchResultsModel::setDefenseText(const QString& text) {
    auto next = filters_;
    next.defenseText = edopro_next::ui::defenseFilterApplies(filters_) ? text : QString();
    setFilters(next);
}

void SearchResultsModel::setLevelText(const QString& text) {
    auto next = filters_;
    next.levelText = text;
    setFilters(next);
}

void SearchResultsModel::setScaleText(const QString& text) {
    auto next = filters_;
    next.scaleText = text;
    setFilters(next);
}

QStringList SearchResultsModel::categoryNames() const { return edopro_next::ui::categoryLabels(); }

int SearchResultsModel::selectedCategoryCount() const {
    int count = 0;
    for (std::uint32_t mask = filters_.categoryMask; mask; mask &= mask - 1)
        ++count;
    return count;
}

bool SearchResultsModel::categorySelected(int index) const {
    if (index < 0 || index >= 32)
        return false;
    return (filters_.categoryMask >> index) & 1u;
}

void SearchResultsModel::setCategorySelected(int index, bool selected) {
    if (index < 0 || index >= 32)
        return;
    auto next = filters_;
    const std::uint32_t bit = std::uint32_t{1} << index;
    next.categoryMask = selected ? (next.categoryMask | bit) : (next.categoryMask & ~bit);
    setFilters(next);
}

QStringList SearchResultsModel::linkMarkerGlyphs() const {
    QStringList out;
    for (const auto& choice : edopro_next::ui::linkMarkerChoices())
        out.push_back(choice.glyph);
    return out;
}

bool SearchResultsModel::linkMarkerSelected(int index) const {
    const auto& choices = edopro_next::ui::linkMarkerChoices();
    if (index < 0 || index >= static_cast<int>(choices.size()))
        return false;
    return (filters_.linkMarkerMask & choices[static_cast<std::size_t>(index)].bit) != 0;
}

void SearchResultsModel::setLinkMarkerSelected(int index, bool selected) {
    const auto& choices = edopro_next::ui::linkMarkerChoices();
    if (index < 0 || index >= static_cast<int>(choices.size()))
        return;
    auto next = filters_;
    const auto bit = choices[static_cast<std::size_t>(index)].bit;
    next.linkMarkerMask = selected ? (next.linkMarkerMask | bit) : (next.linkMarkerMask & ~bit);
    setFilters(next);
}

QStringList SearchResultsModel::limitationNames() const {
    const auto* list = selectedList();
    const bool whitelist = list && list->whitelist;
    QStringList out;
    for (auto choice : limitationChoices())
        out.push_back(edopro_next::ui::limitationLabel(choice, whitelist));
    return out;
}

int SearchResultsModel::limitation() const {
    const auto choices = limitationChoices();
    const auto it = std::find(choices.begin(), choices.end(), filters_.limitation);
    return it == choices.end() ? 0 : static_cast<int>(it - choices.begin());
}

void SearchResultsModel::setLimitation(int index) {
    const auto choices = limitationChoices();
    if (index < 0 || index >= static_cast<int>(choices.size()))
        return;
    auto next = filters_;
    next.limitation = choices[static_cast<std::size_t>(index)];
    setFilters(next);
}

void SearchResultsModel::setShowNonOfficial(bool show) {
    auto next = filters_;
    next.showNonOfficial = show;
    setFilters(next);
}

bool SearchResultsModel::nonOfficialSwitchEnabled() const {
    return edopro_next::policy::non_official_switch_available(selectedList());
}

bool SearchResultsModel::filtersActive() const {
    SearchFilterState defaults;
    defaults.showNonOfficial = filters_.showNonOfficial;
    auto current = filters_;
    // A limit choice that is no longer offered is not in effect.
    current.limitation = limitationChoices()[static_cast<std::size_t>(limitation())];
    return !(current == defaults);
}

void SearchResultsModel::clearFilters() {
    // deck_con.cpp:1363-1397 (ClearSearch, ClearFilter): card type, sub-type,
    // attribute, race, limit, the four boxes, categories and markers.
    SearchFilterState next;
    next.showNonOfficial = filters_.showNonOfficial;
    setFilters(next);
}
