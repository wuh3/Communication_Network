#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <queue>
#include <string>
#include <sstream>

using namespace std;
#define MAXCOST 100000;
unordered_map<int, unordered_map<int, int>> costMap;  // topo
unordered_map<int, unordered_map<int, pair<int, int> > > forwardingTable;
set <int> allNodes;  //nodes

void dijkstra() {
    FILE * fpout;
    fpout = fopen("output.txt", "a");

    forwardingTable.clear();
    int numNodes = allNodes.size();
    for (auto node: allNodes) {
        unordered_set<int> confirmed;
        unordered_map<int, int> visited;
        unordered_map<int, int> costs;

        confirmed.insert(node);
        for (auto n: allNodes) {
            if (costMap[node].count(n) == 0) {
                costs[n] = MAXCOST;
            } else {
                costs[n] = costMap[node][n];
            }
            visited[n] = node;
        }

        queue<int> minCost_nodes;

        int times = 0;   //dijkstra
        while(times < numNodes-1){
            int min_cost = MAXCOST; int min_node = MAXCOST;
            for (auto v: allNodes) {
                if (confirmed.count(v) > 0) {continue;
                } else {
                    // Link State: if there is a tie, choose the lowest node ID
                    if (costs[v] == min_cost) {min_node = min(v, min_node);}
                    if (costs[v] < min_cost) {
                        min_cost = costs[v];
                        min_node = v;
                    }
                }
            }
            confirmed.insert(min_node);
            minCost_nodes.push(min_node);

            for (auto pair: costMap[min_node]) {
                if (confirmed.count(pair.first) > 0) {continue;
                } else {
                    if ((costs[min_node]+pair.second == costs[pair.first]) && (min_node < visited[pair.first]) && (costs[min_node] != 100000)) {
                        visited[pair.first] = min_node;
                    }
                    if ((costs[min_node]+pair.second < costs[pair.first]) && (costs[min_node] != 100000)) {
                        costs[pair.first] = costs[min_node] + pair.second;
                        visited[pair.first] = min_node;
                    }
                }
            }

            times += 1;
        }

        unordered_map<int, pair<int, int> > fTable;
        fTable[node].first = node;
        fTable[node].second = 0;
        while (minCost_nodes.empty() != true) {
            int first = minCost_nodes.front();
            minCost_nodes.pop();
            if (visited[first] != node) {
                fTable[first].first = fTable[visited[first]].first;
                fTable[first].second = costs[first];
            } else {
                fTable[first].first = first;
                fTable[first].second = costs[first];
            }
        }
        forwardingTable[node] = fTable;
    }

    // output forwardingTable
    for (int i = 1; i <= numNodes; i++) {
        for (int j = 1; j <= numNodes; j++) {
            if ((allNodes.count(i) == 0) || (allNodes.count(j) == 0)) {continue;}
            if (forwardingTable[i][j].second == 100000) {continue;}
            if (i == j) {fprintf(fpout, "%d %d %d\n", j, j, 0); continue;}
            fprintf(fpout, "%d %d %d\n", j, forwardingTable[i][j].first, forwardingTable[i][j].second);
        }
        fprintf(fpout, "\n");
    }

    fclose(fpout);
}


void messagetext(char* messagefile) {
    ifstream fin;
    string line;
    fin.open(messagefile);
    if (!fin.is_open()) {
        cout<<"open messagefile failed";
        exit(1);
    } else {
        while (getline(fin, line)) {
            char message[line.length()];
            int source = -1, destination = -1;
            sscanf(line.c_str(), "%d %d %[^\n]", &source, &destination, message);
            if (source == -1 || destination == -1) {continue;
            } else {
                FILE * fpout;
                fpout = fopen("output.txt", "a");
                if (forwardingTable[source][destination].second == 100000 || allNodes.count(source) <= 0 || allNodes.count(destination) <=0) {
                    fprintf(fpout, "from %d to %d cost infinite hops unreachable message %s\n", source, destination, message);
                    fclose(fpout);
                    continue;
                }

                vector<int> hops;
                int hop_now = source;
                while (hop_now != destination) {
                    hops.push_back(hop_now);
                    hop_now = forwardingTable[hop_now][destination].first;
                }
                fprintf(fpout, "from %d to %d cost %d hops ", source, destination, forwardingTable[source][destination].second);
                for (auto hop : hops) {fprintf(fpout, "%d ", hop);}
                if (hops.empty() == true) {fprintf(fpout, " ");}
                fprintf(fpout, "message %s\n", message);
                fprintf(fpout, "\n\n");
                fclose(fpout);
            }
        }
        fin.close();
    }

}

void change(char* changefile, char* messagefile) {
    ifstream fin;
    int src, dest, cost;
    fin.open(changefile);
    if (!fin.is_open()) {
        cout<<"open changefile failed";
        exit(1);
    } else {
        while (fin >> src >> dest >> cost) {
            if (src == 0 || dest == 0 || cost == 0) {continue;
            } else if (cost == -999) {
                costMap[src].erase(dest);
                costMap[dest].erase(src);
                if (costMap.count(src) <= 0) {allNodes.erase(src);}
                if (costMap.count(dest) <= 0) {allNodes.erase(dest);}
            } else {
                allNodes.insert(src);
                allNodes.insert(dest);
                costMap[src][dest] = cost;
                costMap[dest][src] = cost;
            }
            dijkstra();
            messagetext(messagefile);
        }
        fin.close();
    }
}

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }

    FILE *fpOut;
    fpOut = fopen("output.txt", "w");

    ifstream fin;
    int init_source, init_dest, init_cost;
    fin.open(argv[1]);

    if (!fin.is_open()) {
        cout<<"open topofile failed";
        exit(1);
    } else {
        while (fin >> init_source >> init_dest >> init_cost) {
            if (init_source == 0 || init_dest == 0) {
                continue;
            }
            costMap[init_source][init_dest] = init_cost;
            costMap[init_dest][init_source] = init_cost;
            allNodes.insert(init_source);
            allNodes.insert(init_dest);
        }
        fin.close();
    }
    dijkstra();
    messagetext(argv[2]);
    change(argv[3], argv[2]);

    fclose(fpOut);
    return 0;
}
