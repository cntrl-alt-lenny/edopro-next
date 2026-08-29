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

QString writeSyntheticDatabase(const QString& path, quint32 code, const QString& name) {
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
    run(db, qPrintable(QStringLiteral("INSERT INTO datas (id,ot,alias,setcode,type,atk,def,"
                                       "level,race,attribute,category) VALUES (%1,0,0,0,1,"
                                       "1000,1000,4,0,0,0);")
                            .arg(code)));
    run(db, qPrintable(QStringLiteral("INSERT INTO texts (id,name,desc) VALUES (%1,'%2','synthetic text');")
                            .arg(code)
                            .arg(name)));
    sqlite3_close(db);
    return path;
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

QTEST_MAIN(TestDeckBuilderScreen)
#include "test_deckbuilder_screen.moc"
