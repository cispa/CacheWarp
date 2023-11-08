all: amd

CFLAG= $(INCLUDE_DIRS)  -Wpedantic -Wall -g3

LDFLAGS= $(LIBRARY_DIRS) -ldl -lpthread -Wl,-rpath=.

amd: main.c
	gcc $(INCLUDE_DIRS) main.c $(LDFLAGS) -o hist

clean:
	rm hist 
