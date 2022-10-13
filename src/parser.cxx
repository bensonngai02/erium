#include "parser.h"

#define LPP_FILENAME_OFFSET 3

std::unordered_map<UNIT, PARAM> unitToParam {
    { UNIT::LITER, PARAM::VOLUME },
    { UNIT::SEC, PARAM::TIME },
    { UNIT::MIN, PARAM::TIME },
    { UNIT::HR, PARAM::TIME },
    { UNIT::GRAM, PARAM::MASS },
    { UNIT::CELSIUS, PARAM::TEMP },
    { UNIT::FAHRENHEIT, PARAM::TEMP },
    { UNIT::KELVIN, PARAM::TEMP },
    { UNIT::VOLT, PARAM::VOLTAGE },
    { UNIT::AMPERE, PARAM::VOLTAGE },
    { UNIT::MOL, PARAM::MOLS },
    { UNIT::MOLARITY, PARAM::MOLS },
    { UNIT::MOLALITY, PARAM::MOLS },
    // { UNIT::CANDELA, PARAM::BRIGHTNESS },
    { UNIT::RPM, PARAM::SPEED },
    { UNIT::GFORCE, PARAM::SPEED },
};

std::unordered_map<KEYWORD, BLOCK_TYPE> keywordToBlock {
    { KEYWORD::CONTAINER, BLOCK_TYPE::CONTAINER },
    { KEYWORD::PROTEIN, BLOCK_TYPE::PROTEIN },
    { KEYWORD::COMPLEX, BLOCK_TYPE::COMPLEX },
    { KEYWORD::PROTOCOL, BLOCK_TYPE::PROTOCOL },
    { KEYWORD::REAGENT, BLOCK_TYPE::REAGENT},
    { KEYWORD::REACTION, BLOCK_TYPE::REACTION },
    { KEYWORD::MEMBRANE, BLOCK_TYPE::MEMBRANE },
    { KEYWORD::PATHWAY, BLOCK_TYPE::PATHWAY },
    { KEYWORD::PLASM, BLOCK_TYPE::PLASM },
    { KEYWORD::DOM, BLOCK_TYPE::DOM },
    { KEYWORD::IMPORT, BLOCK_TYPE::GLOBAL },
    { KEYWORD::UNINITIALIZED, BLOCK_TYPE::GLOBAL }
};

// Tree constructor + helper methods
// Class to parse statements, expressions, and generate a tree
// Returns a vector of tree roots, where each root represents its own
// AST (an individual statement)
// ===========================================================
Parser::Parser() {}
Parser::Parser(Tokenizer::Token* tokenListHead) :
    // sets the current token to the head of passed-in token list
    prevToken(NULL),
    curToken(tokenListHead),
    curBlockType(BLOCK_TYPE::GLOBAL),
    unitSeen(UNIT::NO_UNIT)
    {}

