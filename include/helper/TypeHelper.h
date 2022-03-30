#ifndef __MISRA_TYPE_HELPER__
#define __MISRA_TYPE_HELPER__

#include "clang/AST/Type.h"

bool isObjectPointerType(const clang::Type *t);
bool isSameFunctionType(const clang::FunctionType *a,
                        const clang::FunctionType *b);
bool isNestedFunctionPointerType(const clang::Type *t);
#endif
