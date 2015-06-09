/* dnssync-web.c
 *
 * Multi-thread special HTTP server
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>

#define ISspace(x) isspace((int)(x))

#define SERVERS_NUM 100
#define CLIENTS_NUM 100
#define SBUF_SIZE 1024

#define H_501 "HTTP/1.0 501 Method Not Implemented\r\n"
#define R_501 "<HTML><HEAD><TITLE>Method Not Implemented</TITLE></HEAD><BODY><P>HTTP request method not supenv_ported.</BODY></HTML>\r\n"

#define H_404 "HTTP/1.0 404 Not Found\r\n"
#define R_404 "<HTML><TITLE>Not Found</TITLE><BODY><P>The server could not fulfill your request because the resouce specified is unavailable or nonexistent.</BODY></HTML>\r\n"

#define H_200 "HTTP/1.0 200 OK\r\n"

#define CTCL "Content-Type: text/html\r\nContent-Length: %d\r\n\r\n"

#define E_OCCURED "Error occured: %s\n"

struct server {
 char hostname[255];
 struct addrinfo* ai;
};

struct client {
 char hostname[255];
 char ip[20];
 char key[50];
 char ttl[20];
 time_t ts; // long int
 // IP TS TTL KEY
};

struct thread {
 pthread_t tid;
 int csock;
 int cliaddr_len;
 struct sockaddr_in cliaddr;
};

struct server *servers[SERVERS_NUM];
struct client *clients[CLIENTS_NUM];

int ssock;
int snum = 0;
int cnum = 0;
int threadmax = 0;

char* env_localip;
char* env_port;
char* env_root;
char* env_threadmax;

pthread_t th;

/* FUNCTIONS */

int get_line(int, char *, int);
void* comth(void*);
void ok(int);
void unimplemented(int);
void not_found(int);
void retrive(int);
int check_ip(struct sockaddr_in* addr);
int check_key_url(char * url);


int get_line(int sock, char *buf, int size) {
 int i = 0;
 char c = '\0';
 int n;

 while ((i < size - 1) && (c != '\n')) {
  n = recv(sock, &c, 1, 0);
  if (n > 0) {
   if (c == '\r') {
    n = recv(sock, &c, 1, MSG_PEEK);
    if ((n > 0) && (c == '\n'))
     recv(sock, &c, 1, 0);
    else
     c = '\n';
   }
   buf[i] = c;
   i++;
  }
  else
   c = '\n';
 }
 buf[i] = '\0';
 
 return(i);
}

void* comth(void* arg)
{
  int c;
  char buf[1024];
  char method[100];
  char url[255];
  int nbytes;
  size_t i, j;
  struct thread self;

  memcpy(&self, (struct thread *) arg, sizeof(struct thread));
  free(arg);
  self.tid = pthread_self();

  c = sizeof(struct sockaddr_in);

  // inet_ntoa -> inet_ntop
  fprintf(stderr, "incoming connection (client_ip: %s)\n", inet_ntoa(self.cliaddr.sin_addr));

  // get first line
  nbytes = get_line(self.csock, buf, sizeof(buf));

  // get method
  i = 0; j = 0;
  while (!ISspace(buf[j]) && (i < sizeof(method) - 1)) {
   method[i] = buf[j];
   i++; j++;
  }
  method[i] = '\0';

  if (strcasecmp(method, "GET")) {
   unimplemented(self.csock);
   close(self.csock);
   return NULL;
  }

  // get URL
  i = 0;
  while (ISspace(buf[j]) && (j < sizeof(buf)))
   j++;
  while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf))) {
   url[i] = buf[j];
   i++; j++;
  }
  url[i] = '\0';

  while ((nbytes > 0) && strcmp("\n", buf))  /* read & discard headers */
    nbytes = get_line(self.csock, buf, sizeof(buf));

  fprintf(stderr, "%s %s\n", method, url);

  // function retrive
  if (strcmp(url, "/dnssync/retrive/") == 0) {
   if (check_ip(&self.cliaddr) == 0) {
     fprintf(stderr, "sending zone file.\n");
     retrive(self.csock);
   } else {
     fprintf(stderr, "retrive not allowed for client.\n");
     not_found(self.csock);
   }
  } else {
  // function client update
   int key = 0;
   if ((key = check_key_url(url)) != 0) {
    strcpy(clients[key-1]->ip, inet_ntoa(self.cliaddr.sin_addr)); 
    clients[key-1]->ts = time(NULL);
    fprintf(stderr, "updating IP address\n");
    ok(self.csock);
   } else {
    not_found(self.csock);
   }
  }

  close(self.csock);
  return NULL;
}

