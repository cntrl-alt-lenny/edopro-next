// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Behavioural coverage for the *actual* DeckBuilderScreen.qml, loaded
// through a real QQmlEngine (via TestHarness.qml) - not just the C++
// adapter classes test_deckbuilder.cpp already covers in isolation.
// External review found real bugs (stale selection highlights, a
// no-catalog deck editor that was entirely hidden) that construction-only
// adapter tests cannot see, because they never touch the QML that was
// actually wrong. Selection is simulated the way a real click actually
// drives it - setting a DeckSectionList's own `currentIndex` (see
// Harness::selectByClick()), which triggers the same entryActivated ->
// selectDeckEntry() chain a click does - rather than calling
// selectDeckEntry() itself directly, which would skip the step that sets
// the clicked list's own currentIndex and silently pass without it. Other
// screen functions (clearAllSelection, removeSelectedDeckEntry) are
// invoked directly, and native FileDialog interaction is avoided entirely
// (fragile and platform-dependent for no additional coverage) - these are
// the exact same functions the real buttons and shortcuts call.

#include <QGuiApplication>
#include <QPair>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <sqlite3.h>

#include <fstream>

#include "card_catalog.h"
#include "deck_controller.h"

namespace {

// Same minimal synthetic-.cdb builder as test_deckbuilder.cpp, duplicated
// rather than shared - see that file's own doc comment for why a tiny
// local copy is preferred over cross-test-file coupling.
void run(sqlite3* db, const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK)
        qFatal("synthetic .cdb setup failed: %s", err ? err : "unknown error");
}

// A full-control synthetic card row - see test_deckbuilder.cpp's own copy
// of this struct for why the Level/Rank/Link Rating presentation tests
// need explicit `type`/`level`/raw-`def` control rather than the fixed
// TYPE_MONSTER/1000/1000/4 the two-argument convenience below always
// writes. Duplicated here, not shared, matching this file's own established
// preference for a tiny local copy over cross-test-file coupling.
struct SyntheticCard {
    quint32 code;
    QString name;
    quint32 type = 0x1; // TYPE_MONSTER (ocgcore/ocgapi_constants.h:33)
    qint32 attack = 1000;
    // Raw `datas.def` column value - a real defense for a non-Link type, or
    // the link-marker bitmask for a Link type (data/src/card_database.cpp).
    qint32 defenseOrLinkMarker = 1000;
    qint32 level = 4;
};

