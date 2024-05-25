CC = gcc
CFLAGS = -O2 -std=gnu2x -Wall -Wno-pointer-sign -Werror=vla -fsanitize=address
LD = gcc
LDFLAGS = -std=gnu2x -Wall -Wno-pointer-sign -Werror=vla -fsanitize=address
LDLIBS = -lcurl

all: smarthome

clean:
	rm -rf *.o smarthome

smarthome: b64_encoding.o hub.o crc8_encoding.o read_context.o
	$(LD) $(LDFLAGS) $(LDLIBS) $^ -o $@

hub.o: hub.c
	$(CC) $(CFLAGS) -fPIC -c $^ -o $@

crc8_encoding.o: crc8_encoding.c
	$(CC) $(CFLAGS) -fPIC -c $^ -o $@

b64_encoding.o: b64_encoding.c
	$(CC) $(CFLAGS) -fPIC -c $^ -o $@

read_context.o: read_context.c
	$(CC) $(CFLAGS) -fPIC -c $^ -o $@
