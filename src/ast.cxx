#include "ast.h"
#include "error.h"
#include <stack>

// Requires all values in map are unique
template <class Tkey, class Tvalue>
std::unordered_map<Tvalue, Tkey> reverseMap (std::unordered_map<Tkey, Tvalue> map) {
    std::unordered_map<Tvalue, Tkey> reversedMap;
    for (auto &[key, value] : map) {
        if (reversedMap.count(value) != 0) {
            error("reverseMap() called on map with non-unique values.");
        }
        reversedMap[value] = key;
    }
    return reversedMap;
}

// Hashmaps for constant-time string->type conversion
std::unordered_map<std::string, PREFIX> prefixTextToType {
    { "Y", PREFIX::Y },    // yotta
    { "Z", PREFIX::Z },    // zetta
    { "E", PREFIX::E},    // exa
    { "P", PREFIX::P },    // peta
    { "T", PREFIX::T },    // tera
    { "G", PREFIX::G },     // giga
    { "M", PREFIX::M },     // mega
    { "k", PREFIX::k },     // kilo
    { "h", PREFIX::h },     // hecto
    { "da", PREFIX::da},     // deka
    { "d", PREFIX::d },    // deci
    { "c", PREFIX::c },    // centi
    { "m", PREFIX::m },    // milli
    { "u", PREFIX::u },    // micro
    { "n", PREFIX::n },    // nano
    { "p", PREFIX::p },   // pico
    { "f", PREFIX::f },   // femto
    { "a", PREFIX::a },   // atto
    { "z", PREFIX::z },   // zepto
    { "y", PREFIX::y },   // yocto
};

std::unordered_map<PREFIX, std::string> prefixTypeToText = reverseMap(prefixTextToType);

std::unordered_map<PREFIX, float> prefixToNumber {
    { PREFIX::Y, 1e24 },    // yotta
    { PREFIX::Z, 1e21 },    // zetta
    { PREFIX::E, 1e18 },    // exa
    { PREFIX::P, 1e15 },    // peta
    { PREFIX::T, 1e12 },    // tera
    { PREFIX::G, 1e9 },     // giga
    { PREFIX::M, 1e6 },     // mega
    { PREFIX::k, 1e3 },     // kilo
    { PREFIX::h, 1e2 },     // hecto
    { PREFIX::da, 1e1 },    // deka
    {PREFIX::NO_PREFIX, 1e0},
    { PREFIX::d, 1e-1 },    // deci
    { PREFIX::c, 1e-2 },    // centi
    { PREFIX::m, 1e-3 },    // milli
    { PREFIX::u, 1e-6 },    // micro
    { PREFIX::n, 1e-9 },    // nano
    { PREFIX::p, 1e-12 },   // pico
    { PREFIX::f, 1e-15 },   // femto
    { PREFIX::a, 1e-18 },   // atto
    { PREFIX::z, 1e-21 },   // zepto
    { PREFIX::y, 1e-24 },   // yocto
};

std::unordered_map<std::string, UNIT> unitTextToType {
    { "L", UNIT::LITER },
    { "s", UNIT::SEC },
    { "min", UNIT::MIN },
    { "h", UNIT::HR },
    { "g", UNIT::GRAM },
    { "C", UNIT::CELSIUS },
    { "F", UNIT::FAHRENHEIT },
    { "K", UNIT::KELVIN },
    { "V", UNIT::VOLT },
    { "A", UNIT::AMPERE },
    { "mol", UNIT::MOL },
    { "M", UNIT::MOLARITY },
    { "m", UNIT::MOLALITY },
    { "cd", UNIT::CANDELA },
    { "rpm", UNIT::RPM },
    { "G", UNIT::GFORCE },
};

std::unordered_map<UNIT, std::string> unitTypeToText = reverseMap(unitTextToType);

std::unordered_map<std::string, PARAM> paramTextToType {
    { "ctr", PARAM::CONTAINR },
    { "time", PARAM::TIME },
    { "spd", PARAM::SPEED },
    { "vol", PARAM::VOLUME },
    { "temp", PARAM::TEMP },
    { "form", PARAM::FORMULA },
    { "voltage", PARAM::VOLTAGE },
    { "config", PARAM::CONFIG },
    { "eq", PARAM::EQUATION },
    { "krev", PARAM::KREV },
    { "kcat", PARAM::KCAT },
    { "KM", PARAM::KM },
    { "k", PARAM::K },
    { "Ki", PARAM::Ki },
    { "n", PARAM::n_param },
    { "Ka", PARAM::Ka }
};

std::unordered_map<PARAM, std::string> paramTypeToText = reverseMap(paramTextToType);

