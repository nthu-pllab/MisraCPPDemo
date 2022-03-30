import json
import os
import pickle
import pprint
import re
import shutil
import subprocess

from collections import deque
from collections import OrderedDict

from cmdfilters import ArgInfo
from cmdfilters import CmdPattern
from cmdfilters import CmdFilterManager
from libmisrascan import listof
from libmisrascan import MisraNamedTuple


class SymbolPattern:
    BLANKS = re.compile(r'\s*')
    COMMA = re.compile(r',')
    EQSIGN = re.compile(r'=')
    INT = re.compile(r'[+-]?\d+')
    LPARENTHESE = re.compile(r'\(')
    LSQBRACKET = re.compile(r'\[')
    RPARENTHESE = re.compile(r'\)')
    RSQBRACKET = re.compile(r'\]')
    RESUMED = re.compile(r'resumed>')
    STRING = re.compile(r'"([^\\"]|\\.)*"')


class CmdRecord(MisraNamedTuple(
    'CmdRecord',
    field_names=['argv',
                 'pwd',
                 'arginfo'],
    default_type={
        'argv': listof(str),
        'arginfo': ArgInfo
    })):

    @property
    def isCC(self):
        cmd = self.argv[0]
        return CmdPattern.GCC.match(cmd) or \
            CmdPattern.CC.match(cmd) or \
            CmdPattern.CLANG.match(cmd) or \
            CmdPattern.LLVMGCC.match(cmd)

    @property
    def isCXX(self):
        cmd = self.argv[0]
        return CmdPattern.GPLUSPLUS.match(cmd) or \
            CmdPattern.CPLUSPLUS.match(cmd) or \
            CmdPattern.CLANGPLUSPLUS.match(cmd) or \
            CmdPattern.LLVMGPLUSPLUS.match(cmd)

    def getFullPath(self, path):
        full_path = os.path.join(self.pwd, path)
        if os.path.exists(full_path):
            full_path = os.path.realpath(full_path)
        return full_path

    def getFullPaths(self, paths):
        return [self.getFullPath(p) for p in paths]


class StraceLogLexer:

    def consume(self, pattern, line):
        match = pattern.match(line)
        if match:
            _, end = match.span()
            return line[end:]
        else:
            return line

    def lex(self, pattern, line):
        line = self.consume(SymbolPattern.BLANKS, line)
        match = pattern.match(line)
        if match:
            start, end = match.span()
            return line[start:end], line[end:]
        else:
            return '', line

    def lexRawUntil(self, pattern, line, drop=False):
        line = self.consume(SymbolPattern.BLANKS, line)
        match = pattern.search(line)
        if match:
            start, end = match.span()
            if drop:
                return line[:start], line[end:]
            else:
                return line[:end], line[end:]
        else:
            return '', line

    def lexInt(self, line):
        num, line = self.lex(SymbolPattern.INT, line)
        return int(num), line

    def lexString(self, line):
        content, line = self.lex(SymbolPattern.STRING, line)
        return eval(content), line


class StraceLogParsingError(Exception):
    pass


class StraceLogParser:

    def __init__(self, log_path):
        self.log_path = log_path
        self.lexer = StraceLogLexer()

    def fetchLines(self):
        with open(self.log_path) as fp:
            for line in fp:
                yield line

    def parseStringArray(self, line):
        ret = []

        _, line = self.lexer.lex(SymbolPattern.LSQBRACKET, line)

        element, line = self.lexer.lexString(line)
        if element:
            ret.append(element)

        sep, line = self.lexer.lex(SymbolPattern.COMMA, line)
        while sep:
            element, line = self.lexer.lexString(line)
            sep, line = self.lexer.lex(SymbolPattern.COMMA, line)
            ret.append(element)

        _, line = self.lexer.lex(SymbolPattern.RSQBRACKET, line)

        return ret, line

    def parseExecveArguments(self, line):
        filename, line = self.lexer.lexString(line)
        _, line = self.lexer.lex(SymbolPattern.COMMA, line)
        argv, line = self.parseStringArray(line)
        _, line = self.lexer.lex(SymbolPattern.COMMA, line)
        env, line = self.parseStringArray(line)
        return (filename, argv, env), line

    def parseLine(self, line, pending_syscalls, succeeded_syscalls):
        resumed = False

        pid, line = self.lexer.lexInt(line)
        line = self.lexer.consume(SymbolPattern.BLANKS, line)

        if not line:
            raise StraceLogParsingError

        next_char = line[0]
        # handling '+++ exited with $INT +++'
        if next_char == '+':
            return 1
        # handling '<... execve resumed> )'
        elif next_char == '<':
            resumed, line = self.lexer.lexRawUntil(SymbolPattern.RESUMED, line)
            if not resumed:
                raise StraceLogParsingError
        # handling system call
        else:
            syscall, line = self.lexer.lexRawUntil(
                SymbolPattern.LPARENTHESE, line, drop=True)
            if syscall != 'execve':
                raise StraceLogParsingError

        if not resumed:
            arginfo, line = self.parseExecveArguments(line)
            sep, line = self.lexer.lex(SymbolPattern.RPARENTHESE, line)
            # save information about pending syscall
            if not sep:
                pending_syscalls[pid] = arginfo
                return 1

        _, line = self.lexer.lexRawUntil(SymbolPattern.EQSIGN, line)
        status, _ = self.lexer.lexInt(line)

        if resumed:
            arginfo = pending_syscalls.pop(pid)

        if status == 0:
            succeeded_syscalls.append(arginfo)

        return status

    def extractPwdFromEnvVars(self, env_vars):
        for var in env_vars:
            key, val = var.split('=', maxsplit=1)
            if key == 'PWD':
                return val
        return None


