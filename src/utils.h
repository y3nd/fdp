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

#define CLIENTNB 2

#define DEBUG 1

#define DOMAIN AF_INET

#define FILENAME_LEN 256
#define MSG_LENGTH 20
#define PORT_LENGTH 4
#define ACK_NO_LENGTH 6
#define FILE_CHUNK_SIZE 1466  // 1472 - 6 (ACK_NO_LENGTH)
#define SEGMENT_LENGTH 1472
#define BASE_WINDOW_SIZE 2
#define SLOWSTART_MULT 2  // multiplicate window size with this value each seg sent
#define SLOWSTART_DIV 2   // divide winow size with this value each timeout
#define BUFFER_SIZE 32    // equals to max window size

#if CLIENTNB == 1
#define TIMEOUT_BASE_US 50000
#endif

#if CLIENTNB == 2
#define TIMEOUT_BASE_US 20000
#endif

#define SYN "SYN"
#define SYN_ACK "SYN-ACK"
#define ACK "ACK"
#define FIN "FIN"

#if DEBUG
#define d_printf(...) print_ts(), printf(__VA_ARGS__);
#else
#define d_printf(...) ;
#endif

uint64_t get_ts() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;
}

void checkerr(long err, char *msg) {
  if (err < 0) {
    printf("%s err = %ld\n", msg, err);
    exit(1);
  }
}

void print_ts() {
  printf("(%ld) ", get_ts());
}

long send_str(int s, char *msg, struct sockaddr_in *addr_ptr) {
  d_printf("Sending \"%s\" to %s:%d\n", msg, inet_ntoa(addr_ptr->sin_addr), ntohs(addr_ptr->sin_port));
  ssize_t n = sendto(s, msg, strlen(msg) + 1, 0, (struct sockaddr *)addr_ptr, sizeof(struct sockaddr_in));
  checkerr(n, "send_str");
  return n;
}

long send_bytes(int s, char *buffer, size_t len, struct sockaddr_in *addr_ptr) {
  d_printf("Sending %ld bytes to %s:%d\n", len, inet_ntoa(addr_ptr->sin_addr), ntohs(addr_ptr->sin_port));
  ssize_t n = sendto(s, buffer, len, 0, (struct sockaddr *)addr_ptr, sizeof(struct sockaddr_in));
  checkerr(n, "send_bytes");
  return n;
}

long recv_str(int s, char *msg, struct sockaddr_in *addr_ptr) {
  socklen_t size = sizeof(struct sockaddr_in);
  ssize_t n = recvfrom(s, msg, MSG_LENGTH, 0, (struct sockaddr *)addr_ptr, &size);
  checkerr(n, "recv_str");
  return n;
}

long recv_bytes(int s, char *buffer, size_t len, struct sockaddr_in *addr_ptr) {
  socklen_t size = sizeof(struct sockaddr_in);
  ssize_t n = recvfrom(s, buffer, len, 0, (struct sockaddr *)addr_ptr, &size);
  checkerr(n, "recv_bytes");
  d_printf("Received %ld bytes from %s:%d\n", n, inet_ntoa(addr_ptr->sin_addr), ntohs(addr_ptr->sin_port));
  return n;
}

long recv_control_str(int s, char *control_str, struct sockaddr_in *addr_ptr) {
  d_printf("Waiting for \"%s\" on socket %d...\n", control_str, s);

  char msg[MSG_LENGTH];
  long n = recv_str(s, msg, addr_ptr);

  if (strncmp(msg, control_str, strlen(control_str)) != 0) {
    d_printf("Expected %s, got %s\n", control_str, msg);
    return 0;
  }

  return n;
}

int new_socket(struct sockaddr_in *addr_ptr, unsigned short port) {
  int sock = socket(DOMAIN, SOCK_DGRAM, 0);
  checkerr(sock, "socket");

  int reuse = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  memset((char *)addr_ptr, 0, sizeof(struct sockaddr_in));
  addr_ptr->sin_family = DOMAIN;
  addr_ptr->sin_port = htons(port);
  addr_ptr->sin_addr.s_addr = htonl(INADDR_ANY);

  int err = bind(sock, (struct sockaddr *)addr_ptr, sizeof(struct sockaddr_in));
  checkerr(err, "bind");

  d_printf("New UDP socket %d listening on port %d\n", sock, port);

  return sock;
}

#endif
