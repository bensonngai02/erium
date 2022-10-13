#include "tokenizer.h"

class FileNode {
   std::string fileName;
   std::string directory;
   Tokenizer::Token* fileHead;
   Tokenizer::Token* fileTail;
   std::vector<FileNode*> dependencies;   
   Tokenizer::Token* noImportStream;

  public:
    FileNode(std::string newFileName, std::string newDirectoryName) : 
        fileName(newFileName), directory(newDirectoryName), fileHead(nullptr), fileTail(nullptr), noImportStream(nullptr) {}
    FileNode(std::string newFileName, std::string newDirectoryName, Tokenizer::Token* newHead, Tokenizer::Token* newTail) :
            fileName(newFileName), directory(newDirectoryName), fileHead(newHead), fileTail(newTail), noImportStream(nullptr) {};
    ~FileNode() {};

    bool hasDependency() {
        return dependencies.size() != 0;
    }

    void addDependency(std::string newFileName, std::string newFileDirectory) {
        std::string modified = newFileName;
        std::cout << "trying to add dependency " << newFileDirectory << newFileName << "..." << std::endl;
        ErrorCollector collect;
        std::cout << "checkpoint\n";
        char* newInput = readFile(newFileName.c_str());
        Tokenizer* newTokenizer = new Tokenizer(newInput, &collect);
        newTokenizer->setFileSize(getFileSize(newFileName.c_str()));
        std::pair<Tokenizer::Token*, Tokenizer::Token*> newImport = newTokenizer->tokenize(newInput);
        newTokenizer->findIdentifiers(newImport.first);
        newTokenizer->findChemicals(newImport.first);
        newTokenizer->printTokens(newImport.first, newFileName);
        dependencies.push_back(new FileNode(newFileName, newFileDirectory, newImport.first, newImport.second));
        std::cout << "added dependency " << newFileName << " successfully!" << std::endl;
    }

    void pushDependencies(std::vector<FileNode*> newDependencies) {
        for (FileNode* dependency : newDependencies) {
        dependencies.push_back(dependency);
        }
    }

    std::vector<FileNode*> getDependencies() {
        return dependencies;
    }

    std::string getDependenciesNames() {
        std::string allDependencies = "";
        for (FileNode* dependency : dependencies) {
        allDependencies += dependency->getFileName();
        allDependencies += ", ";
        }
        return allDependencies;
    }

    bool isTokenized() {
        return fileHead != nullptr && fileTail != nullptr;
    }

    void setVisited(std::unordered_set<std::string> allFileNames) {
        allFileNames.insert(fileName);
    }

    bool isVisited(std::unordered_set<std::string> allFileNames) {
        return allFileNames.count(fileName) != 0;
    }

    std::string getFileName() {
        return fileName;
    }
    
    std::string getDirectory() {
        return directory;
    }

    void setDirectory(std::string newDirectoryName) {
        directory = newDirectoryName;
    }

    void setFileHead(Tokenizer::Token* newHead) {
        fileHead = newHead;
    }

    void setFileTail(Tokenizer::Token* newTail) {
        fileTail = newTail;
    }

    Tokenizer::Token* getFileHead() {
        return fileHead;
    }

    Tokenizer::Token* getFileTail() {
        return fileTail;
    }

    Tokenizer::Token* getNoImportStream() {
        return noImportStream;
    }

    void setNoImportStream(Tokenizer::Token* newHead) {
        noImportStream = newHead;
    }
};