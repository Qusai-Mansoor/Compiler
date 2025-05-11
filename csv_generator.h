#ifndef CSV_GENERATOR_H
#define CSV_GENERATOR_H

#include "ast.h"
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <memory>

// Structure to define a table schema
struct TableSchema {
    std::string name;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
};

// Structure to track object shapes for table generation
struct ObjectShape {
    std::string signature;   // Sorted keys joined by commas
    std::string tableName;   // Derived table name
    std::vector<std::string> columns;  // Column names
};

// Main CSV generator class
class CSVGenerator {
private:
    std::string outputDir;
    bool streamingMode;  // Whether to stream rows directly to files
    std::map<std::string, std::shared_ptr<TableSchema>> tables;
    std::map<std::string, std::shared_ptr<ObjectShape>> objectShapes;
    std::map<std::string, std::ofstream> tableFiles;  // For streaming mode
    
    // Internal methods for analyzing the AST
    void analyzeAst(const std::shared_ptr<AstNode>& node);
    void analyzeObject(const std::shared_ptr<ObjectNode>& objNode);
    void analyzeArray(const std::shared_ptr<ArrayNode>& arrayNode, const std::string& parentKey);
    
    // Methods for generating rows
    void generateRowsFromAst(const std::shared_ptr<AstNode>& node);
    void generateRowsFromObject(const std::shared_ptr<ObjectNode>& objNode);
    void generateRowsFromArray(const std::shared_ptr<ArrayNode>& arrayNode);
    
    // Helper methods
    std::string getTableNameForObjectShape(const std::string& signature);
    std::string getTableNameForArray(const std::string& parentTable, const std::string& key);
    std::string quoteCSVField(const std::string& field);
    void writeTableRow(const std::string& tableName, const std::vector<std::string>& row);
    
public:
    explicit CSVGenerator(std::string outputDir, bool streaming = true);
    ~CSVGenerator();
    
    // Generate CSV files from the AST
    void generateCSV(const AST& ast);
    
    // Get the list of generated tables (for testing)
    std::vector<std::string> getTableNames() const;
};

#endif // CSV_GENERATOR_H