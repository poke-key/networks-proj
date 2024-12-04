#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_PKT_NUM 255

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
    //fprintf(stdout, "Port: %d\n", port);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        printf("Error opening socket");
        exit(EXIT_FAILURE);
    }
    /*
    * build the server's Internet address
    */
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons((unsigned short)port);
    /* 
    * bind: associate the parent socket with a port 
    */
    if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        printf("ERROR on binding");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    memset((char *)&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    client_addr.sin_port = htons(port);
    Packet send_packet, recv_packet;

    while (1) {
        recvfrom(sockfd, &recv_packet, sizeof(Packet), 0,
                (struct sockaddr *)&client_addr, &addrlen);
        if (recv_packet.flag != SYN) {
            printf("Expected SYN packet! Received %d instead, ignoring\n", recv_packet.flag);
        } else {
        if (recv_packet.ack != 0) {
            printf("Received incorrect ACK, ignoring.\n");
        } else {
            printf("Received SYN-ACK packet\n");
            break;
        }
        }
    }    
    close(sockfd);
    
}