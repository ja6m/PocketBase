#include "kvstore.h"
#include "sql_lexer.h"
#include "utilities.h"

#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>

static void printHelp() {
    std::cout <<
        "Commands:\n"
        "  SET <key> <value...>   store a value (value may contain spaces)\n"
        "  GET <key>              fetch a value\n"
        "  DEL <key>              delete a key\n"
        "  SCAN [prefix]          list keys, optionally filtered by prefix\n"
        "  RANGE <start> [end]    list keys in [start, end] (end omitted = to the last key)\n"
        "  COMPACT                rewrite the log to drop stale/deleted entries\n"
        "  COUNT                  show number of live keys\n"
        "  TOKENS <sql...>        show how the SQL tokenizer splits a statement\n"
        "  HELP                   show this message\n"
        "  EXIT / QUIT            leave the REPL\n";
}

int main(int argc, char** argv) {
    std::string dbPath = (argc > 1) ? argv[1] : "pocketdb.log";

    KVStore db(dbPath);
    std::cout << "PocketBase - a toy append-only-log key-value store\n";
    std::cout << "Database file: " << dbPath << " (" << db.size() << " keys loaded)\n";
    std::cout << "Type HELP for commands.\n\n";

    std::string line;
    while (true) {
        std::cout << "pocketbase> ";
        if (!std::getline(std::cin, line)) break; // EOF (Ctrl-D)
        if (line.empty()) continue;

        std::string cmd, rest;
        splitFirstWord(line, cmd, rest);
        cmd = toUpper(cmd);

        if (cmd == "EXIT" || cmd == "QUIT") {
            break;

        }
        else if (cmd == "HELP") {
            printHelp();

        }
        else if (cmd == "SET") {
            std::string key, value;
            splitFirstWord(rest, key, value);
            if (key.empty() || value.empty()) {
                std::cout << "usage: SET <key> <value...>\n";
                continue;
            }
            db.put(key, value);
            std::cout << "OK\n";

        }
        else if (cmd == "GET") {
            std::string key = rest;
            if (key.empty()) {
                std::cout << "usage: GET <key>\n";
                continue;
            }
            auto value = db.get(key);
            if (value) {
                std::cout << *value << "\n";
            }
            else {
                std::cout << "(nil)\n";
            }

        }
        else if (cmd == "DEL") {
            std::string key = rest;
            if (key.empty()) {
                std::cout << "usage: DEL <key>\n";
                continue;
            }
            db.del(key);
            std::cout << "OK\n";

        }
        else if (cmd == "SCAN") {
            auto keys = db.scan(rest);
            if (keys.empty()) {
                std::cout << "(no matching keys)\n";
            }
            else {
                for (const auto& k : keys) std::cout << "  " << k << "\n";
            }

        }
        else if (cmd == "RANGE") {
            std::string start, end;
            splitFirstWord(rest, start, end);
            if (start.empty()) {
                std::cout << "usage: RANGE <start> [end]\n";
                continue;
            }
            auto keys = db.range(start, end);
            if (keys.empty()) {
                std::cout << "(no matching keys)\n";
            }
            else {
                for (const auto& k : keys) std::cout << "  " << k << "\n";
            }

        }
        else if (cmd == "COMPACT") {
            db.compact();
            std::cout << "OK, log compacted\n";

        }
        else if (cmd == "COUNT") {
            std::cout << db.size() << "\n";

        }
        else if (cmd == "TOKENS") {
            // Debug aid for Stage 3: lets us check the tokenizer by hand
            // before the parser exists to consume its output.
            try {
                for (const auto& t : tokenize(rest)) {
                    std::cout << "  " << t.line << ":" << t.column << "  "
                              << tokenTypeName(t.type);
                    if (!t.text.empty()) std::cout << "  [" << t.text << "]";
                    std::cout << "\n";
                }
            }
            catch (const std::runtime_error& e) {
                std::cout << e.what() << "\n";
            }

        }
        else {
            std::cout << "unknown command: " << cmd << " (type HELP)\n";
        }
    }

    std::cout << "bye.\n";
    return 0;
}
