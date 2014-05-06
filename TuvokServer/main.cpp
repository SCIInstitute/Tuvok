#include <iostream>
//#include <omp.h>
#include <mpi.h>
#include "tvkserver.h"

static int srcRank = 0;

int main(int argc, char *argv[])
{
    int rank, numprocs;
    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    //printf("Test from process %d of %d\n", rank, numprocs);

    TvkServer *server = NULL;
    if(rank == srcRank) {
        server = new TvkServer();
    }

    int shouldShutdown = false;
    while(!shouldShutdown) {

        int clientPort;
        if(server != NULL)
            clientPort = server->waitAndAccept();

        int shouldDisconnect = false;
        while(!shouldDisconnect) {
            ParameterWrapper *params = NULL;
            if(server != NULL) {
                 params = server->processNextCommand(clientPort);
                 if (params == NULL)
                    shouldDisconnect = true;
                 else if(params->code == nds_SHUTDOWN) {
                    shouldDisconnect    = true;
                    shouldShutdown      = true;
                 }
            }
            MPI_Bcast(&shouldDisconnect, 1, MPI_INT, srcRank, MPI_COMM_WORLD);
            if (shouldDisconnect)
                break;

            //Sync params between MPI Processes
            {
                int8_t code;
                if(rank == srcRank) {
                    code = params->code;
                }

                MPI_Bcast(&code, 1, MPI_INT8_T, srcRank, MPI_COMM_WORLD);

                if(rank != srcRank) {
                    params = ParamFactory::createFrom((NetDSCommandCode)code, -1);
                }
                MPI_Barrier(MPI_COMM_WORLD);

                params->mpi_sync(rank, srcRank);
            }

            //Perform request and return answer
            params->perform(clientPort, NULL);

            delete params;
            params = NULL;

        }

        if(server != NULL) {
            server->disconnect(clientPort);
        }

        MPI_Bcast(&shouldShutdown, 1, MPI_INT, srcRank, MPI_COMM_WORLD);
    }
    if(rank == srcRank)
        printf("Server received shutdown command!\n");

    MPI_Finalize();
}
