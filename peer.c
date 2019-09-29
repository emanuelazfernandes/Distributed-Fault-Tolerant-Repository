#include "API.h"
#include <ifaddrs.h>
#include <sys/ipc.h>
#include <sys/shm.h>

typedef struct photo_{
	int photo_id;
	char ** keyword_array;//pode ter mais que uma keyword
	char * photo_name;
	struct photo_ *next;
} photo;

typedef struct photo_memory_{
	photo *firstPhoto;
	photo *lastPhoto;
} photo_memory;

photo_memory *photoList;
int32_t global_photo_id;
//this will be the address clients connect to
struct sockaddr_in addr_4clients;
bool in_ready = false;//bool for incoming thread readiness
int my_peer_id = -125;
float my_busy_lvl = 6.78;

/*photo *photoListINIT(photo * first, photo * last){
	photo *aux;
	if (first == NULL){
		//empty list: initialize
		aux= (photo *) malloc(sizeof(photo));
		if (aux == NULL){
			perror("malloc");
				exit(-1);
		}
		first = aux;
		last = aux;
	}else{
		//list has at least one element: allocate for one more
		aux = (photo *) malloc(sizeof(photo));
		if (aux == NULL){
			perror("malloc");
			exit(-1);
		}
		last->next = aux;
		last = aux;//lastPhoto aponta sempre para o ultimo peer. 
		//assim, os peers são sempre adicionados ao fim da lista, ordenadamente
	}
	last->next = NULL;
	return aux;
}*/

