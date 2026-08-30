// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Qt Test coverage for the deck-builder C++ adapter layer
// (ui/src/deckbuilder/) - CardCatalog, SearchResultsModel, DeckSectionModel,
// DeckController. Qt Test is not a new dependency: Qt is already this
// project's UI framework (ui/CMakeLists.txt). Every fixture here is a tiny
// synthetic SQLite database and/or .ydk text built at runtime - never a
// committed Project Ignis `.cdb` file (CLAUDE.md) and never real card data.

#include <QAbstractItemModelTester>
#include <QObject>
#include <QPair>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <sqlite3.h>

#include <fstream>

#include "card_catalog.h"
#include "card_entry.h"
#include "deck_controller.h"
#include "edopro_next/data/ydk.h"
#include "search_results_model.h"

namespace {

// A minimal synthetic-.cdb builder, scoped to exactly what this test file
// needs - a fresh, small copy rather than reaching into data/tests/'s own
// synthetic_cdb.h, matching this codebase's established preference for
// test-only helpers not to create cross-module coupling
// (data/tests/test_support.h's own doc comment explains the same choice
// for client/ vs data/).
void run(sqlite3* db, const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        qFatal("synthetic .cdb setup failed: %s", err ? err : "unknown error");
    }
}

// A full-control synthetic card row: everything the two-argument
// (code, name) convenience below defaults for an ordinary Level monster
// (TYPE_MONSTER, atk/def 1000, level 4) can be overridden explicitly - used
// by the Level/Rank/Link Rating presentation tests, which each need a
// distinct real `type` bit combination and, for the Link case, a raw `def`
// column value that CardDatabase::load_database() reinterprets as
// `link_marker` (data/src/card_database.cpp - see kTypeLinkBit there).
struct SyntheticCard {
    quint32 code;
    QString name;
    quint32 type = 0x1; // TYPE_MONSTER (ocgcore/ocgapi_constants.h:33)
    qint32 attack = 1000;
    // Raw `datas.def` column value - a real defense for a non-Link type,
    // or the link-marker bitmask for a Link type (see struct doc above).
    qint32 defenseOrLinkMarker = 1000;
    qint32 level = 4;
};

// A distinct name, not an overload of writeSyntheticDatabase() below: a
// braced-init-list argument like {{code, "name"}} is equally viable for
// either QList<SyntheticCard> (via aggregate init) or
// QList<QPair<quint32, QString>>, which the compiler rejects as an
// ambiguous overload - confirmed empirically (GCC: "call of overloaded
// 'writeSyntheticDatabase(...)' is ambiguous") against every pre-existing
// call site that passes such a list directly, not through a named
// QList<QPair<...>> variable.
QString writeSyntheticDatabaseWithFields(const QString& path, const QList<SyntheticCard>& cards) {
    sqlite3* db = nullptr;
    if (sqlite3_open(path.toStdString().c_str(), &db) != SQLITE_OK)
        qFatal("failed to create synthetic .cdb at %s", qPrintable(path));
    run(db, "CREATE TABLE datas (id INTEGER PRIMARY KEY NOT NULL, ot INTEGER NOT NULL, "
            "alias INTEGER NOT NULL, setcode INTEGER NOT NULL, type INTEGER NOT NULL, "
            "atk INTEGER NOT NULL, def INTEGER NOT NULL, level INTEGER NOT NULL, "
            "race INTEGER NOT NULL, attribute INTEGER NOT NULL, category INTEGER NOT NULL);");
    run(db, "CREATE TABLE texts (id INTEGER PRIMARY KEY NOT NULL, name TEXT, desc TEXT, "
            "str1 TEXT, str2 TEXT, str3 TEXT, str4 TEXT, str5 TEXT, str6 TEXT, str7 TEXT, "
            "str8 TEXT, str9 TEXT, str10 TEXT, str11 TEXT, str12 TEXT, str13 TEXT, "
            "str14 TEXT, str15 TEXT, str16 TEXT);");
    for (const auto& card : cards) {
        run(db, qPrintable(QStringLiteral("INSERT INTO datas (id,ot,alias,setcode,type,atk,def,"
                                           "level,race,attribute,category) VALUES (%1,0,0,0,%2,"
                                           "%3,%4,%5,0,0,0);")
                                .arg(card.code)
                                .arg(card.type)
                                .arg(card.attack)
                                .arg(card.defenseOrLinkMarker)
                                .arg(card.level)));
        // str1..str16 are left at their column default (NULL - the schema
        // above does not mark them NOT NULL) by omitting them from the
        // column list entirely, rather than hand-counting sixteen '' value
        // placeholders to match a fixed column list.
        run(db, qPrintable(QStringLiteral("INSERT INTO texts (id,name,desc) "
                                           "VALUES (%1,'%2','synthetic text');")
                                .arg(card.code)
                                .arg(card.name)));
    }
    sqlite3_close(db);
    return path;
}

