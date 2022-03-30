#include <set>
#include <string>

std::set<std::string> funcID = std::set<std::string>{
#define FUNC(NAME) #NAME,
#include "visitor/Stdtable.def"
#undef FUNC
};

std::set<std::string> macroID = std::set<std::string>{
#define MACRO(NAME) #NAME,
#include "visitor/Stdtable.def"
#undef MACRO
};

std::set<std::string> typeID = std::set<std::string>{
#define TYPE(NAME) #NAME,
#include "visitor/Stdtable.def"
#undef TYPE
};
