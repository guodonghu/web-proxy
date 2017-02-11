#include <stdio.h>
#include "network.h"
#include <pthread.h>
#include <vector>
#include <string>
#include <iostream>

#define   FILTER_FILE   "proxy.filter"
#define   LOG_FILE      "proxy.log"
#define   DEBUG_FILE	"proxy.debug"


/*============================================================
 * function declarations
 *============================================================*/
		       
void *forwarder(void* args);
void *webTalk(void* args);
void secureTalk(int clientfd, rio_t client, char *inHost, int serverPort);
void ignore();

int proxyPort;
pthread_mutex_t mutex;

typedef struct {
	char buf[MAXLINE];
	size_t length;
}piece_t;

typedef struct {
	std::string request;
	std::vector<piece_t> response;
}cache_t;

std::vector<cache_t> cache;

/* main function for the proxy program */

int main(int argc, char *argv[])
{
  int listenfd, connfd, optval, serverPort;
  unsigned int clientlen;
  struct sockaddr_in clientaddr;
  pthread_t tid;
  
  if (argc < 2) {
    printf("Usage: %s <port>\n", argv[0]);
    exit(1);
  }
  
  serverPort = 80;

  Signal(SIGPIPE, (handler_t*) ignore);
  
  proxyPort = atoi(argv[1]);

  /* start listening on proxy port */

  listenfd = open_listenfd(proxyPort);

  optval = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int)); 
  
  pthread_mutex_init(&mutex, NULL);

  while(1) {

    clientlen = sizeof(clientaddr);

    /* accept a new connection from a client here */
    connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);
    if (connfd < 0) {
    	continue;
    }
    
    /* create a new thread to process the new connection */
    int* fdp = (int*)malloc(2*sizeof(int));
    fdp[0] = connfd;
    fdp[1] = serverPort;
    pthread_create(&tid, NULL, webTalk, fdp);
    pthread_detach(tid);

  }
  pthread_mutex_destroy(&mutex);
  return 0;
}

void parseAddress(char* url, char* host, char** file, int* serverPort)
{
	char *point1;
  char *saveptr;

	if(strstr(url, "http://"))
		url = &(url[7]);
	*file = strchr(url, '/');
	
	strcpy(host, url);

	/* first time strtok_r is called, returns pointer to host */
	/* strtok_r (and strtok) destroy the string that is tokenized */

	/* get rid of everything after the first / */

	strtok_r(host, "/", &saveptr);

	/* now look to see if we have a colon */

	point1 = strchr(host, ':');
	if(!point1) {
		*serverPort = 80;
		return;
	}
	
	/* we do have a colon, so get the host part out */
	strtok_r(host, ":", &saveptr);

	/* now get the part after the : */
	*serverPort = atoi(strtok_r(NULL, "/",&saveptr));
}

