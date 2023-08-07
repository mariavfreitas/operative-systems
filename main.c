#include "fs/operations.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <strings.h>
#include <pthread.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#define MAX_COMMANDS 150000
#define MAX_INPUT_SIZE 100
#define DIM 100

//novas variaveis
int sockfd;
struct sockaddr_un server_addr;
char socketname[DIM];

int numberThreads = 0;

struct timeval ending_time, initial_time;

char filepath[MAX_INPUT_SIZE];
char outputfile[MAX_INPUT_SIZE];

void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}

int writeOutput(char *outputfile){
  FILE *outputfp;
  outputfp = fopen(outputfile, "w");

  if(outputfp == NULL){
    printf("Error: accessing output file\n");
    return -1;
  }

  print_tecnicofs_tree(outputfp);

  if(fclose(outputfp) != 0){
    printf("Error: closing access to output file\n");
    return -1;    
  }
  return 0;
}

void* applyCommands(){
  while(1) {

    socklen_t addrlen;
    struct sockaddr_un client_addr;
    char in_buffer[DIM];
    char out_buffer[DIM];

    int c;

    memset(in_buffer, 0, sizeof(char) * DIM);
    memset(out_buffer, 0, sizeof(char) * DIM);

    addrlen = sizeof(struct sockaddr_un);
    c = recvfrom(sockfd, in_buffer, sizeof(in_buffer)-1, 0, (struct sockaddr *)&client_addr, &addrlen);

    if(c <= 0){
      continue;
    }

    in_buffer[c] = '\0';

    char token;
    char name[MAX_INPUT_SIZE], type[MAX_INPUT_SIZE];
    char name_move[MAX_INPUT_SIZE], type_move[MAX_INPUT_SIZE];

    int numTokens = sscanf(in_buffer, "%c %s %s", &token, name, type);

    if (numTokens < 2) {
        fprintf(stderr, "Error: invalid command in Queue\n");
        exit(EXIT_FAILURE);
    }

    int searchResult;
    switch (token) {
        case 'c':
            switch (type[0]) {
                case 'f':
                    if (strcmp(&type[1], "\0") == 0) {
                        if (create(name, T_FILE) == 0) {
                          c = sprintf(out_buffer, "yes");
                          sendto(sockfd, out_buffer, c+1, 0, (struct sockaddr *)&client_addr, addrlen); 
                        } else {
                          c = sprintf(out_buffer, "no");
                          sendto(sockfd, out_buffer, c+1, 0, (struct sockaddr *)&client_addr, addrlen);   
                        } 
                    } else {
                          c = sprintf(out_buffer, "no");
                          sendto(sockfd, out_buffer, c+1, 0,  (struct sockaddr *)&client_addr, addrlen);
                    }
                    break;
                case 'd':
                    if (strcmp(&type[1], "\0") == 0) {
                        if (create(name, T_DIRECTORY) == 0) {
                          c = sprintf(out_buffer, "yes");
                          sendto(sockfd, out_buffer, c+1, 0,  (struct sockaddr *)&client_addr, addrlen); 
                        } else {
                          c = sprintf(out_buffer, "no");
                          sendto(sockfd, out_buffer, c+1, 0,  (struct sockaddr *)&client_addr, addrlen);   
                        } 
                    } else {
                          c = sprintf(out_buffer, "no");
                          sendto(sockfd, out_buffer, c+1, 0,  (struct sockaddr *)&client_addr, addrlen); 
                    }
                    break;
                default:
                    fprintf(stderr, "Error: invalid node type\n");
                    exit(EXIT_FAILURE);
            }
            break;
        case 'l': 
            searchResult = lookup_protect(name);
            if (searchResult >= 0){
              c = sprintf(out_buffer, "yes");
              sendto(sockfd, out_buffer, c+1, 0,  (struct sockaddr *)&client_addr, addrlen);
            } else{
              c = sprintf(out_buffer, "no");
              sendto(sockfd, out_buffer, c+1, 0,  (struct sockaddr *)&client_addr, addrlen); 
            }
            break;
        case 'd':
            if (delete(name) == 0){
              c = sprintf(out_buffer, "yes");
              sendto(sockfd, out_buffer, c+1, 0,  (struct sockaddr *)&client_addr, addrlen);
            } else {
              c = sprintf(out_buffer, "no");
              sendto(sockfd, out_buffer, c+1, 0,  (struct sockaddr *)&client_addr, addrlen);
            }
            break;
        case 'm':
            strcpy(name_move, name);   
            strcpy(type_move, type);
            searchResult = move(name, type);
            if (searchResult >= 0){
              c = sprintf(out_buffer, "yes");
              sendto(sockfd, out_buffer, c+1, 0,  (struct sockaddr *)&client_addr, addrlen);
            } else {
              c = sprintf(out_buffer, "no");
              sendto(sockfd, out_buffer, c+1, 0,  (struct sockaddr *)&client_addr, addrlen);
            }
            break;
        case 'p':
            searchResult = writeOutput(name);
            if (searchResult == 0){
              c = sprintf(out_buffer, "yes");
              sendto(sockfd, out_buffer, c+1, 0,  (struct sockaddr *)&client_addr, addrlen);
            } else {
              c = sprintf(out_buffer, "no");
              sendto(sockfd, out_buffer, c+1, 0,  (struct sockaddr *)&client_addr, addrlen);
            }
            break;
        default:
              c = sprintf(out_buffer, "default");
              sendto(sockfd, out_buffer, c+1, 0,  (struct sockaddr *)&client_addr, addrlen);
    }
  }
  return 0;
}