std::unordered_map<std::string, SYMBOL> symbolTextToType {
    { "+", SYMBOL::ADD },
    { "-", SYMBOL::SUBTRACT },
    { "/", SYMBOL::DIVIDE },
    { "=", SYMBOL::ASSIGNMENT },
    { "==", SYMBOL::EQUALS },
    { "!", SYMBOL::NOT },
    { "!=", SYMBOL::NOT_EQUALS },
    { ",", SYMBOL::COMMA },
    { ".", SYMBOL::DOT },
    { ">", SYMBOL::GT },
    { "<", SYMBOL::LT },
    { "\"", SYMBOL::QUOTE_DOUBLE },
    { "\'", SYMBOL::QUOTE_SINGLE },
    { "?", SYMBOL::QUESTION },
    { "%", SYMBOL::PERCENT },
    { "^", SYMBOL::CARAT },
    { "|", SYMBOL::BIT_OR },
    { "&", SYMBOL::BIT_AND },
    { "||", SYMBOL::LOGI_OR },
    { "&&", SYMBOL::LOGI_AND },
    { "_", SYMBOL::UNDERSCORE },
    { ":", SYMBOL::COLON },
    { ";", SYMBOL::SEMICOLON },
    { "(", SYMBOL::PAREN_OPEN },
    { ")", SYMBOL::PAREN_CLOSED },
    { "{", SYMBOL::CURLY_OPEN },
    { "}", SYMBOL::CURLY_CLOSED },
    { "[", SYMBOL::BRACKET_OPEN },
    { "]", SYMBOL::BRACKET_CLOSED },
    { "-->", SYMBOL::FORWARD },
    { "<--", SYMBOL::BACKWARD },
    { "<->", SYMBOL::REVERSIBLE },
    { "--|", SYMBOL::INHIBITION }
};

std::unordered_map<SYMBOL, std::string> symbolTypeToText = reverseMap(symbolTextToType);

std::unordered_map<std::string, IMPORT_TYPE> importTextToType {
    { "Centrifuge", IMPORT_TYPE::CENTRIFUGE },
    { "Electrophoresis", IMPORT_TYPE::ELECTROPHORESIS }
};

std::unordered_map<IMPORT_TYPE, std::string> importTypeToText = reverseMap(importTextToType);

std::unordered_map<std::string, KEYWORD> keywordTextToType {
    { "reagent", KEYWORD::REAGENT },
    { "protocol", KEYWORD::PROTOCOL },
    { "container", KEYWORD::CONTAINER },
    { "import", KEYWORD::IMPORT },
    { "reaction", KEYWORD::REACTION },
    { "protein", KEYWORD::PROTEIN },
    { "complex", KEYWORD::COMPLEX},
    { "pathway", KEYWORD::PATHWAY } ,
    { "membrane", KEYWORD::MEMBRANE },
    { "domain", KEYWORD::DOM }, 
    { "plasm", KEYWORD::PLASM }
};

std::unordered_map<KEYWORD, std::string> keywordTypeToText = reverseMap(keywordTextToType);

std::unordered_map<std::string, PRIMITIVE_TYPE> primitiveTextToType {
    { "int", PRIMITIVE_TYPE::PRIM_INT },
    { "double", PRIMITIVE_TYPE::PRIM_DOUBLE },
    { "float", PRIMITIVE_TYPE::PRIM_FLOAT },
    { "bool", PRIMITIVE_TYPE::PRIM_BOOL },
    { "string", PRIMITIVE_TYPE::PRIM_STRING }
};

std::unordered_map<PRIMITIVE_TYPE, std::string> primitiveTypeToText = reverseMap(primitiveTextToType);

std::string prefixTexts[] = {"NONE", "Y", "Z", "E", "P", "T", "G", "M", "k", "h", "da",
                             "d", "c", "m", "u", "n", "p", "f", "a", "z", "y"};
std::string unitTexts[] = {"NONE", "LITER", "SEC", "MIN", "HR", "GRAM", "CELSIUS",
                           "FAHRENHEIT", "KELVIN", "VOLT", "AMPERE", "MOL", "MOLARITY",
                           "MOLALITY", "CANDELA", "RPM", "GFORCE"};
std::string paramTexts[] = {"UNINITIALIZED", "CONTAINER", "TIME", "MASS", "SPEED", "VOLUME", "TEMP", "FORMULA",
                            "VOLTAGE","CONFIG", "CHEM", "EQUATION", "KREV", "KCAT", "KM", "k",
                            "Ki", "n", "Ka"};
std::string symbolTexts[] = {"UNINITIALIZED", "ADD", "SUBTRACT", "MULTIPLY", "DIVIDE", "ASSIGNMENT",
                             "EQUALS", "NOT", "NOT_EQUALS", "COMMA", "DOT", "GEQ", "LEQ",
                             "GT", "LT", "QUOTE_DOUBLE", "QUOTE_SINGLE", "QUESTION", "PERCENT",
                             "CARAT", "BIT_OR", "BIT_AND", "LOGI_OR", "LOGI_AND", "UNDERSCORE",
                             "COLON", "SEMICOLON", "PAREN_OPEN", "PAREN_CLOSED", "CURLY_OPEN",
                             "CURLY_CLOSED", "BRACKET_OPEN", "BRACKET_CLOSED", "FORWARD", 
                             "BACKWARD", "REVERSIBLE", "INHIBITION", "UNKNOWN"};
std::string loopingTexts[] = {"UNINITIALIZED", "FOR", "WHILE", "DO"};
std::string keywordTexts[] = {"UNINITIALIZED", "REAGENT", "PROTOCOL", "CONTAINER", "IMPORT", "REACTION", "PROTEIN",
                              "PATHWAY", "MEMBRANE", "DOMAIN", "PLASM"};
std::string keywordTypeTexts[] = {"UNINITIALIZED", "FUNCTION", "NON_FUNCTION"};
std::string numberTexts[] = {"FLOAT", "INTEGER"};
std::string importTexts[] = {"UNINITIALIZED", "CENTRIFUGE", "ELECTROPHORESIS"};
std::string identifierTexts[] = {"UNINITIALIZED", "PRIMITIVE", "NON_FUNCTION", "FUNCTION"};
std::string primitiveTexts[] = {"NON-PRIMITIVE", "INT", "DOUBLE", "FLOAT", "BOOL", "STRING"};
std::string functionTexts[] = {"UNINITIALIZED", "INSTANCE", "STATIC", "CLASS"};
std::string returnTexts[] = {"UNINITIALIZED", "VOID", "RETURN"};

