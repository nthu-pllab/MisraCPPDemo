#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugReporter.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/BugReporter/PathDiagnostic.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/Tooling/Tooling.h"

//#include "MisraVisitor.hpp"
#include "Reporter.hpp"
#include <iostream>
#include <string>

using namespace clang;
using namespace ento;

class ReportHelper {
public:
  static MisraReport::Issue CreateIssue(const SourceManager &sm,
                                        const SourceLocation &Loc,
                                        const SourceRange &SR);
  static MisraReport::Issue CreateIssue(const SourceManager &sm,
                                        const SourceLocation &Loc,
                                        const SourceRange &SR,
                                        const std::string Msg);
  static MisraReport::Issue CreateIssue(const SourceManager &sm,
                                        const SourceLocation &Loc,
                                        const SourceRange &SR,
                                        const std::string Msg,
                                        const std::string ExtMsg);
  static void SimpleBugReport(const ASTContext *C, SourceRange SR,
                              std::string NoRule, std::string Msg,
                              string ExtMsg, MisraReport::MisraBugReport &BR,
                              string checkername, string describe);
  static void CheckerBugReport(CheckerContext &C, const CheckerBase *checker,
                               SourceRange SR, std::string NoRule,
                               std::string Msg);
  static void SimpleBugReportonMacro(const ASTContext *C, SourceRange SR,
                                     std::string NoRule, std::string Msg,
                                     string ExtMsg,
                                     MisraReport::MisraBugReport &BR,
                                     string checkername, string describe);

}; // end of class ReportHelper
