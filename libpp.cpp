#include <unistd.h>
#include "mpi1.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <cerrno>
#include <string>
#include <sys/ioctl.h>
#include <list>
#include <vector>
#include <netdb.h>
#include <sys/time.h>
#include <unordered_map>

#define RECV_OK    		0
#define RECV_EOF  		-1
#define DEFAULT_BACKLOG		5
#define NON_RESERVED_PORT 	5001
#define BUFFER_LEN	 	1024	
#define SERVER_ACK		"ACK_FROM_SERVER"

using namespace std;

struct MPI_Send_req{
    int src;
    int tag;
    MPI_Comm comm;
    int fd;
};

struct MPI_Cxn_info{
    int rank;
    string hn;
    int port;
};

struct iItem {
    void * buf;
    int count;
    MPI_Datatype datatype;
    int dest;
    int tag;
    MPI_Comm comm;
    MPI_Request * request;
    bool complete;
    int fd;
};

int MPI_My_server;
int MPI_Server;
list<MPI_Send_req> MPI_Send_req_list;
vector<MPI_Cxn_info *> MPI_Cxn_vec(1, nullptr);
list<int> cxns;
//ISend list
list<iItem> isend_list;

//IRecv list
list<iItem> irecv_list;

unordered_map<MPI_Comm, int> MPI_Comm_mapping;
long long unsigned int MPI_Comm_count;

unordered_map<MPI_Op, MPI_User_function *> MPI_Fxn_map;
int MPI_Op_counter;

void server();
void serve_one_connection(int to_client_socket);
void client(char *server_host);
int setup_to_accept(int port);
int accept_connection(int accept_socket);
int connect_to_server(char *hostname, int port);
int recv_msg(int fd, char *buf);
void send_msg(int fd, char *buf, int size);
void error_check(int val, char *str);
int send_int(int num, int fd);
int receive_int(int *num, int fd);
bool isclosed(int sock);
int send_x_chars(char * data, int x, int fd);
int receive_chars(char * data, int x, int fd);
void MPI_Progress_engine();
void MPI_Fxn_sum(void*, void*, int*, MPI_Datatype*);


int MPI_Init(int * argc, char *** argv){
    int bytes_w;
    int sz, rn, col_pos, host_port, myport;
    int rc, len = sizeof(sockaddr_in);
    sockaddr_in sin;
    int send_port;
    fd_set s;
    MPI_Op_counter = 1;

    MPI_Fxn_map.emplace(MPI_SUM, MPI_Fxn_sum);
    
    string host_info, hostname;

    MPI_Comm_mapping[MPI_COMM_WORLD] = 0;
    MPI_Comm_count = 1;

    //Get rank and size info
    MPI_Comm_size(0, &sz);
    MPI_Comm_rank(0, &rn);

    //Resize and initialize cxn vector
    MPI_Cxn_vec.resize(sz, nullptr);
    
    //Setup my socket
    MPI_My_server = setup_to_accept(0);
    rc = getsockname(MPI_My_server, (sockaddr *) &sin, (socklen_t *) &len);
    myport = ntohs(sin.sin_port);

    //Get host info and parse
    host_info = getenv("PP_MPI_HOST_PORT");
    col_pos = host_info.find(":");
    hostname = host_info.substr(0, col_pos);
    host_port = stoi(host_info.substr(col_pos + 1));
    char * hn = new char[hostname.size() + 1];
    copy(hostname.begin(), hostname.end(), hn);
    hn[hostname.size()] = '\0';

    //Connect to host and send my connection info
    MPI_Server = connect_to_server(hn, host_port);
    FD_ZERO(&s);
    FD_SET(MPI_Server, &s);
    rc = select(FD_SETSIZE, NULL, &s, NULL, NULL);
    bytes_w = send_int(rn, MPI_Server);
    FD_ZERO(&s);
    FD_SET(MPI_Server, &s);
    rc = select(FD_SETSIZE, NULL, &s, NULL, NULL);
    bytes_w = send_int(myport, MPI_Server);

}

int MPI_Comm_size(MPI_Comm comm, int * size){
    (*size) = atoi(getenv("PP_MPI_SIZE"));
    return 1;
}

int MPI_Comm_rank(MPI_Comm comm, int * rank){
    (*rank) = atoi(getenv("PP_MPI_RANK"));
    return 1;
}

