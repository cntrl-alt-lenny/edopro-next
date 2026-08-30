// SPDX-License-Identifier: AGPL-3.0-or-later

#include "deck_controller.h"

#include <QFileInfo>

#include "edopro_next/data/ydk.h"
#include "qt_path.h"

DeckController::DeckController(QObject* parent)
    : QObject(parent),
      mainModel_(new DeckSectionModel(this)),
      extraModel_(new DeckSectionModel(this)),
      sideModel_(new DeckSectionModel(this)) {
    rebindModels();
}

CardCatalog* DeckController::catalog() const { return catalog_; }

void DeckController::setCatalog(CardCatalog* catalog) {
    if (catalog_ == catalog)
        return;
    catalog_ = catalog;
    rebindModels();
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
