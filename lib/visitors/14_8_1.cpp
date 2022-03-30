#include "MisraVisitor.hpp"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/TemplateBase.h"

static char err_kind[] = "Misra CPP Rule 14-8-1";
static char err_desc[] =
    "Overloaded function templates shall not explicitly specialized";

class Rule_14_8_1 : public RecursiveASTVisitor<Rule_14_8_1>,
                    public MisraVisitor {
public:
  using MisraVisitor::MisraVisitor;
  bool VisitFunctionTemplateDecl(FunctionTemplateDecl *FTD);
};

bool Rule_14_8_1::VisitFunctionTemplateDecl(FunctionTemplateDecl *FTD) {
  // if (!sm->isInMainFile(FTD->getLocation()))
    // return true;

  auto *DC = FTD->getDeclContext();
  if(DC->isTransparentContext())
    return true;

  auto look_result = DC->lookup(FTD->getDeclName()); // look up for overloaded set
  int size = 0;
  for(auto &&i : look_result) {
    if(isa<FunctionTemplateDecl>(*i))
      size++;
  }

  // if (look_result.size() > 1) {
  if (size > 1) {
    for (auto i = FTD->spec_begin(); i != FTD->spec_end(); i++) { // iterate through specialization
      if (i->isTemplateInstantiation()) { // check that not explicitly specialized
        continue;
      }
      SourceRange sr(i->getLocation());
      SimpleBugReport(&sr, err_kind, err_desc);
    }
  }
  return true;
}

REGISTER_VISITOR_CHECKER(Rule_14_8_1, "MisraCPP.14_8_1", "This is a sample")