int MPI_Ssend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm){
    char r = 'r';
    int myrank;
    int rc = MPI_Comm_rank(comm, &myrank);
    int dest_fd, port, hn_len;
    char * dest_hn;
    int svr;
    int resp;
    fd_set s;
    timeval tv;
    int mpi_comm_tag = comm - MPI_COMM_WORLD;


    MPI_Cxn_info * new_cxn;

    if(MPI_Cxn_vec[dest] == nullptr){
        //Request cxn info from ppexec
        do{
            MPI_Progress_engine();
    
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            FD_ZERO(&s);
            FD_SET(MPI_Server, &s);
            rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
        }while(rc == 0);
        rc = send_x_chars(&r, 1, MPI_Server);
        do{
            MPI_Progress_engine();
    
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            FD_ZERO(&s);
            FD_SET(MPI_Server, &s);
            rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
        }while(rc == 0);
        rc = send_int(myrank, MPI_Server);
        do{
            MPI_Progress_engine();
    
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            FD_ZERO(&s);
            FD_SET(MPI_Server, &s);
            rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
        }while(rc == 0);
        rc = send_int(dest, MPI_Server);

        MPI_Cxn_vec[dest] = new MPI_Cxn_info;
        new_cxn = MPI_Cxn_vec[dest];
        new_cxn->rank = dest;

        do{
            MPI_Progress_engine();
    
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            FD_ZERO(&s);
            FD_SET(MPI_Server, &s);
            rc = select(FD_SETSIZE,&s, NULL, NULL, &tv);
        }while(rc == 0);
        rc = receive_int(&hn_len, MPI_Server);
        dest_hn = new char[hn_len];

        do{
            MPI_Progress_engine();
    
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            FD_ZERO(&s);
            FD_SET(MPI_Server, &s);
            rc = select(FD_SETSIZE,&s, NULL, NULL, &tv);
        }while(rc == 0);
        rc = receive_chars(dest_hn, hn_len, MPI_Server);
        new_cxn->hn = dest_hn;

        do{
            MPI_Progress_engine();
    
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            FD_ZERO(&s);
            FD_SET(MPI_Server, &s);
            rc = select(FD_SETSIZE,&s, NULL, NULL, &tv);
        }while(rc == 0);
        rc = receive_int(&(new_cxn->port), MPI_Server); 
        
        delete[] dest_hn;
    }
    
    //Connect to other rank    
    new_cxn = MPI_Cxn_vec[dest];
    dest_hn = new char[(new_cxn->hn).size() + 1];
    copy((new_cxn->hn).begin(), (new_cxn->hn).end(), dest_hn);
    dest_hn[(new_cxn->hn).size()] = '\0';
    svr = connect_to_server(dest_hn, new_cxn->port);
    cxns.push_back(svr);

    //Send envelope
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    FD_ZERO(&s);
    FD_SET(svr, &s);
    rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);

    while(rc == 0){
        MPI_Progress_engine();

        tv.tv_sec = 2;
        tv.tv_usec = 0;
        FD_ZERO(&s);
        FD_SET(svr, &s);
        rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
    }
    send_int(myrank, svr);

    tv.tv_sec = 2;
    tv.tv_usec = 0;
    FD_ZERO(&s);
    FD_SET(svr, &s);
    rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);

    while(rc == 0){
        MPI_Progress_engine();

        tv.tv_sec = 2;
        tv.tv_usec = 0;
        FD_ZERO(&s);
        FD_SET(svr, &s);
        rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
    }
    send_int(tag, svr);

    tv.tv_sec = 2;
    tv.tv_usec = 0;
    FD_ZERO(&s);
    FD_SET(svr, &s);
    rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
    
    //Send comm info
    while(rc == 0){
        MPI_Progress_engine();

        tv.tv_sec = 2;
        tv.tv_usec = 0;
        FD_ZERO(&s);
        FD_SET(svr, &s);
        rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
    }
    send_int(mpi_comm_tag, svr);

    //Wait for response
    
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    FD_ZERO(&s);
    FD_SET(svr, &s);

    rc = select(FD_SETSIZE, &s, NULL, NULL, &tv);
    while(rc == 0){
        MPI_Progress_engine();
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        FD_ZERO(&s);
        FD_SET(svr, &s);

        rc = select(FD_SETSIZE, &s, NULL, NULL, &tv);
    }
    
    receive_int(&resp, svr);

    FD_ZERO(&s);
    FD_SET(svr, &s);

    tv.tv_sec = 1;
    tv.tv_usec = 0;
    
    //Progress engine while waiting to write
    rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
    while(rc == 0){
        MPI_Progress_engine();
    

        tv.tv_sec = 2;
        tv.tv_usec = 0;

        FD_ZERO(&s);
        FD_SET(svr, &s);
        rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
    }

    switch(datatype){
    case MPI_CHAR:
        {
        char * char_buf = (char *)buf;
        hn_len = sizeof(char) * count;

        send_x_chars(char_buf, hn_len, svr);
        break;
        }
    case MPI_INT:
        {
        int * int_buf = (int *)buf;
        for(int i = 0; i < count; i++){
            do{
                tv.tv_sec = 10;
                tv.tv_usec = 0;

                FD_ZERO(&s);
                FD_SET(svr, &s);
                rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
            }while(rc == 0);
            send_int(int_buf[i], svr);
        }
        break;
        }
    }
    
}

int MPI_Send(const void* buf, int count, MPI_Datatype datatype, int dest, 
    int tag, MPI_Comm comm){

    MPI_Ssend(buf, count, datatype, dest, tag, comm); 
}

