/* CuteChat - the ChatDock against a stub endpoint and a test ChatHost (offscreen, no net):
 * tools are registered through the host, a question is answered, the chat lands in the
 * host's backend under the context id, a context switch brings it back, the approval question
 * and the "tool finished" hook reach the host. Same moc rule as test_llmclient.cpp.
 * Claude Generated 2026. */

#include <QtTest/QtTest>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryDir>
#include <QtWidgets/QApplication>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>

#include <cutechat/chatbackend.h>
#include <cutechat/chatdock.h>
#include <QAction>
#include <QMenu>
#include <QSpinBox>
#include <QToolButton>
#include <cutechat/chatengine.h>
#include <cutechat/chathost.h>
#include <cutechat/llmconfig.h>
#include <cutechat/llmsession.h>

#include "test_stubserver.h"

namespace {
class TestHost : public ChatHost {
public:
    explicit TestHost(const QString& dir)
        : backend(dir)
    {
    }
    QString systemPrompt() const override { return QStringLiteral("You are a test assistant."); }
    void registerTools(ToolRegistry& registry) override
    {
        ToolSpec spec;
        spec.name = QStringLiteral("change_state");
        spec.description = QStringLiteral("changes something");
        spec.effect = ToolEffect::Mutate;
        spec.paramSchema = QJsonObject { { QStringLiteral("type"), QStringLiteral("object") } };
        spec.handler = [](const QJsonObject&) { return ToolResult::success(QJsonObject { { QStringLiteral("done"), true } }); };
        registry.add(spec);
    }
    ChatBackend* chatBackend() override { return &backend; }
    QStringList contextArguments() const override { return { QStringLiteral("ctx") }; }
    QString appVersion() const override { return QStringLiteral("9.9"); }
    ChatUiTexts uiTexts() const override
    {
        ChatUiTexts t;
        t.dockTitle = QStringLiteral("Test assistant");
        return t;
    }
    bool approveToolCall(const ToolSpec& spec, const QJsonObject&, QWidget*) override
    {
        approvals << spec.name;
        return true;
    }
    void toolFinished(const QString& name, const QJsonObject&, const ToolResult& result) override
    {
        finished << name + (result.ok ? QStringLiteral(":ok") : QStringLiteral(":failed"));
    }

    bool agentOnWorkerThread() const override { return threaded; }
    bool showAgentControls() const override { return controls; }
    bool controls = false;
    QString helpText() const override { return help; }
    QString help;

    bool threaded = false;
    FileChatBackend backend;
    QStringList approvals, finished;
};

bool waitFor(const std::function<bool()>& condition, int ms = 8000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < ms)
        QTest::qWait(20);
    return condition();
}
} // namespace

class ChatDockTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true); // llm.json, history and QSettings stay in the test locations
        QCoreApplication::setOrganizationName(QStringLiteral("CuteChatTest"));
        QCoreApplication::setApplicationName(QStringLiteral("chatdock"));
        QDir(QFileInfo(LlmConfig::defaultPath()).absolutePath()).removeRecursively();
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();
    }

    void questionIsAnsweredStoredAndRestored_data()
    {
        QTest::addColumn<bool>("threaded");
        QTest::newRow("same thread") << false;
        QTest::newRow("worker thread") << true;
    }
    void questionIsAnsweredStoredAndRestored()
    {
        QFETCH(bool, threaded);
        StubServer server;
        QVERIFY(server.isListening());
        QTemporaryDir dir;
        TestHost host(dir.path());
        host.threaded = threaded;

        // llm.json with the stub endpoint as the only profile
        LlmConfig config;
        LlmProfile profile;
        profile.name = QStringLiteral("stub");
        profile.baseUrl = server.baseUrl();
        profile.model = QStringLiteral("test-model");
        profile.stream = false;
        config.setProfiles({ profile });
        config.setActiveProfileName(QStringLiteral("stub"));
        QString error;
        QVERIFY2(config.save(LlmConfig::defaultPath(), &error), qPrintable(error));

        server.enqueue("{\"data\":[{\"id\":\"test-model\"}]}");                       // GET /models
        server.enqueue("{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":null,\"tool_calls\":[{\"id\":\"c1\","
                       "\"type\":\"function\",\"function\":{\"name\":\"change_state\",\"arguments\":\"{}\"}}]}}]}");
        server.enqueue("{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"all done\"}}]}");

        ChatDock dock(&host);
        dock.resize(420, 700);
        dock.show();
        QCOMPARE(dock.windowTitle(), QStringLiteral("Test assistant"));
        QCOMPARE(dock.objectName(), QStringLiteral("chat_dock"));
        ToolSpec found;
        QVERIFY(dock.registry()->tool(QStringLiteral("change_state"), found));
        QVERIFY(waitFor([&] { return server.requests.size() >= 1; })); // the model list was asked for

        dock.setContext(QStringLiteral("ctx-A"));
        QCOMPARE(dock.contextId(), QStringLiteral("ctx-A"));
        dock.setAutonomyIndex(0); // ask: the Mutate tool needs the host's approval
        dock.askQuestion(QStringLiteral("please change it"));
        QVERIFY(waitFor([&] { return host.backend.load(QStringLiteral("ctx-A")).size() == 1 && !dock.engine()->isBusy(); }));

        QCOMPARE(host.approvals, QStringList { QStringLiteral("change_state") });
        QCOMPARE(host.finished, QStringList { QStringLiteral("change_state:ok") });
        const QVector<ChatLog> stored = host.backend.load(QStringLiteral("ctx-A"));
        QCOMPARE(stored.first().appVersion(), QStringLiteral("9.9"));
        QCOMPARE(stored.first().lastText(ChatEntry::Kind::Assistant), QStringLiteral("all done"));
        bool toolEntry = false;
        for (const ChatEntry& e : stored.first().entries())
            if (e.kind == ChatEntry::Kind::Tool) {
                toolEntry = true;
                QCOMPARE(e.approval, QStringLiteral("approved"));
                QCOMPARE(e.decidedBy, QStringLiteral("user"));
            }
        QVERIFY(toolEntry);

        // another context shows an empty chat, going back brings the conversation back
        dock.setContext(QStringLiteral("ctx-B"));
        QVERIFY(host.backend.load(QStringLiteral("ctx-B")).isEmpty());
        dock.setContext(QStringLiteral("ctx-A"));
        QVERIFY(dock.grab().width() > 0);
        QCOMPARE(dock.engine()->conversation().size(), stored.first().conversation().size());
    }

    void reasoningBlockFoldsAndControlsAreRemembered()
    {
        StubServer server;
        QVERIFY(server.isListening());
        QTemporaryDir dir;
        TestHost host(dir.path());
        host.threaded = true;
        host.controls = true;

        LlmConfig config;
        LlmProfile profile;
        profile.name = QStringLiteral("stub");
        profile.baseUrl = server.baseUrl();
        profile.model = QStringLiteral("test-model");
        profile.stream = true;
        profile.maxToolIterations = 17;
        config.setProfiles({ profile });
        config.setActiveProfileName(QStringLiteral("stub"));
        QString error;
        QVERIFY2(config.save(LlmConfig::defaultPath(), &error), qPrintable(error));
        QSettings().remove(QStringLiteral("chat/rounds"));
        QSettings().remove(QStringLiteral("chat/reasoning"));

        server.enqueue("{\"data\":[{\"id\":\"test-model\"}]}"); // GET /models
        server.reply(200, QByteArray(), "text/event-stream");
        server.enqueue("{\"data\":[{\"id\":\"test-model\"}]}");

        ChatDock dock(&host);
        dock.show();
        auto* rounds = dock.findChild<QSpinBox*>();
        QVERIFY(rounds);
        QCOMPARE(rounds->value(), 17); // the profile's own limit until the user says otherwise
        QVERIFY(waitFor([&] { return server.requests.size() >= 1; }));

        // the user's choice is remembered for the profile and reaches the engine
        rounds->setValue(5);
        emit rounds->editingFinished();
        QCOMPARE(dock.engine()->maxIterations(), 5);
        QCOMPARE(QSettings().value(QStringLiteral("chat/rounds/stub")).toInt(), 5);

        server.reply(200,
            "data: {\"choices\":[{\"delta\":{\"reasoning\":\"17\"}}]}\n"
            "data: {\"choices\":[{\"delta\":{\"reasoning\":\"*3\"}}]}\n"
            "data: {\"choices\":[{\"delta\":{\"content\":\"51\"}}]}\n"
            "data: [DONE]\n",
            "text/event-stream");
        dock.askQuestion(QStringLiteral("17 times 3?"));
        QVERIFY(waitFor([&] { return !dock.engine()->isBusy() && server.requests.size() >= 2; }));
        QTest::qWait(150);

        // one reasoning block, titled with its length and folded now that the answer is there
        QToolButton* title = nullptr;
        for (auto* button : dock.findChildren<QToolButton*>())
            if (button->text().startsWith(QStringLiteral("Thinking")))
                title = button;
        QVERIFY2(title, "no reasoning block in the transcript");
        QVERIFY(title->text().contains(QStringLiteral("4"))); // "17" + "*3"
        QVERIFY(!title->isChecked());
    }

    void helpIsAToolAndOptionallyInThePrompt()
    {
        StubServer server;
        QVERIFY(server.isListening());
        QTemporaryDir dir;
        TestHost host(dir.path());
        host.help = QStringLiteral("<!-- provenance -->\n# Help\n\n## 1. Importing\nClick **Open Project** to import.\n\n## 2. Fitting\nPress **Fit**.\n");

        LlmConfig config;
        LlmProfile profile;
        profile.name = QStringLiteral("stub");
        profile.baseUrl = server.baseUrl();
        profile.model = QStringLiteral("test-model");
        profile.stream = false;
        config.setProfiles({ profile });
        config.setActiveProfileName(QStringLiteral("stub"));
        QString error;
        QVERIFY2(config.save(LlmConfig::defaultPath(), &error), qPrintable(error));
        QSettings().remove(QStringLiteral("chat/help_in_prompt"));

        server.enqueue("{\"data\":[{\"id\":\"test-model\"}]}");
        server.enqueue("{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"one\"}}]}");
        server.enqueue("{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"two\"}}]}");

        ChatDock dock(&host);
        dock.show();
        ToolSpec spec;
        QVERIFY2(dock.registry()->tool(QStringLiteral("get_help"), spec), "the dock registers get_help when the host has help");
        QCOMPARE(spec.effect, ToolEffect::Read);
        QAction* toggle = nullptr;
        for (QAction* a : dock.actionsMenu()->actions())
            if (a->text() == QStringLiteral("Program help in the prompt"))
                toggle = a;
        QVERIFY(toggle);
        QVERIFY(!toggle->isChecked()); // the host's default
        QVERIFY(waitFor([&] { return server.requests.size() >= 1; }));

        // off: a hint to use the tool, not the text
        dock.askQuestion(QStringLiteral("how do I import?"));
        QVERIFY(waitFor([&] { return server.requests.size() >= 2 && !dock.engine()->isBusy(); }));
        QString system = QJsonDocument::fromJson(server.requests.at(1)).object().value(QStringLiteral("messages")).toArray().first().toObject().value(QStringLiteral("content")).toString();
        QVERIFY(system.contains(QStringLiteral("get_help")));
        QVERIFY2(!system.contains(QStringLiteral("Open Project")), "tool mode must not put the text into the prompt");
        QVERIFY(!system.contains(QStringLiteral("provenance")));

        // on: the whole text, remembered
        toggle->setChecked(true);
        QCOMPARE(QSettings().value(QStringLiteral("chat/help_in_prompt")).toBool(), true);
        dock.newChat();
        dock.askQuestion(QStringLiteral("and now?"));
        QVERIFY(waitFor([&] { return server.requests.size() >= 3 && !dock.engine()->isBusy(); }));
        system = QJsonDocument::fromJson(server.requests.at(2)).object().value(QStringLiteral("messages")).toArray().first().toObject().value(QStringLiteral("content")).toString();
        QVERIFY(system.contains(QStringLiteral("Click **Open Project** to import.")));
        QVERIFY(system.contains(QStringLiteral("Press **Fit**.")));
        QSettings().remove(QStringLiteral("chat/help_in_prompt"));
    }
};

QTEST_MAIN(ChatDockTest)
#include "test_chatdock.moc"
