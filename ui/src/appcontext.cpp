// SPDX-License-Identifier: AGPL-3.0-or-later

#include "appcontext.h"

#include <QSysInfo>
#include <QtGlobal>

#ifndef EDOPRO_NEXT_GIT_SHA
#define EDOPRO_NEXT_GIT_SHA "unknown"
#endif
#ifndef EDOPRO_NEXT_VERSION
#define EDOPRO_NEXT_VERSION "0.0.0"
#endif

AppContext::AppContext(QObject* parent) : QObject(parent) {}

QString AppContext::appVersion() const { return QStringLiteral(EDOPRO_NEXT_VERSION); }

QString AppContext::gitSha() const { return QStringLiteral(EDOPRO_NEXT_GIT_SHA); }

QString AppContext::qtVersion() const { return QStringLiteral(QT_VERSION_STR); }

QString AppContext::platform() const {
    return QSysInfo::prettyProductName() + QStringLiteral(" (") +
           QSysInfo::currentCpuArchitecture() + QStringLiteral(")");
}
