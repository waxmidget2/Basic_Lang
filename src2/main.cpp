#include <iostream>
#include <string>
#include "lexer.h"
#include "parser.h"
#include "ast.h"


int main() {
    std::string input;
    std::cout << "Enter an expression: ";
    std::getline(std::cin, input);

    if (input.empty()) {
        std::cerr << "Error: Input cannot be empty." << std::endl;
        return 1;
    } else if (input.size() > 1000) {
        std::cerr << "Error: Input is too long." << std::endl;
        return 1;
    }
    
    parser parser(input);
    parser.parse();
    std::cout << '\n';

    parser.print_bucket(parser.IdQueue);
    parser.print_bucket(parser.NumQueue);
    parser.print_bucket(parser.OpQueue);
    parser.print_bucket(parser.LParen);
    parser.print_bucket(parser.RParen);
        std::cout << '\n';

    return 0;
}