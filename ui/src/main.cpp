// SPDX-License-Identifier: AGPL-3.0-or-later

#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

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

    // --capture <path>: render one frame, write a PNG, exit. Used to produce
    // documentation screenshots of the real UI rather than a mockup, and
    // usable as a CI visual smoke check. Not part of normal operation.
    const auto args = QGuiApplication::arguments();
    const auto captureIndex = args.indexOf(QStringLiteral("--capture"));
    if (captureIndex > 0 && captureIndex + 1 < args.size()) {
        const QString path = args.at(captureIndex + 1);
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        if (!window)
            return -1;
        // Let bindings settle and the scene graph produce a frame first.
        QTimer::singleShot(1200, window, [window, path]() {
            const QImage frame = window->grabWindow();
            if (frame.isNull() || !frame.save(path)) {
                qWarning("capture failed: %s", qPrintable(path));
                QGuiApplication::exit(1);
                return;
            }
            QGuiApplication::quit();
        });
    }

    return app.exec();
}
