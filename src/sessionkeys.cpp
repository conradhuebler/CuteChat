// sessionkeys.cpp - see sessionkeys.h
// Copyright (C) 2016 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026
#include <cutechat/sessionkeys.h>

#include <QHash>

namespace SessionKeys {
namespace {
    QHash<QString, QString>& store()
    {
        static QHash<QString, QString> keys;
        return keys;
    }
}

QString key(const QString& profileName)
{
    return store().value(profileName);
}

void setKey(const QString& profileName, const QString& key)
{
    if (key.isEmpty())
        store().remove(profileName);
    else
        store().insert(profileName, key);
}

QString effectiveKey(const LlmProfile& profile)
{
    const QString env = LlmConfig::apiKeyFor(profile);
    return env.isEmpty() ? key(profile.name) : env;
}
} // namespace SessionKeys