ASTNode* Parser::parseStatement() {
    if (checkCurType(Tokenizer::TYPE_IF)) {
        // skip open-paren '('
        // set left child to symbol (binary tree)
        // consume '{' if existing
        // set center child to if-body
        // consume '}'
        // check 'else' exists. if yes, set right child to else-body
        
        ASTNode* left = parseExpression();
        ASTNode* center = parseBlock();

        // if we consume "else" (else exists)
        if (checkCurType(Tokenizer::TYPE_ELSE)) {
            ASTNode* right = parseBlock();
            IfElseNode* ifElseNode = new IfElseNode(curToken, 
                                                    left, center, right);
            return ifElseNode;
        } else {
            IfNode* ifNode = new IfNode();
            ifNode->setText("if");
            ifNode->setType(Tokenizer::TYPE_IF);
            ifNode->setLeft(left);
            ifNode->setRight(center);
            return ifNode;
        }
    }
    else if (checkCurType(Tokenizer::TYPE_LOOPING)) {
        // for loops
        if (checkCurText("for")) {
            if (consume(Tokenizer::TYPE_SYMBOL_PAREN_OPEN)) {
                
                // consume int declaration + semicolon (ie. int i = 0;)
                consume(Tokenizer::TYPE_PRIMITIVE);
                ASTNode* declaration = parsePrimitive();        // for now, should be primitive assignment
                // curToken is now at token after semicolon
                // consume inequality + semicolon (ie. i < 3;)
                ASTNode* condition = parseExpression();
                semicolon();
                next();
                // consume increment (ie. i = i + 1)
                ASTNode* increment = parseAssignment(curToken, IDENTIFIER_TYPE::NON_FUNCTION, false);
                ASTNode* codeBlock = parseBlock();
                
                /* Following for-loop tree will be created:
                                for
                        |                  |
                    identifier          if-else
                                 |          |        |
                         condition     code block    i++
                */
                IfElseNode* runOrIncrement = new IfElseNode(curToken,
                                                            condition, codeBlock, increment);
                LoopingNode* forLoop = new LoopingNode(curToken, 
                                                       declaration, runOrIncrement, LOOPING::FOR);
                return forLoop;
            }
        }

        // while loops
        else if (checkCurText("while")) {
            // left child is condition
            // right child is code to execute if true
            ASTNode* condition = parseExpression();
            ASTNode* codeToExecute = parseBlock();
            LoopingNode* whileLoop = new LoopingNode(curToken, 
                                                     condition, codeToExecute, LOOPING::WHILE);
            return whileLoop;
        }
        next();

        // do-while loops (future)
    }
    else if (checkCurType(Tokenizer::TYPE_RETURN)) {
        // must be one of the following:
        // 1. identifier
        // 2. literal (number, string)
        print("parsing return...");
        ReturnNode* returnNode = new ReturnNode(curToken);
        ASTNode* valueToReturn = parseExpression();
        // sole child should be identifier/literal AST
        returnNode->setChild(valueToReturn);
        semicolon();
        next();        
        return returnNode;
    }
    else if (checkCurType(Tokenizer::TYPE_KEYWORD)) {
        // left is name of identifier
        // right is body AST (everything between {})
        print("found keyword " + curToken->text);
        KEYWORD key = keywordTextToType.at(curToken->text);

        // parses reaction declaration in format WITHOUT curly braces
        // ie. reaction r1(eq = ..., krev = ...);
        if ((checkCurText("reaction") || checkCurText("protein") ||
             checkCurText("reagent") || checkCurText("container")) && 
            checkNextNextType(Tokenizer::TYPE_SYMBOL_PAREN_OPEN)) {
            print("in reaction declaration with ()");
            KeywordNode* reaction = parseReaction();
            reaction->setAllowStatements(false);
            return reaction;
        }

        if (consume(Tokenizer::TYPE_IDENTIFIER)) {
            curBlockType = keywordToBlock.at(key);
            // must have identifier after keyword
            
            // creating + setting identifier node with name
            std::string name = curToken->text;
            IdentifierNode* identifierNode = new IdentifierNode(curToken, name);
            print("found identifier " + curToken->text);

            
            
            openScope(name);

            /* check following character cases:
                1. () - function declaration
                2. {} - non-function declaration
            */
            bool isParen = checkNext()->type == Tokenizer::TYPE_SYMBOL_PAREN_OPEN;
            bool isCurly = checkNext()->type == Tokenizer::TYPE_SYMBOL_CURLY_OPEN;

            if (isParen) {
                print("isParen!");
                identifierNode->setType(IDENTIFIER_TYPE::FUNCTION);
                // to-do: currently don't support parameters in ()
                curScope->put(name, Tokenizer::TYPE_IDENTIFIER, "function");
                consume("(");
                consume(")");
            }
            else if (isCurly) {
                print("isCurly!");
                identifierNode->setType(IDENTIFIER_TYPE::NON_FUNCTION);
                curScope->put(name, Tokenizer::TYPE_IDENTIFIER, "class");
            }   
            else {
                fail("Keyword identifier should be followed with () for function or {} for non-function. \n", curToken);
            }
                
            ASTNode* block = parseBlock();  // for both cases
            KeywordNode* keywordNode = new KeywordNode(curToken, 
                                                       identifierNode, block, key);
            keywordNode->setKeyword(key);

            closeScope(name);
            curBlockType = BLOCK_TYPE::GLOBAL;
            return keywordNode;
        }
        else if (consume(Tokenizer::TYPE_IMPORT)) {
            ImportNode* importNode = parseImport();
            print("current text is: " + curToken->text);
            print("next text is: " + curToken->next->text);
            semicolon();
            next();
            return importNode;
        }
    }
    else if (checkCurType(Tokenizer::TYPE_PARAM)) {
        print("parsing parameter");
        SymbolNode* paramNode = parseParam();
        return paramNode;
    }
    else if (checkCurType(Tokenizer::TYPE_IDENTIFIER)) {
        print("parsing identifier " + curToken->text);
        if (checkNextType(Tokenizer::TYPE_SYMBOL_EQUAL)) {
            SymbolNode* assignmentNode = parseAssignment(curToken, IDENTIFIER_TYPE::NON_FUNCTION);
            next();
            return assignmentNode;
        }
        else if (checkNextType(Tokenizer::TYPE_SYMBOL_DOT)) {
            // function call on object
            print("function call");
            SymbolNode* dotNode = parseFunction();
            next();
            return dotNode;
        }
        else if (checkNextType(Tokenizer::TYPE_SYMBOL_BRACKET_OPEN)) {
            Tokenizer::Token* identifierToken = curToken;
            IndexNode* indexNode = parseIndex();
            SymbolNode* assignmentNode = parseAssignment(identifierToken, IDENTIFIER_TYPE::NON_FUNCTION, false);
            assignmentNode->setLeft(indexNode);
            next();
            return assignmentNode;
        }
    }
    else if (checkCurType(Tokenizer::TYPE_PRIMITIVE)) {
        print("parsing primitive...");
        SymbolNode* assignmentNode = parsePrimitive();
        next();
        return assignmentNode;
    }
    else if (checkCurType(Tokenizer::TYPE_INTEGER) ||
             checkCurType(Tokenizer::TYPE_FLOAT) ||
             checkCurType(Tokenizer::TYPE_CHEMICAL)) {
        /* only allowed for the following types:
            1. reagent (eq, mL, temp)
            2. container (mL, temp) */
        if (curBlockType == BLOCK_TYPE::CONTAINER ||
            curBlockType == BLOCK_TYPE::REAGENT) {
            SymbolNode* assignment = inferParam();
            return assignment;
        } 
        else {
            fail("Parameter cannot be inferred in this block type.", curToken);
            return nullptr;
        }
        
    }
    else {
        print("curToken text: " + curToken->text);
        fail("Failed to parse statement.\n", curToken);
        return nullptr;
    }
}

ASTNode* Parser::parseExpression() {
    // recursive call to parseTernaryOp(), which is the lowest
    // level of the expression hierarchy. Method recursively calls
    // upwards towards expression types with higher precedence.
    ASTNode* expression = parseTernaryOp();
    // Returns an expression AST.
    print("finished parsing expression");
    print(curToken->text);
    return expression;
}

ASTNode* Parser::parse() {

    // empty file or all commented out
    if (curToken->type == Tokenizer::TYPE_END) {
        print("ERROR: No tokens to parse. Empty file or all code in file is commented out.\n");
        exit(1);
    }
    
    std::cout << "+ Parsing..." << std::endl;
    bool first = true;
    // open global scope
    openScope("global");

    ASTNode* root = parseStatement();
    ASTNode* curNode = root;

    while (curToken->type != Tokenizer::TYPE_END) {
        ASTNode* nextStatement = parseStatement();
        curNode->setNextStatement(nextStatement);
        curNode = curNode->getNextStatement();
    }

    std::cout << "=================================" << std::endl;
    std::cout << "+ Tree successfully parsed!" << std::endl;
    std::cout << "---------------------------------" << std::endl;
    std::cout << "NOTE: '->' signifies next statement on the same level of the most\n";
    std::cout << "recent child (does not signify new child branch of parent).\n";
    std::cout << "The 'n' number of dashes represents the nth scope. For example,\n";
    std::cout << "statements with --> will be an inner scope, whereas -> will be the\n";
    std::cout << "outer scope. Arrows of the same type are chronological statements\n";
    std::cout << "in the same scope.\n";
    std::cout << "---------------------------------" << std::endl;

    // evaluate number operations
    // evaluateOperations(root);
    root->resetVisited();

    root->traverse("", "");
    root->resetVisited();
    // close global scope
    closeScope("global");
    printScopes();

    return root;
}

bool Parser::checkCur(Tokenizer::TokenType type, std::string text) {
    return curToken->type == type && 
           curToken->text == text;
}

bool Parser::checkCurText(std::string text) {
    return curToken->text == text;
}

