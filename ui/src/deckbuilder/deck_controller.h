// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The single owner of the deck currently being edited. DeckController holds
// the one canonical edopro_next::data::Deck for this editing session -
// three ordered vectors of CardCode, exactly as data/'s M3B model defines
// it (docs/architecture/deck-model.md) - and every mutation (add/remove/
// load/new) goes through this class. QML never holds a second copy of deck
// contents: it reads through mainModel/extraModel/sideModel, which are
// thin views over this object's own Deck (deck_section_model.h).
//
// Section placement: this class never decides by itself which section a
// card belongs in. addCardToDeck() asks policy::classify_card() - the one
// definition of upstream's Extra Deck rule (policy/include/edopro_next/
// policy/deck_placement.h, ADR 0011) - and puts the card where that answer
// says; addCard() puts a card exactly where its caller names, unclassified.
// See docs/architecture/deck-builder-ui.md §7 and deck-placement.md §5.
//
// Legality validation:
// Deck legality is computed strictly by policy::validate_deck() (CLAUDE.md,
// ADR 0007, ADR 0010). This controller owns the user-selected ruleset and
// banlist inputs, and re-validates the deck reactively whenever the deck,
// catalog, ruleset, or banlist changes. Results are exposed as advisory
// properties (isLegal, legalityMessage, etc.) for QML to render.

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <qqmlintegration.h>

#include "banlist_store.h"
#include "deck_section_model.h"
#include "edopro_next/data/deck.h"
#include "edopro_next/policy/deck_placement.h"
#include "edopro_next/policy/deck_validation.h"
#include "ruleset.h"

class CardCatalog;

class DeckController : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(CardCatalog* catalog READ catalog WRITE setCatalog NOTIFY catalogChanged)

    Q_PROPERTY(DeckSectionModel* mainModel READ mainModel CONSTANT)
    Q_PROPERTY(DeckSectionModel* extraModel READ extraModel CONSTANT)
    Q_PROPERTY(DeckSectionModel* sideModel READ sideModel CONSTANT)

    Q_PROPERTY(int mainCount READ mainCount NOTIFY deckChanged)
    Q_PROPERTY(int extraCount READ extraCount NOTIFY deckChanged)
    Q_PROPERTY(int sideCount READ sideCount NOTIFY deckChanged)

    // See docs/architecture/deck-builder-ui.md#dirty-state-contract for the
    // exact transition table this implements.
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    // Empty when the current deck has never been loaded from, or saved to,
    // a file - a caller should route "Save" to saveDeckAs() in that case.
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QString currentFileName READ currentFileName NOTIFY currentPathChanged)
    // The most recent load/save failure's message; empty after a
    // successful operation. Not cumulative - only the latest attempt.
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

    // Ruleset and banlist selection for deck legality validation (M3 Brief 015 / ADR 0010).
    Q_PROPERTY(QStringList rulesetNames READ rulesetNames CONSTANT)
    Q_PROPERTY(int selectedRulesetIndex READ selectedRulesetIndex WRITE setSelectedRulesetIndex NOTIFY selectedRulesetChanged)

    Q_PROPERTY(QStringList banlistNames READ banlistNames NOTIFY banlistsChanged)
    Q_PROPERTY(int selectedBanlistIndex READ selectedBanlistIndex WRITE setSelectedBanlistIndex NOTIFY selectedBanlistChanged)

    // Advisory legality status computed by policy::validate_deck().
    Q_PROPERTY(bool isLegal READ isLegal NOTIFY legalityChanged)
    Q_PROPERTY(QString legalityMessage READ legalityMessage NOTIFY legalityChanged)
    Q_PROPERTY(int legalityErrorType READ legalityErrorType NOTIFY legalityChanged)
    Q_PROPERTY(quint32 legalityCardCode READ legalityCardCode NOTIFY legalityChanged)