// A distinct name, not an overload of writeSyntheticDatabase() below - see
// test_deckbuilder.cpp's identical copy of this comment for why: a
// braced-init-list argument is ambiguous between QList<SyntheticCard> (via
// aggregate init) and QList<QPair<quint32, QString>>, confirmed empirically
// against every pre-existing call site in this file that passes such a
// list directly.
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
        run(db, qPrintable(QStringLiteral("INSERT INTO texts (id,name,desc) VALUES (%1,'%2','synthetic text');")
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

QString writeSyntheticDatabase(const QString& path, quint32 code, const QString& name) {
    return writeSyntheticDatabase(path, QList<QPair<quint32, QString>>{{code, name}});
}

void writeFile(const QString& path, const QString& contents) {
    std::ofstream file(path.toStdString(), std::ios::binary);
    const std::string text = contents.toStdString();
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
}

// Hosts one real DeckBuilderScreen instance (via TestHarness.qml) per
// test, with fresh CardCatalog/DeckController C++ objects behind it -
// mirrors main.cpp's own wiring exactly (same context property names,
// same setCatalog() call), so this is testing the real integration, not
// an approximation of it.
class Harness {
public:
    Harness() {
        controller.setCatalog(&catalog);
        engine.rootContext()->setContextProperty(QStringLiteral("cardCatalog"), &catalog);
        engine.rootContext()->setContextProperty(QStringLiteral("deckController"), &controller);
        engine.loadFromModule("EdoproNext", "TestHarness");
    }

    bool valid() const { return !engine.rootObjects().isEmpty() && screen() != nullptr; }

    QObject* screen() const {
        if (engine.rootObjects().isEmpty())
            return nullptr;
        return engine.rootObjects().constFirst()->findChild<QObject*>(QStringLiteral("screen"));
    }

    QObject* child(const char* name) const { return screen()->findChild<QObject*>(name); }

    QVariant prop(const char* name) const { return screen()->property(name); }

    void invoke(const char* method) {
        QMetaObject::invokeMethod(screen(), method);
    }

    int currentIndexOf(const char* listObjectName) const {
        return child(listObjectName)->property("currentIndex").toInt();
    }

    // Mirrors exactly what a real click does (DeckSectionList.qml's
    // ItemDelegate.onClicked: `listView.currentIndex = index`) - setting
    // the list's own currentIndex, which triggers its onCurrentIndexChanged
    // -> entryActivated -> DeckBuilderScreen.selectDeckEntry() chain via
    // ordinary Qt property change notification, exactly as a click would.
    // Calling selectDeckEntry() directly from a test would skip that first
    // step, since selectDeckEntry() only clears *other* lists' selection -
    // it relies on the clicked list's own currentIndex having already been
    // set by the click that led to it being called, matching the real
    // control flow documented in deck-builder-ui.md#7-1.
    void selectByClick(const char* listObjectName, int row) {
        child(listObjectName)->setProperty("currentIndex", row);
    }

    // Emits DeckSectionList's own removeRequested(row) signal directly -
    // the exact same signal its real Keys.onDeletePressed/Backspace
    // handlers emit (DeckSectionList.qml) - rather than simulating a
    // literal keypress (fragile under the offscreen QPA platform, which
    // has no real window-manager focus semantics) or calling a
    // test-only alternative path. QML-declared signals are ordinary
    // invokable members of the dynamic meta-object, so invoking one by
    // name through QMetaObject::invokeMethod emits it exactly as `emit
    // removeRequested(row)` from within the component's own QML would.
    void requestKeyboardRemoval(const char* listObjectName, int row) {
        // Q_ARG(int, ...), not Q_ARG(QVariant, ...): unlike a QML-declared
        // JS function (which always takes QVariant parameters, since JS
        // has no static types), a QML `signal removeRequested(int row)`
        // generates a real Qt signal whose parameter keeps its declared
        // C++ type - invokeMethod's overload resolution matches on that
        // exact type, confirmed empirically (QVariant here produced "No
        // such method ...::removeRequested(QVariant); Candidates are:
        // removeRequested(int)").
        QMetaObject::invokeMethod(child(listObjectName), "removeRequested", Q_ARG(int, row));
    }

    void setSearchQuery(const QString& text) {
        child("searchField")->setProperty("text", text);
    }

    CardCatalog catalog;
    DeckController controller;
    QQmlApplicationEngine engine;
};

} // namespace

class TestDeckBuilderScreen : public QObject {
    Q_OBJECT

private slots:
    // A) no-catalog editor remains usable
    void noCatalogDeckEditorStaysFunctional();

    // B) unknown-card deck with no catalog, selectable and previewable
    void unknownCardDeckIsSelectableWithNoCatalog();

    // C) selection is mutually exclusive across sections and search
    void selectingOneSectionClearsAllOthers();

    // D) Escape-equivalent clears every visible selection
    void clearAllSelectionClearsEveryList();

    // E) New clears stale preview/selection
    void newDeckClearsStaleSelection();

    // F) Load/replace clears stale preview/selection
    void loadDeckClearsStaleSelection();

    // Section 15: a card code above INT_MAX survives the real QML
    // selection/preview path exactly, not just the underlying C++ model.
    void aboveIntMaxCardCodeSurvivesSelection();

    // Found via visual verification, not by any earlier automated test:
    // Qt Quick's ListView defaults currentIndex to 0 (not -1) the instant
    // a non-empty model is set, with no user interaction at all. A
    // screenshot of an otherwise-untouched deck showed its first entry
    // already highlighted, "Remove selected" already enabled, and the
    // preview pane already populated. Adapter-only tests never caught
    // this because DeckController/DeckSectionModel have no concept of
    // "highlighted" at all - it is purely a QML ListView property no
    // C++-only test ever reads.
    void populatingASectionNeverAutoSelectsItsFirstRow();
    void populatingSearchResultsNeverAutoSelectsTheFirstRow();

    // External review: DeckSectionList's Keys.onDeletePressed/Backspace
    // originally called deckController.removeAt() directly, bypassing
    // DeckBuilderScreen.removeSelectedDeckEntry()'s clearAllSelection()
    // entirely - leaving stale selectedSection/selectedDeckRow/hasPreview/
    // previewCode, and "Remove selected" enabled against a row that no
    // longer represents the selected card. Both exercise the exact
    // removeRequested signal the real keyboard handlers emit (see
    // Harness::requestKeyboardRemoval()), not a parallel test-only path.
    void keyboardRemovalOneRowSectionClearsSelection();
    void keyboardRemovalMultiRowSectionRemovesOnlyTheIntendedCard();

    // External review, second finding: SearchResultsModel::refresh() does
    // a full model reset for every query change or catalog reload, but a
    // previously selected result's currentIndex can survive that reset as
    // the same *number* pointing at completely different data - Qt Quick's
    // ListView does not reliably re-fire currentIndexChanged just because
    // the data at an unchanged index changed. populatingSearchResults...
    // above only covers the pristine case, before any selection has ever
    // broken the initial currentIndex: -1 binding; these three cover the
    // state after a real selection has already happened.
    void selectedResultClearsOnDifferentQuery();
    void selectedResultClearsOnZeroResultQuery();
    void selectedResultClearsOnCatalogReplacement();

    // External review, blocker: the real CardPreview.qml labelled every
    // monster's stored level/rank/link-rating magnitude "Level"
    // unconditionally (an Xyz's Rank and a Link's Link Rating both showed
    // as "Level"), presented a Link's DEF as though it were a real stat,
    // and never rendered link_marker at all - a source-fidelity bug against
    // gframe/game.cpp's card-info formatting and DataManager::
    // FormatLinkMarker() (gframe/data_manager.cpp). These exercise the
    // actual rendered Text items (objectName-tagged in CardPreview.qml),
    // not just CardEntry's presentation flags in isolation - the adapter-
    // level complement lives in test_deckbuilder.cpp.
    void ordinaryMonsterPreviewLabelsItsLevelCorrectly();
    void xyzMonsterPreviewLabelsItsRankNotLevel();
    void linkMonsterPreviewHidesDefenseAndShowsLinkRatingAndMarkers();
};

void TestDeckBuilderScreen::noCatalogDeckEditorStaysFunctional() {
    Harness h;
    QVERIFY(h.valid());
    QCOMPARE(h.prop("hasCatalog").toBool(), false);

    // The no-catalog message replaces the search UI only - not the deck
    // pane or preview pane, which must stay visible and usable.
    QCOMPARE(h.child("noCatalogMessage")->property("visible").toBool(), true);
    QCOMPARE(h.child("mainList")->property("visible").toBool(), true);
    QCOMPARE(h.child("extraList")->property("visible").toBool(), true);
    QCOMPARE(h.child("sideList")->property("visible").toBool(), true);

    h.controller.addCard(111, DeckController::Section::Main);
    QCOMPARE(h.controller.mainCount(), 1);

    h.selectByClick("mainList", 0);
    QCOMPARE(h.prop("hasPreview").toBool(), true);
    QCOMPARE(h.prop("previewKnown").toBool(), false);
}

void TestDeckBuilderScreen::unknownCardDeckIsSelectableWithNoCatalog() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString ydkPath = dir.filePath("deck.ydk");
    writeFile(ydkPath, "#main\n999999999\n#extra\n!side\n");

    Harness h;
    QVERIFY(h.valid());
    QVERIFY(h.controller.loadDeck(QUrl::fromLocalFile(ydkPath)));
    QCOMPARE(h.controller.mainCount(), 1);

    h.selectByClick("mainList", 0);
    QCOMPARE(h.prop("hasPreview").toBool(), true);
    QCOMPARE(h.prop("previewKnown").toBool(), false);
    QCOMPARE(h.prop("previewCode").toDouble(), 999999999.0);

    // Removable too, with no catalog loaded at all.
    h.invoke("removeSelectedDeckEntry");
    QCOMPARE(h.controller.mainCount(), 0);
    QCOMPARE(h.prop("hasPreview").toBool(), false);
}

