#!/usr/bin/env python

import re
import os.path
import sys
import subprocess

class TestConfiguration(object):
    ''' Represents a test to run.  '''

    def __init__(self):
        ''' Initialized from program arguments.
        Exists with code 2 and prints usage message on invalid arguments.
        '''
        if len(sys.argv) < 3 or \
                (not os.path.isfile(sys.argv[1])) or \
                (not os.path.isfile(sys.argv[2])):
            print(sys.argv)
            print('''
Usage: {} <lfscc> <plf>

Return:
    Returns the exit code of LFSCC.
    Echos LFSCC's stdout and stderr if the exit code is non-zero

Dependencies:
    The PLF file may contain lines like
    ; Deps: [<file to include before this one> ...]

    Dependencies are recursively resolved''')
            sys.exit(2)
        self.lfscc = sys.argv[1]
        self.path = sys.argv[2]
        self.dep_graph = DepGraph(self.path)

class DepGraph(object):
    ''' Represents a dependency graph of LFSC input files '''
    def __init__(self, root_path):
        ''' Creates a dependency graph rooted a `root_path`.
        Computes a root-last topological sort.
        Exits with exitcode 1 on cyclic dependencies'''

        # Root of the graph
        self._r = root_path

        # Nodes (paths) that have been visited
        self._visited = set()

        # Nodes (paths) that have been ordered
        self._ordered_set = set()

        # The order of nodes (paths). Root is last.
        self._ordered_paths = []

        # Start DFS topo-order
        self._visit(root_path)

    def _visit(self, p):
        ''' Puts the descendents of p in the order, parent-last '''
        node = TestFile(p)
        self._visited.add(p)
        for n in node.dep_paths:
            if n not in self._ordered_set:
                if n in self._visited:
                    # Our child is is an ancestor our ours!?
                    print("{} and {} are in a dependency cycle".format(p, n))
                    sys.exit(1)
                else:
                    self._visit(n)
        self._ordered_paths.append(p)
        self._ordered_set.add(p)

    def getPathsInOrder(self):
        return self._ordered_paths

class TestFile(object):
    ''' Represents a testable input file to LFSC '''
    def __init__(self, path):
        ''' Read the file at `path` and determine its immediate dependencies'''
        self.path = path
        self._get_config_map()
        self.deps = self.config_map['deps'].split() if ('deps' in self.config_map) else []
        self.dir = os.path.dirname(self.path)
        self.dep_paths = [os.path.join(self.dir, d) for d in self.deps]

    def _get_comment_lines(self):
        ''' Return an iterator over comment lines, ;'s included '''
        with open(self.path, 'r') as test_file:
            return (line for line in test_file.readlines() if \
                    re.match(r'^\s*;.*$', line) is not None)

    def _get_config_map(self):
        ''' Populate self.config_map.
        Config variables are set using the syntax
        ; Var Name Spaces Okay: space separated values'''
        m = {}
        for l in self._get_comment_lines():
            match = re.match(r'^.*;\s*(\w+(?:\s+\w+)*)\s*:(.*)$', l)
            if match is not None:
                m[match.group(1).replace(' ','').lower()] = match.group(2)
        self.config_map = m

def main():
    configuration = TestConfiguration()
    cmd = [configuration.lfscc] + configuration.dep_graph.getPathsInOrder()
    print('Command: ', cmd)
    result = subprocess.Popen(cmd, stderr=subprocess.STDOUT, stdout=subprocess.PIPE)
    (stdout, _) = result.communicate()
    if 0 != result.returncode:
        if stdout:
            print(stdout.decode())
    return result.returncode

if __name__ == '__main__':
    sys.exit(main())
