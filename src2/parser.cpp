#include "parser.h"

void parser::parse() {
    tokenizer compile(compiler_source);

    TokenContainer token = compile.getNextToken();
    
    while (token.id != TokenID::Eof) {
        switch (token.id) {
            case TokenID::Number:
                NumQueue.push(token);
                break;
            case TokenID::Identifier:
                IdQueue.push(token);
                break;
            case TokenID::Operator:
                OpQueue.push(token);
                break;
            case TokenID::LParen:
                LParen.push(token);
                break;
            case TokenID::RParen:
                RParen.push(token);
                break;
            case TokenID::SemiColon:
                SemiColon.push(token);
                break;
            default:
                std::cout << "We are fucked." << std::endl;
        }
        token = compile.getNextToken();
    }
}

// lets do this linerally first lol

// {"1", "+", "3", "+", "(", "2", "*", "4", ")"}
// --> "1 + 3" => "4 + (2 * 4)" --> "4 + 8" => "12"
