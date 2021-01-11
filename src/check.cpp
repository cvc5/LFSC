#include "check.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "code.h"
#include "expr.h"
#include "libwriter.h"
#include "sccwriter.h"
#include "trie.h"
#ifndef _MSC_VER
#include <libgen.h>
#endif
#include <string.h>
#include <time.h>
#include <stack>
#include <utility>
#include "print_smt2.h"
#include "scccode.h"

using namespace std;
#ifndef _MSC_VER
using namespace __gnu_cxx;
#endif

symmap2 progs;
std::vector<Expr *> ascHoles;

Trie<pair<Expr *, Expr *> > *symbols = new Trie<pair<Expr *, Expr *> >;

hash_map<string, bool> imports;
std::map<SymExpr *, int> mark_map;
std::vector<std::pair<std::string, std::pair<Expr *, Expr *> > >
    local_sym_names;

bool tail_calls = true;
bool big_check = true;

Expr *call_run_code(Expr *code)
{
  if (dbg_prog)
  {
    cout << "[Running ";
    code->print(cout);
    cout << "\n";
  }
  Expr *computed_result = run_code(code);
  if (dbg_prog)
  {
    cout << "] returning ";
    if (computed_result)
      computed_result->print(cout);
    else
      cout << "fail";
    cout << "\n";
  }
  return computed_result;
}

Expr *statType = new CExpr(TYPE);
Expr *statKind = new CExpr(KIND);
Expr *statMpz = new CExpr(MPZ);
Expr *statMpq = new CExpr(MPQ);

int open_parens = 0;

// only call in check()
void eat_rparen()
{
  eat_token(Token::Close);
  open_parens--;
}

void eat_excess(int prev)
{
  while (open_parens > prev) eat_rparen();
}

/* There are four cases for check():

1. expected=0, create is false: check() sets computed to be the classifier of
   the checked term.

2. expected=0, create is true: check() returns
   the checked expression and sets computed to be its classifier.

3. expected is non-null, create is false: check returns NULL.

4. expected is non-null, create is true: check returns the term that
   was checked.

We consume the reference for expected, to enable tail calls in the
application case.

If is_hole is NULL, then the expression parsed may not be a hole.
Otherwise, it may be, and we will set *is_hole to true if it is
(but leave *is_hole alone if it is not).

*/

bool allow_run = false;
int app_rec_level = 0;

