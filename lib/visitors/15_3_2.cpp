#include "MisraVisitor.hpp"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"

static char err_kind[] = "Misra CPP Rule 15-3-2";
static char rule_desc[] = "There should be at least one exception handler to catch"
                          " all otherwise unhandled exceptions.";

class Rule_15_3_2 : public RecursiveASTVisitor<Rule_15_3_2>, public MisraVisitor {
public:
  using MisraVisitor::MisraVisitor;
  bool VisitFunctionDecl(FunctionDecl *D);
  bool VisitCXXTryStmt(CXXTryStmt *TS);

private:
  bool isCatchAll = false;
  

};
bool Rule_15_3_2::VisitFunctionDecl(FunctionDecl *D){
  if(D->isMain()){
    if (dyn_cast_or_null<Stmt>(D->getBody()) != nullptr){
      if (dyn_cast_or_null<CXXTryStmt>(*(D->getBody()->child_begin())) == nullptr) {
        SimpleBugReport(D, err_kind, rule_desc);
      }
    }
  }
  return true;
}
bool Rule_15_3_2::VisitCXXTryStmt(CXXTryStmt *TS) {

  unsigned hn = TS->getNumHandlers();

  for(unsigned c = 0 ; c < hn ; c++){
  
    auto CS = TS->getHandler(c);
    // CS->dumpColor();

    if(CS->getExceptionDecl() == nullptr){
      isCatchAll = true; 
    }

  }

  if(!isCatchAll){
      SimpleBugReport(TS, err_kind, rule_desc);
  }

  return true;
}


REGISTER_VISITOR_CHECKER(Rule_15_3_2, "MisraCPP.15_3_2", rule_desc)
