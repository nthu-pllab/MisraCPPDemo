import glob
import hashlib
import json
import os
import re
import shutil
import tempfile

from abc import ABC
from abc import abstractmethod
from collections import deque
from collections import namedtuple
from collections import OrderedDict
from operator import attrgetter
from itertools import compress

from jinja2 import Environment, FileSystemLoader, Template, select_autoescape
from libmisrascan import LIBMISRASCAN_TEMPLATES
from libmisrascan import listof
from libmisrascan import MisraNamedTuple


JINJA2_ENV = Environment(
    loader=FileSystemLoader(LIBMISRASCAN_TEMPLATES),
    autoescape=select_autoescape(['html']),
    trim_blocks=True,
    lstrip_blocks=True
)
SUMMARY_ASSETS = ['scanview.css', 'sorttable.js', 'summary.js']
SUMMARY_TEMPLATE = JINJA2_ENV.get_template('summary.html')
REPORT_ASSETS = ['report.css']
MISRA_REPORT_TEMPLATE = JINJA2_ENV.get_template('misra_report.html')
COV_REPORT_TEMPLATE = JINJA2_ENV.get_template('cov_report.html')
RESOURCE_GRAPH_ASSETS = ['cytoscape-dagre.js']
RESOURCE_GRAPH_TEMPLATE = JINJA2_ENV.get_template('resource_graph.html')
CODE_VIEWER_TEMPLATE = JINJA2_ENV.get_template('code_viewer.html')


def computeSHA256Digest(file_path):
    with open(file_path, 'rb') as fp:
        contents = fp.read()
        digest = hashlib.sha256(contents).hexdigest()
    return digest


def outputHTML(output_path, assets, html_content):
    output_dir = os.path.dirname(output_path)
    assets_dir = os.path.join(output_dir, 'assets')
    if not os.path.isdir(assets_dir):
        os.makedirs(assets_dir, exist_ok=True)

    # copy static files
    for filename in assets:
        src = os.path.join(LIBMISRASCAN_TEMPLATES, filename)
        dest = os.path.join(assets_dir, filename)
        if not os.path.exists(dest):
            shutil.copyfile(src, dest)

    # write html
    with open(output_path, 'w') as fp:
        fp.write(html_content)


SourceLocation = MisraNamedTuple(
    'SourceLocation',
    field_names=['file',
                 'line',
                 'column'],
    default_type={
        'line': int,
        'column': int
    })


class PathDiagnosticPiece(MisraNamedTuple(
        'PathDiagnosticPiece',
        field_names=['depth',
                     'kind',
                     'message',
                     'extended_message',
                     'location',
                     'ranges'],
        default_val={'depth': 0},
        default_type={
            'depth': int,
            'location': SourceLocation,
            'ranges': listof(SourceLocation)
        })):

    @property
    def range_begin(self):
        return self.ranges[0]

    @property
    def range_end(self):
        return self.ranges[1]


PathDiagnostic = MisraNamedTuple(
    'PathDiagnostic',
    field_names=['category',
                 'type',
                 'description',
                 'check_name',
                 'location',
                 'path'],
    default_type={
        'location': SourceLocation,
        'path': listof(PathDiagnosticPiece)
    })


MisraCmdLog = MisraNamedTuple(
    'MisraCmdLog',
    field_names=['cwd',
                 'cmd'])


MisraAnalyzerFailure = MisraNamedTuple(
    'MisraAnalyzerFailure',
    field_names=['cwd',
                 'cmd',
                 'type',
                 'src',
                 'clang_preprocessed_file',
                 'stderr',
                 'exit_status'])


MisraBugReport = MisraNamedTuple(
    'MisraBugReport',
    field_names=['clang_version',
                 # 'files'
                 'diagnostics'],
    default_type={
        'diagnostics': listof(PathDiagnostic)
    })


class BugReportEntry(namedtuple('BugReportEntry',
                     ['bug_category', 'bug_type', 'bug_description',
                      'file', 'function', 'line', 'path_length', 'report_link'])):

    def __new__(cls, *args, **kwargs):
        for key in tuple(kwargs):
            if key not in cls._fields:
                del kwargs[key]
        return super().__new__(cls, *args, **kwargs)

    @property
    def bt_class(self):
        return self.bug_type.replace(' ', '_')