bool Parser::checkCurType(Tokenizer::TokenType type) {
    return curToken->type == type;
}


void Parser::next() {
    if (curToken->type != Tokenizer::TYPE_END || 
        curToken->next != nullptr) {
            prevToken = curToken;
            curToken = curToken->next;
        }
    else 
        fail("Failed to retrieve next token with next().\n", curToken);
}

void Parser::prev() {
    if (curToken->prev != NULL) {
        curToken = prevToken;
        prevToken = prevToken->prev;
    }
    else
        fail("Failed to retrieve previous token with prev().\n", curToken);
}

bool Parser::consume(std::string text) {
    if (checkNextText(text)) {
        next();
        return true;
    }
    else {
        return false;
        fprintf(stderr, "Failed to consume %s\n", text.c_str());
    }
}

bool Parser::consume(Tokenizer::TokenType type) {
    if (checkNextType(type)) {
        next();
        return true;
    }
    else {
        return false;
        fprintf(stderr, "Failed to consume symbol enum ref %d\n", type);
    }
}

void Parser::semicolon() {
    bool exists = consume(";");
    if (!exists) {
        fail("Missing semicolon.\n", curToken);
    }
}

void Parser::semicolonOrParen() {
    if (checkNextType(Tokenizer::TYPE_SYMBOL_SEMICOLON))
        semicolon();
    else if (checkNextType(Tokenizer::TYPE_SYMBOL_PAREN_CLOSED))
        consume(Tokenizer::TYPE_SYMBOL_PAREN_CLOSED);
    else {
        print("Curtoken: " + curToken->text);
        fail("Found neither a required semicolon nor a closing parentheses.", curToken);
    }
}

void Parser::semicolonOrComma() {
    if (checkNextType(Tokenizer::TYPE_SYMBOL_SEMICOLON))
        semicolon();
    else if (checkNextType(Tokenizer::TYPE_SYMBOL_COMMA))
        consume(Tokenizer::TYPE_SYMBOL_COMMA);
    else {
        fail("Found neither a required semicolon nor a comma.", curToken);
    }
}

Tokenizer::Token* Parser::checkNext() {
    if (curToken->type != Tokenizer::TYPE_END || 
        curToken->next != nullptr) {
            return curToken->next;
        }
    else {
        fail("Failed to perform checkNext().\n", curToken);
        return nullptr;
    }
}

bool Parser::checkNextType(Tokenizer::TokenType type) {
    if (curToken->type == Tokenizer::TYPE_END) {
        fail("Cannot check next type b/c curToken is null.\n", curToken);
    }
    return curToken->next->type == type;
}

bool Parser::checkNextNextType(Tokenizer::TokenType type) {
    if (curToken->type == Tokenizer::TYPE_END ||
        curToken->next->type == Tokenizer::TYPE_END) {
        fail("Cannot check next type b/c curToken is null.\n", curToken);
    }
    return curToken->next->next->type == type;
}

bool Parser::checkNextNextNextType(Tokenizer::TokenType type) {
    if (curToken->type == Tokenizer::TYPE_END ||
        curToken->next->type == Tokenizer::TYPE_END) {
        fail("Cannot check next type b/c curToken is null.\n", curToken);
    }
    return curToken->next->next->next->type == type;
}

bool Parser::checkNextText(std::string text) {
    if (curToken->type == Tokenizer::TYPE_END) {
        fail("Cannot check next text b/c curToken is null.\n", curToken);
    }
    return checkNext()->text == text;
}

/* Expression Hierarchy */
// https://techvidvan.com/tutorials/expression-parsing-in-data-structure/

ASTNode* Parser::parseTopLevelExpression() {
    if (checkNextType(Tokenizer::TYPE_SYMBOL_PAREN_OPEN)) {
        ASTNode* parenExp = parseParen();
        return parenExp;
    }
    else if (checkNextType(Tokenizer::TYPE_IDENTIFIER)) {
        print("found identifier");
        IdentifierNode* identifier = parseIdentifier();
        identifier->printNode();
        print("identifier printed. Now at: " + curToken->text);
        return identifier;
    }
    else if (checkNextType(Tokenizer::TYPE_CHEMICAL)) {
        ChemicalNode* chemical = parseChemical();
        return chemical;
    }
    else if (checkNextType(Tokenizer::TYPE_INTEGER) ||
             checkNextType(Tokenizer::TYPE_FLOAT)) {
        print("parsing literal...");
        ASTNode* number = parseLiteral();
        number->printNode();
        return number;
    }
    else {
        fail("Failed at parsing top level expression.\n", curToken);
    }
    return nullptr;
}

ASTNode* Parser::parseBracket() {
    ASTNode* op = parseTopLevelExpression();
    print("brackets");
    return op;
}

ASTNode* Parser::parseIncrement() {
    ASTNode* op = parseBracket();
    print("increment");
    return op;
}

// x = 5 * 3
ASTNode* Parser::parseMulDivMod() {
    print("parsing mul div mod...");
    ASTNode* op = parseIncrement(); // above recursively
    print("finished mul div mod");
    bool mulDivOrMod = checkNextType(Tokenizer::TYPE_SYMBOL_MULTIPLY) || 
                       checkNextType(Tokenizer::TYPE_SYMBOL_DIVIDE) || 
                       checkNextType(Tokenizer::TYPE_SYMBOL_PERCENT);
    
    if (mulDivOrMod) {
        SymbolNode* symbolNode = new SymbolNode(curToken);
        if (consume(Tokenizer::TYPE_SYMBOL_MULTIPLY)) {
            symbolNode->setSymbol(SYMBOL::MULTIPLY);
        }
        else if (consume(Tokenizer::TYPE_SYMBOL_DIVIDE)) {
            symbolNode->setSymbol(SYMBOL::DIVIDE);
        }
        else if (consume(Tokenizer::TYPE_SYMBOL_PERCENT)) {
            symbolNode->setSymbol(SYMBOL::PERCENT);
        }
        symbolNode->setLeft(op);
        symbolNode->setRight(parseMulDivMod());
        print("back from increment");
        return symbolNode;
    }
    else {
        print("down. cur text is: " + curToken->text);
        return op;
    }
}

