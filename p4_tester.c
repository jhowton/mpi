#include "mpi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define BALL_VEC_SZ 100000

int Wtime();
int NonBlock();

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
    else if (strcmp(argv[1],"nonblock") == 0)
        NonBlock();      // nranks 2
    else
    {
        printf("INVALID ARG TO PGM\n");
        exit(-1);
    }
}


int NonBlock() //nonblocking send/recv
{
    int size;
    int rank;
    int home = 0;
    double stime;
    MPI_Status status;

    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    stime = MPI_Wtime();

    printf("Rank: %d\tSize:%d\n", rank, size);

    if (rank == 0)
    {
        int flag;

        MPI_Request req1, req2, req3;
        MPI_Status st1, st2, st3;

        int buf1 = 1111;
        int buf2 = 2222;
        int buf3 = 3333;

        MPI_Isend(&buf1, 1, MPI_INT, 1, 50, MPI_COMM_WORLD, &req1);
        MPI_Isend(&buf2, 1, MPI_INT, 1, 60, MPI_COMM_WORLD, &req2);
        MPI_Isend(&buf3, 1, MPI_INT, 1, 50, MPI_COMM_WORLD, &req3);


        do{
            MPI_Test(&req1, &flag, &st1);
        }
        while (!flag)
            ;

        printf("time in Test %f\n",MPI_Wtime()-stime);

        //MPI_Wait(&req1, &st1);
        MPI_Wait(&req2, &st2);
        MPI_Wait(&req3, &st3);

        fprintf(stderr,"I'm done with the first stage\n");
    }
    if (rank == 1)
    {
        int flag;

        MPI_Request req1, req2, req3;
        MPI_Status st1, st2, st3;

        int buf1, buf2, buf3;

        MPI_Irecv(&buf2, 1, MPI_INT, 0, 60, MPI_COMM_WORLD, &req2); //notice how 60 is out of order
        MPI_Irecv(&buf1, 1, MPI_INT, 0, 50, MPI_COMM_WORLD, &req1);
        MPI_Irecv(&buf3, 1, MPI_INT, 0, 50, MPI_COMM_WORLD, &req3);


        MPI_Wait(&req1, &st1);
        MPI_Wait(&req2, &st2);
        MPI_Wait(&req3, &st3);

        fprintf(stderr,"Stage 1 Received: %d %d %d\n", buf1, buf2, buf3);
    }
    sleep(1);

    //stage 2
    if (rank == 0)
    {
        int flag;

        MPI_Request req1, req2, req3;
        MPI_Status st1, st2, st3;

        int buf1 = 1111;
        int buf2 = 2222;
        int buf3 = 3333;

        MPI_Isend(&buf1, 1, MPI_INT, 1, 10, MPI_COMM_WORLD, &req1);
        MPI_Isend(&buf2, 1, MPI_INT, 1, 20, MPI_COMM_WORLD, &req2);
        MPI_Isend(&buf3, 1, MPI_INT, 1, 30, MPI_COMM_WORLD, &req3);

        MPI_Wait(&req1, &st1);
        MPI_Wait(&req2, &st2);
        MPI_Wait(&req3, &st3);

        fprintf(stderr,"I'm done with the second stage\n");
    }
    if (rank == 1)
    {
        int flag;

        MPI_Request req1, req2, req3;
        MPI_Status st1, st2, st3;

        int buf1, buf2, buf3;

        MPI_Irecv(&buf1, 1, MPI_INT, 0, 20, MPI_COMM_WORLD, &req1); //2222
        MPI_Irecv(&buf2, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &req2); // 1111
        MPI_Irecv(&buf3, 1, MPI_INT, 0, 30, MPI_COMM_WORLD, &req3); // 3333

        MPI_Wait(&req1, &st1);
        MPI_Wait(&req2, &st2);
        MPI_Wait(&req3, &st3);

        fprintf(stderr,"Stage 2 Received: %d %d %d\n", buf1, buf2, buf3); //2222 1111 3333
    }
    sleep(1);

    //stage 3
    if (rank == 0)
    {
        int flag;

        MPI_Request req1, req2, req3;
        MPI_Status st1, st2, st3;

        int buf1 = 1111;
        int buf2 = 2222;
        int buf3 = 3333;

        MPI_Isend(&buf1, 1, MPI_INT, 1, 10, MPI_COMM_WORLD, &req1);
        MPI_Isend(&buf2, 1, MPI_INT, 1, 20, MPI_COMM_WORLD, &req2);
        MPI_Send(&buf3, 1, MPI_INT, 1, 30, MPI_COMM_WORLD);

        MPI_Wait(&req1, &st1);
        MPI_Wait(&req2, &st2);

        fprintf(stderr,"I'm done with the third stage\n");
    }
    if (rank == 1)
    {
        int flag;

        MPI_Request req1, req2, req3;
        MPI_Status st1, st2, st3;

        int buf1, buf2, buf3;

        MPI_Irecv(&buf1, 1, MPI_INT, 0, 20, MPI_COMM_WORLD, &req1); //2222
        MPI_Irecv(&buf2, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &req2); // 1111
        MPI_Irecv(&buf3, 1, MPI_INT, 0, 30, MPI_COMM_WORLD, &req3); // 3333

        MPI_Wait(&req1, &st1);
        MPI_Wait(&req2, &st2);
        MPI_Wait(&req3, &st3);

        fprintf(stderr,"Stage 3 Received: %d %d %d\n", buf1, buf2, buf3); //2222 1111 3333
    }
    sleep(1);

    //stage 4
    if (rank == 0)
    {
        int flag;

        MPI_Request req1, req2, req3;
        MPI_Status st1, st2, st3;

        int buf1 = 1111;
        int buf2 = 2222;
        int buf3 = 3333;

        MPI_Isend(&buf1, 1, MPI_INT, 1, 10, MPI_COMM_WORLD, &req1);
        MPI_Isend(&buf2, 1, MPI_INT, 1, 20, MPI_COMM_WORLD, &req2);
        MPI_Send(&buf3, 1, MPI_INT, 1, 30, MPI_COMM_WORLD);

        MPI_Wait(&req1, &st1);
        MPI_Wait(&req2, &st2);

        fprintf(stderr,"I'm done with the fourth stage\n");
    }
    if (rank == 1)
    {
        int flag;

        MPI_Request req1, req2, req3;
        MPI_Status st1, st2, st3;

        int buf1, buf2, buf3;

        MPI_Irecv(&buf1, 1, MPI_INT, 0, 20, MPI_COMM_WORLD, &req1); //2222
        MPI_Irecv(&buf2, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &req2); // 1111
        MPI_Recv(&buf3, 1, MPI_INT, 0, 30, MPI_COMM_WORLD, &st3); // 3333

        MPI_Wait(&req1, &st1);
        MPI_Wait(&req2, &st2);

        fprintf(stderr,"Stage 4 Received: %d %d %d\n", buf1, buf2, buf3); //2222 1111 3333
    }
    sleep(1);

    printf("TIME %f\n\n",MPI_Wtime()-stime);

    MPI_Finalize();

    printf("SUCCESS\n");

    return 0;
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
