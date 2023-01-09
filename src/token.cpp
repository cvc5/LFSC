#include "token.h"

#include <iostream>

namespace Token {
std::ostream& operator<<(std::ostream& o, Token t)
{
  switch (t)
  {
    case Eof: return o << "Eof";
    case Declare: return o << "Declare";
    case Define: return o << "Define";
    case Check: return o << "Check";
    case Opaque : return o << "Opaque";
    case Run : return o << "Run";
    case Program: return o << "Program";
    case Method: return o << "Method";
    case Type: return o << "Type";
    case Percent: return o << "Percent";
    case Bang: return o << "Bang";
    case At: return o << "At";
    case Colon: return o << "Colon";
    case ReverseSolidus: return o << "ReverseSolidus";
    case Caret: return o << "Caret";
    case Hole: return o << "Hole";
    case Let: return o << "Let";
    case Tilde: return o << "Tilde";
    case Pound: return o << "Pound";
    case Do: return o << "Do";
    case Match: return o << "Match";
    case Default: return o << "Default";
    case Mpz: return o << "Mpz";
    case Mpq: return o << "Mpq";
    case MpAdd: return o << "MpAdd";
    case MpNeg: return o << "MpNeg";
    case MpDiv: return o << "MpDiv";
    case MpMul: return o << "MpMul";
    case MpIfNeg: return o << "MpIfNeg";
    case MpIfZero: return o << "MpIfZero";
    case MpzToMpq: return o << "MpzToMpq";
    case Compare: return o << "Compare";
    case IfEqual: return o << "IfEqual";
    case Fail: return o << "Fail";
    case MarkVar: return o << "MarkVar";
    case IfMarked: return o << "IfMarked";
    case Open: return o << "Open";
    case Close: return o << "Close";
    case Natural: return o << "Natural";
    case Rational: return o << "Rational";
    case Ident: return o << "Ident";
    case TokenErr: return o << "TokenErr";
    case Provided: return o << "Provided";
    case DeclareRule: return o << "DeclareRule";
    case DeclareType: return o << "DeclareType";
    case DefineConst: return o << "DefineConst";
    case Forall: return o << "Forall";
    case Arrow: return o << "Arrow";
    case Lam: return o << "Lam";
    case CheckAssuming: return o << "CheckAssuming";
    default: return o << "Unknown Token: " << unsigned(t);
  }
}
}  // namespace Token
