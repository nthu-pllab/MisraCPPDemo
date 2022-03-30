import argparse
import time
import datetime
import getpass
import multiprocessing
import os
import re
import socket
import subprocess
import tempfile
from abc import ABC
from abc import abstractmethod

from libmisrascan import LIBMISRASCAN_BIN
from libmisrascan import exists
from libmisrascan import getClangVersion
from libmisrascan import runDispatchedCommand
from cmdanalyzer import CmdAnalyzer
from cmdanalyzer import VirtualLinker
from reportutils import HTMLReportHelper
from reportutils import JSONReportHelper


class IllegalArgumentError(Exception):
    pass


class ScanBuildBase(ABC):

    @property
    @abstractmethod
    def CC_ANALYZER(self):
        pass

    @property
    @abstractmethod
    def CXX_ANALYZER(self):
        pass

    def createBasicArgumentParser(self):
        parser = argparse.ArgumentParser()
        checker_opts = parser.add_argument_group('checker options')
        build_opts = parser.add_argument_group('build options')
        output_opts = parser.add_argument_group('output options')
        format_group = output_opts.add_mutually_exclusive_group()
        advanced_opts = parser.add_argument_group('advanced options')

        build_opts.add_argument(
            '--keep-going',
            '-k',
            dest='ignore_errors',
            action='store_true',
            help="""Add a "keep on going" option to the specified build command.""")
        build_opts.add_argument(
            '--use-cc',
            metavar='<path>',
            dest='cc',
            help="""When '%(prog)s' analyzes a project by interposing a compiler
            wrapper, which executes a real compiler for compilation and do other
            tasks (record the compiler invocation). Because of this interposing,
            '%(prog)s' does not know what compiler your project normally uses.
            Instead, it simply overrides the CC environment variable, and guesses
            your default compiler.

            If you need '%(prog)s' to use a specific compiler for *compilation*
            then you can use this option to specify a path to that compiler.""")
        build_opts.add_argument(
            '--use-c++',
            metavar='<path>',
            dest='cxx',
            help="""This is the same as "--use-cc" but for C++ code.""")

        checker_opts.add_argument(
            '--load-plugin',
            '-load-plugin',
            metavar='<plugin library>',
            dest='plugins',
            default=[],
            action='append',
            help="""Loading external checkers using the clang plugin interface.""")

        output_opts.add_argument(
            '--output',
            '-o',
            metavar='<path>',
            default=tempfile.gettempdir(),
            help="""Specifies the output directory for analyzer reports.
            Subdirectory will be created if default directory is targeted.""")
        output_opts.add_argument(
            '--keep-empty',
            action='store_true',
            help="""Don't remove the build results directory even if no issues
            were reported.""")
        output_opts.add_argument(
            '--no-failure-reports',
            '-no-failure-reports',
            dest='report_failures',
            action='store_false',
            help="""Do not create a 'failures' subdirectory that includes analyzer
            crash reports and preprocessed source files.""")
        output_opts.add_argument(
            '--html-title',
            metavar='<title>',
            default=os.path.basename(os.getcwd())+' - misra-scan results',
            help="""Specify the title used on generated HTML pages.
            If not specified, a default title will be used.""")
        format_group.add_argument(
            '--plist',
            '-plist',
            dest='output_format',
            const='plist',
            default='html',
            action='store_const',
            help="""Cause the results as a set of .plist files.""")
        format_group.add_argument(
            '--plist-html',
            '-plist-html',
            dest='output_format',
            const='plist-html',
            default='html',
            action='store_const',
            help="""Cause the results as a set of .html and .plist files.""")

        advanced_opts.add_argument(
            '--use-analyzer',
            metavar='<path>',
            dest='clang',
            help="""'%(prog)s' uses the 'clang' executable from the PATH for
            static analysis. One can override this behavior with this option by
            using the 'clang' packaged with Xcode (on OS X) or from the PATH.""")
        advanced_opts.add_argument(
            '--status-bugs',
            action='store_true',
            help="""The exit status of '%(prog)s' is the same as the executed
            build command. This option ignores the build exit status and sets to
            be non zero if it found potential bugs or zero otherwise.""")

        parser.add_argument(
            '--verbose',
            '-v',
            action='count',
            default=0,
            help="""Enable verbose output from '%(prog)s'. A second, third and
            fourth flags increases verbosity.""")
        parser.add_argument(
            dest='build', nargs=argparse.REMAINDER, help="""Command to run.""")

        opts = {
            'build': build_opts,
            'checker': checker_opts,
            'output': output_opts,
            'format': format_group,
            'advanced': advanced_opts
        }
        return parser, opts

    @abstractmethod
    def registerCustomizedOptions(self, parser, opts):
        pass

    def parseArguments(self):
        parser, opts = self.createBasicArgumentParser()
        self.registerCustomizedOptions(parser, opts)
        return parser.parse_args()

    @abstractmethod
    def checkArgumentValidity(self, args):
        pass

    def findClang(self, clang):
        if clang is None:
            path = exists('clang')
            if path:
                return path

            raise RuntimeError(
                "Cannot find the executable 'clang' from PATH. Consider using "
                "--use-analyzer to specify path to 'clang' to use for static "
                "analysis.")

        if os.path.isdir(clang):
            path = exists(os.path.join(clang, 'clang'))
            if path:
                return path

            path = exists(os.path.join(clang, 'bin', 'clang'))
            if path:
                return path

            raise RuntimeError(
                "Cannot find the executable 'clang' from '%s'. Consider using "
                "--use-analyzer to specify path to 'clang' to use for static "
                "analysis." % clang)
        else:
            path = exists(clang)
            if path:
                return path

            raise RuntimeError(
                "Cannot find the executable '%s'." % clang)

    def createOutputDir(self, dir):
        time_format = '%Y-%m-%d-%H%M%S-%f'
        date_string = datetime.datetime.now().strftime(time_format)
        stamp = '%s-%d' % (date_string, os.getpid())

        parent_dir = os.path.abspath(dir)
        if not os.path.exists(parent_dir):
            os.makedirs(parent_dir)

        new_dir = os.path.join(parent_dir, stamp)
        if os.path.exists(new_dir):
            raise RuntimeError(
                "The output directory '%s' already exists." % new_dir)

        os.mkdir(new_dir)
        return new_dir

    def getScanBuildEnv(self, args):
        return {
            'html_title': args.html_title,
            'user': '%s@%s' % (getpass.getuser(), socket.gethostname()),
            'cwd': os.getcwd(),
            'cmd': ' '.join(args.build),
            'clang_version': getClangVersion(args.clang),
            'date': datetime.datetime.now().strftime('%a %b %d %H:%M:%S %Y')
        }

    @abstractmethod
    def genAnalyzerParams(self, args):
        pass

    @abstractmethod
    def setupCustomizedEnvVars(self, args, env):
        pass

    def setupEnvVars(self, args):
        env = dict(os.environ)
        env_vars = {
            'CC': self.CC_ANALYZER,
            'CXX': self.CXX_ANALYZER,
            'CCC_ANALYZER_ANALYSIS': self.genAnalyzerParams(args),
            'CCC_ANALYZER_OUTPUT_DIR': args.output,
            'CCC_ANALYZER_OUTPUT_FORMAT': args.output_format,
            'CCC_ANALYZER_OUTPUT_FAILURES': 'yes' if args.report_failures else '',
            'CCC_ANALYZER_PROJECT_ROOT': os.getcwd()
        }
        if args.clang:
            env_vars['CLANG'] = args.clang
        if args.clang:
            env_vars['CLANG_CXX'] = args.clang  # TODO: construct CLANG_CXX by CLANG
        if args.cc:
            env_vars['CCC_CC'] = args.cc
        if args.cxx:
            env_vars['CCC_CXX'] = args.cxx
        env.update(env_vars)
        self.setupCustomizedEnvVars(args, env)
        return env

    @abstractmethod
    def runModifiedBuildCommand(self, args):
        pass

    @abstractmethod
    def postprocess(self, args, env):
        pass

    def scan(self):
        args = self.parseArguments()
        self.checkArgumentValidity(args)
        args.clang = self.findClang(args.clang)
        output_basedir = args.output
        args.output = self.createOutputDir(output_basedir)
        env = self.getScanBuildEnv(args)
        self.runModifiedBuildCommand(args)
        self.postprocess(args, env)


