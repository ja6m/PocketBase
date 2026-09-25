#include "kvstore.h"

#include <stdexcept>
#include <cstring>
#include <cstdint>

KVStore::KVStore(const std::string& path) : path_(path) {
    // Open for read+write, create if missing, keep existing contents.
    log_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
    if (!log_.is_open()) {
        // File probably doesn't exist yet -- create it, then reopen in
        // read+write mode.
        std::ofstream create(path_, std::ios::binary);
        create.close();
        log_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
        if (!log_.is_open()) {
            throw std::runtime_error("KVStore: could not open or create log file: " + path_);
        }
    }
    loadFromLog();
}

KVStore::~KVStore() {
    if (log_.is_open()) {
        log_.flush();
        log_.close();
    }
}

void KVStore::loadFromLog() {
    log_.clear();
    log_.seekg(0, std::ios::beg);

    while (true) {
        // Record the offset of THIS record before reading anything, since
        // that's what we'll store in the index if it turns out to be live.
        uint64_t record_offset = static_cast<uint64_t>(log_.tellg());

        uint32_t key_len = 0, value_len = 0;
        uint8_t tombstone = 0;

        log_.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        if (log_.eof()) break;
        if (!log_) throw std::runtime_error("KVStore: corrupt log (key_len)");

        std::string key(key_len, '\0');
        log_.read(key.data(), key_len);
        if (!log_) throw std::runtime_error("KVStore: corrupt log (key bytes)");

        log_.read(reinterpret_cast<char*>(&value_len), sizeof(value_len));
        if (!log_) throw std::runtime_error("KVStore: corrupt log (value_len)");

        // Skip over the value bytes -- we don't need the contents during
        // replay, only their location, so seek past them instead of reading.
        if (value_len > 0) {
            log_.seekg(value_len, std::ios::cur);
            if (!log_) throw std::runtime_error("KVStore: corrupt log (value bytes)");
        }

        log_.read(reinterpret_cast<char*>(&tombstone), sizeof(tombstone));
        if (!log_) throw std::runtime_error("KVStore: corrupt log (tombstone)");

        if (tombstone) {
            index_.erase(key);
        } else {
            index_[key] = record_offset;
        }
    }

    log_.clear(); // reset eof/fail bits before further use
}

uint64_t KVStore::appendRecord(const std::string& key, const std::string& value, bool tombstone) {
    log_.clear();
    log_.seekp(0, std::ios::end);
    // tellp() here (after seeking to end, before writing) is exactly the
    // offset this record will start at -- that's what the index needs.
    uint64_t record_offset = static_cast<uint64_t>(log_.tellp());

    uint32_t key_len = static_cast<uint32_t>(key.size());
    uint32_t value_len = tombstone ? 0 : static_cast<uint32_t>(value.size());
    uint8_t tomb = tombstone ? 1 : 0;

    log_.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
    log_.write(key.data(), key_len);
    log_.write(reinterpret_cast<const char*>(&value_len), sizeof(value_len));
    if (!tombstone) {
        log_.write(value.data(), value_len);
    }
    log_.write(reinterpret_cast<const char*>(&tomb), sizeof(tomb));

    log_.flush(); // durability: don't return until this write hits the OS/disk
    if (!log_) throw std::runtime_error("KVStore: failed writing record to log");

    return record_offset;
}

std::string KVStore::readValueAt(uint64_t offset) const {
    log_.clear();
    log_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);

    uint32_t key_len = 0, value_len = 0;
    log_.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
    if (!log_) throw std::runtime_error("KVStore: corrupt log reading key_len at offset");

    // Skip the key bytes -- we already know the key, we just need the value.
    log_.seekg(key_len, std::ios::cur);

    log_.read(reinterpret_cast<char*>(&value_len), sizeof(value_len));
    if (!log_) throw std::runtime_error("KVStore: corrupt log reading value_len at offset");

    std::string value(value_len, '\0');
    if (value_len > 0) {
        log_.read(value.data(), value_len);
        if (!log_) throw std::runtime_error("KVStore: corrupt log reading value bytes at offset");
    }
    return value;
}

void KVStore::put(const std::string& key, const std::string& value) {
    uint64_t offset = appendRecord(key, value, /*tombstone=*/false);
    index_[key] = offset;
}

std::optional<std::string> KVStore::get(const std::string& key) const {
    auto it = index_.find(key);
    if (it == index_.end()) return std::nullopt;
    return readValueAt(it->second);
}

void KVStore::del(const std::string& key) {
    if (index_.find(key) == index_.end()) return; // nothing to delete
    appendRecord(key, "", /*tombstone=*/true);
    index_.erase(key);
}

std::vector<std::string> KVStore::scan(const std::string& prefix) const {
    std::vector<std::string> result;

    if (prefix.empty()) {
        // No prefix means "every key" -- std::map already keeps them sorted,
        // so a plain in-order walk is all that's needed.
        for (const auto& [k, v] : index_) result.push_back(k);
        return result;
    }

    // lower_bound() jumps straight to the first key >= prefix in O(log n),
    // skipping every key that sorts before it. From there we walk forward
    // only as long as keys still start with `prefix`, then stop the moment
    // they don't -- since the index is sorted, once a key no longer matches
    // the prefix, no later key can either. This is the whole point of
    // switching to std::map: an unordered_map would have to check every
    // single key in the store, with no way to know when to give up early.
    for (auto it = index_.lower_bound(prefix);
         it != index_.end() && it->first.compare(0, prefix.size(), prefix) == 0;
         ++it) {
        result.push_back(it->first);
    }
    return result;
}

std::vector<std::string> KVStore::range(const std::string& start, const std::string& end) const {
    std::vector<std::string> result;
    // Same lower_bound() trick as scan(): seek directly to the first key
    // >= start rather than walking past every smaller key first.
    for (auto it = index_.lower_bound(start); it != index_.end(); ++it) {
        if (!end.empty() && it->first > end) break; // past the requested range
        result.push_back(it->first);
    }
    return result;
}

void KVStore::compact() {
    // Write the live index out to a temp file, then atomically replace
    // the old log with it. This throws away every stale/overwritten
    // value and every tombstone, shrinking the file to just the live set.
    std::string tmp_path = path_ + ".compact.tmp";
    // New offsets for every key in the rewritten file -- since compaction
    // shrinks/reorders the log, every old offset is invalidated and must
    // be replaced before we swap files in.
    std::map<std::string, uint64_t> new_index;
    {
        std::ofstream tmp(tmp_path, std::ios::binary | std::ios::trunc);
        if (!tmp.is_open()) throw std::runtime_error("KVStore: could not create compaction temp file");

        for (const auto& [key, old_offset] : index_) {
            std::string value = readValueAt(old_offset); // pull the live value off the OLD log

            uint64_t new_offset = static_cast<uint64_t>(tmp.tellp());
            uint32_t key_len = static_cast<uint32_t>(key.size());
            uint32_t value_len = static_cast<uint32_t>(value.size());
            uint8_t tomb = 0;

            tmp.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
            tmp.write(key.data(), key_len);
            tmp.write(reinterpret_cast<const char*>(&value_len), sizeof(value_len));
            tmp.write(value.data(), value_len);
            tmp.write(reinterpret_cast<const char*>(&tomb), sizeof(tomb));

            new_index[key] = new_offset;
        }
    }

    log_.close();
    std::remove(path_.c_str());
    std::rename(tmp_path.c_str(), path_.c_str());

    log_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
    if (!log_.is_open()) throw std::runtime_error("KVStore: could not reopen log after compaction");

    index_ = std::move(new_index); // only swap in the new offsets once the new log is live
}