class ReportHelper(ABC):

    def __init__(self, report_dir=None, filename_pattern=None):
        self.report_dir = report_dir
        self.filename_pattern = filename_pattern

    @property
    def reports(self):
        if getattr(self, '_reports', None) is None:
            self._reports = self.scanReports()
        return self._reports

    @property
    def bug_report_entries(self):
        if getattr(self, '_bug_report_entries', None) is None:
            self._bug_report_entries = self.getBugReportEntries()
        return self._bug_report_entries

    @property
    def analyzer_failures(self):
        if getattr(self, '_analyzer_failures', None) is None:
            self._analyzer_failures = self.getAnalyzerFailures()
        return self._analyzer_failures

    @abstractmethod
    def getAnalyzerFailures(self):
        pass

    @abstractmethod
    def getBugReportEntries(self):
        pass

    @abstractmethod
    def readFailure(self, report_path):
        pass

    @abstractmethod
    def readReport(self, report_path):
        pass

    @abstractmethod
    def isEmptyReport(self, content):
        pass

    def scanReports(self):
        path_pattern = os.path.join(self.report_dir, self.filename_pattern)
        reports = []
        hash_have_seen = {}

        for file_path in glob.glob(path_pattern):
            content = self.readReport(file_path)

            # remove empty reports
            if self.isEmptyReport(content):
                os.remove(file_path)
                continue

            # remove duplicated reports
            digest = computeSHA256Digest(file_path)
            if hash_have_seen.get(digest):
                os.remove(file_path)
                continue

            hash_have_seen.update({digest: 1})
            reports.append(content)

        return reports

    def genSummaryHTML(self, env, output_dir=None, filename='index.html'):
        if output_dir is None:
            output_dir = self.report_dir

        output_path = os.path.join(output_dir, filename)
        html_content = SUMMARY_TEMPLATE.render(env=env,
                                               failures=self.analyzer_failures,
                                               reports=self.bug_report_entries)
        outputHTML(output_path, SUMMARY_ASSETS, html_content)
        print(output_path)

    def process(self, args, env):
        if self.bug_report_entries or self.analyzer_failures:
            self.genSummaryHTML(env)
        elif not args.keep_empty:
            print("Removing directory '%s' because it contains no reports." % self.report_dir)
            shutil.rmtree(self.report_dir)


