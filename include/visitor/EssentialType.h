#ifndef MISRA_ESSENTIALTYPE_H
#define MISRA_ESSENTIALTYPE_H

#include "clang/AST/Type.h"
#include <string>

#ifndef NDEBUG
//#define R_10_VERBOSE
#endif

using namespace clang;

enum ET {
  ET_BOOL = 0x01,
  ET_CHAR = 0x02,
  ET_SIGNED = 0x04,
  ET_UNSIGNED = 0x08,
  ET_ENUM = 0x10,
  ET_FLOAT = 0x20,
  ET_OTHER = 0x40, // single node but not belong essential type: ptr, struct...
  ET_NONE          // Uninit type, invalid type
};

// Essential type category
class EssentialT {

public:
  EssentialT() : value(ET_NONE) {}

  EssentialT(Expr *e) : value(ET_NONE), width(0) { setET(e); }

  enum ET setET(Expr *e) {

#ifdef R_10_VERBOSE
    llvm::outs() << "-----------------------"
                 << "\n";
    e->dumpColor();
#endif
    e = e->IgnoreParenImpCasts();
    value = Expr_to_Essential(e);
    setType(e);

#ifdef R_10_VERBOSE
    llvm::outs() << "Essential Type: " << EssentialT::getStr(value) << "\n";
    llvm::outs() << "-----------------------"
                 << "\n\n";
#endif

    return value;
  }

  // This should be called after value set
  void setType(Expr *e) {
    //  Directly desugar? -> No We need info like type width
    type = e->getType().getTypePtr();

    if (value == ET_ENUM) {
      // For enum type def tyoe
      while (const TypedefType *tt = dyn_cast<TypedefType>(type)) {
        // if ( type->isSugared()) {
        type = tt->desugar().getTypePtr();
        //}
      }

      // For ElaboratedType enum, because we have to compare with
      // EnumConstantDecl
      if (const ElaboratedType *et = dyn_cast<ElaboratedType>(type)) {
        if (et->isSugared()) {
          type = et->desugar().getTypePtr();
        }
      }

      // For EnumConstantDecl
      if (DeclRefExpr *dre = dyn_cast<DeclRefExpr>(e)) {

        ValueDecl *nd = dre->getDecl();

        if (EnumDecl *ed = dyn_cast<EnumDecl>(nd->getDeclContext())) {
          type = ed->getTypeForDecl();
          if ((ed->getIdentifier())) {
            // llvm::outs() << II->getName()  << "\n";
          } else {
            // anonymous enum
            if (isa<EnumConstantDecl>(nd)) {
              value = ET_SIGNED;
              width = 64; // Give biggest width
              // show you are const FIXME
            }
          }
        } else {
#ifdef R_10_VERBOSE
          llvm::outs() << "Failed to get type of [EnumConstantDecl]\n";
          nd->dumpColor();
#endif
        }
      }
    } else { // To get data width except enum

      while (const TypedefType *tt = dyn_cast<TypedefType>(type)) {
        if (tt->isSugared()) {
          type = tt->desugar().getTypePtr();
        } else {
          break;
        }
      }
      if (!isa<BuiltinType>(type)) {
        goto end;
      }
      const BuiltinType *bt = dyn_cast<BuiltinType>(type);
      switch (bt->getKind()) {
      case BuiltinType::Bool:
      case BuiltinType::Char_U:
      case BuiltinType::Char_S:
      case BuiltinType::UChar:
      case BuiltinType::SChar:
        width = 8;
        break;
      // case BuiltinType::WChar_U:
      // case BuiltinType::WChar_S:
      case BuiltinType::Char16:
      case BuiltinType::UShort:
      case BuiltinType::Short:
      case BuiltinType::Float16:
        width = 16;
        break;
      case BuiltinType::Char32:
      case BuiltinType::Int:
      case BuiltinType::UInt:
      case BuiltinType::Float:
        width = 32;
        break;
      case BuiltinType::Long:
      case BuiltinType::ULong:
      case BuiltinType::Double:
        width = 64;
        break;
      case BuiltinType::Int128:
      case BuiltinType::UInt128:
      case BuiltinType::LongLong:
      case BuiltinType::ULongLong:
      case BuiltinType::LongDouble:
      case BuiltinType::Float128:
        width = 128;
        break;
      default: {
#ifdef R_10_VERBOSE
        llvm::outs() << "Unknown width \n";
#endif
        width = 0;
      }
      }
#ifdef R_10_VERBOSE
      llvm::outs() << "Width:" << (int)width << " type \n";
#endif
    }
  end:;
#ifdef R_10_VERBOSE
    llvm::outs() << "Clang Type:\n";
    type->dump();
#endif
  }

  static std::string getStr(enum ET Cat) {
    switch (Cat) {
#define CASE(CAT)                                                              \
  case CAT:                                                                    \
    return #CAT;
      CASE(ET_BOOL)
      CASE(ET_CHAR)
      CASE(ET_SIGNED)
      CASE(ET_UNSIGNED)
      CASE(ET_ENUM)
      CASE(ET_FLOAT)
      CASE(ET_OTHER)
      CASE(ET_NONE)
#undef CASE
    default:
      assert(0 && "Invalid essential type");
    }
  }

