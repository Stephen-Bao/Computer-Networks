#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>

#include "MapItem.hpp"
#include "PeerInfo.hpp"

using namespace std;

bool compare_by_metric(const pair<string, int> lhs, const pair<string, int> rhs) {
    return lhs.second < rhs.second;
}

vector<pair<string, int>> rankByMetric(vector<MapItem> network, string myUuid) {
    map<string, int> done;
    map<string, int> horizon;
    set<string> unseen;
    // Initialization
    done[myUuid] = 0;
    for (MapItem &item : network) {
        if (item.getId() == myUuid) {
            for (PeerInfo &peer : item.getPeers()) {
                horizon[peer.getId()] = peer.getMetric();
            }
        }
    }
    for (MapItem &item : network) {
        if (item.getId() != myUuid && horizon.find(item.getId()) == horizon.end()) {
            unseen.insert(item.getId());
        }
    }
    // Iteration
    while (horizon.size() != 0) {
        // Find least cost in horizon, then add related elems into done
        int minCostInHorizion = horizon.begin()->second;
        for (auto &elem : horizon) {
            if (elem.second < minCostInHorizion) {
                minCostInHorizion = elem.second;
            }
        }
        vector<string> newDoneNodesId;
        auto it = horizon.begin();
        while (it != horizon.end()) {
            if (it->second == minCostInHorizion) {
                newDoneNodesId.push_back(it->first);
                done.insert(*it);
                it = horizon.erase(it);
            }
            else {
                it++;
            }
        }
        // Update horizon and unseen
        for (string id : newDoneNodesId) {
            for (MapItem &item : network) {
                if (item.getId() == id) {
                    for (PeerInfo &peer : item.getPeers()) {
                        if (done.find(peer.getId()) != done.end()) {
                            continue;
                        }
                        else if (horizon.find(peer.getId()) != horizon.end()) {
                            int potentialNewCost = done[id] + peer.getMetric();
                            if (potentialNewCost < horizon[peer.getId()]) {
                                horizon[peer.getId()] = potentialNewCost;
                            }
                        }
                        else if (unseen.find(peer.getId()) != unseen.end()) {
                            horizon[peer.getId()] = done[id] + peer.getMetric();
                            unseen.erase(peer.getId());
                        }
                    }
                }
            }
        }
    }
    // convert done collection and return
    vector<pair<string, int>> rankVector(done.begin(), done.end());
    sort(rankVector.begin(), rankVector.end(), compare_by_metric);
    rankVector.erase(rankVector.begin());
    for (auto &elem : rankVector) {
        for (MapItem &item : network) {
            if (item.getId() == elem.first && item.getName() != "N/A") {
                elem.first = item.getName();
            }
        }
    }

    return rankVector;
}
















