/*
 * Copyright 1993 Christopher Seiwald.
 */

/*
 * make.c - bring a target up to date, once rules are in place
 *
 * This modules controls the execution of rules to bring a target and
 * its dependencies up to date.  It is invoked after the targets, rules,
 * et. al. described in rules.h are created by the interpreting of the
 * jam files.
 *
 * This file contains the main make() entry point and the first pass
 * make0().  The second pass, make1(), which actually does the command
 * execution, is in make1.c.
 *
 * External routines:
 *	make() - make a target, given its name
 *
 * Internal routines:
 * 	make0() - bind and scan everything to make a TARGET
 *
 * 12/26/93 (seiwald) - allow NOTIME targets to be expanded via $(<), $(>)
 * 01/04/94 (seiwald) - print all targets, bounded, when tracing commands
 * 04/08/94 (seiwald) - progress report now reflects only targets with actions
 * 04/11/94 (seiwald) - Combined deps & headers into deps[2] in TARGET.
 * 12/20/94 (seiwald) - NOTIME renamed NOTFILE.
 */

# include "jam.h"

# include "lists.h"
# include "parse.h"
# include "variable.h"
# include "rules.h"

# include "search.h"
# include "newstr.h"
# include "make.h"
# include "headers.h"
# include "command.h"

static void make0();

# define max( a,b ) ((a)>(b)?(a):(b))

typedef struct {
	int	temp;
	int	updating;
	int	dontknow;
	int	targets;
	int	made;
} COUNTS ;

static char *target_fate[] = 
{
	"init",
	"making",
	"ok",
	"touched",
	"temp",
	"missing",
	"old",
	"update",
	"can't"
} ;

# define spaces(x) ( "                " + 16 - ( x > 16 ? 16 : x ) )

/*
 * make() - make a target, given its name
 */

void
make( n_targets, targets, anyhow )
int	n_targets;
char	**targets;
int	anyhow;
{
	int i;
	COUNTS counts[1];

	memset( (char *)counts, 0, sizeof( *counts ) );

	for( i = 0; i < n_targets; i++ )
	    make0( bindtarget( targets[i] ), (time_t)0, 0, counts, anyhow );

	if( DEBUG_MAKEQ )
	{
	    if( counts->targets )
		printf( "...found %d target(s)...\n", counts->targets );
	}

	if( DEBUG_MAKE )
	{
	    if( counts->temp )
		printf( "...using %d temp target(s)...\n", counts->temp );
	    if( counts->updating )
		printf( "...updating %d target(s)...\n", counts->updating );
	    if( counts->dontknow )
		printf( "...can't make %d target(s)...\n", counts->dontknow );
	}

	for( i = 0; i < n_targets; i++ )
	    make1( bindtarget( targets[i] ), counts );
}

/*
 * make0() - bind and scan everything to make a TARGET
 *
 * Make0() recursively binds a target, searches for #included headers,
 * calls itself on those headers, and calls itself on any dependents.
 */

