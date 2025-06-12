#pragma once

#include <string>
#include <cctype>
#include <variant>
#include <vector>

class tokenizer {
    public:
        enum class TokenID {
            Number,
            Identifier,
            Operator,

            LParen,
            RParen,
            SemiColon,

            Eof,
            Unknown
        };

        struct TokenContainer {
            std::variant<int, std::string, char> token;
            TokenID id;
            int pos;

            TokenContainer() = default; // Default constructor
            
            TokenContainer(int tok, TokenID T, int position) : token(tok), id(T), pos(position) {}
            TokenContainer(const std::string& tok, TokenID T, int position) : token(tok), id(T), pos(position) {}
            TokenContainer(char tok, TokenID T, int position) : token(tok), id(T), pos(position) {}
        };

        tokenizer(const std::string& source) : _source(source), position(0) {};  // Default constructor
        tokenizer() = default;

        std::vector<std::string> vector_container(TokenContainer& token);
        TokenContainer getNextToken();
        void reset() { position = 0; }

    private:
        std::string _source;
        int position {};
        int CurrentToken {0};
};