#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

#include <uuid/uuid.h>
#include <pthread.h>
#include <unistd.h>
#include <cstring>

#include "CurrentNode.hpp"
#include "PeerNode.hpp"
#include "MapItem.hpp"
#include "PeerInfo.hpp"

using namespace std;

CurrentNode *pMyHost;
vector<MapItem> map;

// Function that sends udp message
void sendMessage(string hostname, int port, string message);
// Function that receives udp message
vector<string> recvMessage(int port);
// Function that generates a node list ranked by shortest path algorithm
vector<pair<string, int>> rankByMetric(vector<MapItem> network, string myUuid);

// Update the host's map item
void updateMyItem() {
    auto it = map.begin();
    while (it != map.end()) {
        if ((*it).getId() == pMyHost->getUuid()) {
            map.erase(it);
            break;
        }
        it++;
    }
    vector<PeerInfo> myPeerInfo;
    for (PeerNode &p : pMyHost->getNeighbors()) {
        if (p.getReachable()) {
            string name = p.getName();
            if (name == "") name = "N/A";
            PeerInfo info(name, p.getId(), p.getDistance());
            myPeerInfo.push_back(info);

        }
    } 
    MapItem myItem(pMyHost->getName(), pMyHost->getUuid(), pMyHost->getLinkstateSeq(), myPeerInfo);
    map.push_back(myItem);
}

// This functions sends link state message to all active neighbors
void sendLinkstateMessage() {
    // Only send link-state to reachable neighbors
    for (PeerNode &peer : pMyHost->getNeighbors()) {
        if (peer.getReachable()) {
            string hostname = peer.getHostname();
            int port = peer.getPort();
            string msg = "[link-state]\nfrom:\n";
            msg += "name: ";
            if (pMyHost->getName() == "") {
                msg += "N/A";
            }
            else {
                msg += pMyHost->getName();
            }
            msg += " uuid: " + pMyHost->getUuid();
            msg += " sequence: " + to_string(pMyHost->getLinkstateSeq()) + "\n";
            msg.append("PeerCount: ").append(to_string(pMyHost->getActivePeerNum())).append("\n");
            for (PeerNode &p : pMyHost->getNeighbors()) {
                if (!p.getReachable()) {
                    continue;
                }
                string name = p.getName();
                if (name == "") {
                    name = "N/A";
                }
                msg.append("name: ").append(name).append(" uuid: ").append(p.getId()).
                    append(" metric: ").append(to_string(p.getDistance())).append("\n");
            }
            sendMessage(hostname, port, msg);
        }
        pMyHost->addLinkstateSeq();
    }
}

// This function sends keepalive message to all neighbors
void *pulseFunc(void *pArg) {
    while (1) {
        vector<PeerNode> neighbors = pMyHost->getNeighbors();
        for (PeerNode peer : neighbors) {
            string hostname = peer.getHostname();
            int port = peer.getPort();
            string name = (pMyHost->getName() == "" ? "N/A" : pMyHost->getName());
            string message = "[keepalive] uuid=" + pMyHost->getUuid() + " name=" + name 
                + " port=" + to_string(pMyHost->getBackendPort()) + " metric=" + to_string(peer.getDistance());
            sendMessage(hostname, port, message);
        }
        sleep(10);
    }
}

