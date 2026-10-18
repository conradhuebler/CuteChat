// chathelp.h - How-to help for the assistant: a Markdown text the application ships.
// Copyright (C) 2016 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - An assistant that drives a program through tools still gets asked "how do I
// import a thermogram?". The application writes that answer once, as a Markdown document whose
// sections start with "## <number>. <title>", and hands it over (ChatHost::helpText). The library
// offers the text to the model in two ways that the user can choose between:
//
//   Mode::Tool    one line in the system prompt plus the tool get_help: the model fetches a section
//                 when a question needs it. Costs tokens only when asked; needs a model that does look things up.
//   Mode::Prompt  the whole text is appended to the system prompt. Always at hand, but its length is
//                 paid on every request. get_help stays available.
//
// The text is not interpreted beyond cutting it into sections. Qt Core only (cutechat_tools).
#pragma once

#include <QString>
#include <QVector>

class ToolRegistry;

struct HelpSection {
    int number = 0;   ///< "## 3. Adding a model" -> 3
    QString title;    ///< "Adding a model"
    QString text;     ///< the section including its heading line
};

class ChatHelp {
public:
    enum class Mode { Tool, Prompt };

    ChatHelp() = default;
    /// @p markdown may start with an HTML comment (copyright, provenance); it is dropped.
    explicit ChatHelp(const QString& markdown);

    bool isEmpty() const { return m_text.isEmpty(); }
    /// The whole document without the leading comment.
    const QString& text() const { return m_text; }
    /// Sections in document order; the introduction before the first numbered one is not a section.
    const QVector<HelpSection>& sections() const { return m_sections; }
    /// A section by number ("5") or by a part of its title (case-insensitive); empty if none matches.
    QString section(const QString& topic) const;
    /// Rough size of text() in tokens (characters / 3.5), so a setting can say what Mode::Prompt costs.
    int estimatedTokens() const;

    /// Registers get_help (Read, any thread) into @p registry unless it has one already. Returns 1 if added.
    int registerTool(ToolRegistry& registry) const;
    /// @p base extended for @p mode; @p base unchanged if there is no help.
    QString extendPrompt(const QString& base, Mode mode) const;

private:
    QString m_text;
    QVector<HelpSection> m_sections;
};