Expr *check(bool create,
            Expr *expected,
            Expr **computed = NULL,
            bool *is_hole = 0,
            bool return_pos = false,
            bool inAsc = false)
{
start_check:
  // std::cout << "check code ";
  // if( expected )
  //  expected->print( std::cout );
  // std::cout << std::endl;
  switch (next_token())
  {
    case Token::Open:
    {
      open_parens++;

      Token::Token c = next_token();
      switch (c)
      {
        case Token::Eof:
        {
          report_error("Unexpected end of file.");
          break;
        }
        case Token::Bang:
        {  // the pi case
          string id(prefix_id());
#ifdef DEBUG_SYM_NAMES
          Expr *sym = new SymSExpr(id, SYMS_EXPR);
#else
          Expr *sym = new SymExpr(id);
          // std::cout << "name " << id << " " << sym << std::endl;
#endif
          allow_run = true;
          int prevo = open_parens;
          Expr *domain = check(true, statType);
          eat_excess(prevo);
          allow_run = false;
          pair<Expr *, Expr *> prev =
              symbols->insert(id.c_str(), pair<Expr *, Expr *>(sym, domain));
          if (expected) expected->inc();
          Expr *range = check(create, expected, computed, NULL, return_pos);
          eat_excess(prevo);
          eat_rparen();

          symbols->insert(id.c_str(), prev);
          if (expected)
          {
            int o = expected->followDefs()->getop();
            expected->dec();
            if (o != TYPE && o != KIND)
              report_error(
                  string("The expected classifier for a pi abstraction")
                  + string("is neither \"type\" nor \"kind\".\n")
                  + string("1. the expected classifier: ")
                  + expected->toString());
            if (create)
            {
              CExpr *ret = new CExpr(PI, sym, domain, range);
              ret->calc_free_in();
              return ret;
            }
            return 0;
          }
          else
          {
            if (create)
            {
              CExpr *ret = new CExpr(PI, sym, domain, range);
              ret->calc_free_in();
              return ret;
            }
            int o = (*computed)->followDefs()->getop();
            if (o != TYPE && o != KIND)
              report_error(string("The classifier for the range of a pi")
                           + string("abstraction is neither \"type\" nor ")
                           + string("\"kind\".\n1. the computed classifier: ")
                           + range->toString());
            return 0;
          }
        }
        case Token::Arrow:
        {  // the arrow case
          DeclList decls = check_decl_list(create);
          Expr* ret_kind;
          Expr* ret = check(create, nullptr, &ret_kind);
          for (const auto binding : decls.old_bindings)
          {
            symbols->insert(get<0>(binding).c_str(),
                            {get<1>(binding), get<2>(binding)});
          }
          eat_rparen();
          auto p = build_validate_pi(move(decls.decls), ret, ret_kind, create);
          if (expected)
          {
            // Checked by build-validate pi
            p.second->dec();
          }
          else
          {
            *computed = p.second;
          }
          return p.first;
        }
        case Token::Pound:
        {
          // Annotated lambda case
          string id(prefix_id());
#ifdef DEBUG_SYM_NAMES
          Expr *sym = new SymSExpr(id, SYMS_EXPR);
#else
          Expr *sym = new SymExpr(id);
#endif
          allow_run = true;
          int prevo = open_parens;
          Expr *domain = check(true, statType);
          eat_excess(prevo);
          allow_run = false;
          pair<Expr *, Expr *> prev =
              symbols->insert(id.c_str(), pair<Expr *, Expr *>(sym, domain));
          Expr* rec_expected = nullptr;
          if (expected)
          {
            expected->inc();
            if (expected->followDefs()->getop() != PI)
              report_error(
                  string("The expected classifier for a # (annotated lambda) abstraction")
                  + string("is not a pi")
                  + string("1. the expected classifier: ")
                  + expected->toString());
            CExpr* cexpected = static_cast<CExpr*>(expected->followDefs());
            if (!cexpected->kids[1]->defeq(domain)) {
              report_error(
                  string("The expected domain for a # (annotated lambda) abstraction ")
                  + string("should be: ") + cexpected->kids[1]->toString()
                  + string("\n, but is: ") + domain->toString());
            }
            rec_expected = cexpected->kids[2];
          }
          Expr* rec_computed = nullptr;
          Expr* range =
              check(create, rec_expected, &rec_computed, NULL, return_pos);
          if (expected)
          {
            expected->dec();
          }
          eat_excess(prevo);
          eat_rparen();
          CExpr *tmp = new CExpr(PI, sym, domain, rec_computed);
          tmp->calc_free_in();
          if (tmp->get_free_in())
          {
            std::ostringstream o;
            o << "The type of an annotated lambda is dependent."
              << "\n1. The type    : ";
            tmp->print(o);
            o << "\n2. The variable: ";
            sym->print(o);
            o << "\n3. The body    : ";
            range->print(o);
            report_error(o.str());
          }
          // Since `sym` is the SymSExpr used inside the *value* of this
          // Lambda, having it also be in the type would cause type-checking
          // bindings to change the value.
          //
          // We change the type's symbol to avoid this.
          tmp->kids[0] = new SymSExpr(id);
          *computed = static_cast<Expr*>(tmp);

          symbols->insert(id.c_str(), prev);
          if (create)
          {
            CExpr* ret = new CExpr(LAM, sym, range);
            // Mark this as "cloned" to block no-clone optimization
            ret->setcloned();
            return ret;
          }
          return 0;
        }
        case Token::Percent:
        {  // the case for big lambda
          if (expected || create || !return_pos || !big_check)
            report_error(string("Big lambda abstractions can only be used")
                         + string("in the return position of a \"bigcheck\"\n")
                         + string("command."));
          string id(prefix_id());
#ifdef DEBUG_SYM_NAMES
          SymExpr *sym = new SymSExpr(id, SYMS_EXPR);
#else
          SymExpr *sym = new SymExpr(id);
          // std::cout << "name " << id << " " << sym << std::endl;
#endif

          int prevo = open_parens;
          Expr *expected_domain = check(true, statType);
          eat_excess(prevo);

          pair<Expr *, Expr *> prevpr = symbols->insert(
              id.c_str(), pair<Expr *, Expr *>(sym, expected_domain));
          Expr *prev = prevpr.first;
          Expr *prevtp = prevpr.second;
          expected_domain
              ->inc();  // because we have stored it in the symbol table

          // will clean up local sym name eventually
          local_sym_names.push_back(
              std::pair<std::string, std::pair<Expr *, Expr *> >(id, prevpr));
          if (prev) prev->dec();
          if (prevtp) prevtp->dec();
          create = false;
          expected = NULL;
          // computed unchanged
          is_hole = NULL;
          // return_pos unchanged

          // note we will not store the proper return type in computed.

          goto start_check;
        }

        case Token::ReverseSolidus:
        {  // the lambda case
          if (!expected)
            report_error(string("We are computing a type for a lambda ")
                         + string("abstraction, but we can only check\n")
                         + string("such against a type.  Try inserting an ")
                         + string("ascription (using ':').\n"));
          Expr *orig_expected = expected;
          expected = expected->followDefs();
          if (expected->getop() != PI)
            report_error(
                string("We are type-checking a lambda abstraction, but\n")
                + string("the expected type is not a pi abstraction.\n")
                + string("1. The expected type: ") + expected->toString());
          string id(prefix_id());
#ifdef DEBUG_SYM_NAMES
          SymExpr *sym = new SymSExpr(id, SYMS_EXPR);
#else
          SymExpr *sym = new SymExpr(id);
          // std::cout << "name " << id << " " << sym << std::endl;
#endif

          CExpr *pitp = (CExpr *)expected;
          Expr *expected_domain = pitp->kids[1];
          Expr *expected_range = pitp->kids[2];
          SymExpr *pivar = (SymExpr *)pitp->kids[0];
          if (expected_range->followDefs()->getop() == TYPE)
            report_error(
                string("The expected classifier for a lambda abstraction")
                + string(" a kind, not a type.\n")
                + string("1. The expected classifier: ")
                + expected->toString());

            /* we need to map the pivar to the new sym, because in our
               higher-order matching we may have (_ x) to unify with t.
               The x must be something from an expected type, since only these
               can have holes.  We want to map expected vars x to computed vars
               y, so that we can set the hole to be \ y t, where t contains ys
               but not xs. */

          pair<Expr *, Expr *> prevpr = symbols->insert(
              id.c_str(), pair<Expr *, Expr *>(sym, expected_domain));
          Expr *prev = prevpr.first;
          Expr *prevtp = prevpr.second;

          Expr *prev_pivar_val = pivar->val;
          sym->inc();
          pivar->val = sym;

          expected_domain
              ->inc();  // because we have stored it in the symbol table
          expected_range->inc();  // because we will pass it to a recursive call

          if (tail_calls && big_check && return_pos && !create)
          {
            // will clean up local sym name eventually
            local_sym_names.push_back(
                std::pair<std::string, std::pair<Expr *, Expr *> >(id, prevpr));
            if (prev_pivar_val) prev_pivar_val->dec();
            if (prev) prev->dec();
            if (prevtp) prevtp->dec();
            orig_expected->dec();
            create = false;
            expected = expected_range;
            computed = NULL;
            is_hole = NULL;
            // return_pos unchanged
            goto start_check;
          }
          else
          {
            int prev = open_parens;
            Expr *range = check(create, expected_range, NULL, NULL, return_pos);
            eat_excess(prev);
            eat_rparen();

            symbols->insert(id.c_str(), prevpr);

            expected_domain
                ->dec();  // because removed from the symbol table now

            pivar->val = prev_pivar_val;

            orig_expected->dec();

            sym->dec();  // the pivar->val reference
            if (create) return new CExpr(LAM, sym, range);
            sym->dec();  // the symbol table reference, otherwise in the new LAM
            return 0;
          }
        }
        case Token::Caret:
        {  // the run case
          if (!allow_run || !create || !expected)
            report_error(string("A run expression (operator \"^\") appears in")
                         + string(" a disallowed position."));

          Expr *code = read_code();
          // string errstr = (string("The first argument in a run expression
          // must be")
          //   +string(" a call to a program.\n1. the argument: ")
          //   +code->toString());

          /* determine expected type of the result term, and make sure
             the code term is an allowed one. */
#if 0
      Expr *progret;
      if (code->isArithTerm())
        progret = statMpz;
      else {
        if (code->getop() != APP)
          report_error(errstr);

        CExpr *call = (CExpr *)code;

        // prog is not known to be a SymExpr yet
        CExpr *prog = (CExpr *)call->get_head();

        if (prog->getop() != PROG)
          report_error(errstr);

        progret = prog->kids[0]->get_body();
      }
#else
          Expr *progret = NULL;
          if (code->isArithTerm())
            progret = statMpz;
          else
          {
            if (code->getop() == APP)
            {
              CExpr *call = (CExpr *)code;

              // prog is not known to be a SymExpr yet
              CExpr *prog = (CExpr *)call->get_head();

              if (prog->getop() == PROG) progret = prog->kids[0]->get_body();
            }
          }
#endif
          /* determine expected type of the result term, and make sure
                  the code term is an allowed one. */
          // Expr* progret = check_code( code );

          /* the next term cannot be a hole where run expressions are
             introduced. When they are checked in applications, it can be. */
          int prev = open_parens;
          if (progret) progret->inc();
          Expr *trm = check(true, progret);
          eat_excess(prev);
          eat_rparen();

          if (expected->getop() != TYPE)
            report_error(
                string("The expected type for a run expression is not ")
                + string("\"type\".\n") + string("1. The expected type: ")
                + expected->toString());
          expected->dec();
          return new CExpr(RUN, code, trm);
        }

        case Token::Colon:
        {  // the ascription case
          statType->inc();
          int prev = open_parens;
          Expr *tp = check(true, statType, NULL, NULL, false, true);
          eat_excess(prev);

          if (!expected) tp->inc();

          Expr *trm = check(create, tp, NULL, NULL, return_pos);
          eat_excess(prev);
          eat_rparen();
          if (expected)
          {
            if (!expected->defeq(tp))
              report_error(
                  string("The expected type does not match the ")
                  + string("ascribed type in an ascription.\n")
                  + string("1. The expected type: ") + expected->toString()
                  + string("\n2. The ascribed type: ") + tp->toString());

            // no need to dec tp, since it was consumed by the call to check
            expected->dec();
            if (create) return trm;
            trm->dec();
            return 0;
          }
          else
          {
            *computed = tp;
            if (create) return trm;
            return 0;
          }
        }
        case Token::At:
        {  // the local definition case
          string id(prefix_id());
#ifdef DEBUG_SYM_NAMES
          SymExpr *sym = new SymSExpr(id, SYMS_EXPR);
#else
          SymExpr *sym = new SymExpr(id);
#endif
          int prev_open = open_parens;
          Expr *tp_of_trm = NULL;
          Expr *trm = check(true, NULL, &tp_of_trm);
          eat_excess(prev_open);

          sym->val = trm;

          pair<Expr *, Expr *> prevpr =
              symbols->insert(id.c_str(), pair<Expr *, Expr *>(sym, tp_of_trm));
          Expr *prev = prevpr.first;
          Expr *prevtp = prevpr.second;

          if (tail_calls && big_check && return_pos && !create)
          {
            if (prev) prev->dec();
            if (prevtp) prevtp->dec();
            // all parameters to check() unchanged here
            goto start_check;
          }
          else
          {
            int prev_open = open_parens;
            Expr *body = check(create, expected, computed, is_hole, return_pos);
            eat_excess(prev_open);
            eat_rparen();

            symbols->insert(id.c_str(), prevpr);

            tp_of_trm->dec();  // because removed from the symbol table now

            sym->dec();
            return body;
          }
        }
        case Token::Tilde:
        {
          int prev = open_parens;
          Expr *e = check(create, expected, computed, is_hole, return_pos);
          eat_excess(prev);
          eat_rparen();

          // this has been only very lightly tested -- ads.

          if (expected)
          {
            if (expected != statMpz && expected != statMpq)
              report_error(
                  "Negative sign where an numeric expression is expected.");
          }
          else
          {
            if ((*computed) != statMpz && (*computed) != statMpq)
              report_error(
                  "Negative sign where an numeric expression is expected.");
          }

          if (create)
          {
            if (e->getclass() == INT_EXPR)
            {
              IntExpr *ee = (IntExpr *)e;
              mpz_neg(ee->n, ee->n);
              return ee;
            }
            else if (e->getclass() == RAT_EXPR)
            {
              RatExpr *ee = (RatExpr *)e;
              mpq_neg(ee->n, ee->n);
              return ee;
            }
            else
            {
              report_error(
                  "Negative sign with expr that is not an int. literal.");
            }
          }
          else
            return 0;
        }
        default:
        {  // the application case
          reinsert_token(c);
          Expr *head_computed;
          int prev = open_parens;
          Expr *headtrm = check(create, 0, &head_computed);
          eat_excess(prev);

          CExpr *headtp = (CExpr *)head_computed->followDefs();
          headtp->inc();
          head_computed->dec();
          if (headtp->cloned())
          {
            // we must clone
            Expr *orig_headtp = headtp;
            headtp = (CExpr *)headtp->clone();
            orig_headtp->dec();
          }
          else
            headtp->setcloned();
#ifdef DEBUG_APPS
          char tmp[100];
          sprintf(tmp, "(%d) ", app_rec_level++);
          cout << tmp << "{ headtp = ";
          headtp->debug();
#endif
          Token::Token c;
          vector<HoleExpr *> holes;
          while ((c = next_token()) != Token::Close)
          {
            reinsert_token(c);
            if (headtp->getop() != PI)
              report_error(
                  string("The type of an applied term is not ")
                  + string("a pi-type.\n")
                  + string("\n1. the type of the term: ") + headtp->toString()
                  + (headtrm ? (string("\n2. the term: ") + headtrm->toString())
                             : string("")));
            SymExpr *headtp_var = (SymExpr *)headtp->kids[0];
            Expr *headtp_domain = headtp->kids[1];
            Expr *headtp_range = headtp->kids[2];
            if (headtp_domain->getop() == RUN)
            {
              CExpr *run = (CExpr *)headtp_domain;
              Expr *code = run->kids[0];
              Expr *expected_result = run->kids[1];
              Expr *computed_result = call_run_code(code);
              if (!computed_result)
                report_error(string("A side condition failed.\n")
                             + string("1. the side condition: ")
                             + code->toString());
              if (!expected_result->defeq(computed_result))
                report_error(string("The expected result of a side condition ")
                             + string("does not match the computed result.\n")
                             + string("1. expected result: ")
                             + expected_result->toString()
                             + string("\n2. computed result: ")
                             + computed_result->toString());
              computed_result->dec();
            }
            else
            {
              // check an argument
              bool var_in_range =
                  headtp->get_free_in();  // headtp_range->free_in(headtp_var);
              bool arg_is_hole = false;
              bool consumed_arg = false;

              bool create_arg = (create || var_in_range);

              headtp_domain->inc();

              if (tail_calls && !create_arg && headtp_range->getop() != PI)
              {
                // we can make a tail call to check() here.

                if (expected)
                {
                  if (!expected->defeq(headtp_range))
                    report_error(string("The type expected for an application ")
                                 + string("does not match the computed type.\n")
                                 + string("1. The expected type: ")
                                 + expected->toString()
                                 + string("\n2. The computed type: ")
                                 + headtp_range->toString()
                                 + (headtrm ? (string("\n3. the application: ")
                                               + headtrm->toString())
                                            : string("")));
                  expected->dec();
                }
                else
                {
                  headtp_range->inc();
                  *computed = headtp_range;
                }

                headtp->dec();

                // same as below
                for (int i = 0, iend = holes.size(); i < iend; i++)
                {
                  if (!holes[i]->val)
                    /* if the hole is free in the domain, we will be filling
                       it in when we make our tail call, since the domain
                       is the expected type for the argument */
                    if (!headtp_domain->free_in(holes[i]))
                      report_error(string("A hole was left unfilled after ")
                                   + string("checking an application.\n"));
                  holes[i]->dec();
                }

                create = false;
                expected = headtp_domain;
                computed = NULL;
                is_hole = NULL;  // the argument cannot be a hole
                                 // return_pos is unchanged

#ifdef DEBUG_APPS
                cout << "Making tail call.\n";
#endif

                goto start_check;
              }

              Expr *arg = check(create_arg, headtp_domain, NULL, &arg_is_hole);
              eat_excess(prev);
              if (create)
              {
                Expr *orig_headtrm = headtrm;
                headtrm = Expr::make_app(headtrm, arg);
                if (orig_headtrm->getclass() == CEXPR)
                {
                  orig_headtrm->dec();
                }
                consumed_arg = true;
              }
              if (var_in_range)
              {
                Expr *tmp = arg->followDefs();
                tmp->inc();
                headtp_var->val = tmp;
              }
              if (arg_is_hole)
              {
                if (consumed_arg)
                  arg->inc();
                else
                  consumed_arg = true;  // not used currently
#ifdef DEBUG_HOLES
                cout << "An argument is a hole: ";
                arg->debug();
#endif
                holes.push_back((HoleExpr *)arg);
              }
            }
            headtp_range->inc();
            headtp->dec();
            headtp = (CExpr *)headtp_range;
          }
          open_parens--;

          // check for remaining RUN in the head's type after all the arguments

          if (headtp->getop() == PI && headtp->kids[1]->getop() == RUN)
          {
            CExpr *run = (CExpr *)headtp->kids[1];
            Expr *code = run->kids[0]->followDefs();
            Expr *expected_result = run->kids[1];
            Expr *computed_result = call_run_code(code);
            if (!computed_result)
              report_error(string("A side condition failed.\n")
                           + string("1. the side condition: ")
                           + code->toString());
            if (!expected_result->defeq(computed_result))
              report_error(string("The expected result of a side condition ")
                           + string("does not match the computed result.\n")
                           + string("1. expected result: ")
                           + expected_result->toString()
                           + string("\n2. computed result: ")
                           + computed_result->toString());
            Expr *tmp = headtp->kids[2];
            tmp->inc();
            headtp->dec();
            headtp = (CExpr *)tmp;
            computed_result->dec();
          }

#ifdef DEBUG_APPS
          for (int i = 0, iend = holes.size(); i < iend; i++)
          {
            cout << tmp << "hole ";
            holes[i]->debug();
          }
          cout << "}";
          app_rec_level--;
#endif

          Expr *ret = 0;
          if (expected)
          {
            if (!expected->defeq(headtp))
            {
              report_error(
                  string("The type expected for an application does not")
                  + string(" match the computed type.(2) \n")
                  + string("1. The expected type: ") + expected->toString()
                  + string("\n2. The computed type: ") + headtp->toString()
                  + (headtrm ? (string("\n3. the application: ")
                                + headtrm->toString())
                             : string("")));
            }
            expected->dec();
            headtp->dec();
            if (create) ret = headtrm;
          }
          else
          {
            *computed = headtp;
            if (create) ret = headtrm;
          }

          /* do this check here to give the defeq() call above a
             chance to fill in some holes */
          for (int i = 0, iend = holes.size(); i < iend; i++)
          {
            if (!holes[i]->val)
            {
              if (inAsc)
              {
#ifdef DEBUG_HOLES
                std::cout << "Ascription Hole: ";
                holes[i]->print(std::cout);
                std::cout << std::endl;
#endif
                ascHoles.push_back(holes[i]);
              }
              else
              {
                report_error(string("A hole was left unfilled after checking")
                             + string(" an application (2).\n"));
              }
            }
            holes[i]->dec();
          }

          return ret;

        }  // end application case
      }
    }
    case Token::Eof:
    {
      report_error("Unexpected end of file.");
      break;
    }

    case Token::Hole:
    {
      if (!is_hole)
        report_error("A hole is being used in a disallowed position.");
      *is_hole = true;
      if (expected) expected->dec();
      return new HoleExpr();
    }
    case Token::Natural:
    {
      if (expected)
      {
        if (expected != statMpz)
          report_error(string("We parsed an integer, but were ")
                       + string("expecting a term of a different type.\n")
                       + string("1. the expected type: ")
                       + expected->toString());
        expected->dec();
      }
      else
      {
        statMpz->inc();
        *computed = statMpz;
      }
      if (create)
      {
        mpz_t num;
        if (mpz_init_set_str(num, token_str(), 10) == -1)
          report_error("Error reading a numeral.");
        return new IntExpr(num);
      }
      else
      {
        return nullptr;
      }
    }
    case Token::Rational:
    {
      if (expected)
      {
        if (expected != statMpq)
          report_error(string("We parsed a rational, but were ")
                       + string("expecting a term of a different type.\n")
                       + string("1. the expected type: ")
                       + expected->toString());
        expected->dec();
      }
      else
      {
        statMpq->inc();
        *computed = statMpq;
      }
      if (create)
      {
        mpq_t num;
        mpq_init(num);
        if (mpq_set_str(num, token_str(), 10) == -1)
          report_error("Error reading a numeral.");
        return new RatExpr(num);
      }
      else
      {
        return nullptr;
      }
    }
    // NB: We could match on identifiers, but by not doing that, we allow
    // (contextual) keyword identifiers
    default:
    {
      string id(token_str());
      pair<Expr *, Expr *> p = symbols->get(id.c_str());
      Expr *ret = p.first;
      Expr *rettp = p.second;
      if (!ret) report_error(string("Undeclared identifier: ") + id);
      if (expected)
      {
        if (!expected->defeq(rettp))
          report_error(
              string("The type expected for a symbol does not")
              + string(" match the symbol's type.\n")
              + string("1. The symbol: ") + id
              + string("\n2. The expected type: ") + expected->toString()
              + string("\n3. The symbol's type: ") + rettp->toString());
        expected->dec();
        if (create)
        {
          ret->inc();
          return ret;
        }
        return 0;
      }
      else
      {
        if (computed)
        {
          *computed = rettp;
          (*computed)->inc();
        }
        if (create)
        {
          ret->inc();
          return ret;
        }
        return 0;
      }
    }
  }

  report_error("Unexpected operator at the start of a term.");
  return 0;
}

