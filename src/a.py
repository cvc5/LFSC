import subprocess as sub

sub.run('grep check check.cpp | wc -l', shell=True, check=True)
