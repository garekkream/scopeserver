#ifndef SCOPE_HANDLERS_H
#define SCOPE_HANDLERS_H

#define HNDL_MAX_BUFFER			80

struct fifo_data {
	unsigned int msg_id;
	unsigned int dev_id;
	char flags;
	char payload[HNDL_MAX_BUFFER];
};

#endif //SCOPE_HANDLERS_H
