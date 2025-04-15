#include "mylib.h"

void usage(char *path, int client)
{
	if (client)
	{
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "%s list (get the list of files in server)\n", path);
		fprintf(stderr, "%s get 'file_name' (download the file named 'file_name' from the server)\n", path);
		fprintf(stderr, "%s put 'file_name' (upload the file named 'file_name' to the server)\n", path);
	}
	else
	{
		fprintf(stderr, "Usage: %s\n", path);
	}

	exit(EXIT_FAILURE);
}

void error_msg(char * msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

void error_td_msg(char *msg)
{
	perror(msg);
	pthread_exit(0);
}

void lock(pthread_mutex_t *mtx)
{
	if (pthread_mutex_lock(mtx))
		error_msg("errore nella lock");
}

void unlock(pthread_mutex_t *mtx)
{
	if (pthread_mutex_unlock(mtx))
		error_msg("errore nella unlock");
}

void awake(pthread_cond_t *cond)
{
	if (pthread_cond_signal(cond))
		error_msg("errore nell' awake");
}

void wait(pthread_cond_t *cond, pthread_mutex_t *mtx)
{
	if (pthread_cond_wait(cond,mtx))
		error_msg("errore in wait");
}

/*int timed_wait(pthread_cond_t *cond, pthread_mutex_t *mtx,struct timespec *time)
{
	int i = pthread_cond_timedwait(cond,mtx,time);
	if (i == 0)
	{	
		DEBUG_STAMP("timer interrotto\n");
		return 1;
	}	
	
	if (i == ETIMEDOUT)
	{
		DEBUG_STAMP("timer scaduto\n");
		return 0;	
	}		
	DEBUG_STAMP("%d\n",i);	
	error_msg("error timed_wait");
	return -1;		
}*/

void str_cat(char *buf, char *str1,char *str2)
{		
	strncpy(buf,str1,strlen(str1)+1);
	strncat(buf,str2,strlen(str2)+1);
}

int exist_file(char *name, char *directory)
{
	DIR *dir = NULL;
	struct dirent *dir_file;
	
	dir = opendir(directory);
		
	while ((dir_file = readdir(dir)) != NULL)
	{
		if (strcmp(dir_file->d_name,name) == 0)
		{
			closedir(dir);
			return 1;
		}
	}
	
	closedir(dir);
	return 0;
}

int rd_open_file(char * path)
{
	int fd = open(path,O_RDONLY);
	if (fd == -1)
		error_msg("error in rd_open_file");
	return fd;	
}

off_t file_size(int fd)
{
	off_t size = lseek(fd,0,SEEK_END);
	if (size == -1)
		error_msg("error in lseek");
	DEBUG_STAMP("size = %ld\n", size);	
	return size;
}

char *map_file(int fd, int size, int offs)
{	
	char *buf = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, offs);
	if (buf == MAP_FAILED)
		error_msg("error in mmap");
	return buf;	
}

void read_file(int fd, char *buf, int size, int offs)
{
	lseek(fd, offs, SEEK_SET);
	while (size > 0)
	{
		int rc = read(fd, buf, size);
		if (rc == -1)
			error_msg("error in write");
		buf += rc;
		size -= rc;
	}
}

int wr_open_file(char *path)
{
	int fd;
	fd = open(path, O_WRONLY | O_CREAT, 0666);
	
	if(fd == -1)
		error_msg("error in wr_open_file");
	return fd;	
}

void write_file(int fd, char *buf, int size, int offs)
{
	lseek(fd, offs, SEEK_SET);
	while(size > 0)
	{
		int rc = write(fd, buf, size);
		if(rc == -1)
			error_msg("error in write");
		buf += rc;
		size -= rc;
	}
}
