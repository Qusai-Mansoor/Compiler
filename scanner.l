%{
#include <string>
#include <cstdlib>
#include "ast.h"
#include "parser.tab.h"

// Track line and column for error reporting
int lineno = 1;
int column = 1;
int prev_column = 1;

// Update column position
void update_column() {
    prev_column = column;
    column += yyleng;
}

// Update line and reset column
void update_line() {
    lineno++;
    prev_column = column;
    column = 1;
}

// Helper function to handle strings with escapes
std::string process_string() {
    std::string result;
    // Skip the opening quote
    const char* start = yytext + 1;
    // Skip the closing quote
    const char* end = yytext + yyleng - 1;
    
    for (const char* p = start; p < end; ++p) {
        if (*p == '\\') {
            // Handle escape sequences
            ++p;
            if (p >= end) break;
            
            switch (*p) {
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                case '/': result += '/'; break;
                case 'u': {
                    // Handle Unicode escape sequences
                    if (p + 4 >= end) {
                        result += '?'; // Invalid unicode sequence
                    } else {
                        char hex[5] = {p[1], p[2], p[3], p[4], 0};
                        int code = strtol(hex, nullptr, 16);
                        // Convert UTF-16 to UTF-8
                        if (code <= 0x7F) {
                            result += static_cast<char>(code);
                        } else if (code <= 0x7FF) {
                            result += static_cast<char>(0xC0 | (code >> 6));
                            result += static_cast<char>(0x80 | (code & 0x3F));
                        } else {
                            result += static_cast<char>(0xE0 | (code >> 12));
                            result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            result += static_cast<char>(0x80 | (code & 0x3F));
                        }
                        p += 4; // Skip the hex digits
                    }
                    break;
                }
                default: result += *p; break;
            }
        } else {
            result += *p;
        }
    }
    
    return result;
}
%}

%option noyywrap
%option never-interactive

DIGIT       [0-9]
INT         -?{DIGIT}+
FRAC        \.{DIGIT}+
EXP         [eE][+-]?{DIGIT}+
NUMBER      {INT}{FRAC}?{EXP}?

%%

[\t ]       { update_column(); }
[\r]        { update_column(); }
[\n]        { update_line(); }

"{"         { update_column(); return '{'; }
"}"         { update_column(); return '}'; }
"["         { update_column(); return '['; }
"]"         { update_column(); return ']'; }
":"         { update_column(); return ':'; }
","         { update_column(); return ','; }

\"(\\.|[^"\\])*\"    { 
    update_column(); 
    yylval.string_val = new std::string(process_string());
    return STRING;
}

{NUMBER}    { 
    update_column(); 
    yylval.string_val = new std::string(yytext);
    return NUMBER;
}

"true"      { update_column(); yylval.bool_val = true; return TRUE; }
"false"     { update_column(); yylval.bool_val = false; return FALSE; }
"null"      { update_column(); return NUL; }

.           {
    update_column();
    fprintf(stderr, "Error: Unexpected character '%c' at line %d, column %d\n", 
            yytext[0], lineno, prev_column);
    return ERROR;
}

%%

// Reset scanner state for error recovery
void reset_scanner() {
    YY_FLUSH_BUFFER;
}