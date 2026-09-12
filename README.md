# PocketBase

A minimal append-only-log key-value store.

## Overview

PocketBase is a lightweight, high-performance key-value store built around an append-only log architecture. All writes are sequentially appended to disk, ensuring durability and fast write throughput, with background compaction support to clean up stale and deleted records.

## Interactive REPL Commands

Once running, you can interact with PocketBase using the following commands:

| Command | Description |
| :--- | :--- |
| `SET <key> <value...>` | Store a value (values may contain spaces). |
| `GET <key>` | Fetch the value associated with a given key. |
| `DEL <key>` | Delete an existing key from the store. |
| `SCAN [prefix]` | List all keys, optionally filtered by an optional string prefix. |
| `COMPACT` | Rewrite the underlying log file to drop stale and deleted entries, reclaiming disk space. |
| `COUNT` | Display the current number of live keys in the store. |
| `HELP` | Display the interactive help message. |
| `EXIT` / `QUIT` | Leave and terminate the REPL session. |
