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
    char payload; /* Data payload (1 character for this project) */
} Packet;

/* Function to set socket timeout */
void set_socket_timeout(int sockfd, int sec, int usec) {
    struct timeval timeout;
    timeout.tv_sec = sec;
    timeout.tv_usec = usec;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in client_addr, server_addr;
    socklen_t addrlen = sizeof(client_addr);
    int port = atoi(argv[1]);
    printf("Server starting on port %d\n", port);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error opening socket");
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
        exit(EXIT_FAILURE);
    }
    printf("Socket bound successfully\n");

    Packet send_packet, recv_packet;
    int cur_seq = 0;
    int client_seq = 0;
    char send_char = 'A';

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
            send_packet.payload = 0;
            
            sendto(sockfd, &send_packet, sizeof(Packet), 0,
                  (struct sockaddr *)&client_addr, addrlen);
            printf("Sent SYN-ACK packet\n");
            break;
        }
    }

    // Wait for final ACK
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
            window_size = recv_packet.payload;
            client_seq = recv_packet.seq;
            printf("Received window size N=%d\n", window_size);
        } else {
            total_bytes = recv_packet.payload;
            client_seq = recv_packet.seq;
            printf("Received byte request S=%d\n", total_bytes);
            break;
        }
    }

    // Set timeout for Go-Back-N
    set_socket_timeout(sockfd, TIMEOUT_SEC, TIMEOUT_USEC);

    // Step 3: Implement Go-Back-N protocol
    int base = 1;
    int next_seq_num = 1;
    int current_window_size = window_size;
    int expected_ack = client_seq;
    int window_start = 1;

    printf("Starting data transmission...\n");
    while (base <= total_bytes) {
        // Send packets within window
        while (next_seq_num < base + current_window_size && next_seq_num <= total_bytes) {
            send_packet.seq = next_seq_num;
            send_packet.ack = expected_ack;
            send_packet.flag = ACK;
            send_packet.payload = send_char++;
            
            sendto(sockfd, &send_packet, sizeof(Packet), 0,
                  (struct sockaddr *)&client_addr, addrlen);
            printf("Sent data packet %d (ack=%d)\n", next_seq_num, expected_ack);
            next_seq_num++;
        }

        // Wait for ACK
        if (recvfrom(sockfd, &recv_packet, sizeof(Packet), 0,
                    (struct sockaddr *)&client_addr, &addrlen) < 0) {
            // Timeout occurred
            printf("Timeout, resending window from sequence %d\n", base);
            next_seq_num = base;
            send_char = 'A' + (base - 1);  // Reset character
            window_start = base;  // Mark where this window started
            
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
                
                // Only restore window if we've successfully sent all packets in the current window
                if (current_window_size < window_size && base > window_start + current_window_size) {
                    current_window_size = window_size;
                    printf("Restored window size to %d\n", current_window_size);
                    window_start = base;
                }
            } else {
                printf("Received duplicate ACK %d\n", recv_packet.ack);
            }
        }
    }

    // Send RST packet
    send_packet.seq = next_seq_num;
    send_packet.ack = expected_ack;
    send_packet.flag = RST;
    send_packet.payload = 0;
    sendto(sockfd, &send_packet, sizeof(Packet), 0,
           (struct sockaddr *)&client_addr, addrlen);
    printf("Sent RST packet, transmission complete\n");

    close(sockfd);
    return 0;
}