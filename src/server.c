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

  printf("opening %s\n", filename);
  // read file
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    printf("Error opening file %s\n", filename);
    exit(1);
  }

  fd_set read_set;
  FD_ZERO(&read_set);

  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  struct timeval polling;
  polling.tv_sec = 0;
  polling.tv_usec = 0;

  uint64_t start_ts = get_ts();

  unsigned int actual_last_seg_no = 0;
  unsigned int actual_last_seg_length = 0;
  unsigned int last_generated_seg_no = 0;
  unsigned int last_received_ack_no = 0;
  unsigned int last_sent_seg_no = 1;
  unsigned int window_size = BASE_WINDOW_SIZE;
  char buffer[BUFFER_SIZE][SEGMENT_LENGTH];
  unsigned long total_bytes_read = 0;
  unsigned int fin_sent_count = 0;

  //printf("f\n");

  size_t bytes_read;
  while (1) {
    unsigned int credit = last_sent_seg_no - last_received_ack_no;  // nb of unacknowledged segments

    char *seg = buffer[(last_sent_seg_no - 1) % BUFFER_SIZE];

    // segment generation
    if (last_sent_seg_no > last_generated_seg_no) {
      d_printf("need to generate new segment %d\n", last_sent_seg_no);

      // ACK_NO_LENGTH + 1 to include null terminator
      snprintf(seg, ACK_NO_LENGTH + 1, "%06d", last_sent_seg_no);
      d_printf("writing seg in buffer i=%d\n", last_sent_seg_no - 1 % BUFFER_SIZE);
      bytes_read = fread(&seg[ACK_NO_LENGTH], 1, FILE_CHUNK_SIZE, fp);  // overwrites null terminator
      total_bytes_read += bytes_read;

      if (feof(fp)) {
        // if (bytes_read != FILE_CHUNK_SIZE) {
        actual_last_seg_no = last_sent_seg_no;
        actual_last_seg_length = bytes_read;  // might be different from FILE_CHUNK_SIZE
      }

      last_generated_seg_no = last_sent_seg_no;
    }

    // send data and save in seg_buffer
    if (bytes_read > 0) {
      size_t seg_length = ACK_NO_LENGTH + (actual_last_seg_no != last_sent_seg_no ? FILE_CHUNK_SIZE : actual_last_seg_length);
      d_printf("sending segment %d (%ld bytes)\n", last_sent_seg_no, seg_length);
      send_bytes(c_sock, seg, seg_length, c_addr_ptr);
    }

    // crash check
    if (credit > window_size) {
      d_printf("Error: credit = %d = %d - %d > window_size\n", credit, last_sent_seg_no, last_received_ack_no);
      exit(1);
    }
    d_printf("credit = %d\n", credit);

    // poll for ACK, or if window is full, wait for ACK until timeout
    struct timeval *time_ptr = credit == window_size || last_sent_seg_no == actual_last_seg_no ? &timeout : &polling;
    if (time_ptr == &timeout) {
      d_printf("waiting for ack because window is full..\n");
    }

  select_p:
    FD_SET(c_sock, &read_set);
    select(c_sock + 1, &read_set, NULL, NULL, time_ptr);
    unsigned short received_ack = 0;
    if (FD_ISSET(c_sock, &read_set)) {
      // expect ACK
      char msg[3 + ACK_NO_LENGTH];
      recv_str(c_sock, msg, c_addr_ptr);
      d_printf("recv: %s\n", msg);

      if (strncmp(msg, ACK, 3) == 0) {
        unsigned int ack_no = atoi(&msg[3]);
        if (ack_no == actual_last_seg_no) {
          print_ts();
          printf("sending FIN\n");
          send_str(c_sock, FIN, c_addr_ptr);
          fin_sent_count++;
          if(fin_sent_count == 5) break;
          goto select_p;
        }

        if (ack_no > last_received_ack_no) {
          d_printf("RECEIVED ACK %d\n", ack_no);
          last_received_ack_no = ack_no;
          if (last_received_ack_no > last_sent_seg_no) {
            last_sent_seg_no = last_received_ack_no;
          }
          received_ack = 1;  // cancel timeout trigger

          // slowstart
          int new_window_size = window_size*SLOWSTART_MULT;
          if(new_window_size <= BUFFER_SIZE) {
            window_size = new_window_size;
          } else {
            window_size = BUFFER_SIZE;
          }
        } else {
          goto select_p;
        }
      }
    }

    if (time_ptr == &timeout && !received_ack) {
      // timeout : no ACK received for window, resend since last_ack_received
      // edit credit
      last_sent_seg_no = last_received_ack_no;  // will be incremented
      d_printf("Timeout ! resending from seg %d new credit = %d\n", last_sent_seg_no + 1, last_sent_seg_no - last_received_ack_no);
      // slowstart
      int new_window_size = window_size/SLOWSTART_DIV;
      if(new_window_size >= 2) {
        window_size = new_window_size;
      } else {
        window_size = 2;
      }
    }

    // it should not be incremented if it's the last segment
    if (last_sent_seg_no != actual_last_seg_no) {
      last_sent_seg_no++;
    }
  }

  fclose(fp);

  uint64_t total_time_us = get_ts() - start_ts;
  // printf("%ld %ld", get_ts(), start_ts);
  uint64_t total_time_ms = total_time_us / 1000;
  printf("data sent: %ld bytes | time: %ld ms\n", total_bytes_read, total_time_ms);
  printf("speed %ld kbits / sec \n", (total_bytes_read / total_time_ms)*8);
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
