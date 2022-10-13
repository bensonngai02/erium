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
// Class for parsing tokenized text from a ZeroCopyInputStream.

#pragma once

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <regex>
#include <filesystem>
#ifdef __APPLE__
   #include <sqlite3.h>
#endif
#ifdef __linux__
    #include <sqlite3.h>
#endif
#ifdef _WIN32
    #include "sqlite3/sqlite3.h"
#endif

#include "error.h"

/* Interface defined in 'zero_copy_stream.(h/cxx)', implementation(s) located
   in 'zero_copy_stream_impl.(h/cxx) */
// class ZeroCopyInputStream;

// Defined in this file.
class ErrorCollector;
class Tokenizer;
class FileNode;

// By "column number", the proto compiler refers to a count of the number
// of bytes before a given byte, except that a tab character advances to
// the next multiple of 8 bytes.  Note in particular that column numbers
// are zero-based, while many user interfaces use one-based column numbers.
typedef int ColumnNumber;

/* ErrorCollector is modified from the Google Protobuf API. It is directly
   implemented as a class rather than an abstract interface. */
class ErrorCollector {
public:
    inline ErrorCollector() {}

    /* Indicates that there was an error in the input at the given line and
        column numbers. The numbers are zero-based, so you may want to add 1 to
        each before printing them. */
    void AddError(int line, ColumnNumber column, const std::string& message);

    /* Indicates that there was a warning in the input at the given line and
       column numbers. The numbers are zero-based, so you may want to add 1 to each
       before printing them. */
    void AddWarning(int line, ColumnNumber column, const std::string& message);
};


class Tokenizer {
  public:
   /* Note: For now, let's assume we'll just read the input file by using
      a standard C++ buffer that reads in a text file */
   Tokenizer(char* input, ErrorCollector* collect);
   Tokenizer(std::string input, ErrorCollector* collect);

   enum TokenType {
      TYPE_START,        /* Next() has not yet been called. */

      TYPE_END,          /* End of input reached.  "text" is empty */

      TYPE_IDENTIFIER,   /* A sequence of letters, digits, and underscores, not
                        starting with a digit.  It is an error for a number
                        to be followed by an identifier with no space in
                        between. */

      TYPE_CHEMICAL,     /* A sequence of letters, digits, {}, and _ to indicate
                        a chemical formula. An example is "5H_{2}O", meaning
                        five water molecules */

      TYPE_KEYWORD,      /* A sequence of strictly letters. Only words part of
                           'reserved_keywords' are considered this type. */

      TYPE_FUNCTION,     /* A sequence of strictly letters. Only words part of
                           'reserved_functions' are considered this type. */

      TYPE_PARAM,        /* A sequence of strictly letters. Only words part of
                        'reserved_params' are considered this type. */

      TYPE_IMPORT,       /* A sequence of strictly letters. Only words part of
                           'reserved_imports' are considered this type. */

      TYPE_UNIT,         /* A sequence of strictly letters. Only supported units
                           part of 'units' are considered this type. */

      TYPE_INTEGER,      /* A sequence of digits representing an integer. */

      TYPE_FLOAT,        /* A floating point literal, with a fractional part and/or
                        an exponent.  Always in decimal.  Again, never
                        negative. */

      TYPE_STRING,       /* A quoted sequence of escaped characters.  Either single
                        or double quotes can be used, but they must match.
                        A string literal cannot cross a line break. */

