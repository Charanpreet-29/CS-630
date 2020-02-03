#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <setjmp.h>

#define BUFLEN 128
#define INSNUM 8
/*internal instructions*/
char *instr[INSNUM] = {"show","set","export","unexport","show","exit","wait","team"};
/*predefined variables*/
/*varvalue[0] stores the rootpid of xssh*/
/*varvalue[3] stores the childpid of the last process that was executed by xssh in the background*/
int varmax = 3;
char varname[BUFLEN][BUFLEN] = {"$\0", "?\0", "!\0",'\0'};
char varvalue[BUFLEN][BUFLEN] = {'\0', '\0', '\0'};
/*remember pid*/
int childnum = 0;
pid_t childpid = 0;
pid_t rootpid = 0;
/*current dir*/
char rootdir[BUFLEN] = "\0";

//jmp_buf  JumpBuffer;

/*functions for parsing the commands*/
int deinstr(char buffer[BUFLEN]);
void substitute(char *buffer);

/*functions to be completed*/
int xsshexit(char buffer[BUFLEN]);
void show(char buffer[BUFLEN]);
void team(char buffer[BUFLEN]);
int program(char buffer[BUFLEN]);
void ctrlsig(int sig);
void waitchild(char buffer[BUFLEN]);
void set(char buffer[BUFLEN]);
void export(char buffer[BUFLEN]);
void unexport(char buffer[BUFLEN]);

/*for extra credit, implement the function below*/
int pipeprog(char buffer[BUFLEN]);
//
void run_exec (int in, int out, char *cmd[]);


void handle_child_end(int sig)
{
	int saved_errno=errno;
	pid_t echildpid=0;
	while ((echildpid=waitpid((pid_t)(-1), 0 , WNOHANG)) > 0) 
	{
		//printf("Child %d exited\n", (int)echildpid);		//DEBUG
		childnum--;
	}
	errno=saved_errno;
}
//Used in program redirection function to add space betwee < and >
void add_space(char *buffer,char * ptr1)
{
	char new=' ';
	char temp=' ';
	int q=0;
	for (q=0 ;ptr1[q]!='\0';q++)
	{
		temp=ptr1[q];
		ptr1[q]=new;
		new=temp;
	}
	ptr1[q]=new;
	ptr1[q+1]='\0';
}
//checks if str is a valid shell variable
int valid_variable(char *str)
{
	if(strlen(str)==0 || strchr(str,' ')!=NULL || isdigit(str[0]) )
	{
		return 0;
	}
	
	int r=0;
	for (r=0;r<strlen(str);r++)
	{
		if( !isalnum(str[r]) && str[r]!='_' ) 
		{
			return 0;
		}
	}
	return 1;

}

/*main function*/
int main()
{
	/*set the variable $$*/
	rootpid = getpid();
	childpid = rootpid;
	sprintf(varvalue[0], "%d\0", rootpid);
	
	//sigaction to handle child termination
	struct sigaction sa;
	sa.sa_handler = &handle_child_end;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
	if (sigaction(SIGCHLD, &sa, 0) == -1) 
	{
		perror("Sigaction error ");
		exit(1);
	}
		
	/*capture the ctrl+C*/
	if(signal(SIGINT, ctrlsig) == SIG_ERR)
	{
		printf("-xssh: Error on signal ctrlsig\n");
		exit(0);
	}
	
	//sigaction to handle ^C 
	// struct sigaction sa_int;
	// sa_int.sa_handler = &ctrlsig;
	// sigemptyset(&sa_int.sa_mask);
	// if (sigaction(SIGINT, &sa_int, 0) == -1) 
	// {
		// perror("Sigaction error ");
		// exit(1);
	// }
	
	/*run the xssh, read the input instrcution*/
	int xsshprint = 0;
	if(isatty(fileno(stdin))) xsshprint = 1;
	if(xsshprint) printf("xssh>> ");
	char buffer[BUFLEN];
	while(fgets(buffer, BUFLEN, stdin) > 0)
	{
		/*substitute the variables*/
		substitute(buffer);
		/*delete the comment*/
		char *p = strchr(buffer, '#');
		if(p != NULL)
		{
			*p = '\n';
			*(p+1) = '\0';
		}
		/*decode the instructions*/
		int ins = deinstr(buffer);
		/*run according to the decoding*/
		if(ins == 1)
			show(buffer);
		else if(ins == 2)
			set(buffer);
		else if(ins == 3)
			export(buffer);
		else if(ins == 4)
			unexport(buffer);
		else if(ins == 5) show(buffer); //Not used for now
		else if(ins == 6)
			//return xsshexit(buffer);
		{	int retval = xsshexit(buffer);
			sprintf(varvalue[1], "%d\0", retval);
		}
		else if(ins == 7)
			waitchild(buffer);
		else if(ins == 8)
			team(buffer);
		else if(ins == 9)
			continue;
		else
		{
			char *ptr = strchr(buffer, '|');
			if(ptr != NULL)
			{
				int err = pipeprog(buffer);
				sprintf(varvalue[1], "%d\0", err);
			}
			else
			{
				int err = program(buffer);
				sprintf(varvalue[1], "%d\0", err);
			}
		}
		if(xsshprint) printf("xssh>> ");
		memset(buffer, 0, BUFLEN);
	}
	return -1;
}

