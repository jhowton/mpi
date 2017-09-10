#include <stdio.h>
#include "mpi.h"
#include <math.h>

int main(int argc, char *argv[])
{
    int i, j, n, myrank, numranks, rc, partner_rank;
    MPI_Status status;
    char hostname[128];

    MPI_Init(NULL,NULL);
    MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
    MPI_Comm_size(MPI_COMM_WORLD,&numranks);

    gethostname(hostname,128);

    printf("%d:  size %d  onhost %s\n",myrank,numranks,hostname);

    partner_rank = numranks - myrank - 1;

    if (myrank < partner_rank)
    {
	n = 0;
	rc = MPI_Recv(&n,1,MPI_INT,MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&status);
	printf("%d:  recvrc=%d  n=%d  from=%d  tag=%d\n",
	       myrank, rc, n, status.MPI_SOURCE, status.MPI_TAG);
        if (status.MPI_SOURCE != partner_rank)
        {
            printf("OOPS wrong sender %d  expected %d\n",status.MPI_SOURCE,partner_rank);
        }
        n = 88;
        rc = MPI_Send(&n,1,MPI_INT,partner_rank,44,MPI_COMM_WORLD);
    }
    else
    {
        n = 99;
        rc = MPI_Send(&n,1,MPI_INT,partner_rank,44,MPI_COMM_WORLD);
	n = 0;
	rc = MPI_Recv(&n,1,MPI_INT,MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&status);
	printf("%d:  recvrc=%d  n=%d  from=%d  tag=%d\n",
	       myrank, rc, n, status.MPI_SOURCE, status.MPI_TAG);
        if (status.MPI_SOURCE != partner_rank)
        {
            printf("OOPS wrong sender %d  expected %d\n",status.MPI_SOURCE,partner_rank);
        }
    }

    MPI_Finalize();
}
