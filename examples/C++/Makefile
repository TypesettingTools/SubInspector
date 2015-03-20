# Configuration
SHARED := 1
DEBUG := 0

# Programs
UNAME := $(shell uname -s)
CXX := clang++
AR  := ar

# Constants
WARNINGS :=-Wall -Wunreachable-code -Wfloat-equal -Wredundant-decls -Winit-self -Wpedantic
OPTIMIZATION :=-O3
ADDITIONAL :=-std=c++11 -stdlib=libc++
LIBS :=-L../.. -lSubInspector
CXXFLAGS := $(WARNINGS) $(OPTIMIZATIONS) $(DEFINES) $(ADDITIONAL)
LFLAGS := $(LIBS)
NAME := SI_example

# Modifications
ifeq ($(DEBUG),1)
	CXXFLAGS += -O0 -DDEBUG -g
endif
ifeq ($(OS),Windows_NT)
	$LIB_EXT := .exe
endif
LIB_FULLNAME := $(NAME)$(LIB_EXT)

# File shortcuts
SOURCES := $(wildcard *.cpp)
OBJECTS := $(SOURCES:.cpp=.o)

# Build targets
.PHONY: all clean debug

all: $(LIB_FULLNAME)

debug: CXXFLAGS += -O0 -DDEBUG -g
debug: all

$(LIB_FULLNAME): $(OBJECTS)
	@echo LINK $@
	@$(CXX) $^ $(LFLAGS) -o $@

%.o: %.cpp
	@echo CXX $@
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(LIB_FULLNAME) $(OBJECTS)
