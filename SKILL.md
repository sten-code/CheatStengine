---
name: cheatstengine
description: Drive Cheat Stengine (a Windows reverse-engineering tool) over MCP: list/attach processes, read/write and scan memory, disassemble, assemble, dissect structs, and generate byte signatures. Use when inspecting or modifying a running process's memory on this machine.
---

# Cheat Stengine MCP

This tool exposes a live reverse-engineering engine over MCP at `http://127.0.0.1:13777/mcp`.
Auth is off by default, so you can connect with just the URL. If the user turns auth on, send `Authorization: Bearer 2187c0e6df08f002e36a6303069998a5341e94ade9aec213cf81cfe083229c3a`.

## Workflow

1. `list_processes` - find the target's pid.
2. `open_process` { pid } - attach. Everything below needs an attached process.
3. Inspect: `list_modules`, `read_memory`, `query_memory`, `disassemble`, `dissect_struct`.
4. Resolve addresses symbolically with `resolve_address` { expression: "module.dll+0x1234" }; most tools also accept an expression string directly in place of a numeric address.
5. Modify: `write_memory` { address, hex }, or `assemble` { source, address, write: true }.

## Async tools (poll, don't block)

- `pattern_scan` { pattern } returns a `jobId`. Poll `job_status` { id } until status is `completed`, then read `result.matches`.
- `scan_value` { valueType, scanType, value } starts a value scan. Poll `scan_results` until `scanning` is false. Refine with `scan_value` { ..., next: true }.

## Notes

- Addresses in results are hex strings like `0x7FF6...`.
- Value types: int8/16/32/64, uint8/16/32/64, float, double.
- Scan types: exact, bigger, smaller, between, unknown.
- `pattern` is space-separated hex with `??` wildcards, e.g. `48 8B ?? C3`.
