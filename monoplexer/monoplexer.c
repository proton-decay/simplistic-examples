/* reference:
http://www.kernel.org/doc/man-pages/online/pages/man3/exec.3.html
http://www.yolinux.com/TUTORIALS/LinuxTutorialPosixThreads.html
http://www.kernel.org/doc/man-pages/online/pages/man4/tty_ioctl.4.html
http://www.cplusplus.com/forum/general/29137/
http://www.kernel.org/doc/man-pages/online/pages/man3/termios.3.html
*/

#define _XOPEN_SOURCE
#include <stdlib.h> /* exit(), grantpt(), unlockpt(), ptsname() */
#include <string.h> /* strlen(), strcpy(), strcmp() */
#include <stdio.h> /* printf(), fprinf(), getchar() */
#include <fcntl.h> /* open() */
#include <unistd.h> /* close(), read(), fork(), execvp() */
#include <errno.h> /* perror() */
#include <sys/wait.h> /* signal(), waitpid() */
#include <sys/ioctl.h> /* ioctl() */
#include <termios.h> /* struct termios, tcsetattr() */
#include <pthread.h> /* pthread_create(), pthread_join() */

typedef struct {unsigned short r, c, x, y;} Winsize;

int pid;
int fd_master;
int fd_slave;
char *master_name = "/dev/ptmx";
char slave_name[64] = {'\0'};
char *ptrpty;
char *argv_sh[2];
pthread_t thread_reader, thread_writer;
struct termios termios, termios_default;

void *keep_writing(void *data) {
	char c;
	int *fd = (int *) data;
	while ((c = getchar())) {
		write(*fd, &c, 1);
	}
	return NULL;
}

void *keep_reading(void *data) {
	int len;
	int *fd = (int *) data;
	char buffer[64] = {'\0'};
	while ((len = read(*fd, buffer, 1)) > 0) {
		buffer[len] = '\0'; /* read() doesn't nullbite string */
		printf(buffer);
		fflush(stdout); /* ??? Why this isn't flushed by default? */
	}
	return NULL;
}

void cleanup() {
	tcsetattr(STDIN_FILENO, TCSANOW, &termios_default);
	close(fd_slave);
	close(fd_master);
}

void handle_sigchld() {
	int status;
	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) {
		printf("Child exited\n");
		cleanup();
		perror("At the end"); /* Should print "Success" */
		exit(errno);
	}
}

int main() {
	Winsize w;

	/* Remember actual terminal settings */
	tcgetattr(STDIN_FILENO, &termios_default);

	/* Open master and grant permissions */
	fd_master = open(master_name, O_RDWR|O_NOCTTY);
	if (fd_master < 0) {
		perror("Couldn't open master file");
	    return errno;
	}
	if (grantpt(fd_master) == -1 || unlockpt(fd_master) == -1) {
		fprintf(stderr, "Couldn't grant permissions");
		return -1;
	}

	/* Get slave name */
	ptrpty = ptsname(fd_master);
	if (ptrpty == NULL) {
		close(fd_master);
		perror("Couldn't find slave file");
	    return errno;
	}
	if (strlen(ptrpty)+1 > sizeof(slave_name)) {
		close(fd_master);
		fprintf(stderr, "Slave file name is too long.");
		return -1;
	} else {
		strcpy(slave_name, ptrpty);
		printf("Using slave: %s\n", slave_name);
	}

	/* Open slave */
	fd_slave = open(slave_name, O_RDWR|O_NOCTTY);
	if (fd_slave < 0) {
		perror("Couldn't open slave file");
	    return errno;
	}

	/* Read from pty */
	/*while ((len = read(fd_master, buffer, sizeof(buffer)-1)) > 0) {*/
		/*buffer[len] = '\0'; [> Read doesn't "end" string <]*/
		/*printf("Read: %i\n'%s'\n", len, buffer);*/
		/*if (strcmp(buffer, "exit") == 0) {*/
			/*break;*/
		/*}*/
	/*}*/
	
	/* Now lets check this on bigger stuff */
	signal(SIGCHLD, handle_sigchld);
	switch (pid = fork()) {
	  case 0: /* Child - Spawn shell */
		close(STDIN_FILENO); close(STDOUT_FILENO); close(STDERR_FILENO);
		dup(fd_slave); dup(fd_slave); dup(fd_slave);

		setsid(); /* Required by shell to enable job control */

		/* Set size */
		w.c = 70;
		w.r = 30;
		ioctl(fd_master, TIOCSWINSZ, &w);

		argv_sh[0] = "/bin/sh";
		argv_sh[1] = NULL;
		execv(argv_sh[0], argv_sh);
		perror("Cannot run shell");
		return errno;
		break;

	  case -1: /* Error */
		perror("Fork");
		break;

	  default: /* Parent - Display shell spawned by child */
		printf("Child pid: %i\n", pid);

		tcgetattr(STDIN_FILENO, &termios);
		termios.c_lflag &= ~ICANON; /* Send without pressing enter */
		termios.c_lflag &= ~ECHO; /* Don't echo input */
		tcsetattr(STDIN_FILENO, TCSANOW, &termios);

		pthread_create(&thread_reader, NULL, keep_reading, (void *) &fd_master);
		pthread_create(&thread_writer, NULL, keep_writing, (void *) &fd_master);

		pthread_join(thread_reader, NULL);
		pthread_join(thread_writer, NULL);
	}

	/* We exit when child exits so */
	/* if we reach here something went wrong */
	perror("End error");
	cleanup();
	return -1;
}

