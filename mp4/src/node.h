#ifndef NODE_H
#define NODE_H

#include <iostream>
#include <stdlib.h>
#include <string>
#include <vector>

using namespace std;

class node {
    public:
      int backoff;
      int collisionNum;
      int transmitTime;
      bool isCollided;
      node (int col, int trans, bool collided) {
          this->collisionNum = col;
          this->transmitTime = trans;
          this->isCollided = collided;
      }
      void randomizeCollisonNum(int i, int offset, vector<int> &R) {
          backoff = (i + offset) % R[collisionNum];
      }
};

#endif