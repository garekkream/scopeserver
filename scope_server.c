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
		
		return -EALREADY;
	}
	return 0;
}

int server_start(void)
{

	return 0;
}

void server_stop(void)
{

}
