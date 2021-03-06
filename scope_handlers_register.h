#ifndef SCOPE_HANDLERS_REGISTER_H
#define SCOPE_HANDLERS_REGISTER_H

int find_socket_by_devid(int dev_id);
int update_socket_by_devid(int dev_id, int new_socket);
int register_init(void);
int register_cleanup(void);
void *handler_register(void *data);
void *handler_deregister(void *data);

#endif //SCOPE_HANDLERS_REGISTER_H
