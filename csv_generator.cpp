#include "csv_generator.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <set>

namespace fs = std::filesystem;

CSVGenerator::CSVGenerator(std::string outputDir, bool streaming)
    : outputDir(std::move(outputDir)), streamingMode(streaming) {
    
    // Create output directory if it doesn't exist
    if (!outputDir.empty() && !fs::exists(outputDir)) {
        fs::create_directories(outputDir);
    }
}

std::string CSVGenerator::quoteCSVField(const std::string& field) {
    // Check if field needs quoting
    bool needsQuote = field.find(',') != std::string::npos ||
                      field.find('"') != std::string::npos ||
                      field.find('\n') != std::string::npos ||
                      field.find('\r') != std::string::npos;
    
    if (!needsQuote) {
        return field;
    }
    
    // Quote and escape the field
    std::stringstream ss;
    ss << '"';
    
    for (char c : field) {
        if (c == '"') {
            ss << "\"\""; // Double quotes are escaped as double-double quotes
        } else {
            ss << c;
        }
    }
    
    ss << '"';
    return ss.str();
}

void CSVGenerator::writeTableRow(const std::string& tableName, const std::vector<std::string>& row) {
    // Get or open the file
    auto fileIt = tableFiles.find(tableName);
    if (fileIt == tableFiles.end() || !fileIt->second.is_open()) {
        std::string filename = tableName + ".csv";
        if (!outputDir.empty()) {
            filename = outputDir + "/" + filename;
        }
        
        // Open the file
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file " << filename << std::endl;
            return;
        }
        
        // Write header
        auto schemaIt = tables.find(tableName);
        if (schemaIt != tables.end()) {
            const auto& columns = schemaIt->second->columns;
            for (size_t i = 0; i < columns.size(); ++i) {
                if (i > 0) file << ",";
                file << quoteCSVField(columns[i]);
            }
            file << std::endl;
        }
        
        // Store the file stream
        tableFiles[tableName] = std::move(file);
        fileIt = tableFiles.find(tableName);
    }
    
    // Write the row
    for (size_t i = 0; i < row.size(); ++i) {
        if (i > 0) fileIt->second << ",";
        fileIt->second << quoteCSVField(row[i]);
    }
    fileIt->second << std::endl;
}

void CSVGenerator::generateCSV(const AST& ast) {
    auto root = ast.getRoot();
    if (!root) return;
    
    // First pass: analyze the AST to identify tables
    analyzeAst(root);
    
    // Add seq column to tables with arrays of objects
    for (const auto& tablePair : tables) {
        auto& schema = tablePair.second;
        
        // Check if this table needs a sequence column
        bool needsSeq = false;
        for (const auto& objShape : objectShapes) {
            if (objShape.second->tableName == schema->name) {
                // Check if objects of this shape are used in arrays
                auto it = std::find(schema->columns.begin(), schema->columns.end(), 
                                  "parent_id");
                if (it != schema->columns.end()) {
                    needsSeq = true;
                    break;
                }
            }
        }
        
        if (needsSeq && std::find(schema->columns.begin(), schema->columns.end(), "seq") == schema->columns.end()) {
            // Add seq column after id and parent_id
            auto it = schema->columns.begin();
            ++it; // Skip id
            
            if (it != schema->columns.end() && it->find("_id") != std::string::npos) {
                ++it; // Skip parent_id
            }
            
            schema->columns.insert(it, "seq");
        }
    }
    
    // Open files for streaming mode
    if (streamingMode) {
        for (const auto& tablePair : tables) {
            const auto& tableName = tablePair.first;
            std::string filename = tableName + ".csv";
            if (!outputDir.empty()) {
                filename = outputDir + "/" + filename;
            }
            
            // Open file and write header
            std::ofstream file(filename);
            if (!file.is_open()) {
                std::cerr << "Error: Could not open file " << filename << std::endl;
                continue;
            }
            
            // Write header
            const auto& columns = tablePair.second->columns;
            for (size_t i = 0; i < columns.size(); ++i) {
                if (i > 0) file << ",";
                file << quoteCSVField(columns[i]); 
            }
            file << std::endl;
            
            // Store the file stream
            tableFiles[tableName] = std::move(file);
        }
    }
    
    // Second pass: generate rows
    generateRowsFromAst(root);
    
    // If not in streaming mode, write all tables to files
    if (!streamingMode) {
        for (const auto& tablePair : tables) {
            const auto& tableName = tablePair.first;
            const auto& schema = tablePair.second;
            
            // Open file
            std::string filename = tableName + ".csv";
            if (!outputDir.empty()) {
                filename = outputDir + "/" + filename;
            }
            
            std::ofstream file(filename);
            if (!file.is_open()) {
                std::cerr << "Error: Could not open file " << filename << std::endl;
                continue;
            }
            
            // Write header
            for (size_t i = 0; i < schema->columns.size(); ++i) {
                if (i > 0) file << ",";
                file << quoteCSVField(schema->columns[i]);
            }
            file << std::endl;
            
            // Write rows
            for (const auto& row : schema->rows) {
                for (size_t i = 0; i < row.size(); ++i) {
                    if (i > 0) file << ",";
                    file << quoteCSVField(row[i]);
                }
                file << std::endl;
            }
            
            file.close();
        }
    }
    
    // Close all open files
    for (auto& pair : tableFiles) {
        if (pair.second.is_open()) {
            pair.second.close();
        }
    }
}

