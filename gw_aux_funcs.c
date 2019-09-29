#include "gw_aux_funcs.h"

//int number_of_peers = 5;

//PeersList proto_peers_list;//meter isto na shared memory?
//Peer **proto_peer_aux;//meter isto na shared memory?
//peers_list__init(&proto_peers_list);//isto agora está aqui, mas depois tem de estar no main

// Função que converte uma peer_list numa estrutura de PeersList (.proto), para
// ser enviada da gw para cada peer
//chamar como proto_peers_list = create_peers_list(ShmPTR->firstPeer) [I think... a ideia é alterar o conteúdo da PeersList global]
void proto_peers_list_convert(peer_list *peer_list_in, int number_of_peers,
	PeersList *proto_peers_list, Peer **proto_peer_aux){
	
	peer_list *auxPeer = peer_list_in;//aponta para o primeiro
	int i = 0;
	
	//peers_list__init(&proto_peers_list);//se estiver no main, podes apagar isto

	proto_peer_aux = realloc(proto_peer_aux, sizeof(Peer *) * number_of_peers);
	if (proto_peer_aux == NULL){
		perror("malloc");
		exit(-1);
	}

	for(i = 0; i < number_of_peers && auxPeer != NULL; i++, auxPeer = auxPeer->next){
		proto_peer_aux[i] = realloc(proto_peer_aux[i], sizeof(Peer));
		if (proto_peer_aux[i] == NULL){
			perror("malloc");
			exit(-1);
		}
		peer__init(proto_peer_aux[i]);
		proto_peer_aux[i]->peer_id = auxPeer->id;//isto é uma string, não faço a mínima se isto está bem...
		proto_peer_aux[i]->ip = auxPeer->addr;
		proto_peer_aux[i]->port = auxPeer->port;
	}
	proto_peers_list->n_peer = i;//podia ser number_of_peers, mas pode dar erro...
	proto_peers_list->peer = proto_peer_aux;//no exemplo funciona assim...
	//printf("convert: i = %d\n", i);
	//printf("convert: n_peer = %lu\n", proto_peers_list->n_peer);

/*
	//clean up your mess...
	for (i = 0 ; i < proto_peers_list->n_peer; i++){
		free(proto_peer_aux[i]);
	}
	free(proto_peer_aux);
*/
}
