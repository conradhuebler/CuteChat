// chatengine.cpp - Owns the LLM client and the agent session, optionally on a thread of their own.
// Copyright (C) 2016 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - see chatengine.h for the threading rules.
#include <cutechat/chatengine.h>

#include <cutechat/tooldispatcher.h>
#include <cutechat/toolregistry.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMetaObject>
#include <QPointer>
#include <QThread>

ChatEngine::ChatEngine(ToolRegistry* registry, ToolDispatcher* dispatcher, bool ownThread, QObject* parent)
    : QObject(parent)
{
    // queued signals carry these by value
    qRegisterMetaType<ToolResult>();
    qRegisterMetaType<LlmModelInfo>();

    // Parentless when threaded: an object cannot move to another thread while it has a parent here.
    QObject* owner = ownThread ? nullptr : this;
    m_client = new LlmClient(owner);
    m_session = new LlmSession(m_client, registry, dispatcher, owner);

    // Same-thread: direct. Worker: the receiver (this) lives on the GUI thread, so the connection
    // is queued by AutoConnection and every signal below arrives here, in order.
    connect(m_session, &LlmSession::assistantMessage, this, &ChatEngine::assistantMessage);
    connect(m_session, &LlmSession::assistantChunk, this, &ChatEngine::assistantChunk);
    connect(m_session, &LlmSession::reasoningChunk, this, &ChatEngine::reasoningChunk);
    connect(m_session, &LlmSession::toolStarted, this, &ChatEngine::toolStarted);
    connect(m_session, &LlmSession::toolFinished, this, &ChatEngine::toolFinished);
    connect(m_session, &LlmSession::toolRefused, this, &ChatEngine::toolRefused);
    connect(m_session, &LlmSession::finished, this, &ChatEngine::finished);
    connect(m_session, &LlmSession::failed, this, &ChatEngine::failed);
    connect(m_session, &LlmSession::noted, this, &ChatEngine::noted);
    connect(m_session, &LlmSession::busyChanged, this, [this](bool busy) {
        m_busy = busy;
        emit busyChanged(busy);
    });
    connect(m_client, &LlmClient::modelsListed, this, &ChatEngine::modelsListed);
    connect(m_client, &LlmClient::modelListFailed, this, &ChatEngine::modelListFailed);
    connect(m_client, &LlmClient::modelInfo, this, &ChatEngine::modelInfo);

    if (ownThread) {
        m_thread = new QThread(this);
        m_thread->setObjectName(QStringLiteral("chat-agent"));
        // both are deleted by their own thread once its loop has ended
        connect(m_thread, &QThread::finished, m_session, &QObject::deleteLater);
        connect(m_thread, &QThread::finished, m_client, &QObject::deleteLater);
        m_client->moveToThread(m_thread);
        m_session->moveToThread(m_thread);
        m_thread->start();
    }
}

ChatEngine::~ChatEngine()
{
    if (!m_thread)
        return; // children: Qt deletes them
    // A running turn is cancelled on the worker, then the loop ends and the objects delete themselves.
    // The worker may be blocked in a call that only this thread can answer (the dispatcher's tool
    // catalogue, an approval), so waiting must keep serving this thread's events; a plain wait()
    // would be a deadlock that ends in QThread's "destroyed while running" abort.
    QMetaObject::invokeMethod(m_session, &LlmSession::cancel, Qt::QueuedConnection);
    m_thread->quit();
    QElapsedTimer waited;
    waited.start();
    while (!m_thread->wait(10) && waited.elapsed() < 5000)
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    if (m_thread->isRunning()) {
        // A tool that never returns. Better a thread that outlives its owner than a crash at exit:
        // it deletes itself when (if) it ends.
        m_thread->setParent(nullptr);
        connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    }
}

void ChatEngine::setProfile(const LlmProfile& profile)
{
    // the same rule as LlmClient::setProfile: an override belongs to the profile it was chosen for
    if (profile.name != m_profile.name || profile.baseUrl != m_profile.baseUrl)
        m_model.clear();
    m_profile = profile;
    LlmClient* client = m_client;
    onWorker([client, profile] { client->setProfile(profile); });
}

void ChatEngine::setApiKey(const QString& key)
{
    LlmClient* client = m_client;
    onWorker([client, key] { client->setApiKey(key); });
}

void ChatEngine::setModel(const QString& model)
{
    m_model = model;
    LlmClient* client = m_client;
    onWorker([client, model] { client->setModel(model); });
}

