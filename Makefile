# Protocoale de comunicatii
# Laborator 7 - TCP
# Echo Server
# Makefile

CFLAGS = -Wall -g -Werror -Wno-error=unused-variable

# Portul pe care asculta serverul
# PORT = 8080

# Adresa IP a serverului
IP_SERVER = 127.0.0.1

all: server subscriber

common.o: common.c

# Compileaza server.c
server: server.c common.o

# Compileaza subscriber.c
subscriber: subscriber.c common.o

.PHONY: clean run_server run_subscriber

# Ruleaza serverul
run_server:
	./server ${PORT}

# Ruleaza clientul 	
run_subscriber:
	./subscriber ${ID} ${IP_SERVER} ${PORT}

clean:
	rm -rf server subscriber *.o *.dSYM
