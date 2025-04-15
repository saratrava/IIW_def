#ifndef SEND_WINDOW_LIB_H_   /* Include guard */
#define SEND_WINDOW_LIB_H_

#include <signal.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "mylib.h"
#include "mytrslib.h"
#include "constant_lib.h"
#include "timeout_lib.h"
#include "math.h"


#define SEND_CLIENT 1
#define MAXDATA() (MAX_PDU - HEADER_MAX_SIZE)

#define CALCULATE_DATA_SIZE(MAXDATA,buf_size,msg_id) (MAXDATA < (buf_size - (MAXDATA * msg_id) ) ) ? MAXDATA : (buf_size-(MAXDATA * msg_id))	
#define ACK_RECEVED 1
#define TO_SEND 2
#define MSG_FINISHED 3
#define WAITING_ACK 4
#define TIMEOUT_EXPIRED 5

#define SIG SIGRTMIN


typedef struct send_window
{
	char *msg;
	int msg_size;
	unsigned int msg_id;
	timer_t timer_id;
	int ack_rec;
	int msg_restrasmitted;
}send_window;

struct send_list
{
	send_window *pack;
	struct send_list *next;
};

int rel_send(int sock_fd,char *send_buf,int size,char *recv_buf,unsigned long int timeout_us,int timeout_adapt,
				unsigned int window_size, struct drand48_data *seed_buf,unsigned int p_loss,int client);

#endif // SEND_WINDOW_LIB_H_
