#include "scope.h"
#include <stdexcept>
#include <iomanip>

// Scope Class
Scope::Scope() :
    hasParent(false),
    parent(NULL)
    {}

Scope::~Scope() {
    std::cout << "Scope destructed" << std::endl;
}

bool Scope::hasSymbol(std::string symbol) {
    return symbolTable.count(symbol);
}

Tokenizer::TokenType Scope::getSymbolType(std::string symbol) {
    try {
        return symbolTable.at(symbol).first;
    } catch (const std::out_of_range& oor) {
        fprintf(stderr, "Symbol doesn't exist in symbol table.\n");
    }
}
std::variant<double, std::string> Scope::getSymbolValue(std::string symbol) {
    try {
        return symbolTable.at(symbol).second;
    } catch (const std::out_of_range& oor) {
        fprintf(stderr, "Symbol doesn't exist in symbol table.\n");
    }
}

bool Scope::hasParentScope() {
    return hasParent;
}

Scope* Scope::getParentScope() {
    return parent;
}

Scope* Scope::getChildScope() {
    return child;
}

void Scope::put(std::string newSymbol, Tokenizer::TokenType newType, std::variant<double, std::string> newValue) {
    std::pair typeValue = { newType, newValue };
    std::pair newPair = { newSymbol, typeValue };
    symbolTable.insert(newPair);
}

void Scope::putVal(std::string newSymbol, std::variant<double, std::string> newValue) {
    try {
        std::cout << "\nputting val rn\n";
        exit(1);
        std::cout << "newsymbol: " << newSymbol << std::endl;
        if (newValue.index() == 0)
            std::cout << "newValue: " << std::get<double>(newValue) << std::endl;
        else 
            std::cout << "newValue: " << std::get<std::string>(newValue) << std::endl;
        symbolTable.at(newSymbol).second = newValue;
    } catch (const std::out_of_range& oor) {
        std::string failMsg = "Cannot putVal() b/c symbol \'" + newSymbol + "\' doesn't exist in table.\n";
        fprintf(stderr, failMsg.c_str());
    }
}

void Scope::setParentScope(bool newHasParent, Scope* newParent) {
    hasParent = newHasParent;
    parent = newParent;
}

void Scope::setChildScope(bool newHasChild, Scope* newChild) {
    hasChild = newHasChild;
    child = newChild;
}

void Scope::printSymbolTable() {
    std::cout << std::left << "Key\t\t Type\t\t Value" << std::endl;
    std::cout << std::left << "-----\t\t -----\t\t -----" << std::endl;
    for (auto const& entry : symbolTable) {
        std::cout << std::left << entry.first << "\t\t " << Tokenizer::translateTokenType(entry.second.first) <<  "\t\t ";
        if (entry.second.second.index() == 0) {
            std::cout << std::get<double>(entry.second.second);
        }
        else {
            std::cout << std::get<std::string>(entry.second.second);
        }
        
        std::cout << std::endl;
    }
}