std::vector<std::string> CSVGenerator::getTableNames() const {
    std::vector<std::string> names;
    for (const auto& pair : tables) {
        names.push_back(pair.first);
    }
    return names;
}

CSVGenerator::~CSVGenerator() {
    // Close any open files
    for (auto& pair : tableFiles) {
        if (pair.second.is_open()) {
            pair.second.close();
        }
    }
}

std::string CSVGenerator::getTableNameForObjectShape(const std::string& signature) {
    // Find existing table for this object shape
    auto it = objectShapes.find(signature);
    if (it != objectShapes.end()) {
        return it->second->tableName;
    }
    
    // If no table exists, create a name based on the keys
    std::string tableName;
    std::istringstream iss(signature);
    std::string key;
    
    // Get the first key as the base table name
    if (std::getline(iss, key, ',')) {
        tableName = key;
        
        // Convert to plural form if it's not already
        if (!tableName.empty() && tableName.back() != 's') {
            tableName += 's';
        }
    } else {
        // Fallback name if signature is empty
        tableName = "items";
    }
    
    // Make sure the table name is unique
    std::string uniqueName = tableName;
    int counter = 1;
    while (std::any_of(objectShapes.begin(), objectShapes.end(), 
                      [&uniqueName](const auto& pair) { 
                          return pair.second->tableName == uniqueName; 
                      })) {
        uniqueName = tableName + std::to_string(counter++);
    }
    
    return uniqueName;
}

std::string CSVGenerator::getTableNameForArray(const std::string& parentTable, const std::string& key) {
    // Use the key name directly if it's plural, otherwise use singular parent table name + key
    if (!key.empty() && key.back() == 's') {
        return key;
    }
    
    return key;
}

void CSVGenerator::analyzeAst(const std::shared_ptr<AstNode>& node) {
    if (!node) return;
    
    switch (node->getType()) {
        case NodeType::OBJECT: {
            auto objNode = std::dynamic_pointer_cast<ObjectNode>(node);
            analyzeObject(objNode);
            break;
        }
        case NodeType::ARRAY: {
            auto arrayNode = std::dynamic_pointer_cast<ArrayNode>(node);
            analyzeArray(arrayNode, "root");
            break;
        }
        default:
            // Scalar values at root level are ignored
            break;
    }
}

