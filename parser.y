%{
#include <iostream>
#include <string>
#include <memory>
#include <vector>
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
;

value: object    { $$ = $1; }
    | array      { $$ = $1; }
    | STRING     { $$ = new StringNode(*$1); delete $1; }
    | NUMBER     { $$ = new NumberNode(*$1); delete $1; }
    | TRUE       { $$ = new BooleanNode(true); }
    | FALSE      { $$ = new BooleanNode(false); }
    | NUL        { $$ = new NullNode(); }
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
;

pair: STRING ':' value {
    $$ = new KeyValuePair(*$1, std::shared_ptr<AstNode>($3));
    delete $1;
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
;

elements: value {
    $$ = new std::vector<std::shared_ptr<AstNode>>();
    $$->push_back(std::shared_ptr<AstNode>($1));
}
    | elements ',' value {
    $$ = $1;
    $$->push_back(std::shared_ptr<AstNode>($3));
}
;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Error: %s at line %d, column %d\n", s, lineno, column);
}