/* nf.c -- this program can be run to generate a new environment for the
 * FORTH interpreter forth.c. It takes the dictionary from the standard input.
 * Normally, this dictionary is in the file "forth.dict", so 
 *  nf < forth.dict
 * will do the trick.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include ".\..\common.h"
#include "forth.lex.h"      /* #defines for lexical analysis */

#ifdef DEBUG
    #define DBGOUT printf
#else
    #define DBGOUT __noop
#endif

#define isoctal(c)  (c >= '0' && c <= '7')  /* augument ctype.h */

#define assert(c,s) (!(c) ? failassert(s) : 1)
#define chklit()    (!prev_lit ? dictwarn("Qustionable literal") : 1)

#define LINK struct linkrec
#define CHAIN struct chainrec

struct chainrec {
    char chaintext[32];
    int defloc;             /* CFA or label loc */
    int chaintype;          /* 0=undef'd, 1=absolute, 2=relative */
    CHAIN *nextchain;
    LINK *firstlink;
};

struct linkrec {
    int loc;
    LINK *nextlink;
};

CHAIN firstchain;

#define newchain()  (CHAIN *)(calloc(1,sizeof(CHAIN)))
#define newlink()   (LINK *)(calloc(1,sizeof(LINK)))

void failassert( char* s );
CHAIN *find();
CHAIN *lastchain();
LINK *lastlink();
void dicterr( char* s, char* p1 );  /* might be char * -- printf uses it */
void dictwarn( char* s );           /* almost like dicterr, but don't exit */
void comma( int i );                /* put at mem[dp]; increment dp */
void mkword( char* s, short v );    // make a dictionary entry
int  mkval( TOKEN* t );              /* convert t->text to integer based on type */
void mkstr( char* s );    /* modifies a string in place with escapes compacted. Strips leading & trailing \" */
int  mkhex( char* s );    /* convert hex ascii to integer */
int  mkoctal( char* s );  /* convert Octal ascii to integer */
int mkdecimal( char* s ); /* convert ascii to decimal */
int  instance( char* s );
void define( char* s, int t );      /* define s at current dp */
void skipcomment();
void printword( int n ); /* prints word from specified memory address */

int dp = DPBASE;    // dictionary pointer
int latest;         // index of latest word
short mem[INITMEM]; // the forth memory where system is bootstraped

char* inputBuffer = NULL;   // input dict file
int   inpSize = 0;          // size of input file
int   inPos = 0;            // current position in input buffer

void init_input( const char* fileName )
{
    FILE* fp = fopen( fileName, "rb" );
    if( fp ) {
        fseek( fp, 0, SEEK_END );
        inpSize = ftell( fp );
        fseek( fp, 0, SEEK_SET );
        inputBuffer = malloc( inpSize + 1 );
        if( inputBuffer ) {
            fread( inputBuffer, 1, inpSize, fp );
        } else {
            fclose( fp );
            dicterr( "Unable to allocate memory: %d bytes\n", inpSize );
        }
        fclose( fp );
    } else {
        dicterr( "Unable to open input file", NULL );
    }
}

void free_buffer()
{
    if( inputBuffer ) {
        free( inputBuffer );
        inputBuffer = NULL;
    }
}

char my_getchar() {
    if( inPos < inpSize ) {
        return inputBuffer[inPos++];
    } else {
        return EOF;
    }
}

void buildcore() /* set up low core */
{
    mem[USER_DEFAULTS+0] = INITS0;      /* initial S0 */
    mem[USER_DEFAULTS+1] = INITR0;      /* initial R0 */
    mem[USER_DEFAULTS+2] = TIB_START;   /* initial TIB */
    mem[USER_DEFAULTS+3] = MAXWIDTH;    /* initial WIDTH */
    mem[USER_DEFAULTS+4] = 0;           /* initial WARNING */
    mem[USER_DEFAULTS+5] = dp;          /* initial FENCE */
    mem[USER_DEFAULTS+6] = dp;          /* initial DP */
    mem[USER_DEFAULTS+7] = instance( "FORTH" ) + 3;   /* initial CONTEXT */
    mem[SAVEDIP] = 0;                   /* not a saved FORTH */
}

