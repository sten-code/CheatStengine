<h1>
  <img src="Images/128x128.png" width="48" height="48" align="center" alt="Cheat Stengine Logo">
  Cheat Stengine
</h1>

Cheat Stengine is a reverse engineering tool for Windows
The goal is to create a tool that has a better user experience than Cheat Engine, while still providing powerful
features for reverse engineering, debugging, memory analysis, and game hacking.

> [!WARNING]
> Cheat Stengine is still in the very early stages of development. Many features are not yet implemented, and the tool
> may be unstable. Expect bugs, crashes, and breaking changes between versions.

> [!NOTE]
> If you're on an AMD chipset, you might face some issues. If you do, please copy the generated dump file and create an issue
> with any relevant information that could help us reproduce the problem.
>
> We are not responsible for individual data loss, system instability, or other issues caused by using Cheat Stengine.
> Use it at your own risk.

## Preview

<p align="center">
    <img src="https://raw.githubusercontent.com/sten-code/CheatStengine/master/Images/preview.png" width="900"/>
</p>

## Compiling from Source

```bash
cmake -S . -B build
cmake --build build --config Release
```

## MCP Server

Cheat Stengine ships with a built-in MCP server so an AI agent can drive the
engine: list and attach processes, read/write and scan memory, disassemble,
assemble, dissect structs, and generate byte signatures.

The server starts automatically on launch, binds to loopback, and prints its
URL and bearer token to the log. Open the **MCP** tab in Settings
(`Ctrl+,`) to:

* toggle the server on or off,
* require the bearer token (off by default, since the loopback bind is the
  gate),
* copy the endpoint and token,
* open the web dashboard and configuration pages, and
* reinstall the client configs for Claude Code, Cursor, Claude Desktop, Codex
  and the skill.

The dashboard (served at the printed URL) shows live sessions, jobs and the
tool catalogue, and lets you enable or disable individual tools. The
configuration page at `/config.html` manages the connection and install
targets.

## Features

* [x] Pattern Scanner
* [x] MCP Support
* [x] Disassembler
* [x] Assembler
* [x] Struct Dissector
* [x] Memory Scanner
* [x] Address Watcher
* [x] Module List
* [x] PE Viewer
* [x] Kernel Mode
* [x] Pattern Generator
* [ ] Process Dumper
* [ ] String Scanner/Viewer
* [ ] Code Cave Scanner
* [ ] Code Injection
* [ ] Syscall Tracer
* [ ] Lua Scripting
* [ ] Pointer Scanner
* [ ] Memory Viewer
* [ ] Debugger
* [ ] Thread Explorer
* [ ] Handle Viewer
* [ ] Plugin System
* [ ] DBVM

## Development

Cheat Stengine is under active development. Features marked as incomplete are planned or currently being worked on.
The project is still experimental, so functionality and APIs may change as development progresses.

Bug reports and contributions are welcome. If you encounter a crash, please include relevant logs or dump files when
creating an issue.