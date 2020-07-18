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

// Currrent lexer
extern FlexLexer* s_lexer;
// Name of current file
extern std::string s_filename;
extern Token::Token s_peeked;

Token::Token next_token();
void eat_token(Token::Token t);
std::string prefix_id();
void reinsert_token(Token::Token t);
void report_error(const std::string&);
void unexpected_token_error(Token::Token t, const std::string& info);

#endif  // SC2_LEXER_H
