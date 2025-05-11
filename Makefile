CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g
LEX = flex
YACC = bison

# Add -lfl if needed for flex library
LDFLAGS = -lstdc++fs

all: json2relcsv

json2relcsv: lex.yy.o parser.tab.o ast.o csv_generator.o main.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

lex.yy.c: scanner.l parser.tab.h
	$(LEX) -o $@ $<

parser.tab.c parser.tab.h: parser.y
	$(YACC) -d -o parser.tab.c $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

lex.yy.o: lex.yy.c
	$(CXX) $(CXXFLAGS) -c $< -o $@

parser.tab.o: parser.tab.c
	$(CXX) $(CXXFLAGS) -c $< -o $@

ast.o: ast.cpp ast.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

csv_generator.o: csv_generator.cpp csv_generator.h ast.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

main.o: main.cpp ast.h csv_generator.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o lex.yy.c parser.tab.c parser.tab.h json2relcsv

.PHONY: all clean