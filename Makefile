all: lab4c_tcp lab4c_tls


lab4c_tls: lab4c_tls.c
	gcc -Wall -Wextra -g  lab4c_tls.c -o lab4c_tls -lrobotcontrol -lpthread -lm -lssl -lcrypto


lab4c_tcp: lab4c_tcp.c
	gcc -Wall -Wextra -g  lab4c_tcp.c -o lab4c_tcp -lrobotcontrol -lpthread -lm

clean:
	rm -f *.o
	rm -f lab4c_tcp
	rm -f lab4c_tls
	rm -f *.gz
	rm -f *.txt

dist: 
	tar -zcvf lab4c-40205638.tar.gz lab4c_tcp.c README Makefile
