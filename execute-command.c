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
	//return immediately if the command is not NULL
	//safety check even though while loop condition to go here
	//is to end if the command is ever NULL
	if(cmd == NULL)
		return;

	//if the command is a subshell command then use recursion on the command in the subshell
	//after that check the input/output that the entire subshell depends on
	if(cmd->type == SUBSHELL_COMMAND)
	{
		getDep(cmd->u.subshell_command, dList);
		//check if the command has a required input
		if(cmd->input != NULL)
		{
			//reallocate more memory if input char** is full
			if(dList->inputCounter >= dList->maxInputCounter)
			{
				dList->maxInputCounter += 2 * INITIAL_LENGTH;
				dList->input = (char**) checked_realloc (dList->input, sizeof(char*) * 
					dList->maxInputCounter);
			}
			//add the input into the dependency list as well as keep count
			dList->input[dList->inputCounter] = cmd->input;
			dList->inputCounter++;
		}
		//check if the command has a required output
		if(cmd->output != NULL)
		{
			//reallocate more memory if the output char** is full
			if(dList->outputCounter >= dList->maxOutputCounter)
			{
				dList->maxOutputCounter += 2 * INITIAL_LENGTH;
				dList->output = (char**) checked_realloc (dList->output, sizeof(char*) *
					dList->maxOutputCounter);
			}
			//add the output into the dependency list as well as keep count
			dList->output[dList->outputCounter] = cmd->output;
			dList->outputCounter++;
		}
	}
	
	//if the command is a simple command
	else if(cmd->type == SIMPLE_COMMAND)
	{
		//if there is no input or output in the List, initialize them
		if(dList->input == NULL)
			dList->input = (char**) checked_malloc(sizeof(char*) * INITIAL_LENGTH * 2);
		
		if(dList->output == NULL)
			dList->output = (char**) checked_malloc(sizeof(char*) * INITIAL_LENGTH * 2);

		//for simple commands, inputs can come in through 2 forms:
		//(1) the input is an argument to the command (i.e. cat filename)
		//(2) the input comes in through redirection (i.e. cat < filename)
		//iterate through the words in the command (start at word[1] because word[0] is command name)
		int i = 1;
		while(cmd->u.word[i] != NULL)
		{
			//reallocate memory if full
			if(dList->inputCounter >= dList->maxInputCounter)
			{
				dList->maxInputCounter += 2 * INITIAL_LENGTH;
				dList->input = (char**) checked_realloc(dList->input, sizeof(char*) * 
					dList->maxInputCounter);
			}
			//add the word into the input
			dList->input[dList->inputCounter] = cmd->u.word[i];
			dList->inputCounter++;
			i++;
		}
		
		//check if there is an input to the command and add in it so
		if(cmd->input != NULL)
		{
			//reallocate more memory if full
			if(dList->inputCounter >= dList->maxInputCounter)
         	{
				dList->maxInputCounter += 2 * INITIAL_LENGTH;
				dList->input = (char**) checked_realloc(dList->input, sizeof(char*) *
					dList->maxInputCounter);
         	}
			//add input into the dependency list and increment for count
         	dList->input[dList->inputCounter] = cmd->input;
         	dList->inputCounter++;
		}

		//check if the command outputs
		if(cmd->output != NULL)
		{
			//reallocate more memory if full
			if(dList->outputCounter >= dList->maxOutputCounter)
         	{
            	dList->maxOutputCounter += 2 * INITIAL_LENGTH;
            	dList->output = (char**) checked_realloc(dList->output, sizeof(char*) *
               		dList->maxOutputCounter);
         	}
			//add output into dependency list and increment for count
         	dList->output[dList->outputCounter] = cmd->output;
         	dList->outputCounter++;
		}
	}
	//for the rest of the command types (AND, OR...) get the dependency for both sides
	else 
	{
		getDep(cmd->u.command[0], dList);
		getDep(cmd->u.command[1], dList);
	}
}

//Split sequence commands into simple commands
void
fixSeq(struct command_stream* tempCmd)
{
     while(tempCmd != NULL)
     {   
         if(tempCmd->command->type == SEQUENCE_COMMAND)
         {   
             struct command_stream* temp = (struct command_stream*) checked_malloc(sizeof(struct command_stream));
             temp->command = tempCmd->command->u.command[1];
             temp->next = tempCmd->next;
             tempCmd->command = tempCmd->command->u.command[0];
             tempCmd->next = temp;
         }
		 else
         	tempCmd = tempCmd->next;
     }

}

