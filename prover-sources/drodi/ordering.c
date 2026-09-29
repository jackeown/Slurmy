/*
 ============================================================================
 Name        : ordering.c
 Author      : Oscar Contreras
 Version     :
 Copyright   : Copyright (C) 2015-2016 Oscar Contreras. All rights reserved.
 Description : First order logic theorem prover in C
 ============================================================================
 */

/****************************************************************
*
*                 ordering (ordering functions module)
*
* All of the documentation and software included in the Drodi package
* is copyrighted by Oscar Contreras.
*
* Copyright (C) 2015-2016 Oscar Contreras. All rights reserved.
*
*
* This software is provided "as-is," without any express or implied warranty.
* In no event shall the author be held liable for any damages arising from the
* use, or the result of use, of this software and its documentation in terms
* of correctness, accuracy, reliability, currentness, or otherwise.
*
* Permission is granted to anyone to use this software for any purpose,
* including commercial use, and to alter and redistribute it, provided
* that the following conditions are met:
*
* 1. All redistributions of source code files must retain all copyright
*    notices that are currently in place, and this list of conditions without
*    modification.
*
* 2. All redistributions in binary form must retain all occurrences of the
*    above copyright notice that are currently in place (for example, in
*    the About boxes).
*
* 3. The origin of this software must not be misrepresented; you must not
*    claim that you wrote the original software.
*
* 4. Modified versions in source or binary form must be plainly marked as
*    such, and must not be misrepresented as being the original software.
*
*
* Oscar Contreras
* rwtrainer10 AT gmail.com
*
****************************************************************/

/*-------------------------------------------------------------
 *
 *  DESCRIPTION:
 *
 *    This source contains the ordering functions of the Drodi package.
 *
 *    Drodi is a multi-platform Open Source Artificial Intelligence application.
 *    See Drodi.c for additional information.
 *
 *
 *  IMPLEMENTATION:
 *
 *    Multi-platform
 *
 *  NOTES:
 *
 *
 *--------------------------------------------------------------*/

/* Include for all C sources */
#include "global.h"

/** Global variables for this module: only those that need initialization in their definition. */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Compute predicate or function weight)  OCJ
 *
 *    This function computes and returns the weight of a predicate
 *    or function.
 *
 *  ARGUMENTS:
 *
 *    item: Pointer to predicate or function.
 *
 *  RETURNS:
 *
 *    Weight of predicate or function.
 *
 *--------------------------------------------------------------*/
static inline int32_t Weight(uint8_t *item){
	if (kbset.opts.fweight==ARITY){
		return(1+10*(((symbol *)&item[1])->symbol->arity));
	}
	return(1);
} /* Weight */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Increase variable balance)  OCJ
 *
 *    This function increases the variable balance in wbdata
 *    global variable for a given variable. It also updates
 *    the poscnt and negcnt fields of wbdata.
 *
 *  ARGUMENTS:
 *
 *    varnum: Variable number.
 *
 *  RETURNS:
 *
 *    Weight of predicate or function.
 *
 *--------------------------------------------------------------*/
static inline void Incvar(int32_t varnum){
	wbdata.vb[varnum]++;
	if (wbdata.vb[varnum]==0){
		wbdata.negcnt--;
	} else if (wbdata.vb[varnum]==1){
		wbdata.poscnt++;
	}
	return;
} /* Incvar */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Decrease variable balance)  OCJ
 *
 *    This function decreases the variable balance in wbdata
 *    global variable for a given variable. It also updates
 *    the poscnt and negcnt fields of wbdata.
 *
 *  ARGUMENTS:
 *
 *    varnum: Variable number.
 *
 *  RETURNS:
 *
 *    Weight of predicate or function.
 *
 *--------------------------------------------------------------*/
static inline void Decvar(int32_t varnum){
	wbdata.vb[varnum]--;
	if (wbdata.vb[varnum]==0){
		wbdata.poscnt--;
	} else if (wbdata.vb[varnum]==-1){
		wbdata.negcnt++;
	}
	return;
} /* Decvar */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Literals comparison)  OCJ
 *
 *    This function performs a literals comparison. This comparison
 *    is based partially in a term ordering which in turn may be
 *    the standard or the non recursive Knuth-Bendix ordering,
 *    depending on the value of flag.
 *
 *    The comparison is performed as indicated in the section 3.1.1
 *    of document "Implementing an efficient theorem prover"
 *    by Alexandre Riazanov. The return conditions of this
 *    function are:
 *
 *    GREATER if literal1 > literal2
 *    LESSER if literal2 > literal1
 *    EQUAL if literal2 = literal1
 *    FAILURE otherwise
 *
 *    See also "The design and implementation of VAMPIRE" section 2.6 by
 *    Alexandre Riazanov and Andrei Voronkov for details about handling
 *    positive and negative literal comparison logic.
 *
 *  ARGUMENTS:
 *
 *    literal1: Pointer to first literal to be compared.
 *    literal2: Pointer to second literal to be compared.
 *
 *  RETURNS:
 *
 *    GREATER, LESSER, EQUAL, FAILURE or NOMEMORY (see above).
 *
 *--------------------------------------------------------------*/
int32_t CompareLiterals(uint8_t *literal1,uint8_t *literal2){

	/* First literal is an equality. */
	if (EQUALITY&literal1[0]){

		/* Second literal is an equality. */
		if (EQUALITY&literal2[0]){
			return(CompareEqualities(literal1,literal2));
		}

		/* Second literal is not an equality. */
		return(LESSER);

	/* First literal is not an equality, second literal is an equality. */
	} else if (EQUALITY&literal2[0]){
		return(GREATER);
	}

	/* None of the literals is an equality. */
	switch(CompareItems(literal1,literal2,-2)){
		case GREATER:
			return(GREATER);
			break;
		case LESSER:
			return(LESSER);
			break;
		case EQUAL:
			if ((MAXIMAL|SINGLENEG|MULTINEG)&kbset.opts.select){
				if ((NEGATED&literal1[0])&&(0==(NEGATED&literal2[0]))){
					return(GREATER);
				} else if ((NEGATED&literal2[0])&&(0==(NEGATED&literal1[0]))){
					return(LESSER);
				}
			} else {
				if ((NEGATED&literal1[0])&&(0==(NEGATED&literal2[0]))){
					return(LESSER);
				} else if ((NEGATED&literal2[0])&&(0==(NEGATED&literal1[0]))){
					return(GREATER);
				}
			}
			return(EQUAL);
			break;
		case FAILURE:
			return(FAILURE);
			break;
		case NOMEMORY:
			return(NOMEMORY);
			break;
	}
	return(FAILURE);
} /* CompareLiterals */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Equality literals comparison)  OCJ
 *
 *    This function performs an equality literals comparison. This
 *    comparison is performed as indicated in the section 3.1.1
 *    of document "Implementing an efficient theorem prover"
 *    by Alexandre Riazanov and is based in the multiset ordering
 *    described in the "Multisets" section of page 26 of the above
 *    document.
 *
 *    Specifically, equalities are compared as multisets, being
 *    {s,t} the multiset assigned to positive equality s=t and
 *    {s,s,t,t} the multiset assigned to negative equality s!=t.
 *    Let |> be the multiset ordering and :> the term reduction
 *    ordering as described in section 3.1.5 of the above document.
 *    Let S1 and S2 be two multisets. Let S1(x) the multiset integer
 *    assigned to element x in multiset S1 and S2(x) the integer
 *    assigned to element x in multiset S2.
 *
 *    Then S1 |> S2 if and only if for all x such that S1(x) < S2(x)
 *    there exists y such that y :> x and S1(y) > S2(y).
 *
 *    The return conditions are:
 *
 *    GREATER if literal1 > literal2
 *    LESSER if literal2 > literal1
 *    EQUAL if literal2 = literal1
 *    FAILURE otherwise
 *
 *    Symmetry axiom is considered for comparisons so two literals are
 *    equal if they are identical or one is identical to the symmetric
 *    of the other.
 *
 *
 *  ARGUMENTS:
 *
 *    literal1: Pointer to first equality literal to be compared.
 *    literal2: Pointer to second equality literal to be compared.
 *
 *  RETURNS:
 *
 *    GREATER, LESSER, EQUAL, FAILURE or NOMEMORY (see above).
 *
 *--------------------------------------------------------------*/
