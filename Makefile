# Lab 5 build rules for the mini-computer simulator.
CC=gcc
CFLAGS=-Wall -Wextra -std=c11
TARGET=minicomputer.exe
OBJS=main.o os.o processor.o memory.o compiler.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c os.h
	$(CC) $(CFLAGS) -c main.c

os.o: os.c os.h compiler.h processor.h memory.h
	$(CC) $(CFLAGS) -c os.c

processor.o: processor.c processor.h memory.h
	$(CC) $(CFLAGS) -c processor.c

memory.o: memory.c memory.h
	$(CC) $(CFLAGS) -c memory.c

compiler.o: compiler.c compiler.h
	$(CC) $(CFLAGS) -c compiler.c

clean:
	rm -f $(OBJS) $(TARGET)
