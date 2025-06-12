// LLVM INCLUDES
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Support/Error.h" 
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/Support/DynamicLibrary.h"
// END LLVM INCLUDES

// C++ INCLUDES
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <iostream> 
// END C++ INCLUDES

// BEGIN TOKEN ENUMERATION
enum Token {
 tok_eof = -1,
 tok_def = -2,
 tok_extern = -3,

 tok_identifier = -4,
 tok_number = -5,
};
// END TOKEN ENUMERATION

// BEGIN AST FORWARD DECLARATIONS
class ExprAST;
class NumberExprAST;
class VariableExprAST;
class BinaryExprAST;
class CallExprAST;
class PrototypeAST;
class FunctionAST;
// END AST FORWARD DECLARATIONS

// BEGIN LLVM CONTEXT
static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::Module> TheModule;
static std::unique_ptr<llvm::IRBuilder<>> Builder;
static std::unique_ptr<llvm::orc::LLJIT> TheJIT;
static std::unique_ptr<llvm::FunctionPassManager> TheFPM;
static std::unique_ptr<llvm::LoopAnalysisManager> TheLAM;
static std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
static std::unique_ptr<llvm::CGSCCAnalysisManager> TheCGAM;
static std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;
static std::unique_ptr<llvm::PassInstrumentationCallbacks> ThePIC;
static std::unique_ptr<llvm::StandardInstrumentations> TheSI;
static std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
static std::map<std::string, llvm::Value *> NamedValues;
static llvm::ExitOnError ExitOnErr;
// END LLVM CONTEXT


// ---------------------------------BEGIN AST DEFINITION ------------------------------------------

/*
  Comments with '*' are used to explain the purpose of each class and method.

  The AST (Abstract Syntax Tree) is a representation of the structure of the source code.
  Each class represents a different type of expression or statement in the language.
*/

// * AST Base Class.
struct ExprAST {
  // [1st] * LLVM Code generation, inherited by derived classes.
  // [2nd] Virtual destructor to ensure proper cleanup of derived classes.
    
  virtual ~ExprAST() = default; // [1st]

  virtual llvm::Value *codegen() = 0; // [2nd]
}; 
// FOLLOWING CLASSES USE EXPRAST: class foobar : public ExprAST { body }; 

// * Expression AST Nodes.
class NumberExprAST : public ExprAST {
  private:
    // [1st] Accepts a value for a node.
    
    double Val; // [1st]
  
  public:
    // [1st] Constructor for NumberExprAST, initializes the value.
    // [2nd] * LLVM Code Generation function for NumberExprAST.
    
    NumberExprAST(double Val) : Val(Val) {} // [1st]

    llvm::Value *codegen() override; // [2nd]
};

// * VariableExprAST AST Nodes.
class VariableExprAST : public ExprAST {
private:
  // [1st] Holds the name of the variable.

  std::string Name; // [1st]

public:
  // [1st] Constructor for VariableExprAST, initializes the variable name. 
  // [2nd] * Getter function for variable name.
  // [3rd] * LLVM Code Generation function for VariableExprAST.


  VariableExprAST(const std::string& Name) : Name(Name) {} // [1st]

  const std::string &getName() const { return Name; } // [2nd]
  
  llvm::Value *codegen() override; // [3rd]
};

// * BinaryExprAST represents a binary operation
class BinaryExprAST : public ExprAST {
private:
  // [1st] Holds the operator for the binary expression, e.g., '+', '-', '*', '<'.
  // [2nd] Holds the left-hand side and right-hand side expressions.
  //       e.x., for 'a + b', lhs is 'a' and rhs is 'b'.
  
  char Op; // [1st]

  std::unique_ptr<ExprAST> LHS, RHS; // [2nd]

public:
  // [1st] Constructor for BinaryExprAST, initializes the operator and the two expressions.
  //       Owner uses std::move to transfer. [IMPORTANT]
  // [2nd] * LLVM Code Generation function for BinaryExprAST.

  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {} // [1st]

  llvm::Value *codegen() override; // [2nd]
};

// * CallExprAST represents a function call expression
class CallExprAST : public ExprAST {
private:
  // [1st] Holds callee name (function name).
  // [2nd] Holds arguments for the function call.

  std::string Callee; // [1st]

  std::vector<std::unique_ptr<ExprAST>> Args; // [2nd]
 
 public:
    // [1st] Constructor for CallExprAST
    // [2nd] * LLVM Code generation function.

   CallExprAST(const std::string& Callee, std::vector<std::unique_ptr<ExprAST>> Args)
       : Callee(Callee), Args(std::move(Args)) {} // [1st]