ASTNode* Parser::parseAddSub() {
    ASTNode* op = parseMulDivMod(); // above recursively
    print("parsing add sub: " + curToken->text);

    bool addOrSub = checkNextType(Tokenizer::TYPE_SYMBOL_ADD) ||
                    (checkNextType(Tokenizer::TYPE_SYMBOL_SUBTRACT) &&
                     !checkNextNextType(Tokenizer::TYPE_SYMBOL_SUBTRACT));

    print("checking add or sub");
    if (addOrSub) {
        SymbolNode* symbolNode = new SymbolNode(curToken);
        if (consume(Tokenizer::TYPE_SYMBOL_ADD)) {
            print("adding...");
            symbolNode->setSymbol(SYMBOL::ADD);
        }
        else if (consume(Tokenizer::TYPE_SYMBOL_SUBTRACT)) {
            print("subtracting...");
            symbolNode->setSymbol(SYMBOL::SUBTRACT);
        }
        symbolNode->setLeft(op);
        symbolNode->setRight(parseAddSub());
        return symbolNode;
    }
    else {
        print("down");
        return op;
    }
}


ASTNode* Parser::parseArrow() {
    ASTNode* op = parseAddSub();
    print("parsing arrow");
    print(curToken->text);
    
    bool forward = checkNextType(Tokenizer::TYPE_SYMBOL_SUBTRACT) &&
                   checkNextNextType(Tokenizer::TYPE_SYMBOL_SUBTRACT) &&
                   checkNextNextNextType(Tokenizer::TYPE_SYMBOL_GT);
    bool backward = checkNextType(Tokenizer::TYPE_SYMBOL_LT) &&
                    checkNextNextType(Tokenizer::TYPE_SYMBOL_SUBTRACT) &&
                    checkNextNextNextType(Tokenizer::TYPE_SYMBOL_SUBTRACT);
    bool reversible = checkNextType(Tokenizer::TYPE_SYMBOL_LT) &&
                      checkNextNextType(Tokenizer::TYPE_SYMBOL_SUBTRACT) &&
                      checkNextNextNextType(Tokenizer::TYPE_SYMBOL_GT);
    bool inhibition = checkNextType(Tokenizer::TYPE_SYMBOL_SUBTRACT) &&
                      checkNextNextType(Tokenizer::TYPE_SYMBOL_SUBTRACT) &&
                      checkNextNextNextType(Tokenizer::TYPE_SYMBOL_OR);

    SymbolNode* arrow = new SymbolNode();
    if (forward) {
        print("forward");
        consume(Tokenizer::TYPE_SYMBOL_SUBTRACT);
        consume(Tokenizer::TYPE_SYMBOL_SUBTRACT);
        consume(Tokenizer::TYPE_SYMBOL_GT);
        arrow->setSymbol(SYMBOL::FORWARD);
    }
    else if (backward) {
        print("backward");
        consume(Tokenizer::TYPE_SYMBOL_LT);
        consume(Tokenizer::TYPE_SYMBOL_SUBTRACT);
        consume(Tokenizer::TYPE_SYMBOL_SUBTRACT);
        arrow->setSymbol(SYMBOL::BACKWARD);
    }
    else if (reversible) {
        print("reversible");
        consume(Tokenizer::TYPE_SYMBOL_LT);
        consume(Tokenizer::TYPE_SYMBOL_SUBTRACT);
        consume(Tokenizer::TYPE_SYMBOL_GT);
        arrow->setSymbol(SYMBOL::REVERSIBLE);
    }
    else if (inhibition) {
        print("inhibition");
        consume(Tokenizer::TYPE_SYMBOL_SUBTRACT);
        consume(Tokenizer::TYPE_SYMBOL_SUBTRACT);
        consume(Tokenizer::TYPE_SYMBOL_OR);
        arrow->setSymbol(SYMBOL::INHIBITION);
    }
    else {
        print("deleting arrow..");
        delete arrow;
        return op;
    }

    arrow->setLeft(op);
    arrow->setRight(parseAddSub());
    return arrow;
}

ASTNode* Parser::parseSlice() {
    print("slicing...");
    SymbolNode* slice = new SymbolNode(curToken);
    slice->setSymbol(SYMBOL::COLON);
    ASTNode* op;
    NumberNode* firstIndex;

    // case 1: slice in format [:]
    if (checkNextType(Tokenizer::TYPE_SYMBOL_COLON)) {
        consume(Tokenizer::TYPE_SYMBOL_COLON);
        if (checkNextType(Tokenizer::TYPE_SYMBOL_BRACKET_CLOSED)) {
            slice->setLeft(new ASTNode());
            slice->setRight(new ASTNode());
            return slice;
        }
        else {
            slice->setLeft(new ASTNode());
            slice->setRight(parseArrow());
            return slice;
        }
    }
    else {
        op = parseArrow();
    }
    
    // case 2: slice in format [0:]
    if (checkNextType(Tokenizer::TYPE_SYMBOL_COLON)) {
        consume(Tokenizer::TYPE_SYMBOL_COLON);
        
        firstIndex = op->evaluate(curScope);

        if (checkNextType(Tokenizer::TYPE_SYMBOL_BRACKET_CLOSED)) {
            firstIndex = op->evaluate(curScope);
            slice->setLeft(firstIndex);
            slice->setRight(new ASTNode());
            return slice;
        }
        // case 3 (general): slice in format [0:1]
        else {
            slice->setLeft(firstIndex);
            NumberNode* secondIndex = parseArrow()->evaluate(curScope);
            slice->setRight(secondIndex);
            return slice;
        }
    }
    else {
        delete slice;
        return op;
    }
}

ASTNode* Parser::parseLesserGreater() {
    ASTNode* op = parseSlice();
    op->printNode();
    op->singlePrintNodeChildren();
    bool lesserOrGreater = checkNextType(Tokenizer::TYPE_SYMBOL_GEQ) ||
                           checkNextType(Tokenizer::TYPE_SYMBOL_LEQ) ||
                           checkNextType(Tokenizer::TYPE_SYMBOL_GT) ||
                           checkNextType(Tokenizer::TYPE_SYMBOL_LT);
    if (lesserOrGreater) {
        SymbolNode* symbolNode = new SymbolNode(curToken);
        if (consume(Tokenizer::TYPE_SYMBOL_LEQ)) {
            symbolNode->setSymbol(SYMBOL::LEQ);
        }
        else if (consume(Tokenizer::TYPE_SYMBOL_LT)) {
            symbolNode->setSymbol(SYMBOL::LT);
        }
        else if (consume(Tokenizer::TYPE_SYMBOL_GEQ)) {
            symbolNode->setSymbol(SYMBOL::GEQ);
        }
        else if (consume(Tokenizer::TYPE_SYMBOL_GT)) {
            symbolNode->setSymbol(SYMBOL::GT);
        }
        symbolNode->setLeft(op);
        symbolNode->setRight(parseAddSub());
        return symbolNode;
    }
    else {
        return op;
    }
}

