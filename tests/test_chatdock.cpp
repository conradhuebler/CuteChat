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

    void questionIsAnsweredStoredAndRestored()
    {
        StubServer server;
        QVERIFY(server.isListening());
        QTemporaryDir dir;
        TestHost host(dir.path());

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
        QVERIFY(waitFor([&] { return host.backend.load(QStringLiteral("ctx-A")).size() == 1 && !dock.session()->isBusy(); }));

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
        QCOMPARE(dock.session()->conversation().size(), stored.first().conversation().size());
    }
};

QTEST_MAIN(ChatDockTest)
#include "test_chatdock.moc"
