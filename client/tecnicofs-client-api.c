#include "tecnicofs-client-api.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>

#define DIM 100

int sockfd;
socklen_t clilen, servlen;
struct sockaddr_un serv_addr, client_addr;

char client_name[DIM];
char server_name[DIM];

int setSockAddrUn(char *path, struct sockaddr_un *addr) {
  if (addr == NULL)
    return 0;
  bzero((char *)addr, sizeof(struct sockaddr_un));
  addr->sun_family = AF_UNIX;
  strcpy(addr->sun_path, path);
  return SUN_LEN(addr);
}

int tfsCreate(char *filename, char nodeType){
	char in_buffer[DIM];
    char out_buffer[DIM];

	servlen = setSockAddrUn(server_name, &serv_addr);
	sprintf(out_buffer, "c %s %c", filename, nodeType);
	if(sendto(sockfd, out_buffer, strlen(out_buffer)+1, 0, (struct sockaddr *) &serv_addr, servlen) < 0)
		return -1;

	if(recvfrom(sockfd, in_buffer, sizeof(in_buffer), 0, 0,0) < 0)
		return -1;
	
	if (strcmp(in_buffer, "yes") != 0)
		return -1;

	return 0;
}

int tfsDelete(char *path) {
	char in_buffer[DIM];
    char out_buffer[DIM];
	
	servlen = setSockAddrUn(server_name, &serv_addr);

	sprintf(out_buffer, "d %s", path);

	if(sendto(sockfd, out_buffer, strlen(out_buffer)+1, 0, (struct sockaddr *) &serv_addr, servlen) < 0)
		return -1;

	if(recvfrom(sockfd, in_buffer, sizeof(in_buffer), 0, 0, 0) < 0)
		return -1;
	
	if (strcmp(in_buffer, "yes") != 0)
		return -1;

	return 0;
}

int tfsMove(char *from, char *to) {
	char in_buffer[DIM];
    char out_buffer[DIM];
	
	servlen = setSockAddrUn(server_name, &serv_addr);

	sprintf(out_buffer, "m %s %s", from, to);

	if(sendto(sockfd, out_buffer, strlen(out_buffer)+1, 0, (struct sockaddr *) &serv_addr, servlen) < 0)
		return -1;
	
	if(recvfrom(sockfd, in_buffer, sizeof(in_buffer), 0, 0, 0) < 0)
		return -1;
	
	if (strcmp(in_buffer, "yes") != 0)
		return -1;

	return 0;
}

int tfsLookup(char *path) {
	char in_buffer[DIM];
    char out_buffer[DIM];
	
	servlen = setSockAddrUn(server_name, &serv_addr);

	sprintf(out_buffer, "l %s", path);

	if(sendto(sockfd, out_buffer, strlen(out_buffer)+1, 0, (struct sockaddr *) &serv_addr, servlen) < 0)
		return -1;
	
	if(recvfrom(sockfd, in_buffer, sizeof(in_buffer), 0, 0, 0) < 0)
		return -1;
	
	if (strcmp(in_buffer, "yes") != 0)
		return -1;

	return 0;
}

int tfsPrint(char *name) {
	char in_buffer[DIM];
    char out_buffer[DIM];
	
	servlen = setSockAddrUn(server_name, &serv_addr);

	sprintf(out_buffer, "p %s", name);

	if(sendto(sockfd, out_buffer, strlen(out_buffer)+1, 0, (struct sockaddr *) &serv_addr, servlen) < 0)
		return -1;
	
	if(recvfrom(sockfd, in_buffer, sizeof(in_buffer), 0, 0, 0) < 0)
		return -1;
	
	if (strcmp(in_buffer, "yes") != 0)
		return -1;

	return 0;
}

int tfsMount(char * sockPath) {
	pid_t pid = getpid();
	sprintf(server_name, "%s", sockPath);
	sprintf(client_name, "/tmp/%d", pid);

	if((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0){
		printf("Error: client can't open socket\n");
		return -1;
	}	

	unlink(client_name);

	clilen = setSockAddrUn(client_name, &client_addr);
	if(bind(sockfd, (struct sockaddr *) &client_addr, clilen) < 0){
		printf("Error: client bind error\n");
		return -1;
	}

	return 0;
}

int tfsUnmount() {
	close(sockfd);
  return -1;
}