int32_t CompareEqualities(uint8_t *literal1,uint8_t *literal2){
	auto uint8_t *eqtrm11,*eqtrm21;                 /* Pointers to literal1 equality root terms */
	auto uint8_t *eqtrm12,*eqtrm22;                 /* Pointers to literal2 equality root terms */
	auto int32_t o11,o12,o21,o22;                   /* Results from comparison */
	auto int32_t jj;                                /* Auxiliary. */


	/* Both equalities are positive or both are negative. */
	if ((literal1[0]&NEGATED)==(literal2[0]&NEGATED)){

		/* Get pointers to equality root terms. */
		eqtrm11=NextItem(literal1,IMMED);
		eqtrm21=NextItem(eqtrm11,OVERSUBTERMS);
		eqtrm12=NextItem(literal2,IMMED);
		eqtrm22=NextItem(eqtrm12,OVERSUBTERMS);

		/* Case EQUAL (two pairs of equality root terms equal). */
		if (NOMEMORY==(o11=CompareItems(eqtrm11,eqtrm12,-2))){
			return(NOMEMORY);
		}
		if (NOMEMORY==(o22=CompareItems(eqtrm21,eqtrm22,-2))){
			return(NOMEMORY);
		}
		if ((o11==EQUAL)&&(o22==EQUAL)){
			return(EQUAL);
		}
		if (NOMEMORY==(o12=CompareItems(eqtrm11,eqtrm22,-2))){
			return(NOMEMORY);
		}
		if (NOMEMORY==(o21=CompareItems(eqtrm21,eqtrm12,-2))){
			return(NOMEMORY);
		}
		if ((o12==EQUAL)&&(o21==EQUAL)){
			return(EQUAL);
		}

		/* Case of one pair of equality root terms equal. */
		if (o11==EQUAL){
			if (o22==GREATER){
				return(GREATER);
			} else if (o22==LESSER){
				return(LESSER);
			}
		} else if (o12==EQUAL){
			if (o21==GREATER){
				return(GREATER);
			} else if (o21==LESSER){
				return(LESSER);
			}
		} else if (o21==EQUAL){
			if (o12==GREATER){
				return(GREATER);
			} else if (o12==LESSER){
				return(LESSER);
			}
		} else if (o22==EQUAL){
			if (o11==GREATER){
				return(GREATER);
			} else if (o11==LESSER){
				return(LESSER);
			}
		}

		/* Case of no pairs of equality root terms equal. */
		if (((o11==GREATER)||(o21==GREATER))&&((o12==GREATER)||(o22==GREATER))){
			return(GREATER);
		} else if (((o11==LESSER)||(o12==LESSER))&&((o21==LESSER)||(o22==LESSER))){
			return(LESSER);
		}

	/* One equality is positive and the other is negative. */
	} else {

		/* Get pointers to equality root terms. The equality sub-index 1 is */
		/* assigned to the equality that produces the bigger multiset. */
		if (((literal1[0]&NEGATED)&&((MAXIMAL|SINGLENEG|MULTINEG)&kbset.opts.select))
				||((0==(literal1[0]&NEGATED))&&((SINGLEPOS|MULTIPOS)&kbset.opts.select))){
			eqtrm11=NextItem(literal1,IMMED);
			eqtrm21=NextItem(eqtrm11,OVERSUBTERMS);
			eqtrm12=NextItem(literal2,IMMED);
			eqtrm22=NextItem(eqtrm12,OVERSUBTERMS);
			jj=1;
		} else {
			eqtrm11=NextItem(literal2,IMMED);
			eqtrm21=NextItem(eqtrm11,OVERSUBTERMS);
			eqtrm12=NextItem(literal1,IMMED);
			eqtrm22=NextItem(eqtrm12,OVERSUBTERMS);
			jj=0;
		}

		/* Case of two pairs of equality root terms equal. */
		if (NOMEMORY==(o11=CompareItems(eqtrm11,eqtrm12,-2))){
			return(NOMEMORY);
		}
		if (NOMEMORY==(o22=CompareItems(eqtrm21,eqtrm22,-2))){
			return(NOMEMORY);
		}
		if ((o11==EQUAL)&&(o22==EQUAL)){
			if (jj){
				return(GREATER);
			} else {
				return(LESSER);
			}
		}
		if (NOMEMORY==(o12=CompareItems(eqtrm11,eqtrm22,-2))){
			return(NOMEMORY);
		}
		if (NOMEMORY==(o21=CompareItems(eqtrm21,eqtrm12,-2))){
			return(NOMEMORY);
		}
		if ((o12==EQUAL)&&(o21==EQUAL)){
			if (jj){
				return(GREATER);
			} else {
				return(LESSER);
			}
		}

		/* Case of one pair of equality root terms equal. */
		if (o11==EQUAL){
			if ((o22==GREATER)||(o12==GREATER)){
				if (jj){
					return(GREATER);
				} else {
					return(LESSER);
				}
			} else if ((o22==LESSER)&&(o12==LESSER)){
				if (jj){
					return(LESSER);
				} else {
					return(GREATER);
				}
			}
		} else if (o12==EQUAL){
			if ((o21==GREATER)||(o11==GREATER)){
				if (jj){
					return(GREATER);
				} else {
					return(LESSER);
				}
			} else if ((o21==LESSER)&&(o11==LESSER)){
				if (jj){
					return(LESSER);
				} else {
					return(GREATER);
				}
			}
		} else if (o21==EQUAL){
			if ((o12==GREATER)||(o22==GREATER)){
				if (jj){
					return(GREATER);
				} else {
					return(LESSER);
				}
			} else if ((o12==LESSER)&&(o22==LESSER)){
				if (jj){
					return(LESSER);
				} else {
					return(GREATER);
				}
			}
		} else if (o22==EQUAL){
			if ((o11==GREATER)||(o21==GREATER)){
				if (jj){
					return(GREATER);
				} else {
					return(LESSER);
				}
			} else if ((o11==LESSER)&&(o21==LESSER)){
				if (jj){
					return(LESSER);
				} else {
					return(GREATER);
				}
			}
		}

		/* Case of no pairs of equality root terms equal. */
		if (((o11==GREATER)||(o21==GREATER))&&((o12==GREATER)||(o22==GREATER))){
			if (jj){
				return(GREATER);
			} else {
				return(LESSER);
			}
		} else if (((o11==LESSER)||(o12==LESSER))&&((o21==LESSER)||(o22==LESSER))){
			if (jj){
				return(LESSER);
			} else {
				return(GREATER);
			}
		}
	}

	return(FAILURE);
} /* CompareEqualities */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Standard and non recursive KBO items comparison front end)  OCJ
 *
 *    This function initializes wbdata structure and calls the
 *    appropriate items comparison function depending on the
 *    current type of ordering.
 *
 *
 *  ARGUMENTS:
 *
 *    item1: Pointer to first item to be compared. It cannot be
 *           an equality. In some cases it is necessary that item1
 *           belongs a clause with maxvarnb field properly initialized,
 *           see maxvarnb description for details.
 *    item2: Pointer to first item to be compared. It cannot be
 *           an equality. In some cases it is necessary that item1
 *           belongs a clause with maxvarnb field properly initialized,
 *           see maxvarnb description for details.
 *    maxvarnb: Maximum variable number of both item1 and item2.
 *              If this is less than -1 then it is taken from
 *              the clauses of item1 and item2. In this case both
 *              item1 and item2 must belong to a clause with maxvarnb
 *              fields properly initialized and the greater value
 *              will be used.
 *
 *  RETURNS:
 *
 *    EQUAL, GREATER, LESSER, FAILURE or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t CompareItems(uint8_t *item1,uint8_t *item2,int32_t maxvarnb){
	auto int32_t *ptr1;                             /* Auxiliary pointer */
	auto int32_t ii;                                /* Auxiliary. */

	/* Compute maxvarnb real value if necessary. If any of the */
	/* items is a variable maxvarnb will not be used and will */
	/* be set to zero. */
	if (maxvarnb<-1){
		if ((VARIABLE&item1[0])||(VARIABLE&item2[0])){
			maxvarnb=0;
		} else {
			maxvarnb=((binprefix *)(item1-(binpsize+((symbol *)&item1[1])->offset)))->maxvarnb;
			ii=((binprefix *)(item2-(binpsize+((symbol *)&item2[1])->offset)))->maxvarnb;
			if (ii>maxvarnb){
				maxvarnb=ii;;
			}
		}
	}

	/* Check space of wb buffer field of wbdata global variable. */
	if (maxvarnb>=wbdata.size){
		ii=maxvarnb+1+VARBALANCE_CHUNK_SIZE;
		if (NULL==(ptr1=MYREALLOC(wbdata.vb,ii*sizeof(*wbdata.vb)))){
			return(NOMEMORY);
		}
		wbdata.vb=ptr1;
		wbdata.size=ii;
	}

	/* Initialize wbdata fields. */
	memset(wbdata.vb,0,(maxvarnb+1)*sizeof(*wbdata.vb));
	wbdata.poscnt=wbdata.negcnt=wbdata.weight=0;

	/* Call appropriate ordering function. */
	if (PREDICATE&item1[0]){
		if (kbset.opts.litord==STANDARD){
			return(KBOCompare(item1,item2));
		}
		return(NROCompare(item1,item2));
	}
	if (kbset.opts.termord==STANDARD){
		return(KBOCompare(item1,item2));
	}
	return(NROCompare(item1,item2));
} /* CompareItems */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Standard KBO comparison)  OCJ
 *
 *    This function performs a standard Knuth-Bendix ordering comparison
 *    of terms and predicates. Equalities are not allowed.
 *
 *    The comparison is performed as described in document "Things to
 *    Know When Implementing KBO" by Bernd Löchner. This algorithm
 *    is linear instead of quadratic. The trick to achieve this is
 *    to traverse the compared terms only once and perform both
 *    weight and variable balance calculations together with the
 *    lexicographic comparison, avoiding repeated weight and variable
 *    balance calculations. This is possible because when the time
 *    to perform the critical lexicographic comparison arrives
 *    all the previous traversed terms were identical so the weight
 *    and variable balance is zero. Therefore the new calculations
 *    for the lexicographic comparison can be used for the global
 *    KBO comparison and need not to be repeated. A few additional
 *    optimization tricks are included, as explained in the paper
 *    mentioned above.
 *
 *    The return conditions of this function are:
 *
 *    GREATER if item1 is greater than item2
 *    LESSER if item1 is lesser than item2
 *    EQUAL if item1 is equal to item2
 *    FAILURE otherwise
 *
 *
 *  ARGUMENTS:
 *
 *    item1: Pointer to first item to be compared.
 *    item2: Pointer to second item to be compared.
 *
 *  RETURNS:
 *
 *    EQUAL, GREATER, LESSER, FAILURE or NOMEMORY (see above).
 *
 *--------------------------------------------------------------*/
int32_t KBOCompare(uint8_t *item1,uint8_t *item2){
	auto int32_t lex;                               /* Result of lexicographical comparison */
	auto int32_t GorF;                              /* GREATER or FAILURE depending on negcnt field of wbdata */
	auto int32_t LorF;                              /* LESSER or FAILURE depending on poscnt field of wbdata */
	auto int32_t ii,jj;                             /* Auxiliary */

	/* First test for identical unary symbols. In this case */
	/* f(s)>f(t) if and only if s>t so it is enough to compare */
	/* s and t. */
	/* Loop until the condition above is not met. */
	while (1){
		if ((0==(VARIABLE&item1[0]))&&(0==(VARIABLE&item2[0]))
				&&(((symbol *)&item1[1])->symbol==((symbol *)&item2[1])->symbol)
				&&(((symbol *)&item1[1])->symbol->arity==1)){
			item1=NextItem(item1,IMMED);
			item2=NextItem(item2,IMMED);
		} else {
			break;
		}
	}

	/* Item 1 is a variable. */
	if (VARIABLE&item1[0]){
		ii=*((int16_t *)&item1[1]); /* Get item1 variable number. */

		/* Item 2 is a variable.  If both items are the same variable then */
		/* return EQUAL, else adjust variable count and return FAILURE. */
		if (VARIABLE&item2[0]){
			jj=*((int16_t *)&item2[1]); /* Get item2 variable number. */
			if (ii==jj){
				return(EQUAL);
			} else {
				Incvar(ii);
				Decvar(jj);
				return(FAILURE);
			}
		}

		/* Item 2 is not a variable. Adjust weight and variable balance */
		/* and return LESSER if variable item1 is not in item2 or FAILURE */
		/* otherwise. */
		jj=UpdateVWB1(item2,ii,0);
		Incvar(ii);
		wbdata.weight++;
		if (jj){
			return(LESSER);
		}
		return(FAILURE);
	}

	/* Item 1 is not a variable and item2 is a variable. Adjust weight */
	/* and variable balance and return GREATER if variable item2 is not */
	/* in item1 or FAILURE otherwise. */
	if (VARIABLE&item2[0]){
		ii=*((int16_t *)&item2[1]); /* Get item2 variable number. */
		jj=UpdateVWB1(item1,ii,1);
		Decvar(ii);
		wbdata.weight--;
		if (jj){
			return(GREATER);
		}
		return(FAILURE);
	}

	/* Neither item1 nor item2 are variables. Perform and store */
	/* lexicographical comparison of items and adjust weight */
	/* and variable balance. */
	if (((symbol *)&item1[1])->symbol==((symbol *)&item2[1])->symbol){
		lex=KBOLex(item1,item2);
	} else {
		UpdateVWB2(NextItem(item1,IMMED),NextItem(item1,OVERSUBTERMS),1);
		UpdateVWB2(NextItem(item2,IMMED),NextItem(item2,OVERSUBTERMS),0);
		lex=FAILURE;
	}
	wbdata.weight+=(Weight(item1)-Weight(item2));

	/* Update GorF and LorF according to positive and negative */
	/* variable balance counts in wbdata. */
	if (wbdata.negcnt==0){
		GorF=GREATER;
	} else {
		GorF=FAILURE;
	}
	if (wbdata.poscnt==0){
		LorF=LESSER;
	} else {
		LorF=FAILURE;
	}

	/* Return cases depending on the results obtained. */
	if (wbdata.weight>0){
		return(GorF);
	} else if (wbdata.weight<0){
		return(LorF);
	} else if (((symbol *)&item1[1])->symbol->precedence>((symbol *)&item2[1])->symbol->precedence){
		return(GorF);
	} else if (((symbol *)&item2[1])->symbol->precedence>((symbol *)&item1[1])->symbol->precedence){
		return(LorF);
	}
	switch (lex){
		case GREATER:
			return(GorF);
			break;
		case LESSER:
			return(LorF);
			break;
	}
	return(lex);
} /* KBOCompare */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Non recursive version of KBO comparison)  OCJ
 *
 *    This function performs a non recursive version of Knuth-Bendix
 *    ordering comparison of terms and predicates. Equalities are not
 *    allowed.
 *
 *    This is non recursive version of KBO as described in the
 *    definition 3.15 of document "Implementing an efficient theorem
 *    prover" by Alexandre Riazanov but with the applicable optimizations
 *    described in document "Things to Know When Implementing KBO"
 *    by Bernd Löchner. See KBOCompare() global functions for additional
 *    information.
 *
 *    The non lexicographic part and other portions of this function
 *    are identical to those of KBOCompare() function. In fact both
 *    functions can be merged with a simple modifications of the
 *    block titled as "Neither item1 nor item2 are variables...".
 *    However by the moment this is not done and two functions are
 *    defined.
 *
 *    The return conditions of this function are:
 *
 *    GREATER if item1 is greater than item2
 *    LESSER if item1 is lesser than item2
 *    EQUAL if item1 is equal to item2
 *    FAILURE otherwise
 *
 *
 *  ARGUMENTS:
 *
 *    item1: Pointer to first item to be compared.
 *    item2: Pointer to second item to be compared.
 *
 *  RETURNS:
 *
 *    EQUAL, GREATER, LESSER, FAILURE or NOMEMORY (see above).
 *
 *--------------------------------------------------------------*/
