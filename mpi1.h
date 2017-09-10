#ifndef MPI_H
#define MPI_H

typedef int MPI_Datatype;
#define MPI_CHAR           ((MPI_Datatype)0x4c000101)
#define MPI_INT            ((MPI_Datatype)0x4c000405)

/* Communicators */
typedef int MPI_Comm;
#define MPI_COMM_WORLD ((MPI_Comm)0x44000000)

/* MPI request opjects */
typedef int MPI_Request;

typedef int MPI_Aint;

/* Collective operations */
typedef int MPI_Op;

#define MPI_SUM     (MPI_Op)(0x00000000)
typedef void (MPI_User_function)(void*, void*, int*, MPI_Datatype*);

typedef struct MPI_Status {
    int count;
    int cancelled;
    int MPI_SOURCE;
    int MPI_TAG;
    int MPI_ERROR;
} MPI_Status;

/* for info */
typedef int MPI_Info;
#define MPI_INFO_NULL         ((MPI_Info)0x1c000000)
#define MPI_MAX_INFO_KEY       255
#define MPI_MAX_INFO_VAL      1024

/* MPI's error classes */
#define MPI_SUCCESS          0      /* Successful return code */
/* Communication argument parameters */
#define MPI_ERR_BUFFER       1      /* Invalid buffer pointer */
#define MPI_ERR_COUNT        2      /* Invalid count argument */
#define MPI_ERR_TYPE         3      /* Invalid datatype argument */
#define MPI_ERR_TAG          4      /* Invalid tag argument */
#define MPI_ERR_COMM         5      /* Invalid communicator */
#define MPI_ERR_RANK         6      /* Invalid rank */
#define MPI_ERR_ROOT         7      /* Invalid root */
#define MPI_ERR_TRUNCATE    14      /* Message truncated on receive */

/*Any tag and Any source*/
#define MPI_ANY_SOURCE      (-30)
#define MPI_ANY_TAG      (-30)

/* Prototypes */
extern "C"
{
int MPI_TESTING();
int MPI_Init(int * argc, char *** argv);
int MPI_Comm_size(MPI_Comm comm, int * size);
int MPI_Comm_rank(MPI_Comm comm, int * rank);
int MPI_Finalize();
int MPI_Barrier(MPI_Comm comm);
int MPI_Send(const void* buf, int count, MPI_Datatype datatype, 
    int dest, int tag, MPI_Comm comm);
int MPI_Ssend(const void* buf, int count, MPI_Datatype datatype, 
    int dest, int tag, MPI_Comm comm);
int MPI_Recv(void* buf, int count, MPI_Datatype datatype, 
    int source, int tag, MPI_Comm comm, MPI_Status *status);
double MPI_Wtime();
int MPI_Gather(void* , int, MPI_Datatype, void*, int, MPI_Datatype, int, MPI_Comm); 
int MPI_Bcast(void*, int, MPI_Datatype, int, MPI_Comm );
int MPI_Comm_dup(MPI_Comm, MPI_Comm *);
int MPI_Isend(void*, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int MPI_Irecv(void*, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int MPI_Test(MPI_Request *, int *, MPI_Status *);
int MPI_Wait(MPI_Request *, MPI_Status *);
int MPI_Reduce(void* , void*, int, MPI_Datatype, MPI_Op, int, MPI_Comm);
int MPI_Op_create(MPI_User_function *, int, MPI_Op *);
}

#endif
