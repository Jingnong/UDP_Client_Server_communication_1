//
//  main.c
//  Client1
//
//  Created by Jingnong Wang on 2017/2/8.
//  Copyright © 2017年 Jingnong Wang. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <sys/time.h>

#define PORT "8888"
#define SERVER "127.0.0.1"
#define BUFFER_SIZE 300

/* The front and end part of the packet */
struct packet_front {
    unsigned short int start_of_packet_id;
    unsigned short int client_id:8;
    unsigned short int packet_type;
    unsigned short int segment_no:8;
    unsigned short int length:8;
} packet_front = {0xFFFF, 0x01, 0xFFF1, 0x0, 0xFF};

struct packet_end {
    unsigned short int end_of_packet_id;
} packet_end = {0xFFFF};

/* Recieved error packet */
struct packet_recieved {
    unsigned short int start_of_packet_id;
    unsigned short int client_id:8;
    unsigned short int packet_type;
    unsigned short int reject_sub_code;
    unsigned short int segment_no:8;
    unsigned short int end_of_packet_id;
} packet_recieved = {0xFFFF, 0x0, 0xFFF3, 0x0, 0x0, 0xFFFF};

char sending_buffer[BUFFER_SIZE];
char recieving_buffer[BUFFER_SIZE];
char payload[256];

struct addrinfo hints, *servinfo, *p;

void sendPacket(int, int);

int main() {
    int socketClient;
    int rv;
    int send_counter;
    int packet_no;
    send_counter = 0;

    /* Creat a UDP socket client */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    
    if ((rv = getaddrinfo(SERVER, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((socketClient = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("socket error");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "failed to create socket\n");
        return 2;
    }
    
    /* Set ack timer */
    struct timeval timeout;
    timeout.tv_sec  = 3;
    timeout.tv_usec = 0;
    setsockopt(socketClient, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    
    /* Set payload array */
    char array[5][256] = {{"payload0"},
                          {"payload1"},
                          {"payload2"},
                          {"payload3"},
                          {"payload4"}};
    
    /* Send packets */
    for (packet_no = 0; packet_no < 5; packet_no++) {
        rv = -1;
        bzero(payload, 255);
        memcpy(payload, array[packet_no], 8);
        sendPacket(socketClient, packet_no);
        memset(recieving_buffer, 0, BUFFER_SIZE);
        
        /* Listen for ack, count time and resend packet if time out */
        while (send_counter < 4 && rv < 0) {
            rv = recvfrom(socketClient, recieving_buffer, BUFFER_SIZE-1, MSG_WAITALL, p->ai_addr, &p->ai_addrlen);
            if(rv == -1 && errno == EAGAIN){
                if(send_counter == 3){
                    printf("Server does not Respond.\n");
                    return -1;
                }
                send_counter++;
                printf("Did not receive ack in time. Resend the %d time.\n", send_counter);
                sendPacket(socketClient, packet_no);
            }
        }
        
        memcpy(&packet_front, recieving_buffer, 5);
        printf("Recieved packet. Type: \"%#X\"\n", packet_front.packet_type);
        
        /* Process error packet */
        if ( packet_front.packet_type == 0xFFF3) {
            memcpy(&packet_recieved, recieving_buffer, sizeof(packet_recieved));
            
            switch(packet_recieved.reject_sub_code) {
                case 0xFFF4:
                    printf("Packet is rejected. Error message: Out of sequence. %#X \n", packet_recieved.reject_sub_code);
                    break;
                case 0xFFF5:
                    printf("Packet is rejected. Error message: Length mismatch. %#X \n", packet_recieved.reject_sub_code);
                    break;
                case 0xFFF6:
                    printf("Packet is rejected. Error message: End of packet missing. %#X \n", packet_recieved.reject_sub_code);
                    break;
                case 0xFFF7:
                    printf("Packet is rejected. Error message: Duplicate packet. %#X \n", packet_recieved.reject_sub_code);
                    break;
            }
        }
    }
    
    freeaddrinfo(servinfo);
    close(socketClient);
    return 0;
}

void sendPacket(int socketClient, int packet_no) {
    int sendlen;
    
    /* Setup sending packet */
    packet_front.packet_type = 0XFFF1;
    packet_front.segment_no = packet_no;
    
    switch(packet_no) {
        case 0: // correct packet
            packet_front.length = strlen(payload);
            break;
        case 1: // duplicate
            packet_front.segment_no = 0;
            break;
        case 2: // out of sequence
            packet_front.segment_no = 100;
            break;
        case 4: // length mismatch
            packet_front.length = 100;
            break;
    }
    
    memset(sending_buffer, 0, BUFFER_SIZE);
    memcpy(sending_buffer, &packet_front, sizeof(packet_front));
    memcpy(sending_buffer + sizeof(packet_front), payload, strlen(payload));

    if (packet_no != 3) { // missing end of packet
        memcpy(sending_buffer + sizeof(packet_front) + strlen(payload), &packet_end, 2);
    }
    
    /* Sending packet */
    printf("\nSending packet %d...\n", packet_front.segment_no);
    sendlen = sendto(socketClient, sending_buffer, sizeof(packet_front) + strlen(payload) + 2, 0, p->ai_addr, p->ai_addrlen);
    if (sendlen == -1) {
        perror("send error");
        exit(1);
    }
    printf("Packet sent. Sent %d bytes\n", sendlen);
}
