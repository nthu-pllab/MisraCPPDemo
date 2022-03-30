#pragma once
#include "DebugInfo.h"
#include "Reporter.hpp"
#include "helper/SrcHelper.h"
#include "plugin_registry.h"
#include "visitor/MisraVisitor.hpp"

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/CrossTU/CrossTranslationUnit.h"
#include "clang/Driver/Options.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Frontend/MultiplexConsumer.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Serialization/ASTWriter.h"

#include "clang/StaticAnalyzer/Frontend/AnalysisConsumer.h"
#include "clang/StaticAnalyzer/Frontend/CheckerRegistration.h"
#include "clang/StaticAnalyzer/Frontend/FrontendActions.h"

#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/BugReporter/PathDiagnostic.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/CheckerOptInfo.h"
#include "clang/StaticAnalyzer/Core/CheckerRegistry.h"

#include <nlohmann/json.hpp>

struct Config {
  std::string name;
  std::vector<std::string> checkers;
  std::string astdir;
  std::string indexfile;
  std::string filename;
  bool ctu;
};

class Misradebug : public PragmaHandler {

private:
  DebugLoc loc;

public:
  Misradebug() : PragmaHandler("misra_debug") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducerKind Introducer,
                    Token &PragmaTok) {
    std::cout << "Found misra_debug\n";
    loc.push(PragmaTok.getLocation());
  }

  DebugLoc getDebugInfo() { return loc; }
};

class MisraPluginAction : public PluginASTAction {
  MisraManager *mgr = new MisraManager();
  MisraManager *ctu_mgr = new MisraManager();
  Config config;
  MisraReport::MisraBugReport *MBR = new MisraReport::MisraBugReport();
  std::string filename;
  std::string reportdir;
  bool list;
  unsigned int num_analysis = 0;

protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef file) override;

  bool BeginSourceFileAction(CompilerInstance &CI) override;
  bool ParseArgs(const CompilerInstance &CI, const vector<string> &args);
  void EndSourceFileAction() override;
};

class MisraASTConsumer : public ASTConsumer {
private:
  MisraManager &mgr;
  CompilerInstance *CI;
  Misradebug *handler;
  Config config;
  MisraReport::MisraBugReport *mbr;
  std::vector<std::string> ValidName;
  cross_tu::CrossTranslationUnitContext CTU;

public:
  explicit MisraASTConsumer(CompilerInstance *CI, MisraManager &MM,
                            Config config, MisraReport::MisraBugReport *MBR)
      : mgr(MM), CI(CI), config(config), mbr(MBR), CTU(*CI) {}

  virtual void Initialize(ASTContext &Context);
  virtual bool HandleTopLevelDecl(DeclGroupRef DG);
  virtual void HandleTranslationUnit(ASTContext &Context);
};

class IndexConsumer : public ASTConsumer {
private:
  CompilerInstance *CI;
  Misradebug *handler;
  Config config;
  fstream fp;

public:
  explicit IndexConsumer(CompilerInstance *CI, Config config)
      : CI(CI), config(config) {}

  virtual void Initialize(ASTContext &Context);
  virtual bool HandleTopLevelDecl(DeclGroupRef DG);
  virtual void HandleTranslationUnit(ASTContext &Context);
};

class MisraDiagnosticConsumer : public ento::PathDiagnosticConsumer {
private:
  MisraReport::MisraBugReport &report;

public:
  MisraDiagnosticConsumer(MisraReport::MisraBugReport &MBR) : report(MBR) {}
  void FlushDiagnosticsImpl(std::vector<const ento::PathDiagnostic *> &Diags,
                            FilesMade *filesMade) override {
    std::cout << "Create a MisraBugReport\n";
    for (const ento::PathDiagnostic *PD : Diags) {
      MisraReport::Diag *D = new MisraReport::Diag();
      D->setinfo(PD->getShortDescription().str(), PD->getCategory().str(),
                 PD->getBugType().str(), PD->getCheckName().str());

      for (auto it : PD->path.flatten(true)) {

        auto loc = it->getLocation().asLocation();
        auto range = it->getRanges();
        auto &sm = loc.getManager();
        FullSourceLoc begin(range[0].getBegin(), loc.getManager());
        FullSourceLoc end(range[0].getEnd(), loc.getManager());
        MisraReport::Issue i;

        i = ReportHelper::CreateIssue(sm, range[0].getBegin(), range[0],
                                      it->getString().str());
        // ReportHelper::CreateIssue(sm, range[0].getBegin(),
        // SourceRange{begin,end});
        D->path.push_back(i);
      }
      report.AddDiag(*D);
      std::cout << "\n";
    }
  }

  StringRef getName() const override { return "MisraDiags"; }
  bool supportsLogicalOpControlFlow() const override { return true; }
  bool supportsCrossFileDiagnostics() const override { return true; }
};