// This function handles all receiving messages
void *msgHandleFunc(void *pArg) {
    // Load messageSenderName and message
    string *pString = (string *)pArg;
    string msgStr = *pString;
    delete (string *)pArg;
    stringstream s(msgStr);
    string messageSenderName;
    s >> messageSenderName;
    for (char &c : messageSenderName) {
        c = tolower(c);
    }
    string msg = msgStr.substr(messageSenderName.size() + 1);

    string word;
    stringstream ss(msg);
    ss >> word;

    // Keepalive message
    if (word == "[keepalive]") {
        ss >> word;
        string uuid = word.substr(5);
        ss >> word;
        string name = word.substr(5);
        /* cout << "[keepalive] from " + name << endl; */

        // Update link info wrt the uuid
        bool newNeighbor = true;
        bool infoUpdated = false;
        for (PeerNode &p : pMyHost->getNeighbors()) {
            if (p.getId() == uuid) {
                newNeighbor = false;
                p.setTimeCount(0);
                if (!p.getReachable()) {
                    p.setReachable(true);
                    infoUpdated = true;
                }
                // Record neighbor's name
                if (p.getName() == "") {
                    p.setName(name);
                    infoUpdated = true;
                }
            }
        }
        // Add a new neighbor if no existing neighbor's uuid matches
        if (newNeighbor) {
            infoUpdated = true;
            ss >> word;
            int port = stoi(word.substr(5));
            ss >> word;
            int metric = stoi(word.substr(7));
            PeerNode newNeighbor(uuid, messageSenderName, port, metric, true, 0);
            pMyHost->addNeighbor(newNeighbor);
        }
        // Neighbor status changes, link-state message triggered
        if (infoUpdated) {
            updateMyItem();
            sendLinkstateMessage();
        }
    }

    // Link-state message
    if (word == "[link-state]") {
        // Load sender info
        string senderName;
        string senderUuid;
        ss.seekg(12, ios::cur);
        ss >> senderName;
        if (senderName == "") senderName = "N/A";
        /* cout << "[link-state] from " + senderName << endl; */
        ss >> senderUuid >> senderUuid;
        if (senderUuid == pMyHost->getUuid()) {
            pthread_exit(NULL);
        }
        int seqNum;
        int peerNum;
        string temp;
        ss >> temp >> temp;
        seqNum = stoi(temp);
        ss >> temp >> temp;
        peerNum = stoi(temp);
        // Load peer info
        vector<PeerInfo> newPeers;
        for (int i = 0; i < peerNum; i++) {
            string name;
            string uuid;
            int metric;
            ss >> name >> name;
            ss >> uuid >> uuid;
            ss >> temp >> temp;
            metric = stoi(temp);
            PeerInfo info(name, uuid, metric);
            newPeers.push_back(info);
        }

        // Update map if it's a newer advertisement
        bool newItem = true;
        bool outdated = false;
        vector<string> downNodes;
        for (MapItem &item : map) {
            if (senderUuid == item.getId()) {
                newItem = false;
                if (seqNum <= item.getSequence()) {
                    outdated = true;
                    break;
                }
                // Check whether a remote node is down
                if (newPeers.size() < item.getPeers().size()) {
                    for (PeerInfo &oldInfo : item.getPeers()) {
                        bool found = false;
                        for (PeerInfo &newInfo : newPeers) {
                            if (oldInfo.getId() == newInfo.getId()) {
                                found = true;
                            }
                        }
                        if (found == false) {
                            downNodes.push_back(oldInfo.getId());
                        }
                    }
                }
                item.setName(senderName);
                item.setSequence(seqNum);
                item.setPeers(newPeers);
            }
        }
        // Erase down nodes' items
        if (downNodes.size() != 0) {
            for (string downNodeId : downNodes) {
                auto it = map.begin();
                while (it != map.end()) {
                    if ((*it).getId() == downNodeId) {
                        it = map.erase(it);
                    }
                    else {
                        it++;
                    }
                }
            }
        }

        if (outdated) {
            pthread_exit(NULL);
        }
        // Add item if it's new
        if (newItem) {
            MapItem newItem(senderName, senderUuid, seqNum, newPeers);           
            map.push_back(newItem);
            // Send link-state message to notify the new node about our link info
            sendLinkstateMessage();
        }
        // Forward message if it's new
        for (PeerNode peer : pMyHost->getNeighbors()) {
            // Avoid forwarding back to direct sender and orininal sender
            if (peer.getId() == senderUuid || peer.getHostname() == messageSenderName) {
                continue;
            }
            sendMessage(peer.getHostname(), peer.getPort(), msg);
        }
    }
    pthread_exit(NULL);
}