int32_t NROCompare(uint8_t *item1,uint8_t *item2){
	auto int32_t lex;                               /* Result of lexicographical comparison */
	auto int32_t GorF;                              /* GREATER or FAILURE depending on negcnt field of wbdata */
	auto int32_t LorF;                              /* LESSER or FAILURE depending on poscnt field of wbdata */
	auto int32_t ii,jj;                             /* Auxiliary */

	/* First test for identical unary symbols. In this case */
	/* f(s)>f(t) if and only if s>t. In this case it is enough */
	/* to compare s and t. */
	/* Loop until the condition above is not met. */
	while (1){
		if ((0==(VARIABLE&item1[0]))&&(0==(VARIABLE&item2[0]))
				&&(((symbol *)&item1[1])->symbol==((symbol *)&item2[1])->symbol)
				&&(((symbol *)&item1[1])->symbol->arity==1)){
			item1=NextItem(item1,IMMED);
			item2=NextItem(item2,IMMED);
		} else {
			break;
		}
	}

	/* Item 1 is a variable. */
	if (VARIABLE&item1[0]){
		ii=*((int16_t *)&item1[1]); /* Get item1 variable number. */

		/* Item 2 is a variable.  If both items are the same variable then */
		/* return EQUAL, else adjust variable count and return FAILURE. */
		if (VARIABLE&item2[0]){
			jj=*((int16_t *)&item2[1]); /* Get item2 variable number. */
			if (ii==jj){
				return(EQUAL);
			} else {
				Incvar(ii);
				Decvar(jj);
				return(FAILURE);
			}
		}

		/* Item 2 is not a variable. Adjust weight and variable balance */
		/* and return LESSER if variable item1 is not in item2 or FAILURE */
		/* otherwise. */
		jj=UpdateVWB1(item2,ii,0);
		Incvar(ii);
		wbdata.weight++;
		if (jj){
			return(LESSER);
		}
		return(FAILURE);
	}

	/* Item 1 is not a variable and item2 is a variable. Adjust weight */
	/* and variable balance and return GREATER if variable item2 is not */
	/* in item1 or FAILURE otherwise. */
	if (VARIABLE&item2[0]){
		ii=*((int16_t *)&item2[1]); /* Get item2 variable number. */
		jj=UpdateVWB1(item1,ii,1);
		Decvar(ii);
		wbdata.weight--;
		if (jj){
			return(GREATER);
		}
		return(FAILURE);
	}

	/* Neither item1 nor item2 are variables. Perform and store */
	/* lexicographical comparison of items. */
	lex=NROLex(item1,item2);

	/* Update GorF and LorF according to positive and negative */
	/* variable balance counts in wbdata. */
	if (wbdata.negcnt==0){
		GorF=GREATER;
	} else {
		GorF=FAILURE;
	}
	if (wbdata.poscnt==0){
		LorF=LESSER;
	} else {
		LorF=FAILURE;
	}

	/* Return cases depending on the results obtained. */
	if (wbdata.weight>0){
		return(GorF);
	} else if (wbdata.weight<0){
		return(LorF);
	} else if (((symbol *)&item1[1])->symbol->precedence>((symbol *)&item2[1])->symbol->precedence){
		return(GorF);
	} else if (((symbol *)&item2[1])->symbol->precedence>((symbol *)&item1[1])->symbol->precedence){
		return(LorF);
	}
	switch (lex){
		case GREATER:
			return(GorF);
			break;
		case LESSER:
			return(LorF);
			break;
	}
	return(lex);
} /* NROCompare */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (KBO lexicographic comparison)  OCJ
 *
 *    This function performs the lexicographic comparison part of
 *    Knuth-Bendix ordering of the sub-terms of the two items passed
 *    as arguments. These items may be terms or predicates. Equalities
 *    are not allowed.
 *
 *    This is described in document "Things to Know When Implementing KBO"
 *    by Bernd Löchner. See KBOCompare() global functions for additional
 *    information.
 *
 *
 *  ARGUMENTS:
 *
 *    item1: Pointer to first item whose sub-terms will be lexicographically
 *           compared. This must be of the form f(...). It cannot be a variable.
 *    item2: Pointer to second item whose sub-terms will be lexicographically
 *           compared. This must also be of the form f(...), that is, the
 *           initial function or predicate of both item1 and item2 must be the
 *           same.
 *
 *  RETURNS:
 *
 *    EQUAL, GREATER, LESSER, FAILURE or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t KBOLex(uint8_t *item1,uint8_t *item2){
	auto int32_t lex;                               /* Result of lexicographical comparison */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4;           /* Auxiliary pointers */

	/* Make a recursive call to KBOCompare() for every pair of subterms. */
	/* Loop through sub-terms. */
	for (ptr1=NextItem(item1,IMMED),ptr2=NextItem(item1,OVERSUBTERMS),ptr3=NextItem(item2,IMMED),
			ptr4=NextItem(item2,OVERSUBTERMS),lex=EQUAL;
			ptr1<ptr2;ptr1=NextItem(ptr1,OVERSUBTERMS),ptr3=NextItem(ptr3,OVERSUBTERMS)){
		if (EQUAL!=(lex=KBOCompare(ptr1,ptr3))){
			UpdateVWB2(NextItem(ptr1,OVERSUBTERMS),ptr2,1);
			UpdateVWB2(NextItem(ptr3,OVERSUBTERMS),ptr4,0);
			break;
		}
	}
	return(lex);
} /* KBOLex */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Non recursive KBO lexicographic comparison control function)  OCJ
 *
 *    This function controls the lexicographic comparison part of the
 *    non recursive version of Knuth-Bendix ordering of the two items
 *    items passed as arguments. These items may be terms or predicates.
 *    Equalities are not allowed.
 *
 *    This non recursive version of KBO is described in the definition
 *    3.15 of document "Implementing an efficient theorem prover"
 *    by Alexandre Riazanov
 *
 *
 *  ARGUMENTS:
 *
 *    item1: Pointer to first item. It cannot be a variable. It must be
 *           of the form f(...).
 *    item2: Pointer to second item. It cannot be a variable. It must be
 *           of the form g(...).
 *
 *  RETURNS:
 *
 *    EQUAL, GREATER, LESSER, FAILURE or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t NROLex(uint8_t *item1,uint8_t *item2){

	/* Both item1 and item2 are of the form f(...). */
	if (((symbol *)&item1[1])->symbol==((symbol *)&item2[1])->symbol){
		return(NROLex2(item1,item2));
	}

	/* Item 1 is of the form f(...) and item2 is of the form g(...) with f!=g. */
	/* Compute weight and variable balance of remaining list of terms and */
	/* return the lexicographical comparison according to the items precedence. */
	UpdateVWB2(NextItem(item1,IMMED),NextItem(item1,OVERSUBTERMS),1);
	UpdateVWB2(NextItem(item2,IMMED),NextItem(item2,OVERSUBTERMS),0);
	wbdata.weight+=(Weight(item1)-Weight(item2));
	if (((symbol *)&item1[1])->symbol->precedence>((symbol *)&item2[1])->symbol->precedence){
		return(GREATER);
	}
	return(LESSER);
} /* NROLex */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Non recursive KBO lexicographic comparison perform function)  OCJ
 *
 *    This function performs the lexicographic comparison part of the
 *    non recursive version of Knuth-Bendix ordering of the sub-terms
 *    of the two items passed as arguments. These items may be terms
 *    or predicates. Equalities are not allowed.
 *
 *    This non recursive version of KBO is described in the definition
 *    3.15 of document "Implementing an efficient theorem prover"
 *    by Alexandre Riazanov
 *
 *
 *  ARGUMENTS:
 *
 *    item1: Pointer to first item whose sub-terms will be lexicographically
 *           compared. This must be of the form f(...). It cannot be a variable.
 *    item2: Pointer to second item whose sub-terms will be lexicographically
 *           compared. This must also be of the form f(...), that is, the
 *           initial function or predicate of both item1 and item2 must be the
 *           same.
 *
 *  RETURNS:
 *
 *    EQUAL, GREATER, LESSER, FAILURE or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t NROLex2(uint8_t *item1,uint8_t *item2){
	auto int32_t lex;                               /* Result of lexicographical comparison */
	auto uint8_t *ptr1,*ptr2,*ptr3;                 /* Auxiliary pointers */
	auto int32_t ii,jj,kk;                          /* Auxiliary */

	/* First test for identical unary symbols. In this case */
	/* f(s)>f(t) if and only if s>t. In this case it is enough */
	/* to compare s and t. */
	/* Loop until the condition above is not met. */
	while (1){
		if (((symbol *)&item1[1])->symbol->arity==1){
			ptr1=NextItem(item1,IMMED);
			ptr2=NextItem(item2,IMMED);
			if ((0==(VARIABLE&ptr1[0]))&&(0==(VARIABLE&ptr2[0]))
					&&(((symbol *)&ptr1[1])->symbol==((symbol *)&ptr2[1])->symbol)){
				item1=ptr1;
				item2=ptr2;
			} else {
				break;
			}
		} else {
			break;
		}
	}

	/* Loop through the arguments of item1 and item2 until */
	/* a definite comparison is available. */
	for (ptr1=NextItem(item1,IMMED),ptr2=NextItem(item1,OVERSUBTERMS),ptr3=NextItem(item2,IMMED),kk=0,lex=EQUAL;
			(ptr1<ptr2)&&(kk==0);ptr1=NextItem(ptr1,OVERSUBTERMS),ptr3=NextItem(ptr3,OVERSUBTERMS)){

		/* There is no lexicographical comparison result yet. */
		if (kk==0){

			/* The term ptr1 is a variable. */
			if (VARIABLE&ptr1[0]){
				ii=*((int16_t *)&ptr1[1]); /* Get ptr1 variable number. */

				/* The term ptr3 is a variable. */
				if (VARIABLE&ptr3[0]){
					jj=*((int16_t *)&ptr3[1]); /* Get ptr3 variable number. */
					if (ii!=jj){
						Incvar(ii);
						Decvar(jj);
						lex=FAILURE;
						kk=1;
					}
					continue;
				}

				/* The term ptr3 is not a variable. */
				UpdateVWB3(ptr3,0);
		        Incvar(ii);
		        wbdata.weight++;
		        lex=FAILURE;
		        kk=1;
		        continue;
			}

			/* The term ptr1 is not a variable but ptr3 is a variable. */
			if (VARIABLE&ptr3[0]){
				ii=*((int16_t *)&ptr3[1]); /* Get ptr3 variable number. */
				UpdateVWB3(ptr1,1);
		        Decvar(ii);
		        wbdata.weight--;
		        lex=FAILURE;
		        kk=1;
		        continue;
			}

			/* Neither ptr1 nor ptr3 are variables. */
			ii=NROLex(ptr1,ptr3);
			if (ii!=EQUAL){
				lex=ii;
				kk=1;
			}
		}
	}

	/* Compute weight and variable balance contribution of any remaining */
	/* terms in the list of arguments and return. */
	if (ptr1<ptr2){
		UpdateVWB2(ptr1,ptr2,1);
		UpdateVWB2(ptr3,NextItem(item2,OVERSUBTERMS),0);
	}
	return(lex);
} /* NROLex2 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Update weight and variable balance of a term and
 *                sub terms and check if a variable is in them)  OCJ
 *
 *    This function updates the weights and variable balances of an
 *    item and its sub-terms. It also checks if a given variable
 *    is in the term o its sub-terms.
 *
 *
 *  ARGUMENTS:
 *
 *    item: Pointer to first item. It must be a variable, predicate
 *          or function.
 *    varnum: Number of variable that will be searched in the item.
 *    flag: 1 if weight and variable balances must be increased.
 *          0 if weight and variable balances must be decreased.
 *
 *  RETURNS:
 *
 *    1 if variable is in the term or its sub terms, 0 otherwise.
 *
 *--------------------------------------------------------------*/