/*exit I*/
int xsshexit(char buffer[BUFLEN])
{
	int i, start =4;
	if(buffer[4]!=' '&& buffer[4]!='\0'&& buffer[4]!='\n') //To Handle case where command starts with exit*. eg: "exitsfsfwef"
	{
		printf("-xssh: Unable to execute the instruction %s",buffer);
		return -1;
	}
	//start=5;
	
	char number[BUFLEN] = {'\0'};
	while(buffer[start]==' ')start++;
	for(i = start; (i < strlen(buffer))&&(buffer[i]!='\n')&&(buffer[i]!='#'); i++)
	{
		number[i-start] = buffer[i];
	}
    number[i-start] = '\0';
	
	if (strlen(number)==0) exit(0);
	
	char *endptr;
	int exitval = strtol(number, &endptr, 10);

	if((*number != '\0')&&(*endptr == '\0'))
	{
		exit(exitval);
	}
	else //invalid argument
	{
		exit(-1);
	}
	
	//FIXME: exit with a return value I that is stored in buffer
	//hint: where is the start of the string of return value I?
	//printf("Replace me with code for exit I\n");
}

/*show W*/
void show(char buffer[BUFLEN])
{
	//FIXME: print the string after "show " in buffer
	//hint: where is the start of this string?
	//printf("Replace me with code for show W\n");
	int start = 5;
	while(buffer[start]==' ')start++;
	printf("%s",buffer+start);
}

/*team T*/
void team(char buffer[BUFLEN])
{
	//FIXME: print the members of your team in the format "team members: xxx; yyy; zzz" in one line
	printf("Team Members: Anjo Sam Thomas; Charanpreet Kaur; Soumyadeep Basu; finished optional (a) and (b)\n");
}

