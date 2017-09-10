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
        n = 99;
        rc = MPI_Send(&n,1,MPI_INT,1,17,MPI_COMM_WORLD);
	n = 0;
	rc = MPI_Recv(&n,1,MPI_INT,1,22,MPI_COMM_WORLD,&status);
	printf("%d:  recvrc=%d  n=%d  from=%d  tag=%d\n",
	       myrank, rc, n, status.MPI_SOURCE, status.MPI_TAG);
    }
    else
    {
	n = 0;
	rc = MPI_Recv(&n,1,MPI_INT,0,17,MPI_COMM_WORLD,&status);
	printf("%d:  recvrc=%d  n=%d  from=%d  tag=%d\n",
	       myrank, rc, n, status.MPI_SOURCE, status.MPI_TAG);
        n = 88;
        rc = MPI_Send(&n,1,MPI_INT,0,22,MPI_COMM_WORLD);
    }

    MPI_Finalize();
}
