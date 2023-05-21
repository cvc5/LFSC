#ifndef SC2_CODE_H
#define SC2_CODE_H

#include "expr.h"

Expr *read_code();
Expr *check_code(Expr *);  // compute the type for the given code

/**
 * Return the result of running the code e.
 * @param e The code to run
 */
Expr* run_code(Expr* e);
/**
 * Mark that program s is a function (its results are cached).
 */
void markProgramAsFunction(Expr* s);

extern bool dbg_prog;
extern bool run_scc;

#endif