void TestDeckBuilderScreen::selectingOneSectionClearsAllOthers() {
    Harness h;
    QVERIFY(h.valid());
    h.controller.addCard(1, DeckController::Section::Main);
    h.controller.addCard(2, DeckController::Section::Extra);
    h.controller.addCard(3, DeckController::Section::Side);

    h.selectByClick("mainList", 0);
    QCOMPARE(h.currentIndexOf("mainList"), 0);
    QCOMPARE(h.currentIndexOf("extraList"), -1);
    QCOMPARE(h.currentIndexOf("sideList"), -1);

    h.selectByClick("extraList", 0);
    QCOMPARE(h.currentIndexOf("mainList"), -1);
    QCOMPARE(h.currentIndexOf("extraList"), 0);
    QCOMPARE(h.currentIndexOf("sideList"), -1);
    QCOMPARE(h.prop("selectedSection").toInt(), static_cast<int>(DeckController::Section::Extra));

    h.selectByClick("sideList", 0);
    QCOMPARE(h.currentIndexOf("mainList"), -1);
    QCOMPARE(h.currentIndexOf("extraList"), -1);
    QCOMPARE(h.currentIndexOf("sideList"), 0);
}

void TestDeckBuilderScreen::clearAllSelectionClearsEveryList() {
    Harness h;
    QVERIFY(h.valid());
    h.controller.addCard(1, DeckController::Section::Main);
    h.selectByClick("mainList", 0);
    QCOMPARE(h.currentIndexOf("mainList"), 0);
    QCOMPARE(h.prop("hasPreview").toBool(), true);

    // clearAllSelection() is exactly what the Escape Shortcut calls -
    // invoked directly here rather than through a simulated keypress,
    // which would depend on offscreen-platform window-activation behaviour
    // for no additional coverage of this screen's own logic.
    h.invoke("clearAllSelection");
    QCOMPARE(h.currentIndexOf("mainList"), -1);
    QCOMPARE(h.currentIndexOf("extraList"), -1);
    QCOMPARE(h.currentIndexOf("sideList"), -1);
    QCOMPARE(h.currentIndexOf("resultsList"), -1);
    QCOMPARE(h.prop("hasPreview").toBool(), false);
    QCOMPARE(h.prop("selectedSection").toInt(), -1);
}

