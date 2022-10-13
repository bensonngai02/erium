// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// The traditional approach to lexing is to use lex to generate a lexer for
// you.  Unfortunately, lex's output is ridiculously ugly and difficult to
// integrate cleanly with C++ code, especially abstract code or code meant
// as a library.  Better parser-generators exist but would add dependencies
// which most users won't already have, which we'd like to avoid.  (GNU flex
// has a C++ output option, but it's still ridiculously ugly, non-abstract,
// and not library-friendly.)
//
// The next approach that any good software engineer should look at is to
// use regular expressions.  And, indeed, I did.  I have code which
// implements this same class using regular expressions.  It's about 200
// lines shorter.  However:
// - Rather than error messages telling you "This string has an invalid
//   escape sequence at line 5, column 45", you get error messages like
//   "Parse error on line 5".  Giving more precise errors requires adding
//   a lot of code that ends up basically as complex as the hand-coded
//   version anyway.
// - The regular expression to match a string literal looks like this:
//     kString  = new RE("(\"([^\"\\\\]|"              // non-escaped
//                       "\\\\[abfnrtv?\"'\\\\0-7]|"   // normal escape
//                       "\\\\x[0-9a-fA-F])*\"|"       // hex escape
//                       "\'([^\'\\\\]|"        // Also support single-quotes.
//                       "\\\\[abfnrtv?\"'\\\\0-7]|"
//                       "\\\\x[0-9a-fA-F])*\')");
//   Verifying the correctness of this line noise is actually harder than
//   verifying the correctness of ConsumeString(), defined below.  I'm not
//   even confident that the above is correct, after staring at it for some
//   time.
// - PCRE is fast, but there's still more overhead involved than the code
//   below.
// - Sadly, regular expressions are not part of the C standard library, so
//   using them would require depending on some other library.  For the
//   open source release, this could be really annoying.  Nobody likes
//   downloading one piece of software just to find that they need to
//   download something else to make it work, and in all likelihood
//   people downloading Protocol Buffers will already be doing so just
//   to make something else work.  We could include a copy of PCRE with
//   our code, but that obligates us to keep it up-to-date and just seems
//   like a big waste just to save 200 lines of code.


#include "tokenizer.h"
#include "fileNode.h"

#define LPP_FILENAME_OFFSET 3

namespace {

    /* "Character Classes" are designed to be used in template methods. */
#define CHARACTER_CLASS(NAME, EXPRESSION)                               \
    class NAME {                                                        \
        public:                                                         \
            static inline bool InClass(char c) { return EXPRESSION; }   \
    }

/* Various escape sequences:
    \n = new line
    \t = horizontal tab
    \r = carriage return
    \v = vertical tab
    \f = formfeed page break */
CHARACTER_CLASS(Whitespace, c == ' ' || c == '\n' || c == '\t' || c == '\r' ||
                            c == '\v' || c == '\f');

CHARACTER_CLASS(WhitespaceNoNewline,
                c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f');

CHARACTER_CLASS(Unprintable, c<' ' && c> '\0');

CHARACTER_CLASS(Digit, '0' <= c && c <= '9');

CHARACTER_CLASS(Letter, ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || (c == '_'));

CHARACTER_CLASS(Alphanumeric, '0' <= c && c <= '9' || ('a' <= c && c <= 'z') ||
                               ('A' <= c && c <= 'Z') || (c == '_'));

CHARACTER_CLASS(Chemical, '0' <= c && c <= '9' || ('a' <= c && c <= 'z') ||
                               ('A' <= c && c <= 'Z') || (c == '^') || (c == '(')
                               || (c == ')') || (c == '+') || (c == '-'));

CHARACTER_CLASS(Angular, (c == '<') || (c == '>'));

CHARACTER_CLASS(Escape, c == 'a' || c == 'b' || c == 'f' || c == 'n' ||
                        c == 'r' || c == 't' || c == 'v' || c == '\\' ||
                        c == '?' || c == '\'' || c== '\"');
#undef CHARACTER_CLASS

}

/* Sets needed for various important reserved keywords in L++ */
std::unordered_set<std::string> reserved_imports {"Centrifuge", "Electrophoresis"};
std::unordered_set<std::string> reserved_keywords { "import", "container", "protocol", "reagent", "protein", "reaction",
                                                    "pathway", "membrane", "domain", "plasm"};
std::unordered_set<std::string> reserved_functions { "getReagent", "mix", "add", "clear", 
                                                    "close", "pellet", "supernatant", "remove" };
std::unordered_set<std::string> reserved_params { "ctr", "time", "spd", "vol", "temp", 
                                                "form", "voltage", "config", "eq", "krev", "kcat",
                                                "KM", "k", "Ki", "n", "Ka"};
std::unordered_set<std::string> primitives {"int", "double", "float", "bool", "string"};
std::unordered_set<std::string> looping {"for", "while", "do"};

Tokenizer::Token::Token() :
    type(TokenType::TYPE_NULL),
    text(),
    line(-1),
    column(-1),
    end_column(-1)
{

}

Tokenizer::Token::~Token() {
    delete next;
}

Tokenizer::ChemicalToken::~ChemicalToken() = default;
// ===================================================================
Tokenizer::Tokenizer(char* in, ErrorCollector* error_collect) :
    input(in),
    collect(error_collect),
    type_tbd(false),
    symbol_tbd(false),
    file_size(0),
    buffer(in),
    buffer_pos(0),
    
    line(1),
    column(0),
    
