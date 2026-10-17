// chatbackend.h - Where chats bound to a context are stored.
// Copyright (C) 2016 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - A "context" is whatever an application binds a conversation to: a
// SupraFit project (uuid), a qurcuma structure/session, a document. The chat dock asks the host
// for a backend and loads/stores the chats of the current context through it. SupraFit stores them
// inside its project file (ProjectChatBackend); FileChatBackend is the default for applications
// without such a place. GUI-free, so it is testable headless.
#pragma once

#include <cutechat/chatlog.h>

#include <QString>
#include <QStringList>
#include <QVector>

class ChatBackend {
public:
    virtual ~ChatBackend() = default;

    /// Chats of @p contextId, newest order not guaranteed; damaged ones are skipped with a line
    /// in @p warnings.
    virtual QVector<ChatLog> load(const QString& contextId, QStringList* warnings = nullptr) = 0;
    /// Insert @p chat, or replace the stored chat with the same id. Empty chats are not stored.
    virtual void store(const QString& contextId, const ChatLog& chat) = 0;
    /// Remove every chat of @p contextId.
    virtual void clear(const QString& contextId) = 0;
    /// Can chats be stored for @p contextId right now (SupraFit: the project is still loaded)?
    virtual bool accepts(const QString& contextId) const { return !contextId.isEmpty(); }
};

/// One JSON file per chat: `<directory>/<context hash>/<chat id>.json`. The context id is hashed
/// (it may be a path or contain characters a file name cannot). Default directory:
/// `<AppDataLocation>/context-chats`.
class FileChatBackend : public ChatBackend {
public:
    explicit FileChatBackend(const QString& directory = defaultDirectory());
    static QString defaultDirectory();
    QString directory() const { return m_directory; }
    /// The folder holding the chats of @p contextId.
    QString contextDirectory(const QString& contextId) const;

    QVector<ChatLog> load(const QString& contextId, QStringList* warnings = nullptr) override;
    void store(const QString& contextId, const ChatLog& chat) override;
    void clear(const QString& contextId) override;

private:
    QString m_directory;
};