/* Abstract Superclass ASTNode + helper methods */
ASTNode::ASTNode() :
    type(Tokenizer::TYPE_NULL),
    text(""),
    line(0),
    col(0),
    colEnd(0),
    nextStatement(NULL),
    nodeType(NODE::AST_NODE) {}

ASTNode::ASTNode(Tokenizer::TokenType newType) :
    type(newType),
    text(""),
    line(0),
    col(0),
    colEnd(0),
    nextStatement(NULL),
    nodeType(NODE::AST_NODE) {}
    
ASTNode::ASTNode(Tokenizer::Token* newToken) :
    type(newToken->type),
    text(newToken->text),
    line(newToken->line),
    col(newToken->column),
    colEnd(newToken->end_column),
    nextStatement(NULL),
    nodeType(NODE::AST_NODE) {}

ASTNode::~ASTNode() {
    // std::cout << "Node destructed" << std::endl;
    delete nextStatement;
}

void ASTNode::printNode() {
    std::string type; 
    std::cout << "ASTNode: " << line << "," << col;
    printNodeType();
    std::cout << ", ";
    printNodeText();
    std::cout << std::endl;
}

Tokenizer::TokenType ASTNode::getType() {
    return type;
}

std::string ASTNode::getText() {
    return text;
}

ASTNode* ASTNode::getNextStatement() {
    return nextStatement;
}

void ASTNode::setType(Tokenizer::TokenType newType) {
    type = newType;
}

void ASTNode::setText(std::string newText) {
    text = newText;
}

int ASTNode::getLine() {
    return line;
}

ColumnNumber ASTNode::getCol() {
    return col;
}

std::string ASTNode::getPos() {
    std::string position = "";
    position += " <" + std::to_string(line) + ", " + std::to_string(col) + "> ";
    return position;
}

void ASTNode::setLine(int newLine) {
    line = newLine;
}

void ASTNode::setCol(ColumnNumber newCol) {
    col = newCol;
}

void ASTNode::setColEnd(ColumnNumber newColEnd) {
    colEnd = newColEnd;
}

void ASTNode::setNextStatement(ASTNode* newNextStatement) {
    hasNextStatement = true;
    nextStatement = newNextStatement;
}

void ASTNode::printNodeType() {
    std::cout << "Type: " << Tokenizer::translateTokenType(type);
}

void ASTNode::printNodeText() {
    std::cout << "Text: " << text << std::endl;
}

NODE ASTNode::getNodeType() {
    return nodeType;
}

void ASTNode::setNodeType(NODE newNodeType) {
    nodeType = newNodeType;
}

void ASTNode::assertNodeType(NODE expectedNodeType, std::string errorMessageOnFail, bool reversed) {
#ifdef DEBUG
    if (!reversed) {
        if (nodeType != expectedNodeType) {
            error(errorMessageOnFail);
        }
    } else {
        if (nodeType == expectedNodeType) {
            error(errorMessageOnFail);
        }
    }
#endif
}

void ASTNode::assertNodeType(std::unordered_set<NODE> expectedNodeTypes, std::string errorMessageOnFail, bool reversed) {
#ifdef DEBUG
    if (!reversed) {
        if (expectedNodeTypes.count(nodeType) == 0) {
            error(errorMessageOnFail);
        }
    } else {
        if (expectedNodeTypes.count(nodeType) == 1) {
            error(errorMessageOnFail);
        }
    }
#endif
}

void ASTNode::traverse(std::string tabs, std::string dashes) {
    // // dfs recursive algorithm
    visited = true;
    std::cout << tabs;
    printNode();
    std::vector<ASTNode*> children = getChildren();
    for (ASTNode* child : children) {
        if (!child->visited) {
            child->traverse(tabs + "\t", dashes + "-");
        }
    }
    if (hasNextStatement) {
        std::cout << tabs << dashes << "-" << "> ";
        getNextStatement()->traverse(tabs, dashes);
    }
}

void ASTNode::resetVisited() {
    if (this != nullptr) {
        visited = false;
        nextStatement->resetVisited();
        for (ASTNode* child : getChildren()) {
            child->resetVisited();
        }
    }
}

