#include "code.h"
#include <cstddef>
#include <string>
#include <sstream>
#include "check.h"
#include "token.h"
#include "lexer.h"

#include "scccode.h"

using namespace std;

// Returns null on "default"
SymSExpr *read_ctor()
{
  string id(prefix_id());

  if (id == "default")
  {
    return nullptr;
  }

  pair<Expr *, Expr *> p = symbols->get(id.c_str());
  Expr *s = p.first;
  Expr *stp = p.second;

  if (!stp) report_error("Undeclared identifier parsing a pattern.");

  if (s->getclass() != SYMS_EXPR || ((SymExpr *)s)->val)
    report_error("The head of a pattern is not a constructor.");

  s->inc();

  return (SymSExpr *)s;
}

Expr *read_case()
{
  eat_token(Token::Open);
  Expr *pat = NULL;
  vector<SymSExpr *> vars;

  vector<pair<Expr *, Expr *> > prevs;
  Token::Token d = next_token();
  switch (d)
  {
    case Token::Open:
    {
      // parse application
      SymSExpr *s = read_ctor();
      pat = s;
      Token::Token c;
      while ((c = next_token()) != Token::Close)
      {
        reinsert_token(c);
        string varstr(prefix_id());
        // To avoid non-termination in the case that the user has provided an
        // illegal identifier here (such as a nested match pattern), we check
        // whether the string read for the identifier is empty.
        if (varstr.size() == 0)
        {
          report_error(
              "Could not read identifier in a pattern of match expression. "
              "Note that nested match patterns are not supported.");
        }
        SymSExpr *var = new SymSExpr(varstr);
        vars.push_back(var);
        prevs.push_back(
            symbols->insert(varstr.c_str(), pair<Expr *, Expr *>(var, NULL)));
        Expr *orig_pat = pat;
        pat = Expr::make_app(pat, var);
        if (orig_pat->getclass() == CEXPR)
        {
          orig_pat->dec();
        }
      }
      break;
    }
    case Token::Eof:
    {
      report_error("Unexpected end of file parsing a pattern.");
      break;
    }
    default:
      // could be an identifier or "default"
      reinsert_token(d);
      pat = read_ctor();
      break;
  }

  Expr *ret = read_code();
  if (pat) ret = new CExpr(CASE, pat, ret);

  for (int i = 0, iend = prevs.size(); i < iend; i++)
  {
    string &s = vars[i]->s;
    symbols->insert(s.c_str(), prevs[i]);
  }

  eat_token(Token::Close);

  return ret;
}

