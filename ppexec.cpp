#include<iostream>
#include<unistd.h>
#include<limits.h>
#include<string>
#include<cstdlib>
#include<fstream>
#include<vector>
#include<sys/types.h>
#include<sys/wait.h> 
#include<cstdio>
#include<sys/socket.h>
#include<netdb.h>
#include<netinet/in.h>
#include<netinet/tcp.h>
#include<signal.h>
#include<errno.h>
#include<sys/time.h>
#include<set>
#include<netinet/in.h>
#include<netinet/tcp.h>
#include<cstring>
#include<cerrno>
#include<cstdint>
#include<sys/ioctl.h>
   
#define RECV_OK    		0
#define RECV_EOF  		-1
#define DEFAULT_BACKLOG		5
#define NON_RESERVED_PORT 	5001
#define BUFFER_LEN	 	1024	
#define SERVER_ACK		"ACK_FROM_SERVER"

using namespace std;

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
string readXBytes(int fd, int x);
int writeString(string * s, int fd, int x);
bool isclosed(int sock);
int send_x_chars(char * data, int x, int fd);
int receive_chars(char * data, int x, int fd);


bool file_exists(const string fname);

struct job{
    int src_rank;
    int dest_rank;
};

int main(int argc, char * argv[]){
    //setbuf(stdout, NULL);
    int n = 1, opt;
    string host_file = "hostnames";
    string prog_to_run;
    string curr_host;
    char main_hostname[HOST_NAME_MAX];
    ifstream hosts_in;
    bool have_file;
    int rc, status;
    vector<char *> arg_list;
    char * arg;
    string built_arg;
    char * cwd;
    sockaddr_in sin;
    sockaddr_in add;
    int len = sizeof(sockaddr_in);
    vector<string> hns;
    vector<job> to_do;
    int b_total = 0;
    int f_total = 0;
    char cmd;

    //Get cwd
    cwd = get_current_dir_name();
    string cd_cwd = "cd " + (string)cwd + ";";

    //Parse args-------------------------------------------------------
    while((opt = getopt(argc, argv, "f:n:")) != -1){
        switch(opt){
        case 'f':
            host_file = optarg;
            break;
        case 'n':
            n = atoi(optarg);
            break;
        default:
            fprintf(stderr, "Usage: %s [-f hostfile] [-n num_ranks] program program_args...",
                    argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    for(int i = optind; i < argc; i++){
        prog_to_run += argv[i];
        if(i != argc - 1)
            prog_to_run += " ";
    }

    //End Parse args---------------------------------------------------

    //Setup vector of ports for rank contact info - initialize to 0
    vector<unsigned int> sock_set(n, 0);

    gethostname(main_hostname, HOST_NAME_MAX);
    string main_hostname_str = main_hostname;

    //Setup initial socket
    int mysock;
    sin.sin_family = AF_INET;   //IPv4 Socket
    sin.sin_addr.s_addr = INADDR_ANY;   //Attach to all protocols
    sin.sin_port = htons(0);    //Random port

    vector<int> lookup_table(n,0);

    string mpi_rank;
    string mpi_rank_temp = "export PP_MPI_RANK=";
    string mpi_size = "export PP_MPI_SIZE=" + to_string(n) + ";";
    string mpi_host_port;
    string mpi_host_port_temp = "export PP_MPI_HOST_PORT=\'" + 
        main_hostname_str + ":" ;

    //Check if host_file exists
    have_file = file_exists(host_file);
    if(have_file){
        hosts_in.open(host_file);
    }else{
        curr_host = main_hostname;
    }
    mysock = setup_to_accept(0);
    rc = getsockname(mysock, (sockaddr *) &add, 
        (socklen_t *) &len);

    for(int i = 0; i < n; i++){
        if(have_file){
            hosts_in >> curr_host;
            if(hosts_in.eof()){
                hosts_in.close();
                hosts_in.open(host_file);
                hosts_in >> curr_host;
            }
            
        }
        mpi_rank = mpi_rank_temp + to_string(i) + ";";
        mpi_host_port = mpi_host_port_temp;

/*
        mysock = socket(AF_INET, SOCK_STREAM, 0);
        rc = bind(mysock, (sockaddr *)&sin, sizeof(sin)); //Bind to random port
        rc = listen(mysock, n); //Listen on mysock to (up to) n queued connections
        rc = getsockname(mysock, (sockaddr *) &add, (socklen_t *) &len);
        
*/
        string mpi_port = to_string(ntohs(add.sin_port)) + "\';";

        rc = fork();
        if(rc == -1){
            cerr << "\n*************FORK ERROR***************\n";
            exit(0);
        }else if(rc == 0){
           if(curr_host == main_hostname){
                arg_list.push_back((char *)"bash");
                arg_list.push_back((char *)"-c");
                built_arg = cd_cwd + mpi_rank + mpi_size + 
                    mpi_host_port + mpi_port + prog_to_run;
                arg = new char[built_arg.size() + 1];
                copy(built_arg.begin(), built_arg.end(), arg);
                arg[built_arg.size()] = '\0';
                arg_list.push_back(arg);
                arg_list.push_back(0);
                execvp(arg_list[0], &arg_list[0]);
            }else{
                arg_list.push_back((char *)"ssh");
                arg = new char[curr_host.size() + 1];
                copy(curr_host.begin(), curr_host.end(), arg);
                arg[curr_host.size()] = '\0';
                arg_list.push_back(arg);
                built_arg = "bash -c \"" + cd_cwd + mpi_rank + mpi_size + 
                    mpi_host_port + mpi_port + prog_to_run + "\"";
                arg = new char[built_arg.size() + 1];
                copy(built_arg.begin(), built_arg.end(), arg);
                arg[built_arg.size()] = '\0';
                arg_list.push_back(arg);
                arg_list.push_back(0);
                execvp(arg_list[0], &arg_list[0]);
            } 
        }
        hns.push_back(curr_host);
    }


    int exited_children = 0;
    fd_set s, my_fd, fd_test;
    int fd_rdy;
    timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    vector<unsigned int>::iterator iter;
    int bytes_read, bytes_sent;
    char buf[1];
    char * hn_buf;
    job new_job;
    unsigned int msg_len;
    string dest_hn; 
    int new_sock;
    int src_rank, dest_rank, port;
    int t_contact_info = 0;
    

    while(exited_children < n){
        if(t_contact_info < n){
            tv.tv_sec = 1;
            tv.tv_usec = 0;

            FD_ZERO(&my_fd);
            FD_SET(mysock, &my_fd);
            fd_rdy = select(FD_SETSIZE, &my_fd, NULL, NULL, &tv);
            while(fd_rdy > 0 && FD_ISSET(mysock, &my_fd)){
                new_sock = accept_connection(mysock);
                //Store contact info
                tv.tv_sec = 1;
                tv.tv_usec = 0;

                FD_ZERO(&s);
                FD_SET(new_sock, &s);
                fd_rdy = select(FD_SETSIZE, &s, NULL, NULL, &tv);
                while(fd_rdy == 0){
                    tv.tv_sec = 1;
                    tv.tv_usec = 0;

                    FD_ZERO(&s);
                    FD_SET(new_sock, &s);
                    fd_rdy = select(FD_SETSIZE, &s, NULL, NULL, &tv);

                }
                bytes_read = receive_int(&src_rank, new_sock);

                tv.tv_sec = 1;
                tv.tv_usec = 0;

                FD_ZERO(&s);
                FD_SET(new_sock, &s);
                fd_rdy = select(FD_SETSIZE, &s, NULL, NULL, &tv);
                while(fd_rdy == 0){
                    tv.tv_sec = 1;
                    tv.tv_usec = 0;

                    FD_ZERO(&s);
                    FD_SET(new_sock, &s);
                    fd_rdy = select(FD_SETSIZE, &s, NULL, NULL, &tv);

                }
         
                bytes_read = receive_int(&port, new_sock);
                lookup_table[src_rank] = port;
    
                sock_set[src_rank] = new_sock;
                t_contact_info++;
    
                vector<job>::iterator j_iter = to_do.begin();
                while(j_iter != to_do.end()){
                    if(lookup_table[j_iter->dest_rank] != 0){
                        //Send info
                        printf("Sending rank %d info for rank %d\n", 
                            j_iter->src_rank, 
                            j_iter->dest_rank);
                        dest_hn = hns[j_iter->dest_rank];
                        hn_buf = new char[dest_hn.size() + 1];
                        copy(dest_hn.begin(), dest_hn.end(), hn_buf);
                        msg_len = dest_hn.size() + 1;
                        hn_buf[msg_len - 1] = '\0';
                        port = lookup_table[j_iter->dest_rank];

                        FD_ZERO(&s);
                        FD_SET(sock_set[j_iter->src_rank], &s);
                        fd_rdy = select(FD_SETSIZE, NULL, &s, NULL, NULL);
                        send_int(msg_len, sock_set[j_iter->src_rank]);


                        FD_ZERO(&s);
                        FD_SET(sock_set[j_iter->src_rank], &s);
                        fd_rdy = select(FD_SETSIZE, NULL, &s, NULL, NULL);
                        send_x_chars(hn_buf, msg_len, sock_set[j_iter->src_rank]);

                        FD_ZERO(&s);
                        FD_SET(sock_set[j_iter->src_rank], &s);
                        fd_rdy = select(FD_SETSIZE, NULL, &s, NULL, NULL);
                        send_int(port, sock_set[j_iter->src_rank]);

                        j_iter = to_do.erase(j_iter);
                    }else{
                        j_iter++;
                    }
                }
            

                tv.tv_sec = 0;
                tv.tv_usec = 0;
        
                FD_ZERO(&my_fd);
                FD_SET(mysock, &my_fd);
                fd_rdy = select(FD_SETSIZE, &my_fd, NULL, NULL, &tv);
            }
        }

        FD_ZERO(&s);
        iter = sock_set.begin();
        while(iter != sock_set.end()){
            if(*iter != 0){
                FD_SET(*iter, &s);
            }
            iter++;
        }
        tv.tv_sec = 1;
        tv.tv_usec = 0;
    
        exited_children += waitpid(0, &status, WNOHANG);
        fd_rdy = select(FD_SETSIZE, &s, NULL, NULL, &tv);
        iter = sock_set.begin();
        if(fd_rdy > 0){
            while(iter != sock_set.end()){
                if(isclosed(*iter)){
                    close(*iter);
                    *iter = 0;
                }
            
                if(*iter != 0 && FD_ISSET(*iter, &s)){
                    receive_chars(&cmd, 1, *iter);

                    switch(cmd){
                    case 'r':
                    {                        //Requesting contact info
                        //Requesting contact info
                        
                        do{
                            tv.tv_sec = 1;
                            tv.tv_usec = 0;

                            FD_ZERO(&s);
                            FD_SET(*iter, &s);
                            fd_rdy = select(FD_SETSIZE, &s, NULL, NULL, &tv);

                        }while(fd_rdy == 0);

                        receive_int(&src_rank, *iter);

                        do{
                            tv.tv_sec = 1;
                            tv.tv_usec = 0;

                            FD_ZERO(&s);
                            FD_SET(*iter, &s);
                            fd_rdy = select(FD_SETSIZE, &s, NULL, NULL, &tv);

                        }while(fd_rdy == 0);

                        receive_int(&dest_rank, *iter);

                        if(lookup_table[dest_rank] == 0){
                            //Add to jobs
                            new_job.src_rank = src_rank;
                            new_job.dest_rank = dest_rank;
                            to_do.push_back(new_job);
                        }else{
                           //Send info

                            dest_hn = hns[dest_rank];
                            hn_buf = new char[dest_hn.size() + 1];
                            copy(dest_hn.begin(), dest_hn.end(), hn_buf);
                            msg_len = dest_hn.size() + 1;
                            hn_buf[msg_len - 1] = '\0';
                            port = lookup_table[dest_rank];
                            do{
                                tv.tv_sec = 1;
                                tv.tv_usec = 0;
    
                                FD_ZERO(&s);
                                FD_SET(*iter, &s);
                                fd_rdy = select(FD_SETSIZE, NULL, &s, NULL, &tv);
    
                            }while(fd_rdy == 0);
                            send_int(msg_len, *iter);

                            do{
                                tv.tv_sec = 1;
                                tv.tv_usec = 0;
    
                                FD_ZERO(&s);
                                FD_SET(*iter, &s);
                                fd_rdy = select(FD_SETSIZE, NULL, &s, NULL, &tv);
    
                            }while(fd_rdy == 0);
                            send_x_chars(hn_buf, msg_len, *iter);

                            do{
                                tv.tv_sec = 1;
                                tv.tv_usec = 0;
    
                                FD_ZERO(&s);
                                FD_SET(*iter, &s);
                                fd_rdy = select(FD_SETSIZE, NULL, &s, NULL, &tv);
    
                            }while(fd_rdy == 0);
                            send_int(port, *iter);
                        }
                        break;
                    }
                    case 'f':
                    {
                        f_total++;
                        if(f_total == n){
                            for(int j = 0; j < n; j++){
                                FD_ZERO(&s);
                                FD_SET(sock_set[j], &s);
                                fd_rdy = select(FD_SETSIZE, NULL, &s, NULL, NULL);
                                send_int(0, sock_set[j]);
                            }
                        }

                        break;
                    }
                    case 'b':
                    {
                        b_total++;
                        if(b_total == n){
                            b_total = 0;
                            for(int j = 0; j < n; j++){
                                FD_ZERO(&s);
                                FD_SET(sock_set[j], &s);
                                fd_rdy = select(FD_SETSIZE, NULL, &s, NULL, NULL);
                                send_int(1, sock_set[j]);
                            }
                        }

                        break;
                    }
                        
                    }
                }
                iter++;
            }
        }
    }

    for(int i = 0; i < n; i++){
        close(sock_set[i]);
    }
    
    return 0;

}

bool file_exists(const string fname){
    ifstream myIn(fname);
    return myIn;
}



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
    int rc, to_server_socket;
    int optval = 1;
    struct sockaddr_in listener;
    struct hostent *hp;

    hp = gethostbyname(hostname);
    if (hp == NULL)
    {
	printf("connect_to_server: gethostbyname %s: %s -- exiting\n",
		hostname, errno);
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
	printf("%s :%d: %s\n", str, val, errno);
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
    fd_set s;
    timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    FD_ZERO(&s);
    FD_SET(fd, &s);
    rc = select(FD_SETSIZE, &s, NULL, NULL, &tv);
    
    do {
        if(rc > 0)
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
        if (rc <= 0) {
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