void TestDeckBuilderScreen::newDeckClearsStaleSelection() {
    Harness h;
    QVERIFY(h.valid());
    h.controller.addCard(1, DeckController::Section::Main);
    h.selectByClick("mainList", 0);
    QCOMPARE(h.prop("hasPreview").toBool(), true);

    // Exactly what the New button's handler does once confirmThen() has
    // decided to proceed (deckController.newDeck(); root.clearAllSelection();).
    h.controller.newDeck();
    h.invoke("clearAllSelection");

    QCOMPARE(h.controller.mainCount(), 0);
    QCOMPARE(h.prop("hasPreview").toBool(), false);
    QCOMPARE(h.currentIndexOf("mainList"), -1);
}

void TestDeckBuilderScreen::loadDeckClearsStaleSelection() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("d.ydk");
    writeFile(path, "#main\n1\n#extra\n!side\n");

    Harness h;
    QVERIFY(h.valid());
    h.controller.addCard(42, DeckController::Section::Main);
    h.selectByClick("mainList", 0);
    QCOMPARE(h.prop("hasPreview").toBool(), true);
    QCOMPARE(h.prop("previewCode").toDouble(), 42.0);

    // Exactly what the Open dialog's onAccepted does on success.
    QVERIFY(h.controller.loadDeck(QUrl::fromLocalFile(path)));
    h.invoke("clearAllSelection");

    QCOMPARE(h.controller.mainCount(), 1);
    QCOMPARE(h.prop("hasPreview").toBool(), false);
    QCOMPARE(h.currentIndexOf("mainList"), -1);
}

void TestDeckBuilderScreen::aboveIntMaxCardCodeSurvivesSelection() {
    // A valid, non-zero uint32 code above INT_MAX (2147483647) - the exact
    // class of value a signed `int` sentinel could not represent, which is
    // why previewCode is a `double` (see DeckBuilderScreen.qml).
    constexpr quint32 kLargeCode = 4000000000u;

    Harness h;
    QVERIFY(h.valid());
    h.controller.addCard(kLargeCode, DeckController::Section::Main);

    h.selectByClick("mainList", 0);
    QCOMPARE(h.prop("hasPreview").toBool(), true);
    // previewCode is exactly what the preview pane's `entry:` binding
    // passes to cardCatalog.cardDetails() (DeckBuilderScreen.qml) - a
    // signed 32-bit int sentinel could not have represented this value at
    // all (external review's exact concern); double is exact here.
    QCOMPARE(h.prop("previewCode").toDouble(), static_cast<double>(kLargeCode));
    QCOMPARE(h.child("cardPreview")->property("visible").toBool(), true);
}