Expr *read_code()
{
  Token::Token d = next_token();
  switch (d)
  {
    case Token::Open:
    {
      Token::Token c = next_token();
      switch (c)
      {
        case Token::Do:
        {
          Expr* ret = read_code();
          while ((c = next_token()) != Token::Close)
          {
            reinsert_token(c);
            ret = new CExpr(DO, ret, read_code());
          }
          return ret;
        }
        case Token::Fail:
        {
          Expr* c = read_code();
          eat_token(Token::Close);

          // do we need to check this???
          // if (c->getclass() != SYMS_EXPR || ((SymExpr *)c)->val)
          // report_error(string("\"fail\" must be used with a (undefined) base
          // ")
          //  +string("type.\n1. the expression used: "+c->toString()));

          return new CExpr(FAIL, c);
        }
        case Token::At:
        {
          string id(prefix_id());
          SymSExpr* var = new SymSExpr(id);

          Expr* t1 = read_code();

          pair<Expr*, Expr*> prev =
              symbols->insert(id.c_str(), pair<Expr*, Expr*>(var, NULL));

          Expr* t2 = read_code();

          symbols->insert(id.c_str(), prev);

          eat_token(Token::Close);
          return new CExpr(LET, var, t1, t2);
        }
        case Token::IfMarked:
        {
          int index = s_lexer->YYLeng() > 8 ? atoi(s_lexer->YYText() + 8) : 1;
          Expr* e1 = read_code();
          Expr* e2 = read_code();
          Expr* e3 = read_code();
          Expr* ret = NULL;
          if (index >= 1 && index <= 32)
          {
            ret = new CExpr(IFMARKED, new IntExpr(index - 1), e1, e2, e3);
          }
          else
          {
            std::cout << "Can't make IFMARKED with index = " << index
                      << std::endl;
          }
          Expr::markedCount++;
          eat_token(Token::Close);
          return ret;
        }
        case Token::IfEqual:
        {
          Expr* e1 = read_code();
          Expr* e2 = read_code();
          Expr* e3 = read_code();
          Expr* e4 = read_code();
          Expr* ret = new CExpr(IFEQUAL, e1, e2, e3, e4);
          eat_token(Token::Close);
          return ret;
        }
        case Token::Match:
        {
          Token::Token c;
          vector<Expr*> cases;
          cases.push_back(read_code());  // the scrutinee
          while ((c = next_token()) != Token::Close && c != 'd')
          {
            reinsert_token(c);
            cases.push_back(read_case());
          }
          if (cases.size() == 1)  // counting scrutinee
            report_error("A match has no cases.");
          return new CExpr(MATCH, cases);
        }
        case Token::MarkVar:
        {
          int index = s_lexer->YYLeng() > 7 ? atoi(s_lexer->YYText() + 7) : 1;
          CExpr* ret = NULL;
          if (index >= 1 && index <= 32)
          {
            ret = new CExpr(MARKVAR, new IntExpr(index - 1), read_code());
          }
          else
          {
            std::cout << "Can't make MARKVAR with index = " << index
                      << std::endl;
          }
          Expr::markedCount++;
          eat_token(Token::Close);
          return ret;
        }
        case Token::MpAdd:
        case Token::MpMul:
        case Token::MpDiv:
        {
          auto op = c == Token::MpAdd ? ADD : c == Token::MpMul ? MUL : DIV;
          Expr* e1 = read_code();
          Expr* e2 = read_code();
          Expr* ret = new CExpr(op, e1, e2);
          eat_token(Token::Close);
          return ret;
        }
        case Token::MpNeg:
        case Token::MpzToMpq:
        {
          Expr* ret =
              new CExpr(c == Token::MpNeg ? NEG : MPZ_TO_MPQ, read_code());
          eat_token(Token::Close);
          return ret;
        }
        case Token::MpIfNeg:
        case Token::MpIfZero:
        {
          Expr* e1 = read_code();
          Expr* e2 = read_code();
          Expr* e3 = read_code();
          Expr* ret =
              new CExpr(c == Token::MpIfNeg ? IFNEG : IFZERO, e1, e2, e3);
          eat_token(Token::Close);
          return ret;
        }
        case Token::Tilde:
        {
          Expr* e = read_code();
          if (e->getclass() == INT_EXPR)
          {
            IntExpr* ee = (IntExpr*)e;
            mpz_neg(ee->n, ee->n);
            eat_token(Token::Close);
            return ee;
          }
          else if (e->getclass() == RAT_EXPR)
          {
            RatExpr* ee = (RatExpr*)e;
            mpq_neg(ee->n, ee->n);
            eat_token(Token::Close);
            return ee;
          }
          else
          {
            report_error(
                "Negative sign with expr that is not an numeric literal.");
          }
        }
        case Token::Compare:
        {
          Expr* e1 = read_code();
          Expr* e2 = read_code();
          Expr* e3 = read_code();
          Expr* e4 = read_code();
          eat_token(Token::Close);
          return new CExpr(COMPARE, e1, e2, e3, e4);
        }
        case Token::Eof:
        {
          report_error("Unexpected end of file.");
          break;
        }
        default:
        {  // the application case
          std::string pref = s_lexer->YYText();
          Expr* ret = progs[pref];
          if (!ret) ret = symbols->get(pref.c_str()).first;

          if (!ret)
            report_error(
                string("Undeclared identifier at head of an application: ")
                + pref);

          ret->inc();

          while ((c = next_token()) != Token::Close)
          {
            reinsert_token(c);
            Expr* ke = read_code();
            Expr* orig_ret = ret;
            ret = Expr::make_app(ret, ke);
            if (orig_ret->getclass() == CEXPR)
            {
              orig_ret->dec();
            }
          }
          return ret;
        }
      }
    }  // end case '('
    case Token::Natural:
    {
      mpz_t num;
      if (mpz_init_set_str(num, s_lexer->YYText(), 10) == -1)
        report_error("Error reading a numeral.");
      return new IntExpr(num);
    }
    case Token::Rational:
    {
      mpq_t num;
      mpq_init(num);
      if (mpq_set_str(num, s_lexer->YYText(), 10) == -1)
        report_error("Error reading a mpq numeral.");

      return new RatExpr(num);
    }
    case Token::Eof:
    {
      report_error("Unexpected end of file.");
      break;
    }
    default:
    {
      string id(s_lexer->YYText());
      pair<Expr*, Expr*> p = symbols->get(id.c_str());
      Expr* ret = p.first;
      if (!ret) ret = progs[id];
      if (!ret) report_error(string("Undeclared identifier: ") + id);
      ret->inc();
      return ret;
    }
  }
  report_error("Unexpected operator in a piece of code.");
  return 0;
}

