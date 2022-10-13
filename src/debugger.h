#pragma once

#include "context.h"
#include "parser.h"
#include "scope.h"

#include <unordered_map>
#include <vector>
#include <stack>
#include <queue>

enum class DEBUG_MODE {
    TOKENS,
    TREE,
    SIMULATION
};

std::unordered_map<std::string, DEBUG_MODE> debugModeMap {
    { "tokens", DEBUG_MODE::TOKENS },
    { "tree", DEBUG_MODE::TREE },
    { "simulation", DEBUG_MODE::SIMULATION }
};

class Debugger {
  public:
    Debugger();
    Debugger(Tokenizer::Token* tokens);
    Debugger(ASTNode* tree);
    ~Debugger();
    void debug(Tokenizer* tokenizer, Tokenizer::Token* tokens);
    void debug(Parser* parser, ASTNode* tree);
    void getScope();
    void next();
  private:
    Tokenizer::Token* tokens;
    ASTNode* tree;
};