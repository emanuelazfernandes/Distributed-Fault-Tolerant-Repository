#include "API.h"

typedef struct peer_list_{
	bool isAvailable;//aqui, depois, em vez disto meter um criterio de availability para o round-robin
	bool isAlive;
	float busy_lvl;
	int port;// -> esta é a porta 4clients!
	char addr[20];//123.567.901.345\0 => 16 max
	int id;
	struct sockaddr_in peer_addr;
	//int n_clients;
	//struct sockaddr_in clients_list[100];
	struct peer_list_ *next;
} peer_list;


typedef struct memory_{
	peer_list *firstPeer;
	peer_list *lastPeer;
} memory;

//structure used by incoming_handler
typedef struct inc_thr_args_struct_{
	char *buf;
	int len;
	struct sockaddr_in outter_addr;
} inc_thr_args_struct;

void proto_peers_list_convert(peer_list *peer_list_in, int number_of_peers,
	PeersList *proto_peers_list, Peer **proto_peer_aux);
