#include "lexer.h"
#include "ast.h"
#include <cctype>
#include <string>
#include <iostream>

tokenizer::TokenContainer tokenizer::getNextToken() {
    if (position >= _source.size()) {
        return TokenContainer(0, TokenID::Eof, position);
    } // end of input

    while (position < _source.size() && std::isspace(_source[position])) {
        position++; 
    } // skip whitespace

    char currentCharacter = _source[position];
    if (std::isdigit(currentCharacter)) {
        std::string NumberContainer = "";
        while (position < _source.size() && std::isdigit(currentCharacter)) {
            NumberContainer += _source[position];
            position++;
            if (position < _source.size()) {
                currentCharacter = _source[position];
            }
        }
        return TokenContainer(std::stoi(NumberContainer), TokenID::Number, position);
    } // checks for numbers

    if (std::isalpha(currentCharacter)) {
        std::string IdentifierContainer = "";
        while(position < _source.size() && std::isalnum(currentCharacter)) {
            IdentifierContainer += _source[position];
            position++;
            if (position < _source.size()) {
                currentCharacter = _source[position];
            }
        }
        return TokenContainer(std::string(IdentifierContainer), TokenID::Identifier, position);
    } // checks for identifiers

    if (currentCharacter == '(') {
        position++;
        return TokenContainer(currentCharacter, TokenID::LParen, position);
    } // left parenthesis

    if (currentCharacter == ')') {
        position++;
        return TokenContainer(currentCharacter, TokenID::RParen, position);
    } // right parenthesis

    if (currentCharacter == '+' || currentCharacter == '-' || currentCharacter == '*' || currentCharacter == '/') {
        position++;
        return TokenContainer(currentCharacter, TokenID::Operator, position);
    } // checks for operators
    if (currentCharacter == ';') {
        position++;
        return TokenContainer(currentCharacter, TokenID::SemiColon, position);
    }

    // Unknown token
    ++position;
    return TokenContainer(0, TokenID::Unknown, position);
}
