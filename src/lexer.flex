   // An example of using the flex C++ scanner class.

%option noyywrap
%option nounput
%option yylineno
%option full
%option c++

%{
#include "lexer.h"
#include "position.h"
#include <sstream>
#include <cassert>
#include <iostream>
%}

ws          [ \n\t\f]+
dig         [0-9]
markvar     markvar{dig}*
ifmarked    ifmarked{dig}*
natural     {dig}+
rational    {dig}+\/{dig}+
ident       [^0-9^\\~:@!%() \t\n\f;][^() \t\n\f;]*
comment     ;[^\n]*\n

%%

"declare"       return Token::Declare;
"define"        return Token::Define;
"check"         return Token::Check;
"program"       return Token::Program;
"opaque"        return Token::Opaque;
"run"           return Token::Run;

"type"          return Token::Type;

"\%"            return Token::Percent;
"!"             return Token::Bang;
"#"             return Token::Pound;
"@"             return Token::At;
":"             return Token::Colon;
"\\"            return Token::ReverseSolidus;
"^"             return Token::Caret;
"_"             return Token::Hole;

"let"           return Token::Let;
"~"             return Token::Tilde;
"do"            return Token::Do;
"match"         return Token::Match;
"default"       return Token::Default;
"mpz"           return Token::Mpz;
"mpq"           return Token::Mpq;
"mp_add"        return Token::MpAdd;
"mp_neg"        return Token::MpNeg;
"mp_div"        return Token::MpDiv;
"mp_mul"        return Token::MpMul;
"mp_ifneg"      return Token::MpIfNeg;
"mp_ifzero"     return Token::MpIfZero;
"mpz_to_mpq"    return Token::MpzToMpq;
"compare"       return Token::Compare;
"ifequal"       return Token::IfEqual;
"fail"          return Token::Fail;

"("             return Token::Open;
")"             return Token::Close;

{markvar}       return Token::MarkVar;
{ifmarked}      return Token::IfMarked;
{natural}       return Token::Natural;
{rational}      return Token::Rational;
{ws}            /* skip blanks and tabs */
{comment}       /* skip comments */


";"    {
        int c;

        while((c = yyinput()) != 0)
            {
            if(c == '\n')
                break;
            }
        }

{ident}         return Token::Ident;

%%

// Name of current file
std::string s_filename{};
// Currrent lexer
FlexLexer* s_lexer = nullptr;
Token::Token s_peeked = Token::TokenErr;

void reinsert_token(Token::Token t)
{
  assert(s_peeked == Token::TokenErr);
  s_peeked = t;
}

Token::Token next_token()
{
  Token::Token t;
  if (s_peeked == Token::TokenErr)
  {
    t = Token::Token(s_lexer->yylex());
  }
  else
  {
    t = s_peeked;
    s_peeked = Token::TokenErr;
  }
  return t;
}

void report_error(const std::string &msg)
{
  if (s_filename.length())
  {
    Position p(s_filename.c_str(), s_lexer ? s_lexer->lineno() : 0, 0);
    p.print(std::cerr);
  }
  std::cerr << "\n";
  std::cerr << msg;
  std::cerr << "\n";
  std::cerr.flush();
  exit(1);
}

void unexpected_token_error(Token::Token t, const std::string& info)
{
  std::ostringstream o{};
  o << "Scanned token " << t << ", `" << s_lexer->YYText() << "`, which is invalid in this position";
  if (info.length()) {
    o << std::endl << "Info: " << info;
  }
  report_error(o.str());
}

std::string prefix_id() {
  next_token();
  //eat_token(Token::Ident);
  return s_lexer->YYText();
}

void eat_token(Token::Token t)
{
  auto tt = next_token();
  if (t != tt) {
    std::ostringstream o{};
    o << "Expected a " << t << ", but got a " << tt << ", `" << s_lexer->YYText() << "`";
    unexpected_token_error(tt, o.str());
  }
}