/*export variable --- set the variable name in the varname list*/
void export(char buffer[BUFLEN])
{
	int i, j;
	//flag == 1, if variable name exists in the varname list
	int flag = 0;
	//parse and store the variable name in buffer[]
	char str[BUFLEN];
	int start = 7;
	while(buffer[start]==' ')start++;
	
	for(i = start; (i < strlen(buffer))&&(buffer[i]!='#')&&(buffer[i]!='\n'); i++)
	{
			str[i-start] = buffer[i];
	}
	str[i-start] = '\0';
	
	if(!valid_variable(str))
	{
		printf("-xssh: Export error.\nUsage: export <variable>\nwhere variable can contain alphabets, underscores and numbers but does not start with number.\n");
		sprintf(varvalue[1], "-1\0");
		return;
	}	
	
	for(j = 0; j < varmax; j++)
	{
		//FIXME: if the variable name (in "str") exist in the
		//varname list (in "varname[j]"), set the flag to be 1
		//using strcmp()

		if(strcmp(str,varname[j]) == 0)
		{
			flag = 1;
			break;
		}
	}
	if(flag == 0) //variable name does not exist in the varname list
	{
		//FIXME: copy the variable name to "varname[varmax]" using strcpy()
		//FIXME: set the corresponding value in "varvalue[varmax]" to empty string '\0'
		//FIXME: update the 'varmax' (by +1)
		//FIXME: print "-xssh: Export variable str.", where str is newly exported variable name
		//printf("Replace me with code for export W\n");
		strncpy( varname[varmax], str, BUFLEN );
		varvalue[varmax][0]='\0';
		printf("-xssh: Exported variable %s.\n",varname[varmax]);
		varmax++;
	}
	else //variable name already exists in the varname list
	{
		//FIXME: print "-xssh: Existing variable str is value.", where str is newly exported variable name and value is its corresponding value (stored in varvalue list)
		if (varvalue[j][0]=='\0')
		{
			printf("-xssh: Variable \"%s\" already exists but its value is not set.\n",varname[j]);		
		}
		else
		{
			printf("-xssh: Variable \"%s\" already exists and its value is \"%s\".\n",varname[j],varvalue[j]);
		}
	}
}

/*unexport the variable --- remove the variable name in the varname list*/
void unexport(char buffer[BUFLEN])
{
        int i, j;
	//flag == 1, if variable name exists in the varname list
        int flag = 0;
	//parse and store the variable name in buffer[]
        char str[BUFLEN];
	int start = 9;
	while(buffer[start]==' ')start++;
	for(i = start; (i < strlen(buffer))&&(buffer[i]!='#')&&(buffer[i]!=' ')&&(buffer[i]!='\n'); i++)
	{
			str[i-start] = buffer[i];
	}
	str[i-start] = '\0';
	
	if(!valid_variable(str))
	{
		printf("-xssh: Unexport error.\nUsage: unexport <variable>\nwhere variable can contain alphabets, underscores and numbers but does not start with number.\n");
		sprintf(varvalue[1], "-1\0");
		return;
	}	
	
	for(j = 0; j < varmax; j++)
	{
		//FIXME: if the variable name (in "str") exist in the
		//varname list (in "varname[j]"), set the flag to be 1
		//using strcmp() --- same with export()
		if(strcmp(str,varname[j]) == 0)
		{
		flag = 1;
		break;
		}
	}
	if(flag == 0) //variable name does not exist in the varname list
	{
		//FIXME: print "-xssh: Variable str does not exist.",
		//where str is the variable name to be unexported
		printf("-xssh: Variable \"%s\" does not exist.\n",str);		
	}
	else //variable name already exists in the varname list
	{
		//FIXME: clear the found variable by setting its
		//"varname" and "varvalue" both to '\0'
		//FIXME: print "-xssh: Variable str is unexported.",
		//where str is the variable name to be unexported
		//printf("Replace me with code for unexport W\n");
		varname[j][0]='\0';
		varvalue[j][0]='\0';
		printf("-xssh: Variable \"%s\" has been unexported.\n",str);
	}
}

