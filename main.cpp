#include "kvstore.h"

#include <iostream>
#include <sstream>
#include <string>

// Splits input into the command word and "the rest of the line", since
// values are allowed to contain spaces (e.g. SET name John Smith).
static void splitFirstWord(const std::string& line, std::string& first, std::string& rest) {
    size_t pos = line.find_first_of(" \t");
    if (pos == std::string::npos) {
        first = line;
        rest.clear();
    } else {
        first = line.substr(0, pos);
        size_t restStart = line.find_first_not_of(" \t", pos);
        rest = (restStart == std::string::npos) ? "" : line.substr(restStart);
    }
}

static std::string toUpper(std::string s) {
    for (auto& c : s) c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));
    return s;
}

static void printHelp() {
    std::cout <<
        "Commands:\n"
        "  SET <key> <value...>   store a value (value may contain spaces)\n"
        "  GET <key>              fetch a value\n"
        "  DEL <key>              delete a key\n"
        "  SCAN [prefix]          list keys, optionally filtered by prefix\n"
        "  COMPACT                rewrite the log to drop stale/deleted entries\n"
        "  COUNT                  show number of live keys\n"
        "  HELP                   show this message\n"
        "  EXIT / QUIT            leave the REPL\n";
}

int main(int argc, char** argv) {
    std::string dbPath = (argc > 1) ? argv[1] : "tinydb.log";

    KVStore db(dbPath);
    std::cout << "TinyDB - a toy append-only-log key-value store\n";
    std::cout << "Database file: " << dbPath << " (" << db.size() << " keys loaded)\n";
    std::cout << "Type HELP for commands.\n\n";

    std::string line;
    while (true) {
        std::cout << "tinydb> ";
        if (!std::getline(std::cin, line)) break; // EOF (Ctrl-D)
        if (line.empty()) continue;

        std::string cmd, rest;
        splitFirstWord(line, cmd, rest);
        cmd = toUpper(cmd);

        if (cmd == "EXIT" || cmd == "QUIT") {
            break;

        } else if (cmd == "HELP") {
            printHelp();

        } else if (cmd == "SET") {
            std::string key, value;
            splitFirstWord(rest, key, value);
            if (key.empty() || value.empty()) {
                std::cout << "usage: SET <key> <value...>\n";
                continue;
            }
            db.put(key, value);
            std::cout << "OK\n";

        } else if (cmd == "GET") {
            std::string key = rest;
            if (key.empty()) {
                std::cout << "usage: GET <key>\n";
                continue;
            }
            auto value = db.get(key);
            if (value) {
                std::cout << *value << "\n";
            } else {
                std::cout << "(nil)\n";
            }

        } else if (cmd == "DEL") {
            std::string key = rest;
            if (key.empty()) {
                std::cout << "usage: DEL <key>\n";
                continue;
            }
            db.del(key);
            std::cout << "OK\n";

        } else if (cmd == "SCAN") {
            auto keys = db.scan(rest);
            if (keys.empty()) {
                std::cout << "(no matching keys)\n";
            } else {
                for (const auto& k : keys) std::cout << "  " << k << "\n";
            }

        } else if (cmd == "COMPACT") {
            db.compact();
            std::cout << "OK, log compacted\n";

        } else if (cmd == "COUNT") {
            std::cout << db.size() << "\n";

        } else {
            std::cout << "unknown command: " << cmd << " (type HELP)\n";
        }
    }

    std::cout << "bye.\n";
    return 0;
}