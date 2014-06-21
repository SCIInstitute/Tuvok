#ifndef TVKSERVER_H
#define TVKSERVER_H

#include "../IO/sockethelper/parameterwrapper.h"

static const unsigned short defaultPort = 4445;
static const unsigned short defaultBPort = 4446;

class TvkServer {
    int listen_a; //For regular request+reply model
    int listen_b; //For sending a stream of data to the client
    int conn_a;
    int conn_b;

public:
    TvkServer(unsigned short port = defaultPort, unsigned short portB = defaultBPort);
    bool waitAndAccept();
    void disconnect(int socket);
    ParameterWrapper* processNextCommand(int socket);

    int getRequestSocket();
    int getBatchSocket();
};

#endif // TVKSERVER_H
