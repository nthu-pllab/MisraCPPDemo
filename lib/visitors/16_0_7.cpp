#include "MisraVisitor.hpp"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include <string>
#include <vector>

static char err_kind[] = "Misra CPP Rule 16-0-7";
static char rule_desc[] =
    "Undefined macro identifiers shall not be used in #if or"
    " #elif preprocessor directives, except as operands to the"
    " defined operator.";


class Rule_16_0_7 : public RecursiveASTVisitor<Rule_16_0_7>, public MisraVisitor, public PPCallbacks {
public:
  using MisraVisitor::MisraVisitor;
  bool VisitTranslationUnitDecl (TranslationUnitDecl *D);
  void MacroDefined (const Token &MacroNameTok, const MacroDirective *MD);
  // void MacroExpands (const Token &MacroNameTok, const MacroDefinition &MD, SourceRange Range, const MacroArgs *Args);
  void If(SourceLocation Loc, SourceRange ConditionRange, ConditionValueKind ConditionValue);
  void Elif (SourceLocation Loc, SourceRange ConditionRange, ConditionValueKind ConditionValue, SourceLocation IfLoc);
  void Else (SourceLocation Loc, SourceLocation IfLoc);
  void Defined (const Token &MacroNameTok, const MacroDefinition &MD, SourceRange Range);

private:
    FileID f;
    std::string err_msg = ""; 
    bool traverseMacroToken(std::string target);
    void checkFile(FileID &f, SourceLocation Loc);
    void checkRule(SourceRange ConditionRange);
    std::vector<Token> tok_arr;
    bool isDefined = false;
    bool isDefine = false;
    bool already = false;
    bool check_inside = true;
    bool ld_check = false;
};
bool Rule_16_0_7::VisitTranslationUnitDecl (TranslationUnitDecl *D){
    // f = sm->getMainFileID();
    return true;
}
void Rule_16_0_7::MacroDefined (const Token &MacroNameTok, const MacroDirective *MD){
    // std::cout << "enter MD" << std::endl;
    tok_arr.push_back(MacroNameTok);
}
// void Rule_16_0_7::MacroExpands (const Token &MacroNameTok, const MacroDefinition &MD, SourceRange Range, const MacroArgs *Args){
//     tok_arr.push_back(MacroNameTok);
// } 


void Rule_16_0_7::If(SourceLocation Loc, SourceRange ConditionRange, ConditionValueKind ConditionValue){
    // std::cout << "enter If" << std::endl;
    FileID f = sm->getMainFileID();
    SourceLocation loc = ConditionRange.getEnd();
    checkRule(ConditionRange);
    if(check_inside){
        checkFile(f, loc);
    }
    // check_inside = true;
}
void Rule_16_0_7::Else(SourceLocation Loc, SourceLocation IfLoc){
    FileID f = sm->getMainFileID();
    if(check_inside == false){
      checkFile(f, Loc);
    }
    // check_inside = true;
}
void Rule_16_0_7::Elif (SourceLocation Loc, SourceRange ConditionRange, ConditionValueKind ConditionValue, SourceLocation IfLoc){
    // std::cout << "enter Elif" << std::endl;
    FileID f = sm->getMainFileID();
    SourceLocation loc = ConditionRange.getEnd();
    checkRule(ConditionRange);
    if(check_inside){
        checkFile(f, loc);
    }
    // check_inside = true;
}

void Rule_16_0_7::Defined (const Token &MacroNameTok, const MacroDefinition &MD, SourceRange Range){
    // std::cout << "enter Defined" << std::endl;
    // const IdentifierInfo *II = MacroNameTok.getIdentifierInfo();
    // MacroDirective *Md = PP->getLocalMacroDirective(II);
    // tok_arr.push_back(MacroNameTok);
	  isDefined = true;
}

