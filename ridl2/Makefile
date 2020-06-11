CC = gcc
CFLAGS = -O2 -Wno-unused-result

all: attacker victim

attacker: attacker.c
	$(CC) -o $@ $< $(CFLAGS)

victim: victim.c
	$(CC) -o $@ $< $(CFLAGS)

clean:
	rm -rf attacker victim

