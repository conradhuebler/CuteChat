// chathelp.cpp - see chathelp.h
// Copyright (C) 2016 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026
#include <cutechat/chathelp.h>

#include <cutechat/tool.h>
#include <cutechat/toolregistry.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>

ChatHelp::ChatHelp(const QString& markdown)
{
    m_text = markdown.trimmed();
    // the copyright / provenance comment at the top is for people, not for the model
    if (m_text.startsWith(QLatin1String("<!--"))) {
        const int end = m_text.indexOf(QLatin1String("-->"));
        if (end > 0)
            m_text = m_text.mid(end + 3).trimmed();
    }
    static const QRegularExpression heading(QStringLiteral("^## (\\d+)\\.\\s*(.+)$"), QRegularExpression::MultilineOption);
    QVector<QRegularExpressionMatch> found;
    QRegularExpressionMatchIterator it = heading.globalMatch(m_text);
    while (it.hasNext())
        found.append(it.next());
    for (int i = 0; i < found.size(); ++i) {
        HelpSection section;
        section.number = found[i].captured(1).toInt();
        section.title = found[i].captured(2).trimmed();
        const int start = found[i].capturedStart();
        const int end = i + 1 < found.size() ? found[i + 1].capturedStart() : m_text.size();
        section.text = m_text.mid(start, end - start).trimmed();
        m_sections.append(section);
    }
}

QString ChatHelp::section(const QString& topic) const
{
    const QString wanted = topic.trimmed();
    if (wanted.isEmpty())
        return QString();
    bool isNumber = false;
    const int number = wanted.toInt(&isNumber);
    if (isNumber)
        for (const HelpSection& s : m_sections)
            if (s.number == number)
                return s.text;
    for (const HelpSection& s : m_sections)
        if (s.title.contains(wanted, Qt::CaseInsensitive))
            return s.text;
    return QString();
}

int ChatHelp::estimatedTokens() const
{
    return int(m_text.size() / 3.5);
}

int ChatHelp::registerTool(ToolRegistry& registry) const
{
    if (isEmpty() || registry.contains(QStringLiteral("get_help")))
        return 0;
    const ChatHelp help = *this; // the handler outlives this object; the text is shared, not copied
    ToolSpec spec;
    spec.name = QStringLiteral("get_help");
    spec.description = QStringLiteral(
        "How to do things in this program: menus and buttons, data formats, workflows, settings. Call it for any "
        "how-do-I question before answering from memory. Without 'topic' it lists the sections; with a section "
        "number or a word of its title it returns that section.");
    spec.category = QStringLiteral("help");
    spec.paramSchema = QJsonObject{ { QStringLiteral("type"), QStringLiteral("object") },
        { QStringLiteral("properties"), QJsonObject{ { QStringLiteral("topic"), QJsonObject{ { QStringLiteral("type"), QStringLiteral("string") }, { QStringLiteral("description"), QStringLiteral("Section number (e.g. \"3\") or a word of its title. Empty: list the sections.") } } } } } };
    spec.effect = ToolEffect::Read;
    spec.affinity = ToolAffinity::Any;
    spec.handler = [help](const QJsonObject& args) -> ToolResult {
        const QString topic = args.value(QStringLiteral("topic")).toString().trimmed();
        if (topic.isEmpty()) {
            QString list;
            QJsonArray topics;
            for (const HelpSection& s : help.sections()) {
                list += QStringLiteral("%1. %2\n").arg(s.number).arg(s.title);
                topics.append(QJsonObject{ { QStringLiteral("number"), s.number }, { QStringLiteral("title"), s.title } });
            }
            return ToolResult::success(QJsonObject{ { QStringLiteral("sections"), topics } }, list.trimmed());
        }
        const QString text = help.section(topic);
        if (text.isEmpty())
            return ToolResult::failure(QStringLiteral("no help section matches \"%1\"; call get_help without a topic to list them").arg(topic));
        return ToolResult::success(QJsonObject(), text);
    };
    return registry.add(spec) ? 1 : 0;
}

QString ChatHelp::extendPrompt(const QString& base, Mode mode) const
{
    if (isEmpty())
        return base;
    if (mode == Mode::Prompt)
        return base + QStringLiteral("\n\nProgram help (how to do things in this program; answer how-to questions from it):\n\n") + m_text;
    return base + QStringLiteral("\nFor how-to questions about the program itself (menus, buttons, formats, workflows) call get_help first.");
}
