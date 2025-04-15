#include "send_window_lib.h"

static send_window *init_window(char *data_buf, unsigned int buf_size, unsigned int packet_num, unsigned int window_size);
static void free_window(send_window *wind, unsigned int window_size);
static int slide_window(send_window *wind, char *data_buf, off_t buf_size, unsigned int packet_num, unsigned int *next_ack, 
						unsigned int window_size);
static int read_ack(int sock_fd, char *buf, send_window *wind, unsigned int window_size);					
static void load_packet_in_window(send_window *wind,char *data_buf,off_t buf_size,unsigned int window_size);	

static unsigned long int get_adapt_timeout(unsigned long int timeout_ns, int exp_n, int msg_restrasmitted);		

void handler(int sig, siginfo_t *si, void *uc)
{
	send_window *packet = si->si_value.sival_ptr;
	packet->ack_rec = TIMEOUT_EXPIRED;
	sig = sig;
	uc = uc; //signal context
}

static void create_timer(timer_t *timerid, send_window *packet)
{
	struct sigevent sev;
	sev.sigev_notify =  SIGEV_THREAD_ID;
	sev.sigev_signo = SIG;
	sev._sigev_un._tid = syscall (SYS_gettid);
	sev.sigev_value.sival_ptr = packet;
	
	if (timer_create(CLOCK_REALTIME, &sev, timerid) == -1)
		error_td_msg("error in create_timer\n");
}

static void start_timer(timer_t timer_id, struct timespec timeout)
{
	struct itimerspec time;
	time.it_value.tv_sec = timeout.tv_sec;
	time.it_value.tv_nsec = timeout.tv_nsec;
	time.it_interval.tv_sec = timeout.tv_sec;
	time.it_interval.tv_nsec = timeout.tv_nsec;
	
	if (timer_settime(timer_id, 0, &time, NULL) == -1)
		error_td_msg("error in start_timer\n");
}

static void stop_timer(timer_t timer_id)
{
	struct itimerspec time;
	time.it_value.tv_sec = 0;
	time.it_value.tv_nsec = 0;
	time.it_interval.tv_sec = 0;
	time.it_interval.tv_nsec = 0;
	
	if (timer_settime(timer_id, 0, &time, NULL) == -1)
		error_td_msg("error in stop_timer\n");
}

static unsigned long int stop_timer_and_get_rtt(timer_t timer_id)
{
	struct timespec diff;
	struct itimerspec time;
	struct itimerspec old_time;
	time.it_value.tv_sec = 0;
	time.it_value.tv_nsec = 0;
	time.it_interval.tv_sec = 0;
	time.it_interval.tv_nsec = 0;
	
	if (timer_settime(timer_id, 0, &time, &old_time) == -1)
		error_td_msg("error in stop_timer\n");		
	timespec_diff(&(old_time.it_value), &(old_time.it_interval), &diff);
	return (diff.tv_sec * TIME_PRECISION) + diff.tv_nsec;
}

static send_window *init_window(char *data_buf, unsigned int buf_size, unsigned int packet_num, unsigned int window_size)
{
	send_window *wind;
	if((wind = malloc(window_size * sizeof(send_window))) == NULL)
	{
		perror("error in malloc at init_window\n");
		return NULL;
	}
	unsigned int i;
	for (i = 0; i < window_size; i++)
	{
		wind[i].msg_id = i;
		wind[i].msg = malloc(MAX_PDU);
		create_timer(&(wind[i].timer_id), wind + i);
		
		if (wind[i].msg == NULL)
		{
			perror("error in malloc at init_window\n");
			return NULL;
		}	
		
		if (packet_num >= (wind[i].msg_id + 1))
		{
			load_packet_in_window((wind+i), data_buf, buf_size, window_size);
			wind[i].ack_rec = TO_SEND;
			wind[i].msg_restrasmitted = 0;
		}
		else
			wind[i].ack_rec = MSG_FINISHED;	
	}
	return wind;	
}

static void free_window(send_window *wind, unsigned int window_size)
{
	unsigned int i;
	for (i = 0; i < window_size; i++)
	{
		free((wind+i)->msg);
	}
	free(wind);
}

