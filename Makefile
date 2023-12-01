all: lab4c

lab4c: lab4c.c
	gcc -Wall -Wextra -g  lab4c.c -o lab4c -lrobotcontrol -lpthread -lm

clean:
	rm -f *.o
	rm -f lab4b
	rm -f test_buttons
	rm -f *.gz
	rm -f *.txt

dist: 
	tar -zcvf lab4c-40205638.tar.gz lab4c.c README smoke_test.sh Makefile

check:
	./smoke_test.sh