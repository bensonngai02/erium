#pragma once

#include "ast.h"
#include "scope.h"

#include <vector>
#include <queue>

extern std::unordered_map<UNIT, PARAM> unitToParam;

enum class BLOCK_TYPE {
    GLOBAL,
    IF,
    ELSE,
    CONTAINER,
    PROTOCOL,
    REAGENT,
    REACTION,
    PROTEIN,
    COMPLEX,
    PATHWAY,
    MEMBRANE,
    DOM,    // cannot use DOMAIN b/c it is a conflicting system macro
    PLASM
};

extern std::unordered_map<KEYWORD, BLOCK_TYPE> keywordToBlock;

class Parser {
  public:
    Parser();
    Parser(Tokenizer::Token* tokenList);
    ASTNode* parse();
    ASTNode* parseStatement();
    ASTNode* parseExpression();
    bool checkCur(Tokenizer::TokenType type, std::string text);
    bool checkCurType(Tokenizer::TokenType type);
    bool checkCurText(std::string text);

    bool checkText(Tokenizer::Token* t, std::string text);
    // Moves onto the next token. Sets curToken to next token in list.
    void next();
    void prev();
    // Retrieves the next token but does NOT move on. 
    Tokenizer::Token* checkNext();
    bool checkNextType(Tokenizer::TokenType type);
    bool checkNextNextType(Tokenizer::TokenType type);
    bool checkNextNextNextType(Tokenizer::TokenType type);
    // checks whether next token's text matches passed in string.
    bool checkNextText(std::string text);
    bool consume(std::string text);
    bool consume(Tokenizer::TokenType);
    void semicolon();
    void semicolonOrParen();
    void semicolonOrComma();

    /* Recursive descent expression parsing hierarchy. 
    Precedence of parsing from ordered from highest to lowest method.
    Matching L++ syntax commented below next to each method. 
    Each return ASTNode* to allow recursive parsing calls. */
    ASTNode* parseTopLevelExpression();
    KeywordNode* parseKeyword();        // container, reagent, protocol
    IdentifierNode* parseIdentifier();
    ASTNode* parseParen();          // ()
    ChemicalNode* parseChemical();       // chemical notation formulas
    ASTNode* parseLiteral();        // integers, floats
    ASTNode* parseSlice();
    ASTNode* parseBracket();        // []
    ASTNode* parseIncrement();      // ++, -- (postfix then prefix)
    ASTNode* parseArrow();
    ASTNode* parseMulDivMod();      // *, /, %
    ASTNode* parseAddSub();         // +, -
    ASTNode* parseLesserGreater();  // <=, <. >=, >
    ASTNode* parseEq();             // ==, !=
    ASTNode* parseBitAnd();         // &
    ASTNode* parseBitOr();          // |
    ASTNode* parseLogiAnd();        // &&
    ASTNode* parseLogiOr();         // ||
    ASTNode* parseTernaryOp();      // ?, :
    ASTNode* parseComma();          // ,

    void parseUnit(NumberNode* precedingNumber);
    SymbolNode* parseParam();
    void parseNextParam(SymbolNode* curParam);
    SymbolNode* parseAssignment(Tokenizer::Token* identifierToken, IDENTIFIER_TYPE type, bool evaluate = true, PRIMITIVE_TYPE = PRIMITIVE_TYPE::NON_PRIMITIVE);  // =
    SymbolNode* parseFunction();
    SymbolNode* parsePrimitive();
    SymbolNode* parseChemEq();
    KeywordNode* parseReaction();
    IndexNode* parseIndex();

    /* helper function to parse function call parentheses: 
            1. Default - ()
            2. Parameters - (time = ..., rpm = ..., etc.)
    */
    bool parseFunctionParen(FunctionNode* function);

    LoopingNode* parseLooping(Tokenizer::Token* t);
    ReturnNode* parseReturn(Tokenizer::Token* t);
    ImportNode* parseImport();
    ASTNode* parseBlock();
    SymbolNode* inferParam();

    PARAM inferUnit(UNIT unitToInfer);
    void resetUnitSeen();

    Tokenizer::Token* prevToken;
    Tokenizer::Token* curToken; 
    ASTNode* root;                  // current root
    std::stack<Scope*> spaghetti;      // spaghetti stack / parent-pointer tree
    std::unordered_map<std::string, Scope*> scopes;      // scopes
    Scope* curScope; 
    std::string curScopeName;
    BLOCK_TYPE curBlockType;
    UNIT unitSeen;

    Scope* getScope(std::string scopeName);
    void openScope(std::string scopeName);
    void closeScope(std::string scopeName);     
    void printScopes();

    void evaluateOperations(ASTNode* root);
};


// Node helper methods for parsing each supported type.
// ===========================================================
static bool isInteger(float num) {
    return ((int) num) == num;
}

static PREFIX translatePrefixType(std::string prefixText) {
    PREFIX translated;
    try {
        translated = prefixTextToType.at(prefixText);
    }
    catch (const std::out_of_range& oor) {
        fail("The prefix found in " + prefixText + " is invalid." \
            "L++ supports all prefixes within the range of yocto(y) and yotta (Y).", nullptr);
    }
    return translated;
}

static UNIT translateUnitType(std::string unitText) {
    UNIT translated;
    try {
        translated = unitTextToType.at(unitText);
    }
    catch (const std::out_of_range& oor) {
        fail("The unit " + unitText + " is invalid." \
            "L++ supports all SI units and further. See supported units in documentation.", nullptr);
    }
    return translated;
}

static KEYWORD translateKeywordType(std::string keywordText) {
    KEYWORD translated;
    try {
        translated = keywordTextToType.at(keywordText);
    }
    catch (const std::out_of_range& oor) {
        fail("The keyword " + keywordText + " is invalid." \
            "Keyword not supported. See supported keywords in documentation.", nullptr);
    }
    return translated;
}

static PARAM translateParamType(std::string paramText) {
    PARAM translated;
    try {
        translated = paramTextToType.at(paramText);
    }
    catch (const std::out_of_range& oor) {
        fail("The parameter " + paramText + " is invalid." \
            "Parameter not supported. See supported parameters in documentation.", nullptr);
    }
    return translated;
}

static IMPORT_TYPE translateImportType(std::string importText) {
    IMPORT_TYPE translated;
    try {
        translated = importTextToType.at(importText); 
    }
    catch (const std::out_of_range& oor) {
        fail("The import " + importText + " is invalid." \
            "Import not supported. See supported imports in documentation.", nullptr);        
    }
    return translated;
}

static PRIMITIVE_TYPE translatePrimitiveType(std::string primitiveText) {
    PRIMITIVE_TYPE translated;
    try {
        translated = primitiveTextToType.at(primitiveText);
    }
    catch (const std::out_of_range& oor) {
        fail("The primitive " + primitiveText + " is invalid." \
            "Primitive not supported. See supported primitives in documentation.", nullptr);
    }
    return translated;
}

// Debug
static void print(std::string error) {
    std::cout << error << std::endl;
}