static int slide_window(send_window *wind, char *data_buf, off_t buf_size, unsigned int packet_num, unsigned int *next_ack, 
					unsigned int window_size)
{
	unsigned int i, count = 0;
	for (i = (*next_ack % window_size); i < window_size + (*next_ack % window_size); i++)
	{
		unsigned int j = i % window_size;
		if (wind[j].ack_rec  != ACK_RECEVED)
			break;
				
		wind[j].msg_id = wind[j].msg_id + window_size; //aggiorna id con l' id del nuovo messaggio
		
		if (packet_num >= (wind[j].msg_id + 1)) //msg not finished
		{
			load_packet_in_window(wind + j, data_buf, buf_size, window_size); // load the next packet to send in the window
			wind[j].ack_rec = TO_SEND;
			wind[j].msg_restrasmitted = 0;
		}
		else
			wind[j].ack_rec = MSG_FINISHED; //flag that means that the packet dosen't contain any packet
		count++;
	}
	*next_ack = *next_ack + count;
	DEBUG_STAMP("NEXT_ACK : %u  PACK_NUM : %u\n", *next_ack, packet_num);
	return ((*next_ack) > (packet_num - 1)); //ack start from 0 so last ack is pack_num -1
}

static void load_packet_in_window(send_window *wind,char *data_buf,off_t buf_size,unsigned int window_size)//can be optimized
{
	int data_size,header_size;
	
	data_size = CALCULATE_DATA_SIZE(MAXDATA(), buf_size, (wind->msg_id));
	memset(wind->msg, 0, MAX_PDU);
	snprintf(wind->msg, HEADER_MAX_SIZE, "MSG_N:%u\n", (wind->msg_id % (2 * window_size)));
	header_size = strlen(wind->msg);
	memcpy(wind->msg + header_size, data_buf + MAXDATA() * wind->msg_id, data_size);
	wind->msg_size = header_size + data_size;
}

static int read_ack(int sock_fd,char *buf,send_window *wind,unsigned int window_size)
{
	unsigned int num_ack;
	memset(buf, 0, MAX_PDU);
	
	if (urel_recv_msg(sock_fd, buf, MAX_PDU) > 0)
	{
		if (sscanf(buf, "ACK %u", &num_ack) == 1)
		{
			unsigned int i = num_ack % window_size;
			if ((wind[i].msg_id % (window_size * 2)) == (num_ack))
			{
				DEBUG_STAMP("RECEIVED ACK N:%u \n", num_ack);
				wind[i].ack_rec = ACK_RECEVED;
				return i;
			}
			else
			{
				DEBUG_STAMP("ACK DUPLICATE N:%u \n", num_ack);
			}
		}		
		else
		{
			 DEBUG_STAMP("ack non riconosciuto %s \n", buf);
		}
	}
	return -1;		
}

static void init_sigaction(struct sigaction *sa)
{
	sa->sa_flags = SA_SIGINFO;
	sa->sa_sigaction = handler;
	sigemptyset(&(sa->sa_mask));
	sigaddset(&(sa->sa_mask), SIG);
	if (sigaction(SIG, sa, NULL) == -1)
		error_td_msg("error in sigaction\n");
}

static void send_msg_from_window(send_window *packet, int sock_fd, struct drand48_data *seed_buf, unsigned int p_loss,
									struct timespec timeout, unsigned int window_size)
{
	if (urel_send_msg(packet->msg , packet->msg_size, sock_fd, seed_buf , p_loss)) //send msg
	{
		DEBUG_STAMP("MSG_N^ %d seq_n %d MSG_SIZE :%u SENDED\n", packet->msg_id, packet->msg_id % (2 * window_size), packet->msg_size);
	}
	else
	{ 
		DEBUG_STAMP("MSG_N^ %d seq_n %d MSG_SIZE :%u FAILED\n", packet->msg_id, packet->msg_id % (2 * window_size), packet->msg_size);
	}
	packet->ack_rec = WAITING_ACK;
	start_timer(packet->timer_id, timeout);
	window_size = window_size;
}

