#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h>

#include <unistd.h> 
#include <stdio.h> 
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <pthread.h>
#include <dirent.h>

#include "mylib.h"
#include "mytrslib.h"
#include "send_window_lib.h"
#include "recv_window_lib.h"
#include "constant_lib.h"

unsigned int SERVER_PORT, THREAD_PORT, WINDOW_SIZE, TIMEOUT_TIME, TIMEOUT_ADAPT, P_LOSS;
char *FILE_DIR;

int SOCK_FD;
struct sockaddr_in SERVER_ADDR, THREAD_ADDR;
pthread_mutex_t list_mtx;
pthread_t main_thread; 
struct node_t **head;

static void set_serv_addr(struct sockaddr_in *addr, unsigned int port)
{
	memset((void *)addr, 0, sizeof(*addr));
	addr->sin_family = AF_INET;       /* set family addr */
	addr->sin_addr.s_addr = htonl(INADDR_ANY); /* il server accetta pacchetti su una qualunque delle sue interfacce di rete */
	addr->sin_port = htons(port); /* server port number */
}

static void init()
{
	const int buf_size = 20;
	FILE *conf = fopen(SERVER_CONF_FILE, "r");  
	
	if (conf == NULL)
		error_msg("error in init");
	FILE_DIR = malloc(buf_size);
	if (FILE_DIR == NULL)
		error_msg("errore in malloc");
		
	if (CONFIG_U(conf, SERVER_PORT) == EOF)
		error_msg("error in fscanf");
	if (CONFIG_U(conf, THREAD_PORT) == EOF)
		error_msg("error in fscanf");	
	if (CONFIG_S(conf, FILE_DIR) == EOF)
		error_msg("error in fscanf");	
	if (CONFIG_U(conf, WINDOW_SIZE) == EOF)
		error_msg("error in fscanf");
	if (CONFIG_U(conf, TIMEOUT_TIME) == EOF)
		error_msg("error in fscanf");
	if (CONFIG_U(conf, TIMEOUT_ADAPT) == EOF)
		error_msg("error in fscanf");	
	if (CONFIG_U(conf, P_LOSS) == EOF)
		error_msg("error in fscanf");
	fclose(conf);				
	
	FILE_DIR[strlen(FILE_DIR) -1] = '\0';
	
	set_serv_addr(&SERVER_ADDR, SERVER_PORT);
	set_serv_addr(&THREAD_ADDR, THREAD_PORT);
	if ((SOCK_FD = socket(AF_INET, SOCK_DGRAM , 0)) < 0) /* create socket */
		error_msg("error in socket");
	
	if (bind(SOCK_FD , (struct sockaddr *) &SERVER_ADDR, sizeof(SERVER_ADDR)) < 0) /* assign address to socket */
		error_msg("error in bind");
	
	if (pthread_mutex_init(&list_mtx, NULL))
		error_msg("error in mutex init\n");
	
	main_thread = pthread_self();		
}

static void init_thread_socket(int * sockfd, struct sockaddr_in *my_addr)
{
	if ((*sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) /* create socket */
    error_msg("error in socket");
    
    int i = 1;
    if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&i, sizeof(int)) < 0)
		error_msg("error in setsockopt reuseaddr\n");
	
	if (bind(*sockfd, (struct sockaddr *)my_addr, sizeof(*my_addr)) < 0) /* assign address to socket */
		error_msg("error in bind");
}

static char *list()
{
	DIR *dir = NULL;
	struct dirent *dir_file;
	char *s = NULL;
	int s_size = 0;
	//s = calloc(1, 0);
	dir = opendir(FILE_DIR);
	
	fflush(stdout);
	while ((dir_file = readdir(dir)) != NULL)
	{
		if (strcmp(dir_file->d_name, ".") != 0 && strcmp(dir_file->d_name, "..") != 0)
		{
			s = realloc(s, s_size + strlen(dir_file->d_name) + 2);
			strcpy(s + s_size, dir_file->d_name);
			strcat(s, "\n");
			s_size = s_size + strlen(dir_file->d_name) + 1;
		}
	}
	closedir(dir);
	return s;
}

