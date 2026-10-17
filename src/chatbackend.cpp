// chatbackend.cpp - see chatbackend.h
// Copyright (C) 2016 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026
#include <cutechat/chatbackend.h>

#include <QCryptographicHash>
#include <QDir>
#include <QStandardPaths>

FileChatBackend::FileChatBackend(const QString& directory)
    : m_directory(directory)
{
}

QString FileChatBackend::defaultDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/context-chats");
}

QString FileChatBackend::contextDirectory(const QString& contextId) const
{
    const QByteArray hash = QCryptographicHash::hash(contextId.toUtf8(), QCryptographicHash::Sha1).toHex();
    return m_directory + QLatin1Char('/') + QString::fromLatin1(hash.left(16));
}

QVector<ChatLog> FileChatBackend::load(const QString& contextId, QStringList* warnings)
{
    QVector<ChatLog> chats;
    if (contextId.isEmpty())
        return chats;
    const QDir dir(contextDirectory(contextId));
    for (const QString& name : dir.entryList(QStringList { QStringLiteral("*.json") }, QDir::Files)) {
        ChatLog log;
        QString error;
        if (ChatLog::load(dir.filePath(name), &log, &error))
            chats.append(log);
        else if (warnings)
            warnings->append(QObject::tr("Stored chat %1 was skipped: %2").arg(name, error));
    }
    return chats;
}

void FileChatBackend::store(const QString& contextId, const ChatLog& chat)
{
    if (contextId.isEmpty() || chat.isEmpty())
        return;
    QDir().mkpath(contextDirectory(contextId));
    chat.save(contextDirectory(contextId) + QLatin1Char('/') + chat.id() + QStringLiteral(".json"));
}

void FileChatBackend::clear(const QString& contextId)
{
    if (contextId.isEmpty())
        return;
    QDir(contextDirectory(contextId)).removeRecursively();
}
