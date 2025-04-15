#ifndef MYLIB_H_   /* Include guard */
#define MYLIB_H_

#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

//#define DEBUG

#define CONFIG_U(conf_file,tag) fscanf(conf_file," "#tag "='%u'",&tag)
#define CONFIG_S(conf_file,tag) fscanf(conf_file," "#tag "='%s'",tag)

#ifdef DEBUG
#define DEBUG_STAMP(...) printf(__VA_ARGS__)
#else
#define DEBUG_STAMP(...) 
#endif 

void usage(char *path, int client);
void error_msg(char * msg);
void error_td_msg(char *msg);

void str_cat(char *buf, char *str1,char *str2); 
int exist_file(char *name, char *directory);
int rd_open_file(char * path);
int wr_open_file(char * path);
off_t file_size(int fd);
char * map_file(int fd,int size,int offs);
void read_file(int fd,char *buf,int size,int offs);
void write_file(int fd,char *buf,int size,int offs);

void lock(pthread_mutex_t *mtx);
void unlock(pthread_mutex_t *mtx);

void wait(pthread_cond_t *cond, pthread_mutex_t *mtx);
//int timed_wait(pthread_cond_t *cond, pthread_mutex_t *mtx,struct timespec *time);
void awake(pthread_cond_t *cond);

#endif // MYLIB_H_