/*set the variable --- set the variable value for the given variable name*/
void set(char buffer[BUFLEN])
{
	int i, j;
	//flag == 1, if variable name exists in the varname list
	int flag = 0;
	//parse and store the variable name in buffer[]
	char str[BUFLEN];
	int start = 4;
	while(buffer[start]==' ')start++;
	for(i = start; (i < strlen(buffer))&&(buffer[i]!=' ')&&(buffer[i]!='#')&&(buffer[i]!='\n'); i++)
	{
		str[i-start] = buffer[i];
	}
	str[i-start] = '\0';
	
	if(!valid_variable(str))
	{
		printf("-xssh: Set error.\nUsage: set <variable> <value>\nwhere variable can contain alphabets, underscores and numbers but does not start with number.\n");
		sprintf(varvalue[1], "-1\0");
		return;
	}	
	while(buffer[i]==' ')i++;
	if((buffer[i]=='\n'))
	{
		printf("No value to set!\n");
		sprintf(varvalue[1], "-1\0");
		return;
	}
	for(j = 0; j < varmax; j++)
	{
		if(strcmp(str,varname[j]) == 0)
		{
			flag = 1;
			break;
		}
		//FIXME: if the variable name (in "str") exist in the
		//varname list (in "varname[j]"), set the flag to be 1
		//using strcmp() --- same with export()
	}
	if(flag == 0)
	{
		printf("-xssh: Variable \"%s\" does not exist.\n",str);		
		sprintf(varvalue[1], "-1\0");
		//FIXME: print "-xssh: Variable str does not exist.",
		//where str is the variable name to be unexported
		//printf("Replace me with code for set W1 W2\n");
	}
	else
	{
		char value[BUFLEN];
		int valstart;
		for ( valstart = i; (i < strlen(buffer))&&(buffer[i]!=' ')&&(buffer[i]!='#')&&(buffer[i]!='\n'); i++)
		{
			value[i-valstart]=buffer[i];
		}
		value[i-valstart]='\0';
		strncpy( varvalue[j], value, BUFLEN );
		printf("-xssh: Set existing variable \"%s\" to \"%s\".\n",varname[j],varvalue[j]);
			
		//hint: try to print "buffer[i]" to see what's stored there
		//hint: may need to add '\0' by the end of a string
		//FIXME: set the corresponding varvalue to be value (in buffer[i]) using strcpy()
		//FIXME: print "-xssh: Set existing variable str to value.", where str is newly exported variable name and value is its corresponding value (stored in varvalue list)
		//printf("Replace me with code for set W1 W2\n");
	}
}


/*ctrl+C handler*/
void ctrlsig(int sig)
{
	if(childpid!=rootpid)
	{
		printf("\n-xssh: Exited pid %d\n",childpid);
		fflush(stdout);
		childpid=rootpid;
	}
	else
	{	printf("\nxssh>> ");
		fflush(stdout);
	}
	//FIXME: first check if the foreground process (pid stored in childpid) is xssh itself (pid stored in rootpid)
	//FIXME: if it is not xssh itself, kill the foreground process and print "-xssh: Exit pid childpid", where childpid is the pid of the current process
	//hint: remember to put the code "fflush(stdout);" after printing the message above for a clear output
	//printf("Replace me for ctrl+C handler\n");
}

/*wait instruction*/
void waitchild(char buffer[BUFLEN])
{
	int i;
	int start = 5;

	/*store the childpid in pid*/
	char number[BUFLEN] = {'\0'};
	while(buffer[start]==' ')start++;
	for(i = start; (i < strlen(buffer))&&(buffer[i]!='\n')&&(buffer[i]!='#'); i++)
	{
		number[i-start] = buffer[i];
	}
        number[i-start] = '\0';
	char *endptr;
	int pid = strtol(number, &endptr, 10);

	/*simple check to see if the input is valid or not*/
	if((*number != '\0')&&(*endptr == '\0'))
	{
		int wstatus;
		if (pid>0)
		{			
			if(	waitpid(pid,&wstatus,0) ==-1)
			{
				printf("-xssh: Unsuccessful wait for the background process %d\n",pid);
				sprintf(varvalue[1], "-1\0");
			}
			else
			{
				printf("-xssh: Have finished waiting for process %d\n",pid);
				childnum--;
			}
		}
		else if(pid==-1)
		{
			if(childnum<1)
			{
				printf("-xssh: No background processes running.\n");
				sprintf(varvalue[1], "-1\0");
				return;
			}
			
			printf("-xssh: Waiting for %d background process(es)\n",childnum);
			while(childnum!=0)
			{
				int wait_child = waitpid(pid,&wstatus,0);
				if(wait_child==-1)
				{
					printf("-xssh: Unsuccessful wait for the background process\n");
					sprintf(varvalue[1], "-1\0");
				}
				else
				{
					printf("-xssh: Have finished waiting for process %d\n",wait_child);
					childnum--;
				}
			}
		}
		//FIXME: if pid is not -1, try to wait the background process pid
		//FIXME: if successful, print "-xssh: Have finished waiting process pid", where pid is the pid of the background process
		//FIXME: if not successful, print "-xssh: Unsuccessfully wait the background process pid", where pid is the pid of the background process


		//FIXME: if pid is -1, print "-xssh: wait childnum background processes" where childnum stores the number of background processes, and wait all the background processes
		//hint: remember to set the childnum correctly after waiting!
		//printf("Replace me for wait P\n");

	}
	else printf("-xssh: wait: Invalid pid\n");
	sprintf(varvalue[1], "-1\0");
}