QString writeSyntheticDatabase(const QString& path, const QList<QPair<quint32, QString>>& cards) {
    QList<SyntheticCard> converted;
    converted.reserve(cards.size());
    for (const auto& [code, name] : cards)
        converted.push_back(SyntheticCard{code, name});
    return writeSyntheticDatabaseWithFields(path, converted);
}

void writeFile(const QString& path, const QString& contents) {
    std::ofstream file(path.toStdString(), std::ios::binary);
    const std::string text = contents.toStdString();
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
}

} // namespace

class TestDeckBuilder : public QObject {
    Q_OBJECT

private slots:
    // A) database/search
    void loadDatabaseAndSearch();
    void failedDatabaseLoadIsReportedHonestly();

    // B) add/remove/order/multiplicity
    void addRemoveOrderAndMultiplicity();

    // C) explicit sections
    void explicitSectionChoiceIsNeverReclassified();

    // D) YDK load
    void loadPreservesOrderDuplicatesAndUnknownCodes();

    // E) YDK save
    void savedDeckRoundTripsThroughParseYdk();

    // F) failed load
    void failedLoadDoesNotDestroyCurrentDeck();

    // G) dirty state
    void dirtyStateTransitionsMatchContract();

    // H) search/deck separation
    void searchingNeverMutatesTheDeck();

    // I) model safety
    void reloadingTheCatalogNeverLeavesStaleResults();

    // Section 8 of the follow-up review pass: Qt's own QAbstractItemModel
    // contract, enforced by QAbstractItemModelTester rather than only by
    // this file's own Deck-content assertions - which the real P1
    // model-notification-ordering bug (mutating the vector, then calling a
    // single combined begin+end pair) passed cleanly, because none of them
    // inspected rowCount() at the moment a begin*/end* signal fired.
    void modelInvariantsHoldAcrossEveryMutation();

    // Section 5 of the follow-up review pass: DeckSectionModel resolves
    // NameRole/KnownRole from CardCatalog at display time rather than
    // storing them, but was never told when the bound catalog's contents
    // actually changed - a live view could keep showing a stale name or
    // "Unknown card" after a reload until something else happened to
    // touch that row.
    void deckRowsRefreshWhenCatalogReloads();

    // Second follow-up review pass, section 5: CardCatalog::loaded()'s
    // documented contract ("true once at least one database file has
    // loaded successfully") disagreed with its own implementation
    // (`database_.size() > 0`) - a syntactically valid, schema-correct but
    // legitimately empty .cdb would load successfully yet report
    // loaded() == false. Fixed by tracking success explicitly rather than
    // deriving it from cardCount.
    void emptyButValidDatabaseCountsAsLoaded();
    void partialSuccessAcrossMultiplePathsStillCountsAsLoaded();

    // Second follow-up review pass, section 6: CardCode::None (0) is
    // documented everywhere in data/ as "not a real card" and is never
    // produced by either real UI path that can add a card
    // (Add-to-section only ever offers codes CardSearchIndex actually
    // found in a loaded CardDatabase, which itself rejects code-0 rows as
    // a load failure; parse_ydk excludes a code-0 line from the resulting
    // Deck) - but DeckController::addCard() is a public Q_INVOKABLE, and
    // nothing stopped it from accepting 0 directly.
    void addCardSilentlyRejectsCardCodeZero();