    allow_multiline_strings(false),
    whitespace(false),
    read_error(false),
    newlines(true),
    end_of_file(false)
    {
        cur.type = TYPE_START;
        cur_char = buffer[0];
    }

/* Driver program that tokenizes entire input */
std::pair<Tokenizer::Token*, Tokenizer::Token*> Tokenizer::tokenize(char* input) {
    Token* head = Next();
    Token* token = head;
    token->prev = new Tokenizer::Token;
    token->prev->type = Tokenizer::TYPE_START;
    token->prev->text = "";
    token->prev->line = 0;
    token->prev->column = 0;
    token->prev->end_column = 0;
    token->prev->next = head;
    Token* temp;
    
    while (buffer_pos < file_size) {
        token->next = Next();
        // covering edge case of comments at end 
        if (token->next->text == "") {
            break;
        }
        temp = token;
        token = token->next;
        token->prev = temp;
        // this->printState();
    }

    Token* tail = new Token();
    tail->type = TYPE_END;
    token->next = tail;
    tail->prev = token;

    std::cout << "+ Tokenization Complete. " << std::endl;
    return std::make_pair(head, tail);
}

bool Tokenizer::endOrFail() {
    return buffer_pos >= file_size;
} 

std::vector<FileNode*> Tokenizer::searchImports(FileNode* file, std::unordered_set<std::string> allFileNames) {
    bool importSeen = false;
    file->setVisited(allFileNames);
    Token* cur = file->getFileHead();
    Token* noImportStream;

    // Searching for + saving all imports / dependencies in file
    while (cur != NULL) {
        if (cur->type == Tokenizer::TYPE_IMPORT) {
            importSeen = true;
            std::string fileName = cur->text;
            std::string modifiedFileName = file->getDirectory() + fileName + ".lpp";

            std::cout << "found import: " << modifiedFileName << std::endl;
            // cannot try to import yourself
            if (modifiedFileName != file->getFileName()) {
                file->addDependency(modifiedFileName, file->getDirectory());  // adds dependency as FileNode with name = modifiedFileName
            } else {
                fail("Tried to import yourself, creating circular dependency.\n", nullptr);
            }
            std::cout << "added import:" << modifiedFileName << std::endl;
            if (cur->next != nullptr && cur->next->type != TYPE_SYMBOL_SEMICOLON) 
                fail("Semicolon not found after \'import " + fileName + "\'\n", cur);
        }
        else if (cur->text != "import" && cur->next->type != TYPE_IMPORT) {
            if (importSeen)
                noImportStream = cur->next; // get token AFTER semicolon.
            else
                noImportStream = cur;
            break;
        }
        cur = cur->next;
    }

    if (file->hasDependency()) {
        for (FileNode* dependency : file->getDependencies()) {            
            if (!dependency->isVisited(allFileNames)) {
                file->pushDependencies(searchImports(dependency, allFileNames));
            }
        }
    }
    file->setFileHead(noImportStream);
    return file->getDependencies();
}

FileNode* Tokenizer::mergeTokenStreams(FileNode* newStream, FileNode* curStream) {
    // new stream must go before curStream as curStream dependent ON newStream
    Token* tempEnd = newStream->getFileTail();
    newStream->setFileTail(newStream->getFileTail()->prev); // rid of TYPE_END
    tempEnd->prev = nullptr;
    delete tempEnd;
    newStream->getFileTail()->next = curStream->getFileHead();
    curStream->getFileHead()->prev = newStream->getFileTail();
    return newStream;
}

FileNode* Tokenizer::reformatTokens(FileNode* file) {
    for (FileNode* dependency : file->getDependencies()) {
        file = mergeTokenStreams(dependency, file);
    }
    return file;
}

FileNode* Tokenizer::linkImports(std::string fileName, std::string directory, Token* head, Token* tail) {
    std::cout << "linking imports..." << std::endl;

    FileNode* curFile = new FileNode(fileName, directory, head, tail);
    std::unordered_set<std::string> allFileNames;
    searchImports(curFile, allFileNames);
    std::cout << "all dependencies: " << curFile->getDependenciesNames() << std::endl;
    FileNode* masterFile = reformatTokens(curFile);
    printTokens(masterFile->getFileHead(), fileName);

    return masterFile;
}

void Tokenizer::findIdentifiers(Token* cur) {
    bool isIdentifier = false;
    while (cur != NULL) {
        if (cur->type == Tokenizer::TYPE_KEYWORD ||
            cur->type == Tokenizer::TYPE_PRIMITIVE ||
            cur->type == Tokenizer::TYPE_RETURN) {
            isIdentifier = true;
        }
        else if (cur->type == Tokenizer::TYPE_SYMBOL_COMMA ||
                 cur->type == Tokenizer::TYPE_SYMBOL_SEMICOLON ||
                 cur->type == Tokenizer::TYPE_SYMBOL_PAREN_OPEN ||
                 cur->type == Tokenizer::TYPE_SYMBOL_PAREN_CLOSED ||
                 cur->type == Tokenizer::TYPE_SYMBOL_CURLY_OPEN ||
                 cur->type == Tokenizer::TYPE_SYMBOL_CURLY_CLOSED) {
            // finished declaring identifier
            isIdentifier = false;
        }
        else if (isIdentifier && cur->type == Tokenizer::TYPE_IDENTIFIER) {
            identifiers.insert(cur->text);
            std::cout << "LOCATED IDENTIFIER: " << cur->text << std::endl;
        }
        cur = cur->next;
    }
}

/* sets the matching formula from the synonym registry number for chemToken from callback function in sqllite lookup */
static void setFormulaInCallback(std::string matchingFormula, Tokenizer::ChemicalToken* chemToken) {
    if (matchingFormula != "NULL") {
        std::cout << "Changed chemToken text from " << chemToken->text << " to " << matchingFormula << std::endl;
        chemToken->setFormula(matchingFormula);
    }
    else if (matchingFormula == "MISSING") {
        std::cout << "The formula synonym \'" << chemToken->text << "\' is currently not supported" \
        "by our chemical database. Please enter the compound in its chemical formula format." << std::endl;
        exit(1);
    }
    else {
        // should already be in chemical form (ie. H2O)
        std::cout << "Chemical is already in formula form: " << chemToken->text << std::endl;
        chemToken->setFormula(matchingFormula);
    }
}

/* sets the CAS registry number for chemToken from callback function in sqllite lookup */
static void setCASInCallback(std::string matchingCAS, Tokenizer::ChemicalToken* chemToken) {
    if (matchingCAS != "NULL") {
        std::cout << "Changed chemToken CAS from " << chemToken->cas << " to " << matchingCAS << std::endl;
        chemToken->setCAS(matchingCAS);
        
    }
    else if (matchingCAS == "MISSING") {
        std::cout << "The formula synonym \'" << chemToken->text << "\' is currently not supported" \
        "by our chemical database. Please enter the compound in its chemical formula format." << std::endl;
        exit(1);
    }
    else {
        // should already be in chemical form (ie. H2O)
        std::cout << "Chemical is already in formula form: " << chemToken->text << std::endl;
    }
}

static int callback(void *again, int count, char **data, char **columns) {
    Tokenizer::Token* againToken = (Tokenizer::Token*) again;
    Tokenizer::ChemicalToken* chemToken = new Tokenizer::ChemicalToken(againToken);
    
    std::cout << "CALLBACK SANITY CHECK" << std::endl;
    std::cout << "matchingFormula check: " << data[0] << std::endl;

    std::string matchingFormula = data[0] ? data[0] : "NULL";
    
    std::cout << "\nmatching formula: " << matchingFormula << std::endl;
    std::cout << **data << std::endl;

    setFormulaInCallback(matchingFormula, chemToken);
    std::string matchingCAS = data[1] ? data[1] : "NULL";
    setCASInCallback(matchingCAS, chemToken);


    /* replaces the basic token in the tokenstream with a special chemical token containing
       formula and CAS member variables */
    Tokenizer::Token* tempNext = againToken->next;
    againToken->prev->next = chemToken;
    chemToken->next = tempNext;

    return 0;
}


void Tokenizer::findChemicals(Token* root) {
    Token* cur = root;
    bool inParam = false;
    while (cur != NULL) {
        // if the marked identifier is NOT ACTUALLY an identifier,
        // it should be a chemical
        if ((cur->text == "reaction" || cur->text == "reagent") && 
            (cur->next->next->type == Tokenizer::TYPE_SYMBOL_PAREN_OPEN ||
             cur->next->next->type == Tokenizer::TYPE_SYMBOL_CURLY_OPEN)) {
            inParam = true;
        }
        else if (cur->type == Tokenizer::TYPE_SYMBOL_PAREN_CLOSED ||
                 cur->type == Tokenizer::TYPE_SYMBOL_CURLY_CLOSED) {
            inParam = false;
        }
        if (cur->type == Tokenizer::TYPE_IDENTIFIER &&
            identifiers.count(cur->text) == 0 &&
            inParam) {
                cur->type = Tokenizer::TYPE_CHEMICAL;
                std::transform(cur->text.begin(), cur->text.end(), cur->text.begin(), ::toupper);

        }
        cur = cur->next;
    }

    /* Second parse through tokens to find chemical synonyms and replace with matching formula */
    Token* again = root;
    sqlite3* chemicalDB;
    int rc = 0;
    char* error = 0;
    
    rc = sqlite3_open("chemBIChemicalsCASSetUpper.db", &chemicalDB);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Could not open chemBIChemicalsCASSetUpper.db database file.\n");
        exit(1);
    }
    else {
        std::cout << "chemBIChemicalsCASSetUpper.db opened successfully!" << std::endl;
    }
    while (again != NULL) {
        if (Tokenizer::IsChemical(again)) {
            // insert code for database
            std::string synonym = again->text;
            std::string lookup = "SELECT Formula, CAS from chemBIChemicalsCASSetUpper WHERE Name=\"" + synonym + "\"";
            int foundCAS = sqlite3_exec(chemicalDB, lookup.c_str(), callback, (void*) ((ChemicalToken*) again), &error);
        }
        again = again->next;
    }

