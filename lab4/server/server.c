#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <netdb.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <arpa/inet.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <time.h>

#define MAX 256
#define PORT 1234

int n, cfd; //made cfd global for easier use between functions

char ans[MAX];
char line[MAX];

// LAB4 CODE =================================================================

char gpath[128];    // hold token strings
char *arg[64];      // token string pointers
char cwd[128];     // current working directory
int  n;             // number of token strings

//Function Declarations ===========
int lab4_run_command(char* cmd);
int tokenize(char *pathname);
int my_ls(void);
int my_cd(void);
int my_pwd(void);
int my_mkdir(void);
int my_rmdir(void);
int my_rm(void);
int ls_file(char* fname);
int ls_dir(char* dname);
int my_get(void);
int send_packet(int end);
//=================================

int lab4_run_command(char* cmd){ // 'main' function that runs lab4
  
  tokenize(cmd); //tokenize cmd and put into arg[]
  
  
  strcpy(ans, ""); //resets ans to start building new message that will be send back to client later


  if(strcmp(arg[0], "ls") == 0){ // cmd: ls
    return my_ls();
  }

  if(strcmp(arg[0], "cd") == 0){ // cmd: cd
    return my_cd();
  }

  if(strcmp(arg[0], "pwd") == 0){ // cmd: pwd
    return my_pwd();
  }

  if(strcmp(arg[0], "mkdir") == 0){ // cmd: mkdir
    return my_mkdir();
  }

  if(strcmp(arg[0], "rmdir") == 0){ // cmd: rmdir
    return my_rmdir();
  }

  if(strcmp(arg[0], "rm") == 0){ // cmd: rm
    return my_rm();
  }

  if(strcmp(arg[0], "get") == 0){ // cmd: rm
    return my_get();
  }


  //CMD is not defined
  strcat(ans, "Invalid cmd: ");
  strcat(ans, arg[0]);
  strcat(ans, "\n");


  return -1;
}

int tokenize(char *pathname) // Tokenize from previous lab
{                            
  char *s;
  strcpy(gpath, pathname);   // copy into global gpath[]
  s = strtok(gpath, " ");    
  n = 0;
  while(s){
    arg[n++] = s;           // token string pointers  
    s = strtok(0, " ");
  }
  arg[n] =0;                // arg[n] = NULL pointer
}

int my_cd(void){

  return chdir(arg[1]);
}

int my_pwd(void){
  getcwd(cwd, 127);
  strcpy(ans, cwd);
  strcat(ans, "\n");

  return 0;
}

int my_mkdir(void){

  return mkdir(arg[1], 0755);
}

int my_rmdir(void){

  return rmdir(arg[1]);
}

int my_rm(void){

  return unlink(arg[1]);
}

int my_ls(void){
  
  getcwd(cwd, 127);
  return ls_dir(cwd);
}

int ls_file(char* fname){

  char *t1 = "xwrxwrxwr-------";
  char *t2 = "----------------";

  struct stat fstat, *sp;
  int r, i;
  char ftime[64];
  char linkname[255];
  char char_to_add[2] = { '\0' }; //concat 1 char to ans
  char buf[64]; //concat formatted lines to ans

  sp = &fstat;
  if((r = lstat(fname, &fstat)) < 0) {

    printf("can't stat %s\n", fname);
    exit(1);

  }

  if ((sp->st_mode & 0xF000) == 0x8000) // if (S_ISREG())                       
    strcat(ans, "-");

  if ((sp->st_mode & 0xF000) == 0x4000) // if (S_ISDIR())                       
    strcat(ans, "d");


  if ((sp->st_mode & 0xF000) == 0xA000) // if (S_ISLNK())                       
    strcat(ans, "l");
  for(i = 8; i >= 0; i--){
    if(sp->st_mode & (1 << i)){ //print r | w | x   
      char_to_add[0] = t1[i];
      strcat(ans, char_to_add);
    }
    else{
      char_to_add[0] = t2[i];
      strcat(ans, char_to_add);   

    }                                          
   }

  sprintf(buf, "%4d ",sp->st_nlink); // link count
  strcat(ans, buf);

  sprintf(buf, "%4d ",sp->st_gid); // gid
  strcat(ans, buf);

  sprintf(buf, "%4d ",sp->st_uid); // uid   
  strcat(ans, buf);

  sprintf(buf, "%8d ",sp->st_size); // file size  
  strcat(ans, buf);

  // print time  
  strcpy(ftime, ctime(&sp->st_ctime)); // print time in calendar form           
  ftime[strlen(ftime)-1] = 0; // kill \n at end                                 
  strcat(ans, ftime);

  // print name                                                                 
  sprintf(buf, " %s \t", basename(fname)); // print file basename  
  strcat(ans, buf);

  // print -> linkname if symbolic file                                         
  if ((sp->st_mode & 0xF000)== 0xA000){

    // use readlink() to read linkname                                          
    readlink(fname, linkname, 255);
    strcat(ans, " -> "); // print linked name    
    strcat(ans, linkname);

  
  }

  return 0;

}

