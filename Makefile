CC = gcc
CFLAGS = -I /mingw64/include
LDFLAGS = -L /mingw64/lib
LDLIBS = -l SDL2

tetris: tetris.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

tetris.o: tetris.c
	$(CC) -c $(CFLAGS) $<

clean:
	rm -f *.o tetris