std::vector<ASTNode*> ASTNode::getChildren() {
    NODE nodeType = getNodeType();
    std::vector<ASTNode*> children;
    if (this == nullptr) {
        return children;
    }
    switch (nodeType) {
        // ternary
        case NODE::IF_ELSE_NODE: {
            IfElseNode* loop = dynamic_cast<IfElseNode*>(this);
            children.push_back(loop->getLeft());
            children.push_back(loop->getCenter());
            children.push_back(loop->getRight());
            break;
        }
        // binary
        case NODE::LOOPING_NODE: {
            LoopingNode* loop = dynamic_cast<LoopingNode*>(this);
            children.push_back(loop->getLeft());
            children.push_back(loop->getRight());
            break;
        }
        case NODE::SYMBOL_NODE: {
            SymbolNode* symbol = dynamic_cast<SymbolNode*>(this);
            children.push_back(symbol->getLeft());
            children.push_back(symbol->getRight());
            break;
        }
        case NODE::KEYWORD_NODE: {
            KeywordNode* keyword = dynamic_cast<KeywordNode*>(this);
            children.push_back(keyword->getLeft());
            children.push_back(keyword->getRight());
            break;
        }
        case NODE::IF_NODE: {
            IfNode* ifNode = dynamic_cast<IfNode*>(this);
            children.push_back(ifNode->getLeft());
            children.push_back(ifNode->getRight());
            break;
        }
        case NODE::INDEX_NODE: {
            IndexNode* indexNode = dynamic_cast<IndexNode*>(this);
            children.push_back(indexNode->getLeft());
            children.push_back(indexNode->getRight());
            break;
        }
        // unary
        case NODE::RETURN_NODE: {
            ReturnNode* ret = dynamic_cast<ReturnNode*>(this);
            children.push_back(ret->getChild());
            break;
        }
        case NODE::FUNCTION_NODE: {
            FunctionNode* function = dynamic_cast<FunctionNode*>(this);
            // push child (linked list of parameters (symbol nodes))
            if (function->hasParams()) {
                SymbolNode* params = dynamic_cast<SymbolNode*>(function->getChild());
                children.push_back(params->getLeft());
                children.push_back(params->getRight());
            }
            break;
        }
        // no children
        case NODE::NUMBER_NODE:
            break;
        case NODE::IDENTIFIER_NODE:
            break;
        case NODE::CHEMICAL_NODE:
            break;
        case NODE::IMPORT_NODE:
            break;
        case NODE::PARAM_NODE:
            break;
        case NODE::UNARY_NODE: {
            UnaryNode* unary = dynamic_cast<UnaryNode*>(this);
            children.push_back(unary->getChild());
            break;
        }
        case NODE::BINARY_NODE: {
            BinaryNode* binary = dynamic_cast<BinaryNode*>(this);
            children.push_back(binary->getLeft());
            children.push_back(binary->getRight());
            break;
        }
        case NODE::TERNARY_NODE: {
            TernaryNode* ternary = dynamic_cast<TernaryNode*>(this);
            children.push_back(ternary->getLeft());
            children.push_back(ternary->getCenter());
            children.push_back(ternary->getRight());
            break;
        }
        default:
            // an AST Node, no children to push
            break;
    }
    return children;
}

NumberNode* ASTNode::evaluate(Scope* curScope) {
    std::cout << "in evaluate!!";
    
    if (nodeType == NODE::SYMBOL_NODE) {
        std::cout << "checking symbol node";
        SymbolNode* symbol = dynamic_cast<SymbolNode*>(this);
        NumberNode* left = symbol->getLeft()->evaluate(curScope);
        // left->printNode();
        
        NumberNode* right = symbol->getRight()->evaluate(curScope);
        // right->printNode();
        std::cout << "about to compare prefixes + units";
        
        bool samePrefixUnit = left->comparePrefixUnit(right);
        std::cout << "compared prefixes + units";

        NumberNode* res = new NumberNode();
        float resValue = 0.0;

        switch(symbol->getSymbol()) {
            case SYMBOL::ADD: {
                resValue = left->getNum() + right->getNum();
                break;
            }
            case SYMBOL::SUBTRACT: {
                resValue = left->getNum() - right->getNum();
                break;
            }
            case SYMBOL::MULTIPLY: {
                resValue = left->getNum() * right->getNum();
                break;
            }
            case SYMBOL::DIVIDE: {
                resValue = left->getNum() / right->getNum();
                break;
            }
            case SYMBOL::CARAT: {
                resValue = std::pow(left->getNum(), right->getNum());
                break;
            }
            case SYMBOL::PERCENT: {
                resValue = (int) left->getNum() % (int) right->getNum();
            }
            case SYMBOL::LOGI_OR: {
                resValue = left->getNum() || right->getNum();
                break;
            }
            case SYMBOL::LOGI_AND: {
                resValue = left->getNum() && right->getNum();
                break;
            }
            case SYMBOL::EQUALS: {
                resValue = left->getNum() == right->getNum();
                break;
            }
            case SYMBOL::NOT_EQUALS: {
                resValue = left->getNum() != right->getNum();
                break;
            }
            case SYMBOL::GEQ: {
                resValue = left->getNum() >= right->getNum();
                break;
            }
            case SYMBOL::GT: {
                resValue = left->getNum() > right->getNum();
                break;
            }
            case SYMBOL::LEQ: {
                resValue = left->getNum() <= right->getNum();
                break;
            }
            case SYMBOL::LT: {
                resValue = left->getNum() < right->getNum();
                break;
            }
            default: {
                std::string failMsg = "Evaluation operation cannot be performed with symbol " + symbol->getText();
                fprintf(stderr, failMsg.c_str());
                exit(1);
            }
        }
            res->setNum(resValue);
            res->setNumType(NUMBER::FLOAT);
            
            if (samePrefixUnit) {
                res->setPrefix(left->getPrefix());
                res->setUnit(left->getUnit());
            }
            else {
                // infer prefix if only unit for 1 number is passed in.
                if (left->getPrefix() == PREFIX::NO_PREFIX) {
                    res->setPrefix(right->getPrefix());
                } else {
                    res->setPrefix(left->getPrefix());
                }
                // infer unit if only unit for 1 number is passed in.
                if (left->getUnit() == UNIT::NO_UNIT) {
                    res->setUnit(right->getUnit());
                } else {
                    res->setUnit(left->getUnit());
                }
            }

            return res;
        
    }
    else if (nodeType == NODE::NUMBER_NODE) {
        NumberNode* result = dynamic_cast<NumberNode*>(this);
        return result;
    }
    else if (nodeType == NODE::IDENTIFIER_NODE) {
        IdentifierNode* identifier = dynamic_cast<IdentifierNode*>(this);        
        // to-do: look up on symbol table
        if (curScope->hasSymbol(identifier->getName())){
            std::variant<double, std::string> answer = curScope->getSymbolValue(identifier->getName());
            if (answer.index() == 0) {
                NumberNode* result = new NumberNode();
                result->setNum(std::get<double>(answer));
                return result;
            } else {
                fprintf(stderr, "Found identifier in symboltable but the value is not a number.\n");
                exit(1);
            }
        } else {
            std::string failMsg = "Identifier " + identifier->getName() + " is not declared.\n";
            fprintf(stderr, failMsg.c_str());
            exit(1);
        }

    }
}

