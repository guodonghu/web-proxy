#include "network.h"

//rio_readn - robustly read n bytes (unbuffered)
ssize_t rio_readn(int fd, void *usrbuf, ssize_t n) {
    size_t nleft = n;
    ssize_t nread;
    char *bufp = (char*)usrbuf;

    while (nleft > 0) {
      if ((nread = read(fd, bufp, nleft)) < 0) {
        if (errno == EINTR) { // interrupted by sig handler return */
          nread = 0;
        }
        else {
          return -1;
        }
      } 
      else if (nread == 0) {
        break;
      }
      nleft -= nread;
      bufp += nread;
    }
    return (n - nleft);         
}

//rio_writen - robustly write n bytes (unbuffered)
ssize_t rio_writen(int fd, const void* usrbuf, ssize_t n) {
  size_t nleft = n;
  ssize_t nwritten;
  char *bufp = (char*)usrbuf;
  
  while (nleft > 0) {
    if ((nwritten = write(fd, bufp, nleft)) <= 0) {
      if (errno == EINTR) { //interrupted by sig handler return 
        nwritten = 0;
      }
      else {
        return -1;
      }
    }
    nleft -= nwritten;
    bufp += nwritten;
  }
  return n;
}

ssize_t rio_read(rio_t *rp, char *usrbuf, ssize_t n) {
    int cnt;
    while (rp->rio_cnt <= 0) {  //refill buffer 
      rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));
      if (rp->rio_cnt < 0) {
        if (errno != EINTR) {  // interrupted by sig handler return 
          return -1;
        }
      }
      else if (rp->rio_cnt == 0)  {
        return 0;
      }
      else { 
        rp->rio_bufptr = rp->rio_buf; // reset buffer ptr 
      }
    }
    cnt = n;          
    if (rp->rio_cnt < n) {
      cnt = rp->rio_cnt;
    }
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}

void rio_readinitb(rio_t *rp, int fd) {
    rp->rio_fd = fd;  
    rp->rio_cnt = 0;  
    rp->rio_bufptr = rp->rio_buf;
}


//read a line at a time
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, ssize_t maxlen) {
  ssize_t n, rc;
  char c, *bufp = (char*)usrbuf;
  n = 0;
  while (n < maxlen-1){
    if ((rc = rio_read(rp, &c, 1)) == 1) {
      n++;
      *bufp++ = c;
      if (c == '\n') {
        break;
      }
    }
    else if (rc == 0) {
      break;
    }
    else {
      return -1;
    }
  }
  //*bufp = 0;
  return n;
}

ssize_t rio_readp(int fd, void *ptr, ssize_t nbytes) {
  int n;
  while(1){
    if ((n = read(fd, ptr, nbytes)) < 0) {
      if (errno == EINTR) {
        continue;
      }
    }
    break;
  }
  return n;
}

ssize_t rio_writep(int fd, void *ptr, ssize_t nbytes) {
  int n;
  while(1){
    if ((n = write(fd, ptr, nbytes)) < 0) {
      if (errno == EINTR) continue;
    }
    break;
  }
  return n;
}


/* open_clientfd - open connection to server at <hostname, port> 
 *   and return a socket descriptor ready for reading and writing.
 *   Returns -1 and sets errno on Unix error. 
 *   Returns -2 and sets h_errno on DNS (gethostbyname) error.
 */

int open_clientfd(char *hostname, int port) {
    int clientfd;
    struct hostent *hp;
    struct sockaddr_in serveraddr;
    
    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      return -1; /* check errno for cause of error */
    }
    
    if ((hp = gethostbyname(hostname)) == NULL) {
      return -2; 
    }
    memset((char *) &serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    memcpy((char *)&serveraddr.sin_addr.s_addr, (char *)hp->h_addr, hp->h_length);
    serveraddr.sin_port = htons(port);

    if (connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0) {
      return -1;
    }
    return clientfd;
}

 
// bind and listen on a port
int open_listenfd(int port) {
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;
    
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      return -1;
    }
    
    //eliminates "Address already in use" error from bind.
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0) {
      return -1;
    }
    
    memset((char *) &serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)port); 
    if (bind(listenfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0) {
      return -1;
    }
    
    if (listen(listenfd, LISTENQ) < 0) {
      return -1;
    }
    return listenfd;
}




