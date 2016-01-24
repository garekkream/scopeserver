#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "scope_server.h"


/**
 * @brief Signal handler for server.
 *
 * @param sig - Signal No. provided by system
 */
void server_sig_handler(int sig)
{
	switch(sig) {
	case SIGTERM:
		server_stop();
		break;
	default:
		break;
	}
}


/**
 * @brief  Main function of whole server.
 *
 * @return	EXIT_SUCCESS - when forked properly
 * @return	EXIT_FAILURE - on error during server initialization
 * @return	0 - on application exit
 */
int main(void)
{
	pid_t pid, sid;
	int fd;
	struct sigaction sa;
	char buff[16] = {0};

	sa.sa_handler = server_sig_handler;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGTERM, &sa, NULL);

	openlog("ScopeServer", LOG_PID | LOG_LOCAL0,  LOG_LOCAL0);

	pid = fork();
	if(pid < 0) {
		syslog(LOG_ERR, "Fork failed, daemon couldn't start! (errno = %d)\n", -errno);
		return EXIT_FAILURE;
	} else if(pid > 0){
		return EXIT_SUCCESS;
	}

	openlog("ScopeServer", LOG_PID | LOG_LOCAL0,  LOG_LOCAL0);

	umask(0);

	syslog(LOG_INFO, "[ScopeServer]: Daemon started!\n");

	sid = setsid();
	if(sid < 0) {
		syslog(LOG_ERR, "Setsid failed, couldn't create new sid! (errno = %d)\n", -errno);
		return EXIT_FAILURE;
	}

	sprintf(buff, "%ld", (long)getpid());
	fd = open(SCOPE_FILE_PID, O_CREAT | O_WRONLY);
	if(fd <0) {
		syslog(LOG_ERR, "Couldn't create pid file! (errno = %d)\n", -errno);
		return EXIT_FAILURE;
	}

	if(write(fd, buff, strlen(buff)+1) < 0) {
		syslog(LOG_ERR, "Couldn't save pid file for ScopeServer! (errno = %d)\n", -errno);
		close(fd);
		return EXIT_FAILURE;
	}
	close(fd);

	if(mkfifo(SCOPE_FILE_FIFO, O_RDWR) < 0) {
		syslog(LOG_ERR, "Couldn't create fifo file: %s! (errno = %d)\n", SCOPE_FILE_FIFO, -errno);
		return EXIT_FAILURE;
	}

	if(chdir("/") < 0) {
		syslog(LOG_ERR, "Chdir failed! (errno = %d)\n", -errno);
		return EXIT_FAILURE;
	}
	
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	if(server_init() < 0) {
		syslog(LOG_ERR, "Unable to initizalize the server! (errno = %d)\n", -errno);
		return EXIT_FAILURE;
	}

	server_start();

	unlink(SCOPE_FILE_PID);
	unlink(SCOPE_FILE_FIFO);
	closelog();

	return 0;
}