// the input is owned by the caller, the output by us (so do not dec it).
Expr *check_code(Expr *_e)
{
  CExpr *e = (CExpr *)_e;
  switch (e->getop())
  {
    case NOT_CEXPR:
      switch (e->getclass())
      {
        case INT_EXPR: return statMpz;
        case RAT_EXPR: return statMpq;
        case SYM_EXPR:
        {
          report_error("Internal error: an LF variable is encountered in code");
          break;
        }
        case SYMS_EXPR: {
          Expr *tp = symbols->get(((SymSExpr *)e)->s.c_str()).second;
          if (!tp)
            report_error(
                string("A symbol is missing a type in a piece of code.")
                + string("\n1. the symbol: ") + ((SymSExpr *)e)->s);
          return tp;
        }
        case HOLE_EXPR:
          report_error("Encountered a hole unexpectedly in code.");
        default: report_error("Unrecognized form of expr in code.");
      }
    case APP: {
      Expr *h = e->kids[0]->followDefs();
      vector<Expr *> argtps;
      int counter = 1;
      while (e->kids[counter])
      {
        argtps.push_back(check_code(e->kids[counter]));
        counter++;
      }
      int iend = counter - 1;

      Expr *tp = NULL;
      if (h->getop() == PROG)
      {
        tp = ((CExpr *)h)->kids[0];
      }
      else if (h->getclass() == SYMS_EXPR)
      {
        tp = symbols->get(((SymSExpr *)h)->s.c_str()).second;
      }
      else if (e->kids[0]->getclass() == SYMS_EXPR)
      {
        // The head is not a symbol.
        // Perhaps it is a macro? If so, it is a symbol whose values
        // we've determined with the above "followDefs".
        // Let's try backing up to the underlying symbol.
        tp = symbols->get(((SymSExpr *)e->kids[0])->s.c_str()).second;
      }
      else {
        ostringstream s;
        s << "An application's head is neither a program nor symbol";
        s << "\n1. The application: " << *e;
        report_error(s.str());
      }

      if (!tp)
        report_error(string("The head of an application is missing a type in ")
                     + string("code.\n1. the application: ") + e->toString());

      tp = tp->followDefs();

      if (tp->getop() != PI)
        report_error(string("The head of an application does not have ")
                     + string("functional type in code.")
                     + string("\n1. the application: ") + e->toString());

      CExpr *cur = (CExpr *)tp;
      int i = 0;
      while (cur->getop() == PI)
      {
        if (i >= iend)
          report_error(
              string("A function is not being fully applied in code.\n")
              + string("1. the application: ") + e->toString()
              + string("\n2. its (functional) type: ") + cur->toString());
        bool types_match = argtps[i]->defeq(cur->kids[1]);
        bool app_types_match =
            argtps[i]->getop() == APP
            && static_cast<CExpr*>(argtps[i])->kids[0]->defeq(cur->kids[1]);
        if (!types_match && !app_types_match)
        {
          report_error(
              string("Type mismatch for argument ") + std::to_string(i)
              + string(" in application in code.\n")
              + string("1. the application: ") + e->toString()
              + string("\n2. the head's type: ") + tp->toString()
              + string("\n3. the argument: ") + e->kids[i + 1]->toString()
              + string("\n4. computed type: ") + argtps[i]->toString()
              + string("\n5. expected type: ") + cur->kids[1]->toString());
        }

        // if (cur->kids[2]->free_in((SymExpr *)cur->kids[0]))
        if (cur->get_free_in())
        {
          cur->calc_free_in();
          // are you sure?
          if (cur->get_free_in())
            report_error(
                string("A dependently typed function is being applied in")
                + string(" code.\n1. the application: ") + e->toString()
                + string("\n2. the head's type: ") + tp->toString());
          // ok, reset the mark
          cur->setexmark();
        }

        i++;
        cur = (CExpr *)cur->kids[2];
      }
      if (i < iend)
        report_error(string("A function is being fully applied to too many ")
                     + string("arguments in code.\n")
                     + string("1. the application: ") + e->toString()
                     + string("\n2. the head's type: ") + tp->toString());

      return cur;
    }
    // is this right?
    case MPZ: return statType; break;
    case MPQ: return statType; break;
    case DO: check_code(e->kids[0]); return check_code(e->kids[1]);

    case LET:
    {
      SymSExpr *var = (SymSExpr *)e->kids[0];

      Expr *tp1 = check_code(e->kids[1]);

      pair<Expr *, Expr *> prev =
          symbols->insert(var->s.c_str(), pair<Expr *, Expr *>(NULL, tp1));

      Expr *tp2 = check_code(e->kids[2]);

      symbols->insert(var->s.c_str(), prev);

      return tp2;
    }

    case ADD:
    case MUL:
    case DIV:
    {
      Expr *tp0 = check_code(e->kids[0]);
      Expr *tp1 = check_code(e->kids[1]);
      tp0 = tp0->followDefs();
      tp1 = tp1->followDefs();
      if (tp0 != statMpz && tp0 != statMpq)
        report_error(string("Argument to mp_[arith] does not have type \"mpz\" "
                            "or \"mpq\".\n")
                     + string("1. the argument: ") + e->kids[0]->toString()
                     + string("\n1. its type: ") + tp0->toString());

      if (tp0 != tp1)
        report_error(string("Arguments to mp_[arith] have differing types.\n")
                     + string("1. argument 1: ") + e->kids[0]->toString()
                     + string("\n1. its type: ") + tp0->toString()
                     + string("2. argument 2: ") + e->kids[1]->toString()
                     + string("\n2. its type: ") + tp1->toString());

      return tp0;
    }

    case NEG:
    {
      Expr *tp0 = check_code(e->kids[0]);
      tp0 = tp0->followDefs();
      if (tp0 != statMpz && tp0 != statMpq)
        report_error(
            string(
                "Argument to mp_neg does not have type \"mpz\" or \"mpq\".\n")
            + string("1. the argument: ") + e->kids[0]->toString()
            + string("\n1. its type: ") + tp0->toString());

      return tp0;
    }

    case MPZ_TO_MPQ:
    {
      Expr *tp0 = check_code(e->kids[0]);
      tp0 = tp0->followDefs();
      if (tp0 != statMpz)
        report_error(
            string(
                "Argument to mpz_to_mpq does not have type \"mpz\".\n")
            + string("1. the argument: ") + e->kids[0]->toString()
            + string("\n1. its type: ") + tp0->toString());

      return statMpq;
    }

    case IFNEG:
    case IFZERO:
    {
      Expr *tp0 = check_code(e->kids[0]);
      tp0 = tp0->followDefs();
      if (tp0 != statMpz && tp0 != statMpq)
        report_error(
            string("Argument to mp_if does not have type \"mpz\" or \"mpq\".\n")
            + string("1. the argument: ") + e->kids[0]->toString()
            + string("\n1. its type: ") + tp0->toString());

      SymSExpr *tp1 = (SymSExpr *)check_code(e->kids[1]);
      SymSExpr *tp2 = (SymSExpr *)check_code(e->kids[2]);
      if (tp1 != tp2)
      {
        report_error(
            string("\"mp_if\" used with expressions that do not ")
            + string("have equal datatypes\nfor their types.\n")
            + string("0. 0'th expression: ") + e->kids[0]->toString()
            + string("\n1. first expression: ") + e->kids[1]->toString()
            + string("\n2. second expression: ") + e->kids[2]->toString()
            + string("\n3. first expression's type: ") + tp1->toString()
            + string("\n4. second expression's type: ") + tp2->toString());
      }
      return tp1;
    }

    case FAIL:
    {
      Expr *tp = check_code(e->kids[0]);
      if (tp != statType)
        report_error(string("\"fail\" is used with an expression which is ")
                     + string("not a type.\n1. the expression :")
                     + e->kids[0]->toString() + string("\n2. its type: ")
                     + tp->toString());
      return e->kids[0];
    }
    case MARKVAR:
    {
      SymSExpr *tp = (SymSExpr *)check_code(e->kids[1]);

      Expr *tptp = NULL;

      if (tp->getclass() == SYMS_EXPR && !tp->val)
      {
        tptp = symbols->get(tp->s.c_str()).second;
      }

      if (!tptp->isType(statType))
      {
        string errstr =
            (string("\"markvar\" is used with an expression which ")
             + string("cannot be a lambda-bound variable.\n")
             + string("1. the expression :") + e->kids[1]->toString()
             + string("\n2. its type: ") + tp->toString());
        report_error(errstr);
      }

      return tp;
    }

    case IFMARKED:
    {
      SymSExpr *tp = (SymSExpr *)check_code(e->kids[1]);

      Expr *tptp = NULL;

      if (tp->getclass() == SYMS_EXPR && !tp->val)
      {
        tptp = symbols->get(tp->s.c_str()).second;
      }

      if (!tptp->isType(statType))
      {
        string errstr =
            (string("\"ifmarked\" is used with an expression which ")
             + string("cannot be a lambda-bound variable.\n")
             + string("1. the expression :") + e->kids[1]->toString()
             + string("\n2. its type: ") + tp->toString());
        report_error(errstr);
      }

      SymSExpr *tp1 = (SymSExpr *)check_code(e->kids[2]);
      SymSExpr *tp2 = (SymSExpr *)check_code(e->kids[3]);
      if (tp1->getclass() != SYMS_EXPR || tp1->val || tp1 != tp2)
        report_error(
            string("\"ifmarked\" used with expressions that do not ")
            + string("have equal simple datatypes\nfor their types.\n")
            + string("0. 0'th expression: ") + e->kids[1]->toString()
            + string("\n1. first expression: ") + e->kids[2]->toString()
            + string("\n2. second expression: ") + e->kids[3]->toString()
            + string("\n3. first expression's type: ") + tp1->toString()
            + string("\n4. second expression's type: ") + tp2->toString());
      return tp1;
    }
    case COMPARE:
    {
      SymSExpr *tp0 = (SymSExpr *)check_code(e->kids[0]);
      if (tp0->getclass() != SYMS_EXPR || tp0->val)
      {
        string errstr0 =
            (string("\"compare\" is used with a first expression which ")
             + string("cannot be a lambda-bound variable.\n")
             + string("1. the expression :") + e->kids[0]->toString()
             + string("\n2. its type: ") + tp0->toString());
        report_error(errstr0);
      }

      SymSExpr *tp1 = (SymSExpr *)check_code(e->kids[1]);

      if (tp1->getclass() != SYMS_EXPR || tp1->val)
      {
        string errstr1 =
            (string("\"compare\" is used with a second expression which ")
             + string("cannot be a lambda-bound variable.\n")
             + string("1. the expression :") + e->kids[1]->toString()
             + string("\n2. its type: ") + tp1->toString());
        report_error(errstr1);
      }

      SymSExpr *tp2 = (SymSExpr *)check_code(e->kids[2]);
      SymSExpr *tp3 = (SymSExpr *)check_code(e->kids[3]);
      if (tp2->getclass() != SYMS_EXPR || tp2->val || tp2 != tp3)
        report_error(
            string("\"compare\" used with expressions that do not ")
            + string("have equal simple datatypes\nfor their types.\n")
            + string("\n1. first expression: ") + e->kids[2]->toString()
            + string("\n2. second expression: ") + e->kids[3]->toString()
            + string("\n3. first expression's type: ") + tp2->toString()
            + string("\n4. second expression's type: ") + tp3->toString());
      return tp2;
    }
    case IFEQUAL:
    {
      SymSExpr *tp0 = (SymSExpr *)check_code(e->kids[0]);
      SymSExpr *tp1 = (SymSExpr *)check_code(e->kids[1]);

      if (tp0 != tp1)
      {
        report_error(
            string("\"ifequal\" used with compare expressions that do not ")
            + string("have equal types\n") + string("\n1. first expression: ")
            + e->kids[0]->toString() + string("\n2. second expression: ")
            + e->kids[1]->toString() + string("\n3. first expression's type: ")
            + tp0->toString() + string("\n4. second expression's type: ")
            + tp1->toString());
      }

      SymSExpr *tpc1 = (SymSExpr *)check_code(e->kids[2]);
      SymSExpr *tpc2 = (SymSExpr *)check_code(e->kids[3]);
      if (tpc1->getclass() != SYMS_EXPR || tpc1->val || tpc1 != tpc2)
        report_error(
            string("\"ifequal\" used with return expressions that do not ")
            + string("have equal simple datatypes\nfor their types.\n")
            + string("\n1. first expression: ") + e->kids[2]->toString()
            + string("\n2. second expression: ") + e->kids[3]->toString()
            + string("\n3. first expression's type: ") + tpc1->toString()
            + string("\n4. second expression's type: ") + tpc2->toString());
      return tpc1;
    }
    case MATCH:
    {
      SymSExpr *scruttp = (SymSExpr *)check_code(e->kids[0]);
      Expr* kind = compute_kind(scruttp);
      if (kind != statType && !scruttp->isDatatype())
      {
        report_error(
            string(
                "The match scrutinee's type is neither proper, nor a datatype")
            + string("\n1. the type: ") + scruttp->toString()
            + string("\n2. its kind: ")
            + (kind == nullptr ? string("none") : kind->toString()));
      }

      int i = 1;
      Expr **cur = &e->kids[i];
      Expr *mtp = NULL;
      Expr *c_or_default;
      CExpr *c;
      while ((c_or_default = *cur++))
      {
        Expr *tp = NULL;
        CExpr *pat = NULL;
        if (c_or_default->getop() != CASE)
          // this is the default of the MATCH
          tp = check_code(c_or_default);
        else
        {
          // this is a CASE of the MATCH
          c = (CExpr *)c_or_default;
          pat = (CExpr *)c->kids[0];  // might be just a SYMS_EXPR
          if (pat->getclass() == SYMS_EXPR)
            tp = check_code(c->kids[1]);
          else
          {
            // extend type context and then check the body of the case
            vector<pair<Expr *, Expr *> > prevs;
            vector<Expr *> vars;
            SymSExpr *ctor = (SymSExpr *)pat->collect_args(vars);
            CExpr *ctortp = (CExpr *)symbols->get(ctor->s.c_str()).second;
            CExpr *curtp = ctortp;
            for (int i = 0, iend = vars.size(); i < iend; i++)
            {
              if (curtp->followDefs()->getop() != PI)
                report_error(
                    string("Too many arguments to a constructor in")
                    + string(" a pattern.\n1. the pattern: ") + pat->toString()
                    + string("\n2. the head's type: " + ctortp->toString()));
              prevs.push_back(symbols->insert(
                  ((SymSExpr *)vars[i])->s.c_str(),
                  pair<Expr *, Expr *>(
                      NULL, ((CExpr *)(curtp->followDefs()))->kids[1])));
              curtp = (CExpr *)((CExpr *)(curtp->followDefs()))->kids[2];
            }
            // if we have not consumed enough pattern arguments
            if (curtp->followDefs()->getop() == PI)
            {
              report_error(
                  string("Too few arguments to a constructor in")
                  + string(" a pattern.\n1. the pattern: ") + pat->toString()
                  + string("\n2. the head's type: " + ctortp->toString()));
            }

            tp = check_code(c->kids[1]);

            for (int i = 0, iend = prevs.size(); i < iend; i++)
            {
              symbols->insert(((SymSExpr *)vars[i])->s.c_str(), prevs[i]);
            }
          }
        }

        // check that the type for the body of this case -- or the default value
        // -- matches the type for the previous case if we had one.

        if (!mtp)
          mtp = tp;
        else if (!mtp->defeq(tp))
          report_error(
              string("Types for bodies of match cases or the default differ.")
              + string("\n1. type for first case's body: ") + mtp->toString()
              + (pat == NULL ? string("\n2. type for the default")
                             : (string("\n2. type for the body of case for ")
                                + pat->toString()))
              + string(": ") + tp->toString());
      }

      return mtp;
    }
  }  // end switch

  report_error("Type checking an unrecognized form of code (internal error).");
  return NULL;
}

