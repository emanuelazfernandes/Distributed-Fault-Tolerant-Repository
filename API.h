#ifndef API_H
#define API_H

//libraries
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
//import structures from protocol buffers
#include "API.pb-c.h"

//MACROS
#define INIT_PORT 3000
#define MAX_MSG_SIZE 4096//limita o tamanho máximo dos protocol buffers lidos

typedef struct udp_message_{
	char addr[20];//123.567.901.345\0 => 16 max
	int port;
} udp_message;//debug _ ja nao usamos isto pois nao ???


//functions definitions
int gallery_connect(char *host, in_port_t port);
uint32_t gallery_add_photo(int peer_socket, char **file_name);
int gallery_add_keyword(int peer_socket, uint32_t id_photo, char *keyword);
int gallery_search_photo(int peer_socket, char *keyword, uint32_t **id_photos);
int gallery_delete_photo(int peer_socket, uint32_t id_photo);
int gallery_get_photo_name(int peer_socket, uint32_t id_photo, char **photo_name);
int gallery_get_photo(int peer_socket, uint32_t id_photom, char *file_name);

void print_buffer(uint8_t *buf, int size_bytes);


#endif //API_H
