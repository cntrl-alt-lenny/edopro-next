// SPDX-License-Identifier: AGPL-3.0-or-later

#include "deck_controller.h"

#include <QFileInfo>
#include <algorithm>

#include "card_catalog.h"
#include "edopro_next/data/ydk.h"
#include "qt_path.h"

DeckController::DeckController(QObject* parent)
    : QObject(parent),
      mainModel_(new DeckSectionModel(this)),
      extraModel_(new DeckSectionModel(this)),
      sideModel_(new DeckSectionModel(this)) {
    rebindModels();
    validateLegality();
}

CardCatalog* DeckController::catalog() const { return catalog_; }

void DeckController::setCatalog(CardCatalog* catalog) {
    if (catalog_ == catalog)
        return;
    catalog_ = catalog;
    rebindModels();
    validateLegality();
    emit catalogChanged();
}

void DeckController::rebindModels() {
    // The section vectors live inside `deck_`, a plain member - their
    // addresses never change for this object's lifetime, even when `deck_`
    // itself is reassigned wholesale (loadDeck()/newDeck()). Only the
    // catalog pointer genuinely needs re-binding when it changes; binding
    // the section pointers again here too is harmless and keeps this one
    // function the only place that wires the models up.
    mainModel_->bind(&deck_.main, catalog_);
    extraModel_->bind(&deck_.extra, catalog_);
    sideModel_->bind(&deck_.side, catalog_);
}

int DeckController::mainCount() const { return static_cast<int>(deck_.main.size()); }
int DeckController::extraCount() const { return static_cast<int>(deck_.extra.size()); }
int DeckController::sideCount() const { return static_cast<int>(deck_.side.size()); }

bool DeckController::dirty() const { return dirty_; }

QString DeckController::currentPath() const { return currentPath_; }

QString DeckController::currentFileName() const {
    if (currentPath_.isEmpty())
        return QString();
    return QFileInfo(currentPath_).fileName();
}

QString DeckController::lastError() const { return lastError_; }

QStringList DeckController::rulesetNames() const {
    const auto& rulesets = edopro_next::ui::availableRulesets();
    QStringList names;
    names.reserve(static_cast<qsizetype>(rulesets.size()));
    for (const auto& r : rulesets) {
        names.append(r.displayName);
    }
    return names;
}

void DeckController::setSelectedRulesetIndex(int index) {
    const auto& rulesets = edopro_next::ui::availableRulesets();
    if (rulesets.empty())
        return;
    const int clamped = std::clamp(index, 0, static_cast<int>(rulesets.size()) - 1);
    if (selectedRulesetIndex_ == clamped)
        return;
    selectedRulesetIndex_ = clamped;
    validateLegality();
    emit selectedRulesetChanged();
}

QStringList DeckController::banlistNames() const {
    return banlistStore_.names();
}

void DeckController::setSelectedBanlistIndex(int index) {
    const int count = banlistStore_.count();
    if (count <= 0)
        return;
    const int clamped = std::clamp(index, 0, count - 1);
    if (selectedBanlistIndex_ == clamped)
        return;
    selectedBanlistIndex_ = clamped;
    validateLegality();
    emit selectedBanlistChanged();
}

bool DeckController::loadBanlistFile(const QString& path) {
    const bool ok = banlistStore_.loadFromFile(to_fs_path(path));
    if (ok) {
        if (selectedBanlistIndex_ >= banlistStore_.count()) {
            selectedBanlistIndex_ = 0;
            emit selectedBanlistChanged();
        }
        emit banlistsChanged();
        validateLegality();
    }
    return ok;
}

void DeckController::loadBanlists(const QStringList& paths) {
    bool anyLoaded = false;
    for (const auto& path : paths) {
        if (banlistStore_.loadFromFile(to_fs_path(path))) {
            anyLoaded = true;
        }
    }
    if (anyLoaded) {
        if (selectedBanlistIndex_ >= banlistStore_.count()) {
            selectedBanlistIndex_ = 0;
            emit selectedBanlistChanged();
        }
        emit banlistsChanged();
        validateLegality();
    }
}

