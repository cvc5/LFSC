#ifndef LFSC_CHECKER_H
#define LFSC_CHECKER_H

#include <cstddef>
#include <vector>

class sccwriter;
class libwriter;

void lfscc_init();

void lfscc_check_file(const char* filename,
                      bool show_runs,
                      bool no_tail_calls,
                      bool compile_scc,
                      bool compile_scc_debug,
                      bool run_scc,
                      bool use_nested_app,
                      sccwriter* scw = nullptr);

void lfscc_check_file(std::istream& in,
                      bool show_runs,
                      bool no_tail_calls,
                      bool compile_scc,
                      bool compile_scc_debug,
                      bool run_scc,
                      bool use_nested_app,
                      sccwriter* scw = nullptr);

void lfscc_cleanup();

#endif