   llvm::Value *codegen() override; // [2nd]
};

// * PrototypeAST represents a function prototype
class PrototypeAST {
private:
  // [1st] Holds the name of the function.
  // [2nd] Holds the argument names for the function prototype.

  std::string Name; // [1st]

  std::vector<std::string> Args; // [2nd]

public:
  // [1st] Constructor for PrototypeAST, initializes the function name and arguments.
  // [2nd] * Getter function for the function name.
  // [3rd] * LLVM Code generation function for PrototypeAST.

  PrototypeAST(const std::string &Name, std::vector<std::string> Args)
      : Name(Name), Args(std::move(Args)) {} // [1st]

  const std::string &getName() const { return Name; } // [2nd]

  llvm::Function *codegen(); // [3rd]
};

class FunctionAST {
private:
  // [1st] Holds the function prototype.
  // [2nd] Holds the function body (expression).

  std::unique_ptr<PrototypeAST> Proto; // [1st]

  std::unique_ptr<ExprAST> Body; // [2nd]
 
public:
  // [1st] Constructor for FunctionAST, initializes the prototype and body.
  // [2nd] * LLVM Code generation function for FunctionAST.
  // [3rd] * Getter function for the prototype.

  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<ExprAST> Body)
      : Proto(std::move(Proto)), Body(std::move(Body)) {} // [1st]

  llvm::Function *codegen(); // [2nd]
  const PrototypeAST *getProto() const { return Proto.get(); } // [3rd]
};

class AssignExprAST : public ExprAST {
private:
  // [1st] Holds the variable name for assignment.
  // [2nd] Holds the expression to assign to the variable.
  
  std::string VarName; // [1st]

  std::unique_ptr<ExprAST> Expr; // [2nd]
public:
  // [1st] Constructor for AssignExprAST, initializes the variable name and expression.
  // [2nd] * Getter function for the variable name.
  // [3rd] * LLVM Code generation function for AssignExprAST.

  AssignExprAST(const std::string &VarName, std::unique_ptr<ExprAST> Expr)
      : VarName(VarName), Expr(std::move(Expr)) {} // [1st]

  const std::string &getName() const { return VarName; } // [2nd]

  llvm::Value *codegen() override; // [3rd]
};

// -------------------------------------END AST DEFINITION ------------------------------------------

// * LLVM ERROR HANDLING
llvm::Value *LogErrorV(const char *Str) {
 llvm::errs() << "LLVM Error: " << Str << '\n';
 return nullptr;
}
// * END LLVM ERROR HANDLING

// ---------------------------------BEGIN CODEGEN IMPLEMENTATIONS--------------------------------
llvm::Value *NumberExprAST::codegen() {
 return llvm::ConstantFP::get(*TheContext, llvm::APFloat(Val));
}
llvm::Value *VariableExprAST::codegen() {
 llvm::Value *V = NamedValues[Name];
 if (!V) {
   return LogErrorV("Unknown variable name");
 }
 return V;
}

llvm::Value *BinaryExprAST::codegen() {
 llvm::Value *L = LHS->codegen();
 llvm::Value *R = RHS->codegen();
 if(!L || !R) {
   return nullptr;
 }

 switch (Op) {
   case '+' :
     return Builder->CreateFAdd(L, R, "addtmp");
   case '-':
     return Builder->CreateFSub(L, R, "subtmp");
   case '*':
     return Builder->CreateFMul(L, R, "multmp");
   case '<':
     L = Builder->CreateFCmpULT(L, R, "cmptmp");
     return Builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*TheContext), "booltmp");
   default:
     return LogErrorV("invalid binary operator");
 }
}
llvm::Value *AssignExprAST::codegen() {
 llvm::Value *Val = Expr->codegen();
 if (!Val)
   return nullptr;
 NamedValues[VarName] = Val;
 return Val;
}
llvm::Value *CallExprAST::codegen() {
 // Look up the name in the global module table.
 llvm::Function *CalleeF = TheModule->getFunction(Callee);
 if (!CalleeF) {
   // If not, check if it's a known prototype.
   auto FI = FunctionProtos.find(Callee);
   if (FI != FunctionProtos.end()) {
       CalleeF = FI->second->codegen();
   } else {
       return LogErrorV("Unknown Function Referenced");
   }
 }

 if (CalleeF->arg_size() != Args.size()) {
   return LogErrorV("Incorrect # args passed");
 }
 std::vector<llvm::Value*> ArgsV;
 for (unsigned i {0}, e = Args.size(); i != e; ++i) {
   ArgsV.push_back(Args[i]->codegen());
   if(!ArgsV.back()) {
     return nullptr;
   }
 }
 return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

llvm::Function *PrototypeAST::codegen() {
 std::vector<llvm::Type *> Doubles(Args.size(), llvm::Type::getDoubleTy(*TheContext));
 llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(*TheContext), Doubles, false);

 llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name, TheModule.get());
 unsigned idx = 0;
 for (auto &Arg : F->args()) {
   Arg.setName(Args[idx++]);
 }
 return F;
}
 
