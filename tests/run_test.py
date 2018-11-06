#!/usr/bin/env python3

import re
import os.path
import sys
import subprocess

class DepGraph(object):
    '''
    '''
    def __init__(self, root_path):
        self._r = root_path
        self._neighbors = {}
        self._ordered_paths = []
        self._ordered_set = set()

        self._visit(root_path)

    def _visit(self, p):
        if p in self._ordered_set:
            return
        elif p in self._neighbors:
            print("Cycle in dependencies")
            sys.exit(1)
        else:
            node = TestFile(p)
            self._neighbors[p] = node.dep_paths
            for n in node.dep_paths:
                if n not in self._neighbors:
                    self._visit(n)
            self._ordered_paths.append(p)
            self._ordered_set.add(p)

    def getPaths(self):
        return self._ordered_paths

class TestFile(object):
    def __init__(self, path):
        self.path = path
        self.get_config_map()
        self.deps = self.config_map['deps'].split() if ('deps' in self.config_map) else []
        self.dir = os.path.dirname(self.path)
        self.dep_paths = [os.path.join(self.dir, d) for d in self.deps]

    def get_comment_lines(self):
        with open(self.path, 'r') as test_file:
            return (line for line in test_file.readlines() if \
                    re.match(r'^\s*;.*$', line) is not None)

    def get_config_map(self):
        m = {}
        for l in self.get_comment_lines():
            match = re.match(r'^.*;\s*(\w+(?:\s+\w+)*)\s*:(.*)$', l)
            if match is not None:
                m[match.group(1).replace(' ','').lower()] = match.group(2)
        self.config_map = m


class TestConfiguration(object):
    def __init__(self):
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

def main():
    configuration = TestConfiguration()
    cmd = [configuration.lfscc] + configuration.dep_graph.getPaths()
    print('Command: ', cmd)
    result = subprocess.run(cmd, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
    code = result.returncode
    if (code != 0):
        if result.stdout:
            print(result.stdout.decode('utf-8'))
        if result.stderr:
            print(result.stderr.decode('utf-8'), file=sys.stderr)
    return code

if __name__ == '__main__':
    sys.exit(main())
