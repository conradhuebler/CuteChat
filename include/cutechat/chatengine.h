// chatengine.h - Owns the LLM client and the agent session, optionally on a thread of their own.
// Copyright (C) 2016 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - The chat dock used to build its LlmClient and LlmSession itself and call
// them directly, which ties the agent loop to the GUI thread. An application whose tools wait
// (a calculation, a simulation reaching a state) cannot live with that: the wait would block the
// very thread that has to deliver the event it waits for. qurcuma therefore ran client and session
// on a worker thread and kept its own dock.
//
// ChatEngine is the one place that knows about that choice. It is an object of the GUI thread with
// the same vocabulary as the session and the client; each call is either made directly (no thread)
// or posted to the worker (thread), and each signal arrives on the GUI thread either way. Values
// the dock has to read at once (profile, model, busy flag) are mirrored here, because a getter that
// had to cross the thread boundary would block the GUI.
//
// Rules for the threaded mode:
//  * the ToolRegistry and ToolDispatcher stay owned by the application (GUI thread); the dispatcher
//    already marshals Gui-affinity tools there,
//  * the approval policy is asked on the GUI thread (the session blocks until it answers), so a
//    dialog and the audit recorder never run on the worker,
//  * conversation() is the only getter that waits for the worker, and it waits in an event loop.
#pragma once

#include <cutechat/llmclient.h>
#include <cutechat/llmconfig.h>
#include <cutechat/llmsession.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

class QThread;
class ToolDispatcher;
class ToolRegistry;

class ChatEngine : public QObject {
    Q_OBJECT

public:
    /// @p registry and @p dispatcher are borrowed and must outlive the engine. With @p ownThread
    /// the client and the session run on a thread the engine creates and joins in its destructor.
    ChatEngine(ToolRegistry* registry, ToolDispatcher* dispatcher, bool ownThread = false,
               QObject* parent = nullptr);
    ~ChatEngine() override;

    bool isThreaded() const { return m_thread != nullptr; }

    /// The objects themselves. Without a thread they are plain children of the engine and may be
    /// called directly; with one they belong to the worker and must only be connected to, never
    /// called. Applications that need to reach the session's signals (a recorder) use these.
    LlmClient* client() const { return m_client; }
    LlmSession* session() const { return m_session; }

    // ---- endpoint ----
    void setProfile(const LlmProfile& profile);
    LlmProfile profile() const { return m_profile; }
    void setApiKey(const QString& key);
    /// The model override of the current profile; empty = the profile's own model (as LlmClient).
    void setModel(const QString& model);
    QString model() const { return m_model.isEmpty() ? m_profile.model : m_model; }
    void setReasoningEffort(const QString& effort);
    QString reasoningEffort() const { return m_reasoningEffort; }
    void setSortModels(bool sorted);
    void listModels();
    void queryCapabilities(const QStringList& models);
    void describeModel(const QString& id);

    // ---- session ----
    void setSystemPrompt(const QString& prompt);
    void setMaxIterations(int iterations);
    int maxIterations() const { return m_maxIterations; }
    void setAutonomy(ToolAutonomy autonomy);
    ToolAutonomy autonomy() const { return m_autonomy; }
    void setVisionCapable(bool capable);
    void setKeepHistory(bool keep);
    /// Asked, on the GUI thread, for every tool the autonomy does not let run unasked.
    void setApprovalPolicy(LlmSession::ApprovalFn approve);

    void ask(const QString& userMessage);
    void cancel();
    void reset();
    bool isBusy() const { return m_busy; }
    /// Asks the worker when threaded. Waits in a local event loop, so a worker that is itself waiting
    /// for the GUI thread (a tool, an approval) is still served.
    QJsonArray conversation() const;
    void setConversation(const QJsonArray& messages);

signals:
    // from the session
    void assistantMessage(const QString& text);
    void assistantChunk(const QString& text);
    void reasoningChunk(const QString& text);
    void toolStarted(const QString& name, const QJsonObject& args);
    void toolFinished(const QString& name, const ToolResult& result);
    void toolRefused(const QString& name, const QString& reason);
    void finished();
    void failed(const QString& error);
    void busyChanged(bool busy);
    void noted(int level, const QString& text);
    // from the client
    void modelsListed(const QStringList& models);
    void modelListFailed(const QString& error);
    void modelInfo(const LlmModelInfo& info);

private:
    /// Run @p fn on the thread the session lives on: directly, or posted.
    template <typename Fn>
    void onWorker(Fn&& fn)
    {
        if (m_thread)
            QMetaObject::invokeMethod(m_session, std::forward<Fn>(fn), Qt::QueuedConnection);
        else
            fn();
    }

    QThread* m_thread = nullptr;
    LlmClient* m_client = nullptr;
    LlmSession* m_session = nullptr;
    LlmSession::ApprovalFn m_approve;

    // mirrors, GUI thread
    LlmProfile m_profile;
    QString m_model;
    QString m_reasoningEffort = QStringLiteral("medium");
    ToolAutonomy m_autonomy = ToolAutonomy::Ask;
    int m_maxIterations = 50;
    bool m_busy = false;
};
