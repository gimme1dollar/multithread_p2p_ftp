#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/mman.h>

#define BUF_SIZE 30
int main(int argc, char *argv[])
{
	// Argument check
	if (argc != 2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	// Socket instiantiation
	int serv_sock, clnt_sock;
	int str_len;
	char msg[BUF_SIZE];
	char* buf = malloc(sizeof(char)*BUF_SIZE);
	char* con = malloc(sizeof(char)*BUF_SIZE);
	struct sockaddr_in serv_adr, clnt_adr;
	socklen_t adr_sz;

	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(atoi(argv[1]));
	
	// Socket binding
	if (bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr)) == -1)
		printf("bind() error\n");

	// Socket listening
	if (listen(serv_sock, 5) == -1)
		printf("listen() error\n");

	clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);
	if (clnt_sock == -1)
		printf("accepct() error\n");
	else
		printf("client connected");

	while(1) {

	}
}
