#include "recv_window_lib.h"

static void init_rec_window(recv_window *wind, unsigned int window_size);
static void free_rec_window(recv_window *wind, unsigned int window_size);
static void slide_window_and_save(recv_window *wind,int fd,unsigned int *offset,unsigned int window_size);
static void save_msg_in_window(recv_window *wind,char *msg, int  msg_size,int seq_num);
static void send_ack(int sock_fd, char *send_buf, int seq_num, struct drand48_data *seed_buf, unsigned int p_loss);

static void init_rec_window(recv_window *wind, unsigned int window_size)
{
	unsigned int i;
	for(i=0; i<window_size; i++)
	{
		(wind+i)->seq_num = i;
		(wind+i)->msg_receved = 0;
		if(((wind+i) -> msg = malloc(MAX_PDU)) == NULL )
			error_msg("error in realloc");	
	}
	//DEBUG_STAMP("window inizializated\n");	
}

static void free_rec_window(recv_window *wind, unsigned int window_size)
{
	unsigned int i;
	for(i=0; i<window_size; i++)
	{
		free((wind+i)->msg);
	}
}

static void slide_window_and_save(recv_window *wind,int fd,unsigned int *offset, unsigned int window_size)
{
	unsigned int i;
	for (i = *offset; i < (*offset) + window_size; i++)
	{
		recv_window * packet = wind + (i % window_size);
		if (packet->msg_receved)
		{
			write_file(fd, packet->msg, packet->msg_size, (*offset) * (MAX_PDU - HEADER_MAX_SIZE)); 
			packet->msg_receved = 0;
			packet->seq_num = (packet->seq_num + window_size) % (window_size * 2);
			*offset = *offset + 1;
		}
		else
			break;
	}		
}

static char *slide_window_and_save_to_buffer(recv_window *wind, char *buffer, unsigned int * buf_size,unsigned int *offset, 
												unsigned int window_size)
{
	unsigned int i;
	
	for (i = *offset; i < *offset + window_size; i++)
	{
		recv_window * packet = wind + (i % window_size);
		if (packet->msg_receved)
		{
			buffer = realloc(buffer, *buf_size + packet->msg_size + 1);
			if (buffer == NULL)
				error_td_msg("error in realloc\n");
			strncpy(buffer + *buf_size, packet->msg, *buf_size + packet->msg_size + 1);
			*buf_size += (packet->msg_size + 1);
			
			packet->msg_receved = 0;
			packet->seq_num = (packet->seq_num + window_size) % (window_size * 2);
			offset++;
		}
		else
			break;
	}
	return buffer;		
}

static void save_msg_in_window(recv_window *wind, char *msg, int  msg_size, int seq_num)
{
	if( (!(wind->msg_receved)) && ( seq_num == wind->seq_num )  )
	{
		memcpy(wind->msg, msg, msg_size);	
		wind->msg_size = msg_size;
		wind->msg_receved = 1;
		
		DEBUG_STAMP("messaggio indice %d accettato\n",seq_num);
	}
	else
	{
		DEBUG_STAMP("messaggio indice %d scartato  msg_id:%d  msg_is_recv: %d\n",seq_num ,wind->seq_num,wind->msg_receved);
	}
}

static void send_ack(int sock_fd,char *send_buf, int seq_num, struct drand48_data *seed_buf, unsigned int p_loss)
{
	sprintf(send_buf, "ACK %d", seq_num);
	//memset(send_buf + 4, 0, HEADER_MAX_SIZE - 4); //4 = "ACK " char
	//strncpy(send_buf + 4, msg_id,HEADER_MAX_SIZE);
	DEBUG_STAMP("ACK_N^ %s\n",msg_id);
	//printf("%s\n", send_buf);
		
	if (urel_send_msg(send_buf,strlen(send_buf), sock_fd, seed_buf, p_loss))
	{
		DEBUG_STAMP(" SENDED\n");
	}
	else
	{
		DEBUG_STAMP(" FAILED\n");
	}		
}

