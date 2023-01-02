/*
 * @author Daphine Kirubi
 * @date 11/30/22
 * A shell of my own
 */

#define _GNU_SOURCE  
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <ctype.h>
#include <fcntl.h>

/**
 * It prints the current working directory
 * 
 * @param cmd the command and its arguments
 * @param cmdlen the number of arguments in the command
 * 
 */
bool cmd_pwd (char ** cmd, int cmdlen)
{
  if (0 != strcmp(cmd[0], "pwd")) return false;

  if(cmdlen != 1){
    fprintf(stderr,"error : pwd has no arguments");
  }
   
   char cwd[1024];
   getcwd(cwd, sizeof(cwd));
   printf("%s\n",cwd);     
  
  return true;
}


/**
 * Takes in a command and its length, and if the command is `set`, it sets the
 * environment variable to the enviornment passed in by user
 * 
 * @param cmd the array of strings that the user typed in
 * @param cmdlen the number of arguments in the command
 *
 */
bool cmd_set (char ** cmd, int cmdlen){
   if (0 != strcmp(cmd[0], "set")) return false;

   if(cmdlen == 3){ 
    if(setenv(cmd[1],cmd[2],1) < 0){   //if setenv fails
      fprintf(stderr,"error: cannot set environment");
    }
   }
   else{  //set should be passed in with 3 commands
    return false;
   }

   return true;
}



/**
 * `cmd_exit` checks if the first argument is `exit` and if so, exits the program
 * 
 * @param cmd The command that the user entered.
 * @param cmdlen the number of arguments in the command
 *
 */
bool cmd_exit (char ** cmd, int cmdlen)
{
  if (0 != strcmp(cmd[0], "exit")) return false;

  if(cmdlen == 1){
    exit(0);
  }

  //exits with command
  if(cmdlen == 2)   
  {
   exit(atoi(cmd[1]));
  }

  return true; 
}

/**
 * 'cmd_cd' changes the current working directory to the directory specified by the user
 * 
 * @param cmd The command line, split into an array of strings.
 * @param cmdlen the number of arguments in the command
 */
bool cmd_cd (char ** cmd, int cmdlen)
{
  if (0 != strcmp(cmd[0], "cd")) return false;

  if(cmdlen == 1){
    chdir(getenv("HOME"));
  }
  else if(cmdlen == 2 && chdir(cmd[1]) == 0 ){
      chdir(cmd[1]);
  } 
  else{
    fprintf(stderr,"error: No such directory");   
    return false;
  }
  return true;
}

/**
 * 'try_external' forks a child process and then executes the command in the child process. If the
 * command is to be run in the background, the parent process does not wait for the child process to
 * finish
 * 
 * @param cmd the command to be executed
 * @param cmdlen the length of the command
 */
void try_external (char ** cmd,int cmdlen)
{
  pid_t  pid;
  pid_t status;
  bool background;
  
  if(0 == strcmp(cmd[cmdlen-1],"&")){ //check if the process should run in the background
      background = true;
      cmd[cmdlen-1] = NULL;  //remove '&' so exec works
    }
  cmd[cmdlen] = NULL; //add null so exec knows when to stop

  pid = fork();

  if ( pid < 0) {     // fork failed        
      fprintf(stderr,"error: forking child process failed");
      exit(1);
     }
  if (pid == 0){
    if(execvp(cmd[0],cmd)== -1){  //exec failed
      fprintf(stderr,"error: exec failed");
      exit(1);
    }
  }   
  else {                              
    if(!background){         
        while (wait(&status) != pid);    //parent waits for child
         }                                                    
  }
   background = false;  
}

/**
 * > Reap all finished child processes
 */
void reap_all_finished_children (void)
{ 
  int status = 0 ;
  pid_t pid ;
 
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {   
    // Reap child process 

    if (pid == 0) { //no process to reap
      break ;
   }
   }   
}

void sighandler (int){}

/**
 * 'is_Blank' checks if the string is empty or not.
 * 
 * @param c the string to be checked
 * 
 * @return  boolean value.
 */
bool isBlank(char c[]) {
  for (int i = 0; i < strlen(c); i++) {
    if (isspace(c[i]) == 0) {
      return false;
    }
    }
    //only white spaces ;retuns true
    return true;
  }


int main (int argc, char * argv[])
{
  //Check argc/argv
  if(argc != 1){
    fprintf(stderr,"./mhcsh doesnt take arguments\n");
    exit(1);
  }

  // This makes it so we ignore if the user types Ctrl-C
  signal(SIGINT, sighandler);

  
  char * raw_input = NULL;
  size_t raw_input_len = 0;    
  char * cmd[255] ;      //array of commands
  int cmdlen = 0 ; 

  while (true)
  {
    printf("mhcsh> ");

    //set PWD variable
    char cwd[1024];    
    setenv("PWD", getcwd(cwd, sizeof(cwd)), 1);
    
    //Print prompt and read input
    int charRead = getline(&raw_input,&raw_input_len, stdin);

    //Checks for bad input and EOF
    if (charRead == -1){ 
    if (feof(stdin)) {  //received eof
      exit(0);
    }
      else{      //bad input 
        exit(1) ;
      }
    }
   
    // Background processes may have quit; 
    reap_all_finished_children();

    //handle funny input cases here, 
    if (raw_input[charRead -1] == '\n'){  //trim newline
      raw_input[charRead-1] = '\0' ;
    }

    //check for correct user input
    if(raw_input[0]=='\0')continue;    //ignore enter only
    if(isBlank(raw_input))continue;      //ignore blank input
    
    //Break commandline into "words".
    char *token = strtok(raw_input," ");
    int i = 0 ;
    cmdlen = 0 ;
    
    while(token != NULL){
      cmd[i] = token ;
      cmdlen ++;
      token = strtok(NULL," ");
      i++;
    }
      
    if (cmd_exit(cmd,cmdlen)) continue;
    if (cmd_cd(cmd,cmdlen)) continue;
    if (cmd_pwd(cmd,cmdlen)) continue;
    if (cmd_set(cmd,cmdlen)) continue;

  
    try_external(cmd,cmdlen);
    printf("\n");
   
  }
  free(raw_input);
  return 0;
}
