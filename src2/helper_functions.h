#ifndef helper_functions_h
#define helper_functions_h

#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <algorithm>
#include "lexer.h"

using TokenContainer = tokenizer::TokenContainer;
using TokenID = tokenizer::TokenID;

namespace HF {
    auto expose_variant(const TokenContainer& token) {
        switch (token.id) {
            case TokenID::Number:
                return std::get<int>(token.token);
            case TokenID::Identifier:
                return std::get<std::string>(token.token);
            case TokenID::Operator:
                return std::get<char>(token.token);
            case TokenID::LParen:
                return '(';
            case TokenID::RParen:
                return ')';
            case TokenID::Eof:
                return -1; // or some other sentinel value
        }
    }
}



#endif // helper_functions_h