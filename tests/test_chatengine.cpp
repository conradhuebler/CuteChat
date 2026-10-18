/*
 * CuteChat - ChatEngine: the same agent turn on the calling thread and on a worker thread
 * Copyright (C) 2016 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* The engine promises that the dock cannot tell the two modes apart: every signal arrives on the
 * GUI thread, in order, the approval policy runs there, and a tool that waits does not freeze it.
 * Each test below runs in both modes (data rows), against the stub endpoint, no network.
 * Same moc rule as the other tests: escaped quotes only in plain string concatenation.
 * Claude Generated 2026. */

#include <QtTest/QtTest>
#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <cutechat/chatengine.h>
#include <cutechat/tooldispatcher.h>
#include <cutechat/toolregistry.h>

#include "test_stubserver.h"

class ChatEngineTest : public QObject {
    Q_OBJECT

private:
    ToolRegistry m_registry;
    ToolDispatcher m_dispatcher { &m_registry };
    Qt::HANDLE m_guiThread = nullptr;
    int m_computeCalls = 0;
    int m_waitedOnThread = 0;

    static QByteArray textResponse(const char* text)
    {
        return QByteArray("{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"") + text
            + QByteArray("\"}}]}");
    }
    static QByteArray callResponse(const char* name, const char* callId)
    {
        return QByteArray("{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":null,"
                          "\"tool_calls\":[{\"id\":\"")
            + QByteArray(callId) + QByteArray("\",\"type\":\"function\",\"function\":{\"name\":\"")
            + QByteArray(name) + QByteArray("\",\"arguments\":\"{}\"}}]}}]}");
    }

    static LlmProfile stubProfile(const StubServer& server)
    {
        LlmProfile profile;
        profile.name = QStringLiteral("stub");
        profile.baseUrl = server.baseUrl();
        profile.model = QStringLiteral("test-model");
        profile.stream = false;
        return profile;
    }

    /// One turn, until finished()/failed(); returns the final answer.
    QString runTurn(ChatEngine& engine, const QString& question, QString* error = nullptr)
    {
        QEventLoop loop;
        QString answer;
        const auto c1 = connect(&engine, &ChatEngine::assistantMessage, &loop, [&answer](const QString& t) { answer = t; });
        const auto c2 = connect(&engine, &ChatEngine::finished, &loop, [&loop] { loop.quit(); });
        const auto c3 = connect(&engine, &ChatEngine::failed, &loop, [&](const QString& e) {
            if (error)
                *error = e;
            loop.quit();
        });
        QTimer::singleShot(8000, &loop, [&loop] { loop.quit(); });
        engine.ask(question);
        loop.exec();
        disconnect(c1);
        disconnect(c2);
        disconnect(c3);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        return answer;
    }

private slots:
    void initTestCase()
    {
        m_guiThread = QThread::currentThreadId();
        {
            ToolSpec spec;
            spec.name = QStringLiteral("compute_test");
            spec.description = QStringLiteral("needs approval");
            spec.category = QStringLiteral("test");
            spec.effect = ToolEffect::Compute;
            spec.paramSchema = QJsonObject{ { QStringLiteral("type"), QStringLiteral("object") } };
            spec.handler = [this](const QJsonObject&) {
                ++m_computeCalls;
                return ToolResult::success(QJsonObject{ { QStringLiteral("started"), true } });
            };
            QVERIFY(m_registry.add(spec));
        }
        {
            // a tool that waits on the calling thread, as a simulation_status with wait_seconds does
            ToolSpec spec;
            spec.name = QStringLiteral("wait_test");
            spec.description = QStringLiteral("waits");
            spec.category = QStringLiteral("test");
            spec.effect = ToolEffect::Read;
            spec.affinity = ToolAffinity::Any;
            spec.paramSchema = QJsonObject{ { QStringLiteral("type"), QStringLiteral("object") } };
            spec.handler = [this](const QJsonObject&) {
                if (QThread::currentThreadId() != m_guiThread)
                    ++m_waitedOnThread;
                QThread::msleep(400);
                return ToolResult::success(QJsonObject{ { QStringLiteral("done"), true } });
            };
            QVERIFY(m_registry.add(spec));
        }
    }

    void plainAnswerAndFollowUp_data()
    {
        QTest::addColumn<bool>("threaded");
        QTest::newRow("same thread") << false;
        QTest::newRow("worker thread") << true;
    }
    void plainAnswerAndFollowUp()
    {
        QFETCH(bool, threaded);
        StubServer server;
        QVERIFY(server.isListening());
        ChatEngine engine(&m_registry, &m_dispatcher, threaded);
        QCOMPARE(engine.isThreaded(), threaded);
        engine.setProfile(stubProfile(server));
        engine.setSystemPrompt(QStringLiteral("You are a test."));
        engine.setKeepHistory(true);

        QVector<bool> busy;
        connect(&engine, &ChatEngine::busyChanged, this, [&busy](bool b) { busy.append(b); });

        server.reply(200, textResponse("first"));
        QCOMPARE(runTurn(engine, QStringLiteral("one")), QStringLiteral("first"));
        server.reply(200, textResponse("second"));
        QCOMPARE(runTurn(engine, QStringLiteral("two")), QStringLiteral("second"));

        // the second request carried system, user, assistant, user: history crossed the thread edge
        QCOMPARE(server.requests.size(), 2);
        const QJsonArray sent = QJsonDocument::fromJson(server.requests.at(1)).object().value(QStringLiteral("messages")).toArray();
        QCOMPARE(sent.size(), 4);
        // busy went up and down once per turn, and the mirror agrees with the end state
        QCOMPARE(busy, (QVector<bool> { true, false, true, false }));
        QVERIFY(!engine.isBusy());
        QCOMPARE(engine.conversation().size(), 5); // + the second answer
    }

    void approvalIsAskedOnTheGuiThread_data() { plainAnswerAndFollowUp_data(); }
    void approvalIsAskedOnTheGuiThread()
    {
        QFETCH(bool, threaded);
        StubServer server;
        QVERIFY(server.isListening());
        ChatEngine engine(&m_registry, &m_dispatcher, threaded);
        engine.setProfile(stubProfile(server));
        engine.setAutonomy(ToolAutonomy::Ask);

        bool askedOnGui = false;
        int asked = 0;
        QStringList order;
        connect(&engine, &ChatEngine::toolStarted, this, [&order](const QString& n, const QJsonObject&) { order << QStringLiteral("started:") + n; });
        engine.setApprovalPolicy([&](const ToolSpec& spec, const QJsonObject&) {
            ++asked;
            askedOnGui = QThread::currentThreadId() == m_guiThread;
            order << QStringLiteral("asked:") + spec.name;
            return false; // refuse
        });

        const int before = m_computeCalls;
        server.enqueue(callResponse("compute_test", "c1"));
        server.enqueue(textResponse("understood"));
        QCOMPARE(runTurn(engine, QStringLiteral("run it")), QStringLiteral("understood"));
        QCOMPARE(asked, 1);
        QVERIFY2(askedOnGui, "the approval policy must run on the GUI thread");
        QCOMPARE(m_computeCalls, before); // refused: never ran
        // the view hears about the call before the question is put (the recorder relies on it)
        QCOMPARE(order, (QStringList { QStringLiteral("started:compute_test"), QStringLiteral("asked:compute_test") }));
    }

    void aWaitingToolDoesNotFreezeTheGuiThread()
    {
        StubServer server;
        QVERIFY(server.isListening());
        ChatEngine engine(&m_registry, &m_dispatcher, true);
        engine.setProfile(stubProfile(server));

        int ticks = 0;
        QTimer ticker;
        ticker.setInterval(20);
        connect(&ticker, &QTimer::timeout, this, [&ticks] { ++ticks; });
        ticker.start();

        m_waitedOnThread = 0;
        server.enqueue(callResponse("wait_test", "w1"));
        server.enqueue(textResponse("waited"));
        QCOMPARE(runTurn(engine, QStringLiteral("wait")), QStringLiteral("waited"));
        QCOMPARE(m_waitedOnThread, 1);            // the tool did run off the GUI thread
        QVERIFY2(ticks >= 10, qPrintable(QStringLiteral("only %1 GUI ticks during a 400 ms wait").arg(ticks)));
    }

    void conversationCanBeAskedWhileAGuiToolIsInFlight()
    {
        // A Gui-affinity tool is run on the GUI thread on behalf of the worker. A GUI thread that sat
        // blocked in conversation() at that moment would never run it: both would wait for each other.
        ToolSpec spec;
        spec.name = QStringLiteral("gui_test");
        spec.description = QStringLiteral("runs on the GUI thread");
        spec.category = QStringLiteral("test");
        spec.effect = ToolEffect::Read;
        spec.affinity = ToolAffinity::Gui;
        spec.paramSchema = QJsonObject{ { QStringLiteral("type"), QStringLiteral("object") } };
        spec.handler = [](const QJsonObject&) { return ToolResult::success(QJsonObject{ { QStringLiteral("ok"), true } }); };
        QVERIFY(m_registry.add(spec));

        StubServer server;
        QVERIFY(server.isListening());
        ChatEngine engine(&m_registry, &m_dispatcher, true);
        engine.setProfile(stubProfile(server));
        server.enqueue(callResponse("gui_test", "g1"));
        server.enqueue(textResponse("fine"));

        QEventLoop loop;
        connect(&engine, &ChatEngine::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(8000, &loop, &QEventLoop::quit);
        // toolStarted reaches this thread just as the worker hands the Gui-affinity call over to it:
        // asking for the conversation right there is the moment a blocking getter would deadlock
        int asked = 0;
        connect(&engine, &ChatEngine::toolStarted, this, [&](const QString&, const QJsonObject&) {
            ++asked;
            engine.conversation();
        });
        engine.ask(QStringLiteral("go"));
        loop.exec();
        QVERIFY(!engine.isBusy());
        QVERIFY2(engine.conversation().size() >= 4, "the turn did not complete (deadlock?)");
        QVERIFY(asked > 0);
    }

    void modelOverrideBelongsToItsProfile()
    {
        StubServer server;
        ChatEngine engine(&m_registry, &m_dispatcher, false);
        LlmProfile a = stubProfile(server);
        engine.setProfile(a);
        QCOMPARE(engine.model(), QStringLiteral("test-model"));
        engine.setModel(QStringLiteral("other"));
        QCOMPARE(engine.model(), QStringLiteral("other"));
        engine.setProfile(a); // same endpoint: the choice stays
        QCOMPARE(engine.model(), QStringLiteral("other"));
        a.name = QStringLiteral("second");
        engine.setProfile(a); // another profile: back to its own model
        QCOMPARE(engine.model(), QStringLiteral("test-model"));
        // and the mirror is what the client really does
        QCOMPARE(engine.client()->model(), engine.model());
    }

    void destroyingDuringATurnJoinsTheWorker()
    {
        StubServer server;
        QVERIFY(server.isListening());
        {
            ChatEngine engine(&m_registry, &m_dispatcher, true);
            engine.setProfile(stubProfile(server));
            m_waitedOnThread = 0;
            server.enqueue(callResponse("wait_test", "w2"));
            server.enqueue(textResponse("never read"));
            engine.ask(QStringLiteral("go"));
            QTest::qWait(100); // the tool is now sleeping on the worker
        } // ~ChatEngine: cancel, quit, join
        QVERIFY(true); // reaching this line without a crash or a hang is the check
    }
};

QTEST_GUILESS_MAIN(ChatEngineTest)
#include "test_chatengine.moc"
