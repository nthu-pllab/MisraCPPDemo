import os
from abc import ABC
from abc import abstractmethod
from operator import attrgetter

from libmisrascan import MisraNamedTuple
from reportutils import SourceLocation
from reportutils import PathDiagnosticPiece
from reportutils import PathDiagnostic


class IllegalArgumentError(Exception):
    pass


class ReportFilterManager:

    _filters = []

    @classmethod
    def register(_cls, filter_cls):
        if not issubclass(filter_cls, ReportFilterBase):
            raise IllegalArgumentError(
                "'%s' is not a subclass of ReportFilterBase" % filter_cls)
        try:
            filter_cls()
        except TypeError:
            raise NotImplementedError(
                "'%s' does not implement generateFilterMask" % filter_cls)

        _cls._filters.append(filter_cls)

    def filters(self):
        for filter_class in self._filters:
            yield filter_class()


class ReportFilterBase(ABC):
    """
    Base class of any report filter class.
    """


    @abstractmethod
    def generateFilterMask(self, diagnostics):
        """

        Generate a boolean list to determine each element in diagonostics
        should be outputed to report.

        Returns
        -------
        filter_mask: list
            A list contains boolean values. True for keeping corresponding
            diagnostic, False for removing it from the report.

        Parameters
        ----------
        diagnostics : list
            A list contains instances of PathDiagnostic. The elements in 
            the list are not sorted by specific attributes. After calling
            this method, diagnostics might be modified by being sorted with
            diagnostics.sort(key=key). However, each element in diagnostics
            is not modified.

        See Also
        --------
        reportutils.PathDiagnostic

        """

        pass


class FileMissingFilter(ReportFilterBase):

    def generateFilterMask(self, diagnostics):

        def isAlive(file_path):
            if file_path in records:
                return records[file_path]

            ret = os.path.exists(file_path)
            records[file_path] = ret
            return ret

        records = {}
        return [isAlive(diag.location.file) for diag in diagnostics]


class MisraCCategoryFilter(ReportFilterBase):

    def generateFilterMask(self, diagnostics):
        return [diag.category == "MisraC" for diag in diagnostics]


class UseSystemMacroFilter(ReportFilterBase):

    Rules = [
             "7.4",
             "9.1", "9.3",
             "10.1", "10.2", "10.3", "10.4", "10.5",
             "11.1", "11.2", "11.3", "11.4", "11.5", "11.6", "11.7", "11.8",
             "12.2", "12.3", "12.4", "12.5",
             "13.4",
             "15.1", "15.2", "15.3", "15.4", "15.5", "15.6", "15.7",
             "16.2", "16.3", "16.4", "16.5", "16.6", "16.7",
             "17.1", "17.3",
             "18.4", "19.2",
             "21.1", "21.2", "21.3", "21.4", "21.5", "21.6", "21.7", "21.8", "21.9", "21.10", "21.11",
             "21.12"]

    def generateFilterMask(self, diagnostics):

        def containSystemMacro(self, diag):
            # Apply to only specific rules
            type_name = diag.type.split(" ")[-1]
            if type_name not in self.Rules:
                return True

            for path in diag.path:
                if path.kind == "macro":
                    rel_path = os.path.relpath(path.location.file)
                    if rel_path.startswith('..'):
                        return False
            return True

        return [containSystemMacro(self, diag) for diag in diagnostics]


class R2_3Filter(ReportFilterBase):

    DeclInfo = MisraNamedTuple(
        'DeclInfo',
        field_names=['file',
                     'decl_name',
                     'referenced',
                     'diag_index'],
        default_type={
            'diag_index': int
        })

    def generateFilterMask(self, diagnostics):
        mask = [True] * len(diagnostics)

        enum_involved_diags = ((index, diag) for index, diag in enumerate(diagnostics) if
                               diag.type.split(' ')[-1] == '2.3')
        decl_infos = []
        for index, diag in enum_involved_diags:
            decl_name, referenced = diag.path[0].extended_message.split(' ')
            decl_infos.append(self.DeclInfo(diag.location.file,
                                            decl_name,
                                            referenced,
                                            index))

        decl_infos.sort(key=attrgetter('file',
                                       'decl_name',
                                       'referenced',
                                       'diag_index'))

        for index, info in enumerate(decl_infos):
            if index == 0:
                mask[info.diag_index] = info.referenced == 'N'
            elif info.referenced == 'Y':
                mask[info.diag_index] = False
                prev_info = decl_infos[index - 1]
                if info.decl_name == prev_info.decl_name and\
                   info.file == prev_info.file:
                    mask[prev_info.diag_index] = False

        return mask


ReportFilterManager.register(FileMissingFilter)
ReportFilterManager.register(MisraCCategoryFilter)
ReportFilterManager.register(UseSystemMacroFilter)
ReportFilterManager.register(R2_3Filter)
