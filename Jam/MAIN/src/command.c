/*
 * Copyright 1993 Christopher Seiwald.
 */

/*
 * command.c - maintain lists of commands
 *
 * Each target has a list of actions that are to be applied to it, but due
 * to modifications on the actions they do not map one-to-one to the commands
 * that are the be executed against the target.  The CMD datatype holds
 * a single command that is to be executed against a target, and they can
 * chain together to represent the full collection of commands used to
 * update a target.
 *
 * External routines:
 *	cmd_new() - make a new CMD and chain it
 *	cmd_free() - free a CMD
 *
 * Macros:
 *	cmd_next() - follow chain of CMDs	
 */

# include "jam.h"

# include "lists.h"
# include "parse.h"
# include "variable.h"
# include "rules.h"

# include "command.h"

/*
 * cmd_new() - make a new CMD and chain it
 */

CMD *
cmd_new( chain, rule, targets, sources )
CMD	*chain;
RULE	*rule;
LIST	*targets;
LIST	*sources;
{
	int     len;

	CMD *cmd = (CMD *)malloc( sizeof( CMD ) );

	cmd->rule = rule;
	cmd->targets = targets;
	cmd->sources = sources;

	len = var_string( rule->actions, cmd->buf, targets, sources );
	
	if( len > MAXCMD )
	{
	    /* Can't do much here - we just blew our buffer! */
	    printf( "fatal error: command too long\n" );
	    exit( -1 );
	}

	if( !chain ) chain = cmd;
	else chain->tail->next = cmd;
	chain->tail = cmd;
	cmd->next = 0;

	return chain;
}

/*
 * cmd_free() - free a CMD
 */

void
cmd_free( cmd )
CMD	*cmd;
{
	list_free( cmd->sources );
	list_free( cmd->targets );

	free( (char *)cmd );
}
