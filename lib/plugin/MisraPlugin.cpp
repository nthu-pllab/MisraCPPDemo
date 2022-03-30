//#define MISRA_DEBUG
#include "MisraPlugin.h"

using json = nlohmann::json;

#include <ctime>
#include <fstream>
#include <functional>
#include <iostream>
#include <vector>

using namespace std;
using namespace clang;
using namespace llvm;

void to_json(json &j, const Config &p) {
  j = json{{"name", p.name}, {"checkers", p.checkers}};
}

void from_json(const json &j, Config &p) {
  p.name = j.at("name").get<std::string>();
  p.checkers = j.at("checkers").get<std::vector<std::string>>();
}

json loadConfig(std::string configpath) {
  //==================Found config file====================
  std::ifstream configfile{configpath};
  if (configfile.fail()) {
    std::cout << "Error no config file\n";
    std::cout << "Here is Checker list\n";
    // TODO GIVEN A EXCEPTION
    std::cout << "\n=============================\n";
    return NULL;
  } else {
    json j;
    configfile >> j;
    // TODO Check json format
    if (j.find("name") != j.end() && j.find("checkers") != j.end()) {
      return j;
    } else {
      std::cout << "Error config format is not match\n";
      return NULL;
    }
  }
}

bool MisraPluginAction::ParseArgs(const CompilerInstance &CI,
                                  const vector<string> &args) {
  DEBUG_MSG("Entry Point");

  std::string configpath = "config.json";
  std::string indexpath;
  std::string astfilepath;

  bool ctu = false;

  for (unsigned i = 0, e = args.size(); i != e; ++i) {
    size_t pos = args[i].find("=", 0);
    if (pos <= args[i].size()) {
      std::string field = args[i].substr(0, pos);
      std::string value = args[i].substr(pos + 1, args[i].size() - pos);

      llvm::errs() << "arg = " << field << ":" << value << "\n";

      using Optionfunc = std::function<int(std::string)>;
      Optionfunc options =
          llvm::StringSwitch<Optionfunc>(field)
              .Case("-config",
                    [&configpath](std::string val) {
                      configpath = val;
                      return 0;
                    })
              .Case("-index",
                    [&indexpath = indexpath](std::string val) {
                      indexpath = val;
                      return 0;
                    })
              .Case("-astdir",
                    [&astfilepath = astfilepath](std::string val) {
                      astfilepath = val;
                      return 0;
                    })
              .Case("-help",
                    [](std::string val) {
                      std::cout << "print help\n";
                      return 1;
                    })
              .Case("-list",
                    [&list = list](std::string val) {
                      if (val.size() < 1 || val == "true")
                        list = true;
                      else
                        list = false;
                      return 0;
                    })
              .Case("-o",
                    [&reportdir = reportdir](std::string val) {
                      reportdir = val;
                      return 0;
                    })
              .Case("-ctu",
                    [&ctu = ctu](std::string val) {
                      ctu = true;
                      return 0;
                    })
              .Default([](std::string val) {
                std::cout << "Error Args\n";
                return 1;
              });

      if (options(value)) {
        return false;
      }
    }
  }

  // fill in the config by command line argument don't do more than fill in
  // config
  if (loadConfig(configpath) != NULL) {
    config = loadConfig(configpath);
  }

  config.indexfile = indexpath;
  config.astdir = astfilepath;
  config.ctu = ctu;

  return true;
}