    // External review, blocker: CardPreview.qml labelled every monster's
    // stored level/rank/link-rating magnitude "Level" unconditionally, and
    // never rendered link_marker at all - a source-fidelity bug against
    // gframe/game.cpp's own card-info formatting and
    // DataManager::FormatLinkMarker() (gframe/data_manager.cpp). These pin
    // make_card_entry()'s presentation flags/fields directly; the real QML
    // rendering is additionally covered in test_deckbuilder_screen.cpp,
    // which is the mandatory assertion - this file's coverage is the
    // adapter-level complement, not a substitute for it.
    void ordinaryMonsterEntryIsNotClassifiedAsXyzOrLink();
    void xyzMonsterEntryIsClassifiedAsXyzNotLink();
    void linkMonsterEntryHasNoRealDefenseAndFormatsItsMarkers();

    // External review, third follow-up pass: -1/-2 are real, displayed
    // "varies" values for a stored attack/defense (CardRecord's own doc
    // comment), and upstream's own card-info panel (gframe/game.cpp)
    // renders any negative value as "?" - CardPreview and the search
    // summary line were both showing the literal negative number instead.
    void negativeCombatStatsRenderAsQuestionMarks();
    void searchSummaryRendersUnknownCombatStatsAsQuestionMarks();
};

void TestDeckBuilder::loadDatabaseAndSearch() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath =
        writeSyntheticDatabase(dir.filePath("cards.cdb"), {{111, "Alpha Dragon"}, {222, "Beta Knight"}});

    CardCatalog catalog;
    QVERIFY(catalog.loadDatabases({dbPath}));
    QVERIFY(catalog.loaded());
    QCOMPARE(catalog.cardCount(), 2);
    QVERIFY(catalog.lastError().isEmpty());

    SearchResultsModel results;
    results.setCatalog(&catalog);
    results.setQueryText("Dragon");
    QCOMPARE(results.resultCount(), 1);
    QCOMPARE(results.cardCodeAt(0), 111u);
    QCOMPARE(results.data(results.index(0, 0), SearchResultsModel::NameRole).toString(),
             QStringLiteral("Alpha Dragon"));
}

void TestDeckBuilder::failedDatabaseLoadIsReportedHonestly() {
    CardCatalog catalog;
    QVERIFY(!catalog.loadDatabases({"/definitely/does/not/exist.cdb"}));
    QVERIFY(!catalog.loaded());
    QCOMPARE(catalog.cardCount(), 0);
    QVERIFY(!catalog.lastError().isEmpty());
}

void TestDeckBuilder::addRemoveOrderAndMultiplicity() {
    DeckController controller;
    controller.addCard(1, DeckController::Section::Main);
    controller.addCard(2, DeckController::Section::Main);
    controller.addCard(1, DeckController::Section::Main); // duplicate, kept as a third entry
    QCOMPARE(controller.mainCount(), 3);
    QCOMPARE(controller.deck().main,
             (std::vector<edopro_next::data::CardCode>{edopro_next::data::CardCode{1},
                                                         edopro_next::data::CardCode{2},
                                                         edopro_next::data::CardCode{1}}));

    // Remove exactly the middle occurrence - one entry, not "all copies of 1".
    controller.removeAt(DeckController::Section::Main, 1);
    QCOMPARE(controller.mainCount(), 2);
    QCOMPARE(controller.deck().main,
             (std::vector<edopro_next::data::CardCode>{edopro_next::data::CardCode{1},
                                                         edopro_next::data::CardCode{1}}));
}

void TestDeckBuilder::explicitSectionChoiceIsNeverReclassified() {
    DeckController controller;
    controller.addCard(999, DeckController::Section::Main);
    controller.addCard(999, DeckController::Section::Extra);
    controller.addCard(999, DeckController::Section::Side);
    QCOMPARE(controller.mainCount(), 1);
    QCOMPARE(controller.extraCount(), 1);
    QCOMPARE(controller.sideCount(), 1);
    QCOMPARE(controller.deck().main.front(), edopro_next::data::CardCode{999});
    QCOMPARE(controller.deck().extra.front(), edopro_next::data::CardCode{999});
    QCOMPARE(controller.deck().side.front(), edopro_next::data::CardCode{999});
}

