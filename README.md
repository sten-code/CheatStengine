# Cheat Stengine

Cheat Stengine is a reverse engineering tool for Windows processes.
The goal is to create a tool that has a better user experience than Cheat Engine, while still providing powerful
features for reverse engineering and game hacking.

> [!WARNING]
> Cheat Stengine is still in the very early stages of development. Many features are not yet implemented, and the tool
> may be unstable.

## Preview

<p align="center">
    <img src="https://raw.githubusercontent.com/sten-code/CheatStengine/master/Images/preview.png" width="900"/>
</p>

## Compiling from Source

```console
cmake -S . -B build
cmake --build build --config Release
```

## Features

- [x] Pattern Scanner
- [x] Disassembler
- [x] Assembler
- [x] Struct Dissector
- [x] Memory Scanner
- [x] Address Watcher
- [x] Module List
- [x] PE Viewer
- [x] Kernel Mode
- [ ] Process Dumper
- [ ] String Scanner/Viewer
- [ ] Code cave scanner
- [ ] Code injection
- [ ] Syscall Tracer
- [ ] Lua scripting
- [ ] Pointer Scanner
- [ ] Memory Viewer
- [ ] Debugger
- [ ] Thread Explorer
- [ ] Handle Viewer
- [ ] Plugin System