  bool isCompoundExpr() { return CompoundExpr; }
  bool isInvalid() { return (value == ET_OTHER) || (value == ET_NONE); }

  uint8_t getWitdth() { return width; }
  //  QualType qtype;
  // IdentifierInfo* type_id;
  enum ET value;    // Essential type
  uint8_t width;    // data width ,except enum is 0 (unknwon)
  const Type *type; // clang type

private:
  bool CompoundExpr = false;

  enum ET Expr_to_Essential(Expr *e) {
    if (isa<DeclRefExpr>(e)) {

      DeclRefExpr *dre = dyn_cast<DeclRefExpr>(e);
      ValueDecl *nd = dre->getDecl();

      // Deal with EnumConstantDecl type record
      // TODO An enumeration constant from an anonymous enum
      // has essentially signed type.
      if (isa<EnumConstantDecl>(nd)) {

#ifdef R_10_VERBOSE
        llvm::outs() << "EnumConstantDecl\n";
        if (EnumDecl *ed = dyn_cast<EnumDecl>(nd->getDeclContext())) {
          if (TagDecl *td = dyn_cast<TagDecl>(ed)) {
            llvm::outs() << td->isBeingDefined() << td->isCompleteDefinition()
                         << "\n";
            if (TypedefNameDecl *tnd = td->getTypedefNameForAnonDecl()) {
              tnd->dump();
            }
          }
        }
#endif

        return ET_ENUM;
      }

      return QualType_to_Essential(dre->getType());
    }

    // IntegerLiteral
    if (isa<IntegerLiteral>(e)) {
      return QualType_to_Essential(dyn_cast<IntegerLiteral>(e)->getType());
    }
    if (UnaryOperator *uo = dyn_cast<UnaryOperator>(e)) {
      if (uo->getOpcode() == UO_Minus || uo->getOpcode() == UO_Plus) {
        if (isa<IntegerLiteral>(uo->getSubExpr()->IgnoreParenImpCasts())) {
          return QualType_to_Essential(e->getType());
        }
      }
    }

    // *PTR
    if (UnaryOperator *uo = dyn_cast<UnaryOperator>(e)) {
      if (uo->getOpcode() == UO_Deref) {
        return QualType_to_Essential(e->getType());
      }
    }

    // struct A->member, string
    if (isa<MemberExpr>(e) || isa<StringLiteral>(e)) {
      return QualType_to_Essential(e->getType());
    }

    // Array[ ]
    if (isa<ArraySubscriptExpr>(e)) {
      return QualType_to_Essential(e->getType());
    }

    // Function call
    if (isa<CallExpr>(e)) {
      return QualType_to_Essential(e->getType());
    }

    // Ignore Ctyle or not: No this include type wide info
    if (isa<CStyleCastExpr>(e)) {
      return QualType_to_Essential(e->getType());
    }
    if (isa<CharacterLiteral>(e)) {
      return ET_CHAR;
    }
    if (isa<FloatingLiteral>(e)) {
      return ET_FLOAT;
    }
    // Ignore check, direct getType

    // Skip compound statement
    // return QualType_to_Essential(e->getType());

    // Compound statement is not belong essential type
    CompoundExpr = true;
    return QualType_to_Essential(e->getType());
    // return ET_NONE;
  }

  // We need functions to convert these CStyleCastExpr, IntegerLiteral,
  // DeclRefExpr

  // FIXME make sure you know all type function
  // Or know clang Types
  // clang/include/clang/AST/BuiltinTypes.def

  enum ET QualType_to_Essential(QualType qt) {

    // QualType Ty = dre->getType();
    const Type *T = qt.getTypePtr();

    // FIXME Deal with sugar here  uint8_t
    if (!isa<BuiltinType>(T)) {

      if (T->isEnumeralType()) {
        return ET_ENUM;
      }
      if (isa<TypedefType>(T)) {
        const TypedefType *tt = dyn_cast<TypedefType>(T);

        return QualType_to_Essential(tt->desugar());
      }

#ifdef R_10_VERBOSE
      qt.dump();
      llvm::outs() << "Unkonwn Non-built-in type\n";
#endif
      return ET_OTHER;
    }

    // llvm::outs() << "Built-in type\n";
    const BuiltinType *bt = dyn_cast<const BuiltinType>(T);

    if (T->isBooleanType()) {
      return ET_BOOL;
    } else if ((bt->getKind() == BuiltinType::Char_U ||
                bt->getKind() == BuiltinType::Char_S)) {
      // unsigned char is also this type
      return ET_CHAR;
    } else if (T->isSignedIntegerType()) {
      // signed char is also this type
      return ET_SIGNED;
    } else if (T->isUnsignedIntegerType()) {
      // A complete enum is also this type
      return ET_UNSIGNED;
    } else if (T->isFloatingType()) {
      // TODO 0.0f does not go here
      return ET_FLOAT;
    } else {

      return ET_OTHER;
    }
  }
};

#endif
