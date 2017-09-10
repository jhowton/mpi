#include <stdio.h>
#include "mpi.h"
#include <math.h>

int main(int argc, char *argv[])
{
    int i, j, n, myrank, numranks, rc;
    MPI_Status status;
    char hostname[128];

    rc = MPI_Init(NULL,NULL);
    rc = MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
    rc = MPI_Comm_size(MPI_COMM_WORLD,&numranks);

    gethostname(hostname,128);

    printf("%d:  size %d  onhost %s\n",myrank,numranks,hostname);

    MPI_Finalize();
}
