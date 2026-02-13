CC = gcc
CFLAGS = -Wall -Wextra -g
SRC = src/main.c src/lexer.c src/compiler.c src/vm.c
OBJ = $(SRC:.c=.o)
TARGET = opo

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lm -ldl -pthread $(shell pkg-config --libs libffi)

%.o: %.c
	$(CC) $(CFLAGS) $(shell pkg-config --cflags libffi) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