/* Unary node constructors + helper methods */
UnaryNode::UnaryNode() : ASTNode() {
    child = nullptr;
    setNodeType(NODE::UNARY_NODE);
}
UnaryNode::UnaryNode(ASTNode* newChild) {
    child = newChild;
    setNodeType(NODE::UNARY_NODE);
}
/* superclass ASTNode type_ = type and text_ = text set in initialization
   list in ast.h file. Same with Binary and Ternary nodes. */
UnaryNode::UnaryNode(Tokenizer::Token* newToken) :
    ASTNode(newToken) {
	   child = nullptr;
        setNodeType(NODE::UNARY_NODE);
}
UnaryNode::~UnaryNode() {
    // std::cout << "Unary Node destructed" << std::endl;
    delete child;
}

ASTNode* UnaryNode::getChild() {
    return child;
}

void UnaryNode::setChild(ASTNode* node) {
    child = node;
}

void UnaryNode::printNode() {
    std::cout << " ** UnaryNode" << getPos() << ": ";
    printNodeType();
    std::cout << ", ";
    printNodeText();
}

/* Binary node constructors + helper methods */
BinaryNode::BinaryNode() : ASTNode() {
    left = nullptr;
    right = nullptr;
    setNodeType(NODE::BINARY_NODE);
}
BinaryNode::BinaryNode(Tokenizer::Token* newToken) : 
                       ASTNode(newToken) {
    left = nullptr;
    right = nullptr;
    setNodeType(NODE::BINARY_NODE);
}
BinaryNode::BinaryNode(Tokenizer::Token* newToken,
                       ASTNode* newLeft, ASTNode* newRight) : ASTNode(newToken) {
    left = newLeft;
    right = newRight;
    setNodeType(NODE::BINARY_NODE);
}

BinaryNode::~BinaryNode() {
    // std::cout << "Binary Node destructed" << std::endl;
    delete left;
    delete right;
}

ASTNode* BinaryNode::getLeft() {
    return left;
}

ASTNode* BinaryNode::getRight() {
    return right;
}

void BinaryNode::setLeft(ASTNode* node) {
    left = node;
}

void BinaryNode::setRight(ASTNode* node) {
    right = node;
}

void BinaryNode::printNode() {
    printNodeType();
    std::cout << ", ";
    printNodeText();
}

/* Ternary node constructors + helper methods */
TernaryNode::TernaryNode() : ASTNode() {
    left = nullptr;
    center = nullptr;
    right = nullptr;
    setNodeType(NODE::TERNARY_NODE);
}
TernaryNode::TernaryNode(Tokenizer::Token* newToken, 
                         ASTNode* newLeft, ASTNode* newCenter, ASTNode* newRight) :
        ASTNode(newToken) {
        left = newLeft;
        center = newCenter;
        right = newRight;
        setNodeType(NODE::TERNARY_NODE);
}
TernaryNode::~TernaryNode() {
    // std::cout << "Ternary Node destructed" << std::endl;
    delete left;
    delete center;
    delete right;
}

ASTNode* TernaryNode::getLeft() {
    return left;
}

ASTNode* TernaryNode::getCenter() {
    return center;
}

ASTNode* TernaryNode::getRight() {
    return right;
}

void TernaryNode::setLeft(ASTNode* node) {
    left = node;
}

void TernaryNode::setCenter(ASTNode* node) {
    center = node;
}

void TernaryNode::setRight(ASTNode* node) {
    right = node;
}

void TernaryNode::printNode() {
    std::cout << " ** Ternary Node" << getPos() << ": ";
    printNodeType();
    std::cout << ", ";
    printNodeText();
}

/* Number node constructors + helper methods */
NumberNode::NumberNode() : ASTNode() {
    // either int or float is fine
    setType(Tokenizer::TokenType::TYPE_FLOAT); 
    setNodeType(NODE::NUMBER_NODE);
    num = 0;
    numType = NUMBER::FLOAT;
    prefix = PREFIX::NO_PREFIX;
    unit = UNIT::NO_UNIT;
}
NumberNode::NumberNode(Tokenizer::Token* newToken) :
    ASTNode(newToken) {
    setNodeType(NODE::NUMBER_NODE);
    num = 0;
    numType = NUMBER::FLOAT;
    prefix = PREFIX::NO_PREFIX;
    unit = UNIT::NO_UNIT;
}

NumberNode::~NumberNode() {
    std::cout << "Number Node destructed" << std::endl;
}

double NumberNode::getNum() {
    return num;
}

