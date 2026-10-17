// sessionkeys.h - API keys typed by the user, kept in memory only.
// Copyright (C) 2016 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - The endpoint manager lets a key be typed for the running session; it is
// never written to llm.json or QSettings. Moved here from SupraFit's endpoint dialog so the core
// library (and a headless client) can use the same lookup.
#pragma once

#include <cutechat/llmconfig.h>

#include <QString>

namespace SessionKeys {
/// Key typed for @p profileName in this process (empty if none).
QString key(const QString& profileName);
/// Remember @p key for @p profileName (empty forgets it).
void setKey(const QString& profileName, const QString& key);
/// The key of @p profile: the environment variable it names, else the session key.
QString effectiveKey(const LlmProfile& profile);
} // namespace SessionKeys
