%{
#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <cstdlib>
#include "ast.h"

// Forward declarations
extern int yylex();
extern int yyparse();
extern FILE *yyin;
extern int lineno;
extern int column;
void reset_scanner();
void yyerror(const char *s);

// Global AST object
AST ast;

// For printing the AST
extern bool print_ast;

// Error recovery flag
bool has_syntax_error = false;

%}

%union {
    std::string* string_val;
    bool bool_val;
    AstNode* node_val;
    ObjectNode* object_val;
    ArrayNode* array_val;
    KeyValuePair* pair_val;
    std::vector<KeyValuePair>* pairs_val;
    std::vector<std::shared_ptr<AstNode>>* elements_val;
}

%token <string_val> STRING NUMBER
%token <bool_val> TRUE FALSE
%token NUL ERROR

%type <node_val> value
%type <object_val> object
%type <array_val> array
%type <pair_val> pair
%type <pairs_val> pairs
%type <elements_val> elements

%start json

%%

json: value {
    ast.setRoot(std::shared_ptr<AstNode>($1));
    if (print_ast) {
        ast.print(std::cout);
    }
}
| error {
    has_syntax_error = true;
    std::cerr << "JSON syntax error, attempting to recover..." << std::endl;
    // Create a minimal valid AST
    ast.setRoot(std::make_shared<ObjectNode>());
    YYABORT;
}
;

value: object    { $$ = $1; }
    | array      { $$ = $1; }
    | STRING     { $$ = new StringNode(*$1); delete $1; }
    | NUMBER     { $$ = new NumberNode(*$1); delete $1; }
    | TRUE       { $$ = new BooleanNode(true); }
    | FALSE      { $$ = new BooleanNode(false); }
    | NUL        { $$ = new NullNode(); }
    | error      { 
        $$ = new NullNode(); 
        has_syntax_error = true;
        std::cerr << "Syntax error in value at line " << lineno << ", column " << column << std::endl;
    }
;

object: '{' pairs '}' {
    $$ = new ObjectNode();
    if ($2) {
        $$->pairs = std::move(*$2);
        delete $2;
    }
}
    | '{' '}' {
    $$ = new ObjectNode();
}
    | '{' pairs error {
    $$ = new ObjectNode();
    if ($2) {
        $$->pairs = std::move(*$2);
        delete $2;
    }
    has_syntax_error = true;
    std::cerr << "Missing closing brace in object definition at line " << lineno << std::endl;
}
;

pairs: pair {
    $$ = new std::vector<KeyValuePair>();
    $$->push_back(std::move(*$1));
    delete $1;
}
    | pairs ',' pair {
    $$ = $1;
    $$->push_back(std::move(*$3));
    delete $3;
}
    | pairs error pair {
    $$ = $1;
    $$->push_back(std::move(*$3));
    delete $3;
    has_syntax_error = true;
    std::cerr << "Missing comma in object definition at line " << lineno << std::endl;
}
;

pair: STRING ':' value {
    $$ = new KeyValuePair(*$1, std::shared_ptr<AstNode>($3));
    delete $1;
}
    | STRING error value {
    $$ = new KeyValuePair(*$1, std::shared_ptr<AstNode>($3));
    delete $1;
    has_syntax_error = true;
    std::cerr << "Missing colon in key-value pair at line " << lineno << std::endl;
}
;

array: '[' elements ']' {
    $$ = new ArrayNode();
    if ($2) {
        $$->elements = std::move(*$2);
        delete $2;
    }
}
    | '[' ']' {
    $$ = new ArrayNode();
}
    | '[' elements error {
    $$ = new ArrayNode();
    if ($2) {
        $$->elements = std::move(*$2);
        delete $2;
    }
    has_syntax_error = true;
    std::cerr << "Missing closing bracket in array definition at line " << lineno << std::endl;
}
;

elements: value {
    $$ = new std::vector<std::shared_ptr<AstNode>>();
    $$->push_back(std::shared_ptr<AstNode>($1));
}
    | elements ',' value {
    $$ = $1;
    $$->push_back(std::shared_ptr<AstNode>($3));
}
    | elements error value {
    $$ = $1;
    $$->push_back(std::shared_ptr<AstNode>($3));
    has_syntax_error = true;
    std::cerr << "Missing comma in array elements at line " << lineno << std::endl;
}
;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Error: %s at line %d, column %d\n", s, lineno, column);
    has_syntax_error = true;
}