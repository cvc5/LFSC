#ifndef SC2_CHECK_H
#define SC2_CHECK_H

#include "expr.h"
#include "token.h"
#include "trie.h"

#ifdef _MSC_VER
#include <stdio.h>
#include <hash_map>
#else
#include <ext/hash_map>
#endif

#include <cstddef>
#include <iosfwd>
#include <map>
#include <stack>
#include <string>
#include "lexer.h"

// see the help message in main.cpp for explanation
typedef struct args
{
  std::vector<std::string> files;
  bool show_runs;
  bool no_tail_calls;
  bool compile_scc;
  bool compile_scc_debug;
  bool run_scc;
  bool use_nested_app;
  bool compile_lib;
} args;

extern int check_time;

class sccwriter;
class libwriter;

void init();

void check_file(const char *_filename,
                args a,
                sccwriter *scw = NULL,
                libwriter *lw = NULL);

void check_file(std::istream& in,
                const std::string& filename,
                args a,
                sccwriter* scw = NULL,
                libwriter* lw = NULL);

struct DeclList
{
  // The declarations: (symbol, type) pairs.
  std::vector<std::pair<Expr*, Expr*>> decls;
  // Old bindings to restore:
  // (name, old value, old type).
  // Necessary because, for each (symbol, type) pair in the `decls`, we bind
  // `symbol` to `type` in the enviroment, possibly overwriting the prior
  // binding for `symbol`. This member contains the information needed to
  // restore the binding.
  std::vector<std::tuple<std::string, Expr*, Expr*>> old_bindings;
};

// Checks for a declaration list item.
// Such items have two forms:
//   (:  NAME TYPE) -> (VarExpr(NAME), Expr(TYPE))
//   TYPE           -> (nullpty      , Expr(TYPE))
//
// Returns a pair:
//   the name of the declared symbol ("" if no name)
//   the type of the declaration     (TYPE in the above)
std::pair<std::string, Expr*> check_decl_list_item();

// Checks a list of declarations
// e.g.
//   ((: a bool) bool (: p (holds a)))
// Returns a list of (symbol, type) bindings ("_" is the symbol for
// declarations which are anonymous), binding those symbols in the enviroment
// as it does.  Also returns a list of old bindings.
//
// See DeclList structure documentation for the details of the return value.
//
// If create is not set, then we return an empty list of declarations.
// The old bindings are still returned.
DeclList check_decl_list(bool create);

// Builds an validates a nested PI expression.
//
// E.g., from (a bool) (b bool) ..., it builds (! a bool (! b bool ...))
//
// Parameters:
// * `args`: a (symbol, type) list. Each member corresponds to one PI in the
// nest.
// * `ret`: the body of the innermost PI
// * `ret_kind`: the type of the innermost PI
// * `create`: whether to acutally build the body of the constructed PI
//             (the type of the PI is always constructed)
// Returns:
// * The body of the constructed PI
// * The type of the constructed PI (which is just the ret_kind, it turns out)
//
// Note:
// Checks that the PI's return kind is TYPE or KIND. Otherwise, throws an error.
std::pair<Expr*, Expr*> build_validate_pi(
    std::vector<std::pair<Expr*, Expr*>>&& args,
    Expr* ret,
    Expr* ret_kind,
    bool create);

// Builds and validates a nested macro (LAM) expression.
// Very similar to the above, except:
// * Always constructs the body of the nested LAM.
// * Checks that each layer of the type of the nested LAM (which is a nested
//   PI) is not dependent.
std::pair<Expr*, Expr*> build_macro(std::vector<std::pair<Expr*, Expr*>>&& args,
                                    Expr* ret,
                                    Expr* ret_ty,
                                    bool create);

void cleanup();


#ifdef _MSC_VER
typedef std::hash_map<std::string, Expr *> symmap;
typedef std::hash_map<std::string, SymExpr *> symmap2;
#else
typedef __gnu_cxx::hash_map<std::string, Expr *> symmap;
typedef __gnu_cxx::hash_map<std::string, SymExpr *> symmap2;
#endif
extern symmap2 progs;
extern std::vector<Expr *> ascHoles;

extern Trie<std::pair<Expr *, Expr *> > *symbols;

extern std::map<SymExpr *, int> mark_map;

extern std::vector<std::pair<std::string, std::pair<Expr *, Expr *> > >
    local_sym_names;

#ifndef _MSC_VER
namespace __gnu_cxx {
template <>
struct hash<std::string>
{
  size_t operator()(const std::string &x) const
  {
    return hash<const char *>()(x.c_str());
  }
};
}  // namespace __gnu_cxx
#endif

extern Expr *statMpz;
extern Expr *statMpq;
extern Expr *statType;


/**
 * Given a type, `e`, computes its kind.
 *
 * In particular, this will be `statType` if the kind is that of proper types.
 *
 * Since our system doen't draw a clear distinction between kinding and typing,
 * this function doesn't either. It could also be regarded as a function that
 * computes the type of a term. While values technically have no kind, this
 * function would return their type instead of an error.
 *
 * It is different from "check" in that it computes the kind/type of in-memory
 * terms, not serialized ones.
 *
 * Given these declarations:
 *
 *     (declare bool type)
 *     (declare sort type)
 *     (declare Real sort)
 *     (declare term (! s sort type))
 *
 * Examples of proper types:
 *
 *     bool
 *     sort
 *     (term Real)
 *
 * Example of non-proper types:
 *
 *   * term (it has kind `(! s sort type)`)
 *   * Real (it is not a type, and does not have a kind)
 *          (this function would return `sort`)
 *
 * Bibliography:
 *   * _Advanced Topics in Types and Programming Languages_
 */
Expr* compute_kind(Expr* e);

#endif
