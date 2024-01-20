CC = gcc
CFLAGS = -Wall -Wextra -I/usr/include/json-c -I./
LDFLAGS = -ljson-c -lm

HDR = distribution.h logger.h runtime.h
SRC = main.c distribution.c distribution.h 
OBJ = main.o distribution.o runtime.o
LIB = libdistribution.so
EXEC = histog

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(LIB): $(OBJ)

%.o: %.c $(HDR)
	$(CC) $(CFLAGS) -fPIC -c $<

clean:
	rm -f $(OBJ) $(EXEC) $(LIB)

.PHONY: all clean