void CSVGenerator::analyzeObject(const std::shared_ptr<ObjectNode>& objNode) {
    if (!objNode) return;
    
    // Get the signature of this object
    std::string signature = objNode->getKeySignature();
    
    // Check if we've seen this object shape before
    auto shapeIt = objectShapes.find(signature);
    if (shapeIt == objectShapes.end()) {
        // New object shape, create table schema
        auto shape = std::make_shared<ObjectShape>();
        shape->signature = signature;
        shape->tableName = getTableNameForObjectShape(signature);
        
        // Always add 'id' column first
        shape->columns.push_back("id");
        
        // Add parent_id column if this is a nested object
        if (!objNode->parentTable.empty()) {
            shape->columns.push_back(objNode->parentTable + "_id");
        }
        
        // Add columns for all scalar values
        for (const auto& pair : objNode->pairs) {
            auto valueType = pair.value->getType();
            if (valueType == NodeType::STRING || valueType == NodeType::NUMBER || 
                valueType == NodeType::BOOLEAN || valueType == NodeType::NULL_VALUE) {
                shape->columns.push_back(pair.key);
            }
        }
        
        // Store the shape
        objectShapes[signature] = shape;
        
        // Create table schema
        auto schema = std::make_shared<TableSchema>();
        schema->name = shape->tableName;
        schema->columns = shape->columns;
        tables[schema->name] = schema;
        
        // Set the table name in the object node for future reference
        objNode->tableName = shape->tableName;
    } else {
        // Existing object shape, just set the table name
        objNode->tableName = shapeIt->second->tableName;
    }
    
    // Process nested structures
    for (const auto& pair : objNode->pairs) {
        if (pair.value->getType() == NodeType::OBJECT) {
            auto nestedObj = std::dynamic_pointer_cast<ObjectNode>(pair.value);
            analyzeObject(nestedObj);
        } else if (pair.value->getType() == NodeType::ARRAY) {
            auto arrayNode = std::dynamic_pointer_cast<ArrayNode>(pair.value);
            analyzeArray(arrayNode, pair.key);
        }
    }
}

void CSVGenerator::analyzeArray(const std::shared_ptr<ArrayNode>& arrayNode, const std::string& parentKey) {
    if (!arrayNode || arrayNode->elements.empty()) return;
    
    // Handle array of objects (child table)
    if (arrayNode->isArrayOfObjects()) {
        // Process each object in the array
        for (const auto& element : arrayNode->elements) {
            auto objNode = std::dynamic_pointer_cast<ObjectNode>(element);
            analyzeObject(objNode);
        }
    }
    // Handle array of scalars (junction table)
    else if (arrayNode->isArrayOfScalars() && !arrayNode->parentTable.empty()) {
        // Create junction table: parent_id, index, value
        std::string tableName = getTableNameForArray(arrayNode->parentTable, arrayNode->parentKey);
        
        // Check if table already exists
        if (tables.find(tableName) == tables.end()) {
            auto schema = std::make_shared<TableSchema>();
            schema->name = tableName;
            schema->columns = {
                arrayNode->parentTable + "_id",
                "index",
                "value"
            };
            tables[tableName] = schema;
        }
    }
    // Handle array of mixed types or nested arrays
    else {
        // Process each element in the array
        for (const auto& element : arrayNode->elements) {
            if (element->getType() == NodeType::OBJECT) {
                auto objNode = std::dynamic_pointer_cast<ObjectNode>(element);
                analyzeObject(objNode);
            } else if (element->getType() == NodeType::ARRAY) {
                auto nestedArray = std::dynamic_pointer_cast<ArrayNode>(element);
                analyzeArray(nestedArray, parentKey + "_item");
            }
        }
    }
}

void CSVGenerator::generateRowsFromAst(const std::shared_ptr<AstNode>& node) {
    if (!node) return;
    
    switch (node->getType()) {
        case NodeType::OBJECT: {
            auto objNode = std::dynamic_pointer_cast<ObjectNode>(node);
            generateRowsFromObject(objNode);
            break;
        }
        case NodeType::ARRAY: {
            auto arrayNode = std::dynamic_pointer_cast<ArrayNode>(node);
            generateRowsFromArray(arrayNode);
            break;
        }
        default:
            // Scalar values at root level are ignored
            break;
    }
}

