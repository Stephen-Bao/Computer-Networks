#include <iostream>
#include <string>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

using namespace std;

void sendMessage(string hostname, int port, string message) {
    int sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sfd == -1) {
        perror("ERROR opening socket");
        exit(-1);
    }

    struct hostent *server = gethostbyname(hostname.c_str());
    if (server == NULL) {
        cerr << "No such host as " << hostname << endl;
        exit(-1);
    }

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;   
    bcopy((char *)server->h_addr, (char *)&addr.sin_addr.s_addr, server->h_length);
    addr.sin_port = htons(port);

    const char *msg = message.c_str();
    sendto(sfd, msg, strlen(msg), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr));

    close(sfd);
}

