void builddict() /* read the dictionary */
{
    int prev_lit = 0, lit_flag = 0;
    int temp;
    char s[256];
    TOKEN *token;

    while( (token = yylex()) != NULL ) /* EOF returned as a null pointer */
    { 
        DBGOUT( "\ntoken: %s: %d ",token->text, token->type );

        switch (token->type)
        {
        case PRIM:
            DBGOUT( "primitive ");
            if( (token = yylex()) == NULL ) /* get the next word */
                dicterr( "No word following PRIM", NULL );
            strcpy( s, token->text );

            DBGOUT( ".%s. ",s );
            if( (token == yylex()) == NULL ) /* get the value */
                dicterr( "No value following PRIM <word>", NULL );
            mkword( s, mkval( token ) );
            break;

        case CONST:
            DBGOUT( "constant " );
            if( (token = yylex()) == NULL ) /* get the word */
                dicterr( "No word following CONST", NULL );
            strcpy( s, token->text ); /* s holds word */

            DBGOUT( ".%s. ", s );
            if( !find( "DOCON" ) )
                dicterr( "Constant definition before DOCON: %s", s );
            /* put the CF of DOCON into this word's CF */
            mkword( s, (int)mem[instance("DOCON")] );
            if( (token = yylex()) == NULL ) /* get the value */
                dicterr( "No value following CONST <word>", NULL );
            temp = mkval( token );

            if( strcmp( s, "FIRST" ) == 0 ) { /* two special-case constants */
                temp = INITR0;
            } else if( strcmp( s, "LIMIT" ) == 0 ) {
                temp = DPBASE;
            }

            comma( temp );
            break;

        case VAR:
            DBGOUT( "variable " );
            if( (token = yylex()) == NULL)  /* get the variable name */
                dicterr( "No word following VAR", NULL );
            strcpy( s, token->text );

            DBGOUT( ".%s. ",s );
            if( !find( "DOVAR" ) )
                dicterr( "Variable declaration before DOVAR: %s", s );

            mkword( s, (int)mem[instance("DOVAR")] );
            if( (token = yylex()) == NULL ) /* get the value */
                dicterr( "No value following VAR <word>", NULL );
            comma( mkval(token) );
            break;

        case USER:
            DBGOUT( "uservar " );
            if( (token = yylex()) == NULL)  /* get uservar name */
                dicterr( "No name following USER", NULL );
            strcpy( s, token->text );

            DBGOUT( ".%s. ",s );
            if( !find("DOUSE") )
                dicterr( "User variable declared before DOUSE: %s", s );
            mkword( s, (int)mem[instance("DOUSE")] );
            if( (token = yylex()) == NULL ) /* get the value */
                dicterr( "No value following USER <word>", NULL );
            comma( mkval( token ) );
            break;

        case COLON:
            DBGOUT( "colon def'n " );
            if( (token = yylex()) == NULL ) /* get name of word */
                dicterr( "No word following : in definition", NULL );
            strcpy( s,token->text );

            DBGOUT(".%s.\n",s);
            if( !find("DOCOL") )
                dicterr( "Colon definition appears before DOCOL: %s", s );

            if( token->type == NUL ) {   /* special zero-named word */
                int here = dp;      /* new latest */
                DBGOUT( "NULL WORD AT 0x%04x\n", dp );
                comma( 0xC1 );
                comma( 0x80 );
                comma( latest );
                latest = here;
                comma( (int)mem[instance("DOCOL")] );
            } else {
                mkword( s, (int)mem[instance("DOCOL")] );
            }
            break;

        case SEMICOLON:
            DBGOUT( "end colon def'n\n" );
            comma( instance(";S") );
            break;

        case SEMISTAR:
            DBGOUT("end colon w/IMMEDIATE ");
            comma( instance (";S") ); /* compile cfA of ;S, not CF */
            mem[latest] |= IMMEDIATE; /* make the word immediate */
            break;

        case STRING_LIT:
            DBGOUT( "string literal " );
            strcpy( s, token->text );
            mkstr( s );   /* mkstr compacts the string in place */
            DBGOUT( "string=(%zd) \"%s\" ", strlen( s ), s );
            comma( (int)strlen( s ) );
            {
                char *stemp = s;
                while( *stemp ) {
                    comma( *stemp++ );
                }
            }
            break;

        case COMMENT:
            DBGOUT( "comment " );
            skipcomment();
            break;

        case LABEL:
            DBGOUT( "label: " );
            if( (token = yylex()) == NULL )
                dicterr( "No name following LABEL", NULL );
            DBGOUT( ".%s. ", token->text );
            /* place in sym. table w/o compiling anything into dictionary; 2 means defining a label */
            define( token->text, 2 ); 
            break;

        case LIT:
            lit_flag = 1;   /* and fall through to the rest */
        default:
            if( find( token->text ) != NULL ) {    /* is word defined? */
                DBGOUT("  normal: %s\n",token->text);
                comma( instance( token->text ) );
                break;
            }
            /* else */
            /* the literal types all call chklit(). This macro checks to
                if the previous word was "LIT"; if not, it warns */
            switch(token->type) {
            case DECIMAL:   chklit(); comma( mkdecimal( token->text ) ); break;
            case HEX:       chklit(); comma( mkhex( token->text ) );    break;
            case OCTAL:     chklit(); comma( mkoctal( token->text ) );  break;
            case C_BS:      chklit(); comma( '\b' );                    break;
            case C_FF:      chklit(); comma( '\f' );                    break;
            case C_NL:      chklit(); comma( '\n' );                    break;
            case C_CR:      chklit(); comma( '\r' );                    break;
            case C_TAB:     chklit(); comma( '\t' );                    break;
            case C_BSLASH:  chklit(); comma( 0x5c );                    break;  /* ASCII backslash */
            case C_LIT:     chklit(); comma( *((token->text)+1) );      break;
            default:
                DBGOUT( "forward reference" );
                comma( instance( token->text ) ); /* create an instance, to be resolved at definition */
            }
        }
#ifdef DEBUG
    if( lit_flag ) DBGOUT("expect a literal");
#endif DEBUG
        prev_lit = lit_flag;    /* to be used by chklit() next time */
        lit_flag = 0;
    }
}

