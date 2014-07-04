#include "tvkserver.h"
#include "sockhelp.h"
#include "DebugOut/debug.h"

#define DEBUG_PEER 1
#define DEBUG_SERVER 1

#define LISTEN_BACKLOG 50

DECLARE_CHANNEL(log);

// get port, IPv4 or IPv6:
in_port_t get_in_port(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return (((struct sockaddr_in*)sa)->sin_port);
    }

    return (((struct sockaddr_in6*)sa)->sin6_port);
}

int listenAndBind(unsigned short port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family     = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype   = SOCK_STREAM;  /* Datagram socket */
    hints.ai_flags      = AI_PASSIVE;   /* For wildcard IP address */
    hints.ai_protocol   = 0;            /* Any protocol */
    hints.ai_canonname  = NULL;
    hints.ai_addr       = NULL;
    hints.ai_next       = NULL;

    char portc[16];
    if(snprintf(portc, 15, "%hu", port) != 4) {
      fprintf(stderr, "Server setup: port conversion to string failed\n");
      exit(EXIT_FAILURE);
    }

    struct addrinfo *result;
    int s = getaddrinfo(NULL, portc, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "Server setup: getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
           Try each address until we successfully bind(2).
           If socket(2) (or bind(2)) fails, we (close the socket
           and) try the next address. */
    struct addrinfo *rp;

    int listen_s;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        listen_s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listen_s == -1)
            continue;

        // @TODO: Remove setsockopt again, only for debug reasons!
        int foo = 1;
        setsockopt(listen_s, SOL_SOCKET, SO_REUSEADDR, &foo, sizeof(int));

        if (bind(listen_s, rp->ai_addr, rp->ai_addrlen) == 0)
            break;                  /* Success */

        close(listen_s);
    }

    if (rp == NULL) {               /* No address succeeded */
        fprintf(stderr, "Could not bind to socket. Already in use?\n");
        exit(EXIT_FAILURE);
    }
    else if ( listen(listen_s, LISTEN_BACKLOG) < 0 ) {
        fprintf(stderr, "ECHOSERV: Error calling listen()\n");
        exit(EXIT_FAILURE);
    }

#if DEBUG_SERVER
    printf("listening on port %d\n", ntohs(get_in_port(rp->ai_addr)));
#endif

    freeaddrinfo(result); /* No longer needed */
    return listen_s;
}

TvkServer::TvkServer(unsigned short port, unsigned short portB) {
    listen_a = listenAndBind(port);
    listen_b = listenAndBind(portB);
    printf("Server created.\n");
}

bool magicCheck(int socket) {
    char buf[4];
    int byteCount = readFromSocket(socket, buf, sizeof(char)*4);

    if (byteCount < 4) {
        ERR(log, "Could not find magic on stream (not enough data)!");
        return false;
    }

    if (buf[0] != 'I' || buf[1] != 'V' || buf[2] != '3' || buf[3] != 'D') {
        ERR(log, "Could not find magic on stream!");
        return false;
    }

    return true;
}

int acceptOnListeningPort(int listen_s) {
    /*  Wait for a connection, then accept() it  */
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_size = sizeof(struct sockaddr_in);

    int conn_s;
    if ( (conn_s = accept(listen_s, (struct sockaddr *)&peer_addr, &peer_addr_size) ) < 0 ) {
        fprintf(stderr, "ECHOSERV: Error calling accept()\n");
        exit(EXIT_FAILURE);
    }

#if DEBUG_PEER
    printf("\nNew connection from ip: %s on port: %d\n",
           inet_ntoa(peer_addr.sin_addr),
           ntohs(get_in_port((struct sockaddr *)&peer_addr)));
#endif

    //Check for magic
    if(!magicCheck(conn_s)) {
        close(conn_s);
        exit(EXIT_FAILURE);
    }
    checkEndianness(conn_s);

    return conn_s;
}

bool TvkServer::waitAndAccept() {
    TRACE(log, "Waiting for a new client connection...");
    conn_a = acceptOnListeningPort(listen_a);
    conn_b = acceptOnListeningPort(listen_b);
    // @TODO: should also check for same address

    return true;
}

void TvkServer::disconnect(int socket) {
    /*  Close the connected socket*/
    if ( close(socket) < 0 ) {
        fprintf(stderr, "ECHOSERV: Error calling close()\n");
        exit(EXIT_FAILURE);
    }
    else
        printf("Client disconnected.\n");
}

ParameterWrapper* TvkServer::processNextCommand(int socket) {
    uint8_t cmd;
    if(!ru8(socket, &cmd))
        return NULL; //Should only happen if a connection error occurs, since reads are blocking
    else
        return ParamFactory::createFrom((NetDSCommandCode)cmd, socket);
}

int TvkServer::getRequestSocket() {
    return conn_a;
}

int TvkServer::getBatchSocket() {
    return conn_b;
}