    std::cout << "Finished replacing chemical synonyms" << std::endl;
    sqlite3_close(chemicalDB);
}


bool Tokenizer::reportWhitespace() const { return whitespace; }

void Tokenizer::setReportWhitespace(bool report) {
    whitespace = report;
    newlines &= report;
}

bool Tokenizer::reportNewLines() const { return newlines; }

void Tokenizer::setReportNewLines(bool report) {
    newlines = report;
    whitespace |= report;
}

const Tokenizer::Token& Tokenizer::current() {
    return cur;
}

const Tokenizer::Token& Tokenizer::previous() {
    return prev;
}

// -------------------------------------------------------------------
/* Performs parsing for the next tokenizable string (word, digit, escape, symbol, etc.) */
Tokenizer::Token* Tokenizer::Next() {
  Token* new_token = new Token;
  prev = cur;
  type_tbd = false;
  symbol_tbd = false;

  while (buffer_pos < file_size) {
    StartToken();
    TryConsumeWhitespace() || TryConsumeNewline();
    EndToken();

    /* Handles comment-parsing */
    switch (TryConsumeCommentStart()) {
      case LINE_COMMENT:
        ConsumeLineComment(NULL);
        continue;
      case BLOCK_COMMENT:
        ConsumeBlockComment(NULL);
        continue;
      case SLASH_NOT_COMMENT:
        break;
      case NO_COMMENT:
        break;
    }

    if (LookingAt<Unprintable>()) {
        std::stringstream s;
        
        s << std::hex << "0x" << int(cur_char);
        if (int(cur_char) == 10) {
            // just a newline (ASCII 10 == newline), no error encountered.
        } else {
            AddError("Invalid control character " + s.str() + " encountered in text at line " + std::to_string(line) + " col " + std::to_string(column) + ".");
        }
        NextChar();
        // Skip more unprintable characters, too.  But, remember that '\0' is
        // also what current_char_ is set to after EOF / read error.  We have
        // to be careful not to go into an infinite loop of trying to consume
        // it, so make sure to check read_error_ explicitly before consuming
        // '\0'.
        while (TryConsumeOne<Unprintable>() ||
                (!endOrFail() && TryConsume('\0'))) {
        // Ignore.
        }

    } else {
        // Reading some sort of token.
        std::cout << "+ Recording Token..." << std::endl;
        StartToken();

        if (TryConsumeOne<Letter>()) {
            std::cout << "+ 1" << std::endl;
            /* Could be any of the following:
                1.  Reserved Keyword (protocol, reagent, container)
                2.  Reserved Parameter  (vol, temp, time, form)
                3.  Reserved Function (add(), close(), mix())
                4.  Identifier - custom identifiers
                5.  Primitives
                6.  Looping
                7.  Return
                8.  Imports
                9.  If-Else
            */
            std::cout << "Consuming Letter(s)" << std::endl;
            ConsumeZeroOrMore<Alphanumeric>();
            type_tbd = true;
        } else if (TryConsume('.')) {
            std::cout << "+ 2" << std::endl;
            // This could be the beginning of a floating-point number, or it could
            // just be a '.' symbol.
            if (TryConsumeOne<Digit>()) {
                // It's a floating-point number.
                if (prev.type == TYPE_IDENTIFIER &&
                    cur.line == prev.line &&
                    cur.column == prev.end_column) {
                    // We don't accept syntax like "blah.123".
                    collect->AddError(line, column - 2,
                    "Need space between identifier and decimal point.");
                }
                cur.type = ConsumeNumber(true);
            } else {
                std::cout << "Consuming Floating Point" << std::endl;
                // not a number. treated like symbol (function calling on objects)
                cur.type = TYPE_SYMBOL_DOT;
            }
        } else if (TryConsumeOne<Digit>()) {
            std::cout << "+ 3" << std::endl;
            std::cout << "Consuming Number" << std::endl;
            cur.type = ConsumeNumber(false);
        }
          else if (TryConsume('\"')) {
            std::cout << "+ 4" << std::endl;
            ConsumeString('\"');
            cur.type = TYPE_STRING;
        } else if (TryConsume('\'')) {
            std::cout << "+ 5" << std::endl;
            ConsumeString('\'');
            cur.type = TYPE_STRING;
        } else {
            std::cout << "+ 6" << std::endl;
            NextChar();
            symbol_tbd = true;
        }
        EndToken(); 

        std::cout << "+ Finished Recording! Token recorded: " << current().text << std::endl;

        SetAlphanumericType();
        SetSymbolType();
        type_tbd = false;
        symbol_tbd = false;
        *new_token = cur;
        return new_token;
    }
  }
    
  new_token->type = TYPE_END;
  new_token->text.clear();
  new_token->line = line;
  new_token->column = column;
  new_token->end_column = column;
  
  return new_token;
}

