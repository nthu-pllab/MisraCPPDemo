#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include "MisraVisitor.hpp"

using namespace clang;

static char err_kind[] = "Misra CPP Rule 5-0-10";
static char rule_desc[] =
    "If the bitwise operators ~ and << are applied to an operand with an "
    "underlying type of unsigned char or unsigned short, the result shall be "
    "immediately cast to the underlying type of the operand";

/*
 * visit ~ and << operator
 *    1. operand is small integer type
 *    2. parent node not cast back to underlying type
 */

class Rule_5_0_10 : public RecursiveASTVisitor<Rule_5_0_10>,
                    public MisraVisitor {
public:
  using MisraVisitor::MisraVisitor;
  bool VisitUnaryOperator(UnaryOperator *UO);
  bool VisitBinaryOperator(BinaryOperator *BO);

private:
  void checkRule(const Expr *op, const Expr *operand);
  bool isSmallIntegerType(const Expr *expr);
  bool isTemplateType(const Expr *expr);
  bool sameTypeWidth(const Expr *A, const Expr *B);
  const ExplicitCastExpr *findParentECE(const Stmt *cur);
};

bool Rule_5_0_10::VisitUnaryOperator(UnaryOperator *UO) {
  if (sm->isInSystemHeader(UO->getExprLoc()))
    return true;

  if (UO->getOpcode() == UO_Not) {
    auto *expr = UO->getSubExpr()->IgnoreParenImpCasts();
    checkRule(UO, expr);
  }

  return true;
}

bool Rule_5_0_10::VisitBinaryOperator(BinaryOperator *BO) {
  if (sm->isInSystemHeader(BO->getExprLoc())) {
    return true;
  }

  if (BO->getOpcode() != BO_Shl) {
    return true;
  }

  BinaryOperator *cur = BO;
  while (true) {
    if (auto *lhs = dyn_cast_or_null<BinaryOperator>(cur->getLHS())) {
      if (lhs->getOpcode() == BO_Shl ) {
        cur = lhs;
      }
    } else if (auto *OCE = dyn_cast_or_null<CXXOperatorCallExpr>(cur->getLHS())) {
      if (OCE->getOperator() == OO_LessLess) {
        return true;
      }
    } else {
      break;
    }
  }

  auto *expr = BO->getLHS()->IgnoreParenImpCasts();
  checkRule(BO, expr);

  return true;
}

void Rule_5_0_10::checkRule(const Expr *op, const Expr *operand) {
  if (isSmallIntegerType(operand)) {
    const auto *ECE = findParentECE(op);
    if (ECE && isSmallIntegerType(ECE) && sameTypeWidth(operand, ECE))
      return;
    SimpleBugReport(op, err_kind, rule_desc);
  }

  if (isTemplateType(operand)) {
    const auto *ECE = findParentECE(op);
    if (ECE) {
      if (isTemplateType(ECE) || isSmallIntegerType(ECE)) {
        return;
      }
    }
    SimpleBugReport(op, err_kind, rule_desc);
  }
}

bool Rule_5_0_10::isSmallIntegerType(const Expr *expr) {
  // small integer type: unsigned char or unsigned short
  auto Type = expr->getType().getTypePtrOrNull();
  if (Type == nullptr)
    return false;

  if (!Type->isIntegerType())
    return false;

  auto Width = Context->getTypeInfo(Type).Width;
  auto Signed = Type->isSignedIntegerType();

  return (Signed == false) && (Width == 8 || Width == 16);
}

bool Rule_5_0_10::isTemplateType(const Expr *expr) {
  auto Type = expr->getType().getTypePtrOrNull();
  if (Type == nullptr)
    return false;

  return Type->isInstantiationDependentType();
}

bool Rule_5_0_10::sameTypeWidth(const Expr *A, const Expr *B) {
  auto WidthA = Context->getTypeInfo(A->getType()).Width;
  auto WidthB = Context->getTypeInfo(B->getType()).Width;
  return WidthA == WidthB;
}

const ExplicitCastExpr *Rule_5_0_10::findParentECE(const Stmt *cur) {
  while (true) {
    const auto &parents = Context->getParents(*cur);
    if (parents.empty())
      break;

    cur = parents[0].get<Stmt>();
    if (cur) {
      if (isa<ImplicitCastExpr>(cur)) {
        continue;
      } else if (auto *ECE = dyn_cast<ExplicitCastExpr>(cur)) {
        return ECE;
      }
    }
    break;
  }
  return nullptr;
}

REGISTER_VISITOR_CHECKER(Rule_5_0_10, "MisraCPP.5_0_10", rule_desc)
