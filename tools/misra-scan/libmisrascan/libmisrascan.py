import json
import os
import subprocess

from collections import Mapping
from collections import namedtuple
from collections import OrderedDict
from types import FunctionType
from types import LambdaType


LIBMISRASCAN_ROOT = os.path.dirname(os.path.abspath(__file__))
LIBMISRASCAN_BIN = os.path.join(LIBMISRASCAN_ROOT, 'bin')
LIBMISRASCAN_TEMPLATES = os.path.join(LIBMISRASCAN_ROOT, 'templates')
SCAN_BUILD = os.path.join(LIBMISRASCAN_BIN, 'scan-build')
MISRA_SCAN_BUILD = os.path.join(LIBMISRASCAN_BIN, 'misra-scan-build')


def isExecutable(file):
    return os.access(file, os.F_OK | os.X_OK)


def exists(file):
    if isExecutable(file):
        return os.path.realpath(file)

    env_path = os.getenv('PATH')
    if env_path:
        for dir in env_path.split(os.pathsep):
            location = os.path.join(dir, file)
            if isExecutable(location):
                return os.path.realpath(location)

    return False


def getClangCC1Args(clang, args):
    cmd = [clang, '-###'] + args
    output = runCommandAndGetOutput(cmd)
    lastline = output[-1]
    return lastline


def getClangVersion(clang):
    cmd = [clang, '-v']
    output = runCommandAndGetOutput(cmd)
    return output[0]


def runDispatchedCommand(args):
    argv, pwd, env = args
    if os.path.isdir(pwd):
        os.chdir(pwd)
        subprocess.call(argv, env=env)


def runCommandAndGetOutput(cmd, shell=False):
    try:
        output = subprocess.check_output(cmd,
                                         shell=shell,
                                         stderr=subprocess.STDOUT)
        return output.decode('utf-8').splitlines()
    except subprocess.CalledProcessError as ex:
        ex.output = ex.output.decode('utf-8')
        raise ex


def makelist(iterable, convert=str):
    if iterable:
        ret = []
        for elem in iterable:
            if isinstance(elem, convert):
                ret.append(elem)
            else:
                ret.append(convert(**elem))
        return ret
    else:
        return []


def listof(misra_namedtuple_type):
    return lambda x: makelist(x, convert=misra_namedtuple_type)


def MisraNamedTuple(typename,
                    field_names,
                    default_val=(),
                    default_type={},
                    **kwargs):

    def __new__(_cls, *args, **kwargs):
        # remove irrevalant fields
        for key in tuple(kwargs):
            if key not in _cls._fields:
                del kwargs[key]
        # type conversion for each field
        for field, convert in _cls.__convert__.items():
            if field in _cls._fields and field in kwargs:
                arg = kwargs[field]
                if isinstance(arg, Mapping):
                    kwargs[field] = convert(**arg)
                elif isinstance(convert, LambdaType):
                    kwargs[field] = convert(arg)
                elif not isinstance(arg, convert):
                    kwargs[field] = convert(arg)
        # call original namedtuple constructor
        return _cls.__purenew__(_cls, *args, **kwargs)

    def __hash__(self):
        return hash(self.tojson())

    def _asrealdict(self):

        def serialize(field, obj):
            if field not in self.__convert__:
                return obj
            convert_func = self.__convert__[field]
            if isinstance(convert_func, LambdaType):
                try:
                    return [e._asrealdict() for e in obj]
                except:
                    return obj
            if hasattr(obj, '_asrealdict'):
                return obj._asrealdict()
            return obj

        value = [serialize(f, self[i]) for i, f in enumerate(self._fields)]
        return OrderedDict(zip(self._fields, value))

    def tojson(self):
        return json.dumps(self._asrealdict())

    NewType = namedtuple(typename, field_names)

    # use default constructor to get the tuple of default values for each field
    NewType.__new__.__defaults__ = (None,) * len(NewType._fields)
    if isinstance(default_val, Mapping):
        prototype = NewType(**default_val)
    else:
        prototype = NewType(*default_val)

    # replace default constructor so that type conversion for each field
    # becomes available
    NewType.__purenew__ = NewType.__new__
    NewType.__purenew__.__defaults__ = tuple(prototype)
    NewType.__convert__ = default_type
    NewType.__new__ = __new__

    # add _asrealdict method to help convert MisraNamedTuple to json-like dict
    NewType._asrealdict = _asrealdict
    NewType.tojson = tojson

    # use json format string for computing the hash
    NewType.__hash__ = __hash__

    return NewType
