# CuteChat - open items
- `ChatEngine`: owns client + session on a worker thread with marshalled setters, so qurcuma can use `ChatDock` itself.
- Dock: collapsible reasoning block per turn, thinking-effort and rounds controls (qurcuma has them; opt-in via `ChatUiTexts`/options).
- Generic headless runner (SupraFit's `agentrunner`) once a second application needs it.
- Remote repository URL; then `external/CuteChat` as a git submodule in both applications.
