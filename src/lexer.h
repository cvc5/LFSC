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
#include <cstdint>

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

// Lexer explanation.
//
// This a lookahead-two lexer, backed by a length-two buffer.
//
// View it as a stack of tokens. The topmost is first.
//
// It is implemented with functions for pulling tokens out of the lexer,
// getting information about the token just pulled, and pushing a token back
// into the conceptual stream/concrete buffer.

// Private components
// Currrent lexer
extern FlexLexer* s_lexer;
// Name of current file
extern std::string s_filename;
// The buffer. 0 is first, then 1.
extern Token::Token s_peeked[2];
// Used to initialize s_span.
void init_s_span();
// Sets the spans start to its current end.
void bump_span();
// Add columns or lines to the end location of the span.
void add_columns(uint32_t columns);
void add_lines(uint32_t lines);

// Public interface

// Core functions
// Advance to the next token (pop from stack)
Token::Token next_token();
// Add a token back into the stream (push to stack)
void reinsert_token(Token::Token t);
// String corresponding to the last token (old top of stack)
const char* token_str();
// Span of last token pulled from underlying lexer (old top of stack)
extern Span s_span;
// Used to report errors, with the current source location attached.
void report_error(const std::string&);

// Derived functions
// Expect a token `t` as the next one. Error o.w.
void eat_token(Token::Token t);
// Interpret the next token as an identifier (even if it isn't) and return its string
std::string prefix_id();
// Error. Got `t`, expected `info`.
void unexpected_token_error(Token::Token t, const std::string& info);

#endif  // SC2_LEXER_H