int MPI_Recv(void* buf, int count, MPI_Datatype datatype, int source, int tag, 
    MPI_Comm comm, MPI_Status *status){

    int mpi_comm_tag = (long long unsigned int)comm - (long long unsigned int)MPI_COMM_WORLD;

//ADD ERROR CHECKS

    int src, d_tag, fd, rc, len;
    fd_set s;
    timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    list<MPI_Send_req>::iterator env_iter = MPI_Send_req_list.begin();

    if(source != MPI_ANY_SOURCE && tag != MPI_ANY_TAG){
        while(env_iter != MPI_Send_req_list.end() &&
            (env_iter->src != source ||
            env_iter->tag != tag  ||
            env_iter->comm != mpi_comm_tag))
        {
            env_iter++;    
        }

    }else if(source == MPI_ANY_SOURCE && tag != MPI_ANY_TAG){
        while(env_iter != MPI_Send_req_list.end() &&
            (env_iter->comm != mpi_comm_tag ||
            env_iter->tag != tag ))
        {
            env_iter++;    
        }

    }else if(source != MPI_ANY_SOURCE && tag == MPI_ANY_TAG){
        while(env_iter != MPI_Send_req_list.end() &&
            (env_iter->src != source ||
             env_iter->comm != mpi_comm_tag))
        {
            env_iter++;    
        }
    }

    while(env_iter == MPI_Send_req_list.end()){    
        MPI_Progress_engine();
            
        env_iter = MPI_Send_req_list.begin();

        if(source != MPI_ANY_SOURCE && tag != MPI_ANY_TAG){
            while(env_iter != MPI_Send_req_list.end() &&
                (env_iter->src != source ||
                env_iter->tag != tag  ||
                env_iter->comm != mpi_comm_tag))
            {
                env_iter++;    
            }
    
        }else if(source == MPI_ANY_SOURCE && tag != MPI_ANY_TAG){
            while(env_iter != MPI_Send_req_list.end() &&
                (env_iter->comm != mpi_comm_tag ||
                env_iter->tag != tag ))
            {
                env_iter++;    
            }
    
        }else if(source != MPI_ANY_SOURCE && tag == MPI_ANY_TAG){
            while(env_iter != MPI_Send_req_list.end() &&
                (env_iter->src != source ||
                env_iter->comm != mpi_comm_tag))
            {
                env_iter++;    
            }
        }
            
    }

    src = env_iter->src; 
    d_tag = env_iter->tag;
    fd = env_iter->fd;

    status->count = count;
    status->cancelled = 0;
    status->MPI_SOURCE = src;
    status->MPI_TAG = d_tag;
    status->MPI_ERROR = MPI_SUCCESS;

    MPI_Send_req_list.erase(env_iter);

    FD_ZERO(&s);
    FD_SET(fd, &s);
    
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    //Progress engine while waiting to write
    rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
    while(rc == 0){
        MPI_Progress_engine();

        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        FD_ZERO(&s);
        FD_SET(fd, &s);
        rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
    }

    send_int(1, fd);

    FD_ZERO(&s);
    FD_SET(fd, &s);
    
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    //Progress engine while waiting to read
    rc = select(FD_SETSIZE, &s, NULL, NULL, &tv);
    while(rc == 0){
        MPI_Progress_engine();
        
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        FD_ZERO(&s);
        FD_SET(fd, &s);
        rc = select(FD_SETSIZE, &s, NULL, NULL, &tv);
    }
    
    
    switch(datatype){
    case MPI_CHAR:
        {
        char * char_buf = (char *)buf;
        len = sizeof(char) * count;

        receive_chars(char_buf, len, fd);
        break;
        }
    case MPI_INT:
        {
        int * int_buf = (int *)buf;
        for(int i = 0; i < count; i++){
            receive_int(&int_buf[i], fd);
        }
        break;
        }
    }
    
}

int MPI_Barrier(MPI_Comm comm){
        char b = 'b';
        int rc;
        int e;
        timeval tv;
        fd_set s;

        tv.tv_sec = 1;
        tv.tv_usec = 0;
        FD_ZERO(&s);
        FD_SET(MPI_Server, &s);
        rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);

        while(rc == 0){
            MPI_Progress_engine();

            tv.tv_sec = 1;
            tv.tv_usec = 0;
            FD_ZERO(&s);
            FD_SET(MPI_Server, &s);
            rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
        }
        //Send that on barrier and wait;
        rc = send_x_chars(&b, 1, MPI_Server);
        
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        FD_ZERO(&s);
        FD_SET(MPI_Server, &s);
        rc = select(FD_SETSIZE, &s, NULL, NULL, &tv);

        while(rc == 0){
            MPI_Progress_engine();

            tv.tv_sec = 1;
            tv.tv_usec = 0;
            FD_ZERO(&s);
            FD_SET(MPI_Server, &s);
            rc = select(FD_SETSIZE, &s, NULL, NULL, &tv);
        }
        rc = receive_int(&e, MPI_Server);

        return 0;
}

int MPI_Finalize(){
int myrank;
MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
        char f = 'f';
        int rc;
        int e;
        timeval tv;
        fd_set s;

        tv.tv_sec = 1;
        tv.tv_usec = 0;
        FD_ZERO(&s);
        FD_SET(MPI_Server, &s);
        rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);

        while(rc == 0){
            MPI_Progress_engine();

            tv.tv_sec = 1;
            tv.tv_usec = 0;
            FD_ZERO(&s);
            FD_SET(MPI_Server, &s);
            rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
        }

        //Send that on barrier and wait;
        rc = send_x_chars(&f, 1, MPI_Server);
        
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        FD_ZERO(&s);
        FD_SET(MPI_Server, &s);
        rc = select(FD_SETSIZE, &s, NULL, NULL, &tv);

        while(rc == 0){
            MPI_Progress_engine();

            tv.tv_sec = 1;
            tv.tv_usec = 0;
            FD_ZERO(&s);
            FD_SET(MPI_Server, &s);
            rc = select(FD_SETSIZE, &s, NULL, NULL, &tv);
        }

        rc = receive_int(&e, MPI_Server);

        return 0;
}



