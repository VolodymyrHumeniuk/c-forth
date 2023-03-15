/*
 * prims.c -- code for the primitive functions declared in forth.dict
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>  /* used in "digit" */
#include "common.h"
#include "forth.h"
#include "prims.h"  /* macro primitives */

/*
             ----------------------------------------------------
                            PRIMITIVE DEFINITIONS
             ----------------------------------------------------
*/

void zbranch()   /* add an offset (branch) if tos == 0 */
{
    if( pop() == 0 ) { 
        ip += mem[ip];
    } else {
        ip++;   /* else skip over the offset */
    }
}

void ploop() /* (loop) -- loop control */
{                       // Optimized --VH
    if( rsp + 1 >= INITR0 )
        errexit( "POP FROM EMPTY RETURN STACK!" );
    cell index = rtop() + 1;
    if( index < rsecond() ) {   /* if the new index < the limit */
        rtop() = index; /* set the index (incremented) */
        branch();       /* and go back to the top of the loop */
    } else {
        rdrop2();
        ip++;           /* skip over the offset, and exit, having popped the limit & index */
    }
}

void pploop()           /* (+loop) -- almost the same */
{                       // Optimized --VH
    if( rsp + 1 >= INITR0 )
        errexit( "POP FROM EMPTY RETURN STACK!" );
    cell index = rtop() + pop();       /* get index & add increment */
    if( index < rsecond() ) { /* if new index < limit */
        rtop() = index;  /* restore the new index */
        branch();        /* and branch back to the top */
    } else {
        ip++;            /* skip over branch offset */
    }
}

void pdo()              /* (do): limit init -- [pushed to rstack] */
{
    cell init = pop();  // swap();
    cell limit = pop();
    rpush( limit );
    rpush( init );
}

void i()    /* copy top of return stack to cstack */
{
    push( rtop() );
}

void r() /* this must be a primitive as well as I because otherwise it always returns its own address */
{
    i();
}

void digit()    /* digit: c -- FALSE or [v TRUE] */
{
    /* C is ASCII char, convert to val. BASE is used for range checking */
    cell base = pop();
    cell c    = pop();
    if( !isascii( c ) ) {
        push( FALSE );
        return;
    }
    /* lc -> UC if necessary */
    if( islower( c ) ) 
        c = toupper( c );

    if( c < '0' || (c > '9' && c < 'A') || c > 'Z' ) {
        push(FALSE);    /* not a digit */
    } else {            /* it is numeric or UC Alpha */
        if( c >= 'A' )
            c -= 7;     /* put A-Z right after 0-9 */

        c -= '0';       /* now c is 0..35 */

        if( c >= base ) {
            push( FALSE );	/* FALSE - not a digit */
        } else {            /* OKAY: push value, then TRUE */
            push( c );
            push( TRUE );
        }
    }
}

void pfind()   /* WORD TOP -- xx FLAG, where TOP is NFA to start at;
            WORD is the word to find; xx is PFA of found word;
            yy is actual length of the word found;
            FLAG is 1 if found. If not found, 0 alone is stacked. */
{
    ucell  worka, workb, workc, match;

    ucell current = pop();
    ucell word    = pop();
    while( current ) {       /* stop at end of dictionary */
        if( !( (mem[current] ^ mem[word]) & 0x3f) ) {
            /* match lengths & smudge */
            worka = current + 1;/* point to the first letter */
            workb = word + 1;
            workc = mem[word];  /* workc gets count */
            match = TRUE;       /* initally true, for looping */
            while( workc-- && match ) {
                match = ( (mem[worka++] & 0x7f) == (mem[workb++] & 0x7f) );
            }
            if( match ) {                /* exited with match TRUE -- FOUND IT */
                push( worka + 2 );       /* worka=LFA; push PFA */
                push( mem[current] );    /* push length byte */
                push( TRUE );            /* and TRUE flag */
                return;
            }
        }
        /* failed to match, follow link field to next word */
        current = mem[current + (mem[current] & 0x1f) + 1];
    }
    push( FALSE );   /* current = 0; end of dict; not found */
}

void enclose() // it seems that it is used to fetch word from tib... --VH
{
    cell offset = -1;
	cell delim = pop();
	cell current = pop();
	push( current );
	current--;
encl1:
	current++;
	offset++;
	if( mem[current] == delim ) 
        goto encl1;

	push( offset );
	if( mem[current] == 0 ) {
		offset++;
		push( offset );
		offset--;
		push( offset );
		return;
	}
encl2:
	current++;
	offset++;
	if( mem[current] == delim )
        goto encl4;
	if( mem[current] != 0 ) 
        goto encl2;
	/* mem[current] is null.. */
	push( offset );
	push( offset );
	return;
encl4:	/* found the trailing delimiter */
	push( offset );
	offset++;
	push( offset );
	return;
}

