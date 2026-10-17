/* CuteChat - features merged in from qurcuma and the new interfaces (headless, no net):
 * reasoning effort in the request, vision messages, free waiting rounds, the dispatcher's log
 * sink, LlmConfig::updateBaseUrl, FileChatBackend, SessionKeys, ChatLog format prefix.
 * Same moc rule as test_llmclient.cpp: JSON with escaped quotes goes into plain string
 * concatenation, never a raw string. Claude Generated 2026. */

#include <QtTest/QtTest>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>

#include <cutechat/chatbackend.h>
#include <cutechat/chatlog.h>
#include <cutechat/llmclient.h>
#include <cutechat/llmconfig.h>
#include <cutechat/llmsession.h>
#include <cutechat/sessionkeys.h>
#include <cutechat/tooldispatcher.h>
#include <cutechat/toolregistry.h>

#include "test_stubserver.h"

namespace {
QByteArray textResponse(const char* text)
{
    return QByteArray("{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"") + text
        + QByteArray("\"}}]}");
}

QByteArray callResponse(const char* name, const char* callId)
{
    return QByteArray("{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":null,\"tool_calls\":[{\"id\":\"")
        + QByteArray(callId) + QByteArray("\",\"type\":\"function\",\"function\":{\"name\":\"")
        + QByteArray(name) + QByteArray("\",\"arguments\":\"{}\"}}]}}]}");
}

LlmProfile stubProfile(const StubServer& server)
{
    LlmProfile profile;
    profile.name = QStringLiteral("stub");
    profile.baseUrl = server.baseUrl();
    profile.model = QStringLiteral("test-model");
    profile.stream = false;
    return profile;
}

void runTurn(LlmSession& session, const QString& question, QString& answer, QString& error)
{
    QEventLoop loop;
    answer.clear();
    error.clear();
    QObject::connect(&session, &LlmSession::assistantMessage, &loop, [&answer](const QString& t) { answer = t; });
    QObject::connect(&session, &LlmSession::finished, &loop, [&loop] { loop.quit(); });
    QObject::connect(&session, &LlmSession::failed, &loop, [&error, &loop](const QString& e) {
        error = e;
        loop.quit();
    });
    QTimer::singleShot(8000, &loop, [&loop] { loop.quit(); });
    session.ask(question);
    loop.exec();
}

bool bodyHasImage(const QByteArray& body)
{
    const QJsonArray messages = QJsonDocument::fromJson(body).object().value(QStringLiteral("messages")).toArray();
    for (const QJsonValue& m : messages)
        for (const QJsonValue& part : m.toObject().value(QStringLiteral("content")).toArray())
            if (part.toObject().value(QStringLiteral("type")).toString() == QLatin1String("image_url"))
                return true;
    return false;
}
} // namespace

class CuteChatExtrasTest : public QObject {
    Q_OBJECT

private slots:
    // --- reasoning effort -----------------------------------------------------
    void reasoningEffortIsSentUnderTheProfilesFieldName()
    {
        StubServer server;
        LlmProfile profile = stubProfile(server);
        LlmClient client;
        client.setProfile(profile);
        QEventLoop loop;
        connect(&client, &LlmClient::finished, &loop, &QEventLoop::quit);
        connect(&client, &LlmClient::failed, &loop, &QEventLoop::quit);
        auto sendOnce = [&]() {
            server.reply(200, textResponse("x"));
            client.send(QJsonArray { QJsonObject { { "role", "user" }, { "content", "hi" } } });
            loop.exec();
            return QJsonDocument::fromJson(server.requests.last()).object();
        };
        QCOMPARE(sendOnce().value(QStringLiteral("reasoning_effort")).toString(), QStringLiteral("medium")); // default
        client.setReasoningEffort(QStringLiteral("low"));
        QCOMPARE(sendOnce().value(QStringLiteral("reasoning_effort")).toString(), QStringLiteral("low"));
        client.setReasoningEffort(QStringLiteral("off"));
        QCOMPARE(sendOnce().value(QStringLiteral("reasoning_effort")).toString(), QStringLiteral("none"));
        client.setReasoningEffort(QString());
        QVERIFY(!sendOnce().contains(QStringLiteral("reasoning_effort"))); // endpoint default: field left out

        profile.reasoningField = QStringLiteral("think"); // Ollama's own switch
        client.setProfile(profile);
        client.setReasoningEffort(QStringLiteral("off"));
        QCOMPARE(sendOnce().value(QStringLiteral("think")).toBool(true), false);
        client.setReasoningEffort(QStringLiteral("high"));
        QCOMPARE(sendOnce().value(QStringLiteral("think")).toBool(false), true);
    }

