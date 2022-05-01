#pragma once

#include <string>

using namespace std;

class PeerInfo
{
public:
    PeerInfo(string name, string uuid, int metric) {
        _name = name;
        _uuid = uuid;
        _metric = metric;
    }

    string getName() { return _name; }
    string getId() { return _uuid; }
    int getMetric() { return _metric; }

    void setName(string name) { _name = name; }

private:
    string _name;
    string _uuid;
    int _metric;
};












