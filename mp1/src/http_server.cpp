/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include <string>
#include <fstream>
#include <vector>
#include <iostream>

#define BACKLOG 10	 // how many pending connections queue will hold
#define MAXDATASIZE 10000 // max datasize server can get

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//return type, host, path and HTTP version
std::string getRequest(std::string& url) {
    // Method to parse the incoming URL request and convert to file path
    std::string path = "";
	for (int i = 0; i < url.length(); i++){
		if (url[i] == '/'){
			url = url.substr(i + 1);
            path = url.erase(url.find_last_not_of(" \n\r\t") + 1); // Trim the filePath
			break;
		}
	}
	int idx = path.find_first_of(" ");
	path = path.substr(0, idx);
	return path;
}

int main(int argc, char *argv[])
{
	int sockfd, newFd;  // listen on sock_fd, new connection on newFd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	if (argc != 2) {
	    fprintf(stderr,"usage: server port number\n");
	    exit(1);
	}
	std::string PORT = std::string(argv[1]);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT.c_str(), &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		newFd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (newFd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener
			int receiveLen;
			char buf[MAXDATASIZE];
			std::string getReq = "", path = "", getType = "";
			while (true) {
				receiveLen = recv(newFd, buf, MAXDATASIZE, 0);
				if (receiveLen <= 0){
                    // Error handling
                    break;
                }

				getReq = std::string(buf);
				if ((getType = getReq.substr(0,3)) != "GET"){
                    std::string head = "HTTP/1.1 400 Bad getReq\r\n\r\n";
					if (send(newFd, head.c_str(), head.length(), 0) == -1)
						perror("send 400");
                }
				path = getRequest(getReq);
				std::cout<<"Filepath: "<<path<<std::endl;

                char buff[MAXDATASIZE];
                FILE *fp = fopen(path.c_str(), "rb");
                if (!fp) {
                    // If file not found
                    std::string head = "HTTP/1.1 404 Not Found\r\n\r\n";
                    if (send(newFd, head.c_str(), head.length(), 0) == -1) perror("404 Not Found");
                } else {
                    std::string head = "HTTP/1.1 200 OK\r\n\r\n";
                    if (send(newFd, head.c_str(), head.length(), 0) == -1) perror("200 Error");

                    while (!feof(fp)) {
                        int callBack = fread(buff, sizeof(char), MAXDATASIZE, fp);
                        if (callBack == MAXDATASIZE){ 
                            printf("Reading\n");
                            if (send(newFd, buff, MAXDATASIZE, 0) == -1) perror("send");
                        } else if (callBack <= 0) {
                            printf("Closed Reading\n");
                            break;
                        } else {
                            printf("Reading\n");
                            if (send(newFd, buff, (int)callBack, 0) == -1) perror("send");
                        }
                    }
                    fclose(fp);
                }

                
				break;
			}

			close(newFd);
			exit(0);
		}
		close(newFd);  // parent doesn't need this
	}

	return 0;
}
