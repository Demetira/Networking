#include "minet_socket.h"
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fstream>
#include <iostream>

using namespace std;

#define BUFSIZE 1024
#define FILENAMESIZE 100

int handle_connection(int);
int writenbytes(int,char *,int);
int readnbytes(int,char *,int);

int main(int argc,char *argv[])
{
  int server_port;
  int sock,sock2;
  struct sockaddr_in sa,sa2;
  int rc,i;
  fd_set readlist;
  fd_set connections;
  int maxfd;

  /* parse command line args */
  if (argc != 3)
  {
    fprintf(stderr, "usage: http_server1 k|u port\n");
    exit(-1);
  }
  server_port = atoi(argv[2]);
  if (server_port < 1500)
  {
    fprintf(stderr,"INVALID PORT NUMBER: %d; can't be < 1500\n",server_port);
    exit(-1);
  }

  /* initialize and make socket */
    if (toupper(*(argv[1])) == 'K') {
        minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') {
        minet_init(MINET_USER);
    } else {
        fprintf(stderr, "First argument must be k or u\n");
        exit(-1);
    }
    
    // create server socket
    sock = minet_socket(SOCK_STREAM);
    if(sock < 0){
        minet_perror("ERROR opening socket\n");
    }
    
  /* set server address*/
    memset(&sa,0,sizeof (sa));
    sa.sin_port=htons(server_port);
    sa.sin_addr.s_addr=htonl(INADDR_ANY);
    sa.sin_family=AF_INET;

  /* bind listening socket */
    if(minet_bind(sock,(struct sockaddr_in*)&sa) < 0){
        minet_perror("ERROR binding\n");
        exit(1);
    }

  /* start listening */
    if(minet_listen(sock, SOMAXCONN) < 0){
        minet_perror("ERROR listening\n");
    }
    
    FD_ZERO(&connections);
    FD_SET(sock,&connections);
    maxfd = sock;
    
    cout<< "SOCK" << sock<<"\n";

  /* connection handling loop */
  while(1)
  {
    /* create read list */
      FD_ZERO(&readlist);
      for(int j = 0; j <= maxfd; j++){
          if(FD_ISSET(j, &connections)){
              FD_SET(j, &readlist);
          }
      }
      
    /* do a select */
      do{
        rc = minet_select(maxfd + 1, &readlist, NULL, NULL, NULL);
      }while(rc < 0);
      
    /* process sockets that are ready */
      for(int i = maxfd; i >= 0; i--){
              /* for the accept socket, add accepted connection to connections */
          if(FD_ISSET(i, &readlist)){
              if (i == sock){
                  sock2 = minet_accept(i, (struct sockaddr_in*)&sa2);
                  maxfd = (sock2 > maxfd ? sock2 : maxfd);
                  cout<< "sock2: " << sock2<<"\n";
                  FD_SET(sock2, &connections);
              }
              else/* for a connection socket, handle the connection */
              {
                  rc = handle_connection(i);
                  if(rc < 0){
                      minet_perror("ERROR handling connection\n");
                  }
                  if(rc == 0){
                      cout << "connection finish\n";
                      minet_close(i);
                      FD_CLR(i, &connections);
                      if(i == maxfd){
                          for(int j = maxfd -1; j >= 0; j--){
                              if(FD_ISSET(j, &connections)){
                                  maxfd = j;
                                  break;
                              }
                          }
                      }
                  }
              }

          }
      }
  }
    minet_close(sock);
    minet_deinit();
}

int handle_connection(int sock2)
{
  char filename[FILENAMESIZE+1];
  int rc;
  int fd;
  struct stat filestat;
  char buf[BUFSIZE+1];
  char *headers;
  char *endheaders;
  char *bptr;
  int datalen=0;
  char *ok_response_f = "HTTP/1.0 200 OK\r\n"\
                      "Content-type: text/plain\r\n"\
                      "Content-length: %d \r\n\r\n";
  char ok_response[100];
  char *notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
                         "Content-type: text/html\r\n\r\n"\
                         "<html><body bgColor=black text=white>\n"\
                         "<h2>404 FILE NOT FOUND</h2>\n"\
                         "</body></html>\n";
  bool ok=true;

  /* first read loop -- get request and headers*/
    rc = read(sock2, buf, BUFSIZE);
    if(rc < 0){
        minet_perror("ERROR reading request\n");
    }
    
  /* parse request to get file name */
  /* Assumption: this is a GET request and filename contains no spaces*/
    string request = "";
    int stop_point = 0;
    for(int i = 0; i < BUFSIZE && buf[i] != ' '; i++){
        request += buf[i];
        stop_point = i;
    }
    
    if(request == "GET"){
        bzero(filename,FILENAMESIZE);
        int count = 0;
        for(int i = stop_point + 2; i < BUFSIZE && buf[i] != ' '; i++, count++){
            filename[count] = buf[i];
            stop_point = i;
        }
        filename[count] = '\0';
    }

    /* try opening the file */
    ifstream fs(filename);
    fd = stat (filename, &filestat);
    datalen = (fd == 0 ? filestat.st_size:-1);
    ok = (fs.is_open() && datalen > 0);

  /* send response */
  if (ok)
  {
    /* send headers */
      sprintf(ok_response, ok_response_f, datalen);
      
      minet_write(sock2, ok_response, sizeof (ok_response));

    /* send file */
      string file_content = "";
      while (!fs.eof()){
          string temp;
          getline(fs, temp);
          file_content += temp+"\n";
      }
      
      char send_content[datalen];
      strcpy(send_content, file_content.c_str());
      minet_write(sock2, send_content, strlen (send_content));
      fs.close();
  }
  else	// send error response
  {
      minet_write(sock2, notok_response, strlen(notok_response));
      fs.close();
  }

  /* close socket and free space */
    minet_close(sock2);

  if (ok)
    return 0;
  else
    return -1;
}

int readnbytes(int fd,char *buf,int size)
{
  int rc = 0;
  int totalread = 0;
  while ((rc = minet_read(fd,buf+totalread,size-totalread)) > 0)
    totalread += rc;

  if (rc < 0)
  {
    return -1;
  }
  else
    return totalread;
}

int writenbytes(int fd,char *str,int size)
{
  int rc = 0;
  int totalwritten =0;
  while ((rc = minet_write(fd,str+totalwritten,size-totalwritten)) > 0)
    totalwritten += rc;

  if (rc < 0)
    return -1;
  else
    return totalwritten;
}

