#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>

// Forward declarations
class AstNode;
class ObjectNode;
class ArrayNode;
class ValueNode;
class StringNode;
class NumberNode;
class BooleanNode;
class NullNode;

// AST node types
enum class NodeType {
    OBJECT,
    ARRAY,
    STRING,
    NUMBER,
    BOOLEAN,
    NULL_VALUE
};

// Base class for all AST nodes
class AstNode {
public:
    virtual ~AstNode() = default;
    virtual NodeType getType() const = 0;
    virtual void print(std::ostream& os, int indent = 0) const = 0;
    
    // For table generation
    virtual int assignIds(int nextId, std::map<std::string, int>& tableIds) = 0;
};

// Class for key-value pair in an object
class KeyValuePair {
public:
    std::string key;
    std::shared_ptr<AstNode> value;
    
    KeyValuePair(const std::string& k, std::shared_ptr<AstNode> v) 
        : key(k), value(std::move(v)) {}
};

// Class for JSON object node
class ObjectNode : public AstNode {
public:
    std::vector<KeyValuePair> pairs;
    std::string tableName;  // Derived table name
    int id = -1;           // Assigned row id
    int parentId = -1;     // Parent object's id (for foreign key)
    std::string parentTable; // Parent table name
    std::string parentKey;   // The key in parent object that points to this object
    int arrayIndex = -1;    // Array index if this is an array element
    
    NodeType getType() const override { return NodeType::OBJECT; }
    void print(std::ostream& os, int indent = 0) const override;
    int assignIds(int nextId, std::map<std::string, int>& tableIds) override;
    
    // Helper to get a signature of keys (for table identification)
    std::string getKeySignature() const;
};

// Class for JSON array node
class ArrayNode : public AstNode {
public:
    std::vector<std::shared_ptr<AstNode>> elements;
    std::string parentKey;  // Key in parent object (if any)
    int parentId = -1;     // Parent object's id
    std::string parentTable; // Parent table name
    
    NodeType getType() const override { return NodeType::ARRAY; }
    void print(std::ostream& os, int indent = 0) const override;
    int assignIds(int nextId, std::map<std::string, int>& tableIds) override;
    
    // Check if this is an array of objects with the same structure
    bool isArrayOfObjects() const;
    
    // Check if this is an array of scalar values
    bool isArrayOfScalars() const;
    
    // Get the signature of objects if this is an array of objects
    std::string getObjectSignature() const;
};

// Base class for all value nodes
class ValueNode : public AstNode {
public:
    virtual std::string toString() const = 0;
};

// Class for string value
class StringNode : public ValueNode {
public:
    std::string value;
    
    explicit StringNode(std::string v) : value(std::move(v)) {}
    
    NodeType getType() const override { return NodeType::STRING; }
    void print(std::ostream& os, int indent = 0) const override;
    int assignIds(int nextId, std::map<std::string, int>& tableIds) override;
    std::string toString() const override { return value; }
};

// Class for number value
class NumberNode : public ValueNode {
public:
    std::string value;  // Keep as string to preserve precision
    
    explicit NumberNode(std::string v) : value(std::move(v)) {}
    
    NodeType getType() const override { return NodeType::NUMBER; }
    void print(std::ostream& os, int indent = 0) const override;
    int assignIds(int nextId, std::map<std::string, int>& tableIds) override;
    std::string toString() const override { return value; }
};

// Class for boolean value
class BooleanNode : public ValueNode {
public:
    bool value;
    
    explicit BooleanNode(bool v) : value(v) {}
    
    NodeType getType() const override { return NodeType::BOOLEAN; }
    void print(std::ostream& os, int indent = 0) const override;
    int assignIds(int nextId, std::map<std::string, int>& tableIds) override;
    std::string toString() const override { return value ? "true" : "false"; }
};

// Class for null value
class NullNode : public ValueNode {
public:
    NodeType getType() const override { return NodeType::NULL_VALUE; }
    void print(std::ostream& os, int indent = 0) const override;
    int assignIds(int nextId, std::map<std::string, int>& tableIds) override;
    std::string toString() const override { return ""; }
};

// Main AST class 
class AST {
private:
    std::shared_ptr<AstNode> root;
    
public:
    AST() = default;
    
    void setRoot(std::shared_ptr<AstNode> node) {
        root = std::move(node);
    }
    
    std::shared_ptr<AstNode> getRoot() const {
        return root;
    }
    
    void print(std::ostream& os) const;
    
    // Assign IDs to all nodes in the AST
    void assignIds();
};

#endif // AST_H