// SPDX-License-Identifier: AGPL-3.0-or-later
// Disposable round 026 observer. Never included by the shipped application.
#include <QStyleHints>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QDebug>
#include <QKeyEvent>
class Round026KeyObserver final : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject* object, QEvent* e) override {
        if (e->type() == QEvent::KeyPress) {
            auto* k=static_cast<QKeyEvent*>(e);
            auto* window=qobject_cast<QQuickWindow*>(object);
            qInfo() << "DIAG key-window active" << (window && window->isActive());
            qInfo() << "DIAG delivered key" << k->key() << "modifiers" << k->modifiers() << "spontaneous" << k->spontaneous();
        }
        return false;
    }
};
inline void observeFocus(QQmlApplicationEngine& engine) {
    auto* w = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    if (!w) return;
    w->installEventFilter(new Round026KeyObserver(w));
    const int width = qEnvironmentVariableIntValue("EDOPRO_DIAG_WIDTH");
    if (width) w->resize(width, width == 960 ? 600 : 800);
    const auto logFocus = [w] {
        auto* i=w->activeFocusItem();
        if (!i) { qInfo() << "DIAG focus null"; return; }
        const QRectF r=i->mapRectToScene(QRectF(0,0,i->width(),i->height()));
        QRectF viewport(0,0,w->width(),w->height());
        for(auto* p=i->parentItem();p;p=p->parentItem())
            if(p->clip()) viewport=viewport.intersected(p->mapRectToScene(QRectF(0,0,p->width(),p->height())));
        qInfo() << "DIAG focus" << i->objectName() << i->metaObject()->className()
                << "bounds" << r << "viewport" << viewport
                << "visible" << (i->isVisible() && viewport.adjusted(-0.5,-0.5,0.5,0.5).contains(r))
                << "reason" << i->property("focusReason");
    };
    QObject::connect(w, &QQuickWindow::activeFocusItemChanged, w, [w, logFocus] {
        logFocus();
        QTimer::singleShot(60, w, logFocus);
    });
    QTimer::singleShot(1500,w,[w] {
        qInfo() << "DIAG runtime Qt" << qVersion() << "platform" << QGuiApplication::platformName()
                << "style" << QQuickStyle::name() << "active" << w->isActive()
                << "size" << w->size() << "policy" << QGuiApplication::styleHints()->tabFocusBehavior();
    });
}