void Rule_16_0_7::checkFile(FileID &f, SourceLocation Loc) {
  bool invalid;
  
  if (f.isInvalid()) {
    return;
  }
//   const char *mb_ptr = mb->getBuffer().data();
  const char *mb_ptr = sm->getCharacterData(Loc, &invalid);
  int mb_len = 0;
  if(mb_ptr != nullptr){
     mb_len = strlen(mb_ptr);
  }
  int index = 0;
  int if_c = 0;
//   int endif_c = 0;
  while ( (index >= 0) && ((index + 2) <= mb_len) ){
      int loc_offset = index;
      // std::cout << "mb_ptr length = " << strlen(mb_ptr) << std::endl;
    //   std::cout << *mb_ptr << std::endl;
    // std::cout << "OUT mb_len = " << mb_len << "\tindex = " << index<< std::endl;
    if ( (index>=0) && (index < mb_len)){
      if ( (index >= 0) && ((mb_len - index ) >= 4 ) ){
            // std::cout << "IN  mb_len = " << mb_len << "\tindex = " << index<< std::endl;
            if( (index >=0) && ((mb_ptr[0] == '#') && (mb_ptr[1] =='e') && (mb_ptr[2] == 'l') && (mb_ptr[3] == 'i'))){
            //   std::cout << "------>if occur!<------ :"  << mb_ptr[1] << mb_ptr[2]<< std::endl;
              while(*mb_ptr != ' '){
                mb_ptr++;
                index++;
              }

            //   std::cout << "------>ignore #if<------" << std::endl;
              mb_ptr++;
              index++;
              std::string find_str = "";
              const char *buf = &(mb_ptr[0]);
              std::string buf_str(buf);
              int c = 0;
              bool ignore = false;
              if(((mb_len - index + 1) >= 7 )){
                if((index >=0) && (mb_ptr[0] == 'd') && (mb_ptr[1] == 'e') && (mb_ptr[2] == 'f') && (mb_ptr[3] == 'i') && (mb_ptr[4] == 'n') && (mb_ptr[5] == 'e') && (mb_ptr[6] == 'd')){
                    // mb_ptr+=7;
                    // index+=7;
                    continue;
                }
              }
            //   buf = buf.substr(0, 1);
              while( (mb_ptr[0] != ' ') && (mb_ptr[0] != '\n') && (mb_ptr[0] != '\r') && (mb_ptr[0] != '\0') ){
                if((mb_ptr[0] == '!')){
                  c++;
                  mb_ptr++;
                  index++;
                  continue;
                }
                if((index >=0) && ((mb_len - index + 1) >= 7 )){
                  if((mb_ptr[0] == 'd') && (mb_ptr[1] == 'e') && (mb_ptr[2] == 'f') && (mb_ptr[3] == 'i') && (mb_ptr[4] == 'n') && (mb_ptr[5] == 'e') && (mb_ptr[6] == 'd')){
                    mb_ptr++;
                    index++;
                    ignore = true;
                    break;
                  }
                }
                
                
                find_str.append(buf_str.substr(c, 1));
                c++;
                mb_ptr++;
                index++;
              }
            //   std::cout << "find_str = " << find_str << std::endl; 
            //   already = false;   
              if(traverseMacroToken(find_str) == false && ignore == false){
                  SourceRange sr(Loc.getLocWithOffset(loc_offset), Loc.getLocWithOffset(index-1));
                  SimpleBugReport(&sr, err_kind, rule_desc);
                  // SimpleBugReport(&sr, err_kind, "??");
                //   already = true;
              }

          }

      }else if(((mb_len - index + 1) >= 4 ) ){
        if((index >=0) && ((mb_ptr[0] == '#') && (mb_ptr[1] =='i') && (mb_ptr[2] == 'f'))){
            //   std::cout << "------>if occur!<------ :"  << mb_ptr[1] << mb_ptr[2]<< std::endl;
              if ((mb_ptr[1] =='i') && (mb_ptr[2] == 'f')){
                  if_c++;
              }

              if((mb_ptr[3] =='n') || (mb_ptr[3] == 'd')){
                mb_ptr++;
                index++;
                continue;
              }
              while(*mb_ptr != ' '){
                mb_ptr++;
                index++;
              }

            //   std::cout << "------>ignore #if<------" << std::endl;
              mb_ptr++;
              index++;
              std::string find_str = "";
              const char *buf = &(mb_ptr[0]);
              std::string buf_str(buf);
              int c = 0;
              bool ignore = false;
              if(((mb_len - index + 1) >= 7 )){
                if((mb_ptr[0] == 'd') && (mb_ptr[1] == 'e') && (mb_ptr[2] == 'f') && (mb_ptr[3] == 'i') && (mb_ptr[4] == 'n') && (mb_ptr[5] == 'e') && (mb_ptr[6] == 'd')){
                    // mb_ptr+=7;
                    // index+=7;
                    continue;
                }
              }
            //   buf = buf.substr(0, 1);
              while( (mb_ptr[0] != ' ') && (mb_ptr[0] != '\n') && (mb_ptr[0] != '\r') && (mb_ptr[0] != '\0') ){
                if((mb_ptr[0] == '!')){
                  c++;
                  mb_ptr++;
                  index++;
                  continue;
                }
                if(((mb_len - index + 1) >= 7 )){
                  if((mb_ptr[0] == 'd') && (mb_ptr[1] == 'e') && (mb_ptr[2] == 'f') && (mb_ptr[3] == 'i') && (mb_ptr[4] == 'n') && (mb_ptr[5] == 'e') && (mb_ptr[6] == 'd')){
                    mb_ptr++;
                    index++;
                    ignore = true;
                    break;
                  }
                }
                
                
                find_str.append(buf_str.substr(c, 1));
                c++;
                mb_ptr++;
                index++;
              }
            //   std::cout << "find_str = " << find_str << std::endl; 
            //   already = false;   
              if(traverseMacroToken(find_str) == false && ignore == false){
                  SourceRange sr(Loc.getLocWithOffset(loc_offset), Loc.getLocWithOffset(index-1));
                  SimpleBugReport(&sr, err_kind, rule_desc);
                  // SimpleBugReport(&sr, err_kind, "??");
                //   already = true;
              }

          }

      }else if (((mb_len - index + 1) >= 2 ) ){
        if ((index >=0) && (mb_ptr[0] == '/') && (mb_ptr[1] =='/')){
          while((mb_ptr[0] != '\n') && (mb_ptr[0] != '\r')){
            mb_ptr++;
            index++;
          }

        }else if ((index >=0) && (mb_ptr[0] == '/') && (mb_ptr[1] =='*')){
          while((mb_ptr[0] != '*') || (mb_ptr[1] != '/')){
            mb_ptr++;
            index++;
          }
        }

      }
      if ( (index >=0) && ((mb_len - index + 1) >= 4 ) && (mb_ptr[0] == '#') && (mb_ptr[1] =='e') && (mb_ptr[2] == 'n') && (mb_ptr[3] == 'd') ){
      //   std::cout << "#endif occur!" << std::endl;
          if_c--;
      }
    }
    if(if_c < 0){
      break;
    }
    mb_ptr++;
    index++;
  }
  check_inside = true;
  return;
}

