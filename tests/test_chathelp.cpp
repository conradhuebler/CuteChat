/*
 * CuteChat - ChatHelp: sections, get_help tool, prompt modes
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
/* Claude Generated 2026. */

#include <QtTest/QtTest>

#include <cutechat/chathelp.h>
#include <cutechat/tooldispatcher.h>
#include <cutechat/toolregistry.h>

namespace {
const char* kDoc =
    "<!-- Copyright, provenance: not for the model -->\n"
    "\n"
    "# Program help\n"
    "Introduction line.\n"
    "\n"
    "## 1. Getting data in\n"
    "Use **Open Project**.\n"
    "\n"
    "## 2. Fitting models\n"
    "Press **Fit**.\n"
    "\n"
    "## 3. Statistics\n"
    "Monte Carlo and friends.\n";
}

class ChatHelpTest : public QObject {
    Q_OBJECT
private slots:
    void sectionsAreCutAtNumberedHeadings()
    {
        const ChatHelp help { QString::fromUtf8(kDoc) };
        QVERIFY(!help.isEmpty());
        QVERIFY2(!help.text().contains(QLatin1String("provenance")), "the leading comment must not reach the model");
        QCOMPARE(help.sections().size(), 3);
        QCOMPARE(help.sections().at(1).number, 2);
        QCOMPARE(help.sections().at(1).title, QStringLiteral("Fitting models"));
        QVERIFY(help.sections().at(1).text.startsWith(QLatin1String("## 2. Fitting models")));
        QVERIFY(help.sections().at(1).text.contains(QLatin1String("Press **Fit**.")));
        QVERIFY(!help.sections().at(1).text.contains(QLatin1String("Monte Carlo"))); // stops at the next heading
    }

    void sectionByNumberOrTitle()
    {
        const ChatHelp help { QString::fromUtf8(kDoc) };
        QVERIFY(help.section(QStringLiteral("1")).contains(QLatin1String("Open Project")));
        QVERIFY(help.section(QStringLiteral("stat")).contains(QLatin1String("Monte Carlo")));
        QVERIFY(help.section(QStringLiteral("FITTING")).contains(QLatin1String("Fit")));
        QVERIFY(help.section(QStringLiteral("nothing like it")).isEmpty());
        QVERIFY(help.section(QString()).isEmpty());
        QVERIFY(help.section(QStringLiteral("9")).isEmpty());
    }

    void toolListsAndFetchesSections()
    {
        ToolRegistry registry;
        ToolDispatcher dispatcher(&registry);
        const ChatHelp help { QString::fromUtf8(kDoc) };
        QCOMPARE(help.registerTool(registry), 1);
        QCOMPARE(help.registerTool(registry), 0); // already there: a shared registry is not asked twice
        ToolSpec spec;
        QVERIFY(registry.tool(QStringLiteral("get_help"), spec));
        QCOMPARE(spec.effect, ToolEffect::Read); // runs without a question

        const ToolResult list = dispatcher.dispatch(QStringLiteral("get_help"), QJsonObject());
        QVERIFY(list.ok);
        QVERIFY(list.text.contains(QLatin1String("3. Statistics")));
        const ToolResult one = dispatcher.dispatch(QStringLiteral("get_help"), QJsonObject{ { QStringLiteral("topic"), QStringLiteral("2") } });
        QVERIFY(one.ok);
        QVERIFY(one.text.contains(QLatin1String("Press **Fit**.")));
        const ToolResult none = dispatcher.dispatch(QStringLiteral("get_help"), QJsonObject{ { QStringLiteral("topic"), QStringLiteral("zzz") } });
        QVERIFY(!none.ok);
        QVERIFY(none.error.contains(QLatin1String("without a topic")));
    }

    void promptModes()
    {
        const ChatHelp help { QString::fromUtf8(kDoc) };
        const QString base = QStringLiteral("You are an assistant.");
        const QString asTool = help.extendPrompt(base, ChatHelp::Mode::Tool);
        QVERIFY(asTool.startsWith(base));
        QVERIFY(asTool.contains(QLatin1String("get_help")));
        QVERIFY2(!asTool.contains(QLatin1String("Open Project")), "tool mode must not carry the text");
        const QString inPrompt = help.extendPrompt(base, ChatHelp::Mode::Prompt);
        QVERIFY(inPrompt.contains(QLatin1String("Open Project")));
        QVERIFY(inPrompt.contains(QLatin1String("Monte Carlo")));
        QVERIFY(!inPrompt.contains(QLatin1String("provenance")));
        QVERIFY(help.estimatedTokens() > 0);

        const ChatHelp none;
        QVERIFY(none.isEmpty());
        QCOMPARE(none.extendPrompt(base, ChatHelp::Mode::Prompt), base);
        ToolRegistry registry;
        QCOMPARE(none.registerTool(registry), 0); // no help: no tool for the model to call in vain
    }
};

QTEST_GUILESS_MAIN(ChatHelpTest)
#include "test_chathelp.moc"