/* Macros to convert from integer to net byte order and vice versa */
#define i_to_n(n)  (int) htonl( (u_long) n)
#define n_to_i(n)  (int) ntohl( (u_long) n)


void server()
{
    int rc, accept_socket, to_client_socket;

    accept_socket = setup_to_accept(NON_RESERVED_PORT);
    for (;;)
    {
	to_client_socket = accept_connection(accept_socket);
	if ((rc = fork()) == -1) /* Error Condition */
	{
	    printf("server: fork failed\n");
	    exit(99);
	}
	else if (rc > 0)  /* Parent Process, i.e. rc = child's pid */
	{
	    close(to_client_socket);
	}
	else          /* Child Process, rc = 0 */
	{
	    close(accept_socket);
	    serve_one_connection(to_client_socket);
	    exit(0);
	}
    }
}

void serve_one_connection(int to_client_socket)
{
    int rc, ack_length;
    char buf[BUFFER_LEN];

    ack_length = strlen(SERVER_ACK)+1;
    rc = recv_msg(to_client_socket, buf);	
    while (rc != RECV_EOF)
    {
	printf("server received: %s \n",buf);
	send_msg(to_client_socket, (char *)SERVER_ACK, ack_length);
	rc = recv_msg(to_client_socket, buf);	
    }
    close(to_client_socket);
}

void client(char *server_host)
{
    int to_server_socket;
    char buf[BUFFER_LEN];

    to_server_socket = connect_to_server(server_host,NON_RESERVED_PORT);
    printf("\nEnter a line of text to send to the server or EOF to exit\n");
    while (fgets(buf,BUFFER_LEN,stdin) != NULL)
    {
	send_msg(to_server_socket, buf, strlen(buf)+1);
	recv_msg(to_server_socket, buf);	
	printf("client received: %s \n",buf);
        printf("\nEnter a line of text to send to the server or EOF to exit\n");
    }
}

int setup_to_accept(int port)	
{
    int rc, accept_socket;
    int optval = 1;
    struct sockaddr_in sin, from;

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);

    accept_socket = socket(AF_INET, SOCK_STREAM, 0);
    error_check(accept_socket,(char*)"setup_to_accept socket");

    setsockopt(accept_socket,SOL_SOCKET,SO_REUSEADDR,(char *)&optval,sizeof(optval));

    rc = bind(accept_socket, (struct sockaddr *)&sin ,sizeof(sin));
    error_check(rc,(char*)"setup_to_accept bind");

    rc = listen(accept_socket, DEFAULT_BACKLOG);
    error_check(rc,(char*)"setup_to_accept listen");

    return(accept_socket);
}

int accept_connection(int accept_socket)	
{
    struct sockaddr_in from;
    int fromlen, to_client_socket, gotit;
    int optval = 1;

    fromlen = sizeof(from);
    gotit = 0;
    while (!gotit)
    {
	to_client_socket = accept(accept_socket, (struct sockaddr *)&from, (socklen_t *)&fromlen);
	if (to_client_socket == -1)
	{
	    /* Did we get interrupted? If so, try again */
	    if (errno == EINTR)
		continue;
	    else
		error_check(to_client_socket, (char*)"accept_connection accept");
	}
	else
	    gotit = 1;
    }

    setsockopt(to_client_socket,IPPROTO_TCP,TCP_NODELAY,(char *)&optval,sizeof(optval));
    return(to_client_socket);
}


int connect_to_server(char *hostname, int port)	
{
    int myrank;
    int mpi_rc = MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    int rc, to_server_socket;
    int optval = 1;
    struct sockaddr_in listener;
    struct hostent *hp;

    hp = gethostbyname(hostname);
    while (hp == nullptr)
    {
    int i = 0; 
    char c = hostname[i];
    printf("%d: hostname strlen = %d\n", myrank, strlen(hostname));
    while(c != '\0'){
        printf("*%c*\n", c);
    }
	printf("%d: connect_to_server: gethostbyname %s: %s -- exiting\n",
		myrank, hostname, strerror(errno));
    herror("gethostbyname");
    hp = gethostbyname(hostname);
	exit(99);
    }

    bzero((void *)&listener, sizeof(listener));
    bcopy((void *)hp->h_addr, (void *)&listener.sin_addr, hp->h_length);
    listener.sin_family = hp->h_addrtype;
    listener.sin_port = htons(port);

    to_server_socket = socket(AF_INET, SOCK_STREAM, 0);
    error_check(to_server_socket, (char*)"net_connect_to_server socket");

    setsockopt(to_server_socket,IPPROTO_TCP,TCP_NODELAY,(char *)&optval,sizeof(optval));

    rc = connect(to_server_socket,(struct sockaddr *) &listener, sizeof(listener));
    error_check(rc, (char*)"net_connect_to_server connect");

    return(to_server_socket);
}


