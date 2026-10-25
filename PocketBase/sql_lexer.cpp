#include "sql_lexer.h"

#include <stdexcept>
#include <unordered_set>
#include <cctype>

namespace {

// Keywords are matched case-insensitively but stored upper-cased, so the
// parser can compare against "SELECT" without caring how the user typed it.
const std::unordered_set<std::string> kKeywords = {
    "CREATE", "TABLE", "INSERT", "INTO", "VALUES", "SELECT", "FROM",
    "WHERE", "DELETE", "AND", "OR", "NOT", "NULL", "INT", "INTEGER", "TEXT",
};

// The <cctype> functions are undefined for negative values, and plain char
// is signed on most platforms (bytes >= 0x80 in UTF-8 input would be
// negative), so always go through unsigned char.
bool isDigit(char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }
bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_'; }
bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_'; }

std::string toUpper(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

// Walks the input one character at a time while tracking line/column, so
// every token and error can point at where it came from.
class Lexer {
public:
    explicit Lexer(const std::string& sql) : src_(sql) {}

    std::vector<Token> run() {
        std::vector<Token> tokens;
        while (true) {
            skipWhitespaceAndComments();
            if (atEnd()) break;
            tokens.push_back(next());
        }
        tokens.push_back({TokenType::End, "", line_, col_});
        return tokens;
    }

private:
    bool atEnd() const { return pos_ >= src_.size(); }
    char peek(size_t ahead = 0) const {
        return pos_ + ahead < src_.size() ? src_[pos_ + ahead] : '\0';
    }

    char advance() {
        char c = src_[pos_++];
        if (c == '\n') {
            ++line_;
            col_ = 1;
        }
        else {
            ++col_;
        }
        return c;
    }

    [[noreturn]] void fail(const std::string& msg, size_t line, size_t col) const {
        throw std::runtime_error("SQL syntax error at " + std::to_string(line) + ":" +
                                 std::to_string(col) + ": " + msg);
    }

    void skipWhitespaceAndComments() {
        while (!atEnd()) {
            char c = peek();
            if (std::isspace(static_cast<unsigned char>(c))) {
                advance();
            }
            else if (c == '-' && peek(1) == '-') {
                while (!atEnd() && peek() != '\n') advance();
            }
            else {
                break;
            }
        }
    }

    Token next() {
        size_t line = line_, col = col_;
        char c = peek();

        if (isIdentStart(c)) {
            std::string word;
            while (!atEnd() && isIdentChar(peek())) word += advance();
            std::string upper = toUpper(word);
            if (kKeywords.count(upper)) return {TokenType::Keyword, upper, line, col};
            return {TokenType::Identifier, word, line, col};
        }

        // A '-' directly followed by a digit is lexed as part of the number.
        // There's no arithmetic in the grammar yet, so '-' can't mean
        // subtraction; if that changes, move negation into the parser.
        if (isDigit(c) || (c == '-' && isDigit(peek(1)))) {
            std::string num;
            num += advance();
            while (!atEnd() && isDigit(peek())) num += advance();
            // Catch "123abc" here; otherwise it would quietly split into
            // Integer(123) + Identifier(abc) and confuse the parser later.
            if (isIdentStart(peek())) fail("invalid number '" + num + peek() + "...'", line, col);
            return {TokenType::Integer, num, line, col};
        }

        if (c == '\'') return readString(line, col);

        advance();
        switch (c) {
            case '(': return {TokenType::LParen, "(", line, col};
            case ')': return {TokenType::RParen, ")", line, col};
            case ',': return {TokenType::Comma, ",", line, col};
            case ';': return {TokenType::Semicolon, ";", line, col};
            case '*': return {TokenType::Star, "*", line, col};
            case '=': return {TokenType::Eq, "=", line, col};
            case '!':
                if (peek() == '=') {
                    advance();
                    return {TokenType::NotEq, "!=", line, col};
                }
                fail("expected '=' after '!'", line, col);
            case '<':
                if (peek() == '=') {
                    advance();
                    return {TokenType::LtEq, "<=", line, col};
                }
                if (peek() == '>') {
                    advance();
                    return {TokenType::NotEq, "<>", line, col};
                }
                return {TokenType::Lt, "<", line, col};
            case '>':
                if (peek() == '=') {
                    advance();
                    return {TokenType::GtEq, ">=", line, col};
                }
                return {TokenType::Gt, ">", line, col};
            default:
                fail(std::string("unexpected character '") + c + "'", line, col);
        }
    }

    // SQL escapes a quote inside a string by doubling it: 'it''s' -> it's.
    Token readString(size_t line, size_t col) {
        advance(); // opening quote
        std::string value;
        while (true) {
            if (atEnd()) fail("unterminated string literal", line, col);
            char c = advance();
            if (c == '\'') {
                if (peek() == '\'') {
                    advance();
                    value += '\'';
                }
                else {
                    break;
                }
            }
            else {
                value += c;
            }
        }
        return {TokenType::String, value, line, col};
    }

    const std::string& src_;
    size_t pos_ = 0;
    size_t line_ = 1;
    size_t col_ = 1;
};

} // namespace

std::vector<Token> tokenize(const std::string& sql) {
    return Lexer(sql).run();
}

std::string tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::Keyword:    return "Keyword";
        case TokenType::Identifier: return "Identifier";
        case TokenType::Integer:    return "Integer";
        case TokenType::String:     return "String";
        case TokenType::LParen:     return "'('";
        case TokenType::RParen:     return "')'";
        case TokenType::Comma:      return "','";
        case TokenType::Semicolon:  return "';'";
        case TokenType::Star:       return "'*'";
        case TokenType::Eq:         return "'='";
        case TokenType::NotEq:      return "'!='";
        case TokenType::Lt:         return "'<'";
        case TokenType::LtEq:       return "'<='";
        case TokenType::Gt:         return "'>'";
        case TokenType::GtEq:       return "'>='";
        case TokenType::End:        return "end of input";
    }
    return "unknown";
}
