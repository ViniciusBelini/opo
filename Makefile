CC = gcc
CFLAGS = -Wall -Wextra -g
SRC = src/main.c src/lexer.c src/compiler.c src/vm.c
OBJ = $(SRC:.c=.o)
TARGET = opo

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