int recv_msg(int fd, char *buf)
{
    int bytes_read;

    bytes_read = read(fd, buf, BUFFER_LEN);
    error_check( bytes_read, (char*)"recv_msg read");
    if (bytes_read == 0)
	return(RECV_EOF);
    return( bytes_read );
}

void send_msg(int fd, char *buf, int size)	
{
    int n;

    n = write(fd, buf, size);
    error_check(n, (char *)"send_msg write");
}

void error_check(int val, char *str)	
{
    if (val < 0)
    {
	printf("%s :%d: %s\n", str, val, strerror(errno));
	exit(1);
    }
}

int send_int(int num, int fd)
{
    int32_t conv = htonl(num);
    char *data = (char*)&conv;
    int left = sizeof(conv);
    int rc;
    fd_set s;
    timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    
    FD_ZERO(&s);
    FD_SET(fd, &s);
    rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);

    do {
        if(rc > 0)
            rc = write(fd, data, left);
        if (rc < 0) {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
                tv.tv_sec = 1;
                tv.tv_usec = 0;

                FD_ZERO(&s);
                FD_SET(fd, &s);
                rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
                
            }
            else if (errno != EINTR) {
                return -1;
            }
        }
        else {
            data += rc;
            left -= rc;
        }
    }
    while (left > 0);
    return 0;
}

int receive_int(int *num, int fd)
{
    int32_t ret;
    char *data = (char*)&ret;
    int left = sizeof(ret);
    int rc;
    int myrank;
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    fd_set s;
    timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    FD_ZERO(&s);
    FD_SET(fd, &s);
    rc = select(FD_SETSIZE, &s, NULL, NULL, &tv);
    do {
    FD_ZERO(&s);
    FD_SET(fd, &s);
    rc = select(FD_SETSIZE, &s, NULL, NULL, &tv);
        if(rc > 0){
            rc = read(fd, data, left);
}
        if (rc <= 0) {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                tv.tv_sec = 1;
                tv.tv_usec = 0;

                FD_ZERO(&s);
                FD_SET(fd, &s);
                rc = select(FD_SETSIZE, &s, NULL, NULL, &tv);
            }
            else if (errno == -1) {
                continue;
            }
            else if (errno != EINTR) {
                return -1;
            }
        }
        else {
            data += rc;
            left -= rc;
        }
    }
    while (left > 0);
    *num = ntohl(ret);
    return 0;
}

bool isclosed(int sock) {
  fd_set rfd;
  FD_ZERO(&rfd);
  FD_SET(sock, &rfd);
  timeval tv = { 0 };
  select(sock+1, &rfd, 0, 0, &tv);
  if (!FD_ISSET(sock, &rfd))
    return false;
  int n = 0;
  ioctl(sock, FIONREAD, &n);
  return n == 0;
}

int send_x_chars(char * data, int x, int fd)
{
    int left = sizeof(char) * x;
    int rc;
    fd_set s;
    timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    
    FD_ZERO(&s);
    FD_SET(fd, &s);
    rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
    
    do {
        if(rc > 0)
            rc = write(fd, data, left);
        if (rc < 0) {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
                tv.tv_sec = 1;
                tv.tv_usec = 0;

                FD_ZERO(&s);
                FD_SET(fd, &s);
                rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
                
            }
            else if (errno != EINTR) {
                return -1;
            }
        }
        else {
            data += rc;
            left -= rc;
        }
    }
    while (left > 0);
    return 0;
}

int receive_chars(char * data, int x, int fd)
{
    int left = sizeof(char) * x;
    int rc;
    fd_set s;
    timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    
    FD_ZERO(&s);
    FD_SET(fd, &s);
    rc = select(FD_SETSIZE, &s, NULL, NULL, &tv);
    do {
        if(rc >0)
            rc = read(fd, data, left);
        if (rc <= 0) {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                tv.tv_sec = 1;
                tv.tv_usec = 0;
        
                FD_ZERO(&s);
                FD_SET(fd, &s);
                rc = select(FD_SETSIZE, &s, NULL, NULL, &tv);
            }
            else if (errno != EINTR) {
                return -1;
            }
        }
        else {
            data += rc;
            left -= rc;
        }
    }
    while (left > 0);
    return 0;
}

