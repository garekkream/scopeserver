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
#include "scope_handlers.h"

#ifndef __VERSION_TAG
	#define __VERSION_TAG		"unknown"
#endif //__VERSION_TAG

#define INDEX_SIZE			0
#define INDEX_MSG_ID			1
#define INDEX_DEV_ID			2
#define INDEX_FLAGS			3
#define INDEX_DATA			4

static unsigned char server_status;
static int socket_desc;
static struct sockaddr_in serv;

void *worker(void *data)
{
	int fd = open(SCOPE_FILE_FIFO, O_WRONLY);
	int socket = *(int*)data;
	int size = 0;
	uint8_t buffer[SERVER_MAX_BUFFER] = {0x00};
	ScopeMsgClientReq *request = NULL;

	if((size = recv(socket, buffer, (SERVER_MAX_BUFFER - 1), 0)) < 0) {
		syslog(LOG_ERR, "Failed to read data from the socket! (errno = %d)\n", -errno);
		return NULL;
	}

	request = scope_msg_client_req__unpack(NULL, size, buffer);
	if(request == NULL) {
		syslog(LOG_ERR, "Failed to unpack received data!\n");
		return NULL;
	}

	memset(buffer, 0x00, SERVER_MAX_BUFFER);
	buffer[INDEX_SIZE] = 1;
	buffer[INDEX_MSG_ID] = request->msg_id;
	buffer[INDEX_SIZE] += 1;
	buffer[INDEX_DEV_ID] = request->device_id;
	if(request->has_payload_flags) {
		buffer[INDEX_SIZE] += 1;
		buffer[INDEX_FLAGS] = request->payload_flags & 0xF;
	}
	if(request->has_payload_data) {
		buffer[INDEX_SIZE] += request->payload_data.len;
		memcpy(&buffer[INDEX_DATA], request->payload_data.data, request->payload_data.len);
	}

	if(write(fd, buffer, (buffer[INDEX_SIZE] + 1)) < 0)
		syslog(LOG_ERR, "Failed to write to fifo, message will be lost! (errno = %d)\n", -errno);

	scope_msg_client_req__free_unpacked(request, NULL);
	close(fd);

	return NULL;
}

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
		pthread_t worker_id;

		pthread_create(&worker_id, NULL, &worker, (void*)&socket_new);
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
