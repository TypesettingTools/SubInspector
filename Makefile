CC      := clang
CFLAGS  := -Wall -ggdb -std=c99 -D_XOPEN_SOURCE=600
LDFLAGS := -shared -fPIC -lass -ggdb

UNAME   := $(shell uname -s)
ifeq ($(UNAME), Darwin)
	TARGET  := libOptimASS.dylib
# I am aware windows does not have uname. Don't build this on windows.
# Cross compile from lunix.
else ifeq ($(UNAME), Windows)
	TARGET  := libOptimASS.dll
# Do any other operating systems use weird extensions for their dynamic
# libraries?
else
	TARGET  := libOptimASS.so
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
