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


typedef struct {
	int sock;
	int peer_num;
	int *collect;
} acpt_params;

typedef struct {
	int sock;
	int *collect;
} recv_params;

typedef struct {
	int sock;
	char *file_name;
	int chnk_idx;	
} send_params;


pthread_mutex_t mtx;
void *routine(void *arg);
void *exception(void *arg);
void *acpt_peer(void *arg);
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
	pthread_t t_id, e_id;
	clnt_params *param = (clnt_params *) malloc(sizeof(clnt_params));
	param->clnt_sock = clnt_sock;
	param->clnt_port = clnt_port;
	pthread_create(&t_id, NULL, routine, (void *)param);
	pthread_join(t_id, NULL);
	pthread_detach(t_id);
	
	// free variable
	close(clnt_sock);
	return 0;
}

void *exception (void *arg) {
	char buf[BUF_SIZE];

	while(1) {
		sleep(1);
		pthread_mutex_lock(&mtx);
		fgets(buf, BUF_SIZE, stdin);
		if(strstr(buf, "q") != NULL) {
			printf("pressed quit\n");
		}
		pthread_mutex_unlock(&mtx);
	}

}

void *routine(void *arg) {
	int i;
	clnt_params param = *(clnt_params *) arg;

	int clnt_sock = param.clnt_sock;
	int clnt_port = param.clnt_port;

	int serv_sock;
	struct sockaddr_in serv_addr;

	int clnt_stage = 0;
	int chnk_idx;

	char file_name_org[BUF_SIZE];

	int peer_num;
	char **peer_addr_list = (char**) malloc ( sizeof(char*) * MAX_CLNT );
	int *peer_port_list = (int *)malloc(sizeof(int) * MAX_CLNT );
	for(i = 0; i < MAX_CLNT; i++){
	    peer_addr_list[i] = (char*) malloc ( sizeof(char) * BUF_SIZE );
	}
	int peer_collected = 0;

	char buf[BUF_SIZE];
	int str_len = 0;
	while(1) {
		pthread_mutex_lock(&mtx);
		//printf("stage %d\n", clnt_stage);

		if(clnt_stage == 0) {
			read(clnt_sock, buf, BUF_SIZE);
			int file_num = atoi(buf);

			printf("\n****** File list (num of %d) ******\n", file_num);
			int i;
			for(i = 0; i < file_num; i++) {
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
			write(clnt_sock, buf, BUF_SIZE);

			clnt_stage += 1;
		} else if(clnt_stage == 4) {
			read(clnt_sock, buf, BUF_SIZE);
			peer_num = atoi(buf);

			printf("\n*** Client ip list (num of %d) ***\n", peer_num);
			for(i = 0; i < peer_num; i++) {
				read(clnt_sock, buf, BUF_SIZE);

				printf("%d: %s\n", i, buf);

				// get ip and port from buffer
				char ip_tmp[BUF_SIZE];
				char port_tmp[BUF_SIZE];

				strcpy (ip_tmp, strtok (buf, " "));
				strcpy (port_tmp, strtok (NULL, " "));

				peer_addr_list[i] = ip_tmp;
				peer_port_list[i] = atoi (port_tmp);

				printf ("peer_addr_list: %s\n", peer_addr_list[i]);
				printf ("peer_port_list: %d\n", peer_port_list[i]);
			}
			printf("*********************************\n");
			clnt_stage += 1;
		} else if(clnt_stage == 5) {
			char file_name[2*BUF_SIZE] = "./repo/";

			// get file name
			memset(&buf, 0, BUF_SIZE);
			read(clnt_sock, buf, BUF_SIZE);
			strcpy(file_name_org, buf);
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
				printf("%s", buf);
				fwrite(buf, 1, str_len, recv_file);
			}

			fflush(recv_file);
			fclose(recv_file);
			
			// complete message
			sprintf(buf, "complete file\n");
			write(clnt_sock, buf, BUF_SIZE);

			printf("finished get_data\n");

			clnt_stage += 1;
		} else if(clnt_stage == 6) {
			// thread for accept (recv file)
			pthread_t acpt_t;

			acpt_params *acpt_param = (acpt_params *) malloc (sizeof (acpt_params));
			acpt_param->sock = serv_sock;
			acpt_param->peer_num = peer_num;
			acpt_param->collect = &peer_collected;

			if(pthread_create(&acpt_t, NULL, acpt_peer, (void *)acpt_param) != 0) {
		          	printf("...thread create error\n");
		          	close(serv_sock);
		          	continue;
			}
			pthread_detach(acpt_t);

			// connect to other peers
			for (i = 0; i < peer_num; i++) {
				// Socket instiantia
				int peer_sock;
				struct sockaddr_in peer_addr;
				peer_sock = socket(AF_INET, SOCK_STREAM, 0);

				memset(&peer_addr, 0, sizeof(peer_addr));
				peer_addr.sin_family = AF_INET;
				peer_addr.sin_addr.s_addr = inet_addr(peer_addr_list[i]);
				peer_addr.sin_port = htons(peer_port_list[i]);

				if (connect (peer_sock, (struct sockaddr*) &peer_addr, sizeof (peer_addr)) == -1) {
					printf("connect() error!");
					exit(1);
				}

				// thread send file
				pthread_t send_t;
				send_params *send_param = (send_params *) malloc (sizeof (send_params));
				send_param->sock = peer_sock;
				send_param->file_name = file_name_org;
				send_param->chnk_idx = chnk_idx;

				if(pthread_create(&send_t, NULL, send_file, (void *)send_param) != 0) {
			        	printf("...thread create error\n");
			        	close(peer_sock);
			        	continue;
				}
				pthread_detach(send_t);

			}
			clnt_stage += 1;
		} else if(clnt_stage == 7) {
			// wait for peer sending files
			//printf("peer_collected %d\n", peer_collected);

			if(peer_collected >= peer_num) {
				clnt_stage +=1;
			}
		} else if(clnt_stage == 8) {
			sleep(1);
			printf("%s\n", file_name_org);

			// concat all the files
			FILE* concat_file = fopen(file_name_org, "wb");
			if (concat_file == NULL)
			{
				printf("[concat_file] file error\n");
				return NULL;
			}

			for(i = 0; i < peer_num+1 ; i++) {
				char chnk_name[2*BUF_SIZE] = "./repo/\0";
				char tmp[2*BUF_SIZE];
				sprintf(tmp, "%s_%d", file_name_org, i);
				strcat(chnk_name, tmp);

				FILE* chnk_file;
				chnk_file = fopen(chnk_name, "rb");

				// file size
				fseek(chnk_file, 0, SEEK_END);
				int file_size = ftell(chnk_file);

				// file content
				fseek(chnk_file, 0, SEEK_SET);
				int count = 0, str_len = BUF_SIZE;
				while((count < file_size) && (str_len == BUF_SIZE)) {
					str_len = fread(buf, 1, BUF_SIZE, chnk_file);
					printf("%s\n", buf);
					count += str_len;
					fwrite(buf, 1, str_len, concat_file);
				}

				fflush(chnk_file);
				fclose(chnk_file);
			} 

			fflush(concat_file);
			fclose(concat_file);
			printf("\n\n finished getting %s\n", file_name_org);
			break;
		}

		pthread_mutex_unlock(&mtx);
	}

	close(clnt_sock);
	return NULL;
}

