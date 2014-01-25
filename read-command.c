// UCLA CS 111 Lab 1 command reading

#include "command.h"
#include "command-internals.h"
#include "alloc.h"

#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

//printCounter is used to aid in printing lines
int printCounter = 0;

//create and initialize values for a new command
command_t
makeCommand(enum command_type type)
{
	command_t cmd = (struct command *) checked_malloc(sizeof(struct command));
	cmd->type = type;
	cmd->status = -1;
	cmd->input = NULL;
	cmd->output = NULL;
	return cmd;
}

//contain the command and the command following
struct command_stream {
	command_t command;
	struct command_stream* next;
};

//parse partioned sections of the input
command_t
parser(token* tArr, int start, int end)
{
	int i;
	//return NULL if the start and end are the same index
	if (end <= start)
		return NULL;
	
	//closedParen keeps track of subshells, 0 if parentheses are balanced
	//nonzero values indicate you are in a subshell
	int closedParen = 0;
	for (i = end-1; i >= start; i--)
	{
		if(tArr[i].type == RIGHT_PAREN_TOKEN)
		{
			closedParen++;
			continue;
		}
		else if(tArr[i].type == LEFT_PAREN_TOKEN)
			closedParen--;
		
		//check if there are multiple semicolons in succession
		if (tArr[i].type == SEQUENCE_TOKEN || tArr[i+1].type == SEQUENCE_TOKEN)
			error(1,0,"%i: Simultaneous multiple sequences are invalid",
				tArr[i].lineNumber);

		//if you are at a semicolon, then split the partition between left and right and recurse
		//a newline within a subshell is equivalent to a semicolon
		if ((tArr[i].type == SEQUENCE_TOKEN || tArr[i].type == NEWLINE_TOKEN) && closedParen == 0)
		{
			if (i == (end-1) || tArr[i+1].type == END_TOKEN || 
				tArr[i+1].type == NEWLINE_TOKEN)
			{
				command_t result = parser(tArr, start, i);
				if (result == NULL)
					error(1,0,"%i: Missing command on the left of the semicolon.\n", 
						tArr[i].lineNumber);
				
				return result;
			}
			
			//creating index for split between left and right of the semicolon
			int lStart = start;
			int lEnd = i;
			int rStart = i+1;
			int rEnd = end;
			
			command_t cmd = makeCommand(SEQUENCE_COMMAND);
			cmd->u.command[0] = parser(tArr, lStart, lEnd);
			if (cmd->u.command[0] == NULL)
				error(1,0,"%i: Missing command on the left of the semicolon.\n", 
					tArr[i].lineNumber);
			
			cmd->u.command[1] = parser(tArr, rStart, rEnd);
			return cmd;
		}
	}
	
	//taking care of the && and ||
	closedParen = 0;
	for (i = end-1; i >= start; i--)
	{
		//same as previous to check if within subshell
		if(tArr[i].type == RIGHT_PAREN_TOKEN)
		{
			closedParen++;
			continue;
		}
		else if(tArr[i].type == LEFT_PAREN_TOKEN)
			closedParen--;

		//if &&/|| found, then once again split between the left and the right of the operator and recurse
		if ((tArr[i].type == OR_TOKEN || tArr[i].type == AND_TOKEN) && closedParen == 0)
		{			
			int lStart = start;
			int lEnd = i;
			int rStart = i+1;
			int rEnd = end;
			
			command_t cmd;
			if (tArr[i].type == OR_TOKEN)
				cmd = makeCommand(OR_COMMAND);
			else
				cmd = makeCommand(AND_COMMAND);
			
				
			cmd->u.command[0] = parser(tArr, lStart, lEnd);
			cmd->u.command[1] = parser(tArr, rStart, rEnd);
				
			//if either arguments are NULL, then error
			if (cmd->u.command[0] == NULL || cmd->u.command[1] == NULL)
				error(1,0, "%i: Cannot have null for and/or.\n",
				tArr[i].lineNumber);
			return cmd;
		}
	}
	
	// taking care of the pipelines
	closedParen = 0;
	for (i = end-1; i >= start; i--)
	{
		if(tArr[i].type == RIGHT_PAREN_TOKEN)
		{
			closedParen++;
			continue;
		}
		else if(tArr[i].type == LEFT_PAREN_TOKEN)
			closedParen--;

		if (tArr[i].type == PIPELINE_TOKEN && closedParen == 0)
		{			
			//splitting index between left/right of pipeline and recurse
			int lStart = start;
			int lEnd = i;
			int rStart = i+1;
			int rEnd = end;
			
			command_t cmd = makeCommand(PIPE_COMMAND);
			cmd->u.command[0] = parser(tArr, lStart, lEnd);
			cmd->u.command[1] = parser(tArr, rStart, rEnd);
			
			if (cmd->u.command[0] == NULL || cmd->u.command[1] == NULL)
				error(1,0, "%i: Cannot have null for pipelines.\n",
					tArr[i].lineNumber);
			return cmd;
		}
	}
	
	//taking care of redirections
	int redirectRight2 = -1;
	int redirectLeft2 = -1;
	//lastToken is used to check the token prior to another token
	tokenType lastToken = OTHER_TOKEN;
	
	for (i = (end-1);i >= start; i--)
	{
		if (tArr[i].type == REDIRECTRIGHT_TOKEN || tArr[i].type == REDIRECTLEFT_TOKEN)
		{
			if(tArr[i].type == REDIRECTRIGHT_TOKEN && redirectRight2 != -1)
			{
				error(1,0, "%i: Invalid number of '>' or '<'.\n",
					  tArr[i].lineNumber);
				redirectRight2 = i;
			}
			if(tArr[i].type == REDIRECTLEFT_TOKEN && redirectLeft2 != -1)	
			{
				error(1,0, "%i: Invalid number of '>' or '<'.\n",
					  tArr[i].lineNumber);
				redirectLeft2 = i;
			}
			//redirectRight2 = i;
			lastToken = tArr[i].type;
			continue;
		}
		
		if (tArr[i].type == WORD_TOKEN)
			lastToken = WORD_TOKEN;
		
		//ensure the token following an end parentheses is a word
		if (tArr[i].type == RIGHT_PAREN_TOKEN)
		{
			if (lastToken == WORD_TOKEN)
				error(1,0, "%i: Cannot follow end parentheses with a word.\n",
					  tArr[i].lineNumber);
			
			//track the subshell
			int closedParen = 1;
			int openParen;
			
			tokenType previousToken = RIGHT_PAREN_TOKEN;
			for (openParen = (i-1); openParen >= start; openParen--)
			{
				//ensure the token prior to an open parentheses is not a word
				if (previousToken == LEFT_PAREN_TOKEN && tArr[openParen].type == WORD_TOKEN)
					error(1,0, "%i: Cannot follow a word with open parentheses.\n",
						tArr[i].lineNumber);
				
				if (tArr[openParen].type == RIGHT_PAREN_TOKEN)
				{
					if (previousToken == WORD_TOKEN)
						error(1,0, "%i: Cannot follow end parentheses with a word.\n",
							tArr[i].lineNumber);
					closedParen++;
					previousToken = RIGHT_PAREN_TOKEN;
				}
				else if (tArr[openParen].type == LEFT_PAREN_TOKEN)
				{
					closedParen--;
					previousToken = LEFT_PAREN_TOKEN;
				}
				else if (tArr[openParen].type == WORD_TOKEN)
					previousToken = WORD_TOKEN;
				else
					previousToken = OTHER_TOKEN;

				if (closedParen == 0)
				{
					//ensure '>' does not precede '<'
					if (redirectLeft2 != -1 && redirectRight2 != -1)
						if (redirectRight2 < redirectLeft2)
							error(1,0, "%i: Cannot have '>' precede '<'.\n",
									tArr[i].lineNumber);
					if (openParen != start)
						error(1,0, "%i: Invalid token near open parentheses.\n",
							  tArr[i].lineNumber);
					
					struct command* cmd = (struct command*) checked_malloc(sizeof(struct command));
					cmd->type = SUBSHELL_COMMAND;
					cmd->status = -1;
					
					if (redirectLeft2 != -1 && tArr[redirectLeft2+1].type !=WORD_TOKEN)
						error(1,0, "%i: A word must follow '<'.\n",
							  tArr[i].lineNumber);
					if (redirectLeft2 == -1) 
						cmd->input = NULL;
					else
						cmd->input = tArr[redirectLeft2+1].word;
					
					if (redirectRight2 != -1 && tArr[redirectRight2+1].type !=WORD_TOKEN)
						error(1,0, "%i: A word must follow '>'.\n",
							  tArr[i].lineNumber);
					if (redirectLeft2 == -1) 
						cmd->input = NULL;
					else
						cmd->input = tArr[redirectLeft2+1].word;
						
					cmd->u.subshell_command = parser(tArr, openParen+1, i);
					return cmd;
				}
			}
		}
	}
	
	//take care of simple commands
	command_t cmd = makeCommand(SIMPLE_COMMAND);
	cmd->u.word = (char**) checked_malloc(sizeof(char*)*INITIAL_LENGTH);
	int maxLength = INITIAL_LENGTH;
	int wordIndex = 0;
	tokenType previousToken = OTHER_TOKEN;
	int redirectLeft3 = -1;
	int redirectRight3 = -1;
	
	for (i = start; i < end; i++)
	{
		//ensure a word is on both sides of a redirection
		if (tArr[i].type == REDIRECTLEFT_TOKEN || tArr[i].type == REDIRECTRIGHT_TOKEN)
		{
			if(previousToken != WORD_TOKEN)
				error(1,0, "%i: A word must precede '<' or '>'.\n",
								  tArr[i].lineNumber);
			if (tArr[i+1].type != WORD_TOKEN)
				error(1,0, "%i: A word must follow '<' or '>'.\n",
								  tArr[i].lineNumber);
			
			previousToken = tArr[i].type;
			redirectLeft3 = i;
			continue;
		}

		//put the input as the word if previous token was <
		if (previousToken == REDIRECTLEFT_TOKEN)
		{
			cmd->input = tArr[i].word;
			previousToken = WORD_TOKEN;
			continue;
		}
		//put the output as the word if previous token was >
		if (previousToken == REDIRECTRIGHT_TOKEN)
		{
			cmd->output = tArr[i].word;
			previousToken = WORD_TOKEN;
			continue;
		}
		
		//if word is too long, double the amount of space
		if (wordIndex >= (maxLength-1))
		{
			maxLength = 2*maxLength;
			cmd->u.word = (char**) checked_realloc(cmd->u.word, maxLength*sizeof(char*));
		}
		
		cmd->u.word[wordIndex++] = tArr[i].word;
		previousToken = WORD_TOKEN;
	}
	
	//input last character of the array as null to denote end of word
	cmd->u.word[wordIndex] = '\0';
	if ((redirectLeft3 != -1 && redirectRight3 != -1) && (redirectLeft3 >= redirectRight3))
		error(1,0, "%i: Cannot have '>' precede '<'.\n",
							  tArr[i].lineNumber);
	return cmd;
}
				
	
//looks at the token array and separates it based on operator tokens
struct command_stream*
partition(token* tArr, int tArrSize)
{
	//initializing index of separations and resulting command_stream
	int start = 0;
	int end = 0;
	struct command_stream* streamList = NULL;
	struct command_stream* streamTail = NULL;

	//iterates through token array
	while((end < tArrSize) && (tArr[end].type != END_TOKEN))
	{
		int openParenCounter = 0;
		int isSubshellBalanced = 0;
		
		while((openParenCounter != 0 || tArr[end].type != NEWLINE_TOKEN) &&
			   tArr[end].type != END_TOKEN)
		{
			//keeps track of parentheses to ensure it is balanced
			if (tArr[end].type == LEFT_PAREN_TOKEN)
			{
				if (isSubshellBalanced == 1 && openParenCounter == 0)
				{
					error(1,0, "%i: Unbalanced parentheses.\n", tArr[end].lineNumber);
				}
				openParenCounter++;
			}
			else if (tArr[end].type == RIGHT_PAREN_TOKEN)
			{
				openParenCounter--;
				if(openParenCounter == 0)
					isSubshellBalanced = 1;
			}
			end++;
		}
		
		//parse the separated portion of the token array
		command_t topCommand = parser(tArr, start, end);
		
		//if first time, then allocate memory for streamList and add in the resulting command
		if (streamList == NULL)
		{
			streamList = (struct command_stream*) checked_malloc(sizeof(struct command_stream));
			streamTail = streamList;
			streamList->command = topCommand;
			streamList->next = NULL;
		}
		//creates a new command to append to the end of the stream
		else
		{
			struct command_stream* newTail = (struct command_stream*) checked_malloc(sizeof(struct command_stream));
			newTail->command = topCommand;
			newTail->next = NULL;
			streamTail->next = newTail;
			streamTail = newTail;
		}
		//increment to begin at the end of the separated portion
		start = end+1;
		end = start;
	}
	return streamList;
}