bool MisraPluginAction::BeginSourceFileAction(CompilerInstance &CI) {
  DEBUG_MSG("Entry Point");

  // We Get filename at here the first time we can get CI and checker the config
  filename = SrcHelper::getMainFileName(CI);
  filename = filename.substr(filename.find_last_of('/') + 1, filename.back());
  config.filename = filename;

  // check the config
  if (config.astdir.size() < 1) {
    config.astdir = "./";
  }
  if (config.astdir.back() != '/') {
    config.astdir.append("/");
  }

  if (config.indexfile.size() < 1) {
    config.indexfile = config.astdir + config.filename + ".index";
  }

  if (config.checkers.empty()) {
    std::cout << "Disable all checkers exit program\n";
    return false;
  }

  // Then load misra checker we can get all checker name but real object og CSA
  // checker is not load yet.
  misra_visitor_register(mgr);
  misra_ctu_visitor_register(ctu_mgr);
  //  auto ctu_analysis = misra_register(ctu_mgr, true);

  auto visitors = mgr->getChecker();

  // argument list
  if (list) {
    for (auto it : getCheckersList()) {
      std::cout << it.first << ":" << it.second.first << "(" << it.second.second
                << ")" << std::endl;
    }
    return false;
  } else {
    // bool ret = true;
    std::vector<std::string> valid_checkers;

    for (auto checker_name : config.checkers) {
      auto it = getCheckersList().find(checker_name);
      if (it == getCheckersList().end()) {
        std::cout << "We Can't find checker called " << checker_name
                  << std::endl;
        // return false;
      } else {
        valid_checkers.push_back(checker_name);
        std::cout << "We Will use:";
        std::cout << it->first << ":" << it->second.first << "("
                  << it->second.second << ")\n";
        if (it->second.second == "analyzer") {
          std::cout << "remove " << it->first
                    << "in config and add it to analyzer\n";
          CI.getAnalyzerOpts()->CheckersControlList.push_back(
              std::pair<std::string, bool>(checker_name, true));
        }
      }
    }

    config.checkers.clear();
    for (auto checker_name : valid_checkers) {
      config.checkers.push_back(checker_name);
    }
    return true;
  }
}

std::unique_ptr<ASTConsumer>
MisraPluginAction::CreateASTConsumer(CompilerInstance &CI, StringRef file) {
  DEBUG_MSG("Entry Point");

  std::vector<std::unique_ptr<ASTConsumer>> Consumers;

  if (!config.ctu) {
    std::string OutputFile = config.astdir + config.filename + ".ast";
    std::unique_ptr<raw_pwrite_stream> OS =
        CI.createOutputFile(OutputFile, true, false, file, "", true);

    if (!OS)
      return nullptr;

    std::string Sysroot;
    auto Buffer = std::make_shared<PCHBuffer>();
    Consumers.push_back(llvm::make_unique<PCHGenerator>(
        CI.getPreprocessor(), OutputFile, Sysroot, Buffer,
        CI.getFrontendOpts().ModuleFileExtensions, false,
        +CI.getFrontendOpts().BuildingImplicitModule));

    Consumers.push_back(CI.getPCHContainerWriter().CreatePCHContainerGenerator(
        CI, file, OutputFile, std::move(OS), Buffer));
    Consumers.push_back(llvm::make_unique<IndexConsumer>(&CI, config));
  }

  // Create a AnalysisConsumer
  if (config.checkers.size() < 1 && num_analysis < 1) {
    std::cout << "no Valid Checker\n";
    if (config.ctu)
      return nullptr;
  } else {
    std::cout << "We have :" << num_analysis << " analyzer checker\n";
    if (config.ctu) {
      mgr = ctu_mgr;
      std::cout << mgr->getChecker().size() << "and CTU visitor\n";
    }
    std::unique_ptr<ento::AnalysisASTConsumer> AnalysisConsumer =
        ento::CreateAnalysisConsumer(CI);
    AnalysisConsumer->AddDiagnosticConsumer(new MisraDiagnosticConsumer(*MBR));
    AnalysisConsumer->AddCheckerRegistrationFn(misra_analysis_register);
    Consumers.push_back(std::move(AnalysisConsumer));

    Consumers.push_back(
        llvm::make_unique<MisraASTConsumer>(&CI, *mgr, config, MBR));
  }

  return llvm::make_unique<MultiplexConsumer>(std::move(Consumers));
}

void MisraPluginAction::EndSourceFileAction() {
  DEBUG_MSG("Entry Point");
  json report_json = MBR->CreateJson();

  std::string out_filename;
  if (reportdir.size() < 1) {
    std::cout << "End Analysis (without Report) " << filename << "\n";
    std::cout << "if you want to look json uncomment source code at "
                 "MisraPlugin:251\n";
    std::cout << std::setw(4) << report_json << std::endl;
  } else {
    out_filename = reportdir;
    fstream fp;
    fp.open(out_filename, ios::out);
    if (!fp) {
      cout << "Fail to open file: " << out_filename << endl;
    }
    fp << report_json;

    std::cout << "End Analysis " << filename << "\n";
  }
  // free
}
