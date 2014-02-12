// UCLA CS 111 Lab 1 command execution

#include "command.h"
#include "command-internals.h"
#include "alloc.h"
#include <error.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

/* FIXME: You may need to add #include directives, macro definitions,
   static function definitions, etc.  */

int
command_status (command_t c)
{
  return c->status;
}

void
execute_command (command_t c, int time_travel)
{
  /* FIXME: Replace this with your implementation.  You may need to
     add auxiliary functions and otherwise modify the source code.
     You can also use external functions defined in the GNU C Library.  */
  /*time_travel = 1;
  error (time_travel, 0, "command execution not yet implemented");
  c = (struct command*) checked_malloc(sizeof(struct command));
  c->type = SEQUENCE_COMMAND;
  c->status = -1;
  c->input = NULL;
  c->output = NULL;*/

	//since we have yet to implement time travel and time travel needs
	//to be used, we put the current code into this conditional statement
	if(time_travel == 0) {
		//check that there is an actual command
		if(c == NULL)
			return;
		
		pid_t child, grandchild;
		int p_status;
		int in, out;
		int fd[2];
		
		//use switch statement to check command type and execute
		//commands accordingly
		switch(c->type){
			//for a && b, b only executes if a exists w/ zero
			case AND_COMMAND:
				execute_command(c->u.command[0],0);
				if(c->u.command[0]->status != 0)
					c->status = 1;
				else {
					execute_command(c->u.command[1],0);
					c->status = c->u.command[1]->status;
				}
				break;
		
			//just execute the left side and then the right side	
			case SEQUENCE_COMMAND:
				execute_command(c->u.command[0],0);
				execute_command(c->u.command[1],0);
				c->status = c->u.command[1]->status;
				break;

			//for a || b, b only executes if a exists w/ non-zero
			case OR_COMMAND:
				execute_command(c->u.command[0],0);
				if(c->u.command[0]->status == 0)
					c->status = 0;
				else {
					execute_command(c->u.command[1],0);
					c->status = c->u.command[1]->status;
				}
				break;

			//use fork initially so we can pipe and run one for the
			//left side and one for the right side (requiring us to 
			//fork another time inside)
			case PIPE_COMMAND:
				child = fork();
				if (child < 0)
					error(1,0, "Problems occurred during forking.");
				else if (child == 0) {
					pipe(fd);
					grandchild = fork();
					if (grandchild < 0)
						error(1,0, "Problems occurred during forking.");
					else if (grandchild == 0) {
						close(fd[0]);
						dup2(fd[1], 1);
						close(fd[1]);
						//printf("boop\n");
						execute_command(c->u.command[0],0);
						exit(c->u.command[0]->status); 
					}
					else {
						close(fd[1]);
						dup2(fd[0], 0);
						close(fd[0]);
						if (waitpid(grandchild, &p_status,0) == -1)
							error(1,0, "Failure with G child process.");
						//printf("beep\n");
						execute_command(c->u.command[1],0);
						c->u.command[0]->status = WEXITSTATUS(p_status);
						exit(c->u.command[1]->status);
					}
				}
				else {
					if (waitpid(child, &p_status, 0) == -1) 
						error(1,0, "Failure with MY child process.");
					c->u.command[1]->status = WEXITSTATUS(p_status);
					c->status = c->u.command[1]->status;
					return;
				}
				break;
					

			//fork to run the simple command in another process
			case SIMPLE_COMMAND:
				//special case: exec, check for the word and then
				//if exec is by itself, error and exit. If not, then
				//run the command immediately following exec
				if(strcmp(c->u.word[0],"exec") == 0) {
					if(c->u.word[1] == '\0')
						error(1,0,"No command following exec");
					if(c->input != NULL) {
						in = open(c->input, O_RDONLY);
						if(in == -1)
							error(1,0,"Error occurred while opening file1.");
						dup2(in, 0);
						close(in);
					}
					if(c->output != NULL) {
						out = open(c->output, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
						dup2(out,1);
						close(out);
					}
					execvp(c->u.word[1],c->u.word+1);
					error(1,0,"Error when executing command2.");
				}
				//fork and check for redirections before executing the command
				child = fork();
				if(child < 0)
					error(1,0,"Problems occurred during forking1.");
				else if (child == 0) {
					if(c->input != NULL) {
						in = open(c->input, O_RDONLY);
						if(in == -1)
							error(1,0,"Error occurred while opening file1.");
						dup2(in, 0);
						close(in);
					}
					if(c->output != NULL) {
						out = open(c->output, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
						dup2(out,1);
						close(out);
					}
					execvp(c->u.word[0],c->u.word);
					error(1,0,"Error when executing command1.");
					
				}
				else {
					if(waitpid(child,&p_status,0) == -1) {
						error(1,0,"Failure with child process1.");
					}
					c->status = WEXITSTATUS(p_status);
					return;
				}
				break;

			//This is basically the same as the last part of the SIMPLE_COMMAND
			//without the special case for exec and the running of the subshell
			//command through recursion.
			case SUBSHELL_COMMAND:
				child = fork();
				if(child < 0)
					error(1,0,"Problems occurred during forking1.");
				//if child process
				else if(child == 0){
					//handle redirections
					if(c->input != NULL) {
						//if in here, there is a <
						in = open(c->input, O_RDONLY);
						if(in == -1)
							error(1,0,"Error occurred while opening file1.");
						dup2(in,0);
						close(in);
					}
					if(c->output != NULL) {
						//if in here, there is a >
						//printf("boop");
						out = open(c->output, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
						dup2(out,1);
						close(out);
					}
					execute_command(c->u.subshell_command,0);
					c->status = c->u.subshell_command->status;
					exit(c->status);
				}
				else {
					if(waitpid(child,&p_status,0) == -1) {
						error(1,0,"Failure with child process2.");
					}
					c->status = WEXITSTATUS(p_status);
					return;
				}
				break;

			//If it is possibly anything thing else, there error 
			default:
				error(1,0,"Unknown/unsupported command type.");
		}
	}
}

void
getDep(command_t cmd, comDep* dList)
{
	if(cmd == NULL)
		return;

	if(cmd->type == SUBSHELL_COMMAND)
	{
		getDep(cmd->u.subshell_command, dList);
		if(cmd->input != NULL)
		{
			if(dList->inputCounter >= dList->maxInputCounter)
			{
				dList->maxInputCounter += 2 * INITIAL_LENGTH;
				dList->input = (char**) checked_realloc (dList->input, sizeof(char*) * 
					dList->maxInputCounter);
			}
			dList->input[dList->inputCounter] = cmd->input;
			dList->inputCounter++;
		}
		if(cmd->output != NULL)
		{
			if(dList->outputCounter >= dList->maxOutputCounter)
			{
				dList->maxOutputCounter += 2 * INITIAL_LENGTH;
				dList->output = (char**) checked_realloc (dList->output, sizeof(char*) *
					dList->maxOutputCounter);
			}
			dList->output[dList->outputCounter] = cmd->output;
			dList->outputCounter++;
		}
	}
	
	else if(cmd->type == SIMPLE_COMMAND)
	{
		if(dList->input == NULL)
			dList->input = (char**) checked_malloc(sizeof(char*) * INITIAL_LENGTH);
		
		if(dList->output == NULL)
			dList->output = (char**) checked_malloc(sizeof(char*) * INITIAL_LENGTH);

		int i = 1;
		while(cmd->u.word[i] != NULL)
		{
			if(dList->inputCounter >= dList->maxInputCounter)
			{
				dList->maxInputCounter += 2 * INITIAL_LENGTH;
				dList->input = (char**) checked_realloc(dList->input, sizeof(char*) * 
					dList->maxInputCounter);
			}
			dList->input[dList->inputCounter] = cmd->u.word[i];
			dList->inputCounter++;
			i++;
		}
		
		if(cmd->input != NULL)
		{
			if(dList->inputCounter >= dList->maxInputCounter)
         {
            dList->maxInputCounter += 2 * INITIAL_LENGTH;
            dList->input = (char**) checked_realloc(dList->input, sizeof(char*) *
               dList->maxInputCounter);
         }
         dList->input[dList->inputCounter] = cmd->input;
         dList->inputCounter++;
		}

		if(cmd->output != NULL)
		{
			if(dList->outputCounter >= dList->maxOutputCounter)
         {
            dList->maxOutputCounter += 2 * INITIAL_LENGTH;
            dList->output = (char**) checked_realloc(dList->output, sizeof(char*) *
               dList->maxOutputCounter);
         }
         dList->output[dList->outputCounter] = cmd->output;
         dList->outputCounter++;
		}
	}
	else 
	{
		getDep(cmd->u.command[0], dList);
		getDep(cmd->u.command[1], dList);
	}
}
		

comDep**
findDep(struct command_stream* comStream)
{
	struct command_stream* cmd = comStream;
	comDep** comDepList = (comDep**) checked_malloc(sizeof(comDep**) * INITIAL_LENGTH);
	
	int maxLength = INITIAL_LENGTH;
	int count = 0;
	comDep* curDepList;
	
	while(cmd != NULL)
	{
		curDepList->inputCounter = 0;
		curDepList->outputCounter = 0;
		curDepList->maxInputCounter = 0;
		curDepList->maxOutputCounter = 0;
		curDepList->dependsOnCounter = 0;
		curDepList->maxDependsOn = INITIAL_LENGTH;
		curDepList->input = NULL;
		curDepList->output = NULL;
		curDepList->dependsOn = NULL;
		curDepList->wasRun = 0;

		getDep(cmd->command, curDepList);
		if(count >= maxLength)
		{
			maxLength += 2 * INITIAL_LENGTH;
			comDepList = (comDep**) checked_realloc(comDepList, (sizeof(comDep*) * maxLength));
		}

		comDepList[count] = curDepList;
		count++;
		cmd = cmd->next;
	}

	int deps, index;
	for(deps = 0; deps < count; deps++)
	{
		struct comDep* dList = comDepList[deps];
		char** depInput = dList->input;
		char** depOutput = dList->output;
		int depInputCounter = dList->inputCounter;
		int depOutputCounter = dList->outputCounter;
 		dList->dependsOn = NULL;
		dList->dependsOnCounter = 0;
		dList->maxDependsOn = INITIAL_LENGTH;
		
		for(index = deps-1; index >= 0; index--)
		{
			int isFinished = 0;
			int inDeps, outIndex;
			char** currOut = comDepList[index]->output;
			int currOutCounter = comDepList[index]->outputCounter;
		
			for(inDeps = 0; inDeps < depInputCounter; inDeps++)
			{
				for(outIndex = 0; outIndex < currOutCounter; outIndex++)
				{
					if(!strcmp(depInput[inDeps], currOut[outIndex]))
					{
						if(dList->dependsOn == NULL)
							dList->dependsOn = (int*) checked_malloc(sizeof(int) * INITIAL_LENGTH);
						if(dList->dependsOnCounter >= dList->maxDependsON)
						{
							dList->maxDependson += 2 * INITIAL_LENGTH;
							dList->dependsOn = (int*) checked_realloc(dList->dependsOn, sizeof(int) *
								dList->maxDependsOn);
						}
						dList->dependsOn[dList->dependsOnCounter] = index;
						dList->dependsOnCounter++;
						isFinished=1;
						break;
					}
				}
				if(isFinished == 1)
					break;
			}
			
			if(isFinished == 0)
			{
				int outputDep;
				for(outputDep = 0; outputDep < depOutputCounter; outputDep++)
				{
					for(outIndex = 0; outIndex < currOutCounter; outIndex++)
					{
						if(!strcmp(depOutput[outputDep], currOut[outIndex]))
						{
							if(dList->dependsOn == NULL)
								dList->dependsOn = (int*) checked_malloc(sizeof(int) * INITIAL_LENGTH);
							if(dList->dependsOnCounter >= dList->maxDependsOn)
							{
								dList->maxDependsOn += 2 * INITIAL_LENGTH;
								dList->dependsOn = (int*) checked_malloc(sizeof(int) * dList->maxDependsOn);		
							}
							dList->dependsOn[dList->dependsOnCounter] = index;
							dList->dependsOnCounter++;
							isFinished = 1;
							break;
						}
					}
					if(isFinished == 1)
						break:
				}	
			}
		}
	}
	topCmdCounter = count;
	return comDepList;
}

void
timeTraveler(struct command_stream* cmd, comDep** dList)
{
	int finishedCmd = 0;
	if(cmd == NULL)
		return;

	while(finishedCmd < topCmdCounter)
	{
		struct command_stream* iter = cmd->command;
		int* cmdArr[topCmdCounter];
		int i;
		for(i = 0; i < topCmdCounter; i++)
		{
			cmdArr[i] = iter;
			iter = iter->next;
		}

		int* indepCmds = (int*) checked_malloc(sizeof(int) * INITIAL_LENGTH);
		int indepCmdCounter = 0;
		int maxCmd = INITIAL_LENGTH;

		for(i = 0; i < topCmdCounter; i++)
		{
			if(dList[i]->dependsOnCounter == 0 && dList[i]->wasRun == 0)
			{
				if(indCmdCounter > maxCmd)
				{
					maxCmd += 2 * INITIAL_LENGTH;
					indepCmds = (int*) checked_realloc(indepCmds, sizeof(int) * maxCmd);
				}
				indepCmds[indepCmdCounter] = i;
				indepCmdCounter++;
			}
		}
		
		pid_t pidArr[indepCmdCounter];

		for(i = 0; i < indepCmdCounter; i++)
		{
			pidArr[i] = -1;
		}

		for(i = 0; i < indepCmdCounter; i++)
		{
			pid_t p = fork();
			if(p == 0)
			{
				execute_command(cmdArr[indepCmds[i]],0);
				exit(0);
			}
			pidArr[i] = p;
		}

		int doneCounter = 0;
		while(true)
		{
			int status;
			for(i = 0; i < indepCmdCounter; i++)
			{
				if(pidArr[i] != -1 && !waitpid(pidArr[i], &status, WNOHANG))
					continue;
				if(pidArr[i] != -1)
				{
					doneCounter++;
					pidArr[i] = -1;
				}
			}
			if(doneCounter >= indepCmdCounter)
				break;
		}
	}
}
