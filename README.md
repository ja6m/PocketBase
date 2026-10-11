# PocketBase

A minimal append-only-log key-value store.

## Overview

PocketBase is a small, from-scratch key-value store written in C++17 as a learning project. It is built around an append-only log, in the style of Bitcask. Every write is appended to the end of a single log file, so a write is one `O(1)` append and is flushed to the OS before it counts as complete. On startup the store replays the whole log to rebuild an in-memory index. The index maps each key to the byte offset of its latest record, and the values stay on disk. This means RAM usage grows with the number of keys, not with the size of the values.

The index is a sorted `std::map`, not a hash map. Lookups cost `O(log n)` instead of `O(1)`. In exchange, keys are always in order, so prefix and range scans can jump straight to the first matching key and walk forward only over the matches.

Because writes are only ever appended, overwritten and deleted keys leave stale records behind in the log. A `COMPACT` operation rewrites the log to contain only the current, live key/value pairs, reclaiming disk space once the log has grown much larger than the actual dataset.

### On-disk format

The database is a single binary log file (e.g. `pocketdb.log`). Every `SET` or `DEL` appends one record to the end of that file, and nothing already on disk is rewritten or overwritten in place:

```
[uint32_t key_len][key bytes][uint32_t value_len][value bytes][uint8_t tombstone]
```

- `key_len` / `value_len` are 4-byte lengths telling the reader how many bytes of key/value data follow, so keys and values can be arbitrary binary data of any length.
- `tombstone` is a single byte: `0` for a normal write (`value_len`/value bytes are meaningful), `1` for a deletion marker (`value_len` is written as `0` and no value bytes follow).
- A `SET` appends a fresh record with the new value; it does not touch or erase the previous record for that key, which simply becomes stale.
- A `DEL` appends a tombstone record rather than deleting anything from the file.
- Each append is flushed to the OS before the call returns, so a completed `SET`/`DEL` survives if the process crashes immediately afterward. It is not `fsync`'d, so a power loss or OS crash can still lose recent writes.

On startup, `KVStore` seeks to the beginning of the file and replays every record in order. For each record it notes the record's starting offset and reads the key, then seeks past the value bytes without reading them. A normal record points that key's index entry at its offset, and a tombstone removes the key from the index. Whichever record for a given key was written last "wins," so after replay the index covers only the live data, even though the log may still contain older, stale records for the same keys.

`SCAN`, `RANGE` and `COUNT` are answered from the index alone. `GET` does one `O(log n)` index lookup, then seeks to the stored offset and reads the value from disk.

`COMPACT` reclaims the space taken up by stale data. It reads each live value from the current log and writes it to `<path>.compact.tmp` as one fresh, non-tombstone record per key, building a new key → offset map as it goes. It then closes the log and swaps the temp file into place in a single atomic step: `MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` on Windows, `rename()` on POSIX. At no point is there a moment where neither the old log nor the new one exists. The new offset map replaces the index only after the swap succeeds. If the swap fails, the old log is reopened unchanged, the temp file is removed, and an error is reported.

## Build & run

The Makefile is for Linux with `g++`. The code itself is portable and also targets Windows (MinGW).

```
make            # build ./pocketbase
make run        # build and start the REPL
make debug      # -g -O0 build for gdb
make clean      # remove build/ and the binary
make rebuild
```

```
./pocketbase [db_path]    # opens or creates the log file (default: pocketdb.log)
```

## Interactive REPL Commands

Once running, you can interact with PocketBase using the following commands:

| Command | Description |
| :--- | :--- |
| `SET <key> <value...>` | Store a value (values may contain spaces). |
| `GET <key>` | Fetch the value associated with a given key. |
| `DEL <key>` | Delete an existing key from the store. |
| `SCAN [prefix]` | List keys in sorted order, optionally filtered by a string prefix. |
| `RANGE <start> [end]` | List keys in the inclusive range `[start, end]` in sorted order. If `end` is omitted, the list runs to the last key. |
| `COMPACT` | Rewrite the underlying log file to drop stale and deleted entries, reclaiming disk space. |
| `COUNT` | Display the current number of live keys in the store. |
| `HELP` | Display the interactive help message. |
| `EXIT` / `QUIT` | Leave and terminate the REPL session. |

## Limitations

- No `fsync`: neither appends nor the compaction temp file are forced to disk, so durability holds only against process crashes, not power loss.
- No per-record checksum: a torn write at the end of the log makes startup fail with a "corrupt log" error.
- Not thread-safe.
- The Windows `MoveFileExW` compaction path has not yet been compiled and tested on the Windows toolchain.
