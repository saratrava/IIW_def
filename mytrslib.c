#include "mytrslib.h"

struct node_t * create_node()
{
	struct node_t *n=malloc(sizeof(struct node_t));
	if(n==NULL)
		error_msg("error in malloc");
	n->next=NULL;
	return n;
}

void add_next(struct node_t **pnext,struct node_t *new)
{
	new->next= *pnext;
	*pnext=new;
}

void remove_next(struct node_t **pnext)
{
	 struct node_t *p=*pnext;
	 p=p->next;
	 free(*pnext);
	 *pnext=p;
}

/*struct node_t * get_by_addr(struct node_t *list,struct sockaddr_in addr )
{
	while(list!=NULL &&  !(list->thread_data.client.sin_addr.s_addr == addr.sin_addr.s_addr 
	&& list->thread_data.client.sin_port == addr.sin_port))
		list=list->next;
	return list;	
}*/

void remove_thread(struct node_t **list, pthread_t tid )
{
	while (*list != NULL && ( (*list)->thread_data.tid != tid) )
		list = &((*list)->next);
	if (*list != NULL)
		remove_next(list);		
}

int exist_request(struct node_t *list, char *request)
{
	while(list != NULL && (strcmp(list->thread_data.request,request) != 0))
	list = list->next;
	return (list!=NULL);	
}

static int msg_fail(struct drand48_data *seed_buf, unsigned int p_loss)
{
	double r;
	drand48_r(seed_buf, &r);
	unsigned int r_n = (unsigned int) (r * 100);
	
	return !(p_loss > r_n);
}

int urel_send_msg_to(const char * msg, const int msg_size, int sockfd, struct sockaddr_in *client, unsigned int addr_size,
						struct drand48_data *seed_buf, unsigned int p_loss)
{
	if (msg_fail(seed_buf, p_loss) == 0)
		return 0;
	
	if (sendto(sockfd,msg, msg_size, 0, (struct sockaddr *)client, addr_size) < 0)
	{
		fprintf(stderr,"error in sendto buf:\n%.10s\n",msg);
		exit(EXIT_FAILURE);
	}			
	return 1;	
}

int urel_send_msg(const char * msg, const int msg_size, int sockfd, struct drand48_data *seed_buf, unsigned int p_loss)
{
	if (msg_fail(seed_buf, p_loss) == 0)
		return 0;
	
	if (send(sockfd, msg, msg_size, 0) < 0)
	{
		fprintf(stderr,"error in sendto buf:\n%.10s\n",msg);
		exit(EXIT_FAILURE);
	}			
	return 1;	
}

int urel_recv_msg(const int sock_fd, char *recbuf, const int max_size)
{
	errno = 0;
	int msg_size = recv(sock_fd, recbuf, max_size, 0);
	if (msg_size  < 0)
	{
		if(errno == EWOULDBLOCK)
		{
			DEBUG_STAMP("timeout expired\n");
			return 0;
		}
		else
		return -1;
	}
	return msg_size;
}

int urel_recv_msg_from(const int sock_fd,char *recbuf,const int max_size, struct sockaddr_in *sender,unsigned int *addr_len)
{
	errno = 0;
	int msg_size = recvfrom(sock_fd, recbuf, max_size, 0, (struct sockaddr *)sender, addr_len);
	if (msg_size  < 0)
	{
		if(errno == EWOULDBLOCK)
		{
			DEBUG_STAMP("timeout expired\n");
			return 0;
		}
		else
		return -1;
	}
	return msg_size;	
}

void set_addr(struct sockaddr_in * addr,char * ip, unsigned int port)
{
	memset((void *)addr, 0, sizeof(*addr));
	addr->sin_family = AF_INET;       /* assegna il tipo di indirizzo */
	addr->sin_port = htons(port);  /* assegna la porta del server */
	
	if (inet_pton(AF_INET,ip,&(addr->sin_addr)) <= 0) //assegna l'indirizzo del server che è una stringa da convertire in intero secondo network byte order
	error_msg("error in inet_pton");
}

int send_control_msg(int sock_fd, struct sockaddr_in *client, char *msg, char *recv_buf, char *exp_ack, unsigned int timeout, 
						unsigned int p_loss, struct drand48_data *seed_buf)//may use timeval
{	
	int n_fail = 0;//number of failed read
	int EOT = 0;
	unsigned long int timeout_ns = timeout;
	timeout_ns = timeout_ns * 1000;
	int max_tent = GET_MAX_TENTATIVE((timeout_ns));
	struct timeval pack_timeout; 
	pack_timeout = GET_TIMEOUT_US(timeout);
	
	if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&pack_timeout, sizeof(pack_timeout)) < 0)
		error_msg("setsockopt failed\n");
	
	while (n_fail < max_tent && (!EOT))
	{									
		if (urel_send_msg_to(msg, strlen(msg) + 1, sock_fd, client, sizeof(*client), seed_buf, p_loss))	
			{DEBUG_STAMP("C_MSG: %.10s SENDED\n",msg);}
		else
			{DEBUG_STAMP("C_MSG: %.10s FAILED\n",msg);}
		
		fflush(stdout);	
		
		if(urel_recv_msg(sock_fd,recv_buf,MAX_PDU))
		{	
			if(strncmp(recv_buf,exp_ack,7) == 0)
			{
				DEBUG_STAMP("%s RECEVED\n",exp_ack);
				break;
			}
			else
			{
				DEBUG_STAMP("ACK NOT RECOGNISED recv_buf : %.10s\n", recv_buf);
			}
		}
		else
		{
			DEBUG_STAMP("%s NOT RECEIVED\n",exp_ack);
		}
		n_fail++;
	}
	
	if(n_fail != max_tent)
		return 0;
	else
	{
		perror("Connection aborted in handshake: too many packets lost");
		return -1;
	}	
}	

void send_fin_ack(int sock_fd,char *recvline, int max_size,unsigned int p_loss, struct drand48_data *seed_buf)
{
	struct timeval to = {.tv_sec = FIN_TIMEOUT, .tv_usec = 0};
	
	if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&to,sizeof(to)) < 0)
		error_msg("setsockopt failed\n");
	
	const char *ack = "FIN_ACK";
	while (1)
	{	
		if (urel_send_msg(ack,strlen(ack),sock_fd, seed_buf, p_loss))
		{ DEBUG_STAMP("%s SENDED\n",ack); }
		else
		{ DEBUG_STAMP("%s FAILED\n",ack); }
	
		if (urel_recv_msg(sock_fd,recvline,max_size) == 0)
			break;
	}
}