void MPI_Progress_engine(){
    fd_set s;
    timeval tv, nowait;
    tv.tv_sec = 0;
    nowait.tv_sec = 0;
    tv.tv_usec = nowait.tv_usec = 0;
    int rc, newsock;
    int data;
    MPI_Send_req new_send_req;
    MPI_Comm s_comm;

    int myrank;
    rc = MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    list<int>::iterator fd_iter = cxns.begin();
    while(fd_iter != cxns.end()){
        if(isclosed(*fd_iter)){
            close(*fd_iter);
            fd_iter = cxns.erase(fd_iter);
        }else{
            fd_iter++;
        }
    }

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    FD_ZERO(&s);
    FD_SET(MPI_My_server, &s);

    rc = select(FD_SETSIZE, &s, NULL, NULL, &tv);
    while(rc > 0){
        //Get new connection
        newsock = accept_connection(MPI_My_server);
        new_send_req.fd = newsock;

        //Get sending rank info
        do{
    
            tv.tv_sec = 0;
            tv.tv_usec = 0;
            FD_ZERO(&s);
            FD_SET(newsock, &s);
            rc = select(FD_SETSIZE, &s, NULL, NULL, &tv);
        }while(rc == 0);
        rc = receive_int(&data, newsock);
        new_send_req.src = data;

        //Get tag of message
        do{
    
            tv.tv_sec = 0;
            tv.tv_usec = 0;
            FD_ZERO(&s);
            FD_SET(newsock, &s);
            rc = select(FD_SETSIZE, &s, NULL, NULL, &tv);
        }while(rc == 0);
        rc = receive_int(&data, newsock);
        new_send_req.tag = data;

        //Get tag of message
        do{
    
            tv.tv_sec = 0;
            tv.tv_usec = 0;
            FD_ZERO(&s);
            FD_SET(newsock, &s);
            rc = select(FD_SETSIZE, &s, NULL, NULL, &tv);
        }while(rc == 0);
        rc = receive_int(&data, newsock);
        new_send_req.comm = data;

        //Store the send request
        MPI_Send_req_list.push_back(new_send_req);

        tv.tv_sec = 0;
        tv.tv_usec = 0;

        //Check for other new connections
        FD_ZERO(&s);
        FD_SET(MPI_My_server, &s);
        rc = select(FD_SETSIZE, &s, NULL, NULL, &nowait);
    }

    int resp;
    int hn_len;


    //Deal with ISend stuff
    list<iItem>::iterator iIter = isend_list.begin();
    while(iIter != isend_list.end()){
        tv.tv_sec = 0;
        tv.tv_usec = 0;
    
        FD_ZERO(&s);
        FD_SET(iIter->fd, &s);
    
        rc = select(FD_SETSIZE, &s, NULL, NULL, &tv);
        if(rc != 0){
            receive_int(&resp, iIter->fd);
        
            FD_ZERO(&s);
            FD_SET(iIter->fd, &s);
        
            rc = select(FD_SETSIZE, NULL, &s, NULL, NULL);
        
            switch(iIter->datatype){
            case MPI_CHAR:
                {
                char * char_buf = (char *)(iIter->buf);
                hn_len = sizeof(char) * (iIter->count);
        
                send_x_chars(char_buf, hn_len, iIter->fd);
                break;
                }
            case MPI_INT:
                {
                int * int_buf = (int *)(iIter->buf);
                for(int i = 0; i < (iIter->count); i++){
                    do{
                        FD_ZERO(&s);
                        FD_SET(iIter->fd, &s);
                        rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
                    }while(rc == 0);
                    send_int(int_buf[i], iIter->fd);
                }
                break;
                }
            }
            
            *(iIter->request) = 0;
            iIter->complete = true;
            iIter = isend_list.erase(iIter);
        }
            
        if(iIter != isend_list.end())
            iIter++;
    }

    iIter = irecv_list.begin();
    while(iIter != irecv_list.end()){
        int mpi_comm_tag = (long long unsigned int)(iIter->comm) - (long long unsigned int)MPI_COMM_WORLD;
    
        int src, d_tag, fd, rc, len;
        fd_set s;
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
    
        list<MPI_Send_req>::iterator env_iter = MPI_Send_req_list.begin();
    
        if(iIter->dest != MPI_ANY_SOURCE && iIter->tag != MPI_ANY_TAG){
            while(env_iter != MPI_Send_req_list.end() &&
                (env_iter->src != iIter->dest ||
                env_iter->tag != iIter->tag  ||
                env_iter->comm != mpi_comm_tag))
            {
                env_iter++;    
            }
    
        }else if(iIter->dest == MPI_ANY_SOURCE && iIter->tag != MPI_ANY_TAG){
            while(env_iter != MPI_Send_req_list.end() &&
                (env_iter->comm != mpi_comm_tag ||
                env_iter->tag != iIter->tag ))
            {
                env_iter++;    
            }
    
        }else if(iIter->dest != MPI_ANY_SOURCE && iIter->tag == MPI_ANY_TAG){
            while(env_iter != MPI_Send_req_list.end() &&
                (env_iter->src != iIter->dest ||
                env_iter->comm != mpi_comm_tag))
            {
                env_iter++;    
            }
        }
    
        if(env_iter != MPI_Send_req_list.end()){
            src = env_iter->src; 
            d_tag = env_iter->tag;
            fd = env_iter->fd;
        
            MPI_Send_req_list.erase(env_iter);
        
            FD_ZERO(&s);
            FD_SET(fd, &s);
            
            rc = select(FD_SETSIZE, NULL, &s, NULL, NULL);
        
            send_int(1, fd);
        
            FD_ZERO(&s);
            FD_SET(fd, &s);
            
            rc = select(FD_SETSIZE, &s, NULL, NULL, NULL);
            
            
            switch(iIter->datatype){
            case MPI_CHAR:
                {
                char * char_buf = (char *)(iIter->buf);
                len = sizeof(char) * (iIter->count);
        
                receive_chars(char_buf, len, fd);
                break;
                }
            case MPI_INT:
                {
                int * int_buf = (int *)(iIter->buf);
                for(int i = 0; i < (iIter->count); i++){
                    receive_int(&int_buf[i], fd);
                }
                break;
                }
            }

            *(iIter->request) = 0;
            iIter->complete = true;
            iIter = irecv_list.erase(iIter);

        }

        if(iIter != irecv_list.end())
            iIter++;
    }

}

