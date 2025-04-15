#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h>

#include <unistd.h> 
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

#define PROFILE

#include "mylib.h"
#include "mytrslib.h"
#include "recv_window_lib.h"
#include "constant_lib.h"
#include "send_window_lib.h"

unsigned int SERVER_PORT;
char *FILE_DIR;
char *SERVER_IP;

struct sockaddr_in SERVER_ADDR;
pthread_mutex_t list_mtx;

static void init()
{
	FILE *conf = fopen(CLIENT_CONF_FILE, "r");  
	if( conf==NULL)
		error_msg("error in init");
	
	int buf_size=20;
	FILE_DIR=malloc(buf_size);
	if(FILE_DIR==NULL)
		error_msg("errore in malloc");
	SERVER_IP=malloc(buf_size);
	if(SERVER_IP==NULL)
		error_msg("errore in malloc");
		
	if(CONFIG_U(conf,SERVER_PORT) == EOF )
	error_msg("error in fscanf");
	if(CONFIG_S(conf,SERVER_IP) == EOF )
	error_msg("error in fscanf");	
	if(CONFIG_S(conf,FILE_DIR) == EOF )
	error_msg("error in fscanf");
		
	fclose(conf);	
	
	FILE_DIR[strlen(FILE_DIR)-1]='\0';
	SERVER_IP[strlen(SERVER_IP)-1]='\0';
	
	set_addr(&SERVER_ADDR,SERVER_IP,SERVER_PORT);		
}

static void set_connection_prop(struct sockaddr_in *thread_addr, unsigned int *window_size, unsigned int *timeout_time, 
								unsigned int *timeout_adapt, unsigned int *p_loss, char *msg)
{	
	unsigned int thread_port;
	if (sscanf(msg,CON_MSG_FORMAT,&thread_port,window_size,timeout_time,timeout_adapt,p_loss)!=5)
		error_msg("errore in scanf");
	set_addr(thread_addr,SERVER_IP,thread_port);   	
}

static unsigned int err_connection_prop(struct sockaddr_in *thread_addr, unsigned int *timeout_time, unsigned int *p_loss, char *msg)
{
    unsigned int thread_port,err;
    if (sscanf(msg, ERR_MSG_FORMAT, &thread_port, timeout_time, p_loss, &err)!= 4)
            error_msg("errore in scanf");
    set_addr(thread_addr,SERVER_IP,thread_port);       
    return err;
}

static void con_error_handler(unsigned int err)
{
	switch(err)
	{
		case CON_ERR_FILE_NOT_EXIST:
			printf("\033File not found\033\n");
			break;
		case CON_ERR_FILE_ALREADY_UPLOADED:
			printf("\033File cannot be uploaded because another user is already uploading it\033\n");
			break;
        default:
			printf("\033An unexpected error occured\033\n");
			break;
    }
}

void *get_file_procedure(void *arg)
{
	#ifdef PROFILE
	struct timespec start_time,end_time;
	get_current_time(&start_time);
	#endif
	struct td_data *td=arg; 
	char  recv_buf[MAX_PDU];
	struct drand48_data seed_buf;
	srand48_r(time(NULL), &seed_buf);

	unsigned int window_size,timeout_time,timeout_adapt,p_loss;
	struct timeval conn_timeout = { .tv_sec=CONN_TIMEOUT, .tv_usec = 0};
	
	char *path = alloca(strlen(td->request) + strlen(FILE_DIR) - 4);
	str_cat(path,FILE_DIR,td->request + 4);
		
	urel_send_msg_to(td->request, strlen(td->request), td->sock_fd, &SERVER_ADDR, sizeof(SERVER_ADDR),
						&seed_buf, 0);	//SEND REQUEST TO SERVER	
	DEBUG_STAMP("request :%s sended\n",td->request);	
			
	if(urel_recv_msg(td->sock_fd, recv_buf, MAX_PDU) == 0)//recv connection response
		error_td_msg("connection aborted\n");
	
	if(strncmp(recv_buf, "ERR_MSG", 7) == 0)
    {
        unsigned int err = err_connection_prop(&(td->client), &timeout_time,&p_loss, recv_buf);
    
		if (setsockopt(td->sock_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&conn_timeout, sizeof(conn_timeout)) < 0)
			error_td_msg("setsockopt failed\n");	
            
        if(connect(td->sock_fd,(struct sockaddr *) &(td->client), sizeof(td->client)) == -1)
			error_td_msg("error in connect");
            
		send_fin_ack(td->sock_fd, recv_buf, MAX_PDU, p_loss, &seed_buf);
        con_error_handler(err);
		pthread_exit(0);
    }
    
    if (strncmp(recv_buf,"CON_MSG",7)!=0)
		error_td_msg("error in connection\n");
	
	set_connection_prop(&(td->client), &window_size, &timeout_time, &timeout_adapt, &p_loss, recv_buf);  
    DEBUG_STAMP("con_prop setted\n");
    
	if (setsockopt (td->sock_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&conn_timeout, sizeof(conn_timeout)) < 0)
		error_td_msg("setsockopt failed\n");	
	
	if (connect(td->sock_fd, (struct sockaddr *)&(td->client), sizeof(td->client)) == -1)
		error_td_msg("error in connect");
	
	if (urel_send_msg("CON_ACK", 7, td->sock_fd, &seed_buf, p_loss)) //send con_ack	
		{ DEBUG_STAMP("CON_ACK SENDED\n"); }
	else
		{ DEBUG_STAMP("CON_ACK FAILED\n"); }
	
	//connection established windowed protocol now on	
	if(rel_recv(td->sock_fd, recv_buf,path, window_size, &seed_buf, p_loss,1) == -1)
		error_td_msg("error in rel_recv\n");
	printf("\032file downloaded: %s\nsending fin_ack\032\n",td->request+4);
	
	#ifdef PROFILE
	get_current_time(&end_time);
	timespec_diff(&start_time,&end_time, &end_time);
	double time_sec = end_time.tv_sec + end_time.tv_nsec / 1000000000.0;
	printf(/*"total time:\t"*/"%f s\t",time_sec);
	#endif
	send_fin_ack(td->sock_fd,recv_buf,MAX_PDU,p_loss,&seed_buf);	
	
	if (close(td->sock_fd) == -1)
		error_td_msg("error in close socket");
	free(td->request);
	pthread_exit(0);
}

