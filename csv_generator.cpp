#include "csv_generator.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <set>
#include <unordered_set>

namespace fs = std::filesystem;

CSVGenerator::CSVGenerator(std::string outputDir, bool streaming)
    : outputDir(std::move(outputDir)), streamingMode(streaming) {
}

// Destructor
CSVGenerator::~CSVGenerator() {
    // Close any open file handles
    for (auto& pair : tableFiles) {
        if (pair.second && pair.second->is_open()) {
            pair.second->close();
        }
    }
}

// Quote and escape CSV fields as needed
std::string CSVGenerator::quoteCSVField(const std::string& field) {
    // If field is already quoted, return as is
    if (!field.empty() && field.front() == '"' && field.back() == '"') {
        return field;
    }
    
    // If field contains comma, quote it
    if (field.find(',') != std::string::npos || 
        field.find('"') != std::string::npos || 
        field.find('\n') != std::string::npos) {
        
        std::string result = "\"";
        for (char c : field) {
            if (c == '"') {
                result += "\"\""; // Double quotes to escape
            } else {
                result += c;
            }
        }
        result += "\"";
        return result;
    }
    
    return field;
}

// Write a single row to a table file
void CSVGenerator::writeTableRow(const std::string& tableName, const std::vector<std::string>& row) {
    auto fileIt = tableFiles.find(tableName);
    
    // If file not open yet, open it
    if (fileIt == tableFiles.end()) {
        if (!outputDir.empty()) {
            std::string filename = outputDir + "/" + tableName + ".csv";
            auto file = std::make_unique<std::ofstream>(filename);
            
            if (!file->is_open()) {
                std::cerr << "Error: Could not open file " << filename << std::endl;
                return;
            }
            
            tableFiles[tableName] = std::move(file);
        } else {
            std::cerr << "Error: No output directory specified" << std::endl;
            return;
        }
        
        // Write header if this is a new file
        auto schemaIt = tables.find(tableName);
        if (schemaIt != tables.end() && !schemaIt->second->columns.empty()) {
            std::vector<std::string> quotedHeaders;
            for (const auto& col : schemaIt->second->columns) {
                quotedHeaders.push_back(quoteCSVField(col));
            }
            
            *tableFiles[tableName] << quotedHeaders[0];
            for (size_t i = 1; i < quotedHeaders.size(); ++i) {
                *tableFiles[tableName] << "," << quotedHeaders[i];
            }
            *tableFiles[tableName] << std::endl;
        }
    }
    
    // Write the row
    *tableFiles[tableName] << row[0];
    for (size_t i = 1; i < row.size(); ++i) {
        *tableFiles[tableName] << "," << row[i];
    }
    *tableFiles[tableName] << std::endl;
}

// Generate CSV from AST
void CSVGenerator::generateCSV(const AST& ast) {
    auto root = ast.getRoot();
    if (!root) return;
    
    // First pass: analyze the structure
    analyzeAst(root);
    
    // Add ID columns to all tables
    for (const auto& tablePair : tables) {
        auto& schema = tablePair.second;
        
        // Ensure columns list starts with 'id'
        auto it = std::find(schema->columns.begin(), schema->columns.end(), "id");
        if (it != schema->columns.begin()) {
            if (it != schema->columns.end()) {
                schema->columns.erase(it);
            }
            schema->columns.insert(schema->columns.begin(), "id");
        }
    }
    
    // Add foreign key columns for object shapes
    for (const auto& objShape : objectShapes) {
        // Add parent_id column for non-root objects
        if (objShape.second->tableName != "root") {
            auto& schema = tables[objShape.second->tableName];
            
            if (schema) {
                auto it = std::find(schema->columns.begin(), schema->columns.end(), "root_id");
                if (it == schema->columns.end()) {
                    // Add after ID column
                    schema->columns.insert(schema->columns.begin() + 1, "root_id");
                }
            }
        }
    }
    
    // Setup streaming mode if needed
    if (streamingMode) {
        for (const auto& tablePair : tables) {
            const std::string& tableName = tablePair.first;
            const auto& schema = tablePair.second;
            
            std::string filename;
            if (!outputDir.empty()) {
                filename = outputDir + "/" + tableName + ".csv";
            } else {
                filename = tableName + ".csv";
            }
            
            std::unique_ptr<std::ofstream> file = std::make_unique<std::ofstream>(filename);
            if (!file->is_open()) {
                std::cerr << "Error: Could not open file " << filename << std::endl;
                continue;
            }
            
            // Write headers
            if (!schema->columns.empty()) {
                *file << schema->columns[0];
                for (size_t i = 1; i < schema->columns.size(); ++i) {
                    *file << "," << schema->columns[i];
                }
                *file << std::endl;
            }
            
            tableFiles[tableName] = std::move(file);
        }
    }
    
    // Second pass: generate actual CSV data
    generateRowsFromAst(root);
    
    // If not in streaming mode, write all tables at once
    if (!streamingMode) {
        for (const auto& tablePair : tables) {
            const std::string& tableName = tablePair.first;
            const auto& schema = tablePair.second;
            
            std::string filename;
            if (!outputDir.empty()) {
                filename = outputDir + "/" + tableName + ".csv";
            } else {
                filename = tableName + ".csv";
            }
            
            std::ofstream outfile(filename);
            if (!outfile.is_open()) {
                std::cerr << "Error: Could not open file " << filename << std::endl;
                continue;
            }
            
            // Write headers
            if (!schema->columns.empty()) {
                outfile << schema->columns[0];
                for (size_t i = 1; i < schema->columns.size(); ++i) {
                    outfile << "," << schema->columns[i];
                }
                outfile << std::endl;
            }
            
            // Write rows
            for (const auto& row : schema->rows) {
                if (!row.empty()) {
                    outfile << row[0];
                    for (size_t i = 1; i < row.size(); ++i) {
                        outfile << "," << row[i];
                    }
                    outfile << std::endl;
                }
            }
            
            outfile.close();
        }
    }
    
    // Close any open files
    for (auto& pair : tableFiles) {
        if (pair.second && pair.second->is_open()) {
            pair.second->close();
        }
    }
}