// -------------------------------------------------------------------
// Internal helpers.
void Tokenizer::NextChar() {
    if (buffer_pos < file_size - 1) {
        /* Updates line + column counters based on character
            being consumed */
        if (cur_char == '\n') {
            ++line;
            column = 0;
        } else if (cur_char == '\t') {
            column += kTabWidth - column % kTabWidth;
        } else {
            ++column;
        }
        buffer_pos++;

        /* Advance to the next character */
        cur_char = buffer[buffer_pos];
    } else {
        // force tokenizer to stop tokenizing
        cur_char = '\0';
        buffer_pos++;
    }
}

void ErrorCollector::AddError(int line, ColumnNumber column, const std::string& message) {
    std::cout << message << " at <" << line << ", " << column << ">\n";
    // error(message);
}

void Tokenizer::AddError(const std::string& message) {
    collect->AddError(line, column, message);
    // error(message);
}

inline void Tokenizer::RecordTo(std::string* target) {
    record_target = target;
    record_start = buffer_pos;
}

inline void Tokenizer::StopRecording() {
    if (buffer_pos != record_start) {
        record_target->append(buffer + record_start,
                              buffer_pos - record_start);
    }
    record_target = NULL;
    record_start = -1;
}

inline void Tokenizer::StartToken() {
    cur.type = TYPE_START;
    cur.text.clear();
    cur.line = line;
    cur.column = column;
    RecordTo(&cur.text);
}

inline void Tokenizer::EndToken() {
    StopRecording();
    cur.end_column = column;
}

/* Helper Methods that consume characters */
template <typename CharacterClass>
inline bool Tokenizer::LookingAt() {
    return CharacterClass::InClass(cur_char);
}

template <typename CharacterClass>
inline bool Tokenizer::TryConsumeOne() {
    if (CharacterClass::InClass(cur_char)) {
        NextChar();
        return true;
    } else {
        return false;
    }
}

inline bool Tokenizer::TryConsume(char c) {
    if (cur_char == c) {
        NextChar();
        return true;
    } else {
        return false;
    }
}


inline bool Tokenizer::TryConsume(std::string s) {
    for (int c = 0; c < s.size(); c++) {
        if (cur_char != s[c]) {
            return false;
        }
        NextChar();
    }
    return true;
}

template <typename CharacterClass>
inline void Tokenizer::ConsumeZeroOrMore() {
    while(CharacterClass::InClass(cur_char) && !end_of_file) {
        std::cout << "ConsumeZeroOrMore(): cur_char is " << cur_char << std::endl;
        NextChar();
    }
}

template<typename CharacterClass>
inline void Tokenizer::ConsumeOneOrMore(const char* error) {
    if (!CharacterClass::InClass(cur_char)) {
        AddError(error);
    } else {
        do {
            NextChar();
        } while (CharacterClass::InClass(cur_char));
    }
}

