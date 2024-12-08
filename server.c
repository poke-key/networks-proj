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

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error opening socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket created successfully\n");

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons((unsigned short)port);

    // Bind socket
    if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("ERROR on binding");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("Socket bound successfully\n");

    Packet send_packet, recv_packet;
    int cur_seq = 0;
    printf("Waiting for SYN packet...\n");

    // Step 1: Three-way handshake - Wait for SYN
    while (1) {
        ssize_t recv_len = recvfrom(sockfd, &recv_packet, sizeof(Packet), 0,
                    (struct sockaddr *)&client_addr, &addrlen);
        
        if (recv_len < 0) {
            perror("Error receiving packet");
            continue;
        }
        
        printf("Received packet with flag: %d\n", recv_packet.flag);
        
        if (recv_packet.flag == SYN && recv_packet.ack == 0) {
            printf("Received SYN packet\n");
            
            // Send SYN-ACK
            send_packet.seq = cur_seq;
            send_packet.ack = recv_packet.seq;
            send_packet.flag = SYN_ACK;
            send_packet.payload = 0;
            
            ssize_t sent_len = sendto(sockfd, &send_packet, sizeof(Packet), 0,
                  (struct sockaddr *)&client_addr, addrlen);
            
            if (sent_len < 0) {
                perror("Error sending SYN-ACK");
                continue;
            }
            printf("Sent SYN-ACK packet\n");
            break;
        } else {
            printf("Unexpected packet flag: %d\n", recv_packet.flag);
        }
    }

    // Rest of the implementation...
    // (For now, we'll just keep the server running)
    printf("Server running... Press Ctrl+C to stop\n");
    while(1) {
        sleep(1);
    }

    close(sockfd);
    return 0;
}