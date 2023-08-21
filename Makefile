TARGET       := e1000-test
CC           := gcc
CFLAGS       := -I include
CFLAGS       := $(CFLAGS) -g # -Wall -Werror
LD_FLAGS     := -lpthread
OBJ_PATH     := obj
SRC_PATH     := src
INCLUDE_PATH := include
OBJS         := $(OBJ_PATH)/main.o      \
                $(OBJ_PATH)/e1000.o     \
                $(OBJ_PATH)/mem_alloc.o \

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LD_FLAGS)

$(OBJ_PATH)/%.o: $(SRC_PATH)/%.c $(OBJ_PATH)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_PATH): 
	@mkdir -p $(OBJ_PATH)

.PHONY: clean
clean:
	rm -rf $(OBJ_PATH) $(TARGET)

# `bear -- make` to generate compile_commands.json