      TYPE_SYMBOL_ADD,                // +
      TYPE_SYMBOL_SUBTRACT,           // -
      TYPE_SYMBOL_MULTIPLY,           // *
      TYPE_SYMBOL_DIVIDE,             // /
      TYPE_SYMBOL_EQUAL,              // =
      TYPE_SYMBOL_NOT,                // !
      TYPE_SYMBOL_COMMA,              // ,
      TYPE_SYMBOL_DOT,                // .
      TYPE_SYMBOL_GEQ,                // >=
      TYPE_SYMBOL_LEQ,                // <=
      TYPE_SYMBOL_GT,                 // >
      TYPE_SYMBOL_LT,                 // <
      TYPE_SYMBOL_QUOTE_DOUBLE,       // "
      TYPE_SYMBOL_QUOTE_SINGLE,       // '
      TYPE_SYMBOL_QUESTION,           // ?
      TYPE_SYMBOL_PERCENT,            // %
      TYPE_SYMBOL_CARAT,              // ^
      TYPE_SYMBOL_OR,                 // |
      TYPE_SYMBOL_AND,                // &
      TYPE_SYMBOL_UNDERSCORE,         // _
      TYPE_SYMBOL_COLON,              // :
      TYPE_SYMBOL_SEMICOLON,          // ;
      TYPE_SYMBOL_PAREN_OPEN,         // (
      TYPE_SYMBOL_PAREN_CLOSED,       // )
      TYPE_SYMBOL_CURLY_OPEN,         // {
      TYPE_SYMBOL_CURLY_CLOSED,       // }
      TYPE_SYMBOL_BRACKET_OPEN,       // [
      TYPE_SYMBOL_BRACKET_CLOSED,      // ]
      TYPE_SYMBOL_UNKNOWN,             // unknown, unsupported symbol(s)


      TYPE_PRIMITIVE,    /* A sequence of strictly letters. Only words part of 'primitives'
                           are considered this type. */

      TYPE_LOOPING,      /* A sequence of strictly letters. Only words part of 'looping'
                           are considered this type. */

      TYPE_RETURN,       /* A sequence of strictly letters iff only matching "return" */

      TYPE_WHITESPACE,   /* A sequence of whitespace.  This token type is only
                        produced if report_whitespace() is true.  It is not
                        reported for whitespace within comments or strings. */

      TYPE_NEWLINE,      /* A newline (\n).  This token type is only
                        produced if report_whitespace() is true and
                        report_newlines() is true.  It is not reported for
                        newlines in comments or strings. */

      TYPE_IF,           /* If statement */

      TYPE_ELSE,         /* Else statement */

      TYPE_NULL,        /* For managing leaves in AST construction after tokenization. */
   };

   typedef struct Token {
      Token();
      virtual ~Token();

      TokenType type;
      /* Exact text of the token as appeared in input.*/
      std::string text;
      /* "Line" and "column" specify position of the first character
         of the token with in the input stream. Zero-based. */
      int line;
      ColumnNumber column;
      ColumnNumber end_column;
      Token* next = NULL;
      Token* prev = NULL;

      std::string printTokenType(){
         std::string output;
         output = translateTokenType(type);

         if (output == "default") {
            /* Token verification. Should not have "default" type 0 must belong 
                  to one of other class types. */
            fprintf(stderr, 
                     "ERROR: Default token found. Syntax for the following text is not recognized: \"%s\"\n", 
                     text.c_str());
            exit(1);
         }

         return output;
      }

      virtual void print(std::ofstream& out) {
         out << std::setw(0) << "line: " << line; 
         out << std::setw(5) << "\tcol: " << column;
         out << std::setw(5) << "\t{";
         out << printTokenType();
         out << ", \'";
         out << text;
         out << "\'}"; 
         out << std::setw(30) << "prev token: ";
         if (prev == NULL)
               out << "NULL" << std::endl;
         else
               out << prev->text << std::endl;
      }
   } Token;

   typedef struct ChemicalToken : public Token {
      ~ChemicalToken() override;

      ChemicalToken(Token* token) {
         type = token->type;
         text = token->text;
         line = token->line;
         column = token->column;
         end_column = token->end_column;
         next = token->next;
         prev = token->prev;
         formula = token->text;
         cas = "MISSING";
      };

      std::string formula;
      std::string cas;

      void setFormula(std::string newFormula) {
         formula = newFormula;
      }

      void setCAS(std::string newCAS) {
         cas = newCAS;
      }

      void print(std::ofstream& out) override {
         Tokenizer::Token::print(out);
         out << std::setw(20) << "\t\t\tformula: " << formula << std::endl;
         out << std::setw(20) << "\t\t\tcas: " << cas << std::endl;
      }

   } ChemicalToken;