llvm::Function *FunctionAST::codegen() {
 llvm::Function *TheFunction = TheModule->getFunction(Proto->getName());

 if(!TheFunction) {
   TheFunction = Proto->codegen();
 }
 
 if(!TheFunction) {
   return nullptr;
 }
 
 llvm::BasicBlock *BB = llvm::BasicBlock::Create(*TheContext, "entry", TheFunction);
 Builder->SetInsertPoint(BB);

 NamedValues.clear();
 for (auto &Arg : TheFunction->args()) {
   NamedValues[std::string(Arg.getName())] = &Arg;
 }
 if (llvm::Value *RetVal = Body->codegen()) {
   Builder->CreateRet(RetVal);

   llvm::verifyFunction(*TheFunction);
   
   // Run the optimizer on the function.
   if (TheFPM) {
       TheFPM->run(*TheFunction, *TheFAM);
   }

   return TheFunction;
 }
 // Error reading body, remove function.
 TheFunction->eraseFromParent();
 return nullptr;
}

struct lexer {
   std::string IdentifierStr;
   double NumVal;
   int CurTok;
   int LastChar;

   lexer() : CurTok(0), LastChar(' ') {} // Initialize CurTok
   int gettok() {
       while (isspace(LastChar)) { LastChar = getchar(); }

     if (isalpha(LastChar)) { 
       IdentifierStr.clear();
       IdentifierStr += static_cast<char>(LastChar);

       while(isalnum((LastChar = getchar()))) { // Corrected: assign then check
         IdentifierStr += static_cast<char>(LastChar);
       }

       if (IdentifierStr == "fn") {
         return tok_def;
       }
       if (IdentifierStr == "incl") {
         return tok_extern;
       }
       return tok_identifier;
     }

     if (isdigit(LastChar) || LastChar == '.') {
       std::string NumStr;
       do {
         NumStr += static_cast<char>(LastChar);
         LastChar = getchar();
       } while (isdigit(LastChar) || LastChar == '.');

       NumVal = std::stod(NumStr);
       return tok_number;
     }

     if (LastChar == '#') { // Comment until end of line
       do {
         LastChar = getchar();
       } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

       if (LastChar != EOF) {
         return gettok();
       }
     }

     if (LastChar == EOF) {
       return tok_eof;
     }

     int ThisChar = LastChar;
     LastChar = getchar();
     return ThisChar;
   }
   const std::string& getIdentifierStr() const { return IdentifierStr; }
   double getNumVal() const { return NumVal; }
   // void setCurTok(int tok) { CurTok = tok; } // Not used in this version
   int getCurTok() const { return CurTok; }
};

class parser {
 private:
   lexer& m_lexer;

 public:
   std::map<char, int> BinopPrecedence;
   parser(lexer& lexer_instance) : m_lexer(lexer_instance) {
     BinopPrecedence['<'] = 10;
     BinopPrecedence['+'] = 20;
     BinopPrecedence['-'] = 20; // Same precedence as +
     BinopPrecedence['*'] = 40; // Higher precedence
   }

   
   int getTokPrecedence() {
     if (!isascii(m_lexer.getCurTok())) {
       return -1;
     }

     // Make sure it's a declared binop.
     int TokPrec = BinopPrecedence[static_cast<char>(m_lexer.getCurTok())];
     if (TokPrec <= 0) return -1;
     return TokPrec;
   }
   
   int getNextToken() {
     return m_lexer.CurTok = m_lexer.gettok();
   }

   template <typename T>
   static std::unique_ptr<T> LogError(const char *str)
   {
     llvm::errs() << "Error: " << str << '\n';
     return nullptr;
   }

   std::unique_ptr<ExprAST> ParsePrimary() {
    switch(m_lexer.getCurTok()) {
      default:
        return LogError<ExprAST>("Unknown token when expecting an expression");
      case tok_identifier: {
        auto LHS = ParseIdentifierExpr();
        if (m_lexer.getCurTok() == '=') {
          return ParseAssignmentExpr(std::move(LHS), *this);
        }
        return LHS;
      }
      case tok_number:
        return ParseNumberExpr();
      case '(':
        return ParseParenExpr();
    }
  }