void comma( int i )    /* put at mem[dp]; increment dp */
{
    mem[dp++] = (unsigned short)i;
    if( dp > INITMEM ) 
        dicterr( "DICTIONARY OVERFLOW", NULL );
}

/*
 * make a word in the dictionary.  the new word will have name *s, its CF
 * will contain v. Also, resolve any previously-unresolved references by
 * calling define()
 */
void mkword( char* s, short v )
{
    int here, count = 0;
    char *olds;
    olds = s;   /* preserve this for resolving references */

    DBGOUT( "%s ",s );

    here = dp;  /* hold this value to place length byte */

    while( *s ) {    /* for each character */
        mem[++dp] = (unsigned short)*s; // VH: it uses 2 bytes per character!!! why???
        count++; s++;
    }

    if( count >= MAXWIDTH )
        dicterr( "Input word name too long", NULL );

    mem[here] = (short)(count | 0x80); /* set MSB on */
    mem[dp++] |= 0x80;          /* set hi bit of last char in name */
    mem[dp++] = (short)latest;  /* the link field */
    latest = here;              /* update the link */
    mem[dp] = v;                /* code field; leave dp = CFA */
    define( olds, 1 );          /* place in symbol table. 1 == "not a label" */
    dp++;                       /* now leave dp holding PFA */
    /* that's all. Now dp points (once again) to the first UNallocated spot in mem, and everybody's happy. */
}