int rel_recv(int sock_fd, char *recv_buf, char *path,unsigned int window_size, struct drand48_data *seed_buf,
				unsigned int p_loss, int client)
{	
	recv_window *wind;
	unsigned int offset = 0;
	char *msg_id = alloca(HEADER_MAX_SIZE);
	char *send_buf = alloca(HEADER_MAX_SIZE);
	int receved_msg = 0;
	
	if((wind = malloc(window_size * sizeof(recv_window))) ==NULL)
	{
		perror("error in malloc\n");
		return -1;
	}				
	init_rec_window(wind,window_size);
	
	int fd = wr_open_file(path);
	while (1)
	{	
		memset((void *)recv_buf, 0, MAX_PDU);
		int msg_size = urel_recv_msg(sock_fd, recv_buf, MAX_PDU);  
		if (msg_size == 0)
		{
			perror("Connection aborted in file reception,timeout expired\n");
			close(fd);
			return -1;
		}
		
		if ((!receved_msg) && client && strncmp(recv_buf, "CON_MSG", 7) == 0)
		{
			if(urel_send_msg("CON_ACK",7,sock_fd,seed_buf,p_loss))
				{ DEBUG_STAMP("CON_ACK SENDED in recv\n"); }
			else
				{ DEBUG_STAMP("CON_ACK FAILED in recv\n"); }
		}
		else
		{
			if (strncmp(recv_buf,"MSG_N",4) == 0)
			{
				receved_msg=1;	
				sscanf(recv_buf,"MSG_N:%[^\n]",msg_id);
			
				int seq_number=strtol(msg_id, NULL, 0);
				unsigned int header_size = strlen(msg_id) + 6 + 1;
				send_ack(sock_fd, send_buf, seq_number, seed_buf, p_loss);
				
				DEBUG_STAMP("ricevuto: MSG_N:%s datasize: %u  tot_size %d\n",msg_id , msg_size - header_size , msg_size);			
				save_msg_in_window(wind + (seq_number % window_size), recv_buf + header_size, msg_size - header_size, seq_number);
				slide_window_and_save(wind, fd, &offset, window_size);
			}
			else if (strncmp(recv_buf,"CLOSE_CONN",10) == 0)
			{
				slide_window_and_save(wind,fd,&offset,window_size);
				break;
			}
			else if (strncmp(recv_buf, "CON_ACK", 7) != 0)
			{
					fprintf(stderr, "connection aborted MSG NOT VALID message:\n%.10s\n", recv_buf);
					return -1;
			}	
		}	
	}
	close(fd);
	
	free_rec_window(wind, window_size);
	free(wind);
	return 0;
}

char *list_recv(int sock_fd, char *recv_buf, unsigned int window_size, struct drand48_data *seed_buf, unsigned int p_loss)
{
	recv_window *wind=NULL;
	char *file_list = NULL;
	unsigned int offset = 0;
	unsigned int file_list_size = 0;
	int receved_msg = 0;
	char *msg_id = alloca(HEADER_MAX_SIZE);
	char *send_buf = alloca(HEADER_MAX_SIZE);
	
	if((wind = malloc(window_size * sizeof(recv_window))) ==NULL)
		error_td_msg("error in malloc\n");
						
	init_rec_window(wind,window_size);
	
	while(1)
	{
		memset((void *) recv_buf, 0, MAX_PDU);
		int msg_size = urel_recv_msg(sock_fd,recv_buf,MAX_PDU);
		
		if (msg_size == 0)
		{
			perror("connection aborted in file reception,timeout expired\n");
			return NULL;
		}

		if ((!receved_msg) && strncmp(recv_buf,"CON_MSG",7)==0)
		{
			if(urel_send_msg("CON_ACK",7,sock_fd,seed_buf,p_loss))
				{ DEBUG_STAMP("CON_ACK SENDED\n"); }
			else
				{ DEBUG_STAMP("CON_ACK FAILED\n"); }
		}
		else
		{
			if (strncmp(recv_buf,"MSG_N",5)==0)
			{
				receved_msg=1;
				sscanf(recv_buf,"MSG_N:%[^\n]",msg_id);
				
				int seq_number = strtol(msg_id, NULL, 0);
				unsigned int header_size = strlen(msg_id) + 6 + 1;
				send_ack(sock_fd, send_buf, seq_number, seed_buf, p_loss);
				
				DEBUG_STAMP("ricevuto: MSG_N:%s datasize: %lu  tot_size %d\n",msg_id , msg_size - (6+strlen(msg_id)+1) ,msg_size);
				
				save_msg_in_window(wind + (seq_number % window_size), recv_buf + header_size, msg_size - header_size, seq_number);
				file_list = slide_window_and_save_to_buffer(wind, file_list, &file_list_size, &offset, window_size);
			}
			else if (strncmp(recv_buf,"CLOSE_CONN",10)==0)
			{
				file_list = slide_window_and_save_to_buffer(wind, file_list, &file_list_size, &offset, window_size);
				break;
			}	
			else if (strncmp(recv_buf, "CON_ACK", 7) != 0)
			{
					fprintf(stderr, "connection aborted MSG NOT VALID message:\n%.10s\n", recv_buf);
					return NULL;
			}	
		}
	}
	free_rec_window(wind, window_size);
	free(wind);
	return file_list;	
}				
