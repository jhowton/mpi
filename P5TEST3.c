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
    int vals[10];
    int sumvals[10];
    MPI_Op rmbsumop;

    MPI_Init(NULL,NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    for (i=0; i < 10; i++)
    {
        vals[i] = (myrank*i) + 100 + i;
    }
    printf("%d: vals[0] is %d\n",myrank,vals[0]);
    printf("%d: vals[9] is %d\n",myrank,vals[9]);
    
    MPI_Op_create( rmb_sum, 1, &rmbsumop );

    MPI_Reduce( vals, sumvals, 10, MPI_INT, rmbsumop, 0, MPI_COMM_WORLD );

    if (myrank == 0)
        for (i=0; i < 10; i++)
            printf("%d: answer %d is %d\n",myrank,i,sumvals[i]);

    MPI_Finalize();
}
