//
//  main.c
//  Server1
//
//  Created by Jingnong Wang on 2017/2/8.
//  Copyright © 2017年 Jingnong Wang. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT "8888"    // the port users will be connecting to
#define MAXBUFFER 300

/* Error packet */
struct error_packet {
    unsigned short int start_of_packet_id;
    unsigned short int client_id:8;
    unsigned short int packet_type;
    unsigned short int reject_sub_code;
    unsigned short int segment_no:8;
    unsigned short int end_of_packet_id;
} error_packet = {0xFFFF, 0x0, 0xFFF3, 0x0, 0x0, 0xFFFF};


/* The front and end part of the recieved packet */
struct packet_front {
    unsigned short int start_of_packet_id;
    unsigned short int client_id:8;
    unsigned short int packet_type;
    unsigned short int segment_no:8;
    unsigned short int length:8;
} packet_front = {0x0, 0x0, 0x0, 0x0, 0x00};

struct packet_end {
    unsigned short int end_of_packet_id;
} packet_end = {0x0};


char payload[256];
char recieved[MAXBUFFER];

struct addrinfo hints, *servinfo, *p;
struct sockaddr_storage their_addr;
socklen_t addr_len;
char s[INET6_ADDRSTRLEN];
int prev = -1;

 /* Setup a UDP socket server */
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void recieving(int);

int main() {
    int sockServer;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    struct sockaddr_storage their_addr;
    
     /* Setup a UDP socket server */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockServer = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket error");
            continue;
        }
        if (bind(sockServer, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockServer);
            perror("bind faied");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "bind faied");
        return 2;
    }
    freeaddrinfo(servinfo);
    printf("Listening to data...\n");
    addr_len = sizeof their_addr;
    
     /* Recieving 5 packets from client*/
    for (int i = 0; i < 5; i++) {
        recieving(sockServer);
    }
    close(sockServer);
    return 0;
}

void recieving(int sockfd){
    int numbytes;
    bzero(recieved, MAXBUFFER);
    bzero(payload, 256);
    memset(&packet_front, 0, sizeof(packet_front));
    
    /* Recieve from client */
    numbytes = recvfrom(sockfd, recieved, MAXBUFFER-1 , 0,(struct sockaddr *)&their_addr, &addr_len);
    if (numbytes == -1) {
        perror("recieve error\n");
        exit(1);
    }
    
    /* Get each part of recieved packet */
    memcpy(&packet_front, recieved, sizeof(packet_front));
    memcpy(payload, recieved + sizeof(packet_front), numbytes - sizeof(packet_front) - 2); payload[strlen(payload)] = '\0';
    memcpy(&packet_end, recieved + numbytes - 2, 2);
    
    printf("\nRecieved packet No. %d \n", packet_front.segment_no);
    printf("start_of_packet_id: \"%#X\"\n", packet_front.start_of_packet_id);
    printf("client_id: \"%#X\"\n", packet_front.client_id);
    printf("packet_type: \"%#X\"\n", packet_front.packet_type);
    printf("segment_no: \"%d\"\n", packet_front.segment_no);
    printf("length: \"%hu\"\n", packet_front.length);
    printf("real length of payload \"%lu\"\n", strlen(payload));
    printf("payload: \"%s\"\n", payload);
    printf("end_of_packet_id:  \"%#X\"\n", packet_end.end_of_packet_id);
    
    bzero(recieved, MAXBUFFER);
    
    /* Setup error packet */
    if (packet_front.length != strlen(payload)) {
        printf("Packet rejected. Error: Length mismatch.\n");
        error_packet.reject_sub_code = 0xFFF5;
        error_packet.client_id = packet_front.client_id;
        error_packet.segment_no = packet_front.segment_no;
        memcpy(recieved, &error_packet, sizeof(error_packet));
    }
    else if (packet_end.end_of_packet_id != 0xFFFF) {
        printf("Packet rejected. Error: End of packet missing.\n");
        error_packet.reject_sub_code = 0xFFF6;
        error_packet.client_id = packet_front.client_id;
        error_packet.segment_no = packet_front.segment_no;
        memcpy(recieved, &error_packet, sizeof(error_packet));
    }
    else if (prev == packet_front.segment_no) {
        printf("Packet rejected. Error: Duplicate packets.\n");
        error_packet.reject_sub_code = 0xFFF7;
        error_packet.client_id = packet_front.client_id;
        error_packet.segment_no = packet_front.segment_no;
        memcpy(recieved, &error_packet, sizeof(error_packet));
    }
    else if (prev + 1 != packet_front.segment_no) {
        printf("Packet rejected. Error: Out of sequence.\n");
        error_packet.reject_sub_code = 0xFFF4;
        error_packet.client_id = packet_front.client_id;
        error_packet.segment_no = packet_front.segment_no;
        memcpy(recieved, &error_packet, sizeof(error_packet));
    }
    else {
        packet_front.packet_type = 0xFFF2;
        memcpy(recieved, &packet_front, sizeof(packet_front) - 1);
        packet_end.end_of_packet_id = 0xFFFF;
        memcpy(recieved + sizeof(packet_front), &packet_end, 2);
    }
    prev = packet_front.segment_no;
    
    /* Send error packet */
    numbytes = sendto(sockfd, recieved, sizeof(packet_front)+1, 0, (struct sockaddr *)&their_addr, addr_len);
    if (numbytes == -1) {
        perror("send error\n");
        exit(1);
    }
    
}