void *put_file_procedure(void *arg)
{
	#ifdef PROFILE
	struct timespec start_time, end_time;
	get_current_time(&start_time);
	#endif
	struct td_data *td = arg;
	 
	if(!exist_file(td->request + 4, FILE_DIR))
	{
		fprintf(stderr,"\033File '%s' not found\033\n", td->request + 4);
		pthread_exit(0);
	}
	
	char  recv_buf[MAX_PDU];
	struct drand48_data seed_buf;
	srand48_r(time(NULL), &seed_buf);
	
	unsigned int window_size, timeout_time, timeout_adapt, p_loss;
	struct timeval conn_timeout = {.tv_sec = CONN_TIMEOUT, .tv_usec = 0 };		
	
	char *path = alloca(strlen(td->request) + strlen(FILE_DIR) -4);
	str_cat(path, FILE_DIR, td->request + 4);

	int fd = rd_open_file(path);
	int fsize = file_size(fd);
	char *send_buf = map_file(fd,fsize,0);
		
	urel_send_msg_to(td->request, strlen(td->request), td->sock_fd, &SERVER_ADDR, sizeof(SERVER_ADDR), &seed_buf,0);	//SEND REQUEST TO SERVER	
	DEBUG_STAMP("request %s sended\n", td->request);	
			
	if(urel_recv_msg(td->sock_fd,recv_buf,MAX_PDU) == 0)//RECV CON_MSG
		error_td_msg("connection aborted\n");
	
	if(strncmp(recv_buf,"ERR_MSG", 7) == 0) //error procedure
    {
        unsigned int err = err_connection_prop(&(td->client), &timeout_time, &p_loss, recv_buf);    
		if (setsockopt(td->sock_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&conn_timeout, sizeof(conn_timeout)) < 0)
			error_td_msg("setsockopt failed\n");	           
        if (connect(td->sock_fd,(struct sockaddr *) &(td->client), sizeof(td->client)) == -1)
			error_td_msg("error in connect");
            
		send_fin_ack(td->sock_fd, recv_buf, MAX_PDU, p_loss, &seed_buf);
        con_error_handler(err);
		pthread_exit(0);
    }
	set_connection_prop(&(td->client), &window_size, &timeout_time, &timeout_adapt, &p_loss, recv_buf);
	
	if (connect(td->sock_fd, (struct sockaddr *) &(td->client),sizeof(td->client)) == -1)
		error_td_msg("error in connect\n");
	
	//connection established windowed protocol now on  SEND CON_ACK as first message
	if (rel_send(td->sock_fd, send_buf, fsize, recv_buf, timeout_time, timeout_adapt, window_size, &seed_buf, p_loss, 1) == -1)
		error_td_msg("error in rel_send\n");
			
	send_control_msg(td->sock_fd, NULL, "CLOSE_CONN", recv_buf, "FIN_ACK", timeout_time, p_loss, &seed_buf);
	printf("\032file uploaded :%s\033\n", td->request+4);
	
	#ifdef PROFILE
	get_current_time(&end_time);
	timespec_diff(&start_time,&end_time,&end_time);
	printf("total time: %lu sec %lu microsec\n", end_time.tv_sec, end_time.tv_nsec / 1000 );
	#endif
	
	sleep(FIN_TIMEOUT);
	
	free(td->request);
	if(close(td->sock_fd) == -1)
		error_td_msg("error in close socket");
	pthread_exit(0);
}

