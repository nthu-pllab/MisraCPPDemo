#include "MisraVisitor.hpp"
#include "clang/AST/ASTContext.h"
#include "clang/AST/CXXInheritance.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

static char err_kind[] = "Misra CPP Rule 10-1-2";
static char err_desc[] = "A base class shall only be declared virtual if it is "
                         "used in a diamond hierarchy.";

class Rule_10_1_2 : public RecursiveASTVisitor<Rule_10_1_2>,
                    public MisraVisitor {
public:
  using MisraVisitor::MisraVisitor;
  bool VisitCXXRecordDecl(CXXRecordDecl *RD);
  bool TraverseTranslationUnitDecl(TranslationUnitDecl *TUD);

private:
  vector<CXXRecordDecl *> record_list; // For highest hierarchy base class
  vector<CXXRecordDecl *> error_list;
  vector<CXXRecordDecl *> temp_list;
};

bool Rule_10_1_2::TraverseTranslationUnitDecl(TranslationUnitDecl *TUD) {
  /*for (auto &&i : TUD->decls()) {
    if (auto *t = dyn_cast<CXXRecordDecl>(i))
      Rule_10_1_2::CheckCXXRecordDecl(t);
  }*/
  RecursiveASTVisitor<Rule_10_1_2>::TraverseTranslationUnitDecl(TUD);
  // cout << "After traverse" << endl;
  for (auto k = error_list.begin(); k != error_list.end(); ++k) {
    SourceRange sr((*k)->getLocation());
    SimpleBugReport(&sr, err_kind, err_desc);
  }
  record_list.clear();
  error_list.clear();
  temp_list.clear();
  return true;
}

bool Rule_10_1_2::VisitCXXRecordDecl(CXXRecordDecl *RD) {
  // cout << RD->getDeclName().getAsString() << endl;
  if (!RD->hasDefinition())
    return true;
  /*if(RD->getDeclName().getAsString().compare("zap_client_t") == 0)*/
  // cout << "has definition" << endl;
  CXXBasePaths Paths(true, true, true);
  for (auto i : RD->bases()) {
    if (i.isVirtual()) {
      auto *t = i.getType().getTypePtrOrNull();
      if (t == NULL)
        return true;
      auto *RD_V = t->getAsCXXRecordDecl();
      if(RD_V != NULL) {
        if (all_of(record_list.begin(), record_list.end(),
                   [RD_V](CXXRecordDecl *RD_L) { return RD_L != RD_V; })) {
          record_list.push_back(RD_V);
        }
        if (all_of(error_list.begin(), error_list.end(),
                   [RD](CXXRecordDecl *RD_L) { return RD_L != RD; })) {
          error_list.push_back(RD);
        }
      }
    }
  }
  /* for (auto i = record_list.begin(); i != record_list.end(); ++i) {
    if ((*i) == NULL) {
      cout << "NULL record_list instance" << endl;
    }
  } */
  for (auto i = record_list.begin(); i != record_list.end(); ++i) {
    bool Derived = false;
    if ((*i) != NULL) {
      Derived = RD->isDerivedFrom((*i), Paths);
    }
    if (Derived) {
      temp_list.clear();
      for (auto Path : Paths) {
        // cout << Path.size() << endl;
        if (Path.size() == 2) {
          for (auto element : Path) {
            if (((*i) == element.Base->getType()->getAsCXXRecordDecl()) &&
                (element.SubobjectNumber == 0)) {
              temp_list.push_back(const_cast<CXXRecordDecl *>(element.Class));
            }
          }
        }
      }
      if (temp_list.size() >= 2) {
        for (auto j = temp_list.begin(); j != temp_list.end(); ++j) {
          auto erase = find(error_list.begin(), error_list.end(), *j);
          if (erase != error_list.end())
            error_list.erase(erase);
        }
      }
    }
  }
  return true;
}

REGISTER_VISITOR_CHECKER(Rule_10_1_2, "MisraCPP.10_1_2", err_desc)
