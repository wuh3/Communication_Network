/*
** client.c -- a stream socket client demo
*/

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

#define PORT "80"  // the port client will be connecting to

#define MAXDATASIZE 1000000  // max number of bytes we can get at once

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

//  	return ip, port number, and file path
//  	http://hostname[:port]/path/to/file

// e.g.  http://127.0.0.1/index.html
//		http://illinois.edu/index.html
//		http://12.34.56.78:8888/somefile.txt
//		http://localhost:5678/somedir/anotherfile.html

vector<string> cli_process(string &url) {
  string ip, port_num, file_path;
  url = url.substr(7);
  int url_len = url.length();
  for (int i = 0; i < url_len; i++) {
    if (url[i] == ':') {
      ip = url.substr(0, i);
      url = url.substr(i + 1);
      for (int j = 0; j < url.length(); j++) {
        if (url[j] == '/') {
          port_num = url.substr(0, j);
          file_path = url.substr(j);
        }
      }
    }
    if (url[i] == '/') {
      ip = url.substr(0, i);
      port_num = "80";
      file_path = url.substr(i);  // /somefile.txt
    }
  }
  vector<string> result;
  result.push_back(ip);
  result.push_back(port_num);
  result.push_back(file_path);
  return result;
}

//	HTTP/1.1 200 OK
//	HTTP/1.1 404 Not Found
//	HTTP/1.1 400 Bad Request

int server_headers_process(string header) {
  string response_code = "";
  int header_len = header.length();
  for (int i = 0; i < header_len; i++) {
    if (header[i] == ' ') {
      header = header.substr(i + 1);
      for (int j = 0; j < header.length(); j++) {
        if (header[j] == ' ') {
          response_code = header.substr(0, j);
        }
      }
    }
  }
  cout << response_code << endl;
  if (response_code != "200") {
    return 0;
  } else {
    return 1;
  }
}

int main(int argc, char *argv[]) {
  int sockfd, numbytes;
  char buf[MAXDATASIZE];
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];

  if (argc != 2) {
    fprintf(stderr, "usage: client hostname\n");
    exit(1);
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  // pre process input
  string url = std::string(argv[1]);
  vector<string> result = cli_process(url);
  // string get = "GET " + result[2] + " HTTP/1.1\r\n\r\n";
  string get = "GET " + result[2] + " HTTP/1.1\r\n" +
               "User-Agent: Wget/1.12 (linux-gnu)\r\n" + "Host: " + result[0] +
               ":" + result[1] + "\r\n" + "Connection: Keep-Alive\r\n\r\n";
  cout << get << endl;

  const char *getbuff = get.c_str();
  int get_len = strlen(getbuff);

  if ((rv = getaddrinfo(result[0].c_str(), result[1].c_str(), &hints,
                        &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and connect to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("client: socket");
      continue;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("client: connect");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    return 2;
  }

  inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s,
            sizeof s);
  printf("client: connecting to %s\n", s);

  freeaddrinfo(servinfo);  // all done with this structure

  // send HTTP GET
  send(sockfd, getbuff, get_len, 0);
  cout << "Send HTTP GET" << endl;

  string received = "";
  int count = 0, line = 0;
  char rec_buf;

  cout << "Output File" << endl;
  // output file
  FILE *fp;
  fp = fopen("output", "wb");
  memset(buf, '\0', MAXDATASIZE);
  numbytes = recv(sockfd, buf, MAXDATASIZE, 0);

  if (numbytes <= 0) {
    fclose(fp);
  } else {
    char *strStart = strstr(buf, "\r\n\r\n");
    strStart += 4;
    fwrite(strStart, sizeof(char), strlen(strStart), fp);
  }

  while (1) {
    memset(buf, '\0', MAXDATASIZE);
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) > 0) {
      fprintf(fp, "%s", buf);
    } else {
      fclose(fp);
      break;
    }
  }

  close(sockfd);

  return 0;
}
