#include "helper/DeclHelper.h"
#include <iostream>

bool DeclHelper::isDeclUnused(NamedDecl *decl) {
  std::cout << "Using DeclUnused\n";
  bool Referenced = false;
  if (decl->isInvalidDecl())
    return false;
  Referenced = false;
  if (auto *DD = dyn_cast<DecompositionDecl>(decl)) {
    for (auto *BD : DD->bindings()) {
      if (BD->isReferenced()) {
        Referenced = true;
        break;
      }
    }
  } else if (!decl->getDeclName()) {
    return false;
  } else if (decl->isReferenced() || decl->isUsed()) {
    Referenced = true;
  }
  return Referenced;
}
