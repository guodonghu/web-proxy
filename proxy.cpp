#include <stdio.h>
#include "network.h"
#include <thread>
#include <vector>
#include <string>
#include <iostream>
#include <unordered_map>
#include <fstream>
#include <mutex>
#include <chrono>
#include <ctime>

using namespace std;

int UID = 0;
int proxyPort;
mutex log_mtx;
mutex uid_mtx;
ofstream log_fs;
#define LOG "/var/log/erss-proxy.log"


typedef struct {
	char buf[BUFFER_SIZE];
	size_t length;
} piece;

class HTTP {
public:
  //key: Expires, Date, max-age, Etag
  unordered_map<string, string> cache_info;
  vector<piece> response;
};


unordered_map<string, HTTP> cache;

void signal_handler(int signal) {
  return;
}

string getTime() {
  chrono::time_point<chrono::system_clock> now;
  now = std::chrono::system_clock::now();
  time_t now_time = chrono::system_clock::to_time_t(now);
  return ctime(&now_time);
}

int getUID() {
  int temp;
  uid_mtx.lock();
  ++UID;
  temp = UID;
  uid_mtx.unlock();
  return temp;
}


int start_daemon() {
  //ignore signals
  signal(SIGPIPE,SIG_IGN);
  if (signal(SIGHUP, signal_handler) == SIG_ERR) {
    cerr << "can not handle SIGHUP" << endl;
    return -1;
  }
  
  //open log file
  log_fs.open(LOG, ofstream::out);
  if (!log_fs.is_open()) {
    cerr << "can not open log file" << endl;
    return -1;
  }
  //drop privilege
  uid_t temp = getuid();
  seteuid(temp);

  
  if (daemon(0, 0) == -1) {
    return -1;
  }
  //clear umask
  umask(0);
  //fork second time not to be the session leader
  pid_t pid = 0;
  pid = fork();
  if (pid < 0) {
    exit(EXIT_FAILURE);
  }
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }
  log_fs << "dameon id is :" << getpid() << endl;
  //successfully set up daemon
  return 0;
}

void write_log(string message) {
  log_mtx.lock();
  log_fs << message << endl;
  log_mtx.unlock();
}



int main(int argc, char *argv[]) {
  int listenfd, connfd, optval, serverPort;
  unsigned int clientlen;
  struct sockaddr_in clientaddr;
  
  if (argc < 2) {
    printf("Usage: %s <port>\n", argv[0]);
    exit(1);
  }

  if (start_daemon() == -1) {
    perror("can not start deamon:");
    return EXIT_FAILURE;
  }

  serverPort = 80;
  proxyPort = atoi(argv[1]);

  // start listening on proxy port

  listenfd = open_listenfd(proxyPort);

  optval = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int)); 
  
  while(1) {
    clientlen = sizeof(clientaddr);
    // accept a new connection from a client here
    connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);
    if (connfd < 0) {
    	continue;
    }

    char client_ip[100];
    strcpy(client_ip,inet_ntoa(clientaddr.sin_addr));
    
    // create a new thread to process the new connection 
    thread t(httpConnection, connfd, serverPort, client_ip);
    t.detach();
  }
  
  close(listenfd);
  return 0;
}




void parseAddress(char* url, char* host, char** file, int* serverPort) {
	char *point1;
  char *saveptr;

	if(strstr(url, "http://")) {
		url = &(url[7]);
  }
	*file = strchr(url, '/');
	strcpy(host, url);
	//get pointer to host
	strtok_r(host, "/", &saveptr);

	point1 = strchr(host, ':');
	if(!point1) {
		*serverPort = 80;
		return;
	}	
	strtok_r(host, ":", &saveptr);
	*serverPort = atoi(strtok_r(NULL, "/",&saveptr));
}

