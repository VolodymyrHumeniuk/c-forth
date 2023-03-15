/* prims.h: This file defines inline primitives, which are called as functions
   from the big SWITCH in forth.c */

// Moved here to prevent the warning - pop undefined, assuming extern returned int!
// As the result, the compiler the generate the code for 32 bit int!!! but it should be for short.
// Finally, in the optimized release version, the math and comparision words don't work and
// produce strange results... --VH

void  push( short v );
cell  pop();
void  rpush( short v );
cell  rpop();

#define top()    ( mem[csp] )      // access top stack item
#define second() ( mem[csp+1] )    // access second stack item
#define third()  ( mem[csp+2] )    // access third stack item

#define rtop()    ( mem[rsp] )      // access top item on ret stack
#define rsecond() ( mem[rsp+1] )    // access second item on ret stack
#define rthird()  ( mem[rsp+2] )    // access third item on ret stack
#define rdrop()   { rsp++; }        // drop item from return stack
#define rdrop2()  { rsp += 2; }     // drop 2 items from return stack

 /* push mem[ip] to cstack */
#define lit() { push( mem[ip++] ); }

/* add an offset (this word) to ip */
#define branch() { ip += mem[ip]; }

/* return a key from input */
#define key() { push( pkey() ); }

/* return TRUE if break key pressed */
#define qterminal() { pqterm(); }

/* and: a b -- a & b */
#define and() { push( pop() & pop() ); }

/* or: a b -- a | b */
#define or() { push( pop() | pop() ); }

/* xor: a b -- a ^ b */
#define xor() { push( ( ( pop() ) ^ ( pop() ) ) ); }

/* sp@: push the stack pointer */
#define spfetch() { push( csp ); }

/* sp!: load initial value into SP */
#define spstore() { csp = mem[S0]; }

/* rp@: fetch the return stack pointer */
#define rpfetch() { push( rsp ); }

/* rp!: load initial value into RP */
#define rpstore() { rsp = mem[R0]; }

/* ;S: ends a colon definition. */
#define semis() { ip = rpop(); }

/* @: addr -- mem[addr] */
#define fetch() { push( mem[pop()] ); }

/* C@: addr -- mem[addr] */
#define cfetch() { push( mem[pop()] & 0xff ); }

/* push to return stack */
#define tor() { rpush( pop() ); }

/* pop from return stack */
#define fromr() { push( rpop() ); }

/* 0=: a -- (a == 0) */
#define zeq() { push( ( pop() == 0 ) ? TRUE : FALSE ); }

/* 0<: a -- (a < 0) */
#define zless() { push( ( pop() < 0 ) ? TRUE : FALSE ); }

/* +: a b -- (a+b) */
#define plus() { push( pop() + pop() ); }

/* MINUS: negate a number */
#define minus() { push( -pop() ); }

/* drop: a -- */
#define drop() { pop(); }

/* DOCOL: push ip & start a thread */
#define docol() { rpush( ip ); ip = w+1; }

/* do a constant: push the value at mem[w+1] */
#define docon() { push( mem[w+1] ); }

/* do a variable: push (w+1) (the PFA) to the stack */
#define dovar() { push( w+1 ); }

/* execute a user variable: add UP to the offset found in PF */
#define douse() { push( mem[w+1] + ORIGIN ); }

#define allot() { Callot( pop() ); }

/* comparison tests */
#define equal() { push(pop() == pop()); }

/* not equal */
#define noteq() { push (pop() != pop()); }

/* DODOES -- not supported */
#define dodoes() { errexit("DOES> is not supported."); }

/* DOVOC -- not supported */
#define dovoc() { errexit("VOCABULARIES are not supported."); }

/* (BYE) -- exit with error code */
#define pbye() { exit(0); }

/* TRON -- trace at pop() depth */
#define tron() { trace = TRUE; tracedepth = pop(); }

/* TROFF -- stop tracing */
#define troff() { trace = 0; }

void zbranch();  /* add an offset (branch) if tos == 0 */
void ploop();    /* (loop) -- loop control */
void pploop();   /* (+loop) -- almost the same */
void pdo();      /* (do): limit init -- [pushed to rstack] */
void i();        /* copy top of return stack to cstack */
void r();        
void digit();    /* digit: c -- FALSE or [v TRUE] */
void pfind();
void enclose();  // Looks like it is used to fetch the word from tib?
void cmove();    /* cmove: source dest number -- */
void fill();     /* fill: c dest number -- INCORRECT!!! MUST BE addr number char -- */
void ustar();    /* u*: a b -- a*b.hi a*b.lo */
void uslash();   /* u/: NUM.LO NUM.HI DENOM -- REM QUOT */
void swap();     /* swap: a b -- b a */
void rot();      /* rotate */
void tfetch();   /* 2@: addr -- mem[addr+1] mem[addr] */
void store();    /* !: val addr -- <set mem[addr] = val> */
void cstore();   /* C!: val addr --  */
void tstore();	 /* 2!: val1 val2 addr -- */
void leave();    /* set the index = the limit of a DO */
void dplus();    /* D+: double-add */
void subtract(); /* -: a b -- (a-b) */
void dsubtract(); /* D-: double-subtract */
void dminus();   /* DMINUS: negate a double number */
void over();     /* over: a b -- a b a */
void dup();      /* dup: a -- a a */
void tdup();     /* 2dup: a b -- a b a b */
void pstore();   /* +!: val addr -- <add val to mem[addr]> */
void toggle();   /* toggle: addr bits -- <xor mem[addr] with bits, store in mem[addr]> */
void less();
void pcold();
void prslw();
void psave();

void errexit( const char* fmt, ... ); /* An error occurred -- clean up (?) and exit. */