void TestDeckBuilder::loadPreservesOrderDuplicatesAndUnknownCodes() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString ydkPath = dir.filePath("deck.ydk");
    writeFile(ydkPath, "#main\n1\n1\n2\n#extra\n3\n!side\n999999999\n");

    DeckController controller;
    QVERIFY(controller.loadDeck(QUrl::fromLocalFile(ydkPath)));
    QCOMPARE(controller.mainCount(), 3);
    QCOMPARE(controller.extraCount(), 1);
    QCOMPARE(controller.sideCount(), 1);
    QCOMPARE(controller.deck().main,
             (std::vector<edopro_next::data::CardCode>{edopro_next::data::CardCode{1},
                                                         edopro_next::data::CardCode{1},
                                                         edopro_next::data::CardCode{2}}));
    // The unknown code (no catalog loaded at all here) survives intact.
    QCOMPARE(controller.deck().side.front(), edopro_next::data::CardCode{999999999});

    CardCatalog emptyCatalog;
    controller.setCatalog(&emptyCatalog);
    const auto index = controller.sideModel()->index(0, 0);
    QCOMPARE(controller.sideModel()->data(index, DeckSectionModel::KnownRole).toBool(), false);
    QCOMPARE(controller.sideModel()->data(index, DeckSectionModel::CardCodeRole).toUInt(),
             999999999u);
}

void TestDeckBuilder::savedDeckRoundTripsThroughParseYdk() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("saved.ydk");

    DeckController controller;
    controller.addCard(10, DeckController::Section::Main);
    controller.addCard(10, DeckController::Section::Main);
    controller.addCard(20, DeckController::Section::Extra);
    controller.addCard(30, DeckController::Section::Side);
    QVERIFY(controller.saveDeckAs(QUrl::fromLocalFile(path)));

    const auto loaded = edopro_next::data::load_ydk(std::filesystem::path(path.toStdString()));
    QVERIFY(loaded.ok);
    QCOMPARE(loaded.deck == controller.deck(), true);
}

void TestDeckBuilder::failedLoadDoesNotDestroyCurrentDeck() {
    DeckController controller;
    controller.addCard(42, DeckController::Section::Main);
    QVERIFY(!controller.loadDeck(QUrl::fromLocalFile("/definitely/does/not/exist.ydk")));
    QCOMPARE(controller.mainCount(), 1);
    QCOMPARE(controller.deck().main.front(), edopro_next::data::CardCode{42});
    QVERIFY(!controller.lastError().isEmpty());
}

void TestDeckBuilder::dirtyStateTransitionsMatchContract() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    DeckController controller;

    QCOMPARE(controller.dirty(), false); // freshly created: not dirty

    controller.addCard(1, DeckController::Section::Main);
    QCOMPARE(controller.dirty(), true); // editing: dirty

    const QString path = dir.filePath("d.ydk");
    QVERIFY(controller.saveDeckAs(QUrl::fromLocalFile(path)));
    QCOMPARE(controller.dirty(), false); // successful save: not dirty

    controller.removeAt(DeckController::Section::Main, 0);
    QCOMPARE(controller.dirty(), true); // editing again: dirty

    // A save to an unwritable path fails and leaves dirty untouched - and
    // currentPath must stay at the last genuinely successful path, not the
    // failed one, so a later plain "Save" still targets somewhere real.
    QVERIFY(!controller.saveDeckAs(QUrl::fromLocalFile("/definitely/not/writable/x.ydk")));
    QCOMPARE(controller.dirty(), true); // failed save: remains dirty
    QCOMPARE(controller.currentPath(), path);

    controller.addCard(2, DeckController::Section::Main);
    QVERIFY(controller.saveDeckAs(QUrl::fromLocalFile(path)));
    QCOMPARE(controller.dirty(), false);

    QVERIFY(controller.loadDeck(QUrl::fromLocalFile(path)));
    QCOMPARE(controller.dirty(), false); // successful load: not dirty

    DeckController fresh;
    fresh.newDeck();
    QCOMPARE(fresh.dirty(), false); // new deck: not dirty
}

