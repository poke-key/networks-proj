#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#define MAX_PKT_NUM 255
#define TIMEOUT_SEC 1
#define TIMEOUT_USEC 0
#define MAX_PAYLOAD 1024

/* Define control flags */
#define SYN 1
#define SYN_ACK 2
#define ACK 3
#define RST 4

/* Packet data structure */
typedef struct {
    int seq;      /* Sequence number */
    int ack;      /* Acknowledgement number */
    int flag;     /* Control flag. Indicate type of packet */
    char payload[MAX_PAYLOAD]; /* Data payload for file content */
    size_t payload_size;      /* Actual size of payload */
} Packet;

/* 
Function to set socket timeout 
used instead of the select() function because it has the same goal, this one let's recvfrom() 
automatically timeout after specificed period

select() would let us wait for socket events and timeout after a certain period
*/
void set_socket_timeout(int sockfd, int sec, int usec) {
    struct timeval timeout;
    timeout.tv_sec = sec;
    timeout.tv_usec = usec;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <server_port> <file_to_send>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    //open the file to send
    FILE *file = fopen(argv[2], "rb");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    //get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    printf("File size: %ld bytes\n", file_size);

    int sockfd;
    struct sockaddr_in client_addr, server_addr;
    socklen_t addrlen = sizeof(client_addr);
    int port = atoi(argv[1]);
    printf("Server starting on port %d\n", port);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error opening socket");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    printf("Socket created successfully\n");

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons((unsigned short)port);

    if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("ERROR on binding");
        close(sockfd);
        fclose(file);
        exit(EXIT_FAILURE);
    }
    printf("Socket bound successfully\n");

    Packet send_packet, recv_packet;
    int cur_seq = 0;
    int client_seq = 0;

    // Step 1: Three-way handshake
    printf("Waiting for SYN packet...\n");
    while (1) {
        if (recvfrom(sockfd, &recv_packet, sizeof(Packet), 0,
                    (struct sockaddr *)&client_addr, &addrlen) < 0) {
            continue;
        }
        
        if (recv_packet.flag == SYN && recv_packet.ack == 0) {
            printf("Received SYN packet\n");
            client_seq = recv_packet.seq;
            
            send_packet.seq = cur_seq;
            send_packet.ack = client_seq;
            send_packet.flag = SYN_ACK;
            send_packet.payload_size = 0;
            
            sendto(sockfd, &send_packet, sizeof(Packet), 0,
                  (struct sockaddr *)&client_addr, addrlen);
            printf("Sent SYN-ACK packet\n");
            break;
        }
    }

    //wait for final ACK
    printf("Waiting for ACK...\n");
    while (1) {
        if (recvfrom(sockfd, &recv_packet, sizeof(Packet), 0,
                    (struct sockaddr *)&client_addr, &addrlen) < 0) {
            continue;
        }

        if (recv_packet.flag == ACK) {
            printf("Handshake complete, received final ACK\n");
            client_seq = recv_packet.seq;
            break;
        }
    }

    // Step 2: Receive window size and byte count
    int window_size = 0, total_bytes = 0;
    printf("Waiting for window size and byte count...\n");
    
    while (1) {
        if (recvfrom(sockfd, &recv_packet, sizeof(Packet), 0,
                    (struct sockaddr *)&client_addr, &addrlen) < 0) {
            continue;
        }
        
        if (window_size == 0) {
            window_size = atoi(recv_packet.payload);
            client_seq = recv_packet.seq;
            printf("Received window size N=%d\n", window_size);
        } else {
            total_bytes = atoi(recv_packet.payload);
            client_seq = recv_packet.seq;
            printf("Received byte request S=%d\n", total_bytes);
            break;
        }
    }

    //call the function to set timeout for Go-Back-N
    set_socket_timeout(sockfd, TIMEOUT_SEC, TIMEOUT_USEC);

    // Step 3: Implement Go-Back-N protocol
    int base = 1;
    int next_seq_num = 1;
    int current_window_size = window_size;
    int expected_ack = client_seq;
    int window_start = 1;
    int successful_windows = 0;
    int window_end = window_start + current_window_size - 1;
    int loss_in_window = 0;

    printf("Starting file transmission...\n");
    while (base <= total_bytes) {
        //send packets within window
        while (next_seq_num < base + current_window_size && next_seq_num <= total_bytes) {
            //read chunk from file
            size_t bytes_read = fread(send_packet.payload, 1, MAX_PAYLOAD, file);
            send_packet.payload_size = bytes_read;
            send_packet.seq = next_seq_num;
            send_packet.ack = expected_ack;
            send_packet.flag = ACK;
            
            sendto(sockfd, &send_packet, sizeof(Packet), 0,
                  (struct sockaddr *)&client_addr, addrlen);
            printf("Sent data packet %d (ack=%d, size=%zu)\n", next_seq_num, expected_ack, bytes_read);
            next_seq_num++;
        }

        // Wait for ACK
        if (recvfrom(sockfd, &recv_packet, sizeof(Packet), 0,
                    (struct sockaddr *)&client_addr, &addrlen) < 0) {
            //timeout has occured
            printf("Timeout, resending window from sequence %d\n", base);
            fseek(file, (base - 1) * MAX_PAYLOAD, SEEK_SET);  //reset file position
            next_seq_num = base;
            window_start = base;
            window_end = window_start + current_window_size - 1;
            successful_windows = 0;
            loss_in_window = 1;
            
            if (current_window_size == window_size) {
                current_window_size = window_size / 2;
                if (current_window_size < 1) current_window_size = 1;
                printf("Reduced window size to %d\n", current_window_size);
            }
            continue;
        }

        // Process ACK
        if (recv_packet.flag == ACK) {
            expected_ack = recv_packet.seq;
            if (recv_packet.ack >= base) {
                printf("Received valid ACK for packet %d\n", recv_packet.ack);
                base = recv_packet.ack + 1;
                fseek(file, (base - 1) * MAX_PAYLOAD, SEEK_SET);  //update file position

                if (base > window_end) {
                    if (!loss_in_window) {
                        successful_windows++;
                        printf("Completed window successfully (%d/2)\n", successful_windows);
                    }
                    
                    if (successful_windows >= 2 && current_window_size < window_size) {
                        current_window_size = window_size;
                        printf("Restored window size to %d after two successful windows\n", current_window_size);
                        successful_windows = 0;
                    }
                    
                    window_start = base;
                    window_end = window_start + current_window_size - 1;
                    loss_in_window = 0;
                }
            } else {
                printf("Received duplicate ACK %d\n", recv_packet.ack);
                loss_in_window = 1;
                successful_windows = 0;
            }
        }
    }

    //send the RST packet
    send_packet.seq = next_seq_num;
    send_packet.ack = expected_ack;
    send_packet.flag = RST;
    send_packet.payload_size = 0;
    sendto(sockfd, &send_packet, sizeof(Packet), 0,
           (struct sockaddr *)&client_addr, addrlen);
    printf("Sent RST packet, transmission complete\n");

    fclose(file); //close file
    close(sockfd); //close socket
    return 0;
}