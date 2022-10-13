#pragma once

#include "tokenizer.h"
#include <unordered_map>
#include <variant>


class Scope {
      public:
            Scope();
            ~Scope();
            bool hasSymbol(std::string symbol);
            Tokenizer::TokenType getSymbolType(std::string symbol);
            std::variant<double, std::string> getSymbolValue(std::string symbol);
            bool hasParentScope();
            Scope* getParentScope();
            Scope* getChildScope();
            void put(std::string newSymbol, Tokenizer::TokenType newType, std::variant<double, std::string> newValue);
            void putVal(std::string newSymbol, std::variant<double, std::string> newValue);
            void setParentScope(bool newHasParent, Scope* newParent);
            void setChildScope(bool newHasChild, Scope* newChild);
            void printSymbolTable();
      private:
            // std::unordered_map<std::string, Tokenizer::TokenType> symbolTable;
            std::unordered_map<std::string, std::pair<Tokenizer::TokenType, std::variant<double, std::string>>> symbolTable;
            bool hasParent;
            bool hasChild;
            Scope* parent;
            Scope* child;
};