#pragma once

#include "tokenizer.h"
#include "scope.h"

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <optional>
#include <cmath>
#include <iomanip>


class NumberNode;

enum class PREFIX {
   NO_PREFIX,    // no prefix
   Y,       // yotta
   Z,       // zetta
   E,       // exa
   P,       // peta
   T,       // tera
   G,       // giga
   M,       // mega
   k,       // kilo
   h,       // hecto
   da,      // deka
   d,       // deci
   c,       // centi
   m,       // milli
   u,       // micro
   n,       // nano
   p,       // pico
   f,       // femto
   a,       // atto
   z,       // zepto
   y       // yocto
};

enum class UNIT {
    NO_UNIT,

    // volume
    LITER,

    // time
    SEC,
    MIN,
    HR,

    // mass/weight
    GRAM,

    // temperature
    CELSIUS,
    FAHRENHEIT,
    KELVIN,

    // electrical
    VOLT,
    AMPERE,

    // amount of substance
    MOL,
    MOLARITY,
    MOLALITY,

    // luminous intensity
    CANDELA,

    // speed
    RPM,
    GFORCE
};

enum class PARAM {
    UNINITIALIZED,
    // matching L++ syntax commented below
    CONTAINR,           // ctr
    TIME,               // time
    MASS,
    SPEED,              // spd
    VOLUME,             // vol
    TEMP,               // temp
    FORMULA,            // form
    VOLTAGE,            // voltage
    CONFIG,              // config
    EQUATION,
    MOLS,
    KREV,
    KCAT,
    KM,
    K,
    Ki,
    n_param,
    Ka
};

enum class SYMBOL {
    UNINITIALIZED,

    ADD,                // +
    SUBTRACT,           // -
    MULTIPLY,           // *
    DIVIDE,             // /
    ASSIGNMENT,         // =
    EQUALS,             // ==
    NOT,                // !
    NOT_EQUALS,         // !=
    COMMA,              // ,
    DOT,                // .
    GEQ,                // >=
    LEQ,                // <=
    GT,                 // >
    LT,                 // <
    QUOTE_DOUBLE,       // "
    QUOTE_SINGLE,       // '
    QUESTION,           // ?
    PERCENT,            // %
    CARAT,              // ^
    BIT_OR,             // |
    BIT_AND,            // &
    LOGI_OR,            // ||
    LOGI_AND,           // &&
    UNDERSCORE,         // _
    COLON,              // :
    SEMICOLON,          // ;
    PAREN_OPEN,         // (
    PAREN_CLOSED,       // )
    CURLY_OPEN,         // {
    CURLY_CLOSED,       // }
    BRACKET_OPEN,       // [
    BRACKET_CLOSED,     // ]
    FORWARD,            // ->
    BACKWARD,           // <-
    REVERSIBLE,         // <->
    INHIBITION,         // --|
    UNKNOWN
};

enum class LOOPING {
    UNINITIALIZED,
    /* matching L++ syntax commented below */
    FOR,        // for
    WHILE,      // while
    DO,         // do
};

enum class KEYWORD {
    UNINITIALIZED,
    REAGENT,
    
    PROTOCOL,
    CONTAINER,
    IMPORT,
    REACTION,
    PROTEIN,
    COMPLEX,
    PATHWAY,
    MEMBRANE,
    DOM,    // cannot use DOMAIN b/c it is a conflicting system macro
    PLASM
};

enum class IDENTIFIER_TYPE {
    UNINITIALIZED,
    PRIMITIVE,
    NON_FUNCTION,
    FUNCTION
};

enum class FUNCTION_TYPE {
    UNINITIALIZED,
    INSTANCE,
    STATIC,
    CLASS
};

enum class RETURN_TYPE {
    UNINITIALIZED,
    VOID,
    RETURN
};


enum class NUMBER {
    FLOAT,
    INTEGER
};

enum class IMPORT_TYPE {
    UNINITIALIZED,
    CENTRIFUGE, 
    ELECTROPHORESIS
};

