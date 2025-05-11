#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <cstring>
#include "ast.h"
#include "csv_generator.h"

// External declarations from parser
extern AST ast;
extern int yyparse();
extern FILE *yyin;
extern void reset_scanner();

// Flag for printing AST
bool print_ast = false;

void print_usage() {
    std::cerr << "Usage: json2relcsv [--print-ast] [--out-dir DIR]" << std::endl;
}

int main(int argc, char** argv) {
    std::string out_dir = ".";
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--print-ast") == 0) {
            print_ast = true;
        } else if (strcmp(argv[i], "--out-dir") == 0) {
            if (i + 1 < argc) {
                out_dir = argv[++i];
            } else {
                std::cerr << "Error: --out-dir requires a directory path" << std::endl;
                print_usage();
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown argument: " << argv[i] << std::endl;
            print_usage();
            return 1;
        }
    }
    
    // Set input source to stdin
    yyin = stdin;
    
    // Parse the input
    int parse_result = yyparse();
    
    // Check for parsing errors
    if (parse_result != 0) {
        return 1;
    }
    
    // Assign IDs to AST nodes
    ast.assignIds();
    
    // Generate CSV files
    CSVGenerator generator(out_dir);
    generator.generateCSV(ast);
    
    return 0;
}