/*execute the external command*/
int program(char buffer[BUFLEN])
{
	/*if backflag == 0, xssh need to wait for the external command to complete*/
	/*if backflag == 1, xssh need to execute the external command in the background*/
	int backflag = 0;
	char *ptr = strchr(buffer, '&');
	if(ptr != NULL) backflag = 1;

	pid_t pid;
	
	char instr[BUFLEN];
	strcpy(instr,buffer);
	
	char *args[BUFLEN];
	int c=0;
	
	if(buffer[strlen(buffer)-1]=='\n'){
		buffer[strlen(buffer)-1]='\0';
	}
	if(strcmp(buffer,"show")==0)  //to handle case where command is just 'show'
	{
		printf("\n");
		return 0;
	}
	
	if(strcmp(buffer,"wait")==0)  //to handle case where command is just 'wait'
	{
		printf("Usage: wait <P>, where P>-2.\n");
		return -1;
	}
	
	if(strcmp(buffer,"export")==0||strcmp(buffer,"unexport")==0||strcmp(buffer,"set")==0)
	{
		printf("-xssh: Please pass the required arguments.\n");
		return -1;
	}
	
	if(buffer[strlen(buffer)-1]=='&'){
		buffer[strlen(buffer)-1]='\0';
	}
//--------------Redirection handling-----------------
	char *ptr1 = strchr(buffer, '<');
	char *ptr2 = strchr(buffer, '>');
	char *str;
	int in =0,out =0;
	if(ptr1 || ptr2)
	{
		if(ptr1!=NULL)
		{	add_space(buffer,ptr1);
			add_space(buffer,ptr1+2);
		}
		ptr2 = strchr(buffer, '>');

		if(ptr2!=NULL)
		{	add_space(buffer,ptr2);
			add_space(buffer,ptr2+2);
		}
		//printf("%s\n",buffer); //DEBUG
		
		str= strtok(buffer," ");
		while (str!=NULL)
		{
			args[c]= str;
			if(strcmp(str,"<")==0){
				in=c;
				c--;
			}
			
			if(strcmp(str,">")==0){
				out=c;
				c--;
			}
			if(strcmp(str,"&")==0){
				c--;
			}
			c++;
			str= strtok(NULL," ");
		}
		args[c]=NULL;
		// if ( strcmp(args[c-1],"&")==0){
			// args[c-1]=NULL;
		// }
//DEBUG		
		// int m=0;
		// for(m=0;m<=c;m++)
			// printf("Args: %s\n",args[m]);
//DEBUG		
	
		pid=fork();
		if (pid==-1)			//Fork error
		{			
			perror("-xssh: Process creation error ");
			return -2;
		}
		else if (pid==0)		//Child
		{
			if(in)				//If < exist, read from file
			{
				int fread;
				if( (fread=open(args[in],O_RDONLY,0))==-1  )
				{
					perror("File open error ");
					return -1;
				}
				dup2(fread,0);
				close(fread);
				args[in]=NULL;
			}
			if(out)				//If > exist, redirect to file
			{
				int fwrite;
				if( (fwrite=open(args[out],O_WRONLY | O_TRUNC | O_CREAT , S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))==-1  )
				{
					perror("File write error ");
					return -1;
				}
				dup2(fwrite,1);
				close(fwrite);
				args[out]=NULL;
			}
			if(execvp(args[0],args) ==-1)
			{
				printf("-xssh: Unable to execute the instruction %s",instr);
				return -1;
			}
			
		}
		else					//Parent
		{	//printf("Child %d created.\n",(int)pid);  //DEBUG
			if(backflag==0)		//foreground process handling
			{	
				childpid=pid;
				int wstatus;
				waitpid(pid,&wstatus,0);
				childpid=rootpid;
				fflush(stdout);
				fflush(stdin);
				if(WIFEXITED(wstatus))
				{
					return(WEXITSTATUS(wstatus));	
				}
			}
			else				//background process handling
			{
				childnum++;
				sprintf(varvalue[2], "%d\0", pid);
				//printf("Childnum: %d\n",childnum);		//DEBUG
			}
		}
	
		return -1;
	}
	
//------------Execute without redirection------------
	args[c]= strtok(buffer," ");
	
	while (args[c]!=NULL){
		c++;
		args[c]= strtok(NULL," ");
	}
	if ( strcmp(args[c-1],"&")==0){
		args[c-1]=NULL;
	}
	
	pid=fork();
	if (pid==-1)			//Fork error
	{			
		perror("-xssh: Process creation error ");
		return -2;
	}
	else if (pid==0)		//Child
	{
		if(execvp(args[0],args) ==-1)
		{
			printf("-xssh: Unable to execute the instruction %s",instr);
			exit(-1);
		}
	}
	else					//Parent
	{	//printf("Child %d created.\n",(int)pid);  //DEBUG
		if(backflag==0)		//foreground process handling
		{	
			childpid=pid;
			int wstatus;
			waitpid(pid,&wstatus,0);
			childpid=rootpid;
			fflush(stdout);
			fflush(stdin);
			if(WIFEXITED(wstatus))
			{
				return(WEXITSTATUS(wstatus));	
			}
		}
		else			//background process handling
		{
			childnum++;
			sprintf(varvalue[2], "%d\0", pid);
			//printf("Childnum: %d\n",childnum);		//DEBUG
		}
	}
		
	return -1;
	//FIXME: create a new process for executing the external command

	//FIXME: remember to check if the process creation is successful or not. if not, print error message and return -2, see codes below;


	//FIXME: write the code to execute the external command in the newly created process, using execvp()
	//hint: the external command is stored in buffer, but before execute it you may need to do some basic validation check or minor changes, depending on how you execute
	//FIXME: remember to check if the external command is executed successfully; if not, print error message "-xssh: Unable to execute the instruction buffer", where buffer is replaced with the actual external command to be printed
	//hint: after executing the extenal command using execvp(), you need to return -1;
	/*for extra credit, implement stdin/stdout redirection in here*/

	//printf("Replace me for executing external commands\n");

	//FIXME: in the xssh process, remember to act differently, based on whether backflag is 0 or 1
	//hint: the codes below are necessary to support command "wait -1", but you need to put them in the correct place
	//		childnum++;
	//		childnum--; //this may or may not be needed, depending on where you put the previous line
	//hint: the code below is necessary to support command "show $!", but you need to put it in the correct place
			
}

