// chatdock.h - Chat dock for an LLM agent that drives the application through tools.
// Copyright (C) 2016 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - A chat transcript with a profile, model and autonomy selector and a
// confirmation question for tools that the chosen autonomy does not allow to run unasked. The
// transcript is a list of message widgets (questions right, answers left as bubbles with
// Markdown, tool calls as compact lines), the input is a growing multi-line field with history,
// chats are saved automatically and can be reopened, exported (Markdown, JSON) and imported.
//
// Everything application specific comes from a ChatHost (chathost.h): system prompt, tools,
// storage backend, texts. The data side lives in chatlog.h and chatbackend.h. Colours come from
// the palette, so the dock follows a light/dark switch of the application.
//
// Usage:  auto* dock = new ChatDock(&host, window);   window->addDockWidget(area, dock);
//         dock->setContext(currentContextId);   // whenever the bound thing changes
#pragma once

#include <QDockWidget>
#include <QIcon>
#include <QJsonObject>
#include <QMap>
#include <QRect>
#include <QString>

#include <cutechat/chathelp.h>
#include <cutechat/chatlog.h>
#include <cutechat/llmconfig.h>
#include <cutechat/tool.h> // ToolAutonomy

class QComboBox;
class QSpinBox;
class QLabel;
class QMenu;
class QPushButton;
class QTimer;
class QToolButton;

class ChatEngine;
class ChatHost;
class ChatInput;
class ChatRecorder;
class ChatView;
class ReasoningBlock;
class LlmClient;
class LlmSession;
class ToolDispatcher;
class ToolRegistry;

class ChatDock : public QDockWidget {
    Q_OBJECT

public:
    /// @p host is borrowed and must outlive the dock. Claude Generated.
    explicit ChatDock(ChatHost* host, QWidget* parent = nullptr);
    ~ChatDock() override;

    /// Show a saved chat (history or import file) and continue its conversation.
    bool openChat(const QString& path, QString* error = nullptr);
    /// Send @p text as if typed (used by the capture tooling as well).
    void askQuestion(const QString& text);
    /// Select the autonomy level by index (0 ask, 1 run in program, 2 full).
    void setAutonomyIndex(int index);
    /// The level in the selector, and select one without emitting autonomyChanged().
    ToolAutonomy autonomy() const;
    void setAutonomy(ToolAutonomy autonomy);
    /// The thinking effort chosen in the selector (empty: the endpoint's default, or no selector).
    QString reasoningEffort() const;
    /// Whether the whole help text goes into the system prompt (menu entry "Program help in the prompt").
    ChatHelp::Mode helpMode() const;
    /// The system prompt of the next question: the host's, extended by the help as chosen.
    QString systemPrompt() const;
    /// The "more" menu (new chat, history, export, import, ...).
    QMenu* actionsMenu() const { return m_menu; }
    QMenu* historyMenu() const { return m_historyMenu; }
    bool isMaximized() const { return m_maximized; }

    QString contextId() const { return m_contextId; }
    /// The pieces the dock owns, for applications that connect to them (log, macro recorder, ...).
    ToolRegistry* registry() const { return m_registry; }
    ToolDispatcher* dispatcher() const { return m_dispatcher; }
    ChatEngine* engine() const { return m_engine; }
    /// The engine's client and session. Call them only when the host did not ask for a worker
    /// thread (ChatHost::agentOnWorkerThread); otherwise connect to them or use engine().
    LlmClient* client() const;
    LlmSession* session() const;

signals:
    /// The user picked another autonomy level (also by setAutonomyIndex). With
    /// ChatHost::rememberAutonomy() the level is stored under settingsPrefix() first.
    void autonomyChanged(ToolAutonomy autonomy);
    /// The user picked another thinking effort (ChatHost::showAgentControls only).
    void reasoningEffortChanged(const QString& effort);

public slots:
    void toggleMaximized();
    void newChat();
    /// Bind the dock to @p contextId (SupraFit: the project uuid; empty: no context, the chat is
    /// kept only in the history) and show its most recent chat. Claude Generated.
    void setContext(const QString& contextId);
    void removeContextChats();

private slots:
    void send();
    void stop();
    void reloadProfiles();
    void exportMarkdown();
    void exportJson();
    void importJson();
    void copyLastAnswer();
    void fillHistoryMenu();
    void editEndpoints();

private:
    bool prepareClient();
    void loadAgentControls(const LlmProfile& profile);
    void rememberAgentControls();
    void setRunning(bool running);
    void addNotice(const QString& text, bool error = false, bool store = true);
    void flushStream();
    void foldReasoning();
    void updateAutonomyNote();
    void finishTurn();
    void autosave();
    void showLog(const ChatLog& log);
    void showChat(const ChatLog& log, bool fromProject);
    void resetLog();
    bool storeInBackend() const;
    QString settingsKey(const QString& name) const;
    QString lastDirectory() const;
    bool currentProfile(LlmProfile& profile) const;
    void listModelsForProfile();
    void applyModel(const QString& model, bool remember);
    void updateIdentity();
    static QIcon toolIcon(bool tools);
    void rememberDirectory(const QString& file);

    QComboBox *m_profile, *m_model, *m_autonomy;
    QComboBox* m_thinking = nullptr; ///< only with ChatHost::showAgentControls
    QSpinBox* m_rounds = nullptr;
    QPushButton *m_send, *m_stop, *m_refresh;
    QToolButton *m_menuButton, *m_maxButton;
    QMenu *m_menu, *m_historyMenu;
    QLabel *m_status, *m_identity;
    QLabel* m_autonomyNote = nullptr;
    ChatView* m_view;
    ChatInput* m_input;

    ChatHost* m_host;
    bool m_ownsRegistry = true;
    ToolRegistry* m_registry;
    ToolDispatcher* m_dispatcher;
    ChatEngine* m_engine;
    ChatHelp m_help;
    QAction* m_helpInPrompt = nullptr;
    ChatRecorder* m_recorder;

    ChatLog m_log;
    ChatHistoryStore m_store;
    InputHistory m_inputHistory;
    LlmModelSelection m_selection;
    QStringList m_listed;
    QMap<QString, LlmModelInfo> m_infos;
    QString m_listStatus, m_listError;

    // streaming: chunks are collected and shown at most every ~90 ms
    QTimer* m_streamTimer;
    QString m_streamText;
    ReasoningBlock* m_reasoning = nullptr; ///< the block of the round in progress
    bool m_streamOpen = false;

    QString m_contextId; ///< context the current chat belongs to (empty: none)
    QString m_lastToolName;
    QJsonObject m_lastToolArgs; ///< arguments of the tool call in flight, for ChatHost::toolFinished
    bool m_maximized = false;
    bool m_wasFloating = false;
    QRect m_previousGeometry;
};