class ScanBuild(ScanBuildBase):

    @property
    def CC_ANALYZER(self):
        return os.path.join(LIBMISRASCAN_BIN, 'ccc-analyzer')

    @property
    def CXX_ANALYZER(self):
        return os.path.join(LIBMISRASCAN_BIN, 'c++-analyzer')

    def registerCustomizedOptions(self, parser, opts):
        checker_opts = opts['checker']
        advanced_opts = opts['advanced']

        checker_opts.add_argument(
            '--enable-checker',
            '-enable-checker',
            metavar='<checker name>',
            dest='enable_checkers',
            default=[],
            action='append',
            help="""Enable specific checker.""")
        checker_opts.add_argument(
            '--disable-checker',
            '-disable-checker',
            metavar='<checker name>',
            dest='disable_checkers',
            default=[],
            action='append',
            help="""Disable specific checker.""")

        advanced_opts.add_argument(
            '--analyze-headers',
            action='store_true',
            help="""Also analyze functions in #included files. By default, such
            functions are skipped unless they are called by functions within the
            main source file.""")
        advanced_opts.add_argument(
            '--store',
            '-store',
            metavar='<model>',
            dest='store_model',
            choices=['region', 'basic'],
            help="""Specify the store model used by the analyzer. 'region'
            specifies a field- sensitive store model. 'basic' which is far less
            precise but can more quickly analyze code. 'basic' was the default
            store model for checker-0.221 and earlier.""")
        advanced_opts.add_argument(
            '--constraints',
            '-constraints',
            metavar='<model>',
            dest='constraints_model',
            choices=['range', 'basic'],
            help="""Specify the constraint engine used by the analyzer. Specifying
            'basic' uses a simpler, less powerful constraint model used by
            checker-0.160 and earlier.""")
        advanced_opts.add_argument(
            '--internal-stats',
            action='store_true',
            help="""Generate internal analyzer statistics.""")
        advanced_opts.add_argument(
            '--analyzer-config',
            '-analyzer-config',
            metavar='<options>',
            help="""Provide options to pass through to the analyzer's
            -analyzer-config flag. Several options are separated with comma:
            'key1=val1,key2=val2'

            Available options:
                stable-report-filename=true or false (default)

            Switch the page naming to:
            report-<filename>-<function/method name>-<id>.html
            instead of report-XXXXXX.html""")
        advanced_opts.add_argument(
            '--stats',
            '-stats',
            action='store_true',
            help="""Generates visitation statistics for the project.""")
        advanced_opts.add_argument(
            '--maxloop',
            '-maxloop',
            metavar='<loop count>',
            type=int,
            default=0,
            help="""Specifiy the number of times a block can be visited before
            giving up. Increase for more comprehensive coverage at a cost of
            speed.""")
        advanced_opts.add_argument(
            '--force-analyze-debug-code',
            dest='force_debug',
            action='store_true',
            help="""Tells analyzer to enable assertions in code even if they were
            disabled during compilation, enabling more precise results.""")

    def checkArgumentValidity(self, args):
        pass

    def genAnalyzerParams(self, args):
        params = []

        if args.store_model:
            params.append('-analyzer-store=%s' % args.store_model)
        if args.constraints_model:
            params.append('-analyzer-constraints=%s' % args.constraints_model)
        if args.internal_stats:
            params.append('-analyzer-stats')
        if args.analyze_headers:
            params.append('-analyzer-opt-analyze-headers')
        if args.stats:
            params.append('-analyzer-checker=debug.Stats')
        if args.maxloop > 0:
            params.extend(['-analyzer-max-loop', args.maxloop])
        if args.analyzer_config:
            params.extend(['-analyzer-config', args.analyzer_config])
        if args.output_format:
            params.append('-analyzer-output=%s' % args.output_format)
        for plugin in args.plugins:
            params.extend(['-load', plugin])
        for checker in args.enable_checkers:
            params.extend(['-analyzer-checker', checker])
        for checker in args.disable_checkers:
            params.extend(['-analyzer-disable-checker', checker])

        return ' '.join(params)

    def setupCustomizedEnvVars(self, args, env):
        pass

    def replaceBuildCmd(self, args):

        def isCC(cmd):
            return re.match(r'.*\/?gcc[^\/]*$', cmd) or \
                re.match(r'.*\/?cc[^\/]*$', cmd) or \
                re.match(r'.*\/?llvm-gcc[^\/]*$', cmd) or \
                re.match(r'.*\/?clang$', cmd) or \
                re.match(r'.*\/?ccc-analyzer[^\/]*$', cmd)

        def isCXX(cmd):
            return re.match(r'.*\/?g\+\+[^\/]*$', cmd) or \
                re.match(r'.*\/?c\+\+[^\/]*$', cmd) or \
                re.match(r'.*\/?llvm-g\+\+[^\/]*$', cmd) or \
                re.match(r'.*\/?clang\+\+$', cmd) or \
                re.match(r'.*\/?c\+\+-analyzer[^\/]*$', cmd)

        def isMake(cmd):
            return cmd == 'make' or cmd == 'gmake'

        cmd = args.build[0]
        # TODO: do not generate output if cmd is 'configure' or 'autogen'
        if isCC(cmd):
            args.build[0] = self.CC_ANALYZER
        elif isCXX(cmd):
            args.build[0] = self.CXX_ANALYZER
        elif isMake(cmd):
            if args.ignore_errors:
                args.build.extend(['-k', '-i'])
            args.build.extend(['CC=%s' % self.CC_ANALYZER,
                               'CXX=%s' % self.CXX_ANALYZER])

    def runModifiedBuildCommand(self, args):
        env = self.setupEnvVars(args)
        # TODO: escape shell command
        self.replaceBuildCmd(args)
        subprocess.call(args.build, env=env)

    def postprocess(self, args, env):
        report_helper = HTMLReportHelper(report_dir=args.output)
        report_helper.process(args, env)


