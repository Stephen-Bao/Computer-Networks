#pragma once

#include <iostream>
#include <string>

using namespace std;

class PeerNode
{
public:
    PeerNode(string uuid, string hostname, int backendPort, int distance, bool reachable, int timeCount) {
        _uuid = uuid;
        _hostname = hostname;
        _backendPort = backendPort;
        _distance = distance;
        _reachable = reachable;
        _timeCount = timeCount;
        _name = "";
    }

    void printPeerNode() {
        cout << "uuid: " << _uuid << ", name: " << _name << ", host: " << _hostname
             << ", backendPort: " << _backendPort << ", distance: " << _distance << endl;
    }

    string getId() { return _uuid;  }
    string getName() { return _name; }
    string getHostname() { return _hostname;  }
    int getPort() {return _backendPort;}
    int getDistance() { return _distance; }
    bool getReachable() { return _reachable; }
    int getTimeCount() { return _timeCount; }

    void setName(string name) { _name = name; }
    void setReachable(bool reachable) { _reachable = reachable; }
    void addTimeCount() { _timeCount++; }
    void setTimeCount(int count) { _timeCount = count; }

private:
    string _uuid;
    string _name;
    string _hostname;
    int _backendPort;
    int _distance;
    bool _reachable;
    int _timeCount;
};












