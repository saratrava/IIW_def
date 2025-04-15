#ifndef MYTRS_LIB_H_   /* Include guard */
#define MYTRS_LIB_H_

#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h>

#include "constant_lib.h"
#include "mylib.h"
#include "timeout_lib.h"

struct td_data
{
	pthread_t  tid;
	int sock_fd;
	struct sockaddr_in client;
	char *request;
};

struct node_t
{
	struct td_data thread_data;
	struct node_t *next;
};

struct node_t * create_node();
void add_next(struct node_t **pnext,struct node_t *new);
void remove_next(struct node_t **pnext);
void remove_thread(struct node_t **list, pthread_t tid );
int exist_request(struct node_t *list,char *request);

void set_addr(struct sockaddr_in * addr,char * ip, unsigned int port);

int urel_send_msg(const char * msg, const int msg_size, int sockfd, struct drand48_data *seed_buf, unsigned int p_loss);
int urel_send_msg_to(const char * msg, const int msg_size, const int sockfd, struct sockaddr_in *client, unsigned int addr_size,
						struct drand48_data *seed_buf, unsigned int p_loss);
int urel_recv_msg(const int sock_fd, char *recbuf, const int max_size);						
int urel_recv_msg_from(const int sock_fd, char *recbuf, const int max_size, struct sockaddr_in *sender,unsigned int *addr_len);

int send_control_msg(int sock_fd,struct sockaddr_in *client,char *msg,char *recv_buf,char *exp_ack,
						unsigned int timeout,unsigned int p_loss, struct drand48_data *seed_buf);

void send_fin_ack(int sock_fd,char *recvline, int max_size,unsigned int p_loss, struct drand48_data *seed_buf);

#endif // MYTRS_LIB_H_
