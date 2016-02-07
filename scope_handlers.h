#ifndef SCOPE_HANDLERS_H
#define SCOPE_HANDLERS_H

#define HNDL_MAX_BUFFER			80

enum handler_option {
	HANDLER_THREAD,
	HANDLER_NO_THREAD
};

struct fifo_data {
	unsigned int msg_id;
	unsigned int dev_id;
	char flags;
	char payload[HNDL_MAX_BUFFER];
};

struct handler_data {
	unsigned int id;
	void* (*handler)(void *data);
	enum handler_option hdata;
};

unsigned long int get_handlers_size(void);

void *handler_dummy(void *data);

#endif //SCOPE_HANDLERS_H
