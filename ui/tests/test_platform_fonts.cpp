// SPDX-License-Identifier: AGPL-3.0-or-later
#include <QFontDatabase>
#include <QFontInfo>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QtTest>
#include <memory>

class PlatformFontTest : public QObject {
    Q_OBJECT
private slots:
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
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    PlatformFontTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "test_platform_fonts.moc"