// This thread function listen to all messages coming in and sends them to msgHandle thread to handle
void *listenFunc(void *pArg) {
    int port = pMyHost->getBackendPort();
    while (1) {
        vector<string> msgVec = recvMessage(port);
        string msg = msgVec[1] + "\n" + msgVec[0];
        string *pString = new string(msg);
        pthread_t msgHandleThread;
        pthread_create(&msgHandleThread, NULL, msgHandleFunc, (void *)pString);
    }
}

// This function is a timer for keepalive message
void *timerFunc(void *pArg) {
    while (1) {
        sleep(1);
        for (PeerNode &peer : pMyHost->getNeighbors()) {
            if (peer.getReachable()) {
                peer.addTimeCount();
            }
            // Mark as unreachable when timeout and set timeCount as -1
            if (peer.getTimeCount() == 30) {
                peer.setReachable(false);
                peer.setTimeCount(-1);
                // Delete corresponding item in the map
                auto it = map.begin();
                while (it != map.end()) {
                    if ((*it).getId() == peer.getId()) {
                        it = map.erase(it);
                    }
                    else
                        ++it;
                }
                // Update my item
                updateMyItem();
                // Link-state message triggered
                sendLinkstateMessage();
            }
        }
    }
}

// This function periodically sends link-state messages
void *linkstateFunc(void *pArg) {
    // Sleep for a second to prevent sending keepalive and link-state at the same time
    sleep(1);
    // Periodically send link-state advertisements to reachable neighbors
    while (1) {
        sendLinkstateMessage();
        sleep(60);
    }
}