std::pair<std::string, Expr*> check_decl_list_item()
{
  Token::Token t = next_token();
  if (t == Token::Open)
  {
    Token::Token t2 = next_token();
    if (t2 == Token::Colon)
    {
      // The (: NAME TYPE) case
      string id(prefix_id());
      Expr* ty = check(true, statType);
      eat_token(Token::Close);
      return {id, ty};
    }
    else
    {
      // This is a TYPE case, that starts with tokens: '(' and t2, where t2 is
      // not `:`.
      // We reinsert those tokens to the stream, and check TYPE.
      reinsert_token(t2);
      reinsert_token(t);
      Expr* dummy;
      Expr* ty = check(true, nullptr, &dummy);
      dummy->dec();
      return {"", ty};
    }
  }
  else
  {
    // This is a TYPE case, that starts with token: '('.
    // We reinsert it an check TYPE.
    reinsert_token(t);
    Expr* ty = check(true, statType);
    return {"", ty};
  }
}

DeclList check_decl_list(const bool create)
{
  std::vector<std::tuple<std::string, Expr*, Expr*>> old_bindings;
  std::vector<std::pair<Expr*, Expr*>> decls;
  // Eat opening '('
  eat_token(Token::Open);
  Token::Token t = next_token();
  // While the list is unclosed
  while (t != Token::Close)
  {
    // Another item in the declaration list.
    reinsert_token(t);
    // Get the (ident, type) pair of the item.
    std::pair<std::string, Expr*> p = check_decl_list_item();
    Expr* sym;
    // Check whether this declaration binds an identifier (has non-empty ident)
    if (p.first.size())
    {
      // It does. Create the symbol, bind it, save the old binding.
#ifdef DEBUG_SYM_NAMES
      sym = new SymSExpr(p.first, SYMS_EXPR);
#else
      sym = new SymExpr(p.first);
#endif
      auto o = symbols->insert(p.first.c_str(), {sym, p.second});
      old_bindings.push_back({p.first, o.first, o.second});
    }
    else
    {
      // It does not. Create a "_" symbol. Do not bind it.
      string id("_");
#ifdef DEBUG_SYM_NAMES
      sym = new SymSExpr(id, SYMS_EXPR);
#else
      sym = new SymExpr(id);
#endif
    }
    // If creating, save this (symbol, type) pair in the list.
    if (create)
    {
      decls.emplace_back(sym, p.second);
    }
    t = next_token();
  }
  // We've closed the list
  return {decls, old_bindings};
}