//thread that handles all incoming connections
void *incoming_handler(void *arg){

	socklen_t addr_size;
	int sock_fd_4clients, incoming_sock_fd;

	//inbound
	NetworkMessage *network_message_in;
	Content *content_in;
	//char c_in;
	int d_in = 0;
	char buf_in[MAX_MSG_SIZE];
	//outbound
	NetworkMessage network_message_out;
	Content content_out;
	unsigned len_out;
	char *buf_out;
	bool flag_send = false;
	bool flag_broadcast = false;
	//aux variables for content_out
	int out_message_type;
	/*char out_ip[20], out_keyword[1024];
	int out_port, out_peer_id, out_op_result, out_id_photo;
	float out_busy_lvl = 0;
	PeersList peers_list_out;
	Peer peer_out;
	Metadata metadata_out;
	*/

	puts("@incoming_handler");
	//prepare my "outter" socket
	addr_4clients.sin_family = AF_INET;
	addr_4clients.sin_port = INIT_PORT + getpid();//randomize port
	addr_4clients.sin_addr.s_addr = INADDR_ANY;

	//socket that the peer will use for any incoming message
	sock_fd_4clients = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd_4clients == -1){
		perror("socket (incoming_handler):");
		exit(-1);
	}
	printf("Incoming socket created...");
	//prepare the socket to receive connections from anyone (1st gw then clients)
	int err = bind(sock_fd_4clients, (struct sockaddr *) &addr_4clients,
									sizeof(addr_4clients));
	if (err == -1){
		perror("bind (incoming_handler)");
		exit(-1);
	}
	printf(" and binded..");
	//10 ou 20 = length of incoming queue before accept
	if (listen(sock_fd_4clients, 20) == -1){
		perror("listen (incoming_handler)");
		exit(-1);
	}
	printf(" Listening...");
	in_ready = true;


	struct sockaddr_in addr_incoming;
	addr_size = sizeof(struct sockaddr_in);
	puts("waiting @ accept...");
	incoming_sock_fd = accept(sock_fd_4clients,
								(struct sockaddr *) &addr_incoming, &addr_size);
	if (incoming_sock_fd == -1){
		perror("accept (incoming_handler)");
		exit(-1);
	}
	printf("done accept\n");
	while(1){
		

		printf("@ incoming thread waiting for messages\n");
		d_in = recv(incoming_sock_fd, &buf_in, MAX_MSG_SIZE, 0);
		//printf("secalhar não (na thread)\n");



		printf("@ incoming thread received %d bytes from %s:%d \n", d_in,
							inet_ntoa(addr_incoming.sin_addr), ntohs(addr_incoming.sin_port));
		if(d_in <= 0){
			printf("I shouldn't be getting %d bytes from them.", d_in);
			getchar();
			continue;//ignore message
		}
		//debug
		printf("buf_in = ");
		print_buffer((uint8_t *) buf_in, d_in);
		printf("unpacking buf_in...");
		//unpack received buffer and populate structure
		network_message_in = network_message__unpack(NULL, d_in, (uint8_t *) buf_in);
		puts("done");
		printf("Received message: UserType = %u | MessageType = %u\n",
						network_message_in->user_type, network_message_in->message_type);//debug
		if (network_message_in->content != NULL){
			printf(" | Content = true\n");
		}else{
			printf(" | Content = false\n");
		}

		//reinitialize structure
		content__init(&content_out);
		switch (network_message_in->user_type){
			case 0://GW = 0
				puts("Hello gateway, I shouldn't be getting messages from you here...I'm not going to process it");
				break;
			case 1://PEER = 1
				puts("hello my fellow peer!");
				switch (network_message_in->message_type){
					case 3://OP = 3
						content_in = network_message_in->content;
						if (content_in == NULL){
							puts("peer, didn't receive content...");
						} else{
							puts("received an operation message from another peer!");
							if (!(content_in->has_op_type)){
								puts("peer, no op_type?");
							}else{
								//doesn't need to send anything out, just update my own structures
								switch(content_in->op_type){
									case 0:
										//new photo coming in! - updated on one of the peers

										break;
									case 1:
										//new keyword added to a photo: verify if we already have it...

										break;
									case 2:
										//delete a photo

										break;
									default:
										printf("peer, I wasn't supposed to receive this op_type = %d\n", content_in->op_type);
										break;
								}
							}
						}
						break;
					default:
						printf("peer, received a wrong message_type = %d\n", network_message_in->message_type);
						break;
				}
				break;
			case 2://CLIENT = 2
				printf("Hello client, ");//debug
				switch (network_message_in->message_type){
					case 0://CONNECT = 0
						puts("received a connect message.");//debug
						flag_send = true;
						out_message_type = 0;//CONNECT = 0
						content_out.has_op_result = true;
						content_out.op_result = 1;
						break;
					case 3://OP = 3
						puts("received an operation message");//debug
						out_message_type = 3;
						flag_send = true;
						content_out.has_op_result = true;
						content_in = network_message_in->content;
						if(content_in == NULL){
							puts("but it has no content! continuing...");
							break;
						}
						switch (content_in->op_type){
							case 0://ADD_PHOTO
								puts("gallery_add_photo - inserting new photo on list!");
								//need to check if this peer already has a list of photos

								photo *new_photo = (photo *) malloc(sizeof(photo));
								if (new_photo == NULL){
									perror("malloc");
									exit(-1);
								}
								if (photoList->firstPhoto == NULL){
									//empty list: initialize
									puts("empty photo list");
									photoList->firstPhoto = new_photo;
									photoList->lastPhoto = new_photo;
								}else{
									puts("photo list initialized, adding new one");
									//list has at least one element: allocate for one more
									photoList->lastPhoto->next = new_photo;
									photoList->lastPhoto = new_photo;//lastPhoto aponta sempre para o ultimo peer. 
									//assim, os peers são sempre adicionados ao fim da lista, ordenadamente
								}
								photoList->lastPhoto->next = NULL;

								global_photo_id++;
								new_photo->photo_id = global_photo_id;
								puts("here I should notify peers that global_photo_id changed");
								//BROADCAST MESSAGE TO ALL PEERS
								
								content_out.op_result = new_photo->photo_id;//teste
								break;
							case 1://ADD_KW
								puts("gallery_add_keyword()");
								content_out.op_result = 1;//teste, conforme o enunciado
								break;
							case 2://DEL
								puts("gallery_delete_photo()");
								content_out.op_result = 1;//teste, conforme o enunciado
								break;
							case 3://SEARCH
								puts("gallery_search_photo()");
								content_out.op_result = 33;//teste, conforme o enunciado
								//HERE I SHOULD RETURN THE PHOTO_ID...
								break;
							case 4://GET_NAME
								puts("gallery_get_photo_name()");
								content_out.op_result = 1;//teste, conforme o enunciado
								break;
							case 5://DOWNLOAD
								puts("gallery_get_photo()");
								content_out.op_result = 1;//teste, conforme o enunciado
								break;
							default:
								printf("error op_type\n");
								content_out.op_result = -1;//atenção que o erro a retornar tem 
								break; //valores diferentes para cada função...
						}
						break;
					default:
						printf("client, you sent me an illegal message_type = %d\n", network_message_in->message_type);//debug
						break;
				}
				break;
			default:
				printf("something went wrong, received user_type = %d\n",
							network_message_in->user_type);
				break;
		}

		if (flag_send){
			network_message__init(&network_message_out);
			network_message_out.user_type = 1;//PEER = 1
			network_message_out.message_type = out_message_type;

			//content__init é sempre feito lá em cima
			network_message_out.content = &content_out;
			len_out = network_message__get_packed_size(&network_message_out);
			buf_out = (char *) malloc(len_out);
			if (buf_out == NULL){
				perror("malloc");
				exit(-1);
			}

			//pack it and send to gw
			network_message__pack(&network_message_out, (uint8_t *) buf_out);
			//debug
			printf("buf_out = ");
			print_buffer((uint8_t *) buf_out, len_out);

			printf("out_message_type: %d\n", network_message_out.message_type);
			printf("Press enter when ready to send buf_out to [%d] (2=client) ) @ %s:%d",
				network_message_in->user_type, inet_ntoa(addr_incoming.sin_addr),
				ntohs(addr_incoming.sin_port));
			getchar();

			send(incoming_sock_fd, buf_out, len_out, 0);
			puts("sent");

			free(buf_out);
		}
		flag_send = false;
		// broadcast message to all other peers: send a photo and metadata
		if (flag_broadcast){
			//NO TIME :(
		}
		flag_broadcast = false;
		puts("next!");
	}
	// FALTA CAPTURAR CTRL-C
	close(sock_fd_4clients); 
}