static void
make0( t, parent, depth, counts, anyhow )
TARGET	*t;
time_t	parent;
int	depth;
COUNTS	*counts;
int	anyhow;
{
	TARGETS	*c;
	int	fate, hfate;
	time_t	last, hlast;
	char	*flag = "";

	if( DEBUG_MAKEPROG )
	    printf( "make\t--\t%s%s\n", spaces( depth ), t->name );

	/* 
	 * Step 1: don't remake if already trying or tried 
	 */

	switch( t->fate )
	{
	case T_FATE_MAKING:
	    printf( "warning: %s depends on itself\n", t->name );
	    return;

	default:
	    return;

	case T_FATE_INIT:
	    break;
	}

	t->fate = T_FATE_MAKING;

	/*
	 * Step 2: under the influence of "on target" variables,
	 * bind the target and search for headers.
	 */

	/* Step 2a: set "on target" variables. */

	pushsettings( t->settings );

	/* Step 2b: find and timestamp the target file (if it's a file). */

	if( t->binding == T_BIND_UNBOUND && !( t->flags & T_FLAG_NOTFILE ) )
	{
	    t->boundname = search( t->name, &t->time );
	    t->binding = t->time ? T_BIND_EXISTS : T_BIND_MISSING;
	}

	/* If temp file doesn't exist, use parent */

	if( t->binding == T_BIND_MISSING && t->flags & T_FLAG_TEMP && parent )
	{
	    t->time = parent;
	    t->binding = t->time ? T_BIND_TEMP : T_BIND_MISSING;
	}

	/* Step 2c: If its a file, search for headers. */

	if( t->binding == T_BIND_EXISTS )
	    headers( t );

	/* Step 2d: reset "on target" variables */

	popsettings( t->settings );

	/* 
	 * Step 3: recursively make0() dependents 
	 */

	/* Step 3a: recursively make0() dependents */

	last = 0;
	fate = T_FATE_STABLE;

	for( c = t->deps[ T_DEPS_DEPENDS ]; c; c = c->next )
	{
	    make0( c->target, t->time, depth + 1, counts, anyhow );
	    last = max( last, c->target->time );
	    last = max( last, c->target->htime );
	    fate = max( fate, c->target->fate );
	    fate = max( fate, c->target->hfate );
	}

	/* Step 3b: recursively make0() headers */

	hlast = 0;
	hfate = T_FATE_STABLE;

	for( c = t->deps[ T_DEPS_INCLUDES ]; c; c = c->next )
	{
	    make0( c->target, parent, depth + 1, counts, anyhow );
	    hlast = max( hlast, c->target->time );
	    hlast = max( hlast, c->target->htime );
	    hfate = max( hfate, c->target->fate );
	    hfate = max( hfate, c->target->hfate );
	}

	/* 
	 * Step 4: aftermath: determine fate and propapate dependents time
	 * and fate.
	 */

	/* Step 4a: determine fate: rebuild target or what? */
	/* If children newer than target or */
	/* If target doesn't exist, rebuild.  */

	if( fate > T_FATE_STABLE )
	{
	    fate = T_FATE_UPDATE;
	}
	else if( t->binding == T_BIND_MISSING )
	{
	    fate = T_FATE_MISSING;
	}
	else if( t->binding == T_BIND_EXISTS && last > t->time )
	{
	    fate = T_FATE_OUTDATED;
	}
	else if( t->binding == T_BIND_TEMP && last > t->time )
	{
	    fate = T_FATE_OUTDATED;
	}
	else if( t->binding == T_BIND_EXISTS && t->flags & T_FLAG_TEMP )
	{
	    fate = T_FATE_ISTMP;
	}
	else if( t->flags & T_FLAG_TOUCHED || anyhow )
	{
	    fate = T_FATE_TOUCHED;
	}

	/* Step 4b: handle missing files */
	/* If it's missing and there are no actions to create it, boom. */
	/* If we can't make a target we don't care about, 'sokay */

	if( fate == T_FATE_MISSING && !t->actions && !t->deps[ T_DEPS_DEPENDS ] )
	{
	    if( t->flags & T_FLAG_NOCARE )
	    {
		fate = T_FATE_STABLE;
	    }
	    else
	    {
		fate = T_FATE_DONTKNOW;
		printf( "don't know how to make %s\n", t->name );
	    }
	}

	/* Step 4c: Step 6: propagate dependents' time & fate. */

	t->time = max( t->time, last );
	t->fate = fate;

	t->htime = hlast;
	t->hfate = hfate;

	/* 
	 * Step 5: a little harmless tabulating for tracing purposes 
	 */

	if( !( ++counts->targets % 1000 ) && DEBUG_MAKE )
	    printf( "...patience...\n" );

	if( fate > T_FATE_ISTMP && t->actions )
	    counts->updating++;
	else if( fate == T_FATE_ISTMP )
	    counts->temp++;
	else if( fate == T_FATE_DONTKNOW )
	    counts->dontknow++;

	if( !( t->flags & T_FLAG_NOTFILE ) && fate > T_FATE_STABLE )
	    flag = "+";
	else if( t->binding == T_BIND_EXISTS && parent && t->time > parent )
	    flag = "*";

	if( DEBUG_MAKEPROG )
	    printf( "make%s\t%s\t%s%s\n", 
		flag, target_fate[ t->fate ], 
		spaces( depth ), t->name );
}

