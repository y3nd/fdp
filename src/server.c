#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"

void handle_client(int c_sock, struct sockaddr_in *c_addr_ptr) {
  char filename[FILENAME_LEN];
  recv_str(c_sock, filename, c_addr_ptr);

  // read file
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    printf("Error opening file %s\n", filename);
    exit(1);
  }

  fd_set read_set;
  FD_ZERO(&read_set);

  struct timeval timeout;
  timeout.tv_sec = 30;
  timeout.tv_usec = 0;

  struct timeval polling;
  polling.tv_sec = 0;
  polling.tv_usec = 0;

  long actual_last_seg_no = -1;
  long last_received_ack_no = -1;
  unsigned int last_sent_seg_no = 0;
  unsigned int window_size = BASE_WINDOW_SIZE;
  char window[window_size][SEGMENT_LENGTH];

  size_t bytes_read;
  while (1) {
    unsigned int credit = last_sent_seg_no - last_received_ack_no;  // nb of unacknowledged segments

    char *seg = window[last_sent_seg_no % window_size];

    snprintf(seg, ACK_NO_LENGTH + 1, "%06d", last_sent_seg_no);  // ACK_NO_LENGTH + 1 to include null terminator

    bytes_read = fread(&seg[ACK_NO_LENGTH], 1, FILE_CHUNK_SIZE, fp);  // overwrites null terminator
    if (bytes_read == 0) {
      actual_last_seg_no = last_sent_seg_no - 1;
    }

    // send data and save in seg_buffer
    if (bytes_read > 0) {
      printf("sending segment %d (%ld bytes)\n", last_sent_seg_no, ACK_NO_LENGTH + bytes_read);
      send_bytes(c_sock, seg, ACK_NO_LENGTH + bytes_read, c_addr_ptr);
    }

    // poll for ACK, or if window is full, wait for ACK until timeout
    if (credit > window_size) {
      printf("Error: credit = %d = %d - %ld > window_size\n", credit, last_sent_seg_no, last_received_ack_no);
      exit(1);
    }
    printf("credit = %d\n", credit);
    struct timeval *time_ptr = credit == window_size ? &timeout : &polling;
    //printf("select using timeout ? %d\n", time_ptr == &timeout);
    if(time_ptr == &timeout) printf("waiting for ack because window is full..\n");
    FD_SET(c_sock, &read_set);
    select(c_sock + 1, &read_set, NULL, NULL, time_ptr);
    int received_ack = 0;
    if (FD_ISSET(c_sock, &read_set)) {
      //printf("expect ACK\n");
      // expect ACK
      char msg[3 + ACK_NO_LENGTH];
      size_t n = recv_str(c_sock, msg, c_addr_ptr);
      //printf("recv: %s\n", msg);

      if (strncmp(msg, ACK, 3) == 0) {
        unsigned int ack = atoi(&msg[3]);
        if (ack == actual_last_seg_no) {  // or ==
          printf("end of file reached\n");
          break;
        }

        //printf("ack > last_received_ack_no => %d > %ld\n", ack, last_received_ack_no);
        if (ack > last_received_ack_no) {
          print_ts();
          printf("RECEIVED ACK %d\n", ack);
          last_received_ack_no = ack;
          received_ack = 1; // cancel timeout trigger
        }
      } else {
        print_ts();
        printf("Expected ACK, got %s\n", msg);
      }
    }
    
    if (time_ptr == &timeout && !received_ack) {
      // timeout : no ACK received for window, resend since last_ack_received
      printf("Timeout\n");
      // edit credit
      last_sent_seg_no = last_received_ack_no;
      printf("resending from seg %d new credit = %ld\n", last_sent_seg_no+1, last_sent_seg_no - last_received_ack_no);
    } /* else {
       // no ACK received, but window is not full
     }*/

    // it should not be incremented if it's the last segment
    if (bytes_read == FILE_CHUNK_SIZE) {
      last_sent_seg_no++;
    }
  }

  fclose(fp);

  send_str(c_sock, FIN, c_addr_ptr);
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <udp port>\n", argv[0]);
    exit(1);
  }
  unsigned short server_port = atoi(argv[1]);
  struct sockaddr_in server_addr;
  int server_socket = new_socket(&server_addr, server_port);

  unsigned short client_no = 1;

  while (1) {
    struct sockaddr_in c_addr;

    if (recv_control_str(server_socket, SYN, &c_addr)) {
      // SYN-ACK <port>
      char syn_ack[sizeof(SYN_ACK) + PORT_LENGTH + 1];
      short c_port = server_port + client_no++;
      sprintf(syn_ack, "%s%d", SYN_ACK, c_port);

      // create new socket for client
      struct sockaddr_in c_server_addr;
      int c_sock = new_socket(&c_server_addr, c_port);

      // send SYN-ACK
      do {
        send_str(server_socket, syn_ack, &c_addr);
      } while (!recv_control_str(server_socket, ACK, &c_addr));

      if (fork() == 0) {
        close(server_socket);
        handle_client(c_sock, &c_addr);
        close(c_sock);
        exit(0);
      } else {
        close(c_sock);
      }
    }
  }

  return 0;
}
