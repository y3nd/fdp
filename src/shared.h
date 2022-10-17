#ifndef SHARED_H
#define SHARED_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define DOMAIN AF_INET

#define MSG_LENGTH 64
#define PORT_LENGTH 5
#define ACK_NO_LENGTH 5
#define FILE_CHUNK_SIZE 1024
#define BASE_WINDOW_SIZE 5

#define SYN "SYN"
#define SYN_ACK "SYN-ACK"
#define ACK "ACK"
#define ACK_NO "ACK_"
#define FIN "FIN"

struct segment {
  unsigned short no;
  size_t size;
  unsigned int window_size;
  char data[FILE_CHUNK_SIZE];
};

void checkerr(long err, char *msg) {
  if (err < 0) {
    printf("%s err = %ld\n", msg, err);
    exit(1);
  }
}

void printPID() {
  printf("(%ld) [%d] ", time(0), getpid());
}

long send_str(int s, char *msg, struct sockaddr_in *addr_ptr) {
  printPID();
  printf("Sending \"%s\" to %s:%d\n", msg, inet_ntoa(addr_ptr->sin_addr), ntohs(addr_ptr->sin_port));
  ssize_t n = sendto(s, msg, strlen(msg) + 1, 0, (struct sockaddr *)addr_ptr, sizeof(struct sockaddr_in));
  checkerr(n, "send_str");
  return n;
}

long send_bytes(int s, char *buffer, size_t len, struct sockaddr_in *addr_ptr) {
  printPID();
  printf("Sending %ld bytes to %s:%d\n", len, inet_ntoa(addr_ptr->sin_addr), ntohs(addr_ptr->sin_port));
  ssize_t n = sendto(s, buffer, len, 0, (struct sockaddr *)addr_ptr, sizeof(struct sockaddr_in));
  checkerr(n, "send_bytes");
  return n;
}

long recv_str(int s, char *msg, struct sockaddr_in *addr_ptr) {
  socklen_t size = sizeof(struct sockaddr_in);
  ssize_t n = recvfrom(s, msg, MSG_LENGTH, 0, (struct sockaddr *)addr_ptr, &size);
  checkerr(n, "recv_str");
  printPID();
  printf("Received \"%s\" from %s:%d\n", msg, inet_ntoa(addr_ptr->sin_addr), ntohs(addr_ptr->sin_port));
  return n;
}

long recv_bytes(int s, char *buffer, size_t len, struct sockaddr_in *addr_ptr) {
  socklen_t size = sizeof(struct sockaddr_in);
  ssize_t n = recvfrom(s, buffer, len, 0, (struct sockaddr *)addr_ptr, &size);
  checkerr(n, "recv_bytes");
  printPID();
  printf("Received %ld bytes from %s:%d (seg %d)\n", n, inet_ntoa(addr_ptr->sin_addr), ntohs(addr_ptr->sin_port), ((struct segment *)buffer)->no);
  return n;
}

long recv_control_str(int s, char *control_str, struct sockaddr_in *addr_ptr) {
  printPID();
  printf("Waiting for \"%s\" on socket %d...\n", control_str, s);

  char msg[MSG_LENGTH];
  long n = recv_str(s, msg, addr_ptr);

  if (strncmp(msg, control_str, strlen(control_str)) != 0) {
    printPID();
    printf("Expected %s, got %s\n", control_str, msg);
    return 0;
  }

  return n;
}

#endif