bool Rule_16_0_7::traverseMacroToken(std::string target){
    // traverse all of defined macro definition.
    // std::cout << "traverseMacroToken " << std::endl; 
    if(target == "0" || target == "1" ){
      isDefine = true;
      return true;
    }   
    for(auto t=tok_arr.begin(); t!=tok_arr.end(); ++t){
        bool invalid;
        // make token (#define) to string
        SourceRange tsr(t->getLocation(),t->getEndLoc());
        llvm::StringRef tok_ptr = Lexer::getSourceText(CharSourceRange::getTokenRange(tsr), *sm, LangOptions(), &invalid);
        std::istringstream iss2(tok_ptr.str());
        std::vector<string> tok_str;
        std::string t2;
        while (iss2 >> t2) {
            tok_str.push_back(t2);
        }
        // check if identifier is defined.
        if(ld_check == false && tok_str[0]  == "USES_LONGDOUBLE"){
            std::cout << "here!!!!!! find USELONGDOUBLE" << std::endl;
            ld_check = true;
        }
        if(tok_str[0] == target){
            // std::cout<<"is Defined : t = " << tok_str[0] <<std::endl;
            // std::string err_msg = tok_str[0].append(" is found in macro.");
            // SimpleBugReport(&ConditionRange, err_kind, err_msg);
            isDefine = true;
            return true;
        }
    }
    return false;
}


