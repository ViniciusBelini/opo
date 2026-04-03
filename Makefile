CC = gcc
CFLAGS = -Wall -Wextra -g
SRC = src/main.c src/lexer.c src/compiler.c src/vm.c
OBJ = $(SRC:.c=.o)
TARGET = opo

LSP_SRC = src/lsp.c src/lexer.c src/compiler.c src/vm.c
LSP_OBJ = src/lsp.o src/lexer.o src/compiler.o src/vm.o
LSP_TARGET = opolsp

all: $(TARGET) $(LSP_TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lm -ldl -pthread $(shell pkg-config --libs libffi)

$(LSP_TARGET): $(LSP_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lm -ldl -pthread $(shell pkg-config --libs libffi)

%.o: %.c
	$(CC) $(CFLAGS) $(shell pkg-config --cflags libffi) -c $< -o $@

clean:
	rm -f $(OBJ) src/lsp.o $(TARGET) $(LSP_TARGET)
