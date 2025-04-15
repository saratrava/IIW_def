#ifndef RECV_WINDOW_LIB_H_   /* Include guard */
#define RECV_WINDOW_LIB_H_

#include "mylib.h"
#include "mytrslib.h"
#include "constant_lib.h"

typedef struct recv_window
{
	char * msg;
	int seq_num;
	int msg_size;
	int msg_receved;
}recv_window;

int rel_recv(int sock_fd, char *recv_buf, char *path, unsigned int window_size, struct drand48_data *seed_buf,
				unsigned int p_loss,int client);
char *list_recv(int sock_fd, char *recv_buf, unsigned int window_size, struct drand48_data *seed_buf, unsigned int p_loss);

#endif // RECV_WINDOW_LIB_H_
