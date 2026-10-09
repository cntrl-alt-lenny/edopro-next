// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

// Presentation only. Resolve installed families before any QML/control font
// is constructed; never send CSS generic aliases to the platform backend.
class PlatformFonts : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString proportional READ proportional CONSTANT)
    Q_PROPERTY(QString monospace READ monospace CONSTANT)
public:
    explicit PlatformFonts(QObject* parent = nullptr) : QObject(parent) {}
    static QString proportional();
    static QString monospace();
    static void initializeApplicationFont();
};
