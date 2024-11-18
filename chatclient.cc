#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h> 
#include <bits/stdc++.h>


int main(int argc, char *argv[]) {

  if (argc != 3) {
    fprintf(stderr, "argc = %d *** Author: Your name here (SEASlogin here)\n",argc);
    exit(1);
  }
 
  int sock = socket(PF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    fprintf(stderr, "Cannot open socket (%s)\n", strerror(errno));
    exit(1);
  }  
  /*Connect to the provided server ip and port*/
  struct sockaddr_in dest;
  bzero(&dest, sizeof(dest));
  dest.sin_family = AF_INET;
  dest.sin_port = htons(atoi(argv[2]));/*UDP port*/
  inet_pton(AF_INET, argv[1], &(dest.sin_addr));
  connect(sock,(struct sockaddr*)&dest, sizeof(dest));
  // sendto(sock, , strlen(argv[2]), 0, (struct sockaddr*)&dest, sizeof(dest));  

  /*Define set for select*/
  while(true){
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    FD_SET(STDIN_FILENO, &readfds);
    int maxFD = sock;
    int ret = select(maxFD+1, &readfds, nullptr, NULL, NULL);
    if (FD_ISSET(sock, &readfds)) {
      char buf[1024];
      // handleIncomingConnection();
      std::cout<<"Handling incoming connection"<<std::endl;
      struct sockaddr_in src;
      socklen_t srcSize = sizeof(src);
      int rlen = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&src, &srcSize);
      buf[rlen] = 0;
      printf("Echo: [%s] (%d bytes) from %s\n", buf, rlen, inet_ntoa(src.sin_addr));  
    }
    if (FD_ISSET(STDIN_FILENO, &readfds)) {
        // handleRead(i);
        std::string line;
        while (true) {
          char buf[1024];
          int len = read(STDIN_FILENO, buf, sizeof(buf));
          if (len <= 0) {
            // Either an error occurred or the user closed the standard input
            break;
          }
          line.append(buf, len);
          if(line.find("\n") != std::string::npos){
            sendto(sock, line.c_str(), line.size(), 0, (struct sockaddr*)&dest, sizeof(dest));
            printf("Sending: [%s]\n", line.c_str());  
            line.clear();
            break;
          }
        }
        
        // Now 'line' contains all the data from the standard input
    }
  }
  close(sock);
  return 0;

}