void TestDeckBuilderScreen::populatingASectionNeverAutoSelectsItsFirstRow() {
    Harness h;
    QVERIFY(h.valid());

    // Adding a card is exactly what happens right after Add to Main/
    // Extra/Side, or after loading a .ydk - none of these are a
    // selection, and none should be treated as one.
    h.controller.addCard(111, DeckController::Section::Main);

    QCOMPARE(h.currentIndexOf("mainList"), -1);
    QCOMPARE(h.prop("hasPreview").toBool(), false);
    QCOMPARE(h.prop("selectedSection").toInt(), -1);
}

void TestDeckBuilderScreen::populatingSearchResultsNeverAutoSelectsTheFirstRow() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = writeSyntheticDatabase(dir.filePath("cards.cdb"), 111, QStringLiteral("Alpha"));

    Harness h;
    QVERIFY(h.valid());
    QVERIFY(h.catalog.loadDatabases({dbPath}));

    // An unfiltered (empty) query text - the screen's own default state -
    // returns every loaded card, exactly the "results appear the instant
    // a database loads" case a screenshot caught auto-selecting itself.
    h.child("searchField")->setProperty("text", QStringLiteral(""));

    QCOMPARE(h.currentIndexOf("resultsList"), -1);
    QCOMPARE(h.prop("hasPreview").toBool(), false);
    QCOMPARE(h.prop("selectedResultRow").toInt(), -1);
}

void TestDeckBuilderScreen::keyboardRemovalOneRowSectionClearsSelection() {
    Harness h;
    QVERIFY(h.valid());
    h.controller.addCard(111, DeckController::Section::Main);
    h.selectByClick("mainList", 0);
    QCOMPARE(h.prop("hasPreview").toBool(), true);

    h.requestKeyboardRemoval("mainList", 0);

    QCOMPARE(h.controller.mainCount(), 0);
    QCOMPARE(h.currentIndexOf("mainList"), -1);
    QCOMPARE(h.prop("selectedSection").toInt(), -1);
    QCOMPARE(h.prop("selectedDeckRow").toInt(), -1);
    QCOMPARE(h.prop("hasPreview").toBool(), false);
}

void TestDeckBuilderScreen::keyboardRemovalMultiRowSectionRemovesOnlyTheIntendedCard() {
    Harness h;
    QVERIFY(h.valid());
    h.controller.addCard(111, DeckController::Section::Main);
    h.controller.addCard(222, DeckController::Section::Main);
    h.selectByClick("mainList", 0);
    QCOMPARE(h.prop("previewCode").toDouble(), 111.0);

    h.requestKeyboardRemoval("mainList", 0);

    // Only the intended first card (111) was removed - 222 remains, now
    // shifted into index 0 but never itself selected or previewed.
    QCOMPARE(h.controller.mainCount(), 1);
    QCOMPARE(h.controller.deck().main.front(), edopro_next::data::CardCode{222});
    QCOMPARE(h.currentIndexOf("mainList"), -1);
    QCOMPARE(h.prop("hasPreview").toBool(), false);
    QCOMPARE(h.prop("selectedSection").toInt(), -1);
    QCOMPARE(h.prop("selectedDeckRow").toInt(), -1);

    // Nothing is selected any more, so "Remove selected" - wired to
    // removeSelectedDeckEntry(), the same function the button calls -
    // must not accidentally remove card 222.
    h.invoke("removeSelectedDeckEntry");
    QCOMPARE(h.controller.mainCount(), 1);
    QCOMPARE(h.controller.deck().main.front(), edopro_next::data::CardCode{222});
}