void Tokenizer::SetSymbolType() {
    std::string unknown = cur.text;
    std::string type;

    if (symbol_tbd) {
        if (IsAdd(unknown)) {
            cur.type = TYPE_SYMBOL_ADD;
            type = "ADD";
        }
        else if (IsSubtract(unknown)) {
            cur.type = TYPE_SYMBOL_SUBTRACT;
            type = "SUBTRACT";
        }
        else if (IsMultiply(unknown)) {
            cur.type = TYPE_SYMBOL_MULTIPLY;
            type = "MULTIPLY";
        }
        else if (IsDivide(unknown)) {
            cur.type = TYPE_SYMBOL_DIVIDE;
            type = "DIVIDE";
        }
        else if (IsEqual(unknown)) {
            cur.type = TYPE_SYMBOL_EQUAL;
            type = "EQUAL";
        }
        else if (IsNot(unknown)) {
            cur.type = TYPE_SYMBOL_NOT;
            type = "NOT";
        }
        else if (IsComma(unknown)) {
            cur.type = TYPE_SYMBOL_COMMA;
            type = "COMMA";
        }
        else if (IsDot(unknown)) {
            cur.type = TYPE_SYMBOL_DOT;
            type = "DOT";
        }
        else if (IsGEQ(unknown)) {
            cur.type = TYPE_SYMBOL_GEQ;
            type = "GEQ";
        }
        else if (IsLEQ(unknown)) {
            cur.type = TYPE_SYMBOL_LEQ;
            type = "LEQ";
        }
        else if (IsGT(unknown)) {
            cur.type = TYPE_SYMBOL_GT;
            type = "GT";
        }
        else if (IsLT(unknown)) {
            cur.type = TYPE_SYMBOL_LT;
            type = "LT";
        }
        else if (IsQuoteDouble(unknown)) {
            cur.type = TYPE_SYMBOL_QUOTE_DOUBLE;
            type = "QUOTE_DOUBLE";
        }
        else if (IsQuoteSingle(unknown)) {
            cur.type = TYPE_SYMBOL_QUOTE_SINGLE;
            type = "QUOTE_SINGLE";
        }
        else if (IsQuestion(unknown)) {
            cur.type = TYPE_SYMBOL_QUESTION;
            type = "QUESTION";
        }
        else if (IsPercent(unknown)) {
            cur.type = TYPE_SYMBOL_PERCENT;
            type = "PERCENT";
        }
        else if (IsCarat(unknown)) {
            cur.type = TYPE_SYMBOL_CARAT;
            type = "CARAT";
        }
        else if (IsOr(unknown)) {
            cur.type = TYPE_SYMBOL_OR;
            type = "OR";
        }
        else if (IsAnd(unknown)) {
            cur.type = TYPE_SYMBOL_AND;
            type = "AND";
        }
        else if (IsUnderscore(unknown)) {
            cur.type = TYPE_SYMBOL_UNDERSCORE;
            type = "UNDERSCORE";
        }
        else if (IsColon(unknown)) {
            cur.type = TYPE_SYMBOL_COLON;
            type = "COLON";
        }
        else if (IsSemicolon(unknown)) {
            cur.type = TYPE_SYMBOL_SEMICOLON;
            type = "SEMICOLON";
        }
        else if (IsParenOpen(unknown)) {
            cur.type = TYPE_SYMBOL_PAREN_OPEN;
            type = "PAREN_OPEN";
        }
        else if (IsParenClosed(unknown)) {
            cur.type = TYPE_SYMBOL_PAREN_CLOSED;
            type = "PAREN_CLOSED";
        }
        else if (IsCurlyOpen(unknown)) {
            cur.type = TYPE_SYMBOL_CURLY_OPEN;
            type = "CURLY_OPEN";
        }
        else if (IsCurlyClosed(unknown)) {
            cur.type = TYPE_SYMBOL_CURLY_CLOSED;
            type = "CURLY_CLOSED";
        }
        else if (IsBracketOpen(unknown)) {
            cur.type = TYPE_SYMBOL_BRACKET_OPEN;
            type = "BRACKET_OPEN";
        }
        else if (IsBracketClosed(unknown)) {
            cur.type = TYPE_SYMBOL_BRACKET_CLOSED;
            type = "BRACKET_CLOSED";
        }
        else {
            cur.type = TYPE_SYMBOL_UNKNOWN;
            type = "SYMBOL_UNKNOWN";
        } 
    }
    
    std::cout << "symbol type is: " << type << " for" << unknown << std::endl;
}

void Tokenizer::SetAlphanumericType() {
    // if (cur.text == "Canvas") {
    //     std::cout << translateTokenType(cur.type) << std::endl;
    //     std::cout << (foundImport ? "import found" : "import not found") << std::endl;
    //     std::cout << "isImport: " << IsImport(cur.text, foundImport) << std::endl;
    //     exit(1);
    // }
    if (type_tbd) {
        std::string unknown = cur.text;
        std::string type;

        if (IsKeyword(unknown)) {
            cur.type = TYPE_KEYWORD;
            type = "KEYWORD";
            if (cur.text == "import") {
                // set found import state variable
                foundImport = true;
            }
        }
        else if (IsParam(unknown)) {
            cur.type = TYPE_PARAM;
            type = "PARAM";
        }
        else if (IsFunction(unknown)) {
            cur.type = TYPE_FUNCTION;
            type = "FUNCTION";
        }
        else if (IsUnit(unknown)) {
            cur.type = TYPE_UNIT;
            type = "UNIT";
        }
        else if (IsPrimitive(unknown)) {
            cur.type = TYPE_PRIMITIVE;
            type = "PRIMITIVE";
        }
        else if (IsLooping(unknown)) {
            cur.type = TYPE_LOOPING;
            type = "LOOPING";
        }
        else if (IsReturn(unknown)) {
            cur.type = TYPE_RETURN;
            type = "RETURN";
        }
        else if (IsImport(unknown, foundImport)) {
            cur.type = TYPE_IMPORT;
            type = "IMPORT";
            // reset found import
            foundImport = false;
        }
        else if (IsIf(unknown)) {
            cur.type = TYPE_IF;
            type = "IF";
        }
        else if (IsElse(unknown)) {
            cur.type = TYPE_ELSE;
            type = "ELSE";
        }
        else {
            cur.type = TYPE_IDENTIFIER;
            type = "IDENTIFIER";
        } 
        std::cout << "Alphanumeric type for \'" << unknown << "\': " << type << std::endl;
    }
}