/*for extra credit, implement the function below*/
/*execute the pipe programs*/
int pipeprog(char buffer[BUFLEN])
{
	
	if(buffer[strlen(buffer)-1]=='\n'){
		buffer[strlen(buffer)-1]='\0';
	}	
	
	int pipecount=0;
	char *p=strchr(buffer,'|');
	while(p!=NULL)
	{
		pipecount++;
		p=strchr(p+1,'|');
	}
	
	int c=0,k=0;
	char *commands[BUFLEN];
	commands[c]= strtok(buffer,"|");
	
	while (commands[c]!=NULL){
		c++;
		commands[c]= strtok(NULL,"|");
	}
	
	char *commandset[BUFLEN][BUFLEN];
	int d=0,a=0;
	for (d=0;d<c;d++)
	{
		a=0;
		commandset[d][a]=strtok(commands[d]," ");
		while (commandset[d][a]!=NULL){
			a++;
			commandset[d][a]= strtok(NULL," ");
		}
		commandset[d][a+1]='\0';
	}
	
	int e=0, in=0;
	for(e=0;e<pipecount;e++)
	{
		int fd[2];
		
		pid_t pid;
		if(pipe(fd)==-1) {
			perror("Pipe error ");
			return -1;
		}
		if ((pid=fork()) ==-1){
			perror ("Fork error");
			return -1;
		}
		if (pid==0) //child
		{
			close(fd[0]);		//child dont need in-pipe (read pipe)
			run_exec(in, fd[1],commandset[e]);
		}
		else 		//Parent
		{
			close(fd[1]);   //parent dont need out-pipe (write pipe)
			if(in!=0) close(in);
			int wstatus;
			waitpid(pid,&wstatus,0);
			in=fd[0];
		}
	}
	
	pid_t pid2;
	if ((pid2=fork()) ==-1){
		perror ("Fork error");
		return -1;
	}
	if (pid2==0) //child
	{
		run_exec(in, 1,commandset[e]);	
	}
	else
	{	
		int wstatus2;
		waitpid(pid2,&wstatus2,0);
	}
	
	//printf("-xssh: For extra credit: currently not supported.\n");
	return 0;
}