void ChatEngine::setReasoningEffort(const QString& effort)
{
    m_reasoningEffort = effort;
    LlmClient* client = m_client;
    onWorker([client, effort] { client->setReasoningEffort(effort); });
}

void ChatEngine::setSortModels(bool sorted)
{
    LlmClient* client = m_client;
    onWorker([client, sorted] { client->setSortModels(sorted); });
}

void ChatEngine::listModels()
{
    LlmClient* client = m_client;
    onWorker([client] { client->listModels(); });
}

void ChatEngine::queryCapabilities(const QStringList& models)
{
    LlmClient* client = m_client;
    onWorker([client, models] { client->queryCapabilities(models); });
}

void ChatEngine::describeModel(const QString& id)
{
    LlmClient* client = m_client;
    onWorker([client, id] { client->describeModel(id); });
}

void ChatEngine::setSystemPrompt(const QString& prompt)
{
    LlmSession* session = m_session;
    onWorker([session, prompt] { session->setSystemPrompt(prompt); });
}

void ChatEngine::setMaxIterations(int iterations)
{
    m_maxIterations = iterations;
    LlmSession* session = m_session;
    onWorker([session, iterations] { session->setMaxIterations(iterations); });
}

void ChatEngine::setAutonomy(ToolAutonomy autonomy)
{
    m_autonomy = autonomy;
    LlmSession* session = m_session;
    onWorker([session, autonomy] { session->setAutonomy(autonomy); });
}

void ChatEngine::setVisionCapable(bool capable)
{
    LlmSession* session = m_session;
    onWorker([session, capable] { session->setVisionCapable(capable); });
}

void ChatEngine::setKeepHistory(bool keep)
{
    LlmSession* session = m_session;
    onWorker([session, keep] { session->setKeepHistory(keep); });
}

void ChatEngine::setApprovalPolicy(LlmSession::ApprovalFn approve)
{
    m_approve = std::move(approve);
    if (!m_thread) {
        m_session->setApprovalPolicy(m_approve);
        return;
    }
    // The session asks on the worker and waits. The policy (a dialog, the audit recorder) answers on
    // this thread. Blocking is safe: nothing on the GUI thread ever waits for the worker except
    // conversation(), and that one is not called while a turn is asking for approval.
    QPointer<ChatEngine> self(this);
    LlmSession::ApprovalFn marshalled = [self](const ToolSpec& spec, const QJsonObject& args) {
        if (!self)
            return false;
        bool approved = false;
        QMetaObject::invokeMethod(self.data(), [self, &spec, &args, &approved] {
            approved = self && self->m_approve && self->m_approve(spec, args);
        }, Qt::BlockingQueuedConnection);
        return approved;
    };
    LlmSession* session = m_session;
    QMetaObject::invokeMethod(session, [session, marshalled] { session->setApprovalPolicy(marshalled); },
        Qt::QueuedConnection);
}

void ChatEngine::ask(const QString& userMessage)
{
    LlmSession* session = m_session;
    onWorker([session, userMessage] { session->ask(userMessage); });
}

void ChatEngine::cancel()
{
    LlmSession* session = m_session;
    onWorker([session] { session->cancel(); });
}

void ChatEngine::reset()
{
    LlmSession* session = m_session;
    onWorker([session] { session->reset(); });
}

QJsonArray ChatEngine::conversation() const
{
    if (!m_thread)
        return m_session->conversation();
    // Not a BlockingQueuedConnection: while a turn runs, the worker may be waiting for this thread
    // (a Gui-affinity tool, an approval), and a GUI thread blocked here would never answer it. The
    // wait keeps serving events instead; the reply is posted back to this thread.
    QJsonArray result;
    bool ready = false;
    QEventLoop loop;
    LlmSession* session = m_session;
    ChatEngine* self = const_cast<ChatEngine*>(this);
    QMetaObject::invokeMethod(session, [session, self, &result, &ready, &loop] {
        const QJsonArray copy = session->conversation();
        QMetaObject::invokeMethod(self, [copy, &result, &ready, &loop] {
            result = copy;
            ready = true;
            loop.quit();
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
    if (!ready)
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    return result;
}

void ChatEngine::setConversation(const QJsonArray& messages)
{
    LlmSession* session = m_session;
    onWorker([session, messages] { session->setConversation(messages); });
}