double MPI_Wtime(){
    timeval tv;
    gettimeofday( &tv, nullptr);
    return (tv.tv_sec + (tv.tv_usec / 1000000.0));
}

int MPI_Comm_dup(MPI_Comm comm, MPI_Comm * newcomm){
    *newcomm = (long long unsigned int)((long long unsigned int)(MPI_COMM_WORLD) + MPI_Comm_count);
    MPI_Comm_count++;
}

int MPI_Gather(void* sendbuf, int sendcount, MPI_Datatype sendtype, void* recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm){
    int myRank;
    int commSize;
    int rc = MPI_Comm_rank(comm, &myRank);
    MPI_Status status;
    rc = MPI_Comm_size(comm, &commSize);

    if(myRank == root){
        MPI_Barrier(comm);
        
        for(int i = 0; i < commSize; i++){
            if(myRank == i){
                if(sendtype == MPI_INT){
                    for(int j = 0; j < sendcount; j++){
                        ((int *)recvbuf)[(i * sendcount) + j] = ((int *)sendbuf)[j];
                    }
                }else{
                    for(int j = 0; j < sendcount; j++){
                        ((char *)recvbuf)[(i * sendcount) + j] = ((char *)sendbuf)[j];
                    }
                }
                continue;
            }
            MPI_Recv((void *)&(((int*)recvbuf)[i * sendcount]), sendcount, sendtype, i, -10, comm, &status);
        }
        MPI_Barrier(comm);
    }else{
        MPI_Barrier(comm);

        MPI_Send(sendbuf, sendcount, sendtype, root, -10, comm);

        MPI_Barrier(comm);
    }
}

int MPI_Bcast(void * buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm){
    int myrank;
    int commSize;
    int rc = MPI_Comm_rank(comm, &myrank);
    MPI_Status status;
    rc = MPI_Comm_size(comm, &commSize);

    MPI_Barrier(comm);
    if(myrank == root){
        for(int i = 0; i < commSize; i++){
            if (i == myrank)
                continue;
            MPI_Send(buffer, count, datatype, i, -10, comm);
        }
    }else{
            MPI_Recv(buffer, count, datatype, root, -10, comm, &status);
    }
}


int MPI_Isend(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request * request){
    char r = 'r';
    int myrank;
    int rc = MPI_Comm_rank(comm, &myrank);
    int dest_fd, port, hn_len;
    char * dest_hn;
    int svr;
    int resp;
    fd_set s;
    timeval tv;
    int mpi_comm_tag = comm - MPI_COMM_WORLD;


    MPI_Cxn_info * new_cxn;

    if(MPI_Cxn_vec[dest] == nullptr){
        
        //Request cxn info from ppexec
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        FD_ZERO(&s);
        FD_SET(MPI_Server, &s);
        rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
        while(rc == 0){
            MPI_Progress_engine();
    
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            FD_ZERO(&s);
            FD_SET(MPI_Server, &s);
            rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
        }
        rc = send_x_chars(&r, 1, MPI_Server);

        tv.tv_sec = 10;
        tv.tv_usec = 0;
        FD_ZERO(&s);
        FD_SET(MPI_Server, &s);
        rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
        while(rc == 0){
            MPI_Progress_engine();
    
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            FD_ZERO(&s);
            FD_SET(MPI_Server, &s);
            rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
        }
        rc = send_int(myrank, MPI_Server);

        tv.tv_sec = 10;
        tv.tv_usec = 0;
        FD_ZERO(&s);
        FD_SET(MPI_Server, &s);
        rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
        while(rc == 0){
            MPI_Progress_engine();
    
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            FD_ZERO(&s);
            FD_SET(MPI_Server, &s);
            rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
        }
        rc = send_int(dest, MPI_Server);

        MPI_Cxn_vec[dest] = new MPI_Cxn_info;
        new_cxn = MPI_Cxn_vec[dest];
        new_cxn->rank = dest;

        tv.tv_sec = 10;
        tv.tv_usec = 0;
        FD_ZERO(&s);
        FD_SET(MPI_Server, &s);
        rc = select(FD_SETSIZE,&s, NULL, NULL, &tv);
        while(rc == 0){
            MPI_Progress_engine();
    
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            FD_ZERO(&s);
            FD_SET(MPI_Server, &s);
            rc = select(FD_SETSIZE,&s, NULL, NULL, &tv);
        }
        rc = receive_int(&hn_len, MPI_Server);
        dest_hn = new char[hn_len];

        tv.tv_sec = 10;
        tv.tv_usec = 0;
        FD_ZERO(&s);
        FD_SET(MPI_Server, &s);
        rc = select(FD_SETSIZE,&s, NULL, NULL, &tv);
        while(rc == 0){
            MPI_Progress_engine();
    
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            FD_ZERO(&s);
            FD_SET(MPI_Server, &s);
            rc = select(FD_SETSIZE,&s, NULL, NULL, &tv);
        }
        rc = receive_chars(dest_hn, hn_len, MPI_Server);
        new_cxn->hn = dest_hn;

        tv.tv_sec = 10;
        tv.tv_usec = 0;
        FD_ZERO(&s);
        FD_SET(MPI_Server, &s);
        rc = select(FD_SETSIZE,&s, NULL, NULL, &tv);
        while(rc == 0){
            MPI_Progress_engine();
    
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            FD_ZERO(&s);
            FD_SET(MPI_Server, &s);
            rc = select(FD_SETSIZE,&s, NULL, NULL, &tv);
        }
        rc = receive_int(&(new_cxn->port), MPI_Server); 
        
        delete[] dest_hn;
    }
    
    //Connect to other rank    
    new_cxn = MPI_Cxn_vec[dest];
    dest_hn = new char[(new_cxn->hn).size() + 1];
    copy((new_cxn->hn).begin(), (new_cxn->hn).end(), dest_hn);
    dest_hn[(new_cxn->hn).size()] = '\0';
    svr = connect_to_server(dest_hn, new_cxn->port);

    //Send envelope
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    FD_ZERO(&s);
    FD_SET(svr, &s);
    rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);

    while(rc == 0){
        MPI_Progress_engine();

        tv.tv_sec = 2;
        tv.tv_usec = 0;
        FD_ZERO(&s);
        FD_SET(svr, &s);
        rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
    }
    send_int(myrank, svr);

    tv.tv_sec = 2;
    tv.tv_usec = 0;
    FD_ZERO(&s);
    FD_SET(svr, &s);
    rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);

    while(rc == 0){
        MPI_Progress_engine();

        tv.tv_sec = 2;
        tv.tv_usec = 0;
        FD_ZERO(&s);
        FD_SET(svr, &s);
        rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
    }
    send_int(tag, svr);

    tv.tv_sec = 2;
    tv.tv_usec = 0;
    FD_ZERO(&s);
    FD_SET(svr, &s);
    rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
    
    //Send comm info
    while(rc == 0){
        MPI_Progress_engine();

        tv.tv_sec = 2;
        tv.tv_usec = 0;
        FD_ZERO(&s);
        FD_SET(svr, &s);
        rc = select(FD_SETSIZE, NULL, &s, NULL, &tv);
    }
    send_int(mpi_comm_tag, svr);

    *request = 1;
    
    //Store info in send_list
    iItem isi;
    isi.buf = buf;
    isi.count = count;
    isi.datatype = datatype;
    isi.dest = dest;
    isi.tag = tag;
    isi.comm = comm;
    isi.request = request;
    isi.fd = svr;

    isend_list.push_back(isi);
    
}

