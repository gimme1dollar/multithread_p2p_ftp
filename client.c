#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_CLNT 10
#define BUF_SIZE 30
#define UNT_FILE 128*8

typedef struct {
	int clnt_sock;
	int clnt_port;
} clnt_params;

pthread_mutex_t mtx;
void *t_function(void *arg);

void *recv_file(void *arg);
void *send_file(void *arg);

int main(int argc, char *argv[])
{
	// Argument check
	if (argc != 4) {
		printf("Usage : %s <serv_IP> <serv_port> <clnt_port>\n", argv[0]);
		exit(1);
	}
	
	// Setting
	int clnt_port = atoi(argv[3]);
	if(clnt_port == 0) {
		printf("client port can't be 0\n");
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
	pthread_create(&t_id, NULL, t_function, (void *)&clnt_sock);
	pthread_detach(t_id);

	// free variable
	close(clnt_sock);
	return 0;
}

void *t_function(void *arg) {
	clnt_params param = *(clnt_params *) arg;

	int clnt_sock = param.clnt_sock;
	int clnt_port = param.clnt_port;

	int clnt_stage = 0;
	int chnk_idx;

	char **peer_addr_list = (char**) malloc ( sizeof(char*) * MAX_CLNT );
	for(int i = 0; i < MAX_CLNT; i++){
	    peer_addr_list[i] = (char*) malloc ( sizeof(char) * BUF_SIZE );
	}

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
			// Socket instiantiation
			int serv_sock;
			struct sockaddr_in serv_addr;
			serv_sock = socket(AF_INET, SOCK_STREAM, 0);  
	
			memset(&serv_addr, 0, sizeof(serv_addr));
			serv_addr.sin_family = AF_INET;
			serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
			printf("%d\n", clnt_port);
			serv_addr.sin_port = htons(clnt_port);

			sprintf(buf, "%s", "error");

			// Socket binding
			if (bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1) {
				printf("bind() error\n");
				write(clnt_sock, buf, BUF_SIZE);
			}
	
			// Socket listening
			if (listen(serv_sock, 5) == -1) {
				printf("listen() error\n");
				write(clnt_sock, buf, BUF_SIZE);
			}

			// send port info to server if no error
			sprintf(buf, "%d", clnt_port);
			printf("%s", buf);
			write(clnt_sock, buf, BUF_SIZE);

			clnt_stage += 1;
		} else if(clnt_stage == 4) {
			read(clnt_sock, buf, BUF_SIZE);
			int clnt_num = atoi(buf);

			
			printf("\n*** Client ip list (num of %d) ***\n", clnt_num);
			for(int i = 0; i < clnt_num; i++) {
				read(clnt_sock, buf, BUF_SIZE);
				
				printf("%d: %s\n", i, buf);
			}
			printf("*********************************\n");
			clnt_stage += 1;
		} else if(clnt_stage == 5) {
			char file_name[2 * BUF_SIZE] = "./repo/";

			// get file name
			memset(&buf, 0, BUF_SIZE);
			read(clnt_sock, buf, BUF_SIZE);
			strcat(file_name, buf);

			// get index of my chunk
			memset(&buf, 0, BUF_SIZE);
			read(clnt_sock, buf, BUF_SIZE);
			chnk_idx = atoi(buf);
			printf("Chunk idx: %d\n", chnk_idx);
			strcat(file_name, "_");
			strcat(file_name, buf);

			// open file to be written
			printf("File name: %s\n", file_name);
			FILE* recv_file = fopen(file_name, "wb");
			if (recv_file == NULL)
			{
				fclose(recv_file);
			}

			//get chunk content
			int count = 0, str_len = BUF_SIZE;
			while((count < UNT_FILE) && (str_len == BUF_SIZE)) {
				memset(&buf, 0, BUF_SIZE);
				str_len = read(clnt_sock, buf, BUF_SIZE);
				count += str_len;
				fwrite(buf, 1, str_len, recv_file);
			}
			
		
			fflush(recv_file);
			fclose(recv_file);

			printf("finished get_data\n");	

			clnt_stage += 1;
		} else if(clnt_stage == 6) {
			// connect to other peers
		} 

		pthread_mutex_unlock(&mtx);
	}
	
	close(clnt_sock);
	return NULL;
}
