#include <stdio.h>

#include "mpi.h"


void rmb_sum(void *inp, void *inoutp, int *len, MPI_Datatype *dptr)
{
    int i;
    int r;
    int *in    = (int *)inp;
    int *inout = (int *)inoutp;

    printf("RMB inside rmb_sum\n");
    for (i=0; i < *len; i++)
    {
        r = *in + *inout;
        *inout = r;
        in++;
        inout++;
    }
}

int main()
{
    int i, myrank;
    int val;
    int sumvals = 0;
    MPI_Op rmbsumop;

    MPI_Init(NULL,NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    val = myrank + 100;
    printf("%d: val is %d\n",myrank,val);
    
    MPI_Op_create( rmb_sum, 1, &rmbsumop );

    MPI_Reduce( &val, &sumvals, 1, MPI_INT, rmbsumop, 0, MPI_COMM_WORLD );

    if (myrank == 0)
        printf("%d: sumvals is %d\n",myrank,sumvals);

    MPI_Finalize();
}
