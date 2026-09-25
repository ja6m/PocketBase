#pragma once

#include <string>
#include <map>
#include <optional>
#include <vector>
#include <fstream>
#include <cstdint>

// PocketBase: a minimal append-only-log key-value store.
//
// On-disk format (repeated for every write):
//   [uint32_t key_len][key bytes][uint32_t value_len][value bytes][uint8_t tombstone]
//
// - tombstone == 0  -> normal record, value_len/value are meaningful
// - tombstone == 1  -> deletion marker, value_len is 0 and no value bytes follow
//
// On startup we replay the whole log sequentially to rebuild an in-memory
// index (key -> byte offset of that key's most recent record). This is the
// classic Bitcask design: writes are O(1) appends, reads are one index
// lookup plus one disk seek+read, and durability comes from the fact that
// every mutation is fsync'd to disk before we consider it complete.
// Crucially, RAM usage now scales with the number of keys, not the total
// size of the values -- the log on disk can be far bigger than memory.
//
// The index is a std::map (a red-black tree) rather than a hash map, so
// keys are always stored in sorted order. That costs O(log n) per put/get
// instead of unordered_map's amortized O(1), but it means prefix and range
// scans can jump straight to the first matching key with lower_bound() and
// walk forward only over matches, instead of scanning every key in the store.
class KVStore {
public:
    // Opens (or creates) the database at `path` and replays its log.
    explicit KVStore(const std::string& path);
    ~KVStore();

    // Basic operations
    void put(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    void del(const std::string& key);

    // Returns all keys currently starting with `prefix` (prefix == "" -> all keys),
    // in sorted order. O(log n + k) where k is the number of matches, since the
    // sorted index lets us seek straight to the first match instead of scanning
    // every key.
    std::vector<std::string> scan(const std::string& prefix = "") const;

    // Returns all keys in the inclusive range [start, end], in sorted order.
    // Pass "" for `end` to mean "no upper bound" (i.e. scan to the last key).
    std::vector<std::string> range(const std::string& start, const std::string& end = "") const;

    // Rewrites the log file so it only contains the current, live key/value
    // pairs (drops old overwritten values and tombstoned deletes). Useful
    // once the log has grown much larger than the actual live dataset.
    void compact();

    // Number of live keys currently in the store.
    size_t size() const { return index_.size(); }

private:
    void loadFromLog();
    // Appends a record and returns the byte offset where it starts,
    // so the caller can record that offset in the index.
    uint64_t appendRecord(const std::string& key, const std::string& value, bool tombstone);
    // Reads the value stored at `offset` by seeking directly to it.
    std::string readValueAt(uint64_t offset) const;

    std::string path_;
    // mutable: get() is logically const (it doesn't change what the store
    // contains) but still needs to seek/read the underlying stream.
    mutable std::fstream log_;
    // In-memory index: key -> byte offset of its most recent record in the
    // log, kept in sorted order by std::map. The value itself lives on disk
    // and is read lazily on get().
    std::map<std::string, uint64_t> index_;
};
