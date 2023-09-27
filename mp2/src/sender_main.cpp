/* 
 * File:   sender_main.c
 * Author: rmin3
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include <netdb.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <math.h>
#include <iostream>
#include <queue>

using namespace std;

#define RTT 20           // 20ms
#define MAXDATASIZE 1500 //Maximum Segment Size in one IP packet  MTU-28=1472
#define BUFFER_SIZE 300  //slide window size
#define DATA 0
#define SYN 1
#define ACK 2
#define FIN 3
#define FINACK 4
#define MAX_SEQUENCE_NUM 100000  // max sequence number ----> 100000


class Packet{
    public:
        int size;
        int seqNum;
        int ackNum;
        int type;   
        char content[MAXDATASIZE];
};


int s;
FILE *fp;
unsigned long long int pkt_total;
unsigned long long int read_bytes;

// Socket 
struct sockaddr_storage other_addr;     // receiver address
socklen_t addr_len = sizeof other_addr;
struct addrinfo hints, *recvinfo, *p;
int character_num;

// Slide Window
unsigned long long int seq_number;
char sw_buffer[sizeof(Packet)];
queue<Packet> buffer;
queue<Packet> waitforack;

// Congestion Window
double cwnd = 1.0;
double thread = 64, dupAckCount = 0;
enum socket_state {SLOW_START, CONGESTION_AVOIDANCE, FAST_RECOVERY, FIN_WAIT};
int cw_state = SLOW_START;  /*{SLOW_START, CONGESTION_AVOIDANCE, FAST_RECOVERY, FIN_WAIT}*/


int packetToBuffer(int pkt_number);
void send(int s);
void congestion_window(bool newACK, bool timeout);


int packetToBuffer(int pkt_num) {  
    Packet pkt;
    int count_bytes;
    char buf[MAXDATASIZE]; //max->MTU
    int c = 0;
    if (pkt_num){
        for (int i = 0; read_bytes!= 0 && i < pkt_num; ++i) {       
            if (read_bytes < MAXDATASIZE) {count_bytes = read_bytes;} else {count_bytes = MAXDATASIZE;}
            int file_size = fread(buf, sizeof(char), count_bytes, fp);
            if (file_size > 0) {
                pkt.size = file_size;
                pkt.type = DATA;
                pkt.seqNum = seq_number;
                memcpy(pkt.content, &buf, sizeof(char)*count_bytes);
                buffer.push(pkt);
                seq_number = (seq_number + 1) % MAX_SEQUENCE_NUM;
            }          
            read_bytes -= file_size;
            c = i;
        }
        return c;
    } else {
        return 0;
    }   
}


void send(int s) {   
    int send_packets;
    if ((cwnd - waitforack.size()) <= buffer.size()) {
        send_packets = cwnd - waitforack.size();
    } else {
        send_packets = buffer.size();
    }

    if (cwnd - waitforack.size() < 1) {
        memcpy(sw_buffer, &waitforack.front(), sizeof(Packet));
        character_num = sendto(s, sw_buffer, sizeof(Packet), 0, p->ai_addr, p->ai_addrlen); //UDP: sendto
        if(character_num == -1) {        // fail -1
            perror("fail to push packets into slide window buffer");
            printf("fail to push the %d packet", waitforack.front().seqNum);
            exit(2);
        }
        cout << "sending packet "<< waitforack.front().seqNum << "cwnd = "<< cwnd << endl;
        return;
    }

    if (buffer.empty()) {
        cout << "No more packets" << endl;
        return;
    }

    for (int i = 0; i < send_packets; ++i) {
        memcpy(sw_buffer, &buffer.front(), sizeof(Packet));
        character_num = sendto(s, sw_buffer, sizeof(Packet), 0, p->ai_addr, p->ai_addrlen);
        if(character_num == -1){      // fail -1
            perror("fail to push packets into slide window buffer");
            printf("fail to push the %d packet", buffer.front().seqNum);
            exit(2);
        }
        cout << "sending packet  "<< buffer.front().seqNum << ", cwnd = "<< cwnd << endl;
       
        waitforack.push(buffer.front());
        buffer.pop();
    }
    packetToBuffer(send_packets);
}


