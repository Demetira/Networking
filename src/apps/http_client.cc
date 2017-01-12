#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>


using namespace std;

#define BUFSIZE 1024

int write_n_bytes(int fd, char * buf, int count);

int main(int argc, char * argv[]) {
    char * server_name = NULL;
    int server_port = 0;
    char * server_path = NULL;

    int sock = 0;
    int rc = -1;
    int datalen = 0;
    bool ok = false;
    struct sockaddr_in sa;
    FILE * wheretoprint = stdout;
    struct hostent * site = NULL;
    char * req = NULL;

    char buf[BUFSIZE + 1];
    char * bptr = NULL;
    char * bptr2 = NULL;
    char * endheaders = NULL;
   
    struct timeval timeout;
    fd_set set;

    /*parse args */
    if (argc != 5) {
	fprintf(stderr, "usage: http_client k|u server port path\n");
	exit(-1);
    }

    server_name = argv[2];
    server_port = atoi(argv[3]);
    server_path = argv[4];



    /* initialize minet */
    if (toupper(*(argv[1])) == 'K') { 
	minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') { 
	minet_init(MINET_USER);
    } else {
	fprintf(stderr, "First argument must be k or u\n");
	exit(-1);
    }

    /* create socket */
//    sock = socket(AF_INET, SOCK_STREAM, 0);
    sock = minet_socket(SOCK_STREAM);
    if(sock < 0){
       cout<<"ERROR opening socket\n";
        exit(1);
    } else {
       cout<<"Success opening socket\n";
    }

    // Do DNS lookup
    /* Hint: use gethostbyname() */
    site = gethostbyname(server_name);
    
    if(site == NULL){
        cout<<"ERROR, no such host\n";
        exit(0);
    }else{
        cout<<"Success find host\n";
    }
    

    /* set address */
    
    memset (&sa, 0, sizeof (sa));
    // bzero((char *) &site, sizeof(site));
    // clear the memory for server address
    
    sa.sin_port = htons(server_port);
    memcpy((char *)&sa.sin_addr.s_addr,(char *)site->h_addr, site->h_length);
    //bcopy((char *)site->h_addr, (char *)&sa.sin_addr.s_addr, site->h_length);
    
    sa.sin_family = AF_INET;
    

    /* connect socket */
//    if(connect(sock, (struct sockaddr*)&sa, sizeof sa) < 0){
    if(minet_connect(sock, (struct sockaddr_in*)&sa) < 0){
        cout<<"ERROR connecting\n";
        exit(1);
    }else{
        cout<<"Success connecting\n";
    }
        

    /* send request */
    string request = "GET ";
    request.append(server_path);
    request.append(" HTTP/1.0\n\n");
    req = new char[request.size() + 1];
    copy(request.begin(), request.end(), req);
    req[request.size()] = '\0';
    
    if(minet_sendto(sock, req, strlen(req), (struct sockaddr_in*)&sa) != (int)strlen(req)){
        cout<<"ERROR sending request\n";
        exit(1);
    }else{
        cout<<"Success sending request\n";
    }
    /* wait till socket can be read */
    /* Hint: use select(), and ignore timeout for now. */
    // initialize the set
    do{
        FD_ZERO(&set);
        FD_SET(sock, &set);
        rc = minet_select(sock+1, &set, NULL, NULL, NULL);
    }while(rc == -1 && errno == EINTR);
    
    string return_code = "";
    
    if(rc > 0){
        if(FD_ISSET(sock, &set)){
            /* The sock has data available to be read */
            rc = minet_read(sock, buf, BUFSIZE);
            if (rc == 0) {
                /* This means the other side closed the socket */
                /*close socket and deinitialize */
                minet_close(sock);
                minet_deinit();
            }else {
                /* first read loop -- read headers */
                /* examine return code */
                //Skip "HTTP/1.0"
                //remove the '\0'
                // Normal reply has return code 200

                int stop_point = 0;
                for(int i = 0; i < BUFSIZE && buf[i] != ' '; i++){
                    stop_point = i;
                }
                for(int i = stop_point + 2; i < BUFSIZE && buf[i] != ' '; i++){
                    return_code += buf[i];
                }

                if(return_code == "200"){
                    cout<<"Success reading from socket\n";
                    /* print first part of response */
                    while(rc > 0){
                        /* second read loop -- print out the rest of the response */
                        cout<<buf;
                        bzero(buf,BUFSIZE);
                        rc = minet_read(sock, buf, BUFSIZE);
                    }
                    if (rc == 0) {
                        /* This means the other side closed the socket */
                        /*close socket and deinitialize */
                        cout<<"\n";
                        ok = true;
                    }
                    if(rc < 0){
                         cout<<"ERROR reading from socket\n";
                    }
                }else{
                    cout<<"ERROR web return code is not 200\n";
                    cout<<"Web return code is:"<< return_code <<"\n";
                }
            }
        }
    }else if(rc < 0){
        
        cout<<"ERROR reading from socket\n";
    }
    
    minet_close(sock);
    minet_deinit();

    if (ok) {
	return 0;
    } else {
	return -1;
    }
}

int write_n_bytes(int fd, char * buf, int count) {
    int rc = 0;
    int totalwritten = 0;

    while ((rc = minet_write(fd, buf + totalwritten, count - totalwritten)) > 0) {
	totalwritten += rc;
    }
    
    if (rc < 0) {
	return -1;
    } else {
	return totalwritten;
    }
}


