#include <iostream>
#include <string>
#include <vector>

#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

using namespace std;

vector<string> recvMessage(int port) {

    vector<string> ret;
    int sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sfd == -1) {
        cerr << "ERROR opening socket" << endl;
        exit(-1);
    }

    int reuse = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) == -1) {
        cerr << "ERROR binding" << endl;
        exit(-1);
    } 

    char buf[1024] = {0};
    struct sockaddr_in fromaddr;
    bzero(&fromaddr, sizeof(fromaddr));
    int fromaddrlen = sizeof(struct sockaddr);
    recvfrom(sfd, buf, sizeof(buf), 0, (struct sockaddr *)&fromaddr, (socklen_t *)&fromaddrlen);

    // Get the sender's hostname
    struct hostent *hostp;
    hostp = gethostbyaddr((const char *)&fromaddr.sin_addr.s_addr, sizeof(fromaddr.sin_addr.s_addr), AF_INET);
    if (hostp == nullptr) {
        cerr << "ERROR on gethostbyaddr" << endl;
        exit(-1);
    }

    ret.push_back(buf);
    ret.push_back(hostp->h_name);

    return ret;

}











