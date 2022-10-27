#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>
#include <semaphore.h>
#include "list.h"

#define MAX_LEN 4096

//help from brian fraser socket tutorial
LIST* listSend;
LIST* listReceive;
static int srcPort;
static int destPort;
char* destName;
struct sockaddr_in sinA, sinR;
bool flag = true;
static pthread_mutex_t mutSend;
static pthread_mutex_t mutReceive;
static pthread_cond_t sAvail;
static pthread_cond_t sAvailBuf;
static pthread_cond_t rAvail;
static pthread_cond_t rAvailBuf;
static int socketDescriptor;


void* srcInput(void* socket) {
	char msgBuffer[MAX_LEN];
	while(flag) {
		fgets(msgBuffer, sizeof(msgBuffer), stdin);
		msgBuffer[strcspn(msgBuffer, "\r\n")] = 0;
		if(msgBuffer[0] != '\0'){
			pthread_mutex_lock(&mutSend);
			if(ListCount(listSend) >= LIST_MAX_NUM_NODES) {
				pthread_cond_wait(&sAvailBuf, &mutSend);
			}
			ListAppend(listSend, &msgBuffer);
			pthread_cond_signal(&sAvail);
			pthread_mutex_unlock(&mutSend);
		}
	}
}

void* destInput(void* socket) {
	char msgBuffer[MAX_LEN];
  unsigned int sinLen = sizeof(sinR);
	while(flag) {
		recvfrom(*(int*)socket, msgBuffer, sizeof(msgBuffer), 0, (struct sockaddr*)&sinR, &sinLen);
		pthread_mutex_lock(&mutReceive);
		if(ListCount(listReceive) >= LIST_MAX_NUM_NODES) {
			pthread_cond_wait(&rAvailBuf, &mutReceive);
		}
		if(!flag) {
			pthread_mutex_unlock(&mutReceive);
			return 0;
		}
		ListAppend(listReceive, &msgBuffer);
		pthread_cond_signal(&rAvail);
		pthread_mutex_unlock(&mutReceive);
	}
}


void* srcOutput(void* socket) {
	char msgBuffer[MAX_LEN];
	memset(&msgBuffer, 0, MAX_LEN);
	while(flag) {
		pthread_mutex_lock(&mutSend);
		if(ListCount(listSend) == 0) {
			pthread_cond_wait(&sAvail, &mutSend);
		}
		if(!flag) {
			pthread_mutex_unlock(&mutSend);
			return 0;
		}
		ListFirst(listSend);
		strcpy(msgBuffer, (char*)ListRemove(listSend));
		pthread_cond_signal(&sAvailBuf);
		pthread_mutex_unlock(&mutSend);
    unsigned int sinLen = sizeof(sinR);
		sendto(*(int*)socket, msgBuffer, sizeof(msgBuffer), 0, (struct sockaddr*)&sinR, sinLen);
		if(strcmp(msgBuffer, "!") == 0) { //help from stack overflow
			printf("Shutdown");
			close(socketDescriptor);
			shutdown(socketDescriptor, SHUT_RDWR);
			flag = false;
			pthread_cond_signal(&sAvail);
			pthread_cond_signal(&rAvailBuf);
			pthread_cond_signal(&rAvail);
			exit(0);
			return 0;
		}
	}
}


void* destOutput() {
	char* msgBuffer;
	while(flag) {
		pthread_mutex_lock(&mutReceive);
		if(ListCount(listReceive) == 0) {
			pthread_cond_wait(&rAvail, &mutReceive);
		}
		if(!flag) {
			pthread_mutex_unlock(&mutReceive);
			return 0;
		}
		ListFirst(listReceive);
		msgBuffer = (char*)ListRemove(listReceive);
		pthread_cond_signal(&rAvailBuf);
		pthread_mutex_unlock(&mutReceive);
		printf("%s: ", destName);
		if(strcmp(msgBuffer, "!") == 0) { //help from stack overflow
			printf("has sent a shutdown signal.\n");
			close(socketDescriptor);
			shutdown(socketDescriptor, SHUT_RDWR);
			flag = false;
			pthread_cond_signal(&sAvail);
			pthread_cond_signal(&rAvailBuf);
			pthread_cond_signal(&rAvail);
			exit(0);
			return 0;
		} else{
			printf("%s\n", msgBuffer); 
		}
	}
}


int main(int argc, char* argv[]) {
	if(argc == 4){
		srcPort = atoi(argv[1]);
		destName = argv[2];
		destPort = atoi(argv[3]);
	} else{
		printf("Invalid Input. \nExample: ./s-talk [your port number] [remote machine name] [remote port number]");
		return 0;
	}
		struct hostent* host = gethostbyname(destName);
		if(!gethostbyname(destName)){
			printf("ERROR:\n Host name '%s' does not exist.\n", destName);
			return 0;
		}
		struct in_addr** hostAdds = (struct in_addr**)host->h_addr_list;
		char* destIp = inet_ntoa(*hostAdds[0]);
		sinA.sin_family = PF_INET;
    sinR.sin_family = PF_INET;
		sinA.sin_addr.s_addr = htonl(INADDR_ANY);
    sinA.sin_port = htons(srcPort);
		sinR.sin_port = htons(destPort);
		if(inet_aton(destIp, &sinR.sin_addr) == 0) {
			printf("ERROR:\n Host name '%s' may be wrong\n Address '%s' invalid.\n", destName, destIp);
			return 0;
		}
    socketDescriptor = socket(PF_INET, SOCK_DGRAM, 0); //AF_INET > PF_INET
		if(socketDescriptor == 0) {
			printf("ERROR:\n The source port '%d' cannot create the socket to '%d'.\n Check your ports and try again.", srcPort, destPort);
			return 0;
		}
		if(bind(socketDescriptor, (struct sockaddr*)&sinA, sizeof(struct sockaddr_in)) == -1) {
			printf("ERROR:\n The source port '%d' cannot bind to the socket '%d'.\n Check your ports and try again.",srcPort, destPort);
			return 0;
		}

		listSend = ListCreate();
		listReceive = ListCreate();

		printf("Connected.\n");
    printf("To exit the session, enter '!'\n");

		pthread_mutex_init(&mutSend, NULL);
		pthread_mutex_init(&mutReceive, NULL);
		pthread_cond_init(&sAvail, NULL);
		pthread_cond_init(&sAvailBuf, NULL);
		pthread_cond_init(&rAvail, NULL);
		pthread_cond_init(&rAvailBuf, NULL);
		pthread_t threads[4];
		pthread_create(&threads[0], NULL, &srcInput, &socketDescriptor);
		pthread_create(&threads[1], NULL, &srcOutput, &socketDescriptor);
		pthread_create(&threads[2], NULL, &destInput, &socketDescriptor);
		pthread_create(&threads[3], NULL, &destOutput, NULL);
		pthread_join(threads[0], NULL);
		pthread_join(threads[1], NULL);
		pthread_join(threads[2], NULL);
		pthread_join(threads[3], NULL);

		return 0;
}
