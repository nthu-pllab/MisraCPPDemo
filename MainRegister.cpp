#include "MisraPlugin.h"
#include "plugin_registry.h"
#include <clang/StaticAnalyzer/Core/BugReporter/BugType.h>
#include <clang/StaticAnalyzer/Core/Checker.h>
#include <clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h>
#include <iostream>
#include <set>

static std::vector<register_checker_callback_t> &getAnalysisCallbacks() {
  static std::vector<register_checker_callback_t> callbacks;
  return callbacks;
}

static std::vector<register_checker_callback_t> &getCTUAnalysisCallbacks() {
  static std::vector<register_checker_callback_t> callbacks;
  return callbacks;
}

static checkerlist &getCheckersName() {
  static std::map<std::string, std::pair<std::string, std::string>> Table;
  return Table;
}

checkerlist &getCheckersList() { return getCheckersName(); }

static std::map<std::string, std::string> &getCTUAnalysisName() {
  static std::map<std::string, std::string> Table;
  return Table;
}

extern "C" void add_analyzer_checker(register_checker_callback_t f, string name,
                                     string desc) {
  getAnalysisCallbacks().push_back(f);
  getCheckersName().insert(make_pair(name, make_pair(desc, "analyzer")));
}

extern "C" void add_analyzer_ctu_checker(register_checker_callback_t f,
                                         string name, string desc) {
  getCTUAnalysisCallbacks().push_back(f);
  getCTUAnalysisName().insert(make_pair(name, desc));
}

#define gen_Register(name)                                                     \
  static std::vector<plugin_callback> &getPlugin_##name##_Callbacks() {        \
    static std::vector<plugin_callback> callbacks;                             \
    return callbacks;                                                          \
  }                                                                            \
  extern "C" void add_plugin_##name##_checker(                                 \
      plugin_callback f, string checkername, string desc) {                    \
    getPlugin_##name##_Callbacks().push_back(f);                               \
    getCheckersName().insert(make_pair(checkername, make_pair(desc, #name)));  \
  }                                                                            \
  void misra_##name##_register(MisraManager *mgr) {                            \
    vector<plugin_callback> callbacks;                                         \
    callbacks = getPlugin_##name##_Callbacks();                                \
    for (auto it : callbacks) {                                                \
      (*it)(mgr);                                                              \
    }                                                                          \
  }

gen_Register(visitor) gen_Register(ctu_visitor)

    void misra_analysis_register(clang::ento::CheckerRegistry &registry) {
  auto callbacks = getAnalysisCallbacks();
  for (auto it : callbacks) {
    (*it)(registry);
  }
}

void misra_analysis_ctu_register(clang::ento::CheckerRegistry &registry) {
  auto callbacks = getCTUAnalysisCallbacks();
  for (auto it : callbacks) {
    (*it)(registry);
  }
}

extern "C" const char clang_analyzerAPIVersionString[] =
    CLANG_ANALYZER_API_VERSION_STRING;

static FrontendPluginRegistry::Add<MisraPluginAction> X("Misra-Checker",
                                                        "Misra CPP:2008 ");