   std::unique_ptr<ExprAST> ParseExpression() {
     auto LHS = ParsePrimary();
     if (!LHS) {
       return nullptr;
     }

     return ParseBinOpRHS(0, std::move(LHS));
   }

   std::unique_ptr<ExprAST> ParseNumberExpr() {
     auto Result = std::make_unique<NumberExprAST>(m_lexer.getNumVal());
     getNextToken(); // consume the number
     return std::move(Result);
   } 
   
   std::unique_ptr<ExprAST> ParseParenExpr() {
     getNextToken(); // eat (.
     auto V = ParseExpression();
     
     if (!V) {
       return nullptr;
     }

     if (m_lexer.getCurTok() != ')') {
       return LogError<ExprAST>("expected ')'");
     }
     
     getNextToken(); // eat ).
     return V;
   }
  std::unique_ptr<ExprAST> ParseAssignmentExpr(std::unique_ptr<ExprAST> LHS, parser &p) {
    auto *Var = dynamic_cast<VariableExprAST*>(LHS.get());
    if (!Var)
      return nullptr;
    p.getNextToken(); // eat '='
    auto RHS = p.ParseExpression();
    if (!RHS)
      return nullptr;
    return std::make_unique<AssignExprAST>(Var->getName(), std::move(RHS));
  }
   std::unique_ptr<ExprAST> ParseIdentifierExpr() {
     std::string IdName = m_lexer.getIdentifierStr();

     getNextToken(); // eat identifier.

     if (m_lexer.getCurTok() != '(') { // Simple variable ref.
       return std::make_unique<VariableExprAST> (IdName);
     }

     // Call.
     getNextToken(); // eat (
     std::vector<std::unique_ptr<ExprAST>> Args;
     
     if(m_lexer.getCurTok() != ')') {
       while (true) {
         if (auto Arg = ParseExpression()) {
           Args.push_back(std::move(Arg));
         } else {
           return nullptr;
         }
         if (m_lexer.getCurTok() == ')') {
           break;
         }
         if (m_lexer.getCurTok() != ',') {
           return LogError<ExprAST>("Expected ')' or ',' in argument list");
         }
         getNextToken();
       }
     }
     getNextToken(); // Eat the ')'.

     return std::make_unique<CallExprAST> (IdName, std::move(Args));
   }
   std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS) {
     while (true) {
       int TokPrec = getTokPrecedence();

       if (TokPrec < ExprPrec) {
         return LHS;
       }

       int BinOp = m_lexer.getCurTok();
       getNextToken(); // eat binop

       auto RHS = ParsePrimary();
       if (!RHS) {
         return nullptr;
       }

       int NextTokPrec = getTokPrecedence();
       if (TokPrec < NextTokPrec) {
         RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
         if (!RHS) {
           return nullptr;
         }
       }
       LHS = std::make_unique<BinaryExprAST>(static_cast<char>(BinOp), std::move(LHS), std::move(RHS));
     }
   }
   std::unique_ptr<PrototypeAST> ParsePrototype() {
     if (m_lexer.getCurTok() != tok_identifier) {
       return LogError<PrototypeAST>("Expected function name in prototype!");
     }
     std::string fnName = m_lexer.getIdentifierStr();
     getNextToken();

     if (m_lexer.getCurTok() != '(') {
       return LogError<PrototypeAST>("Expected '(' in prototype!");
     }

     std::vector<std::string> ArgNames;
     // eat '(', then look for identifiers for arguments
     while(getNextToken() == tok_identifier) { 
       ArgNames.push_back(m_lexer.getIdentifierStr());
     }
     if (m_lexer.getCurTok() != ')') {
       return LogError<PrototypeAST>("Expected ')' in prototype!");
     }
     getNextToken(); // eat ')'

     return std::make_unique<PrototypeAST> (fnName, std::move(ArgNames));
   }
   std::unique_ptr<FunctionAST> ParseDefinition() {
     getNextToken(); // eat fn.
     auto Proto = ParsePrototype();
     if (!Proto) return nullptr;

     if(auto E = ParseExpression()) {
       return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
     }
     return nullptr;
   }
   std::unique_ptr<PrototypeAST> ParseExtern() {
     getNextToken(); // eat incl.
     return ParsePrototype();
   }
   std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
     if (auto E = ParseExpression()) {
       auto Proto = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>());
       return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
     }
     return nullptr;
   }

   void InitializeModuleAndPassManager() {
    TheContext = std::make_unique<llvm::LLVMContext>();
    TheModule = std::make_unique<llvm::Module>("small_lang", *TheContext);
    TheModule->setDataLayout(TheJIT->getDataLayout());
    Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);

    TheFPM = std::make_unique<llvm::FunctionPassManager>();
    TheLAM = std::make_unique<llvm::LoopAnalysisManager>();
    TheFAM = std::make_unique<llvm::FunctionAnalysisManager>();
    TheCGAM = std::make_unique<llvm::CGSCCAnalysisManager>();
    TheMAM = std::make_unique<llvm::ModuleAnalysisManager>();
    ThePIC = std::make_unique<llvm::PassInstrumentationCallbacks>();
    TheSI = std::make_unique<llvm::StandardInstrumentations>();

    TheSI->registerCallbacks(*ThePIC);

    TheFPM->addPass(llvm::InstCombinePass());
    TheFPM->addPass(llvm::ReassociatePass());
    TheFPM->addPass(llvm::SimplifyCFGPass());

    llvm::PassBuilder PB;
    PB.registerModuleAnalyses(*TheMAM);
    PB.registerFunctionAnalyses(*TheFAM);
    PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
    PB.registerCGSCCAnalyses(*TheCGAM);
    PB.registerLoopAnalyses(*TheLAM);
    PB.registerFunctionAnalyses(*TheFAM);
    PB.registerModuleAnalyses(*TheMAM);

   }

   void HandleDefinition() {
     if (auto fnAST = ParseDefinition()) {
       if (auto *fnIR = fnAST->codegen()) {
         llvm::outs() << "Parsed a function definition:\n";
         fnIR->print(llvm::errs());
         llvm::errs() << '\n';
       }
     } else {
       // Skip token for error recovery.
       getNextToken();
     }
   }
   void HandleExtern() {
     if(auto ProtoAST = ParseExtern()) {
       if (auto *fnIR = ProtoAST->codegen()) {
         llvm::outs() << "Parsed an extern:\n";
         fnIR->print(llvm::errs());
         llvm::errs() << '\n';
         FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST); 
       }
     } else {
       // Skip token for error recovery.
       getNextToken();
     }
  }
  void HandleTopLevelExpression() {
    if (auto fnAST = ParseTopLevelExpr()) {
        // Check if the top-level expression is an assignment
        if (dynamic_cast<AssignExprAST*>(fnAST->getProto()->getName().c_str() == nullptr)) {
            llvm::outs() << "Assignment at top level is not supported.\n";
            return;
        }
        if (auto *fnIR = fnAST->codegen()) {  
            llvm::outs() << "Parsed a top-level expr:\n";
            fnIR->print(llvm::errs());
            llvm::errs() << '\n';

            if (TheJIT) {
                ExitOnErr(TheJIT->addIRModule(
                    llvm::orc::ThreadSafeModule(std::move(TheModule), std::move(TheContext))
                ));

                InitializeModuleAndPassManager(); 

                auto ExprSymbol = ExitOnErr(TheJIT->lookup("__anon_expr"));
                auto Addr = ExprSymbol.getAddress();
                auto *FP = reinterpret_cast<double (*)()>(static_cast<uintptr_t>(Addr));
                llvm::outs() << "Evaluated to: " << FP() << "\n";
            }
        }
    } else {
        getNextToken();
    }
   }
  void MainLoop() {
    while (true) {
      llvm::outs() << "ready> ";
      switch (m_lexer.getCurTok()) { 
        case tok_eof:
          llvm::outs() << "Exiting.\n";
          return;
        case ';': // ignore top-level semicolons.
          getNextToken();
          break;
        case tok_def:
          HandleDefinition();
          break;
        case tok_extern:
          HandleExtern();
          break;
        default:
          HandleTopLevelExpression();
          break;
       }
     }
   }
};

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

extern "C" double printd(double);
DLLEXPORT double printd(double x) {
  std::cout << x << std::endl;
  return x;
}

static auto *printd_addr = (void*)&printd;


int main() {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  lexer lex;
  parser my_lang(lex);

  TheJIT = ExitOnErr(llvm::orc::LLJITBuilder().create());
  if (!TheJIT) {
    llvm::errs() << "Failed to create LLJIT instance.\n";
    return 1;
  }

  // Register host process symbols for JIT (LLVM 10 way)
  llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

  my_lang.InitializeModuleAndPassManager();

  llvm::outs() << "ready> ";
  my_lang.getNextToken();

  my_lang.MainLoop();

  return 0;
}