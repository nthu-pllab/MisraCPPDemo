#ifndef MISRA_VISITOR_MISRACHECKER_H_
#define MISRA_VISITOR_MISRACHECKER_H_
#include "Rule_desc.def"
#include "plugin_registry.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugReporter.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/BugReporter/PathDiagnostic.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"

#include "Reporter.hpp"
#include "helper/ReportHelper.h"
#include "helper/SrcHelper.h"

#include "DebugInfo.h"

#include <iostream>

using namespace clang;
using namespace ento;

class MisraVisitor {

  //====================Don't Put any function or varible decl in this
  // block===================
public:
  explicit MisraVisitor(ASTContext *Context, MisraReport::MisraBugReport &mbr)
      : MBR(&mbr), Context(Context), Debug(Context) {
    kind = Plugin;
    sm = &Context->getSourceManager();
  }

  explicit MisraVisitor(ASTContext *Context)
      : Context(Context), Debug(Context) {
    kind = SubVisitor;
    sm = &Context->getSourceManager();
  }

  void setCheckerInfo(string name, string desc) {
    checkname = name;
    describe = desc;
  }
  void setPreprocessor(Preprocessor *Pp) { PP = Pp; }

  virtual void handlePre(){};
  virtual void handlePost(){};
  virtual void Init(){};
  //=============================================================================================
private:
  enum Kind { Plugin, SubVisitor } kind;
  const CheckerBase *Ch;
  BugReporter *BR;
  MisraReport::MisraBugReport *MBR;

  string checkname;
  string describe;

protected:
  ASTContext *Context;
  Preprocessor *PP;
  SourceManager *sm;

  //================================Add Useful function at
  // here=================================

public:
  DebugInfo Debug;

  // getToken will get source code of token directly
  template <typename T> StringRef getToken(T *token) {
    return SrcHelper::getToken(Context, token);
  }
  /*
    void SimpleBugReport(const Token &token, std::string NoRule, std::string
    Msg) { SimpleBugReport(token, NoRule, Msg, Msg);
    }

    template <typename T>
    void SimpleBugReport(T *token, std::string NoRule, std::string Msg) {
      SimpleBugReport(token, NoRule, Msg, Msg);
    }

    template <typename T>
    void SimpleBugReportWithMacro(T *token, std::string NoRule, std::string Msg)
    { SimpleBugReportWithMacro(token, NoRule, Msg, Msg);
    }
  */

  // With Token
  void SimpleBugReport(const Token &token, std::string NoRule, std::string Msg,
                       std::string ExtMsg = std::string()) {
    SourceRange SR(token.getLocation(), token.getEndLoc());
    ReportHelper::SimpleBugReport(Context, SR, NoRule, Msg, ExtMsg, *MBR,
                                  checkname, describe);
  }

  // With AST Node
  template <typename T>
  void SimpleBugReport(T *token, std::string NoRule, std::string Msg,
                       std::string ExtMsg = std::string()) {
    ReportHelper::SimpleBugReport(Context, token->getSourceRange(), NoRule, Msg,
                                  ExtMsg, *MBR, checkname, describe);
  }

  // With SourceRange
  void SimpleBugReport(SourceRange *SR, std::string NoRule, std::string Msg,
                       std::string ExtMsg = std::string()) {
    if (ExtMsg.size() == 0) {
      ExtMsg = Msg;
    }
    ReportHelper::SimpleBugReport(Context, *SR, NoRule, Msg, ExtMsg, *MBR,
                                  checkname, describe);
  }

  // Report on Macro
  template <typename T>
  void SimpleBugReportWithMacro(T *token, std::string NoRule, std::string Msg,
                                std::string ExtMsg = std::string()) {
    // ReportHelper::SimpleBugReportWithMacro(Context, token, NoRule, Msg,
    // ExtMsg,
    ReportHelper::SimpleBugReportonMacro(Context, token->getSourceRange(),
                                         NoRule, Msg, ExtMsg, *MBR, checkname,
                                         describe);
  }

  template <typename T> std::vector<Token> lexTokens(T decl) {
    return SrcHelper::getTokens(Context, PP, decl->getSourceRange());
  }

  void SourceRangeBugReport(SourceRange sr, std::string NoRule,
                            std::string Msg) {
    // if (kind == Analyzer)
    ReportHelper::SimpleBugReport(Context, sr, NoRule, Msg, Msg, *MBR,
                                  checkname, describe);
  }

  // Return true if a is before b in source line of same file
  bool isBefore(const SourceLocation a, const SourceLocation b) {
    BeforeThanCompare<SourceLocation> compare(*sm);
    return compare(a, b);
  }

  // TODO: remove this getter when SimpleBugReport for PPCallBacks is refined
  MisraReport::MisraBugReport *getMisraBugReport() { return MBR; }
};

#endif // MISRA_VISITOR_MISRACHECKER_H_
