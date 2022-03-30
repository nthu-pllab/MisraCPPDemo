import collections
import datetime
import glob
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
from abc import ABC
from abc import abstractmethod

from libmisrascan import exists
from libmisrascan import getClangCC1Args
from libmisrascan import runCommandAndGetOutput
from cmdanalyzer import ResourceGraph
from cmdfilters import CCCmdFilter
from reportutils import JSONReportHelper


class FakeCompilerBase(ABC):

    def __init__(self):
        self.IS_CXX_MODE = re.match(r'(.+)c\+\+(.*)',
                                    os.path.basename(sys.argv[0]))

        self.LANG_MAP = {
            '.c': 'c++' if self.IS_CXX_MODE else 'c',
            '.cp': 'c++',
            '.cpp': 'c++',
            '.cxx': 'c++',
            '.c++': 'c++',
            '.C++': 'c++',
            '.txx': 'c++',
            '.cc': 'c++',
            '.C': 'c++',
            '.CC': 'c++',
            '.ii': 'c++-cpp-output',
            '.i': 'c++-cpp-output' if self.IS_CXX_MODE else 'c-cpp-output',
            '.m': 'objective-c',
            '.mi': 'objective-c-cpp-output',
            '.mm': 'objective-c++',
            '.mii': 'objective-c++-cpp-output',
        }

        self.PREPROCESS_OUTPUT_FORMAT_MAP = {
            'objective-c++-cpp-output': '.mii',
            'objective-c++': '.mii',
            'objective-c-cpp-output': '.mi',
            'objective-c': '.mi',
            'c++-cpp-output': '.ii',
            'c++': '.ii'
        }

    def getRealCompiler(self):

        def getDefaultCompiler():
            if platform.system() == 'Darwin':
                return 'clang++' if self.IS_CXX_MODE else 'clang'
            else:
                return 'c++' if self.IS_CXX_MODE else 'cc'

        default_compiler = getDefaultCompiler()
        compiler = os.getenv('CCC_CXX' if self.IS_CXX_MODE else 'CCC_CC')
        if compiler is None or not exists(compiler):
            return default_compiler
        else:
            return compiler

    def getClang(self):
        default_compiler = 'clang++' if self.IS_CXX_MODE else 'clang'
        compiler = os.getenv('CLANG_CXX' if self.IS_CXX_MODE else 'CLANG')
        if compiler is None or not exists(compiler):
            return default_compiler
        else:
            return compiler

    def getSupportedFileFormat(self, filename):
        _, extension = os.path.splitext(os.path.basename(filename))
        return self.LANG_MAP.get(extension)

    def invokeRealCompiler(self):
        compiler = self.getRealCompiler()
        cmd = [compiler] + sys.argv[1:]
        return subprocess.call(cmd)

    @abstractmethod
    def retriveCustomizedParametersFromScanBuild(self, param):
        pass

    def retriveParametersFromScanBuild(self):
        analyzer_args = os.getenv('CCC_ANALYZER_ANALYSIS', '').split(' ')
        if analyzer_args:
            analyzer_args = [x for a in analyzer_args for x in ['-Xclang', a]]

        param = {
            'clang': self.getClang(),
            'analyzer_args': analyzer_args,
            'output_dir': os.getenv('CCC_ANALYZER_OUTPUT_DIR'),
            'output_format': os.getenv('CCC_ANALYZER_OUTPUT_FORMAT'),
            'output_failures': os.getenv('CCC_ANALYZER_OUTPUT_FAILURES'),
            'project_root': os.getenv('CCC_ANALYZER_PROJECT_ROOT')
        }
        self.retriveCustomizedParametersFromScanBuild(param)

        return param

    @abstractmethod
    def preprocess(self, param):
        pass

    @abstractmethod
    def registerCustomizedParameters(self,
                                     src,
                                     indexfile=None,
                                     arch_args=[],
                                     lang_args=[],
                                     c_args=[]):
        pass

    @abstractmethod
    def generateAnalyzerCommandByParameters(self, param):
        pass

    def reportFailure(self, param, errorInfo):

        def getPPFormat(language):
            return self.PREPROCESS_OUTPUT_FORMAT_MAP.get(language, '.i')

        def createFailureReportDir(dir):
            failures_dir = os.path.join(dir, 'failures')
            if not os.path.isdir(failures_dir):
                os.makedirs(failures_dir)
            return failures_dir

        def getOutputPath(dir, language, error_type):
            (handle, name) = tempfile.mkstemp(prefix='clang_%s_' % error_type,
                                              suffix=getPPFormat(language),
                                              dir=createFailureReportDir(dir))
            os.close(handle)
            os.chmod(name, 0o664)
            return name

        clang = param['clang']
        lang = param['lang']
        src = param['src']
        o_dir = param['output_dir']
        c_args = param['compiler_args']

        # output preprocessed source file
        error_type = 'crash' if errorInfo.returncode < 0 else 'other_error'
        failure_id = getOutputPath(o_dir, lang, error_type)
        o_args = ['-o', failure_id]
        cmd = getClangCC1Args(clang, ['-fsyntax-only', '-E']+c_args+o_args)
        subprocess.call(cmd, shell=True,
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        # output information about this failure
        with open(failure_id+'.info.json', 'w') as fp:
            json.dump({
                'cwd': os.getcwd(),
                'cmd': errorInfo.cmd,
                'type': error_type.title().replace('_', ' '),
                'src': os.path.realpath(src),
                'clang_preprocessed_file': os.path.basename(failure_id),
                'stderr': os.path.basename(failure_id)+'.stderr.txt',
                # 'os_info': ' '.join(os.uname()),
                'exit_status': errorInfo.returncode
            }, fp)

        # output captured stderr
        with open(failure_id+'.stderr.txt', 'w', encoding='utf-8') as fp:
            fp.write(errorInfo.output)

        # print("returncode: %d" % errorInfo.returncode)

    @abstractmethod
    def postprocess(self, param):
        pass

    def invokeAnalyzer(self, param):
        try:
            cmd = self.generateAnalyzerCommandByParameters(param)
        except subprocess.CalledProcessError:
            exit(0)

        with open('run.sh','a') as fp:
            ShellScript = "CmdTable[\"%s\"]=\"%s\"\n" % (param['src'],cmd.strip().replace("\"",""))
            fp.write(ShellScript)

        try:
            # TODO: do not use shell=True for security issues
            runCommandAndGetOutput(cmd, shell=True)
        except subprocess.CalledProcessError as errorInfo:
            if param.get('output_failures'):
                self.reportFailure(param, errorInfo)
        finally:
            self.postprocess(param)

    def analyze(self):
        # compile source files with real compiler
        # exit_status = self.invokeRealCompiler()
        # if exit_status != 0:
        #     exit(exit_status)

        # retreive useful information from command line arguments
        arginfo = CCCmdFilter().inspectArgs(sys.argv)

        # retrieve parameters set by scan-build
        parameters = self.retriveParametersFromScanBuild()

        self.preprocess(parameters)

        ctumode = parameters.get('ctumode')
        if ctumode:
            resource_graph = ResourceGraph.load(parameters.get('resource_graph_path'))

        # scan each source file in the command
        for src in arginfo.inputs:
            # check file language and existence
            lang = arginfo.lang if arginfo.lang else self.getSupportedFileFormat(src)
            if not lang or not os.path.exists(src):
                continue

            lang_args = ['-x', lang]
            parameters.update({'lang': lang})

            if ctumode:
                vertex = resource_graph.vertex_manager.getVertexByAbsPath(os.path.abspath(src))
                indexfiles = vertex.indexfile_targets
            else:
                indexfiles = [None]

            for indexfile in indexfiles:
                self.registerCustomizedParameters(parameters,
                                                  src,
                                                  indexfile=indexfile,
                                                  lang_args=lang_args,
                                                  c_args=arginfo.options)
                self.invokeAnalyzer(parameters)


class StaticAnalyzerFakeCompiler(FakeCompilerBase):

    def retriveCustomizedParametersFromScanBuild(self, param):
        pass

    def registerCustomizedParameters(self,
                                     param,
                                     src,
                                     indexfile=None,
                                     arch_args=[],
                                     lang_args=[],
                                     c_args=[]):
        param.update({
            'compiler_args': arch_args+lang_args+c_args+[src],
            'src': src
        })

    def generateAnalyzerCommandByParameters(self, param):

        def getOutputPath(format, dir):
            if format in ['plist', 'plist-html']:
                (handle, name) = tempfile.mkstemp(prefix='report-',
                                                  suffix='.plist',
                                                  dir=dir)
                os.close(handle)
                os.chmod(name, 0o664)
                return name
            return dir

        clang = param['clang']
        o_fmt = param['output_format']
        o_dir = param['output_dir']
        c_args = param['compiler_args']
        a_args = param['analyzer_args']
        o_args = ['-o', getOutputPath(o_fmt, o_dir)]

        cmd = getClangCC1Args(clang, ['--analyze']+a_args+c_args+o_args)
        return cmd

    def preprocess(self, param):
        pass

    def postprocess(self, param):
        pass


class MisraFakeCompiler(FakeCompilerBase):

    def retriveCustomizedParametersFromScanBuild(self, param):
        param.update({
            'ctumode': os.getenv('CCC_ANALYZER_CTUMODE'),
            'resource_graph_path': os.getenv('CCC_ANALYZER_RESOURCE_GRAPH_PATH')
        })
        return param

    def registerCustomizedParameters(self,
                                     param,
                                     src,
                                     indexfile=None,
                                     arch_args=[],
                                     lang_args=[],
                                     c_args=[]):

        def getReportOutputPath(output_dir, src):
            prefix, _ = os.path.splitext(os.path.basename(src))
            time_format = '%Y-%m-%d-%H%M%S-%f'
            time_stamp = datetime.datetime.now().strftime(time_format)
            suffix = '_%s.json' % time_stamp
            return os.path.join(output_dir, prefix + suffix)

        def getAstOutputDir(output_dir, project_root, src):
            src_relpath = os.path.relpath(src, start=project_root)
            src_reldir = os.path.dirname(src_relpath)
            if src_reldir.startswith('..'):
                src_reldir = '.'
            ast_dir = os.path.join(output_dir, 'ast', src_reldir)
            if not os.path.isdir(ast_dir):
                os.makedirs(ast_dir, exist_ok=True)
            if not ast_dir.endswith(os.path.sep):
                ast_dir = ast_dir + os.path.sep
            return ast_dir

        ctumode = param['ctumode']
        o_dir = param['output_dir']
        proj_root = param['project_root']
        a_args = param['analyzer_args']

        # param['analyzer_args'] does not contain output flags for JSON report
        # and .ast files, so we are going to fill in the flags.
        # Notice that we have to reserve param['analyzer_args'] because the
        # output flags are inconvenient for users to invoke the analyzer
        # command again.
        report_path = getReportOutputPath(o_dir, src)
        ast_dir = getAstOutputDir(o_dir, proj_root, src)
        o_args = ['-plugin-arg-Misra-Checker', '-o=%s' % report_path]
        ast_args = ['-plugin-arg-Misra-Checker', '-astdir=%s' % ast_dir]
        if ctumode:
            ast_args.extend(['-plugin-arg-Misra-Checker', '-ctu=true'])
            ast_args.extend(['-plugin-arg-Misra-Checker', '-index=%s' % indexfile])
        ext_analyzer_args = [x for a in o_args+ast_args for x in ['-Xclang', a]]
        param['final_analyzer_args'] = a_args + ext_analyzer_args

        param.update({
            'compiler_args': arch_args+lang_args+c_args+[src],
            'src': src,
            'report_path': report_path
        })

    def generateAnalyzerCommandByParameters(self, param):
        clang = param['clang']
        c_args = param['compiler_args']
        a_args = param['analyzer_args']
        final_a_args = param['final_analyzer_args']
        e_args = ['-fsyntax-only', '-fparse-all-comments', '-fno-trigraphs']

        # this is for user to re-invoke the analyzer command again
        param['analyzer_cmd'] = getClangCC1Args(clang, e_args+a_args+c_args)
        # this is the real analyzer command
        cmd = getClangCC1Args(clang, e_args+final_a_args+c_args)
        return cmd

    def log(self, param):

        def createLogDir(output_dir):
            log_dir = os.path.join(output_dir, 'logs')
            if not os.path.isdir(log_dir):
                os.makedirs(log_dir)
            return log_dir

        def getLogPath(output_dir):
            (handle, name) = tempfile.mkstemp(prefix='cmd_',
                                              suffix='.log.json',
                                              dir=createLogDir(output_dir))
            os.close(handle)
            os.chmod(name, 0o664)
            return name

        log_path = getLogPath(param['output_dir'])
        with open(log_path, 'w') as fp:
            json.dump({
                'cwd': os.getcwd(),
                'cmd': param['analyzer_cmd'],
                'report_path': param['report_path'],
            }, fp)

    def preprocess(self, param):
        pass

    def postprocess(self, param):
        report_path = param['report_path']
        if os.path.exists(report_path):
            JSONReportHelper.rewriteWithAbsPath(report_path)
            self.log(param)
