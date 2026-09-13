# PocketBase

A minimal append-only-log key-value store.

## Overview

PocketBase is a lightweight, high-performance key-value store built around an append-only log architecture, in the style of Bitcask. Every write is sequentially appended to a single log file on disk, which keeps writes fast (`O(1)` appends) and durable, since a mutation is only considered complete once it has been flushed to disk. On startup, the store replays the entire log to rebuild an in-memory index mapping each key to its latest value, so reads are simple `O(1)` hashmap lookups.

Because writes are only ever appended, overwritten and deleted keys leave stale records behind in the log. A `COMPACT` operation rewrites the log to contain only the current, live key/value pairs, reclaiming disk space once the log has grown much larger than the actual dataset.

### On-disk format

The database is a single binary log file (e.g. `pocketdb.log`). Every `SET` or `DEL` appends one fixed-layout record to the end of that file — nothing already on disk is ever rewritten or overwritten in place:

```
[uint32_t key_len][key bytes][uint32_t value_len][value bytes][uint8_t tombstone]
```

- `key_len` / `value_len` are 4-byte lengths telling the reader how many bytes of key/value data follow, so keys and values can be arbitrary binary data of any length.
- `tombstone` is a single byte: `0` for a normal write (`value_len`/value bytes are meaningful), `1` for a deletion marker (`value_len` is written as `0` and no value bytes follow).
- A `SET` appends a fresh record with the new value; it does not touch or erase the previous record for that key, which simply becomes stale.
- A `DEL` appends a tombstone record rather than deleting anything from the file.
- Each append is flushed to disk before the call returns, so a completed `SET`/`DEL` is durable even if the process crashes immediately afterward.

On startup, `KVStore` seeks to the beginning of the file and replays every record in order into an in-memory `unordered_map<key, value>`: a normal record sets/overwrites that key's entry, a tombstone erases it. Whichever record for a given key was written last "wins," so after replay the map reflects only the live data, even though the log itself may still contain older, stale records for the same keys. All reads (`GET`, `SCAN`, `COUNT`) are served entirely from this in-memory index rather than by scanning the file.

`COMPACT` reclaims the space taken up by that stale data: it writes the current in-memory index out to a temp file as one fresh, non-tombstone record per live key, then closes the log, deletes the old file, and renames the temp file into its place — leaving a log with exactly one record per live key and nothing else.

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
