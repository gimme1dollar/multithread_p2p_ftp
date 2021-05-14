#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUF_SIZE 30

pthread_mutex_t mtx;

int clnt_stage = -1;
void *initial_flow(void *arg);

char **peer_addr_list;
void *recv_file(void *arg);
void *send_file(void *arg);

int main(int argc, char *argv[])
{
	// Argument check
	if (argc != 3) {
		printf("Usage : %s <IP> <port>\n", argv[0]);
		exit(1);
	}
	
	// Socket instiantiation
	int clnt_sock;
	struct sockaddr_in clnt_addr;
	clnt_sock = socket(AF_INET, SOCK_STREAM, 0);  
	
	memset(&clnt_addr, 0, sizeof(clnt_addr));
	clnt_addr.sin_family = AF_INET;
	clnt_addr.sin_addr.s_addr = inet_addr(argv[1]);
	clnt_addr.sin_port = htons(atoi(argv[2]));
	
	if (connect(clnt_sock, (struct sockaddr*)&clnt_addr, sizeof(clnt_addr)) == -1) {
		printf("connect() error!");
		exit(1);
	}

	// Get chunk of files & list of peers from server
	pthread_mutex_init(&mtx, NULL);
	pthread_t t_id;

	clnt_stage = 0;
	pthread_create(&t_id, NULL, initial_flow, (void *)&clnt_sock);
	pthread_join(t_id, NULL);
	pthread_detach(t_id);
	close(clnt_sock);

	// Build flow-network with thread
	int serv_sock;
	struct sockaddr_in serv_addr;

	return 0;
}

void *initial_flow(void *arg) {
	int clnt_sock = *((int *)arg);
	char buf[BUF_SIZE];
	int str_len = 0;

	while(1) {
		pthread_mutex_lock(&mtx);
		//printf("stage %d\n", clnt_stage);

		if(clnt_stage == 0) {
			read(clnt_sock, buf, BUF_SIZE);
			int file_num = atoi(buf);

			printf("\n****** File list (num of %d) ******\n", file_num);
			for(int i = 0; i < file_num; i++) {
				read(clnt_sock, buf, BUF_SIZE);
				printf("%s\n", buf);
			}
			printf("*********************************\n");

			clnt_stage += 1;
		} else if(clnt_stage == 1) {
			// File selection
			int sel;

			printf("Select file number: ");
			fgets(buf, BUF_SIZE, stdin);
			write(clnt_sock, buf, BUF_SIZE);
			printf("\n");

			clnt_stage += 1;
		} else if(clnt_stage == 2) {
			str_len = read(clnt_sock, buf, BUF_SIZE);
			if(str_len > 0) {
				printf("%s\n", buf);

				if( strstr(buf, "complete") != NULL ) {
					printf("\n");
					clnt_stage += 1;
				}
			}
		} else if(clnt_stage == 3) {
			read(clnt_sock, buf, BUF_SIZE);
			int clnt_num = atoi(buf);

			
			printf("\n*** Client ip list (num of %d) ***\n", clnt_num);
			for(int i = 0; i < clnt_num; i++) {
				read(clnt_sock, buf, BUF_SIZE);
				printf("%d: %s\n", i, buf);
			}
			printf("*********************************\n");
			clnt_stage += 1;
		}

		pthread_mutex_unlock(&mtx);
	}
	
	close(clnt_sock);
	return NULL;
}

void *recv_file(void *arg) {
	return NULL;
}