public:
    // QML_ELEMENT on the enclosing QObject is enough to expose a Q_ENUM to
    // QML as DeckController.Main/.Extra/.Side - no separate registration.
    enum class Section { Main, Extra, Side };
    Q_ENUM(Section)

    explicit DeckController(QObject* parent = nullptr);

    CardCatalog* catalog() const;
    void setCatalog(CardCatalog* catalog);

    DeckSectionModel* mainModel() const { return mainModel_; }
    DeckSectionModel* extraModel() const { return extraModel_; }
    DeckSectionModel* sideModel() const { return sideModel_; }

    int mainCount() const;
    int extraCount() const;
    int sideCount() const;
    bool dirty() const;
    QString currentPath() const;
    QString currentFileName() const;
    QString lastError() const;

    QStringList rulesetNames() const;
    int selectedRulesetIndex() const { return selectedRulesetIndex_; }
    void setSelectedRulesetIndex(int index);

    QStringList banlistNames() const;
    int selectedBanlistIndex() const { return selectedBanlistIndex_; }
    void setSelectedBanlistIndex(int index);

    bool isLegal() const { return isLegal_; }
    QString legalityMessage() const { return legalityMessage_; }
    int legalityErrorType() const { return legalityErrorType_; }
    quint32 legalityCardCode() const { return legalityCardCode_; }

    // Banlist loading affordances
    Q_INVOKABLE bool loadBanlistFile(const QString& path);
    void loadBanlists(const QStringList& paths);
    void loadBanlistFromText(const std::string& text);
    const edopro_next::ui::BanlistStore& banlistStore() const { return banlistStore_; }

    // Where addCardToDeck() would put `code`: Section::Main or
    // Section::Extra as an int, or -1 when it would not add it at all (a
    // token, a code the loaded catalog does not know, code 0, or no catalog
    // bound). The answer is policy::classify_card()'s, under
    // kAddRitualPlacement; QML renders it and decides nothing itself.
    Q_INVOKABLE int placementFor(quint32 code) const;

    // The deck builder's "add" action: appends `code` to the end of the
    // section placementFor() names. That is where upstream's deck builder
    // lands a card added from its search results (deck_con.cpp:725,
    // push_main then push_extra) for every card except the families
    // deck-placement.md §3.3 records, where this editor follows LoadDeck
    // instead (ADR 0011, Decision 2). Returns that
    // section as an int, or -1 - with the deck untouched - when
    // placementFor() is -1.
    Q_INVOKABLE int addCardToDeck(quint32 code);

    // Appends `code` to the end of the requested section, unclassified -
    // order and duplicates are exactly what the caller asked for, never
    // deduplicated, never reordered, never moved. The screen uses it for
    // Side (upstream's Shift+right-click, deck_con.cpp:721-722); a card put
    // in the "wrong" Main/Extra section through it stays there and
    // advisory legality reports it (ADR 0011, Decision 3).
    Q_INVOKABLE void addCard(quint32 code, Section section);
    // Removes exactly the occurrence at `index` within `section` - one
    // entry, not "all copies of this code".
    Q_INVOKABLE void removeAt(Section section, int index);

    // Discards the current deck unconditionally - a caller that must not
    // lose unsaved edits is responsible for checking `dirty` and confirming
    // with the user first (docs/architecture/deck-builder-ui.md
    // #new-deck-and-destructive-actions); this method does not ask.
    Q_INVOKABLE void newDeck();

    // On failure, the current deck is left completely untouched - this
    // relies on load_ydk()'s own transactional contract
    // (docs/architecture/deck-model.md), not a separate guard here.
    Q_INVOKABLE bool loadDeck(const QUrl& fileUrl);
    // False, with no effect, if there is no currentPath yet - the caller
    // should offer saveDeckAs() instead.
    Q_INVOKABLE bool saveDeck();
    Q_INVOKABLE bool saveDeckAs(const QUrl& fileUrl);

    // C++-only - used by tests to compare against a Deck produced by
    // parse_ydk()/load_ydk() directly.
    const edopro_next::data::Deck& deck() const { return deck_; }

    // The sentence legalityMessage() shows for `error`. Public, and C++-only,
    // so the tests can pin every message, including the ones no small
    // synthetic deck reaches. Every message speaks about the deck *as
    // arranged* in the editor's own sections, never about what upstream's
    // duel entry would do: the server re-derives Main and Extra from card
    // type and drops tokens before it validates (deck-placement.md §5.4), so
    // it can accept a deck this rejects and, with a token present, reject one
    // this passes.
    QString formatLegalityError(const edopro_next::policy::DeckValidationError& error) const;

    // The RITUAL_LOCATION upstream's deck builder effectively applies when
    // it adds a card outside side-decking: push_main refuses a Rush Ritual
    // Monster and push_extra refuses any other Ritual Monster
    // (deck_con.cpp:1578-1583, :1611-1616), which is RITUAL_LOCATION::DEFAULT
    // (deck-placement.md §3; ADR 0011, Decision 2). It does not follow the
    // selected ruleset's ritualsBelongInExtra, because upstream's editor
    // does not consult DUEL_EXTRA_DECK_RITUAL unless it is side-decking.
    static constexpr edopro_next::policy::RitualPlacement kAddRitualPlacement =
        edopro_next::policy::RitualPlacement::RushInExtra;

signals:
    void catalogChanged();
    void deckChanged();
    void dirtyChanged();
    void currentPathChanged();
    void lastErrorChanged();
    void selectedRulesetChanged();
    void banlistsChanged();
    void selectedBanlistChanged();
    void legalityChanged();

private:
    std::vector<edopro_next::data::CardCode>& sectionVector(Section section);
    DeckSectionModel* modelFor(Section section);
    void setDirty(bool value);
    void setLastError(const QString& error);
    void rebindModels();
    bool saveToPath(const QString& path);

    void validateLegality();
    QString formatCard(edopro_next::data::CardCode code) const;

    edopro_next::data::Deck deck_;
    CardCatalog* catalog_ = nullptr;
    DeckSectionModel* mainModel_;
    DeckSectionModel* extraModel_;
    DeckSectionModel* sideModel_;
    bool dirty_ = false;
    QString currentPath_;
    QString lastError_;

    edopro_next::ui::BanlistStore banlistStore_;
    int selectedRulesetIndex_ = 0;
    int selectedBanlistIndex_ = 0;

    bool isLegal_ = false;
    QString legalityMessage_;
    int legalityErrorType_ = 0;
    quint32 legalityCardCode_ = 0;
};
