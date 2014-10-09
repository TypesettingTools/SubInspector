# Configuration
SHARED := 1
DEBUG := 0

# Programs
UNAME := $(shell uname -s)
CC := clang
AR := ar

# Constants
WARNINGS := -Wall -Wunreachable-code -Wfloat-equal -Wredundant-decls -Winit-self -Wpedantic
OPTIMIZATION := -O3
ADDITIONAL := -std=c99
LIBS := -lass
CFLAGS := $(WARNINGS) $(OPTIMIZATIONS) $(DEFINES) $(ADDITIONAL)
LFLAGS := $(LIBS)
LIB_NAME := libASSInspector

# Modifications
ifeq ($(DEBUG),1)
	CFLAGS += -DDEBUG -g
else
	LFLAGS += -s
endif
ifeq ($(OS),Windows_NT)
	CFLAGS += -D_WIN32
	ifeq ($(SHARED),1)
		CFLAGS += -DBUILD_DLL
		LIB_EXT :=.dll
	else
		CFLAGS += -DASS_INSPECTOR_STATIC
		LIB_EXT :=.a
	endif
else
	ifeq ($(SHARED),1)
		CFLAGS += -fPIC
		ifeq ($(UNAME),Darwin)
			LIB_EXT :=.dylib
		else
			LIB_EXT :=.so
		endif
	else
		LIB_EXT :=.a
	endif
endif
ifeq ($(SHARED),1)
	LFLAGS += -shared
endif
LIB_FULLNAME := $(LIB_NAME)$(LIB_EXT)

# File shortcuts
SOURCES := $(wildcard src/*.c)
OBJECTS := $(SOURCES:.c=.o)

# Build targets
.PHONY: all clean

all: $(LIB_FULLNAME)

$(LIB_FULLNAME): $(OBJECTS)
ifeq ($(SHARED),1)
	@echo LINK $@
	@$(CC) $^ $(LFLAGS) -o $@
else
	@echo ARCHIVE $@
	@$(AR) rcs $@ $^
endif

%.o: %.c
	@echo CC $@
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(LIB_FULLNAME) $(OBJECTS)