void congestion_window(bool newACK, bool timeout) {
    switch (cw_state) {
        case SLOW_START:
            if (timeout) {
                thread = cwnd/2.0;
                cwnd = 1;
                dupAckCount = 0;
                return;
            }
            if (newACK) {
                dupAckCount = 0;
                cwnd = (cwnd+1 >= BUFFER_SIZE) ? BUFFER_SIZE-1 : cwnd+1;
            } else {
                dupAckCount++;
            }
            if (cwnd >= thread) {
                cout << "SLOW_START to CONGESTION_AVOIDANCE, cwnd = " << cwnd <<endl;
                cw_state = CONGESTION_AVOIDANCE;
            }
            break;
        case CONGESTION_AVOIDANCE:
            if (timeout) {
                thread = cwnd/2.0;
                cwnd = 1;
                dupAckCount = 0;
                cout << "CONGESTION_AVOIDANCE to SLOW_START, cwnd=" << cwnd <<endl;
                cw_state = SLOW_START;
                return;
            }
            if (newACK) {
                cwnd = (cwnd+ 1.0/cwnd >= BUFFER_SIZE) ? BUFFER_SIZE-1 : cwnd+ 1.0/cwnd;
                dupAckCount = 0;
            } else {
                dupAckCount++;
            }
            break;
        case FAST_RECOVERY:
            if (timeout) {
                thread = cwnd/2.0;
                cwnd = 1;
                dupAckCount = 0;
                cout << "FAST_RECOVERY is SLOW_START, cwnd= " << cwnd <<endl;
                cw_state = SLOW_START;
                return;
            }
            if (newACK) {
                cwnd = thread;
                dupAckCount = 0;
                cout << "FAST_RECOVERY is CONGESTION_AVOIDANCE, cwnd = " << cwnd << endl;
                cw_state = CONGESTION_AVOIDANCE;
            } else {
                cwnd = (cwnd+1 >= BUFFER_SIZE) ? BUFFER_SIZE-1 : cwnd+1;
            }
            break;
        default:
            break;
    }
}




void cw_status(int s) {
    Packet pkt;
    while (!buffer.empty() || !waitforack.empty()) {
        
        if((character_num = recvfrom(s, sw_buffer, sizeof(Packet), 0, NULL, NULL)) == -1) {                        //receive ack fail
            if (errno != EAGAIN || errno != EWOULDBLOCK) { perror("can not receive ack from receiver"); exit(2);}
            cout << "need resend packet" << waitforack.front().seqNum << endl;
            memcpy(sw_buffer, &waitforack.front(), sizeof(Packet));
            
            if((character_num = sendto(s, sw_buffer, sizeof(Packet), 0, p->ai_addr, p->ai_addrlen)) == -1){
                perror("fail to push packets into slide window buffer");
                printf("fail to push the %d packet", waitforack.front().seqNum);
                exit(2);
            }
            congestion_window(false, true);
            /*
            thread = cwnd/2.0;
            cwnd = 1;
            dupAckCount = 0;
            dupAckCount++;
            if (cwnd >= thread) {
                cout << "CW status: SLOW_START -> CONGESTION_AVOIDANCE, cwnd = " << cwnd <<endl;
                cw_state = CONGESTION_AVOIDANCE;
            }         
            */    

        } else {
            memcpy(&pkt, sw_buffer, sizeof(Packet));
            if (pkt.type == ACK) {
                cout << "ACK received successfully " << pkt.ackNum << endl;
                if (pkt.ackNum > waitforack.front().seqNum) {
                    while (!waitforack.empty() && waitforack.front().seqNum < pkt.ackNum) {
                        congestion_window(true, false);
                        
                        /*
                        dupAckCount = 0;
                        cwnd = (cwnd+1 >= BUFFER_SIZE) ? BUFFER_SIZE-1 : cwnd+1; 
                        if (cwnd >= thread) {
                            cout << "CW status: SLOW_START -> CONGESTION_AVOIDANCE, cwnd = " << cwnd <<endl;
                            cw_state = CONGESTION_AVOIDANCE;
                        }                       
                        */
                        waitforack.pop();
                    }
                    send(s);
                } else if (pkt.ackNum == waitforack.front().seqNum) {
                    congestion_window(false, false);
                    /*
                    dupAckCount++;
                    if (cwnd >= thread) {
                        cout << "CW status: SLOW_START -> CONGESTION_AVOIDANCE, cwnd = " << cwnd <<endl;
                        cw_state = CONGESTION_AVOIDANCE;
                    }
                    
                    */
                
                    if (dupAckCount == 3) {
                        thread = cwnd/2.0;
                        cwnd = thread + 3;
                        dupAckCount = 0;
                        cw_state = FAST_RECOVERY;
                        cout << "CW status: -> FAST_RECOVERY, cwnd = " << cwnd <<endl;
                        memcpy(sw_buffer, &waitforack.front(), sizeof(Packet));                          // resend duplicated pkt
                        
                        if((character_num = sendto(s, sw_buffer, sizeof(Packet), 0, p->ai_addr, p->ai_addrlen)) == -1){
                            perror("fail to push packets into slide window buffer");
                            printf("fail to push the %d packet", waitforack.front().seqNum);
                            exit(2);
                        }         
                        cout << "need resend packet " << waitforack.front().seqNum << endl;
                    }
                } 
            }
        }
    }
}