double NumberNode::getSIValue() { 
    // TODO: account for conversions to SI
    if (unit != UNIT::MOL && unit != UNIT::NO_UNIT && unit != UNIT::MOLARITY) {
        error("getSIValue() not yet implemented for non-mol units.");
    }
    return num * prefixToNumber[prefix];
}

NUMBER NumberNode::getNumType() {
    return numType;
}

PREFIX NumberNode::getPrefix() {
    return prefix;
}

UNIT NumberNode::getUnit() {
    return unit;
}

void NumberNode::setNum(float newNum) {
    num = newNum;
}

void NumberNode::setNumType(NUMBER newNumType) {
    numType = newNumType;
}

void NumberNode::setPrefix(PREFIX newPrefix) {
    prefix = newPrefix;
}

void NumberNode::setUnit(UNIT newUnit) {
    unit = newUnit;
}

bool NumberNode::comparePrefixUnit(NumberNode* other) {
    std::cout << "inside comparing prefix unit";
    return convertEnum(prefix) == convertEnum(other->prefix) &&
           convertEnum(unit) == convertEnum(other->unit);
}

void NumberNode::printNode() {
    std::cout << "NumberNode" << getPos() << ": ";
    std::cout << num << " (num), ";
    std::cout << numberTexts[convertEnum(numType)] << " (numType), ";
    std::cout << prefixTexts[convertEnum(prefix)] << " (prefix), ";
    std::cout << unitTexts[convertEnum(unit)] << " (unit)" << std::endl;
}

/* Symbol node constructors + helper methods */
SymbolNode::SymbolNode() : BinaryNode() {
    setNodeType(NODE::SYMBOL_NODE);
    symbol = SYMBOL::UNINITIALIZED;
}
SymbolNode::SymbolNode(Tokenizer::Token* newToken) : 
                       BinaryNode(newToken) {
        setNodeType(NODE::SYMBOL_NODE);
        symbol = SYMBOL::UNINITIALIZED;
    }
SymbolNode::~SymbolNode() {
    std::cout << "Symbol Node with symbol " << symbolTypeToText[symbol] << " destructed" << std::endl;
}

SYMBOL SymbolNode::getSymbol() {
    return symbol;
}

void SymbolNode::setSymbol(SYMBOL newSymbol) {
    symbol = newSymbol;
    this->setText(symbolTypeToText[newSymbol]);
}

void SymbolNode::assertSymbol(SYMBOL expectedSymbol, std::string errorMessageOnFail, bool reversed) {
#ifdef DEBUG
    if (!reversed) {
        if (symbol != expectedSymbol) {
            error(errorMessageOnFail);
        }
    } else {
        if (symbol == expectedSymbol) {
            error(errorMessageOnFail);
        }
    }
#endif
}

void SymbolNode::printNode() {
    std::cout << "SymbolNode" << getPos() << ": ";
    std::cout << symbolTexts[convertEnum(symbol)] << std::endl;
}

/* Looping node constructors + helper methods */
LoopingNode::LoopingNode() : BinaryNode() {
    setNodeType(NODE::LOOPING_NODE);
}
LoopingNode::LoopingNode(Tokenizer::Token* newToken,
                         ASTNode* newLeft, ASTNode* newRight, LOOPING newLoopType) :
                         BinaryNode(newToken, newLeft, newRight) {
        setNodeType(NODE::LOOPING_NODE);
        loopType = newLoopType;
    }
LoopingNode::~LoopingNode() {
    std::cout << "Looping Node destructed" << std::endl;
}

LOOPING LoopingNode::getLoopType() {
    return loopType;
}
void LoopingNode::setLoopType(LOOPING newLoopType) {
    loopType = newLoopType;
}

void LoopingNode::printNode() {
    std::cout << "LoopingNode" << getPos() << ": ";
    std::cout << loopingTexts[convertEnum(loopType)] << std::endl;
}

/* If node constructors + helper methods */
IfNode::IfNode() : BinaryNode() { 
    setNodeType(NODE::IF_NODE);
}

IfNode::IfNode(Tokenizer::Token* newToken,
               ASTNode* newLeft, ASTNode* newRight) :
               BinaryNode(newToken, newLeft, newRight) {
        setNodeType(NODE::IF_NODE);
    }
IfNode::~IfNode() {
    std::cout << "If Node destructed" << std::endl;
}
void IfNode::printNode() {
    std::cout << "IfNode" << getPos() << ": " << std::endl;
};

/* If-else node constructors + helper methods */
IfElseNode::IfElseNode() : TernaryNode() { 
    setNodeType(NODE::IF_ELSE_NODE);
}

IfElseNode::IfElseNode(Tokenizer::Token* newToken,
                       ASTNode* newLeft, ASTNode* newCenter, ASTNode* newRight) :
    TernaryNode(newToken, newLeft, newCenter, newRight) {
        setNodeType(NODE::IF_ELSE_NODE);
    }
    

IfElseNode::~IfElseNode() {
    std::cout << "If-Else Node destructed" << std::endl;
}
void IfElseNode::printNode() {
    std::cout << "IfElseNode" << getPos() << ": " << std::endl;
};

