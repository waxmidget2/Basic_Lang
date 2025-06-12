#pragma once
#include <iostream>
#include <memory>
#include <string>

struct ExprAST {
    virtual ~ExprAST() = default; // Virtual destructor for ExprAST
    virtual void print() const = 0;
};

/*
Able to construct
*/
class NumberExprAST : public ExprAST {
    private:
        double Val;
    
    public:
        NumberExprAST(double Val) : Val(Val) {} // Constructing NumberExprAST
        void print() const override {
            std::cout << "Visited NumberExprAST, value: " << Val << '\n';
        }
};

/*

Example AST for the expression: 4 + 2 * 3

        (+)
       /   \
     (4)   (*)
          /   \
        (2)   (3)

Example AST for the expression: (4 * 9) + (10 * 3) + 4
    (+)
   /  \
 (4)  (+)
     /   \
   (*)   (*)
  /   \   |  \
(4)  (9) (10) (3)

*/

class operatorExprAST : public ExprAST {
    private:
        char Op;
        std::unique_ptr<ExprAST> LHS, RHS;
    public:
        operatorExprAST(char Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
            : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {} // Constructing operatorExprAST
};