class MisraScanBuild(ScanBuildBase):

    @property
    def CC_ANALYZER(self):
        return os.path.join(LIBMISRASCAN_BIN, 'misra-ccc-analyzer')

    @property
    def CXX_ANALYZER(self):
        return os.path.join(LIBMISRASCAN_BIN, 'misra-c++-analyzer')

    def registerCustomizedOptions(self, parser, opts):
        checker_opts = opts['checker']

        checker_opts.add_argument(
            '--config-path',
            '-config-path',
            metavar='<config path>',
            dest='config_path',
            help="""Loading config file of external checkers.""")

    def checkArgumentValidity(self, args):
        if not args.plugins:
            raise IllegalArgumentError(
                "Absolute path to MisraC plugin library should be specified "
                "by '-load-plugin' option")

        if args.config_path is None:
            raise IllegalArgumentError(
                "Relative path to config of MisraC plugin should be specified "
                "by '-config-path' option")

    def genAnalyzerParams(self, args):
        params = []

        if args.plugins:
            for plugin in args.plugins:
                params.extend(['-load', plugin])
        params.extend(['-plugin', 'Misra-Checker'])
        if args.config_path:
            abspath = os.path.abspath(args.config_path)
            params.extend(['-plugin-arg-Misra-Checker', '-config=%s' % abspath])

        return ' '.join(params)

    def setupCustomizedEnvVars(self, args, env):
        pass

    def replaceBuildCmd(self, args):

        def isMake(cmd):
            return cmd == 'make' or cmd == 'gmake'

        cmd = args.build[0]
        if args.ignore_errors and isMake(cmd):
            args.build.extend(['-k', '-i'])

    def runModifiedBuildCommand(self, args):
        env = self.setupEnvVars(args)
        self.replaceBuildCmd(args)

        cmd_analyzer = CmdAnalyzer()
        strace_log = cmd_analyzer.trace(args)

        print("[misra-scan] analyzing build commands...", end="")
        time_begin = time.time()
        cmd_records = cmd_analyzer.analyze(strace_log)
        integrated_args = []
        for cmd_record in cmd_records:
            if cmd_record.isCC:
                cmd_record.argv[0] = self.CC_ANALYZER
            elif cmd_record.isCXX:
                cmd_record.argv[0] = self.CXX_ANALYZER
            else:
                continue
            integrated_args.append((cmd_record.argv, cmd_record.pwd, env))
        elapsed_time = time.time() - time_begin
        print(f" ({elapsed_time})")

        print("[misra-scan] running single-translation-unit checkers...", end="")
        time_begin = time.time()
        with multiprocessing.Pool() as process_pool:
            process_pool.map(runDispatchedCommand, integrated_args)
        elapsed_time = time.time() - time_begin
        print(f" ({elapsed_time})")

        print("[misra-scan] running cross-translation-unit checkers...", end="")
        time_begin = time.time()
        project_root = os.getcwd()
        vitual_linker = VirtualLinker(project_root, args.output)
        report_helper = JSONReportHelper(report_dir=args.output)
        resource_graph = cmd_analyzer.buildResourceGraph(project_root, cmd_records)
        resource_graph = vitual_linker.updateIndexfilesOnResourceGraph(resource_graph)
        shared_obj_path = vitual_linker.save(resource_graph)
        report_helper.genResourceGraphHTML(resource_graph)
        env.update({
            'CCC_ANALYZER_CTUMODE': 'yes',
            'CCC_ANALYZER_RESOURCE_GRAPH_PATH': shared_obj_path
        })
        with multiprocessing.Pool() as process_pool:
            process_pool.map(runDispatchedCommand, integrated_args)
        elapsed_time = time.time() - time_begin
        print(f" ({elapsed_time})")

    def postprocess(self, args, env):
        print("[misra-scan] collecting reports...")
        report_helper = JSONReportHelper(src_dir=os.getcwd(),
                                         report_dir=args.output)
        report_helper.process(args, env)
