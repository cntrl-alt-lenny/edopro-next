// SPDX-License-Identifier: AGPL-3.0-or-later
#include <QAbstractItemModelTester>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <sqlite3.h>

#include "card_catalog.h"
#include "deck_controller.h"
#include "search_results_model.h"

namespace {
void sql(sqlite3* db, const QString& statement) {
    if (sqlite3_exec(db, qPrintable(statement), nullptr, nullptr, nullptr) != SQLITE_OK)
        qFatal("synthetic catalogue setup failed: %s", sqlite3_errmsg(db));
}
QString fixture(const QString& path, bool includeTarget = true) {
    sqlite3* db = nullptr;
    if (sqlite3_open(qPrintable(path), &db) != SQLITE_OK)
        qFatal("cannot create synthetic catalogue");
    sql(db, "CREATE TABLE datas (id INTEGER PRIMARY KEY, ot INTEGER, alias INTEGER, setcode INTEGER, "
            "type INTEGER, atk INTEGER, def INTEGER, level INTEGER, race INTEGER, attribute INTEGER, category INTEGER)");
    sql(db, "CREATE TABLE texts (id INTEGER PRIMARY KEY, name TEXT, desc TEXT, "
            "str1 TEXT, str2 TEXT, str3 TEXT, str4 TEXT, str5 TEXT, str6 TEXT, str7 TEXT, str8 TEXT, "
            "str9 TEXT, str10 TEXT, str11 TEXT, str12 TEXT, str13 TEXT, str14 TEXT, str15 TEXT, str16 TEXT)");
    const auto add = [db](quint32 code, const QString& name, quint32 type = 1, quint32 scope = 3) {
        sql(db, QString("INSERT INTO datas VALUES (%1,%2,0,0,%3,1000,1000,4,1,1,1)").arg(code).arg(scope).arg(type));
        sql(db, QString("INSERT INTO texts (id,name,desc) VALUES (%1,'%2','synthetic description')").arg(code).arg(name));
    };
    if (includeTarget)
        add(123, "Alpha");
    add(4294967295u, "Maximum");
    add(500, "Anime", 1, 0x8);
    add(501, "Hidden", 1, 0x1000);
    add(502, "Token", 0x4001);
    // Numeric-looking text must remain searchable when input is not a known code.
    add(600, "0 999 4294967296 +123 -123 12x 12 3 123x １２３ ١٢٣ 4294967419");
    for (quint32 code = 1000; code < 1210; ++code)
        add(code, QString("Common"));
    sqlite3_close(db);
    return path;
}
QList<quint32> codes(const SearchResultsModel& model) {
    QList<quint32> out;
    for (int row = 0; row < model.rowCount(); ++row)
        out.push_back(model.cardCodeAt(row));
    return out;
}
int choice(const QStringList& choices, const QString& label) {
    const auto index = choices.indexOf(label);
    if (index < 0)
        qFatal("missing filter choice: %s", qPrintable(label));
    return static_cast<int>(index);
}
}

