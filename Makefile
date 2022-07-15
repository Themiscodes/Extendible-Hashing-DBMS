# paths
LIB = lib
INCLUDE = include
SRC = src
BUILD = build

# compiler
CC = gcc

# compile options
CFLAGS = -Wall -Werror -g -I$(INCLUDE)
LDFLAGS = -lm

# object files .o
OBJS = examples/sht_main.o $(SRC)/hash_file.o $(SRC)/sht_file.o $(LIB)/libbf.so

# executable
EXEC = test

# arguments
ARGS =

$(EXEC): $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)
	@if [ -f $(EXEC).exe ]; then ln -sf $(EXEC).exe $(EXEC); fi

clean:
	rm -f *.db *.o $(SRC)/hash_file.o $(SRC)/sht_file.o examples/sht_main.o $(BUILD)/runner $(EXEC)

run: $(EXEC)
	./$(EXEC) $(ARGS)