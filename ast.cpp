#include "ast.h"
#include <algorithm>
#include <sstream>
#include <set>
#include <iomanip>

// Utility function to print indentation
static void printIndent(std::ostream& os, int indent) {
    for (int i = 0; i < indent; ++i) {
        os << "  ";
    }
}

// Print object node
void ObjectNode::print(std::ostream& os, int indent) const {
    printIndent(os, indent);
    os << "OBJECT";
    if (!tableName.empty()) {
        os << " (Table: " << tableName << ", ID: " << id << ")";
    }
    os << " {\n";
    
    for (const auto& pair : pairs) {
        printIndent(os, indent + 1);
        os << "\"" << pair.key << "\": ";
        pair.value->print(os, indent + 1);
        os << "\n";
    }
    
    printIndent(os, indent);
    os << "}";
}

// Print array node
void ArrayNode::print(std::ostream& os, int indent) const {
    printIndent(os, indent);
    os << "ARRAY";
    if (!parentKey.empty()) {
        os << " (Key: " << parentKey << ")";
    }
    os << " [\n";
    
    for (size_t i = 0; i < elements.size(); ++i) {
        printIndent(os, indent + 1);
        os << "[" << i << "]: ";
        elements[i]->print(os, indent + 1);
        os << "\n";
    }
    
    printIndent(os, indent);
    os << "]";
}

// Print string node
void StringNode::print(std::ostream& os, int indent) const {
    os << "STRING \"" << value << "\"";
}

// Print number node
void NumberNode::print(std::ostream& os, int indent) const {
    os << "NUMBER " << value;
}

// Print boolean node
void BooleanNode::print(std::ostream& os, int indent) const {
    os << "BOOLEAN " << (value ? "true" : "false");
}

// Print null node
void NullNode::print(std::ostream& os, int indent) const {
    os << "NULL";
}

// Print the entire AST
void AST::print(std::ostream& os) const {
    if (root) {
        root->print(os);
        os << std::endl;
    } else {
        os << "Empty AST" << std::endl;
    }
}

// Get a signature of object keys for table identification
std::string ObjectNode::getKeySignature() const {
    std::vector<std::string> keys;
    for (const auto& pair : pairs) {
        keys.push_back(pair.key);
    }
    
    std::sort(keys.begin(), keys.end());
    
    std::stringstream ss;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i > 0) ss << ",";
        ss << keys[i];
    }
    
    return ss.str();
}

// Check if this is an array of objects with the same structure
bool ArrayNode::isArrayOfObjects() const {
    if (elements.empty()) return false;
    
    // Check if all elements are objects
    for (const auto& element : elements) {
        if (element->getType() != NodeType::OBJECT) {
            return false;
        }
    }
    
    // Check if all objects have the same structure
    if (elements.size() > 1) {
        auto firstObject = std::dynamic_pointer_cast<ObjectNode>(elements[0]);
        std::string signature = firstObject->getKeySignature();
        
        for (size_t i = 1; i < elements.size(); ++i) {
            auto obj = std::dynamic_pointer_cast<ObjectNode>(elements[i]);
            if (obj->getKeySignature() != signature) {
                return false;
            }
        }
    }
    
    return true;
}

// Check if this is an array of scalar values
bool ArrayNode::isArrayOfScalars() const {
    if (elements.empty()) return false;
    
    // Check if all elements are scalar values (string, number, boolean, or null)
    for (const auto& element : elements) {
        NodeType type = element->getType();
        if (type != NodeType::STRING && type != NodeType::NUMBER && 
            type != NodeType::BOOLEAN && type != NodeType::NULL_VALUE) {
            return false;
        }
    }
    
    return true;
}

// Get the signature of objects if this is an array of objects
std::string ArrayNode::getObjectSignature() const {
    if (!isArrayOfObjects() || elements.empty()) {
        return "";
    }
    
    auto firstObject = std::dynamic_pointer_cast<ObjectNode>(elements[0]);
    return firstObject->getKeySignature();
}

// Assign IDs to all nodes in the AST
void AST::assignIds() {
    if (!root) return;
    
    std::map<std::string, int> tableIds;
    root->assignIds(1, tableIds);
}

// Assign IDs to object nodes
int ObjectNode::assignIds(int nextId, std::map<std::string, int>& tableIds) {
    // Determine table name from parent key or use default
    if (parentTable.empty()) {
        tableName = "root"; // Root object
    } else if (!parentKey.empty()) {
        tableName = parentKey; // Use the key from parent
    } else {
        std::stringstream ss;
        ss << parentTable << "_" << nextId; // Fallback name
        tableName = ss.str();
    }
    
    // Assign ID
    id = nextId++;
    
    // Process all key-value pairs
    for (auto& pair : pairs) {
        if (pair.value->getType() == NodeType::OBJECT) {
            // Handle nested object
            auto objectNode = std::dynamic_pointer_cast<ObjectNode>(pair.value);
            objectNode->parentId = id;
            objectNode->parentTable = tableName;
            objectNode->parentKey = pair.key;
            nextId = objectNode->assignIds(nextId, tableIds);
        } 
        else if (pair.value->getType() == NodeType::ARRAY) {
            // Handle array
            auto arrayNode = std::dynamic_pointer_cast<ArrayNode>(pair.value);
            arrayNode->parentId = id;
            arrayNode->parentTable = tableName;
            arrayNode->parentKey = pair.key;
            nextId = arrayNode->assignIds(nextId, tableIds);
        }
    }
    
    return nextId;
}

// Assign IDs to array nodes
int ArrayNode::assignIds(int nextId, std::map<std::string, int>& tableIds) {
    // If array contains objects, we need to process them
    if (isArrayOfObjects()) {
        // Process each object element
        int index = 0;
        for (auto& element : elements) {
            if (element->getType() == NodeType::OBJECT) {
                auto objectNode = std::dynamic_pointer_cast<ObjectNode>(element);
                objectNode->parentId = parentId;
                objectNode->parentTable = parentTable;
                
                // If no parent key is set, use a numeric index
                if (parentKey.empty()) {
                    std::stringstream ss;
                    ss << "item_" << index;
                    objectNode->parentKey = ss.str();
                } else {
                    objectNode->parentKey = parentKey + "_" + std::to_string(index);
                }
                
                nextId = objectNode->assignIds(nextId, tableIds);
            }
            index++;
        }
    }
    
    // For scalar arrays, we don't need to assign IDs to the elements
    // They will be handled by the CSV generator
    
    return nextId;
}

// These nodes don't need to assign IDs
int StringNode::assignIds(int nextId, std::map<std::string, int>&) { return nextId; }
int NumberNode::assignIds(int nextId, std::map<std::string, int>&) { return nextId; }
int BooleanNode::assignIds(int nextId, std::map<std::string, int>&) { return nextId; }
int NullNode::assignIds(int nextId, std::map<std::string, int>&) { return nextId; }