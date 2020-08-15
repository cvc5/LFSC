#ifndef SC2_TOKEN_H
#define SC2_TOKEN_H

#include <iosfwd>

namespace Token {

enum Token : int
{
  Eof = 0,
  Declare,
  Define,
  Check,
  Program,
  Opaque,
  Run,

  // Terms
  Type,
  // Function-names
  Percent,
  Bang,
  At,
  Colon,
  ReverseSolidus,
  Pound,
  Caret,
  Hole,
  // Program constructs
  Let,
  Tilde,
  Do,
  Match,
  Default,
  Mpz,
  Mpq,
  MpAdd,
  MpNeg,
  MpDiv,
  MpMul,
  MpIfNeg,
  MpIfZero,
  MpzToMpq,
  Compare,
  IfEqual,
  Fail,
  MarkVar,
  IfMarked,

  Open,
  Close,

  Natural,

  Rational,

  Ident,

  TokenErr,
};

std::ostream& operator<<(std::ostream& o, Token t);

}  // namespace Token

#endif  // SC2_TOKEN_H
