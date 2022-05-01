#include <iostream>
#include <string>
#include <sstream>
#include <ctime>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

using namespace std;

string getTimestamp(time_t t)
{
    tm *gmtm = gmtime(&t);
    char buffer[80];

    strftime(buffer, 80, "%a, %d %b %Y %H:%M:%S GMT", gmtm);

    return string(buffer);
}

int main(int argc, char *argv[])
{
    const int MAX_CHUNKSIZE = 5 * (1 << 20); // 5MB

    // Create a socket
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1)
    {
        cerr << "socket" << endl;
        return -1;
    }

    // Set sockaddr_in
    struct sockaddr_in myAddr;
    bzero(&myAddr, sizeof(struct sockaddr));
    myAddr.sin_family = AF_INET;
    myAddr.sin_port = htons(8888);
    myAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Setsockopt
    int optval = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(optval));

    // Bind
    if (bind(sfd, (struct sockaddr *)&myAddr, sizeof(struct sockaddr)) == -1)
    {
        cerr << "bind" << endl;
        close(sfd);
        return -1;
    }

    // Listen
    if (listen(sfd, 10) == -1)
    {
        cerr << "listen" << endl;
        close(sfd);
        return -1;
    }

    // Accept
    struct sockaddr_in clientAddr;
    bzero(&clientAddr, sizeof(struct sockaddr));
    int addrlen = sizeof(struct sockaddr);
    int connFd = accept(sfd, (struct sockaddr *)&clientAddr, (socklen_t *)&addrlen);
    if (connFd == -1)
    {
        cerr << "connect" << endl;
        close(sfd);
        return -1;
    }

    // Set sending buffer size as 100kB
    socklen_t sendbuflen = 0;
    socklen_t len = sizeof(sendbuflen);
    getsockopt(connFd, SOL_SOCKET, SO_SNDBUF, (void*)&sendbuflen, &len);
    printf("default, sendbuf:%d\n", sendbuflen);
    sendbuflen = 102400;
    setsockopt(connFd, SOL_SOCKET, SO_SNDBUF, (void*)&sendbuflen, len);
    getsockopt(connFd, SOL_SOCKET, SO_SNDBUF, (void*)&sendbuflen, &len);
    printf("now, sendbuf:%d\n\n", sendbuflen);

    while (1)
    {
        // Receive message
        char buf[1024] = {0};
        int ret = recv(connFd, buf, sizeof(buf), 0);
        if (ret == 0)
        {
            cout << "Client disconnect!" << endl;
            break;
        }
        else if (ret == -1)
        {
            cerr << "recv" << endl;
            cerr << "errno = " << errno << ", " << strerror(errno) << endl;
            // return -1;
            break;
        }
        cout << "HTTP REQUEST:" << endl << buf;

        // Parse http request
        string request(buf);
        string requestLine, method, path;
        stringstream requestStream(request);
        getline(requestStream, requestLine);
        stringstream requestLineStream(requestLine);
        requestLineStream >> method >> path;

        string headerLine;
        bool rangeFlag = false;
        string startPosition;
        while (getline(requestStream, headerLine))
        {
            stringstream headerLineStream(headerLine);
            string key;
            headerLineStream >> key;
            if (key == "Range:")
            {
                rangeFlag = true;
                string rangeContent;
                headerLineStream >> rangeContent;
                startPosition = rangeContent.substr(6, rangeContent.size() - 7);
                break;
            }
        }

        path = "./content" + path;
        string response;
        int fd = open(path.c_str(), O_RDWR);

        // Can not find content
        if (fd == -1)
        {
            response.append("HTTP/1.1 404 Not Found\r\n");
            response.append("Date: ").append(getTimestamp(time(0))).append("\r\n");
            response.append("Content-Length: 13\r\n");
            response.append("Conection: keep-alive\r\n");
            response.append("Content-Type: text/plain\r\n");
            response.append("\r\n");
            response.append("404 Not Found");
            send(connFd, response.c_str(), response.size(), 0);
            cout << "404 Not Found!" << endl << endl;

            continue;
        }

        // Get file size
        struct stat fileInfo;
        fstat(fd, &fileInfo);
        int size = fileInfo.st_size;

        // Handle normal GET method
        if (method == "GET" && !rangeFlag)
        {
            // Send the whole file
            if (size <= MAX_CHUNKSIZE)
            {
                response.append("HTTP/1.1 200 OK\r\n");
                response.append("Date: ").append(getTimestamp(time(0))).append("\r\n");
                response.append("Content-Length: ").append(to_string(size)).append("\r\n");
                response.append("Connection: Keep-Alive\r\n");
                if (path == "/favicon.ico") {
                    response.append("Content-Type: image/ico\r\n");
                }
                response.append("\r\n");

                bzero(buf, sizeof(buf));
                int dataLen = 0;
                // Send response header
                send(connFd, response.c_str(), response.size(), 0);
                // Send response body (file)
                while (dataLen = read(fd, buf, sizeof(buf)))
                {
                    send(connFd, buf, dataLen, 0);
                    bzero(buf, sizeof(buf));
                }

                cout << "200 OK Response sent!" << endl << endl;
            }
            // If file size is too large, send partial content
            else
            {
                // Get file last-modified info
                time_t lastModTime = fileInfo.st_mtime;
                string lastModTimestamp = getTimestamp(lastModTime);

                response.append("HTTP/1.1 206 Partial Content\r\n");
                response.append("Date: ").append(getTimestamp(time(0))).append("\r\n");
                response.append("Connection: Keep-Alive\r\n");
                response.append("Last-Modified: ").append(lastModTimestamp).append("\r\n");
                response.append("Content-Range: bytes ").append("0-").append(to_string(MAX_CHUNKSIZE - 1)).append("/").append(to_string(size)).append("\r\n");
                response.append("Content-Length: ")
                    .append(to_string(MAX_CHUNKSIZE))
                    .append("\r\n");
                response.append("\r\n");
                send(connFd, response.c_str(), response.size(), 0);

                int bytesLeft = MAX_CHUNKSIZE;
                int dataLen = 0;
                while (dataLen = read(fd, buf, sizeof(buf)))
                {
                    int ret = send(connFd, buf, dataLen, 0);
                    if (ret == -1)
                    {
                        cerr << "send" << endl;
                        cerr << "errno = " << errno << ", " << strerror(errno) << endl;
                        return -1;
                    }
                    bzero(buf, sizeof(buf));
                    bytesLeft -= dataLen;
                    if (bytesLeft < sizeof(buf))
                    {
                        break;
                    }
                }
                read(fd, buf, bytesLeft);
                send(connFd, buf, bytesLeft, 0);
                bzero(buf, sizeof(buf));

                cout << "206 Partial Content Response sent!" << endl << endl;
            }
        }
        // Handle GET with Range header
        else if (method == "GET" && rangeFlag)
        {
            lseek(fd, stoi(startPosition), SEEK_SET);

            int bytesLeft = size - stoi(startPosition);
            cout << "Bytes left = " << bytesLeft << endl;
            time_t lastModTime = fileInfo.st_mtime;
            string lastModTimestamp = getTimestamp(lastModTime);
            response.append("HTTP/1.1 206 Partial Content\r\n");
            response.append("Date: ").append(getTimestamp(time(0))).append("\r\n");
            response.append("Conection: Keep-Alive\r\n");
            response.append("Last-Modified: ").append(lastModTimestamp).append("\r\n");
            response.append("Content-Range: bytes ").append(startPosition).append("-");

            if (bytesLeft <= MAX_CHUNKSIZE) {

                cout << "Sending all remaining file..." << endl;

                response.append(to_string(size - 1)).append("/").append(to_string(size)).append("\r\n");
                response.append("Content-Length: ").append(to_string(bytesLeft)).append("\r\n");
                response.append("\r\n");
                send(connFd, response.c_str(), response.size(), 0);

                int dataLen = 0;
                int count  = 0;
                while (dataLen = read(fd, buf, sizeof(buf)))
                {
                    send(connFd, buf, dataLen, 0);
                    bzero(buf, sizeof(buf));
                    count += dataLen;
                }
                cout << "Remaining file sent!" << endl;
                cout << "Sent bytes count = " << count << endl << endl;
            }
            else {

                cout << "Sending next chunk of file..." << endl;

                int startIndex = stoi(startPosition);
                response.append(to_string(startIndex + MAX_CHUNKSIZE - 1)).append("/").append(to_string(size)).append("\r\n");
                response.append("Content-Length: ").append(to_string(MAX_CHUNKSIZE)).append("\r\n");
                response.append("\r\n");
                send(connFd, response.c_str(), response.size(), 0);
                // cout << response;

                int bytesToSend = MAX_CHUNKSIZE;
                int dataLen = 0;
                while (dataLen = read(fd, buf, sizeof(buf)))
                {
                    int ret = send(connFd, buf, dataLen, 0);
                    if (ret == -1)
                    {
                        cerr << "send" << endl;
                        cerr << "errno = " << errno << ", " << strerror(errno) << endl;
                        // return -1;
                        break;
                    }
                    bzero(buf, sizeof(buf));
                    bytesToSend -= dataLen;
                    if (bytesToSend < sizeof(buf))
                    {
                        break;
                    }
                }
                read(fd, buf, bytesToSend);
                send(connFd, buf, bytesToSend, 0);
                bzero(buf, sizeof(buf));

                cout << "Next chunk sent!" << endl << endl;
            }
        }
    
        close(fd);
    }
    close(connFd);
}
