FLAGS=-std=c99 -Wall
all:
	$(CC) $(FLAGS) jack_client.c -o jack_client -lm -ljack
run:
	jackd -P80 -p16 -t2000 -d alsa -d hw:0 -p 1024 -n 2 -r 44100 -s &