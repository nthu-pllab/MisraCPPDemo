#include "DebugInfo.h"
#include "Reporter.hpp"
#include "helper/SrcHelper.h"
#include "plugin_registry.h"
#include "visitor/MisraVisitor.hpp"

#include "llvm/ADT/Optional.h"
#include "llvm/Support/CachePruning.h"
#include "llvm/Support/raw_ostream.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Driver/Options.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Index/USRGeneration.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Sema/Sema.h"

#include "clang/StaticAnalyzer/Frontend/AnalysisConsumer.h"
#include "clang/StaticAnalyzer/Frontend/CheckerRegistration.h"
#include "clang/StaticAnalyzer/Frontend/FrontendActions.h"

#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/BugReporter/PathDiagnostic.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/CheckerOptInfo.h"
#include "clang/StaticAnalyzer/Core/CheckerRegistry.h"

#include <nlohmann/json.hpp>

#include "MisraPlugin.h"

void MisraASTConsumer ::Initialize(ASTContext &Context) {

  clang::Preprocessor &pp = CI->getPreprocessor();
  handler = new Misradebug();
  pp.AddPragmaHandler(handler);

  auto Checkers = mgr.getChecker();

  for (auto it : config.checkers) {
    if (Checkers.find(it) == Checkers.end()) {
      std::cout << it << " is not exists but we don't expect run this stmt\n";
      continue;
    }

    Checkers[it].first->setCheckerInfo(it, Checkers[it].second);
    Checkers[it].first->Init(Context, *mbr);
    Checkers[it].first->regPPCallbacks(*CI);

    ValidName.push_back(it);
  }
}

bool MisraASTConsumer ::HandleTopLevelDecl(DeclGroupRef DG) { return true; }

void MisraASTConsumer::HandleTranslationUnit(ASTContext &Context) {

  if (config.ctu) {
    for (auto ele : Context.getTranslationUnitDecl()->decls()) {
      if (auto *FD = dyn_cast<FunctionDecl>(ele)) {
        // std::cout << "@FunctionDecl:" << FD->getNameAsString() <<
        // FD->isDefined() << ":" << FD->hasBody() << std::endl;
        if (!FD->isDefined()) {
          llvm::Expected<const FunctionDecl *> NewFDorError =
              CTU.getCrossTUDefinition(FD, "", config.indexfile);
          if (auto err = NewFDorError.takeError()) {
            std::string errstr =
                "[Index \"" + FD->getNameAsString() + "\" Missing] ";
            logAllUnhandledErrors(std::move(err), llvm::errs(), errstr);
            continue;
          }
        }
      }
    }
    // Context.getTranslationUnitDecl()->dump();
  }

  auto Checkers = mgr.getChecker();
  for (auto it : ValidName) {
    std::cout << "Run " << it << std::endl;
    Checkers[it].first->setDebugLoc(handler->getDebugInfo());
    Checkers[it].first->runChecker(Context);
  }
}