class TestCardCodeSearch : public QObject {
    Q_OBJECT
private slots:
    void knownCodesAndClearing() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        CardCatalog catalog;
        QVERIFY(catalog.loadDatabases({fixture(dir.filePath("cards.cdb"))}));
        SearchResultsModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setCatalog(&catalog);
        for (const QString& input : {QString("123"), QString(" \t00123\n"), QString(100, '0') + "123"}) {
            model.setQueryText(input);
            QCOMPARE(codes(model), QList<quint32>{123});
            QCOMPARE(model.data(model.index(0), SearchResultsModel::MatchKindRole).toInt(),
                     static_cast<int>(edopro_next::data::MatchKind::ExactCode));
            QCOMPARE(model.queryText(), input);
        }
        model.setQueryText("4294967295");
        QCOMPARE(codes(model), QList<quint32>{4294967295u});
        model.setQueryText("");
        QCOMPARE(model.resultCount(), 200);
        model.setQueryText("Common");
        QCOMPARE(model.resultCount(), 200);
        model.setQueryText("Alpha");
        QCOMPARE(codes(model), QList<quint32>{123});
    }
    void fallback_data() {
        QTest::addColumn<QString>("input");
        for (const QString& input : {QString("0"), QString("999"), QString("4294967296"),
                                    QString("4294967419"), QString("+123"), QString("-123"),
                                    QString("12x"), QString("12 3"), QString("123x"),
                                    QString("１２３"), QString("١٢٣"), QString(" 999 ")})
            QTest::newRow(qPrintable(input)) << input;
    }
    void fallback() {
        QFETCH(QString, input);
        QTemporaryDir dir;
        CardCatalog catalog;
        QVERIFY(catalog.loadDatabases({fixture(dir.filePath("cards.cdb"))}));
        SearchResultsModel model;
        model.setCatalog(&catalog);
        model.setQueryText(input);
        QCOMPARE(codes(model), QList<quint32>{600});
        QVERIFY(model.data(model.index(0), SearchResultsModel::MatchKindRole).toInt() !=
                static_cast<int>(edopro_next::data::MatchKind::ExactCode));
    }
    void filtersAndVisibility() {
        QTemporaryDir dir;
        CardCatalog catalog;
        QVERIFY(catalog.loadDatabases({fixture(dir.filePath("cards.cdb"))}));
        SearchResultsModel model;
        model.setCatalog(&catalog);
        model.setQueryText("123");
        model.setCardType(choice(model.cardTypeNames(), "Spell"));
        QCOMPARE(model.resultCount(), 0);
        model.setCardType(choice(model.cardTypeNames(), "Monster"));
        QCOMPARE(codes(model), QList<quint32>{123});
        model.setAttackText(">1000");
        QCOMPARE(model.resultCount(), 0);
        model.setAttackText("1000");
        QCOMPARE(codes(model), QList<quint32>{123});
        model.setAttribute(2); // target attribute is 1
        QCOMPARE(model.resultCount(), 0);
        model.clearFilters();
        model.setCategorySelected(1, true); // target category is bit 0
        QCOMPARE(model.resultCount(), 0);
        model.clearFilters();
        QCOMPARE(codes(model), QList<quint32>{123});
        model.setQueryText("500");
        QCOMPARE(model.resultCount(), 0);
        model.setShowNonOfficial(true);
        QCOMPARE(codes(model), QList<quint32>{500});
        for (const QString& input : {QString("501"), QString("502")}) {
            model.setQueryText(input);
            QCOMPARE(model.resultCount(), 0);
        }
        model.setShowNonOfficial(false);
        DeckController controller;
        controller.setCatalog(&catalog);
        model.setDeckController(&controller);
        controller.loadBanlistFromText("!Black\n123 0\n!White\n$whitelist\n600 3\n");
        controller.setSelectedBanlistIndex(choice(controller.banlistNames(), "Black"));
        model.setQueryText("123");
        model.setLimitation(choice(model.limitationNames(), "Unlimited"));
        QCOMPARE(model.resultCount(), 0);
        model.setLimitation(choice(model.limitationNames(), "Banned"));
        QCOMPARE(codes(model), QList<quint32>{123});
        model.setLimitation(0);
        controller.setSelectedBanlistIndex(choice(controller.banlistNames(), "White"));
        QCOMPARE(model.resultCount(), 0);
        model.setLimitation(choice(model.limitationNames(), "All cards"));
        QCOMPARE(codes(model), QList<quint32>{123});
        controller.loadBanlistFromText("!Replacement\n$whitelist\n123 3\n");
        model.setLimitation(0);
        QCOMPARE(codes(model), QList<quint32>{123});
    }
    void reloadReevaluatesMode() {
        QTemporaryDir dir;
        CardCatalog catalog;
        SearchResultsModel model;
        model.setQueryText("123");
        model.setCatalog(&catalog);
        QCOMPARE(model.resultCount(), 0);
        QVERIFY(catalog.loadDatabases({fixture(dir.filePath("unknown.cdb"), false)}));
        QCOMPARE(codes(model), QList<quint32>{600});
        QVERIFY(catalog.loadDatabases({fixture(dir.filePath("known.cdb"))}));
        QCOMPARE(codes(model), QList<quint32>{123});
        QVERIFY(catalog.loadDatabases({dir.filePath("unknown.cdb")}));
        QCOMPARE(codes(model), QList<quint32>{600});
    }
};
QTEST_GUILESS_MAIN(TestCardCodeSearch)
#include "test_card_code_search.moc"
