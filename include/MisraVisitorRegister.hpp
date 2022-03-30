#include "clang/StaticAnalyzer/Core/BugReporter/BugReporter.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"

// TODO REMOVE Static Analyzer Header
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/BugReporter/PathDiagnostic.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"

#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"

#include "DebugInfo.h"
#include "Reporter.hpp"

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>

using namespace clang;
// TODO REMOVE Static Analyzer Header
using namespace ento;

template <typename T> using is_PPCallbacks = std::is_base_of<PPCallbacks, T>;

template <typename T>
using is_visitor = std::is_base_of<RecursiveASTVisitor<T>, T>;

typedef struct {
} FullVisitor;

template <typename T>
using WhatKind = typename std::conditional<
    is_PPCallbacks<T>::value && is_visitor<T>::value, FullVisitor,
    typename std::conditional<
        is_PPCallbacks<T>::value, PPCallbacks,
        typename std::conditional<is_visitor<T>::value, RecursiveASTVisitor<T>,
                                  std::false_type>::type>::type>::type;

// clang plugin register
class Misrabase {
protected:
  string checkername;
  string describe;

  Preprocessor *PP;

public:
  virtual void runChecker(ASTContext &Context) = 0;
  virtual void Init(ASTContext &Context, MisraReport::MisraBugReport &mbr) = 0;
  virtual void regPPCallbacks(CompilerInstance &CI) {}
  virtual void setDebugLoc(DebugLoc debug) {}

  void setCheckerInfo(string name, string desc) {
    checkername = name;
    describe = desc;
  }
};

template <typename VisitorClass, typename Kind>
class Plugin_VisitorChecker : public Misrabase {

private:
  std::unique_ptr<VisitorClass> Visitor;
  VisitorClass *V;

public:
  void Init(ASTContext &Context, MisraReport::MisraBugReport &mbr) override {
    std::cout << "null class\n";
  }
  void runChecker(ASTContext &Context) override { std::cout << "Do Nothing\n"; }
};

//特化1
template <class VisitorClass>
class Plugin_VisitorChecker<VisitorClass, PPCallbacks> : public Misrabase {
private:
  std::unique_ptr<VisitorClass> Visitor;
  VisitorClass *V;

public:
  void Init(ASTContext &Context, MisraReport::MisraBugReport &mbr) override {
    std::cout << "PPCallbacks only\n";
    Visitor = std::move(
        std::unique_ptr<VisitorClass>(new VisitorClass(&Context, mbr)));
    Visitor->Init();
  }
  void runChecker(ASTContext &Context) override {}

  void regPPCallbacks(CompilerInstance &CI) override {
    Visitor->setPreprocessor(&CI.getPreprocessor());
    CI.getPreprocessor().addPPCallbacks(std::move(Visitor));
  }
};

//特化2
template <class VisitorClass> // VisitorClass Should be MisraVisitor
class Plugin_VisitorChecker<VisitorClass, FullVisitor> : public Misrabase {
private:
  std::unique_ptr<VisitorClass> Visitor;
  VisitorClass *V;

public:
  void Init(ASTContext &Context, MisraReport::MisraBugReport &mbr) override {
    Visitor = std::move(
        std::unique_ptr<VisitorClass>(new VisitorClass(&Context, mbr)));
    Visitor->Init();
    V = Visitor.get();
  }
  void runChecker(ASTContext &Context) override {
    V->handlePre();
    V->TraverseDecl(Context.getTranslationUnitDecl());
    V->handlePost();
  }

  void setDebugLoc(DebugLoc debug) override { V->Debug.debugloc = debug; }

  void regPPCallbacks(CompilerInstance &CI) override {
    Visitor->setPreprocessor(&CI.getPreprocessor());
    CI.getPreprocessor().addPPCallbacks(std::move(Visitor));
  }
};

//特化3
template <class VisitorClass>
class Plugin_VisitorChecker<VisitorClass, RecursiveASTVisitor<VisitorClass>>
    : public Misrabase {
private:
  VisitorClass *Visitor;

public:
  void Init(ASTContext &Context, MisraReport::MisraBugReport &mbr) override {
    Visitor = new VisitorClass(&Context, mbr);
    Visitor->Init();
  }
  void runChecker(ASTContext &Context) override {
    Visitor->handlePre();
    Visitor->TraverseDecl(Context.getTranslationUnitDecl());
    Visitor->handlePost();
  }

  void setDebugLoc(DebugLoc debug) override { Visitor->Debug.debugloc = debug; }
};

class MisraManager {
public:
  MisraManager() {}

private:
  std::map<std::string, std::pair<Misrabase *, std::string>> CheckerTable;

public:
  template <class Visitor> int addChecker(std::string name, std::string desc) {
    Visitor *checker = new Visitor();
    Misrabase *ref = checker;
    std::pair<Misrabase *, std::string> registry(ref, desc);
    CheckerTable.insert(
        std::pair<std::string, std::pair<Misrabase *, std::string>>(name,
                                                                    registry));
    return 1;
  }
  std::map<std::string, std::pair<Misrabase *, std::string>> getChecker() {
    return CheckerTable;
  }
};
