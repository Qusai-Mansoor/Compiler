#ifndef CSV_GENERATOR_H
#define CSV_GENERATOR_H

#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <set>
#include "ast.h"

// Forward declarations
struct ObjectShape;
struct TableSchema;

class CSVGenerator {
private:
    std::string outputDir;
    bool streamingMode;
    
    // Map to store table schemas
    std::map<std::string, std::shared_ptr<TableSchema>> tables;
    
    // Map to track object types/shapes
    std::map<std::string, std::shared_ptr<ObjectShape>> objectShapes;
    
    // Map for open file handles when in streaming mode
    std::map<std::string, std::unique_ptr<std::ofstream>> tableFiles;
    
    // Set of tables that were merged into other tables
    std::set<std::string> mergedTables;
    
    // Maps for tracking parent-child relationships
    std::map<std::string, std::vector<std::string>> objArrayMappings;
    std::map<std::string, std::vector<std::string>> scalarArrayMappings;
    
    // Helper methods for analyzing the AST
    void analyzeAst(const std::shared_ptr<AstNode>& node);
    void analyzeObject(const std::shared_ptr<ObjectNode>& objNode);
    void analyzeArray(const std::shared_ptr<ArrayNode>& arrayNode, const std::string& parentKey);
    
    // Table name and relationship management
    void renameTablesBasedOnContent();
    void processRelationships();
    void mergeTable(const std::string& sourceTable, const std::string& targetTable);
    
    // Helper methods for generating CSV rows
    void generateRowsFromAst(const std::shared_ptr<AstNode>& node);
    void generateRowsFromObject(const std::shared_ptr<ObjectNode>& objNode);
    void generateRowsFromArray(const std::shared_ptr<ArrayNode>& arrayNode);
    
    // Helper methods for CSV output
    std::string quoteCSVField(const std::string& field);
    void writeTableRow(const std::string& tableName, const std::vector<std::string>& row);
    
    // Helpers for determining table names
    std::string getTableNameForObjectShape(const std::string& signature);
    std::string getTableNameForArray(const std::string& parentTable, const std::string& key);
    
public:
    CSVGenerator(std::string outputDir = "", bool streaming = false);
    ~CSVGenerator();
    
    // Generate CSV files from AST
    void generateCSV(const AST& ast);
    
    // Get all table names
    std::vector<std::string> getTableNames() const;
};

// Structure to represent an object's shape (field names and types)
struct ObjectShape {
    std::string tableName;
    std::map<std::string, NodeType> fields;
};

// Structure to represent a table schema
struct TableSchema {
    std::string name;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
};

#endif // CSV_GENERATOR_H