    // --- vision ---------------------------------------------------------------
    void imageIsSentOnlyToVisionModels_data()
    {
        QTest::addColumn<bool>("vision");
        QTest::newRow("vision model") << true;
        QTest::newRow("text-only model") << false;
    }
    void imageIsSentOnlyToVisionModels()
    {
        QFETCH(bool, vision);
        ToolRegistry registry;
        ToolSpec spec;
        spec.name = QStringLiteral("render");
        spec.description = QStringLiteral("render a picture");
        spec.effect = ToolEffect::Display;
        spec.paramSchema = QJsonObject { { QStringLiteral("type"), QStringLiteral("object") } };
        spec.handler = [](const QJsonObject&) {
            ToolResult r = ToolResult::success(QJsonObject());
            r.image = QByteArray("\x89PNG not really");
            r.imageMimeType = QStringLiteral("image/png");
            return r;
        };
        QVERIFY(registry.add(spec));
        ToolDispatcher dispatcher(&registry);

        StubServer server;
        server.enqueue(callResponse("render", "call_1"));
        server.enqueue(textResponse("seen"));
        LlmClient client;
        client.setProfile(stubProfile(server));
        LlmSession session(&client, &registry, &dispatcher);
        session.setVisionCapable(vision);
        QString answer, error;
        runTurn(session, QStringLiteral("show it"), answer, error);
        QCOMPARE(answer, QStringLiteral("seen"));
        QCOMPARE(server.requests.size(), 2);
        QCOMPARE(bodyHasImage(server.requests.last()), vision);
        const QByteArray text = server.requests.last();
        QCOMPARE(text.contains("does not take images"), !vision);
    }

    // --- waiting rounds are free ----------------------------------------------
    void roundsThatOnlyWaitDoNotCountAgainstTheBudget()
    {
        ToolRegistry registry;
        ToolSpec spec;
        spec.name = QStringLiteral("watch");
        spec.description = QStringLiteral("wait for something");
        spec.effect = ToolEffect::Read;
        spec.paramSchema = QJsonObject { { QStringLiteral("type"), QStringLiteral("object") } };
        spec.handler = [](const QJsonObject&) { return ToolResult::success(QJsonObject()); };
        QVERIFY(registry.add(spec));
        ToolDispatcher dispatcher(&registry);

        StubServer server;
        for (int i = 0; i < 4; ++i)
            server.enqueue(callResponse("watch", QByteArray("call_" + QByteArray::number(i)).constData()));
        server.enqueue(textResponse("done waiting"));
        LlmClient client;
        client.setProfile(stubProfile(server));
        LlmSession session(&client, &registry, &dispatcher);
        session.setMaxIterations(2);
        session.setWaitRoundThresholdMs(0); // every read-only round counts as "waited"
        QString answer, error;
        runTurn(session, QStringLiteral("wait"), answer, error);
        QCOMPARE(answer, QStringLiteral("done waiting"));
        QVERIFY(!server.requests.last().contains("budget"));
    }

