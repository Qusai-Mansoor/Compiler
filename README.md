# JSON to CSV Converter - Implementation Plan

## Project Structure
```
├── scanner.l         # Flex specification for tokenizing JSON
├── parser.y          # Yacc/Bison grammar for parsing JSON
├── ast.h             # AST node definitions
├── ast.cpp           # AST implementation
├── csv_generator.h   # CSV output generation
├── csv_generator.cpp # CSV file creation and writing
├── main.cpp          # Command-line handling and main function
├── Makefile          # Build instructions
└── README.md         # Documentation and usage instructions
```

## Implementation Steps

1. **Lexical Analysis (scanner.l)**
   - Define tokens for JSON elements (braces, brackets, commas, colons)
   - Handle strings, numbers, true/false/null
   - Track line and column numbers for error reporting
   - Handle Unicode escape sequences

2. **Parsing (parser.y)**
   - Define grammar rules for JSON
   - Build AST nodes during parsing
   - Connect nodes to form a complete AST
   - Report syntax errors with location information

3. **AST Implementation (ast.h/cpp)**
   - Define node types for JSON elements (object, array, string, number, boolean, null)
   - Implement AST traversal functions
   - Implement AST printing functionality
   - Memory management for the AST

4. **Schema Analysis**
   - Analyze AST to identify tables and relationships
   - Group objects with same keys into tables
   - Handle nested objects and arrays
   - Assign primary and foreign keys

5. **CSV Generation (csv_generator.h/cpp)**
   - Create table schemas based on JSON structure
   - Generate CSV headers
   - Stream data rows to CSV files
   - Handle quoting and escaping

6. **Error Handling**
   - Report lexical and syntax errors with location
   - Handle file I/O errors
   - Clean up resources on error

7. **Main Program**
   - Parse command-line arguments
   - Coordinate lexing, parsing, and output generation
   - Handle the `--print-ast` and `--out-dir` options
