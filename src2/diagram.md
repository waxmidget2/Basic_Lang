# Abstract Syntax Tree (AST) Diagram

```mermaid
classDiagram
    class Token {
        +TokenID id
        +variant<int, string, char> value
    }

    class ExprAST {
        <<abstract>>
        +virtual codegen()
    }
    class NumberExprAST {
        +double Val
    }
    class VariableExprAST {
        +string Name
    }
    class BinaryExprAST {
        +char Op
        +ExprAST* LHS
        +ExprAST* RHS
    }
    class CallExprAST {
        +string Callee
        +vector<ExprAST*> Args
    }

    Token <.. ExprAST : "used by"
    ExprAST <|-- NumberExprAST
    ExprAST <|-- VariableExprAST
    ExprAST <|-- BinaryExprAST
    ExprAST <|-- CallExprAST
    BinaryExprAST o-- ExprAST : LHS/RHS
    CallExprAST o-- ExprAST : Args