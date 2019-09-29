#include "API.h"

int main(int argc, char const *argv[]){

	char gwIP[60], gwPort[60];// 60 escolhido arbitrariamente (por excesso)
	int gatewayPort;
	int peer_socket;

	//outbound
	NetworkMessage network_message_out;
	network_message__init(&network_message_out);
	
	
	//get gateway info
	printf("What is the gateway IP?\n");
	fgets(gwIP, 60, stdin);
	// falta fazer verificação!!!

	printf("What is the gateway port?\n");
	if (fgets(gwPort, 60, stdin) != NULL){
		gatewayPort = atoi(gwPort);
	}

	//strcpy(gwIP, "192.168.1.106");//debug
	//strcpy(gwIP, "10.0.2.15");//debug
	strcpy(gwPort, "7654");//debug
	gatewayPort = atoi(gwPort);
	printf("gateway ip = %s, port = %s. Right?\n", gwIP, gwPort);

	puts("here I should be asking the user to call the gallery_connect (menu bonito e tal)");//debug


	//get peer_socket
	peer_socket = gallery_connect(gwIP, gatewayPort);

	//test cases
	switch (peer_socket){
		case -1:
			printf("Can't connect to gateway. Check the IP and/or port...\n");
			break;

		case 0:
			printf("Successfully connected to gateway, but no peer is available!\n");
			break;

		default:
			printf("Connected to a peer. peer_socket = %d\n", peer_socket);

	}

	printf("let's call *add_photo* [press enter]");
	getchar();
	int foo = gallery_add_photo(peer_socket, NULL);
	printf("(test) return value: %d - add_photo", foo);
	getchar();
	/*foo = gallery_add_keyword(peer_socket, 1, NULL);
	printf("(test) return value: %d - add_kw", foo);

	foo = gallery_search_photo(peer_socket, NULL, NULL);
	printf("(test) return value: %d search", foo);

	foo = gallery_delete_photo(peer_socket, 1);
	printf("(test) return value: %d DEL", foo);

	foo = gallery_get_photo_name(peer_socket, 1, NULL);
	printf("(test) return value: %d get_name", foo);

	foo = gallery_get_photo(peer_socket, 1, NULL);
	printf("(test) return value: %d DL", foo);
	*/

	puts("closing");
	close(peer_socket);
	exit(0);
}