void TestDeckBuilderScreen::selectedResultClearsOnDifferentQuery() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = writeSyntheticDatabase(
        dir.filePath("cards.cdb"), {{111, QStringLiteral("Alpha")}, {222, QStringLiteral("Beta")}});

    Harness h;
    QVERIFY(h.valid());
    QVERIFY(h.catalog.loadDatabases({dbPath}));

    h.setSearchQuery(QStringLiteral("Alpha"));
    h.selectByClick("resultsList", 0);
    QCOMPARE(h.prop("hasPreview").toBool(), true);
    QCOMPARE(h.prop("previewCode").toDouble(), 111.0);

    // A different, still non-empty query - SearchResultsModel::refresh()
    // does a full model reset here, exactly as it would for a real
    // keystroke.
    h.setSearchQuery(QStringLiteral("Beta"));

    QCOMPARE(h.currentIndexOf("resultsList"), -1);
    QCOMPARE(h.prop("selectedResultRow").toInt(), -1);
    QCOMPARE(h.prop("hasPreview").toBool(), false);
}

void TestDeckBuilderScreen::selectedResultClearsOnZeroResultQuery() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = writeSyntheticDatabase(dir.filePath("cards.cdb"), 111, QStringLiteral("Alpha"));

    Harness h;
    QVERIFY(h.valid());
    QVERIFY(h.catalog.loadDatabases({dbPath}));

    h.setSearchQuery(QStringLiteral("Alpha"));
    h.selectByClick("resultsList", 0);
    QCOMPARE(h.prop("hasPreview").toBool(), true);

    h.setSearchQuery(QStringLiteral("nonexistent card name"));

    QCOMPARE(h.child("resultsList")->property("count").toInt(), 0);
    QCOMPARE(h.currentIndexOf("resultsList"), -1);
    QCOMPARE(h.prop("selectedResultRow").toInt(), -1);
    QCOMPARE(h.prop("hasPreview").toBool(), false);
}

void TestDeckBuilderScreen::selectedResultClearsOnCatalogReplacement() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPathA = writeSyntheticDatabase(dir.filePath("a.cdb"), 111, QStringLiteral("Alpha"));
    const QString dbPathB = writeSyntheticDatabase(dir.filePath("b.cdb"), 222, QStringLiteral("Beta"));

    Harness h;
    QVERIFY(h.valid());
    QVERIFY(h.catalog.loadDatabases({dbPathA}));

    h.setSearchQuery(QStringLiteral("Alpha"));
    h.selectByClick("resultsList", 0);
    QCOMPARE(h.prop("hasPreview").toBool(), true);
    QCOMPARE(h.prop("previewCode").toDouble(), 111.0);

    // Replaces the catalog entirely - code 111 is no longer present at
    // all, not merely unmatched by the current query text.
    QVERIFY(h.catalog.loadDatabases({dbPathB}));

    QCOMPARE(h.currentIndexOf("resultsList"), -1);
    QCOMPARE(h.prop("selectedResultRow").toInt(), -1);
    QCOMPARE(h.prop("hasPreview").toBool(), false);
}

void TestDeckBuilderScreen::ordinaryMonsterPreviewLabelsItsLevelCorrectly() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = writeSyntheticDatabaseWithFields(
        dir.filePath("cards.cdb"),
        QList<SyntheticCard>{SyntheticCard{111, QStringLiteral("Ordinary"), 0x1, 1800, 1200, 4}});

    Harness h;
    QVERIFY(h.valid());
    QVERIFY(h.catalog.loadDatabases({dbPath}));

    h.setSearchQuery(QStringLiteral("Ordinary"));
    h.selectByClick("resultsList", 0);
    QCOMPARE(h.prop("hasPreview").toBool(), true);

    QCOMPARE(h.child("atkValueText")->property("text").toString(), QStringLiteral("1800"));
    QCOMPARE(h.child("defRowLabel")->property("visible").toBool(), true);
    QCOMPARE(h.child("defRowValue")->property("visible").toBool(), true);
    QCOMPARE(h.child("defRowValue")->property("text").toString(), QStringLiteral("1200"));
    QCOMPARE(h.child("levelRankLabelText")->property("text").toString(), QStringLiteral("Level"));
    QCOMPARE(h.child("levelRankValueText")->property("text").toString(), QStringLiteral("4"));
    QCOMPARE(h.child("linkMarkerRowLabel")->property("visible").toBool(), false);
    QCOMPARE(h.child("linkMarkerValueText")->property("visible").toBool(), false);
}