bool Tokenizer::IsKeyword(std::string word) {
    return reserved_keywords.count(word) != 0;
}

bool Tokenizer::IsParam(std::string word) {
    return reserved_params.count(word) != 0;
}

bool Tokenizer::IsFunction(std::string word) {
    return reserved_functions.count(word) != 0;
}

bool Tokenizer::IsUnit(std::string word) {
    std::string prefixRegex = "(Y|Z|E|P|T|G|M|k|h|da|d|c|m|u|n|p|f|a|z|y){0,1}";
    std::string unitRegex = "(L|s|min|h|g|C|F|K|V|A|mol|M|m|cd|G|rpm){1}";
    return std::regex_match(word, std::regex(prefixRegex + unitRegex));
}

bool Tokenizer::IsPrimitive(std::string word) {
    return primitives.count(word) != 0;
}

bool Tokenizer::IsLooping(std::string word) {
    return looping.count(word) != 0;
}

bool Tokenizer::IsReturn(std::string word) {
    // if word == return, .compare() returns 0. Invert.
    return !word.compare("return");
}

bool Tokenizer::IsImport(std::string word, bool foundImport) {
    return foundImport;
    // return reserved_imports.count(word) != 0;
}

bool Tokenizer::IsIf(std::string word) {
    return !word.compare("if");
}

bool Tokenizer::IsElse(std::string word) {
    return !word.compare("else");
}

bool Tokenizer::IsAdd(std::string word) {
    return word == "+";
}

bool Tokenizer::IsSubtract(std::string(word)) {
    return word == "-";
}

bool Tokenizer::IsMultiply(std::string(word)) {
    return word == "*";
}

bool Tokenizer::IsDivide(std::string(word)) {
    return word == "/";
}

bool Tokenizer::IsEqual(std::string(word)) {
    return word == "=";
}

bool Tokenizer::IsNot(std::string(word)) {
    return word == "!";
}

bool Tokenizer::IsComma(std::string word) {
    return word == ",";
}

bool Tokenizer::IsDot(std::string(word)) {
    return word == ".";
}

bool Tokenizer::IsGEQ(std::string(word)) {
    return word == ">=";
}

bool Tokenizer::IsLEQ(std::string(word)) {
    return word == "<=";
}

bool Tokenizer::IsGT(std::string(word)) {
    return word == ">";
}

bool Tokenizer::IsLT(std::string(word)) {
    return word == "<";
}

bool Tokenizer::IsQuoteDouble(std::string(word)) {
    return word == "\"";
}
bool Tokenizer::IsQuoteSingle(std::string(word)) {
    return word == "\'";
}
bool Tokenizer::IsQuestion(std::string(word)) {
    return word == "?";
}

bool Tokenizer::IsPercent(std::string word) {
    return word == "%";
}

bool Tokenizer::IsCarat(std::string(word)) {
    return word == "^";
}

bool Tokenizer::IsOr(std::string(word)) {
    return word == "|";
}
bool Tokenizer::IsAnd(std::string(word)) {
    return word == "&";
}
bool Tokenizer::IsUnderscore(std::string(word)) {
    return word == "_";
}

bool Tokenizer::IsColon(std::string(word)) {
    return word == ":";
}
bool Tokenizer::IsSemicolon(std::string(word)) {
    return word == ";";
}
bool Tokenizer::IsParenOpen(std::string(word)) {
    return word == "(";
}

bool Tokenizer::IsParenClosed(std::string word) {
    return word == ")";
}

bool Tokenizer::IsCurlyOpen(std::string(word)) {
    return word == "{";
}

bool Tokenizer::IsCurlyClosed(std::string(word)) {
    return word == "}";
}
bool Tokenizer::IsBracketOpen(std::string(word)) {
    return word == "[";
}
bool Tokenizer::IsBracketClosed(std::string(word)) {
    return word == "]";
}
bool Tokenizer::IsChemical(Token* token) {
    return token->type == Tokenizer::TYPE_CHEMICAL || 
           (token->type == Tokenizer::TYPE_INTEGER && 
            token->next->type == Tokenizer::TYPE_CHEMICAL);
}



// -------------------------------------------------------------------
// Methods that read whole strings & numbers
void Tokenizer::ConsumeString(char delimiter) {
  while (true) {
    switch (cur_char) {
      case '\0':
        AddError("Unexpected end of string.");
        return;

      case '\n': {
        if (!allow_multiline_strings) {
          AddError("String literals cannot cross line boundaries.");
          return;
        }
        NextChar();
        break;
      }

      case '\\': {
        // An escape sequence.
        NextChar();
        if (TryConsumeOne<Escape>()) {
          // Valid escape sequence.
        } else {
          AddError("Invalid escape sequence in string literal.");
        }
        break;
      }
      default: {
        // used to scan through punctuation
        if (cur_char == delimiter) {
          NextChar();
          return;
        }
        NextChar();
        break;
      }
    }
  }
}