void DeckController::loadBanlistFromText(const std::string& text) {
    banlistStore_.loadFromText(text);
    if (selectedBanlistIndex_ >= banlistStore_.count()) {
        selectedBanlistIndex_ = 0;
        emit selectedBanlistChanged();
    }
    emit banlistsChanged();
    validateLegality();
}

void DeckController::setDirty(bool value) {
    if (dirty_ == value)
        return;
    dirty_ = value;
    emit dirtyChanged();
}

void DeckController::setLastError(const QString& error) {
    if (lastError_ == error)
        return;
    lastError_ = error;
    emit lastErrorChanged();
}

std::vector<edopro_next::data::CardCode>& DeckController::sectionVector(Section section) {
    switch (section) {
    case Section::Main:
        return deck_.main;
    case Section::Extra:
        return deck_.extra;
    case Section::Side:
        return deck_.side;
    }
    return deck_.main; // unreachable
}

DeckSectionModel* DeckController::modelFor(Section section) {
    switch (section) {
    case Section::Main:
        return mainModel_;
    case Section::Extra:
        return extraModel_;
    case Section::Side:
        return sideModel_;
    }
    return mainModel_; // unreachable
}

void DeckController::addCard(quint32 code, Section section) {
    // CardCode::None (0) is never a valid Deck entry (data/'s own
    // invariant - deck-model.md#5); neither real UI path that can add a
    // card can trigger this today (search results never contain code 0,
    // since CardDatabase::load_database() itself rejects a code-0 row as
    // a load failure; parse_ydk excludes a code-0 line from the resulting
    // Deck), but addCard() is a public Q_INVOKABLE, and a silent, correct
    // no-op here is a small, deliberate guard against the public surface
    // ever being able to violate that invariant, from any caller.
    if (code == 0)
        return;
    auto& vec = sectionVector(section);
    auto* model = modelFor(section);
    const int index = static_cast<int>(vec.size());
    // beginInsertRows() must run before the vector actually grows - not
    // after, which is what calling a single begin+end pair post-mutation
    // would do - so views observe the old row count for exactly as long as
    // QAbstractItemModel's own contract requires (caught by external
    // review, not by ui/tests/test_deckbuilder.cpp: a plain single-row
    // ListView append tolerates the wrong order in practice, which is
    // exactly why this needs the contract stated, not just "it worked").
    model->notifyAboutToInsert(index);
    vec.push_back(static_cast<edopro_next::data::CardCode>(code));
    model->notifyInserted();
    setDirty(true);
    emit deckChanged();
    validateLegality();
}

void DeckController::removeAt(Section section, int index) {
    auto& vec = sectionVector(section);
    if (index < 0 || static_cast<std::size_t>(index) >= vec.size())
        return;
    auto* model = modelFor(section);
    model->notifyAboutToRemove(index);
    vec.erase(vec.begin() + index);
    model->notifyRemoved();
    setDirty(true);
    emit deckChanged();
    validateLegality();
}

void DeckController::newDeck() {
    mainModel_->notifyAboutToReset();
    extraModel_->notifyAboutToReset();
    sideModel_->notifyAboutToReset();
    deck_.clear();
    mainModel_->notifyReset();
    extraModel_->notifyReset();
    sideModel_->notifyReset();
    currentPath_.clear();
    emit currentPathChanged();
    setLastError(QString());
    setDirty(false);
    emit deckChanged();
    validateLegality();
}

bool DeckController::loadDeck(const QUrl& fileUrl) {
    if (!fileUrl.isLocalFile()) {
        setLastError(QStringLiteral("Not a local file: %1").arg(fileUrl.toString()));
        return false;
    }
    const QString path = fileUrl.toLocalFile();
    auto result = edopro_next::data::load_ydk(to_fs_path(path));
    if (!result.ok) {
        // load_ydk()'s own contract: a failed load returns an empty Deck
        // in a fresh result value, never mutating anything - `deck_` is
        // simply never touched below, so it (and `dirty_`) are exactly
        // what they were before this call.
        setLastError(QString::fromStdString(result.error));
        return false;
    }
    mainModel_->notifyAboutToReset();
    extraModel_->notifyAboutToReset();
    sideModel_->notifyAboutToReset();
    deck_ = std::move(result.deck);
    mainModel_->notifyReset();
    extraModel_->notifyReset();
    sideModel_->notifyReset();
    currentPath_ = path;
    emit currentPathChanged();
    setLastError(QString());
    setDirty(false);
    emit deckChanged();
    validateLegality();
    return true;
}

