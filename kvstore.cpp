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

        std::string value;
        if (value_len > 0) {
            value.resize(value_len);
            log_.read(value.data(), value_len);
            if (!log_) throw std::runtime_error("KVStore: corrupt log (value bytes)");
        }

        log_.read(reinterpret_cast<char*>(&tombstone), sizeof(tombstone));
        if (!log_) throw std::runtime_error("KVStore: corrupt log (tombstone)");

        if (tombstone) {
            index_.erase(key);
        } else {
            index_[key] = value;
        }
    }

    log_.clear(); // reset eof/fail bits before further use
}

void KVStore::appendRecord(const std::string& key, const std::string& value, bool tombstone) {
    log_.clear();
    log_.seekp(0, std::ios::end);

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
}

void KVStore::put(const std::string& key, const std::string& value) {
    appendRecord(key, value, /*tombstone=*/false);
    index_[key] = value;
}

std::optional<std::string> KVStore::get(const std::string& key) const {
    auto it = index_.find(key);
    if (it == index_.end()) return std::nullopt;
    return it->second;
}

void KVStore::del(const std::string& key) {
    if (index_.find(key) == index_.end()) return; // nothing to delete
    appendRecord(key, "", /*tombstone=*/true);
    index_.erase(key);
}

std::vector<std::string> KVStore::scan(const std::string& prefix) const {
    std::vector<std::string> result;
    for (const auto& [k, v] : index_) {
        if (prefix.empty() || k.compare(0, prefix.size(), prefix) == 0) {
            result.push_back(k);
        }
    }
    return result;
}

void KVStore::compact() {
    // Write the live index out to a temp file, then atomically replace
    // the old log with it. This throws away every stale/overwritten
    // value and every tombstone, shrinking the file to just the live set.
    std::string tmp_path = path_ + ".compact.tmp";
    {
        std::ofstream tmp(tmp_path, std::ios::binary | std::ios::trunc);
        if (!tmp.is_open()) throw std::runtime_error("KVStore: could not create compaction temp file");

        for (const auto& [key, value] : index_) {
            uint32_t key_len = static_cast<uint32_t>(key.size());
            uint32_t value_len = static_cast<uint32_t>(value.size());
            uint8_t tomb = 0;

            tmp.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
            tmp.write(key.data(), key_len);
            tmp.write(reinterpret_cast<const char*>(&value_len), sizeof(value_len));
            tmp.write(value.data(), value_len);
            tmp.write(reinterpret_cast<const char*>(&tomb), sizeof(tomb));
        }
    }

    log_.close();
    std::remove(path_.c_str());
    std::rename(tmp_path.c_str(), path_.c_str());

    log_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
    if (!log_.is_open()) throw std::runtime_error("KVStore: could not reopen log after compaction");
}