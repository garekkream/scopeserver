#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "scope_server.h"
#include "scope_handlers.h"
#include "list.h"

#define NAME_DEVICE_INTERNAL		"webpage"

#define MAX_REG_INTERN_DEVICES		1
#define MAX_REG_USER_DEVICES		10
#define MAX_REG_DEVICES			(MAX_REG_INTERN_DEVICES + MAX_REG_USER_DEVICES)

#define STATUS_ID_NOTFOUND		0
#define STATUS_ID_FOUND			1
#define STATUS_DEV_NOTFOUND		2
#define STATUS_DEV_FOUND		3

#ifndef __VERSION_TAG
	#define __VERSION_TAG		"unknown"
#endif //__VERSION_TAG

LIST_HEAD(devices_list);

static int find_free_id(void);
static int extract_register_data(const char *in_data, char *out_data);
static int extract_ip_from_socket(int socket, char *out_data);
static int register_response(int dev_id);
static void print_devices(void);

struct register_data {
	struct list_head list;

	unsigned char id;
	char name[SERVER_MAX_BUFFER];
	char sw_ver[SERVER_MAX_BUFFER];
	char client_ip[SERVER_MAX_BUFFER];
	int socket;
};

static int devices_cnt;

static int find_free_id(void)
{
	int id = devices_cnt;
	char status = STATUS_ID_NOTFOUND;
	struct list_head *pos, *q;

	while(status != STATUS_ID_FOUND) {
		list_for_each_safe(pos, q, &devices_list) {
			struct register_data *dev;

			dev = list_entry(pos, struct register_data, list);

			if(dev->id == id) {
				if(id <= MAX_REG_DEVICES)
					id++;
				else
					id = 0;
			} else
				status = STATUS_ID_FOUND;
		}
	}

	return id;
}

static int extract_register_data(const char *in_data, char *out_data)
{
	int pos = 0;
	int len = in_data[pos++]; //payload = [0]<len>[1..n]<data>

	if(len >= SERVER_MAX_BUFFER) {
		syslog(LOG_ERR, "Payload data is bigger than max buffer size!");

		return -EINVAL;
	}

	memcpy(out_data, &(in_data[pos]), len);

	return (pos + len);
}

static int extract_ip_from_socket(int socket, char *out_data)
{
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);

	getpeername(socket, (struct sockaddr *)&addr, &len);
	strcpy(out_data, inet_ntoa(addr.sin_addr));

	return strlen((const char *)out_data) ? 0 : -ENODATA;
}

static int register_response(int dev_id)
{
	int flags = 0x00;
	char buffer[SERVER_MAX_BUFFER] = {0x00};
	char size = 0;
	
	flags |= SCOPE_MSG_SERVER_RES__SCOPE_REGISTER_FLAGS_RES__SERVER_SW_VER;
	size = strlen((const char *)__VERSION_TAG);
	buffer[0] = size;
	
	memcpy(&(buffer[1]), (const char *)__VERSION_TAG, size);

	if(scope_send_msg(SCOPE_MSG_SERVER_RES__SCOPE_MSG_ID_RES__SCOPE_MSGID_REGISTER_RES, dev_id, flags, buffer, size + 1) < 0) {
		syslog(LOG_ERR, "Failed to send registration response! (dev_id = %d, errno = %d)", dev_id, -errno);
		return -errno;
	}

	return 0;
}

static void print_devices(void)
{
	struct list_head *pos, *q;

	syslog(LOG_INFO, "Devices list: ");
	syslog(LOG_INFO, "=============================");

	list_for_each_safe(pos, q, &devices_list) {
		struct register_data *dev = list_entry(pos, struct register_data, list);

		syslog(LOG_INFO, "Device id: %d", dev->id);
		syslog(LOG_INFO, "Device name: %s", dev->name);
		syslog(LOG_INFO, "Device sw_ver: %s", dev->sw_ver);
		syslog(LOG_INFO, "Device socket: %d", dev->socket);
		syslog(LOG_INFO, "Device IP: %s", dev->client_ip);
	}
}

int find_socket_by_devid(int dev_id)
{
	struct list_head *pos, *q;

	list_for_each_safe(pos, q, &devices_list) {
		struct register_data *dev = list_entry(pos, struct register_data, list);

		if(dev->id == dev_id)
			return dev->socket;
	}

	return -EINVAL;
}

int update_socket_by_devid(int dev_id, int new_socket)
{
	struct list_head *pos, *q;

	list_for_each_safe(pos, q, &devices_list) {
		struct register_data *dev = list_entry(pos, struct register_data, list);

		if(dev->id == dev_id) {
			syslog(LOG_INFO, "Old socket: %d, New socket: %d", dev->socket, new_socket);
			dev->socket = new_socket;
			return 0;
		}
	}

	return -EINVAL;
}

