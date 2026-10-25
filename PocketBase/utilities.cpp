#include "utilities.h"

#include <cctype>

void splitFirstWord(const std::string& line, std::string& first, std::string& rest) {
    size_t pos = line.find_first_of(" \t");
    if (pos == std::string::npos) {
        first = line;
        rest.clear();
    }
    else {
        first = line.substr(0, pos);
        size_t restStart = line.find_first_not_of(" \t", pos);
        rest = (restStart == std::string::npos) ? "" : line.substr(restStart);
    }
}

std::string toUpper(std::string value) {
    for (auto& c : value) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return value;
}