void TestDeckBuilder::searchingNeverMutatesTheDeck() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = writeSyntheticDatabase(dir.filePath("cards.cdb"), {{1, "Alpha"}});

    CardCatalog catalog;
    QVERIFY(catalog.loadDatabases({dbPath}));

    DeckController controller;
    controller.addCard(1, DeckController::Section::Main);
    const auto before = controller.deck();

    SearchResultsModel results;
    results.setCatalog(&catalog);
    results.setQueryText("Alpha");
    results.setQueryText("");
    results.setQueryText("nonexistent card name");

    QCOMPARE(controller.deck() == before, true);
}

void TestDeckBuilder::reloadingTheCatalogNeverLeavesStaleResults() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // Deliberately adversarial, not just "the same code, renamed": A has a
    // code (2) that B does not. The old, buggy in-place-merge
    // implementation (load_database() called directly on the existing
    // member) would still pass a same-code-renamed-only test, since
    // load_database()'s own last-file-wins overlay already handles that
    // case correctly on its own - the bug only shows up for a code that
    // should *disappear* on replacement, which is exactly what this checks.
    const QString dbPathA =
        writeSyntheticDatabase(dir.filePath("a.cdb"), {{1, "Original Name"}, {2, "Only In A"}});
    const QString dbPathB = writeSyntheticDatabase(dir.filePath("b.cdb"), {{1, "Replaced Name"}});

    CardCatalog catalog;
    QVERIFY(catalog.loadDatabases({dbPathA}));
    QCOMPARE(catalog.cardCount(), 2);

    SearchResultsModel results;
    results.setCatalog(&catalog);
    results.setQueryText("Original");
    QCOMPARE(results.resultCount(), 1);

    QVERIFY(catalog.loadDatabases({dbPathB}));
    QCOMPARE(catalog.cardCount(), 1); // not 2 - code 2 must be gone, not merged in

    results.setQueryText("Original");
    QCOMPARE(results.resultCount(), 0);
    results.setQueryText("Replaced");
    QCOMPARE(results.resultCount(), 1);
    QCOMPARE(results.data(results.index(0, 0), SearchResultsModel::NameRole).toString(),
             QStringLiteral("Replaced Name"));

    // Code 2 must be completely gone - not just unmatched by name, but
    // absent from the catalog and unresolvable by its exact code either.
    results.setQueryText("Only In A");
    QCOMPARE(results.resultCount(), 0);

    // An all-failed subsequent reload must clear everything, not preserve
    // B's data as if the failed call had never happened.
    QVERIFY(!catalog.loadDatabases({"/definitely/does/not/exist.cdb"}));
    QCOMPARE(catalog.loaded(), false);
    QCOMPARE(catalog.cardCount(), 0);
    QVERIFY(!catalog.lastError().isEmpty());
    results.setQueryText("Replaced");
    QCOMPARE(results.resultCount(), 0);
}