/* Identifier node constructors + helper methods */
IdentifierNode::IdentifierNode() : ASTNode(Tokenizer::TYPE_IDENTIFIER) {
    setNodeType(NODE::IDENTIFIER_NODE);
    type = IDENTIFIER_TYPE::UNINITIALIZED;
    primitiveType = PRIMITIVE_TYPE::NON_PRIMITIVE;
}
IdentifierNode::IdentifierNode(Tokenizer::Token* token) : ASTNode(token) {
    name = token->text;
    type = IDENTIFIER_TYPE::UNINITIALIZED;
    primitiveType = PRIMITIVE_TYPE::NON_PRIMITIVE;
    setNodeType(NODE::IDENTIFIER_NODE);
}
IdentifierNode::IdentifierNode(Tokenizer::Token* newToken, 
                               std::string newName, IDENTIFIER_TYPE newIdType) :
                               ASTNode(newToken) {
        setNodeType(NODE::IDENTIFIER_NODE);
        name = newName;
        type = newIdType;
        primitiveType = PRIMITIVE_TYPE::NON_PRIMITIVE;
    };
IdentifierNode::~IdentifierNode() {};

std::string IdentifierNode::getName() {
    return name;
}

void IdentifierNode::setName(std::string newName) {
    // currently simply set name. 
    // create symbol table DURING AST traversal, not now.
    name = newName;
}

IDENTIFIER_TYPE IdentifierNode::getType() {
    return type;
}

void IdentifierNode::setType(IDENTIFIER_TYPE newType) {
    type = newType;
}

PRIMITIVE_TYPE IdentifierNode::getPrimitiveType() {
    return primitiveType;
}

void IdentifierNode::setPrimitiveType(PRIMITIVE_TYPE newPrimitiveType) {
    primitiveType = newPrimitiveType;
}

void IdentifierNode::printNode() {
    std::cout << "IdentifierNode" << getPos() << ": ";
    std::cout << name << " (name), ";
    std::cout << identifierTexts[convertEnum(type)] << " (identifier type), ";
    std::cout << primitiveTexts[convertEnum(primitiveType)] << " (primitive type)" << std::endl;
}

/* Function node constructors + helper methods */
FunctionNode::FunctionNode() : UnaryNode() {
    name = "";
    setNodeType(NODE::FUNCTION_NODE);
    functionType = FUNCTION_TYPE::UNINITIALIZED;
    returnType = RETURN_TYPE::UNINITIALIZED;
    paramsExist = false;
}
    
FunctionNode::FunctionNode(Tokenizer::Token* token) {
    name = token->text;
    setNodeType(NODE::FUNCTION_NODE);
    functionType = FUNCTION_TYPE::UNINITIALIZED;
    returnType = RETURN_TYPE::UNINITIALIZED;
    paramsExist = false;
}

FunctionNode::FunctionNode(Tokenizer::Token* newToken,
                           std::string newName, FUNCTION_TYPE newFuncType) :
                           UnaryNode(newToken) {
        name = newName;
        functionType = newFuncType;
        setNodeType(NODE::FUNCTION_NODE);
        returnType = RETURN_TYPE::UNINITIALIZED;
        paramsExist = false;
    };

FunctionNode::~FunctionNode() {}

std::string FunctionNode::getName() {
    return name;
}

void FunctionNode::setName(std::string newName) {
    name = newName;
}

FUNCTION_TYPE FunctionNode::getFunctionType() {
    return functionType;
}

void FunctionNode::setFunctionType(FUNCTION_TYPE newFunctionType) {
    functionType = newFunctionType;
}

RETURN_TYPE FunctionNode::getReturnType() {
    return returnType;
}

void FunctionNode::setReturnType(RETURN_TYPE newReturnType) {
    returnType = newReturnType;
}

bool FunctionNode::hasParams() {
    return paramsExist;
}

void FunctionNode::setHasParams(bool hasParams) {
    paramsExist = hasParams;
}

void FunctionNode::printNode() {
    std::cout << "FunctionNode" << getPos() << ": ";
    std::cout << name << " (name), ";
    std::cout << functionTexts[convertEnum(functionType)] << " (function type), ";
    std::cout << returnTexts[convertEnum(returnType)] << " (return type)" << std::endl;
}

/* Chemical node constructors + helper methods */
ChemicalNode::ChemicalNode() : ASTNode(Tokenizer::TYPE_CHEMICAL) {
    setNodeType(NODE::CHEMICAL_NODE);
}
ChemicalNode::ChemicalNode(Tokenizer::Token* token) : ASTNode(token) {
    setNodeType(NODE::CHEMICAL_NODE);
    formula = token->text;
}
ChemicalNode::ChemicalNode(Tokenizer::Token* newToken, 
                           std::string newFormula) : ASTNode(newToken) {
    setNodeType(NODE::CHEMICAL_NODE);
    formula = newFormula;
};
ChemicalNode::~ChemicalNode() {}

std::string ChemicalNode::getFormula() {
    return formula;
}
void ChemicalNode::setFormula(std::string newFormula) {
    formula = newFormula;
}

void ChemicalNode::printNode() {
    std::cout << "ChemicalNode" << getPos() << ": ";
    std::cout << formula << std::endl;
}

/* Return node constructors + helper methods */
ReturnNode::ReturnNode() : UnaryNode() {
    setNodeType(NODE::RETURN_NODE);
}
ReturnNode::ReturnNode(ASTNode* nodeToReturn) : UnaryNode(nodeToReturn) {
    setNodeType(NODE::RETURN_NODE);
}
ReturnNode::ReturnNode(Tokenizer::Token* newToken) :
    UnaryNode(newToken) {
        setNodeType(NODE::RETURN_NODE);
    }