enum class PRIMITIVE_TYPE {
    NON_PRIMITIVE,
    PRIM_INT,
    PRIM_FLOAT,
    PRIM_DOUBLE,
    PRIM_BOOL,
    PRIM_STRING
};

enum class NODE {
    AST_NODE,

    UNARY_NODE,
    BINARY_NODE,
    TERNARY_NODE,

    LOOPING_NODE,
    IF_NODE,
    IF_ELSE_NODE,
    NUMBER_NODE,
    SYMBOL_NODE,
    IDENTIFIER_NODE,
    FUNCTION_NODE,
    PARAM_NODE,
    RETURN_NODE,
    CHEMICAL_NODE,
    KEYWORD_NODE,
    IMPORT_NODE,
    INDEX_NODE
};

extern std::unordered_map<std::string, PREFIX> prefixTextToType;
extern std::unordered_map<PREFIX, float> prefixToNumber;
extern std::unordered_map<std::string, UNIT> unitTextToType;
extern std::unordered_set<std::string> noPrefixUnits;
extern std::unordered_map<std::string, PARAM> paramTextToType;
extern std::unordered_map<std::string, SYMBOL> symbolTextToType;
extern std::unordered_map<std::string, IMPORT_TYPE> importTextToType;
extern std::unordered_map<std::string, KEYWORD> keywordTextToType;
extern std::unordered_map<std::string, PRIMITIVE_TYPE> primitiveTextToType;

extern std::unordered_map<PREFIX, std::string> prefixTypeToText;
extern std::unordered_map<UNIT, std::string> unitTypeToText;
extern std::unordered_map<PARAM, std::string> paramTypeToText;
extern std::unordered_map<SYMBOL, std::string> symbolTypeToText;
extern std::unordered_map<IMPORT_TYPE, std::string> importTypeToText;
extern std::unordered_map<KEYWORD, std::string> keywordTypeToText;
extern std::unordered_map<PRIMITIVE_TYPE, std::string> primitiveTypeToText;


/*  Abstract Syntax Tree Node Hierarchy:
    --------------------------------------
    Parent Class: AST Node
    Child Classes: UnaryNode, BinaryNode, TernaryNode, NumberNode,
                   IdentifierNode, ImportNode
    Grandchild Classes: ReturnNode, KeywordNode, SymbolNode, LoopingNode

                         ASTNode
            |               |               |                   |
        Unary             Binary            Ternary            NumberNode
        |                   |                   |              IdentifierNode
    ReturnNode          SymbolNode          LoopingNode        ImportNode
    KeywordNode

    It is important to note that several specific node classes (ie. NumberNode)
    are directly children of ASTNode, NOT grandchildren. This is because these
    child node classes due not contain any children. 
*/

class ASTNode {
    public:
        ASTNode();
        ASTNode(Tokenizer::TokenType newType);
        ASTNode(Tokenizer::Token* newToken);
        virtual ~ASTNode();
        virtual void printNode();
        void printNodeType();
        void printNodeText();
        Tokenizer::TokenType getType();
        std::string getText();
        ASTNode* getNextStatement();
        int getLine();
        ColumnNumber getCol();
        std::string getPos();
        void setType(Tokenizer::TokenType newType);
        void setText(std::string newText);
        void setLine(int newLine);
        void setCol(ColumnNumber newCol);
        void setColEnd(ColumnNumber newColEnd);
        void setNextStatement(ASTNode* newNextStatement);
        NODE getNodeType();
        void setNodeType(NODE newNodeType);
        void assertNodeType(NODE expectedNodeType, std::string errorMessageOnFail, bool reversed = false);
        void assertNodeType(std::unordered_set<NODE> expectedNodeTypes, std::string errorMessageOnFail, bool reversed = false);
        void traverse(std::string tabs, std::string dashes);
        void resetVisited();
        std::vector<ASTNode*> getChildren();
        NumberNode* evaluate(Scope* curScope);
        bool hasNextStatement = false;
        bool visited = false;
        void singlePrintNodeChildren();