void httpConnection(int clientfd, int serverPort, char *client_addr) {
  int id = getUID();
  char *cmd, *file;
  std::string request;
  std::vector<piece> response;
  rio_t server, client;
  int serverfd;
  char buf1[BUFFER_SIZE], buf2[BUFFER_SIZE], buf3[BUFFER_SIZE];
  char host[BUFFER_SIZE];
  char url[BUFFER_SIZE];
  
  // clientfd = ((int*)args)[0];
  //serverPort = ((int*)args)[1];
  //free(args);
  memset(buf1, 0, BUFFER_SIZE);
  memset(buf2, 0, BUFFER_SIZE);
  memset(buf3, 0, BUFFER_SIZE);
  memset(url, 0, BUFFER_SIZE);
  rio_readinitb(&client, clientfd);
  rio_readlineb(&client, buf1, BUFFER_SIZE);
  char header[BUFFER_SIZE];
  memset(header, 0, BUFFER_SIZE);
  strcpy(header, buf1);
  sscanf(buf1, "%s %s %s", buf2, url, buf3);
  cmd = buf2;
  parseAddress(url, host, &file, &serverPort);
  // identify method
  if (strcmp(cmd, "GET") == 0 || strcmp(cmd, "POST") == 0) {
	  request = request + header;
    bool hasContent = false;
	  int length;
	  while(1) {
		  length = rio_readlineb(&client, buf2, BUFFER_SIZE);
		  if (length > 0) {
			  if (strcmp(buf2, "Connection: keep-alive\r\n") == 0) {
				  request = request + "Connection: close\r\n";
			  }
			  else {
          if (strstr(buf2, "Content-Length") != NULL) {
            hasContent = true;
          }
				  request = request + buf2;
			  }
		  }
		  else
			  break;
      if (strcmp(buf2, "\r\n") == 0 && !hasContent) {
        request = request + buf2;
        break;
      }
		  memset(buf2,0,strlen(buf2));
	  }
    
    string now = getTime();
    string temp(header);
    string requestMessage = to_string(id) + ": " + temp + " from " + string(client_addr) + " @ " + now;
    requestMessage.erase(requestMessage.find_first_of('\r'), 2);
    requestMessage.erase(requestMessage.find_last_of('\n'), 1);
    write_log(requestMessage);
    
	
    if (cache.find(string(header)) != cache.end()) {
      string message = to_string(id) + ": ";
      message += "in cache";
      write_log(message);
      for (auto i : cache[string(header)].response) {
        if (i.buf != NULL) {
          rio_writen(clientfd, i.buf, i.length);
        }
      }
      close(clientfd);
      return;
    }

    if (string(header).find("GET") != string::npos) {
      string message = to_string(id) + ": ";
      message += "not in cache";
      write_log(message);
    }
    
	  int tries = 10;
	  while (tries) {
		  if ((serverfd = open_clientfd(host, serverPort)) > 0) {
			  break;
      }
		  tries--;
	  }
	  if (serverfd <= 0) {
		  //printf("failed to establish connection.\n");
		  close(clientfd);
		  return;
	  }
	  if (request.size() != 0) {
		  rio_writen(serverfd, request.c_str(), request.size());
	  }
	  //receive the response
	  rio_readinitb(&server, serverfd);
	  while(1) {
		  piece temp;
      memset(temp.buf, 0, BUFFER_SIZE);
		  length = rio_readn(serverfd, temp.buf, BUFFER_SIZE);
		  if (length > 0) {
			  temp.length = length;
        response.push_back(temp);
        rio_writen(clientfd, temp.buf, temp.length);
		  }
		  else{
        break;
      }
	  }
    if (string(header).find("GET") != string::npos) {
      HTTP newRequest;
      newRequest.response = response;
      cache[string(header)] = newRequest;
    }
	  close(serverfd);
  }
  // CONNECT: call a different function, securetalk, for HTTPS
  if (strcmp(cmd, "CONNECT") == 0) {
	  httpsConnection(clientfd, client, host, serverPort);
  }
  
  close(clientfd);
}

void httpsConnection(int clientfd, rio_t client, char *inHost, int serverPort) {
  int serverfd;
  if (serverPort == proxyPort)
    serverPort = 443;
  //client:browser, server: real web server
  while (1) {
    if ((serverfd = open_clientfd(inHost, serverPort)) > 0) {
      break;
    }
  }
  //let the client know we've connected to the server 
  rio_writen(clientfd, "HTTP/1.1 200 Connection established\r\n\r\n", strlen("HTTP/1.1 200 Connection established\r\n\r\n"));
  
  // spawn a thread to pass bytes from origin server to client 
  thread t1(forwarder, clientfd, serverfd);
  thread t2(forwarder, serverfd, clientfd);
  // now pass bytes from client to server 
  //wait two thread to finish, prevent serverfd from being closed too early
  t1.join();
  t2.join();
  close(serverfd);
}

void forwarder(int clientfd, int serverfd) {
  char buf1[BUFFER_SIZE];
  memset(buf1, 0, BUFFER_SIZE);
  
  ssize_t length = 0;
  while(1) {
    if ((length = rio_readp(serverfd, buf1, BUFFER_SIZE)) > 0) {
      rio_writep(clientfd, buf1, length);
    }
    else {
      break;
    }
    memset(buf1,0,strlen(buf1));
  }
}

