#include "MisraVisitor.hpp"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"

static char err_kind[] = "Misra C++ Rule 3-9-3";
static char rule_desc[] = "The underlying bit representations of floating-point"
                          " values shall not be used";

class Rule_3_9_3 : public RecursiveASTVisitor<Rule_3_9_3>, public MisraVisitor {
public:
  using MisraVisitor::MisraVisitor;
  bool VisitCastExpr(CastExpr *rce);
};

bool Rule_3_9_3::VisitCastExpr(CastExpr *rce){
  std::string CastName = rce->clang::CastExpr::getCastKindName();
  std::string subCastName = "None";
  std::string type1 = rce->getType().getAsString();
  if(auto tmp = dyn_cast_or_null<ImplicitCastExpr>(rce->clang::CastExpr::getSubExpr())){
    subCastName = tmp->clang::CastExpr::getCastKindName();
  }

  if(CastName.find("BitCast") != std::string::npos){
    std::string type = rce->clang::CastExpr::getSubExpr()->IgnoreImpCasts()->getType().getAsString();
    if(subCastName.find("LValueToRValue") != std::string::npos) return true;

    if(type.find("float") != std::string::npos || type.find("double") != std::string::npos){
      SimpleBugReport(rce, err_kind, rule_desc, type1 + " and after is " + type + ", " + subCastName);
    }
  }
  return true;
}

REGISTER_VISITOR_CHECKER(Rule_3_9_3, "MisraCPP.3_9_3", rule_desc)