int32_t UpdateVWB1(uint8_t *item,int32_t varnum,int32_t flag){
	auto uint8_t *ptr1,*ptr2;                       /* Auxiliary pointers */
	auto int32_t ii,kk;                             /* Auxiliary */

	/* Loop through the item and sub-terms. */
	for (ptr1=item,ptr2=NextItem(item,OVERSUBTERMS),kk=0;ptr1<ptr2;ptr1=NextItem(ptr1,IMMED)){

		/* The item ptr1 is a variable. */
		if (VARIABLE&ptr1[0]){
			ii=*((int16_t *)&ptr1[1]); /* Get ptr1 variable number. */
			if (flag){
				Incvar(ii);
				wbdata.weight++;
			} else {
				Decvar(ii);
				wbdata.weight--;
			}
			if (ii==varnum){
				kk=1;
			}

		/* The item ptr1 is not a variable. */
		} else {
			if (flag){
				wbdata.weight+=Weight(ptr1);
			} else {
				wbdata.weight-=Weight(ptr1);
			}
		}
	}
	return(kk);
} /* UpdateVWB1 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Update weight and variable balance of a list of terms)  OCJ
 *
 *    This function updates the weights and variable balances of a
 *    list of terms their sub-terms.
 *
 *
 *  ARGUMENTS:
 *
 *    term1: Pointer to first term in the list. It must be a variable
 *           or a function.
 *    listend: Pointer to end of list (first item not in the list).
 *    flag: 1 if weight and variable balances must be increased.
 *          0 if weight and variable balances must be decreased.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void UpdateVWB2(uint8_t *term1,uint8_t *listend,int32_t flag){
	auto uint8_t *ptr1;                             /* Auxiliary pointer */
	auto int32_t ii;                                /* Auxiliary */

	/* Loop through the list terms and sub-terms. */
	for (ptr1=term1;ptr1<listend;ptr1=NextItem(ptr1,IMMED)){

		/* The item ptr1 is a variable. */
		if (VARIABLE&ptr1[0]){
			ii=*((int16_t *)&ptr1[1]); /* Get ptr1 variable number. */
			if (flag){
				Incvar(ii);
				wbdata.weight++;
			} else {
				Decvar(ii);
				wbdata.weight--;
			}

		/* The item ptr1 is not a variable. */
		} else {
			if (flag){
				wbdata.weight+=Weight(ptr1);
			} else {
				wbdata.weight-=Weight(ptr1);
			}
		}
	}
	return;
} /* UpdateVWB2 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Update weight and variable balance of a term)  OCJ
 *
 *    This function updates the weights and variable balances of an
 *    item and its sub-terms.
 *
 *
 *  ARGUMENTS:
 *
 *    item: Pointer to item. It must be a variable, predicate
 *          or function.
 *    flag: 1 if weight and variable balances must be increased.
 *          0 if weight and variable balances must be decreased.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void UpdateVWB3(uint8_t *item,int32_t flag){
	auto uint8_t *ptr1,*ptr2;                       /* Auxiliary pointers */
	auto int32_t ii;                                /* Auxiliary */

	/* Loop through the item and sub-terms. */
	for (ptr1=item,ptr2=NextItem(item,OVERSUBTERMS);ptr1<ptr2;ptr1=NextItem(ptr1,IMMED)){

		/* The item ptr1 is a variable. */
		if (VARIABLE&ptr1[0]){
			ii=*((int16_t *)&ptr1[1]); /* Get ptr1 variable number. */
			if (flag){
				Incvar(ii);
				wbdata.weight++;
			} else {
				Decvar(ii);
				wbdata.weight--;
			}

		/* The item ptr1 is not a variable. */
		} else {
			if (flag){
				wbdata.weight+=Weight(ptr1);
			} else {
				wbdata.weight-=Weight(ptr1);
			}
		}
	}
	return;
} /* UpdateVWB3 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Equality root terms comparison)  OCJ
 *
 *    This function performs a standard or non recursive Knuth-Bendix
 *    comparison of equality root terms.
 *
 *
 *  ARGUMENTS:
 *
 *    binp: Pointer to binprefix structure of clause to which the
 *          terms belong.
 *    term1: Pointer to first term to be compared.
 *    term2: Pointer to second term to be compared.
 *
 *  RETURNS:
 *
 *    EQUAL, GREATER, LESSER, FAILURE or NOMEMORY (see above).
 *
 *--------------------------------------------------------------*/
int32_t CompareEquTerms(binprefix *binp,uint8_t *term1,uint8_t *term2){
	auto uint8_t *equality;                         /* Pointer to root terms equality */

	/* SELECTED equalities are oriented in this clause. */
	if (binp->oriented){

		/* Get pointer to equality to which root terms belong. */
		if (term1<term2){
			equality=term1-1-sizeof(symbol);
		} else {
			equality=term2-1-sizeof(symbol);
		}

		/* If equality is SELECTED then we already have a result. */
		if (SELECTED&equality[0]){
			if (term1[0]&MAXIMUM){
				return(GREATER);
			}
			if (term2[0]&MAXIMUM){
				return(LESSER);
			}
			if ((term1[0]&SELECTED)&&(term1[0]&SELECTED)){
				return(FAILURE);
			}
			return(EQUAL);
		}
	}

	/* Compare root terms and return. */
	return(CompareItems(term1,term2,binp->maxvarnb));
} /* CompareEquTerms */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Terms comparison specific for connectedness check)  OCJ
 *
 *    This function performs a KBO comparison between two terms that
 *    contain only variables that exist in a critical pair whose
 *    connectedness is being checked. The comparison is performed
 *    with the following assumptions:
 *    - The allowed variables are x₁, x₂, ..., xₚ and they will be
 *      substituted by the ground terms a₁, a₂, ... aₚ.
 *    - All a₁, a₂, ... aₚ have weight=1 and their precedence value
 *      is higher than any pre-existing symbol.
 *    - If flag is zero then a₁≺a₂≺ ... ≺aₚ. If flag is not zero
 *      then a₁≻a₂≻ ... ≻aₚ.
 *    - This comparison is done specifically to ensure termination
 *      when checking for connectedness. This is done when ordering
 *      the equality root terms of a critical pair and when comparing
 *      the term to be rewritten with the term resulting from the
 *      rewrite. In the last case the term resulting from the rewrite
 *      may have variables that are not in the original critical pair
 *      equality and in this case the required ordering check doesn't
 *      hold. This is detected because the variable number is higher
 *      than the maxvarnb parameter. In this case UNKNOWN is returned
 *      to save time because it suffices to indicate that the required
 *      ordering doesn't hold. Also to save time it is required that
 *      in this comparison the term resulting from the rewrite is
 *      placed in term2.
 *
 *    This function is called recursively.
 *
 *
 *  ARGUMENTS:
 *
 *    term1: Pointer to first term to be compared.
 *    term2: Pointer to second term to be compared. If the term resulting
 *           from the rewrite of the connectedness checking process is
 *           one of the terms of the comparison then it must be placed
 *           in this term2 parameter (see comments above).
 *    maxvarnb:  Maximum variable number in the original critical pair
 *               equality.
 *    flag: SUBSTITUTION2: If this flag is set then a₁≻a₂≻ ... ≻aₚ
 *          ground substitution will be used for variables when
 *          checking ordering constraint to ensure rewrite termination.
 *          If this flag is not set then a₁≺a₂≺ ... ≺aₚ ground
 *          substitution is used. See comments above.
 *
 *  RETURNS:
 *
 *    EQUAL if term1≡term2, GREATER if term1≻term2, LESSER if term1≺term2
 *    or FAILURE if term2 has a variable that is not in the original
 *    critical pair equality.
 *
 *--------------------------------------------------------------*/