class JSONReportHelper(ReportHelper):

    def __init__(self,
                 src_dir=None,
                 report_dir=None,
                 filename_pattern='*_*.json'):
        super().__init__(report_dir, filename_pattern)
        self.src_dir = src_dir
        self.code_highlighter = SourceCodeHighlighter()

    @property
    def logs(self):
        if not getattr(self, '_logs', None):
            self._logs = self.getLogs()
        return self._logs

    def readLog(self, log_path):
        with open(log_path) as fp:
            content = json.load(fp)
        return content

    def readFailure(self, report_path):
        with open(report_path) as fp:
            content = json.load(fp)
        return content

    def readReport(self, report_path):
        with open(report_path) as fp:
            content = json.load(fp)
        content.update({'report_path': report_path})
        return content

    def isEmptyReport(self, content):
        return not content.get('diagnostics')

    def getLogs(self):
        ret = {}
        path_pattern = os.path.join(self.report_dir, 'logs', 'cmd_*.log.json')
        for file_path in glob.glob(path_pattern):
            content = self.readLog(file_path)
            source_file = content.pop('report_path', None)
            if source_file:
                ret[source_file] = MisraCmdLog(**content)
        return ret

    def getAnalyzerFailures(self):
        ret = []
        path_pattern = os.path.join(self.report_dir, 'failures', '*.info.json')
        for file_path in glob.glob(path_pattern):
            content = self.readFailure(file_path)
            ret.append(MisraAnalyzerFailure(**content))
        ret.sort(key=attrgetter('type', 'src'))
        return ret

    def getBugReportEntries(self):
        from reportfilters import ReportFilterManager

        diagnostics = []
        report_refs = {}
        hash_have_seen = {}
        for report in self.scanReports():
            report_path = report.get('report_path')
            for diag in report.get('diagnostics'):

                paths = diag['path']
                final_path = paths[0]
                depth = -1
                for path in paths:
                    if path['kind'] == 'event' and path['depth'] > depth:
                        depth = path['depth']
                        final_path = path

                diag.update({'location': final_path['location']})
                pd = PathDiagnostic(**diag)
                hash_val = hash(pd)
                # ignore duplicated path diagnostics
                if hash_val not in hash_have_seen:
                    hash_have_seen[hash_val] = 1
                    diagnostics.append(pd)
                    report_refs[hash_val] = report_path
            # remove original report files
            os.remove(report_path)

        if not diagnostics:
            return []

        # customized filter
        filter_manager = ReportFilterManager()
        for filter_obj in filter_manager.filters():
            mask = filter_obj.generateFilterMask(diagnostics)
            diagnostics = list(compress(diagnostics, mask))

        # builtin filter
        new_diags = []
        for diag in diagnostics:
            rel_path = os.path.relpath(diag.location.file,
                                       start=self.src_dir)
            if not rel_path.startswith('..'):
                new_diags.append(diag)
        diagnostics = new_diags

        # sort diagnostics with key priority: file > line > column
        diagnostics.sort(key=attrgetter(
            'location.file',
            'location.line',
            'location.column'
        ))

        # emit merged reports
        report_template = MisraBugReport(
            clang_version=report.get('clang_version'),
            diagnostics=[])
        self.emitMergedReports(report_template, diagnostics)

        # generate html report for each defect
        ret = []
        for diag in diagnostics:
            src_relpath = os.path.relpath(diag.location.file, start=self.src_dir)
            prev_report_link = report_refs.get(hash(diag))
            report_link = self.genSingleDefectHTML(diag, prev_report_link)
            ret.append(BugReportEntry(diag.category,
                                      diag.type,
                                      diag.description,
                                      src_relpath,
                                      'unknown',
                                      diag.location.line,
                                      len(diag.path),
                                      report_link))
        ret.sort(key=attrgetter('bug_type'))
        return ret

    @staticmethod
    def rewriteWithAbsPath(report_path):

        def rewriteLocation(location):
            file = location.get('file')
            # do not rewrite path if it is null
            if file:
                location.update({'file': os.path.realpath(file)})

        with open(report_path) as fp:
            content = json.load(fp)

        diagnostics = content.get('diagnostics')
        if diagnostics is None:
            return

        for diag in diagnostics:
            for path in diag.get('path', []):
                location = path.get('location')
                rewriteLocation(location)
                ranges = path.get('ranges', [])
                for location in ranges:
                    rewriteLocation(location)

        with open(report_path, 'w') as fp:
            fp.write(json.dumps(content))

    def emitMergedReports(self, report_template, diagnostics):

        def emitMergedReport():
            if not merged_report.diagnostics:
                return

            filename, _ = os.path.splitext(os.path.basename(prev_file))
            count = filename_counter.get(filename, 0) + 1
            filename_counter[filename] = count
            output_path = os.path.join(output_dir,
                                       '%s_%d.json' % (filename, count))

            if not os.path.isdir(output_dir):
                os.makedirs(output_dir)
            with open(output_path, 'w') as fp:
                fp.write(merged_report.tojson())

            merged_report.diagnostics.clear()

        output_dir = self.report_dir
        merged_report = report_template
        prev_file = ''
        filename_counter = {}

        for diag in diagnostics:
            current_file = diag.location.file
            if current_file != prev_file:
                emitMergedReport()
                prev_file = current_file
            merged_report.diagnostics.append(diag)
        emitMergedReport()

    def genSingleDefectHTML(self, diagnostic, prev_report_link):

        def getAnalyzerCmd(prev_report_link):
            if not prev_report_link:
                return ''

            log = self.logs.get(prev_report_link)
            if not log:
                return ''

            return 'cd %s\n%s' % (log.cwd, log.cmd)

        def getHtmlPath(output_dir, basename):
            (handle, name) = tempfile.mkstemp(prefix='%s_' % basename,
                                              suffix='.html',
                                              dir=output_dir)
            os.close(handle)
            os.chmod(name, 0o664)
            return name

        source_file = diagnostic.location.file
        basename, _ = os.path.splitext(os.path.basename(source_file))
        cmd = getAnalyzerCmd(prev_report_link)
        source_lines = self.code_highlighter.highlight(diagnostic.path)[0]
        json_diagnostic = json.dumps(diagnostic._asrealdict(), indent=4)

        output_path = getHtmlPath(self.report_dir, basename)
        html_content = MISRA_REPORT_TEMPLATE.render(
            html_title=basename,
            cmd=cmd,
            diagnostic=diagnostic,
            json_diagnostic=json_diagnostic,
            source_lines=source_lines,
            src_relpath=os.path.relpath(source_file, start=self.report_dir))
        outputHTML(output_path, REPORT_ASSETS, html_content)

        code_viewer_path = os.path.join(self.report_dir, 'code_viewer.html')
        code_viewer_content = CODE_VIEWER_TEMPLATE.render()
        outputHTML(code_viewer_path, [], code_viewer_content)

        return os.path.basename(output_path)

    def genResourceGraphHTML(self, resource_graph):
        nodes, edges = resource_graph.getCytoscapeData()
        output_path = os.path.join(self.report_dir, 'resource_graph.html')
        html_content = RESOURCE_GRAPH_TEMPLATE.render(
            html_title='resource_graph', nodes=nodes, edges=edges)
        outputHTML(output_path, RESOURCE_GRAPH_ASSETS, html_content)


