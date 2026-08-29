// SPDX-License-Identifier: AGPL-3.0-or-later

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

#include "deckbuilder/card_catalog.h"
#include "deckbuilder/deck_controller.h"

namespace {

// Maps a --start-screen name to Main.qml's StackLayout index. Purely a
// launch/visual-verification convenience (docs/architecture/
// deck-builder-ui.md#keyboard-and-launch-affordances) - normal navigation
// always starts on Home and uses the nav rail/Ctrl+<N> shortcuts already in
// Main.qml; this never becomes a routing system of its own.
int screenIndexForName(const QString& name) {
    if (name == QStringLiteral("decks"))
        return 1;
    if (name == QStringLiteral("duel"))
        return 2;
    if (name == QStringLiteral("replays"))
        return 3;
    if (name == QStringLiteral("settings"))
        return 4;
    return 0; // home, and any unrecognized name
}

} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    QGuiApplication::setApplicationName(QStringLiteral("edopro-next"));
    QGuiApplication::setOrganizationName(QStringLiteral("edopro-next"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("edopro-next Qt/QML shell"));
    parser.addHelpOption();

    // Repeatable: each path is loaded in order via CardDatabase::
    // load_database(), matching upstream's own base-then-overlay,
    // last-file-wins precedence (docs/architecture/deck-builder-ui.md
    // #bootstrap). No path supplied is a supported, honest state - the
    // Decks screen shows an explicit "no card database loaded" empty state
    // rather than any fabricated data (CLAUDE.md).
    QCommandLineOption cardDbOption(
        QStringLiteral("card-db"),
        QStringLiteral("Path to a Project Ignis-compatible .cdb card database. Repeatable; "
                        "later files override earlier ones for the same card code."),
        QStringLiteral("path"));
    parser.addOption(cardDbOption);

    QCommandLineOption startScreenOption(
        QStringLiteral("start-screen"),
        QStringLiteral("Show this screen on launch instead of Home: home, decks, duel, "
                        "replays, settings. For visual verification only."),
        QStringLiteral("name"), QStringLiteral("home"));
    parser.addOption(startScreenOption);

    // --capture <path>: render one frame, write a PNG, exit. Used to
    // produce documentation screenshots of the real UI rather than a
    // mockup, and usable as a CI visual smoke check. Not part of normal
    // operation.
    QCommandLineOption captureOption(
        QStringLiteral("capture"),
        QStringLiteral("Render one frame to <path> as PNG, then exit."), QStringLiteral("path"));
    parser.addOption(captureOption);

    // Resizes the window before capturing - only meaningful together with
    // --capture, so a layout can be visually verified at a size other than
    // the default 1280x800 (e.g. the 960x600 minimum) without a second,
    // separate window-sizing feature for ordinary use.
    QCommandLineOption captureWidthOption(QStringLiteral("capture-width"),
                                           QStringLiteral("Window width to use with --capture."),
                                           QStringLiteral("pixels"));
    parser.addOption(captureWidthOption);
    QCommandLineOption captureHeightOption(QStringLiteral("capture-height"),
                                            QStringLiteral("Window height to use with --capture."),
                                            QStringLiteral("pixels"));
    parser.addOption(captureHeightOption);

    parser.process(app);

    // Basic style: the design system is ours, not the platform's. Using the
    // platform style here would fight the token system.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    CardCatalog catalog;
    DeckController deckController;
    deckController.setCatalog(&catalog);

    const QStringList cardDbPaths = parser.values(cardDbOption);
    if (!cardDbPaths.isEmpty())
        catalog.loadDatabases(cardDbPaths);

    QQmlApplicationEngine engine;
    // Lowercase, deliberately distinct from the QML_ELEMENT type names
    // (CardCatalog/DeckController) those classes also register under -
    // QML resolves Type.EnumValue (e.g. DeckController.Main) through the
    // type/import system regardless of an instance name in scope, and
    // keeping the names visibly different avoids ever having to reason
    // about which resolves first.
    engine.rootContext()->setContextProperty(QStringLiteral("cardCatalog"), &catalog);
    engine.rootContext()->setContextProperty(QStringLiteral("deckController"), &deckController);
    engine.rootContext()->setContextProperty(
        QStringLiteral("startScreenIndex"), screenIndexForName(parser.value(startScreenOption)));

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("EdoproNext", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;

    if (parser.isSet(captureOption)) {
        const QString path = parser.value(captureOption);
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        if (!window)
            return -1;
        if (parser.isSet(captureWidthOption) && parser.isSet(captureHeightOption)) {
            window->resize(parser.value(captureWidthOption).toInt(),
                            parser.value(captureHeightOption).toInt());
        }
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