int ls_dir(char* dname){ //takes directory and iterates (and prints) through file content

  DIR* dir;
  struct dirent *d;
  dir = opendir(dname);

  if(dir == NULL){
    return -1;
  }

  while(d = readdir(dir)){
      
      ls_file(d->d_name);
      strcat(ans, "\n");

      send_packet(0); //send current ls line and reset ans buf to avoid overload
      strcpy(ans, "");

    
  }

  close(dir);

  return 0;
}

int my_get(void){

  FILE* rFile; //file to read
  char buf[MAX]; //where file content is read to before send_packet()
  rFile = fopen(arg[1], "r");

  if(!rFile){
    strcat(ans, "Error: Can't open file for mode read.\n");
    return -1; //can't open file for mode read
  }

  send_packet(0);
  strcpy(ans, "");
  //begin reading file content
  while(fgets(buf, MAX, rFile) != NULL){

    if((strlen(ans) + strlen(buf)) >= MAX){ //if exceeded packet size (send if over and reset ans)
      send_packet(0);
      strcpy(ans, "");
    }
    strcat(ans, buf);
    
    if(feof(rFile)){ //if reached end of file
      break;
    }
  }
  send_packet(1);
  strcpy(ans, "");

  fclose(rFile);

  return 0;
}

int send_packet(int end){ //takes ans string and writes to client (precondition: Client exists)
  char buf[MAX]; //hold verify data
  
  printf("%s -- %d\n", ans, strlen(ans));
  write(cfd, ans, MAX); //assume client hasn't disconnected

  //verify with client data was recieved
  n = read(cfd, buf, MAX); //get command from client

  if(end){ //this was the last packet 
    write(cfd, buf, MAX); //send back verify message
  }else{ //there are more packets to send (end == 0)
    char* n = &buf[0];
    *n = n+1; //change the verify message to be different
    write(cfd, buf, MAX); //send back altered verify message
  }
}

//============================================================================

int main() 
{ 
    int sfd, len; 
    struct sockaddr_in saddr, caddr; 
    int i, length;
    
    printf("1. create a socket\n");
    sfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sfd < 0) { 
        printf("socket creation failed\n"); 
        exit(0); 
    }
    
    printf("2. fill in server IP and port number\n");
    bzero(&saddr, sizeof(saddr)); 
    saddr.sin_family = AF_INET; 
    //saddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    saddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    saddr.sin_port = htons(PORT);
    
    printf("3. bind socket to server\n");
    if ((bind(sfd, (struct sockaddr *)&saddr, sizeof(saddr))) != 0) { 
        printf("socket bind failed\n"); 
        exit(0); 
    }
      
    // Now server is ready to listen and verification 
    if ((listen(sfd, 5)) != 0) { 
        printf("Listen failed\n"); 
        exit(0); 
    }

    getcwd(cwd, 127);
    chroot(cwd); //change root to cwd


    while(1){
       // Try to accept a client connection as descriptor newsock
       length = sizeof(caddr);

       cfd = accept(sfd, (struct sockaddr *)&caddr, &length);
       
       if (cfd < 0){
          printf("server: accept error\n");
          exit(1);
       }

       printf("server: accepted a client connection from\n");
       printf("-----------------------------------------------\n");
       printf("    IP=%s  port=%d\n", "127.0.0.1", ntohs(caddr.sin_port));
       printf("-----------------------------------------------\n");

       // Processing loop
       while(1){
         int cmd_success = 0;
         printf("server ready for next request ....\n");
         
         n = read(cfd, line, MAX); //get command from client
        
         if (n==0){
           printf("server: client died, server loops\n");
           close(cfd);
           break;
         }
         cmd_success = lab4_run_command(line); // custom function to run lab4 code
         
         if(strcmp(arg[0], "get") != 0){ //dont send cmd result if command is get

          if(!cmd_success){ //cmd_sucess == 0 when lab4_run_command() encountered no issues
              strcat(ans, "[CMD Successful]\n");
          }else{
              strcat(ans, "[CMD Unsuccessful]\n");
          }
           // send the ans to client 
           n = send_packet(1);
         }
         printf("server: ready for next request\n");
       }
    }
}