void Rule_16_0_7::checkRule(SourceRange ConditionRange){
    // std::cout << "call checkRule" << std::endl;
        
    isDefine = false;
    bool not_flag = false;
    bool invalid;

    // make #if to string
    llvm::StringRef text_ptr = Lexer::getSourceText(CharSourceRange::getTokenRange(ConditionRange), *sm, LangOptions(), &invalid);
    std::istringstream iss(text_ptr.str());
    std::vector<string> text_str;
    std::string t1;

    while (iss >> t1) {
        // std::cout<<"text_str T1 = " << t1 <<std::endl;
        // std::cout << "t1.at(0) = " << t1.at(0) << std::endl;
        // if (t1.at(0) == '/'){
        //   // t1 = t1.substr(0, t1.length()-1);
        //   // text_str.push_back(t1);
        //   // t1 = t1.substr(1, t1.length()-1);
        //   break;
        // }
        if( t1.length() > 0){
          while( t1.length() > 0 && ((t1.at(0) == '(' ) || (t1.at(0) == '!'))){
            if((t1.at(0) == '!')){
              not_flag = true;
            }
            t1 = t1.substr(1, t1.length()-1);
          }
            std::size_t found = t1.find(')');
            if ((found != std::string::npos)){
              // std::cout << "found = " << found << "\t t1 = " << t1 << std::endl;
              t1 = t1.substr(0, found);
            }

        }
        // std::cout<<"(after clean (, !) text_str T1 = " << t1 <<std::endl;
        // if (t1.at(0) == '(' ){
        //     t1 = t1.substr(1, t1.length()-1); 
        //     // std::cout << "((" << std::endl;
        //     // tok_str.push_back( t2.substr(1, len(t2)-1) );
            
        // }
        // if (t1.at(0) == '!'){
        //     t1 = t1.substr(1, t1.length()-1); 
        //     // std::cout << "!!!!!!!" << std::endl;
        //     // tok_str.push_back( t2.substr(1, len(t2)-1) );
        // }
        if( t1.length() >= 7 ){
          if ( (t1.at(0) == 'd') && (t1.at(1) == 'e') && (t1.at(2) == 'f') && (t1.at(3) == 'i') && (t1.at(4) == 'n') && (t1.at(5) == 'e') && (t1.at(6) == 'd')){
              // return;
              if (t1.length() ==7){
                isDefined = true; 
                continue;
              }
              t1 = t1.substr(8, t1.length()-8); 
              isDefined = true;    
              // std::cout << "!!!!!!! t1 = " << t1 << std::endl;
              // tok_str.push_back( t2.substr(1, len(t2)-1) );
              
          }
        }
        text_str.push_back(t1);
    }
    // std::cout << "text_str[0] = " << text_str[0] << std::endl;

    if(traverseMacroToken(text_str[0]) == true){
      if (not_flag == true){
        // std::cout << "not flag == true" << text_str[0] << std::endl;
        check_inside = true;
      }else{
        // std::cout << "not flag == false" << text_str[0] << std::endl;
        check_inside = false;
      }
      
    }
    if(!isDefined){
        // std::cout<<"set isDefined = false"  <<std::endl;
        
        if(!isDefine){
            SimpleBugReport(&ConditionRange, err_kind, rule_desc);
            // SimpleBugReport(&ConditionRange, err_kind, err_msg);
        }
    }
    isDefined = false;  

}

REGISTER_VISITOR_CHECKER(Rule_16_0_7, "MisraCPP.16_0_7", rule_desc)
