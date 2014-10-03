CC      := clang
CFLAGS  := -Wall -ggdb -std=c99 -D_XOPEN_SOURCE=600
LDFLAGS := -shared -fPIC -lass -ggdb

UNAME   := $(shell uname -s)

ifeq ($(OS), Windows_NT)
	TARGET  := libOptimASS.dll
else
	ifeq ($(UNAME), Darwin)
		TARGET  := libOptimASS.dylib
	else
		# Do any other operating systems use weird extensions for their
		# dynamic libraries?
		TARGET  := libOptimASS.so
	endif
endif

SOURCES := $(wildcard src/*.c)
OBJECTS := $(SOURCES:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo LINK $@
	@$(CC) $^ $(LDFLAGS) -o $@

%.o: %.c
	@echo CC $@
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(TARGET) $(OBJECTS)