int setSockAddrUn(char *path, struct sockaddr_un *addr){
  if( addr == NULL){
    printf("addr nulo\n");
    return 0;
  }
  bzero((char *)addr, sizeof(struct sockaddr_un));
  addr->sun_family = AF_UNIX;
  strcpy(addr->sun_path, path);
  return SUN_LEN(addr);
}

void printTimeTaken(){
    gettimeofday(&ending_time, NULL);
    long seconds = ending_time.tv_sec - initial_time.tv_sec;
    long useconds = ending_time.tv_usec - initial_time.tv_usec;
    double runtime = seconds + useconds*1e-6;
    printf("TecnicoFS completed in [%.4lf] seconds.\n", runtime);
} 

int main(int argc, char* argv[]) {
  socklen_t addrlen;

  if (argc != 3) {
    printf("Error: number of arguments invalid.\n");
    exit(EXIT_FAILURE);
  }

  numberThreads = atoi(argv[1]);
  pthread_t tid[numberThreads]; 
  sprintf(socketname, "%s", argv[2]);

  if (numberThreads <= 0) {
    printf("Error: number of threads invalid.\n");
    exit(EXIT_FAILURE);
  }

  /* init socket */
  if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0 ) { 
    printf("Error: server can't open socket\n"); 
    exit(EXIT_FAILURE); 
  }
  
  unlink(socketname);

  addrlen = setSockAddrUn(socketname, &server_addr);
  if(bind(sockfd, (struct sockaddr *) &server_addr, addrlen) < 0){
    printf("Error: server bind error\n");
    exit(EXIT_FAILURE);
  }

  /* init filesystem */
  init_fs();

	/*  create thread pool */
	for (int i = 0; i < numberThreads; i++){
		if (pthread_create(&tid[i], NULL, applyCommands, (void*) NULL) != 0){
			printf("Error: creating pthread_t\n");
        exit(EXIT_FAILURE);
		}
	}

  /* end thread pool */
	for (int i = 0; i < numberThreads; i++){
		if(pthread_join(tid[i], NULL) != 0){
			printf("Error: closing thread [%d]\n", i);
      exit(EXIT_FAILURE);
		}
	}

  /* destroy socket */
  close(sockfd);

  /* release allocated memory */
  destroy_fs();


  exit(EXIT_SUCCESS);
}
