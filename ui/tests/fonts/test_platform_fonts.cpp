// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform_fonts.h"
#include "deckbuilder/card_catalog.h"
#include "deckbuilder/deck_controller.h"
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTemporaryDir>
#include <sqlite3.h>
#include <QFontDatabase>
#include <QFontInfo>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QQuickItem>
#include <QtTest>
#include <memory>

class PlatformFontTest : public QObject {
    Q_OBJECT
private slots:
    void realShellTypography_data() {
        QTest::addColumn<QSize>("size");
        QTest::newRow("minimum") << QSize(960, 600);
        QTest::newRow("default") << QSize(1280, 800);
    }
    void realShellTypography() {
        QFETCH(QSize, size);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString dbPath = directory.filePath(QStringLiteral("synthetic.cdb"));
        sqlite3* db = nullptr;
        QCOMPARE(sqlite3_open(qPrintable(dbPath), &db), SQLITE_OK);
        const char* sql =
            "CREATE TABLE datas (id INTEGER PRIMARY KEY,ot INTEGER,alias INTEGER,setcode INTEGER,"
            "type INTEGER,atk INTEGER,def INTEGER,level INTEGER,race INTEGER,attribute INTEGER,category INTEGER);"
            "CREATE TABLE texts (id INTEGER PRIMARY KEY,name TEXT,desc TEXT,str1 TEXT,str2 TEXT,str3 TEXT,"
            "str4 TEXT,str5 TEXT,str6 TEXT,str7 TEXT,str8 TEXT,str9 TEXT,str10 TEXT,str11 TEXT,str12 TEXT,"
            "str13 TEXT,str14 TEXT,str15 TEXT,str16 TEXT);"
            "INSERT INTO datas VALUES (1,3,0,0,33,1500,1200,4,1,16,0);"
            "INSERT INTO texts (id,name,desc) VALUES (1,'Synthetic long title for font wrapping and elision',"
            "'Synthetic visual fixture. A long description checks wrapping in the real card preview. "
            "The body must remain readable at both window sizes without changing the typography hierarchy. "
            "Numbers: 0123456789. Punctuation: commas, brackets (like these), and ordinary effect text.');";
        const int status = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
        sqlite3_close(db);
        QCOMPARE(status, SQLITE_OK);
        CardCatalog catalog;
        catalog.loadDatabases({dbPath});
        DeckController controller;
        controller.setCatalog(&catalog);
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("cardCatalog"), &catalog);
        engine.rootContext()->setContextProperty(QStringLiteral("deckController"), &controller);
        engine.rootContext()->setContextProperty(QStringLiteral("startScreenIndex"), 1);
        engine.loadFromModule("EdoproNext", "Main");
        QVERIFY(!engine.rootObjects().isEmpty());
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        window->resize(size);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* results = window->findChild<QObject*>(QStringLiteral("resultsList"));
        QVERIFY(results);
        QTRY_VERIFY(results->property("count").toInt() > 0);
        QVERIFY(results->setProperty("currentIndex", 0));
        auto* field = window->findChild<QQuickItem*>(QStringLiteral("searchField"));
        QVERIFY(field);
        field->forceActiveFocus();
        QTRY_VERIFY(field->hasActiveFocus());
        QCOMPARE(field->property("font").value<QFont>().family(), PlatformFonts::proportional());
        const auto capture = [&](const QString& state) {
            const QString path = qEnvironmentVariable("EDOPRO_FOCUS_CAPTURES");
            if (!path.isEmpty()) {
                QTest::qWait(100);
                QVERIFY(window->grabWindow().save(QStringLiteral("%1/font-shell-%2-%3.png")
                    .arg(path).arg(size.width()).arg(state)));
            }
        };
        capture(QStringLiteral("preview-focus"));
        auto* popup = window->findChild<QObject*>(QStringLiteral("categoriesPopup"));
        QVERIFY(popup);
        QVERIFY(QMetaObject::invokeMethod(popup, "open"));
        QTRY_VERIFY(popup->property("visible").toBool());
        capture(QStringLiteral("popup"));
        QVERIFY(QMetaObject::invokeMethod(popup, "close"));
    }
    void qmlFamilyIsOneName() {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
import QtQuick
QtObject { property font sample: Qt.font({family: "Inter, Segoe UI, Noto Sans, DejaVu Sans, sans-serif"}) }
)", QUrl());
        std::unique_ptr<QObject> text(component.create());
        QVERIFY2(text, qPrintable(component.errorString()));
        const auto font = text->property("sample").value<QFont>();
        QCOMPARE(font.families(), QStringList{
            QStringLiteral("Inter, Segoe UI, Noto Sans, DejaVu Sans, sans-serif")});
    }
    void ordinaryControlsUseInstalledFonts() {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
import QtQuick
import QtQuick.Controls
import EdoproNext
Window {
    visible: true; width: 640; height: 480
    Column {
        Text { objectName: "plain"; text: "Ordinary text" }
        Text { objectName: "themed"; text: "Theme body"; font.family: Theme.fontFamily; font.pointSize: Theme.textBody }
        Text { objectName: "mono"; text: "0123456789 Wi"; font.family: Theme.fontFamilyMono }
        TextField { objectName: "input"; text: "Search cards" }
        Button { objectName: "button"; text: "Open deck" }
        ComboBox { objectName: "combo"; model: ["All cards", "Effect monsters", "Spell cards"] }
    }
}
)", QUrl());
        std::unique_ptr<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));
        QVERIFY(QTest::qWaitForWindowExposed(qobject_cast<QQuickWindow*>(window.get())));
        for (const auto* name : {"plain", "themed", "mono", "input", "button", "combo"}) {
            auto* item = window->findChild<QObject*>(QString::fromLatin1(name));
            QVERIFY(item);
            const auto font = item->property("font").value<QFont>();
            const QString expected = QString::fromLatin1(name) == QStringLiteral("mono")
                ? PlatformFonts::monospace() : PlatformFonts::proportional();
            QCOMPARE(font.family(), expected);
            QCOMPARE(QFontInfo(font).family(), expected);
        }
        auto* combo = window->findChild<QObject*>(QStringLiteral("combo"));
        auto* popup = combo->property("popup").value<QObject*>();
        QVERIFY(popup);
        QVERIFY(QMetaObject::invokeMethod(popup, "open"));
        QTRY_VERIFY(popup->property("visible").toBool());
        auto* content = popup->property("contentItem").value<QQuickItem*>();
        QVERIFY(content);
        QTRY_VERIFY(!content->childItems().isEmpty());
        QTest::qWait(50);
        QList<QQuickItem*> delegates = content->childItems();
        bool checkedDelegate = false;
        while (!delegates.isEmpty()) {
            auto* object = delegates.takeFirst();
            delegates.append(object->childItems());
            if (!object->inherits("QQuickItemDelegate"))
                continue;
            const auto font = object->property("font").value<QFont>();
            QCOMPARE(font.family(), PlatformFonts::proportional());
            checkedDelegate = true;
        }
        QVERIFY(checkedDelegate);
    }
    void themeFamiliesAreAvailable() {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
import QtQuick
import EdoproNext
QtObject {
    property string proportional: Theme.fontFamily
    property string monospace: Theme.fontFamilyMono
}
)", QUrl());
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        const auto families = QFontDatabase::families();
        const QString proportional = object->property("proportional").toString();
        const QString monospace = object->property("monospace").toString();
        qInfo() << "Theme families:" << proportional << monospace;
        QVERIFY2(families.contains(proportional), qPrintable(proportional));
        QVERIFY2(families.contains(monospace), qPrintable(monospace));
        QVERIFY(!QFontInfo(QFont(proportional)).fixedPitch());
        QVERIFY(QFontInfo(QFont(monospace)).fixedPitch());
    }
};
int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    PlatformFonts::initializeApplicationFont();
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    PlatformFontTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "test_platform_fonts.moc"