class ResourceVertex:

    def __init__(self, path):
        self.path = path
        self.parents = OrderedDict()
        self.children = OrderedDict()
        self.indexfile_resources = []
        self.indexfile_targets = []
        self.indexfile_path = ""

    @property
    def indexfilename(self):
        return self.path + '.index'

    def __repr__(self):
        return "%s %s" % (self.path, self.indexfile_targets)
        # return "%s %s" % (self.path, [v.path for v in self.parents])

    def insertChild(self, vertex):
        # self loop is not allowed
        if id(vertex) == id(self):
            return

        # vertex is already in chilren set
        if vertex in self.children:
            return

        self.children.update({vertex: None})
        vertex.parents.update({self: None})


class ResourceVertexManager:

    def __init__(self, project_root):
        self.project_root = project_root
        self.file_vertex_mapping = {}

    def getVertexByRelPath(self, path, auto_add_vertex=True):
        vertex = self.file_vertex_mapping.get(path)
        if vertex is None and auto_add_vertex:
            vertex = ResourceVertex(path)
            self.file_vertex_mapping[path] = vertex
        return vertex

    def getVertexByAbsPath(self, path):
        rel_path = os.path.relpath(path, start=self.project_root)
        return self.getVertexByRelPath(rel_path)

    def getVertices(self):
        return list(self.file_vertex_mapping.values())

    def removeVertexByRelPath(self, path):
        target = self.file_vertex_mapping.get(path)
        if target:
            for v in target.parents:
                del v.children[target]
            for v in target.children:
                del v.parents[target]
            del self.file_vertex_mapping[path]

    def removeVertexByAbsPath(self, path):
        rel_path = os.path.relpath(path, start=self.project_root)
        self.removeVertexByRelPath(rel_path)


class ResourceGraphContainsCycleException(Exception):
    pass


class ResourceGraph:

    def __init__(self, vertex_manager):
        self.vertex_manager = vertex_manager

    def getVertices(self):
        return self.vertex_manager.getVertices()

    def getTopologicalSortedVertices(self, transpose=False):
        vertex_queue = deque()
        vertices = self.getVertices()
        for v in vertices:
            tail_set = v.children if transpose else v.parents
            v.degree = len(tail_set)
            if v.degree == 0:
                vertex_queue.append(v)

        sorted_vertices = []
        while len(vertex_queue) > 0:
            v = vertex_queue.popleft()
            sorted_vertices.append(v)

            head_set = v.parents if transpose else v.children
            for head in head_set:
                head.degree -= 1
                if head.degree == 0:
                    vertex_queue.append(head)

        for v in vertices:
            del v.degree

        if len(sorted_vertices) != len(vertices):
            raise ResourceGraphContainsCycleException

        return sorted_vertices

    def getCytoscapeData(self):
        vertices = self.getVertices()
        nodes = []
        edges = []
        for v in vertices:
            nodes.append({'data': {'id': v.path}})
            for child in v.children:
                edges.append({'data': {'source': v.path, 'target': child.path}})
        return (nodes, edges)

    def save(self, file_path):
        with open(file_path, 'wb') as fp:
            pickle.dump(self, fp)

    @staticmethod
    def load(file_path):
        with open(file_path, 'rb') as fp:
            ret = pickle.load(fp)
        return ret