class HTMLReportHelper(ReportHelper):

    def __init__(self, report_dir=None, filename_pattern='report-*.html'):
        super().__init__(report_dir, filename_pattern)

    def isEmptyReport(self, content):
        return False

    # TODO
    def readFailure(self, report_path):
        pass

    def readReport(self, report_path):
        patterns = [re.compile(r'<!-- BUGDESC (?P<bug_description>.*) -->$'),
                    re.compile(r'<!-- BUGTYPE (?P<bug_type>.*) -->$'),
                    re.compile(r'<!-- BUGCATEGORY (?P<bug_category>.*) -->$'),
                    re.compile(r'<!-- BUGFILE (?P<file>.*) -->$'),
                    re.compile(r'<!-- FUNCTIONNAME (?P<function>.*) -->$'),
                    re.compile(r'<!-- BUGPATHLENGTH (?P<path_length>.*) -->$'),
                    re.compile(r'<!-- BUGLINE (?P<line>.*) -->$'),
                    re.compile(r'<!-- BUGCOLUMN (?P<column>.*) -->$')]
        endmark = re.compile(r'<!-- BUGMETAEND -->')

        content = {'report_link': report_path}
        with open(report_path) as fp:
            for line in fp.readlines():
                if endmark.match(line):
                    break

                for pattern in patterns:
                    match = pattern.match(line.strip())
                    if match:
                        content.update(match.groupdict())
                        break

        return content

    # TODO
    def getAnalyzerFailures(self):
        pass

    def getBugReportEntries(self):
        return [BugReportEntry(**report) for report in self.reports]


CovPathDiagnosticPiece = MisraNamedTuple(
    'CovPathDiagnosticPiece',
    field_names=['main',
                 'file',
                 'line',
                 'tag',
                 'description'],
    default_val={
        'main': None,
        'file': None
    },
    default_type={
        'line': int
    })


class CovPathDiagnostic(MisraNamedTuple(
        'CovPathDiagnostic',
        field_names=['checker',
                     'file',
                     'function',
                     'event'],
        default_type={
            'event': listof(CovPathDiagnosticPiece)
        })):

    @property
    def main_event(self):
        if getattr(self, '_main_event', None) is None:
            self._main_event = self.getMainEvent()
        return self._main_event

    def getMainEvent(self):
        for event in self.event:
            if event.main is not None:
                return event
        return None


