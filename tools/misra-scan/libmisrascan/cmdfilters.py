import os
import re

from abc import ABC
from abc import abstractmethod

from libmisrascan import listof
from libmisrascan import MisraNamedTuple


ArgInfo = MisraNamedTuple(
    'ArgInfo',
    field_names=['inputs',
                 'outputs',
                 'options',
                 'lang',
                 'archs'],
    default_type={
        'inputs': listof(str),
        'outputs': listof(str),
        'options': listof(str),
        'archs': listof(str)
    })


class CmdPattern:
    AR = re.compile(r'.*\/?ar$')
    CC = re.compile(r'.*\/?cc$')
    CLANG = re.compile(r'.*\/?clang[^\/]*$')
    CLANGPLUSPLUS = re.compile(r'.*\/?clang\+\+[^\/]*$')
    CPLUSPLUS = re.compile(r'.*\/?c\+\+$')
    GCC = re.compile(r'.*\/?gcc[^\/]*$')
    GPLUSPLUS = re.compile(r'.*\/?g\+\+[^\/]*$')
    LLVMGCC = re.compile(r'.*\/?llvm-gcc[^\/]*$')
    LLVMGPLUSPLUS = re.compile(r'.*\/?llvm-g\+\+[^\/]*$')


class CmdFilterBase(ABC):

    @property
    @abstractmethod
    def cmd_patterns(self):
        pass

    def match(self, cmd):
        for pattern in self.cmd_patterns:
            if pattern.match(cmd):
                return True
        return False

    @abstractmethod
    def inspectArgs(self, args):
        pass


