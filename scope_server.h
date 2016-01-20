#ifndef SCOPE_SERVER_H
#define SCOPE_SERVER_H

#define SCOPE_FILE_PID		"/var/run/scope_daemon.pid"
#define SCOPE_FILE_FIFO		"/tmp/scope_fifo"

#define SERVER_STATUS_STOPPED	0
#define SERVER_STATUS_RUNNING	1

extern int server_init(void);
extern int server_start(void);
extern void server_stop(void);

#endif //SCOPE_SERVER_H
