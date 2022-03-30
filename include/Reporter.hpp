#ifndef REPORTER_HPP_
#define REPORTER_HPP_

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Version.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"

#include <iostream>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

using namespace std;
using namespace clang;

namespace MisraReport {

class Issue {
private:
  string kind;
  typedef struct {
    int line;
    int col;
    string file;
    json &CreateJson() {
      json *j = new json({{"line", line}, {"column", col}, {"file", file}});
      return *j;
    }
  } location;

  location loc;
  pair<location, location> range;
  int depth;
  string msg;
  string ext_msg;

public:
  string getfile() { return loc.file; }

  void setloc(int line, int col, string file) {
    loc.line = line;
    loc.col = col;
    loc.file = file;
  }

  void setloc(clang::FullSourceLoc fullloc) {
    if (fullloc.isFileID()) {
      loc.line = fullloc.getPresumedLoc().getLine();
      loc.col = fullloc.getPresumedLoc().getColumn();
      loc.file = fullloc.getPresumedLoc().getFilename();
    } else {
      auto Exloc = fullloc.getManager().getExpansionLoc(fullloc);
      clang::FullSourceLoc Exfull{Exloc, fullloc.getManager()};
      setloc(Exfull);
    }
  }

  void setBegin(clang::FullSourceLoc fullloc) {
    if (fullloc.isFileID()) {
      range.first.line = fullloc.getPresumedLoc().getLine();
      range.first.col = fullloc.getPresumedLoc().getColumn();
      range.first.file = fullloc.getPresumedLoc().getFilename();
    } else {
      auto Exloc = fullloc.getManager().getExpansionLoc(fullloc);
      clang::FullSourceLoc Exfull{Exloc, fullloc.getManager()};
      setBegin(Exfull);
    }
  }

  void setEnd(clang::FullSourceLoc fullloc) {
    if (fullloc.isFileID()) {
      range.second.line = fullloc.getPresumedLoc().getLine();
      range.second.col = fullloc.getPresumedLoc().getColumn();
      range.second.file = fullloc.getPresumedLoc().getFilename();

    } else {
      auto Exloc = fullloc.getManager().getExpansionLoc(fullloc);
      clang::FullSourceLoc Exfull{Exloc, fullloc.getManager()};
      setEnd(Exfull);
    }
  }

  void setBegin(int line, int col, string file) {
    range.first.line = line;
    range.first.col = col;
    range.first.file = file;
  }

  void setEnd(int line, int col, string file) {
    range.second.line = line;
    range.second.col = col;
    range.second.file = file;
  }

  bool setMsg(string m) {
    if (m.size() == 0) {
      msg = "Null";
      return false;
    } else {
      msg = m;
      return true;
    }
  }

  bool setExtMsg(string m) {
    if (m.size() == 0) {
      ext_msg = "Null";
      return false;
    } else {
      ext_msg = m;
      return true;
    }
  }

  void setDepth(int i) { depth = 0; }

  void setKind(string k) { kind = k; }

  json CreateJson() {
    json j;
    j["message"] = msg;
    j["kind"] = kind;
    j["depth"] = depth;
    j["extended_message"] = ext_msg;
    j["location"] = loc.CreateJson();
    j["ranges"] = {range.first.CreateJson(), range.second.CreateJson()};
    return j;
  }
};

class Diag {
public:
  vector<Issue> path;
  string describe;
  string category;
  string type;
  string checkname;

public:
  set<string> getFileList() {
    set<string> *ret = new set<string>();
    for (auto it : path) {
      ret->insert(it.getfile());
    }
    return *ret;
  }

  void setinfo(string desc, string cat, string t, string name) {
    describe = desc;
    category = cat;
    type = t;
    checkname = name;
  }

  json CreateJson() {
    json j;
    j["description"] = describe;
    j["category"] = category;
    j["type"] = type;
    j["check_name"] = checkname;
    for (auto it : path) {
      j["path"].push_back(it.CreateJson());
    }
    return j;
  }
};

class MisraBugReport {
public:
  string version;
  set<string> files;
  vector<Diag> diagnostics;

public:
  void AddDiag(Diag diag) {
    auto f = diag.getFileList();
    files.insert(f.begin(), f.end());
    diagnostics.push_back(diag);
  }

  int getDiagSize() { return diagnostics.size(); }

  json CreateJson() {
    json j;
    version = clang::getClangFullVersion();
    j["clang_version"] = clang::getClangFullVersion();
    j["files"] = files;
    for (auto it : diagnostics)
      j["diagnostics"].push_back(it.CreateJson());

    return j;
  }
};

} // namespace MisraReport
#endif
