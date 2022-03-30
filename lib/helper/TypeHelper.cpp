#include "helper/TypeHelper.h"

using namespace clang;

bool isObjectPointerType(const Type *t) {
  if (!t->isPointerType()) {
    return false;
  }
  while (t->isPointerType()) {
    t = t->getPointeeType()->getUnqualifiedDesugaredType();
  }
  if (t->isObjectType()) {
    return true;
  }
  return false;
}

bool isNestedFunctionPointerType(const Type *t) {
  t = t->getUnqualifiedDesugaredType();
  if (t->isPointerType()) {
    t = t->getPointeeType()->getUnqualifiedDesugaredType();
    if (t->isFunctionType()) {
      return true;
    }
  }
  return false;
}

bool isSameFunctionType(const FunctionType *fa, const FunctionType *fb) {
  const Type *tmp1, *tmp2;
  if ((!fa) || (!fb)) {
    return false;
  }
  if (fa == fb) {
    return true;
  }

  tmp1 = fa->getReturnType()->getUnqualifiedDesugaredType();
  tmp2 = fb->getReturnType()->getUnqualifiedDesugaredType();

  // Or use self implemented type compare
  if (tmp1 != tmp2) {
    return false;
  }

  const FunctionProtoType *fpa = dyn_cast<FunctionProtoType>(fa);
  const FunctionProtoType *fpb = dyn_cast<FunctionProtoType>(fb);

  if ((!fpa) || (!fpb)) {
    return false;
  }

  int fpa_params = fpa->getNumParams();
  int fpb_params = fpb->getNumParams();

  if (fpa_params != fpb_params) {
    return false;
  }
  for (int i = 0; i < fpa_params; i++) {
    tmp1 = fpa->getParamType(i)->getUnqualifiedDesugaredType();
    tmp2 = fpb->getParamType(i)->getUnqualifiedDesugaredType();
    if (tmp1 != tmp2) {
      return false;
    }
  }
  return true;
}