static void thread_get(struct td_data td, char *con_msg)
{
	char recv_buf[MAX_PDU];
	struct drand48_data seed_buf;
	srand48_r(time(NULL), &seed_buf);
		
	if (!exist_file(td.request + 4, FILE_DIR)) //request error
	{
		snprintf(con_msg, 100, ERR_MSG_FORMAT, THREAD_PORT, TIMEOUT_TIME, P_LOSS, CON_ERR_FILE_NOT_EXIST);
		send_control_msg(td.sock_fd, &(td.client), con_msg, recv_buf, "FIN_ACK", TIMEOUT_TIME, P_LOSS, &seed_buf);
	}
	else
	{	
		char *path = alloca(strlen(td.request) + strlen(FILE_DIR) -4);
		str_cat(path, FILE_DIR, td.request + 4);
		int fd = rd_open_file(path);
		off_t fsize = file_size(fd);	
		char *file = map_file(fd,fsize,0);
		DEBUG_STAMP("FILE LOADED\n");
		
		if (send_control_msg(td.sock_fd, NULL, con_msg, recv_buf, "CON_ACK", TIMEOUT_TIME, 
			P_LOSS, &seed_buf) == -1) //send connection accept msg with connection parameter with ARQ
			goto free_resources_and_exit;
				
		if (rel_send(td.sock_fd, file, fsize, recv_buf, TIMEOUT_TIME, TIMEOUT_ADAPT, WINDOW_SIZE, &seed_buf, P_LOSS, 0) == -1)
			goto free_resources_and_exit;
				
		send_control_msg(td.sock_fd, NULL, "CLOSE_CONN", recv_buf, "FIN_ACK",
						 TIMEOUT_TIME, P_LOSS, &seed_buf);//send close msg to the client
	
		if(munmap(file, fsize) == -1)
			error_td_msg("error munmap\n");
		close(fd); //close file descriptor
	}

free_resources_and_exit:
	if (close(td.sock_fd) == -1)
		error_td_msg("error in close socket");		 
}

static void thread_put(struct td_data td, char *con_msg, int request_already_exist)
{
	char recv_buf[MAX_PDU];
	struct drand48_data seed_buf;
	srand48_r(time(NULL), &seed_buf);
	struct timeval conn_timeout = {.tv_sec = CONN_TIMEOUT, .tv_usec = 0};
		
	if (request_already_exist) // request error
	{
		unlock(&list_mtx);
		snprintf(con_msg, 100, ERR_MSG_FORMAT, THREAD_PORT, TIMEOUT_TIME, P_LOSS, CON_ERR_FILE_ALREADY_UPLOADED);
		send_control_msg(td.sock_fd, NULL, con_msg, recv_buf, "FIN_ACK", TIMEOUT_TIME, P_LOSS, &seed_buf);
	}
	else 
	{
		char *path = alloca(strlen(td.request) + strlen(FILE_DIR) -4);
		str_cat(path, FILE_DIR, td.request + 4);
			
		//send connection accept msg with connection parameter with ARQ
		if (send_control_msg(td.sock_fd, NULL, con_msg, recv_buf, "CON_ACK", TIMEOUT_TIME, P_LOSS, &seed_buf) == -1)
			goto free_resources_and_exit;
		
		if (setsockopt(td.sock_fd, SOL_SOCKET, SO_RCVTIMEO, (void *)&conn_timeout, sizeof(struct timeval)) < 0)
			error_td_msg("setsockopt failed\n");
				
		if (rel_recv(td.sock_fd, recv_buf, path, WINDOW_SIZE, &seed_buf, P_LOSS, 0) == -1)
			goto free_resources_and_exit;
					
		send_fin_ack(td.sock_fd, recv_buf, MAX_PDU, P_LOSS, &seed_buf);
	}
	
free_resources_and_exit:	
	sleep(FIN_TIMEOUT);	//wait FIN_TIMEOUT seconds before closing the socket file descriptor
	if (close(td.sock_fd) == -1)
		error_td_msg("error in close socket");
}

static void thread_list(struct td_data td, char *con_msg)
{	
	char recv_buf[MAX_PDU];
	struct drand48_data seed_buf;
	srand48_r(time(NULL), &seed_buf);
	char *file_list = list();
	off_t fsize = strlen(file_list);
	
	//send connection accept msg with connection parameter with ARQ
	if (send_control_msg(td.sock_fd, NULL, con_msg, recv_buf, "CON_ACK", TIMEOUT_TIME, P_LOSS, &seed_buf) == -1)
		goto free_resources_and_exit;
		
	if (rel_send(td.sock_fd, file_list, fsize, recv_buf, TIMEOUT_TIME, TIMEOUT_ADAPT, WINDOW_SIZE, &seed_buf, P_LOSS , 0) == -1)
		goto free_resources_and_exit;
			
	send_control_msg(td.sock_fd, NULL, "CLOSE_CONN", recv_buf, "FIN_ACK", TIMEOUT_TIME, P_LOSS, &seed_buf);//send close msg to the client
	
free_resources_and_exit:	
	free(file_list);
	if (close(td.sock_fd) == -1)
		error_td_msg("error in close socket");
}