void fin_ack(int s) {
    Packet pkt, ack;
    while (true) {
        pkt.type = FIN;
        pkt.size = 0;
        memcpy(sw_buffer, &pkt, sizeof(Packet));
        character_num = sendto(s, sw_buffer, sizeof(Packet), 0, p->ai_addr, p->ai_addrlen);
        if(character_num == -1){ perror("receiver failed to send FIN to sender"); exit(2);}
        character_num = recvfrom(s, sw_buffer, sizeof(Packet), 0, (struct sockaddr *) &other_addr, &addr_len);
        if (character_num == -1) { perror("receiver failed to receive from sender");exit(2);}
        memcpy(&ack, sw_buffer, sizeof(Packet));
        if (ack.type == FINACK) {cout << "receive the finish ACK." << endl;break;}
    }
}

 
void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    // Open the file
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

	/* Determine how many bytes to transfer -> ceil(bytesToTransfer * 1.0 / MAXDATASIZE) */
    read_bytes = bytesToTransfer;
    
    packetToBuffer(BUFFER_SIZE);      // push packets num into buffer  

    int result, s;
    char portStr[10];
    sprintf(portStr, "%d", hostUDPport);
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    memset(&recvinfo, 0 ,sizeof recvinfo);
    result = getaddrinfo(hostname, portStr, &hints, &recvinfo);  // if succeed, getaddrinfo() == 0 
    if (result != 0) {                 
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result)); // Name or service not known -> to screen
        s = 1;         
    } else {
        for(p = recvinfo; p != NULL; p = p->ai_next) {
            s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);  // s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
            if ((s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {perror("socket error"); continue;}                         // if false, socket() == -1
            break;
        }
        if (p == NULL) { fprintf(stderr, "socket failed\n"); exit(1);}
    }

    struct timeval T;
    T.tv_sec = 0;
    T.tv_usec = 2*1000*RTT;     // 20msec 
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &T, sizeof(T));
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &T, sizeof(T)) == -1) {  // success 0; fail -1
        fprintf(stderr, "setting timeout failed\n");
        return;
    }
    
    send(s);

    cw_status(s);

    fclose(fp);

    fin_ack(s);


    /* Send data and receive acknowledgements on s*/

    printf("Closing the socket\n");
    close(s);
    return;

}


int main(int argc, char** argv)
{
    unsigned short int udpPort;
    unsigned long long int numBytes;

    if(argc != 5)
    {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int)atoi(argv[2]);
    numBytes = atoll(argv[4]);



    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);
    
    
    return (EXIT_SUCCESS);  
}
