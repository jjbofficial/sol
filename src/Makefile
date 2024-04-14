CC = gcc
CFLAGS = -g
SOURCES := $(wildcard *.c)
objects = mqtt.o pack.o main.o

sol: $(objects)
	echo "We compiling the server"
	gcc -o $@ $^

$(objects): %.o : %.c
	$(CC) -c $(CFLAGS) $^ -o $@ 

clean:
	rm *.o sol
