// SPDX-License-Identifier: AGPL-3.0-or-later

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    QGuiApplication::setApplicationName(QStringLiteral("edopro-next"));
    QGuiApplication::setOrganizationName(QStringLiteral("edopro-next"));

    // Basic style: the design system is ours, not the platform's. Using the
    // platform style here would fight the token system.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("EdoproNext", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
