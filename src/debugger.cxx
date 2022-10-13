#include "debugger.h"

#define LPP_FILENAME_OFFSET 3

Debugger::Debugger() :
    tokens(NULL),
    tree(NULL)
    {}

Debugger::Debugger(Tokenizer::Token* newTokens) :
    tokens(newTokens),
    tree(NULL)
    {}

Debugger::Debugger(ASTNode* newTree) :
    tokens(NULL),
    tree(newTree)
    {}

void Debugger::debug(Tokenizer* tokenizer, Tokenizer::Token* tokens) {

    std::string input;
    Tokenizer::Token* cur = tokens->prev;
    
    while (true) {
        std::cout << "\n(debug) "; 
        std::cin >> input;
        if (input == "p" || input == "print") {
            tokenizer->printTokenInfo(cur);
        }
        else if (input == "n" || input == "next") {
            if (cur->type == Tokenizer::TYPE_END) {
                fprintf(stderr, "End of token stream. Cannot move onto next token.\n");
                continue;
            }
            else {
                cur = cur->next;
                tokenizer->printTokenInfo(cur);
            }
        }
        else if (input == "b" || input == "back") {
            if (cur->type == Tokenizer::TYPE_START) {
                fprintf(stderr, "Cannot go back because there is no previous token.\n");
                continue;
            }
            else {
                cur = cur->prev;
                tokenizer->printTokenInfo(cur);
            }
        }
        else if (input == "q") {
            break;
        }
        else {
            std::cout << "Invalid command. Enter one of the following commands:\n";
            std::cout << "   • print (p) - print the current token\n";
            std::cout << "   • next (n) - move on to the next token + print\n";
            std::cout << "   • back (b) - go back to the previous token + print\n";
            std::cout << "   • quit (q) - quit debugging process\n";
            continue;
        }
    }
    std::cout << "Terminated debugging process.\n";
}


void Debugger::debug(Parser* parser, ASTNode* tree) {
    std::string input;
    ASTNode* cur = tree;
    std::stack<ASTNode*> unvisited;
    std::vector<ASTNode*> children;
    
    int childNum;
    
    std::stack<ASTNode*> history;
    history.push(cur);
    std::string curScopeName = "global";
    Scope* curScope = parser->getScope(curScopeName);
    std::stack<std::string> scopeStack;
    scopeStack.push(curScopeName);
    std::stack<int> numStatementsStack;
    int numStatements = 1;
    
    
    while (true) {
        std::cout << "\n(debug) "; 
        std::cin >> input;
        if (input == "p" || input == "print") {
            cur->singlePrintNodeChildren();
        }
        else if (input == "st" || input == "symboltable") {
            curScope->printSymbolTable();
        }
        else if (input == "s" || input == "step") {
            if (cur->getNodeType() == NODE::KEYWORD_NODE) {
                history.push(cur);
                KeywordNode* keyword = dynamic_cast<KeywordNode*>(cur);
                IdentifierNode* identifier = dynamic_cast<IdentifierNode*>(keyword->getLeft());
                scopeStack.push(curScopeName);
                curScopeName = identifier->getName();
                curScope = parser->getScope(curScopeName);
                if (keyword->statementsAllowed()) {
                    // we can step into scope
                    std::cout << "Stepping into scope \'" << identifier->getName() << "\'" << std::endl;
                    cur = keyword->getRight();
                }
                else {
                    std::cout << "Stepping into scope \'" << identifier->getName() << "\' that does NOT contain statements." << std::endl;
                    std::cout << "   Option 1: \'symboltable (st)\' to view symboltable of parameters held within this scope." << std::endl;
                    std::cout << "   Option 2: \'back/up\' (b/up) to return to upper scope." << std::endl;
                    cur = keyword->getLeft();
                }
                numStatementsStack.push(numStatements);
                numStatements = 1;
            }
            else {
                std::cout << "No scope to step into." << std::endl;
            }
        }
        else if (input == "n" || input == "next") {
            if (cur->getType() == Tokenizer::TYPE_END) {
                fprintf(stderr, "Last node statement. Cannot move onto next node.\n");
                continue;
            }
            else {
                numStatements++;
                history.push(cur);
                cur = cur->getNextStatement();
                cur->singlePrintNodeChildren();
            }
        }
        else if (input == "u" || input == "up") {
            if (numStatementsStack.empty() || scopeStack.empty()) {
                std::cout << "At uppermost \'global\' scope." << std::endl;
            } else {
                scopeStack.pop();
                curScopeName = scopeStack.top();
                curScope = parser->getScope(curScopeName);
                int numToPop = numStatementsStack.top();
                numStatementsStack.pop();
                for (int statement = 1; statement < numStatements; statement++) {
                    history.pop();
                }
                cur = history.top();
                cur->singlePrintNodeChildren();
            }
        }
        else if (input == "b" || input == "back") {
            if (history.empty()) {
                fprintf(stderr, "Cannot go back because there is no previous node.\n");
                continue;
            }
            else {
                numStatements--;
                cur = history.top();
                history.pop();
                cur->singlePrintNodeChildren();
            }
        }
        else if (input == "q") {
            break;
        }
        else {
            std::cout << "Invalid command. Enter one of the following commands:\n";
            std::cout << "   • print (p) - print the current node\n";
            std::cout << "   • symboltable (st) - print symboltable of current scope\n";
            std::cout << "   • step (s) - step into scope if inner scope exists\n";
            std::cout << "   • next (n) - move on to the next node + print\n";
            std::cout << "   • up (up) - exit scope and move onto next statement of upper scope\n";
            std::cout << "   • back (b) - go back to the previous node + print\n";
            std::cout << "   • quit (q) - quit debugging process\n";
            continue;
        }
    }
    std::cout << "Terminated debugging process.\n";
}