int check_key_url(char * url) {
 int i;

 char * fullurl = malloc(255);

 for (i=0;i<cnum;i++) {
  sprintf(fullurl, "/dnssync/%s/", clients[i]->key);
  if (strcmp(url,fullurl) == 0) {
    free(fullurl);
    return i+1;
  }
 }

 free(fullurl);
 return 0;
}


void response(int client, const char * header1, const char * header2, char * content) {
 char *buf;
 buf = malloc(1024);
 sprintf(buf, "%s", header1);
 send(client, buf, strlen(buf), 0);
 sprintf(buf, header2, (int) strlen(content));
 send(client, buf, strlen(buf), 0);
 if (strcmp(content, "") != 0) {
   sprintf(buf, content);
   send(client, buf, strlen(buf), 0);
 }
 free(buf);
}

void ok(int client) {
 response(client, "HTTP/1.0 200 OK\r\n", CTCL, "OK");
}

void unimplemented(int client) {
 response(client, H_501, CTCL, R_501);
}

void not_found(int client) {
 response(client, H_404, CTCL, R_404);
}

void retrive(int client) {
 int i = 0;
 char *buf;

 buf = malloc(1024);

 sprintf(buf, "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\n");
 send(client, buf, strlen(buf), 0);

 for (i=0; i<cnum; i++) {
  sprintf(buf, "+%s:%s:%s\n", clients[i]->hostname, clients[i]->ip, clients[i]->ttl);
  send(client, buf, strlen(buf), 0);
 }

 free(buf);
}

