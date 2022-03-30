#include "helper/ReportHelper.h"

MisraReport::Issue ReportHelper::CreateIssue(const SourceManager &sm,
                                             const SourceLocation &Loc,
                                             const SourceRange &SR) {
  MisraReport::Issue i;
  i.setloc(FullSourceLoc{Loc, sm});
  i.setBegin(FullSourceLoc{SR.getBegin(), sm});
  i.setEnd(FullSourceLoc{SR.getEnd(), sm});
  i.setMsg("Empty");
  i.setExtMsg("Empty");
  i.setDepth(0);
  i.setKind("event");
  return i;
}

MisraReport::Issue ReportHelper::CreateIssue(const SourceManager &sm,
                                             const SourceLocation &Loc,
                                             const SourceRange &SR,
                                             const std::string Msg) {
  MisraReport::Issue i = CreateIssue(sm, Loc, SR);
  i.setMsg(Msg);
  return i;
}

MisraReport::Issue ReportHelper::CreateIssue(const SourceManager &sm,
                                             const SourceLocation &Loc,
                                             const SourceRange &SR,
                                             const std::string Msg,
                                             const std::string ExtMsg) {
  MisraReport::Issue i = CreateIssue(sm, Loc, SR, Msg);
  i.setExtMsg(ExtMsg);
  return i;
}

/*
    template<typename T>
    static void balabala(const ASTContext *C, const T &token, std::string
   NoRule, std::string kind, MisraReport::MisraBugReport &BR, string
   checkername, string describe)
    {
        auto& sm = C->getSourceManager();
        MisraReport::Issue *issue = ReportHelper::CreateIssue(sm,
   token.getLocStart(), token.getSourceRange()); MisraReport::Diag D;
        issue->setKind(kind);
        D.path.push_back(*issue);
        D.setinfo(describe, "MisraC", NoRule, checkername);
        BR.AddDiag(D);
    }
*/

/*
    static void SimpleBugReport(const ASTContext *C, SourceRange *SR,
   std::string NoRule, std::string Msg, MisraReport::MisraBugReport &BR,
   string checkername, string describe) { SimpleBugReport(C, SR, NoRule, Msg,
   Msg, BR, checkername, describe);
    }
*/
/*
        template <typename T>
        static void SimpleBugReport(const ASTContext *C, T *token, std::string
   NoRule, std::string Msg, MisraReport::MisraBugReport &BR,string
   checkername,string describe) { SimpleBugReport(C, token, NoRule,Msg, Msg,
   BR, checkername, describe);
        }
*/
void ReportHelper::SimpleBugReport(const ASTContext *C, SourceRange SR,
                                   std::string NoRule, std::string Msg,
                                   string ExtMsg,
                                   MisraReport::MisraBugReport &BR,
                                   string checkername, string describe) {
  unsigned ID;
  auto &sm = C->getSourceManager();
  auto &DE = C->getDiagnostics();
  SourceLocation Loc = SR.getBegin();
  const auto Range = clang::CharSourceRange::getCharRange(SR);

  NoRule.insert(9, std::string(" 2008 "));
  Msg.insert(0, NoRule + ": ");

  if (ExtMsg.compare(Msg) && ExtMsg.size() != 0) {
    ID = DE.getCustomDiagID(clang::DiagnosticsEngine::Warning, "%0\nExtMsg:%1");
  } else {
    ID = DE.getCustomDiagID(clang::DiagnosticsEngine::Warning, "%0\n");
  }
  DiagnosticBuilder DB = DE.Report(Loc, ID);
  DB.AddString(Msg);
  DB.AddString(ExtMsg);
  DB.AddSourceRange(Range);

  MisraReport::Diag D;
  MisraReport::Issue issue = CreateIssue(sm, Loc, SR, Msg, ExtMsg);
  D.path.push_back(issue);

  { // TODO record all macro path, now we only report deepest
    SourceLocation loc = Loc;
    while (loc.isMacroID()) {
      loc = sm.getSpellingLoc(loc);
      SourceRange sr(loc);
      // push issue with macro loc and empty msg
      MisraReport::Issue issue = CreateIssue(sm, loc, sr, string(), string());
      issue.setKind("macro");
      D.path.push_back(issue);
    }
  }

  D.setinfo(describe, "MisraC", NoRule, checkername);
  BR.AddDiag(D);
}