    private:
        Tokenizer::TokenType type;
        std::string text;           // strictly for easier debugging. will remove after.
        int line;
        ColumnNumber col;
        ColumnNumber colEnd;
        ASTNode* nextStatement;
        NODE nodeType;
};

class UnaryNode : public ASTNode {
     public:
        UnaryNode();
        UnaryNode(ASTNode* newChild);
        UnaryNode(Tokenizer::Token* newToken);
        ~UnaryNode();
        void printNode() override;
        ASTNode* getChild();
        void setChild(ASTNode* node);
    private:
        ASTNode* child;
};

class BinaryNode : public ASTNode {
    public:
        BinaryNode();
        BinaryNode(Tokenizer::Token* newToken);
        BinaryNode(Tokenizer::Token* newToken, ASTNode* newLeft, ASTNode* newRight);
        ~BinaryNode();
        void printNode() override;
        ASTNode* getLeft();
        ASTNode* getRight();
        void setLeft(ASTNode* node);
        void setRight(ASTNode* node);
    private:
        ASTNode* left;
        ASTNode* right;
};

class TernaryNode : public ASTNode {
    public:
        TernaryNode();
        TernaryNode(Tokenizer::Token* newToken, 
                    ASTNode* newLeft, ASTNode* newCenter, ASTNode* newRight);
        ~TernaryNode();
        void printNode() override;
        ASTNode* getLeft();
        ASTNode* getCenter();
        ASTNode* getRight();
        void setLeft(ASTNode* node);
        void setCenter(ASTNode* node);
        void setRight(ASTNode* node);
    private:
        ASTNode* left;
        ASTNode* center;
        ASTNode* right;
};

class NumberNode : public ASTNode {
    public:
        NumberNode();
        NumberNode(Tokenizer::Token* newToken);
        ~NumberNode();
        void printNode() override;
        double getNum();
        double getSIValue();  // getSIValue = value * prefix * conversion to SI units
        NUMBER getNumType();
        PREFIX getPrefix();
        UNIT getUnit();
        void setNum(float newNum);
        void setNumType(NUMBER newNumType);
        void setPrefix(PREFIX newPrefix);
        void setUnit(UNIT newUnit);

        bool comparePrefixUnit(NumberNode* other);
    private:
        double num;  // turn both ints + floats into floats for now
        NUMBER numType;     // can indicate if int/float, cast at runtime
        PREFIX prefix;
        UNIT unit;
};

class SymbolNode : public BinaryNode {
    public:
        SymbolNode();
        SymbolNode(Tokenizer::Token* newToken);
        ~SymbolNode();
        void printNode() override;
        SYMBOL getSymbol();
        void setSymbol(SYMBOL newSymbol);
        void assertSymbol(SYMBOL expectedSymbol, std::string errorMessageOnFail, bool reversed = false);
    private:
        SYMBOL symbol;
};

class LoopingNode : public BinaryNode {
    public:
        LoopingNode();
        LoopingNode(Tokenizer::Token* newToken,
                    ASTNode* newLeft, ASTNode* newRight, LOOPING newLoopType);
        ~LoopingNode();
        void printNode() override;
        LOOPING getLoopType();
        void setLoopType(LOOPING newLoopType);
    private:
        LOOPING loopType;
};

class IfNode : public BinaryNode {
    public:
        IfNode();
        IfNode(Tokenizer::Token* newToken,
               ASTNode* newLeft, ASTNode* newRight);
        ~IfNode();
        void printNode() override;
};

class IfElseNode : public TernaryNode {
    public:
        IfElseNode();
        IfElseNode(Tokenizer::Token* newToken,
                   ASTNode* left, ASTNode* center, ASTNode* right);
        ~IfElseNode();
        void printNode() override;
};