/* Write out the word FORTH as a no-op with DOCOL as CF, ;S as PF, followed by
 0xA081, and latest in its PF. Also, Put the CFA of ABORT at mem[COLDIP] */
void mkrest()    
{
    mem[COLDIP] = dp;   /* the cold-start IP is here, and the word
                        which will be executed is COLD */
    if( (mem[dp++] = instance ("COLD" ) ) == 0 )
        dicterr( "COLD must be defined to take control at startup", NULL );

    mem[ABORTIP] = dp;  /* the abort-start IP is here, and the word
                        which will be executed is ABORT */
    if( ( mem[dp++] = instance("ABORT") ) == 0 )
        dicterr( "ABORT must be defined to take control at interrupt", NULL );

    mkword( "FORTH", mem[instance("DOCOL")] );
    comma( instance(";S") );
    comma( 0xA081 );  /* magic number for vocabularies */
    comma( latest );  /* NFA of last word in dictionary: FORTH */

    mem[LIMIT] = dp + 1024;
    if (mem[LIMIT] >= INITMEM) 
        mem[LIMIT] = INITMEM-1;
}

void writedict() /* write memory to COREFILE and map to MAPFILE */
{
    FILE   *outfile;
    int     i, temp, tempb, firstzero, nonzero;
    char    chars[9], outline[80], tstr[6];

    outfile = fopen( MAPFILE, "w" );

    for( temp = 0; temp < dp; temp += 8 ) {
        nonzero = FALSE;
        sprintf( outline, "%04x:", temp );
        for( i = temp; i < temp + 8; i++ ) {
            sprintf( tstr, " %04x", (unsigned short)mem[i] );
            strcat( outline, tstr );
            tempb = mem[i] & 0x7f;
            if( tempb < 0x7f && tempb >= ' ' ) {
                chars[i % 8] = tempb;
            } else {
                chars[i % 8] = '.';
            }
            nonzero |= mem[i];
        }
        if( nonzero ) {
            fprintf( outfile, "%s %s\n", outline, chars );
            firstzero = TRUE;
        } else if (firstzero) {
            fprintf( outfile, "----- ZERO ----\n" );
            firstzero = FALSE;
        }
    }
    fclose( outfile );

    printf( "Writing %s; DPBASE=%d; dp=%d\n", COREFILE, DPBASE, dp );

    FILE *outf = NULL;
    if( (outf = fopen( COREFILE, "wb") ) == NULL ) {
        printf( "nf: can't open %s for output.\n", COREFILE );
        exit( 1 );
    }

    if( fwrite( mem, sizeof (*mem), mem[LIMIT], outf ) != mem[LIMIT] ) {
        fprintf( stderr, "Error writing to %s\n", COREFILE );
        exit( 1 );
    }

    if( fclose( outf ) == EOF ) {
        fprintf( stderr, "Error closing %s\n", COREFILE );
        exit( 1 );
    }
}

int mkval(TOKEN* t)    /* convert t->text to integer based on type */
{
    char *s = t->text;
    int sign = 1;

    if( *s == '-' ) {
        sign = -1;
        s++;
    }

    switch( t->type ) {
    case DECIMAL:
        return ( sign * mkdecimal( s ) );
    case HEX:
        return ( sign * mkhex( s ) );
    case OCTAL:
        return ( sign * mkoctal( s ) );
    default:
        dicterr( "Bad value following PRIM, CONST, VAR, or USER", NULL );
        break;
    }
    return 0; // shut the compiler
}

int mkhex( char* s ) /*  convert hex ascii to integer */
{   
    int     temp;
    temp = 0;

    s += 2;                     /* skip over '0x' */
    while( isxdigit (*s) ) {    /* first non-hex char ends */
        temp <<= 4;             /* mul by 16 */
        if( isupper( *s ) ) {
            temp += (*s - 'A') + 10;
        } else {
            if( islower( *s ) )
                temp += (*s - 'a') + 10;
            else
                temp += (*s - '0');
        }
        s++;
    }
    return temp;
}