void ReportHelper::SimpleBugReportonMacro(const ASTContext *C, SourceRange SR,
                                          std::string NoRule, std::string Msg,
                                          string ExtMsg,
                                          MisraReport::MisraBugReport &BR,
                                          string checkername, string describe) {
  unsigned ID;
  auto &sm = C->getSourceManager();
  auto &DE = C->getDiagnostics();
  SourceLocation Loc = SR.getBegin();
  const auto Range = clang::CharSourceRange::getCharRange(SR);

  NoRule.insert(9, std::string(" 2008 "));
  Msg.insert(0, NoRule + ": ");

  while (Loc.isMacroID()) {
    if (sm.getImmediateSpellingLoc(Loc).isValid()) {
      Loc = sm.getImmediateSpellingLoc(Loc);
      SR = sm.getExpansionRange(Loc).getAsRange();
    }
  }

  if (ExtMsg.compare(Msg) && ExtMsg.size() != 0) {
    ID = DE.getCustomDiagID(clang::DiagnosticsEngine::Warning, "%0\nExtMsg:%1");
  } else {
    ID = DE.getCustomDiagID(clang::DiagnosticsEngine::Warning, "%0\n");
  }
  DiagnosticBuilder DB = DE.Report(Loc, ID);
  DB.AddString(Msg);
  DB.AddString(ExtMsg);
  DB.AddSourceRange(Range);

  MisraReport::Issue issue = CreateIssue(sm, Loc, SR, Msg, ExtMsg);
  MisraReport::Diag D;
  D.path.push_back(issue);
  D.setinfo(describe, "MisraC", NoRule, checkername);
  BR.AddDiag(D);
}

/*

template <typename T>
static void SimpleBugReportWithMacro(const ASTContext *C, T *token,
                                     std::string NoRule, std::string Msg,
                                     string ExtMsg,
                                     MisraReport::MisraBugReport &BR,
                                     string checkername, string describe) {
  auto &sm = C->getSourceManager();
  auto &DE = C->getDiagnostics();
  SourceLocation Loc = token->getLocStart();
  SourceRange SR = token->getSourceRange();

  const unsigned ID =
      DE.getCustomDiagID(clang::DiagnosticsEngine::Warning, "%0");
  clang::DiagnosticBuilder DB = DE.Report(Loc, ID);
  DB.AddString(Msg);
  const auto Range = clang::CharSourceRange::getCharRange(SR);
  DB.AddSourceRange(Range);

  // Try to find Macro
  std::vector<std::pair<const SourceLocation, const SourceRange>> MacroPath;
  while (Loc.isMacroID()) {
    MacroPath.push_back(
        std::make_pair(sm.getImmediateSpellingLoc(Loc),
                       sm.getImmediateExpansionRange(Loc).getAsRange()));
    if (sm.getImmediateSpellingLoc(Loc).isValid()) {
      Loc = sm.getImmediateSpellingLoc(Loc);
      SR = sm.getExpansionRange(Loc).getAsRange();
    }
  }

  MisraReport::Issue *issue = CreateIssue(sm, Loc, SR, Msg, ExtMsg);

  MisraReport::Diag D;
  D.path.push_back(*issue);
  D.setinfo(describe, "MisraC", NoRule, checkername);
  BR.AddDiag(D);
}
 Old Reporter
  template <typename T>
  static void SimpleBugReport(const SourceManager &sm, T *token, std::string
  NoRule, std::string Msg, BugReporter &BR, AnalysisDeclContext *AC, const
  CheckerBase *Ch) { SourceRange SR = token->getSourceRange();
    PathDiagnosticLocation ELoc =
        PathDiagnosticLocation(token->getLocStart(), BR.getSourceManager());
    Msg.insert(0, " ");
    Msg.insert(0, NoRule);
    Msg.insert(7, ":2012");
    BR.EmitBasicReport(AC->getDecl(), Ch, NoRule, "MisraC", Msg, ELoc, SR);
  }

  static void SimpleBugReport(const SourceManager &sm, SourceRange SR,
      std::string NoRule, std::string Msg,
      BugReporter &BR, AnalysisDeclContext *AC,const CheckerBase *Ch) {
      PathDiagnosticLocation ELoc =
          PathDiagnosticLocation(SR.getBegin(), BR.getSourceManager());
      BR.EmitBasicReport(AC->getDecl(), Ch, NoRule, "MisraC", Msg, ELoc, SR);
  }
*/

// BugReport for Static Analyzer checkers
void ReportHelper::CheckerBugReport(CheckerContext &C,
                                    const CheckerBase *checker, SourceRange SR,
                                    std::string NoRule, std::string Msg) {

  BugReporter &BR = C.getBugReporter();
  AnalysisDeclContext *AC = C.getCurrentAnalysisDeclContext();
  PathDiagnosticLocation ELoc =
      PathDiagnosticLocation(SR.getBegin(), BR.getSourceManager());
  NoRule.insert(9, std::string(" 2008 "));
  Msg.insert(0, NoRule + ": ");
  BR.EmitBasicReport(AC->getDecl(), checker, NoRule, "MisraC", Msg, ELoc, SR);
}
