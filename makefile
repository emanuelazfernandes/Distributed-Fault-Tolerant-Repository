all: client gateway peer

client: client.c API.pb-c.c API.c
	gcc -Wall -c client.c -c API.pb-c.c -c API.c -g
	gcc -Wall -o client client.o API.pb-c.o API.o -lprotobuf-c -g


gateway: gateway.c API.c API.pb-c.c gw_aux_funcs.c
	gcc -Wall -c gateway.c -c API.c -c API.pb-c.c -c gw_aux_funcs.c -lpthread -g
	gcc -Wall -o gateway gateway.o API.o API.pb-c.o gw_aux_funcs.o -lprotobuf-c -g -lpthread


peer: peer.c API.pb-c.c API.c
	gcc -Wall -c peer.c -c API.pb-c.c -c API.c -lpthread -g
	gcc -Wall -o peer peer.o API.pb-c.o API.o -lprotobuf-c -lpthread -g