bool DeckController::saveDeck() {
    if (currentPath_.isEmpty())
        return false;
    return saveToPath(currentPath_);
}

bool DeckController::saveDeckAs(const QUrl& fileUrl) {
    if (!fileUrl.isLocalFile()) {
        setLastError(QStringLiteral("Not a local file: %1").arg(fileUrl.toString()));
        return false;
    }
    return saveToPath(fileUrl.toLocalFile());
}

bool DeckController::saveToPath(const QString& path) {
    const auto result = edopro_next::data::save_ydk(to_fs_path(path), deck_);
    if (!result.ok) {
        setLastError(QString::fromStdString(result.error));
        return false; // dirty_ deliberately left unchanged - still true
    }
    currentPath_ = path;
    emit currentPathChanged();
    setLastError(QString());
    setDirty(false);
    return true;
}

void DeckController::validateLegality() {
    const auto& rulesets = edopro_next::ui::availableRulesets();
    if (rulesets.empty())
        return;
    const int rIdx = std::clamp(selectedRulesetIndex_, 0, static_cast<int>(rulesets.size()) - 1);
    const auto& ruleset = rulesets[static_cast<std::size_t>(rIdx)];

    const auto& lflistOpt = banlistStore_.listAt(selectedBanlistIndex_);
    auto policy = ruleset.makePolicy(lflistOpt);

    static const edopro_next::data::CardDatabase kEmptyDb;
    const auto& db = (catalog_ != nullptr) ? catalog_->database() : kEmptyDb;

    const auto error = edopro_next::policy::validate_deck(deck_, db, policy);

    const bool newLegal = (error.type == edopro_next::policy::DeckErrorType::None);
    QString newMsg;
    if (!lflistOpt.has_value()) {
        // No banlist selected: policy::validate_deck() takes the same
        // short-circuit upstream's null LFList* does (gframe/deck_manager.cpp
        // :217-218; policy/src/deck_validation.cpp; docs/architecture/
        // deck-legality.md §5) and returns before card-scope,
        // section-placement or the three-copy cap ever run - regardless of
        // what `error` says. S3 (brief 015 reopened corrections): that must
        // never be silently reported as full legality, in either direction.
        if (newLegal) {
            newMsg = QStringLiteral(
                "Deck meets this ruleset's size and type limits. No banlist is selected: "
                "card-scope, section-placement and copy-limit checks are not being made.");
        } else {
            newMsg = formatLegalityError(error)
                + QStringLiteral(" No banlist is selected: card-scope, section-placement "
                                  "and copy-limit checks are not being made either way.");
        }
    } else if (newLegal) {
        newMsg = QStringLiteral("Deck is legal for duel entry under this ruleset and banlist.");
    } else {
        newMsg = formatLegalityError(error);
    }
    const int newErrType = static_cast<int>(error.type);
    const quint32 newCardCode = static_cast<quint32>(error.card);

    bool changed = false;
    if (isLegal_ != newLegal) {
        isLegal_ = newLegal;
        changed = true;
    }
    if (legalityMessage_ != newMsg) {
        legalityMessage_ = newMsg;
        changed = true;
    }
    if (legalityErrorType_ != newErrType) {
        legalityErrorType_ = newErrType;
        changed = true;
    }
    if (legalityCardCode_ != newCardCode) {
        legalityCardCode_ = newCardCode;
        changed = true;
    }

    if (changed) {
        emit legalityChanged();
    }
}

QString DeckController::formatCard(edopro_next::data::CardCode code) const {
    if (code == edopro_next::data::CardCode::None)
        return QString();
    if (catalog_ != nullptr) {
        const auto* record = catalog_->database().find(code);
        if (record && !record->name.empty()) {
            return QStringLiteral("'%1' (%2)")
                .arg(QString::fromUtf8(record->name.data(), static_cast<qsizetype>(record->name.size())))
                .arg(static_cast<quint32>(code));
        }
    }
    return QStringLiteral("card %1").arg(static_cast<quint32>(code));
}

