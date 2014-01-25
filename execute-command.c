// UCLA CS 111 Lab 1 command execution

#include "command.h"
#include "command-internals.h"
#include "alloc.h"
#include <error.h>

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
  time_travel = 1;
  error (time_travel, 0, "command execution not yet implemented");
  c = (struct command*) checked_malloc(sizeof(struct command));
  c->type = SEQUENCE_COMMAND;
  c->status = -1;
  c->input = NULL;
  c->output = NULL;
}
