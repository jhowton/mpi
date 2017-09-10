# mpi
An mpi library written in C++ that is capable of running simple C based MPI programs

Example programs are provided and built using the makefile. Refer to makefile for building with
your own C program. 

The only supported datatypes are MPI_CHAR and MPI_INT.

The MPI_Reduce method only supports MPI_INT datatypes, and the only built-in aggregate function is MPI_SUM.
However, custom aggregate functions can be defined using MPI_Op_create.

All methods are based upon the MPICH implementation.

Available MPI methods:

* MPI_Init

* MPI_Comm_size

* MPI_Comm_rank

* MPI_Finalize

* MPI_Barrier

* MPI_Send

* MPI_Ssend

* MPI_Recv

* MPIWtime

* MPI_Gather

* MPI_Bcast

* MPI_Comm_dup

* MPI_Isend

* MPI_Irecv

* MPI_Test

* MPI_Wait

* MPI_Reduce

* MPI_Op_create
