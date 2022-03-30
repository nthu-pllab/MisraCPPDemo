#ifndef MISRA_RULE_16_HPP
#define MISRA_RULE_16_HPP

#define RULE16_REPORT(stmt)                                                    \
  do {                                                                         \
    fail = true;                                                               \
    if (!stopReport) {                                                         \
      stmt                                                                     \
    }                                                                          \
  } while (0);
#define RULE16_DONT_REPORT(stmt)                                               \
  do {                                                                         \
    if (!stopReport) {                                                         \
      stmt                                                                     \
    }                                                                          \
  } while (0);
class _Rule16 {
public:
  bool isFail() { return fail; }
  void StopReport() { stopReport = true; }
  // void setMsg(char *kind, char *desc) { err_kind = kind; err_desc = desc; }
protected:
  bool fail = false;
  bool stopReport = false;
  // char *err_kind;
  // char *err_desc;
};

class Rule_6_4_3 : public RecursiveASTVisitor<Rule_6_4_3>,
                  public MisraVisitor,
                  public _Rule16 {
public:
  using MisraVisitor::MisraVisitor;
  bool TraverseSwitchStmt(SwitchStmt *ss);
};

class Rule_6_4_4 : public RecursiveASTVisitor<Rule_6_4_4>,
                  public MisraVisitor,
                  public _Rule16 {
public:
  using MisraVisitor::MisraVisitor;
  bool VisitSwitchStmt(SwitchStmt *ss);
};


enum SwitchState { SWITCH_INIT, SWITCH_CASE, SWITCH_BREAKED, SWITCH_OTHER };
/* old impl
class Rule_16_3 : public RecursiveASTVisitor<Rule_16_3>, public MisraVisitor,
public _Rule16 { public: using MisraVisitor::MisraVisitor; bool
VisitSwitchStmt(SwitchStmt *ss); private: void CheckBreakTerminated();
  SwitchCase *CurCase;
  Stmt *lastStmt;
  State state;
};
*/
class SwitchData {
public:
  Stmt *lastStmt;
  SwitchCase *CurCase;
  SwitchState state;
};

class Rule_6_4_5 : public RecursiveASTVisitor<Rule_6_4_5>,
                  public MisraVisitor,
                  public _Rule16 {
public:
  using MisraVisitor::MisraVisitor;
  bool TraverseSwitchStmt(SwitchStmt *ss);
  bool TraverseCaseStmt(CaseStmt *sc);
  // bool VisitSwitchCase(SwitchCase *sc);
  bool VisitStmt(Stmt *s);
  // bool dataTraverseStmtPost(Stmt *s);
  //
  // TO skip
#define Rule_6_4_5_SKIP_COND_AND_LOOP_H(NAME) bool Traverse##NAME(NAME *s);
  Rule_6_4_5_SKIP_COND_AND_LOOP_H(ForStmt) Rule_6_4_5_SKIP_COND_AND_LOOP_H(DoStmt)
      Rule_6_4_5_SKIP_COND_AND_LOOP_H(WhileStmt)
          Rule_6_4_5_SKIP_COND_AND_LOOP_H(IfStmt)

              private : bool inSwitch = false;
  llvm::SmallVector<SwitchData, 3> SwitchStack;
  SwitchData *data;
};
#define MAX_SWITCH_NEST 5 // There should not be a coding style like that
class Rule_6_4_6 : public RecursiveASTVisitor<Rule_6_4_6>,
                  public MisraVisitor,
                  public _Rule16 {
public:
  using MisraVisitor::MisraVisitor;
  bool VisitSwitchStmt(SwitchStmt *ss);
  bool TraverseSwitchStmt(SwitchStmt *ss);
  bool VisitSwitchCase(SwitchCase *sc);

private:
  int8_t nested = -1;
  SwitchStmt *switchStmt[MAX_SWITCH_NEST];
  uint16_t labelTotal[MAX_SWITCH_NEST];
  uint16_t labelCount[MAX_SWITCH_NEST];

};

#define MAX_SWITCH_NEST 5 // There should not be a coding style like that
class Rule_16_5 : public RecursiveASTVisitor<Rule_16_5>,
                  public MisraVisitor,
                  public _Rule16 {
public:
  using MisraVisitor::MisraVisitor;
  bool TraverseSwitchStmt(SwitchStmt *ss);
  bool VisitSwitchCase(SwitchCase *sc);

private:
  int8_t nested = -1;
  SwitchStmt *switchStmt[MAX_SWITCH_NEST];
  uint16_t labelTotal[MAX_SWITCH_NEST];
  uint16_t labelCount[MAX_SWITCH_NEST];
};

class Rule_6_4_8 : public RecursiveASTVisitor<Rule_6_4_8>,
                  public MisraVisitor,
                  public _Rule16 {
public:
  using MisraVisitor::MisraVisitor;
  bool VisitSwitchStmt(SwitchStmt *ss);
};

class Rule_6_4_7 : public RecursiveASTVisitor<Rule_6_4_7>,
                  public MisraVisitor,
                  public _Rule16 {
public:
  using MisraVisitor::MisraVisitor;
  bool VisitSwitchStmt(SwitchStmt *ss);
};

#endif
