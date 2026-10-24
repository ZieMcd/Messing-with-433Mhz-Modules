build-run:
	gcc -Wall -o capture433 capture433.c -llgpio
	./capture433
