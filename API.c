#include "API.h"


void print_buffer(uint8_t *buf, int size_bytes){
	int i;
	printf("[");
	for(i = 0; i < size_bytes; i++){
		printf("%02X|", buf[i]);
	}
	printf("]\n");
}



/*	first interaction between the client and the system
 *		host:		string with the gateway IP
 *		port:		int with the port

 *	returns:
 *			 -1:	gateway not available/can't be accessed
 *			  0:	no peer available
 *	peer_socket:	peer socket fd
 */
int gallery_connect(char *host, in_port_t port){

	//variables
	struct in_addr gatewayIP, peerIP;
	int gw_socket, peer_socket = 0;
	struct sockaddr_in gw_addr, peer_addr;
	socklen_t socklen_gw;
	//from protocol buffers - outbound
	NetworkMessage network_message_out;
	network_message__init(&network_message_out);
	unsigned len_out;
	char *buf_out;
	//inbound
	NetworkMessage *network_message_in, *network_message_in_peer;
	Content *content_in;
	//other
	int d_in = 0;
	char buf_in[MAX_MSG_SIZE], buf_in_peer[MAX_MSG_SIZE];

	//convert string *host to binary and insert it in &gatewayIP
	if (inet_aton(host, &gatewayIP) == 0){
		perror("inet_aton (gallery_connect)");
		return -1;
	}

	//gw_socket => socket used to sendto Gateway
	gw_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (gw_socket == -1){
		perror("socket 4GW (gallery_connect): ");
		return -1;
	}
	printf("socket 4GW socket created\n");//debug

	//populate gw_addr struct
	gw_addr.sin_family = AF_INET;
	gw_addr.sin_addr = gatewayIP;
	gw_addr.sin_port = htons(port);
	socklen_gw = sizeof(gw_addr);

	//populate and allocate outbound message
	network_message_out.user_type = 2;//CLIENT = 2
	network_message_out.message_type = 0;//CONNECT = 0
	len_out = network_message__get_packed_size(&network_message_out);
	buf_out = (char *) malloc(len_out);
	if (buf_out == NULL){
		perror("malloc");
		exit(-1);
	}

	//pack it and send to gw
	network_message__pack(&network_message_out, (uint8_t *) buf_out);

	sendto(gw_socket, buf_out, len_out, 0,
					(const struct sockaddr *) &gw_addr, socklen_gw);
	printf("\nsent connect network_message to gw, awaiting response...\n");//debug
	
	//recv response from gw
	d_in = recvfrom(gw_socket, buf_in, MAX_MSG_SIZE, 0,
									(struct sockaddr *) &gw_addr, &socklen_gw);
	printf("\nreceived %d bytes from gw @ %s:%d\n",
					d_in, inet_ntoa(gw_addr.sin_addr), ntohs(gw_addr.sin_port));
	if(d_in <= 0){
		printf("Unnexpected number of bytes read. couldn't reach GW?\n");
		return -1;
	}

	//unpack received buffer and populate structure
	network_message_in = network_message__unpack(NULL, d_in, (uint8_t *) buf_in);

	//debug
	printf("Received message: UserType = %u | MessageType = %u\n",
					network_message_in->user_type, network_message_in->message_type);
	if (network_message_in->content != NULL){
		printf("Got content:\n");
		content_in = network_message_in->content;
		//printf("content_in->ip = %s | content_in->port =%d\n", content_in->ip, content_in->port);//debug/////////////////////////////////
		
		if (content_in->ip != NULL){
			printf("content_in->ip = %s\n", content_in->ip);
		}else{
			printf("no ip in content\n");
			return 0;
		}
		if (content_in->has_port){
			printf("content_in->port = %d\n", content_in->port);
		}else{
			puts("no port in content");
			return 0;
		}
		if (content_in->port == -1 && strcmp(content_in->ip, "nope") == 0){//condition for no peer available
				puts("gw tells me unequivocally that no peer is available :(");
				return 0;
		}else{
			printf("gw tells me that peer is at %s:%d\n",
							content_in->ip, content_in->port);
		}
	}else{
		puts("Something went wrong, didn't receive content from gw");
		return 0;
	}
	if (network_message_in->user_type != 0){
		puts("wrong user_type received");
		return 0;
	}
	if (network_message_in->message_type != 0){
		puts("message_type other than connect received upon initial contact with gw");
		return 0;
	}

	close(gw_socket);
	//end of gateway communication

	//testing peer socket - handshake //debug - Todo o handshake é apenas para debug certo ?
	//send handshake
	//convert string content_in->ip to binary and insert it in &peerIP
	if (inet_aton(content_in->ip, &peerIP) == 0){
		perror("inet_aton (gallery_connect)");
		return -1;
	}

	//create peer_socket
	peer_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (peer_socket == -1){
		perror("socket [peer] (gallery_connect):");
		return -1;
	}

	//populate peer_addr struct
	peer_addr.sin_family = AF_INET;
	peer_addr.sin_addr = peerIP;
	peer_addr.sin_port = htons(content_in->port);
	
	//connect to peer
	printf("attempting to connect to peer...");
	int err = connect(peer_socket, (const struct sockaddr *) &peer_addr,
										sizeof(peer_addr));
	if(err == -1){
		perror("connect [peer] (gallery_connect):");
		return 0;
	}
	printf("success\n");

	//pack it and send to gw//porque é que estás a fazer pack outra vez? o buf_out não foi alterado! ???????????????????????????

	//porque nao percebo nada disto. mas este handshake é só pra debug certo ? depois do connect podemos sair da gallery_connect

	network_message__pack(&network_message_out, (uint8_t *) buf_out);
	//debug
	printf("buf_out = ");
	print_buffer((uint8_t *) buf_out, len_out);

	printf("Press enter when ready to test my connection to my assigned peer @ %s:%d and send buf_out...", host, port);
	puts("I think I just printed the GW's address but I'm really sending to the peer_socker!");//debug
	getchar();

	//same message that was sent to gw => buf_out [handshake, test connection]
	send(peer_socket, buf_out, len_out, 0);
	printf("handshaking to peer, awaiting response...\n");



	//receive handshake from peer
	d_in = recv(peer_socket, buf_in_peer, MAX_MSG_SIZE, 0);//não sei se dá para reutilizar o buf_in, por isso uso um novo
	printf("received %d bytes from peer @ %s:%d\n",
					d_in, inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
	if(d_in <= 0){
		printf("Unnexpected number of bytes read.\n");
		exit(-1);
	}
	//debug ---- falta verificaçao de d_in > 0 !!!!!
	printf("buf_in_peer = ");
	print_buffer((uint8_t *) buf_in_peer, d_in);

	printf("now let's try to unpack...");
	//unpack received buffer and populate structure
	network_message_in_peer = network_message__unpack(NULL, d_in, (uint8_t *) buf_in_peer);
	printf("unpacked.\n");

	//verifications
	printf("Received message: UserType = %u | MessageType = %u\n",
					network_message_in_peer->user_type,
					network_message_in_peer->message_type);
	if (network_message_in_peer->content != NULL){
		printf("Got content: ");
		content_in = network_message_in_peer->content;
		if (content_in->has_op_result){//não sei porquê isto não funciona =/ -> SE METERES has_op_result = true no envio já funciona!
			printf("op_result = %d\n", content_in->op_result);
			if (content_in->op_result == 1){// condition for handshake success with peer
				puts("handshake success!");
			}else{
				puts("wrong op_result from peer, handshake failed");
				return 0;
			}
		}else{
			puts("Something went wrong, content doesn't contain op_result");
			return 0;
		}
	}/*else{
		puts("Something went wrong, didn't receive content from peer");
		return 0;
	}
	if (network_message_in_peer->user_type != 1){
		puts("wrong user_type received");
		return 0;
	}
	if (network_message_in_peer->message_type != 0){
		puts("message_type other than connect received upon handshake with peer");
		return 0;
	}
	*/


	//free unused memory
	free(buf_out);
	network_message__free_unpacked(network_message_in, NULL);
	network_message__free_unpacked(network_message_in_peer, NULL);

	return peer_socket;
}

/*	adds a new photo_id in the peer's list
 *		peer_socket:		file descriptor to sendto peer
 *		file_name:			[not implemented]
 *
 *	returns:
 *		-1:					failed to add id to the peer's list
 *		int photo_id:		the id of this "photo"
 */
uint32_t gallery_add_photo(int peer_socket, char **file_name){

	//outbound
	NetworkMessage network_message_out;
	network_message__init(&network_message_out);
	unsigned len_out;
	char *buf_out;
	Content content_out;
	//inbound
	NetworkMessage *network_message_in;
	Content *content_in;
	//other
	int d_in = 0;
	char buf_in[MAX_MSG_SIZE];

	if(file_name == NULL){//debug _ depois mudamos para !=NULL quando for um ficheiro a serio e metemos o ==NULL a dizer que ta mal

		puts("we testing this? ok. *inserting* NOT a photo [file_name is NULL]");

		//populate and allocate outbound message
		network_message_out.user_type = 2;//CLIENT = 2
		network_message_out.message_type = 3;//OP = 3
		

		content__init(&content_out);
		content_out.has_op_type = true;
		content_out.op_type = 0;//0 = ADD_PHOTO

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

		printf("Press enter when ready to send buf_out to peer");
		getchar();

		send(peer_socket, buf_out, len_out, 0);
		printf("\nsent ADD_PHOTO to my peer...\n");

		//receive op_result from peer
		d_in = recv(peer_socket, buf_in, MAX_MSG_SIZE, 0);//não sei se dá para reutilizar o buf_in, por isso uso um novo
		printf("received %d bytes from peer\n", d_in);
		if(d_in <= 0){
			printf("Unnexpected number of bytes read.\n");
			exit(-1);
		}
		network_message_in = network_message__unpack(NULL, d_in, (uint8_t *) buf_in);

		//debug
		printf("Received message: UserType = %u | MessageType = %u\n",
						network_message_in->user_type,
						network_message_in->message_type);

		}
		if (network_message_in->content != NULL){
		printf("Got content: ");
		content_in = network_message_in->content;
		if (content_in->has_op_result){//não sei porquê isto não funciona =/ -> SE METERES has_op_result = true no envio já funciona!
			printf("op_result = %d - this is the photo ID!!!\n", content_in->op_result);
			
		}else{
			puts("Something went wrong, content doesn't contain op_result");
			return 0;
		}
		return 1;
	}

	return -1;

}

/*	adds a keyword to a pre-existing photo
 *		peer_socket:		file descriptor to communicate with peer
 *		id_photo:			the id of the "photo" which is getting a new keyword associated
 *		keyword:			the keyword to be added
 *
 *	 returns:
 *		   0:				photo doesn't exist
 *		   1:				the id of this "photo"
 *	negative:				if error occurs
 */
int gallery_add_keyword(int peer_socket, uint32_t id_photo, char *keyword){

	//outbound
	NetworkMessage network_message_out;
	network_message__init(&network_message_out);
	unsigned len_out;
	char *buf_out;
	Content content_out;
	//inbound
	NetworkMessage *network_message_in;
	Content *content_in;
	//other
	int d_in = 0;
	char buf_in[MAX_MSG_SIZE];

	//populate and allocate outbound message
	network_message_out.user_type = 2;//CLIENT = 2
	network_message_out.message_type = 3;//OP = 3
	

	content__init(&content_out);
	content_out.has_op_type = true;
	content_out.op_type = 1;//1 = ADD_KW
	//content_out->has_keyword = true;
	strcpy(content_out.keyword, keyword);


	network_message_out.content = &content_out;

	len_out = network_message__get_packed_size(&network_message_out);
	buf_out = (char *) malloc(len_out);
	if (buf_out == NULL){
		perror("malloc");
		exit(-1);
	}
	//pack it and send to gw
	network_message__pack(&network_message_out, (uint8_t *) buf_out);
	
	printf("buf_out = ");//debug
	print_buffer((uint8_t *) buf_out, len_out);//debug

	send(peer_socket, buf_out, len_out, 0);
	printf("\nsent ADD_KW to my peer...\n");//debug

	//receive op_result from peer
	d_in = recv(peer_socket, buf_in, MAX_MSG_SIZE, 0);//não sei se dá para reutilizar o buf_in, por isso uso um novo
	printf("received %d bytes from peer\n", d_in);//debug

	if(d_in <= 0){
		printf("Unnexpected number of bytes read.\n");
		exit(-1);
	}

	network_message_in = network_message__unpack(NULL, d_in, (uint8_t *) buf_in);

	//debug
	printf("Received message: UserType = %u | MessageType = %u\n",
					network_message_in->user_type,
					network_message_in->message_type);

	
	if (network_message_in->content != NULL){
	
		content_in = network_message_in->content;
		if (!content_in->has_op_result){
			puts("Something went wrong, content doesn't contain op_result");
			return -1;
		}

		printf("op_result = %d\n", content_in->op_result);//debug
		if(content_in->op_result == 1){
			//SUCCESS
			printf("successfully added keyword\n");
		}else if(content_in->op_result == 0){
			printf("photo didn't exist\n");
		}else{//algo nao correu bem
			return -1;
		}
		return content_in->op_result;
	}

	return -1;//se chegar aqui é porque nao correu bem

}

/*	searches for a keyword of a pre-existing photo
 *		peer_socket:		file descriptor to communicate with peer
 *		keyword:			the keyword to be added
 *		id_photos:			pointer to where all the photo with that keyword are be stored
 *
 *	 returns:
 *		   0:				no photo has such keyword
 *		  -1:				error ocurred (network problems/invalid arguments)
 * op_result:				number of photos that have this keyword
 */
int gallery_search_photo(int peer_socket, char *keyword, uint32_t **id_photos){

	//outbound
	NetworkMessage network_message_out;
	network_message__init(&network_message_out);
	unsigned len_out;
	char *buf_out;
	Content content_out;
	//inbound
	NetworkMessage *network_message_in;
	Content *content_in;
	//other
	int d_in = 0;
	char buf_in[MAX_MSG_SIZE];

	//populate and allocate outbound message
	network_message_out.user_type = 2;//CLIENT = 2
	network_message_out.message_type = 3;//OP = 3
	

	content__init(&content_out);
	content_out.has_op_type = true;
	content_out.op_type = 3;//3 = SEARCH
	//content_out.has_keyword = true;
	strcpy(content_out.keyword, keyword);

	network_message_out.content = &content_out;

	len_out = network_message__get_packed_size(&network_message_out);
	buf_out = (char *) malloc(len_out);
	if (buf_out == NULL){
		perror("malloc");
		exit(-1);
	}
	//pack it and send to gw
	network_message__pack(&network_message_out, (uint8_t *) buf_out);
	

	send(peer_socket, buf_out, len_out, 0);
	printf("\nsent SEARCH to my peer...\n");//debug

	//receive op_result from peer
	d_in = recv(peer_socket, buf_in, MAX_MSG_SIZE, 0);//não sei se dá para reutilizar o buf_in, por isso uso um novo
	printf("received %d bytes from peer\n", d_in);//debug

	if(d_in <= 0){
		printf("Unnexpected number of bytes read.\n");
		exit(-1);
	}

	network_message_in = network_message__unpack(NULL, d_in, (uint8_t *) buf_in);

	//verifications _ debug
	printf("Received message: UserType = %u | MessageType = %u\n",
					network_message_in->user_type,
					network_message_in->message_type);

	
	if (network_message_in->content != NULL){
	
		content_in = network_message_in->content;
		if (!content_in->has_op_result){
			puts("Something went wrong, content doesn't contain op_result");
			return -1;
		}

		printf("op_result = %d\n", content_in->op_result);//debug
		if(content_in->op_result < 0){
			//SUCCESS
			printf("error. something happened\n");
		}else if(content_in->op_result == 0){
			printf("no photos have such keyword\n");
		}
		//retorna o numero de photos que têm esse ID
		return content_in->op_result;
	}

	return -1;//se chegar aqui é porque nao correu bem
}

/*	deletes a photo identified by the id_photo
 *		peer_socket:		file descriptor to communicate with peer
 *		id_photo:			the id of the "photo" to be deleted
 *
 *	 returns:
 *		   0:				photo doesn't exist
 *		   1:				on success
 *		  -1:				if error occurs
 */
int gallery_delete_photo(int peer_socket, uint32_t id_photo){
	//outbound
	NetworkMessage network_message_out;
	network_message__init(&network_message_out);
	unsigned len_out;
	char *buf_out;
	Content content_out;
	//inbound
	NetworkMessage *network_message_in;
	Content *content_in;
	//other
	int d_in = 0;
	char buf_in[MAX_MSG_SIZE];

	//populate and allocate outbound message
	network_message_out.user_type = 2;//CLIENT = 2
	network_message_out.message_type = 3;//OP = 3
	

	content__init(&content_out);
	content_out.has_op_type = true;
	content_out.op_type = 2;//2 = DELETE
	content_out.has_id_photo = true;
	content_out.id_photo = id_photo;

	network_message_out.content = &content_out;

	len_out = network_message__get_packed_size(&network_message_out);
	buf_out = (char *) malloc(len_out);
	if (buf_out == NULL){
		perror("malloc");
		exit(-1);
	}
	//pack it and send to gw
	network_message__pack(&network_message_out, (uint8_t *) buf_out);
	

	send(peer_socket, buf_out, len_out, 0);
	printf("\nsent SEARCH to my peer...\n");//debug

	//receive op_result from peer
	d_in = recv(peer_socket, buf_in, MAX_MSG_SIZE, 0);
	printf("received %d bytes from peer\n", d_in);//debug

	if(d_in <= 0){
		printf("Unnexpected number of bytes read.\n");
		exit(-1);
	}

	network_message_in = network_message__unpack(NULL, d_in, (uint8_t *) buf_in);

	//verifications _ debug
	printf("Received message: UserType = %u | MessageType = %u\n",
					network_message_in->user_type,
					network_message_in->message_type);

	
	if (network_message_in->content != NULL){
	
		content_in = network_message_in->content;
		if (!content_in->has_op_result){
			puts("Something went wrong, content doesn't contain op_result");
			return -1;
		}

		printf("op_result = %d\n", content_in->op_result);//debug
		if(content_in->op_result == -1){
			//couldn't remove
			printf("error. failed to remove photo\n");
		}else if(content_in->op_result == 0){
			printf("no photo with that ID was found\n");
		}else if(content_in->op_result == 1){
			printf("successfully removed the photo");
		}
		return content_in->op_result;
	}

	return -1;//se chegar aqui é porque nao correu bem
}

/*	retrieves the photo_name associated with this ID
 *		peer_socket:		file descriptor to communicate with peer
 *		id_photo:			the id of the "photo"
 *		photo_name:			pointer to where the name will be stored
 *
 *	 returns:
 *		   0:				photo doesn't exist
 *		   1:				on success
 *		  -1:				if error occurs
 */
int gallery_get_photo_name(int peer_socket, uint32_t id_photo, char **photo_name){
	//outbound
	NetworkMessage network_message_out;
	network_message__init(&network_message_out);
	unsigned len_out;
	char *buf_out;
	Content content_out;
	//inbound
	NetworkMessage *network_message_in;
	Content *content_in;
	//other
	int d_in = 0;
	char buf_in[MAX_MSG_SIZE];

	//populate and allocate outbound message
	network_message_out.user_type = 2;//CLIENT = 2
	network_message_out.message_type = 3;//OP = 3
	

	content__init(&content_out);
	content_out.has_op_type = true;
	content_out.op_type = 4;//4 = GET_NAME
	content_out.has_id_photo = true;
	content_out.id_photo = id_photo;

	network_message_out.content = &content_out;

	len_out = network_message__get_packed_size(&network_message_out);
	buf_out = (char *) malloc(len_out);
	if (buf_out == NULL){
		perror("malloc");
		exit(-1);
	}
	//pack it and send to gw
	network_message__pack(&network_message_out, (uint8_t *) buf_out);
	

	send(peer_socket, buf_out, len_out, 0);
	printf("\nsent GET_NAME to my peer...\n");//debug

	//receive op_result from peer
	d_in = recv(peer_socket, buf_in, MAX_MSG_SIZE, 0);//não sei se dá para reutilizar o buf_in, por isso uso um novo
	printf("received %d bytes from peer\n", d_in);//debug

	if(d_in <= 0){
		printf("Unnexpected number of bytes read.\n");
		exit(-1);
	}

	network_message_in = network_message__unpack(NULL, d_in, (uint8_t *) buf_in);

	//verifications _ debug
	printf("Received message: UserType = %u | MessageType = %u\n",
					network_message_in->user_type,
					network_message_in->message_type);

	
	if (network_message_in->content != NULL){
	
		content_in = network_message_in->content;
		if (!content_in->has_op_result){
			puts("Something went wrong, content doesn't contain op_result");
			return -1;
		}

		printf("op_result = %d\n", content_in->op_result);//debug
		if(content_in->op_result == -1){
			//couldn't remove
			printf("error. something went wrong\n");
		}else if(content_in->op_result == 0){
			printf("no photo with that ID was found\n");
		}else if(content_in->op_result == 1){
			printf("successfully found the photo");
		}
		return content_in->op_result;
	}

	return -1;//se chegar aqui é porque nao correu bem
}

/*	downloads a photo from the server
 *		peer_socket:		file descriptor to communicate with peer
 *		id_photo:			the id of the "photo"
 *		file_name:			name of the file where the photo will be saved
 *
 *	 returns:
 *		   0:				photo doesn't exist
 *		   1:				on success
 *		  -1:				if error occurs
 */
int gallery_get_photo(int peer_socket, uint32_t id_photom, char *file_name){
	puts("We didn't include this in our implementation.");//debug
	return -1;
}


