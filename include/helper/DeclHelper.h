#ifndef MISRA_HELPER_DECLHELPER
#define MISRA_HELPER_DECLHELPER

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/BugReporter/PathDiagnostic.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;

class DeclHelper {
public:
  static bool isDeclUnused(NamedDecl *decl);
};
#endif // MISRA_HELPER_DECLHELPER