Tokenizer::TokenType Tokenizer::ConsumeNumber(bool started_with_dot) {
    bool is_float = false;

    // A decimal number.
    if (started_with_dot) {
        /* Supports numbers like ".5" */ 
        is_float = true;
        ConsumeZeroOrMore<Digit>();
    } else {
        /* Supports numbers w/ leading zeroes like "0.5" */
        ConsumeZeroOrMore<Digit>();
        if (TryConsume('.')) {
            is_float = true;
            ConsumeZeroOrMore<Digit>();
        }
    }

    /* Support for numbers in scientific notation */
    if (TryConsume('e') || TryConsume('E')) {
        is_float = true;
        TryConsume('-') || TryConsume('+');
        ConsumeOneOrMore<Digit>("\"e\" must be followed by exponent.");
    }
    

    if (LookingAt<Letter>() && require_space_after_num) {
        AddError("Need space between number and identifier.");
    } else if (cur_char == '.') {
        if (is_float) {
            AddError(
                "Already saw decimal point or exponent; can't have another one.");
        }
    }

    return is_float ? TYPE_FLOAT : TYPE_INTEGER;
}

// -------------------------------------------------------------------
// Methods that read comments

void Tokenizer::ConsumeLineComment(std::string* content) {
    /* if not yet at end of file */
    if (content != NULL) 
        RecordTo(content);

    /* consumes entire line comment up until end of line */
    while (cur_char != '\0' && cur_char != '\n') {
        NextChar();
    }
    TryConsume('\n');

    /* if not yet at end of file*/
    if (content != NULL) 
        StopRecording();
}

void Tokenizer::ConsumeBlockComment(std::string* content) {
  int start_line = line;
  int start_column = column - 2;    // revert back before "/*"

  if (content != NULL) 
    RecordTo(content);

  while (true) {
    while (cur_char != '\0' && cur_char != '*' &&
           cur_char != '/' && cur_char != '\n') {
        NextChar();
    }

    if (TryConsume('\n')) {
      if (content != NULL) StopRecording();

      // Consume leading whitespace and asterisk;
      ConsumeZeroOrMore<WhitespaceNoNewline>();
      if (TryConsume('*')) {
        if (TryConsume('/')) {
          // End of comment block
          break;
        }
      }

      if (content != NULL) 
        RecordTo(content);
    } else if (TryConsume('*') && TryConsume('/')) {
        // End of comment.
        if (content != NULL) {
            StopRecording();
            // Strip trailing "*/".
            content->erase(content->size() - 2);
        }
        break;
    } else if (TryConsume('/') && cur_char == '*') {
        // Note:  We didn't consume the '*' because if there is a '/' after it
        //   we want to interpret that as the end of the comment.
        AddError("\"/*\" inside block comment.  Block comments cannot be nested.");
    } else if (cur_char == '\0') {
        AddError("End-of-file inside block comment.");
        collect->AddError(start_line, start_column,"  Comment started here.");
        if (content != NULL) 
            StopRecording();
        break;
    }
  }
}

Tokenizer::NextCommentStatus Tokenizer::TryConsumeCommentStart() {
  if (TryConsume('/')) {
    if (TryConsume('/')) {
        return LINE_COMMENT;
    } else if (TryConsume('*')) {
        return BLOCK_COMMENT;
    } else {
        // Oops, it was just a slash.  Return it.
        cur.type = TYPE_SYMBOL_DIVIDE;
        cur.text = "/";
        cur.line = line;
        cur.column = column - 1;
        cur.end_column = column;
        return SLASH_NOT_COMMENT;
    }
  } else
    return NO_COMMENT;
}


bool Tokenizer::TryConsumeWhitespace() {
    if (newlines) {
        if (TryConsumeOne<WhitespaceNoNewline>()) {
            ConsumeZeroOrMore<WhitespaceNoNewline>();
            cur.type = TYPE_WHITESPACE;
            return true;
        }
        return false;
    }
    if (TryConsumeOne<Whitespace>()) {
        ConsumeZeroOrMore<Whitespace>();
        cur.type = TYPE_WHITESPACE;
        return whitespace;
    }
    return false;
}

bool Tokenizer::TryConsumeNewline() {
  if (!whitespace || !newlines) {
    return false;
  }
  if (TryConsume('\n')) {
    cur.type = TYPE_NEWLINE;
    return true;
  }
  return false;
}


template <typename CharacterClass>
static bool AllInClass(const std::string& s) {
  for (const char character : s) {
    if (!CharacterClass::InClass(character)) return false;
  }
  return true;
}

bool Tokenizer::IsIdentifier(const std::string& text) {
  // Mirrors IDENTIFIER definition in Tokenizer::Next() above.
  if (text.size() == 0) return false;
  if (!Letter::InClass(text.at(0))) return false;
  if (!AllInClass<Alphanumeric>(text.substr(1))) return false;
  return true;
}

// DEBUG ====================================================
void Tokenizer::printTokens(Token* head, std::string input) {
    std::cout << "printing tokens for " << input << ".tokens\n";
    std::ofstream out(input + ".tokens");

    if (head == NULL) {
        std::cout << "error printing tokens.";
        exit(1);
    }
    while (head != NULL) {
        // Uses virtual functions to choose base or derived print
        head->print(out);
        
        head = head->next;
    }
    std::cout << "+ Tokens successfully printed!" << std::endl;
}

void Tokenizer::printTokenInfo(Token * t) {
    std::cout << "{";
    std::cout << printTokenType(t);
    std::cout << ",\'" << t->text << "\',";
    std::cout << t->line << ",";
    std::cout << t->column <<  "}" << std::endl;
}

