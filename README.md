# LFSC Checker
## A High-Performance LFSC Proof Checker

Authors: Andy Reynolds and Aaron Stump

## Building LFSC checker

You need cmake (>= version 2.8.9) to build LFSC Checker.

To build a regular build, issue:

```bash
cd /path/to/lfsc_checker
mkdir build
cd build
cmake ..
make
```

Alternatively you can configure a regular build with

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
```


To build a regular build and install it into /path/to/install, issue:

```bash
cd /path/to/lfsc_checker
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX:PATH=/path/to/install ..
make install
```

To build a debug build, issue:

```bash
cd /path/to/lfsc_checker
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

## Using LFSC checker

```
lfsc [sig_1 .... sig_n] [proof] [opts_1...opts_n]
```

with

```
sig_1 .... sig_n            signature files (.plf)
proof                           the proof to check (.plf)
opts_1...opts_n          options
```

### Options:

```

--compile-scc :
              Write out all side conditions contained in signatures specified on the command line to files scccode.h, scccode.cpp
                (see below for example)

--run-scc :
              Run proof checking with compiled side condition code (see below).

--compile-scc-debug :
              Write side condition code to scccode.h, scccode.cpp that contains print statements
              (for debugging running of side condition code).
```

### Signature Files

You can find example signature files here:  
http://clc.cs.uiowa.edu/lfsc/

### Side Condition Code Compilation:

LFSC may be used with side condition code compilation.  This will take
all side conditions ("program" constructs) in the user signature and
produce equivalent C++ code in the output files scccode.h,
scccode.cpp.

An example for QF_IDL running with side condition code compilation:

1. In the src directory, run LFSC with the command line parameters:
```
lfscc /path/to/sat.plf /path/to/smt.plf /path/to/cnf_conv.plf /path/to/th_base.plf /path/to/th_idl.plf --compile-scc
```
This will produce scccode.h and scccode.cpp in the working directory
where lfscc was run (in our case, src).

2. Recompile the code base for lfscc.  This will produce a copy of the
LFSC checker executable that is capable of calling side conditions directly as
compiled C++ code.

3. To check a proof.plf with side condition code compilation, run
LFSC with the following command line parameters:

```
lfscc /path/to/sat.plf /path/to/smt.plf /path/to/cnf_conv.plf /path/to/th_base.plf /path/to/th_idl.plf --run-scc   proof.plf
```

Note that this proof must be compatible with the proof checking
signature.  The proof generator is responsible for producing a proof
in the proper format that can be checked by the proof signature
specified when running LFSC.

For example, in the case of CLSAT in the QF_IDL logic, older proofs
(proofs produced before Feb 2009) may be incompatible with the newest
version of the resolution checking signature (sat.plf).  The newest
version of CLSAT -- which can be checked out from the Iowa repository
with

```
svn co https://svn.divms.uiowa.edu/repos/clc/clsat/trunk clsat
```

should produce proofs compatible with the current version of sat.plf.