    void roundsThatWorkDoCountAgainstTheBudget()
    {
        ToolRegistry registry;
        ToolSpec spec;
        spec.name = QStringLiteral("edit");
        spec.description = QStringLiteral("changes something");
        spec.effect = ToolEffect::Mutate;
        spec.paramSchema = QJsonObject { { QStringLiteral("type"), QStringLiteral("object") } };
        spec.handler = [](const QJsonObject&) { return ToolResult::success(QJsonObject()); };
        QVERIFY(registry.add(spec));
        ToolDispatcher dispatcher(&registry);

        StubServer server;
        for (int i = 0; i < 3; ++i)
            server.enqueue(callResponse("edit", QByteArray("call_" + QByteArray::number(i)).constData()));
        server.enqueue(textResponse("stopped"));
        LlmClient client;
        client.setProfile(stubProfile(server));
        LlmSession session(&client, &registry, &dispatcher);
        session.setAutonomy(ToolAutonomy::Full);
        session.setMaxIterations(2);
        session.setWaitRoundThresholdMs(0);
        QString answer, error;
        runTurn(session, QStringLiteral("work"), answer, error);
        QVERIFY(server.requests.last().contains("budget")); // the closing round was asked for
    }

    // --- dispatcher log sink --------------------------------------------------
    void dispatcherLogSinkReceivesOneLinePerCall()
    {
        ToolRegistry registry;
        ToolSpec spec;
        spec.name = QStringLiteral("ping");
        spec.description = QStringLiteral("ok");
        spec.effect = ToolEffect::Read;
        spec.paramSchema = QJsonObject { { QStringLiteral("type"), QStringLiteral("object") } };
        spec.handler = [](const QJsonObject&) { return ToolResult::success(QJsonObject()); };
        QVERIFY(registry.add(spec));
        ToolDispatcher dispatcher(&registry);
        QStringList lines;
        QList<int> levels;
        dispatcher.setLogSink([&](int level, const QString& line) {
            levels << level;
            lines << line;
        });
        ToolDispatcher::setCallOrigin(QStringLiteral("assistant"));
        QVERIFY(dispatcher.dispatch(QStringLiteral("ping"), QJsonObject()).ok);
        ToolDispatcher::setCallOrigin(QString());
        QVERIFY(!dispatcher.dispatch(QStringLiteral("nope"), QJsonObject()).ok);
        QCOMPARE(lines.size(), 2);
        QVERIFY(lines.first().startsWith(QStringLiteral("ping (read) [assistant] args={} -> ok in ")));
        QCOMPARE(levels, (QList<int> { 0, 1 }));
    }

    // --- llm.json -------------------------------------------------------------
    void updateBaseUrlKeepsEverythingElse()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("llm.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"_note\":\"keep me\",\"active_profile\":\"a\",\"profiles\":["
                   "{\"name\":\"a\",\"base_url\":\"http://127.0.0.1:1/v1\",\"api_key_env\":\"MY_KEY\",\"custom\":42},"
                   "{\"name\":\"b\",\"base_url\":\"http://127.0.0.1:2/v1\"}]}");
        file.close();
        QString error;
        QVERIFY2(LlmConfig::updateBaseUrl(path, QStringLiteral("a"), QStringLiteral(" http://10.0.0.5:11434/v1 "), &error), qPrintable(error));
        LlmConfig config;
        QVERIFY(config.load(path, &error));
        LlmProfile a, b;
        QVERIFY(config.profile(QStringLiteral("a"), a));
        QVERIFY(config.profile(QStringLiteral("b"), b));
        QCOMPARE(a.baseUrl, QStringLiteral("http://10.0.0.5:11434/v1"));
        QCOMPARE(a.apiKeyEnv, QStringLiteral("MY_KEY"));
        QCOMPARE(a.extra.value(QStringLiteral("custom")).toInt(), 42);
        QCOMPARE(b.baseUrl, QStringLiteral("http://127.0.0.1:2/v1"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().contains("keep me"));
        QVERIFY(!LlmConfig::updateBaseUrl(path, QStringLiteral("missing"), QStringLiteral("x"), &error));
    }

