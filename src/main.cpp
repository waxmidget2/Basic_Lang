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


enum Token {
 tok_eof = -1,
 tok_def = -2,
 tok_extern = -3,

 tok_identifier = -4,
 tok_number = -5,
};


class ExprAST;
class NumberExprAST;
class VariableExprAST;
class BinaryExprAST;
class CallExprAST;
class PrototypeAST;
class FunctionAST;

static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::Module> TheModule;
static std::unique_ptr<llvm::IRBuilder<>> Builder;
static std::map<std::string, llvm::Value *> NamedValues;
static std::unique_ptr<llvm::orc::LLJIT> TheJIT;
static std::unique_ptr<llvm::FunctionPassManager> TheFPM;
static std::unique_ptr<llvm::LoopAnalysisManager> TheLAM;
static std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
static std::unique_ptr<llvm::CGSCCAnalysisManager> TheCGAM;
static std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;
static std::unique_ptr<llvm::PassInstrumentationCallbacks> ThePIC;
static std::unique_ptr<llvm::StandardInstrumentations> TheSI;
static std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
static llvm::ExitOnError ExitOnErr;

// Base class for expression nodes.
class ExprAST {
 public:
   virtual ~ExprAST() = default;
   virtual llvm::Value *codegen() = 0;
};

class NumberExprAST : public ExprAST {
 double Val;

 public:
   NumberExprAST(double Val) : Val(Val) {}
   llvm::Value *codegen() override;
};

class VariableExprAST : public ExprAST {
 private:
   std::string Name;

 public:
   VariableExprAST(const std::string& Name) : Name(Name) {}
    const std::string &getName() const { return Name; }
   llvm::Value *codegen() override;
};

class BinaryExprAST : public ExprAST {
 private:
   char Op;
   std::unique_ptr<ExprAST> LHS, RHS;

 public:
   BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
       : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
   llvm::Value *codegen() override;
};

class CallExprAST : public ExprAST {
 private:
   std::string Callee;
   std::vector<std::unique_ptr<ExprAST>> Args;
 
 public:
   CallExprAST(const std::string& Callee, std::vector<std::unique_ptr<ExprAST>> Args)
       : Callee(Callee), Args(std::move(Args)) {}
   llvm::Value *codegen() override;
};

class PrototypeAST {
 private:
   std::string Name;
   std::vector<std::string> Args;
 public:
   PrototypeAST(const std::string &Name, std::vector<std::string> Args)
       : Name(Name), Args(std::move(Args)) {}

   const std::string &getName() const { return Name; }
   llvm::Function *codegen();
};

class FunctionAST {
 private:
   std::unique_ptr<PrototypeAST> Proto;
   std::unique_ptr<ExprAST> Body;
 
 public:
  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<ExprAST> Body)
      : Proto(std::move(Proto)), Body(std::move(Body)) {}
  llvm::Function *codegen();
};

class AssignExprAST : public ExprAST {
private:
  std::string VarName;
  std::unique_ptr<ExprAST> Expr;
public:
  AssignExprAST(const std::string &VarName, std::unique_ptr<ExprAST> Expr)
      : VarName(VarName), Expr(std::move(Expr)) {}
  const std::string &getName() const { return VarName; }
  llvm::Value *codegen() override;
};
 // error reporting
llvm::Value *LogErrorV(const char *Str) {
 llvm::errs() << "LLVM Error: " << Str << '\n';
 return nullptr;
}
// llvm::Function *LogErrorF(const char *Str) { // This function is declared but not used in the snippet
//  llvm::errs() << "LLVM Error: " << Str << '\n';
//  return nullptr;
// }
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

   void InitializeModuleAndPassManager() { // Renamed for clarity
     TheContext = std::make_unique<llvm::LLVMContext>();
     TheModule = std::make_unique<llvm::Module>("small_lang", *TheContext);
     if (TheJIT) {
        TheModule->setDataLayout(TheJIT->getDataLayout());
     } else {
       llvm::errs() << "Warning: TheJIT is not initialized. Module DataLayout may be incorrect or missing.\n";
     }


     Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);

     TheFPM = std::make_unique<llvm::FunctionPassManager>();
     TheLAM = std::make_unique<llvm::LoopAnalysisManager>();
     TheFAM = std::make_unique<llvm::FunctionAnalysisManager>();
     TheCGAM = std::make_unique<llvm::CGSCCAnalysisManager>();
     TheMAM = std::make_unique<llvm::ModuleAnalysisManager>();
     ThePIC = std::make_unique<llvm::PassInstrumentationCallbacks>();
     TheSI = std::make_unique<llvm::StandardInstrumentations>(*TheContext, false);
     
     TheSI->registerCallbacks(*ThePIC, TheMAM.get());

     TheFPM->addPass(llvm::InstCombinePass());
     TheFPM->addPass(llvm::ReassociatePass());
     TheFPM->addPass(llvm::GVNPass());
     TheFPM->addPass(llvm::SimplifyCFGPass());

     llvm::PassBuilder PB;
     PB.registerModuleAnalyses(*TheMAM);
     PB.registerFunctionAnalyses(*TheFAM);
     PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
     
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
       if (auto *fnIR = fnAST->codegen()) {  
         llvm::outs() << "Parsed a top-level expr:\n";
         fnIR->print(llvm::errs());
         llvm::errs() << '\n';

         if (TheJIT) {
           auto RT = TheJIT->getMainJITDylib().createResourceTracker();
           auto TSM = llvm::orc::ThreadSafeModule(std::move(TheModule), std::move(TheContext));
           ExitOnErr(TheJIT->addIRModule(RT, std::move(TSM)));
           
           // Reinitialize the Module and Context for the next inputs.
           InitializeModuleAndPassManager(); 

           auto ExprSymbol = ExitOnErr(TheJIT->lookup("__anon_expr"));

           // Cast the retrieved address to a function pointer with the correct signature.
           double (*FP)() = ExprSymbol.toPtr<double (*)()>();

           llvm::outs() << "Evaluated to: " << FP() << "\n";

           // Remove the module from the JIT now that we are done with it.
           ExitOnErr(RT->remove());
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

extern "C" DLLEXPORT double putchard(double x) {
  fputc((char)x, stderr);
  return 0;
}

extern "C" DLLEXPORT double printd(double x) {
  fprintf(stderr, "%f\n", x);
  return 0;
}

int main() {
 llvm::InitializeNativeTarget();
 llvm::InitializeNativeTargetAsmPrinter();
 llvm::InitializeNativeTargetAsmParser();

 lexer lex;
 parser my_lang(lex);
 
 auto JITBuilder = llvm::orc::LLJITBuilder();
 TheJIT = ExitOnErr(JITBuilder.create());
 if (!TheJIT) {
     llvm::errs() << "Failed to create LLJIT instance.\n";
     return 1;
 }

 my_lang.InitializeModuleAndPassManager();
 
 llvm::outs() << "ready> ";
 my_lang.getNextToken();
 
 my_lang.MainLoop();

 
 return 0;
}