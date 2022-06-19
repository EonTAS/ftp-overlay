/*
    NXGallery for Nintendo Switch
    Made with love by Jonathan Verbeek (jverbeek.de)

    MIT License

    Copyright (c) 2020-2022 Jonathan Verbeek

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include "server.hpp"
#include <switch.h>

typedef struct
{
    char *name, *value;
} header_t;
static header_t reqhdr[17];

using namespace nxgallery::core;

CWebServer::CWebServer(int port)
{
    // Store the port
    this->port = port;

    // We're not running yet
    isRunning = false;

    // We won't initialize the web server here just now,
    // the caller can do that by calling CWebServer::Start
}

void CWebServer::Start()
{
    // If we're already running, don't try to start again
    if (isRunning)
        return;

    // Construct a socket address where we want to listen for requests
    static struct sockaddr_in serv_addr;
    serv_addr.sin_addr.s_addr = INADDR_ANY; // The Switch'es IP address
    serv_addr.sin_port = htons(port);
    serv_addr.sin_family = PF_INET; // The Switch only supports AF_INET and AF_ROUTE: https://switchbrew.org/wiki/Sockets_services#Socket

    // Create a new STREAM IPv4 socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        printf("Failed to create a web server socket: %d\n", errno);
        return;
    }

    // Set a relatively short timeout for recv() calls, see CWebServer::ServeRequest for more info why
    struct timeval recvTimeout;
    recvTimeout.tv_sec = 1;
    recvTimeout.tv_usec = 0;
    setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&recvTimeout, sizeof(recvTimeout));

    // Enable address and port reusing
    int yes = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));

    // Bind the just-created socket to the address
    if (bind(serverSocket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("Failed to bind web server socket: %d\n", errno);
        return;
    }

    // Start listening to the socket with 5 maximum parallel connections
    if (listen(serverSocket, 10) < 0)
    {
        printf("Failed to listen to the web server socket: %d\n", errno);
        return;
    }

    // Now we're running
    isRunning = true;
}

void CWebServer::GetAddress(char *buffer)
{
    static struct sockaddr_in serv_addr;
    serv_addr.sin_addr.s_addr = gethostid();
    sprintf(buffer, "http://%s:%d/", inet_ntoa(serv_addr.sin_addr), port);
}

void CWebServer::AddMountPoint(const char *path)
{
    // Add it to the mountPoints vector
    mountPoints.push_back(path);
}

void CWebServer::ServeLoop()
{
    // Asynchronous / event-driven loop using poll
    // More here: http://man7.org/linux/man-pages/man2/poll.2.html

    // Do not try to serve anything when the server isn't running
    if (!isRunning)
        return;

    // Will hold the data returned from poll()
    struct pollfd pollInfo;
    pollInfo.fd = serverSocket; // Listen to events from the server socket we opened
    pollInfo.events = POLLIN;   // Only react on incoming events
    pollInfo.revents = 0;       // Gets filled with events later

    // Poll for new events
    if (poll(&pollInfo, 1, 0) > 0)
    {
        // There was an incoming event on the server socket
        if (pollInfo.revents & POLLIN)
        {
            // Will hold data about the new connection
            struct sockaddr_in clientAddress;
            socklen_t addrLen = sizeof(clientAddress);

            // Accept the incoming connection
            int acceptedConnection = accept(serverSocket, (struct sockaddr *)&clientAddress, &addrLen);
            if (acceptedConnection > 0)
            {
#ifdef __DEBUG__
                printf("Accepted connection from %s:%u\n", inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));
#endif

                // Serve ("answer") the request which is waiting at the file descriptor accept() returned
                ServeRequest(acceptedConnection, acceptedConnection, mountPoints);

                // After we served the request, close the connection
                if (close(acceptedConnection) < 0)
                {
#ifdef __DEBUG__
                    printf("Error closing connection %d: %d %s\n", acceptedConnection, errno, strerror(errno));
#endif
                }
            }
            else if (errno == ECONNABORTED && isRunning)
            {
                // Make sure this only happens once
                isRunning = false;

                // Shutdown and close a socket if still open
                shutdown(serverSocket, SHUT_RDWR);
                close(serverSocket);

                // Start the sever again
                Start();
            }
        }
    }
}

void CWebServer::ServeRequest(int in, int out, std::vector<const char *> mountPoints)
{
    char inBuffer[8192] = {0};
    char outBuffer[8192] = {0};

    int rcvd = recv(in, inBuffer, 8192, 0);

    if (rcvd < 0)
    {
        // error
    }
    else if (rcvd == 0)
    {
        // client disconnected
    }
    else
    {
        inBuffer[rcvd] = '\0';
        char *method, *uri, *prot;

        method = strtok(inBuffer, " \t\r\n");
        uri = strtok(NULL, " \t");
        prot = strtok(NULL, " \t\r\n");

        char *queries;
        if ((queries = strchr(uri, '?')))
        {
            *queries++ = '\0'; // split URI
        }
        else
        {
            queries = uri - 1; // use an empty string
        }
        header_t *h = reqhdr;
        char *t, *t2;
        t = NULL;
        while (h < reqhdr + 16)
        {
            char *k, *v;
            k = strtok(NULL, "\r\n: \t");
            if (!k)
                break;
            v = strtok(NULL, "\r\n");
            while (*v && *v == ' ')
                v++;
            h->name = k;
            h->value = v;
            h++;
            fprintf(stderr, "[H] %s: %s\n", k, v);
            t = v + 1 + strlen(v);
            if (t[1] == '\r' && t[2] == '\n')
                break;
        }
        t++;                                   // now the *t shall be the beginning of user payload
        t2 = request_header("Content-Length"); // and the related header if there is
        char *payload = t;
        int payload_size = t2 ? atol(t2) : (rcvd - (t - inBuffer));

        if (strcmp(method, "GET") == 0)
        {
            if (strlen(uri) > 0)
            {
                // Map the requested URL to the path where we serve web assets
                // If the request didn't specify a file but only a "/", we route
                // to index.html aswell.
                if (strcmp(uri, "/") == 0)
                {
                    strcpy(uri, "/index.html");
                }

                char path[50];

                // Find the file in one of the mounted folders
                for (const char *mountPoint : mountPoints)
                {
                    // Map the path of the requested file to this mount point
                    sprintf(path, "%s%s", mountPoint, uri);

                    // Stat to see if the file exists at that mountpoint
                    struct stat fileStat;
                    if (stat(path, &fileStat) == 0)
                    {
                        // If so, break the loop as we have found our file path for the requested file
                        break;
                    }
                }

                // Open the requested file
                int fileToServe = open(path, O_RDONLY);

                // Check if we file we tried to open existed
                if (fileToServe > 0)
                {
                    // The file exists, so lets send an 200 OK to notify the client
                    // that we will continue to send HTML data now
                    sprintf(outBuffer, "HTTP/1.0 200 OK\n\n"); // \nContent-Type: text/html
                    send(out, outBuffer, strlen(outBuffer), 0);

                    // Read the data from the file requested until there is no data left to read
                    int bytesReceived;
                    do
                    {
                        bytesReceived = read(fileToServe, outBuffer, sizeof(outBuffer));

                        // Send it out to the client
                        send(out, outBuffer, bytesReceived, 0);
                    } while (bytesReceived > 0);

                    // We successfully read the file to serve, so close it
                    close(fileToServe);
                }
                else
                {
                    // Return a 404
                    sprintf(outBuffer, "HTTP/1.0 404 Not Found\n\n");
                    send(out, outBuffer, strlen(outBuffer), 0);
                }
            }
        }
        else if (strcmp(method, "POST") == 0)
        {
            sprintf(outBuffer, "HTTP/1.0 200 OK\n\n"); // \nContent-Type: text/html
            send(out, outBuffer, strlen(outBuffer), 0);
            sprintf(outBuffer, "%s : %s q's %s prot %s rest %s", method, uri, queries, prot, payload);
            send(out, outBuffer, strlen(outBuffer), 0);
        }
        else
        {
            // There was no URL given, likely that another request was issued.
            // Return a 501
            sprintf(outBuffer, "HTTP/1.0 501 Method Not Implemented\n\n");
            send(out, outBuffer, strlen(outBuffer), 0);
        }
    }
}
char *CWebServer::request_header(const char *name)
{
    header_t *h = reqhdr;
    while (h->name)
    {
        if (strcmp(h->name, name) == 0)
            return h->value;
        h++;
    }
    return NULL;
}

void CWebServer::Stop()
{
    // Not running anymore
    isRunning = false;

    // Shutdown the server socket, then close it
    shutdown(serverSocket, SHUT_RDWR);
    close(serverSocket);
}