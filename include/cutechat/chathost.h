// chathost.h - Everything application specific the chat dock needs, behind one interface.
// Copyright (C) 2016 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - The dock knows nothing about projects, molecules or fits. The
// application implements ChatHost once and hands it to the ChatDock: system prompt, tool
// registration, an optional persistence backend for chats bound to a context, texts that name
// the application's own concepts, and hooks (approval dialog, "a tool changed the state").
// Only virtuals with a sensible default are optional; systemPrompt() and registerTools() are
// the two every application has to answer.
#pragma once

#include <cutechat/chatbackend.h>
#include <cutechat/tool.h>
#include <cutechat/tooldispatcher.h>
#include <cutechat/toolregistry.h>

#include <QJsonObject>
#include <QString>
#include <QStringList>

class QWidget;

/// Words the dock uses where an application's own concept shows. The defaults are neutral
/// ("context", "chat"); an application replaces what it wants to name differently. All strings
/// are shown to the user, so they are translated by the application (tr() at the call site).
/// Claude Generated.
struct ChatUiTexts {
    QString dockTitle = QStringLiteral("Assistant");
    QString dockObjectName = QStringLiteral("chat_dock");   ///< persisted in the window state: keep stable
    QString inputPlaceholder = QStringLiteral("Ask a question...");
    QString askLabel = QStringLiteral("Ask first");                  ///< autonomy "Ask": which calls are put to the user
    QString inProgramLabel = QStringLiteral("Run in program");
    QString fullLabel = QStringLiteral("Full");
    QString chatFileLabel = QStringLiteral("Chat");                 ///< file dialog filter: "<label> (*.json)"
    QString autonomyTip = QStringLiteral("Which tool calls run without asking");
    /// Shown under the selectors while the level is armed (empty: nothing shown). The dock should
    /// never hide what the assistant is allowed to do unasked.
    QString inProgramNote;
    QString fullNote;
    QString approvalTitle = QStringLiteral("Assistant");
    QString noContextNotice = QStringLiteral("No context selected: this chat is not stored with anything.");
    QString thisContextHistory = QStringLiteral("This context");   ///< history menu section
    QString noChatsInContext = QStringLiteral("No chats stored for this context");
    QString removeChatsAction = QStringLiteral("Remove chats of this context...");
    QString removeChatsTip = QStringLiteral("The stored conversations of this context are removed.");
    QString removeChatsQuestion = QStringLiteral("Remove all chats of this context? Copies stay in the history under \"Other chats\".");
    QString chatsRemovedNotice = QStringLiteral("The chats of this context were removed.");
    QString restoredFromContextNotice = QStringLiteral("Chat of %1, stored with this context. The conversation can be continued.");
    QString restoredFromHistoryNotice = QStringLiteral("Chat of %1 restored from the history. It may belong to another context.");
    QString remoteEndpointNotice = QStringLiteral("Remote endpoint %1: the conversation and the data the assistant reads are sent there.");
    QString remoteIdentityNotice = QStringLiteral("Remote endpoint: questions and data go to %1");
    QString noToolSupportWarning = QStringLiteral("%1 reports no tool support; the assistant needs tools to work.");
};

class ChatHost {
public:
    virtual ~ChatHost() = default;

    /// The system prompt of the next question. Called before every question, so it may depend on
    /// the application's state.
    virtual QString systemPrompt() const = 0;
    /// Register the application's tools; called once by the dock, with a registry the dock owns
    /// (unless sharedRegistry() returns one).
    virtual void registerTools(ToolRegistry& registry) = 0;
    /// An application that keeps its tools in a registry of its own (a singleton) returns it
    /// here; the dock then neither creates a registry nor calls registerTools().
    virtual ToolRegistry* sharedRegistry() { return nullptr; }
    /// Likewise a dispatcher the application configured itself (log sink, interrupt handling). It
    /// must belong to the GUI thread and work on sharedRegistry(); the dock then creates none.
    virtual ToolDispatcher* sharedDispatcher() { return nullptr; }

    /// Where chats bound to a context are stored; null: the dock keeps only the history in
    /// `<AppDataLocation>/chats`.
    virtual ChatBackend* chatBackend() { return nullptr; }
    /// The user's switch "store chats with the context" (SupraFit: the project file).
    virtual bool storeInBackend() const { return true; }
    /// Names of tool arguments that carry a context id (SupraFit: "project_id"): chats then
    /// remember which contexts a conversation touched.
    virtual QStringList contextArguments() const { return {}; }

    /// Run the LLM client and the agent loop on a thread of their own (ChatEngine). An application
    /// whose tools wait for something the GUI thread has to deliver (a calculation, a simulation)
    /// needs this, otherwise the wait blocks the event it waits for. The tools themselves still run
    /// where their ToolAffinity says; the approval question is always asked on the GUI thread.
    virtual bool agentOnWorkerThread() const { return false; }

    /// The how-to help of the application (Markdown, sections "## 1. Title", see chathelp.h). Empty: no
    /// help. The dock then registers the tool get_help and lets the user choose in its menu whether the
    /// whole text also goes into the system prompt; the default of that choice is helpInPromptByDefault().
    virtual QString helpText() const { return QString(); }
    virtual bool helpInPromptByDefault() const { return false; }

    /// Keep the autonomy level across sessions. Off by default: restoring a loosened level on the
    /// next start is a decision for the application (qurcuma does; it logs every change).
    virtual bool rememberAutonomy() const { return false; }

    /// Show the "Thinking" selector and the rounds limit next to the autonomy box. Both are
    /// remembered per profile under settingsPrefix().
    virtual bool showAgentControls() const { return false; }

    /// Version and commit written into the chat record (empty: not recorded).
    virtual QString appVersion() const { return QString(); }
    virtual QString appCommit() const { return QString(); }
    /// Prefix of the QSettings keys of the dock (input history, remembered models, last
    /// directory). The default keeps the keys SupraFit has always used.
    virtual QString settingsPrefix() const { return QStringLiteral("chat"); }
    virtual ChatUiTexts uiTexts() const { return ChatUiTexts(); }

    /// Ask the user whether @p spec may run with @p args (tools whose effect the autonomy does
    /// not allow unasked). Runs on the GUI thread. The default is a yes/no box.
    virtual bool approveToolCall(const ToolSpec& spec, const QJsonObject& args, QWidget* parent);

    /// A tool finished (also failed ones). The place for "the application state changed, show it"
    /// (SupraFit: make sure the model widget of a changed project exists). GUI thread.
    virtual void toolFinished(const QString& name, const QJsonObject& args, const ToolResult& result)
    {
        Q_UNUSED(name) Q_UNUSED(args) Q_UNUSED(result)
    }
};
