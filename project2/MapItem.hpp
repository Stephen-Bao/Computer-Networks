#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "PeerInfo.hpp"

using namespace std;

class MapItem
{
public:
    MapItem(string name, string uuid, int sequence, vector<PeerInfo> peers) {
        _name = name;
        _uuid = uuid;
        _sequence = sequence;
        _peers = peers;
    }

    string getName() { return _name; }
    string getId() { return _uuid; }
    int getSequence() { return _sequence; }
    vector<PeerInfo> getPeers() { return _peers; }

    void setName(string name) { _name = name; }
    void setSequence(int sequence) { _sequence = sequence; }
    void setPeers(vector<PeerInfo> peers) { _peers = peers; }

    void print() {
        string identity;
        if (_name == "N/A") identity = _uuid;
        else identity = _name;
        string printMsg = identity + ": {";
        for (int i = 0; i < _peers.size() - 1; i++) {
            string peerIdentity = (_peers[i].getName() == "N/A" ? _peers[i].getId() : _peers[i].getName());
            printMsg.append(peerIdentity).append(":").append(to_string(_peers[i].getMetric())).append(", ");
        }
        int idx = _peers.size() - 1;
        string peerIdentity = (_peers[idx].getName() == "N/A" ? _peers[idx].getId() : _peers[idx].getName());
        printMsg.append(peerIdentity).append(":").append(to_string(_peers[idx].getMetric())).append("}");

        cout << printMsg;
    }

private:
    string _name;
    string _uuid;
    int _sequence;
    vector<PeerInfo> _peers;
};











