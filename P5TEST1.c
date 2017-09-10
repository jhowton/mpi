#include <stdio.h>
#include "mpi.h"


int main()
{
    int i, myrank;
    int val;
    int sumvals = 0;

    MPI_Init(NULL,NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    val = myrank + 100;
    
    MPI_Reduce( &val, &sumvals, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD );

    if (myrank == 0)
        printf("%d: answer is %d\n",myrank,sumvals);

    MPI_Finalize();
}
