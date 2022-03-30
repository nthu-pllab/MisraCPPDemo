#include "clang/Basic/SourceLocation.h"
#include "llvm/Support/raw_ostream.h"
#include "helper/SrcHelper.h"
#include <queue>
#include <iostream>
#include <fstream>

using namespace std;
using namespace clang;

#ifndef DebugInfo_h
#define DebugInfo_h

#ifdef MISRA_DEBUG
    #define DEBUG_MSG(msg) printf("=misra debug= Message:%s \n\tNote in func:%s \n\tat %s:%d\n",msg,__PRETTY_FUNCTION__,__FILE__,__LINE__);
#else
    #define DEBUG_MSG(msg) 
#endif





typedef queue<SourceLocation> DebugLoc;

class DebugInfo {
private:
	int a;
	ASTContext *Context;
	SourceManager *sm;
	vector<string> DebugLogstr;

public:
	vector<llvm::raw_string_ostream*> DebugLog;
	DebugInfo(ASTContext *C) : Context(C) , DebugLogstr(10) {
		sm = &(C->getSourceManager());
		for (int i = 0; i < 10; i++) {
			DebugLog.push_back(new llvm::raw_string_ostream(DebugLogstr[i]));
		}
	}

	DebugInfo() {}
	DebugLoc debugloc;

  bool DumpAST(const Stmt *ST) {
	  if (!ST) {
		  std::cout << "No Parents\n";
		  return false;
	  }
	  std::cout << "======================================\n";
	  std::cout << "              Debug Dump              \n";
	  std::cout << "======================================\n";
	  std::cout << "===============AST Tree===============\n";
	  ST->dumpColor();
	  std::cout << "==============Source Code=============\n";
	  ST->dumpPretty(*Context);
	  return true;
  }

  bool DumpAST(const Decl *ST) {
	  if (!ST) {
		  std::cout << "No Parents\n";
		  return false;
	  }
	  std::cout << "======================================\n";
	  std::cout << "              Debug Dump              \n";
	  std::cout << "======================================\n";
	  std::cout << "===============AST Tree===============\n";
	  ST->dumpColor();
	  return true;
  }

  bool DumpAST(const Stmt *ST,llvm::raw_ostream &OS) {
	  if (!ST) {
		  std::cout << "No Parents\n";
		  return false;
	  }
	  OS << "======================================\n";
	  OS << "              Debug Dump              \n";
	  OS << "======================================\n";
	  OS << "===============AST Tree===============\n";
	  ST->dump(OS);
	  OS << "==============Source Code=============\n";
	  //ST->dumpPretty(*Context);
	  ST->printPretty(OS, nullptr, PrintingPolicy(Context->getLangOpts()));
	  return true;
  }

  bool DumpAST(const Decl *ST, llvm::raw_ostream &OS) {
	  if (!ST) {
		  std::cout << "No Parents\n";
		  return false;
	  }
	  OS << "======================================\n";
	  OS << "              Debug Dump              \n";
	  OS << "======================================\n";
	  OS << "===============AST Tree===============\n";
	  ST->dump(OS);
	  return true;
  }

 template<class Node> bool DebugMode(Node *node,int cmdmode) {
	 if (debugloc.empty()) {
		 std::cout << "no debugloc\n";
		 return true;
	 }
	  if (SrcHelper::isBefore(Context,debugloc.front(), node->getLocStart())) {
		  int line = sm->getSpellingLineNumber(node->getLocStart());
		  debugloc.pop();
		  
		  
		  if (cmdmode == 0) {
			  llvm::errs() << "debug at line:" << line << "input:\n";
			  std::string cmd;
			  std::cin >> cmd;
			  if (cmd == "p") {
				  DebugAST(node);
			  }
			  else {
				  std::cout << "only support \"p\" now";
			  }
		  }
		  else {
			  std::cout << "DumpAST\n";
			  *DebugLog[0] << "debug at line:" << line << "\n";
			  DumpAST(node, *DebugLog[0]);
			  const Node* ST = node;
			  for (int i = 1; i < cmdmode;i++) 
			  {
					  auto parents = Context->getParents(*ST);
					  if (parents.empty()) {
						  std::cout << "Parent is empty\n";
						  break;
					  }
					  else {
						  ST = parents[0].template get<Node>();
						  if (!DumpAST(ST, *DebugLog[0])) break;
					  }
			  }
		  }
		  DebugSymbol();
	  }
	  return true;
  }

 template<class Node>
void DebugAST(const Node* ST) {

	 DumpAST(ST);
	 std::cout << "find Parent?[yes/no]";
	 string cmd;
	 std::cin >> cmd;

	 while (1) {
		 if (cmd.compare("yes") == 0) {

			 auto parents = Context->getParents(*ST);
			 if (parents.empty()) {
				 std::cout << "Parent is empty\n";
				 return;
			 }
			 else {
				 ST = parents[0].template get<Node>();
				 if (!DumpAST(ST)) return;
			 }
			 std::cout << "find more upper parent?[yes/no]";
			 std::cin >> cmd;
		 }
		 else if (cmd.compare("no") == 0) {
			 return;
		 }
		 else {
			 std::cout << "plz input yes/no\n";
		 }

	 }
}

void DebugShell() {

}

virtual bool DebugDump(std::string cmd) { return true; }

raw_ostream& Debugs(int i) {
	if (i >= 10) {
		llvm::errs() << "Only Support 10 Debuglog, print on the stderr\n";
		return llvm::errs();
	}
	return *DebugLog[i];
}


bool Save(string filename) {
	std::fstream fp;
	fp.open(filename, ios::out);//開啟檔案
	if (!fp) {//如果開啟檔案失敗，fp為0；成功，fp為非0
		cout << "Fail to open file: " << filename << endl;
		return false;
	}
	int i = 0;
	for (auto stream : DebugLog) {
		std::cout << "Writing Debug FIle\n";
		if (!stream->str().empty()) {
			fp << "No. " << i++ << " ===============\n";
			fp << stream->str();
			fp << "\n\n";
		}
	}

	fp.close();//關閉檔案
	return true;
}

void DebugSymbol() {}
};

#endif//define debug