// UCLA CS 111 Lab 1 command internals

#define INITIAL_LENGTH 5

int topCmdCounter;

typedef enum 
{
   REDIRECTLEFT_TOKEN,
   REDIRECTRIGHT_TOKEN,
   AND_TOKEN,
   OR_TOKEN,
   LEFT_PAREN_TOKEN,
   RIGHT_PAREN_TOKEN,
   WORD_TOKEN,
   NEWLINE_TOKEN,
   PIPELINE_TOKEN,
   END_TOKEN,
   OTHER_TOKEN,
   SEQUENCE_TOKEN
} tokenType;

typedef struct 
{
   tokenType type;
   char* word;
   int lineNumber;
}token;

enum command_type
  {
    AND_COMMAND,         // A && B
    SEQUENCE_COMMAND,    // A ; B
    OR_COMMAND,          // A || B
    PIPE_COMMAND,        // A | B
    SIMPLE_COMMAND,      // a simple command
    SUBSHELL_COMMAND,    // ( A )
  };
  
// Data associated with a command.
struct command
{
  enum command_type type;

  // Exit status, or -1 if not known (e.g., because it has not exited yet).
  int status;

  // I/O redirections, or 0 if none.
  char *input;
  char *output;

  union
  {
    // for AND_COMMAND, SEQUENCE_COMMAND, OR_COMMAND, PIPE_COMMAND:
    struct command *command[2];

    // for SIMPLE_COMMAND:
    char **word;

    // for SUBSHELL_COMMAND:
    struct command *subshell_command;
  } u;
};