int32_t ConnectCompare(uint8_t *term1,uint8_t *term2,int32_t maxvarnb,int32_t flag){
	auto int32_t weight1,weight2;                   /* Term weights */
	auto uint8_t *ptr1,*ptr2,*ptr3;                 /* Auxiliary pointers */
	auto int32_t ii,jj;                             /* Auxiliary. */

	/* Compute term weights. If a variable with a number higher than maxvarnb */
	/* exist in term2 then return FAIULURE. */
	for (weight1=0,ptr1=term1,ptr2=NextItem(term1,OVERSUBTERMS);ptr1<ptr2;ptr1=NextItem(ptr1,IMMED)){
		if (VARIABLE&ptr1[0]){
			weight1++;
		} else {
			if (kbset.opts.fweight==ARITY){
				weight1+=(1+10*(((symbol *)&ptr1[1])->symbol->arity));
			} else {
				weight1++;
			}
		}
	}
	for (weight2=0,ptr1=term2,ptr2=NextItem(term2,OVERSUBTERMS);ptr1<ptr2;ptr1=NextItem(ptr1,IMMED)){
		if (VARIABLE&ptr1[0]){
			if (*((int16_t *)&ptr1[1])>maxvarnb){
				return(FAILURE);
			}
			weight2++;
		} else {
			if (kbset.opts.fweight==ARITY){
				weight2+=(1+10*(((symbol *)&ptr1[1])->symbol->arity));
			} else {
				weight2++;
			}
		}
	}

	/* Perform term comparison. */
	/* Weights are different. */
	if (weight1>weight2){
		return(GREATER);
	} else if (weight1<weight2){
		return(LESSER);

	/* Weights are equal. */
	} else {

		/* One term is a function and the other term is a variable. */
		if (((VARIABLE|FUNCTION)&term1[0])!=((VARIABLE|FUNCTION)&term2[0])){
			if (VARIABLE&term1[0]){
				return(GREATER);
			}
			return(LESSER);

		/* Both terms are variables. */
		} else if (VARIABLE&term1[0]){
			ii=*((int16_t *)&term1[1]);
			jj=*((int16_t *)&term2[1]);
			if (flag&SUBSTITUTION2){
				if (ii<jj){
					return(GREATER);
				} else if (ii>jj){
					return(LESSER);
				}
			} else {
				if (ii>jj){
					return(GREATER);
				} else if (ii<jj){
					return(LESSER);
				}
			}
			return(EQUAL);

		/* Both terms are functions. */
		} else {

			/* Symbol precedence relation (>>) holds. */
			if (((symbol *)&term1[1])->symbol->precedence>((symbol *)&term2[1])->symbol->precedence){
				return(GREATER);
			}

			/* Symbol precedence relation (<<) holds. */
			if (((symbol *)&term1[1])->symbol->precedence<((symbol *)&term2[1])->symbol->precedence){
				return(LESSER);
			}

			/* Both symbols have the same precedence so they are the same. */
			/* Loop through the arguments of both items. */
			for (ptr1=NextItem(term1,IMMED),ptr2=NextItem(term2,IMMED),ptr3=NextItem(term1,OVERSUBTERMS);
					ptr1<ptr3;ptr1=NextItem(ptr1,OVERSUBTERMS),ptr2=NextItem(ptr2,OVERSUBTERMS)){
				if (EQUAL!=(ii=ConnectCompare(ptr1,ptr2,maxvarnb,flag))){
					return(ii);
				}
			}
		}
	}

	return(EQUAL);
} /* ConnectCompare */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check term ordering condition for ground joinability)  OCJ
 *
 *    This function performs a comparison between two terms whose
 *    variables must be grounded with a specific ordering relation
 *    between themselves. Let x₁, x₂, ..., xₚ be the variables
 *    in the input terms with a ground ordering relation such that
 *    x₁≻x₂≻...≻xₚ. If the corresponding weights are w₁, w₂, ..., wₚ,
 *    that is, |xₖ|=wₖ, then it must be w₁≥w₂≥...≥wₚ and in case that
 *    some equality applies to weight comparison then the corresponding
 *    grounded variables must satisfy the corresponding lexicographic
 *    ≻ condition.
 *
 *    The required condition that is checked is term1≻term2 and their
 *    variables are considered grounded and satisfying the above
 *    conditions.
 *
 *    This function is called recursively.
 *
 *
 *  ARGUMENTS:
 *
 *    varorder: Pointer to set with the order of variables in the ground
 *              ordering analyzed. If the variable number i is the xₖ
 *              variable (see comments above) then varorder[i]==k-1.
 *    auxbuf: Pointer to auxiliary buffer for ordering checking.
 *            This is allocated once in IsGrJoinable() function so that
 *            there is no need to allocate allocate this memory every
 *            time that this function is called.
 *    term1: Pointer to first term to be compared. This term must be
 *           a root term of an equality. All variables in this term
 *           term must be in the list of variables to be grounded, so this
 *           term must be either one of the root terms in the original
 *           equation to be checked for ground joinability or the
 *           unification root term of the rewriting equation used by
 *           GJRewrite() function.
 *    term2: Pointer to second term to be compared. This term must be
 *           the other root term of the equation to which term1 belongs.
 *           If the term resulting from the rewrite of the connectedness
 *           checking process is one of the terms of the comparison then
 *           it must be placed in this term2 parameter (see comments above).
 *    maxvarnb:  Maximum variable number in the original critical pair
 *               equality. These are the variables to be grounded.
 *
 *  RETURNS:
 *
 *    1 if term1≻term2, 0 otherwise.
 *
 *--------------------------------------------------------------*/
int32_t CheckGJOrdering1(int32_t *varorder,int32_t *auxbuf,uint8_t *term1,
		uint8_t *term2,int32_t maxvarnb){
	auto uint8_t *ptr1,*ptr2,*ptr3;                 /* Auxiliary pointers */
	auto int32_t wdiff;                             /* Weight of term1 minus weight of term2 */
	auto int32_t ii;                                /* Auxiliary. */

	/* If term2 has a variable that is not in the list of variables */
	/* to be grounded then ordering condition term1≻term2 doesn't hold. */
	for (ptr1=term2,ptr2=NextItem(term2,OVERSUBTERMS);ptr1<ptr2;ptr1=NextItem(ptr1,IMMED)){
		if ((VARIABLE&ptr1[0])&&(maxvarnb<(*((int16_t *)&ptr1[1])))){
			return(0);
		}
	}

	/* There are no variables to be grounded. Initialize the weight */
	/* difference of grounded variables to zero. */
	if (maxvarnb<0){
		wdiff=0;

	/* There are variables to be grounded. Initialize the weight */
	/* difference of grounded variables to the worst case, minimum weight. */
	/* Even with the same weight the grounded variables can maintain the */
	/* current ≽ ordering by precedence relation instead of weight. */
	} else {

		/* Build the differences in number of identical variables */
		/* in term1 and term2. */
		memset(auxbuf,0,(1+maxvarnb)*sizeof(auxbuf[0]));
		for (ptr1=term1,ptr2=NextItem(term1,OVERSUBTERMS);ptr1<ptr2;ptr1=NextItem(ptr1,IMMED)){
			if (VARIABLE&ptr1[0]){
				ii=*((int16_t *)&ptr1[1]);
				auxbuf[varorder[ii]]++;
			}
		}
		for (ptr1=term2,ptr2=NextItem(term2,OVERSUBTERMS);ptr1<ptr2;ptr1=NextItem(ptr1,IMMED)){
			if (VARIABLE&ptr1[0]){
				ii=*((int16_t *)&ptr1[1]);
				auxbuf[varorder[ii]]--;
			}
		}

		/* Compute the worst case of weight difference of grounded variables. */
		/* Take into account that the minimum weight of a term is 1. */
		/* If the worst case difference is negative then ordering condition */
		/* term1≻term2 doesn't hold. */
		for (wdiff=ii=0;ii<=maxvarnb;ii++){
			wdiff+=auxbuf[ii];
			if (wdiff<0){
				return(0);
			}
		}
	}

	/* Add the weight difference of terms that are not variables */
	/* to the weight difference of grounded variables. */
	for (ptr1=term1,ptr2=NextItem(term1,OVERSUBTERMS);ptr1<ptr2;ptr1=NextItem(ptr1,IMMED)){
		if (0==(VARIABLE&ptr1[0])){
			if (kbset.opts.fweight==ARITY){
				wdiff+=(1+10*(((symbol *)&ptr1[1])->symbol->arity));
			} else {
				wdiff++;
			}
		}
	}
	for (ptr1=term2,ptr2=NextItem(term2,OVERSUBTERMS);ptr1<ptr2;ptr1=NextItem(ptr1,IMMED)){
		if (0==(VARIABLE&ptr1[0])){
			if (kbset.opts.fweight==ARITY){
				wdiff-=(1+10*(((symbol *)&ptr1[1])->symbol->arity));
			} else {
				wdiff--;
			}
		}
	}

	/* If weight difference is positive then ordering condition term1≻term2 holds. */
	if (wdiff>0){
		return(1);

	/* Weight difference is zero. */
	} else if (wdiff==0){

		/* The item term1 is a variable. */
		if (VARIABLE&term1[0]){

			/* If item term2 is a variable then compare order and return. */
			/* Otherwise return not ground joinable because the term2 */
			/* must be an arity 0 and weight 1 term so the order is */
			/* decided by preference. As precedence of grounded variable */
			/* may take any value then it cannot be greater than term2 */
			/* in a general case. */
			if (VARIABLE&term2[0]){
				if (varorder[*((int16_t *)&term1[1])]<varorder[*((int16_t *)&term2[1])]){
					return(1);
				}
				return(0);
			}
			return(0);
		}

		/* The item term2 is a variable. As term1 is not a variable */
		/* then using the same argument as in the preceding comment */
		/* return terms not ground joinable. */
		if (VARIABLE&term2[0]){
			return(0);
		}

		/* Symbol precedence relation (>>) holds. */
		if (((symbol *)&term1[1])->symbol->precedence>((symbol *)&term2[1])->symbol->precedence){
			return(1);
		}

		/* Symbol precedence relation (<<) holds. */
		if (((symbol *)&term1[1])->symbol->precedence<((symbol *)&term2[1])->symbol->precedence){
			return(0);
		}

		/* Both symbols have the same precedence so they are the same. */
		/* Loop through the arguments of both items. */
		for (ptr1=NextItem(term1,IMMED),ptr2=NextItem(term2,IMMED),ptr3=NextItem(term1,OVERSUBTERMS);
				ptr1<ptr3;ptr1=NextItem(ptr1,OVERSUBTERMS),ptr2=NextItem(ptr2,OVERSUBTERMS)){
			if (0==IsEqual(ptr1,ptr2,0)){
				if (kbset.opts.termord==STANDARD){
					return(CheckGJOrdering1(varorder,auxbuf,ptr1,ptr2,maxvarnb));
				} else {
					if ((VARIABLE&ptr1[0])||(VARIABLE&ptr2[0])){
						return(0);
					}
					if (GREATER==SecondKBOCheck(ptr1,ptr2,NONRECURSIVE)){
						return(1);
					}
					return(0);
				}
			}
		}
	}


	/* Return ordering condition term1≻term2 doesn't hold. */
	return(0);
} /* CheckGJOrdering1 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Front end for CheckGJOrdering2() function)  OCJ
 *
 *    This function is the front end for CheckGJOrdering2() function.
 *    It performs the tasks to be done only in the first call to
 *    CheckGJOrdering2() function but not in the subsequent recursive
 *    calls to it.
 *
 *    See CheckGJOrdering2() function global comments for additional
 *    information.
 *
 *
 *  ARGUMENTS:
 *
 *    varorder: Pointer to set with the order of variables in the ground
 *              ordering analyzed. If the variable number i is the xₖ
 *              variable (see comments above) then varorder[i]==k-1.
 *    auxbuf: Pointer to auxiliary buffer for ordering checking.
 *            This is allocated once in IsGrJoinable() function so that
 *            there is no need to allocate allocate this memory every
 *            time that this function is called.
 *    eqset: Pointer to set of integers used for variable equalization
 *           control. This is allocated once in IsGrJoinable()
 *           function so that there is no need to allocate allocate
 *           this space every time that this function is called.
 *    term1: Pointer to first term to be compared. This term must be
 *           a root term of an equality. All variables in this term
 *           term must be in the list of variables to be grounded, so this
 *           term must be either one of the root terms in the original
 *           equation to be checked for ground joinability or the
 *           unification root term of the rewriting equation used by
 *           GJRewrite() function.
 *    term2: Pointer to second term to be compared. This term must be
 *           the other root term of the equation to which term1 belongs.
 *           If the term resulting from the rewrite of the connectedness
 *           checking process is one of the terms of the comparison then
 *           it must be placed in this term2 parameter (see comments above).
 *    maxvarnb:  Maximum variable number in the original critical pair
 *               equality. These are the variables to be grounded.
 *
 *  RETURNS:
 *
 *    1 if term1≻term2, 0 otherwise.
 *
 *--------------------------------------------------------------*/
