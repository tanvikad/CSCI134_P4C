all: lab4b

lab4b: lab4b.c
	gcc -Wall -Wextra -g  lab4b.c -o lab4b -lrobotcontrol -lpthread -lm

clean:
	rm -f *.o
	rm -f lab4b
	rm -f test_buttons
	rm -f *.gz
	rm -f *.txt

dist: 
	tar -zcvf lab4b-40205638.tar.gz lab4b.c README smoke_test.sh Makefile

check:
	./smoke_test.sh