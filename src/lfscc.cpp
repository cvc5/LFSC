#include <fstream>
#include <iostream>

#include "lfscc.h"
#include "check.h"

void lfscc_init(void) { init(); }

void lfscc_check_file(const char* filename,
                      bool show_runs,
                      bool no_tail_calls,
                      bool compile_scc,
                      bool compile_scc_debug,
                      bool run_scc,
                      bool use_nested_app,
                      sccwriter* scw)
{
  args a;
  a.show_runs = show_runs;
  a.no_tail_calls = no_tail_calls;
  a.compile_scc = compile_scc;
  a.compile_scc_debug = compile_scc_debug;
  a.run_scc = run_scc;
  a.use_nested_app = use_nested_app;
  check_file(filename, a, scw);
}

void lfscc_check_file(std::istream& in,
                      bool show_runs,
                      bool no_tail_calls,
                      bool compile_scc,
                      bool compile_scc_debug,
                      bool run_scc,
                      bool use_nested_app,
                      sccwriter* scw,)
{
  args a;
  a.show_runs = show_runs;
  a.no_tail_calls = no_tail_calls;
  a.compile_scc = compile_scc;
  a.compile_scc_debug = compile_scc_debug;
  a.run_scc = run_scc;
  a.use_nested_app = use_nested_app;
  std::string filename("<stream>");
  check_file(in, filename, a, scw);
}

void lfscc_cleanup(void) { cleanup(); }