ASTNode* Parser::parseEq() {
    ASTNode* op = parseLesserGreater();
    // must have "==", not "="
    bool eq = checkNextType(Tokenizer::TYPE_SYMBOL_EQUAL) && 
              checkNextNextType(Tokenizer::TYPE_SYMBOL_EQUAL);
    
    bool neq = checkNextType(Tokenizer::TYPE_SYMBOL_NOT) && 
               checkNextNextType(Tokenizer::TYPE_SYMBOL_EQUAL);


    if (eq || neq) {
        SymbolNode* symbolNode = new SymbolNode(curToken);
        if (consume(Tokenizer::TYPE_SYMBOL_EQUAL) && 
            consume(Tokenizer::TYPE_SYMBOL_EQUAL)) {
                symbolNode->setSymbol(SYMBOL::EQUALS);
        }
        else if (consume(Tokenizer::TYPE_SYMBOL_NOT) &&
                 consume(Tokenizer::TYPE_SYMBOL_EQUAL)) {
                symbolNode->setSymbol(SYMBOL::NOT_EQUALS);
        }
        symbolNode->setLeft(op);
        symbolNode->setRight(parseLesserGreater());
        return symbolNode;
    }
    else {
        return op;
    }
}

ASTNode* Parser::parseBitAnd() {
    ASTNode* op = parseEq();
    // must have "&", not "&&"
    bool bitAnd = checkNextType(Tokenizer::TYPE_SYMBOL_AND) && 
                  !checkNextNextType(Tokenizer::TYPE_SYMBOL_AND);


    if (bitAnd) {
        SymbolNode* symbolNode = new SymbolNode(curToken);
        if (consume(Tokenizer::TYPE_SYMBOL_EQUAL)) {
            symbolNode->setSymbol(SYMBOL::BIT_AND);
        }
        symbolNode->setLeft(op);
        symbolNode->setRight(parseEq());
        return symbolNode;
    }
    else {
        return op;
    }
}

ASTNode* Parser::parseBitOr() {
    ASTNode* op = parseBitAnd();
    // must have "|", not "||"
    bool bitOr = checkNextType(Tokenizer::TYPE_SYMBOL_OR) && 
                  !checkNextNextType(Tokenizer::TYPE_SYMBOL_OR);

    if (bitOr) {
        SymbolNode* symbolNode = new SymbolNode(curToken);
        if (consume(Tokenizer::TYPE_SYMBOL_OR)) {
            symbolNode->setSymbol(SYMBOL::BIT_OR);
        }
        symbolNode->setLeft(op);
        symbolNode->setRight(parseBitAnd());
        return symbolNode;
    }
    else {
        return op;
    }
}

ASTNode* Parser::parseLogiAnd() {
    ASTNode* op = parseBitOr();
    // must have "&&", not "&"
    bool logiAnd = checkNextType(Tokenizer::TYPE_SYMBOL_AND) && 
                   checkNextNextType(Tokenizer::TYPE_SYMBOL_AND);

    if (logiAnd) {
        SymbolNode* symbolNode = new SymbolNode(curToken);
        if (consume(Tokenizer::TYPE_SYMBOL_AND) && 
            consume(Tokenizer::TYPE_SYMBOL_AND)) {
            symbolNode->setSymbol(SYMBOL::LOGI_AND);
        }
        symbolNode->setLeft(op);
        symbolNode->setRight(parseBitOr());
        return symbolNode;
    }
    else {
        return op;
    }
}

ASTNode* Parser::parseLogiOr() {
    ASTNode* op = parseLogiAnd();
    // must have "||", not "|"
    bool logiOr = checkNextType(Tokenizer::TYPE_SYMBOL_OR) && 
                   checkNextNextType(Tokenizer::TYPE_SYMBOL_OR);

    if (logiOr) {
        SymbolNode* symbolNode = new SymbolNode(curToken);
        if (consume(Tokenizer::TYPE_SYMBOL_OR) && 
            consume(Tokenizer::TYPE_SYMBOL_OR)) {
            symbolNode->setSymbol(SYMBOL::LOGI_OR);
        }
        symbolNode->setLeft(op);
        symbolNode->setRight(parseLogiAnd());
        return symbolNode;
    }
    else {
        return op;
    }
}

ASTNode* Parser::parseTernaryOp() {
    ASTNode* op = parseLogiOr();
    return op;
}


// helper methods
// ====================================
ASTNode* Parser::parseBlock() {
    consume("{");
    next();
    bool first = true;
    // instantiated for if block is empty. 
    ASTNode* blockStatement = new ASTNode();
    blockStatement->setText("<empty block>");
    ASTNode* curStatement;
    while (!checkCurType(Tokenizer::TYPE_SYMBOL_CURLY_CLOSED)) {
        if (first) {
            blockStatement = parseStatement();
            curStatement = blockStatement; 
            first = false;
        }
        else {
            curStatement->setNextStatement(parseStatement());
            curStatement = curStatement->getNextStatement();
        }
    }
    next();
    return blockStatement;
}

SymbolNode* Parser::inferParam() {
    prev();

    bool foundChemical = Tokenizer::IsChemical(checkNext());
    ASTNode* value = parseExpression();
    PARAM inferredParam = inferUnit(unitSeen);
    
    ParamNode* param = new ParamNode(curToken, inferredParam);
    if (foundChemical) {
        param->setParamType(PARAM::EQUATION);
        curScope->put("eq", Tokenizer::TYPE_PARAM, "eq");
    } else {
        curScope->put(paramTypeToText.at(inferredParam), Tokenizer::TYPE_PARAM, value->evaluate(curScope)->getNum());
    }

    SymbolNode* assignment = new SymbolNode();
    assignment->setSymbol(SYMBOL::ASSIGNMENT);
    assignment->setLeft(param);
    if (param->getParamType() == PARAM::EQUATION) {
        assignment->setRight(value);
    } else {
        assignment->setRight(value->evaluate(curScope));
    }
    
    parseNextParam(assignment);
    resetUnitSeen();
    return assignment;
}