// Get all table names
std::vector<std::string> CSVGenerator::getTableNames() const {
    std::vector<std::string> names;
    for (const auto& pair : tables) {
        names.push_back(pair.first);
    }
    return names;
}

// Get table name for an object shape
std::string CSVGenerator::getTableNameForObjectShape(const std::string& signature) {
    // Check if we've already assigned a name
    auto it = objectShapes.find(signature);
    if (it != objectShapes.end()) {
        return it->second->tableName;
    }
    
    // For root object, name is always "root"
    if (signature.empty() || signature == "_ROOT_") {
        return "root";
    }
    
    // For known patterns, use specific names
    if (signature.find("name") != std::string::npos && 
        signature.find("age") != std::string::npos) {
        return "person";  // Special case for person objects
    }
    
    // Otherwise, generate a generic name
    static int tableCounter = 0;
    std::string name = "table" + std::to_string(++tableCounter);
    return name;
}

// Get table name for an array
std::string CSVGenerator::getTableNameForArray(const std::string& parentTable, const std::string& key) {
    // Use parent table name and key to generate array table name
    return parentTable + "_" + key;
}

// Analyze AST structure
void CSVGenerator::analyzeAst(const std::shared_ptr<AstNode>& node) {
    if (!node) return;
    
    // Process based on node type
    if (node->getType() == NodeType::OBJECT) {
        auto objNode = std::dynamic_pointer_cast<ObjectNode>(node);
        if (objNode) {
            analyzeObject(objNode);
        }
    }
    else if (node->getType() == NodeType::ARRAY) {
        auto arrayNode = std::dynamic_pointer_cast<ArrayNode>(node);
        if (arrayNode) {
            analyzeArray(arrayNode, "root");
        }
    }
    // Other node types don't need analysis
}

