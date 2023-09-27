#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <climits>
#include <vector>
#include <stdio.h>
#include <stdlib.h>

using namespace std;
#define NUM_OF_NODE 30

// Initializations of cost and forward tables
int topo[NUM_OF_NODE + 1][NUM_OF_NODE + 1];
pair<int, int> forwardTable[NUM_OF_NODE + 1][NUM_OF_NODE + 1]; // pair: nextId, cost

// set of nodes avoid dups.
set<int> nodes;

class Message {
public:
	int startId;
	int routeId;
	string info;

	Message(int s, int r, string i);
};

Message::Message(int s, int r, string i) {
    startId = s;
    routeId = r;
    info = i;
}

vector<Message> msgs;

// output file
ofstream fpOut("output.txt");

// function
void readMessages(char **argv);
void buildTopo(ifstream &topofile);
void generateForwardTable();
void bellMan();
void sendMessage();
void setUp(char **argv);
void findMinPath(int startId, int currRoute);

int main(int argc, char **argv) {
	if (argc != 4) {
		printf("Usage: ./distvec topofile messagefile changesfile\n");
		return -1;
	}

	setUp(argv);

	int startId, routeId, cost;

	ifstream changesfile(argv[3]);
    string line;

    while (getline(changesfile, line)) {
        stringstream line_ss(line);
        line_ss >> startId;
        line_ss >> routeId;
        line_ss >> cost;

		topo[startId][routeId] = cost;
		topo[routeId][startId] = cost;
		generateForwardTable();
		bellMan();
		sendMessage();
	}

	fpOut.close();

	return 0;
}

void setUp(char **argv) {
	// Read message -> calculate costs -> build cost table -> find min path -> deliever msgs
	readMessages(argv);
	ifstream topofile(argv[1]);
	buildTopo(topofile);
	generateForwardTable();
	bellMan();
	sendMessage();
}

void readMessages(char **argv) {
    int startId, routeId;

    ifstream messagefile(argv[2]);
    string currLine, info;
    while (getline(messagefile, currLine)) {
        if (currLine.length() != 0) {
            stringstream lineStream(currLine);
            lineStream >> startId >> routeId;
            getline(lineStream, info);
            Message msg(startId, routeId, info.substr(1));
            msgs.push_back(msg);
        }
    }
}

void sendMessage() {
	int startId, routeId, currId;
	for (auto & msg : msgs) {
		startId = msg.startId;
		routeId = msg.routeId;
		currId = startId;

		fpOut << "from " << startId << " to " << routeId << " cost ";
        pair<int, int> costPair = forwardTable[startId][routeId];
        if (costPair.second != 0) {
            if (costPair.second < 0) {
                fpOut << "infinite hops unreachable ";
            } else {
                fpOut << costPair.second << " hops ";
                while (currId != routeId) {
                    fpOut << currId << " ";
                    currId = forwardTable[currId][routeId].first;
                }
            }
        } else {
            fpOut << costPair.second << " hops ";
        }
        fpOut << "message " << msg.info << endl;
    }
	fpOut << endl;
}

void bellMan() {
	int size = nodes.size();

	for (int iter = 1; iter <= size; iter++) {
		for (int startId: nodes) {
			for (int routeId: nodes)
			{
				findMinPath(startId, routeId);
			}
		}
	}

	for (int startId: nodes) {
		for (int routeId: nodes) {
			fpOut << routeId << " " << forwardTable[startId][routeId].first << " " << forwardTable[routeId][startId].second << endl;
		}
	}
}

void findMinPath(int currRoute, int startId) {
	int minId, minCost, nextId, currCost, totalCost;
	nextId = forwardTable[startId][currRoute].first;
	currCost = forwardTable[startId][currRoute].second;
	minId = nextId;
	minCost = currCost;
	for (int currId: nodes) {
        int topoCost = topo[startId][currId];
        int ftCost = forwardTable[currId][currRoute].second;
		if (topoCost >= 0 && ftCost >= 0)  {
			totalCost = topoCost + ftCost;
			if (totalCost < minCost || minCost < 0) {
				minId = currId;
				minCost = totalCost;
			}
		}
	}
	forwardTable[startId][currRoute] = make_pair(minId, minCost);
}

void generateForwardTable() {

	for (int startId: nodes) {
		for (int routeId: nodes) {
            if (topo[startId][routeId] >= 0) {
                forwardTable[startId][routeId] = make_pair(routeId, topo[startId][routeId]);
            } else {
                forwardTable[startId][routeId] = make_pair(-1, topo[startId][routeId]);
            }
        }
	}
}

void buildTopo(ifstream &topofile) {
	int routeId;
	int startId;
	int cost;
	fill(&topo[0][0], &topo[NUM_OF_NODE][NUM_OF_NODE], -1);
	for (int i = 0; i <= NUM_OF_NODE; i++) {
		topo[i][i] = 0;
	}
    string line;
    while (getline(topofile, line)) {
        stringstream line_ss(line);
        line_ss >> startId;
        line_ss >> routeId;
        line_ss >> cost;

		topo[routeId][startId] = cost;
		topo[startId][routeId] = cost;
		
		nodes.insert(routeId);
		nodes.insert(startId);
	}
}