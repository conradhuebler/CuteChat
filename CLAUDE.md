# CuteChat - Development Guide

Shared Qt6 library: LLM tool-calling agent core (`cutechat_core`) + chat dock (`cutechat`). Used by SupraFit and
qurcuma. Design and migration notes: [docs/DESIGN.md](docs/DESIGN.md).

## Rules
- **Free of application code**: no SupraFit/qurcuma header, no colour constants (palette roles only). Application
  specifics enter through `ChatHost` (chathost.h) and `ChatBackend` (chatbackend.h).
- Public headers live in `include/cutechat/` (`#include <cutechat/x.h>`), sources in `src/` and `src/widgets/`.
  New header with a Q_OBJECT: add it to the target's source list in CMakeLists.txt (AUTOMOC does not look in include/).
- Global class names are kept on purpose (`ToolSpec`, `LlmClient`, ...); private helpers get a `Chat` prefix (`ChatFlowLayout`).
- The core stays GUI-free (Qt Core + Network). Widgets only in `src/widgets/`, behind `CUTECHAT_BUILD_WIDGETS`.
- `LlmSession` must not run tools inside the network-reply slot (deferred with `QTimer::singleShot(0)`).
- JSON format tags: `ChatLog::setFormatPrefix()`; reading accepts any `<prefix>-chat`. Keep the keys (`projects`, `agent_chats`).
- Qt 6.11 moc: no raw string literals with `\"` before a Q_OBJECT class in the same file; use plain concatenation.
- Mark new functions "Claude Generated"; copyright `Copyright (C) 2016 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>`.
- AI never marks anything TESTED/APPROVED; that is the operator's.

## Program help
An application ships a how-to document (Markdown, sections `## N. Title`) and returns it from `ChatHost::helpText()`. `ChatHelp` (`chathelp.h`, Qt Core only)
gives the model two ways to it: the tool `get_help` (sections on demand) and `ChatHelp::Mode::Prompt` (whole text appended to the system prompt, the
dock's menu entry "Program help in the prompt", default `helpInPromptByDefault()`). Headless runners use `ChatHelp` directly. The library never
interprets the text; keeping it complete is the application's rule (SupraFit: `docs/ASSISTANT_HELP.md`, checked by its `AgentHelpTest`).

## Threading
`ChatEngine` owns client + session, on a worker thread when the host asks (`ChatHost::agentOnWorkerThread`); the dock only talks to it.
Signals arrive on the GUI thread, setters are posted, the approval policy runs on the GUI thread. Never block the GUI thread on the
worker (`BlockingQueuedConnection`): the worker may be waiting for the GUI thread (Gui-affinity tool, approval) — `conversation()` waits in an event loop for that reason.

## Tests (QtTest, no network, no model)
`ToolRegistryTest`, `ToolDispatcherTest`, `LlmClientTest`, `LlmSessionTest`, `ChatLogTest`, `LlmConfigPresetTest`,
`ChatEngineTest` (both threading modes), `ChatHelpTest`, `CuteChatExtrasTest` (reasoning effort, vision, free rounds, log sink, updateBaseUrl, backend), `ChatDockTest` (offscreen,
stub endpoint + test host). Stub endpoint: `tests/test_stubserver.h`. Never send chat requests to a real model in a test.

## Open items
See TODO.md.
