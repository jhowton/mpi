// but for some testing with mpich2, you can do:
//     mpicxx -o tester tester.c

#include "mpi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define BALL_VEC_SZ 100000

int Wtime();
int Gather();
int DupOnly();
int Bcast();
int Barrier();

int callhome_version = 0;

int main(int argc, char** argv)
{
    setbuf(stdout,NULL);

    if (argc < 2)
    {
        printf("USAGE: %s test_to_run\n");
        exit(-1);
    }
    if (strcmp(argv[1],"wtime") == 0)
        Wtime();  // nranks 8+
    else if (strcmp(argv[1],"gather") == 0)
        Gather();        // nranks 2, 8
    else if (strcmp(argv[1],"duponly") == 0)
        DupOnly();    // nranks 2, 8  // hangs; requires exactly 2 ranks
    else if (strcmp(argv[1],"bcast") == 0)
        Bcast();         // nranks 2, 8
    else if (strcmp(argv[1],"barrier") == 0)
        Barrier();       // nranks 2, 8
    else
    {
        printf("INVALID ARG TO PGM\n");
        exit(-1);
    }
}

int Gather() //gather
{
    int i, iter;
    MPI_Init(NULL, NULL);
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int home = 0;

    printf("Rank: %d\tSize:%d\n", rank, size);

    for (iter = 0; iter < size; iter++)
    {
        home = iter;

        int sbuf = (100 * iter) + rank;

        if (rank == home)
        {
            int rbuf[size * sizeof(int)];
            printf("iter %d\n", iter);

            MPI_Gather(&sbuf, 1, MPI_INT, &rbuf, 1, MPI_INT, home, MPI_COMM_WORLD);

            for (i = 0; i < size; i++)
            {
                    int test = rbuf[i];


                    //printf("**************************From %d got %d   iter %d\n", i, buf,iter);
                    if (test != (100 * iter) + i)
                    {
                            printf("EPIC FAIL EPIC FAIL EPIC FAIL EPIC FAIL EPIC FAIL!!!!!!\n");
                            printf("*****************From %d got %d   iter %d\n", i, test, iter);
                            exit(1);
                    }
            }
        } else {
            MPI_Gather(&sbuf, 1, MPI_INT, NULL, 0, MPI_INT, home, MPI_COMM_WORLD);
        }
    }

    MPI_Finalize();
    printf("SUCCESS\n");
    return 0;
}

int DupOnly()
{
    MPI_Init(NULL, NULL);
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int com1;
    int com2;
    MPI_Status st;
    int ball;

    alarm(4);  // DIE after 4 seconds

    if (size > 2)
    {
        printf("** RUN WITH exactly 2 ranks\n");
        exit(-1);
    }
    printf("prints two values by rank 1 and then hangs\n");
    MPI_Comm_dup(MPI_COMM_WORLD, &com2);
    if (rank == 0)
    {
        ball = 1;
        MPI_Send(&ball, 1, MPI_INT, 1, 999, MPI_COMM_WORLD);
        printf("HERE\n");
        ball = 2;
        MPI_Send(&ball, 1, MPI_INT, 1, 999, com2);
        printf("HERE\n");
        ball = 3;
        MPI_Send(&ball, 1, MPI_INT, 1, 999, MPI_COMM_WORLD);
        printf("HERE\n");
    }
    else
    {
        MPI_Recv(&ball, 1, MPI_INT, MPI_ANY_SOURCE, 999, MPI_COMM_WORLD, &st);
        printf("RECVD BY 1:  %d\n",ball);
        MPI_Recv(&ball, 1, MPI_INT, MPI_ANY_SOURCE, 999, com2, &st);
        printf("RECVD BY 1:  %d\n",ball);
        MPI_Recv(&ball, 1, MPI_INT, MPI_ANY_SOURCE, 999, com2, &st);
        printf("RECVD BY 1:  %d\n",ball);
    }
    MPI_Finalize();
}

int Wtime()
{
    MPI_Init(NULL, NULL);
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int com1;
    int com2;
    double stime;

    stime = MPI_Wtime();
    while ((MPI_Wtime() - stime) < 4.567)
        ;
    printf("%d: %f\n",rank,MPI_Wtime()-stime);

    MPI_Finalize();
    return 0;
}

int Bcast() //bcast
{
    int size, rank, iter, ball;

    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    for (iter = 0; iter < size; iter++)
    {
        ball = (100 * iter) + rank;

        if (rank == iter)
        {
            printf("Broadcasting ball: %d - iter %d\n", ball, iter);
            MPI_Bcast(&ball, 1, MPI_INT, iter, MPI_COMM_WORLD);
        } else {
            MPI_Bcast(&ball, 1, MPI_INT, iter, MPI_COMM_WORLD);
            printf("Got the ball! Value: %d\n", ball);
        }
    }

    printf("all done\n");
    MPI_Finalize();
    printf("SUCCESS\n");
    return 0;
}

int Barrier() //barrier
{
    int size, rank, iter;

    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    for (iter = 0; iter < size; iter++)
    {

        if (rank == iter)
        {
            printf("Before barrier, sleeping iter %d\n", iter);
            usleep(100*1000);
            printf("Waking up! %d\n", iter);
            MPI_Barrier(MPI_COMM_WORLD);
            
        } else {
            //printf("waiting\n");
            MPI_Barrier(MPI_COMM_WORLD);
        }
        printf("barrier done iter %d\n", iter);
    }

    MPI_Finalize();
    printf("SUCCESS\n");
    return 0;
}