void TestDeckBuilder::modelInvariantsHoldAcrossEveryMutation() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    DeckController controller;
    // Each tester independently enforces QAbstractItemModel's own
    // contract (begin*() before the model's visible state changes,
    // end*()/dataChanged() after - among other invariants) on every
    // signal its bound model emits for the rest of this test. QtTest mode
    // integrates a violation as an ordinary Qt Test failure rather than
    // aborting the whole binary (Fatal) or only warning (Warning) -
    // matching Qt's own documented recommendation for a Qt Test host.
    QAbstractItemModelTester mainTester(controller.mainModel(),
                                         QAbstractItemModelTester::FailureReportingMode::QtTest);
    QAbstractItemModelTester extraTester(controller.extraModel(),
                                          QAbstractItemModelTester::FailureReportingMode::QtTest);
    QAbstractItemModelTester sideTester(controller.sideModel(),
                                         QAbstractItemModelTester::FailureReportingMode::QtTest);

    // empty -> populated, including a duplicate append.
    controller.addCard(1, DeckController::Section::Main);
    controller.addCard(2, DeckController::Section::Main);
    controller.addCard(1, DeckController::Section::Main); // duplicate
    controller.addCard(10, DeckController::Section::Extra);
    controller.addCard(20, DeckController::Section::Side);
    controller.addCard(21, DeckController::Section::Side);
    controller.addCard(22, DeckController::Section::Side);

    // Remove first, then (what is now) middle, then (what is now) last.
    controller.removeAt(DeckController::Section::Side, 0); // removes 20; [21, 22]
    controller.addCard(23, DeckController::Section::Side); // [21, 22, 23]
    controller.removeAt(DeckController::Section::Side, 1); // removes 22 (middle); [21, 23]
    controller.addCard(24, DeckController::Section::Side); // [21, 23, 24]
    controller.removeAt(DeckController::Section::Side, 2); // removes 24 (last); [21, 23]
    QCOMPARE(controller.sideCount(), 2);

    // populated -> empty, via newDeck().
    controller.newDeck();
    QCOMPARE(controller.mainCount(), 0);
    QCOMPARE(controller.extraCount(), 0);
    QCOMPARE(controller.sideCount(), 0);

    // empty -> populated again, via loadDeck() replacing all three
    // sections in one call.
    const QString path = dir.filePath("model_tester.ydk");
    writeFile(path, "#main\n1\n2\n#extra\n3\n!side\n4\n5\n");
    QVERIFY(controller.loadDeck(QUrl::fromLocalFile(path)));
    QCOMPARE(controller.mainCount(), 2);
    QCOMPARE(controller.extraCount(), 1);
    QCOMPARE(controller.sideCount(), 2);

    // populated -> empty again.
    controller.newDeck();
    QCOMPARE(controller.mainCount(), 0);
    QCOMPARE(controller.extraCount(), 0);
    QCOMPARE(controller.sideCount(), 0);
}

void TestDeckBuilder::deckRowsRefreshWhenCatalogReloads() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPathA = writeSyntheticDatabase(dir.filePath("a.cdb"), {{1, "Alpha"}});
    const QString dbPathB = writeSyntheticDatabase(dir.filePath("b.cdb"), {{1, "Beta"}});

    CardCatalog catalog;
    DeckController controller;
    controller.setCatalog(&catalog);
    controller.addCard(1, DeckController::Section::Main);

    const auto rowIndex = controller.mainModel()->index(0, 0);
    QCOMPARE(controller.mainModel()->data(rowIndex, DeckSectionModel::KnownRole).toBool(), false);

    // unknown -> known must announce itself, not just be true the next
    // time something else happens to call data() on this row.
    QSignalSpy spy(controller.mainModel(), &DeckSectionModel::dataChanged);
    QVERIFY(catalog.loadDatabases({dbPathA}));
    QVERIFY(spy.count() >= 1);

    QCOMPARE(controller.mainModel()->data(rowIndex, DeckSectionModel::KnownRole).toBool(), true);
    QCOMPARE(controller.mainModel()->data(rowIndex, DeckSectionModel::NameRole).toString(),
             QStringLiteral("Alpha"));

    // known(Alpha) -> known(Beta), same code, must announce itself too.
    spy.clear();
    QVERIFY(catalog.loadDatabases({dbPathB}));
    QVERIFY(spy.count() >= 1);
    QCOMPARE(controller.mainModel()->data(rowIndex, DeckSectionModel::NameRole).toString(),
             QStringLiteral("Beta"));
}

void TestDeckBuilder::emptyButValidDatabaseCountsAsLoaded() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // An empty card list still creates the real datas/texts schema (see
    // writeSyntheticDatabase) and simply inserts zero rows - a genuinely
    // valid, schema-correct .cdb that happens to have no cards in it, not
    // a malformed file.
    const QString dbPath = writeSyntheticDatabase(dir.filePath("empty.cdb"), {});

    CardCatalog catalog;
    QVERIFY(catalog.loadDatabases({dbPath}));
    QVERIFY(catalog.loaded());
    QCOMPARE(catalog.cardCount(), 0);
    QVERIFY(catalog.lastError().isEmpty());
}