bool dbg_prog;
bool run_scc;
int dbg_prog_indent_lvl = 0;

void dbg_prog_indent(std::ostream &os)
{
  for (int i = 0; i < dbg_prog_indent_lvl; i++) os << " ";
}

Expr *run_code(Expr *_e)
{
start_run_code:
  CExpr *e = (CExpr *)_e;
  if (e)
  {
    // std::cout << ". ";
    // e->print( std::cout );
    // std::cout << std::endl;
    // std::cout << e->getop() << " " << e->getclass() << std::endl;
  }
  switch (e->getop())
  {
    case NOT_CEXPR:
      switch (e->getclass())
      {
        case INT_EXPR:
        case RAT_EXPR: e->inc(); return e;
        case HOLE_EXPR:
        {
          Expr *tmp = e->followDefs();
          if (tmp == e)
            report_error("Encountered an unfilled hole running code.");
          tmp->inc();
          return tmp;
        }
        case SYMS_EXPR:
        case SYM_EXPR:
        {
          Expr *tmp = e->followDefs();
          // std::cout << "follow def = ";
          // tmp->print( std::cout );
          // std::cout << std::endl;
          if (tmp == e)
          {
            e->inc();
            return e;
          }
          tmp->inc();
          return tmp;
        }
      }
    case FAIL: return NULL;
    case DO:
    {
      Expr *tmp = run_code(e->kids[0]);
      if (!tmp) return NULL;
      tmp->dec();
      _e = e->kids[1];
      goto start_run_code;
    }
    case LET:
    {
      Expr *r0 = run_code(e->kids[1]);
      if (!r0) return NULL;
      SymExpr *var = (SymExpr *)e->kids[0];
      Expr *prev = var->val;
      var->val = r0;
      Expr *r1 = run_code(e->kids[2]);
      var->val = prev;
      r0->dec();
      return r1;
    }
    case ADD:
    case MUL:
    case DIV:
    {
      Expr *r1 = run_code(e->kids[0]);
      if (!r1) return NULL;
      Expr *r2 = run_code(e->kids[1]);
      if (!r2) return NULL;
      if (r1->getclass() == INT_EXPR && r2->getclass() == INT_EXPR)
      {
        mpz_t r;
        mpz_init(r);
        if (e->getop() == ADD)
          mpz_add(r, ((IntExpr *)r1)->n, ((IntExpr *)r2)->n);
        else if (e->getop() == MUL)
          mpz_mul(r, ((IntExpr *)r1)->n, ((IntExpr *)r2)->n);
        else if (e->getop() == DIV)
          mpz_cdiv_q(r, ((IntExpr *)r1)->n, ((IntExpr *)r2)->n);
        r1->dec();
        r2->dec();
        return new IntExpr(r);
      }
      else if (r1->getclass() == RAT_EXPR && r2->getclass() == RAT_EXPR)
      {
        mpq_t q;
        mpq_init(q);
        if (e->getop() == ADD)
          mpq_add(q, ((RatExpr *)r1)->n, ((RatExpr *)r2)->n);
        else if (e->getop() == MUL)
          mpq_mul(q, ((RatExpr *)r1)->n, ((RatExpr *)r2)->n);
        else if (e->getop() == DIV)
          mpq_div(q, ((RatExpr *)r1)->n, ((RatExpr *)r2)->n);
        r1->dec();
        r2->dec();
        return new RatExpr(q);
      }
      else
      {
        // std::cout << "An arithmetic operation failed. " << r1->getclass() <<
        // " " << r2->getclass() << std::endl;
        r1->dec();
        r2->dec();
        return NULL;
      }
    }
    case NEG:
    {
      Expr *r1 = run_code(e->kids[0]);
      if (!r1) return NULL;
      if (r1->getclass() == INT_EXPR)
      {
        mpz_t r;
        mpz_init(r);
        mpz_neg(r, ((IntExpr *)r1)->n);
        r1->dec();
        return new IntExpr(r);
      }
      else if (r1->getclass() == RAT_EXPR)
      {
        mpq_t q;
        mpq_init(q);
        mpq_neg(q, ((RatExpr *)r1)->n);
        r1->dec();
        return new RatExpr(q);
      }
      else
      {
        std::cout << "An arithmetic negation failed. " << r1->getclass()
                  << std::endl;
        //((SymSExpr*)r1)->val->print( std::cout );
        std::cout << ((SymSExpr *)r1)->val << std::endl;
        r1->dec();
        return NULL;
      }
    }
    case MPZ_TO_MPQ:
    {
      Expr *r1 = run_code(e->kids[0]);
      if (!r1) return NULL;
      mpq_t r;
      mpq_init(r);
      mpq_set_num(r, ((IntExpr *)r1)->n);
      return new RatExpr(r);
    }
    case IFNEG:
    case IFZERO:
    {
      Expr *r1 = run_code(e->kids[0]);
      if (!r1) return NULL;

      bool cond = true;
      if (r1->getclass() == INT_EXPR)
      {
        if (e->getop() == IFNEG)
          cond = mpz_sgn(((IntExpr *)r1)->n) < 0;
        else if (e->getop() == IFZERO)
          cond = mpz_sgn(((IntExpr *)r1)->n) == 0;
      }
      else if (r1->getclass() == RAT_EXPR)
      {
        if (e->getop() == IFNEG)
          cond = mpq_sgn(((RatExpr *)r1)->n) < 0;
        else if (e->getop() == IFZERO)
          cond = mpq_sgn(((RatExpr *)r1)->n) == 0;
      }
      else
      {
        std::cout << "An arithmetic if-expression failed. " << r1->getclass()
                  << std::endl;
        r1->dec();
        return NULL;
      }
      r1->dec();

      if (cond)
        _e = e->kids[1];
      else
        _e = e->kids[2];
      goto start_run_code;
    }
    case IFMARKED:
    {
      Expr *r1 = run_code(e->kids[1]);
      if (!r1) return NULL;
      if (r1->getclass() != SYM_EXPR && r1->getclass() != SYMS_EXPR)
      {
        r1->dec();
        return NULL;
      }
      if (((SymExpr *)r1)->getmark(((IntExpr *)e->kids[0])->get_num()))
      {
        r1->dec();
        _e = e->kids[2];
        goto start_run_code;
      }
      // else
      r1->dec();
      _e = e->kids[3];
      goto start_run_code;
    }
    case COMPARE:
    {
      Expr *r1 = run_code(e->kids[0]);
      if (!r1) return NULL;
      if (r1->getclass() != SYM_EXPR && r1->getclass() != SYMS_EXPR)
      {
        r1->dec();
        return NULL;
      }
      Expr *r2 = run_code(e->kids[1]);
      if (!r2) return NULL;
      if (r2->getclass() != SYM_EXPR && r2->getclass() != SYMS_EXPR)
      {
        r2->dec();
        return NULL;
      }
      if (r1 < r2)
      {
        r1->dec();
        _e = e->kids[2];
        goto start_run_code;
      }
      // else
      r2->dec();
      _e = e->kids[3];
      goto start_run_code;
    }
    case IFEQUAL:
    {
      Expr *r1 = run_code(e->kids[0]);
      if (!r1) return NULL;
      Expr *r2 = run_code(e->kids[1]);
      if (!r2) return NULL;
      if (r1->defeq(r2))
      {
        r1->dec();
        r2->dec();
        _e = e->kids[2];
        goto start_run_code;
      }
      // else
      r1->dec();
      r2->dec();
      _e = e->kids[3];
      goto start_run_code;
    }
    case MARKVAR:
    {
      Expr *r1 = run_code(e->kids[1]);
      if (!r1) return NULL;
      if (r1->getclass() != SYM_EXPR && r1->getclass() != SYMS_EXPR)
      {
        r1->dec();
        return NULL;
      }
      if (((SymExpr *)r1)->getmark(((IntExpr *)e->kids[0])->get_num()))
        ((SymExpr *)r1)->clearmark(((IntExpr *)e->kids[0])->get_num());
      else
        ((SymExpr *)r1)->setmark(((IntExpr *)e->kids[0])->get_num());
      return r1;
    }
    case MATCH:
    {
      Expr *scrut = run_code(e->kids[0]);
      if (!scrut) return 0;
      // Apply WHR to c-expressions, otherwise you don't really know the head.
      if (scrut->getclass() == CEXPR)
      {
        Expr *tmp = static_cast<CExpr*>(scrut)->whr();
        // If a new expression is returned, dec the old RC
        if (tmp != scrut)
        {
          scrut->dec();
          scrut = tmp;
        }
      }
      vector<Expr *> args;
      Expr *hd = scrut->collect_args(args);
      Expr **cases = &e->kids[1];
      // CExpr *c;
      Expr *c_or_default;
      while ((c_or_default = *cases++))
      {
        if (c_or_default->getop() != CASE)
        {
          // std::cout << "run the default " << std::endl;
          // c_or_default->print( std::cout );
          // this is the default of the MATCH
          return run_code(c_or_default);
        }

        // this is a CASE of the MATCH
        CExpr *c = (CExpr *)c_or_default;
        Expr *p = c->kids[0];
        if (hd == p->get_head())
        {
          vector<Expr *> vars;
          p->collect_args(vars);
          int jend = args.size();
          vector<Expr *> old_vals(jend);
          for (int j = 0; j < jend; j++)
          {
            SymExpr *var = (SymExpr *)vars[j];
            old_vals[j] = var->val;
            var->val = args[j];
            args[j]->inc();
          }
          scrut->dec();
          Expr *ret = run_code(c->kids[1] /* the body of the case */);
          for (int j = 0; j < jend; j++)
          {
            ((SymExpr *)vars[j])->val = old_vals[j];
            args[j]->dec();
          }
          return ret;
        }
      }
      break;
    }
    case APP:
    {
      vector<Expr *> args;
      Expr *hd = e->collect_args(args);
      for (int i = 0, iend = args.size(); i < iend; i++)
        if (!(args[i] = run_code(args[i])))
        {
          for (int j = 0; j < i; j++) args[j]->dec();
          return NULL;
        }
      if (hd->getop() != PROG)
      {
        hd->inc();
        Expr *tmp = Expr::build_app(hd, args);
        return tmp;
      }

      assert(hd->getclass() == CEXPR);
      CExpr *prog = (CExpr *)hd;
      assert(prog->kids[1]->getclass() == CEXPR);
      Expr **cur = ((CExpr *)prog->kids[1])->kids;
      vector<Expr *> old_vals;
      SymExpr *var;
      size_t i = 0;

      if (run_scc && e->get_head(false)->getclass() == SYMS_EXPR)
      {
        // std::cout << "running " << ((SymSExpr*)e->get_head( false
        // ))->s.c_str() << " with " << (int)args.size() << " arguments" <<
        // std::endl;
        Expr *ret = run_compiled_scc(e->get_head(false), args);
        for (int i = 0, iend = args.size(); i < iend; i++)
        {
          args[i]->dec();
        }
        // ret->inc();
        return ret;
      }
      else
      {
        while ((var = (SymExpr *)*cur++))
        {
          // Check whether not enough arguments were supplied
          if (i >= args.size())
          {
            for (size_t i = 0; i < args.size(); i++)
            {
              args[i]->dec();
            }
            return NULL;
          }

          old_vals.push_back(var->val);
          var->val = args[i++];
        }

        // Check whether too many arguments were supplied
        if (i < args.size())
        {
          for (size_t i = 0; i < args.size(); i++)
          {
            args[i]->dec();
          }
          return NULL;
        }

        if (dbg_prog)
        {
          dbg_prog_indent(cout);
          cout << "[";
          e->print(cout);
          cout << "\n";
        }
        dbg_prog_indent_lvl++;

        Expr *ret = run_code(prog->kids[2]);

        dbg_prog_indent_lvl--;
        if (dbg_prog)
        {
          dbg_prog_indent(cout);
          cout << "= ";
          if (ret)
            ret->print(cout);
          else
            cout << "fail";
          cout << "]\n";
        }

        cur = ((CExpr *)prog->kids[1])->kids;
        i = 0;
        while ((var = (SymExpr *)*cur++))
        {
          assert(i < args.size());
          args[i]->dec();
          var->val = old_vals[i++];
        }
        return ret;
      }
    }
  }  // end switch
  return NULL;
}