void *thread(void *arg)
{
	struct node_t *td_node = arg;
	struct td_data td = td_node->thread_data;
	
	DEBUG_STAMP("THREAD STARTED\n request: %s tid %lu \n",td.request,td.tid);
	
	init_thread_socket(&td.sock_fd, &THREAD_ADDR);
	
	if (connect(td.sock_fd, (struct sockaddr *)&(td.client), sizeof(td.client)) == -1) //thread receve only his client messages
		error_td_msg("error in connect");
	
	char *con_msg = alloca(100);
	snprintf(con_msg, 100, CON_MSG_FORMAT, THREAD_PORT, WINDOW_SIZE, TIMEOUT_TIME, TIMEOUT_ADAPT, P_LOSS);	
	
	if (strncmp(td.request, "get", 3) == 0)
	{
		lock(&list_mtx);
		add_next(head, td_node); //insert thread in working thread list
		unlock(&list_mtx);
		
		thread_get(td,con_msg);
	}
	else if (strncmp(td.request, "put", 3) == 0)
	{
		int req_already_exist;
		lock(&list_mtx);
		req_already_exist = exist_request(*head, td.request);
		add_next(head, td_node); //insert thread in working thread list
		unlock(&list_mtx);
		
		thread_put(td, con_msg, req_already_exist);
	}
	else if (strncmp(td.request, "list", 4) == 0)
	{
		lock(&list_mtx);
		add_next(head, td_node); //insert thread in working thread list
		unlock(&list_mtx);
		
		thread_list(td,con_msg);
	}
	//printf("fine trasmissione request %s  tid: %lu\n", td.request, td.tid);
	
	free(td.request);
	lock(&list_mtx);
	remove_thread(head, td.tid);  //remove the thread from main_thread's list of working thread 
	unlock(&list_mtx);
	
	pthread_exit(0);	
}

static void start_daemon()
{
	char buf[MAX_PDU+1];
	int msg_size;
	struct node_t *threads = NULL;
	pthread_attr_t td_attr;
	head = &threads;
	
	if (pthread_attr_init(&td_attr))
		error_msg("error in pthread attr init");
				
	if (pthread_attr_setdetachstate(&td_attr, PTHREAD_CREATE_DETACHED) )//free allocated memory after thread exit
		error_msg("error in pthread attr_setdetachstate");
	
	while (1)
	{
		struct sockaddr_in client_addr;
		unsigned int client_addr_len = sizeof(client_addr); 
		if ((msg_size = (recvfrom(SOCK_FD, buf, MAX_PDU, 0, (struct sockaddr *)&client_addr, &client_addr_len))) < 0)
			error_msg("error in recvfrom");
		
		buf[msg_size] = '\0';
		DEBUG_STAMP("porta client: %d  indirizzo %s ricevuto messaggio : %s\n", client_addr.sin_port,
					inet_ntoa(client_addr.sin_addr), buf);	
		fflush(stdout);
	
		if (strncasecmp("get", buf, 3) == 0 || strncasecmp("put", buf, 3) == 0
			|| strncasecmp("list", buf, 4) == 0)  //new request create new thread
		{	
			struct node_t * node = create_node();
			node->thread_data.client = client_addr;
			
			if ((node->thread_data.request = malloc(msg_size + 1)) == NULL)
				error_msg("error in malloc");	
			memcpy(node->thread_data.request, buf, msg_size + 1);
			
			if (pthread_create(&(node->thread_data.tid), &td_attr,thread, (void *)node)) //create thread
				error_msg("error in thread_create");	
		}
		else
		{
			DEBUG_STAMP("MSG NOT RECONISED\n");	
		}
  }
}

int main (int argc, char * argv[])
{
	argc = argc;
	argv = argv;	
	init();
	start_daemon();
	exit(EXIT_SUCCESS);	
}