    /* Get the current token. Updated when Next() is called. Before the
       first call to Next(), current() has type TYPE_START and no contents. */
    const Token& current();

    /* Return the previous token */
    const Token& previous();

    /* Like Next(), but also collects comments which appear between the previous
       and next tokens.

       Comments which appear to be attached to the previous token are stored
       in *prev_tailing_comments.  Comments which appear to be attached to the
       next token are stored in *next_leading_comments.  Comments appearing in
       between which do not appear to be attached to either will be added to
       detached_comments.  Any of these parameters can be NULL to simply discard
       the comments.

       A series of line comments appearing on consecutive lines, with no other
       tokens appearing on those lines, will be treated as a single comment.

       Only the comment content is returned; comment markers (e.g. //) are
       stripped out.  For block comments, leading whitespace and an asterisk will
       be stripped from the beginning of each line other than the first.  Newlines
       are included in the output. */
    bool NextWithComments(std::string* prev_trailing_comments,
                          std::vector<std::string>* detached_comments,
                          std::string* next_leading_comments);

    // Parse helpers ---------------------------------------------------

    // // Parses a TYPE_FLOAT token.  This never fails, so long as the text actually
    // // comes from a TYPE_FLOAT token parsed by Tokenizer.  If it doesn't, the
    // // result is undefined (possibly an assert failure).
    // static double ParseFloat(const std::string& text);

    // // Parses a TYPE_STRING token.  This never fails, so long as the text actually
    // // comes from a TYPE_STRING token parsed by Tokenizer.  If it doesn't, the
    // // result is undefined (possibly an assert failure).
    // static void ParseString(const std::string& text, std::string* output);

    // // Identical to ParseString, but appends to output.
    // static void ParseStringAppend(const std::string& text, std::string* output);

    // Parses a TYPE_INTEGER token.  Returns false if the result would be
    // greater than max_value.  Otherwise, returns true and sets *output to the
    // result.  If the text is not from a Token of type TYPE_INTEGER originally
    // parsed by a Tokenizer, the result is undefined (possibly an assert
    // failure).
    static bool ParseInteger(const std::string& text, uint64_t max_value,
                             uint64_t* output);

    /* Boolean helper methods to determine whether word is of specific token type */
    static bool IsKeyword(std::string word);

    static bool IsParam(std::string word);

    static bool IsFunction(std::string word);

    static bool IsUnit(std::string word);

    static bool IsPrimitive(std::string word);

    static bool IsLooping(std::string word);

    static bool IsReturn(std::string word);

    static bool IsImport(std::string word, bool foundImport);

    static bool IsIf(std::string word);

    static bool IsElse(std::string word);

    // for symbols
    static bool IsAdd(std::string word);

    static bool IsSubtract(std::string(word));

    static bool IsMultiply(std::string(word));

    static bool IsDivide(std::string(word));

    static bool IsEqual(std::string(word));

    static bool IsNot(std::string(word));

    static bool IsComma(std::string word);

    static bool IsDot(std::string(word));

    static bool IsGEQ(std::string(word));

    static bool IsLEQ(std::string(word));

    static bool IsGT(std::string(word));

    static bool IsLT(std::string(word));

    static bool IsQuoteDouble(std::string(word));

    static bool IsQuoteSingle(std::string(word));

    static bool IsQuestion(std::string(word));

    static bool IsPercent(std::string word);

    static bool IsCarat(std::string(word));

    static bool IsOr(std::string(word));

    static bool IsAnd(std::string(word));

    static bool IsUnderscore(std::string(word));

    static bool IsColon(std::string(word));

    static bool IsSemicolon(std::string(word));

    static bool IsParenOpen(std::string(word));

    static bool IsParenClosed(std::string word);

    static bool IsCurlyOpen(std::string(word));

    static bool IsCurlyClosed(std::string(word));

    static bool IsBracketOpen(std::string(word));

    static bool IsBracketClosed(std::string(word));


