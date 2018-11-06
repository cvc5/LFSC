# Test Format

Test files should be valid LFSC input files.

They will pass if LFSCC returns 0 on them, and fail otherwise.

They may include comments of the form:
```
; Deps: file.plf file2.plf ...
```

which will cause the indicated files to be included before the file with the
comments. Dependencies are recursively resolved.
