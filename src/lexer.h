#ifndef SC2_LEXER_H
#define SC2_LEXER_H

// Super hack
// https://stackoverflow.com/a/40665154/4917890
#if !defined(yyFlexLexerOnce)
#include <FlexLexer.h>
#endif

#include <iosfwd>
#include <string>

#include "token.h"

struct Location {
  uint32_t line;
  uint32_t column;
};

struct Span {
  Location start;
  Location end;
};

std::ostream& operator<<(std::ostream& o, const Location& l);
std::ostream& operator<<(std::ostream& o, const Span& l);

// Currrent lexer
extern FlexLexer* s_lexer;
// Name of current file
extern std::string s_filename;
// buffer. 0 it first, then 1.
extern Token::Token s_peeked[2];
extern Span s_span;

// Public interface
Token::Token next_token();
void reinsert_token(Token::Token t);
const char* token_str();

void eat_token(Token::Token t);
std::string prefix_id();
void report_error(const std::string&);
void unexpected_token_error(Token::Token t, const std::string& info);
void init_s_span();
void bump_span();
void add_columns(uint32_t columns);
void add_lines(uint32_t lines);

#endif  // SC2_LEXER_H
