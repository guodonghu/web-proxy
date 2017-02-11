#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>


/* Simplifies calls to bind(), connect(), and accept() */
/* $begin sockaddrdef */
typedef struct sockaddr SA;
/* $end sockaddrdef */

/* Persistent state for the robust I/O (Rio) package */
/* $begin rio_t */
#define RIO_BUFSIZE 8192
typedef struct {
    int rio_fd;                /* descriptor for this internal buf */
    ssize_t rio_cnt;               /* unread bytes in internal buf */
    char *rio_bufptr;          /* next unread byte in internal buf */
    char rio_buf[RIO_BUFSIZE]; /* internal buffer */
} rio_t;
/* $end rio_t */

/* Misc constants */
#define	MAXLINE	 8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */


/* Signal wrappers */
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/* Rio (Robust I/O) package */
ssize_t rio_readp(int fd, void *ptr, ssize_t nbytes);
ssize_t rio_writep(int fd, void *ptr, ssize_t nbytes);
ssize_t rio_read(rio_t *rp, char *usrbuf, ssize_t n);
ssize_t rio_readn(int fd, void *usrbuf, ssize_t n);
ssize_t rio_writen(int fd, const void *usrbuf, ssize_t n);
void rio_readinitb(rio_t *rp, int fd); 
ssize_t	rio_readlineb(rio_t *rp, void *usrbuf, ssize_t maxlen);

/* Client/server helper functions */
int open_clientfd(char *hostname, int portno);
int open_listenfd(int portno);

//network connection functions
void *forwarder(void* args);
void *httpConnection(void* args);
void httpsConnection(int clientfd, rio_t client, char *inHost, int serverPort);



#endif
