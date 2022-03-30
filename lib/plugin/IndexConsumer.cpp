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

void IndexConsumer ::Initialize(ASTContext &Context) {
  fp.open(config.indexfile, std::ios::out);
  std::cout << "Create Index\n";
}

bool IndexConsumer ::HandleTopLevelDecl(DeclGroupRef DG) {
  if (fp.fail()) {
    std::cout << "index file set error\n";
  }

  for (DeclGroupRef::iterator i = DG.begin(), e = DG.end(); i != e; i++) {
    Decl *D = *i;
    if (auto *FD = dyn_cast<FunctionDecl>(D)) {
      SmallString<128> DeclUSR;
      if (index::generateUSRForDecl(FD, DeclUSR) || !FD->isDefined())
        continue;
      else {
        std::cout << DeclUSR.str().str() << "\n";
        fp << DeclUSR.str().str() << " "
           << config.astdir + config.filename + ".ast" << std::endl;
      }
    }
  }

  return true;
}

void IndexConsumer::HandleTranslationUnit(ASTContext &Context) { fp.close(); }
