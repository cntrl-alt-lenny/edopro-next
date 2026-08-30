// SPDX-License-Identifier: AGPL-3.0-or-later
//
// QString -> std::filesystem::path, portably. QString::toStdString() is
// always UTF-8, regardless of platform; std::filesystem::path's own
// std::u8string constructor is the one overload the standard guarantees is
// interpreted as UTF-8 on every platform (notably including Windows, where
// the plain std::string/const char* constructors instead assume the active
// code page). Constructing a std::u8string from those same UTF-8 bytes and
// handing that to std::filesystem::path avoids depending on any process-wide
// codepage state - this project's baseline is Linux-only today
// (docs/BASELINE.md), but a path helper has no reason to be Linux-only too.

#pragma once

#include <QString>
#include <filesystem>
#include <string>

inline std::filesystem::path to_fs_path(const QString& path) {
    const std::string utf8 = path.toStdString();
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}
