/*
 * Copyright 1993 Christopher Seiwald.
 */

# ifdef NT

# include "jam.h"
# include "execcmd.h"

/*
 * execunix.c - execute a shell script on UNIX
 *
 * 05/04/94 (seiwald) - async multiprocess interface; noop on NT
 */

static int intr = 0;

void
onintr()
{
	intr++;
	printf( "...interrupted\n" );
}
	
void
execcmd( string, func, closure  )
char *string;
void (*func)();
void *closure;
{
	int status, pid, w, rstat;
	void (*istat)();

	istat = signal( SIGINT, onintr );
	status = system( string );
	signal( SIGINT, istat );

	if( intr )
	    rstat = EXEC_CMD_INTR;
	else if( w == -1 || status != 0 )
	    rstat = EXEC_CMD_FAIL;
	else
	    rstat = EXEC_CMD_OK;

	(*func)( closure, rstat );
}

int 
execwait()
{
	return 0;
}

# endif /* NT */
