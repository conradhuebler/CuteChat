// chatrecorder.h - Writes the audit trail of an agent session into a ChatLog.
// Copyright (C) 2016 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 (UI redesign WP11). Shared by the chat dock and a headless runner,
// so a headless run is as traceable as one in the GUI: every tool call with its arguments, effect,
// full result (capped), duration and approval decision (asked? approved/refused, by whom), and the
// ids of the contexts (projects, structures, ...) the calls named. Questions and answers are added by the caller, because
// the dock shows streamed answers before they are complete.
#pragma once

#include <cutechat/chatlog.h>
#include <cutechat/llmsession.h>

#include <QElapsedTimer>
#include <QObject>

#include <functional>

class ToolRegistry;

class ChatRecorder : public QObject {
public:
    ChatRecorder(LlmSession* session, const ToolRegistry* registry, QObject* parent = nullptr);

    /// Where entries go (may be null: nothing is recorded).
    void setLog(ChatLog* log) { m_log = log; m_pending = -1; }
    ChatLog* log() const { return m_log; }
    void setAutonomy(ToolAutonomy autonomy) { m_autonomy = autonomy; }
    /// Names of tool arguments that carry a context id (SupraFit: "project_id"); their values are
    /// added to ChatLog::contextIds() when a call names them. Default: none. Claude Generated.
    void setContextArguments(const QStringList& names) { m_contextArguments = names; }

    /// Wrap the session's approval policy so each decision lands in the log;
    /// @p decidedBy is "user" (a dialog) or "policy" (a headless rule).
    LlmSession::ApprovalFn wrapApproval(LlmSession::ApprovalFn inner, const QString& decidedBy);

    /// Called with the index of a tool entry when it was added / completed (view updates).
    std::function<void(int)> onToolAdded;
    std::function<void(int)> onToolChanged;

    static QString effectName(ToolEffect effect);

private:
    void started(const QString& name, const QJsonObject& args);
    void finished(const ToolResult& result);
    void refused(const QString& name, const QString& reason);
    void complete(ChatEntry* entry);

    LlmSession* m_session;
    const ToolRegistry* m_registry;
    ChatLog* m_log = nullptr;
    ToolAutonomy m_autonomy = ToolAutonomy::Ask;
    QStringList m_contextArguments;
    int m_pending = -1;
    QElapsedTimer m_timer;
};