void *acpt_peer(void *arg) {
	int acpt_num = 0;
	char buf[BUF_SIZE];

	acpt_params param = *(acpt_params *) arg;

	while (1) {
		struct sockaddr_in clnt_addr;
		socklen_t clnt_addr_sz = sizeof(clnt_addr);

		int clnt_sock = accept(param.sock, (struct sockaddr*)&clnt_addr, &clnt_addr_sz);
		if(clnt_sock < 0) {
			printf("...client accept error");
			exit(1);
		} else {
			acpt_num += 1;
		}

		pthread_t tid;
		recv_params *recv_param = (recv_params *) malloc (sizeof (recv_params));
		recv_param->sock = clnt_sock;
		recv_param->collect = param.collect;

		if(pthread_create(&tid, NULL, recv_file, (void *)recv_param) != 0) {
		          printf("...thread create error\n");
		          close(clnt_sock);
		          continue;
		}
		pthread_detach(tid);

		// acpt_num 
		if(acpt_num >= param.peer_num) break;
	}

	//printf("peers all accpeted\n");
	close(param.sock);
	free(arg);
	return NULL;
}

void *recv_file(void *arg) {
	recv_params param = *(recv_params *) arg;
	char buf[BUF_SIZE];

	// recv file name
	char file_name[2*BUF_SIZE];
	read(param.sock, buf, BUF_SIZE);
	strcpy(file_name, buf);

	// recv file size
	read(param.sock, buf, BUF_SIZE);
	int file_size = atoi(buf);
	printf("[recv_file] file_name %s (%d size)\n", file_name, file_size);
	
	// open file.
	FILE* recv_file = fopen(file_name, "w");
	if (recv_file == NULL)
	{
		printf("[recv_file] file error\n");
		fclose(recv_file);
		free (arg);
		return NULL;
	}

	// get file content
	int count = 0, str_len = BUF_SIZE;
	while((count < file_size) && (str_len == BUF_SIZE)) {
		str_len = read(param.sock, buf, BUF_SIZE);
		printf("%s", buf);
		count += str_len;
		fwrite(buf, 1, str_len, recv_file);
	}

	printf("\ncollected %s\n", file_name);
	*param.collect += 1;
	fflush(recv_file);
	fclose(recv_file);
	close(param.sock);
	free (arg);
	return NULL;
}


void *send_file(void *arg) {
	send_params param = *(send_params *) arg;
	char buf[BUF_SIZE];

	// send file name
	char file_name[2*BUF_SIZE] = "./repo/\0";
	char *file_name_org = param.file_name;
	int chnk_idx = param.chnk_idx;
	sprintf(buf, "%s_%d", file_name_org, chnk_idx);
	strcat(file_name, buf);
	write (param.sock, file_name, BUF_SIZE);

	// open file
	FILE* send_file;
	send_file = fopen(file_name, "r");
	if (send_file == NULL) {
		printf("[send_file] file error\n");
		free (arg);
		return NULL;
	}

	// send file size
	fseek(send_file, 0, SEEK_END);
	int file_size = ftell(send_file);
	sprintf(buf, "%d", file_size);
	write (param.sock, buf, BUF_SIZE);

	// send file content
	fseek(send_file, 0, SEEK_SET);
	int count = 0, str_len = BUF_SIZE;
	while((count < file_size) && (str_len == BUF_SIZE)) {
		str_len = fread(buf, 1, BUF_SIZE, send_file);
		printf("%s\n", buf);
		count += str_len;
		write(param.sock, buf, str_len);
	}

	fflush(send_file);
	fclose(send_file);
	printf("sent %s\n", file_name);
	close(param.sock);
	free (arg);
	return NULL;
}