ReturnNode::~ReturnNode() {
    std::cout << "Return Node destructed" << std::endl;
}

void ReturnNode::printNode() {
    std::cout << "ReturnNode" << getPos() << ": " << std::endl << "\t";
}

/* Keyword node constructors + helper methods */
KeywordNode::KeywordNode() {
    setNodeType(NODE::KEYWORD_NODE);
    keyword = KEYWORD::UNINITIALIZED;
    allowStatements = false;
}
KeywordNode::KeywordNode(Tokenizer::Token* newToken) :
                         BinaryNode(newToken) {
        setNodeType(NODE::KEYWORD_NODE);
        keyword = KEYWORD::UNINITIALIZED;
        allowStatements = false;
    }
KeywordNode::KeywordNode(Tokenizer::Token* newToken,
                         ASTNode* newLeft, ASTNode* newRight, KEYWORD newKeyword) :
                         BinaryNode(newToken, newLeft, newRight) {
        setNodeType(NODE::KEYWORD_NODE);
        keyword = newKeyword;
        switch (convertEnum(newKeyword)) {
            case (2):   // protocol
            case (6):
            case (7):
            case (8):
            case (9):
                allowStatements = true;
            default:
                allowStatements = false;
        }
    }
KeywordNode::~KeywordNode() {
    std::cout << "Keyword Node destructed" << std::endl;
}

KEYWORD KeywordNode::getKeyword() {
    return keyword;
}

bool KeywordNode::statementsAllowed() {
    return allowStatements;
}

void KeywordNode::setKeyword(KEYWORD newKeyword) {
    keyword = newKeyword;
}

void KeywordNode::setAllowStatements(bool permission) {
    allowStatements = permission;
}

void KeywordNode::assertKeyword(KEYWORD expectedKeyword, std::string errorMessageOnFail, bool reversed) {
#ifdef DEBUG
    if (!reversed) {
        if (keyword != expectedKeyword) {
            error(errorMessageOnFail);
        }
    } else {
        if (keyword == expectedKeyword) {
            error(errorMessageOnFail);
        }
    }
#endif
}

void KeywordNode::printNode() {
    std::cout << "KeywordNode" << getPos() << ": ";
    std::cout << keywordTexts[convertEnum(keyword)] << " (keyword), ";
    std::cout << allowStatements << " (allowStatements)" << std::endl;
}

/* Import node constructors + helper methods */
ImportNode::ImportNode() {
    setNodeType(NODE::IMPORT_NODE);
    import = IMPORT_TYPE::UNINITIALIZED;
}
ImportNode::ImportNode(Tokenizer::Token* newToken, IMPORT_TYPE newImport) :
    ASTNode(newToken) {
        setNodeType(NODE::IMPORT_NODE);
        import = newImport;
    }
ImportNode::~ImportNode() {
    std::cout << "Import Node destructed" << std::endl;
}
IMPORT_TYPE ImportNode::getImport() {
    return import;
}

void ImportNode::printNode() {
    std::cout << "ImportNode" << getPos() << ": ";
    std::cout << importTexts[convertEnum(import)] << " (IMPORT_TYPE)" << std::endl;
}

void ImportNode::setImport(IMPORT_TYPE newImport) {
    import = newImport;
}

/* Param node constructors + helper methods */
ParamNode::ParamNode() {
    setNodeType(NODE::PARAM_NODE);
    paramType = PARAM::UNINITIALIZED;
}
ParamNode::ParamNode(Tokenizer::Token* newToken) {
    setNodeType(NODE::PARAM_NODE);
    paramType = PARAM::UNINITIALIZED;
}

ParamNode::ParamNode(PARAM newParamType) :
                     ASTNode() {
        paramType = newParamType;
        setNodeType(NODE::PARAM_NODE);
}


ParamNode::ParamNode(Tokenizer::Token* newToken, PARAM newParamType) :
                     ASTNode(newToken) {
        paramType = newParamType;
        setNodeType(NODE::PARAM_NODE);
}

ParamNode::~ParamNode() {
    std::cout << "Param Node destructed" << std::endl;
}


void ParamNode::setParamType(PARAM newParamType) {
    paramType = newParamType;
    this->setText(paramTypeToText[newParamType]);
}

PARAM ParamNode::getParamType() {
    return paramType;
}

void ParamNode::printNode() {
    std::cout << "ParamNode" << getPos() << ": ";
    std::cout << paramTexts[convertEnum(paramType)] << " (paramType)" << std::endl;
}


IndexNode::IndexNode() {
    setNodeType(NODE::INDEX_NODE);
}
IndexNode::IndexNode(Tokenizer::Token* newToken) {
    setNodeType(NODE::INDEX_NODE);
}
IndexNode::IndexNode(Tokenizer::Token* newToken,
                     ASTNode* newLeft, ASTNode* newRight) :
                     BinaryNode(newToken, newLeft, newRight) {
        setNodeType(NODE::INDEX_NODE);
    }
IndexNode::~IndexNode() {
    std::cout << "Index Node destructed" << std::endl;
}
void IndexNode::printNode() {
    std::cout << "IndexNode" << getPos() << ": " << std::endl;
}

template<typename Type>
static int convertEnum(Type t) {
    return static_cast<int>(t);
}


void ASTNode::singlePrintNodeChildren() {
    std::vector<ASTNode*> children;
    this->printNode();
    children = this->getChildren();
    for (ASTNode* child : children) {
        std::cout << "\t";
        child->printNode();
    }
}