int MPI_Irecv(void* buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *request){

    *request = 1;
    
    //Store info in send_list
    iItem isi;
    isi.buf = buf;
    isi.count = count;
    isi.datatype = datatype;
    isi.dest = source;
    isi.tag = tag;
    isi.comm = comm;
    isi.request = request;
    isi.fd = -1;

    irecv_list.push_back(isi);
    
}

int MPI_Test(MPI_Request * request, int * flag, MPI_Status * status){
    if(*request != 0)
        MPI_Progress_engine();

    if(*request == 0){
        *flag = 1;
    }else{
        *flag = 0;
    }

    printf("In test: flag = %d\n", *flag);

    return 0;
}

int MPI_Wait(MPI_Request * request, MPI_Status * status){
    while(*request != 0){
        MPI_Progress_engine();
    }
}

int MPI_Reduce(void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
               MPI_Op op, int root, MPI_Comm comm){
    int myrank, size, rc;
    MPI_Status status;

    if(datatype != MPI_INT){
        printf("MPI_INT is the only supported datatype\n");
        return -1;
    }

    int * data_buf = new int[count];

    rc = MPI_Comm_rank(comm, &myrank);
    rc = MPI_Comm_size(comm, &size);

    if(myrank == root){
        MPI_Barrier(comm);

        MPI_User_function * fp = MPI_Fxn_map[op];

        if(count == 1){
            *((int*)recvbuf) = *((int*)sendbuf);
        }else{
            for(int i = 0; i < count; i++){
                ((int*)recvbuf)[i] = ((int*)sendbuf)[i];
            }
        }

        for(int i = 1; i < size; i++){
            if(myrank == i){
                continue;
            }else{
                MPI_Recv((void*)data_buf, count, datatype, i, -10, comm, &status);
                (*fp)((void*)data_buf, (void *)recvbuf, &count, &datatype);
            }
        }
        MPI_Barrier(comm);
    }else{
        MPI_Barrier(comm);

        MPI_Send(sendbuf, count, datatype, root, -10, comm);

        MPI_Barrier(comm);
    }

}

int MPI_Op_create(MPI_User_function *user_fn, int commute, MPI_Op *op){
    *op = (MPI_Op)MPI_Op_counter;
    MPI_Op_counter++;

    MPI_Fxn_map.emplace(*op, *user_fn);

    return 0;
}


void MPI_Fxn_sum(void *inp, void *inoutp, int *len, MPI_Datatype *dptr)
{
    int i;
    int r;
    int *in    = (int *)inp;
    int *inout = (int *)inoutp;

    for (i=0; i < *len; i++)
    {
        r = *in + *inout;
        *inout = r;
        in++;
        inout++;
    }
}