// The main thread
int main(int argc, char *argv[])
{
    // Get conf filename
    string conf;
    if (argc == 1) {
        conf = "node.conf";
    }
    else if (argc == 3) {
        conf = argv[2];
    }
    else {
        cerr << ("Invalid input!") << endl;
        return -1;
    }
    // Load config file
    fstream fs(conf);
    if (!fs) {
        cerr << "Can not open configuration file!" << endl;
        return -1;
    }
    string uuid;
    string line;
    vector<string> lines;
    while (getline(fs, line)) {
        lines.push_back(line);
    }
    // Get or create uuid
    istringstream iss(lines[0]);
    string temp;
    iss >> temp;
    if (temp == "uuid") {
        iss >> temp;
        iss >> uuid;
        /* cout << "uuid = " << uuid << endl; */
    }
    else {
        uuid_t tempId;
        char str[36];
        uuid_generate(tempId);
        uuid_unparse(tempId, str);
        uuid = str;
        /* cout << "str = " << str << endl; */
        /* cout << "uuid = " << uuid << endl; */
        ofstream ofs(conf);
        ofs << "uuid = " << uuid << endl;
        for (string &line : lines) {
            ofs << line << endl;
        }
    }
    // Create CurrentNode instance
    CurrentNode myHost(uuid);
    int index = 0, peerNum = 0;
    for (int i = 0; i < lines.size(); i++) {
        string line = lines[i];
        istringstream iss(line);
        string word, temp;
        iss >> word;
        if (word == "name") {
            iss >> temp >> temp;
            myHost.setName(temp);
        }
        else if (word == "backend_port") {
            int portNum;
            iss >> temp;
            iss >> portNum;
            myHost.setBackendPort(portNum);
        }
        else if (word == "peer_count") {
            index = i;
            iss >> temp >> peerNum;
            break;
        }
    }
    // Add peer nodes
    for (int i = 0; i < peerNum; i++) {
        string peerUuid;
        string peerHostname;
        int peerPort;
        int peerDistance;
        string peerInfo = lines[index + 1 + i];
        for (char &c : peerInfo) {
            if (c == ',') {
                c = ' ';
            }
        }
        istringstream iss(peerInfo);
        string temp;
        iss >> temp >> temp >> peerUuid >> peerHostname >> peerPort >> peerDistance;

        // Initially peers should be set as unreachable since we don't know the network state
        PeerNode peer(peerUuid, peerHostname, peerPort, peerDistance, false, 0);
        myHost.addNeighbor(peer);
    }

    pMyHost = &myHost;

    // Create a child thread to send keepalive messages
    pthread_t pulseThread;
    pthread_create(&pulseThread, NULL, pulseFunc, NULL);

    // Create a child thread to receive message
    pthread_t listenThread;
    pthread_create(&listenThread, NULL, listenFunc, NULL);

    // Create a timer thread
    pthread_t timerThread;
    pthread_create(&timerThread, NULL, timerFunc, NULL);

    // Create a thread to send link-state advertisement
    pthread_t linkstateThread;
    pthread_create(&linkstateThread, NULL, linkstateFunc, NULL);

    // Listen from keyboard
    string command;
    string keyword;
    while (getline(cin, command)) {
        istringstream iss(command);
        iss >> keyword;
        if (keyword == "kill") {
            // Update config file
            int activePeerNum = myHost.getActivePeerNum();
            int index = 0;
            ofstream ofs(conf);
            ofs << "uuid = " << myHost.getUuid() << endl;
            ofs << "name = " << myHost.getName() << endl;
            ofs << "backend_port = " << myHost.getBackendPort() << endl;
            ofs << "peer_count = " << activePeerNum << endl;
            
            for (PeerNode p : myHost.getNeighbors()) {
                if (p.getReachable()) {
                    ofs << "peer_" << index << " = " << p.getId() << ", " << p.getHostname() << ", " 
                        << p.getPort() << ", " << p.getDistance() << endl;
                    index++;
                }
            }

            break;
        }
        else if (keyword == "neighbors") {
            myHost.printActiveNeighbors();
        }
        else if (keyword == "addneighbor") {
            string peerUuid;
            string peerName;
            int peerPort;
            int peerDistance;
            for (char &c : command) {
                if (c == '=') {
                    c = ' ';
                }
            }
            istringstream newIss(command);
            string temp;
            vector<string> peerInfo;
            while (newIss >> temp) {
                peerInfo.push_back(temp);
            }
            peerUuid = peerInfo[2];
            peerName = peerInfo[4];
            peerPort = stoi(peerInfo[6]);
            peerDistance = stoi(peerInfo[8]);
            PeerNode peer(peerUuid, peerName, peerPort, peerDistance, false, 0);
            myHost.addNeighbor(peer);

            cout << "Neighbor added!" << endl;
            peer.printPeerNode();
        }
        else if (keyword == "map") {
            // Sort by name
            for (int i = 0; i < map.size(); i++) {
                for (int j = i + 1; j < map.size(); j++) {
                    if (map[i].getName() > map[j].getName()) {
                        MapItem temp = map[i];
                        map[i] = map[j];
                        map[j] = temp;
                    }
                }
            }
            // Print map with reachable node info
            for (MapItem item : map) {
                bool unreachable = false;
                for (PeerNode p : myHost.getNeighbors()) {
                    if (item.getId() == p.getId() && !p.getReachable()) {
                        unreachable = true;
                    }
                }
                if (unreachable) {
                    continue;
                }
                item.print();
                cout << endl;
            }
        }
        else if (keyword == "uuid") {
            cout << "uuid: " + myHost.getUuid() << endl;
        }
        else if (keyword == "rank") {
            vector<pair<string, int>> rankVector = rankByMetric(map, myHost.getUuid());
            for (int i = 0; i < rankVector.size() - 1; i++) {
                cout << rankVector[i].first << ":" << rankVector[i].second << ", ";
            }
            int index = rankVector.size() - 1;
            cout << rankVector[index].first << ":" << rankVector[index].second << endl;
        }
        else {
            cout << "Invalid command!" << endl;
        }
    }


    return 0;
}



















