#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>

#include "API.h"
#include "gw_aux_funcs.h"

#define OPENPORT 7654

// global variables
int sock_fd;
int peer_cnt = 0, number_of_peers = 0;
PeersList proto_peers_list;
Peer **proto_peer_aux;
memory *ShmPTR;//shm pointer
inc_thr_args_struct inc_thr_args;
//sem_t sem_peers_list/*, sem_inc_thr_args*/;

/*
void print_peer_list(peer_list *peer_list_in){
	peer_list *auxPeer = NULL;
	int c = 0;

	if (peer_list_in != NULL){
		c++;
		printf("peer.id = %d | c = %d\n", peer_list_in->id, c);
		auxPeer = peer_list_in->next;
		while (auxPeer != NULL){
			c++;
			printf("auxPeer.id = %d | c = %d\n", auxPeer->id, c);
			auxPeer = auxPeer->next;
		}
		printf("a lista tem %d peers\n", c);
	}else{
		puts("No peers connected");
	}
}
*/

/*
// 
void peer_chk(peer_list *peer_list_in){
	//percorre a lista, e verifica se os peers responderam.
	//os que não tiverem respondido ao fim de *20* segundos são eliminados
	//faltava implementar aqui um timer...
	peer_list *prevPeer, *currPeer;
	while(1){
		//puts("doing peer_list checkup...");//debug
		prevPeer = NULL;
		for (currPeer = peer_list_in; currPeer != NULL;
					prevPeer = currPeer, currPeer = currPeer->next){
			if (!currPeer->isAlive){
				printf("removing peer\n");//adiciona info do peer a ser removido
				//reconecta os clients!

				if (prevPeer == NULL){
					peer_list_in = currPeer->next;
				}else{
					prevPeer->next = currPeer->next;
				}
				free(currPeer);
			}
		}
		puts("peer_chk reached end of peer_list");
		sleep(20);
	}
}
*/

