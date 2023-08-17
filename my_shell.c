#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include<stdbool.h>
#include<signal.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64
#define SIZE_OF_PATH 250
#define MAX_ZOMBIE 300

//Global Variables
int cntCmds = 0;
int zqStart = -1;
int zqEnd = -1;
int zqLen = 0;
pid_t zombieQueue[MAX_ZOMBIE];
pid_t parPid;
pid_t fgPg = -1;

/* 
* Creating a circular queue for storing the 
* backgorund processes pid
*/

//insertion into Zombie Queue/ Background Process Queue
void insertZq(pid_t p){
	if(zqLen == 0)
	{
		zqStart = zqEnd = 0;
		zombieQueue[zqStart]=p;
	}
	else
	{
		zqEnd= (zqEnd+1) % MAX_ZOMBIE;
		zombieQueue[zqEnd] = p;
	}
	++zqLen;
}

//Pop from Zombie Queue/ Backgorund Queue
pid_t frontZq(void){
	if(zqLen == 0)
	{
		return -1;
	}
	pid_t p = zombieQueue[zqStart];
	zqStart = (zqStart+1)%MAX_ZOMBIE;
	--zqLen;
	return p;

}

/* Splits the string by space and returns the array of tokens
*
*/
char **tokenize(char *line){
  char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
  char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
  int i, tokenIndex = 0, tokenNo = 0;

  for(i =0; i < strlen(line); i++){

    char readChar = line[i];

    if (readChar == ' ' || readChar == '\n' || readChar == '\t'){
      token[tokenIndex] = '\0';
      if (tokenIndex != 0){
	tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
	strcpy(tokens[tokenNo++], token);
	tokenIndex = 0; 
      }
    } else {
      token[tokenIndex++] = readChar;
    }
  }
 
  free(token);
  tokens[tokenNo] = NULL ;
  return tokens;
}

//Find the size of Token array
int findLenOfToken(char **tokensList){
	int len = 0;
	while(tokensList[len] != NULL)
	{
		++len;
	}
	return len;
}

/*
*	Function to reap all zombie processes,
*	if any process is running during exit command it kills
*	and reaps the process.
*/
void clearZombies(int killCall){
	
	int temp = zqLen;
	while(temp > 0)
	{
		int pstatus;
		pid_t zp = frontZq();
		pid_t p = waitpid(zp,&pstatus,WNOHANG);
		//printf("%d  %d\n",zp,WEXITSTATUS(status));
		if(p == 0)
		{
			if(killCall)
			{
				kill(zp,SIGKILL);
				p = waitpid(zp,&pstatus,0);

			}
			else
			{
				insertZq(zp);
			}
			
		}
		if(p > 0)
		{
			printf("Shell : Background process finished\n");
		}
		
		--temp;
	}	
	
	//cntCmds = 0;
}



/* 
*Function to handle the SIGINT,
* It calls killpg system call on recent foreground process 
*/
void funcSignal(int signalNo){
	if(fgPg == -1)
	{
		return ;
	}
	if(signalNo == SIGINT)
	{
		setpgid(fgPg,fgPg);
		killpg(fgPg,SIGKILL);
	}
}

/* Frees the memory allocated to tokens */
void freeTokens(char **tokensList){
	int i,n=findLenOfToken(tokensList);
	for(i=0;i<n;++i)
	{
		free(tokensList[i]);
	}
	free(tokensList);
}

/*
*	Main Program for Shell 
*/
int main(int argc, char* argv[]) {
	char  line[MAX_INPUT_SIZE];            
	char  **tokens;              
	int i;
	parPid = getpid();
	if(signal(SIGINT,funcSignal)==SIG_ERR)
	{
		printf("Can't catch signal\n");
	}
	
	while(1) {		

		
		clearZombies(0);
		
		/* BEGIN: TAKING INPUT */
		bzero(line, sizeof(line));
		printf("$ ");
		scanf("%[^\n]", line);
		getchar();

		
		/* END: TAKING INPUT */

		line[strlen(line)] = '\n'; //terminate with new line
		tokens = tokenize(line);
		int tokenLen = findLenOfToken(tokens);
		if(tokenLen == 0 )
		{
			free(tokens);
			continue;
		}

       //Compares the intiates the commands taken
		if(!strcmp(tokens[0],"cd"))
		{
			int cdRet;
			
			if(tokenLen == 1)
			{
				cdRet = chdir(getenv("HOME"));
			}
			else if(tokenLen >2)
			{
				printf("Incorrect cd command\n");
			}
			else
			{
				cdRet = chdir(tokens[1]);
			}
			
			if(cdRet == -1)
			{
				printf("Shell: Incorrect command\n");
			}
		}
		else if(!strcmp(tokens[0],"pwd"))
		{
			char ch[SIZE_OF_PATH];
			if(getcwd(ch,sizeof(ch)) != NULL)
			{
				printf("Current working directory : %s\n",ch);
			}
			else
			{
				printf("Error getting current directory's information\n");
			}
		}
		else if(!strcmp(tokens[0],"exit"))
		{
			{
				clearZombies(1);
				freeTokens(tokens);
				break;
			}
		}
		else
		{
			/* Handles background Processes */
			bool bgProc = false;
			if(!strcmp(tokens[tokenLen-1],"&"))
			{
				tokens[tokenLen - 1] = NULL;
				bgProc = true;
				pid_t bgChildCmd = fork();
				
				if(bgChildCmd < 0)
				{
					fprintf(stderr,"%s\n","Unable to create a new background child process\n");
				}
				else if(bgChildCmd == 0)
				{	
					
					execvp(tokens[0],tokens);
					printf("Command Execution has Failed\n");
					_exit(1);
				}
				else
				{	
					setpgid(bgChildCmd,bgChildCmd);
					insertZq(bgChildCmd);
				}
				
			}
			
			/* Handles all foreground Processes */
			else
			{
				pid_t fgChildCmd = fork();
				
				if(fgChildCmd < 0)
				{
					fprintf(stderr,"%s\n","Unable to create a new child process\n");
				}	
				else if(fgChildCmd == 0)
				{	
					
					execvp(tokens[0],tokens);
					printf("Command Execution has Failed\n");
					_exit(1);
				}
				else
				{
					//setpgid(fgChildCmd,fgChildCmd);
					
					int fgChildstat;
					fgPg = fgChildCmd;
					pid_t parWait = waitpid(fgChildCmd);
					// if(parWait == -1)
					// {
					// 	printf("Error reaping foreground child\n");
					// }
					// else
					// {
					// 	printf("Successfully reaped foreground child\n");
					// }
				}	
			}
			
		}
	
		// Freeing the allocated memory	
		freeTokens(tokens);

	}
	return 0;
}