//create command stream and tokenize commands
command_stream_t
make_command_stream (int (*get_next_byte) (void *),
		     void *get_next_byte_argument)
{

   char character;
   size_t tArrSize = sizeof(token) * 1024;
   token *tArr = (token*) checked_malloc(tArrSize);
   int tokenIndex = 0;
   int wordIndex = 0;
   size_t wordLength;
   int lineNum = 1;
   int leftParen = 0;
   int rightParen = 0;
   
   //traverse through the input file and read each byte
   while ((character = get_next_byte(get_next_byte_argument)) != EOF)
	{
		jump:
		
		//increase the token array if it has grown too much
		if ((size_t)tokenIndex == tArrSize/sizeof(token))
		{
			tArr = (token*) checked_grow_alloc(tArr, &tArrSize);
		}
		
		//match words and operators to tokens and add to token array
		switch (character)
		{
			case '\t':
			case ' ':
				continue;
				
			case '\n':
				if ((tArr[tokenIndex-1].type == REDIRECTLEFT_TOKEN) ||
					(tArr[tokenIndex-1].type == REDIRECTRIGHT_TOKEN) ||
					(tArr[tokenIndex-1].type == AND_TOKEN) ||
					(tArr[tokenIndex-1].type == OR_TOKEN) ||
					(tArr[tokenIndex-1].type == NEWLINE_TOKEN) ||
					(tArr[tokenIndex-1].type == SEQUENCE_TOKEN) ||
					(tArr[tokenIndex-1].type == PIPELINE_TOKEN))
				{
					continue;
				}
				else
				{
					tArr[tokenIndex].type = NEWLINE_TOKEN;
					tArr[tokenIndex++].lineNumber = lineNum;
					lineNum++;
					break;
				}
				
			case '<':
				tArr[tokenIndex].type = REDIRECTLEFT_TOKEN;
				tArr[tokenIndex++].lineNumber = lineNum;
				break;
				
			case '>':
			
				tArr[tokenIndex].type = REDIRECTRIGHT_TOKEN;
				tArr[tokenIndex++].lineNumber = lineNum;
				break;
			
			case ';':
				tArr[tokenIndex].type = SEQUENCE_TOKEN;
				tArr[tokenIndex++].lineNumber = lineNum;
				break;
				
			case '&':
				//ensure character following & is another &
				character = get_next_byte(get_next_byte_argument);
				if (character == '&')
				{
					tArr[tokenIndex].type = AND_TOKEN;
					tArr[tokenIndex++].lineNumber = lineNum;
				}
				else
				{
					free(tArr);
					error(1,0, "%i: Invalid ampersand", lineNum);
				}
				break;
				
			case '|':
				//check character following | to ensure if pipeline or OR
				character = get_next_byte(get_next_byte_argument);
				if (character == '|')
				{
					tArr[tokenIndex].type = OR_TOKEN;
					tArr[tokenIndex++].lineNumber = lineNum;
				}
				else
				{
					tArr[tokenIndex].type = PIPELINE_TOKEN;
					tArr[tokenIndex++].lineNumber = lineNum;
					goto jump;
				}
				break;
				
				//for parentheses, count number of open and end parentheses to ensure
				//parentheses are balanced
			case '(':
				tArr[tokenIndex].type = LEFT_PAREN_TOKEN;
				tArr[tokenIndex++].lineNumber = lineNum;
				leftParen++;
				break;
				
			case ')':
				tArr[tokenIndex].type = RIGHT_PAREN_TOKEN;
				tArr[tokenIndex++].lineNumber = lineNum;
				rightParen++;
				break;
				
			case '#':
				while ((character = get_next_byte(get_next_byte_argument)) != '\n' 
						&& character != EOF)
				{
					continue;
				}
				lineNum++;
				break;
				
				//default case is for words
			default:
				if (character == EOF)
					break;
				do
				{
					if (isalnum(character) || character == '!' 
						|| character == '%' || character == '+' || character == ','
						|| character == '.' || character == '/' || character == ':'
						|| character == '@' || character == '^' || character == '_'
						|| character == '-') 
					{
						//allocate memory for a character array to hold a word
						if (wordIndex == 0)
						{
							wordLength = 32;
							tArr[tokenIndex].word = (char*) checked_malloc(sizeof(char) *wordLength);
						}
						
						//allocate more memory if word is too long
						if ((size_t) wordIndex >= wordLength)
						{
							tArr[tokenIndex].word = (char*) checked_grow_alloc(tArr[tokenIndex].word, &wordLength);
						}
						tArr[tokenIndex].word[wordIndex++] = character;
						tArr[tokenIndex].type = WORD_TOKEN;
					}
					//if the character is an operator, return to beginning of switch statement
					else if (character == '<' || character == '>' || character == ';' ||
							 character == '&' || character == '|' || character == '(' ||
							 character == ')' || character == '#' || isspace(character) )
					{
						tArr[tokenIndex].word[wordIndex] = '\0';
						tArr[tokenIndex].lineNumber = lineNum;
						tokenIndex++;
						wordIndex = 0;
						goto jump;
					}
					//if an invalid character is found, free up memory and error
					else
					{
						free(tArr);
						error(1,0, "%i: Invalid character: %c", lineNum, character);
					}
				} while ((character = get_next_byte(get_next_byte_argument)) != EOF);
				break;
		}
					
	}
	//ensure parentheses are balanced
	if (rightParen != leftParen)
		error(1,0, "Mismatched number of parentheses.\n");
	tArr[tokenIndex].type = END_TOKEN;
	tArr[tokenIndex].lineNumber = lineNum;

	/*
	int j;
	for (j= 0; j < tokenIndex+1; j++)
	{
		switch(tArr[j].type)
		{
			case REDIRECTLEFT_TOKEN:
				fprintf(stderr, "<\n");
				continue;
			case REDIRECTRIGHT_TOKEN:
				printf(">\n");
				continue;
			case AND_TOKEN:
				printf("AND\n");
				continue;
			case OR_TOKEN:
				printf("OR\n");
				continue;
			case LEFT_PAREN_TOKEN:
				printf("(\n");
				continue;
			case RIGHT_PAREN_TOKEN:
				printf(")\n");
				continue;
			case WORD_TOKEN:
				printf("WORD\n");
				continue;
			case NEWLINE_TOKEN:
				printf("NEW LINE\n");
				continue;
			case PIPELINE_TOKEN:
				printf("|\n");
				continue;
			case END_TOKEN:
				printf("END\n");
				continue;
			case OTHER_TOKEN:
				printf("OTHER\n");
				continue;
			case SEQUENCE_TOKEN:
				printf(";\n");
				continue;
		}
	}
	*/
	struct command_stream* stream = partition(tArr, tokenIndex);
	free(tArr);
	return stream;
}
											  
//iterate through command stream and return current command
command_t
read_command_stream (command_stream_t s)
{
	int i;
	for (i = 0; i < printCounter; i++)
		s = s->next;
	if (s == NULL)
		return NULL;
	printCounter++;
	return s->command;
}


