/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

# if defined( NT ) || defined( __OS2__ )

/*
 * execnt.c - execute a shell script on NT
 *
 * Because I can't find a wait() call on NT, this implementation uses
 * cwait() to wait for each process.  That means parallelism is crippled:
 * all concurrent jobs must complete before the next can be issued.
 *
 * External routines:
 *	execcmd() - launch an async command execution
 * 	execwait() - wait and drive at most one execution completion
 *
 * Internal routines:
 *	onintr() - bump intr to note command interruption
 *
 * 05/31/96 (seiwald) - async multiprocess interface for NT
 */

# include "jam.h"
# include "execcmd.h"
# include "lists.h"
# include <errno.h>
# include <process.h>

static int intr = 0;

static int cmdsrunning = 0;

static void (*istat)();

static struct
{
	int	pid;
	void	(*func)();
	void 	*closure;
} cmdtab[ MAXJOBS ] = {0};

/*
 * onintr() - bump intr to note command interruption
 */

void
onintr( disp )
int disp;
{
	intr++;
	printf( "...interrupted\n" );
}

/*
 * execcmd() - launch an async command execution
 */

void
execcmd( string, func, closure, shell )
char *string;
void (*func)();
void *closure;
LIST *shell;
{
	int pid;
	int slot;
	char *argv[ MAXARGC + 1 ];	/* +1 for NULL */
	static char *comspec;

	if( !comspec && !( comspec = getenv( "COMSPEC" ) ) )
	    comspec = "CMD.EXE";

	/* Find a slot in the running commands table for this one. */

	for( slot = 0; slot < MAXJOBS; slot++ )
	    if( !cmdtab[ slot ].pid )
		break;

	if( slot == MAXJOBS )
	{
	    printf( "no slots for child!\n" );
	    exit( EXITBAD );
	}
	else
	{
	    argv[0] = comspec;
	    argv[1] = "/C";
	    argv[2] = string;
	    argv[3] = 0;
	}

	/* Catch interrupts whenever commands are running. */

	if( !cmdsrunning++ )
	    istat = signal( SIGINT, onintr );

	/* Start the command */

	if( ( pid = spawnv( P_NOWAIT, comspec, argv ) ) < 0 )
	{
	    perror( "spawn" );
	    exit( EXITBAD );
	}

	/* Save the operation for execwait() to find. */

	cmdtab[ slot ].pid = pid;
	cmdtab[ slot ].func = func;
	cmdtab[ slot ].closure = closure;

	/* Wait until we're under the limit of concurrent commands. */
	/* Don't trust globs.jobs alone. */

	while( cmdsrunning >= MAXJOBS || cmdsrunning >= globs.jobs )
	    if( !execwait() )
		break;
}

/*
 * execwait() - wait and drive at most one execution completion
 */

int
execwait()
{
	int i;
	int status, w;
	int rstat;

	/* Handle naive make1() which doesn't know if cmds are running. */

	if( !cmdsrunning )
	    return 0;

	for( i = 0; i < MAXJOBS && cmdsrunning; i++ )
	    if( w = cmdtab[ i ].pid )
	{
	    /* Pick up process pid and status */
    
	    w = cwait( &status, w, _WAIT_CHILD );

	    if( w == -1 )
	    {
		printf( "child process(es) lost!\n" );
		perror("wait");
		exit( EXITBAD );
	    }

	    /* Drive the completion */

	    if( !--cmdsrunning )
		signal( SIGINT, istat );

	    if( intr )
		rstat = EXEC_CMD_INTR;
	    else if( w == -1 || status != 0 )
		rstat = EXEC_CMD_FAIL;
	    else
		rstat = EXEC_CMD_OK;

	    cmdtab[ i ].pid = 0;

	    (*cmdtab[ i ].func)( cmdtab[ i ].closure, rstat );
	}

	return 1;
}

# endif /* NT */
