# Compiles tokenizer.cxx + runs for time being
CXX = g++

# Compiler flags
# -o: optimized
# -g: debugging purposes
# -Wall: turns on most compiler warnings
# -std=c++17: use c++17 compatible version of compiler
CXX_FLAGS = -g -Wall -std=c++17 -l sqlite3 -I /usr/local/include


CXX_FILES = ${wildcard *.cxx}
LPP_FILES = ${sort ${wildcard *.lpp}}

COMPILE_FILES = context.cxx parser.cxx scope.cxx ast.cxx tokenizer.cxx error.cxx writer.cxx diagram.cxx
DEBUG_FILES = debugger.cxx parser.cxx scope.cxx ast.cxx tokenizer.cxx error.cxx diagram.cxx

# ****************************************************
# Targets needed to bring the executable up to date

# # tokenizer requires 'tokenizer.o'
# tokenizer: tokenizer.o 
# 	$(CXX) $(CXX_FLAGS) tokenizer tokenizer.o 

# The tokenizer.o target can be written more simply
# tokenizer.o requires 'tokenizer.cxx' and 'tokenizer.h'

tokenizer: tokenizer.o
	./tokenizer $(file)

tokenizer.o: tokenizer.cxx tokenizer.h
	$(CXX) $(CXX_FLAGS) tokenizer.cxx -o tokenizer

parser: parser.o
	./parser $(file)

parser.o: parser.cxx parser.h
	$(CXX) $(CXX_FLAGS) $(COMPILE_FILES) -o parser

ingalls: parser.o
	./parser "../../ingalls/lpp/$(file)"

ingalls.o: parser.cxx parser.h
	$(CXX) $(CXX_FLAGS) $(COMPILE_FILES) -o parser

context: context.o
	./context $(file)

context.o: context.cxx context.h
	$(CXX) $(CXX_FLAGS) $(COMPILE_FILES) -o context

debug: debug.o
	./debug $(mode) $(file)
debug.o : debugger.cxx debugger.h
	$(CXX) $(CXX_FLAGS) $(DEBUG_FILES) -o debug

clean:
	rm -rf tokenizer parser context debug diagram *.tokens *.dSYM ../../ingalls/lpp/*.tokens lcc_out/