int32_t CheckGJOrdering2FE(int32_t *varorder,int32_t *auxbuf,int32_t *eqset,
		uint8_t *term1,uint8_t *term2,int32_t maxvarnb){
	auto uint8_t *ptr1,*ptr2;                       /* Auxiliary pointers */

	/* If term2 has a variable that is not in the list of variables */
	/* to be grounded then ordering condition term1≻term2 doesn't hold. */
	for (ptr1=term2,ptr2=NextItem(term2,OVERSUBTERMS);ptr1<ptr2;ptr1=NextItem(ptr1,IMMED)){
		if ((VARIABLE&ptr1[0])&&(maxvarnb<(*((int16_t *)&ptr1[1])))){
			return(0);
		}
	}

	/* If there are variables to be grounded then initialize eqset set. */
	if (maxvarnb>=0){
		memset(eqset,0xff,(1+maxvarnb)*sizeof(*eqset));
	}

	/* Return result from CheckGJOrdering2() funtion. */
	return(CheckGJOrdering2(varorder,auxbuf,eqset,term1,term2,maxvarnb));
} /* CheckGJOrdering2FE */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check term ordering condition for ground joinability)  OCJ
 *
 *    This function performs a comparison between two terms whose
 *    variables must be grounded with a specific ordering relation
 *    between themselves. Let x₁, x₂, ..., xₚ be the variables
 *    in the input terms with a ground ordering relation such that
 *    x₁≽x₂≽...≽xₚ. If the corresponding weights are w₁, w₂, ..., wₚ,
 *    that is, |xₖ|=wₖ, then it must be w₁≥w₂≥...≥wₚ.
 *
 *    The required condition that is checked is term1≽term2 and their
 *    variables are considered grounded and satisfying the above
 *    conditions.
 *
 *    This function is called recursively.
 *
 *
 *  ARGUMENTS:
 *
 *    varorder: Pointer to set with the order of variables in the ground
 *              ordering analyzed. If the variable number i is the xₖ
 *              variable (see comments above) then varorder[i]==k-1.
 *    auxbuf: Pointer to auxiliary buffer for ordering checking.
 *            This is allocated once in IsGrJoinable() function so that
 *            there is no need to allocate allocate this memory every
 *            time that this function is called.
 *    eqset: Pointer to set of integers used for variable equalization
 *           control. This is allocated once in IsGrJoinable()
 *           function so that there is no need to allocate allocate
 *           this space every time that this function is called.
 *    term1: Pointer to first term to be compared. This term must be
 *           a root term of an equality. All variables in this term
 *           term must be in the list of variables to be grounded, so this
 *           term must be either one of the root terms in the original
 *           equation to be checked for ground joinability or the
 *           unification root term of the rewriting equation used by
 *           GJRewrite() function.
 *    term2: Pointer to second term to be compared. This term must be
 *           the other root term of the equation to which term1 belongs.
 *           If the term resulting from the rewrite of the connectedness
 *           checking process is one of the terms of the comparison then
 *           it must be placed in this term2 parameter (see comments above).
 *    maxvarnb:  Maximum variable number in the original critical pair
 *               equality. These are the variables to be grounded.
 *
 *  RETURNS:
 *
 *    1 if term1≻term2, 0 otherwise.
 *
 *--------------------------------------------------------------*/
int32_t CheckGJOrdering2(int32_t *varorder,int32_t *auxbuf,int32_t *eqset,
		uint8_t *term1,uint8_t *term2,int32_t maxvarnb){
	auto uint8_t *ptr1,*ptr2,*ptr3;                 /* Auxiliary pointers */
	auto int32_t wdiff;                             /* Weight of term1 minus weight of term2 */
	auto int32_t ii,jj;                             /* Auxiliary. */

	/* There are no variables to be grounded. Initialize the weight */
	/* difference of grounded variables to zero. */
	if (maxvarnb<0){
		wdiff=0;

	/* There are variables to be grounded. Initialize the weight */
	/* difference of grounded variables to the worst case, minimum weight. */
	/* Even with the same weight the grounded variables can maintain the */
	/* current ≽ ordering by precedence relation instead of weight. */
	} else {

		/* Build the differences in number of identical variables */
		/* in term1 and term2. */
		memset(auxbuf,0,(1+maxvarnb)*sizeof(auxbuf[0]));
		for (ptr1=term1,ptr2=NextItem(term1,OVERSUBTERMS);ptr1<ptr2;ptr1=NextItem(ptr1,IMMED)){
			if (VARIABLE&ptr1[0]){
				ii=eqset[varorder[*((int16_t *)&ptr1[1])]];
				if (ii==-1){
					auxbuf[varorder[*((int16_t *)&ptr1[1])]]++;
				} else {
					auxbuf[ii]++;
				}
			}
		}
		for (ptr1=term2,ptr2=NextItem(term2,OVERSUBTERMS);ptr1<ptr2;ptr1=NextItem(ptr1,IMMED)){
			if (VARIABLE&ptr1[0]){
				ii=eqset[varorder[*((int16_t *)&ptr1[1])]];
				if (ii==-1){
					auxbuf[varorder[*((int16_t *)&ptr1[1])]]--;
				} else {
					auxbuf[ii]--;
				}
			}
		}

		/* Compute the worst case of weight difference of grounded variables. */
		/* Take into account that the minimum weight of a term is 1. */
		/* If the worst case difference is negative then ordering condition */
		/* term1≻term2 doesn't hold. */
		for (wdiff=ii=0;ii<=maxvarnb;ii++){
			wdiff+=auxbuf[ii];
			if (wdiff<0){
				return(0);
			}
		}
	}

	/* Add the weight difference of terms that are not variables */
	/* to the weight difference of grounded variables. */
	for (ptr1=term1,ptr2=NextItem(term1,OVERSUBTERMS);ptr1<ptr2;ptr1=NextItem(ptr1,IMMED)){
		if (0==(VARIABLE&ptr1[0])){
			if (kbset.opts.fweight==ARITY){
				wdiff+=(1+10*(((symbol *)&ptr1[1])->symbol->arity));
			} else {
				wdiff++;
			}
		}
	}
	for (ptr1=term2,ptr2=NextItem(term2,OVERSUBTERMS);ptr1<ptr2;ptr1=NextItem(ptr1,IMMED)){
		if (0==(VARIABLE&ptr1[0])){
			if (kbset.opts.fweight==ARITY){
				wdiff-=(1+10*(((symbol *)&ptr1[1])->symbol->arity));
			} else {
				wdiff--;
			}
		}
	}

	/* If weight difference is positive then ordering condition term1≻term2 holds. */
	if (wdiff>0){
		return(1);

	/* Weight difference is zero. */
	} else if (wdiff==0){

		/* Symbol precedence relation (>>) holds. */
		if (((symbol *)&term1[1])->symbol->precedence>((symbol *)&term2[1])->symbol->precedence){
			return(1);
		}

		/* Symbol precedence relation (<<) holds. */
		if (((symbol *)&term1[1])->symbol->precedence<((symbol *)&term2[1])->symbol->precedence){
			return(0);
		}

		/* Both symbols have the same precedence so they are the same. */
		/* Loop through the arguments of both items. */
		for (ptr1=NextItem(term1,IMMED),ptr2=NextItem(term2,IMMED),ptr3=NextItem(term1,OVERSUBTERMS);
				ptr1<ptr3;ptr1=NextItem(ptr1,OVERSUBTERMS),ptr2=NextItem(ptr2,OVERSUBTERMS)){

			/* Arguments are different. */
			if (0==IsEqual(ptr1,ptr2,0)){

				/* Argument of term1 is not a variable. */
				if (0==(VARIABLE&ptr1[0])){

					/* Argument of term2 is not a variable. */
					if (0==(VARIABLE&ptr2[0])){
						if (kbset.opts.termord==STANDARD){
							return(CheckGJOrdering2(varorder,auxbuf,eqset,ptr1,ptr2,maxvarnb));
						} else {
							if (GREATER==SecondKBOCheck(ptr1,ptr2,NONRECURSIVE)){
								return(1);
							}
							return(0);
						}

					/* Argument of term2 is a variable. Return not ground joinable */
					/* because as the variable is grounded the comparison will be */
					/* in terms of precedence but the precedence of the grounded */
					/* may be whatever so there are cases that it will not be LESSER. */
					} else {
						return(0);
					}

				/* Argument of term1 is a variable. */
				} else {

					/* If argument in term2 is not a variable then return */
					/* not joinable because as term1 is a variable then */
					/* the same reasoning of case above applies. */
					if (0==(VARIABLE&ptr2[0])){
						return(0);
					}

					/* Both arguments are variables and at least one of them */
					/* is not equalized. */
					ii=*((int16_t *)&ptr1[1]);
					jj=*((int16_t *)&ptr2[1]);
					if ((eqset[varorder[ii]]==-1)||(eqset[varorder[jj]]==-1)){

						/* If variable in term1 is GREATER or EQUAL than */
						/* variable in term2 then equalize both variables. */
						if (varorder[ii]<varorder[jj]){
							Equalize(ii,jj,varorder,eqset,maxvarnb);

						/* If variable in term1 is LESSER or EQUAL than */
						/* variable in term2 then return not joinable. */
						} else {
							return(0);
						}

					/* Both arguments are equalized variables and variable */
					/* in term1 is GREATER or EQUAL than variable in term2. */
					/* Equalize both variables. */
					} else if (eqset[varorder[ii]]<eqset[varorder[jj]]){
						Equalize(ii,jj,varorder,eqset,maxvarnb);

					/* Both arguments are equalized variables and variable */
					/* in term2 is GREATER or EQUAL than variable in term1. */
					/* Return not joinable. */
					} else if (eqset[varorder[ii]]>eqset[varorder[jj]]){
						return(0);
					}
				}
			}
		}

	/* Weight difference is less than zero. */
	/* Return not joinable. */
	} else {
		return(0);
	}

	/* Return ordering condition term1≽term2 holds. */
	return(1);
} /* CheckGJOrdering2 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Equalize two variables for ground joinability checking)  OCJ
 *
 *    This function equalizes two variables that are generically grounded
 *    for ground joinability ordering checking. Once the variables are
 *    "equalized" then they are considered grounded and equal to all effects
 *    during the ordering checking in CheckGJOrdering2 function.
 *
 *    If the two variables are not consecutive in the x₁≽x₂≽...≽xₚ sequence
 *    (see CheckGJOrdering2() function general comments) then al variables
 *    in between are also equalized. Also if one or both variables are
 *    already equalized with other variables but not equalized between
 *    themselves then all involved variables are equalized between
 *    themselves.
 *
 *
 *  ARGUMENTS:
 *
 *    var1: Variable number of first variable. This is the number used
 *          in the internal Drodi binary format and must not be confused
 *          with the indices in the x₁≽x₂≽...≽xₚ sequence. The condition
 *          varorder[var1]<varorder[var2] must be met, otherwise result
 *          will be unpredictable (see var2 and varorder definitions below).
 *    var2: Variable number of second variable. This is the number used
 *          in the internal Drodi binary format and must not be confused
 *          with the indices in the x₁≽x₂≽...≽xₚ sequence. The condition
 *          varorder[var1]<varorder[var2] must be met, otherwise result
 *          will be unpredictable (see var1 definition above and varorder
 *          definition below).
 *    varorder: Pointer to set with the order of variables in the ground
 *              ordering analyzed. If the variable number i is the xₖ
 *              variable then varorder[i]==k-1 (see CheckGJOrdering2()
 *              function general comments).
 *    eqset: Pointer to set of integers used for variable equalization
 *           control. The index order in this set corresponds to the
 *           index order in the x₁≽x₂≽...≽xₚ sequence. If eqset[m-1]==-1
 *           then the variable xₘ in the ordering list above has not been
 *           equalized. If If eqset[m-1]!=-1 then the variable xₘ has been
 *           equalized with all variables with the same value in their
 *           corresponding eqset[] element.
 *    maxvarnb:  Maximum variable number in the original critical pair
 *               equality. These are the variables to be grounded.
 *
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void Equalize(int32_t var1,int32_t var2,int32_t *varorder,int32_t *eqset,int32_t maxvarnb){
	auto int32_t ii,jj,kk,vv;                       /* Auxiliary. */

	/* Get the initial index and value of the range to be updated in eqset. */
	/* The variable var1 is not equalized. */
	kk=varorder[var1];
	if (eqset[kk]==-1){
		ii=vv=kk;

	/* The variable var1 is equalized. */
	} else {
		ii=kk+1;
		vv=eqset[kk];
	}

	/* Get the final index in the equalization range in eqset. */
	/* The variable var2 is not equalized. */
	kk=varorder[var2];
	if (eqset[kk]==-1){
		jj=kk;

	/* The variable var2 is equalized. */
	} else {
		for (jj=kk;(jj<maxvarnb)&&(eqset[jj+1]==eqset[kk]);jj++){
		}
	}

	/* Set the equalization range and return. */
	for (;ii<=jj;ii++){
		eqset[ii]=vv;
	}
	return;
} /* Equalize */

