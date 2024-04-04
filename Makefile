compile:
	gcc main.c parser.c -o eshell
clean:
	rm -rf *.o eshell
run:
	make clean
	