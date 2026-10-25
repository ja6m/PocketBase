#pragma once

#include <string>
#include <vector>
#include <cstddef>

// Stage 3, step one: a hand-written SQL tokenizer.
//
// Turns a SQL string into a flat list of tokens for the recursive-descent
// parser to consume. The lexer knows nothing about grammar -- it doesn't
// care whether "SELECT FROM WHERE" makes sense, only that each word,
// number, string and symbol is well-formed.
//
// Supported input:
//   - Keywords (case-insensitive): CREATE TABLE INSERT INTO VALUES SELECT
//     FROM WHERE DELETE AND OR NOT NULL INT INTEGER TEXT
//   - Identifiers: [A-Za-z_][A-Za-z0-9_]*, case preserved
//   - Integer literals: 42, -7
//   - String literals: 'single quoted', with '' as an escaped quote
//   - Symbols: ( ) , ; * = != <> < <= > >=
//   - Line comments: -- to end of line
enum class TokenType {
    Keyword,     // text is upper-cased so the parser can compare directly
    Identifier,  // table or column name, original case kept
    Integer,     // text holds the digits (and a leading '-' if negative)
    String,      // text holds the unquoted, unescaped contents
    LParen, RParen, Comma, Semicolon, Star,
    Eq, NotEq, Lt, LtEq, Gt, GtEq,
    End          // always the last token, so the parser never reads past the vector
};

struct Token {
    TokenType type;
    std::string text;
    // 1-based position of the token's first character, for error messages.
    size_t line;
    size_t column;
};

// Tokenizes `sql`. Throws std::runtime_error with a line:column position
// on malformed input (unterminated string, unknown character, ...).
std::vector<Token> tokenize(const std::string& sql);

// Human-readable name for a token type, e.g. "Keyword" or "'<='".
// Used by the REPL's TOKENS command and, later, by parser error messages.
std::string tokenTypeName(TokenType type);
