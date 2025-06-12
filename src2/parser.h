#pragma once

#include <iostream>
#include <cctype>
#include <variant>
#include <string>
#include <vector>
#include <queue>
#include "lexer.h"

class parser {
public:
    using TokenContainer = tokenizer::TokenContainer;
    using TokenID = tokenizer::TokenID;

    parser(const std::string& source) : compiler_source(source) {}  
    
    void print_bucket(const std::queue<TokenContainer>& queue) {
        std::queue<TokenContainer> temp = queue;
        while(!temp.empty()) {
            const TokenContainer& token = temp.front();
            if (std::holds_alternative<int>(token.token)) {
                std::cout << std::get<int>(token.token) << " ";
                std::cout << "(" << token.pos << ") ";
            } else if (std::holds_alternative<std::string>(token.token)) {
                std::cout << std::get<std::string>(token.token) << " ";
                std::cout << "(" << token.pos << ") ";
            } else if (std::holds_alternative<char>(token.token)) {
                std::cout << std::get<char>(token.token) << " ";
                std::cout << "(" << token.pos << ") ";
            }
            temp.pop();
        }
    }
    std::queue<TokenContainer> IdQueue;
    std::queue<TokenContainer> NumQueue;
    std::queue<TokenContainer> OpQueue;
    std::queue<TokenContainer> LParen;
    std::queue<TokenContainer> RParen;
    std::queue<TokenContainer> SemiColon;

    // Parses the input string and returns a vector of tokens
    void parse();

    private:
    std::string compiler_source; 
};