std::pair<Expr*, Expr*> build_validate_pi(
    std::vector<std::pair<Expr*, Expr*>>&& args,
    Expr* ret,
    Expr* ret_kind,
    bool create)
{
  // Check that the resulting kind is TYPE or KIND
  if (ret_kind->getop() == TYPE || ret_kind->getop() == KIND)
  {
    if (create)
    {
      // If we should create the body, do so.
      for (size_t i = args.size() - 1; i < args.size(); --i)
      {
        ret = new CExpr(PI, args[i].first, args[i].second, ret);
        ret->calc_free_in();
      }
      return {ret, ret_kind};
    }
  }
  else
  {
    report_error(string("Invalid Pi-range: ") + ret->toString());
  }
  // We're not creating the body.
  return {nullptr, ret_kind};
}

std::pair<Expr*, Expr*> build_macro(std::vector<std::pair<Expr*, Expr*>>&& args,
                                    Expr* ret,
                                    Expr* ret_ty)
{
  // For each argument, add a layer of nesting.
  for (size_t i = args.size() - 1; i < args.size(); --i)
  {
    args[i].second->inc();
    CExpr* tmp = new CExpr(PI, args[i].first, args[i].second, ret_ty);
    tmp->calc_free_in();
    // Assert that the type is not dependent.
    if (tmp->get_free_in())
    {
      std::ostringstream o;
      o << "The type of an annotated lambda is dependent."
        << "\n1. The type    : " << *ret_ty
        << "\n2. The variable: " << *args[i].first
        << "\n3. The body    : " << *ret;
      report_error(o.str());
    }
    // Replace the symbol in the type with a copy, so that there isn't "same
    // symbol" confusion between the value and the type.
    // This doesn't break any references to the symbol, because there are no
    // reference to the symbol in the type---it's not dependent!
    tmp->kids[0] = new SymSExpr(static_cast<SymSExpr*>(args[i].first)->s);
    ret = new CExpr(LAM, args[i].first, ret);
    // Mark this as "cloned" to block no-clone optimization
    ret->setcloned();
    ret_ty = tmp;
  }
  return {ret, ret_ty};
}