QString DeckController::formatLegalityError(const edopro_next::policy::DeckValidationError& error) const {
    switch (error.type) {
    case edopro_next::policy::DeckErrorType::MainCount:
        if (error.count.current < error.count.minimum) {
            return QStringLiteral("Would not be accepted at duel entry: Main deck has %1 cards, fewer than the minimum of %2.")
                .arg(error.count.current)
                .arg(error.count.minimum);
        } else {
            return QStringLiteral("Would not be accepted at duel entry: Main deck has %1 cards, exceeding the maximum of %2.")
                .arg(error.count.current)
                .arg(error.count.maximum);
        }
    case edopro_next::policy::DeckErrorType::ExtraCount:
        if (error.card != edopro_next::data::CardCode::None) {
            return QStringLiteral("Would not be accepted at duel entry: %1 belongs in the Main deck, not the Extra deck.")
                .arg(formatCard(error.card));
        } else if (error.count.current > error.count.maximum) {
            return QStringLiteral("Would not be accepted at duel entry: Extra deck has %1 cards, exceeding the maximum of %2.")
                .arg(error.count.current)
                .arg(error.count.maximum);
        } else {
            return QStringLiteral("Would not be accepted at duel entry: Extra deck has %1 cards, fewer than the minimum of %2.")
                .arg(error.count.current)
                .arg(error.count.minimum);
        }
    case edopro_next::policy::DeckErrorType::SideCount:
        if (error.count.current > error.count.maximum) {
            return QStringLiteral("Would not be accepted at duel entry: Side deck has %1 cards, exceeding the maximum of %2.")
                .arg(error.count.current)
                .arg(error.count.maximum);
        } else {
            return QStringLiteral("Would not be accepted at duel entry: Side deck has %1 cards, fewer than the minimum of %2.")
                .arg(error.count.current)
                .arg(error.count.minimum);
        }
    case edopro_next::policy::DeckErrorType::UnknownCard:
        return QStringLiteral("Would not be accepted at duel entry: Unknown card code %1 (not found in database).")
            .arg(static_cast<quint32>(error.card));
    case edopro_next::policy::DeckErrorType::ForbiddenType:
        return QStringLiteral("Would not be accepted at duel entry: Deck contains cards of a forbidden card type.");
    case edopro_next::policy::DeckErrorType::TooManyLegends:
        return QStringLiteral("Would not be accepted at duel entry: Deck exceeds the allowed number of Legend cards.");
    case edopro_next::policy::DeckErrorType::TooManySkills:
        return QStringLiteral("Would not be accepted at duel entry: Deck exceeds the allowed number of Skill cards.");
    case edopro_next::policy::DeckErrorType::CardCount:
        return QStringLiteral("Would not be accepted at duel entry: %1 exceeds the maximum allowed copy limit.")
            .arg(formatCard(error.card));
    case edopro_next::policy::DeckErrorType::TcgOnly:
        return QStringLiteral("Would not be accepted at duel entry: %1 is TCG-only, not allowed under this ruleset.")
            .arg(formatCard(error.card));
    case edopro_next::policy::DeckErrorType::OcgOnly:
        return QStringLiteral("Would not be accepted at duel entry: %1 is OCG-only, not allowed under this ruleset.")
            .arg(formatCard(error.card));
    case edopro_next::policy::DeckErrorType::UnofficialCard:
        return QStringLiteral("Would not be accepted at duel entry: %1 is an unofficial or custom card.")
            .arg(formatCard(error.card));
    case edopro_next::policy::DeckErrorType::Lflist:
        return QStringLiteral("Would not be accepted at duel entry: %1 exceeds the banlist limitation count.")
            .arg(formatCard(error.card));
    case edopro_next::policy::DeckErrorType::None:
        return QStringLiteral("Deck is legal for duel entry under this ruleset and banlist.");
    }
    return QStringLiteral("Would not be accepted at duel entry: Deck is invalid.");
}
