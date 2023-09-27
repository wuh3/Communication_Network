/*
 * File:   receiver_main.c
 * Author:
 *
 * Created on
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
#include <fcntl.h>
#include "string"
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <map>
#include <set>

#define MAXDATASIZE 1500
#define ACK 2
#define FINACK 4
#define DATA 0
#define FIN 3
using namespace std;

class Packet{
    public:
        int size;
        int seqNum;
        int ackNum;
        int type;
        char content[MAXDATASIZE];
};

struct sockaddr_in si_me, si_other;
int s, slen;
char buf[sizeof(Packet)];

void diep(std::string s) {
    perror(s.c_str());
    exit(1);
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    struct sockaddr_in their_addr;
    socklen_t their_addr_size = sizeof(their_addr);
    slen = sizeof (si_other);
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        diep("socket");
    }

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (::bind(s, (struct sockaddr*)&si_me, sizeof (si_me)) < 0) {
        diep("bind");
    }
    /* Now receive data and send acknowledgements */

    FILE* fp = fopen(destinationFile,"wb");
    map<int, char> buffer; // buffer map to store <seq # -> pack>;
    set<int> acked; // Hashset to keep track of the sequence number got acknoledged
    map<int, int> dataSize; // Store the sizes of the packets
    int expectSeq = 0;
    int numbytes = 0;

    their_addr_size = sizeof their_addr;
    int currSeq = 0;
    while(true) {
        numbytes = recvfrom(s, buf, sizeof(Packet), 0, (struct sockaddr*)&their_addr, &their_addr_size);
        if (numbytes <= 0){
            cout << "Transmission Done. End process" << endl;
            exit(2);
        }
        Packet pckt;
        memcpy(&pckt,buf,sizeof(Packet));
        cout << "Received Packet type " <<  pckt.type << "With seq num " << pckt.seqNum << endl;
        if (pckt.type == DATA){
            if (pckt.seqNum > expectSeq) {
                int earlySeq = currSeq + pckt.seqNum - expectSeq;
                dataSize.insert({earlySeq, pckt.size});
                acked.insert(earlySeq);
                for (int i = 0; i < pckt.size; i++) {
                    // Write early packet to the buffer
                    buffer.insert({earlySeq * MAXDATASIZE + i, pckt.content[i]});
                }
            } else if (pckt.seqNum == expectSeq) {
                for (int i = 0; i < pckt.size; i++) {
                    memcpy(&buffer[currSeq * MAXDATASIZE + i], &pckt.content[i], 1);
                }
                for (int i = 0; i < pckt.size; i++) {
                        fwrite(&buffer[currSeq * MAXDATASIZE + i], sizeof(char), 1, fp);
                }
                expectSeq++;
                currSeq++;
                cout << "Wrote packet with seqNum "<< pckt.seqNum << endl;
                while (acked.find(currSeq) != acked.end()) {
                    int size = MAXDATASIZE;
                    if (dataSize.count(currSeq)) size = dataSize[currSeq];
                    for (int i = 0; i < size; i++) {
                        fwrite(&buffer[currSeq * MAXDATASIZE + i], sizeof(char), 1, fp);
                    }
                    
                    acked.erase(currSeq); // Remove from set
                    currSeq++;
                    expectSeq++;
                }
            }
            Packet ack;
            ack.size = 0; 
            ack.type = ACK;
            ack.ackNum = expectSeq;
            memcpy(buf, &ack, sizeof(Packet));
            sendto(s, buf, sizeof(Packet), 0, (struct sockaddr *) &their_addr, their_addr_size);
            cout << "Sent ack with seqNum " << ack.ackNum << endl;
        } else if(pckt.type == FIN) {
            // Received connection close request
            Packet ack;
            ack.size = 0;
            ack.type = FINACK;
            ack.ackNum = expectSeq;

            memcpy(buf,&ack,sizeof(Packet));
            sendto(s, buf, sizeof(Packet), 0, (struct sockaddr *) &their_addr,their_addr_size);
            break;
        }
    }
    close(s);
    printf("%s received.", destinationFile);
    return;
}

/*
 *
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}