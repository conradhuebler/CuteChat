# CuteChat

An LLM agent core and a chat dock for Qt6 applications, in the spirit of CuteChart: a small,
shareable library that two applications (SupraFit, qurcuma) use instead of each keeping its own copy.

* **`cutechat_core`** (Qt Core + Network): tool contract (`ToolSpec`, `ToolResult`, effects, autonomy),
  `ToolRegistry` (schema check, catalogue), `ToolDispatcher` (thread marshalling, audit), `LlmClient`
  (OpenAI-compatible chat completions, SSE streaming, tool calls, model listing, Ollama capabilities),
  `LlmConfig` + `LlmPresets` (profiles in `llm.json`, never a key), `LlmSession` (agent loop with
  approval policy, vision, round budget), `ChatLog`/`ChatRecorder`/`ChatBackend` (chat history, audit trail,
  storage per context).
* **`cutechat`** (+ Qt Widgets, optional): `ChatDock` (Markdown bubbles, streaming, tool lines, history,
  export/import, maximise) and `EndpointDialog` (endpoint manager). Palette aware, no hard-coded colours.

The application stays in charge of what is specific to it: its tools, the system prompt and where chats
are stored. See [docs/DESIGN.md](docs/DESIGN.md).

## Integration in a dozen lines

```cpp
#include <cutechat/chatdock.h>
#include <cutechat/chathost.h>

class MyHost : public ChatHost {
public:
    QString systemPrompt() const override { return QStringLiteral("You operate MyApp through tools."); }
    void registerTools(ToolRegistry& registry) override { registerMyTools(registry); }   // your ToolSpec handlers
    ChatBackend* chatBackend() override { return &m_chats; }                               // optional
    FileChatBackend m_chats;
};

MyHost host;
auto* dock = new ChatDock(&host, mainWindow);
mainWindow->addDockWidget(Qt::RightDockWidgetArea, dock);
dock->setContext(currentDocumentId());   // when the thing a chat is bound to changes
```

`llm.json` is created in `QStandardPaths::AppConfigLocation` (set `QCoreApplication::organizationName` and
`applicationName`); the first start writes a profile for a local Ollama on `127.0.0.1:11434`.

## CMake

```cmake
add_subdirectory(external/CuteChat)           # or: find_package(CuteChat)
target_link_libraries(myapp PRIVATE CuteChat::cutechat)       # dock + dialog
target_link_libraries(mycli PRIVATE CuteChat::core)           # headless: no Widgets
```

Options: `CUTECHAT_BUILD_WIDGETS` (ON), `CUTECHAT_BUILD_TESTS` (ON when top-level), `CUTECHAT_INSTALL`.
Tests: `cmake -S . -B build && cmake --build build && ctest --test-dir build` (QtTest, offscreen, a stub server
on `127.0.0.1`, no network, no model).

## Licence

GPL-3.0-or-later (see `LICENSE`), Copyright (C) 2016 - 2026 Conrad Hübler. `src/widgets/flowlayout.*` is Qt example
code under its BSD licence (header kept).