void *webTalk(void* args)
{
  int serverfd, clientfd, serverPort;
  char buf1[MAXLINE], buf2[MAXLINE], buf3[MAXLINE];
  char host[MAXLINE];
  char url[MAXLINE];
  char *cmd, *file;
  std::string send;
  std::vector<piece_t> receive;
  cache_t acache;
  rio_t server, client;
  char slash[10];
  strcpy(slash, "/");
  
  clientfd = ((int*)args)[0];
  serverPort = ((int*)args)[1];
  free(args);
  
  memset(buf1, 0, MAXLINE);
  memset(buf2, 0, MAXLINE);
  memset(buf3, 0, MAXLINE);
  memset(url, 0, MAXLINE);
  rio_readinitb(&client, clientfd);

  /*
   * Determine protocol (CONNECT or GET)
   */
  rio_readlineb(&client, buf1, MAXLINE);
  char header[MAXLINE];
  strcpy(header, buf1);
  //split cmd and url from the header
  //cmd = strtok_r(buf1, " ", &saveptr);
  //strcpy(url,strtok_r(NULL, " ", &saveptr));
  //printf("%s\n", cmd);
  sscanf(buf1, "%s %s %s", buf2, url, buf3);
  cmd = buf2;
  parseAddress(url, host, &file, &serverPort);
  //printf("%s\n%d\n",host, serverPort);
  if (strcmp(cmd, "GET") == 0) {

	  send = send + header;
	  int length;
	  while(1) {
		  length = rio_readlineb(&client, buf2, MAXLINE);
		  if (length > 0) {
			  if (strcmp(buf2, "Connection: keep-alive\r\n") == 0) {
				  //printf("%s\n", buf2);
				  send = send + "Connection: close\r\n";
				  //rio_writen(serverfd, "Connection: close\r\n", strlen("Connection: close\r\n"));
				  //printf("Connection: close\r\n");
			  }
			  else if (buf2[0] == 'c' && buf2[1] == 'o') {
				  continue;
			  }
			  else {
				  send = send + buf2;
			  }
		  }
		  else
			  break;
		  if (strcmp(buf2, "\r\n") == 0) {
			  send = send + buf2;
			  //rio_writen(serverfd, "\r\n", strlen("\r\n"));
			  break;
		  }
		  memset(buf2,0,strlen(buf2));
	  }

	  bool responsed = false;
	  for (int i = 0; i < (int)cache.size(); i++) {
		  if (cache[i].request.compare(send) == 0) {
			  //printf("In cache!\n");
			  for (int j = 0; j < (int)cache[i].response.size(); j++) {
				  if (cache[i].response[j].buf != NULL) {
					  rio_writen(clientfd, cache[i].response[j].buf, cache[i].response[j].length);
					  responsed = true;
				  }
			  }
			  if (responsed == true) {
				  //printf("Cache responsed!\n");
			  }
			  //printf("Out cache!\n");
			  close(clientfd);
			  return NULL;
		  }
	  }

	  int time = 5;
	  while (time) {
		  if ((serverfd = open_clientfd(host, serverPort)) > 0)
			  break;
		  time--;
	  }
	  if (serverfd <= 0) {
		  //printf("failed to establish connection.\n");
		  close(clientfd);
		  return NULL;
	  }
	  if (send.size() != 0) {
		  rio_writen(serverfd, send.c_str(), send.size());
	  }
	  //printf("finish send\n");

	  // GET: now receive the response
	  rio_readinitb(&server, serverfd);
	  while(1) {
		  piece_t temp;
		  length = rio_readn(serverfd, temp.buf, MAXLINE);
		  if (length > 0) {
			  temp.length = length;
			  receive.push_back(temp);
		  }
		  //if (strcmp(buf3, "\n") == 0) {
		//	  break;
		  //}
		  else break;
		  memset(buf3,0,MAXLINE);
	  }
	  //printf("%s\n", receive);
	  for (int j = 0; j < (int)receive.size(); j++) {
		  //printf("%s",receive[j]);
		  if (receive[j].buf != NULL) {
			  rio_writen(clientfd, receive[j].buf, receive[j].length);
		  }
	  }
	  //printf("finish receive\n");
	  acache.request = send;
	  acache.response = receive;
	  cache.push_back(acache);
	  close(serverfd);
  }
  // CONNECT: call a different function, securetalk, for HTTPS
  if (strcmp(cmd, "CONNECT") == 0) {
	  secureTalk(clientfd, client, host, serverPort);
  }

  close(clientfd);
  return NULL;

}

/* this function handles the two-way encrypted data transferred in
   an HTTPS connection */

void secureTalk(int clientfd, rio_t client, char *inHost, int serverPort)
{
  int serverfd;
  if (serverPort == proxyPort)
    serverPort = 443;
  
  /* Open connecton to webserver */
  /* clientfd is browser */
  /* serverfd is server */
  while (1) {
  		if ((serverfd = open_clientfd(inHost, serverPort)) > 0)
  			break;
  }

  /* let the client know we've connected to the server */
  rio_writen(clientfd, "HTTP/1.1 200 Connection established\r\n\r\n", strlen("HTTP/1.1 200 Connection established\r\n\r\n"));

  /* spawn a thread to pass bytes from origin server through to client */
  pthread_t thds[2];
  int * fd1 = (int *)malloc(2*sizeof(int));
  fd1[0] = clientfd;
  fd1[1] = serverfd;
  pthread_create(&thds[0], NULL, forwarder, fd1);

  /* now pass bytes from client to server */
  int * fd2 = (int *)malloc(2*sizeof(int));
  fd2[0] = serverfd;
  fd2[1] = clientfd;
  pthread_create(&thds[1], NULL, forwarder, fd2);
  for (int i = 0; i < 2; i++) {
    pthread_join(thds[i], NULL);
  }
  close(serverfd);
}

/* this function is for passing bytes from origin server to client */

void *forwarder(void* args)
{
  int serverfd, clientfd;
  char buf1[MAXLINE];
  clientfd = ((int*)args)[0];
  serverfd = ((int*)args)[1];
  free(args);

  int length;
  while(1) {
    
    /* serverfd is for talking to the web server */
    /* clientfd is for talking to the browser */
	if ((length = rio_readp(serverfd, buf1, MAXLINE)) > 0) {
		rio_writep(clientfd, buf1, length);
	}
	else
		break;
	memset(buf1,0,strlen(buf1));
  }
  return NULL;
}


void ignore()
{
	;
}
