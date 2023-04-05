/****************************************************************************/
/*																			*/
/*	Module:			jamycskl.c												*/
/*																			*/
/*					Copyright (C) Altera Corporation 1997					*/
/*																			*/
/*	Description:	LALR parser driver skeleton file -- used by YACC		*/
/*																			*/
/****************************************************************************/


#ifndef INITIALIZE
#define INITIALIZE
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 200 /* default stack depth */
#endif

#ifndef yyerrok
#define yyerrok ((int) 0)
#endif

#define YYFLAG -1000
#define YYERROR goto yyerrlab
#define YYACCEPT return(0)
#define YYABORT return(1)

YYSTYPE yyv[YYMAXDEPTH];
int token = -1; /* input token */
int errct = 0;  /* error count */
int errfl = 0;  /* error flag */

yyparse()
{ int yys[YYMAXDEPTH];
  int yyj, yym;
  YYSTYPE *yypvt;
  int yystate, *yyps, yyn;
  
  int *yyxi;

  YYSTYPE *yypv;

  yystate = 0;
  token = -1;
  errct = 0;
  errfl = 0;
  yyps= &yys[-1];
  yypv= &yyv[-1];


 yystack:    /* put a state and value onto the stack */

  if( ++yyps> &yys[YYMAXDEPTH] ) { yyerror( "yacc stack overflow" ); return(1); }
    *yyps = yystate;
    ++yypv;
    *yypv = yyval;

  yynewstate:

    yyn = yypact[yystate];

    if( yyn<= YYFLAG ) goto yydefault; /* simple state */

    if( token<0 ) if( (token=yylex())<0 ) token=0;
    if( (yyn += token)<0 || yyn >= YYLAST ) goto yydefault;

    if( yychk[ yyn=yyact[ yyn ] ] == token ){ /* valid shift */
      token = -1;
      yyval = yylval;
      yystate = yyn;
      if( errfl > 0 ) --errfl;
      goto yystack;
    }

 yydefault:

    if( (yyn=yydef[yystate]) == -2 ) {
      if( token<0 ) if( (token=yylex())<0 ) token = 0;
      /* look through exception table */

      for( yyxi=yyexca; (*yyxi!= (-1)) || (yyxi[1]!=yystate) ; yyxi += 2 ) ; /* VOID */

      while( *(yyxi+=2) >= 0 ){
        if( *yyxi == token ) break;
      }
      if( (yyn = yyxi[1]) < 0 ) return(0);   /* accept */
    }

    if( yyn == 0 ){ /* error */

      switch( errfl ){
      	case 0:   /* brand new error */
          yyerror( "syntax error" );
      	  yyerrlab:
          ++errct;

       	case 1:
	  case 2: /* incompletely recovered error ... try again */
	    errfl = 3;

	    /* find a state where "error" is a legal shift action */

	    while ( yyps >= yys ) {
	      yyn = yypact[*yyps] + YYERRCODE;
	      if( yyn>= 0 && yyn < YYLAST && yychk[yyact[yyn]] == YYERRCODE ){
                yystate = yyact[yyn];  /* simulate a shift of "error" */
	        goto yystack;
	      }
	      yyn = yypact[*yyps];
	      /* the current yyps has no shift onn "error", pop stack */
	      --yyps;
	      --yypv;
	    }

	    /* there is no state on the stack with an error shift ... abort */

	yyabort:
	    return(1);
	    case 3:  /* no shift yet; clobber input char */

	    if( token == 0 ) goto yyabort; /* don't discard EOF, quit */
	      token = -1;
	      goto yynewstate;   /* try again in the same state */
	    }

	  }

          /* reduction by production yyn */

	  yyps -= yyr2[yyn];
	  yypvt = yypv;
	  yypv -= yyr2[yyn];
	  yyval = yypv[1];
	  yym=yyn;
	  /* consult goto table to find next state */
	  yyn = yyr1[yyn];
	  yyj = yypgo[yyn] + *yyps + 1;
	  if( yyj>=YYLAST || yychk[ yystate = yyact[yyj] ] != -yyn ) yystate = yyact[yypgo[yyn]];
	    switch(yym){
	    	$A
	    }
	    goto yystack;  /* stack new state and value */

	}