void TestDeckBuilderScreen::xyzMonsterPreviewLabelsItsRankNotLevel() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // TYPE_MONSTER | TYPE_XYZ (ocgcore/ocgapi_constants.h:33,55).
    constexpr quint32 kXyzType = 0x1 | 0x800000;
    const QString dbPath = writeSyntheticDatabaseWithFields(
        dir.filePath("cards.cdb"),
        QList<SyntheticCard>{SyntheticCard{222, QStringLiteral("XyzCard"), kXyzType, 2000, 1500, 4}});

    Harness h;
    QVERIFY(h.valid());
    QVERIFY(h.catalog.loadDatabases({dbPath}));

    h.setSearchQuery(QStringLiteral("XyzCard"));
    h.selectByClick("resultsList", 0);
    QCOMPARE(h.prop("hasPreview").toBool(), true);

    QCOMPARE(h.child("atkValueText")->property("text").toString(), QStringLiteral("2000"));
    // An Xyz has an ordinary DEF stat, unlike a Link.
    QCOMPARE(h.child("defRowLabel")->property("visible").toBool(), true);
    QCOMPARE(h.child("defRowValue")->property("text").toString(), QStringLiteral("1500"));
    // The bug this pins: the Rank magnitude must never be labelled "Level".
    QCOMPARE(h.child("levelRankLabelText")->property("text").toString(), QStringLiteral("Rank"));
    QCOMPARE(h.child("levelRankValueText")->property("text").toString(), QStringLiteral("4"));
    QCOMPARE(h.child("linkMarkerRowLabel")->property("visible").toBool(), false);
}

void TestDeckBuilderScreen::linkMonsterPreviewHidesDefenseAndShowsLinkRatingAndMarkers() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // TYPE_MONSTER | TYPE_LINK (ocgcore/ocgapi_constants.h:33,58).
    constexpr quint32 kLinkType = 0x1 | 0x4000000;
    // LINK_MARKER_TOP_RIGHT | LINK_MARKER_LEFT | LINK_MARKER_BOTTOM_LEFT
    // (ocgcore/ocgapi_constants.h:197-204) - three deliberately non-
    // adjacent bits, chosen so a wrong iteration order (e.g. ascending bit
    // value instead of upstream's fixed positional order) would render
    // visibly different output. Written into the raw `def` column;
    // CardDatabase reinterprets it as `link_marker` for a Link-type row.
    constexpr qint32 kLinkMarkerBits = 0x100 | 0x8 | 0x1;
    const QString dbPath = writeSyntheticDatabaseWithFields(
        dir.filePath("cards.cdb"), QList<SyntheticCard>{SyntheticCard{
                                        333, QStringLiteral("LinkCard"), kLinkType, 2500, kLinkMarkerBits, 3}});

    Harness h;
    QVERIFY(h.valid());
    QVERIFY(h.catalog.loadDatabases({dbPath}));

    h.setSearchQuery(QStringLiteral("LinkCard"));
    h.selectByClick("resultsList", 0);
    QCOMPARE(h.prop("hasPreview").toBool(), true);

    QCOMPARE(h.child("atkValueText")->property("text").toString(), QStringLiteral("2500"));
    // No real DEF stat is presented for a Link - the whole row is hidden,
    // not shown with a fake value.
    QCOMPARE(h.child("defRowLabel")->property("visible").toBool(), false);
    QCOMPARE(h.child("defRowValue")->property("visible").toBool(), false);
    // The bug this pins: the Link Rating magnitude must never be labelled
    // "Level", and DEF must never masquerade as "<ATK> / LINK" text either.
    QCOMPARE(h.child("levelRankLabelText")->property("text").toString(),
             QStringLiteral("Link Rating"));
    QCOMPARE(h.child("levelRankValueText")->property("text").toString(), QStringLiteral("3"));
    // The actual marker arrows, in upstream's own fixed positional order.
    QCOMPARE(h.child("linkMarkerRowLabel")->property("visible").toBool(), true);
    QCOMPARE(h.child("linkMarkerValueText")->property("visible").toBool(), true);
    QCOMPARE(h.child("linkMarkerValueText")->property("text").toString(),
             QStringLiteral("[↗][←][↙]"));
}

QTEST_MAIN(TestDeckBuilderScreen)
#include "test_deckbuilder_screen.moc"