// function that handles the client_response
void client_response(struct sockaddr_in client_addr_in){
	struct sockaddr_in client_addr = client_addr_in;
	NetworkMessage network_message_out;
	network_message__init(&network_message_out);
	int len_out;
	char *buf_out;
	Content content_out;
	content__init(&content_out);
	peer_list *auxPeer, *min_busy_peer;
	float min_busy = 99.99;

	network_message_out.message_type = 0;//CONNECT = 0

	if (ShmPTR->firstPeer == NULL){
		puts("There are no peers on the list, the client should try again later.");//debug
		content_out.ip = "nope";
		content_out.has_port = true;
		content_out.port = -1;
	}else{
		//at least one peer -> prepare structures for outbound message
		for (auxPeer = ShmPTR->firstPeer, min_busy = 99.99;
			auxPeer != NULL;
			auxPeer = auxPeer->next){
			//chooses the first available peer on the list (one client per peer)
			if (auxPeer->busy_lvl < min_busy){
				min_busy_peer = auxPeer;
				min_busy = auxPeer->busy_lvl;
			}
		}
		//min_busy_peer is pointing to peer with lowest busy_lvl
		printf("Found a peer for this client. id = %d, busy_lvl = %.2f\n",
						min_busy_peer->id, min_busy_peer->busy_lvl);
		content_out.ip = min_busy_peer->addr;
		content_out.has_port = true;
		content_out.port = min_busy_peer->port;
		//store new client addr and update n_clients
		//min_busy_peer->clients_list[min_busy_peer->n_clients] = client_addr;
		//min_busy_peer->n_clients++;
	}
	//printf("peer IP = %s | port = %d\n", content_out.ip, content_out.port);//debug

	//encapsulate
	network_message_out.content = &content_out;
	len_out = network_message__get_packed_size(&network_message_out);
	//printf("len_out = %d, ");	
	buf_out = (char *) malloc(len_out);
	if (buf_out == NULL){
		perror("malloc: ");
		exit(-1);
	}

	network_message__pack(&network_message_out, (uint8_t *) buf_out);
	printf("IP = %s | port = %d\n", network_message_out.content->ip,
					network_message_out.content->port);//debug

	//debug
	printf("buf_out = ");
	print_buffer((uint8_t *) buf_out, len_out);
	
	printf("Press enter when ready to send buf_out to client @ %s:%d",
					inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
	getchar();

	//send to client -> era suposto enviar também sempre que o peer desconectar...
	sendto(sock_fd, buf_out, len_out, 0,
					(const struct sockaddr *) &client_addr, sizeof(client_addr));
	puts("sent the peer's info to the client");
	free(buf_out);
}



//anytime the list is changed (new or death of peer) broadcast the list to all peers
void broadcast_peers_list(){
	NetworkMessage network_message_out;
	int len_out;
	char *buf_out;
	Content content_out;
	//peer_list pointer
	peer_list *auxPeer;
	struct sockaddr_in outter_addr;

	//introduzi este peer na lista, vou agora fazer broadcast para todos
	//os peers da PeersList! o último é o recentemente adicionado, por
	//isso para esse tem de se enviar também o seu id
	content__init(&content_out);//reiniciar a estrutura
	content_out.peers_list = &proto_peers_list;
	network_message__init(&network_message_out);
	network_message_out.user_type = 0;//GW = 0
	network_message_out.message_type = 1;//INFO = 1
	network_message_out.content = &content_out;
	puts("protobuf structure all done.");
	//encapsulate
	len_out = network_message__get_packed_size(&network_message_out);
	buf_out = malloc(len_out);
	if (buf_out == NULL){
		perror("malloc");
		exit(-1);
	}
	network_message__pack(&network_message_out, (uint8_t *) buf_out);
	//debug
	printf("buf_out = ");
	print_buffer((uint8_t *) buf_out, len_out);

	printf("Sending PeersList, n = %lu\n", network_message_out.content->peers_list->n_peer);
	printf("Press enter when ready to send buf_out to all peers");
	getchar();

	//sending to all peers
	for (auxPeer = ShmPTR->firstPeer; auxPeer->next != NULL; auxPeer = auxPeer->next){

		//broadcast to all peers! - põe isto numa função...
		outter_addr = auxPeer->peer_addr;// pffffff
		printf("Sending buf_out to peer @ %s:%d", inet_ntoa(outter_addr.sin_addr), ntohs(outter_addr.sin_port));
		//getchar();
		sendto(sock_fd, buf_out, len_out, 0,
						(const struct sockaddr *) &outter_addr, sizeof(outter_addr));
		printf("sent peer_id = %d e a peer_list populada também...\n", auxPeer->id);
	}
	free(buf_out);

	//aqui envia para o ultimo, igual mas com id
	content__init(&content_out);//reiniciar a estrutura
	content_out.has_peer_id = true;
	content_out.peer_id = auxPeer->id;
	content_out.peers_list = &proto_peers_list;
	network_message__init(&network_message_out);
	network_message_out.user_type = 0;//GW = 0
	network_message_out.message_type = 1;//INFO = 1
	network_message_out.content = &content_out;
	puts("protobuf structure all done.");
	//encapsulate
	len_out = network_message__get_packed_size(&network_message_out);
	buf_out = malloc(len_out);
	if (buf_out == NULL){
		perror("malloc");
		exit(-1);
	}
	network_message__pack(&network_message_out, (uint8_t *) buf_out);
	//debug
	printf("buf_out = ");
	print_buffer((uint8_t *) buf_out, len_out);

	printf("Vamos enviar agora ao peer o seu peer_id = %d\n",
					network_message_out.content->peer_id);//debug
	printf("Sending PeersList, n = %lu\n",
					network_message_out.content->peers_list->n_peer);

	outter_addr = auxPeer->peer_addr;
	printf("Sending buf_out to peer @ %s:%d", inet_ntoa(outter_addr.sin_addr),
					ntohs(outter_addr.sin_port));
	//getchar();
	sendto(sock_fd, buf_out, len_out, 0,
					(const struct sockaddr *) &outter_addr, sizeof(outter_addr));
	printf("sent peer_id = %d (e espero que a peer_list populada também...)\n", content_out.peer_id);

	free(buf_out);
	//end of thread
	pthread_exit(NULL);
}


//quando o main recebe um buffer, este é encaminhado para aqui!
void *incoming_handler(void *inc_thr_args_in){

	//from protocol buffers - outbound
	NetworkMessage network_message_out;
	network_message__init(&network_message_out);
	network_message_out.user_type = 0;//GW = 0
	Content content_out;
	content__init(&content_out);
	//inbound
	NetworkMessage *network_message_in;
	Content *content_in;
	//peer_list pointer
	peer_list *auxPeer = NULL;

	//estes valores é suposto manterem-se inalterados aqui dentro, enquanto esta thread faz a sua cena...
	char *buf_in;
	buf_in = inc_thr_args.buf;
	int d_in = inc_thr_args.len;
	printf("d_in = %d | buf_in = ", d_in);
	print_buffer((uint8_t *) buf_in, d_in);
	struct sockaddr_in outter_addr = inc_thr_args.outter_addr;

	printf("inside the thread, let's unpack... ");

	//unpack received buffer and populate structure
	network_message_in = network_message__unpack(NULL, d_in, (uint8_t *) buf_in);

	printf("unpacked.\n");
	printf("Received message: UserType = %u | MessageType = %u",
					network_message_in->user_type, network_message_in->message_type);
	if (network_message_in->content == NULL){
		printf(" | Content = false\n");
	}else{
		printf(" | Content = true\n");
	}
	//process message
	switch (network_message_in->user_type){
		puts("user_type switch");
		case 2://CLIENT = 2
			puts("olá senhor cliente :D");
			client_response(outter_addr);
			break;

		case 1://PEER = 1
			//semaforos nas varias cenas?!
			puts("olá senhor peer :)");
			switch (network_message_in->message_type){
				case 0://CONNECT = 0
					if (network_message_in->content == NULL){
						printf("peer, I didn't receive content...");
						break;
					}
					content_in = network_message_in->content;
					if(!(content_in->has_port)){
						printf("peer, content_in->has_port = false! mekie?");
					}
					printf("Recebi que a porta aberta do peer para os clientes é content_in->port = %d\n",
									content_in->port);

					peer_cnt++;//starts at 1, only increases
					number_of_peers++;//depois no alive, quando morre um peer, isto decrementa
					printf("o seu peer_id será = %d\n", peer_cnt);

					//add peer info received to peer_list
					auxPeer = (peer_list *) malloc(sizeof(peer_list));
					if (auxPeer == NULL){
						perror("malloc");
						exit(-1);
					}
					if (ShmPTR->firstPeer == NULL){
						puts("peer_list empty. will initialize it and insert it");
						//empty list: initialize
						
						ShmPTR->firstPeer = auxPeer;
						ShmPTR->lastPeer = auxPeer;
					}else{
						printf("peer_list já tem pelo menos 1 elemento. O seu id será = %d\n", peer_cnt);
						//list has at least one element
						ShmPTR->lastPeer->next = auxPeer;
						ShmPTR->lastPeer = auxPeer;//lastPeer aponta sempre para o ultimo peer. 
						//assim, os peers são sempre adicionados ao fim da lista, ordenadamente
					}
					//ShmPTR->lastPeer now points to the new peer

					//auxPeer always points to the latest added peer -> depois fazer isto apontar para o que esteja menos busy? * * * * * * * * *
					auxPeer = ShmPTR->lastPeer;
					auxPeer->port = content_in->port;
					printf("port (from the list), auxPeer->port = %d\n", auxPeer->port);//debug
					strcpy(auxPeer->addr, inet_ntoa(outter_addr.sin_addr));

					auxPeer->next = NULL;
					auxPeer->isAvailable = true;
					auxPeer->isAlive = true;
					auxPeer->busy_lvl = 3.14 + peer_cnt;
					auxPeer->id = peer_cnt;
					auxPeer->peer_addr = outter_addr;
					//auxPeer->n_clients = 0;
					//fim das cenas da lista

					//HÁ AQUI UM BUG ALGURES!!

					proto_peers_list_convert(ShmPTR->firstPeer, number_of_peers,
						&proto_peers_list, proto_peer_aux);
					puts("proto converted!");
		      //chamar a broadcast_peers_list
					broadcast_peers_list();

					//semaforo_out
					break;
		//to here_v2
				case 1://INFO = 1
					puts("received message_type info");
					break;
				case 2://ALIVE = 2
					puts("received message_type alive");
					break;
				default:
					printf("default on message_type switch. how did I get here?");
					printf(" message_type = %d\n", network_message_in->message_type);
					break;
			}
			break;

		default:
			printf("user_type = %d. Are we being hacked?\n", network_message_in->user_type);
			//exit(-1);
			break;
	}
	//end of thread
	pthread_exit(NULL);
}

int main(){

	//variables - threads
	pthread_t thr_incoming;
	int pt_error;
	//inet structs
	struct sockaddr_in local_addr, outter_addr;
	socklen_t socklen;
	//shm
	key_t ShmKEY;
	char cwd[60];//para o ftok ficar com o path
	int ShmID;
	//recvfrom buffer
	int d_in = 0;
	char buf_in[MAX_MSG_SIZE];

	//initialization
	peers_list__init(&proto_peers_list);//isto aqui só precisa de ser inicializado uma vez!

	//shm
	//to make sure we have a unique shmkey (directory)
	if (getcwd(cwd, sizeof(cwd)) == NULL){
		perror("getcwd: ");
		//exit(-1);
	}
	//printf("path for cwd = %s\n", cwd);//depois apaga isto
	ShmKEY = ftok(cwd, 'G');//le wild char appears
	//allocating 10 slots in shared memory
	ShmID = shmget(ShmKEY, 10*sizeof(memory), IPC_CREAT | 0666);
	if (ShmID < 0){
		perror("shmget (gateway): ");
		exit(1);
	}
	//puts("Gateway received a shared memory able to hold 10 peer_list elements");
	ShmPTR = (memory *) shmat(ShmID, NULL, 0);
	if (ShmPTR == (void *) -1){
		printf("*** shmat error (gateway) ***\n");
		exit(1);
	}
	//puts("Gateway has successfully attached the shared memory.");
	//initialize ShmPTR
	ShmPTR->firstPeer = NULL;
	ShmPTR->lastPeer = NULL;

	// socket
	sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock_fd == -1){
		perror("socket: ");
		exit(-1);
	}
	printf("DGRAM socket created... ");
	//inet structure - inbound
	local_addr.sin_port = htons(OPENPORT);
	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = INADDR_ANY;
	int err = bind(sock_fd, (struct sockaddr *) &local_addr, sizeof(local_addr));
	if (err == -1) {
		perror("bind: ");
		exit(-1);
	}
	printf("and binded.\nReady to receive messages...\n");

	//main cycle
	while(1){

		// read message
		socklen = sizeof(outter_addr);//não sei se isto está bem assim...
		printf("estou aqui preso...\n");
		d_in = recvfrom(sock_fd, buf_in, MAX_MSG_SIZE, 0,
										(struct sockaddr *) &outter_addr, &socklen);
		printf("not anymore\n");

		printf("\nreceived %d bytes from %s:%d\n",
						d_in, inet_ntoa(outter_addr.sin_addr), ntohs(outter_addr.sin_port));
		if(d_in <= 0){
			puts("maybe I didn't receive the data I was expecting? (press enter)");//debug
			getchar();
		}
		//debug
		printf("buf_in = ");
		print_buffer((uint8_t *) buf_in, d_in);
		
		//populate the incomig thread structure
		//strcpy(inc_thr_args.buf, buf_in);
		inc_thr_args.buf = buf_in;
		printf("inc_thr_args.buf = ");
		print_buffer((uint8_t *) inc_thr_args.buf, d_in);
		inc_thr_args.len = d_in;
		inc_thr_args.outter_addr = outter_addr;//aqui algures vai precisar de levar um semáforo!!!!!!!!!
		puts("let's send it to the thread!");

		//pt_error = pthread_create(&thr_incoming, NULL, incoming_handler, (void *) &inc_thr_args);
		pt_error = pthread_create(&thr_incoming, NULL, incoming_handler, NULL);
		if (pt_error != 0){
			perror("pthread_create");
			exit(-1);
		}
	}

// FALTA CAPTURAR O CTRL-C PARA SAIR BEM
// FALTA LIMPAR A MEMÓRIA PARA FAZER UMA SAÍDA LIMPA

	close(sock_fd);
	printf("OK: closed sock_fd\n");
	exit(0);
}
