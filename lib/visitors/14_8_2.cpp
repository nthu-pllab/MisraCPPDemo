#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

#include "MisraVisitor.hpp"

using namespace clang;
using namespace llvm;

static char err_kind[] = "Misra CPP Rule 14-8-2";
static char rule_desc[] =
    "The viable function set for a function call should either contain no "
    "function specializations, or only contain function specializations.";

class Rule_14_8_2 : public RecursiveASTVisitor<Rule_14_8_2>,
                    public MisraVisitor {
public:
  using MisraVisitor::MisraVisitor;
  bool VisitCallExpr(CallExpr *CE);
};

bool Rule_14_8_2::VisitCallExpr(CallExpr *CE) {
  if (sm->isInSystemHeader(CE->getExprLoc()))
    return true;
  if (auto OCE = dyn_cast_or_null<CXXOperatorCallExpr>(CE)) {
    // operator overloading naturally with function template and function
    // declarations and normally without ::<...>
    return true;
  }

  auto *Callee = CE->getCallee();
  if (Callee == nullptr)
    return true;
  Callee = Callee->IgnoreParenImpCasts();
  if (auto *DRE = dyn_cast_or_null<DeclRefExpr>(Callee)) {
    auto *D = DRE->getDecl();
    if (auto *FD = dyn_cast_or_null<FunctionDecl>(D)) {
      if (DRE->hasExplicitTemplateArgs())
        return true;

      bool hasNormalFunc = false;
      bool hasSpecializedFunc = false;
      DeclarationName DN = FD->getDeclName();
      DeclContext *LookupParent = FD->getLookupParent();
      if (LookupParent == nullptr || LookupParent->isTransparentContext())
        return true;
      for (NamedDecl *ND : LookupParent->lookup(DN)) {
        if (ND == nullptr)
          continue;
        if (sm->isInSystemHeader(ND->getLocation())) {
          return true;
        }
        hasNormalFunc |= isa<FunctionDecl>(ND);
        hasSpecializedFunc |= isa<FunctionTemplateDecl>(ND);
      }

      if (hasSpecializedFunc == false || hasNormalFunc == false)
        return true;
      SimpleBugReport(CE, err_kind, rule_desc);
    }
  } else if (auto *ULE = dyn_cast_or_null<UnresolvedLookupExpr>(Callee)) {
    bool hasNormalFunc = false;
    bool hasSpecializedFunc = false;
    for (NamedDecl *ND : ULE->decls()) {
      if (ND == nullptr)
        continue;
      if (sm->isInSystemHeader(ND->getLocation())) {
        return true;
      }
      hasNormalFunc |= isa<FunctionDecl>(ND);
      hasSpecializedFunc |= isa<FunctionTemplateDecl>(ND);
    }
    if (hasSpecializedFunc == false || hasNormalFunc == false)
      return true;
    SimpleBugReport(CE, err_kind, rule_desc);
  }
  return true;
}

REGISTER_VISITOR_CHECKER(Rule_14_8_2, "MisraCPP.14_8_2", rule_desc)