void run_exec (int in, int out, char *cmd[])
{
	if(in!=0)
	{
		dup2(in,0);
		close(in);
	}
	if(out!=1)
	{
		dup2(out,1);
		close(out);
	}
	execvp(cmd[0],cmd);
}


/*substitute the variable with its value*/
void substitute(char *buffer)
{
	char newbuf[BUFLEN] = {'\0'};
	int i;
	int pos = 0;
	for(i = 0; i < strlen(buffer);i++)
	{
		if(buffer[i]=='#')
		{
			newbuf[pos]='\n';
			pos++;
			break;
		}
		else if(buffer[i]=='$')
		{
			if((buffer[i+1]!='#')&&(buffer[i+1]!=' ')&&(buffer[i+1]!='\n'))
			{
				i++;
				int count = 0;
				char tmp[BUFLEN];
				for(; (buffer[i]!='#')&&(buffer[i]!='\n')&&(buffer[i]!=' '); i++)
				{
					tmp[count] = buffer[i];
					count++;
				}
				tmp[count] = '\0';
				int flag = 0;
        		int j;
				for(j = 0; j < varmax; j++)
				{
					if(strcmp(tmp,varname[j]) == 0)
					{
					flag = 1;
					break;
					}
				}
				if(flag == 0)
				{
				printf("-xssh: Variable \"%s\" does not exist.\n", tmp);
				}
				else
				{
				strcat(&newbuf[pos], varvalue[j]);
				pos = strlen(newbuf);
				}
				i--;
			}
			else
			{
				newbuf[pos] = buffer[i];
				pos++;
			}
		}
		else
		{
			newbuf[pos] = buffer[i];
			pos++;
		}
	}
	if(newbuf[pos-1]!='\n')
	{
		newbuf[pos]='\n';
		pos++;
	}
	newbuf[pos] = '\0';
	strcpy(buffer, newbuf);
	//printf("Decode: %s", buffer);
}

/*decode the instruction*/
int deinstr(char buffer[BUFLEN])
{
	int i;
	int flag = 0;
	for(i = 0; i < INSNUM; i++)
	{
		flag = 0;
		int j;
		int stdlen = strlen(instr[i]);
		int len = strlen(buffer);
		int count = 0;
		j = 0;
		while(buffer[count]==' ')count++;
		if((buffer[count]=='\n')||(buffer[count]=='#'))
		{
			flag = 0;
			i = INSNUM;
			break;
		}
		for(j = count; (j < len)&&(j-count < stdlen); j++)
		{
			if(instr[i][j] != buffer[j])
			{
				flag = 1;
				break;
			}
		}
		if((flag == 0) && (j == stdlen) && (j <= len) && (buffer[j] == ' '))
		{
			break;
		}
		else if((flag == 0) && (j == stdlen) && (j <= len) && (i == 5))
		{
			break;
		}
		else if((flag == 0) && (j == stdlen) && (j <= len) && (i == 7))
		{
			break;
		}
		else
		{
			flag = 1;
		}
	}
	if(flag == 1)
	{
		i = 0;
	}
	else
	{
		i++;
	}
	return i;
}











