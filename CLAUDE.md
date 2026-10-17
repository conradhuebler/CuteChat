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

## Tests (QtTest, no network, no model)
`ToolRegistryTest`, `ToolDispatcherTest`, `LlmClientTest`, `LlmSessionTest`, `ChatLogTest`, `LlmConfigPresetTest`,
`CuteChatExtrasTest` (reasoning effort, vision, free rounds, log sink, updateBaseUrl, backend), `ChatDockTest` (offscreen,
stub endpoint + test host). Stub endpoint: `tests/test_stubserver.h`. Never send chat requests to a real model in a test.

## Open items
See TODO.md.