void cmove()    /* cmove: source dest number -- */
{
    ucell number = pop();
    ucell dest   = pop();
    ucell source = pop();
    for( ; number ; number-- )
        mem[dest++] = mem[source++];
}

void fill()  /* fill: c dest number -- */
{
    cell number = pop(); // NB! it is different from what we have now! --VH
    cell dest   = pop();
    cell c      = pop();

    mem[dest] = c;  /* always at least one */
    if( number == 1 ) return;    /* return if only one */

    push( dest );           /* else push dest as source of cmove */
    push( dest + 1 );       /* dest+1 as dest of cmove */
    push( number - 1);      /* number-1 as number of cmove */
    cmove();
}

void ustar() /* u*: a b -- a*b.hi a*b.lo */
{
    ucell a = (ucell)pop();
    ucell b = (ucell)pop();
    ducell c = a * b;
    /* (short) -1 is probably FFFF, which is just what we want */
    push( (ucell)( c & (cell)-1) );                     /* low word of product */
    push( (cell)((c >> (8*sizeof(cell))) & (cell)-1) ); /* high word of product */
}

void uslash()   /* u/: NUM.LO NUM.HI DENOM -- REM QUOT */
{
    /* the longs below are to be sure the intermediate computation is done long; the results are short */
    ucell denom = pop();
    ucell numhi = pop();
    ucell numlo = pop();
    ucell quot = ((((ducell)numhi) << (8*sizeof(cell))) + (ducell)numlo) / (ducell)denom;
    ucell remainder = ((((ducell)numhi) << (8*sizeof(cell))) + (ducell)numlo) % (ducell)denom;
    push( remainder );
    push( quot);
}

void swap() /* swap: a b -- b a */
{
    if( csp + 1 >= INITS0 ) {
        puts( "Empty Stack!" );
        return;
    }
    cell a   = second();
    second() = top();
    top()    = a;
}

void rot()  /* rotate */
{
    cell a = pop();
    cell b = pop();
    cell c = pop();
    push( b );
    push( a );
    push( c );
}

void tfetch()    /* 2@: addr -- mem[addr+1] mem[addr] */
{
    ucell addr = pop();
    push( mem[addr + 1] );
    push( mem[addr] );
}

void store()    /* !: val addr -- <set mem[addr] = val> */
{
    ucell tmp = pop();
    mem[tmp]  = pop();
}

void cstore()   /* C!: val addr --  */
{
    // store(); /* original implementation was - INCORRECT!!! */
    /* should be as below, but still commented for investigation reasons */
    ucell addr = pop();
    byte* p = (byte *)&mem[addr];
    *p = (byte)( pop() & 0xFF );
}

void tstore()	/* 2!: val1 val2 addr -- mem[addr] = val2, mem[addr+1] = val1 */
{
    ucell tmp  = pop();
    mem[tmp]   = pop();
    mem[tmp+1] = pop();
}

void leave()    /* set the index = the limit of a DO */
{
    rpop();             /* discard old index */
    cell tmp = rpop();  /* and push the limit as */
    rpush( tmp );       /* both the limit */
    rpush( tmp );       /* and the index */
}

void dplus()        /* D+: double-add */
{
    cell bhi = pop();
    cell blo = pop();
    cell ahi = pop();
    cell alo = pop();
    dcell a = ((dcell)ahi << (8*sizeof(cell))) + (dcell)alo;
    dcell b = ((dcell)bhi << (8*sizeof(cell))) + (dcell)blo;
    a = a + b;
    push( (ucell)(a & (cell)-1) );             /* sum lo */
    push( (cell)(a >> (8*sizeof(cell))) );     /* sum hi */
}

void subtract() /* -: a b -- (a-b) */
{
    cell tmp = pop();
    push( pop() - tmp );
}

void dsubtract()     /* D-: double-subtract */
{
    cell bhi = pop();
    cell blo = pop();
    cell ahi = pop();
    cell alo = pop();
    dcell a = ((dcell)ahi << (8*sizeof(cell))) + (dcell)alo;
    dcell b = ((dcell)bhi << (8*sizeof(cell))) + (dcell)blo;
    a = a - b;
    push( (ucell)(a & (cell)-1) );           /* diff lo */
    push( (cell)(a >> (8*sizeof(cell))) );   /* diff hi */
}

