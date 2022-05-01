#pragma once

#include "PeerNode.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

class CurrentNode
{
public:
    CurrentNode(string uuid) {
        _uuid = uuid;
        _backendPort = -1;
        _linkstateSeq = 0;

    }

    string getUuid() { return _uuid; }
    string getName() { return _name; }
    int getBackendPort() { return _backendPort; }
    int getLinkstateSeq() { return _linkstateSeq; }
    vector<PeerNode> &getNeighbors() { return _neighbors;  }
    int getPeerSize() { return _neighbors.size(); }
    int getActivePeerNum() {
        int num = 0;
        for (PeerNode &p : _neighbors) {
            if (p.getReachable()) {
                num++;
            }
        }
        return num;
    }

    void setName(string name) { _name = name; }
    void setBackendPort(int portNum) { _backendPort = portNum; }
    void addLinkstateSeq() { _linkstateSeq++; }
    void addNeighbor(PeerNode pNode) { 
        _neighbors.push_back(pNode);
    }

    void printInfo() {
        cout << "uuid = " << _uuid << endl;
        cout << "name = " << _name << endl;
        cout << "backendPort = " << _backendPort << endl;
        cout << "Number of neighbors = " << _neighbors.size() << endl;
        printAllNeighbors();
    }

    void printAllNeighbors() {
        for (PeerNode peer : _neighbors) {
            peer.printPeerNode();
        }
    }

    void printActiveNeighbors() {
        bool activeNeighbors = false;
        for (PeerNode peer : _neighbors) {
            if (peer.getReachable()) {
                activeNeighbors = true;
                peer.printPeerNode();
            }
        }
        if (!activeNeighbors) {
            cout << "No active neighbors!" << endl;
        }
    }

private:
    string _uuid;
    string _name;
    int _backendPort;
    int _linkstateSeq;
    vector<PeerNode> _neighbors;
};













