#pragma once

#include <string>
#include <unordered_map>
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
// index (key -> latest value). This is the "Bitcask" style design: writes
// are O(1) appends, reads are O(1) hashmap lookups, and durability comes
// from the fact that every mutation is fsync'd to disk before we consider
// it complete.
class KVStore {
public:
    // Opens (or creates) the database at `path` and replays its log.
    explicit KVStore(const std::string& path);
    ~KVStore();

    // Basic operations
    void put(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    void del(const std::string& key);

    // Returns all keys currently starting with `prefix` (prefix == "" -> all keys).
    std::vector<std::string> scan(const std::string& prefix = "") const;

    // Rewrites the log file so it only contains the current, live key/value
    // pairs (drops old overwritten values and tombstoned deletes). Useful
    // once the log has grown much larger than the actual live dataset.
    void compact();

    // Number of live keys currently in the store.
    size_t size() const { return index_.size(); }

private:
    struct RecordHeader {
        uint32_t key_len;
        uint32_t value_len;
        uint8_t tombstone;
    };

    void loadFromLog();
    void appendRecord(const std::string& key, const std::string& value, bool tombstone);

    std::string path_;
    std::fstream log_;
    // In-memory index: key -> current value. Simple and effective for a
    // toy project; a real engine would store a file offset here instead
    // and read the value lazily from disk.
    std::unordered_map<std::string, std::string> index_;
};
