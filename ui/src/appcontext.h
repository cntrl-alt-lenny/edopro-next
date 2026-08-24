// SPDX-License-Identifier: AGPL-3.0-or-later
//
// AppContext is the seam between the C++ application and the QML presentation
// layer. It deliberately exposes only facts the application actually knows.
//
// It must never grow game-rules knowledge. When the semantic duel model exists,
// it will be exposed through its own objects, not bolted on here.

#pragma once

#include <QObject>
#include <QString>
#include <qqmlintegration.h>

class AppContext : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString gitSha READ gitSha CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(QString platform READ platform CONSTANT)

public:
    explicit AppContext(QObject* parent = nullptr);

    QString appVersion() const;
    QString gitSha() const;
    QString qtVersion() const;
    QString platform() const;
};
