#ifndef MISRA_HELPER_SRCHELPER_H_
#define MISRA_HELPER_SRCHELPER_H_

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

#include <string>
#include <vector>

using namespace clang;

class SrcHelper {
public:
  static std::vector<Token> &getTokens(ASTContext *Context, Preprocessor *PP,
                                       SourceRange SR);

  template <typename T>
  static StringRef getToken(ASTContext *Context, T *token) {
    const SourceManager &sm = Context->getSourceManager();
    const LangOptions lopt = Context->getLangOpts();
    SmallVector<char, 25> buffer;
    return Lexer::getSpelling(sm.getSpellingLoc(token->getLocStart()), buffer,
                              sm, lopt, nullptr);
  }

  template <typename T>
  static uint32_t getNodeLine(ASTContext *Context, T node) {
    const SourceManager &sm = Context->getSourceManager();
    return sm.getSpellingLineNumber(node.getLocStart());
  }

  template <typename T>
  static const T *getParent(ASTContext *Context, const Stmt *ST) {
    const auto &parents = Context->getParents(*ST);
    const T *retST = parents[0].get<T>();
    return retST;
  }

  static bool isBefore(ASTContext *Context, const SourceLocation a,
                       const SourceLocation b);

  static std::string getMainFileName(const CompilerInstance &CI);

  static tok::PPKeywordKind getPPKeywordID(const Token);
};     // end of class SrcHelper
#endif // MISRA_HELPER_SRCHELPER_H_