#ifdef SEMANTICTAUTOLOGY
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Term comparison for semantic tautology detection)  OCJ
 *
 *    This function performs a KBO comparison between two terms that
 *    assuming that all variables are skolemized to zero arity skolems.
 *    Both terms share the same variables. The precedence of skolemized
 *    variables are higher than the precedence of any symbol and precedence
 *    comparison between skolemized variables are according to the
 *    variable number. A uniform symbol weight is used.
 *
 *    This comparison is used when detecting semantic tautologies.
 *
 *    This function is called recursively.
 *
 *
 *  ARGUMENTS:
 *
 *    term1: Pointer to first term to be compared.
 *    term2: Pointer to second term to be compared.
 *
 *  RETURNS:
 *
 *    EQUAL if term1≡term2, GREATER if term1≻term2, LESSER if term1≺term2.
 *
 *--------------------------------------------------------------*/
int32_t TtlgCompare(uint8_t *term1,uint8_t *term2){
	auto int32_t weight1,weight2;                   /* Term weights */
	auto uint8_t *ptr1,*ptr2,*ptr3;                 /* Auxiliary pointers */
	auto int32_t ii,jj;                             /* Auxiliary. */

	/* Compute term weights. */
	for (weight1=0,ptr1=term1,ptr2=NextItem(term1,OVERSUBTERMS);ptr1<ptr2;ptr1=NextItem(ptr1,IMMED),weight1++){
	}
	for (weight2=0,ptr1=term2,ptr2=NextItem(term2,OVERSUBTERMS);ptr1<ptr2;ptr1=NextItem(ptr1,IMMED),weight2++){
	}

	/* Perform term comparison. */
	/* Weights are different. */
	if (weight1>weight2){
		return(GREATER);
	} else if (weight1<weight2){
		return(LESSER);
	}

	/* Weights are equal. */
	/* One term is a function and the other term is a skolemized variable. */
	if (((VARIABLE|FUNCTION)&term1[0])!=((VARIABLE|FUNCTION)&term2[0])){
		if (VARIABLE&term1[0]){
			return(GREATER);
		}
		return(LESSER);

	/* Both terms are skolemized variables. */
	} else if (VARIABLE&term1[0]){
		ii=*((int16_t *)&term1[1]);
		jj=*((int16_t *)&term2[1]);
		if (ii>jj){
			return(GREATER);
		} else if (jj>ii){
			return(LESSER);
		}
		return(EQUAL);
	}

	/* Symbol precedence relation (>>) holds. */
	if (((symbol *)&term1[1])->symbol->precedence>((symbol *)&term2[1])->symbol->precedence){
		return(GREATER);
	}

	/* Symbol precedence relation (<<) holds. */
	if (((symbol *)&term1[1])->symbol->precedence<((symbol *)&term2[1])->symbol->precedence){
		return(LESSER);
	}

	/* Both symbols have the same precedence so they are the same symbol. */
	/* Loop through the arguments of both items. */
	for (ptr1=NextItem(term1,IMMED),ptr2=NextItem(term2,IMMED),ptr3=NextItem(term1,OVERSUBTERMS);
			ptr1<ptr3;ptr1=NextItem(ptr1,OVERSUBTERMS),ptr2=NextItem(ptr2,OVERSUBTERMS)){
		if (EQUAL!=(ii=TtlgCompare(ptr1,ptr2))){
			return(ii);
		}
	}

	return(EQUAL);
} /* TtlgCompare */
#endif

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform second KBO check)  OCJ
 *
 *    This function performs the second Knuth-Bendix ordering check
 *    for the case of first check failure, for both standard and
 *    non recursive KBO ordering depending on the value of flags.
 *
 *    This function can be used for terms that are not variables
 *    as well as for predicates, but not for equalities. In the case
 *    of predicates it just compares the atoms, it doesn't take into
 *    account any possible negations.
 *
 *    Let :> be the KBO ordering relation (standard or non recursive,
 *    depending on the value of flag) of definition 3.14 or definition 3.15
 *    as described in document "Implementing an efficient theorem prover"
 *    by Alexandre Riazanov. Let > and ·> be the binary relations of
 *    definition 3.13 of above document. As indicated in the document,
 *    when the relation > fails a second check is performed. This is
 *    the check performed by this function to verify that item1 ·> item2.
 *
 *    The return conditions are:
 *
 *    GREATER if item1 :> item2
 *    LESSER if item2 :> item2
 *    EQUAL if item1 = item2
 *    FAILURE otherwise
 *
 *
 *  ARGUMENTS:
 *
 *    item1: Pointer to first item to be compared.
 *    item2: Pointer to second item to be compared.
 *    flag: STANDARD or NONRECURSIVE
 *
 *
 *  RETURNS:
 *
 *    GREATER, LESSER, EQUAL, FAILURE or NOMEMORY (see above).
 *
 *--------------------------------------------------------------*/