void dminus()   /* DMINUS: negate a double number */
{
    ucell ahi = pop();
    ucell alo = pop();
    dcell a = -(((dcell)ahi << (8*sizeof(cell))) + (dcell)alo);
    push( (ucell)(a & (cell) -1) );         /* -a lo */
    push( (ucell)(a >> (8*sizeof(cell))) ); /* -a hi */
}

void over()  /* over: a b -- a b a */
{
    cell b = pop();
    cell a = pop();
    push( a );
    push( b );
    push( a );
}

void dup()   /* dup: a -- a a */
{
    cell a = pop();
    push( a );
    push( a );
}

void tdup()  /* 2dup: a b -- a b a b */
{
    cell b = pop();
    cell a = pop();
    push( a );
    push( b );
    push( a );
    push( b );
}

void pstore()   /* +!: val addr -- <add val to mem[addr]> */
{
    cell addr = pop();
    cell val = pop();
    mem[addr] += val;
}

void toggle()    /* toggle: addr bits -- <xor mem[addr] with bits, store in mem[addr]> */
{
    cell bits = pop();
    cell addr = pop();
    mem[addr] ^= bits;
}

void less()
{
    cell tmp = pop();
    push( pop() < tmp );
}

void pcold()
{
    csp = INITS0;   /* initialize values */
    rsp = INITR0;
    /* copy USER_DEFAULTS area into UP area */
    push( USER_DEFAULTS );   /* source */
    push( UP );              /* dest */
    push( DEFS_SIZE );       /* count */
    cmove();                 /* move! */
    /* returns, executes ABORT */
}

void prslw() // looks like it is used to load screen from block file
{
    int buffer, flag, addr, i, unWrittenFlag = 0; // was not inited but checked in if
    long fpos, ftell();
    char buf[1024]; /* holds data for xfer */
    size_t temp;

    flag   = pop();
    buffer = pop();
    addr   = pop();
    fpos   = (long)( buffer * 1024 );

    /* extend if necessary */
    if( fpos >= bfilesize ) {
        if( flag == 0 ) {       /* write */
            printf( "Extending block file to %d bytes\n", fpos+1024 );
            /* the "2" below is the fseek magic number for "beyond end" */
            fseek( blockfile, (fpos+1024) - bfilesize, 2 );
            bfilesize = ftell( blockfile );
        } else {                    /* reading unwritten data */
            unWrittenFlag = TRUE;   /* will read all zeroes */
        }
    } else {  /* note that "0" below is fseek magic number for "relative to
        beginning-of-file" */
        fseek( blockfile, fpos, 0 );  /* seek to destination */
    }

    if( flag ) {                /* read */
        if( unWrittenFlag ) {   /* not written yet */
            for( i = 0; i < 1024; i++ ) 
                mem[addr++] = 0;    /* "read" nulls */
        } else {                    /* does exist */
            if( (temp = fread( buf, sizeof(char), 1024, blockfile ) ) != 1024 ) {
                errexit( "File read error %d reading buffer %d\n", temp, buffer );
            }
            for( i = 0; i < 1024; i++ ) 
                mem[addr++] = buf[i];
        }
    } else {    /* write */
        for( i = 0; i < 1024; i++ ) {
            // UNCLEAR, L-value is short (2bytes), R-value is byte, what should be the result? --VH
            buf[i] = (char)( mem[addr++] );
        }
        if( (temp = fwrite (buf, sizeof(char), 1024, blockfile)) != 1024 ) {
            errexit( "File write error %d writing buffer %d\n", temp, buffer );
        }
    }
}

void psave() // SAVE ( -- )
{
    FILE *fp;

    printf( "\nSaving..." );
    fflush( stdout );
    mem[SAVEDIP] = ip;	/* save state */
    mem[SAVEDSP] = csp;
    mem[SAVEDRP] = rsp;

    if( (fp = fopen(sfilename,"w")) == NULL )  /* open for writing only */
        errexit( "Can't open core file %s for writing\n", sfilename );
    
    if( fwrite( mem, sizeof(mem), mem[0], fp) != mem[0] )
        errexit( "Write error on %s\n", sfilename );

    if( fclose( fp ) == EOF )
        errexit( "Close error on %s\n", sfilename );

    puts("Saved. Exit FORTH.");
    exit( 0 );
}
