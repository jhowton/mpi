#include "mpi.h"
#include <cstdio>

using namespace std;

int main(int argc, char * argv[]){
    setbuf(stdout, NULL);
    printf("In Main()\n");
    MPI_Init(&argc, &argv);
    int myrank, sz;
    int val;
    MPI_Status status;

    int rc = MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    rc = MPI_Comm_size(MPI_COMM_WORLD, &sz);

    if(myrank == 0){
        val = 99;
        MPI_Send((void *)&val, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
    }else if(myrank == 1){
        val = 0;
        printf("Starting val for rank %d is %d\n", myrank, val);

        MPI_Recv((void *)&val, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
        printf("New val for rank %d is %d\n", myrank, val);
        
    }


    MPI_Finalize();
    printf("%d: Exiting...\n", myrank);

    return 0;
}