class CmdAnalyzer:

    def trace(self, args):

        def getLogDir(output_dir):
            log_dir = os.path.join(output_dir, 'logs')
            if not os.path.isdir(log_dir):
                os.makedirs(log_dir, exist_ok=True)
            return log_dir

        strace_output = os.path.join(getLogDir(args.output), 'strace.log')
        strace_cmd = ['strace', '-e', 'trace=execve', '-e', 'signal=none',
                      '-s', '65536', '-o', strace_output, '-v', '-f']
        strace_cmd.extend(args.build)
        # print(' '.join(strace_cmd))
        subprocess.call(strace_cmd)
        return strace_output

    def analyze(self, log_path):
        pending_syscalls = {}
        succeeded_syscalls = deque()
        parser = StraceLogParser(log_path)
        cmd_filter_manager = CmdFilterManager()

        for line in parser.fetchLines():
            status = parser.parseLine(
                line, pending_syscalls, succeeded_syscalls)
            if status != 0:
                continue

            cmd, argv, env = succeeded_syscalls.pop()
            for filter_obj in cmd_filter_manager.filters():
                if filter_obj.match(cmd):
                    pwd = parser.extractPwdFromEnvVars(env)
                    arginfo = filter_obj.inspectArgs(argv)
                    succeeded_syscalls.append(CmdRecord(argv=argv,
                                                        pwd=pwd,
                                                        arginfo=arginfo))
                    break

        new_log_path = os.path.join(os.path.dirname(log_path), 'build_cmd.json')
        with open(new_log_path, 'w') as fp:
            content = '[%s]' % ','.join([r.tojson() for r in succeeded_syscalls])
            fp.write(content)

        # os.unlink(log_path)

        return succeeded_syscalls

    def readLog(self, log_path):
        with open(log_path) as fp:
            cmd_records = json.load(fp)
        return [CmdRecord(**rec) for rec in cmd_records]

    def buildResourceGraph(self, project_root, cmd_records):
        vertex_manager = ResourceVertexManager(project_root)

        for cmd_record in cmd_records:
            if not cmd_record.arginfo.inputs or not cmd_record.arginfo.outputs:
                continue

            inputs = cmd_record.getFullPaths(cmd_record.arginfo.inputs)
            outputs = cmd_record.getFullPaths(cmd_record.arginfo.outputs)

            # N-1 mapping
            if len(outputs) == 1:
                output_vertex = vertex_manager.getVertexByAbsPath(outputs[0])
                for p in inputs:
                    input_vertex = vertex_manager.getVertexByAbsPath(p)
                    input_vertex.insertChild(output_vertex)
            # 1-1 mapping
            elif len(outputs) == len(inputs):
                for ip, op in zip(inputs, outputs):
                    input_vertex = vertex_manager.getVertexByAbsPath(ip)
                    output_vertex = vertex_manager.getVertexByAbsPath(op)
                    input_vertex.insertChild(output_vertex)
            else:
                print("[warning] inputs and outputs cannot match")
        return ResourceGraph(vertex_manager)


class VirtualLinker:

    def __init__(self, project_root, report_dir):
        self.project_root = project_root
        self.report_dir = report_dir
        self.ast_dir = os.path.join(self.report_dir, 'ast')

    def updateIndexfileOnResourceVertex(self, vertex):
        # already merged
        if vertex.indexfile_path:
            return

        resource_count = len(vertex.indexfile_resources)
        # nothing to merge
        if resource_count == 0:
            return

        # no need to merge
        if resource_count == 1:
            vertex.indexfile_path = vertex.indexfile_resources[0]
            return

        destination = os.path.join(self.ast_dir, vertex.indexfilename)

        dest_dir = os.path.dirname(destination)
        if not os.path.isdir(dest_dir):
            os.makedirs(dest_dir, exist_ok=True)

        # print(">>> generating %s based on" % os.path.relpath(destination, start=self.ast_dir))
        # pprint.pprint([os.path.relpath(p, start=self.ast_dir) for p in vertex.indexfile_resources])
        # print("===")
        with open(destination, 'wb') as dest_fd:
            for f in vertex.indexfile_resources:
                try:
                    with open(f, 'rb') as src_fd:
                        shutil.copyfileobj(src_fd, dest_fd)
                except:
                    raise

        vertex.indexfile_path = destination
        return vertex

    def updateIndexfileTargetsOnResourceGraph(self, resource_graph):
        vertices = resource_graph.getTopologicalSortedVertices(transpose=True)

        for v in vertices:
            if v.children:
                continue
            if os.path.exists(v.indexfile_path):
                v.indexfile_targets.append(v.indexfile_path)

        for v in vertices:
            for p in v.parents:
                p.indexfile_targets.extend(v.indexfile_targets)

        return resource_graph

    def updateIndexfilesOnResourceGraph(self, resource_graph):
        vertices = resource_graph.getTopologicalSortedVertices()

        # initialize .index files
        for v in vertices:
            if v.parents:
                continue
            indexfile_path = os.path.join(self.ast_dir, v.indexfilename)
            if os.path.exists(indexfile_path):
                v.indexfile_resources.append(indexfile_path)

        # merge .index files
        for v in vertices:
            self.updateIndexfileOnResourceVertex(v)

            if v.indexfile_path:
                if os.path.exists(v.indexfile_path):
                    for child in v.children:
                        child.indexfile_resources.append(v.indexfile_path)

        # remove redundant vertices
        for v in vertices:
            if not v.indexfile_path:
                resource_graph.vertex_manager.removeVertexByRelPath(v.path)

        return self.updateIndexfileTargetsOnResourceGraph(resource_graph)

    def save(self, resource_graph):
        obj_path = os.path.join(self.ast_dir, 'resource_graph.obj')
        resource_graph.save(obj_path)
        return obj_path