// Analyze object node
void CSVGenerator::analyzeObject(const std::shared_ptr<ObjectNode>& objNode) {
    if (!objNode) return;
    
    // Get object signature for table identification
    std::string signature = objNode->getKeySignature();
    std::string tableName;
    
    auto shapeIt = objectShapes.find(signature);
    
    // Create new object shape if not exists
    if (shapeIt == objectShapes.end()) {
        auto shape = std::make_shared<ObjectShape>();
        shape->tableName = getTableNameForObjectShape(signature);
        
        // Record fields and their types
        for (const auto& pair : objNode->pairs) {
            if (pair.value->getType() != NodeType::OBJECT && 
                pair.value->getType() != NodeType::ARRAY) {
                shape->fields[pair.key] = pair.value->getType();
            }
        }
        
        objectShapes[signature] = shape;
        tableName = shape->tableName;
    } else {
        tableName = shapeIt->second->tableName;
    }
    
    // Set table name in the object node
    objNode->tableName = tableName;
    
    // Ensure we have a schema for this table
    auto tableIt = tables.find(tableName);
    if (tableIt == tables.end()) {
        auto schema = std::make_shared<TableSchema>();
        schema->name = tableName;
        schema->columns.push_back("id");  // Always have ID column
        
        // Add columns for simple fields
        for (const auto& pair : objNode->pairs) {
            if (pair.value->getType() != NodeType::OBJECT && 
                pair.value->getType() != NodeType::ARRAY) {
                schema->columns.push_back(pair.key);
            }
        }
        
        tables[schema->name] = schema;
    }
    
    // Process nested objects and arrays
    for (const auto& pair : objNode->pairs) {
        if (pair.value->getType() == NodeType::OBJECT) {
            auto nestedObj = std::dynamic_pointer_cast<ObjectNode>(pair.value);
            if (nestedObj) {
                nestedObj->parentTable = tableName;
                nestedObj->parentKey = pair.key;
                analyzeObject(nestedObj);
            }
        } 
        else if (pair.value->getType() == NodeType::ARRAY) {
            auto arrayNode = std::dynamic_pointer_cast<ArrayNode>(pair.value);
            if (arrayNode) {
                arrayNode->parentTable = tableName;
                arrayNode->parentKey = pair.key;
                analyzeArray(arrayNode, pair.key);
            }
        }
    }
}

// Analyze array node
void CSVGenerator::analyzeArray(const std::shared_ptr<ArrayNode>& arrayNode, const std::string& parentKey) {
    if (!arrayNode) return;
    
    // Different handling based on array content
    if (arrayNode->isArrayOfObjects()) {
        // For arrays of objects, analyze each object
        for (const auto& elem : arrayNode->elements) {
            if (elem->getType() == NodeType::OBJECT) {
                auto objNode = std::dynamic_pointer_cast<ObjectNode>(elem);
                if (objNode) {
                    objNode->parentTable = arrayNode->parentTable;
                    objNode->parentKey = arrayNode->parentKey;
                    analyzeObject(objNode);
                }
            }
        }
    } 
    else if (arrayNode->isArrayOfScalars()) {
        // For arrays of scalars, create a separate table
        std::string tableName = arrayNode->parentTable + "_" + parentKey;
        
        if (tables.find(tableName) == tables.end()) {
            auto schema = std::make_shared<TableSchema>();
            schema->name = tableName;
            schema->columns = {"id", "parent_id", "index", "value"};
            tables[tableName] = schema;
        }
    }
}

// Generate CSV rows from AST
void CSVGenerator::generateRowsFromAst(const std::shared_ptr<AstNode>& node) {
    if (!node) return;
    
    // Process based on node type
    if (node->getType() == NodeType::OBJECT) {
        auto objNode = std::dynamic_pointer_cast<ObjectNode>(node);
        if (objNode) {
            generateRowsFromObject(objNode);
        }
    }
    else if (node->getType() == NodeType::ARRAY) {
        auto arrayNode = std::dynamic_pointer_cast<ArrayNode>(node);
        if (arrayNode) {
            generateRowsFromArray(arrayNode);
        }
    }
    // Other node types don't need processing
}

