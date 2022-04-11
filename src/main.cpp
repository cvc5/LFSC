#include <signal.h>
#include <time.h>
#include <cstddef>
#include "check.h"
#include "expr.h"
#include "token.h"
#include "sccwriter.h"

using namespace std;

args a;

static void parse_args(int argc, char **argv, args &a)
{
  char *arg0 = *argv;

  /* skip 0'th argument */
  argv++;
  argc--;

  while (argc)
  {
    if ((strncmp("-h", *argv, 2) == 0) || (strncmp("--h", *argv, 3) == 0))
    {
      cout << "Usage: " << arg0 << " [options] infile1 ...infile_n\n";
      cout << "If no infiles are named on the command line, input is read\n"
           << "from stdin.  Specifying the infile \"stdin\" will also read\n"
           << "from stdin.  Options are:\n\n";
      cout << "--show-runs: print debugging information for runs of side "
              "condition code\n";
      cout << "--compile-scc: compile side condition code\n";
      cout << "--compile-scc-debug: compile debug versions of side condition "
              "code\n";
      cout << "--run-scc: use compiled side condition code\n";
      exit(0);
    }
    else if (strcmp("--show-runs", *argv) == 0)
    {
      argc--;
      argv++;
      a.show_runs = true;
    }
    else if (strcmp("--no-tail-calls", *argv) == 0)
    {
      // this is just for debugging.
      argc--;
      argv++;
      a.no_tail_calls = true;
    }
    else if (strcmp("--compile-scc", *argv) == 0)
    {
      argc--;
      argv++;
      a.compile_scc = true;
      a.compile_scc_debug = false;
    }
    else if (strcmp("--compile-scc-debug", *argv) == 0)
    {
      argc--;
      argv++;
      a.compile_scc = true;
      a.compile_scc_debug = true;
    }
    else if (strcmp("--run-scc", *argv) == 0)
    {
      argc--;
      argv++;
      a.run_scc = true;
    }
    else if (strcmp("--use-nested-app", *argv) == 0)
    {
      argc--;
      argv++;
      a.use_nested_app = true;  // not implemented yet
    }
    else
    {
      a.files.push_back(*argv);
      argc--;
      argv++;
    }
  }
}

void sighandler(int /* signum */)
{
  cerr << "\nInterrupted.  sc is aborting.\n";
  exit(1);
}

int main(int argc, char **argv)
{
  a.show_runs = false;
  a.no_tail_calls = false;
  a.compile_scc = false;
  a.run_scc = false;
  a.use_nested_app = false;

  signal(SIGINT, sighandler);

  parse_args(argc, argv, a);

  init();

  check_time = (int)clock();

  if (a.files.size())
  {
    sccwriter *scw = NULL;
    if (a.compile_scc)
    {
      scw = new sccwriter(a.compile_scc_debug ? opt_write_call_debug : 0);
    }
    /* process the files named */
    for (const std::string& file : a.files)
    {
      check_file(file.c_str(), a, scw);
    }
    if (scw)
    {
      scw->write_file();
      delete scw;
    }
  }
  else
    check_file("stdin", a);

    // std::cout << "time = " << (int)clock() - t << std::endl;
    // while(1){}

#ifdef DEBUG
  cout << "Clearing globals.\n";
  cout.flush();

  cleanup();
  a.files.clear();
#endif

  //std::cout << "Proof checked successfully!" << std::endl << std::endl;
  //std::cout << "time = " << (int)clock() - check_time << std::endl;
  //std::cout << "sym count = " << SymExpr::symmCount << std::endl;
  //std::cout << "marked count = " << Expr::markedCount << std::endl;
}
