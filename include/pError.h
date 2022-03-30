
#ifndef _MISRAC_PERROR_H_
#define _MISRAC_PERROR_H_

#include <string.h>


template <class T> void __pError(ASTContext *Context, T ASTnode, std::string msg);
template <class T> void pError(ASTContext *Context, T ASTnode, const char *msg);
template <class T> void pError(ASTContext *Context, T ASTnode, std::string msg);

template <class T> void pError(ASTContext *Context, T ASTnode, const char *msg) {
  __pError(Context, ASTnode, std::string(msg));
}

template <class T> void pError(ASTContext *Context, T ASTnode, std::string msg) {
  if ( const Expr* e = dyn_cast<Expr>(ASTnode) ) {
    __pError(Context, e, std::string(msg));
  } else {
    llvm::outs() << "pError: Not a <Expr> object\n";
  }
}



  
template <class T> void __pError(ASTContext *Context, T ASTnode, std::string msg) {
  const SourceManager &sm = Context->getSourceManager();
  const SourceLocation LocStart = ASTnode->getLocStart();
  const SourceLocation SpellingLoc = sm.getSpellingLoc(ASTnode->getLocStart());
  // const SourceLocation FileLoc = sm.getFileLoc(ASTnode->getLocStart());

  // FullSourceLoc is just combination SM & SourceLocation
  // FullSourceLoc FullLocation = Context->getFullLoc(ASTnode->getLocStart());

  if (LocStart.isValid()) {
    //    LocStart.dump(sm);
    SpellingLoc.dump(sm);
    //    FileLoc.dump(sm);
    llvm::outs() << "\n - " << msg << "\n";
    /*llvm::outs() << sm.getFilename(spellingLoc) << ":"
                 << sm.getSpellingLineNumber(spellingLoc) << ":"
                 << sm.getSpellingColumnNumber(spellingLoc) << "  " << msg
                 << "  "
                 << "\n";*/
  } else {
    llvm::outs() << "FullSourceLoc is not valid\n";
    return;
  }

  if (LocStart.isMacroID() == true) {
    llvm::outs() << "Defined in Macro!!\n";
  } else {
    // llvm::outs() << "Not macro\n";
  }

  // Try to print out the line
  int line_num = sm.getSpellingLineNumber(SpellingLoc);

  FileID FID = sm.getFileID(SpellingLoc);
  SourceLocation start = sm.translateLineCol(FID, line_num, 1);
  SourceLocation end = sm.translateLineCol(FID, line_num+1, 1).getLocWithOffset(-1);
  std::string line_data = std::string(sm.getCharacterData(start), sm.getCharacterData(end));

  llvm::outs() << line_data  << "\n";
  llvm::outs() << "\n";
}

#endif