SymbolNode* Parser::parseParam() {
    // parse expression
    // consume comma
    bool chemical = checkNextType(Tokenizer::TYPE_CHEMICAL);
    bool chemicalCoeff = checkNextType(Tokenizer::TYPE_INTEGER) && 
                         checkNextNextType(Tokenizer::TYPE_CHEMICAL);
    bool identifier = checkNextType(Tokenizer::TYPE_IDENTIFIER);
    bool eq = checkNextType(Tokenizer::TYPE_SYMBOL_EQUAL);
    bool param = checkNextType(Tokenizer::TYPE_PARAM);

    if (chemical || chemicalCoeff || eq || param || identifier) {
        // if param exists, consume param
        consume(Tokenizer::TYPE_PARAM);
        ParamNode* paramNode;
        std::string paramName = curToken->text;
        
        if (checkNextType(Tokenizer::TYPE_CHEMICAL) ||
            (checkNextType(Tokenizer::TYPE_INTEGER) && checkNextNextType(Tokenizer::TYPE_CHEMICAL)) ||
            checkNextType(Tokenizer::TYPE_IDENTIFIER) ||
            (checkNextType(Tokenizer::TYPE_INTEGER) && checkNextNextType(Tokenizer::TYPE_IDENTIFIER))) {
            // eliminating need for 'eq' in reactions
            paramNode = new ParamNode(curToken, PARAM::EQUATION);
            paramName = "eq";
        }
        else if (consume(Tokenizer::TYPE_SYMBOL_EQUAL)) {
            consume(Tokenizer::TYPE_SYMBOL_EQUAL);
            paramNode = new ParamNode(curToken, translateParamType(paramName));   
        }

        ASTNode* expressionTree = parseExpression();
        if (paramName == "eq") {
            curScope->put(paramName, Tokenizer::TYPE_PARAM, "eq");  
        } else {
            curScope->put(paramName, Tokenizer::TYPE_PARAM, expressionTree->evaluate(curScope)->getNum());  
        }
        
        SymbolNode* assignmentNode = new SymbolNode(curToken);
        assignmentNode->setSymbol(SYMBOL::ASSIGNMENT);
        assignmentNode->setLeft(paramNode);
        if (paramNode->getParamType() == PARAM::EQUATION) {
            assignmentNode->setRight(expressionTree);
        } else {
            assignmentNode->setRight(expressionTree->evaluate(curScope));
        }
        
        parseNextParam(assignmentNode);

        return assignmentNode;
    }
    else if (consume(Tokenizer::TYPE_INTEGER) || consume(Tokenizer::TYPE_FLOAT)) {
        SymbolNode* assignmentNode = inferParam();
        return assignmentNode;
    }
    else {
        fail("Parsing parameter but equality not found.\n", curToken);
        return nullptr;
    }
}

void Parser::parseNextParam(SymbolNode* curParam) {
    if (consume(Tokenizer::TYPE_SYMBOL_COMMA)) {
        next();
        curParam->hasNextStatement = true;
        // inside parameter of supported function call like Centrifuge
        if (checkCurType(Tokenizer::TYPE_PARAM)) {
            SymbolNode* nextParam = parseParam();
            curParam->setNextStatement(nextParam);
        } else {
            SymbolNode* nextNamelessParam = inferParam();
            curParam->setNextStatement(nextNamelessParam);
            print("finished parsing successive nameless param");
        }
    }
    else {
        // otherwise, if encounter ';' or ')' end of function call,
        // simply return the parameter assignment node.
        semicolonOrParen();
        next();
    }
}

SymbolNode* Parser::parseAssignment(Tokenizer::Token* identifierToken, IDENTIFIER_TYPE type, bool evaluate, PRIMITIVE_TYPE primitive) {
    // create identifier node + set name variable to curToken text
    IdentifierNode* identifierNode = new IdentifierNode(identifierToken, identifierToken->text);
    identifierNode->setType(type);
    
    if (consume(Tokenizer::TYPE_SYMBOL_EQUAL)) {
        ASTNode* expressionTree = parseExpression();
        NumberNode* parsedExpression = expressionTree->evaluate(curScope);
        SymbolNode* assignmentNode = new SymbolNode();
        assignmentNode->setSymbol(SYMBOL::ASSIGNMENT);
        assignmentNode->setLeft(identifierNode);

        if (evaluate) {
            assignmentNode->setRight(parsedExpression);
            if (type == IDENTIFIER_TYPE::PRIMITIVE) {
                identifierNode->setPrimitiveType(primitive);
                curScope->put(identifierNode->getName(), Tokenizer::TYPE_PRIMITIVE, parsedExpression->getNum());
            } else {
                curScope->put(identifierNode->getName(), Tokenizer::TYPE_IDENTIFIER, parsedExpression->getNum());
            }
        } else {
            assignmentNode->setRight(expressionTree);
        }

        if (type == IDENTIFIER_TYPE::PRIMITIVE) {
            identifierNode->setPrimitiveType(primitive);
            curScope->put(identifierNode->getName(), Tokenizer::TYPE_PRIMITIVE, parsedExpression->getNum());
        } else {
            curScope->put(identifierNode->getName(), Tokenizer::TYPE_IDENTIFIER, parsedExpression->getNum());
        }
        
        if (checkNextType(Tokenizer::TYPE_SYMBOL_PAREN_CLOSED)) {
            // only allowed for 'for-loops' - checking should be improved later
            consume(Tokenizer::TYPE_SYMBOL_PAREN_CLOSED);
        }
        else {
            semicolonOrComma();
        }
        
        return assignmentNode;
    }
    else {
        fail("Parsing identifier assignment but equals symbol (=) not found.\n", curToken);
        return nullptr;
    }
}