class CovReportHelper(ReportHelper):
    def __init__(self,
                 src_dir=None,
                 report_dir=None,
                 filename_pattern='*Rule_[0-9]*.[0-9]*.json'):
        super().__init__(report_dir, filename_pattern)
        self.src_dir = src_dir
        self.code_highlighter = CovSourceCodeHighlighter()

    def getAnalyzerFailures(self):
        pass

    def getBugReportEntries(self):
        pass

    def readFailure(self, report_path):
        pass

    def readReport(self, report_path):
        with open(report_path) as fp:
            content = json.load(fp)
        return content

    def isEmptyReport(self, content):
        return False

    def genSummaryHTML(self,
                       diagnostics,
                       output_dir=None,
                       filename='index.html',
                       html_title=None):
        if output_dir is None:
            output_dir = self.report_dir
        if not os.path.isdir(output_dir):
            os.makedirs(output_dir)

        bug_report_entries = []
        counter = 0
        for diagnostic in diagnostics:
            report_link = self.genSingleDefectHTML(diagnostic,
                                                   output_dir,
                                                   '%d.html' % counter)
            cov_diag = CovPathDiagnostic(**diagnostic)
            src_rel_path = os.path.relpath(cov_diag.file, start=self.src_dir)
            bug_report_entries.append(BugReportEntry(
                'MisraC',
                cov_diag.checker,
                cov_diag.main_event.description,
                src_rel_path,
                cov_diag.function,
                cov_diag.main_event.line,
                1,
                report_link))
            counter = counter + 1

        output_path = os.path.join(output_dir, filename)
        html_content = SUMMARY_TEMPLATE.render(reports=bug_report_entries,
                                               html_title=html_title)
        outputHTML(output_path, SUMMARY_ASSETS, html_content)

    def genSingleDefectHTML(self, diagnostic, output_dir=None, filename=None):
        if output_dir is None:
            output_dir = self.report_dir
        if not os.path.isdir(output_dir):
            os.makedirs(output_dir)
        output_path = os.path.join(output_dir, filename)

        json_diagnostic = json.dumps(diagnostic, indent=4)
        diagnostic = CovPathDiagnostic(**diagnostic)
        source_file = diagnostic.file
        basename, _ = os.path.splitext(os.path.basename(source_file))
        source_lines = self.code_highlighter.highlight(diagnostic)[0]

        html_content = COV_REPORT_TEMPLATE.render(
            html_title=basename,
            diagnostic=diagnostic,
            json_diagnostic=json_diagnostic,
            source_lines=source_lines,
            src_relpath=os.path.relpath(source_file, start=output_dir))
        outputHTML(output_path, REPORT_ASSETS, html_content)

        code_viewer_path = os.path.join(output_dir, 'code_viewer.html')
        code_viewer_content = CODE_VIEWER_TEMPLATE.render()
        outputHTML(code_viewer_path, [], code_viewer_content)

        return os.path.basename(output_path)


class SourceCodeManager:

    def __init__(self, max_size=5):
        self.max_size = max_size
        self.cache = OrderedDict()

    def get(self, key, value=None):
        if key in self.cache:
            self.cache.move_to_end(key, last=True)
            return self.cache.get(key)
        return value

    def insert(self, key, value):
        if key in self.cache:
            self.cache.move_to_end(key, last=True)
        elif len(self.cache) == self.max_size:
            self.cache.popitem(last=False)
        self.cache[key] = value

    def fetchSrc(self, src_path):
        ret = self.get(src_path, [])
        if not ret:
            for encoding in ['utf-8', 'ISO-8859-1']:
                try:
                    with open(src_path, encoding=encoding) as fp:
                        ret = [line.rstrip() for line in fp]
                    break
                except:
                    continue
            self.insert(src_path, ret)
        return ret


HighlightInfo = MisraNamedTuple(
    'HighlightEvent',
    field_names=['file',
                 'messages',
                 'ranges'])


CodeSegment = MisraNamedTuple(
    'CodeSegment',
    field_names=['highlight',
                 'code'],
    default_val={'highlight': False},
    default_type={'highlight': bool})


MessageEvent = MisraNamedTuple(
    'MessageEvent',
    field_names=['line',
                 'column',
                 'message'],
    default_type={
        'line': int,
        'column': int
    })


SourceLine = MisraNamedTuple(
    'SourceLine',
    field_names=['lineno',
                 'code_segments',
                 'messages'],
    default_type={
        'code_segments': listof(CodeSegment),
        'messages': listof(MessageEvent)
    })


