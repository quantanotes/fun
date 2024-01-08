CC = gcc -std=c2x
CFLAGS = -O2 -Wall -Wextra -Wno-ignored-attributes -Iinclude 
LDFLAGS = -Llib
SRC_DIR = .
BIN_DIR = bin

TARGET = $(BIN_DIR)/fun.exe

SRC_FILES = $(wildcard $(SRC_DIR)/*.c)

OBJ_FILES = $(patsubst $(SRC_DIR)/%.c, $(BIN_DIR)/%.o, $(SRC_FILES))

$(TARGET): $(OBJ_FILES)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c $< -o $@ $(CFLAGS)

.PHONY: clean

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(BIN_DIR)/*.o $(TARGET)

debug:
	gdb ${TARGET}