comDep**
findDep(struct command_stream* comStream)
{
	struct command_stream* cmd = comStream;
	//create an array of pointer of comDep (command dependencies) to find dependencies of each command
	comDep** comDepList = (comDep**) checked_malloc(sizeof(comDep*) * INITIAL_LENGTH * 2);
	
	//create counters and maximum length to keep count of the list and reallocate memory when needed
	int maxLength = INITIAL_LENGTH * 2;
	int count = 0;
	comDep* curDepList;
	struct command_stream* tempCmd = cmd;
	fixSeq(tempCmd);
	
	while(tempCmd != NULL)
	{
		if(tempCmd->command->type == SEQUENCE_COMMAND)
		{
			struct command_stream* temp = (struct command_stream*) checked_malloc(sizeof(struct command_stream));
			temp->command = tempCmd->command->u.command[1];
			temp->next = tempCmd->next;
			tempCmd->command = tempCmd->command->u.command[0];
			tempCmd->next = temp;
		}
		tempCmd = tempCmd->next;
	}

	//loop through each command of the command stream, create a dependency list for each command
	//initilize all the values in the command dependency list (counters to 0, rest to NULL)
	while(cmd != NULL)
	{
		curDepList = (comDep*) checked_malloc(sizeof(comDep));
		curDepList->inputCounter = 0;
		curDepList->outputCounter = 0;
		curDepList->maxInputCounter = 0;
		curDepList->maxOutputCounter = 0;
		curDepList->input = NULL;
		curDepList->output = NULL;
		curDepList->dependsOn = NULL;
		curDepList->wasRun = 0;

		getDep(cmd->command, curDepList);
		//reallocate memory if reached the maximum size of the allocated memory
		if(count >= maxLength)
		{
			maxLength += 2 * INITIAL_LENGTH;
			comDepList = (comDep**) checked_realloc(comDepList, (sizeof(comDep*) * maxLength));
		}

		//add the dependency list into the array of the pointer of dependency list and move to next command
		comDepList[count] = curDepList;
		count++;
		cmd = cmd->next;
	}

	//after the inputs/outputs are added into the dependency list, which is then put into the array
	//go through the list to identify which command the current command depends on
	int deps, index;
	for(deps = 0; deps < count; deps++)
	{
		comDep* dList = comDepList[deps];	//loop through the array of dependency lists
		char** depInput = dList->input;				//take out the input of the current list
		char** depOutput = dList->output;			//take the output of the current list
		int depInputCounter = dList->inputCounter;		//take the counter for the input
		int depOutputCounter = dList->outputCounter;	//take the counter for the output
 		dList->dependsOn = NULL;						//initialize dependency to NULL
		dList->dependsOnCounter = 0;					//initialize its counter to be 0
		
		//iterate through other commands in the list to compare to the inputs of the current command
		for(index = deps-1; index >= 0; index--)
		{
			int isFinished = 0;
			int inDeps, outIndex;
			char** currOut = comDepList[index]->output;				//get the outputs of the list we're in at this loop
			int currOutCounter = comDepList[index]->outputCounter;	//get the counter of the list we're in at this loop
		
			//loop through the inputs of the list (get that from the first for loop) 
			for(inDeps = 0; inDeps < depInputCounter; inDeps++)
			{
				//loop through the outputs of the list that we get from the second loop
				//so we are looping through all other lists and comparing the inputs of the one command
				//to the all the outputs of all the other commands
				for(outIndex = 0; outIndex < currOutCounter; outIndex++)
				{
					//check if the current input is same as the current output, meaning the 1st is dependent on the 2nd
					if(!strcmp(depInput[inDeps], currOut[outIndex]))
					{
						//allocate memory if it is NULL
						if(dList->dependsOn == NULL)
						{
							dList->dependsOn = (int*) checked_malloc(sizeof(int) * INITIAL_LENGTH * 2);
							dList->maxDependsOn = INITIAL_LENGTH * 2;
						}
						//if memory is full, reallocate memory
						if(dList->dependsOnCounter >= dList->maxDependsOn)
						{
							dList->maxDependsOn += 2 * INITIAL_LENGTH;
							dList->dependsOn = (int*) checked_realloc(dList->dependsOn, sizeof(int) *
								dList->maxDependsOn);
						}
						//add that the the command looped through in the outer-most for-loop depends on
						//the owner of the output that matches, then break
						//we also create set a variable so that we can indicate we have found it and therefore
						//be able to break out of the next loop also
						dList->dependsOn[dList->dependsOnCounter] = index;
						dList->dependsOnCounter++;
						isFinished=1;
						break;
					}
				}
				if(isFinished == 1)
					break;
			}
			
			//if the variable never changed, it means that there were no inputs depended on
			//if that is the case, then check the output to see if there are two that output
			//to the same place
			if(isFinished == 0)
			{
				int outputDep;
				//loop through the outputs of the list from the first loop
				for(outputDep = 0; outputDep < depOutputCounter; outputDep++)
				{
					//loop through the outputs of the list from the second loop
					for(outIndex = 0; outIndex < currOutCounter; outIndex++)
					{
						//check if they are the same
						if(!strcmp(depOutput[outputDep], currOut[outIndex]))
						{
							//initialize and allocate memory if it is still NULL
							if(dList->dependsOn == NULL)
							{
								dList->dependsOn = (int*) checked_malloc(sizeof(int) * INITIAL_LENGTH * 2);
								dList->maxDependsOn = INITIAL_LENGTH * 2;
							}
							//reallocate more memory if full
							if(dList->dependsOnCounter >= dList->maxDependsOn)
							{
								dList->maxDependsOn += 2 * INITIAL_LENGTH;
								dList->dependsOn = (int*) checked_realloc(dList->dependsOn, sizeof(int) * (dList->maxDependsOn));		
							}
							//add to show that the command looped through the outer-most loops is dependent
							//on the owner of the matched output
							dList->dependsOn[dList->dependsOnCounter] = index;
							dList->dependsOnCounter++;
							isFinished = 1;
							break;
						}
					}
					if(isFinished == 1)
						break;
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
	//create counter for the amount of commands finished
	int finishedCmd = 0;
	if(cmd == NULL)
		return;

	while(finishedCmd < topCmdCounter)
	{
		//create an array, each index holding a command
		//allows for call by index number later on
		struct command_stream* iter = cmd;
		command_t cmdArr[topCmdCounter];
		int i;
		for(i = 0; i < topCmdCounter; i++)
		{
			cmdArr[i] = iter->command;
			iter = iter->next;
		}

		//create an array of the numbers (each indicating one of the commands) of 
		//the commands that can be run in parallel
		int* indepCmds = (int*) checked_malloc(sizeof(int) * INITIAL_LENGTH);
		int indepCmdCounter = 0;
		int maxCmd = INITIAL_LENGTH;

		for(i = 0; i < topCmdCounter; i++)
		{
			if(dList[i]->dependsOnCounter == 0 && dList[i]->wasRun == 0)
			{
				if((indepCmdCounter+1) >= maxCmd)
				{
					maxCmd *= 2;
					indepCmds = (int*) checked_realloc(indepCmds, sizeof(int) * maxCmd);
				}
				indepCmds[indepCmdCounter] = i;
				indepCmdCounter++;
			}
		}
		
		//create an array of pid for the independent commands so we can keep track later
		pid_t pidArr[indepCmdCounter];

		//initialize all the array values to be -1 so we can check for values later
		for(i = 0; i < indepCmdCounter; i++)
		{
			pidArr[i] = -1;
		}

		//for each command, we fork and if in child we will execute the command and then
		//assign the pid
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

		//check for all the commands inside the array to be done through the pid and when
		//done, we move on as all independent commands are accomplished
		int doneCounter = 0;
		while(1)
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
		
		for(i = 0; i < indepCmdCounter; i++)
		{
			int j;
			dList[indepCmds[i]]->wasRun = 1;			
			for(j = indepCmds[i]; j < topCmdCounter; j++)
			{
				if(dList[j]->wasRun == 1)
					continue;
				comDep* currDepList = dList[j];
				int k;	
				for (k = 0; k < currDepList->dependsOnCounter; k++)
				{
					if(currDepList->dependsOn[k] == indepCmds[i])
					{
						int temp = currDepList->dependsOn[currDepList->dependsOnCounter - 1];
						currDepList->dependsOn[currDepList->dependsOnCounter - 1] = currDepList->dependsOn[k];
						currDepList->dependsOn[k] = temp;
						currDepList->dependsOnCounter--;
						break;
					}
				}
			}
		}
		finishedCmd += indepCmdCounter;	
		free(indepCmds);
	}
}

void
free_comDep(comDep** dList)
{
	int i;
	for(i = 0; i < topCmdCounter; i++)
	{
		free(dList[i]->input);
		free(dList[i]->output);
		free(dList[i]->dependsOn);
		free(dList[i]);
	}
	free(dList);
}

void
free_command(command_t cmd)
{
	if(cmd == NULL)
      return;

   if(cmd->type == SIMPLE_COMMAND)
      free(cmd->u.word);
   else if (cmd->type == AND_COMMAND || cmd->type == OR_COMMAND || cmd->type == SEQUENCE_COMMAND
         || cmd->type == PIPE_COMMAND)
	{
		free_command(cmd->u.command[0]);
		free_command(cmd->u.command[1]);
	}
	else if(cmd->type == SUBSHELL_COMMAND)
		free_command(cmd->u.subshell_command);
	else
		error(1,0, "Unknown typing found");

	free(cmd);
}

void
free_comStream(command_stream_t comStream)
{
	if(comStream == NULL)
		return;
	
	free_command(comStream->command);
	command_stream_t next = comStream->next;
	free(comStream);
	free_comStream(next);
}