void TestDeckBuilder::partialSuccessAcrossMultiplePathsStillCountsAsLoaded() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString goodPath = writeSyntheticDatabase(dir.filePath("good.cdb"), {{1, "Alpha"}});

    CardCatalog catalog;
    // loadDatabases()'s own documented return contract is "every
    // requested path succeeded", which one missing file here violates -
    // that is a separate claim from loaded(), which only asks whether
    // *any* path succeeded.
    QVERIFY(!catalog.loadDatabases({goodPath, "/definitely/does/not/exist.cdb"}));
    QVERIFY(catalog.loaded());
    QCOMPARE(catalog.cardCount(), 1);
    QVERIFY(!catalog.lastError().isEmpty());
}

void TestDeckBuilder::addCardSilentlyRejectsCardCodeZero() {
    DeckController controller;
    controller.addCard(0, DeckController::Section::Main);
    QCOMPARE(controller.mainCount(), 0);
    // Not even a no-op edit - a rejected add was never a real edit at all.
    QCOMPARE(controller.dirty(), false);
}

void TestDeckBuilder::ordinaryMonsterEntryIsNotClassifiedAsXyzOrLink() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = writeSyntheticDatabaseWithFields(
        dir.filePath("cards.cdb"),
        QList<SyntheticCard>{SyntheticCard{111, QStringLiteral("Ordinary"), 0x1, 1800, 1200, 4}});

    CardCatalog catalog;
    QVERIFY(catalog.loadDatabases({dbPath}));

    const auto entry = catalog.cardDetails(111);
    QVERIFY(entry.known);
    QVERIFY(entry.isMonster);
    QVERIFY(!entry.isXyz);
    QVERIFY(!entry.isLink);
    QCOMPARE(entry.attack, 1800);
    QCOMPARE(entry.defense, 1200);
    QCOMPARE(entry.level, 4u);
    QVERIFY(entry.linkMarkerDisplay.isEmpty());
}

void TestDeckBuilder::xyzMonsterEntryIsClassifiedAsXyzNotLink() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // TYPE_MONSTER | TYPE_XYZ (ocgcore/ocgapi_constants.h:33,55).
    constexpr quint32 kXyzType = 0x1 | 0x800000;
    const QString dbPath = writeSyntheticDatabaseWithFields(
        dir.filePath("cards.cdb"),
        QList<SyntheticCard>{SyntheticCard{222, QStringLiteral("Xyz"), kXyzType, 2000, 1500, 4}});

    CardCatalog catalog;
    QVERIFY(catalog.loadDatabases({dbPath}));

    const auto entry = catalog.cardDetails(222);
    QVERIFY(entry.known);
    QVERIFY(entry.isMonster);
    QVERIFY(entry.isXyz);
    QVERIFY(!entry.isLink);
    // An Xyz has an ordinary DEF stat, unlike a Link.
    QCOMPARE(entry.attack, 2000);
    QCOMPARE(entry.defense, 1500);
    // The Rank magnitude - same field a Level monster's Level lives in;
    // isXyz is what tells the presentation layer to call it "Rank".
    QCOMPARE(entry.level, 4u);
}