/*
void *send_alive(void *gw_addr){
	//thread that sends alive signal to server every 4 seconds -> qual é o tempo ideal?
	int gw_socket_alive;
	network_messages dumpeer_start_msg;

	//create a new, dedicated socket
	gw_socket_alive = socket(AF_INET, SOCK_DGRAM, 0);
	if (gw_socket_alive == -1){
		perror("socket (4GW_alive)");
		exit(-1);
	}
	dumpeer_start_msg.message_type = server_alive;

	while(1){
		sendto(gw_socket_alive, &dumpeer_start_msg, sizeof(network_messages), 0,
					(const struct sockaddr *) &gw_addr, sizeof(gw_addr));
		puts("send_alive here: just sent alive signal to GW");// comenta e descomenta para debug
		sleep(4);
	}
	capture ctrl-c...
}
*/

int main(int argc, char const *argv[]){

	struct in_addr gatewayIP;
	int gatewayPort;
	char gwIP[60], gwPort[60];
	//this will be the gateway's address so I can be announced to clients
	struct sockaddr_in gw_addr;
	socklen_t socklen;
	//protobuf outbound
	NetworkMessage network_message_out;
	network_message__init(&network_message_out);
	network_message_out.user_type = 1;//PEER = 1, sempre o mesmo
	int len_out;
	char *buf_out;
	Content content_out;
	content__init(&content_out);
	//inbound
	NetworkMessage *network_message_in;
	Content *content_in;
	PeersList *peers_list_in;
	//other
	int d_in = 0;
	char buf_in[MAX_MSG_SIZE];
	//threads
	pthread_t thr_id_incoming/*, thr_id_send_alive*/;
	int pt_error;
	int i = 0;
	//shared_memory para as fotos
	//key_t photoShmKEY;
	//char cwd[60];//para o ftok ficar com o path
	//int photoShmID;
	
	/*
	//tentativa de shared memory
	if (getcwd(cwd, sizeof(cwd)) == NULL){
		perror("getcwd: ");
		//exit(-1);
	}
	//printf("path for cwd = %s\n", cwd);//depois apaga isto
	photoShmKEY = ftok(cwd, 'R');//le wild char appears

	//allocating 10 slots in shared memory
	photoShmID = shmget(photoShmKEY, sizeof(photo_memory), IPC_CREAT | 0666);
	if (photoShmID < 0){
		perror("shmget (peer): ");
		exit(1);
	}

	//puts("Gateway received a shared memory able to hold 10 peer_list elements");
	
	photoList = (photo_memory *) shmat(photoShmID, NULL, 0);
	if (photoList == (void *) -1){
		printf("*** shmat error (gateway) ***\n");
		exit(1);
	}
	*/
	photoList = malloc(10 * sizeof(photo));
	photoList->firstPhoto = NULL;
	photoList->lastPhoto = NULL;

	// create a thread that handles all incoming messages
	pt_error = pthread_create(&thr_id_incoming, NULL, incoming_handler, NULL);
	if (pt_error != 0){
		perror("pthread_create (incoming_handler): ");
		exit(-1);
	}

	//wait for thread to start listening socket - pthread_join() faria equivalene
	while(!in_ready){
		puts("waiting for thread incoming_handler to start...");
		sleep(1);
	}

	//Get Gateway info
	printf("what is the GW IP?\n");
	if (fgets(gwIP, 60, stdin) != NULL){
		//puts(gwIP);//debug
		if (inet_aton(gwIP, &gatewayIP) == 0){
			perror("inet_aton : ");
			exit(-1);
		}
	}
	printf("what is the port? default = 7654\n");
	if (fgets(gwPort, 60, stdin) != NULL){
		puts(gwPort);//debug
		gatewayPort = atoi(gwPort);
	}
	
	inet_aton(gwIP, &gatewayIP);
	gatewayPort = atoi(gwPort);
	//printf("gw @ %s:%s\n", gwIP, gwPort);//debug
	

	printf("my port for clients: %d\n", ntohs(addr_4clients.sin_port));
	
	//socket I use to sendto Gateway
	int gw_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (gw_socket == -1){
		perror("socket (4GW): ");
		exit(-1);
	}

	//put it in the proper struct
	gw_addr.sin_family = AF_INET;
	gw_addr.sin_port = htons(gatewayPort);
	gw_addr.sin_addr = gatewayIP;
	socklen = sizeof(gw_addr);
	/*
	printf("My open port to be sent to gw: %d\ngw @ %s:%d\n",
					peer_msg_content_aux->port, inet_ntoa(gw_addr.sin_addr),
					ntohs(gw_addr.sin_port));
	*/

	//populate structure
	network_message_out.message_type = 0;//CONNECT = 0
	content_out.has_port = true;
	content_out.port = ntohs(addr_4clients.sin_port);
	//printf("sin_port:%d\ncontent_out.port:%d\n", addr_4clients.sin_port, content_out.port);
	printf("Packing my connect message to sendo to gateway (user_type = peer, msg_type = connect, content = {port = %d})...", content_out.port);

	network_message_out.content = &content_out;

	len_out = network_message__get_packed_size(&network_message_out);
	buf_out = (char *) malloc(len_out);
	if (buf_out == NULL){
		perror("malloc");
		exit(-1);
	}

	//pack it and send to gw
	network_message__pack(&network_message_out, (uint8_t *) buf_out);
	//debug
	printf("done\nmy buf_out = ");
	print_buffer((uint8_t *) buf_out, len_out);

	printf("Press enter when ready to send buf_out to gw @ %s:%d",
					inet_ntoa(gw_addr.sin_addr), ntohs(gw_addr.sin_port));
	getchar();
	//actual send
	sendto(gw_socket, buf_out, len_out, 0,
					(const struct sockaddr *) &gw_addr, socklen);
	printf("sent connect network_message to gw, awaiting response [still need to know my ID]...\n");

	//já não vamos usar mais, limpa-se...
	free(buf_out);

	puts("waiting for incoming messages (clients, gw and other peers)...");

	//initialize photo_id
	global_photo_id = 0;
	//printf("global_photo_id = %d\n", global_photo_id);//debug _ devia receber o ID e a quanto vai este valor
	if(photoList->firstPhoto == NULL) puts("photo_list uninitialized");

	//create thread to handle send_alive
	/*	pt_error = pthread_create(&thr_id_send_alive, NULL, send_alive, &gw_addr);
	if (pt_error != 0){
		perror("pthread_create (send_alive): ");
		exit(-1);
	}
	*/
	//cycle dedicated for gw, to update peers_list
	while(1){
		d_in = recvfrom(gw_socket, buf_in, MAX_MSG_SIZE, 0,
										(struct sockaddr *) &gw_addr, &socklen);
		printf("received %d bytes from %s:%d \n", d_in,
							inet_ntoa(gw_addr.sin_addr), ntohs(gw_addr.sin_port));
		//debug
		printf("buf_in = ");
		print_buffer((uint8_t *) buf_in, d_in);

		//puts("in this message there should the PeersList, and on first, my id");

		network_message_in = network_message__unpack(NULL, d_in, (uint8_t *) buf_in);
		printf("unpacked buf_in\n");

		//verifications
		printf("Received message: UserType = %u | MessageType = %u",
						network_message_in->user_type, network_message_in->message_type);
		if (network_message_in->content != NULL){
			printf(" | Content = true\n");

			content_in = network_message_in->content;
			/*printf("content_in->ip = %s | content_in->port =%d\n",
						content_in->ip, content_in->port);//debug*/

			if (!(content_in->has_peer_id)){
				puts("didn't receive my id from gateway....");
			}else{
				my_peer_id = content_in->peer_id;
			}
			printf("my_peer_id = %d\n", my_peer_id);

			if (content_in->peers_list != NULL){
				peers_list_in = content_in->peers_list;
				printf("total number of peers on the network = %lu\n", peers_list_in->n_peer);
				for(i = 0; i < peers_list_in->n_peer; i++){
					printf("peer_id = %d | ", peers_list_in->peer[i]->peer_id);
					printf("%s:", peers_list_in->peer[i]->ip);
					printf("%d", peers_list_in->peer[i]->port);
					if (peers_list_in->peer[i]->peer_id == my_peer_id){
						printf(" <- me");
					}
					printf("\n");
				}
			}else{
				puts("didn't receive peers_list from gateway...");
			}
		}else{
			printf(" | Content = false\n");
		}
	}
	//must handle this...
	close(gw_socket);
	puts("peer out, good bye cruel world");
	exit(0);
}