std::string Tokenizer::translateTokenType(Tokenizer::TokenType type) {
    std::string output;
    switch (type) {
        case Tokenizer::TYPE_START: 
            output = "START";
            break;
        case Tokenizer::TYPE_END:
            output = "END";
            break;
        case Tokenizer::TYPE_CHEMICAL:
            output = "CHEMICAL";
            break;
        case Tokenizer::TYPE_FLOAT:
            output = "FLOAT";
            break;
        case Tokenizer::TYPE_FUNCTION:
            output = "FUNCTION";
            break;
        case Tokenizer::TYPE_IDENTIFIER:
            output = "IDENTIFIER";
            break;
        case Tokenizer::TYPE_INTEGER:
            output = "INTEGER";
            break;
        case Tokenizer::TYPE_KEYWORD:
            output = "KEYWORD";
            break;
        case Tokenizer::TYPE_PARAM:
            output = "PARAM";
            break;
        case Tokenizer::TYPE_STRING:
            output = "STRING";
            break;
        case Tokenizer::TYPE_SYMBOL_ADD:
            output = "ADD";
            break;
        case Tokenizer::TYPE_SYMBOL_SUBTRACT:
            output = "SUBTRACT";
            break;
        case Tokenizer::TYPE_SYMBOL_MULTIPLY:
            output = "MULTIPLY";
            break;
        case Tokenizer::TYPE_SYMBOL_DIVIDE:
            output = "DIVIDE";
            break;
        case Tokenizer::TYPE_SYMBOL_EQUAL:
            output = "EQUAL";
            break;
        case Tokenizer::TYPE_SYMBOL_COMMA:
            output = "COMMA";
            break;
        case Tokenizer::TYPE_SYMBOL_DOT: 
            output = "DOT";
            break;
        case Tokenizer::TYPE_SYMBOL_GEQ:
            output = "GEQ";
            break;
        case Tokenizer::TYPE_SYMBOL_LEQ:
            output = "LEQ";
            break;
        case Tokenizer::TYPE_SYMBOL_GT:
            output = "GT";
            break;
        case Tokenizer::TYPE_SYMBOL_LT:
            output = "LT";
            break;
        case Tokenizer::TYPE_SYMBOL_QUOTE_DOUBLE:
            output = "QUOTE_DOUBLE";
            break;
        case Tokenizer::TYPE_SYMBOL_QUOTE_SINGLE:
            output = "QUOTE_SINGLE";
            break;
        case Tokenizer::TYPE_SYMBOL_QUESTION:
            output = "QUESTION";
            break;
        case Tokenizer::TYPE_SYMBOL_PERCENT:
            output = "PERCENT";
            break;
        case Tokenizer::TYPE_SYMBOL_CARAT:
            output = "CARAT";
            break;
        case Tokenizer::TYPE_SYMBOL_OR:
            output = "OR";
            break;
        case Tokenizer::TYPE_SYMBOL_AND:
            output = "AND";
            break;
        case Tokenizer::TYPE_SYMBOL_UNDERSCORE:
            output = "UNDERSCORE";
            break;
        case Tokenizer::TYPE_SYMBOL_COLON:
            output = "COLON";
            break;
        case Tokenizer::TYPE_SYMBOL_SEMICOLON:
            output = "SEMICOLON";
            break;
        case Tokenizer::TYPE_SYMBOL_PAREN_OPEN:
            output = "PAREN_OPEN";
            break;
        case Tokenizer::TYPE_SYMBOL_PAREN_CLOSED:
            output = "PAREN_CLOSED";
            break;
        case Tokenizer::TYPE_SYMBOL_CURLY_OPEN:
            output = "CURLY_OPEN";
            break;
        case Tokenizer::TYPE_SYMBOL_CURLY_CLOSED:
            output = "CURLY_CLOSED";
            break;
        case Tokenizer::TYPE_SYMBOL_BRACKET_OPEN:
            output = "BRACKET_OPEN";
            break;
        case Tokenizer::TYPE_SYMBOL_BRACKET_CLOSED:
            output = "BRACKET_CLOSED";
            break;
        case Tokenizer::TYPE_SYMBOL_UNKNOWN:
            output = "SYMBOL_UNKNOWN";
            break;
        case Tokenizer::TYPE_UNIT:
            output = "UNIT";
            break;
        case Tokenizer::TYPE_PRIMITIVE:
            output = "PRIMITVE";
            break;
        case Tokenizer::TYPE_LOOPING:
            output = "LOOPING";
            break;
        case Tokenizer::TYPE_RETURN:
            output = "RETURN";
            break;
        case Tokenizer::TYPE_WHITESPACE:
            output = "WHITESPACE";
            break;
        case Tokenizer::TYPE_NEWLINE:
            output = "NEWLINE";
            break;
        case Tokenizer::TYPE_IMPORT:
            output = "IMPORT";
            break;
        case Tokenizer::TYPE_IF:
            output = "IF";
            break;
        case Tokenizer::TYPE_ELSE:
            output = "ELSE";
            break;
        default:
            output = "default";
    }
    return output;
}

std::string Tokenizer::printTokenType(Token* t) {
    std::string output;
    output = translateTokenType(t->type);

    if (output == "default") {
        /* Token verification. Should not have "default" type 0 must belong 
            to one of other class types. */
        fprintf(stderr, 
                "ERROR: Default token found. Syntax for the following text is not recognized: \"%s\"\n", 
                t->text.c_str());
        exit(1);
    }

    return output;
}

void printBuffer(const char* buffer, int file_size) {
    for (int i = 0; i < file_size; i++) {
        std::cout << buffer[i];
    }
    std::cout << std::endl;
}

void Tokenizer::printState() {
    std::cout << "type_tbd: " << (this->type_tbd ? "true" : "false") << std::endl;
    std::cout << "cur_char: " << cur_char << std::endl;
    // std::cout << "buffer: " << std::endl;
    // printBuffer(buffer, file_size);
    std::cout << "buffer_pos: " << buffer_pos << std::endl;
    std::cout << "file_size: " << file_size << std::endl;
    std::cout << "read_error: " << (this->read_error ? "true" : "false") << std::endl;
    std::cout << "end_of_file: " << (this->end_of_file ? "true" : "false") << std::endl;

    std::cout << "line: " << line << std::endl;
    std::cout << "column: " << column << std::endl;

    std::cout << "record_target: " << record_target << std::endl;
    std::cout << "record_start: " << record_start << std::endl;
    std::cout << std::endl;
}