// Generate rows from object node
void CSVGenerator::generateRowsFromObject(const std::shared_ptr<ObjectNode>& objNode) {
    if (!objNode || objNode->tableName.empty()) return;
    
    auto tableIt = tables.find(objNode->tableName);
    if (tableIt == tables.end()) return;
    
    auto& schema = tableIt->second;
    std::vector<std::string> row(schema->columns.size());
    
    // Fill in ID
    row[0] = std::to_string(objNode->id);
    
    // Fill in parent ID if applicable
    if (objNode->parentId >= 0) {
        auto parentIdIt = std::find(schema->columns.begin(), schema->columns.end(), 
                                    objNode->parentTable + "_id");
        if (parentIdIt != schema->columns.end()) {
            int index = std::distance(schema->columns.begin(), parentIdIt);
            row[index] = std::to_string(objNode->parentId);
        }
    }
    
    // Fill in scalar values
    for (const auto& pair : objNode->pairs) {
        auto columnIt = std::find(schema->columns.begin(), schema->columns.end(), pair.key);
        if (columnIt != schema->columns.end() && 
            pair.value->getType() != NodeType::OBJECT && 
            pair.value->getType() != NodeType::ARRAY) {
            
            int index = std::distance(schema->columns.begin(), columnIt);
            
            std::string value;
            if (pair.value->getType() == NodeType::STRING) {
                auto strNode = std::dynamic_pointer_cast<StringNode>(pair.value);
                value = "\"" + strNode->toString() + "\"";
            }
            else if (pair.value->getType() == NodeType::NUMBER) {
                auto numNode = std::dynamic_pointer_cast<NumberNode>(pair.value);
                value = "\"" + numNode->toString() + "\"";
            }
            else if (pair.value->getType() == NodeType::BOOLEAN) {
                auto boolNode = std::dynamic_pointer_cast<BooleanNode>(pair.value);
                value = boolNode->toString();
            }
            else if (pair.value->getType() == NodeType::NULL_VALUE) {
                value = "null";
            }
            
            row[index] = value;
        }
    }
    
    // Add the row to the schema or write it directly
    if (streamingMode) {
        std::vector<std::string> quotedRow;
        for (const auto& val : row) {
            quotedRow.push_back(quoteCSVField(val));
        }
        writeTableRow(objNode->tableName, quotedRow);
    } else {
        schema->rows.push_back(row);
    }
    
    // Process nested objects and arrays
    for (const auto& pair : objNode->pairs) {
        if (pair.value->getType() == NodeType::OBJECT) {
            auto nestedObj = std::dynamic_pointer_cast<ObjectNode>(pair.value);
            if (nestedObj) {
                generateRowsFromObject(nestedObj);
            }
        } 
        else if (pair.value->getType() == NodeType::ARRAY) {
            auto arrayNode = std::dynamic_pointer_cast<ArrayNode>(pair.value);
            if (arrayNode) {
                generateRowsFromArray(arrayNode);
            }
        }
    }
}

// Generate rows from array node
void CSVGenerator::generateRowsFromArray(const std::shared_ptr<ArrayNode>& arrayNode) {
    if (!arrayNode) return;
    
    if (arrayNode->isArrayOfObjects()) {
        // For arrays of objects, process each object
        for (const auto& elem : arrayNode->elements) {
            if (elem->getType() == NodeType::OBJECT) {
                auto objNode = std::dynamic_pointer_cast<ObjectNode>(elem);
                if (objNode) {
                    // Ensure the object has the right table name
                    // This might be duplicative since we already did it in analyzeArray
                    // but it's safer to do it again
                    for (auto& tableSchemaPair : tables) {
                        const auto& tableName = tableSchemaPair.first;
                        if (tableName != "root" && 
                            objNode->getKeySignature() == 
                            objectShapes[tableName]->tableName) {
                            
                            objNode->tableName = tableName;
                            break;
                        }
                    }
                    
                    // Process the object
                    if (streamingMode) {
                        generateRowsFromObject(objNode);
                    } else {
                        generateRowsFromObject(objNode);
                    }
                }
            }
        }
    } 
    else if (arrayNode->isArrayOfScalars()) {
        // For arrays of scalars, create rows in the array table
        std::string tableName = arrayNode->parentTable + "_" + arrayNode->parentKey;
        
        auto tableIt = tables.find(tableName);
        if (tableIt == tables.end()) return;
        
        for (size_t i = 0; i < arrayNode->elements.size(); ++i) {
            std::vector<std::string> row(4); // id, parent_id, index, value
            
            row[0] = std::to_string(i + 1);  // 1-based ID
            row[1] = std::to_string(arrayNode->parentId);
            row[2] = std::to_string(i);      // 0-based index
            
            // Get value based on type
            const auto& elem = arrayNode->elements[i];
            if (elem->getType() == NodeType::STRING) {
                auto strNode = std::dynamic_pointer_cast<StringNode>(elem);
                row[3] = "\"" + strNode->toString() + "\"";
            }
            else if (elem->getType() == NodeType::NUMBER) {
                auto numNode = std::dynamic_pointer_cast<NumberNode>(elem);
                row[3] = "\"" + numNode->toString() + "\"";
            }
            else if (elem->getType() == NodeType::BOOLEAN) {
                auto boolNode = std::dynamic_pointer_cast<BooleanNode>(elem);
                row[3] = boolNode->toString();
            }
            else if (elem->getType() == NodeType::NULL_VALUE) {
                row[3] = "null";
            }
            
            if (streamingMode) {
                std::vector<std::string> quotedRow;
                for (const auto& val : row) {
                    quotedRow.push_back(quoteCSVField(val));
                }
                writeTableRow(tableName, quotedRow);
            } else {
                tableIt->second->rows.push_back(row);
            }
        }
    }
}