/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * mkjambase.c - turn Jambase into a big C structure
 *
 * Usage: mkjambase jambase.c Jambase ...
 *
 * Results look like this:
 *
 *	 char *jambase[] = {
 *	 "...\n",
 *	 ...
 *	 0 };
 *
 * Handles \'s and "'s specially; knows to delete blank and comment lines.
 *
 */

# include <stdio.h>
# include <string.h>

main( argc, argv )
int argc;
char **argv;
{
	char buf[ 1024 ];
	FILE *fin;
	FILE *fout;
	char *p;
	int doDotC = 0;

	if( argc < 3 )
	{
	    fprintf( stderr, "usage: %s jambase.c Jambase ...\n", argv[0] );
	    return -1;
	}

	if( !( fout = fopen( argv[1], "w" ) ) )
	{
	    perror( argv[1] );
	    return -1;
	}

	/* If the file ends in .c generate a C source file */

	if( ( p = strrchr( argv[1], '.' ) ) && !strcmp( p, ".c" ) )
	    doDotC++;

	/* Now process the files */

	argc -= 2, argv += 2;

	if( doDotC )
	{
	    fprintf( fout, "/* Generated by mkjambase from Jambase */\n" );
	    fprintf( fout, "char *jambase[] = {\n" );
	}

	for( ; argc--; argv++ )
	{
	    if( !( fin = fopen( *argv, "r" ) ) )
	    {
		perror( *argv );
		return -1;
	    }

	    if( doDotC )
	    {
		fprintf( fout, "/* %s */\n", *argv );
	    }
	    else
	    {
		fprintf( fout, "### %s ###\n", *argv );
	    }

	    while( fgets( buf, sizeof( buf ), fin ) )
	    {
		if( doDotC )
		{
		    char *p = buf;

		    /* Strip leading whitespace. */

		    while( *p == ' ' || *p == '\t' || *p == '\n' )
			p++;

		    /* Drop comments and empty lines. */

		    if( *p == '#' || !*p )
			continue;

		    /* Copy */

		    putc( '"', fout );

		    for( ; *p && *p != '\n'; p++ )
			switch( *p )
		    {
		    case '\\': putc( '\\', fout ); putc( '\\', fout ); break;
		    case '"': putc( '\\', fout ); putc( '"', fout ); break;
		    default: putc( *p, fout ); break;
		    }

		    fprintf( fout, "\\n\",\n" );
		}
		else
		{
		    fprintf( fout, "%s", buf );
		}

	    }

	    fclose( fin );
	}
	    
	if( doDotC )
	    fprintf( fout, "0 };\n" );

	fclose( fout );

	return 0;
}