int check_time;

void check_file(const char *_filename, args a, sccwriter *scw, libwriter *lw)
{
  std::ifstream fs;
  fs.open(_filename, std::fstream::in);
  std::string filenameString(_filename);
  if (!fs.is_open() && filenameString != "stdin")
  {
    report_error(string("Could not open file \"") + _filename
                 + string("\" for reading.\n"));
  }
  check_file(fs, filenameString, a, scw, lw);
  fs.close();
}

void rebind_error(const std::string& id)
{
  stringstream o;
  o << "The top-level identifier \"" << id << "\" was already bound";
  report_error(o.str());
}

void check_file(std::istream& in,
                const std::string& _filename,
                args a,
                sccwriter* scw,
                libwriter* lw)
{
  // from code.h
  dbg_prog = a.show_runs;
  run_scc = a.run_scc;
  tail_calls = !a.no_tail_calls;

  s_lexer = new yyFlexLexer(&in);
  s_filename = _filename;
  init_s_span();

  Token::Token c;
  while ((c = next_token()) != Token::Eof)
  {
    if (c == Token::Open)
    {
      c = next_token();
      switch (c)
      {
        case Token::Define:
        {
          string id(prefix_id());
          Expr* ttp;
          int prevo = open_parens;
          Expr* t = check(true, 0, &ttp, NULL, true);
          eat_excess(prevo);

          int o = ttp->followDefs()->getop();
          if (o == KIND)
            report_error(string("Kind-level definitions are not supported.\n"));
          SymSExpr* s = new SymSExpr(id);
          s->val = t;
          pair<Expr*, Expr*> prev =
              symbols->insert(id.c_str(), pair<Expr*, Expr*>(s, ttp));
          if (prev.first || prev.second)
          {
            rebind_error(id);
          }
          break;
        }
        case Token::Declare:
        {
          string id(prefix_id());
          Expr* ttp;
          int prevo = open_parens;
          Expr* t = check(true, 0, &ttp, NULL, true);
          eat_excess(prevo);

          ttp = ttp->followDefs();
          if (ttp->getop() != TYPE && ttp->getop() != KIND)
            report_error(string("The expression declared for \"") + id
                         + string("\" is neither\na type nor a kind.\n")
                         + string("1. The expression: ") + t->toString()
                         + string("\n2. Its classifier (should be \"type\" ")
                         + string("or \"kind\"): ") + ttp->toString());
          ttp->dec();
          SymSExpr* s = new SymSExpr(id);
          pair<Expr*, Expr*> prev =
              symbols->insert(id.c_str(), pair<Expr*, Expr*>(s, t));
          if (lw) lw->add_symbol(s, t);
          if (prev.first || prev.second)
          {
            rebind_error(id);
          }
          break;
        }
        case Token::DeclareRule:
        {
          // Form: (declare-rule NAME DECL_LIST RESULT)
          // equivalent to: (declare NAME (! decl0id decl0ty (! decl1id decl1ty ... RESULT)))
          string id(prefix_id());
          DeclList decls = check_decl_list(true);
          Expr* ret_kind;
          Expr* ret = check(true, nullptr, &ret_kind);
          // Restore bindings overwritten by decl list
          for (const auto binding : decls.old_bindings)
          {
            symbols->insert(get<0>(binding).c_str(),
                            {get<1>(binding), get<2>(binding)});
          }
          pair<Expr*, Expr*> p =
              build_validate_pi(move(decls.decls), ret, ret_kind, true);
          p.second->dec();
          SymSExpr* s = new SymSExpr(id);
          pair<Expr*, Expr*> prev =
              symbols->insert(id.c_str(), pair<Expr*, Expr*>(s, p.first));
          if (lw) lw->add_symbol(s, p.first);
          if (prev.first || prev.second)
          {
            rebind_error(id);
          }
          break;
        }
        case Token::DeclareType:
        {
          // Form: (declare-type NAME DECL_LIST)
          // equivalent to: (declare NAME (! decl0id decl0ty (! decl1id decl1ty ... type)))
          string id(prefix_id());
          DeclList decls = check_decl_list(true);
          // Restore bindings overwritten by decl list
          for (const auto binding : decls.old_bindings)
          {
            symbols->insert(get<0>(binding).c_str(),
                            {get<1>(binding), get<2>(binding)});
          }
          pair<Expr*, Expr*> p =
              build_validate_pi(move(decls.decls), statType, statKind, true);
          p.second->dec();
          SymSExpr* s = new SymSExpr(id);
          pair<Expr*, Expr*> prev =
              symbols->insert(id.c_str(), pair<Expr*, Expr*>(s, p.first));
          if (lw) lw->add_symbol(s, p.first);
          if (prev.first || prev.second)
          {
            rebind_error(id);
          }
          break;
        }
        case Token::DefineConst:
        {
          // Form: (define-const NAME DECL_LIST RESULT)
          // equivalent to: (define NAME (% decl0id decl0ty (% decl1id decl1ty ... RESULT)))
          string id(prefix_id());
          DeclList decls = check_decl_list(true);
          Expr* ret_ty;
          Expr* ret = check(true, nullptr, &ret_ty);
          pair<Expr*, Expr*> macro =
              build_macro(move(decls.decls), ret, ret_ty);
          // Restore bindings overwritten by decl list
          for (const auto binding : decls.old_bindings)
          {
            symbols->insert(get<0>(binding).c_str(),
                            {get<1>(binding), get<2>(binding)});
          }
          SymSExpr* s = new SymSExpr(id);
          s->val = macro.first;
          pair<Expr*, Expr*> prev =
              symbols->insert(id.c_str(), {s, macro.second});
          if (prev.first || prev.second)
          {
            rebind_error(id);
          }
          break;
        }
        case Token::Check:
        case Token::CheckAssuming:
        {
          // check and check-assuming combined case
          if (run_scc)
          {
            init_compiled_scc();
          }
          int prev = open_parens;
          if (c == Token::Check)
          {
            Expr* computed;
            big_check = true;
            (void)check(false, 0, &computed, NULL, true);

            // print out ascription holes
            for (int a = 0; a < (int)ascHoles.size(); a++)
            {
#ifdef PRINT_SMT2
              print_smt2(ascHoles[a], std::cout);
#else
              ascHoles[a]->print(std::cout);
#endif
              std::cout << std::endl;
            }
            if (!ascHoles.empty()) std::cout << std::endl;
            ascHoles.clear();
            computed->dec();
          }
          else  // CheckAssuming
          {
            DeclList decls = check_decl_list(false);
            Expr* ex_type = check(true, statType, nullptr);
            // consumes the `ex_type` reference
            (void)check(false, ex_type, nullptr);
            for (const auto binding : decls.old_bindings)
            {
              symbols->insert(get<0>(binding).c_str(),
                              {get<1>(binding), get<2>(binding)});
            }
          }

          // clean up local symbols
          for (int a = 0; a < (int)local_sym_names.size(); a++)
          {
            symbols->insert(local_sym_names[a].first.c_str(),
                            local_sym_names[a].second);
          }
          local_sym_names.clear();
          mark_map.clear();

          eat_excess(prev);

          // cleanup();
          // exit(0);
          break;
        }
        case Token::Opaque:
        {
          string id(prefix_id());
          Expr* ttp;
          int prevo = open_parens;
          (void)check(false, 0, &ttp, NULL, true);
          eat_excess(prevo);

          int o = ttp->followDefs()->getop();
          if (o == KIND)
            report_error(string("Kind-level definitions are not supported.\n"));
          SymSExpr* s = new SymSExpr(id);
          pair<Expr*, Expr*> prev =
              symbols->insert(id.c_str(), pair<Expr*, Expr*>(s, ttp));
          if (prev.first || prev.second)
          {
            rebind_error(id);
          }
          break;
        }
        case Token::Run:
        {
          Expr* code = read_code();
          check_code(code);
          cout << "[Running-sc ";
          code->print(cout);
          Expr* tmp = run_code(code);
          cout << "] = \n";
          if (tmp)
          {
            tmp->print(cout);
            tmp->dec();
          }
          else
            cout << "fail";
          cout << "\n";
          code->dec();
          break;
        }
        case Token::Program:
        {
          string progstr(prefix_id());
          SymSExpr* prog = new SymSExpr(progstr);
          if (progs.find(progstr) != progs.end())
            report_error(string("Redeclaring program ") + progstr
                         + string("."));
          progs[progstr] = prog;
          eat_token(Token::Open);
          Token::Token d;
          vector<Expr*> vars;
          vector<Expr*> tps;
          Expr* tmp;
          while ((d = next_token()) != Token::Close)
          {
            reinsert_token(d);
            eat_token(Token::Open);
            string varstr = prefix_id();
            if (symbols->get(varstr.c_str()).first != NULL)
            {
              report_error(string("A program variable is already declared")
                           + string(" (as a constant).\n1. The variable: ")
                           + varstr);
            }
            Expr* var = new SymSExpr(varstr);
            vars.push_back(var);
            statType->inc();
            int prev = open_parens;
            Expr* tp = check(true, NULL, &tmp, 0, true);
            Expr* kind = compute_kind(tp);
            if (kind != statType && !tp->isDatatype())
            {
              report_error(
                  string("A program argument's type is neither proper, nor a "
                         "datatype.")
                  + string("\n1. the type: ") + tp->toString()
                  + string("\n2. its kind: ")
                  + (kind == nullptr ? string("none") : kind->toString()));
            }
            eat_excess(prev);

            tps.push_back(tp);
            eat_token(Token::Close);

            symbols->insert(varstr.c_str(), pair<Expr*, Expr*>(var, tp));
          }

          if (!vars.size()) report_error("A program lacks input variables.");

          statType->inc();
          int prev = open_parens;
          // read the return type of the program
          Expr* progtpret = check(true, statType, &tmp, 0, true);
          eat_excess(prev);
          Expr* kind = compute_kind(progtpret);
          if (kind != statType && !progtpret->isDatatype())
          {
            report_error(
                string("A program's return type is neither proper, nor a "
                       "datatype.")
                + string("\n1. the type: ") + progtpret->toString()
                + string("\n2. its kind: ")
                + (kind == nullptr ? string("none") : kind->toString()));
          }

          Expr* progcode = read_code();

          // now, construct the type of the program
          Expr* progtp = progtpret;
          for (int i = vars.size() - 1, iend = 0; i >= iend; i--)
          {
            vars[i]->inc();  // used below for the program code (progcode)
            progtp = new CExpr(PI, vars[i], tps[i], progtp);
            progtp->calc_free_in();
          }

          // just put the type here for type checking.  Make sure progtp is kid
          // 0.
          prog->val = new CExpr(PROG, progtp);

          Expr* rettp = check_code(progcode);

          // check that the body matches the return type
          if (!rettp->defeq(progtpret))
          {
            report_error(
                string("Return type for a program does not match")
                + string(" its body.\n1. the type: ") + rettp->toString()
                + string("\n2. the expected type: ") + progtpret->toString());
          }

          progcode =
              new CExpr(PROG, progtp, new CExpr(PROGVARS, vars), progcode);
          // if compiling side condition code, give this code to the side
          // condition code writer
          if (a.compile_scc)
          {
            if (scw)
            {
              scw->add_scc(progstr, (CExpr*)progcode);
            }
          }

          // remove the variables from the symbol table.
          for (int i = 0, iend = vars.size(); i < iend; i++)
          {
            string& s = ((SymSExpr*)vars[i])->s;

            symbols->insert(s.c_str(), pair<Expr*, Expr*>(NULL, NULL));
          }

          progtp->inc();
          prog->val->dec();

          prog->val = progcode;

          break;
        }
        default:
        {
          stringstream msg;
          msg << "Acceptable top-level commands are:";
          for (const auto& t : {Token::Declare,
                                Token::Define,
                                Token::Opaque,
                                Token::Run,
                                Token::Check,
                                Token::Program,
                                Token::DeclareRule,
                                Token::DeclareType})
          {
            msg << "\n\t" << t;
          }
          unexpected_token_error(c, msg.str());
          break;
        }
      }
    }
    else if (c == Token::Close)
    {
      // Okay
    }
    else
    {
      unexpected_token_error(c, "Top-level commands must start with parentheses");
    }
  }
}