int mkoctal( char* s ) /*  convert Octal ascii to integer */
{   
    int     temp;
    temp = 0;

    while( isoctal (*s) ) { /* first non-octal char ends */
        temp = temp * 8 + (*s - '0');
        s++;
    }
    return temp;
}

int mkdecimal( char* s )    /* convert ascii to decimal */
{
    return ( atoi ( s ) );   /* alias */
}

void dicterr( char* s, char* p1 ) /* might be char * -- printf uses it */
{
    fprintf( stderr, s, p1 );
    fprintf( stderr, "\nLast word defined was " );
    printword( latest );
/*    fprintf(stderr, "; last word read was \"%s\"", token->text); */
    fprintf( stderr,"\n" );
    exit( 1 );
}

void dictwarn( char* s ) /* almost like dicterr, but don't exit */
{
    fprintf( stderr, "\nWarning: %s\nLast word read was ", s );
    printword( latest );
    putc( '\n', stderr );
}

void printword( int n ) /* prints word from specified memory address */
{
    int count, tmp;
    count = mem[n] & 0x1f;
    for( n++; count; count--, n++ ) {
        tmp = mem[n] & ~0x80;   /* mask eighth bit off */
        if (tmp >= ' ' && tmp <= '~') 
            putc( tmp, stderr );
    }
}

void skipcomment()
{
    while( my_getchar() != ')' ) ;
}

void mkstr( char* s ) /* modifies a string in place with escapes compacted. Strips leading & trailing \" */
{
    char *source;
    char *dest;

    source = dest = s;
    source++;                   /* skip leading quote */
    while( *source != '"' ) {    /* string ends with unescaped \" */
        if( *source == '\\' ) {  /* literal next */
            source++;
        }
        *dest++ = *source++;
    }
    *dest = '\0';
}

void failassert( char* s )
{
    puts( s );
    exit( 1 );
}

checkdict() /* check for unresolved references */
{
    CHAIN *ch = &firstchain;
    DBGOUT("\nCheck for unresolved references\n");

    while( ch != NULL ) {
        DBGOUT( "ch->chaintext = .%s. - ", ch->chaintext );
        if( (ch->firstlink) != NULL ) {
            fprintf( stderr, "Unresolved forward reference: %s\n", ch->chaintext );
            DBGOUT( "still outstanding!\n" );
        } else {
            DBGOUT( "clean.\n" );
        }
        ch = ch->nextchain;
    }
}

/********* structure-handling functions find(s), define(s,t), instance(s) **/

CHAIN *find( char* s )  /* returns a pointer to the chain named s */
{
    CHAIN *ch = &firstchain;
    while( ch != NULL ) {
        if( strcmp( s, ch->chaintext ) == 0 )
            return ch;
        else 
            ch = ch->nextchain;
    }
    return NULL;    /* not found */
}

/* define must create a symbol table entry if none exists, with type t.
   if one does exist, it must have type 0 -- it is an error to redefine
   something at this stage. Change to type t, and fill in the outstanding
   instances, with the current dp if type=1, or relative if type=2. */