void TestDeckBuilder::linkMonsterEntryHasNoRealDefenseAndFormatsItsMarkers() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // TYPE_MONSTER | TYPE_LINK (ocgcore/ocgapi_constants.h:33,58).
    constexpr quint32 kLinkType = 0x1 | 0x4000000;
    // Three deliberately non-adjacent marker bits (LINK_MARKER_TOP_RIGHT |
    // LINK_MARKER_LEFT | LINK_MARKER_BOTTOM_LEFT - ocgcore/
    // ocgapi_constants.h:197-204), chosen so a wrong iteration order (e.g.
    // ascending bit value instead of upstream's fixed positional order)
    // would produce visibly different output. Written into the raw `def`
    // column - CardDatabase::load_database() reinterprets it as
    // `link_marker` for a Link-type row (data/src/card_database.cpp).
    constexpr qint32 kLinkMarkerBits = 0x100 | 0x8 | 0x1;
    const QString dbPath = writeSyntheticDatabaseWithFields(
        dir.filePath("cards.cdb"),
        QList<SyntheticCard>{
            SyntheticCard{333, QStringLiteral("Link"), kLinkType, 2500, kLinkMarkerBits, 3}});

    CardCatalog catalog;
    QVERIFY(catalog.loadDatabases({dbPath}));

    const auto entry = catalog.cardDetails(333);
    QVERIFY(entry.known);
    QVERIFY(entry.isMonster);
    QVERIFY(entry.isLink);
    QVERIFY(!entry.isXyz);
    QCOMPARE(entry.attack, 2500);
    // No real DEF for a Link - CardDatabase already zeroes this at load
    // time (data/'s own documented Link exception), and CardEntry carries
    // that straight through.
    QCOMPARE(entry.defense, 0);
    QCOMPARE(static_cast<quint32>(entry.linkMarker), static_cast<quint32>(kLinkMarkerBits));
    // The Link Rating magnitude - same field a Level monster's Level lives
    // in; isLink is what tells the presentation layer to call it "Link
    // Rating" instead, and to stop calling `defense` a real DEF stat.
    QCOMPARE(entry.level, 3u);
    // Upstream's fixed positional order (top-left, top, top-right, left,
    // right, bottom-left, bottom, bottom-right - gframe/data_manager.cpp's
    // FormatLinkMarker()): only top-right, left, and bottom-left are set
    // here, so they must render in exactly that order, not bit-value order.
    QCOMPARE(entry.linkMarkerDisplay, QStringLiteral("[↗][←][↙]"));
}

void TestDeckBuilder::negativeCombatStatsRenderAsQuestionMarks() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // -1 and -2 are both real, in-use "varies" sentinels (CardRecord's own
    // doc comment) - using both, not just one, confirms the rule is "any
    // negative value", matching gframe/game.cpp's own `< 0` check, not a
    // narrower "== -1" or "== -2" equality check.
    const QString dbPath = writeSyntheticDatabaseWithFields(
        dir.filePath("cards.cdb"),
        QList<SyntheticCard>{SyntheticCard{111, QStringLiteral("Varies"), 0x1, -2, -1, 4}});

    CardCatalog catalog;
    QVERIFY(catalog.loadDatabases({dbPath}));

    const auto entry = catalog.cardDetails(111);
    QVERIFY(entry.known);
    // The raw values are preserved exactly - data/'s own "not sentinels
    // this module strips" contract - only the *display* string says "?".
    QCOMPARE(entry.attack, -2);
    QCOMPARE(entry.defense, -1);
    QCOMPARE(entry.attackDisplay, QStringLiteral("?"));
    QCOMPARE(entry.defenseDisplay, QStringLiteral("?"));

    // An ordinary positive-stat card must still show its real numbers -
    // the fix must not turn every card's stats into "?".
    const QString normalPath = writeSyntheticDatabaseWithFields(
        dir.filePath("normal.cdb"),
        QList<SyntheticCard>{SyntheticCard{222, QStringLiteral("Normal"), 0x1, 1800, 1200, 4}});
    CardCatalog normalCatalog;
    QVERIFY(normalCatalog.loadDatabases({normalPath}));
    const auto normalEntry = normalCatalog.cardDetails(222);
    QCOMPARE(normalEntry.attackDisplay, QStringLiteral("1800"));
    QCOMPARE(normalEntry.defenseDisplay, QStringLiteral("1200"));
}

void TestDeckBuilder::searchSummaryRendersUnknownCombatStatsAsQuestionMarks() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = writeSyntheticDatabaseWithFields(
        dir.filePath("cards.cdb"),
        QList<SyntheticCard>{SyntheticCard{111, QStringLiteral("Varies"), 0x1, -2, -1, 4}});

    CardCatalog catalog;
    QVERIFY(catalog.loadDatabases({dbPath}));

    SearchResultsModel results;
    results.setCatalog(&catalog);
    results.setQueryText("Varies");
    QCOMPARE(results.resultCount(), 1);
    QCOMPARE(results.data(results.index(0, 0), SearchResultsModel::SummaryRole).toString(),
             QStringLiteral("ATK ? / DEF ?"));
}

QTEST_MAIN(TestDeckBuilder)
#include "test_deckbuilder.moc"