class Deref : public Trie<pair<Expr *, Expr *> >::Cleaner
{
 public:
  ~Deref() {}
  void clean(pair<Expr *, Expr *> p)
  {
    Expr *tmp = p.first;
    if (tmp)
    {
#ifdef DEBUG
      cout << "Cleaning up ";
      tmp->debug();
#endif
      tmp->dec();
    }
    tmp = p.second;
    if (tmp)
    {
#ifdef DEBUG
      cout << " : ";
      tmp->debug();
#endif
      tmp->dec();
    }
#ifdef DEBUG
    cout << "\n";
#endif
  }
};

template <>
Trie<pair<Expr *, Expr *> >::Cleaner *Trie<pair<Expr *, Expr *> >::cleaner =
    new Deref;

void cleanup()
{
  symmap::iterator i, iend;

  // clean up programs

  symmap2::iterator j, jend;
  for (j = progs.begin(), jend = progs.end(); j != jend; j++)
  {
    SymExpr *p = j->second;
    if (p)
    {
      Expr *progcode = p->val;
      p->val = NULL;
      progcode->dec();
      p->dec();
    }
  }
}

Expr* compute_kind(Expr* e)
{
  e = e->followDefs();
  switch (e->getclass())
  {
    case CEXPR:
    {
      CExpr* ce = static_cast<CExpr*>(e);
      switch (e->getop())
      {
        case APP:
        {
          Expr* head = compute_kind(ce->kids[0]);
          std::vector<std::pair<SymExpr*, Expr*>> prev_pi_vars_and_values;
          size_t next_kid_i = 1;
          for (; head->getop() == PI && ce->kids[next_kid_i] != nullptr;
               ++next_kid_i)
          {
            Expr* actual_arg = ce->kids[next_kid_i]->followDefs();
            CExpr* pi = static_cast<CExpr*>(head);
            Expr* range = pi->kids[2];
            SymExpr* pi_var = static_cast<SymExpr*>(pi->kids[0]);
            prev_pi_vars_and_values.push_back(
                std::make_pair(pi_var, pi_var->val));
            pi_var->val = actual_arg;
            head = compute_kind(range);
          }
          if (ce->kids[next_kid_i] != nullptr && head->getop() != PI)
          {
            // We're not holding a function anymore, but there are more
            // arguments!
            report_error(string("When reducing ") + e->toString()
                         + string(" I got the expression ") + head->toString()
                         + string(" applied to some arguments, but that "
                                  "expression is not a function"));
          }
          for (const auto& p : prev_pi_vars_and_values)
          {
            p.first->val = p.second;
          }
          return head;
        }
        case MPQ:
        case MPZ:
        {
          return statType;
        }
        default:
        {
          return e;
        }
      }
    }
    case SYMS_EXPR:
    case SYM_EXPR:
    {
      Expr* reference = e->followDefs();
      if (reference->getclass() == SYMS_EXPR)
      {
        auto ref_value_and_type =
            symbols->get(static_cast<SymSExpr*>(reference)->s.c_str());
        if (ref_value_and_type.second != nullptr)
        {
          reference = ref_value_and_type.second;
        }
      }
      return reference;
    }
    case RAT_EXPR:
    case INT_EXPR:
    {
      return e;
    }
    case HOLE_EXPR:
    {
      report_error("Hole expression have no kind.");
      return nullptr;  // unreachable
    }
    default:
    {
      report_error("Unknown expression class in compute_kind()");
      return nullptr;  // unreachable
    }
  }
}

void init()
{
  symbols->insert("type", pair<Expr*, Expr*>(statType, statKind));
  statType->inc();
  symbols->insert("mpz", pair<Expr*, Expr*>(statMpz, statType));
  symbols->insert("mpq", pair<Expr*, Expr*>(statMpq, statType));
}
