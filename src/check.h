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
