#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "scope_server.h"

#ifndef __VERSION_TAG
	#define __VERSION_TAG		"unknown"
#endif //__VERSION_TAG


static unsigned char server_status;
static int socket_desc;
static struct sockaddr_in serv;

int server_init(void)
{
	if(server_status != SERVER_STATUS_STOPPED) {
		syslog(LOG_ERR, "Server is still running! Initialization failed!\n");
		return -EALREADY;
	}

	server_status = SERVER_STATUS_STOPPED;

	socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_desc < 0) {
		syslog(LOG_ERR, "Cannot create socket descriptor! Initialization failed!\n");
		return -EINVAL;
	}

	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = INADDR_ANY;
	serv.sin_port = htons(6633);

	if(bind(socket_desc, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
		syslog(LOG_ERR, "Cannot bind to port! Initialization failde!\n");
		return -EINVAL;
	}

	listen(socket_desc, 3);

	syslog(LOG_INFO, "Server initialized!\n");

	return 0;
}

int server_start(void)
{
	struct sockaddr_in client;
	int socket_new;
	int client_len = sizeof(client);

	server_status = SERVER_STATUS_RUNNING;

	syslog(LOG_INFO, "Server started! (Version =%s)\n", (char *)(__VERSION_TAG));

	while((socket_new = accept(socket_desc, (struct sockaddr *)&serv, (socklen_t *)&(client_len))) && (server_status != SERVER_STATUS_STOPPED)) {
	
	}

	syslog(LOG_INFO, "Exiting\n");

	return 0;
}

void server_stop(void)
{
	server_status = SERVER_STATUS_STOPPED;

	syslog(LOG_INFO, "Server stopped!\n");

	close(socket_desc);
}
