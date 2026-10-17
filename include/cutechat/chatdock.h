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

#include <cutechat/chatlog.h>
#include <cutechat/llmconfig.h>

class QComboBox;
class QLabel;
class QMenu;
class QPushButton;
class QTimer;
class QToolButton;

class ChatHost;
class ChatInput;
class ChatRecorder;
class ChatView;
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
    /// The "more" menu (new chat, history, export, import, ...).
    QMenu* actionsMenu() const { return m_menu; }
    QMenu* historyMenu() const { return m_historyMenu; }
    bool isMaximized() const { return m_maximized; }

    QString contextId() const { return m_contextId; }
    /// The pieces the dock owns, for applications that connect to them (log, macro recorder, ...).
    ToolRegistry* registry() const { return m_registry; }
    ToolDispatcher* dispatcher() const { return m_dispatcher; }
    LlmClient* client() const { return m_client; }
    LlmSession* session() const { return m_session; }

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
    void setRunning(bool running);
    void addNotice(const QString& text, bool error = false, bool store = true);
    void flushStream();
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
    QPushButton *m_send, *m_stop, *m_refresh;
    QToolButton *m_menuButton, *m_maxButton;
    QMenu *m_menu, *m_historyMenu;
    QLabel *m_status, *m_identity;
    ChatView* m_view;
    ChatInput* m_input;

    ChatHost* m_host;
    bool m_ownsRegistry = true;
    ToolRegistry* m_registry;
    ToolDispatcher* m_dispatcher;
    LlmClient* m_client;
    LlmSession* m_session;
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
    bool m_streamOpen = false;

    QString m_contextId; ///< context the current chat belongs to (empty: none)
    QString m_lastToolName;
    QJsonObject m_lastToolArgs; ///< arguments of the tool call in flight, for ChatHost::toolFinished
    bool m_maximized = false;
    bool m_wasFloating = false;
    QRect m_previousGeometry;
};
