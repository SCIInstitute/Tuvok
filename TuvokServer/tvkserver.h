#ifndef TVKSERVER_H
#define TVKSERVER_H

#include "parameterwrapper.h"

static const unsigned short defaultPort = 4445;

class TvkServer {
    int listen_s;
    int conn_s;

    bool magicCheck(int socket);

public:
    TvkServer(unsigned short port = defaultPort);
    int waitAndAccept();
    void disconnect(int socket);
    ParameterWrapper* processNextCommand(int socket);
};

#endif // TVKSERVER_H