int32_t SecondKBOCheck(uint8_t *item1,uint8_t *item2,int32_t flag){
	auto uint8_t *ptr1,*ptr2,*ptr3;                 /* Auxiliary pointers */
	auto int32_t ii;                                /* Auxiliary. */

	/* Symbol precedence relation (>>) holds. */
	if (((symbol *)&item1[1])->symbol->precedence>((symbol *)&item2[1])->symbol->precedence){
		return(GREATER);
	}

	/* Symbol precedence relation (<<) holds. */
	if (((symbol *)&item1[1])->symbol->precedence<((symbol *)&item2[1])->symbol->precedence){
		return(LESSER);
	}

	/* Both symbols have the same precedence. */
	/* Standard ordering. Check standard KBO ordering relation recursively. */
	if (flag&STANDARD){

		/* Loop through the arguments of both items. */
		for (ptr1=NextItem(item1,IMMED),ptr2=NextItem(item2,IMMED),ptr3=NextItem(item1,OVERSUBTERMS);
				ptr1<ptr3;ptr1=NextItem(ptr1,OVERSUBTERMS),ptr2=NextItem(ptr2,OVERSUBTERMS)){
			if (EQUAL!=(ii=CompareItems(ptr1,ptr2,-2))){
				return(ii);
			}
		}

	/* Non recursive ordering. Check >>> ordering relation (slightly modified). */
	} else {

		/* Loop through the arguments of both items. */
		for (ptr1=NextItem(item1,IMMED),ptr2=NextItem(item2,IMMED),ptr3=NextItem(item1,OVERSUBTERMS);
				ptr1<ptr3;ptr1=NextItem(ptr1,OVERSUBTERMS),ptr2=NextItem(ptr2,OVERSUBTERMS)){

			/* None of the items is a variable. */
			if ((0==(ptr1[0]&VARIABLE))&&(0==(ptr2[0]&VARIABLE))){
				if (EQUAL!=(ii=SecondKBOCheck(ptr1,ptr2,flag))){
					return(ii);
				}

			/* Both items are a variable. */
			} else if ((ptr1[0]&VARIABLE)&&(ptr2[0]&VARIABLE)){

				/* The variables are not the same. */
				if ((*((int16_t *)&ptr1[1]))!=(*((int16_t *)&ptr2[1]))){
					return(FAILURE);
				}

			/* One item is a variable and the other is not a variable. */
			} else {
				return(FAILURE);
			}
		}
	}

	return(EQUAL);
} /* SecondKBOCheck */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Apply substitution to two terms)  OCJ
 *
 *    This function applies a substitution to two items that belong to
 *    the same clause. The substitution is applied by building a dummy
 *    clause with just an equality in which the two root terms are the
 *    terms passed as arguments to this function. Then ApplySubst2Clause()
 *    function is called.
 *
 *
 *  ARGUMENTS:
 *
 *    term1: Pointer to first term.
 *    term2: Pointer to second term.
 *    pterm1s: Address of pointer to first term with substitution applied.
 *    pterm2s: Address of pointer to second term with substitution applied.
 *    pclause: Address of pointer to cmprefix structure of dummy clause.
 *    Subst: Pointer to substitution to be applied to term1 and term2
 *           before checking the ordering constraint.
 *    varshift: Shift for variable numbers belonging to a secondary
 *              merged clause. Variable numbers in substitution
 *              coming from secondary clause increased by varshift
 *              match variable numbers in the substitution.
 *    flag: ITEM1SEC: If this flag is set then both terms belong to
 *          a secondary item with respect to the substitution. If this
 *          flag is not set then the terms belong to a primary item.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t ApplySbst2TwoItm(uint8_t *term1,uint8_t *term2,uint8_t **pterm1s,uint8_t **pterm2s,
		cmprefix **pclause,subst *Subst,int32_t varshift,int32_t flag){
	auto uint8_t *ptr1;                             /* Auxiliary pointer */
	auto int32_t ii,jj,kk;                          /* Auxiliary. */

	/* Return if substitution is empty. */
	if ((Subst->buffer==NULL)||(Subst->substsize==1)){
		*pclause=NULL;
		*pterm1s=term1;
		*pterm2s=term2;
		return(0);
	}

	/* Allocate space for dummy clause. */
	if (NULL==((*pclause)=MYALLOC(sizeof(cmprefix)))){
		return(NOMEMORY);
	}
	jj=NextItem(term1,OVERSUBTERMS)-term1;
	kk=NextItem(term2,OVERSUBTERMS)-term2;
	ii=sizeof(binprefix)+2*jj+2*kk+1+sizeof(symbol)+CLAUSE_CHUNK_SIZE;
	if (NULL==(((*pclause)->part2.bin)=MYALLOC(ii))){
		MYFREE(*pclause);
		return(NOMEMORY);
	}
	(*pclause)->part2.bin->clause=*pclause;
	(*pclause)->part2.bin->oriented=0;

	/* Build dummy equality clause formula. */
	(*pclause)->part2.bin->formula[0]=EQUALITY;
	((symbol *)&(*pclause)->part2.bin->formula[1])->symbol=&equkey;
	(*pterm1s)=&(*pclause)->part2.bin->formula[1]+sizeof(symbol);
	memcpy(*pterm1s,term1,jj);
	memcpy(*pterm1s+jj,term2,kk);
	(*pterm1s)[jj+kk]=UNITEND;

	/* If terms belong to a secondary item then shift variable numbers. */
	if (ITEM1SEC&flag){
		for (ptr1=*pterm1s;ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){
			if (VARIABLE&ptr1[0]){
				*((uint16_t *)&ptr1[1])+=varshift;
			}
		}
	}

	/* Apply substitution to dummy clause and update clause parameters. */
	if (NOMEMORY==ApplySubst2Clause(*pclause,&ii,Subst,varshift)){
		MYFREE((*pclause)->part2.bin);
		MYFREE(*pclause);
		return(NOMEMORY);
	}
	(*pclause)->part2.bin->maxvarnb=-1;
	UpdateOffsets((*pclause)->part2.bin,&(*pclause)->part2.bin->formula[0]);

	/* Get pointers to substituted terms. */
	(*pterm1s)=&(*pclause)->part2.bin->formula[1]+sizeof(symbol);
	(*pterm2s)=NextItem(*pterm1s,OVERSUBTERMS);
	return(0);
} /* ApplySbst2TwoItm */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check ordering constraints in equality root terms)  OCJ
 *
 *    This function checks ordering constraints in equality root terms
 *    of a literal or term after applying a substitution to the terms.
 *
 *    It is assumed that term1 and term2 are root terms of the same equality,
 *    they has been compared with no substitution applied and have the following
 *    flags set:
 *      SELECTED if the term is not lesser nor equal than the other term.
 *      MAXIMUM if the term is greater than the other term.
 *
 *
 *  ARGUMENTS:
 *
 *    term1: Equality root term that is checked for being greater than
 *           (MAXIMUM flag set) or not lesser than (MAXIMUM flag no set)
 *           term2.
 *    term2: The other equality root terms that is checked against term1.
 *    Subst: Pointer to substitution to be applied to term1 and term2
 *           before checking the ordering constraint.
 *    varshift: Shift for variable numbers belonging to a secondary
 *              merged clause. Variable numbers in substitution
 *              coming from secondary clause increased by varshift
 *              match variable numbers in the substitution.
 *    flag: MAXIMUM: If this flag is set then term1 is checked for being
 *          greater than term2. If this flag is not set then term1 is
 *          checked for being not lesser nor equal than term2.
 *          ITEM1SEC: If this flag is set then term1 and term2 belong to
 *          a secondary item with respect to the substitution. If this
 *          flag is not set then the terms belong to a primary item.
 *
 *  RETURNS:
 *
 *    1 if terms comply with the ordering constraint.
 *    0 if terms don't comply with the ordering constraint.
 *    NOMEMORY if there is not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t CheckEqRootOrder(uint8_t *term1,uint8_t *term2,subst *Subst,
		int32_t varshift,int32_t maxvarnb,int32_t flag){
	auto uint8_t *term1b,*term2b;                        /* Pointers to root terms with substitution applied */
	auto cmprefix *clause;                               /* Auxiliary clause pointer for call to ApplySbst2TwoItm() */
	auto int32_t ii;                                     /* Auxiliary */

	/* If term1 is greater than term2 before applying the substitution */
	/* then the ordering constraint is met. */
	if (term1[0]&MAXIMUM){
		return(1);

	/* If term2 is greater than term1 before applying the substitution */
	/* then the ordering constraint is not met. */
	} else if (term2[0]&MAXIMUM){
		return(0);
	}

	/* Return failure if both equality root terms are EQUAL. This is */
	/* detected because none of root terms are SELECTED. */
	if ((0==(term1[0]&SELECTED))&&((0==(term2[0]&SELECTED)))){
		return(0);
	}

	/* Substitution is empty. */
	if ((Subst->buffer==NULL)||(Subst->substsize==1)){
		if (0==(flag&MAXIMUM)){
			return(1);
		} else {
			return(0);
		}
	}

	/* Apply substitution to equality root terms. */
	if (NOMEMORY==ApplySbst2TwoItm(term1,term2,&term1b,&term2b,&clause,Subst,varshift,flag)){
		return(NOMEMORY);
	}

	/* Compare root terms, free memory and return according to result comparison. */
	ii=CompareItems(term1b,term2b,maxvarnb);
	MYFREE(clause->part2.bin);
	MYFREE(clause);
	if (flag&MAXIMUM){
		switch (ii){
			case NOMEMORY:
				return(NOMEMORY);
				break;
			case GREATER:
				return(1);
				break;
		}
	} else {
		switch (ii){
			case NOMEMORY:
				return(NOMEMORY);
				break;
			case GREATER:
			case FAILURE:
				return(1);
				break;
		}
	}
 	return(0);
} /* CheckEqRootOrder */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check ordering constraints in equality root terms)  OCJ
 *
 *    This function is similar to CheckEqRootOrder() with the following
 *    differences:
 *    - The flags SELECTED or MAXIMUM will be used if set but they don't
 *      need to be necessarily set.
 *    - No varshift parameter is passed to this function because the shift
 *      has been applied to the terms and doesn't need to be applied to
 *      the substitution.
 *    - FLag ITEM1SEC is not used for the reasons indicated above.
 *
 *
 *  ARGUMENTS:
 *
 *    term1: Equality root term that is checked for being greater than
 *           (MAXIMUM flag set) or not lesser (MAXIMUM flag no set) than
 *           term2.
 *    term2: The other equality root terms that is checked against term1.
 *    Subst: Pointer to substitution to be applied to term1 and term2
 *           before checking the ordering constraint.
 *    flag: MAXIMUM: If this flag is set then term1 is checked for being
 *          greater than term2. If this flag is not set then term1 is
 *          checked for being not lesser nor equal than term2.
 *
 *  RETURNS:
 *
 *    1 if terms comply with the ordering constraint.
 *    0 if terms don't comply with the ordering constraint.
 *    NOMEMORY if there is not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t CheckEqRootOrder2(uint8_t *term1,uint8_t *term2,subst *Subst,int32_t flag){
	auto uint8_t *sterm1,*sterm2;                        /* Pointer to terms with substitution applied */
	auto uint8_t *ptr1;                                  /* Auxiliary */
	auto int32_t ii,vv;                                  /* Auxiliary */

	/* If term1 is greater than term2 before applying the substitution */
	/* then the ordering constraint is met. */
	if (term1[0]&MAXIMUM){
		return(1);
	}

	/* Build terms with substitution applied. */
	if ((Subst->buffer==NULL)||(Subst->substsize==1)){
		sterm1=term1;
		sterm2=term2;
	} else {
		if (NULL==(sterm1=ApplySubst2Term(term1,Subst,0))){
			return(NOMEMORY);
		}
		if (NULL==(sterm2=ApplySubst2Term(term2,Subst,0))){
			MYFREE(sterm1);
			return(NOMEMORY);
		}
	}

	/* Get maximum variable number. */
	for (ptr1=sterm1,vv=-1;ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){
		if (VARIABLE&ptr1[0]){
			ii=*((int16_t *)&ptr1[1]);
			if (ii>vv){
				vv=ii;
			}
		}
	}
	for (ptr1=sterm2;ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){
		if (VARIABLE&ptr1[0]){
			ii=*((int16_t *)&ptr1[1]);
			if (ii>vv){
				vv=ii;
			}
		}
	}

	/* Compare equality root terms with substitution. */
	ii=CompareItems(sterm1,sterm2,vv);
	if ((Subst->buffer!=NULL)&&(Subst->substsize>1)){
		MYFREE(sterm1);
		MYFREE(sterm2);
	}
	switch (ii){
		case NOMEMORY:
			return(NOMEMORY);
			break;
		case GREATER:
			return(1);;
			break;
		case FAILURE:
			if (0==(MAXIMUM&flag)){
				return(1);;
			}
			break;
	}

 	return(0);
} /* CheckEqRootOrder2 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Update item offsets)  OCJ
 *
 *    This function updates the nextoff and offset parameters of
 *    an item and all its subterms such that NextItem() function
 *    will work properly.
 *
 *    The maximum variable number in the binprefix structure is
 *    also updated for use by CompareItemWeight() function when
 *    called by CheckEqRootOrder().
 *
 *    This function is recursively called.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    binp: Pointer to binprefix structure of item.
 *    item: Pointer to item.
 *
 *  RETURNS:
 *
 *    Pointer to next byte after item and all its subterms.
 *
 *
 *--------------------------------------------------------------*/
uint8_t *UpdateOffsets(binprefix *binp,uint8_t *item){
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto int32_t ii,jj;                                  /* Auxiliary */

	/* Item is a variable. */
	if (item[0]&VARIABLE){
		ptr1=&item[3];
		if ((*(int16_t *)&item[1])>binp->maxvarnb){
			binp->maxvarnb=*(int16_t *)&item[1];
		}

	/* Item is not a variable. */
	} else {

		/* Compute symbol arity and set offset field. */
		((symbol *)&item[1])->offset=item-&binp->formula[0];
		if (item[0]&EQUALITY){
			ii=2;
		} else {
			ii=((symbol *)&item[1])->symbol->arity;
		}

		/* Loop through item subterms. */
		for (ptr1=NextItem(item,IMMED),jj=0;jj<ii;jj++){
			ptr1=UpdateOffsets(binp,ptr1);
		}

		/* Update symbol nextoff field. */
		((symbol *)&item[1])->nextoff=((symbol *)&item[1])->offset+(ptr1-item);
	}

	return(ptr1);
} /* UpdateOffsets */