SymbolNode* Parser::parseFunction() {
    // get name of object that function is called upon
    std::string identifierName = curToken->text;

    if (consume(Tokenizer::TYPE_SYMBOL_DOT)) {
        print("consumed dot");
        // check if next function is valid
        if (consume(Tokenizer::TYPE_FUNCTION)) {
            // declare necessary nodes for function
            // consume function
            std::string functionName = curToken->text;
            print("consumed function " + functionName);
            IdentifierNode* identifierNode = new IdentifierNode(curToken, identifierName);
            identifierNode->setType(IDENTIFIER_TYPE::FUNCTION);
            SymbolNode* dotNode = new SymbolNode(curToken);
            dotNode->setSymbol(SYMBOL::DOT);
            FunctionNode* functionNode = new FunctionNode(curToken, 
                                                          functionName, FUNCTION_TYPE::INSTANCE);
            
            // functionNode->setReturnType();   // have to determine if void or return

            if (parseFunctionParen(functionNode)) {
                dotNode->setLeft(identifierNode);
                dotNode->setRight(functionNode);
                semicolon();
                curScope->put(functionName, Tokenizer::TYPE_FUNCTION, "function");
                return dotNode;
            }
            else {
                delete identifierNode;
                delete dotNode;
                delete functionNode;
                fail("Parentheses invalid or not found after function call.\n", curToken);
                return nullptr;
            }
        }
        else {
            fail("Parsing identifier function call but function is either invalid or missing after dot.\n", curToken);
            return nullptr;
        }
    }

}

bool Parser::parseFunctionParen(FunctionNode* function) {
    if (consume(Tokenizer::TYPE_SYMBOL_PAREN_OPEN)) {
        // case 1: default - "()"
        if (consume(Tokenizer::TYPE_SYMBOL_PAREN_CLOSED)) {
            return true;
        }
        else if (checkNextType(Tokenizer::TYPE_PARAM)) {
            // case 2: parameters - "( <n parameters here> )"
            next(); // move onto parameter name
            function->setHasParams(true);
            SymbolNode* param = parseParam();
            function->setChild(param);
            return true;
        }
        // case 3 (to-do): paramters without parameter name - "function(3)"
        return false;
    }
    return false;
}

SymbolNode* Parser::parsePrimitive() {
    PRIMITIVE_TYPE primitive = translatePrimitiveType(curToken->text);

    if (consume(Tokenizer::TYPE_IDENTIFIER)) {
        SymbolNode* assignment = parseAssignment(curToken, IDENTIFIER_TYPE::PRIMITIVE, true, primitive);
        return assignment; 
    }
    else {
        fail("Identifier not found after primitive declaration.\n", curToken);
        return nullptr;
    }
}


// ========================================
// top level expression methods
ASTNode* Parser::parseParen() {
    if (consume("(")) {
        ASTNode* exp = parseExpression();
        consume(")");
        return exp;
    }
}

IdentifierNode* Parser::parseIdentifier() {
    if (consume(Tokenizer::TYPE_IDENTIFIER)) {
        std::string name = curToken->text;
        IdentifierNode* identiferNode = new IdentifierNode(curToken, name);
        print("Parsed Identifier: " + name);
        // curScope->put(name, Tokenizer::TYPE_IDENTIFIER, 0.0);
        return identiferNode;
    }
    fail("Parsing identifier but identifier not found.\n", curToken);
    return nullptr;
}

ChemicalNode* Parser::parseChemical() {
    if (consume(Tokenizer::TYPE_CHEMICAL)) {
        // curToken is chemical type token
        std::string formula = curToken->text;
        ChemicalNode* chemicalNode = new ChemicalNode(curToken, formula);
        curScope->put(curToken->text, Tokenizer::TYPE_CHEMICAL, "chemical");
        return chemicalNode;
    }
    fail("Parsing chemical but chemical not found.\n", curToken);
    return nullptr;
}

ASTNode* Parser::parseLiteral() {
    if (consume(Tokenizer::TYPE_INTEGER) ||
        consume(Tokenizer::TYPE_FLOAT)) {
            /* std::stof - parses str interpreting its content as a floating-point 
            number, which is returned as a value of type float. */
            float stringToFloat = std::stof(curToken->text);
            NumberNode* numNode = new NumberNode(curToken);
            numNode->setNum(stringToFloat);
            NUMBER numType = isInteger(stringToFloat) ? NUMBER::INTEGER : NUMBER::FLOAT;
            numNode->setNumType(numType);
            
            if (consume(Tokenizer::TYPE_UNIT)) {
                // parses and sets unit + prefix of node
                parseUnit(numNode);
            }
            else if (checkNextType(Tokenizer::TYPE_CHEMICAL)) {
                print("parsing chemical with coefficient");
                numNode->setPrefix(PREFIX::NO_PREFIX);
                numNode->setUnit(UNIT::NO_UNIT);
                SymbolNode* symbolNode = new SymbolNode(curToken);
                symbolNode->setSymbol(SYMBOL::MULTIPLY);
                symbolNode->setLeft(numNode);
                symbolNode->setRight(parseChemical());
                print("finished parsing chemical with coefficient");
                return symbolNode;
            }
            else {
                numNode->setPrefix(PREFIX::NO_PREFIX);
                numNode->setUnit(UNIT::NO_UNIT);
            }
            return numNode;
    }
    fail("Parsing literal but literal not found.\n", curToken);
    return nullptr;
}

void Parser::parseUnit(NumberNode* precedingNumber) {
    std::string text = curToken->text;
    std::string prefixText(1, text[0]);       // first character prefix
    std::string unitText;
    int unitStartIndex = 1;
    PREFIX prefix;
    UNIT unit;

    // text contains unit WITH prefix
    if (!unitTextToType.count(text)) {
        if (text[1] == 'a') {
            // edge case for deca
            // check works becase no unit is 'a'. only ampere is 'A'.
            prefixText.append("a");
            unitStartIndex = 2;     // start AFTER  "da", at second index
        }
        
        unitText = text.substr(unitStartIndex);  
        // setting prefix of number node
        prefix = translatePrefixType(prefixText);
        precedingNumber->setPrefix(prefix);
        unit = translateUnitType(unitText);
    }
    else {
        // entire text only contains unit (WITHOUT prefix)
        // this case should only cover the need for "m" and "G", for now.
        // possibly "M" for molarity in the future. molarity = MOLAR for now.
        precedingNumber->setPrefix(PREFIX::NO_PREFIX);
        unit = translateUnitType(text);
    }
    unitSeen = unit;
    precedingNumber->setUnit(unit);
}

ImportNode* Parser::parseImport() {
    if (checkCurType(Tokenizer::TYPE_IMPORT)) {
        // must have valid import type (ie. Centrifuge) after keyword 'import'
        std::string importName = curToken->text;
        IMPORT_TYPE import = translateImportType(importName);
        ImportNode* importNode = new ImportNode(curToken, import);
        curScope->put(importName, Tokenizer::TYPE_IMPORT, "import");
        return importNode;
    }
    fail("Invalid or no import found directly after keyword \'import\'.\n", curToken);
    return nullptr;
}