int rel_send(int sock_fd,char *send_buf,int size,char *recv_buf, unsigned long int timeout_ns, int timeout_adapt
,unsigned int window_size, struct drand48_data *seed_buf, unsigned int p_loss, int client)
{
	struct sigaction sa;	
	init_sigaction(&sa);
	
	timeout_ns = timeout_ns * 1000;
	struct timespec timeout;
	timeout = GET_TIMEOUT(timeout_ns);
	
	unsigned int packet_num = ceil(((double)size) / ((double)(MAX_PDU - HEADER_MAX_SIZE)));
	unsigned int next_ack = 0;
	int receved_ack = 0;
 	send_window *wind = init_window(send_buf, size, packet_num, window_size);;
	
	unsigned int n_fail = 0, max_tent;
	unsigned int exp_n = 0;
	int EOT = 0; //end of transmission
	if(!client)
		receved_ack = 1;
	
	if(timeout_adapt)
		max_tent = 1024;
	else	
		max_tent = GET_MAX_TENTATIVE(timeout_ns);
	
	while(!EOT)
	{
		unsigned int i;
		
		for (i = (next_ack % window_size); i < window_size + (next_ack % window_size); i++)
		{	
			unsigned int j = i % window_size;
			if (wind[j].ack_rec == TO_SEND || wind[j].ack_rec == TIMEOUT_EXPIRED) //packet must be sent
			{	
				if ((!receved_ack) && (i == 0))  //only if the sender is client
				{
					if (urel_send_msg("CON_ACK", 7, sock_fd, seed_buf, p_loss))
						{DEBUG_STAMP("CON_ACK SENDED\n");}
					else
						{DEBUG_STAMP("CON_ACK FAILED\n");}
				}
				if (wind[j].ack_rec == TIMEOUT_EXPIRED)
				{
					if (exp_n >= max_tent) //abort trasmission
					{
						perror("Connection aborted in file transmission too many packets lost\n");
						free_window(wind,window_size);
						return -1;
					}
					exp_n++;
					wind[j].msg_restrasmitted = 1;
					DEBUG_STAMP("PACCHETTO RITRASMESSO %d  timeout %lu  exp_n %d\n", wind[j].msg_id, timeout_ns, exp_n);			
				}
				send_msg_from_window(wind + j, sock_fd, seed_buf, p_loss, timeout, window_size);											
			}
		}
		int recv_pos = read_ack(sock_fd, recv_buf, wind, window_size); // window position of receved ack
		if (recv_pos != -1) //read_ack
		{
			if (timeout_adapt)
			{
				unsigned long int rtt =  stop_timer_and_get_rtt(wind[recv_pos].timer_id) / 1000;
				timeout_ns = get_adapt_timeout(rtt, exp_n, wind[recv_pos].msg_restrasmitted) * 1000; //estimate new timeout
				timeout = GET_TIMEOUT(timeout_ns);
			}	
			else
				stop_timer(wind[recv_pos].timer_id);
			n_fail += exp_n;
			receved_ack = 1;
			exp_n = 0;
			if (slide_window(wind, send_buf, size, packet_num, &next_ack, window_size)) //return 1 if the sender buffer is finished
				EOT = 1;			
		}
	}	
	free_window(wind,window_size);
	printf("timeout scaduti: %u / %u percentuale %u attesi %u\n", n_fail, n_fail + packet_num, (n_fail * 100) / (n_fail + packet_num),
			(10000 - (100 - p_loss) * (100 - p_loss)) / 100);
	return 0;
}

static unsigned long int get_adapt_timeout(unsigned long int rtt, int exp_n, int msg_restrasmitted)
{
	static unsigned long int srtt = 5000;
	static long int dev = 500;
	if (!msg_restrasmitted)
	{
		long int err = rtt - srtt;
		srtt = (unsigned long int) srtt + err / 8;			
		dev = dev + (abs(err) - dev) / 4;
		//printf("timeout : %lu rtt %lu srtt %lu dev %ld  err %ld\n",srtt + 4 * dev, rtt, srtt, dev, err);
	}	
	if ((exp_n > 0) && (srtt < 10000))
	{
		long int err = srtt / 2;
		srtt = srtt + srtt / 8;
		dev = dev + (abs(err) - dev) / 4;
		//printf("timeout : %lu rtt %lu srtt %lu\n",srtt + 4 * dev, rtt, srtt);
	}
	
	return (srtt + 4 * dev);
}