    // --- session keys -----------------------------------------------------------
    void sessionKeyIsUsedWhenNoEnvironmentVariableIsNamed()
    {
        LlmProfile profile;
        profile.name = QStringLiteral("typed");
        profile.baseUrl = QStringLiteral("http://127.0.0.1:1/v1");
        QVERIFY(SessionKeys::effectiveKey(profile).isEmpty());
        SessionKeys::setKey(QStringLiteral("typed"), QStringLiteral("secret"));
        QCOMPARE(SessionKeys::effectiveKey(profile), QStringLiteral("secret"));
        SessionKeys::setKey(QStringLiteral("typed"), QString());
        QVERIFY(SessionKeys::key(QStringLiteral("typed")).isEmpty());
    }

    // --- chat storage -----------------------------------------------------------
    void fileBackendStoresReplacesAndClearsPerContext()
    {
        QTemporaryDir dir;
        FileChatBackend backend(dir.path());
        QVERIFY(!backend.accepts(QString()));
        QVERIFY(backend.accepts(QStringLiteral("/some/where/structure.xyz")));

        ChatLog chat;
        chat.append(ChatEntry::Kind::User, QStringLiteral("first"));
        chat.addContextId(QStringLiteral("ctx-1"));
        backend.store(QStringLiteral("ctx-1"), chat);
        chat.append(ChatEntry::Kind::Assistant, QStringLiteral("answer"));
        backend.store(QStringLiteral("ctx-1"), chat); // same id: replaced
        ChatLog other;
        other.append(ChatEntry::Kind::User, QStringLiteral("elsewhere"));
        backend.store(QStringLiteral("ctx-2"), other);
        backend.store(QStringLiteral("ctx-1"), ChatLog()); // empty: ignored

        QStringList warnings;
        const QVector<ChatLog> loaded = backend.load(QStringLiteral("ctx-1"), &warnings);
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded.first().entries().size(), 2);
        QCOMPARE(loaded.first().contextIds(), QStringList { QStringLiteral("ctx-1") });
        QVERIFY(warnings.isEmpty());
        QCOMPARE(backend.load(QStringLiteral("ctx-2")).size(), 1);
        QVERIFY(backend.load(QStringLiteral("unknown")).isEmpty());

        backend.clear(QStringLiteral("ctx-1"));
        QVERIFY(backend.load(QStringLiteral("ctx-1")).isEmpty());
        QCOMPARE(backend.load(QStringLiteral("ctx-2")).size(), 1);

        // a damaged file is skipped with a warning
        QDir().mkpath(backend.contextDirectory(QStringLiteral("ctx-3")));
        QFile bad(backend.contextDirectory(QStringLiteral("ctx-3")) + QStringLiteral("/broken.json"));
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write("not json");
        bad.close();
        QStringList w;
        QVERIFY(backend.load(QStringLiteral("ctx-3"), &w).isEmpty());
        QCOMPARE(w.size(), 1);
    }

    void formatPrefixIsWrittenAndAnyPrefixIsRead()
    {
        ChatLog chat;
        chat.append(ChatEntry::Kind::User, QStringLiteral("q"));
        QCOMPARE(chat.toJson().value(QStringLiteral("format")).toString(), QStringLiteral("cutechat-chat"));
        ChatLog::setFormatPrefix(QStringLiteral("suprafit"));
        QCOMPARE(chat.toJson().value(QStringLiteral("format")).toString(), QStringLiteral("suprafit-chat"));
        QCOMPARE(ChatLog::toBlock({ chat }).value(QStringLiteral("format")).toString(), QStringLiteral("suprafit-agent-chats"));
        const QJsonObject suprafit = chat.toJson();
        ChatLog::setFormatPrefix(QString()); // back to the default
        ChatLog back;
        QVERIFY(ChatLog::fromJson(suprafit, &back)); // written by another application: still readable
        QCOMPARE(ChatLog::fromBlock(ChatLog::toBlock({ chat })).size(), 1);
    }
};

QTEST_MAIN(CuteChatExtrasTest)
#include "test_cutechat_extras.moc"