void *list_procedure(void *arg)
{
	struct td_data *td = arg; 
	char  recv_buf[MAX_PDU];
	struct drand48_data seed_buf;
	srand48_r(time(NULL), &seed_buf);
	
	unsigned int window_size, timeout_time, timeout_adapt, p_loss;
	struct timeval conn_timeout = { .tv_sec = CONN_TIMEOUT, .tv_usec = 0 };
	
	urel_send_msg_to(td->request, strlen(td->request), td->sock_fd, &SERVER_ADDR, sizeof(SERVER_ADDR), &seed_buf,0);	//SEND REQUEST TO SERVER	
	DEBUG_STAMP("request %s sended\n", td->request);	
			
	if(urel_recv_msg(td->sock_fd, recv_buf, MAX_PDU) == 0)//recv connection response
		error_td_msg("connection aborted\n");

	set_connection_prop( &(td->client), &window_size, &timeout_time, &timeout_adapt, &p_loss, recv_buf);//set connection parameter
	
	if (setsockopt (td->sock_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&conn_timeout, sizeof(conn_timeout)) < 0)
		error_td_msg("setsockopt failed\n");
	
	if(connect(td->sock_fd,(struct sockaddr *) &(td->client), sizeof(td->client)) == -1)
		error_td_msg("error in connect");
		
	urel_send_msg("CON_ACK", 7, td->sock_fd, &seed_buf, p_loss);
	
	//connection established windowed protocol now on
	char *file_list = list_recv(td->sock_fd, recv_buf, window_size, &seed_buf, p_loss);
	if(file_list == NULL)
		error_td_msg("error in list_recv\n");
	
	printf("Server Files: \n%s", file_list);
	send_fin_ack(td->sock_fd, recv_buf, MAX_PDU, p_loss, &seed_buf);
	printf("\n");
	
	if (close(td->sock_fd) == -1)
		error_td_msg("error in close socket");
	free(file_list);
	free(td->request);
	pthread_exit(0);
}

static void start_daemon()
{
	struct node_t *threads = NULL;
	char *request = calloc(50, 1);
	
	while (fgets(request,50, stdin) != NULL)
	{	
		request[strlen(request)-1] = '\0';
		
		if (strncasecmp(request,"get", 3) == 0 || strncasecmp(request, "put", 3) == 0 || strncasecmp(request, "list", 4) == 0)
		{
			struct node_t* node = create_node();
			if ((node->thread_data.sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) // create  socket 
				error_msg("error in socket");
		
			if ((node->thread_data.request = malloc(strlen(request) + 1)) == NULL)// 1 = end of line char
				error_msg("error in malloc");	
			strncpy(node->thread_data.request, request, strlen(request) + 1);
			
			lock(&list_mtx);
			add_next(&threads, node); //insert thread in working thread list
			unlock(&list_mtx);
			
			if (strncasecmp(request, "get", 3) == 0)
			{	
				if (pthread_create(&(threads->thread_data.tid), NULL, get_file_procedure, (void *)&(threads->thread_data))) //create thread
					error_msg("error in thread_create");			
			}
			else if (strncasecmp(request, "put", 3) == 0)
			{
				if (pthread_create(&(threads->thread_data.tid), NULL, put_file_procedure, (void *)&(threads->thread_data))) //create thread
					error_msg("error in thread_create");			
			}
			else
			{	
				if(pthread_create(&(threads->thread_data.tid), NULL, list_procedure, (void *)&(threads->thread_data))) //create thread
					error_msg("error in thread_create");				
			}
		}
		else
		{
			DEBUG_STAMP("request : %s not valid\n", request);
			usage("", 1);
		}	
	}		
	
	while(threads != NULL)
	{
		pthread_join(threads->thread_data.tid, NULL);
		lock(&list_mtx);
		remove_next(&threads);
		unlock(&list_mtx);
	}	
}

int main(int argc, char * argv[])
{ 
	argc = argc;
	argv = argv;
	init();
	DEBUG_STAMP("client eseguito\n");
	start_daemon();
	exit(EXIT_SUCCESS);	
}