void define( char* s, int t ) /* define s at current dp */
{
    CHAIN *ch;
    LINK *ln, *templn;
    DBGOUT("define(%s,%d)\n",s,t);

    if( t < 1 || t > 2 ) /* range check */
        dicterr( "Program error: type in define() not 1 or 2.", NULL );

    if( ( ch = find( s ) ) != NULL ) {   /* defined or instanced? */
        if (ch -> chaintype != 0) { /* already defined! */
            dicterr( "Word already defined: %s", s );
        } else {
            DBGOUT("there are forward refs: ");
            ch->chaintype = t;
            ch->defloc = dp;
        }
    } else {    /* must create a (blank) chain */
        DBGOUT("no forward refs\n");
        /* create a new chain, link it in, leave ch pointing to it */
        ch = ( (lastchain() -> nextchain) = newchain() );
        strcpy( ch->chaintext, s );
        ch->chaintype = t;
        ch->defloc = dp;    /* fill in for future references */
    }

    /* now ch points to the chain (possibly) containing forward refs */
    if( ( ln = ch->firstlink ) == NULL ) 
        return;   /* no links! */

    while( ln != NULL ) {
        DBGOUT( "    Forward ref at 0x%x\n", ln->loc );

        switch( ch->chaintype ) {
        case 1: mem[ln->loc] = (short)dp;   /* absolute */
            break;
        case 2: mem[ln->loc] = (short)(dp - ln->loc);   /* relative */
            break;
        default: 
            dicterr( "Bad type field in define()", NULL );
        }

        /* now skip to the next link & free this one */
        templn = ln;
        ln = ln->nextlink;
        free( templn );
    }
    ch->firstlink = NULL;   /* clean up that last pointer */
}

/* instance must return a value to be compiled into the dictionary at
   dp, consistent with the symbol s: if s is undefined, it returns 0,
   and adds this dp to the chain for s (creating that chain if necessary).
   If s IS defined, it returns <s> (absolute) or (s-dp) (relative), 
   where <s> was the dp when s was defined.
*/
int instance( char* s ) // Nowadays we would say that it returns XT. --VH
{
    CHAIN *ch;
    LINK *ln;
    DBGOUT( "instance(%s):\n", s );

    if( (ch = find(s)) == NULL ) {   /* not defined yet at all */
        DBGOUT( "entirely new -- create a new chain\n" );
        /* create a new chain, link it in, leave ch pointing to it */
        ch = ( (lastchain() -> nextchain) = newchain() );
        strcpy( ch->chaintext, s );
        ln = newlink();     /* make its link */
        ch->firstlink = ln;
        ln->loc = dp;       /* store this location there */
        return 0;           /* all done */
    } else {
        switch( ch->chaintype ) {
        case 0:     /* not defined yet */
            DBGOUT( "still undefined -- add a link\n" );
            /* create a new link, point the last link to it, and
            fill in the loc field with the current dp */
            ( lastlink(ch)->nextlink = newlink() )->loc = dp;
            return 0;
        case 1: /* absolute */
            DBGOUT( "defined absolute." );
            return ch->defloc;
        case 2: /* relative */
            DBGOUT( "defined relative." );
            return ch->defloc - dp;
        default:
            dicterr( "Program error: bad type for chain", NULL );
            break;
        }
    }
    return 0;
}

CHAIN *lastchain()  /* starting from firstchain, find the last chain */
{
    CHAIN *ch = &firstchain;
    while( ch->nextchain != NULL )
        ch = ch->nextchain;
    return ch;
}

/* CHAIN MUST HAVE AT LEAST ONE LINK */
LINK *lastlink( CHAIN *ch )  /* return the last link in the chain */
{
    LINK *ln = ch->firstlink;
    while( ln->nextlink != NULL )
        ln = ln->nextlink;
    return ln;
}

int yywrap()    /* called by yylex(). returning 1 means "all finished" */
{
    return 1;
}

void main( int argc, char* argv[] )
{
    DBGOUT( "Opening output file\n" );

    if( argc < 2 ) {
        printf( "input dict file exepcted\n" );
        return;
    }

    init_input( argv[1] );              // load dict file
    memset( &mem[0], 0, sizeof(mem) );  // zero block

    strcpy( firstchain.chaintext," ** HEADER **" );
    firstchain.nextchain = NULL;
    firstchain.firstlink = NULL;

    DBGOUT( "call builddict\n" );
    builddict();    // the main function that bootstraps the system

    DBGOUT( "Make FORTH and COLDIP\n" );
    mkrest();

    DBGOUT( "Call Buildcore\n" );
    buildcore();

    DBGOUT( "call checkdict\n" );
    checkdict();

    DBGOUT( "call writedict\n" );
    writedict();

    free_buffer();

    printf("%s: done.\n", argv[0]);
}
