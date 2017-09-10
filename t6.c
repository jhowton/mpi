#include <stdio.h>
#include "mpi.h"
#include <math.h>

int main(int argc, char *argv[])
{
    int i, j, n, myrank, numranks, rc;
    MPI_Status status;
    char hostname[128];

    MPI_Init(NULL,NULL);
    MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
    MPI_Comm_size(MPI_COMM_WORLD,&numranks);

    gethostname(hostname,128);

    printf("%d:  size %d  onhost %s\n",myrank,numranks,hostname);

    if (myrank == 0)
    {
        sleep(3);
    }
    rc = MPI_Barrier(MPI_COMM_WORLD);
    printf("%d:  through the barrier, dude\n", myrank);

    MPI_Finalize();
}
