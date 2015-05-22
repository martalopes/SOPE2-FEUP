all: balcao.c ger_cl.c
	gcc  balcao.c -o ./bin/balcao -Wall -lpthread -lrt
	gcc  ger_cl.c -o ./bin/ger_cl -Wall -lpthread -lrt
	
