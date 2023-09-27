

#include <stdlib.h>
#include <iostream>
#include <string>
#include <fstream>
#include <queue>
#include <sstream>
#include <vector>
#include <climits>
#include <algorithm>
#include <math.h>

using namespace std;

#include "node.h"


int  M, T, N, L;
vector<int> R;
int numOfSuccess = 0;

void parseLines(string line) {
    istringstream ss(line);
	string type;
    for(string s; ss >> s; ) {
        if (type == "N") {
        	N = atoi(s.c_str());
        } else if (type == "L") {
        	L = atoi(s.c_str());
        } else if (type == "M") {
        	M = atoi(s.c_str());
        } else if (type == "T") {
        	T = atoi(s.c_str());
        } else if (type == "R") {
        	R.push_back(atoi(s.c_str()));
        }

        if (s == "N" || s == "L" || s == "M" || s == "R"  || s == "T")
        	type = s;

    }
}

void initialization(char* filename) {
	ifstream in;
	string line;
	in.open(filename);

	if (in.is_open()) {
		for (int i =0; i < 5; i++) {
			getline(in, line);
            parseLines(line);
		}
		in.close();
	} else {
		exit(1);
	}
}

void simulation(){
    vector<node> nodes;
    FILE *fpOut;

    for(int i = 0; i < N; ++i){
        node n(0, L, false);
        n.randomizeCollisonNum(i, 0, R);
        nodes.push_back(n);
    }

    for (int currTime = 0; currTime < T; currTime++){
        vector<int> nodesToTransfer;
        for (int i = 0; i < N; ++i){
            node& n = nodes[i];
            if (n.transmitTime != 0 && n.collisionNum != M) {
                if (n.isCollided == true) {
                    n.randomizeCollisonNum(i, currTime, R);
                    n.transmitTime = L;
                }
            } else {
                // reset
                n.collisionNum = 0;
                n.randomizeCollisonNum(i, currTime, R);
                n.transmitTime = L;
            }
            n.isCollided = false;
            if (n.backoff == 0) {
                nodesToTransfer.push_back(i);
            }
        }
        if (!nodesToTransfer.empty()) {
            if (nodesToTransfer.size() == 1) {
                // Only one node, no collision. success++
                nodes[nodesToTransfer[0]].transmitTime--;
                numOfSuccess++;
            } else if (nodesToTransfer.size() > 1) {
                // Collision happens
                for (int idx = 0; idx < nodesToTransfer.size(); idx++) {
                    node &n = nodes[nodesToTransfer[idx]];
                    n.isCollided = true;
                    n.collisionNum++;
                }
            }
        } else {
            for (node &n: nodes)
                n.backoff--;
        }
    }

    fpOut = fopen("output.txt", "w");
    fprintf(fpOut,"%.2f",(float) numOfSuccess /T);
    fclose(fpOut);
}

int main(int argc, char **argv) {

    if (argc != 2) {
        printf("Usage: ./csma input.txt\n");
        return -1;
    }

    initialization(argv[1]);

    simulation();
    return 0;
}
