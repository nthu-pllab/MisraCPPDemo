#include "helper/SrcHelper.h"
std::vector<Token> &SrcHelper::getTokens(ASTContext *Context, Preprocessor *PP,
                                         SourceRange SR) {
  std::vector<Token> *ret = new std::vector<Token>();
  Token Result;
  SourceLocation next = SR.getBegin();
  do {
    if (!PP->getRawToken(next, Result, true)) {
      ret->push_back(Result);
    }
    next = PP->getLocForEndOfToken(Result.getLocation());
  } while (isBefore(Context, next, SR.getEnd().getLocWithOffset(1)));
  return *ret;
}

bool SrcHelper::isBefore(ASTContext *Context, const SourceLocation a,
                         const SourceLocation b) {
  BeforeThanCompare<SourceLocation> compare(Context->getSourceManager());
  if (a.isInvalid() || b.isInvalid())
    return false;
  return compare(a, b);
}

std::string SrcHelper::getMainFileName(const CompilerInstance &CI) {
  FileID mainid = CI.getSourceManager().getMainFileID();
  SourceLocation loc = CI.getSourceManager().getLocForStartOfFile(mainid);
  std::string name = CI.getSourceManager().getFilename(loc);
  return name;
}

tok::PPKeywordKind SrcHelper::getPPKeywordID(const Token Tok) {
  // The following source code is based on clang/lib/Basic/IdentifierTable.cpp

#define HASH(LEN, FIRST, THIRD)                                                \
  (LEN << 5) + (((FIRST - 'a') + (THIRD - 'a')) & 31)
#define CASE(LEN, FIRST, THIRD, NAME)                                          \
  case HASH(LEN, FIRST, THIRD):                                                \
    return memcmp(Name, #NAME, LEN) ? tok::pp_not_keyword : tok::pp_##NAME

  unsigned Len = Tok.getLength();
  if (Len < 2)
    return tok::pp_not_keyword;
  const char *Name = Tok.getRawIdentifier().str().c_str();
  switch (HASH(Len, Name[0], Name[2])) {
  default:
    return tok::pp_not_keyword;
    CASE(2, 'i', '\0', if);
    CASE(4, 'e', 'i', elif);
    CASE(4, 'e', 's', else);
    CASE(4, 'l', 'n', line);
    CASE(4, 's', 'c', sccs);
    CASE(5, 'e', 'd', endif);
    CASE(5, 'e', 'r', error);
    CASE(5, 'i', 'e', ident);
    CASE(5, 'i', 'd', ifdef);
    CASE(5, 'u', 'd', undef);

    CASE(6, 'a', 's', assert);
    CASE(6, 'd', 'f', define);
    CASE(6, 'i', 'n', ifndef);
    CASE(6, 'i', 'p', import);
    CASE(6, 'p', 'a', pragma);

    CASE(7, 'd', 'f', defined);
    CASE(7, 'i', 'c', include);
    CASE(7, 'w', 'r', warning);

    CASE(8, 'u', 'a', unassert);
    CASE(12, 'i', 'c', include_next);

    CASE(14, '_', 'p', __public_macro);

    CASE(15, '_', 'p', __private_macro);

    CASE(16, '_', 'i', __include_macros);
#undef CASE
#undef HASH
  }
}
