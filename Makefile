CC = gcc
CFLAGS = -Wall -Wextra -I./ -DBIN_COUNT=200 -DOUTLIER_COUNT=2 -O0 -g
LDFLAGS = -lm

HDR = histogram.h logger.h runtime.h
SRC = main.c histogram.c histogram.h 
OBJ = main.o histogram.o runtime.o
LIB = libhistogram.so
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