int register_init(void)
{
	struct register_data *dev = NULL;

	dev = (struct register_data *)malloc(sizeof(struct register_data));

	if(!dev) {
		syslog(LOG_ERR, "Failed to allocate memory for internal device! (errno = %d)", -errno);
		return -ENOMEM;
	}

	devices_cnt = 0;

	memset(dev->name, 0x00, (SERVER_MAX_BUFFER -1));
	strcpy(dev->name, (const char *)NAME_DEVICE_INTERNAL);
	strcpy(dev->client_ip, "127.0.0.1");
	dev->id = 0x00;
	dev->socket = 0x00;

	list_add(&(dev->list), &(devices_list));
	devices_cnt++;

	syslog(LOG_INFO, "Registration data initialized!");

	return 0;
}

int register_cleanup(void)
{
	struct list_head *pos, *q;

	list_for_each_safe(pos, q, &devices_list) {
		struct register_data *dev = list_entry(pos, struct register_data, list);

		list_del(pos);

		free(dev);
		dev = NULL;

		devices_cnt--;
	}

	if(devices_cnt != 0) {
		syslog(LOG_ERR, "Something went wrong! Devices counter is not 0!");
		return -EINVAL;
	}

	syslog(LOG_INFO, "Registration data cleaned up!");

	return 0;
}

void *handler_register(void *data)
{
	struct fifo_data hdata = *(struct fifo_data*)data;
	int pos = 0;

	syslog(LOG_INFO, "Started handler %s (id = %d)", __func__, hdata.msg_id);

	if(devices_cnt < MAX_REG_DEVICES) {
		struct register_data *dev = (struct register_data *)malloc(sizeof(struct register_data));

		if(!dev) {
			syslog(LOG_ERR, "Failed to allocate memory for internal device! (errno = %d)", -errno);
			return NULL;
		}

		memset(dev, 0x00, sizeof(struct register_data));

		dev->id = find_free_id();

		if(hdata.flags & SCOPE_MSG_CLIENT_REQ__SCOPE_REGISTER_FLAGS_REQ__CLIENT_NAME) {
			pos += extract_register_data(hdata.payload, dev->name);

			if(pos < 0)
				goto fail_extraction;
		}

		if(hdata.flags & SCOPE_MSG_CLIENT_REQ__SCOPE_REGISTER_FLAGS_REQ__CLIENT_SW_VER) {
			pos += extract_register_data(&(hdata.payload[pos]), dev->sw_ver);

			if(pos < 0)
				goto fail_extraction;
		}
		//Extract socket file descriptor
		{
			char buff[10] = {0};

			pos += extract_register_data(&(hdata.payload[pos]), buff);
			dev->socket = (int)strtol(buff, NULL, 10);

			extract_ip_from_socket(dev->socket, dev->client_ip);
		}

		list_add(&(dev->list), &devices_list);
		devices_cnt++;

		register_response(dev->id);
		print_devices();

		return NULL;

fail_extraction:
		syslog(LOG_ERR, "Failed to register device! Payload data is not correct");

		if(dev) {
			free(dev);
			dev = NULL;
		}

		return NULL;
	}

	syslog(LOG_ERR, "Failed to add new device! List is full!");
	return NULL;
}

void *handler_deregister(void *data)
{
	struct fifo_data hdata = *(struct fifo_data*)data;
	struct list_head *pos, *q;
	char status = STATUS_DEV_NOTFOUND;

	syslog(LOG_INFO, "Started handler %s (id = %d)", __func__, hdata.msg_id);

	list_for_each_safe(pos, q, &devices_list) {
		struct register_data *dev = list_entry(pos, struct register_data, list);

		if(hdata.dev_id == dev->id) {
			if(scope_send_msg(SCOPE_MSG_SERVER_RES__SCOPE_MSG_ID_RES__SCOPE_MSGID_UNREGISTER_RES, dev->id, 0x00, NULL, 0)) {
				syslog(LOG_ERR, "Failed to send unregister message! (dev_id = %d)", dev->id);
				return NULL;
			}

			close(dev->socket);

			list_del(pos);

			free(dev);
			dev = NULL;

			devices_cnt--;

			status = STATUS_DEV_FOUND;
		}
	}

	if(status != STATUS_DEV_FOUND)
		syslog(LOG_ERR, "Unable to remove device from the list! Unknown ID! (devid = %d)", hdata.dev_id);
	else
		syslog(LOG_INFO, "Device sucessfuly unregistered!");

	return NULL;
}
