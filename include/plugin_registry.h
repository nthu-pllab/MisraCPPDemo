#pragma once
#include "MisraVisitorRegister.hpp"
#include "visitor/MisraVisitor.hpp"
#include "clang/StaticAnalyzer/Core/CheckerRegistry.h"

#include <map>

#include <string>

using namespace clang;
using namespace ento;

using checkerlist = std::map<std::string, std::pair<std::string, std::string>>;
checkerlist &getCheckersList();

typedef void (*register_checker_callback_t)(
    clang::ento::CheckerRegistry &registry);
typedef void (*plugin_callback)(MisraManager *mgr);

extern "C" void add_analyzer_checker(register_checker_callback_t f,
                                     std::string name, std::string desc);
extern "C" void add_analyzer_ctu_checker(register_checker_callback_t f,
                                         std::string name, std::string desc);

void misra_analysis_register(CheckerRegistry &registry);

#define gen_Register_proto(name)                                               \
  extern "C" void add_plugin_##name##_checker(                                 \
      plugin_callback f, string checkername, string decs);                     \
  void misra_##name##_register(MisraManager *mgr);

#define REGISTER_CHECKER(type, class_name, name_str, description_str)          \
  void register_plugin_##class_name(MisraManager *mgr) {                       \
    mgr->addChecker<type(class_name)>(name_str, description_str);              \
  }                                                                            \
  __attribute__((constructor)) static void init_registration() {               \
    add_plugin_##type##_checker(&register_plugin_##class_name, name_str,       \
                                description_str);                              \
  }

/* How to Define New CheckerRegister
 * 1. Use gen_Register_proto to declare registor function and use gen_Register
 * to define function at MainRegister.cpp
 * 2. Define same name macro to expand the class of checker consumer
 */
gen_Register_proto(visitor)
#define visitor(class_name)                                                    \
  Plugin_VisitorChecker<class_name, WhatKind<class_name>>

    gen_Register_proto(ctu_visitor)
#define ctu_visitor(class_name)                                                \
  Plugin_VisitorChecker<class_name, WhatKind<class_name>>

// Support Old Checker Register
#define REGISTER_VISITOR_CHECKER(class_name, name_str, description_str)        \
  REGISTER_CHECKER(visitor, class_name, name_str, description_str)

#define REGISTER_CTUVISITOR_CHECKER(class_name, name_str, description_str)     \
  REGISTER_CHECKER(ctu_visitor, class_name, name_str, description_str)

#define REGISTER_aaCHECKER(class_name, name_str, description_str)              \
  void register_##class_name(::clang::ento::CheckerRegistry &registry) {       \
    registry.addChecker<class_name>(name_str, description_str);                \
  }                                                                            \
                                                                               \
  __attribute__((constructor)) static void init_registration() {               \
    add_analyzer_checker(&register_##class_name, name_str, description_str);   \
  }