    // Options ---------------------------------------------------------

    /* If true, whitespace tokens are reported by Next().
       Note: 'set_report_whitespace(false)' implies 'set_report_newlines(false)' */
    bool reportWhitespace() const;

    void setReportWhitespace(bool report);

    /* If true, newline tokens are reported by Next().
       Note: 'set_report_newlines(true)' implies 'set_report_whitespace(true)'. */
    bool reportNewLines() const;

    void setReportNewLines(bool report);

    /* External helper: validate an identifier. */
    static bool IsIdentifier(const std::string& text);

    /* Tokenizes the entire input and returns head to linked list of tokens */
    std::pair<Token*, Token*> tokenize(char* input);

    /* Post tokenization procedures before parsing */
    /* Tokenizes other import files, links to existing tokenization linked list */
    std::vector<FileNode*> searchImports(FileNode* file, std::unordered_set<std::string> allFileNames);
    FileNode* linkImports(std::string fileName, std::string directory, Token* head, Token* tail);
    FileNode* mergeTokenStreams(FileNode* newStream, FileNode* curStream);
    FileNode* reformatTokens(FileNode* file);
    /* Finds all identifiers that are ACTUALLY identifers (declare after keyword) */
    void findIdentifiers(Token* head);
    /* Finds all chemical synonyms + attaches CAS number + matching formula if existing
       in our CheBI adapted database */
    void findChemicals(Token* head);

    static bool IsChemical(Token* token);

    // DEBUG ====================================================
    /* Traverses linkedlist of tokens and prints info in format {TokenType, Token text} */
    static void printTokens(Token* head, std::string input);

    static void printTokenInfo(Token* t);

    /* Prints type of token 't' passed in as argument */
    static std::string printTokenType(Token* t);

    void printState();

    // note: something wrong with 'undefined symbol reference' during
    // compilation despite correctly having in .h file under 'public'
    static std::string translateTokenType(Tokenizer::TokenType type);

    void setFileSize(int newFileSize) {
      file_size = newFileSize;
    }

    // -----------------------------------------------------------------
private:
    Token cur;
    Token prev;

    // ZeroCopyInputStream * input;
    char* input;
    ErrorCollector* collect;

    /* "type to be determined". Needed for determining alphanumeric, continuous,
       valid syntax that can be later classified as one of the following:
          1. Reserved keyword
          2. Reserved function
          3. Reserved parameter
          4. Identifier
       type is set at the end of Next() via SetAlphanumericType() */
    bool type_tbd;
    bool symbol_tbd; // same with symbol

    int file_size;
    const char* buffer;    // Current buffer return from input
    int buffer_pos;         // Current position within a buffer
    char cur_char;          // Same as buffer[buffer_pos]. Updated by NextChar()
    bool read_error;
    bool end_of_file;

    // Line and column nuuber of cur_char within the whole input stream.
    int line;
    ColumnNumber column;

    // List of identifiers - required to differentiate between identifiers vs chemicals.
    std::unordered_set<std::string> identifiers;

    bool foundImport = false;

    /* String to which text should be appended as we advance through it */
    std::string* record_target;
    int record_start;

    // Options
    bool require_space_after_num;
    bool allow_multiline_strings;
    bool whitespace;
    bool newlines;

    /* Since we count columns we need to interpret tabs somehow. Take
       standard 8-characater definition for tab. */
    static const int kTabWidth = 8;

    /* Returns true if at end of file or error occurred when lexing */
    bool endOrFail();

    /* Transforms the next tokenifiable text in input into token */
    Token* Next();


    // -----------------------------------------------------------------
    // Helper Methods

    /* Consume this character and advance to the next one. */
    void NextChar();

    /* Sets record_target = target and start of recording position
       to current buffer position */
    inline void RecordTo(std::string* target);

    inline void StopRecording();

    /* Called when the current character is the first character of a new
       token (not incl. whitespace or comments). */
    inline void StartToken();

    /* Called when current character is the first character after the end
       of the last token. After this returns, cur.text will contain all
       text consumed since StartToken() was called. */
    inline void EndToken();