class IdentifierNode : public ASTNode {
    public:
        IdentifierNode();
        IdentifierNode(Tokenizer::Token* token);
        IdentifierNode(Tokenizer::Token* newToken, 
                       std::string newName, IDENTIFIER_TYPE newIdType = IDENTIFIER_TYPE::NON_FUNCTION);
        ~IdentifierNode();
        void printNode() override;
        std::string getName();
        void setName(std::string newName);
        IDENTIFIER_TYPE getType();
        void setType(IDENTIFIER_TYPE newType);
        PRIMITIVE_TYPE getPrimitiveType();
        void setPrimitiveType(PRIMITIVE_TYPE newPrimitiveType);
    private:
        std::string name;
        IDENTIFIER_TYPE type;
        PRIMITIVE_TYPE primitiveType;
};

class FunctionNode: public UnaryNode {
    public:
        FunctionNode();
        FunctionNode(Tokenizer::Token* token);
        FunctionNode(Tokenizer::Token* newToken, 
                     std::string newName, FUNCTION_TYPE newFuncType);
        ~FunctionNode();
        void printNode() override;
        std::string getName();
        void setName(std::string newName);
        FUNCTION_TYPE getFunctionType();
        void setFunctionType(FUNCTION_TYPE newFunctionType);
        RETURN_TYPE getReturnType();
        void setReturnType(RETURN_TYPE newReturnType);
        bool hasParams();
        void setHasParams(bool hasParams);
    private:
        FUNCTION_TYPE functionType;
        RETURN_TYPE returnType;
        std::string name;
        bool paramsExist;
};

class ChemicalNode : public ASTNode {
    public: 
        ChemicalNode();
        ChemicalNode(Tokenizer::Token* token);
        ChemicalNode(std::string chemicalName);
        ChemicalNode(Tokenizer::Token* newToken,
                     std::string newFormula);
        ~ChemicalNode();
        void printNode() override;
        std::string getFormula();
        void setFormula(std::string newName);
    private:
        std::string formula;
};

class ReturnNode : public UnaryNode {
    public:
        ReturnNode();
        ReturnNode(ASTNode* nodeToReturn);
        ReturnNode(Tokenizer::Token* newToken);
        ~ReturnNode();
        void printNode() override;
};

class KeywordNode : public BinaryNode {
    public:
        KeywordNode();
        KeywordNode(Tokenizer::Token* newToken);
        KeywordNode(Tokenizer::Token* newToken,
                    ASTNode* newLeft, ASTNode* newRight, KEYWORD newKeyword);
        ~KeywordNode();
        void printNode() override;
        KEYWORD getKeyword();
        bool statementsAllowed();
        void setKeyword(KEYWORD newKeyword);
        void setAllowStatements(bool permission);
        void assertKeyword(KEYWORD expectedKeyword, std::string errorMessageOnFail, bool reversed = false);
    private:
        KEYWORD keyword;
        bool allowStatements;
};

class ImportNode : public ASTNode {
    public:
        ImportNode();
        ImportNode(Tokenizer::Token* newToken, IMPORT_TYPE newImport);
        ~ImportNode();
        void printNode() override;
        IMPORT_TYPE getImport();
        void setImport(IMPORT_TYPE newImport);
    private:
        IMPORT_TYPE import;
};

class ParamNode : public ASTNode {
    public:
        ParamNode();
        ParamNode(Tokenizer::Token* newToken);
        ParamNode(PARAM newParamType);
        ParamNode(Tokenizer::Token* newToken, PARAM newParamType);
        ~ParamNode();
        void printNode() override;
        void setParamType(PARAM newParamType);
        PARAM getParamType();
    private:
        PARAM paramType;
};

class IndexNode : public BinaryNode {
    public:
        IndexNode();
        IndexNode(Tokenizer::Token* newToken);
        IndexNode(Tokenizer::Token* newToken,
                  ASTNode* newLeft, ASTNode* newRight);
        ~IndexNode();
        void printNode() override;
};

template<typename Type>
static int convertEnum(Type t);