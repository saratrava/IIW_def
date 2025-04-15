#ifndef CONSTANT_LIB_H_   /* Include guard */
#define CONSTANT_LIB_H_

#define MAX_PDU     1024
#define HEADER_MAX_SIZE  15
#define SERVER_CONF_FILE "server.conf"
#define CLIENT_CONF_FILE "client.conf"

#define CON_MSG_FORMAT "CON_MSG OK TD_PORT=%u \n W_SIZE=%u \n T_TIME=%u\n T_ADAPT=%d\n P_LOSS=%u"
#define ERR_MSG_FORMAT "ERR_MSG TD_PORT=%u \n T_TIME=%u\n P_LOSS=%u\n ERROR=%u"
#define CON_ERR_FILE_NOT_EXIST 1
#define CON_ERR_FILE_ALREADY_UPLOADED 2

#endif // CONSTANT_LIB_H_
