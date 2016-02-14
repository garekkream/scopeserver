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
//#include "scope_handlers_register.h"

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

extern struct handler_data handlers[];

static void print_dbg(const ScopeMsgClientReq *msg)
{
	char buffer[SERVER_MAX_BUFFER * 5] = {0x00};
	int ret = 0;

	ret += sprintf(&buffer[0], "MSG = id: 0x%02x (%d) ", msg->msg_id, msg->msg_id);
	ret += sprintf(&buffer[ret], "device_id: 0x%02x (%d) ", msg->device_id, msg->device_id);
	if(msg->has_payload_flags)
		ret += sprintf(&buffer[ret], "flags: 0x%02x (%d) ", msg->payload_flags, msg->payload_flags);
	if(msg->has_payload_data) {
		size_t i = 0;
		ret += sprintf(&buffer[ret], "data: ");
		for(i = 0; i < msg->payload_data.len; i++) {
			if(ret < (SERVER_MAX_BUFFER * 5))
				ret += sprintf(&buffer[ret], "0x%02x ", *(msg->payload_data.data + i));
		}

		ret += sprintf(&buffer[ret], "(%s)", msg->payload_data.data);
	}

	syslog(LOG_INFO, "%s\n", buffer);

}

int scope_send_msg(int id, int dev_id, int flags, char *payload, int size)
{
	ScopeMsgServerRes response = SCOPE_MSG_SERVER_RES__INIT;
	void *buffer;
	int len = 0;
	int socket = 0;
	int ret = 0;

	response.id = (ScopeMsgServerRes__ScopeMsgIdRes)id;

	response.device_id = dev_id;

	if(flags != 0x00) {
		response.payload_flags = flags;
		response.has_payload_flags = 1;
	}

	if((payload != NULL) && (size > 0)) {
		response.payload_data.data = (unsigned char *)payload;
		response.payload_data.len = size;
		response.has_payload_data = 1;
	}

	len = scope_msg_server_res__get_packed_size(&response);

	buffer = malloc(len);

	scope_msg_server_res__pack(&response, buffer);

	socket = find_socket_by_devid(dev_id);

	if(write(socket, buffer, len) < 0) {
		syslog(LOG_ERR, "Failed to send message to client! (errno = %d, socket = %d, msg_id = %d, dev_id = %d)", -errno, socket, id, dev_id);
		ret =  -errno;
	}

	free(buffer);

	return ret;
}

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

	print_dbg(request);
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

void *consumer(__attribute__((unused)) void *data)
{
	char buffer[SERVER_MAX_BUFFER] = {0x00};
	char msg_size = 0;
	int fd = open(SCOPE_FILE_FIFO, O_RDONLY);

	if(fd < 0) {
		syslog(LOG_ERR, "Failed to open fifo file! (errno = %d)\n", -errno);
		return NULL;
	}

	while(server_status != SERVER_STATUS_STOPPED) {
		if(read(fd, &msg_size, 1)) {
			memset(buffer, 0x00, SERVER_MAX_BUFFER - 1);
			
			if(read(fd, buffer, msg_size)) {
				struct fifo_data *fdata = NULL;
				
				fdata = (struct fifo_data *) malloc(sizeof(struct fifo_data));

				if(fdata != NULL) {
					memset(fdata, 0x00, sizeof(struct fifo_data));
				
					fdata->msg_id	= buffer[0];
					fdata->dev_id	= buffer[1];
					fdata->flags	= buffer[2];
					memcpy(&(fdata->payload), &(buffer[3]), (msg_size - 2));

					if((fdata->msg_id < get_handlers_size())
					   && (handlers[fdata->msg_id].id == fdata->msg_id)
					   && (handlers[fdata->msg_id].handler != NULL)) {
						
						syslog(LOG_INFO, "0x%x 0x%x 0x%x %s", fdata->msg_id, fdata->dev_id, fdata->flags, fdata->payload);

						if(handlers[fdata->msg_id].hdata == HANDLER_NO_THREAD) {
							handlers[fdata->msg_id].handler(fdata);
							if(fdata) {
								free(fdata);
								fdata = NULL;
							}
						} else {
							pthread_t id;
							pthread_attr_t attr;

							pthread_attr_init(&attr);
							pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

							syslog(LOG_INFO, "Starting thread for id = %d!\n", fdata->msg_id);
							pthread_create(&id, &attr, handlers[fdata->msg_id].handler, (void *)fdata);
						}
					 } else {
						if(fdata != NULL) {
							free(fdata);
							fdata = NULL;
						}

						syslog(LOG_ERR, "Failed to start handler! (id = %d [max id = %lu])", buffer[0], (get_handlers_size() -1));
					 }
				} else {
					syslog(LOG_ERR, "Failed to allocate memory for handler! (id = %d)\n", buffer[0]);
					return NULL;
				}
			}
		}
		sleep(1);
	}
	close(fd);
	syslog(LOG_INFO, "Consumer stopped\n");

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
	pthread_t consumer_id;

	server_status = SERVER_STATUS_RUNNING;

	pthread_create(&consumer_id, NULL, &consumer, NULL);

	syslog(LOG_INFO, "Server started! (Version =%s)\n", (char *)(__VERSION_TAG));

	while((socket_new = accept(socket_desc, (struct sockaddr *)&serv, (socklen_t *)&(client_len))) && (server_status != SERVER_STATUS_STOPPED)) {
		pthread_t worker_id;

		pthread_create(&worker_id, NULL, &worker, (void*)&socket_new);
	}

	pthread_join(consumer_id, NULL);

	register_cleanup();
	syslog(LOG_INFO, "Exiting\n");

	return 0;
}

void server_stop(void)
{
	server_status = SERVER_STATUS_STOPPED;

	syslog(LOG_INFO, "Server stopped!\n");

	close(socket_desc);
}