int main (int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "ERROR: Must pass in <debug_mode> as 1st argument and <file_name>.lpp as 2nd argument.\n");
        fprintf(stderr, "\t + Example: make debug mode=\"tokens\" file=\"Canvas.lpp\"\n");
        exit(1);
    }
    char* newArgV[] = {argv[1], argv[2]};
    char* input = Tokenizer::ReadFile(newArgV);
    std::string debugModeString(newArgV[0]);

    DEBUG_MODE mode;
    if (debugModeMap.count(debugModeString)) {
        mode = debugModeMap.at(debugModeString);
    } else {
        fprintf(stderr, "ERROR: Debug Mode passed in is invalid.\n");
        fprintf(stderr, "Possible debug modes:\n");
        fprintf(stderr, "\t\t tokens\n");
        fprintf(stderr, "\t\t tree\n");
        fprintf(stderr, "\t\t simulation\n");
        exit(1);
    }

    std::string inputName(newArgV[1]);
    std::cout << "INPUT NAME: " << inputName;
    size_t lastIndex = inputName.find_last_of(".lpp");
    inputName = inputName.substr(0, lastIndex - LPP_FILENAME_OFFSET);
    ErrorCollector collect;

    Debugger* debugger;

    Tokenizer* tokenizer = new Tokenizer(input, &collect);
    tokenizer->setFileSize(getFileSize(newArgV));
    std::pair<Tokenizer::Token*, Tokenizer::Token*> debugStream = tokenizer->tokenize(input);  // needs freeing
    Tokenizer::Token* head = debugStream.first;
    Tokenizer::Token* tail = debugStream.second;
    tokenizer->findIdentifiers(head);
    tokenizer->findChemicals(head);
    Tokenizer::printTokens(head, inputName);

    if (mode == DEBUG_MODE::TOKENS) {
        debugger = new Debugger(head);
        debugger->debug(tokenizer, head);
    }
    else if (mode == DEBUG_MODE::TREE) {
        Parser* parser = new Parser(head);
        ASTNode* tree = parser->parse();
        debugger = new Debugger(tree);
        debugger->debug(parser, tree);
    }
    else if (mode == DEBUG_MODE::SIMULATION) {

    }
    
    // Simulation* simulation = new Simulation("New Simulation");
    // simulation->buildSimulation(tree);
    // Writer* writer = new Writer();
    // writer->writeSimulation(simulation);
    // delete writer;
    // delete simulation;
    // delete tree;
    // delete head;
    // delete tokenizer;
    // delete[] input;
}