KeywordNode* Parser::parseReaction() {
    if (consume(Tokenizer::TYPE_IDENTIFIER)) {
        KeywordNode* reaction = new KeywordNode(curToken);
        reaction->setKeyword(KEYWORD::REACTION);
        std::string name = curToken->text;
        curScope->put(name, Tokenizer::TYPE_IDENTIFIER, "reaction");
        openScope(name);
        IdentifierNode* reactionName = new IdentifierNode(curToken, name);
        reactionName->setType(IDENTIFIER_TYPE::NON_FUNCTION);
        consume(Tokenizer::TYPE_SYMBOL_PAREN_OPEN);
        // no need to next --> rid of param name "eq"
        ASTNode* reactionParams = parseParam();
        // consume(Tokenizer::TYPE_SYMBOL_PAREN_OPEN);
        reaction->setLeft(reactionName);
        reaction->setRight(reactionParams);
        next(); // move onto first character of next statement
        closeScope(name);
        return reaction;
    }
    fail("Parsing reaction failed.", curToken);
    return nullptr;
}

IndexNode* Parser::parseIndex() {
    IdentifierNode* identifier = new IdentifierNode(curToken,     
                                                    curToken->text, IDENTIFIER_TYPE::NON_FUNCTION);
    consume(Tokenizer::TYPE_SYMBOL_BRACKET_OPEN);
    ASTNode* index = parseExpression();
    if (consume(Tokenizer::TYPE_SYMBOL_BRACKET_CLOSED)) {
        IndexNode* indexNode = new IndexNode();
        indexNode->setLeft(identifier);
        indexNode->setRight(index);
        return indexNode;
    }
    delete identifier;
    delete index;
    fail("Closing square bracket not found.", curToken);
    return nullptr;
} 

Scope* Parser::getScope(std::string newScopeName) {
    Scope* scope;
    try {
        scope = scopes.at(newScopeName);
    }
    catch (const std::out_of_range& oor){
        fail("Scope " + newScopeName + " not found in map of scopes", curToken);
    }
    return scope;
}

void Parser::openScope(std::string newScopeName) {
    Scope* scope = new Scope();
    curScope = scope;
    curScopeName = newScopeName;
    spaghetti.push(scope);
}

void Parser::closeScope(std::string name) {
    print("closing " + name + " scope...");
    Scope* scope = spaghetti.top();
    spaghetti.pop();
    if (!spaghetti.empty()) {
        Scope* parent = spaghetti.top();
        scope->setParentScope(true, parent);
        parent->setChildScope(true, scope);
        curScope = parent;
    } else {
        scope->setParentScope(false, NULL);
    }
    scopes.insert(std::pair<std::string, Scope*>(name, scope));
}

void Parser::printScopes() {
    print("+ Printing all scopes' symbol tables... \n");
    int scopeCount = 1;
    std::unordered_map<Scope*, std::string> reverseScopes;
    for (auto const& scope: scopes) {
        reverseScopes.insert(std::pair<Scope*, std::string>(scope.second, scope.first));
    }
    
    for (auto const& scope : scopes) {
        std::string parentScopeName;
        std::string childScopeName;

        Scope* parent = scope.second->getParentScope();
        Scope* child = scope.second->getChildScope();
        if (reverseScopes.count(parent)) {
            parentScopeName = reverseScopes.at(parent);
        } else {
            parentScopeName = "NONE";
        }
        
        if (reverseScopes.count(child)) {
            childScopeName = reverseScopes.at(child);
        } else {
            childScopeName = "NONE";
        }

        std::cout << scope.first << " scope " << ":\n";
        std::cout << "\tparent = " << parentScopeName << std::endl;
        std::cout << "\tchild = " << childScopeName << std::endl;
        scope.second->printSymbolTable();
        print("\n");
        scopeCount++;
    }
}

void Parser::evaluateOperations(ASTNode* root) {
    ASTNode* cur = root;
    // have to visit every single node possible to find all assignment nodes
    // bfs to find all stuff
    
    std::queue<ASTNode*> queue;
    std::vector<ASTNode*> children;
    queue.push(cur);

    while (!queue.empty()) {
        cur = queue.front();
        queue.pop();
        std::cout << cur->visited;
        if (!cur->visited) {
            cur->visited = true;
            if (cur->getNodeType() == NODE::SYMBOL_NODE) {
                SymbolNode* symbol = dynamic_cast<SymbolNode*>(cur);
                if (symbol->getSymbol() == SYMBOL::ASSIGNMENT) {
                    if (symbol->getLeft()->getNodeType() == NODE::IDENTIFIER_NODE) {
                        IdentifierNode* identifier = dynamic_cast<IdentifierNode*>(symbol->getLeft());
                        NumberNode* result = symbol->getRight()->evaluate(curScope);
                        symbol->setRight(result);
                        std::cout << "PRINTING SCOPES for identifier\n";
                        printScopes();
                        std::cout << identifier->getName();
                        // scopes.at(identifier->getName())->printSymbolTable();
                        // scopes.at(identifier->getName())->putVal(identifier->getName(), result->getNum());
                    }
                    else if (dynamic_cast<ParamNode*>(symbol->getLeft())->getParamType() != PARAM::EQUATION) {
                        ParamNode* param = dynamic_cast<ParamNode*>(symbol->getLeft());
                        NumberNode* result = symbol->getRight()->evaluate(curScope);
                        symbol->setRight(result);
                        std::cout << "PRINTING SCOPES for param\n";
                        printScopes();
                        std::cout << paramTypeToText.at(param->getParamType()) << std::endl;
                    }

                }
            } else {
                // push all children
                // push next statement
                children = cur->getChildren();
                for (ASTNode* child : children) {
                    queue.push(child);
                }
            }
            if (cur->hasNextStatement) {
                queue.push(cur->getNextStatement());  
            }
        }
    }
}

PARAM Parser::inferUnit(UNIT unitToInfer) {
    PARAM inferred;
    try {
        inferred = unitToParam.at(unitToInfer);
    } catch (const std::out_of_range& oor) {
        fail("Unit was not or cannot be inferred successfully.", nullptr);
    }
    return inferred;
}

void Parser::resetUnitSeen() {
    unitSeen = UNIT::NO_UNIT;
}