    /* Convenience method to add error at the current line and column*/
    void AddError(const std::string& message);


    // -----------------------------------------------------------------
    // The following four methods are used to consume tokens of specific
    // types.  They are actually used to consume all characters *after*
    // the first, since the calling function consumes the first character
    // in order to decide what kind of token is being read.

    /* Read + consume a string, ending when the given delimiter is consumed. */
    void ConsumeString(char delimiter);

    /* Read + consume a number, returning TYPE_FLOAT or TYPE INTEGER depending
       on what was read. Needs to know if first character was a '.' to parse
       floating points correctly. */
    TokenType ConsumeNumber(bool start_with_dot);

    /* Consume the rest of a line of single-line comment ("//") */
    void ConsumeLineComment(std::string* content);

    // Consume until "*/"
    void ConsumeBlockComment(std::string* content);

    enum NextCommentStatus {
        /* Started a line comment */
        LINE_COMMENT,

        /* Started a block comment */
        BLOCK_COMMENT,

        /* Consumed a slash, then realized wasn't comment. cur has been filled
           in with a slash token. Caller should return it. */
        SLASH_NOT_COMMENT,

        /* We do not appear to be starting a comment here. */
        NO_COMMENT
    };

    /* If we're at the start of a new comment, consume it + return what kind of
       comment it is. */
    NextCommentStatus TryConsumeCommentStart();

    /* If we're looking at a TYPE_WHITESPACE token and 'report_whitespace' is
       true, consume it and return true. */
    bool TryConsumeWhitespace();

    /* If we're looking at a TYPE_NEWLINE token and 'report_newlines' is true,
       consume it and return true. */
    bool TryConsumeNewline();

    /* -----------------------------------------------------------------
       These helper methods make the parsing cod emore readable. The "character
       classes" referred to are define at the top of the .cxx file. Basically a
       C++ class with one method:
          static bool InClass(char c);
       Method returns true if "c" is a emember of this class (ie. "Letter" or
       "Digit").
    */

    /* Returns true if current character is of given character class.
       DOES NOT CONSUME ANYTHING. */
    template<typename CharacterClass>
    inline bool LookingAt();

    /* If current character is in the given class, consume + return true.
       Otherwise return false. */
    template<typename CharacterClass>
    inline bool TryConsumeOne();

    /* Consumes current character + returns true if character is in given class.
       Otherwise returns false. */
    inline bool TryConsume(char c);

    /* Consumes passed in string + returns true if match. Otherwise returns false */
    inline bool TryConsume(std::string s);

    /* Consume zero or more of the given character class */
    template<typename CharacterClass>
    inline void ConsumeZeroOrMore();

    /* Consume one or more of the given character class
       ie. ConsumneOneOrMore<Digit>("Expected digits."); */
    template<typename CharacterClass>
    inline void ConsumeOneOrMore(const char* error);

    /* Setting type for alphanumeric syntax entities */
    void SetAlphanumericType();

    /* Setting type for symbolic syntax entities */
    void SetSymbolType();
};

static char* readFile(const char* fileName) {
   std::cout << fileName << std::endl;
   std::cout << "in ReadFile for \"" << fileName << "\"" << std::endl;
   std::filesystem::path inputFilePath(fileName);
   std::ifstream input(inputFilePath, std::ios::in | std::ios::binary);
   std::cout << "checking if file " << fileName << " is open\n";
   if (input.is_open()) {
      std::cout << "file is open\n";
      int size = std::filesystem::file_size(inputFilePath);
      char* contents = new char[size];
      input.read(contents, size);
      input.close();
      return contents;
   } else {
      perror("Open() error when reading file.");
   }
}

static int getFileSize(const char* fileName) {
   std::filesystem::path inputFilePath(fileName);
   return std::filesystem::file_size(inputFilePath);
}

static void fail(std::string errorMessage, Tokenizer::Token* curToken) {
   printf("\033[1;31m");    // format color as red
   printf("error: %s", errorMessage.c_str());
   exit(1);
}