class SourceCodeHighlighter:

    def __init__(self):
        self.border_top = 10
        self.border_bottom = 10
        self.max_cached_files = 5
        self.src_manager = SourceCodeManager(self.max_cached_files)

    def getCodeSegments(self, index, code, ranges):
        lineno = index + 1
        range_begin = ranges[0]
        range_end = ranges[1]
        # this line is outside the range
        if lineno < range_begin.line or lineno > range_end.line:
            return [CodeSegment(code=code)]
        # this line is completely inside the range
        if range_begin.line < lineno < range_end.line:
            return [CodeSegment(highlight=True, code=code)]
        # this line is partially inside the range
        column_begin = 1
        column_end = len(code)
        if lineno == range_begin.line:
            column_begin = range_begin.column
        if lineno == range_end.line:
            column_end = range_end.column
        column_begin = column_begin - 1

        ret = []
        if column_begin != 0:
            ret.append(CodeSegment(code=code[:column_begin]))
        if column_end >= column_begin:
            ret.append(CodeSegment(highlight=True,
                                   code=code[column_begin:column_end]))
        if column_end != len(code):
            ret.append(CodeSegment(code=code[column_end:]))
        return ret

    def getSourceLines(self, hinfo):
        file_path = hinfo.file
        lines = self.src_manager.fetchSrc(file_path)
        top_lineno = len(lines) - 1
        bottom_lineno = 0
        messages = deque()
        ranges = deque()

        for pe in hinfo.ranges:
            top_lineno = min(top_lineno, pe.range_begin.line)
            bottom_lineno = max(bottom_lineno, pe.range_end.line)
            ranges.append(pe.ranges)
        for pe in hinfo.messages:
            top_lineno = min(top_lineno, pe.location.line)
            bottom_lineno = max(bottom_lineno, pe.location.line)
            messages.append(MessageEvent(
                pe.location.line,
                pe.location.column,
                pe.message
            ))
        top_lineno = max(0, top_lineno - self.border_top)
        bottom_lineno = min(len(lines), bottom_lineno + self.border_bottom)

        ret = []
        for ln in range(top_lineno, bottom_lineno):
            lineno = ln + 1
            srcline = SourceLine(lineno=lineno,
                                 code_segments=[],
                                 messages=[])
            if ranges:
                srcline.code_segments.extend(
                    self.getCodeSegments(ln, lines[ln], ranges[0]))
                if lineno > ranges[0][1].line:
                    ranges.popleft()
            else:
                srcline.code_segments.append(CodeSegment(code=lines[ln]))

            while messages and messages[0].line == lineno:
                srcline.messages.append(messages.popleft())

            ret.append(srcline)
        return ret

    def highlight(self, path):

        def initHighlightInfo(file_path, file_event_map):
            file_event_map[file_path] = HighlightInfo(file_path, [], [])

        def appendMessage(pe, file_event_map):
            file_path = pe.location.file
            if file_path not in file_event_map:
                initHighlightInfo(file_path, file_event_map)
            file_event_map[file_path].messages.append(pe)

        def appendRange(pe, file_event_map):
            if not pe.ranges:
                return

            file_path = pe.range_begin.file
            if file_path not in file_event_map:
                initHighlightInfo(file_path, file_event_map)
            file_event_map[file_path].ranges.append(pe)

        file_event_map = {}
        path_without_macro = ( p for p in path if p.kind != "macro")
        for pe in path_without_macro:
            appendMessage(pe, file_event_map)
            appendRange(pe, file_event_map)

        for event in file_event_map.values():
            event.messages.sort(key=attrgetter(
                'location.line',
                'location.column'
            ))
            event.ranges.sort(key=attrgetter(
                'range_begin.line',
                'range_begin.column'
            ))

        return [self.getSourceLines(e) for e in file_event_map.values()]


class CovSourceCodeHighlighter(SourceCodeHighlighter):

    def getSourceLines(self, hinfo):
        file_path = hinfo.file
        lines = self.src_manager.fetchSrc(file_path)
        top_lineno = len(lines) - 1
        bottom_lineno = 0
        messages = deque()

        for pe in hinfo.messages:
            top_lineno = min(top_lineno, pe.line)
            bottom_lineno = max(bottom_lineno, pe.line)
            messages.append(MessageEvent(
                pe.line,
                0,
                pe.description
            ))
        top_lineno = max(0, top_lineno - self.border_top)
        bottom_lineno = min(len(lines), bottom_lineno + self.border_bottom)

        ret = []
        for ln in range(top_lineno, bottom_lineno):
            lineno = ln + 1
            srcline = SourceLine(lineno=lineno,
                                 code_segments=[],
                                 messages=[])
            srcline.code_segments.append(CodeSegment(code=lines[ln]))

            while messages and messages[0].line == lineno:
                srcline.messages.append(messages.popleft())

            ret.append(srcline)
        return ret

    def highlight(self, diagnostic):

        def initHighlightInfo(file_path, file_event_map):
            file_event_map[file_path] = HighlightInfo(file_path, [], [])

        def appendMessage(pe, file_event_map):
            if pe.tag == "caretline":
                return
            if pe.description is None:
                return

            file_path = main_file
            if pe.file:
                file_path = pe.file
            if file_path not in file_event_map:
                initHighlightInfo(file_path, file_event_map)
            file_event_map[file_path].messages.append(pe)

        file_event_map = {}
        path = diagnostic.event
        main_file = diagnostic.file
        for pe in path:
            appendMessage(pe, file_event_map)

        for event in file_event_map.values():
            event.messages.sort(key=attrgetter('line'))

        return [self.getSourceLines(e) for e in file_event_map.values()]
