// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform_fonts.h"

#include <QFontDatabase>
#include <QGuiApplication>

namespace {
QString installedFamily(const QStringList& preferred, bool fixedPitch) {
    const auto available = QFontDatabase::families(QFontDatabase::Latin);
    const auto suitable = [fixedPitch](const QString& family) {
        return QFontDatabase::isFixedPitch(family) == fixedPitch;
    };
    for (const auto& candidate : preferred) {
        for (const auto& family : available) {
            if (family.compare(candidate, Qt::CaseInsensitive) == 0 && suitable(family))
                return family;
        }
    }
    // A platform's system family is useful only if it is a real installed
    // family, not an alias such as the offscreen backend's "Sans Serif".
    const auto system = QFontDatabase::systemFont(fixedPitch
        ? QFontDatabase::FixedFont : QFontDatabase::GeneralFont).family();
    if (available.contains(system) && suitable(system))
        return system;
    for (const auto& family : available) {
        if (!QFontDatabase::isPrivateFamily(family) && suitable(family))
            return family;
    }
    qFatal("No installed Latin %s font family is available", fixedPitch ? "monospace" : "proportional");
}
} // namespace

QString PlatformFonts::proportional() {
    static const QString family = installedFamily({QStringLiteral("Inter"),
        QStringLiteral("Segoe UI"), QStringLiteral("Noto Sans"),
        QStringLiteral("DejaVu Sans"), QStringLiteral("Helvetica"),
        QStringLiteral("Arial"), QStringLiteral("Liberation Sans")}, false);
    return family;
}

QString PlatformFonts::monospace() {
    static const QString family = installedFamily({QStringLiteral("JetBrains Mono"),
        QStringLiteral("Cascadia Mono"), QStringLiteral("DejaVu Sans Mono"),
        QStringLiteral("Menlo"), QStringLiteral("Consolas"),
        QStringLiteral("Liberation Mono")}, true);
    return family;
}

void PlatformFonts::initializeApplicationFont() {
    auto font = QGuiApplication::font();
    font.setFamily(proportional());
    QGuiApplication::setFont(font);
}