int process_servers() {
 DIR* dir;
 struct dirent* item;
 struct addrinfo hints;
 struct addrinfo *res;
 char ips[INET6_ADDRSTRLEN];
 char servers_path[255];
 int s;

 memset(&hints, 0, sizeof(hints));
 memset(&servers, 0, sizeof(servers));
 hints.ai_family = AF_UNSPEC;
 hints.ai_socktype = SOCK_STREAM;

 sprintf(servers_path, "%s/servers", env_root);

 if ((dir = opendir(servers_path)) == NULL) {
  perror("opendir");
  return 1;
 }

 errno = 0;
 while((item = readdir(dir)) != NULL) {
  if ((strcmp(item->d_name,".") == 0) || (strcmp(item->d_name,"..") == 0))
    continue;

  servers[snum] = malloc(sizeof(struct server));
  strcpy(servers[snum]->hostname, item->d_name);
  errno = 0;
  if ((s = getaddrinfo(servers[snum]->hostname, NULL, &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
    return 1;
  } else {
    servers[snum]->ai = res;
  }
  if (inet_ntop(AF_INET, &((struct sockaddr_in*)(servers[snum]->ai->ai_addr))->sin_addr, ips, sizeof(ips)) != NULL) {
    fprintf(stderr, "loaded server: %s (ip: %s)\n", servers[snum]->hostname, ips);
  } else {
    fprintf(stderr, "name resolution problem.\n");
  }
  snum++;
 }
 if (errno)
 {
  perror("readdir");
  return 1;
 }
 closedir(dir);
 return 0;
}

void free_servers() {
 int i;
 for (i=0; i<snum; i++) {
   free(servers[i]->ai);
   free(servers[i]);
 }
}

void free_clients() {
 int i;
 for (i=0; i<cnum; i++) {
   if (clients[i] != NULL)
     free(clients[i]);
 }
}

char * readfile(char *pathname) {
 int fd, n;
 char buf[100];

 if ((fd = open(pathname, O_RDONLY)) < 0) {
   perror("open");
   return NULL;
 }

 n = read(fd, &buf, 100);
 buf[n]='\0';
 if (buf[n-1] == '\n') buf[n-1] = '\0';

 close(fd);
 return &buf;
}

void client_path(char *path, char * hostname, char * filename) {
 sprintf(path, "./clients/%s/%s", hostname, filename);
}

struct client *process_client(char *hostname) { /* IP TS TTL HOSTNAME KEY */
 struct client *cptr;
 char *path; 

 path = malloc(255);

 if ((cptr = malloc(sizeof(struct client))) != NULL) {
   cptr->ts = (time_t) 0;
   strcpy(cptr->hostname, hostname);

   client_path(path, hostname, "KEY");
   strcpy(cptr->key, readfile(path));

   client_path(path, hostname, "TTL");
   strcpy(cptr->ttl, readfile(path));

   client_path(path, hostname, "IP");
   strcpy(cptr->ip, readfile(path));

 }

 free(path);

 fprintf(stderr, "loaded client: %s (ip: %s, ttl: %s, ts: %d)\n", cptr->hostname, cptr->ip, cptr->ttl, (int) cptr->ts);
 return cptr;
}

int process_clients() {
 DIR* dir;
 struct dirent* item;
 struct client* cptr;

 if ((dir = opendir("clients")) == NULL) {
   perror("opendir");
   return 1;
 }

 errno = 0;
 while((item = readdir(dir)) != NULL) {
  if ((strcmp(item->d_name,".") == 0) || (strcmp(item->d_name,"..") == 0))
    continue;

  cptr = process_client(item->d_name);
  if (cptr != NULL) {
   clients[cnum] = cptr;
   cnum++;
  }

  if (errno) {
    perror("readdir");
    return 1;
  }
 }

 closedir(dir);
 return 0;
}

int addr_cmp(struct sockaddr_in *in1, struct sockaddr_in *in2) {
 return memcmp(&in1->sin_addr, &in2->sin_addr, sizeof(struct in_addr));
}

int check_ip(struct sockaddr_in* addr) {
 int i = 0;
 struct sockaddr_in *saddr;

 for (i=0; i<snum; i++) {
   saddr = (struct sockaddr_in *) servers[i]->ai->ai_addr;
   if (addr_cmp(saddr, addr) == 0)
     return 0;
 }

 return 1;
}

void signal_handler(int signum) {
 fprintf(stderr, "signal cought: %d\n", signum);
 free_servers();
 free_clients();
 exit(0);
}

int process_env() {
 env_localip = getenv("IP");
 env_port = getenv("PORT");
 env_root = getenv("ROOT");
 env_threadmax = getenv("THREADMAX");

 errno = 0;
 threadmax = strtol(env_threadmax, NULL, 10);
 if (errno) {
   perror("strtol");
   return 1;
 }

 if ((env_localip == NULL) || (env_port == NULL) || (env_root == NULL) || (env_threadmax == NULL)) {
  return 1;
 } else {
  return 0;
 }
}

int droproot()
{
  char *x;
  unsigned long id;

  if (chdir(env_root) == -1)
    return 1;
  if (chroot(".") == -1)
    return 1;

  x = getenv("GID");
  if (!x)
    return 1;
  errno = 0;
  id = strtol(x, NULL, 10);
  if (errno) {
    perror("strtol");
    return 1;
  }
  if (setgid((int) id) == -1)
    return 1;

  x = getenv("UID");
  if (!x)
    return 1;
  errno = 0;
  id = strtol(x, NULL, 10);
  if (errno) {
    perror("strtol");
    return 1;
  }
  if (setuid(id) == -1)
    return 1;

  return 0;
}

int networking() {
  struct sockaddr_in addr;
  int reuse;

  if((ssock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    return 1;
  }

  reuse = 1;
  setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;

  if (inet_aton(env_localip, &addr.sin_addr) == 0) {
    fprintf(stderr, "Address in envvar IP is invalid.\n");
    return 1;
  }
  if (atoi(env_port) == 0) {
    fprintf(stderr, "Port in envvar PORT is invalid");
    return 1;
  }
  addr.sin_port = htons(atoi(env_port));

  if(bind(ssock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("bind");
    return 1;
  }

  if(listen(ssock, 5) < 0) {
    perror("listen");
    return 1;
  }
  return 0;
}

int main() {
  struct thread *t;
  int c;

  if (process_env() != 0) {
    fprintf(stderr, E_OCCURED, "process_env()");
    return 1;
  }

  if (networking() != 0) {
    fprintf(stderr, E_OCCURED, "networking()");
    return 1;
  }

  if (process_servers() != 0) {
    fprintf(stderr, E_OCCURED, "process_servers()");
    return 1;
  }

  if (droproot() == -1) {
    fprintf(stderr, E_OCCURED, "droproot()");
    return 1;
  }

  if (process_clients() != 0) {
    fprintf(stderr, E_OCCURED, "process_clients()");
    return 1;
  }

  if (signal(SIGINT, signal_handler) == SIG_IGN)
    signal(SIGINT, SIG_IGN);
  if (signal(SIGHUP, signal_handler) == SIG_IGN)
    signal(SIGHUP, SIG_IGN);
  if (signal(SIGTERM, signal_handler) == SIG_IGN)
    signal(SIGTERM, SIG_IGN);

  c = sizeof(struct sockaddr_in);

  while(1) {
   t = malloc(sizeof(struct thread));
   t->csock = accept(ssock, (struct sockaddr *)&t->cliaddr, (socklen_t*)&c);
   pthread_create(&th, NULL, comth, t);
   pthread_detach(th);
  }

  return 0;
}