class CCCmdFilter(CmdFilterBase):
    NOP_OPTIONS = [
        '-E',
        '-M',
        '-MM',
        '-print-multiarch',
        '-v',
        '--print-prog-name',
        '--version',
        '-###'
    ]

    COMPILER_OPTIONS = {
        # compiler options
        '-D': 1,
        '-F': 1,
        '-I': 1,
        '-L': 1,
        '-U': 1,
        '-Xclang': 1,
        '-nostdinc': 0,
        '-include': 1,
        '-idirafter': 1,
        '-imacros': 1,
        '-iprefix': 1,
        '-iquote': 1,
        '-iwithprefix': 1,
        '-iwithprefixbefore': 1,

        # compiler linker options
        '-Wwrite-strings': 0,
        '-ftrapv-handler': 1,  # specifically call out separated -f flag
        '-mios-simulator-version-min': 0,  # This really has 1 argument
        '-isysroot': 1,
        '-m32': 0,
        '-m64': 0,
        '-stdlib': 0,  # This is really a 1 argument
        '--sysroot': 1,
        '-target': 1,
        '-v': 0,
        '-mmacosx-version-min': 0,  # This is really a 1 argument
        '-miphoneos-version-min': 0,  # This is really a 1 argument
        '--target': 0
    }

    IGNORED_OPTIONS = {
        # preprocessor options
        '-MD': 0,
        '-MMD': 0,
        '-MG': 0,
        '-MP': 0,
        '-MF': 1,
        '-MT': 1,
        '-MQ': 1,

        # linker options
        '-framework': 1,
        '-fobjc-link-runtime': 0,

        # other options
        '-fsyntax-only': 0,
        '-save-temps': 0,
        '-install_name': 1,
        '-exported_symbols_list': 1,
        '-current_version': 1,
        '-compatibility_version': 1,
        '-init': 1,
        '-e': 1,
        '-seg1addr': 1,
        '-bundle_loader': 1,
        '-multiply_defined': 1,
        '-sectorder': 3,
        '--param': 1,
        '-u': 1,
        '--serialize-diagnostics': 1,
    }

    DISABLED_ARCHS = ['ppc', 'ppc64']

    @property
    def cmd_patterns(self):
        return [CmdPattern.GCC, CmdPattern.CC,
                CmdPattern.CLANG, CmdPattern.LLVMGCC]

    def inspectArgs(self, argv):
        args = iter(argv[1:])
        arginfo_inputs = []
        arginfo_outputs = [None]
        arginfo_options = []
        arginfo_archs = []
        arginfo_lang = None

        for arg in args:
            # no need to compile
            if arg in self.NOP_OPTIONS:
                return ArgInfo([], [], [], None, [])
            # some aruguments contains '=', we should split it first
            argkey = arg.split('=')[0]
            # nothing on the left hand side of '=', just ignore it
            if argkey == '':
                pass
            # some options have nothing to do with compilation, just ignore them
                '''elif argkey in self.IGNORED_OPTIONS:
                count = self.IGNORED_OPTIONS[argkey]
                for _ in range(count):
                    next(args)'''
            # avoid some compiler options being treated as source files
                '''elif argkey in self.COMPILER_OPTIONS:
                count = self.COMPILER_OPTIONS[argkey]
                arginfo_options.append(argkey)
                for _ in range(count):
                    arginfo_options.append(next(args))'''
            # take architecture information
            elif arg == '-arch':
                arginfo_archs.append(next(args))
            # take language information
            elif arg == '-x':
                arginfo_lang = next(args)
            # take output information
            elif arg == '-o':
                arginfo_outputs[0] = next(args)
            # treat this argument as source file
            elif re.match(r'(^.+\.c)|(^.+\.cpp)', arg):
                arginfo_inputs.append(arg)
            # ignore the warnings that we don't care about
            elif re.match(r'^-W.+', arg) and not re.match(r'^-Wno-.+', arg):
                pass
            # treat this argument as compiler option
            else:
                arginfo_options.append(arg)

        # check target architecture info
        if arginfo_archs:
            filtered_archs = [a for a in arginfo_archs if
                              a not in self.DISABLED_ARCHS]
            if filtered_archs:
                arginfo_archs = filtered_archs
            else:
                # no supported architecture, skip analysis
                return ArgInfo([], [], [], None, [])

        # check existence of input files
        if not arginfo_inputs:
            return ArgInfo([], [], [], None, [])

        # predict output filenames if they are not specified
        if arginfo_outputs[0] is None:
            if '-c' in arginfo_options:
                arginfo_outputs = [os.path.splitext(f)[0]+'.o' for f in arginfo_inputs]
            else:
                arginfo_outputs[0] = 'a.out'

        return ArgInfo(inputs=arginfo_inputs,
                       outputs=arginfo_outputs,
                       options=arginfo_options,
                       archs=arginfo_archs,
                       lang=arginfo_lang)


class CXXCmdFilter(CCCmdFilter):

    @property
    def cmd_patterns(self):
        return [CmdPattern.GPLUSPLUS, CmdPattern.CPLUSPLUS,
                CmdPattern.CLANGPLUSPLUS, CmdPattern.LLVMGPLUSPLUS]


class ARCmdFilter(CmdFilterBase):

    @property
    def cmd_patterns(self):
        return [CmdPattern.AR]

    def inspectArgs(self, argv):
        arginfo_inputs = []

        args = iter(argv[1:])
        for arg in args:
            if arg == '--plugin':
                next(args)
            elif arg.startswith('-'):
                pass
            elif '.' in arg:
                arginfo_inputs.append(arg)

        if len(arginfo_inputs) < 2:
            return ArgInfo()

        arginfo_output, *arginfo_inputs = arginfo_inputs
        return ArgInfo(inputs=arginfo_inputs, outputs=[arginfo_output])


class CmdFilterManager:

    _filters = []

    @classmethod
    def register(_cls, filter_cls):
        _cls._filters.append(filter_cls)

    def filters(self):
        for filter_class in self._filters:
            yield filter_class()


CmdFilterManager.register(CCCmdFilter)
CmdFilterManager.register(CXXCmdFilter)
CmdFilterManager.register(ARCmdFilter)