void CSVGenerator::generateRowsFromObject(const std::shared_ptr<ObjectNode>& objNode) {
    if (!objNode || objNode->tableName.empty()) return;
    
    // Get the table schema
    auto tableIt = tables.find(objNode->tableName);
    if (tableIt == tables.end()) return;
    
    auto& schema = tableIt->second;
    
    // Create a row for this object
    std::vector<std::string> row(schema->columns.size(), "");
    
    // Set the ID
    row[0] = std::to_string(objNode->id);
    
    // Set parent ID if this is a nested object
    if (!objNode->parentTable.empty() && objNode->parentId > 0) {
        // Find the parent_id column index
        auto parentIdCol = objNode->parentTable + "_id";
        auto it = std::find(schema->columns.begin(), schema->columns.end(), parentIdCol);
        if (it != schema->columns.end()) {
            int index = std::distance(schema->columns.begin(), it);
            row[index] = std::to_string(objNode->parentId);
        }
    }
    
    // Set values for scalar fields
    for (const auto& pair : objNode->pairs) {
        auto valueType = pair.value->getType();
        if (valueType == NodeType::STRING || valueType == NodeType::NUMBER || 
            valueType == NodeType::BOOLEAN || valueType == NodeType::NULL_VALUE) {
            
            // Find the column index
            auto it = std::find(schema->columns.begin(), schema->columns.end(), pair.key);
            if (it != schema->columns.end()) {
                int index = std::distance(schema->columns.begin(), it);
                
                // Convert value to string
                auto valueNode = std::dynamic_pointer_cast<ValueNode>(pair.value);
                row[index] = valueNode->toString();
            }
        }
    }
    
    // Add the row to the table or stream it directly
    if (streamingMode) {
        writeTableRow(schema->name, row);
    } else {
        schema->rows.push_back(row);
    }
    
    // Process nested structures
    for (const auto& pair : objNode->pairs) {
        if (pair.value->getType() == NodeType::OBJECT) {
            auto nestedObj = std::dynamic_pointer_cast<ObjectNode>(pair.value);
            generateRowsFromObject(nestedObj);
        } else if (pair.value->getType() == NodeType::ARRAY) {
            auto arrayNode = std::dynamic_pointer_cast<ArrayNode>(pair.value);
            generateRowsFromArray(arrayNode);
        }
    }
}

void CSVGenerator::generateRowsFromArray(const std::shared_ptr<ArrayNode>& arrayNode) {
    if (!arrayNode || arrayNode->elements.empty()) return;
    
    // Handle array of objects (child table)
    if (arrayNode->isArrayOfObjects()) {
        // Generate rows for each object
        for (size_t i = 0; i < arrayNode->elements.size(); ++i) {
            auto objNode = std::dynamic_pointer_cast<ObjectNode>(arrayNode->elements[i]);
            
            // Add sequence number if needed
            for (auto& tableSchemaPair : tables) {
                auto& schema = tableSchemaPair.second;
                auto seqIt = std::find(schema->columns.begin(), schema->columns.end(), "seq");
                
                if (seqIt != schema->columns.end() && 
                    objNode->parentId == arrayNode->parentId &&
                    objNode->parentTable == arrayNode->parentTable) {
                    // Find the seq column index
                    int seqIndex = std::distance(schema->columns.begin(), seqIt);
                    
                    // Get or create the row
                    std::vector<std::string> row;
                    if (streamingMode) {
                        row = std::vector<std::string>(schema->columns.size(), "");
                    } else if (i < schema->rows.size()) {
                        row = schema->rows[i];
                    } else {
                        row = std::vector<std::string>(schema->columns.size(), "");
                        schema->rows.push_back(row);
                    }
                    
                    // Set the sequence number
                    row[seqIndex] = std::to_string(i);
                }
            }
            
            generateRowsFromObject(objNode);
        }
    }
    // Handle array of scalars (junction table)
    else if (arrayNode->isArrayOfScalars() && !arrayNode->parentTable.empty()) {
        // Get the junction table name
        std::string tableName = getTableNameForArray(arrayNode->parentTable, arrayNode->parentKey);
        
        // Get the table schema
        auto tableIt = tables.find(tableName);
        if (tableIt == tables.end()) return;
        
        auto& schema = tableIt->second;
        
        // Generate a row for each scalar value
        for (size_t i = 0; i < arrayNode->elements.size(); ++i) {
            auto valueNode = std::dynamic_pointer_cast<ValueNode>(arrayNode->elements[i]);
            
            // Create a row: parent_id, index, value
            std::vector<std::string> row(3);
            row[0] = std::to_string(arrayNode->parentId);
            row[1] = std::to_string(i);
            row[2] = valueNode->toString();
            
            // Add the row to the table or stream it
            if (streamingMode) {
                writeTableRow(tableName, row);
            } else {
                schema->rows.push_back(row);
            }
        }
    }
    // Handle array of mixed types or nested arrays
    else {
        // Process each element in the array
        for (const auto& element : arrayNode->elements) {
            if (element->getType() == NodeType::OBJECT) {
                auto objNode = std::dynamic_pointer_cast<ObjectNode>(element);
                generateRowsFromObject(objNode);
            } else if (element->getType() == NodeType::ARRAY) {
                auto nestedArray = std::dynamic_pointer_cast<ArrayNode>(element);
                generateRowsFromArray(nestedArray);
            }
        }
    }
}