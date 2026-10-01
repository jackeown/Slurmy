/*
 ============================================================================
 Name        : simplify.c
 Author      : Oscar Contreras
 Version     :
 Copyright   : Copyright (C) 2015-2016 Oscar Contreras. All rights reserved.
 Description : First order logic theorem prover in C
 ============================================================================
 */

/****************************************************************
*
*               simplify (Simplification inference functions module)
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
 *    This source contains the simplification inferences of the Drodi package.
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

/* Specific includes. */
#include "perfmon.h"

/* Global variables for this module: only those that need initialization in their definition. */

/* Inline functions specific for this module. */
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (First early pruning of clause subsuming cases)  OCJ
 *
 *    This function checks that the predicate-polarity pairs multiset of a
 *    candidate subsuming clause is a sub-multiset of the predicate-polarity
 *    pairs multiset of a candidate subsumed clause. If this condition is
 *    not true then the subsuming is not possible.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    formula1: Pointer to formula in compiled binary form as it is in the
 *              formula field of binprefix structure. This formula will be
 *              checked for being a good candidate for being subsumed by
 *              binp2. It points to a literal in the clause that is being
 *              checked for being subsumed, but not necessarily to the first
 *              literal. This is useful for forward subsumption, when the
 *              first literals of candidate subsumed clause have been already
 *              checked and found that they cannot participate in the subsumption.
 *    lits1: Number of literals starting at formula1 pointer.
 *    binp2: Pointer to binprefix structure of clause in compiled binary. This
 *           clause will be checked for subsuming formula1.
 *
 *  RETURNS:
 *
 *    0 -> Subsumption is not possible.
 *    1 -> Subsumption may be possible.
 *
 *
 *--------------------------------------------------------------*/
static inline int32_t PruneSubsumption(uint8_t *formula1,int32_t lits1,binprefix *binp2){
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto uint32_t ii;                                    /* Auxiliary */

	/* Reset predicateset if needed. */
	if (sstimestamp>((uint32_t)0xffffffff-lits1)){
		memset(predicateset,0,2*numpreds*sizeof(predicateset[0]));
		sstimestamp=0;
	}

	/* Build the subsumed clause multiset information. */
	/* Loop through candidate subsumed formula literals. */
	for (ptr1=formula1;ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS)){
		ii=(((symbol *)&ptr1[1])->symbol)->prednumber;
		if (ptr1[0]&NEGATED){
			ii+=numpreds;
		}
		if (predicateset[ii]<sstimestamp){
			predicateset[ii]=sstimestamp+1;
		} else {
			predicateset[ii]++;
		}
	}

	/* Compare with the subsuming clause multiset information. */
	/* Loop through candidate subsuming clause literals. */
	for (ptr1=&binp2->formula[0];ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS)){
		ii=(((symbol *)&ptr1[1])->symbol)->prednumber;
		if (ptr1[0]&NEGATED){
			ii+=numpreds;
		}
		if (predicateset[ii]<=sstimestamp){
			sstimestamp+=lits1;
			return(0);
		}
	}

	/* Return subsumption possible. */
	sstimestamp+=lits1;
	return(1);
} /* PruneSubsumption */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (FreeSubsumptionMemory)  OCJ
 *
 *    This function frees all memory allocated to run the subsumption
 *    SAT solver.
 *
 *
 *
 *  ARGUMENTS: None
 *
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *--------------------------------------------------------------*/
static inline void FreeSubsumMem(){
	auto subsatcl *ptr1;                                 /* Auxiliary pointer */
	auto int32_t ii;                                     /* Auxiliary */

	for (ptr1=subsctrl.frstsat;ptr1!=NULL;ptr1=ptr1->glblnext){
		for (ii=0;ii<ptr1->literals;ii++){
			MYFREE(ptr1->formula[ii].psubst[0]->buffer);
			free(ptr1->formula[ii].psubst[0]);
			if (ptr1->formula[ii].psubst[1]!=NULL){
				MYFREE(ptr1->formula[ii].psubst[1]->buffer);
				free(ptr1->formula[ii].psubst[1]);
			}
		}
	}
	free(subsctrl.allmemory);
} /* FreeSubsumMem */

#ifdef REDUNDANCYCOMPLETE
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform demodulation redundancy check)  OCJ
 *
 *    This function checks if a demodulation inference preserves
 *    completion or not. This check is experimental and may be
 *    not correct, so it may have to be corrected in the future.
 *
 *    According to section 3.1.3 "Simplification rules" of document
 *    "Implementing an efficient theorem prover" by Alexandre Riazanov
 *    the demodulation rule can fall outside the general redundancy
 *    criteria if some ground instance of the clause being simplified
 *    by demodulation is smaller than all ground instances of the
 *    demodulating clause. According to this a sufficient (but not
 *    strictly necessary) condition to preserve completeness is
 *    to meet at least one of the three following conditions:
 *    - The clause to be demodulated has at least one literal that
 *      is not an equality or inequality.
 *    - The literal of the demodulating clause is lesser than some
 *      literal of the clause being demodulated.
 *    - The term replaced by demodulation in the clause being demodulated
 *      is not an (in)equality root term. This is inspired in the help
 *      text for --demodulation_redundancy_check option of Vampire V4.0
 *      program.
 *
 *    The above check is the one implemented here.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    repterm: Pointer to term in clause being demodulated that will
 *             be replaced by demodulation.
 *    binpto: Pointer to binprefix structure of clause being
 *            demodulated.
 *    binpfr: Pointer to binprefix structure of demodulating clause.
 *
 *  RETURNS:
 *
 *    0 -> The demodulation DOES NOT preserve completeness.
 *    1 -> The demodulation preserves completeness.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
static inline int32_t RedundancyCheck(uint8_t *repterm,binprefix *binpto,binprefix *binpfr){
	auto int32_t varshift;                               /* Temporary shift of demodulating clause variables */
	auto int32_t tomaxvarnb;                             /* Value of maxvarnb field of demodulated clause */
	auto int32_t frmaxvarnb;                             /* Value of maxvarnb field of demodulating clause */
	auto uint8_t *ptr1,*ptr2;                            /* Auxiliary pointers */
	auto int32_t ii;                                     /* Auxiliary */

	/* Loop through literals in resulting clause being demodulated to */
	/* check that all literals in clause are equalities or inequalities. */
	/* Also check that the replaced term in clause being demodulated is */
	/* an (in)equality root term. */
	for (ptr1=&binpto->formula[0],ii=1;ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS)){
		if (0==(EQUALITY&ptr1[0])){
			return(1);
		}
		if (ii==1){
			if (repterm==(ptr2=NextItem(ptr1,IMMED))){
				ii=0;
			} else if (repterm==NextItem(ptr2,OVERSUBTERMS)){
				ii=0;
			}
		}
	}
	if (ii){
		return(1);
	}

	/* If we are here then all literals in demodulated clause are equalities */
	/* or inequalities, so the demodulation may not preserve completeness. */
	/* Temporarily shift the demodulating clause variables. The maxvarnb */
	/* parameter of demodulated clause is also shifted. This is necessary */
	/* for comparison of literals of different clauses. These shifts must be undone */
	/* before returning. */
	tomaxvarnb=binpto->maxvarnb;
	frmaxvarnb=binpfr->maxvarnb;
	if (binpto->maxvarnb<0){
		varshift=0;
		binpto->maxvarnb=binpfr->maxvarnb;
	} else if (binpfr->maxvarnb>=0){
		varshift=binpto->maxvarnb+1;
		for (ptr1=NextItem(&binpfr->formula[0],IMMED);ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){
			if (VARIABLE&ptr1[0]){
				*((uint16_t *)&ptr1[1])+=varshift;
			}
		}
		binpfr->maxvarnb+=varshift;
		binpto->maxvarnb=binpfr->maxvarnb;
	} else {
		varshift=0;
		binpfr->maxvarnb=binpto->maxvarnb;
	}

	/* Check if the demodulating clause literal is lesser than some */
	/* demodulated clause literal. */
	/* Loop through demodulated clause literals. */
	for (ptr2=&binpto->formula[0],ptr1=&binpfr->formula[0];ptr2[0]!=UNITEND;ptr2=NextItem(ptr2,OVERSUBTERMS)){

		/* Check if the demodulating clause literal is lesser than */
		/* the demodulated clause literal. */
		switch (CompareLiterals(ptr1,ptr2)){

			/* Not enough memory. */
			case NOMEMORY:
				if (varshift>0){
					for (ptr1=NextItem(&binpfr->formula[0],IMMED);ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){
						if (VARIABLE&ptr1[0]){
							*((uint16_t *)&ptr1[1])-=varshift;
						}
					}
				}
				binpfr->maxvarnb=frmaxvarnb;
				binpto->maxvarnb=tomaxvarnb;
				return(NOMEMORY);
				break;

			/* Completeness is preserved. */
			case LESSER:
				if (varshift>0){
					for (ptr1=NextItem(&binpfr->formula[0],IMMED);ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){
						if (VARIABLE&ptr1[0]){
							*((uint16_t *)&ptr1[1])-=varshift;
						}
					}
				}
				binpfr->maxvarnb=frmaxvarnb;
				binpto->maxvarnb=tomaxvarnb;
				return(1);
				break;
		}
	}

	/* Undo demodulation clause variable shift */
	/* and return that completeness is not preserved. */
	if (varshift>0){
		for (ptr1=NextItem(&binpfr->formula[0],IMMED);ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){
			if (VARIABLE&ptr1[0]){
				*((uint16_t *)&ptr1[1])-=varshift;
			}
		}
	}
	binpfr->maxvarnb=frmaxvarnb;
	binpto->maxvarnb=tomaxvarnb;
	return(0);
} /* RedundancyCheck */
#else
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform demodulation redundancy check)  OCJ
 *
 *    This function checks if a demodulation inference preserves
 *    completion or not. This check is experimental and may be
 *    not correct, so it may have to be corrected in the future.
 *
 *    The check is based on a free interpretation of information
 *    from two sources:
 *    - Document "Implementing an efficient theorem prover" by
 *      Alexandre Riazanov.
 *    - Help text for --demodulation_redundancy_check option of
 *      Vampire V4.0 program.
 *
 *    The check is performed as follows:
 *    - if the three following conditions are met then the demodulation
 *      doesn't preserve completeness.
 *      - The term replaced in the demodulated clause must be an
 *        equality root term.
 *      - The term that replaces the root term in the demodulated
 *        must be greater than the other root term in the replaced
 *        (in)equality.
 *      - The demodulating clause must be greater than the resulting
 *        clause from demodulation, according to the multiset and
 *        literal orderings described in chapter 2 and section 3.1.1
 *        of the above mentioned document.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    termfr: Pointer to equality root term in demodulating clause that
 *            is a generalization of a term in demodulated clause.
 *    repterm: Pointer to term in clause resulting from demodulation that
 *             replaced the term generalized by termfr.
 *    binprslt: Pointer to binprefix structure of clause resulting from
 *              demodulation.
 *    binpfr: Pointer to binprefix structure of demodulating clause.
 *
 *  RETURNS:
 *
 *    0 -> The demodulation preserves completeness.
 *    1 -> The demodulation DOES NOT preserve completeness.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
static inline int32_t RedundancyCheck(uint8_t *termfr,uint8_t *repterm,binprefix *binprslt,binprefix *binpfr){
	auto uint8_t *othrttrm;                              /* The other root term of the (in)equality to which repterm belongs */
	auto uint8_t *ptr1,*ptr2;                            /* Auxiliary pointers */
	auto int32_t ii,jj,kk;                               /* Auxiliary */

	/* Loop through literals in resulting clause from demodulation */
	/* to check that the term replaced in the demodulated clause */
	/* is an equality root term and that all literals in clause */
	/* are equalities or inequalities. This last check is a */
	/* prerequisite to check the clauses comparison. */
	othrttrm=NULL; /* Just to prevent compiler warnings. */
	for (ptr1=&binprslt->formula[0],ii=1;ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS)){
		if (0==(EQUALITY&ptr1[0])){
			return(0);
		}
		if (ii){
			if (repterm==(ptr2=NextItem(ptr1,IMMED))){
				ii=0;
			} else if (repterm==NextItem(ptr2,OVERSUBTERMS)){
				ii=0;
			}
		}
	}
	if (ii){
		return(0);
	}

	/* If we are here then all literals in resulting clause are equalities */
	/* or inequalities, so the demodulating clause is possibly greater */
	/* than the resulting clause. Recalculate maxvarnb field of clause */
	/* resulting from demodulation. This is necessary because de calling */
	/* function Demodulate() applies a substitution to the resulting clause */
	/* after pre-computing maxvarnb field and the substitution may change */
	/* its value. */
	binprslt->maxvarnb=-1;
	for (ptr1=NextItem(&binprslt->formula[0],IMMED);ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){
		if (VARIABLE&ptr1[0]){
			if (binprslt->maxvarnb<*((uint16_t *)&ptr1[1])){
				binprslt->maxvarnb=*((uint16_t *)&ptr1[1]);
			}
		}
	}

	/* Check that repterm is greater than othrttrm. If this is not the case */
	/* then the demodulation preserves completeness. */
	jj=binpfr->maxvarnb;
	kk=binprslt->maxvarnb;
	if (jj>kk){
		binprslt->maxvarnb=jj;
	} else {
		binpfr->maxvarnb=kk;
	}
	switch (CompareItems(repterm,othrttrm,binprslt->maxvarnb)){
		case NOMEMORY:
			binpfr->maxvarnb=jj;
			binprslt->maxvarnb=kk;
			return(NOMEMORY);
			break;
		case GREATER:
			break;
		default:
			binpfr->maxvarnb=jj;
			binprslt->maxvarnb=kk;
			return(0);
			break;
	}

	/* Check clause ordering constraint. To check this it is enough to */
	/* check that termfr is greater than all equality root terms in the */
	/* clause resulting from the demodulation clause. */
	/* Loop through literals in resulting clause from demodulation. */
	ii=kbset.opts.termord&STANDARD?STANDARD:NONRECURSIVE;
	for (ptr1=&binprslt->formula[0];ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS)){

		/* Compare with the first equality root term. */
		ptr2=NextItem(ptr1,IMMED);
		switch (CompareItems(termfr,ptr2,binpfr->maxvarnb)){
			case NOMEMORY:
				binpfr->maxvarnb=jj;
				binprslt->maxvarnb=kk;
				return(NOMEMORY);
				break;
			case GREATER:
				break;
			default:
				binpfr->maxvarnb=jj;
				binprslt->maxvarnb=kk;
				return(0);
				break;
		}

		/* Compare with the second equality root term. */
		ptr2=NextItem(ptr2,OVERSUBTERMS);
		switch (CompareItems(termfr,ptr2,binprslt->maxvarnb)){
			case NOMEMORY:
				binpfr->maxvarnb=jj;
				binprslt->maxvarnb=kk;
				return(NOMEMORY);
				break;
			case GREATER:
				break;
			default:
				binpfr->maxvarnb=jj;
				binprslt->maxvarnb=kk;
				return(0);
				break;
		}
	}

	/* Restore maxvarnb fields. */
	binpfr->maxvarnb=jj;
	binprslt->maxvarnb=kk;

	return(1);
} /* RedundancyCheck */
#endif

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform encompassment demodulation redundancy check)  OCJ
 *
 *    This performs an encompassment demodulation redundancy check to verify
 *    if a demodulation inference preserves completion or not. This check
 *    is described in "Ground Joinability and Connectedness in the
 *    Superposition Calculus" by André Duarte and Konstantin Korovin
 *
 *
 *
 *  ARGUMENTS:
 *
 *    binpto: Pointer to binprefix structure of clause being
 *            demodulated.
 *    repterm: Pointer to term in clause being demodulated that will
 *             be replaced by demodulation.
 *    substterm: Pointer to substituting term in the demodulating clause
 *               with the substitution applied. This term will replace
 *               repterm.
 *    Subst: Pointer to generalization substitution that when applied
 *           to genterm obtains binpto.
 *    varshift: Shift of variables in the substitution and related to it.
 *
 *  RETURNS:
 *
 *    0 -> The demodulation DOES NOT preserve completeness.
 *    1 -> The demodulation preserves completeness.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
static inline int32_t RedundancyCheck2(binprefix *binpto,uint8_t *repterm,uint8_t *substterm,
		subst *Subst,int32_t varshift){
	auto uint8_t *rtterm1,*rtterm2;                      /* Pointer to root terms of demodulated equality */
	auto uint8_t *ptr1,*ptr2,*ptr3;                      /* Auxiliary pointers */
	auto int32_t ii;                                     /* Auxiliary */

	/* If the demodulated clause is not a positive unit equality */
	/* then return completeness preservation. */
	if ((binpto->literals>1)||(EQUALITY!=((EQUALITY|NEGATED)&binpto->formula[0]))){
		return(1);
	}

	/* Get pointer to root terms in demodulated positive unit equality. */
	rtterm1=NextItem(&binpto->formula[0],IMMED);
	rtterm2=NextItem(rtterm1,OVERSUBTERMS);

	/* If repterm is not any of the root terms then */
	/* return completeness preservation. */
	if ((repterm!=rtterm1)&&(repterm!=rtterm2)){
		return(1);
	}

	/* If the substitution doesn't generate a variant of the root term */
	/* in the demodulating clause that is a generalization */
	/* of repterm then return completeness preservation. */
	/* This is the same as checking that the substitution is not an */
	/* injective application from variables to variables and is done */
	/* by calling VariantSubst() function. */
	switch (IsVariantSubst(Subst,binpto->maxvarnb+varshift)){
		case 0:
			return(1);
			break;
		case NOMEMORY:
			return(NOMEMORY);
			break;
	}

	/* If the root term that is not repterm is MAXIMUM then */
	/* return completeness preservation. The pointer ptr1 */
	/* is set to the root term in demodulated equality that */
	/* is not affected by demodulation. */
	if (binpto->oriented!=0){
		if (repterm==rtterm1){
			ptr1=rtterm2;
			if (MAXIMUM&rtterm2[0]){
				return(1);
			}
		} else {
			ptr1=rtterm1;
			if (MAXIMUM&rtterm1[0]){
				return(1);
			}
		}
	} else {
		if (repterm==rtterm1){
			ptr1=rtterm2;
		} else {
			ptr1=rtterm1;
		}
	}

	/* Shift variables of root term in the demodulated equality */
	/* that is not affected by the demodulation. */
	for(ptr2=ptr1,ptr3=NextItem(ptr1,OVERSUBTERMS);ptr2<ptr3;ptr2=NextItem(ptr2,IMMED)){
		if (VARIABLE&ptr2[0]){
			*((int16_t *)&ptr2[1])+=varshift;
		}
	}

	/* Compare substterm with the root term in the demodulated */
	/* equality that is not affected by the demodulation. */
	ii=CompareItems(substterm,ptr1,binpto->maxvarnb+varshift);

	/* Unshift variables of root term in the demodulated equality */
	/* that is not affected by the demodulation. */
	for(ptr2=ptr1;ptr2<ptr3;ptr2=NextItem(ptr2,IMMED)){
		if (VARIABLE&ptr2[0]){
			*((int16_t *)&ptr2[1])-=varshift;
		}
	}

	/* If substterm is LESSER than the root term in the demodulated */
	/* equality that is not affected by the demodulation then */
	/* return completeness preservation. */
	switch (ii){
		case LESSER:
			return(1);
			break;
		case NOMEMORY:
			return(NOMEMORY);
			break;
	}

	/* Return that completeness is not preserved. */
	return(0);
} /* RedundancyCheck2 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if a clause is a tautology)  OCJ
 *
 *    This function checks if a clause in internal compiled format
 *    is a tautology.
 *
 *    Three checks are performed to check for a tautology:
 *
 *    The first check is for equational tautologies, that is,
 *    for clauses of the form C|t=t or C|"a"="a" or C|"a"!="b" where C
 *    is a sub-clause, t is a term and "a" and "b" are "distinct objects".
 *
 *    The second check is positive if the clause contains a literal
 *    and its negation, that is, for clauses of the form "C|P|~P"
 *    where C is a sub-clause and P is an atom. The literals must
 *    be one exactly the negation of the other except for equality
 *    symmetry, not just unify with each other. For instance, consider
 *    the following clause:
 *      A(B,x)|!A(y,C)
 *    where x and y are universally quantified variables. This
 *    clause is not a tautology because let A(B,D) be false and
 *    A(E,C) be true. Then the clause is false, so it is not
 *    necessarily true.
 *    Another example, the clause:
 *      A(x)|!A(B)
 *    where x is an universally quantified variable. This is not
 *    a tautology because let A(C) be false and A(B) be true.
 *    Then the clause is false.
 *
 *    The third check is to detect semantic tautologies (the previous
 *    two checks are for syntactic tautologies) and is performed only
 *    if SEMAMANTICTAUTOLOGY is defined. Semantic tautologies have
 *    at least one negative equality. Some examples are:
 *      a!=b|b!=c|c=a
 *      ![X,Y,Z]: (X!=Y|Y!=Z|Z=X)
 *      f(a)=f(b)|a!=b
 *      ![X,Y]: (f(X)=f(Y)|X!=Y)
 *      f(a)|~f(b)|a!=b
 *      ![X,Y]: (f(X)|~f(Y)|X!=Y)
 *      "a"!=c|"b"!=d|c!=d
 *    Let A1|A2|...|An be the clause to be checked and {I1, I2,... Im}
 *    a sub-multiset of {A1. A2,... An} with m<=n such that it has at
 *    least one negative equality and either at least one positive equality
 *    or one pair of literals of the form f(s1,...,sj) and ~f(t1,...,tj)
 *    or two negative equalities with one root term being a distinct object.
 *    if the clause I1|I2|...|Im is a tautology then obviously A1|A2|...|An
 *    is also a tautology. To detect if I1|I2|...|Im is a tautology it
 *    suffices to prove that its negation is unsatisfiable, that is, false.
 *    Then I1|I2|...|Im is necessarily true and therefore a tautology. The
 *    negation of I1|I2|...|Im is ~I1&~I2&...&~Im which is a set of m
 *    single literal clauses. Also the universally quantified variables
 *    are now skolems with arity zero as in the original clause there are
 *    no other quantifiers at a higher level than the clause variables.
 *    Therefore the clauses ~Ij are ground and all equalities can be
 *    oriented and used for demodulations. After each demodulation the
 *    result is checked for being of the type s!=s or "a"="b" or a $false
 *    can be inferred by resolution.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: pointer to clause in binary format.
 *
 *  RETURNS:
 *
 *    0 -> Clause is not a tautology.
 *    1 -> Clause is a tautology.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
#ifndef SEMANTICTAUTOLOGY
int32_t IsTautology(cmprefix *clause){
	auto hashchain *rtterm1s,*rtterm2s;                  /* Pointers to root term symbols in equalities */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5;          /* Auxiliary pointers */

	/* Loop through literals in clause to detect equalities */
	/* of identical terms or "distinct objects" tautologies. */
	if ((UNITEQUALITY|NONUNITEQUALITY)&pbmflags){
		for (ptr1=&clause->part2.bin->formula[0];(*ptr1)!=UNITEND;
				ptr1=NextItem(ptr1,OVERSUBTERMS)){
			if (EQUALITY&ptr1[0]){
				ptr2=NextItem(ptr1,IMMED);
				ptr3=NextItem(ptr2,OVERSUBTERMS);
				if (0==(NEGATED&ptr1[0])){
					if (IsEqual(ptr2,ptr3,0)){
						kbset.prstats.tautologies++;
						return(1);
					}
				} else {
					if ((FUNCTION&ptr2[0])&&(FUNCTION&ptr3[0])){
						rtterm1s=((symbol *)&ptr2[1])->symbol;
						rtterm2s=((symbol *)&ptr3[1])->symbol;
						if ((rtterm1s!=rtterm2s)&&(DISTINCTOBJECT&rtterm1s->type)
								&&(DISTINCTOBJECT&rtterm2s->type)){
							kbset.prstats.tautologies++;
							return(1);
						}
					}
				}
			}
		}
	}

	/* Loop through pairs of literals in clauses with more than one literal. */
	if (clause->part2.bin->literals>1){
		for (ptr1=&clause->part2.bin->formula[0];(*ptr1)!=UNITEND;
				ptr1=NextItem(ptr1,OVERSUBTERMS)){
			for (ptr2=NextItem(ptr1,OVERSUBTERMS);(*ptr2)!=UNITEND;
					ptr2=NextItem(ptr2,OVERSUBTERMS)){
				if (IsEqual(ptr1,ptr2,NEGATED)){
					kbset.prstats.tautologies++;
					return(1);
				} else if ((EQUALITY&ptr1[0])&&(EQUALITY&ptr2[0])&&((NEGATED&ptr1[0])!=((NEGATED&ptr2[0])))){
					ptr3=NextItem(ptr1,IMMED);
					ptr4=NextItem(ptr2,IMMED);
					ptr5=NextItem(ptr4,OVERSUBTERMS);
					if (IsEqual(ptr3,ptr5,0)){
						ptr3=NextItem(ptr3,OVERSUBTERMS);
						if (IsEqual(ptr3,ptr4,0)){
							kbset.prstats.tautologies++;
							return(1);
						}
					}
				}
			}
		}
	}

	return(0);
} /* IsTautology */
#else
int32_t IsTautology(cmprefix *clause){
	auto hashchain *rtterm1s,*rtterm2s;                  /* Pointers to function or term symbols */
	auto ttlgclause *frstpos,*frstneg;                   /* Pointers to first positive and negative clauses */
	auto ttlgclause *auxcl1,*auxcl2,*auxcl3;             /* Pointers to auxiliary clauses */
	auto ttlgdstrleaf **varleafs;                        /* Pointer to set of initial leafs for each variable */
	auto uint8_t *swpequ;                                /* Pointer to equality with swapped root terms */
	auto int32_t size;                                   /* Size of swpequ buffer */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5;          /* Auxiliary pointers */
	auto char *semlits;                                  /* Pointer to set to identify literals relevant for semantic tautologies */
	auto int32_t ii,jj,kk,ll,mm;                         /* Auxiliary */

	/* Allocate storage for semlits set and initialize control data */
	/* used to detect semantic tautologies. */
	if (NULL==(semlits=MYALLOC(clause->part2.bin->literals))){
		return(NOMEMORY);
	}
	ii=0;
	memset(semlits,0,clause->part2.bin->literals);

	/* Loop through literals in clause to to perform the first */
	/* check described in global comments. The bit 0 of variable */
	/* ii and the element semlits[jj] are set to 1 if there is a */
	/* negative equality at position jj. The bit 1 of variable ii */
	/* and the element semlits[jj] are set to 1 if a positive */
	/* equality or two negative equalities with one root term being */
	/* a distinct object are detected. */
	if ((UNITEQUALITY|NONUNITEQUALITY)&pbmflags){
		for (ptr1=&clause->part2.bin->formula[0],jj=mm=0;(*ptr1)!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS),jj++){

			/* Literal is an (in)equality. */
			if (EQUALITY&ptr1[0]){

				/* Literal is a positive equality. */
				ptr2=NextItem(ptr1,IMMED);
				ptr3=NextItem(ptr2,OVERSUBTERMS);
				if (0==(NEGATED&ptr1[0])){

					/* Literal is of the form s=s. */
					if (IsEqual(ptr2,ptr3,0)){
						kbset.prstats.tautologies++;
						MYFREE(semlits);
						return(1);
					}

					/* Set control data to detect semantic tautologies. */
					semlits[jj]=1;
					ii|=2;

				/* Literal is a negative equality. */
				} else {

					/* Check distinct object tautologies of the form "a"!="b". */
					if ((FUNCTION&ptr2[0])&&(FUNCTION&ptr3[0])){
						rtterm1s=((symbol *)&ptr2[1])->symbol;
						rtterm2s=((symbol *)&ptr3[1])->symbol;
						if ((rtterm1s!=rtterm2s)&&(DISTINCTOBJECT&rtterm1s->type)
								&&(DISTINCTOBJECT&rtterm2s->type)){
							kbset.prstats.tautologies++;
							MYFREE(semlits);
							return(1);
						}
						if ((DISTINCTOBJECT&rtterm1s->type)||(DISTINCTOBJECT&rtterm2s->type)){
							mm++;
						}
					}

					/* Set control data to detect semantic tautologies. */
					semlits[jj]=1;
					if (mm==2){
						ii|=2;
					} else {
						ii|=1;
					}
				}
			}
		}
	}

	/* Loop through pairs of literals in clause to perform the second */
	/* check described in global comments. The bit 2 of variable ii */
	/* is set to 1 if there is at least a pair of literals of the */
	/* form f(s1,...,sj) and ~f(t1,...,tj). Pair of literals of that */
	/* type are selected by setting the corresponding semlits[i] */
	/* to 1 because they are relevant for semantic tautology */
	/* detection. */
	for (ptr1=&clause->part2.bin->formula[0],jj=0;(*ptr1)!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS),jj++){
		for (ptr2=NextItem(ptr1,OVERSUBTERMS),kk=jj+1;(*ptr2)!=UNITEND;
				ptr2=NextItem(ptr2,OVERSUBTERMS),kk++){

			/* Literals are of the same type but have have different polarity. */
			if (((NEGATED&ptr1[0])!=(NEGATED&ptr2[0]))
					&&(((symbol *)&ptr1[1])->symbol==((symbol *)&ptr2[1])->symbol)){

				/* Literals are equal except for polarity. */
				if (IsEqual(ptr1,ptr2,NEGATED)){
					kbset.prstats.tautologies++;
					MYFREE(semlits);
					return(1);

				/* Literals are (in)equalities. Check equality symmetry. */
				} else if ((EQUALITY&ptr1[0])&&(EQUALITY&ptr2[0])){
					ptr3=NextItem(ptr1,IMMED);
					ptr4=NextItem(ptr2,IMMED);
					ptr5=NextItem(ptr4,OVERSUBTERMS);
					if (IsEqual(ptr3,ptr5,0)){
						ptr3=NextItem(ptr3,OVERSUBTERMS);
						if (IsEqual(ptr3,ptr4,0)){
							kbset.prstats.tautologies++;
							MYFREE(semlits);
							return(1);
						}
					}

				/* Literals are of the form f(s1,...,sj) and ~f(t1,...,tj). */
				/* Set control data to detect semantic tautologies. */
				} else {
					ii|=2;
					semlits[jj]=semlits[kk]=1;
				}
			}
		}
	}

	/* Clause must be checked for semantic tautologies. */
	if (ii==3){

		/* Renumber variables to minimize the highest number assigned. */
		if (NOMEMORY==RenumberVars(clause->part2.bin)){
			MYFREE(semlits);
			return(NOMEMORY);
		}

		/* Allocate memory for skolemized variables leafs. */
		if (clause->part2.bin->maxvarnb>=0){
			if (NULL==(varleafs=MYALLOC(jj=(1+clause->part2.bin->maxvarnb)*sizeof(*varleafs)))){
				MYFREE(semlits);
				return(NOMEMORY);
			}
			memset(varleafs,0,jj);
		} else {
			varleafs=NULL;
		}

		/* Build the single literal clauses ~I1, ~I2, ..., ~Im */
		/* (see global comments above), chain them and add them */
		/* to discrimination trees. */
		/* Loop through items in clause. */
		frstpos=frstneg=NULL;
		for (ptr1=&clause->part2.bin->formula[0],size=kk=0;(*ptr1)!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS),kk++){

			/* Literal is relevant for semantic tautology detection. */
			if (semlits[kk]){

				/* Allocate storage for clause, copy it and adjust */
				/* offset and nextoff fields. */
				jj=1+(NextItem(ptr1,OVERSUBTERMS)-ptr1);
				if (jj>size){
					size=jj;
				}
				if (NULL==(auxcl1=MYALLOC(ttlgclsize+jj))){
					FreeTtlgTreeMemory(varleafs,clause->part2.bin->maxvarnb);
					for (;frstpos!=NULL;frstpos=auxcl1){
						auxcl1=frstpos->next;
						MYFREE(frstpos);
					}
					for (;frstneg!=NULL;frstneg=auxcl1){
						auxcl1=frstneg->next;
						MYFREE(frstneg);
					}
					MYFREE(semlits);
					return(NOMEMORY);
				}
				memcpy(&auxcl1->formula[0],ptr1,jj-1);
				auxcl1->formula[jj-1]=UNITEND;
				UpdateParams2(&auxcl1->formula[0]);

				/* Link clause and orient it if applicable. */
				auxcl1->formula[0]^=NEGATED;
				if (NOMEMORY==EditTtlgDscTrees(auxcl1,varleafs,ADDTREES)){
					FreeTtlgTreeMemory(varleafs,clause->part2.bin->maxvarnb);
					MYFREE(auxcl1);
					for (;frstpos!=NULL;frstpos=auxcl1){
						auxcl1=frstpos->next;
						MYFREE(frstpos);
					}
					for (;frstneg!=NULL;frstneg=auxcl1){
						auxcl1=frstneg->next;
						MYFREE(frstneg);
					}
					MYFREE(semlits);
					return(NOMEMORY);
				}
				if (NEGATED&auxcl1->formula[0]){
					auxcl1->next=frstneg;
					auxcl1->prev=NULL;
					if (frstneg!=NULL){
						frstneg->prev=auxcl1;
					}
					frstneg=auxcl1;
				} else {
					auxcl1->next=frstpos;
					auxcl1->prev=NULL;
					if (frstpos!=NULL){
						frstpos->prev=auxcl1;
					}
					frstpos=auxcl1;
					if (EQUALITY&auxcl1->formula[0]){
						ptr2=NextItem(&auxcl1->formula[0],IMMED);
						ptr3=NextItem(ptr2,OVERSUBTERMS);
						ptr2[0]&=(~(MAXIMUM|SELECTED));
						ptr3[0]&=(~(MAXIMUM|SELECTED));
						if (GREATER==(jj=TtlgCompare(ptr2,ptr3))){
							ptr2[0]|=MAXIMUM;
						} else if (jj==LESSER){
							ptr3[0]|=MAXIMUM;
						}
					}
				}
			}
		}

		/* Free semlits memory. */
		MYFREE(semlits);

		/* Allocate memory for (in)equality with swapped root terms. */
		size*=2;
		if (NULL==(swpequ=MYALLOC(size))){
			FreeTtlgTreeMemory(varleafs,clause->part2.bin->maxvarnb);
			for (;frstpos!=NULL;frstpos=auxcl1){
				auxcl1=frstpos->next;
				MYFREE(frstpos);
			}
			for (;frstneg!=NULL;frstneg=auxcl1){
				auxcl1=frstneg->next;
				MYFREE(frstneg);
			}
			return(NOMEMORY);
		}

		/* Main loop. */
		for (ii=0;ii==0;){
			ii=1; /* Initially assume no main loop repetition. */

			/* Loop through positive clauses. */
			for (auxcl1=frstpos;auxcl1!=NULL;auxcl1=auxcl1->next){

				/* Clause is an equality. */
				if (EQUALITY&auxcl1->formula[0]){

					/* Look for a clause that can be demodulated. */
					ptr1=NextItem(&auxcl1->formula[0],IMMED);
					ptr2=NextItem(ptr1,OVERSUBTERMS);
					if (0==(MAXIMUM&ptr1[0])){
						if (MAXIMUM&ptr2[0]){
							ptr3=ptr1;
							ptr1=ptr2; /* MAXIMUM root term. */
							ptr2=ptr3; /* Non MAXIMUM root term. */
						} else {
							continue;
						}
					}
					if (0==QueryTtlgDscTree(ptr1,&ptr3,&ptr4,
							VARIABLE&ptr1[0]?(void **)varleafs:(void **)&((((symbol *)&ptr1[1])->symbol->posdsctrttlg)))){
						continue;
					}

					/* Compute necessary pointers and lengths to build demodulation result. */
					/* ptr1 -> MAXIMUM root term (already set). */
					/* ptr2 -> non MAXIMUM root term (already set). */
					/* ptr3 -> matching term (already set). */
					/* ptr4 -> matching term formula (already set). */
					/* ptr5 -> term after matching term and its sub-terms. */
					/* jj -> new clause first section length. */
					/* kk -> non MAXIMUM root term length. */
					/* ll -> new clause last section length. */
					/* auxcl2 -> pointer to clause containing ptr4 (in)equality */
					jj=ptr3-ptr4;
					ptr5=NextItem(ptr3,OVERSUBTERMS);
					kk=NextItem(ptr2,OVERSUBTERMS)-ptr2;
					ll=1+(NextItem(ptr4,OVERSUBTERMS)-ptr5);
					auxcl2=(ttlgclause *)(ptr4-ttlgclsize);

					/* Allocate memory for new clause. */
					if (NULL==(auxcl3=MYALLOC(sizeof(ttlgclause)+jj+kk+ll))){
						MYFREE(swpequ);
						FreeTtlgTreeMemory(varleafs,clause->part2.bin->maxvarnb);
						for (;frstpos!=NULL;frstpos=auxcl1){
							auxcl1=frstpos->next;
							MYFREE(frstpos);
						}
						for (;frstneg!=NULL;frstneg=auxcl1){
							auxcl1=frstneg->next;
							MYFREE(frstneg);
						}
						return(NOMEMORY);
					}

					/* Build demodulation result into auxcl3. The new clause substitutes the */
					/* demodulated clause. The demodulated clause is unlinked from discrimination */
					/* trees and freed. */
					memcpy(&auxcl3->formula[0],&auxcl2->formula[0],jj);
					memcpy(&auxcl3->formula[jj],ptr2,kk);
					memcpy(&auxcl3->formula[jj+kk],ptr5,ll);
					auxcl3->next=auxcl2->next;
					auxcl3->prev=auxcl2->prev;
					if (auxcl2->next!=NULL){
						auxcl2->next->prev=auxcl3;
					}
					if (auxcl2->prev!=NULL){
						auxcl2->prev->next=auxcl3;
					} else {
						if (NEGATED&ptr4[0]){
							frstneg=auxcl3;
						} else {
							frstpos=auxcl3;
						}
					}
					kbset.prstats.fwdemodulations++;
					UpdateParams2(&auxcl3->formula[0]);
					EditTtlgDscTrees(auxcl2,varleafs,DELETETREES);
					MYFREE(auxcl2);

					/* Clause is an (in)equality). */
					ptr1=&auxcl3->formula[0];
					if ((EQUALITY)&ptr1[0]){

						/* Check tautologies due to s!=s contradictions. */
						ptr2=NextItem(ptr1,IMMED);
						if (NEGATED&ptr1[0]){
							if (IsEqual(ptr2,NextItem(ptr2,OVERSUBTERMS),0)){
								MYFREE(swpequ);
								FreeTtlgTreeMemory(varleafs,clause->part2.bin->maxvarnb);
								for (;frstpos!=NULL;frstpos=auxcl1){
									auxcl1=frstpos->next;
									MYFREE(frstpos);
								}
								for (;frstneg!=NULL;frstneg=auxcl1){
									auxcl1=frstneg->next;
									MYFREE(frstneg);
								}
								kbset.prstats.semtautologies++;
								kbset.prstats.triveqres++;
								return(1);
							}

						/* Check tautologies due to "distinct_object1"="distinct_object2" contradictions. */
						} else {
							ptr3=NextItem(ptr2,OVERSUBTERMS);
							if ((FUNCTION&ptr2[0])&&(FUNCTION&ptr3[0])){
								rtterm1s=((symbol *)&ptr2[1])->symbol;
								rtterm2s=((symbol *)&ptr3[1])->symbol;
								if ((DISTINCTOBJECT&rtterm1s->type)&&(DISTINCTOBJECT&rtterm2s->type)&&(rtterm1s!=rtterm2s)){
									MYFREE(swpequ);
									FreeTtlgTreeMemory(varleafs,clause->part2.bin->maxvarnb);
									for (;frstpos!=NULL;frstpos=auxcl1){
										auxcl1=frstpos->next;
										MYFREE(frstpos);
									}
									for (;frstneg!=NULL;frstneg=auxcl1){
										auxcl1=frstneg->next;
										MYFREE(frstneg);
									}
									kbset.prstats.semtautologies++;
									kbset.prstats.triveqres++;
									return(1);
								}
							}
						}
					}

					/* Check resolution related tautologies. The pointer ptr1 must */
					/* point to the formula to be resolved and ptr2 must point */
					/* to the first root term if the clause is an (in)equality) */
					/* before entering the loop. */
					/* Loop for equality symmetry management. */
					if (EQUALITY&ptr1[0]){
						mm=2;
					} else {
						mm=1;
					}
					for (kk=0;kk<mm;kk++){

						/* This is the second iteration for an (in)equality literal. */
						/* Build the set of root terms in reverse order. */
						if (kk){

							/* Reallocate swpequ buffer if necessary. */
							ll=1+(NextItem(ptr1,OVERSUBTERMS)-ptr1);
							if (ll>size){
								if (NULL==(ptr3=MYREALLOC(swpequ,size=2*ll))){
									MYFREE(swpequ);
									FreeTtlgTreeMemory(varleafs,clause->part2.bin->maxvarnb);
									for (;frstpos!=NULL;frstpos=auxcl1){
										auxcl1=frstpos->next;
										MYFREE(frstpos);
									}
									for (;frstneg!=NULL;frstneg=auxcl1){
										auxcl1=frstneg->next;
										MYFREE(frstneg);
									}
									kbset.prstats.semtautologies++;
									kbset.prstats.resolutions++;
									return(NOMEMORY);
								}
								swpequ=ptr3;
							}

							/* Build equality with swapped root terms. */
							ptr3=NextItem(ptr2,OVERSUBTERMS);
							ll=NextItem(ptr3,OVERSUBTERMS)-ptr3;
							memcpy(swpequ,ptr1,jj=ptr2-ptr1);
							memcpy(&swpequ[jj],ptr3,ll);
							jj+=ll;
							memcpy(&swpequ[jj],ptr2,ll=ptr3-ptr2);
							swpequ[jj+ll]=UNITEND;
							UpdateParams2(swpequ);
							ptr1=swpequ;
						}

						/* Query for a matching resolvent. */
						if (QueryTtlgDscTree(ptr1,&ptr3,&ptr4,
								NEGATED&ptr1[0]?&(((symbol *)&ptr1[1])->symbol->posdsctrttlg):
										&(((symbol *)&ptr1[1])->symbol->negdsctrttlg))){
							MYFREE(swpequ);
							FreeTtlgTreeMemory(varleafs,clause->part2.bin->maxvarnb);
							for (;frstpos!=NULL;frstpos=auxcl1){
								auxcl1=frstpos->next;
								MYFREE(frstpos);
							}
							for (;frstneg!=NULL;frstneg=auxcl1){
								auxcl1=frstneg->next;
								MYFREE(frstneg);
							}
							kbset.prstats.semtautologies++;
							kbset.prstats.resolutions++;
							return(1);
						}
					}

					/* Add new clause to discrimination trees. */
					if (NOMEMORY==EditTtlgDscTrees(auxcl3,varleafs,ADDTREES)){
						MYFREE(swpequ);
						FreeTtlgTreeMemory(varleafs,clause->part2.bin->maxvarnb);
						for (;frstpos!=NULL;frstpos=auxcl1){
							auxcl1=frstpos->next;
							MYFREE(frstpos);
						}
						for (;frstneg!=NULL;frstneg=auxcl1){
							auxcl1=frstneg->next;
							MYFREE(frstneg);
						}
						return(NOMEMORY);
					}

					/* Orient new clause if it is a positive equality. */
					if (EQUALITY==((NEGATED|EQUALITY)&auxcl3->formula[0])){
						ptr1=NextItem(&auxcl3->formula[0],IMMED);
						ptr2=NextItem(ptr1,OVERSUBTERMS);
						ptr1[0]&=(~(MAXIMUM|SELECTED));
						ptr2[0]&=(~(MAXIMUM|SELECTED));
						if (GREATER==(jj=TtlgCompare(ptr1,ptr2))){
							ptr1[0]|=MAXIMUM;
						} else if (jj==LESSER){
							ptr2[0]|=MAXIMUM;
						}
					}

					/* Arrange for a new iteration and leave current loop. */
					ii=0;
					break;
				}
			}
		}

		/* Free memory. */
		MYFREE(swpequ);
		FreeTtlgTreeMemory(varleafs,clause->part2.bin->maxvarnb);
		for (;frstpos!=NULL;frstpos=auxcl1){
			auxcl1=frstpos->next;
			MYFREE(frstpos);
		}
		for (;frstneg!=NULL;frstneg=auxcl1){
			auxcl1=frstneg->next;
			MYFREE(frstneg);
		}

	/* If semantic tautology is not possible then free semlits memory. */
	} else {
		MYFREE(semlits);
	}

	return(0);
} /* IsTautology */
#endif

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Remove literal duplicates from clause)  OCJ
 *
 *    This function removes literal duplicates from a clause in
 *    internal compiled binary format. The simplified clause
 *    keeps the original assertions.
 *
 *    This function detect equalities that are duplicates after
 *    applying equality symmetry.
 *
 *    If the clause is successfully simplified then it is placed in
 *    new allocated storage and flagged as PASSIVE. If buildtxt
 *    parameter is not zero then the original clause is converted
 *    to text form and also placed in a new allocated storage that
 *    is linked to the original cmprefix structure. No other modifications
 *    are done to the cmprefix structure. The original binprefix storage
 *    is freed.
 *
 *    It is the caller responsibility to link and/or unlink the clauses
 *    and set the other prefix fields.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to cmprefix structure of clause to be simplified
 *            by factoring.
 *    pnewcl: Address of pointer to cmprefix structure of new clause
 *            to be allocated if there are literal removals.
 *    buildtxt: If not zero and a simplification results then the
 *              original clause is converted to text form and placed
 *              in a new allocated storage that is linked to the original
 *              cmprefix structure.
 *
 *  RETURNS:
 *
 *    0 -> process was OK and no duplicate literals were found.
 *    1 -> Some duplicate literals were removed.
 *    NOMEMORY -> not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t RemoveDuplicates(cmprefix *clause,cmprefix **pnewcl,int32_t buildtxt){
	auto cmprefix *auxcl;                               /* Pointer to auxiliary clause */
	auto int32_t size;                                  /* Size of auxiliary clause binary buffer in bytes */
	auto uint8_t *ptr1,*ptr2,*ptr6,*ptr7,*ptr8;         /* Auxiliary pointers */
	auto char *ptr10;                                   /* Buffer for clause in text form. */
	auto binprefix *ptr4;                               /* Auxiliary pointer */
	auto int32_t ii,pp;                                 /* Auxiliary */

	#ifdef DEBUGCODE
	/* Debug. */
	ptr10=Decompile(clause->part2.bin);
	MYFREE(ptr10);
	#endif

	/* Allocate storage for auxiliary clause and copy */
	/* clause into the first auxiliary clause. */
	size=clause->part2.bin->size+binpsize;
	if ((auxcl=MYALLOC(sizeof(cmprefix)))==NULL){
		return(NOMEMORY);
	}
	if ((auxcl->part2.bin=MYALLOC(size))==NULL){
		MYFREE(auxcl);
		return(NOMEMORY);
	}
	memcpy(auxcl->part2.bin,clause->part2.bin,size);
	auxcl->part2.bin->clause=auxcl;

	/* Loop through literals in auxiliary clause. */
	for (ptr1=&auxcl->part2.bin->formula[0],pp=0;(*ptr1)!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS)){

		/* Loop through pairs of literals in auxiliary clause. */
		for (ptr2=NextItem(ptr1,OVERSUBTERMS);(*ptr2)!=UNITEND;
				ptr2=(ii?NextItem(ptr2,OVERSUBTERMS):ptr2)){
			ii=1;

			/* Identical literals. Delete the ptr2 literal. */
			if (IsEqual(ptr1,ptr2,0)){
					DeleteLiteral(auxcl->part2.bin,ptr2);
					pp=1; /* Indicate there was a removal. */
					ii=0; /* Keep in the same literal position in next iteration for ptr2. */
			} else if ((EQUALITY&ptr1[0])&&(EQUALITY&ptr2[0])&&((NEGATED&ptr1[0])==((NEGATED&ptr2[0])))){
				ptr6=NextItem(ptr1,IMMED);
				ptr7=NextItem(ptr2,IMMED);
				ptr8=NextItem(ptr7,OVERSUBTERMS);
				if (IsEqual(ptr6,ptr8,0)){
					ptr6=NextItem(ptr6,OVERSUBTERMS);
					if (IsEqual(ptr6,ptr7,0)){
						DeleteLiteral(auxcl->part2.bin,ptr2);
						pp=1; /* Indicate there was a removal. */
						ii=0; /* Keep in the same literal position in next iteration for ptr2. */
					}
				}
			}
		}
	}

	/* There is a removal. */
	if (pp){

		/* Reallocate simplification result to exact size */
		/* and set clause pointer. The original assertions */
		/* are kept. but their asymbol's must be adjusted */
		/* to the new clause. */
		if (NULL==(ptr4=MYREALLOC(auxcl->part2.bin,auxcl->part2.bin->size+binpsize))){
			MYFREE(auxcl->part2.bin);
			MYFREE(auxcl);
			return(NOMEMORY);
		}
		auxcl->part2.bin=ptr4;
		auxcl->part2.bin->oriented=0;
		auxcl->part2.bin->maxvarnb=clause->part2.bin->maxvarnb;
		auxcl->inference=REMOVEDUPS;
		auxcl->parent1=clause;
		auxcl->parent2=NULL;
		auxcl->part2.bin->sinedist=clause->part2.bin->sinedist;
		auxcl->flags=UNPROC;
		if (auxcl->part2.bin->ovly.asserts!=NULL){
			for (ptr1=&auxcl->part2.bin->ovly.asserts[0];ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol))){
				((asymbol *)&ptr1[1])->part2.binp=auxcl->part2.bin;
			}
		}
		*pnewcl=auxcl;

		/* Convert original clause to text form if needed. */
		if (buildtxt){
			if (NULL==(ptr10=Decompile(clause->part2.bin))){
				MYFREE(auxcl->part2.bin);
				MYFREE(auxcl);
				return(NOMEMORY);
			}
			MYFREE(clause->part2.bin);
			clause->part2.text=ptr10;
		}

	/* If there was not a removal then free auxiliary clause storage. */
	} else {
		MYFREE(auxcl->part2.bin);
		MYFREE(auxcl);
	}

	return(pp);
} /* RemoveDuplicates */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Trivial equality resolution inference)  OCJ
 *
 *    This function performs trivial equality resolution inference
 *    simplification of a clause in internal compiled binary format.
 *    The simplified clause keeps the original assertions.
 *
 *    If the clause is successfully simplified then it is placed in
 *    new allocated storage and flagged as PASSIVE. The original clause
 *    is converted to text form and also placed in a new allocated
 *    storage that is linked to the original cmprefix structure.
 *    No other modifications are done to the cmprefix structure.
 *    The original binprefix storage is freed.
 *
 *    It is the caller responsibility to link and/or unlink the clauses
 *    and set the other prefix fields.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to cmprefix structure of clause to be simplified
 *            by equivalent factoring.
 *    pnewcl: Address of pointer to cmprefix structure of new clause
 *            to be allocated if a simplification is performed.
 *
 *  RETURNS:
 *
 *    0 -> process was OK and no simplification was performed.
 *    1 -> Clause has been successfully simplified.
 *    NOMEMORY -> not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t TrivEqResolution(cmprefix *clause,cmprefix **pnewcl){
	auto cmprefix *auxcl;                               /* Pointer to auxiliary clause */
	auto int32_t size;                                  /* Size of auxiliary clause binary buffer in bytes */
	auto hashchain *rtterm1s,*rtterm2s;                 /* Pointers to root term symbols in equalities */
	auto uint8_t *ptr1,*ptr2,*ptr3;                     /* Auxiliary pointers */
	auto char *ptr10;                                   /* Buffer for clause in text form. */
	auto binprefix *ptr4;                               /* Auxiliary pointer */
	auto int32_t ii,pp;                                 /* Auxiliary */

	#ifdef DEBUGCODE
	/* Debug. */
	ptr10=Decompile(clause->part2.bin);
	MYFREE(ptr10);
	#endif

	/* Allocate storage for auxiliary clause and copy */
	/* clause into the first auxiliary clause. */
	size=clause->part2.bin->size+binpsize;
	if ((auxcl=MYALLOC(sizeof(cmprefix)))==NULL){
		return(NOMEMORY);
	}
	if ((auxcl->part2.bin=MYALLOC(size))==NULL){
		MYFREE(auxcl);
		return(NOMEMORY);
	}
	memcpy(auxcl->part2.bin,clause->part2.bin,size);
	auxcl->part2.bin->clause=auxcl;

	/* Loop through literals in auxiliary clause. */
	for (ptr1=&auxcl->part2.bin->formula[0],pp=0;(*ptr1)!=UNITEND;
			ptr1=(ii?NextItem(ptr1,OVERSUBTERMS):ptr1)){
		ii=1;

		/* Literal is an equality. */
		if (EQUALITY&ptr1[0]){
			ptr2=NextItem(ptr1,IMMED);
			ptr3=NextItem(ptr2,OVERSUBTERMS);

			/* Negated equality and root terms are equal. */
			if (NEGATED&ptr1[0]){
				if (IsEqual(ptr2,ptr3,0)){
					DeleteLiteral(auxcl->part2.bin,ptr1);
					pp=1; /* Indicate there was a simplification. */
					ii=0; /* Keep in the same literal position in next iteration for ptr2. */
				}

			/* Positive equality and root terms are unequal "distinct objects". */
			} else if ((FUNCTION&ptr2[0])&&(FUNCTION&ptr3[0])){
				rtterm1s=((symbol *)&ptr2[1])->symbol;
				rtterm2s=((symbol *)&ptr3[1])->symbol;
				if ((rtterm1s!=rtterm2s)&&(DISTINCTOBJECT&rtterm1s->type)
									&&(DISTINCTOBJECT&rtterm2s->type)){
					DeleteLiteral(auxcl->part2.bin,ptr1);
					pp=1; /* Indicate there was a simplification. */
					ii=0; /* Keep in the same literal position in next iteration for ptr2. */
				}
			}
		}
	}

	/* If there is a simplification then set clause pointer */
	/* and convert original clause to text form. The original */
	/* assertions are kept but their asymbol's must be */
	/* adjusted to the new clause. */
	if (pp){
		if (NULL==(ptr4=MYREALLOC(auxcl->part2.bin,auxcl->part2.bin->size+binpsize))){
			MYFREE(auxcl->part2.bin);
			MYFREE(auxcl);
			return(NOMEMORY);
		}
		auxcl->part2.bin=ptr4;
		if (NULL==(ptr10=Decompile(clause->part2.bin))){
			MYFREE(auxcl->part2.bin);
			MYFREE(auxcl);
			return(NOMEMORY);
		}
		auxcl->inference=TRIVEQRES;
		auxcl->parent1=clause;
		auxcl->parent2=NULL;
		auxcl->part2.bin->agedist=clause->part2.bin->agedist;
		auxcl->part2.bin->sinedist=clause->part2.bin->sinedist;
		auxcl->flags=UNPROC;
		auxcl->part2.bin->oriented=0;
		auxcl->part2.bin->maxvarnb=clause->part2.bin->maxvarnb;
		if (auxcl->part2.bin->ovly.asserts!=NULL){
			for (ptr1=&auxcl->part2.bin->ovly.asserts[0];ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol))){
				((asymbol *)&ptr1[1])->part2.binp=auxcl->part2.bin;
			}
		}
		*pnewcl=auxcl;
		MYFREE(clause->part2.bin);
		clause->part2.text=ptr10;

	/* If there was not a removal then free auxiliary clause storage. */
	} else {
		MYFREE(auxcl->part2.bin);
		MYFREE(auxcl);
	}

	return(pp);
} /* TrivEqResolution */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Trivial equality resolution inference)  OCJ
 *
 *    This function performs trivial equality resolution inference
 *    simplification of a clause in internal compiled binary format
 *    for type 8 problems (unit equality).
 *
 *    If the clause is successfully simplified then it is placed in
 *    new allocated storage and flagged as PASSIVE. The original clause
 *    is converted to text form and also placed in a new allocated
 *    storage that is linked to the original cmprefix structure.
 *    No other modifications are done to the cmprefix structure.
 *    The original binprefix storage is freed.
 *
 *    It is the caller responsibility to link and/or unlink the clauses
 *    and set the other prefix fields.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to cmprefix structure of clause to be simplified
 *            by equivalent factoring.
 *    pnewcl: Address of pointer to cmprefix structure of new clause
 *            to be allocated if a simplification is performed.
 *
 *  RETURNS:
 *
 *    0 -> process was OK and no simplification was performed.
 *    1 -> Clause has been successfully simplified.
 *    NOMEMORY -> not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t TrivEqResolutionUEQ(cmprefix *clause,cmprefix **pnewcl){
	auto cmprefix *auxcl;                               /* Pointer to auxiliary clause */
	auto int32_t size;                                  /* Size of auxiliary clause binary buffer in bytes */
	auto hashchain *rtterm1s,*rtterm2s;                 /* Pointers to root term symbols in equalities */
	auto uint8_t *ptr1,*ptr2,*ptr3;                     /* Auxiliary pointers */
	auto char *ptr10;                                   /* Buffer for clause in text form. */
	auto int32_t pp;                                    /* Auxiliary */

	#ifdef DEBUGCODE
	/* Debug. */
	ptr10=Decompile(clause->part2.bin);
	MYFREE(ptr10);
	#endif

	/* Some initializations. */
	ptr1=&clause->part2.bin->formula[0];
	pp=0; /* No simplification by the moment. */

	/* Negated equality and root terms are equal. */
	ptr2=NextItem(ptr1,IMMED);
	ptr3=NextItem(ptr2,OVERSUBTERMS);
	if (NEGATED&ptr1[0]){
		if (IsEqual(ptr2,ptr3,0)){
			pp=1; /* Indicate there was a simplification. */
		}

	/* Positive equality and root terms are unequal "distinct objects". */
	} else if ((FUNCTION&ptr2[0])&&(FUNCTION&ptr3[0])){
		rtterm1s=((symbol *)&ptr2[1])->symbol;
		rtterm2s=((symbol *)&ptr3[1])->symbol;
		if ((rtterm1s!=rtterm2s)&&(DISTINCTOBJECT&rtterm1s->type)
							&&(DISTINCTOBJECT&rtterm2s->type)){
			pp=1; /* Indicate there was a simplification. */
		}
	}

	/* There is a simplification. */
	if (pp){

		/* Allocate and set new empty clause. */
		size=1+binpsize;
		if ((auxcl=MYALLOC(sizeof(cmprefix)))==NULL){
			return(NOMEMORY);
		}
		if ((auxcl->part2.bin=MYALLOC(size))==NULL){
			MYFREE(auxcl);
			return(NOMEMORY);
		}
		auxcl->inference=TRIVEQRES;
		auxcl->parent1=clause;
		auxcl->parent2=NULL;
		auxcl->part2.bin->agedist=clause->part2.bin->agedist;
		auxcl->part2.bin->sinedist=clause->part2.bin->sinedist;
		auxcl->flags=UNPROC;
		auxcl->part2.bin->formula[0]=UNITEND;
		auxcl->part2.bin->clause=auxcl;
		auxcl->part2.bin->ovly.asserts=auxcl->part2.bin->lockasserts=NULL;
		auxcl->part2.bin->literals=0;
		auxcl->part2.bin->size=1;
		*pnewcl=auxcl;

		/* Convert original clause to text form. */
		if (NULL==(ptr10=Decompile(clause->part2.bin))){
			MYFREE(auxcl->part2.bin);
			MYFREE(auxcl);
			return(NOMEMORY);
		}
		MYFREE(clause->part2.bin);
		clause->part2.text=ptr10;
	}

	return(pp);
} /* TrivEqResolutionUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Destructive equality resolution inference)  OCJ
 *
 *    This function performs destructive equality resolution inference
 *    simplification of a clause in internal compiled binary format.
 *    The simplified clause keeps the original assertions. In addition
 *    if ENHANCEDDESTEWRES is defined then for unit clause inequalities
 *    if the root terms unify then a null clause is generated.
 *
 *    If the clause is successfully simplified then it is placed in
 *    new allocated storage and flagged as PASSIVE. The original clause
 *    is converted to text form and also placed in a new allocated
 *    storage that is linked to the original cmprefix structure.
 *    No other modifications are done to the cmprefix structure.
 *    The original binprefix storage is freed.
 *
 *    It is the caller responsibility to link and/or unlink the clauses
 *    and set the other prefix fields.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to cmprefix structure of clause to be simplified
 *            by equivalent factoring.
 *    pnewcl: Address of pointer to cmprefix structure of new clause
 *            to be allocated if a simplification is performed.
 *
 *  RETURNS:
 *
 *    0 -> process was OK and no simplification was performed.
 *    1 -> Clause has been successfully simplified.
 *    NOMEMORY -> not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t DestrEqResolution(cmprefix *clause,cmprefix **pnewcl){
	auto cmprefix *auxcl;                               /* Pointer to auxiliary clause */
	auto int32_t size;                                  /* Size of auxiliary clause binary buffer in bytes */
	auto uint8_t *ptr1,*ptr2,*ptr5;                     /* Auxiliary pointers */
	auto char *ptr10;                                   /* Buffer for clause in text form. */
	auto binprefix *ptr4;                               /* Auxiliary pointer */
	auto subst Subst;                                   /* Auxiliary substitution */
	auto int32_t ii,jj,pp;                              /* Auxiliary */

	#ifdef DEBUGCODE
	/* Debug. */
	ptr10=Decompile(clause->part2.bin);
	MYFREE(ptr10);
	#endif

	/* Allocate storage for auxiliary clause cmprefix structure. */
	if ((auxcl=MYALLOC(sizeof(cmprefix)))==NULL){
		return(NOMEMORY);
	}

	/* Initialize substitution. */
	Subst.buffer=NULL;
	Subst.substsize=0;

	#ifdef ENHANCEDDESTEQRES
	/* Clause is an inequality unit clause. If root terms unify then */
	/* build a null clause and return. */
	ptr1=&clause->part2.bin->formula[0];
	if ((clause->part2.bin->literals==1)&&((NEGATED|EQUALITY)==((NEGATED|EQUALITY)&ptr1[0]))){

		/* Check root terms unification. */
		ptr1=NextItem(ptr1,IMMED);
		ptr2=NextItem(ptr1,OVERSUBTERMS);
		ii=Unify(ptr1,ptr2,&Subst,INITSUBST);
		MYFREE(Subst.buffer);
		switch (ii){

			/* Not enough memory. */
			case NOMEMORY:
				MYFREE(auxcl);
				return(NOMEMORY);
				break;

			/* Root terms unify. */
			case 1:

				/* Allocate new clause binprefix structure in auxiliary clause */
				/* and copy binprefix structure from original clause. The original */
				/* assertions are kept but their asymbol's must be adjusted to the */
				/* new clause. */
				if ((auxcl->part2.bin=MYALLOC(1+binpsize))==NULL){
					MYFREE(auxcl);
					return(NOMEMORY);
				}
				memcpy(auxcl->part2.bin,clause->part2.bin,binpsize);
				auxcl->part2.bin->formula[0]=UNITEND;
				auxcl->part2.bin->oriented=0;
				auxcl->part2.bin->maxvarnb=clause->part2.bin->maxvarnb;
				auxcl->inference=DESTEQRES;
				auxcl->parent1=clause;
				auxcl->parent2=NULL;
				auxcl->part2.bin->agedist=clause->part2.bin->agedist;
				auxcl->part2.bin->sinedist=clause->part2.bin->sinedist;
				auxcl->flags=UNPROC;
				if (auxcl->part2.bin->ovly.asserts!=NULL){
					for (ptr1=&auxcl->part2.bin->ovly.asserts[0];ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol))){
						((asymbol *)&ptr1[1])->part2.binp=auxcl->part2.bin;
					}
				}
				*pnewcl=auxcl;

				/* Convert original clause to text form and return clause simplified. */
				if (NULL==(ptr10=Decompile(clause->part2.bin))){
					MYFREE(auxcl->part2.bin);
					MYFREE(auxcl);
					return(NOMEMORY);
				}
				MYFREE(clause->part2.bin);
				clause->part2.text=ptr10;
				return(1);
				break;

			/* Root terms don't unify, return clause not simplified. */
			case 0:
				MYFREE(auxcl);
				return(0);
				break;
		}
	}
	#endif

	/* Allocate storage for auxiliary clause binprefix structure */
	/* and copy clause into the first auxiliary clause. */
	size=clause->part2.bin->size+binpsize;
	if ((auxcl->part2.bin=MYALLOC(size))==NULL){
		MYFREE(auxcl);
		return(NOMEMORY);
	}
	memcpy(auxcl->part2.bin,clause->part2.bin,size);
	auxcl->part2.bin->clause=auxcl;

	/* Loop through literals in auxiliary clause. */
	for (ptr1=&auxcl->part2.bin->formula[0],pp=jj=0;(*ptr1)!=UNITEND;
			ptr1=(ii?NextItem(ptr1,OVERSUBTERMS):ptr1),jj=(ii?jj+1:jj)){
		ii=1;

		/* Literal is a negated equality. */
		if ((NEGATED|EQUALITY)==((NEGATED|EQUALITY)&ptr1[0])){

			/* Check if one of the root equality terms is a variable. */
			/* If completeness is enabled then both root equality */
			/* root terms must be variables. */
			ptr2=NextItem(ptr1,IMMED);
			if ((DEMODCPL1!=kbset.opts.demodulation)&&(DEMODCPL2!=kbset.opts.demodulation)
					&&(DEMODCPL2ORNT!=kbset.opts.demodulation)&&(DEMODCPL2ORNT!=kbset.opts.demodulation)){
				if (VARIABLE&ptr2[0]){
					ptr5=NextItem(ptr2,OVERSUBTERMS);
				} else {
					ptr2=NextItem(ptr5=ptr2,OVERSUBTERMS);
					if (0==(VARIABLE&ptr2[0])){
						ptr5=NULL;
					}
				}
			} else {
				ptr5=NextItem(ptr2,OVERSUBTERMS);
				if ((0==(VARIABLE&ptr2[0]))||(0==(VARIABLE&ptr5[0]))){
					ptr5=NULL;
				}
			}

			/* One of the root equality terms is a variable. */
			if (ptr5!=NULL){

				/* Empty substitution. */
				if (Subst.buffer!=NULL){
					Subst.substsize=1;
					Subst.buffer[0]=UNITEND;
				}

				/* Check unification and build substitution. */
				switch (UnifyVar(ptr2,ptr5,&Subst,0)){

					/* Not enough memory. */
					case NOMEMORY:
						MYFREE(Subst.buffer);
						if (auxcl->part2.bin->ovly.asserts!=NULL){
							MYFREE(auxcl->part2.bin->ovly.asserts);
						}
						MYFREE(auxcl->part2.bin);
						MYFREE(auxcl);
						return(NOMEMORY);
						break;

					/* Unification is possible and we have a substitution. */
					case 1:

						/* Delete equality. */
						DeleteLiteral(auxcl->part2.bin,ptr1);

						/* Apply substitution. */
						if (NOMEMORY==ApplySubst2Clause(auxcl,&size,&Subst,0)){
							MYFREE(Subst.buffer);
							if (auxcl->part2.bin->ovly.asserts!=NULL){
								MYFREE(auxcl->part2.bin->ovly.asserts);
							}
							MYFREE(auxcl->part2.bin);
							MYFREE(auxcl);
							return(NOMEMORY);
						}

						/* Update ptr1. This is necessary because auxiliary clause */
						/* binprefix may have been reallocated by ApplySubst2Clause() */
						/* and/or the positions of the literals may have changed by */
						/* substitution application. */
						ptr1=GetLiteralPtr(auxcl,jj);

						/* Indicate there was a simplification and keep in the */
						/* same literal position in next iteration for ptr2. */
						pp=1;
						ii=0;
						break;
				}
			}
		}
	}

	/* Free substitution storage. */
	MYFREE(Subst.buffer);

	/* There is a simplification. */
	if (pp){

		/* Set clause pointer and convert original clause to text form. */
		/* The original assertions are kept but their asymbol's must be */
		/* adjusted to the new clause. */
		if (NULL==(ptr4=MYREALLOC(auxcl->part2.bin,auxcl->part2.bin->size+binpsize))){
			MYFREE(auxcl->part2.bin);
			MYFREE(auxcl);
			return(NOMEMORY);
		}
		auxcl->part2.bin=ptr4;
		ptr4->oriented=0;
		ptr4->maxvarnb=clause->part2.bin->maxvarnb;
		if (NULL==(ptr10=Decompile(clause->part2.bin))){
			MYFREE(auxcl->part2.bin);
			MYFREE(auxcl);
			return(NOMEMORY);
		}
		auxcl->inference=DESTEQRES;
		auxcl->parent1=clause;
		auxcl->parent2=NULL;
		auxcl->part2.bin->agedist=clause->part2.bin->agedist;
		auxcl->part2.bin->sinedist=clause->part2.bin->sinedist;
		auxcl->flags=UNPROC;
		if (auxcl->part2.bin->ovly.asserts!=NULL){
			for (ptr1=&auxcl->part2.bin->ovly.asserts[0];ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol))){
				((asymbol *)&ptr1[1])->part2.binp=auxcl->part2.bin;
			}
		}
		*pnewcl=auxcl;
		MYFREE(clause->part2.bin);
		clause->part2.text=ptr10;

		/* Debug. */
		#ifdef DEBUGCODE
		ptr10=Decompile(auxcl->part2.bin);
		#ifdef VERBOSE
		if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
			printf("Destructive equality resolution simplification result [%ld]:\n%s\n",kbset.nformulas,ptr10);
		}
		#endif
		MYFREE(ptr10);
		#endif

	/* If there was not a removal then free auxiliary clause storage. */
	} else {
		MYFREE(auxcl->part2.bin);
		MYFREE(auxcl);
	}

	return(pp);
} /* DestrEqResolution */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Destructive equality resolution inference for unit clause)  OCJ
 *
 *    This function performs destructive equality resolution inference
 *    simplification of a clause in internal compiled binary format
 *    for type 8 problems (unit clause). In addition if ENHANCEDDESTEWRES
 *    is defined then for unit clause inequalities if the root terms unify
 *    then a null clause is generated.
 *
 *    If the clause is successfully simplified then it is placed in
 *    new allocated storage and flagged as PASSIVE. The original clause
 *    is converted to text form and also placed in a new allocated
 *    storage that is linked to the original cmprefix structure.
 *    No other modifications are done to the cmprefix structure.
 *    The original binprefix storage is freed.
 *
 *    It is the caller responsibility to link and/or unlink the clauses
 *    and set the other prefix fields.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to cmprefix structure of clause to be simplified
 *            by equivalent factoring.
 *    pnewcl: Address of pointer to cmprefix structure of new clause
 *            to be allocated if a simplification is performed.
 *
 *  RETURNS:
 *
 *    0 -> process was OK and no simplification was performed.
 *    1 -> Clause has been successfully simplified.
 *    NOMEMORY -> not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t DestrEqResolutionUEQ(cmprefix *clause,cmprefix **pnewcl){
	auto cmprefix *auxcl;                               /* Pointer to auxiliary clause */
	auto uint8_t *ptr1,*ptr2;                           /* Auxiliary pointers */
	auto char *ptr10;                                   /* Buffer for clause in text form. */
	auto subst Subst;                                   /* Auxiliary substitution */
	auto int32_t ii;                                    /* Auxiliary */

	#ifdef DEBUGCODE
	/* Debug. */
	ptr10=Decompile(clause->part2.bin);
	MYFREE(ptr10);
	#endif

	/* Initialize substitution. */
	Subst.buffer=NULL;
	Subst.substsize=0;

	/* Clause is an inequality unit clause. If root terms unify then */
	/* build a null clause and return. */
	ptr1=&clause->part2.bin->formula[0];
	if ((NEGATED|EQUALITY)==((NEGATED|EQUALITY)&ptr1[0])){

		/* Check root terms unification. */
		ptr1=NextItem(ptr1,IMMED);
		ptr2=NextItem(ptr1,OVERSUBTERMS);
		ii=Unify(ptr1,ptr2,&Subst,INITSUBST);
		MYFREE(Subst.buffer);
		switch (ii){

			/* Not enough memory. */
			case NOMEMORY:
				return(NOMEMORY);
				break;

			/* Root terms unify. */
			case 1:

				/* Allocate storage for auxiliary clause cmprefix structure. */
				if ((auxcl=MYALLOC(sizeof(cmprefix)))==NULL){
					return(NOMEMORY);
				}

				/* Allocate new clause binprefix structure in auxiliary clause */
				/* and copy binprefix structure from original clause. */
				if ((auxcl->part2.bin=MYALLOC(1+binpsize))==NULL){
					MYFREE(auxcl);
					return(NOMEMORY);
				}
				memcpy(auxcl->part2.bin,clause->part2.bin,binpsize);
				auxcl->part2.bin->formula[0]=UNITEND;
				auxcl->inference=DESTEQRES;
				auxcl->parent1=clause;
				auxcl->parent2=NULL;
				auxcl->flags=UNPROC;
				auxcl->part2.bin->ovly.asserts=auxcl->part2.bin->lockasserts=NULL;
				*pnewcl=auxcl;

				/* Convert original clause to text form and return clause simplified. */
				if (NULL==(ptr10=Decompile(clause->part2.bin))){
					MYFREE(auxcl->part2.bin);
					MYFREE(auxcl);
					return(NOMEMORY);
				}
				MYFREE(clause->part2.bin);
				clause->part2.text=ptr10;
				return(1);
				break;
		}
	}

	return(0);
} /* DestrEqResolutionUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if clause is subsumed)  OCJ
 *
 *    This function checks if the given clause in compiled or binary
 *    format is subsumed by some clause by calling IsSubsumedByClause()
 *    function for every candidate clause. It must also be specified
 *    the type of subsuming clauses that will be considered, either
 *    active or passive clauses.
 *
 *    Candidate clauses are selected by inspecting every literal in the
 *    argument clause and selecting clauses with a generalization literal.
 *    To prevent selection of the same candidate subsuming clauses by two
 *    different literals once a clause is selected it is signed with an
 *    8 bytes integer. If a clause is signed then it is not selected.
 *
 *    Once all candidate clauses corresponding to a given literal have
 *    been unsuccessfully tested then that literal is excluded from the
 *    IsSubsumedByClause() function call to speed the algorithm.
 *
 *    If the clause is subsumed then its locking assertions are set and
 *    the clause is locked if appropriate. However the clause is not
 *    deleted even if it is not locked. This must be done by the calling
 *    function.
 *
 *    See IsSubsumedByClause() function for details of clause subsumption
 *    condition.
 *
 *    This function must only be called for the working KB. It must NOT
 *    be called for main KB.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in compiled binary form.
 *    type: PASSIVE if passive clauses are to be checked as subsuming
 *          clauses. Active clauses are always checked. Any other
 *          flag is ignored.
 *
 *  RETURNS:
 *
 *    0 -> Clause is not subsumed by any clause of the indicated type
 *         in the indicated KB.
 *    1 -> Clause is subsumed by a clause of the indicated type in the
 *         indicated KB.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t FwSubsuming(cmprefix *clause,int32_t type){
	auto binprefix *binp;                                /* Pointer to candidate binprefix structure */
	auto uint8_t *equality;                              /* Pointer to equality with root terms in reverse order */
	auto uint8_t *eqterms;                               /* Pointer to equality root terms in reverse order */
	auto uint8_t *queryitm;                              /* Pointer to QueryDscTree() query item */
	auto int32_t size;                                   /* Size of eqterms buffer */
	auto subst Subst;                                    /* Substitution for calls to IsSubsumedByClause() */
	auto uint64_t signature;                             /* KB signature. */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5;          /* Auxiliary pointers */
	auto int32_t ii,jj,kk,mm,pp,qq,rr,ss,nn;             /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	#endif

	/* Remember KB signature and update it. */
	signature=kbset.signature;
	kbset.signature++;

	/* Initialize substitution for IsSubsumedByClause() calls. */
	Subst.buffer=NULL;
	Subst.substsize=0;

	/* Loop through clause literals. */
	eqterms=equality=NULL;
	size=0;
	for (ptr1=&clause->part2.bin->formula[0],mm=0;ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS),mm++){

		/* Loop for equality symmetry management. */
		ptr4=NextItem(ptr1,IMMED);
		pp=(ptr1[0]&EQUALITY?2:1);
		for (qq=0;qq<pp;qq++){

			/* This is the first iteration. */
			if (qq==0){
				queryitm=ptr1;

			/* This is the second iteration for an equality literal. */
			/* Build the set of root terms in reverse order. */
			} else {
				ptr3=NextItem(ptr4,OVERSUBTERMS);
				rr=ptr3-ptr4;
				ss=NextItem(ptr3,OVERSUBTERMS)-ptr3;
				if (size==0){
					if (NULL==(equality=MYALLOC(size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
						MYFREE(Subst.buffer);
						return(NOMEMORY);
					}
					eqterms=&equality[1+sizeof(symbol)];
				} else if (size<(2+sizeof(symbol)+rr+ss)){
					if (NULL==(ptr5=MYREALLOC(equality,size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
						MYFREE(Subst.buffer);
						MYFREE(equality);
						return(NOMEMORY);
					}
					equality=ptr5;
					eqterms=&equality[1+sizeof(symbol)];
				}
				memcpy(equality,ptr1,1+sizeof(symbol));
				memcpy(eqterms,ptr3,ss);
				memcpy(&eqterms[ss],ptr4,rr);
				eqterms[rr+ss]=UNITEND;
				queryitm=equality;
			}

			/* Loop through ACTIVE and PASSIVE clauses. ACTIVE clauses have */
			/* selected and non selected literals. */
			for (kk=ACTIVE;kk!=-1;
					kk=(kk==ACTIVE?ACTIVE|SELECTED:(kk==PASSIVE?-1:(type&PASSIVE?PASSIVE:-1)))){

				/* Get pointer to top tree node. */
				switch (kk){
					case ACTIVE:
						if (ptr1[0]&NEGATED){
							ptr3=(((symbol *)&ptr1[1])->symbol)->discr[3];
						} else {
							ptr3=(((symbol *)&ptr1[1])->symbol)->discr[2];
						}
						break;
					case PASSIVE:
						if (ptr1[0]&NEGATED){
							ptr3=(((symbol *)&ptr1[1])->symbol)->discr[5];
						} else {
							ptr3=(((symbol *)&ptr1[1])->symbol)->discr[4];
						}
						break;
					default:
						if (ptr1[0]&NEGATED){
							ptr3=(((symbol *)&ptr1[1])->symbol)->discr[1];
						} else {
							ptr3=(((symbol *)&ptr1[1])->symbol)->discr[0];
						}
						break;
				}

				/* Loop through generalization candidates. */
				nn=0;
				do {

					/* Check timeout and solution by other process. */
					if ((procctl->status==TIMEOUT)||(procctl->status==UNSATISFIABLE)
							||(procctl->status==SATISFIABLE)){
						MYFREE(Subst.buffer);
						MYFREE(equality);
						return(TIMEOUT);
					}
					if (procctl->status==NOMEMORY){
						MYFREE(Subst.buffer);
						MYFREE(equality);
						return(procctl->status);
					}

					/* Get a candidate subsuming literal. */
					if (0!=(ii=QueryGenDscTree(queryitm,&ptr2,ptr3,nn))){

						/* Get pointer to candidate binprefix structure. */
						nn=1;
						binp=(binprefix *)(ptr2-(binpsize+((symbol *)&ptr2[1])->offset));

						/* Debug. */
						#ifdef DEBUGCODE
						ptr10=Decompile(binp);
						MYFREE(ptr10);
						#endif

						/* Check if candidate clause has been already processed. */
						if (binp->signature!=signature){

							/* Check subsumption. */
							switch (jj=IsSubsumedByClause(ptr1,clause->part2.bin->literals-mm,
									binp,&Subst,ptr2,ptr1,NULL)){

								/* No memory. */
								case NOMEMORY:
									MYFREE(Subst.buffer);
									MYFREE(equality);
									return(NOMEMORY);
									break;

								/* Clause is not subsumed. Sign candidate to prevent checking */
								/* it again except if return code is 2 because in this case */
								/* the candidate subsuming clause must not be discarded. */
								case 0:
									binp->signature=signature;
								/* No break. */
								case 2:
									break;

								/* Clause is subsumed. */
								default:

									/* Get locking assertions. */
									clause->flags&=~(TEXTFORMULA|UNPROC|PASSIVE|ACTIVE|LOCKED);
									if (NOMEMORY==GetLocksAsserts(binp,clause->part2.bin,NULL)){
										MYFREE(Subst.buffer);
										MYFREE(equality);
										return(NOMEMORY);
									}

									/* Debug. */
									#ifdef DEBUGCODE
									ptr10=Decompile(clause->part2.bin);
									#ifdef VERBOSE
									if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
										printf("Clause [%ld] forward subsumed:\n%s\n",clause->number,ptr10);
										MYFREE(ptr10);
										ptr10=Decompile(binp);
										printf("By clause [%ld]:\n%s\n",binp->clause->number,ptr10);
									}
									#endif
									MYFREE(ptr10);
									#endif

									/* Free memory and return. */
									MYFREE(Subst.buffer);
									MYFREE(equality);
									return(jj);
									break;
							}
						}
					}
				} while(ii);
			}
		}
	}

	/* Free memory and return clause not subsumed. */
	MYFREE(Subst.buffer);
	MYFREE(equality);
	return(0);
} /* FwSubsuming */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if clause is subsumed for unit clause)  OCJ
 *
 *    This function checks if the given clause in compiled or binary
 *    format is subsumed by some clause by calling IsSubsumedByClause()
 *    function for every candidate clause for type 8 problems (unit
 *    equality). It must also be specified the type of subsuming clauses
 *    that will be considered, either active or passive clauses.
 *
 *    Candidate clauses are selected by inspecting every literal in the
 *    argument clause and selecting clauses with a generalization literal.
 *    To prevent selection of the same candidate subsuming clauses by two
 *    different literals once a clause is selected it is signed with an
 *    8 bytes integer. If a clause is signed then it is not selected.
 *
 *    Once all candidate clauses corresponding to a given literal have
 *    been unsuccessfully tested then that literal is excluded from the
 *    IsSubsumedByClause() function call to speed the algorithm.
 *
 *    If the clause is subsumed then its locking assertions are set and
 *    the clause is locked if appropriate. However the clause is not
 *    deleted even if it is not locked. This must be done by the calling
 *    function.
 *
 *    See IsSubsumedByClause() function for details of clause subsumption
 *    condition.
 *
 *    This function must only be called for the working KB. It must NOT
 *    be called for main KB.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in compiled binary form.
 *    type: PASSIVE if passive clauses are to be checked as subsuming
 *          clauses. Active clauses are always checked. Any other
 *          flag is ignored.
 *
 *  RETURNS:
 *
 *    0 -> Clause is not subsumed by any clause of the indicated type
 *         in the indicated KB.
 *    1 -> Clause is subsumed by a clause of the indicated type in the
 *         indicated KB.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t FwSubsumingUEQ(cmprefix *clause,int32_t type){
	auto binprefix *binp;                                /* Pointer to candidate binprefix structure */
	auto uint8_t *equality;                              /* Pointer to equality with root terms in reverse order */
	auto uint8_t *eqterms;                               /* Pointer to equality root terms in reverse order */
	auto uint8_t *queryitm;                              /* Pointer to QueryDscTree() query item */
	auto int32_t size;                                   /* Size of eqterms buffer */
	auto subst Subst;                                    /* Substitution for calls to IsSubsumedByClause() */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5;          /* Auxiliary pointers */
	auto int32_t ii,jj,kk,pp,qq,rr,ss,nn;                /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	#endif

	/* Initialize substitution. */
	Subst.buffer=NULL;
	Subst.substsize=0;

	/* Other initializations. */
	eqterms=equality=NULL;
	size=0;
	ptr1=&clause->part2.bin->formula[0];
	ptr4=NextItem(ptr1,IMMED);
	pp=(ptr1[0]&EQUALITY?2:1);

	/* Loop for equality symmetry management. */
	for (qq=0;qq<pp;qq++){

		/* This is the first iteration. */
		if (qq==0){
			queryitm=ptr1;

		/* This is the second iteration for an equality literal. */
		/* Build the set of root terms in reverse order. */
		} else {
			ptr3=NextItem(ptr4,OVERSUBTERMS);
			rr=ptr3-ptr4;
			ss=NextItem(ptr3,OVERSUBTERMS)-ptr3;
			if (size==0){
				if (NULL==(equality=MYALLOC(size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
					MYFREE(Subst.buffer);
					return(NOMEMORY);
				}
				eqterms=&equality[1+sizeof(symbol)];
			} else if (size<(2+sizeof(symbol)+rr+ss)){
				if (NULL==(ptr5=MYREALLOC(equality,size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(NOMEMORY);
				}
				equality=ptr5;
				eqterms=&equality[1+sizeof(symbol)];
			}
			memcpy(equality,ptr1,1+sizeof(symbol));
			memcpy(eqterms,ptr3,ss);
			memcpy(&eqterms[ss],ptr4,rr);
			eqterms[rr+ss]=UNITEND;
			queryitm=equality;
		}

		/* Loop through ACTIVE and PASSIVE clauses. ACTIVE clauses have */
		/* selected and non selected literals. */
		for (kk=ACTIVE;kk!=-1;
				kk=(kk==ACTIVE?ACTIVE|SELECTED:(kk==PASSIVE?-1:(type&PASSIVE?PASSIVE:-1)))){

			/* Get pointer to top tree node. */
			switch (kk){
				case ACTIVE:
					if (ptr1[0]&NEGATED){
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[3];
					} else {
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[2];
					}
					break;
				case PASSIVE:
					if (ptr1[0]&NEGATED){
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[5];
					} else {
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[4];
					}
					break;
				default:
					if (ptr1[0]&NEGATED){
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[1];
					} else {
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[0];
					}
					break;
			}

			/* Loop through generalization candidates. */
			nn=0;
			do {

				/* Check timeout and solution by other process. */
				if ((procctl->status==TIMEOUT)||(procctl->status==UNSATISFIABLE)
						||(procctl->status==SATISFIABLE)){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(TIMEOUT);
				}
				if (procctl->status==NOMEMORY){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(procctl->status);
				}

				/* Get a candidate subsuming literal. */
				if (0!=(ii=QueryGenDscTree(queryitm,&ptr2,ptr3,nn))){

					/* Get pointer to candidate binprefix structure. */
					nn=1;
					binp=(binprefix *)(ptr2-(binpsize+((symbol *)&ptr2[1])->offset));

					/* Debug. */
					#ifdef DEBUGCODE
					ptr10=Decompile(binp);
					MYFREE(ptr10);
					#endif

					/* Check subsumption. */
					if (0==(jj=Generalize(&binp->formula[0],ptr1,&Subst,INITSUBST))){
						jj=SymEquGeneralize(&binp->formula[0],ptr1,&Subst,INITSUBST);
					}
					switch (jj){

						/* Not enough memory. */
						case NOMEMORY:
							MYFREE(Subst.buffer);
							MYFREE(equality);
							return(NOMEMORY);
							break;

						/* Clause is subsumed. */
						case 1:

							/* Debug. */
							#ifdef DEBUGCODE
							ptr10=Decompile(clause->part2.bin);
							#ifdef VERBOSE
							if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
								printf("Clause [%ld] forward subsumed:\n%s\n",clause->number,ptr10);
								MYFREE(ptr10);
								ptr10=Decompile(binp);
								printf("By clause [%ld]:\n%s\n",binp->clause->number,ptr10);
							}
							#endif
							MYFREE(ptr10);
							#endif

							/* Free memory and return. */
							MYFREE(Subst.buffer);
							MYFREE(equality);
							return(1);
							break;
					}
				}
			} while(ii);
		}
	}

	/* Free memory and return clause not subsumed. */
	MYFREE(Subst.buffer);
	MYFREE(equality);
	return(0);
} /* FwSubsumingUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform backward subsumption inference)  OCJ
 *
 *    This function checks if the given clause in compiled or binary
 *    format subsumes one or more clauses by calling IsSubsumedByClause()
 *    function for every candidate clause. It must also be specified the
 *    type of subsuming clauses that will be considered, either active
 *    or passive clauses.
 *
 *    Candidate clauses are selected by inspecting the first literal in the
 *    argument clause and selecting clauses with an instantiated literal.
 *
 *    Subsumed clauses are either locked and have their locking assertions
 *    set or they are added to the disposal queue but not removed from
 *    their current queue.
 *
 *    See IsSubsumedByClause() function for details of clause subsumption
 *    condition.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in compiled binary form.
 *    type: PASSIVE if passive clauses are to be checked as subsumed
 *          clauses. Active clauses are always checked. Any other
 *          flag is ignored.
 *
 *  RETURNS:
 *
 *    0 -> Process was ok.
 *    1 -> Process performed and an empty clause without assertions was
 *         generated. This function never returns 1 but this is
 *         documented here as a possible return code from functions
 *         called by BckSimplify() function.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t BckSubsuming(cmprefix *clause,int32_t type){
	auto binprefix *binp;                                /* Pointer to candidate binprefix structure */
	auto uint8_t *equality;                              /* Pointer to equality with root terms in reverse order */
	auto uint8_t *eqterms;                               /* Pointer to equality root terms in reverse order */
	auto uint8_t *queryitm;                              /* Pointer to QueryDscTree() query item */
	auto int32_t size;                                   /* Size of eqterms buffer */
	auto subst Subst;                                    /* Substitution for calls to IsSubsumedByClause() */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5;          /* Auxiliary pointers */
	auto int32_t ii,jj,kk,nn,pp,qq,rr,ss;                /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	#endif

	/* Initialize substitution for IsSubsumedByClause() calls. */
	Subst.buffer=NULL;
	Subst.substsize=0;

	/* Loop for equality symmetry management. */
	ptr1=&clause->part2.bin->formula[0];
	ptr4=NextItem(ptr1,IMMED);
	pp=(ptr1[0]&EQUALITY?2:1);
	eqterms=equality=NULL;
	size=0;
	for (qq=0;qq<pp;qq++){

		/* This is the first iteration. */
		if (qq==0){
			queryitm=ptr1;

		/* This is the second iteration for an equality literal. */
		/* Build the set of root terms in reverse order. */
		} else {
			ptr3=NextItem(ptr4,OVERSUBTERMS);
			rr=ptr3-ptr4;
			ss=NextItem(ptr3,OVERSUBTERMS)-ptr3;
			if (size==0){
				if (NULL==(equality=MYALLOC(size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
					MYFREE(Subst.buffer);
					return(NOMEMORY);
				}
				eqterms=&equality[1+sizeof(symbol)];
			} else if (size<(2+sizeof(symbol)+rr+ss)){
				if (NULL==(ptr5=MYREALLOC(equality,size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(NOMEMORY);
				}
				equality=ptr5;
				eqterms=&equality[1+sizeof(symbol)];
			}
			memcpy(equality,ptr1,1+sizeof(symbol));
			memcpy(eqterms,ptr3,ss);
			memcpy(&eqterms[ss],ptr4,rr);
			eqterms[rr+ss]=UNITEND;
			queryitm=equality;
		}

		/* Loop through ACTIVE and PASSIVE clauses. ACTIVE clauses have */
		/* selected and non selected literals. */
		for (kk=ACTIVE;kk!=-1;
				kk=(kk==ACTIVE?ACTIVE|SELECTED:(kk==PASSIVE?-1:(type&PASSIVE?PASSIVE:-1)))){

			/* Get pointer to top tree node. */
			switch (kk){
				case ACTIVE:
					if (ptr1[0]&NEGATED){
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[3];
					} else {
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[2];
					}
					break;
				case PASSIVE:
					if (ptr1[0]&NEGATED){
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[5];
					} else {
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[4];
					}
					break;
				default:
					if (ptr1[0]&NEGATED){
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[1];
					} else {
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[0];
					}
					break;
			}

			/* Loop through instantiation candidates. */
			nn=0;
			do {

				/* Check timeout and solution by other process. */
				if ((procctl->status==TIMEOUT)||(procctl->status==UNSATISFIABLE)
						||(procctl->status==SATISFIABLE)){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(TIMEOUT);
				}
				if (procctl->status==NOMEMORY){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(procctl->status);
				}

				/* Get a candidate subsumed literal. */
				if (0!=(ii=QueryInsDscTree(queryitm,&ptr2,ptr3,nn))){

					/* Get pointer to candidate binprefix structure. */
					nn=1;
					binp=(binprefix *)(ptr2-(binpsize+((symbol *)&ptr2[1])->offset));

					/* Debug. */
					#ifdef DEBUGCODE
					ptr10=Decompile(binp);
					MYFREE(ptr10);
					#endif

					/* Clause is not in disposal queue therefore */
					/* it has not been already simplified. */
					if (0==(binp->clause->flags&INDISPOSALQUEUE)){

						/* Check subsumption. */
						switch (jj=IsSubsumedByClause(&binp->formula[0],binp->literals,
								clause->part2.bin,&Subst,ptr1,ptr2,NULL)){

							/* Not enough memory. */
							case NOMEMORY:
								MYFREE(Subst.buffer);
								MYFREE(equality);
								return(NOMEMORY);
								break;

							/* Candidate clause is subsumed. */
							case 1:

								/* Get locking assertions. */
								if (NOMEMORY==GetLocksAsserts(clause->part2.bin,binp,NULL)){
									MYFREE(Subst.buffer);
									MYFREE(equality);
									return(NOMEMORY);
								}

								/* Add subsumed clause to disposal queue for later deletion or */
								/* locking. This queue interconnects clauses from other queues. */
								/* The clause must not be removed from any queue to which it is */
								/* linked. */
								binp->clause->flags|=INDISPOSALQUEUE;
								binp->clause->prevdisp=kbset.lastdisp;
								kbset.lastdisp=binp->clause;
								kbset.prstats.bcksubsum++;
								kbset.lrscnt++;

								/* Debug. */
								#ifdef DEBUGCODE
								ptr10=Decompile(binp);
								#ifdef VERBOSE
								if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
									printf("Clause [%ld] backward subsumed:\n%s\n",binp->clause->number,ptr10);
									MYFREE(ptr10);
									ptr10=Decompile(clause->part2.bin);
									printf("By clause [%ld]:\n%s\n",clause->number,ptr10);
								}
								#endif
								MYFREE(ptr10);
								#endif
								break;
						}
					}
				}
			} while(ii);
		}
	}

	/* Free memory and return. */
	MYFREE(Subst.buffer);
	MYFREE(equality);
	return(0);
} /* BckSubsuming */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform backward subsumption inference for unit clause)  OCJ
 *
 *    This function checks if the given clause in compiled or binary
 *    format subsumes one or more clauses by calling IsSubsumedByClause()
 *    function for every candidate clause, for type 8 problems (init clause).
 *    It must also be specified the type of subsuming clauses that will be
 *    considered, either active or passive clauses.
 *
 *    Candidate clauses are selected by inspecting the first literal in the
 *    argument clause and selecting clauses with an instantiated literal.
 *
 *    Subsumed clauses are either locked and have their locking assertions
 *    set or they are added to the disposal queue but not removed from
 *    their current queue.
 *
 *    See IsSubsumedByClause() function for details of clause subsumption
 *    condition.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in compiled binary form.
 *    type: PASSIVE if passive clauses are to be checked as subsumed
 *          clauses. Active clauses are always checked. Any other
 *          flag is ignored.
 *
 *  RETURNS:
 *
 *    0 -> Process was ok.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t BckSubsumingUEQ(cmprefix *clause,int32_t type){
	auto binprefix *binp;                                /* Pointer to candidate binprefix structure */
	auto uint8_t *equality;                              /* Pointer to equality with root terms in reverse order */
	auto uint8_t *eqterms;                               /* Pointer to equality root terms in reverse order */
	auto uint8_t *queryitm;                              /* Pointer to QueryDscTree() query item */
	auto int32_t size;                                   /* Size of eqterms buffer */
	auto subst Subst;                                    /* Substitution for calls to IsSubsumedByClause() */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5;          /* Auxiliary pointers */
	auto int32_t ii,jj,kk,nn,pp,qq,rr,ss;                /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	#endif

	/* Initialize substitution. */
	Subst.buffer=NULL;
	Subst.substsize=0;

	/* Loop for equality symmetry management. */
	ptr1=&clause->part2.bin->formula[0];
	ptr4=NextItem(ptr1,IMMED);
	pp=(ptr1[0]&EQUALITY?2:1);
	eqterms=equality=NULL;
	size=0;
	for (qq=0;qq<pp;qq++){

		/* This is the first iteration. */
		if (qq==0){
			queryitm=ptr1;

		/* This is the second iteration for an equality literal. */
		/* Build the set of root terms in reverse order. */
		} else {
			ptr3=NextItem(ptr4,OVERSUBTERMS);
			rr=ptr3-ptr4;
			ss=NextItem(ptr3,OVERSUBTERMS)-ptr3;
			if (size==0){
				if (NULL==(equality=MYALLOC(size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
					MYFREE(Subst.buffer);
					return(NOMEMORY);
				}
				eqterms=&equality[1+sizeof(symbol)];
			} else if (size<(2+sizeof(symbol)+rr+ss)){
				if (NULL==(ptr5=MYREALLOC(equality,size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(NOMEMORY);
				}
				equality=ptr5;
				eqterms=&equality[1+sizeof(symbol)];
			}
			memcpy(equality,ptr1,1+sizeof(symbol));
			memcpy(eqterms,ptr3,ss);
			memcpy(&eqterms[ss],ptr4,rr);
			eqterms[rr+ss]=UNITEND;
			queryitm=equality;
		}

		/* Loop through ACTIVE and PASSIVE clauses. ACTIVE clauses have */
		/* selected and non selected literals. */
		for (kk=ACTIVE;kk!=-1;
				kk=(kk==ACTIVE?ACTIVE|SELECTED:(kk==PASSIVE?-1:(type&PASSIVE?PASSIVE:-1)))){

			/* Get pointer to top tree node. */
			switch (kk){
				case ACTIVE:
					if (ptr1[0]&NEGATED){
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[3];
					} else {
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[2];
					}
					break;
				case PASSIVE:
					if (ptr1[0]&NEGATED){
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[5];
					} else {
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[4];
					}
					break;
				default:
					if (ptr1[0]&NEGATED){
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[1];
					} else {
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[0];
					}
					break;
			}

			/* Loop through instantiation candidates. */
			nn=0;
			do {

				/* Check timeout and solution by other process. */
				if ((procctl->status==TIMEOUT)||(procctl->status==UNSATISFIABLE)
						||(procctl->status==SATISFIABLE)){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(TIMEOUT);
				}
				if (procctl->status==NOMEMORY){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(procctl->status);
				}

				/* Get a candidate subsumed literal. */
				if (0!=(ii=QueryInsDscTree(queryitm,&ptr2,ptr3,nn))){

					/* Get pointer to candidate binprefix structure. */
					nn=1;
					binp=(binprefix *)(ptr2-(binpsize+((symbol *)&ptr2[1])->offset));

					/* Debug. */
					#ifdef DEBUGCODE
					ptr10=Decompile(binp);
					MYFREE(ptr10);
					#endif

					/* Clause is not in disposal queue therefore */
					/* it has not been already simplified. */
					if (0==(binp->clause->flags&INDISPOSALQUEUE)){

						/* Check subsumption. */
						if (0==(jj=Generalize(&clause->part2.bin->formula[0],&binp->formula[0],&Subst,INITSUBST))){
							jj=SymEquGeneralize(&clause->part2.bin->formula[0],&binp->formula[0],&Subst,INITSUBST);
						}
						switch (jj){

							/* Not enough memory. */
							case NOMEMORY:
								MYFREE(Subst.buffer);
								MYFREE(equality);
								return(NOMEMORY);
								break;

							/* Candidate clause is subsumed. */
							case 1:

								/* Add subsumed clause to disposal queue for later deletion or */
								/* locking. This queue interconnects clauses from other queues. */
								/* The clause must not be removed from any queue to which it is */
								/* linked. */
								binp->clause->flags|=INDISPOSALQUEUE;
								binp->clause->prevdisp=kbset.lastdisp;
								kbset.lastdisp=binp->clause;
								kbset.prstats.bcksubsum++;
								kbset.lrscnt++;

								/* Debug. */
								#ifdef DEBUGCODE
								ptr10=Decompile(binp);
								#ifdef VERBOSE
								if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
									printf("Clause [%ld] backward subsumed:\n%s\n",binp->clause->number,ptr10);
									MYFREE(ptr10);
									ptr10=Decompile(clause->part2.bin);
									printf("By clause [%ld]:\n%s\n",clause->number,ptr10);
								}
								#endif
								MYFREE(ptr10);
								#endif
								break;
						}
					}
				}
			} while(ii);
		}
	}

	/* Free memory and return. */
	MYFREE(Subst.buffer);
	MYFREE(equality);
	return(0);
} /* BckSubsumingUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if clause is subsumed by another clause)  OCJ
 *
 *    This function checks if clause of formula1 in compiled binary format
 *    is subsumed by clause in binp2. The clause in formula1 is subsumed
 *    by the clause in binp2 if there exist a substitution such that applied
 *    to binp2 then a sub-multiset of formula1 is obtained. This means that
 *    clause in binp2 is both more general and simpler than formula1 because
 *    it is true whenever formula in binp2 is true and it has the same or
 *    fewer literals. The substitution is only applied to clause in binp2
 *    so the variables substituted must all belong to clause in binp2.
 *
 *    The substitution may be a complex one formed by a set
 *    of single substitutions.
 *
 *    This function doesn't consider assertions at all. This must be done
 *    by the calling function when applicable.
 *
 *    This partially follows the paper "SAT solving for variants of first order
 *    subsumption" by Robin Coutelier et al.
 *
 *    This algorithm is potentially exponential in execution time. To avoid
 *    hang ups a number of HW instructions limit condition checking is introduced.
 *    In case of this limit is exceeded then it is assumed that the clause is
 *    not subsumed. This will affect a little bit the program performance but
 *    it will prevent hang ups and it preserves completeness. This condition
 *    must not be confused with the HW instruction limit for the strategy
 *    being analyzed.
 *
 *    IMPORTANT: To ensure that this function works properly the candidate
 *    clauses must not have duplicate literals.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    formula1: Pointer to formula in compiled binary form as it is in the
 *              formula field of binprefix structure. This formula will be
 *              checked for being subsumed by binp2. It points to a literal
 *              in the clause that is being checked for being subsumed, but
 *              not necessarily to the first literal. This is useful for
 *              forward subsumption, when the first literals of candidate
 *              subsumed clause have been already checked and found that
 *              they cannot participate in the subsumption.
 *    lits1: Number of literals starting at formula1 pointer.
 *    binp2: Pointer to binprefix structure of clause in compiled binary form.
 *           This clause will be checked for subsuming formula1.
 *    Subst: Pointer to a substitution structure. This structure is
 *           kept between calls to this function in order to save
 *           allocation and freeing operations of substitution buffer.
 *    gcandidate: Pointer to candidate literal to generalize literal pointed
 *                by icandidate parameter.
 *    icandidate: Pointer to candidate literal to be an instance of literal
 *                pointed by gcandidate parameter.
 *    plit: On entry this is either NULL or the address of a pointer to a
 *          literal in binp2. If it is not NULL on entry and subsumption is
 *          successful then on exit the pointer pointed by plit will be set to
 *          the literal in formula1 that is an instance of the literal in binp2.
 *          If subsumption is not successful then the content on exit is
 *          unpredictable.
 *
 *  RETURNS:
 *
 *    0 -> Clause is not subsumed or a function timeout condition occurred.
 *    2 -> This subsumption can be discarded by the moment because gcandidate
 *         literal is not a generalization of icandidate literal. However
 *         the candidate subsuming clause must not be signed as discarded in
 *         FwSubsuming() function because it may subsume with a different
 *         generalization literal. This is only valid if SUBSUMPRUNEPLUS
 *         is defined.
 *    1 -> Clause is subsumed.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t IsSubsumedByClause(uint8_t *formula1,int32_t lits1,binprefix *binp2,
		subst *Subst,uint8_t *gcandidate,uint8_t *icandidate,uint8_t **plit){
	auto subsatcl *satclause;                            /* Auxiliary SAT clause to set plit if required */
	auto subslitrl *literal;                             /* Auxiliary SAT clause literal to set plit if required */
	auto uint64_t hwinstr;                               /* Initial value of HW instructions counter. */
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto int32_t ii;                                     /* Auxiliary */

	/* Check the number of literals. */
	if (binp2->literals>lits1){
		return(0);
	}

	/* Check that gcandidate generalizes icandidate. */
	/* In case of a valid generalization if candidate subsuming */
	/* clause has only one literal then return valid subsumption. */
	/* However if the generalization is not valid then the subsumption */
	/* is probably not possible but the candidate simplifying clause */
	/* must not be signed as a discarded candidate (this is only for */
	/* calls from FwSubsuming() function). */
	if (0==(ii=Generalize(gcandidate,icandidate,Subst,INITSUBST))){
		if (gcandidate[0]&EQUALITY){
			ii=SymEquGeneralize(gcandidate,icandidate,Subst,INITSUBST);
		}
	}
	switch (ii){
		case NOMEMORY:
			return(NOMEMORY);
			break;
		#ifdef SUBSUMPRUNEPLUS
		case 0:
			return(2);
			break;
		#endif
		case 1:
			if (binp2->literals==1){
				if ((plit!=NULL)&&(*plit==gcandidate)){
					*plit=icandidate;
				}
				return(1);
			}
			break;
	}

	/* Perform additional pruning for an early return. */
	if (0==PruneSubsumption(formula1,lits1,binp2)){
		return(0);
	}

	/* Setup SAT solver data structures. */
	switch (SubsumSetup(formula1,lits1,binp2,(uint8_t **)plit)){
		case 0:
			return(0);
			break;
		case NOMEMORY:
			return(NOMEMORY);
			break;
	}

	/* Get current value of HW instructions counter. */
	myread(&hwinstr);
	hwinstr+=SUBSUMINSTRLIMIT;

	/* Call SAT solver. */
	switch(SbsSatSolver(hwinstr)){
		case 0:
			FreeSubsumMem();
			return(0);
			break;
		case NOMEMORY:
			FreeSubsumMem();
			return(NOMEMORY);
			break;
	}

	/* Set plit parameter if necessary. */
	if (plit!=NULL){
		satclause=(subsatcl *)*plit;
		for (literal=&satclause->formula[0];0==(literal->flags&POSITIVE);literal++){
		}
		for (ptr1=formula1,ii=0;ii<literal->mainlitnb;ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){
		}
		*plit=ptr1;
	}

	/* Free setup memory and return valid subsumption. */
	FreeSubsumMem();
	return(1);
} /* IsSubsumedByClause */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Forward and backward simplifications)  OCJ
 *
 *    This function performs the forward simplifications of a clause
 *    in the working KB. This function also checks for getting an empty
 *    clause without assertions. If empty clause without assertions is
 *    detected then it is added to the UNPROC queue.
 *
 *    The input clause must be unlinked form the owner KB.
 *
 *    This function just calls the appropriate functions that perform
 *    each simplification inference. Any required memory freeing is
 *    performed by this function or the functions called by it, even
 *    in the case of error and also for the memory allocated to *pclause
 *    by the caller.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    pclause: Address of pointer to cmprefix structure of clause.
 *    type: PASSIVE if passive clauses are to be checked for
 *          simplifications. Active clauses are always checked.
 *          FROMPASSIVE if the clause is coming from PASSIVE queue and
 *          therefore some simplifications must not be performed.
 *
 *  RETURNS:
 *
 *    0 -> Process performed and no empty clause without assertions inferred.
 *    1 -> Process performed and an empty clause without assertions was inferred.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t FwdSimplify(cmprefix **pclause,int32_t type){
	auto int32_t ii;                                     /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;

	/* Debug. */
	ptr10=Decompile((*pclause)->part2.bin);
	#ifdef VERBOSE
	if (active_cores==1){
		printf("===> Starting forward simplifications to clause:\n%s\n",ptr10);
	}
	#endif
	MYFREE(ptr10);
	#endif

	#if DEMODPOSITION == 1
	/* Perform demodulations if there are equalities. */
	/* An initial tautology detection is done before */
	/* the demodulation loop. */
	if (((UNITEQUALITY|NONUNITEQUALITY)&pbmflags)&&(kbset.opts.demodulation!=DEMODOFF)){

		/* Check tautology. */
		switch (IsTautology(*pclause)){
			case NOMEMORY:
				return(NOMEMORY);
				break;
			case 0:
				break;
			default:
				if ((*pclause)->part2.bin->ovly.asserts!=NULL){
					MYFREE((*pclause)->part2.bin->ovly.asserts);
				}
				MYFREE((*pclause)->part2.bin);
				MYFREE(*pclause);
				*pclause=NULL;
				return(0);
				break;
		}

		/* Demodulation loop. */
		for (ii=1;ii!=0;){
			switch (ii=FwdDemodulation(pclause,type)){
				case TIMEOUT:
				case NOMEMORY:
					return(ii);
					break;
				case 1:
					type&=(~FROMPASSIVE);
					break;
				case 2:
					*pclause=NULL;
					return(0);
					break;
			}
		}
	}
	#endif

	/* Perform initial forward simplifications. If there is a NOMEMORY */
	/* error or if the clause is deleted then memory freeing is */
	/* performed by FirstSimplify() function, not here. */
	switch (ii=FirstSimplify(pclause,type)){

		/* Not enough memory or timeout. */
		case NOMEMORY:
		case TIMEOUT:
			return(ii);
			break;

		/* Clause has been deleted or subsumed. */
		case 2:
		case 3:
			*pclause=NULL;
			return(0);
			break;

		/* Clause has been locked. */
		case 4:
			return(0);
			break;
	}

	/* Check empty clause. This can be achieved by trivial equality resolution */
	/* and destructive equality resolution in FirstSimplify() function. */
	if ((*pclause)->part2.bin->formula[0]==UNITEND){

		/* Empty clause has not assertions. */
		if ((*pclause)->part2.bin->ovly.asserts==NULL){
			(*pclause)->flags=UNPROC;
			if (NOMEMORY==AddBinClause2KB(*pclause,&kbset,0)){
				if ((*pclause)->part2.bin->ovly.asserts!=NULL){
					MYFREE((*pclause)->part2.bin->ovly.asserts);
				}
				MYFREE((*pclause)->part2.bin);
				MYFREE(*pclause);
				return(NOMEMORY);
			}
			if (NULL==(kbset.frstprfnode=MYALLOC(sizeof(proofnode)))){
				return(NOMEMORY);
			}
			kbset.frstprfnode->type=ACLAUSE;
			kbset.frstprfnode->ptr.Aclause=*pclause;
			kbset.status=UNSATISFIABLE;
			return(1);
		}

		/* Empty clause has assertions. Return as an empty clause cannot */
		/* be further simplified. */
		return(0);
	}

	#ifdef UNITCLRESOLUTION
	/* Perform unit clause resolution simplification. This is done only */
	/* for unit clauses without assertions. */
	if (((*pclause)->part2.bin->literals==1)&&((*pclause)->part2.bin->ovly.asserts==NULL)){

		/* Perform call and check NOMEMORY and TIMEOUT. */
		switch (ii=UCResolution(*pclause)){
			case NOMEMORY:
			case TIMEOUT:
				return(ii);
				break;
		}

		/* Unit clause resolution was successful. */
		if (ii){
			return(1);
		}
	}
	#endif

	#if DEMODPOSITION == 2
	/* Perform demodulations if there are equalities. */
	if (((UNITEQUALITY|NONUNITEQUALITY)&pbmflags)&&(kbset.opts.demodulation!=DEMODOFF)){
		for (ii=1;ii!=0;){
			switch (ii=FwdDemodulation(pclause,type)){
				case TIMEOUT:
				case NOMEMORY:
					return(ii);
					break;
				case 2:
					*pclause=NULL;
					return(0);
					break;
			}
		}
	}
	#endif

	/* Perform additional forward simplification. If there is a NOMEMORY */
	/* error or if the clause is deleted then memory freeing is */
	/* performed by FwdSimplify() function, not here. */
	switch (ii=SecondSimplify(pclause,type)){
		case NOMEMORY:
		case TIMEOUT:
			return(ii);
			break;
	}

	/* Check empty clause without assertions. This can be achieved by subsumption */
	/* resolution in SecondSimplify() function. */
	if ((*pclause)->part2.bin->formula[0]==UNITEND){

		/* Empty clause has not assertions. */
		if ((*pclause)->part2.bin->ovly.asserts==NULL){
			(*pclause)->flags=UNPROC;
			if (NOMEMORY==AddBinClause2KB(*pclause,&kbset,0)){
				if ((*pclause)->part2.bin->ovly.asserts!=NULL){
					MYFREE((*pclause)->part2.bin->ovly.asserts);
				}
				MYFREE((*pclause)->part2.bin);
				MYFREE(*pclause);
				return(NOMEMORY);
			}
			if (NULL==(kbset.frstprfnode=MYALLOC(sizeof(proofnode)))){
				return(NOMEMORY);
			}
			kbset.frstprfnode->type=ACLAUSE;
			kbset.frstprfnode->ptr.Aclause=*pclause;
			kbset.status=UNSATISFIABLE;
			return(1);
		}

		/* Empty clause has assertions. Return as an empty clause cannot */
		/* be further simplified. */
		return(0);
	}

	#if DEMODPOSITION == 3
	/* Perform demodulations if there are equalities. */
	if (((UNITEQUALITY|NONUNITEQUALITY)&pbmflags)&&(kbset.opts.demodulation!=DEMODOFF)){
		for (ii=1;ii!=0;){
			switch (ii=FwdDemodulation(pclause,type)){
				case TIMEOUT:
				case NOMEMORY:
					return(ii);
					break;
				case 2:
					*pclause=NULL;
					return(0);
					break;
			}
		}
	}
	#endif

	/* Debug. */
	#ifdef DEBUGCODE
	ptr10=Decompile((*pclause)->part2.bin);
	MYFREE(ptr10);
	#endif

	return(0);
} /* FwdSimplify */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Forward and backward simplifications for unit clause)  OCJ
 *
 *    This function performs the forward simplifications of a clause in
 *    the working KB for type 8 problems (unit equality). This function
 *    also checks for getting an empty clause without assertions. If
 *    empty clause without assertions is detected then it is added to
 *    the UNPROC queue.
 *
 *    The input clause must be unlinked form the owner KB.
 *
 *    This function just calls the appropriate functions that perform
 *    each simplification inference. Any required memory freeing is
 *    performed by this function or the functions called by it, even
 *    in the case of error and also for the memory allocated to *pclause
 *    by the caller.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    pclause: Address of pointer to cmprefix structure of clause.
 *    type: PASSIVE if passive clauses are to be checked for
 *          simplifications. Active clauses are always checked.
 *          FROMPASSIVE if the clause is coming from PASSIVE queue and
 *          therefore some simplifications must not be performed.
 *
 *  RETURNS:
 *
 *    0 -> Process performed and no empty clause without assertions inferred.
 *    1 -> Process performed and an empty clause without assertions was inferred.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t FwdSimplifyUEQ(cmprefix **pclause,int32_t type){
	auto int32_t ii;                                     /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;

	/* Debug. */
	ptr10=Decompile((*pclause)->part2.bin);
	#ifdef VERBOSE
	if (active_cores==1){
		printf("===> Starting forward simplifications to clause:\n%s\n",ptr10);
	}
	#endif
	MYFREE(ptr10);
	#endif

	#if DEMODPOSITION == 1
	/* Perform demodulations. An initial tautology detection is done before */
	/* the demodulation loop. */
	if (kbset.opts.demodulation!=DEMODOFF){

		/* Check tautology. */
		switch (IsTautology(*pclause)){
			case NOMEMORY:
				return(NOMEMORY);
				break;
			case 0:
				break;
			default:
				MYFREE((*pclause)->part2.bin);
				MYFREE(*pclause);
				*pclause=NULL;
				return(0);
				break;
		}

		/* Demodulation loop. */
		for (ii=1;ii!=0;){
			switch (ii=FwdDemodulation(pclause,type)){
				case TIMEOUT:
				case NOMEMORY:
					return(ii);
					break;
				case 1:
					type&=(~FROMPASSIVE);
					break;
				case 2:
					*pclause=NULL;
					return(0);
					break;
			}
		}
	}
	#endif

	/* Perform initial forward simplifications. If there is a NOMEMORY */
	/* error or if the clause is deleted then memory freeing is */
	/* performed by FirstSimplify() function, not here. */
	switch (ii=FirstSimplifyUEQ(pclause,type)){

		/* Not enough memory or timeout. */
		case NOMEMORY:
		case TIMEOUT:
			return(ii);
			break;

		/* Clause has been deleted or subsumed. */
		case 2:
		case 3:
			*pclause=NULL;
			return(0);
			break;

		/* Clause has been locked. */
		case 4:
			return(0);
			break;
	}

	/* Check empty clause. This can be achieved by trivial equality resolution */
	/* and destructive equality resolution in FirstSimplify() function. */
	if ((*pclause)->part2.bin->formula[0]==UNITEND){
		(*pclause)->flags=UNPROC;
		if (NOMEMORY==AddBinClause2KBUEQ(*pclause,&kbset,0)){
			MYFREE((*pclause)->part2.bin);
			MYFREE(*pclause);
			return(NOMEMORY);
		}
		if (NULL==(kbset.frstprfnode=MYALLOC(sizeof(proofnode)))){
			return(NOMEMORY);
		}
		kbset.frstprfnode->type=ACLAUSE;
		kbset.frstprfnode->ptr.Aclause=*pclause;
		kbset.status=UNSATISFIABLE;
		return(1);
	}

	#if DEMODPOSITION == 2
	/* Perform demodulations if there are equalities. */
	if (((UNITEQUALITY|NONUNITEQUALITY)&pbmflags)&&(kbset.opts.demodulation!=DEMODOFF)){
		for (ii=1;ii!=0;){
			switch (ii=FwdDemodulationUEQ(pclause,type)){
				case TIMEOUT:
				case NOMEMORY:
					return(ii);
					break;
				case 2:
					*pclause=NULL;
					return(0);
					break;
			}
		}
	}
	#endif

	/* Perform additional forward simplification. If there is a NOMEMORY */
	/* error or if the clause is deleted then memory freeing is */
	/* performed by FwdSimplify() function, not here. */
	switch (ii=SecondSimplifyUEQ(pclause,type)){
		case NOMEMORY:
		case TIMEOUT:
			return(ii);
			break;
	}

	/* Check empty clause. This can be achieved by subsumption resolution */
	/* in SecondSimplify() function. */
	if ((*pclause)->part2.bin->formula[0]==UNITEND){
		(*pclause)->flags=UNPROC;
		if (NOMEMORY==AddBinClause2KBUEQ(*pclause,&kbset,0)){
			MYFREE((*pclause)->part2.bin);
			MYFREE(*pclause);
			return(NOMEMORY);
		}
		if (NULL==(kbset.frstprfnode=MYALLOC(sizeof(proofnode)))){
			return(NOMEMORY);
		}
		kbset.frstprfnode->type=ACLAUSE;
		kbset.frstprfnode->ptr.Aclause=*pclause;
		kbset.status=UNSATISFIABLE;
		return(1);
	}

	#if DEMODPOSITION == 3
	/* Perform demodulations if there are equalities. */
	if (((UNITEQUALITY|NONUNITEQUALITY)&pbmflags)&&(kbset.opts.demodulation!=DEMODOFF)){
		for (ii=1;ii!=0;){
			switch (ii=FwdDemodulationUEQ(pclause,type)){
				case TIMEOUT:
				case NOMEMORY:
					return(ii);
					break;
				case 2:
					*pclause=NULL;
					return(0);
					break;
			}
		}
	}
	#endif

	/* Debug. */
	#ifdef DEBUGCODE
	ptr10=Decompile((*pclause)->part2.bin);
	MYFREE(ptr10);
	#endif

	return(0);
} /* FwdSimplifyUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (First clause simplification)  OCJ
 *
 *    This function performs the following simplifications in a clause:
 *    - Simplifications that detect clause redundancy, leading to
 *      clause deletion: tautology detection and forward subsumption.
 *    - Simplifications that don't involve any other external clause:
 *      remove duplicate literals, trivial equality resolution and
 *      destructive equality resolution.
 *
 *    If the clause is deleted then its storage is freed. The clause
 *    storage is also freed in case of a NOMEMORY error condition.
 *
 *    This function just calls the appropriate functions that perform
 *    each simplification inference. Any required memory freeing is
 *    performed by this function, even in the case of error and also
 *    for the memory allocated to *pclause by the caller.
 *
 *    IMPORTANT: This function must never be called for main KB.
 *
 *
 *  ARGUMENTS:
 *
 *    pclause: Address of pointer to cmprefix structure of clause.
 *    flags: PASSIVE if passive clauses are to be checked for
 *           simplifications. Active clauses are always checked.
 *          FROMPASSIVE if the clause is coming from PASSIVE queue and
 *          therefore some simplifications must not be performed.
 *
 *  RETURNS:
 *
 *    0 -> No simplifications were done.
 *    1 -> Some simplifications were done.
 *    2 -> Clause was deleted because it is a tautology.
 *    3 -> Clause was subsumed.
 *    4 -> Clause was locked because it was subsumed.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t FirstSimplify(cmprefix **pclause,int32_t flags){
	auto cmprefix *smpclauses[5];                        /* Pointers to simplified clauses */
	auto int32_t smpsize;                                /* Number of elements in smpclauses set */
	auto char *ptr10;                                    /* Auxiliary pointer */
	auto int32_t ii,jj;                                  /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr11;                                    /* Auxiliary pointer */
	#endif

	/* Initialize the set of simplified clauses. The last element with */
	/* index [smpsize-1] contains the last result of a simplification */
	/* in binary format. The other elements contain simplified clauses */
	/* in text format. */
	smpsize=1;
	smpclauses[0]=*pclause;

	/* Simplifications done only if clause is not coming from PASSIVE. */
	if (0==(flags&FROMPASSIVE)){

		/* Remove duplicate literals. */
		switch (RemoveDuplicates(*pclause,&smpclauses[1],1)){
			case NOMEMORY:
				if ((*pclause)->part2.bin->ovly.asserts!=NULL){
					MYFREE((*pclause)->part2.bin->ovly.asserts);
				}
				MYFREE((*pclause)->part2.bin);
				MYFREE(*pclause);
				return(NOMEMORY);
				break;
			case 1:
				smpsize=2;
				kbset.prstats.removedups++;
				break;
		}

		/* Debug. */
		#ifdef DEBUGCODE
		ptr11=Decompile(smpclauses[smpsize-1]->part2.bin);
		MYFREE(ptr11);
		#endif

		/* There are not equalities, check if clause is a tautology. */
		/* This is done here only if there are not equalities. Otherwise */
		/* it is done later (see below).In this case IsTautology cannot */
		/* return NOMEMORY. */
		if ((0==((UNITEQUALITY|NONUNITEQUALITY)&pbmflags))||(kbset.opts.demodulation==DEMODOFF)){
			if (IsTautology(smpclauses[smpsize-1])){
				for (ii=0;ii<(smpsize-1);ii++){
					MYFREE(smpclauses[ii]->part2.text);
					MYFREE(smpclauses[ii]);
				}
				if (smpclauses[smpsize-1]->part2.bin->ovly.asserts!=NULL){
					MYFREE(smpclauses[smpsize-1]->part2.bin->ovly.asserts);
				}
				MYFREE(smpclauses[smpsize-1]->part2.bin);
				MYFREE(smpclauses[smpsize-1]);
				return(2);
			}

		/* There are equalities. */
		} else {

			/* Trivial equality resolution. */
			switch (TrivEqResolution(smpclauses[smpsize-1],&smpclauses[smpsize])){

				/* Not enough memory. */
				case NOMEMORY:
					for (ii=0;ii<(smpsize-1);ii++){
						MYFREE(smpclauses[ii]->part2.text);
						MYFREE(smpclauses[ii]);
					}
					if (smpclauses[smpsize-1]->part2.bin->ovly.asserts!=NULL){
						MYFREE(smpclauses[smpsize-1]->part2.bin->ovly.asserts);
					}
					MYFREE(smpclauses[smpsize-1]->part2.bin);
					MYFREE(smpclauses[smpsize-1]);
					return(NOMEMORY);
					break;

				/* Clause was simplified. If the result is an empty clause */
				/* then add text clauses to KB, set pclause to simplified */
				/* clause and return. */
				case 1:
					smpsize++;
					kbset.prstats.triveqres++;
					if (smpclauses[smpsize-1]->part2.bin->formula[0]==UNITEND){
						for (ii=0;ii<(smpsize-1);ii++){
							AddTxtFormula2KB(smpclauses[ii],smpclauses[ii]->inference,&kbset);
						}
						*pclause=smpclauses[smpsize-1];
						return(1);
					}
					break;
			}

			/* Destructive equality resolution. */
			switch (jj=DestrEqResolution(smpclauses[smpsize-1],&smpclauses[smpsize])){

				/* Not enough memory. */
				case NOMEMORY:
					for (ii=0;ii<(smpsize-1);ii++){
						MYFREE(smpclauses[ii]->part2.text);
						MYFREE(smpclauses[ii]);
					}
					if (smpclauses[smpsize-1]->part2.bin->ovly.asserts!=NULL){
						MYFREE(smpclauses[smpsize-1]->part2.bin->ovly.asserts);
					}
					MYFREE(smpclauses[smpsize-1]->part2.bin);
					MYFREE(smpclauses[smpsize-1]);
					return(NOMEMORY);
					break;

				/* Clause was simplified. If the result is an empty clause */
				/* then add text clauses to KB, set pclause to simplified */
				/* clause and return. */
				case 1:
					smpsize++;
					kbset.prstats.desteqres++;
					if (smpclauses[smpsize-1]->part2.bin->formula[0]==UNITEND){
						for (ii=0;ii<(smpsize-1);ii++){
							AddTxtFormula2KB(smpclauses[ii],smpclauses[ii]->inference,&kbset);
						}
						*pclause=smpclauses[smpsize-1];
						return(1);
					}
					break;
			}

			/* If clause was simplified by destructive equality resolution then check */
			/* if clause is a tautology. This is done here because destructive */
			/* equality resolution may produce a tautology. For instance, the clause */
			/* ~X0=X1|~X2=X3|vapp(X0,X2)=vapp(X1,X3) produces the clause */
			/* vapp(X0,X2)=vapp(X0,X2) that is a tautology. */
			if ((jj)||(DEMODPOSITION!=1)){

				/* Remove duplicate literals. This is done here because destructive */
				/* equality resolution may produce duplicate literals. For instance, */
				/* the clause ~X0=X1|~rpoint(X1)|~X2=X3|~rpoint(X3)|vf(X2,X0)=v0|~X2=X0 */
				/* produces the clause ~rpoint(X0)|~rpoint(X0)|vf(X0,X0)=v0 */
				/* with duplicate literals. */
				if (jj){
					switch (RemoveDuplicates(smpclauses[smpsize-1],&smpclauses[smpsize],1)){
						case NOMEMORY:
							for (ii=0;ii<(smpsize-1);ii++){
								MYFREE(smpclauses[ii]->part2.text);
								MYFREE(smpclauses[ii]);
							}
							if (smpclauses[smpsize-1]->part2.bin->ovly.asserts!=NULL){
								MYFREE(smpclauses[smpsize-1]->part2.bin->ovly.asserts);
							}
							MYFREE(smpclauses[smpsize-1]->part2.bin);
							MYFREE(smpclauses[smpsize-1]);
							return(NOMEMORY);
							break;
						case 1:
							smpsize++;
							kbset.prstats.removedups++;
							break;
					}
				}

				/* Check if clause is a tautology. This is done here because destructive */
				/* equality resolution may produce a tautology. For instance, the clause */
				/* ~X0=X1|~X2=X3|vapp(X0,X2)=vapp(X1,X3) produces the clause */
				/* vapp(X0,X2)=vapp(X0,X2) that is a tautology. */
				switch (IsTautology(smpclauses[smpsize-1])){
					case NOMEMORY:
						for (ii=0;ii<(smpsize-1);ii++){
							MYFREE(smpclauses[ii]->part2.text);
							MYFREE(smpclauses[ii]);
						}
						if (smpclauses[smpsize-1]->part2.bin->ovly.asserts!=NULL){
							MYFREE(smpclauses[smpsize-1]->part2.bin->ovly.asserts);
						}
						MYFREE(smpclauses[smpsize-1]->part2.bin);
						MYFREE(smpclauses[smpsize-1]);
						return(NOMEMORY);
						break;
					case 0:
						break;
					default:
						for (ii=0;ii<(smpsize-1);ii++){
							MYFREE(smpclauses[ii]->part2.text);
							MYFREE(smpclauses[ii]);
						}
						if (smpclauses[smpsize-1]->part2.bin->ovly.asserts!=NULL){
							MYFREE(smpclauses[smpsize-1]->part2.bin->ovly.asserts);
						}
						MYFREE(smpclauses[smpsize-1]->part2.bin);
						MYFREE(smpclauses[smpsize-1]);
						return(2);
						break;
				}
			}
		}
	}

	/* Check clause subsumption. */
	switch (jj=FwSubsuming(smpclauses[smpsize-1],flags)){

		/* Not enough memory. */
		case NOMEMORY:
		case TIMEOUT:
			for (ii=0;ii<(smpsize-1);ii++){
				MYFREE(smpclauses[ii]->part2.text);
				MYFREE(smpclauses[ii]);
			}
			if (smpclauses[smpsize-1]->part2.bin->ovly.asserts!=NULL){
				MYFREE(smpclauses[smpsize-1]->part2.bin->ovly.asserts);
			}
			MYFREE(smpclauses[smpsize-1]->part2.bin);
			MYFREE(smpclauses[smpsize-1]);
			return(jj);
			break;

		/* Clause is not subsumed. */
		case 0:

			/* Debug. */
			#ifdef DEBUGCODE
			#ifdef VERBOSE
			if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
				for (ii=1;ii<(smpsize-1);ii++){
					printf("First simplification result:\n%s\n",smpclauses[ii]->part2.text);
				}
			}
			#endif
			if (smpsize>1){
				ptr10=Decompile(smpclauses[smpsize-1]->part2.bin);
				#ifdef VERBOSE
				if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
					printf("First simplification result:\n%s\n",ptr10);
				}
				#endif
				MYFREE(ptr10);
			}
			#endif

			/* Add text clauses to KB and set pclause to simplified clause. */
			for (ii=0;ii<(smpsize-1);ii++){
				AddTxtFormula2KB(smpclauses[ii],smpclauses[ii]->inference,&kbset);
			}
			*pclause=smpclauses[smpsize-1];
			break;

		/* Clause is subsumed. */
		case 1:
			#if defined(DEBUGCODE) && defined(VERBOSE)
			if ((active_cores==1)&&(procctl->status==RUNNING)&&(verbfrom<=kbset.nformulas)
					&&(verbto>=kbset.nformulas)){
				printf("Inferred clause was subsumed\n");
			}
			#endif

			/* Count subsumption. */
			kbset.prstats.fwdsubsum++;

			/* Add text clauses to KB. This is always done because due to AVATAR */
			/* successive lock and unlock cycles a clause can be subsumed after */
			/* generating a simplified successor an so be part of the proof queue. */
			for (ii=0;ii<(smpsize-1);ii++){
				AddTxtFormula2KB(smpclauses[ii],smpclauses[ii]->inference,&kbset);
			}

			/* Clause is not locked. Convert it to text form. This is always done */
			/* because due to AVATAR successive lock and unlock cycles a clause can */
			/* be subsumed after it generates a simplified successor an so be part of */
			/* the proof queue. */
			if (0==(smpclauses[smpsize-1]->flags&LOCKED)){
				if (NULL==(ptr10=Decompile(smpclauses[smpsize-1]->part2.bin))){
					if (smpclauses[smpsize-1]->part2.bin->ovly.asserts!=NULL){
						MYFREE(smpclauses[smpsize-1]->part2.bin->ovly.asserts);
					}
					MYFREE(smpclauses[smpsize-1]->part2.bin);
					MYFREE(smpclauses[smpsize-1]);
					return(NOMEMORY);
				}
				if (smpclauses[smpsize-1]->part2.bin->ovly.asserts!=NULL){
					MYFREE(smpclauses[smpsize-1]->part2.bin->ovly.asserts);
				}
				MYFREE(smpclauses[smpsize-1]->part2.bin);
				smpclauses[smpsize-1]->part2.text=ptr10;
				AddTxtFormula2KB(smpclauses[smpsize-1],smpclauses[smpsize-1]->inference,&kbset);
				return(3);

			/* Clause is locked. */
			} else {

				/* Set pclause to simplified clause. */
				*pclause=smpclauses[smpsize-1];

				/* Add clause to LOCKED queue and return. */
				/* If a NOMEMORY condition arises then delete it. */
				smpclauses[smpsize-1]->flags&=(~(UNPROC|PASSIVE|ACTIVE));
				smpclauses[smpsize-1]->flags|=LOCKED;
				if (NOMEMORY==AddBinClause2KB(smpclauses[smpsize-1],&kbset,0)){
					MYFREE(smpclauses[smpsize-1]->part2.bin->lockasserts);
					if (smpclauses[smpsize-1]->part2.bin->ovly.asserts!=NULL){
						MYFREE(smpclauses[smpsize-1]->part2.bin->ovly.asserts);
					}
					MYFREE(smpclauses[smpsize-1]->part2.bin);
					MYFREE(smpclauses[smpsize-1]);
					return(NOMEMORY);
				}
				return(4);
			}
			break;
	}

	/* Return. */
	if (smpsize>1){
		return(1);
	}
	return(0);
} /* FirstSimplify */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (First clause simplification for unit clause)  OCJ
 *
 *    This function performs the following simplifications in a clause
 *    for type 8 problems (unit equality):
 *    - Simplifications that detect clause redundancy, leading to
 *      clause deletion: tautology detection and forward subsumption.
 *    - Simplifications that don't involve any other external clause:
 *      remove duplicate literals, trivial equality resolution and
 *      destructive equality resolution.
 *
 *    If the clause is deleted then its storage is freed. The clause
 *    storage is also freed in case of a NOMEMORY error condition.
 *
 *    This function just calls the appropriate functions that perform
 *    each simplification inference. Any required memory freeing is
 *    performed by this function, even in the case of error and also
 *    for the memory allocated to *pclause by the caller.
 *
 *    IMPORTANT: This function must never be called for main KB.
 *
 *
 *  ARGUMENTS:
 *
 *    pclause: Address of pointer to cmprefix structure of clause.
 *    flags: PASSIVE if passive clauses are to be checked for
 *           simplifications. Active clauses are always checked.
 *          FROMPASSIVE if the clause is coming from PASSIVE queue and
 *          therefore some simplifications must not be performed.
 *
 *  RETURNS:
 *
 *    0 -> No simplifications were done.
 *    1 -> Some simplifications were done.
 *    2 -> Clause was deleted because it is a tautology.
 *    3 -> Clause was subsumed.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t FirstSimplifyUEQ(cmprefix **pclause,int32_t flags){
	auto cmprefix *smpclauses[5];                        /* Pointers to simplified clauses */
	auto int32_t smpsize;                                /* Number of elements in smpclauses set */
	auto char *ptr10;                                    /* Auxiliary pointer */
	auto int32_t ii,jj;                                  /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr11;                                    /* Auxiliary pointer */
	#endif

	/* Initialize the set of simplified clauses. The last element with */
	/* index [smpsize-1] contains the last result of a simplification */
	/* in binary format. The other elements contain simplified clauses */
	/* in text format. */
	smpsize=1;
	smpclauses[0]=*pclause;

	/* Simplifications done only if clause is not coming from PASSIVE. */
	if (0==(flags&FROMPASSIVE)){

		/* Debug. */
		#ifdef DEBUGCODE
		ptr11=Decompile(smpclauses[smpsize-1]->part2.bin);
		MYFREE(ptr11);
		#endif

		/* There demodulation is disabled check if clause is a tautology. */
		/* This is done here only if there are not equalities. Otherwise */
		/* it is done later (see below).In this case IsTautology cannot */
		/* return NOMEMORY. */
		if (kbset.opts.demodulation==DEMODOFF){
			if (IsTautology(smpclauses[smpsize-1])){
				for (ii=0;ii<(smpsize-1);ii++){
					MYFREE(smpclauses[ii]->part2.text);
					MYFREE(smpclauses[ii]);
				}
				if (smpclauses[smpsize-1]->part2.bin->ovly.asserts!=NULL){
					MYFREE(smpclauses[smpsize-1]->part2.bin->ovly.asserts);
				}
				MYFREE(smpclauses[smpsize-1]->part2.bin);
				MYFREE(smpclauses[smpsize-1]);
				return(2);
			}

		/* Demodulation is enabled. */
		} else {

			/* Trivial equality resolution. */
			switch (TrivEqResolutionUEQ(smpclauses[smpsize-1],&smpclauses[smpsize])){

				/* Not enough memory. */
				case NOMEMORY:
					for (ii=0;ii<(smpsize-1);ii++){
						MYFREE(smpclauses[ii]->part2.text);
						MYFREE(smpclauses[ii]);
					}
					MYFREE(smpclauses[smpsize-1]->part2.bin);
					MYFREE(smpclauses[smpsize-1]);
					return(NOMEMORY);
					break;

				/* Clause was simplified therefore the result is an empty clause. */
				/* Add text clauses to KB, set pclause to simplified clause and return. */
				case 1:
					smpsize++;
					kbset.prstats.triveqres++;
					for (ii=0;ii<(smpsize-1);ii++){
						AddTxtFormula2KB(smpclauses[ii],smpclauses[ii]->inference,&kbset);
					}
					*pclause=smpclauses[smpsize-1];
					return(1);
					break;
			}

			/* Destructive equality resolution. */
			switch (jj=DestrEqResolutionUEQ(smpclauses[smpsize-1],&smpclauses[smpsize])){

				/* Not enough memory. */
				case NOMEMORY:
					for (ii=0;ii<(smpsize-1);ii++){
						MYFREE(smpclauses[ii]->part2.text);
						MYFREE(smpclauses[ii]);
					}
					MYFREE(smpclauses[smpsize-1]->part2.bin);
					MYFREE(smpclauses[smpsize-1]);
					return(NOMEMORY);
					break;

				/* Clause was simplified therefore the result is an empty clause. */
				/* Add text clauses to KB, set pclause to simplified clause and return. */
				case 1:
					smpsize++;
					kbset.prstats.desteqres++;
					for (ii=0;ii<(smpsize-1);ii++){
						AddTxtFormula2KB(smpclauses[ii],smpclauses[ii]->inference,&kbset);
					}
					*pclause=smpclauses[smpsize-1];
					return(1);
					break;
			}

			/* If clause was simplified by destructive equality resolution then check */
			/* if clause is a tautology. This is done here because destructive */
			/* equality resolution may produce a tautology. For instance, the clause */
			/* ~X0=X1|~X2=X3|vapp(X0,X2)=vapp(X1,X3) produces the clause */
			/* vapp(X0,X2)=vapp(X0,X2) that is a tautology. */
			if (DEMODPOSITION!=1){

				/* Check if clause is a tautology. This is done here because destructive */
				/* equality resolution may produce a tautology. For instance, the clause */
				/* ~X0=X1|~X2=X3|vapp(X0,X2)=vapp(X1,X3) produces the clause */
				/* vapp(X0,X2)=vapp(X0,X2) that is a tautology. */
				switch (IsTautology(smpclauses[smpsize-1])){
					case NOMEMORY:
						for (ii=0;ii<(smpsize-1);ii++){
							MYFREE(smpclauses[ii]->part2.text);
							MYFREE(smpclauses[ii]);
						}
						MYFREE(smpclauses[smpsize-1]->part2.bin);
						MYFREE(smpclauses[smpsize-1]);
						return(NOMEMORY);
						break;
					case 0:
						break;
					default:
						for (ii=0;ii<(smpsize-1);ii++){
							MYFREE(smpclauses[ii]->part2.text);
							MYFREE(smpclauses[ii]);
						}
						if (smpclauses[smpsize-1]->part2.bin->ovly.asserts!=NULL){
							MYFREE(smpclauses[smpsize-1]->part2.bin->ovly.asserts);
						}
						MYFREE(smpclauses[smpsize-1]->part2.bin);
						MYFREE(smpclauses[smpsize-1]);
						return(2);
						break;
				}
			}
		}
	}

	/* Check clause subsumption. */
	switch (jj=FwSubsumingUEQ(smpclauses[smpsize-1],flags)){

		/* Not enough memory. */
		case NOMEMORY:
		case TIMEOUT:
			for (ii=0;ii<(smpsize-1);ii++){
				MYFREE(smpclauses[ii]->part2.text);
				MYFREE(smpclauses[ii]);
			}
			MYFREE(smpclauses[smpsize-1]->part2.bin);
			MYFREE(smpclauses[smpsize-1]);
			return(jj);
			break;

		/* Clause is not subsumed. */
		case 0:

			/* Debug. */
			#ifdef DEBUGCODE
			#ifdef VERBOSE
			if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
				for (ii=1;ii<(smpsize-1);ii++){
					printf("First simplification result:\n%s\n",smpclauses[ii]->part2.text);
				}
			}
			#endif
			if (smpsize>1){
				ptr11=Decompile(smpclauses[smpsize-1]->part2.bin);
				#ifdef VERBOSE
				if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
					printf("First simplification result:\n%s\n",ptr11);
				}
				#endif
				MYFREE(ptr11);
			}
			#endif

			/* Add text clauses to KB and set pclause to simplified clause. */
			for (ii=0;ii<(smpsize-1);ii++){
				AddTxtFormula2KB(smpclauses[ii],smpclauses[ii]->inference,&kbset);
			}
			*pclause=smpclauses[smpsize-1];
			break;

		/* Clause is subsumed. */
		case 1:
			#if defined(DEBUGCODE) && defined(VERBOSE)
			if ((active_cores==1)&&(procctl->status==RUNNING)&&(verbfrom<=kbset.nformulas)
					&&(verbto>=kbset.nformulas)){
				printf("Inferred clause was subsumed\n");
			}
			#endif

			/* Count subsumption. */
			kbset.prstats.fwdsubsum++;

			/* Add text clauses to KB. */
			for (ii=0;ii<(smpsize-1);ii++){
				AddTxtFormula2KB(smpclauses[ii],smpclauses[ii]->inference,&kbset);
			}

			/* Convert clause to text form and return. */
			if (NULL==(ptr10=Decompile(smpclauses[smpsize-1]->part2.bin))){
				if (smpclauses[smpsize-1]->part2.bin->ovly.asserts!=NULL){
					MYFREE(smpclauses[smpsize-1]->part2.bin->ovly.asserts);
				}
				MYFREE(smpclauses[smpsize-1]->part2.bin);
				MYFREE(smpclauses[smpsize-1]);
				return(NOMEMORY);
			}
			if (smpclauses[smpsize-1]->part2.bin->ovly.asserts!=NULL){
				MYFREE(smpclauses[smpsize-1]->part2.bin->ovly.asserts);
			}
			MYFREE(smpclauses[smpsize-1]->part2.bin);
			smpclauses[smpsize-1]->part2.text=ptr10;
			AddTxtFormula2KB(smpclauses[smpsize-1],smpclauses[smpsize-1]->inference,&kbset);
			return(3);
			break;
	}

	/* Return. */
	if (smpsize>1){
		return(1);
	}
	return(0);
} /* FirstSimplifyUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Forward clause simplification)  OCJ
 *
 *    This function performs the forward simplifications in a clause
 *    that are not performed by FirstSimplify() function:
 *    - Subsumption resolution
 *    - Other possible simplifications to be added in the future
 *
 *    The clause storage is freed in case of a NOMEMORY error condition.
 *
 *    This function just calls the appropriate functions that perform
 *    each simplification inference. Any required memory freeing is
 *    performed by this function, even in the case of error and also
 *    for the memory allocated to *pclause by the caller.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    pclause: Address of pointer to cmprefix structure of clause.
 *    type: PASSIVE if passive clauses are to be checked for
 *          simplifications. Active clauses are always checked.
 *          Any other flag is ignored.
 *
 *  RETURNS:
 *
 *    0 -> No simplifications were done.
 *    1 -> Simplifications were done.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t SecondSimplify(cmprefix **pclause,int32_t type){
	auto cmprefix *ptr1;                                 /* Auxiliary pointer */
	auto int32_t ii;                                     /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Auxiliary */
	#endif

	/* Debug. */
	#ifdef DEBUGCODE
	ptr10=Decompile((*pclause)->part2.bin);
	MYFREE(ptr10);
	#endif

	/* Perform forward subsumption resolution. */
	switch (ii=FwSubsResltn(*pclause,&ptr1,type)){

		/* Not enough memory or timeout. */
		case NOMEMORY:
		case TIMEOUT:
			if ((*pclause)->part2.bin->ovly.asserts!=NULL){
				MYFREE((*pclause)->part2.bin->ovly.asserts);
			}
			MYFREE((*pclause)->part2.bin);
			MYFREE(*pclause);
			return(ii);
			break;

		/* The clause was simplified. */
		case 1:

			/* If original clause is not locked then add it in text form to KB. */
			if (0==((*pclause)->flags&LOCKED)){
				AddTxtFormula2KB(*pclause,(*pclause)->inference,&kbset);

			/* If original clause is locked then add it to LOCKED queue. */
			} else {
				(*pclause)->flags&=(~(UNPROC|PASSIVE|ACTIVE));
				if (NOMEMORY==AddBinClause2KB(*pclause,&kbset,0)){
					MYFREE((*pclause)->part2.bin->lockasserts);
					if ((*pclause)->part2.bin->ovly.asserts!=NULL){
						MYFREE((*pclause)->part2.bin->ovly.asserts);
					}
					MYFREE((*pclause)->part2.bin);
					MYFREE(*pclause);
					return(NOMEMORY);
				}
			}

			/* Get simplified clause and account for this inference. */
			*pclause=ptr1;
			kbset.prstats.fwsubsresltns++;

			/* If an empty clause has been generated then return as an empty */
			/* clause cannot be further simplified. */
			if (ptr1->part2.bin->formula[0]==UNITEND){
				return(0);
			}
			break;
	}

	return(ii);
} /* SecondSimplify */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Forward clause simplification for unit clause)  OCJ
 *
 *    This function performs the forward simplifications in a clause
 *    that are not performed by FirstSimplify() function for
 *    type 8 problems (unit equality):
 *    - Subsumption resolution
 *    - Other possible simplifications to be added in the future
 *
 *    The clause storage is freed in case of a NOMEMORY error condition.
 *
 *    This function just calls the appropriate functions that perform
 *    each simplification inference. Any required memory freeing is
 *    performed by this function, even in the case of error and also
 *    for the memory allocated to *pclause by the caller.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    pclause: Address of pointer to cmprefix structure of clause.
 *    type: PASSIVE if passive clauses are to be checked for
 *          simplifications. Active clauses are always checked.
 *          Any other flag is ignored.
 *
 *  RETURNS:
 *
 *    0 -> No simplifications were done.
 *    1 -> Simplifications were done.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t SecondSimplifyUEQ(cmprefix **pclause,int32_t type){
	auto cmprefix *ptr1;                                 /* Auxiliary pointer */
	auto int32_t ii;                                     /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Auxiliary */
	#endif

	/* Debug. */
	#ifdef DEBUGCODE
	ptr10=Decompile((*pclause)->part2.bin);
	MYFREE(ptr10);
	#endif

	/* Perform forward subsumption resolution. */
	switch (ii=FwSubsResltnUEQ(*pclause,&ptr1,type)){

		/* Not enough memory or timeout. */
		case NOMEMORY:
		case TIMEOUT:
			MYFREE((*pclause)->part2.bin);
			MYFREE(*pclause);
			return(ii);
			break;

		/* The clause was simplified. */
		case 1:

			/* Add simplified clause in text form to KB. */
			AddTxtFormula2KB(*pclause,(*pclause)->inference,&kbset);

			/* Get simplified clause and account for this inference. */
			*pclause=ptr1;
			kbset.prstats.fwsubsresltns++;

			/* If an empty clause has been generated then return as an empty */
			/* clause cannot be further simplified. */
			if (ptr1->part2.bin->formula[0]==UNITEND){
				return(0);
			}
			break;
	}

	return(ii);
} /* SecondSimplifyUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Backward simplification)  OCJ
 *
 *    This function performs backward simplifications by a clause.
 *
 *    Also, if the function called gets an empty clause without asertions
 *    (as may be the case of subsumption resolution) then it must add
 *    the empty clause to the KB as UNPROC, set the frstprfnode field
 *    of the KB and return with an return code 1 such that this
 *    circumstance is recognized by this function.
 *
 *    This function just calls the appropriate functions that perform
 *    each simplification inference.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to cmprefix structure of clause.
 *    type: PASSIVE if passive clauses are to be checked for
 *          simplifications. Active clauses are always checked.
 *          Any other flag is ignored.
 *
 *  RETURNS:
 *
 *    0 -> Process performed and no empty clause without assertions generated.
 *    1 -> Process performed and an empty clause without assertions was generated.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t BckSimplify(cmprefix *clause,int32_t type){
	auto int32_t ii;                                     /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Auxiliary */
	#endif

	/* Debug. */
	#ifdef DEBUGCODE
	ptr10=Decompile(clause->part2.bin);
	#ifdef VERBOSE
	if (active_cores==1){
		printf("===> Starting backward simplifications by clause:\n%s\n",ptr10);
	}
	#endif
	MYFREE(ptr10);
	#endif

	/* WARNING: Make sure the clause parameters are correctly updated */
	/* as appropriate by the functions that generate them before adding */
	/* them to the KB. */

	/* Perform backward demodulation. */
	if (((UNITEQUALITY|NONUNITEQUALITY)&pbmflags)&&(kbset.opts.demodulation!=DEMODOFF)){
		switch (ii=((UNITEQUALITY|NONUNITEQUALITY)&pbmflags?BckDemodulation(clause,type):0)){

			/* Not enough memory or timeout. */
			case NOMEMORY:
			case TIMEOUT:
				return(ii);
				break;
		}
	}

	/* Perform backward subsumption. */
	switch (ii=BckSubsuming(clause,type)){

		/* Not enough memory or timeout. */
		case NOMEMORY:
		case TIMEOUT:
			return(ii);
			break;
	}

	/* Perform backward subsumption resolution. */
	switch (ii=BkSubsResltn(clause,type)){

		/* Not enough memory, timeout or empty clause without assertions generated. */
		case NOMEMORY:
		case TIMEOUT:
		case 1:
			return(ii);
			break;
	}

	return(0);
} /* BckSimplify */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Backward simplification for unit clause)  OCJ
 *
 *    This function performs backward simplifications by a clause
 *    for type 8 problems (unit equality).
 *
 *
 *    Also, if the function called gets an empty clause without asertions
 *    (as may be the case of subsumption resolution) then it must add
 *    the empty clause to the KB as UNPROC, set the frstprfnode field
 *    of the KB and return with an return code 1 such that this
 *    circumstance is recognized by this function.
 *
 *    This function just calls the appropriate functions that perform
 *    each simplification inference.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to cmprefix structure of clause.
 *    type: PASSIVE if passive clauses are to be checked for
 *          simplifications. Active clauses are always checked.
 *          Any other flag is ignored.
 *
 *  RETURNS:
 *
 *    0 -> Process performed and no empty clause without assertions generated.
 *    1 -> Process performed and an empty clause without assertions was generated.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t BckSimplifyUEQ(cmprefix *clause,int32_t type){
	auto int32_t ii;                                     /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Auxiliary */
	#endif

	/* Debug. */
	#ifdef DEBUGCODE
	ptr10=Decompile(clause->part2.bin);
	#ifdef VERBOSE
	if (active_cores==1){
		printf("===> Starting backward simplifications by clause:\n%s\n",ptr10);
	}
	#endif
	MYFREE(ptr10);
	#endif

	/* WARNING: Make sure the clause parameters are correctly updated */
	/* as appropriate by the functions that generate them before adding */
	/* them to the KB. */

	/* Perform backward demodulation. */
	if (kbset.opts.demodulation!=DEMODOFF){
		switch (ii=((UNITEQUALITY|NONUNITEQUALITY)&pbmflags?BckDemodulation(clause,type):0)){

			/* Not enough memory or timeout. */
			case NOMEMORY:
			case TIMEOUT:
				return(ii);
				break;
		}
	}

	/* Perform backward subsumption. */
	switch (ii=BckSubsumingUEQ(clause,type)){

		/* Not enough memory or timeout. */
		case NOMEMORY:
		case TIMEOUT:
			return(ii);
			break;
	}

	/* Perform backward subsumption resolution. */
	switch (ii=BkSubsResltnUEQ(clause,type)){

		/* Not enough memory, timeout or empty clause without assertions generated. */
		case NOMEMORY:
		case TIMEOUT:
		case 1:
			return(ii);
			break;
	}

	return(0);
} /* BckSimplifyUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Factoring for resolution inferences)  OCJ
 *
 *    This function performs the factoring required for resolution
 *    inferences. It factors the given literal with all unifiable
 *    selected literals in the formula of the binprefix structure.
 *    The given literal must also be in the  formula of the binprefix
 *    structure.
 *
 *    This function allocates memory for the result. It is the caller
 *    responsibility to free that memory.
 *
 *
 *  ARGUMENTS:
 *
 *    binp: Pointer to binprefix structure to be factored.
 *    literal: Pointer to literal in binp formula.
 *    result: Address of pointer where the address of the resulting
 *            binprefix structure will be placed.
 *    factlit: Address of pointer where the address of factored
 *             literal will be placed.
 *    Subst: Pointer to the initial substitution of the resolution inference.
 *           It will be upgraded to a compatible factoring substitution.
 *    flag: If not zero then only the literals following the given literal
 *          are checked for factoring, as it is assumed that the previous
 *          literals have been already checked for resolution and factored
 *          if possible with the given literal. Also it means that the
 *          binp clause is the primary parent and also the primary clause
 *          in the substitution. Therefore if marker is zero it means that
 *          the binp clause is the secondary clause in the substitution.
 *
 *  RETURNS:
 *
 *    0 if no factors were found.
 *    1 if factors were found.
 *    NOMEMORY if not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t ResolFactor(binprefix *binp,uint8_t *literal,binprefix **result,
		uint8_t **factlit,subst *Subst,int32_t flag){
	auto cmprefix auxcl;                                 /* Auxiliary cmprefix structure. */
	auto uint8_t *ptr1,*ptr2,*ptr4;                      /* Auxiliary pointers */
	auto binprefix *ptr3;                                /* Auxiliary pointer */
	auto int32_t ii,jj,kk,nn,ss,rr;                      /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Auxiliary */
	#endif

	/* Allocate storage for result and copy original clause. */
	if (NULL==(auxcl.part2.bin=MYALLOC(binpsize+2*binp->size))){
		return(NOMEMORY);
	}
	memcpy(auxcl.part2.bin,binp,binpsize+binp->size);
	ptr2=&auxcl.part2.bin->formula[0]+(literal-&binp->formula[0]);
	ii=GetLiteralNbr(auxcl.part2.bin,ptr2);

	/* Debug. */
	#ifdef DEBUGCODE
	ptr10=Decompile(auxcl.part2.bin);
	MYFREE(ptr10);
	#endif

	/* Loop through literals. */
	rr=0;
	for (ptr1=(flag?NextItem(ptr2,OVERSUBTERMS):&auxcl.part2.bin->formula[0]),
			ptr4=(flag?NextItem(literal,OVERSUBTERMS):NULL),jj=(flag?ii+1:0);
			ptr1[0]!=UNITEND;ptr1=(kk?ptr1:NextItem(ptr1,OVERSUBTERMS)),
			ptr4=(flag?NextItem(ptr4,OVERSUBTERMS):NULL),jj=(kk?jj:jj+1)){
		kk=0;

		/* Literal is selected and is not the primary literal. */
		if ((ii!=jj)&&(SELECTED&ptr1[0])){

			/* Remember substitution status. */
			if (Subst->buffer!=NULL){
				ss=Subst->substsize;
			} else {
				ss=-1;
			}

			/* Check unification of terms. */
			if (0==(nn=Unify(ptr2,ptr1,Subst,flag?0:ITEM1SEC|ITEM2SEC))){
				if (ptr1[0]&EQUALITY){
					if (ss>0){
						Subst->substsize=ss;
						Subst->buffer[ss-1]=UNITEND;
					} else if (Subst->buffer!=NULL){
						Subst->substsize=1;
						Subst->buffer[0]=UNITEND;
					} else {
						Subst->substsize=0;
					}
					nn=SymEquUnify(ptr2,ptr1,Subst,flag?0:ITEM1SEC|ITEM2SEC);
				}
			}
			switch (nn){

				/* Not enough memory. */
				case NOMEMORY:
					if (auxcl.part2.bin->ovly.asserts!=NULL){
						MYFREE(auxcl.part2.bin->ovly.asserts);
					}
					MYFREE(auxcl.part2.bin);
					return(NOMEMORY);
					break;

				/* Literals unify. */
				case 1:

					/* Delete ptr1 literal. */
					DeleteLiteral(auxcl.part2.bin,ptr1);

					/* Update pointers and literal counters and indicate */
					/* that there was a literal deletion in this iteration. */
					/* Also mark the factored literal if appropriate. */
					if (ii>jj){
						ii--;
						ptr2=GetLiteralPtr(&auxcl,ii);
					}
					kk=rr=1;
					break;

				/* Literals don't unify. Reset substitution to its previous state. */
				default:
					if (ss>0){
						Subst->substsize=ss;
						Subst->buffer[ss-1]=UNITEND;
					} else if (Subst->buffer!=NULL){
						Subst->substsize=1;
						Subst->buffer[0]=UNITEND;
					} else {
						Subst->substsize=0;
					}
					break;
			}
		}
	}

	/* Debug. */
	#ifdef DEBUGCODE
	ptr10=Decompile(auxcl.part2.bin);
	MYFREE(ptr10);
	#endif

	/* Reallocate result to exact size, set factlit and return. */
	if (NULL==(ptr3=MYREALLOC(auxcl.part2.bin,binpsize+auxcl.part2.bin->size))){
		if (auxcl.part2.bin->ovly.asserts!=NULL){
			MYFREE(auxcl.part2.bin->ovly.asserts);
		}
		MYFREE(auxcl.part2.bin);
		return(NOMEMORY);
	}
	*result=ptr3;
	ptr3->oriented=0;
	*factlit=&ptr3->formula[0]+(ptr2-&auxcl.part2.bin->formula[0]);
	return(rr);
} /* ResolFactor */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform forward demodulation)  OCJ
 *
 *    This function tries to simplify the given clause in compiled or binary
 *    format with demodulation inferences by clauses in the indicated KB
 *    by calling Demodulate() function for every simplifier candidate clause
 *    in the requested KB. It must also be specified the type of clauses that
 *    will be considered, either active and/or passive clauses.
 *
 *    Candidate clauses are selected by inspecting every term in the
 *    argument clause and selecting clauses with a generalization term that
 *    is a maximum equality root term in a single literal clause.
 *
 *    The clause storage is freed in case of a NOMEMORY error condition.
 *
 *
 *  ARGUMENTS:
 *
 *    pclause: Address of pointer of clause to be simplified. The pointer to
 *             the simplified clause will be placed here on exit.
 *    type: PASSIVE if passive clauses are to be checked for
 *          simplifications. Active clauses are always checked.
 *          Any other flag is ignored.
 *
 *  RETURNS:
 *
 *    0 -> Clause was not simplified.
 *    1 -> Clause was simplified and the result was NOT a tautology.
 *    2 -> Clause was simplified and the result was a tautology.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t FwdDemodulation(cmprefix **pclause,int32_t type){
	auto cmprefix *clause;                               /* Pointer to clause to be simplified */
	auto binprefix *binp;                                /* Pointer to candidate binprefix structure */
	auto subst Subst;                                    /* Substitution for calls to Demodulate() */
	auto uint8_t *ptr1,*ptr2,*ptr3;                      /* Auxiliary pointers */
	auto binprefix *ptr4;                                /* Auxiliary pointer */
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	auto int32_t ii,kk,nn,oo;                            /* Auxiliary */

	/* Clause is a not oriented positive unit equality and encompassment demodulation is active. */
	/* Pre-compute relevant information to decide if demodulation to this clause is complete. */
	clause=*pclause;
	if ((clause->part2.bin->literals==1)&&(((EQUALITY|NEGATED)&clause->part2.bin->formula[0])==EQUALITY)
			&&(clause->part2.bin->oriented==0)
			&&((kbset.opts.demodulation==DEMODCPL1)||(kbset.opts.demodulation==DEMODCPL1ORNT))){

		/* Set the SELECTED and MAXIMUM flags for equality root terms */
		/* as appropriate. Maximal equality root terms are also */
		/* candidates for demodulating from them as they may become */
		/* maximum after the substitution. If both terms are equal */
		/* then demodulation is not possible as there is no possible */
		/* maximum term even after a substitution. */
		/* Release equality tree data if the root terms ordering */
		/* is completely defined. */
		ptr1=&clause->part2.bin->formula[0];
		ptr2=NextItem(ptr1,IMMED);
		ptr3=NextItem(ptr2,OVERSUBTERMS);
		ptr2[0]&=(~(MAXIMUM|SELECTED));
		ptr3[0]&=(~(MAXIMUM|SELECTED));
		ii=CompareEquTerms(clause->part2.bin,ptr2,ptr3);
		clause->part2.bin->oriented=1;
		switch (ii){
			case NOMEMORY:
				return(NOMEMORY);
				break;
			case EQUAL:
				break;
			case FAILURE:
				ptr2[0]|=SELECTED;
				ptr3[0]|=SELECTED;
				break;
			case GREATER:
				ptr2[0]|=(SELECTED|MAXIMUM);
				break;
			case LESSER:
				ptr3[0]|=(SELECTED|MAXIMUM);
				break;
		}
	}

	/* Initialize clause pointer and substitution for Generalize() calls. */
	Subst.buffer=NULL;
	Subst.substsize=0;

	/* Renumber variables. This is necessary in order for the ordering */
	/* functions to have the correct value if maxvarnb field. */
	/* As the clause has been just inferred it may have shifted variable */
	/* numbers. */
	if (NOMEMORY==RenumberVars(clause->part2.bin)){
		if (clause->part2.bin->ovly.asserts!=NULL){
			MYFREE(clause->part2.bin->ovly.asserts);
		}
		MYFREE(clause->part2.bin);
		MYFREE(clause);
		return(NOMEMORY);
	}

	/* Loop through clause items. */
	for (ptr1=NextItem(&clause->part2.bin->formula[0],IMMED);
			ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){

		/* Iterate if item is not a function. */
		if (0==(FUNCTION&ptr1[0])){
			continue;
		}

		/* Loop through ACTIVE and PASSIVE clauses as appropriate. */
		for (kk=ACTIVE;kk!=-1;kk=((kk==ACTIVE)&&(type&PASSIVE)?PASSIVE:-1)){

			/* Get pointer to top tree node. */
			switch (kk){
				case ACTIVE:
					ptr3=(((symbol *)&ptr1[1])->symbol)->discr[3];
					break;
				case PASSIVE:
					ptr3=(((symbol *)&ptr1[1])->symbol)->discr[4];
					break;
			}

			/* Loop through generalization candidates. */
			nn=0;
			do {

				/* Check timeout and solution by other process. */
				if ((procctl->status==TIMEOUT)||(procctl->status==UNSATISFIABLE)
						||(procctl->status==SATISFIABLE)){
					if (clause->part2.bin->ovly.asserts!=NULL){
						MYFREE(clause->part2.bin->ovly.asserts);
					}
					MYFREE(clause->part2.bin);
					MYFREE(clause);
					if (Subst.buffer!=NULL){
						MYFREE(Subst.buffer);
					}
					return(TIMEOUT);
				}
				if (procctl->status==NOMEMORY){
					if (clause->part2.bin->ovly.asserts!=NULL){
						MYFREE(clause->part2.bin->ovly.asserts);
					}
					MYFREE(clause->part2.bin);
					MYFREE(clause);
					if (Subst.buffer!=NULL){
						MYFREE(Subst.buffer);
					}
					return(procctl->status);
				}

				/* Get a term in a candidate simplifying clause. */
				if (0!=(ii=QueryGenDscTree(ptr1,&ptr2,ptr3,nn))){

					/* Check generalization. */
					nn=1;
					switch (Generalize(ptr2,ptr1,&Subst,INITSUBST)){

						/* Not enough memory. */
						case NOMEMORY:
							if (clause->part2.bin->ovly.asserts!=NULL){
								MYFREE(clause->part2.bin->ovly.asserts);
							}
							MYFREE(clause->part2.bin);
							MYFREE(clause);
							if (Subst.buffer!=NULL){
								MYFREE(Subst.buffer);
							}
							return(NOMEMORY);
							break;

						/* Valid generalization. */
						case 1:

							/* Get pointer to candidate binprefix structure. */
							binp=(binprefix *)(ptr2-(binpsize+((symbol *)&ptr2[1])->offset));

							/* Debug. */
							#ifdef DEBUGCODE
							ptr10=Decompile(binp);
							MYFREE(ptr10);
							#endif

							/* Perform demodulation and check if it is successful. */
							if (1==(oo=Demodulate(clause,ptr1,binp,ptr2,pclause,&Subst))){

								/* Debug. */
								#ifdef DEBUGCODE
								ptr10=Decompile(binp);
								#ifdef VERBOSE
								if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
									printf("Forward demodulated from:\n%s\n",ptr10);
								}
								#endif
								MYFREE(ptr10);
								ptr10=Decompile((*pclause)->part2.bin);
								#ifdef VERBOSE
								if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
									printf("Result [%ld]:\n%s\n",kbset.nformulas,ptr10);
								}
								#endif
								MYFREE(ptr10);
								#endif

								/* Check if result from demodulation is a tautology. */
								switch (IsTautology(*pclause)){

									/* Not enough memory. */
									case NOMEMORY:
										if (clause->part2.bin->ovly.asserts!=NULL){
											MYFREE(clause->part2.bin->ovly.asserts);
										}
										MYFREE(clause->part2.bin);
										MYFREE(clause);
										if (Subst.buffer!=NULL){
											MYFREE(Subst.buffer);
										}
										return(NOMEMORY);
										break;

									/* Result from demodulation is not a tautology. */
									/* Set type of inference and flags in new clause. */
									case 0:
										ptr4=(*pclause)->part2.bin;
										(*pclause)->inference=FWDEMODULATION;
										(*pclause)->flags=UNPROC;
										if (pbmtype==8){
											ptr4->ovly.asserts=ptr4->lockasserts=NULL;
										}
										break;

									/* Clause is a tautology. */
									default:
										oo=2;
										ptr4=NULL;
										MYFREE((*pclause)->part2.bin);
										MYFREE(*pclause);
										break;
								}

								/* Set locks of simplified clause and asserts of demodulation result. */
								if (pbmtype!=8){
									if (NOMEMORY==GetLocksAsserts(binp,clause->part2.bin,ptr4)){
										if (clause->part2.bin->ovly.asserts!=NULL){
											MYFREE(clause->part2.bin->ovly.asserts);
										}
										MYFREE(clause->part2.bin);
										MYFREE(clause);
										if ((*pclause)->part2.bin->ovly.asserts!=NULL){
											MYFREE((*pclause)->part2.bin->ovly.asserts);
										}
										MYFREE((*pclause)->part2.bin);
										MYFREE((*pclause));
										if (Subst.buffer!=NULL){
											MYFREE(Subst.buffer);
										}
										return(NOMEMORY);
									}
								}

								/* If original clause is not locked then convert it to text form. */
								if (0==(LOCKED&clause->flags)){
									if (NULL==(ptr10=Decompile(clause->part2.bin))){
										if (clause->part2.bin->ovly.asserts!=NULL){
											MYFREE(clause->part2.bin->ovly.asserts);
										}
										MYFREE(clause->part2.bin);
										MYFREE(clause);
										if (oo==1){
											if ((*pclause)->part2.bin->ovly.asserts!=NULL){
												MYFREE((*pclause)->part2.bin->ovly.asserts);
											}
											MYFREE((*pclause)->part2.bin);
											MYFREE((*pclause));
										}
										if (Subst.buffer!=NULL){
											MYFREE(Subst.buffer);
										}
										return(NOMEMORY);
									}
									if (clause->part2.bin->ovly.asserts!=NULL){
										MYFREE(clause->part2.bin->ovly.asserts);
									}
									MYFREE(clause->part2.bin);
									clause->part2.text=ptr10;
								}
							}

							/* If clause was simplified or we got a NOMEMORY condition */
							/* then free memory and return. */
							if (oo!=0){
								switch (oo){

									/* Not enough memory or timeout. */
									case NOMEMORY:
									case TIMEOUT:
										if (clause->part2.bin->ovly.asserts!=NULL){
											MYFREE(clause->part2.bin->ovly.asserts);
										}
										MYFREE(clause->part2.bin);
										MYFREE(clause);
										break;

									/* The clause was simplified. */
									case 1:
									case 2:

										/* If original clause is not locked then add it in */
										/* text form to KB. */
										if (0==(clause->flags&LOCKED)){
											AddTxtFormula2KB(clause,clause->inference,&kbset);

										/* If original clause is locked then add it to LOCKED queue. */
										} else {
											clause->flags&=(~(UNPROC|PASSIVE|ACTIVE));
											if (NOMEMORY==(pbmtype==8?AddBinClause2KBUEQ(clause,&kbset,0):AddBinClause2KB(clause,&kbset,0))){
												MYFREE(clause->part2.bin->lockasserts);
												if (clause->part2.bin->ovly.asserts!=NULL){
													MYFREE(clause->part2.bin->ovly.asserts);
												}
												MYFREE(clause->part2.bin);
												MYFREE(clause);
												oo=NOMEMORY;
											}
										}

										/* Account for this inference. */
										kbset.prstats.fwdemodulations++;
										break;
								}
								if (Subst.buffer!=NULL){
									MYFREE(Subst.buffer);
								}
								return(oo);
							}
							break;
					}
				}
			} while(ii);
		}
	}

	/* Restore pointer to original clause, free memory and return clause not simplified. */
	*pclause=clause;
	if (Subst.buffer!=NULL){
		MYFREE(Subst.buffer);
	}
	return(0);
} /* FwdDemodulation */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform backward demodulation)  OCJ
 *
 *    This function tries to simplify the active and/or passive clauses
 *    in the indicated KB by the given clause in compiled or binary format
 *    with demodulation inferences by calling Demodulate() function for
 *    every candidate clause for simplification in the requested KB. It
 *    must also be specified the type of clauses that will be simplified,
 *    either active and/or passive clauses.
 *
 *    The given clause must be suitable for performing demodulation: It must
 *    be a single literal positive equality with a maximum equality root term.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in compiled binary form.
 *    type: PASSIVE if passive clauses are to be checked for
 *          simplifications. Active clauses are always checked.
 *          Any other flag is ignored.
 *
 *  RETURNS:
 *
 *    0 -> Process performed and no empty clause without assertions generated.
 *    1 -> Process performed and an empty clause without assertions was generated.
 *         This function never returns 1 but this is documented here as a possible
 *         return code from functions called by BckSimplify() function.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t BckDemodulation(cmprefix *clause,int32_t type){
	auto cmprefix *newcl;                                /* Pointer to simplified clause cmprefix structure */
	auto binprefix *binp;                                /* Pointer to candidate binprefix structure */
	auto subst Subst;                                    /* Substitution for calls to Demodulate() */
	auto uint8_t *equality;                              /* Pointer to equality literal in clause */
	auto uint8_t *ptr1,*ptr2,*ptr3;                      /* Auxiliary pointers */
	auto int32_t ii,jj,kk,nn,oo;                         /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	#endif

	/* Check if clause is suitable for performing demodulation from it. */
	if ((clause->part2.bin->literals>1)||(EQUALITY!=((EQUALITY|NEGATED)&clause->part2.bin->formula[0]))){
		return(0);
	} else {
		equality=&clause->part2.bin->formula[0];
	}

	/* Get pointers to equality root terms. */
	ptr1=NextItem(equality,IMMED);
	ptr2=NextItem(ptr1,OVERSUBTERMS);

	/* Clause is not oriented. */
	if (clause->part2.bin->oriented==0){

		/* Reset MAXIMUM and SELECTED flags in each of them. */
		ptr1[0]&=(~(MAXIMUM|SELECTED));
		ptr2[0]&=(~(MAXIMUM|SELECTED));

		/* Renumber variables. This is necessary in order for the ordering */
		/* functions to have the correct value in maxvarnb field. */
		/* As the clause may have been just inferred it may have shifted */
		/* variable numbers. */
		if (NOMEMORY==RenumberVars(clause->part2.bin)){
			return(NOMEMORY);
		}

		/* Set the SELECTED and MAXIMUM flags for equality root terms */
		/* as appropriate. Maximal equality root terms are also */
		/* candidates for demodulating from them as they may become */
		/* maximum after the substitution. If both terms are equal */
		/* then demodulation is not possible as there is no possible */
		/* maximum term even after a substitution. */
		ii=CompareEquTerms(clause->part2.bin,ptr1,ptr2);
		clause->part2.bin->oriented=1;
		switch (ii){
			case NOMEMORY:
				return(NOMEMORY);
				break;
			case EQUAL:
				return(0);
				break;
			case FAILURE:
				ptr1[0]|=SELECTED;
				ptr2[0]|=SELECTED;
				if ((kbset.opts.demodulation==DEMODONORNT)||(kbset.opts.demodulation==DEMODCPL1ORNT)
						||(kbset.opts.demodulation==DEMODCPL2ORNT)){
					return(0);
				}
				break;
			case GREATER:
				ptr1[0]|=(SELECTED|MAXIMUM);
				break;
			case LESSER:
				ptr2[0]|=(SELECTED|MAXIMUM);
				break;
		}

	/* Clause is oriented. */
	} else {
		if ((kbset.opts.demodulation==DEMODONORNT)||(kbset.opts.demodulation==DEMODCPL1ORNT)
				||(kbset.opts.demodulation==DEMODCPL2ORNT)){
			if ((0==(MAXIMUM&ptr1[0]))&&(0==(MAXIMUM&ptr2[0]))){
				return(0);
			}
		}
	}

	/* Initialize substitution for Generalize() calls. */
	Subst.buffer=NULL;
	Subst.substsize=0;

	/* Loop through equality root terms. */
	for (ptr1=NextItem(equality,IMMED),jj=0;jj<2;ptr1=NextItem(ptr1,OVERSUBTERMS),jj++){

		/* Iterate if the term is not selected (that is, maximal) */
		/* or it is a variable. Variables are discarded because if */
		/* they are also in the other root term then the variable */
		/* can never be maximum even after a substitution. Also, */
		/* if the variable is not part of the other root term the */
		/* equality makes no sense as it that can be unified with */
		/* everything). */
		if ((0==(SELECTED&ptr1[0]))||(VARIABLE&ptr1[0])){
			continue;
		}

		/* Loop through ACTIVE and PASSIVE clauses as appropriate. */
		for (kk=ACTIVE;kk!=-1;kk=((kk==ACTIVE)&&(type&PASSIVE)?PASSIVE:-1)){

			/* Get pointer to top tree node. */
			switch (kk){
				case ACTIVE:
					ptr3=(((symbol *)&ptr1[1])->symbol)->discr[2];
					break;
				case PASSIVE:
					ptr3=(((symbol *)&ptr1[1])->symbol)->discr[5];
					break;
			}

			/* Loop through candidates that are instances of maximum equality root term. */
			nn=0;
			do {

				/* Check timeout and solution by other process. */
				if ((procctl->status==TIMEOUT)||(procctl->status==UNSATISFIABLE)
						||(procctl->status==SATISFIABLE)){
					if (Subst.buffer!=NULL){
						MYFREE(Subst.buffer);
					}
					return(TIMEOUT);
				}
				if (procctl->status==NOMEMORY){
					if (Subst.buffer!=NULL){
						MYFREE(Subst.buffer);
					}
					return(procctl->status);
				}

				/* Get a term in a candidate clause to be simplified. */
				if (0!=(ii=QueryInsDscTree(ptr1,&ptr2,ptr3,nn))){

					/* Get pointer to binprefix structure of clause to be simplified. */
					nn=1;
					binp=(binprefix *)(ptr2-(binpsize+((symbol *)&ptr2[1])->offset));

					/* Clause is not in disposal queue therefore */
					/* it has not been already simplified. */
					if (0==(binp->clause->flags&INDISPOSALQUEUE)){

						/* Check generalization. */
						switch (Generalize(ptr1,ptr2,&Subst,INITSUBST)){

							/* Not enough memory. */
							case NOMEMORY:
								if (Subst.buffer!=NULL){
									MYFREE(Subst.buffer);
								}
								return(NOMEMORY);
								break;

							/* Valid generalization. */
							case 1:

								/* Debug. */
								#ifdef DEBUGCODE
								ptr10=Decompile(binp);
								MYFREE(ptr10);
								#endif

								/* Perform demodulation. */
								if (NOMEMORY==(oo=Demodulate(binp->clause,ptr2,clause->part2.bin,
										ptr1,&newcl,&Subst))){
									if (Subst.buffer!=NULL){
										MYFREE(Subst.buffer);
									}
									return(NOMEMORY);
								}

								/* Successful simplification. */
								if (oo){

									/* Set locks of simplified clause and asserts of demodulation result. */
									if (pbmtype!=8){
										if (NOMEMORY==GetLocksAsserts(clause->part2.bin,binp,newcl->part2.bin)){
											if (newcl->part2.bin->ovly.asserts!=NULL){
												MYFREE(newcl->part2.bin->ovly.asserts);
											}
											MYFREE(newcl->part2.bin);
											MYFREE(newcl);
											if (Subst.buffer!=NULL){
												MYFREE(Subst.buffer);
											}
											return(NOMEMORY);
										}
									} else {
										newcl->part2.bin->ovly.asserts=newcl->part2.bin->lockasserts=NULL;
									}

									/* Debug. */
									#ifdef DEBUGCODE
									ptr10=Decompile(binp);
									#ifdef VERBOSE
									if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
										printf("Backward demodulating to:\n%s\n",ptr10);
									}
									#endif
									MYFREE(ptr10);
									ptr10=Decompile(newcl->part2.bin);
									#ifdef VERBOSE
									if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
										printf("Result [%ld]:\n%s\n",kbset.nformulas,ptr10);
									}
									#endif
									MYFREE(ptr10);
									#endif

									/* Add original simplified clause to disposal queue. This queue interconnects */
									/* clauses from other queues. The clause must not be removed from any queue */
									/* to which it is linked. */
									binp->clause->flags|=INDISPOSALQUEUE;
									binp->clause->prevdisp=kbset.lastdisp;
									kbset.lastdisp=binp->clause;
									kbset.prstats.bkdemodulations++;

									/* Add new simplified clause to KB as UNPROC. A new number is not assigned. */
									/* It will be assigned when moving this clause to passive, so that it */
									/* has a higher number than its parents. */
									newcl->flags=UNPROC;
									newcl->inference=BKDEMODULATION;
									if (NOMEMORY==(pbmtype==8?AddBinClause2KBUEQ(newcl,&kbset,0):AddBinClause2KB(newcl,&kbset,0))){
										if (newcl->part2.bin->ovly.asserts!=NULL){
											MYFREE(newcl->part2.bin->ovly.asserts);
										}
										MYFREE(newcl->part2.bin);
										MYFREE(newcl);
										if (Subst.buffer!=NULL){
											MYFREE(Subst.buffer);
										}
										return(NOMEMORY);
									}
								}
								break;
						}
					}
				}
			} while(ii);
		}
	}

	/* Free memory and return. */
	if (Subst.buffer!=NULL){
		MYFREE(Subst.buffer);
	}
	return(0);
} /* BckDemodulation */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Demodulate a clause)  OCJ
 *
 *    This function demodulates a clause in internal compiled binary
 *    format. The new clause is placed in new allocated storage and
 *    its parents are set, but no other data is included in the
 *    clause cmprefix.
 *
 *    It is the caller responsibility to link and/or unlink the clauses
 *    and set the other prefix fields.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to cmprefix structure of clause to be simplified
 *            by demodulation.
 *    term1: Pointer to term that will be replaced in clause.
 *    binp: Pointer to binprefix structure of clause from where
 *          demodulation is performed.
 *    term2: Pointer to the term in "from" clause that is a generalization
 *           of term1.
 *    pnewcl: Address of pointer to cmprefix structure of new clause to be
 *            allocated for the result of demodulation.
 *    Subst: Pointer to substitution to be applied to term2.
 *
 *  RETURNS:
 *
 *    0 -> The clause was not simplified because ordering constraints
 *         were not met or redundancy check detected that demodulation
 *         does not preserve completeness.
 *    1 -> The clause was successfully simplified.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t Demodulate(cmprefix *clause,uint8_t *term1,binprefix *binp,
		uint8_t *term2,cmprefix **pnewcl,subst *Subst){
	auto uint8_t *equality;                              /* Pointer to equality literal in binp */
	auto binprefix *binp2;                               /* Auxiliary binary prefix pointer */
	auto uint8_t *ptr1,*ptr2,*ptr3;                      /* Auxiliary pointers */
	auto int32_t ii,jj,kk,nn;                            /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                     /* Auxiliary pointer */

	/* Debug. */
	ptr10=Decompile(clause->part2.bin);
	MYFREE(ptr10);
	ptr10=Decompile(binp);
	MYFREE(ptr10);
	#endif

	/* Get pointer to equality from which demodulation is performed */
	/* and set kk to the variable shift. */
	equality=&binp->formula[0];
	kk=binp->maxvarnb+1;

	/* Set ptr1 to the other root term in the "from" equality. If Next item */
	/* of term2 is the end of clause then it is the second root term in the */
	/* "from" equality. Otherwise term2 is the first root term in the "from" */
	/* equality. Take into account that the "from" clause has only one literal. */
	ptr1=NextItem(term2,OVERSUBTERMS);
	if (ptr1[0]&UNITEND){
		ptr1=NextItem(equality,IMMED);
	}

	/* Generate the term resulting from applying the substitution */
	/* to the substituting root term in demodulating clause. */
	if (NULL==(ptr3=ApplyGenrSubst2Term(ptr1,Subst,kk))){
		return(NOMEMORY);
	}

	/* Perform redundancy check if completeness must be preserved. */
	#ifdef REDUNDANCYCOMPLETE
	switch (kbset.opts.demodulation){
		case DEMODCPL1:
		case DEMODCPL1ORNT:
			if (0==(ii=RedundancyCheck2(clause->part2.bin,term1,ptr3,Subst,kk))){
				MYFREE(ptr3);
				return(ii);
			}
			break;
		case DEMODCPL2:
		case DEMODCPL2ORNT:
			if (0==(ii=RedundancyCheck(term1,clause->part2.bin,binp))){
				MYFREE(ptr3);
				return(ii);
			}
			break;
	}
	#else
	if (((kbset.opts.demodulation==DEMODCPL2)||(kbset.opts.demodulation==DEMODCPL2ORNT))
			&&(0==(ii=RedundancyCheck(clause->part2.bin,term1,term2,Subst)))){
		MYFREE(ptr3);
		return(ii);
	}
	#endif

	/* If demodulating clause is not weakly oriented then check ordering constraint */
	/* of root terms in equality "from" literal. */
	if (0==(WEAKLYORIENTED&binp->clause->flags)){
		if ((kbset.opts.demodulation==DEMODONORNT)||(kbset.opts.demodulation==DEMODCPL1ORNT)
				||(kbset.opts.demodulation==DEMODCPL2ORNT)){
			if (0==(MAXIMUM&term2[0])){
				MYFREE(ptr3);
				return(0);
			}
		} else {
			switch (ii=CheckEqRootOrder(term2,ptr1,Subst,kk,kk+clause->part2.bin->maxvarnb,MAXIMUM)){
				case NOMEMORY:
				case 0:
					MYFREE(ptr3);
					return(ii);
					break;
			}
		}

	/* Otherwise check if weak rewrite is possible. */
	} else {
		switch (ii=CheckWeakRewrite(term1,ptr1,Subst,kk)){
			case NOMEMORY:
				MYFREE(ptr3);
				return(NOMEMORY);
				break;
			case 1:
				MYFREE(ptr3);
				return(0);
				break;
		}
	}

	/* Allocate storage for inferred clause. */
	if (NULL==(*pnewcl=MYALLOC(sizeof(cmprefix)))){
		MYFREE(ptr3);
		return(NOMEMORY);
	}
	nn=binpsize+2*(clause->part2.bin->size+binp->size);
	if (NULL==(((*pnewcl)->part2.bin)=MYALLOC(nn))){
		MYFREE(*pnewcl);
		MYFREE(ptr3);
		return(NOMEMORY);
	}
	(*pnewcl)->part2.bin->clause=*pnewcl;
	(*pnewcl)->part2.bin->oriented=0;

	/* Copy formula of "to" parent clause up to and not including term1. */
	ii=term1-&clause->part2.bin->formula[0];
	memcpy(&(*pnewcl)->part2.bin->formula[0],&clause->part2.bin->formula[0],ii);

	/* Shift the numbers of variables of copied items. */
	(*pnewcl)->part2.bin->maxvarnb=-1;
	for (ptr2=NextItem(&(*pnewcl)->part2.bin->formula[0],IMMED);
			ptr2<&(*pnewcl)->part2.bin->formula[ii];
			ptr2=NextItem(ptr2,IMMED)){
		if (VARIABLE&ptr2[0]){
			*((uint16_t *)&ptr2[1])+=kk;
			if ((*pnewcl)->part2.bin->maxvarnb<*((uint16_t *)&ptr2[1])){
				(*pnewcl)->part2.bin->maxvarnb=*((uint16_t *)&ptr2[1]);
			}
		}
	}

	/* Add the other equality root term of clause from */
	/* where demodulation is performed. */
	jj=NextItem(ptr3,OVERSUBTERMS)-ptr3;
	memcpy(ptr2=&(*pnewcl)->part2.bin->formula[ii],ptr3,jj);
	MYFREE(ptr3);
	ii+=jj;

	/* Update maxvarnb of new clause with respect to the added part. */
	for (ptr3=ptr2+jj;ptr2<ptr3;ptr2=NextItem(ptr2,IMMED)){
		if (VARIABLE&ptr2[0]){
			if ((*pnewcl)->part2.bin->maxvarnb<*((uint16_t *)&ptr2[1])){
				(*pnewcl)->part2.bin->maxvarnb=*((uint16_t *)&ptr2[1]);
			}
		}
	}

	/* Add remaining items of clause being simplified */
	/* including the UNITEND marker byte. */
	ptr1=NextItem(term1,OVERSUBTERMS);
	jj=clause->part2.bin->size-(ptr1-&clause->part2.bin->formula[0]);
	memcpy(&(*pnewcl)->part2.bin->formula[ii],ptr1,jj);

	/* Shift the numbers of variables of last copied items. */
	for (ptr1=&(*pnewcl)->part2.bin->formula[ii];ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){
		if (VARIABLE&ptr1[0]){
			*((uint16_t *)&ptr1[1])+=kk;
			if ((*pnewcl)->part2.bin->maxvarnb<*((uint16_t *)&ptr1[1])){
				(*pnewcl)->part2.bin->maxvarnb=*((uint16_t *)&ptr1[1]);
			}
		}
	}

	/* Update new clause parameters. */
	UpdateParams((*pnewcl)->part2.bin);

	#ifndef REDUNDANCYCOMPLETE
	/* Perform redundancy check if completeness must be preserved. */
	if (((kbset.opts.demodulation==DEMODCPL2)||(kbset.opts.demodulation==DEMODCPL2ORNT))
			&&(0!=(ii=RedundancyCheck(term2,ptr2,(*pnewcl)->part2.bin,binp)))){
		MYFREE((*pnewcl)->part2.bin);
		MYFREE(*pnewcl);
		if (ii==NOMEMORY){
			return(NOMEMORY);
		}
		return(0);
	}
	#endif

	/* Reallocate the formula buffer to the exact length needed. */
	if (NULL==(binp2=MYREALLOC((*pnewcl)->part2.bin,binpsize+(*pnewcl)->part2.bin->size))){
		if ((*pnewcl)->part2.bin->ovly.asserts!=NULL){
			MYFREE((*pnewcl)->part2.bin->ovly.asserts);
		}
		MYFREE((*pnewcl)->part2.bin);
		MYFREE(*pnewcl);
		return(NOMEMORY);
	}
	(*pnewcl)->part2.bin=binp2;

	/* Debug. */
	#ifdef DEBUGCODE
	(*pnewcl)->part2.bin->ovly.asserts=NULL;
	ptr10=Decompile((*pnewcl)->part2.bin);
	MYFREE(ptr10);
	#endif

	/* Set simplified clause cmprefix data. */
	(*pnewcl)->parent1=binp->clause;
	(*pnewcl)->parent2=clause;
	(*pnewcl)->part2.bin->agedist=clause->part2.bin->agedist;
	if (binp->sinedist<clause->part2.bin->sinedist){
		(*pnewcl)->part2.bin->sinedist=binp->sinedist;
	} else {
		(*pnewcl)->part2.bin->sinedist=clause->part2.bin->sinedist;
	}

	return(1);
} /* Demodulate */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Apply a generalization substitution to a term)  OCJ
 *
 *    This function applies a generalization substitution to a term
 *    that is a generalization of another term in another clause.
 *    It is similar to ApplySubst2Clause() function with the following
 *    differences:
 *    - A buffer for the resulting term is allocated by the function.
 *    - It returns a pointer to the buffer allocated for the resulting
 *      new term.
 *    - There is no loop until no substitution is applied because it
 *      is not needed for generalizations.
 *
 *
 *  ARGUMENTS:
 *
 *    term: Pointer to term with a UNITEND ending byte.
 *    Subst: Pointer to substitution.
 *    varshift: Shift for variable numbers belonging to a secondary
 *              clause. Variable numbers in substitution coming from
 *              secondary clause increased by varshift match variable
 *              numbers in the secondary clause.
 *
 *  RETURNS:
 *
 *    Pointer to resulting new term or NULL if not enough memory.
 *
 *
 *--------------------------------------------------------------*/
uint8_t *ApplyGenrSubst2Term(uint8_t *term,subst *Subst,int32_t varshift){
	auto uint8_t *newterm;                               /* Pointer to buffer for resulting new term */
	auto int32_t size;                                   /* Size of newterm buffer */
	auto int32_t maxvarnb;                               /* Maximum number assigned to variable in substitution */
	auto uint8_t **substterms;                           /* Address for set of pointers of substituting terms */
	auto uint8_t *ptr1,*ptr2;                            /* Auxiliary pointer */
	auto int32_t ii;                                     /* Auxiliary */

	/* If substitution is empty then duplicate term and return. */
	if ((Subst->buffer==NULL)||(Subst->substsize==1)){
		size=NextItem(term,OVERSUBTERMS)-term;
		if (NULL!=(newterm=MYALLOC(size+1))){
			memcpy(newterm,term,size);
			newterm[0]&=(~(MAXIMUM|SELECTED));
			newterm[size]=UNITEND;
		}
		return(newterm);
	}

	/* Allocate memory for new term buffer. */
	size=3*(NextItem(term,OVERSUBTERMS)-term);
	if (NULL==(newterm=MYALLOC(size))){
		return(NULL);
	}

	/* Get the maximum variable number in substitution. */
	for (ptr1=Subst->buffer,maxvarnb=0;ptr1[0]!=UNITEND;ptr1=NextItem(&ptr1[3],OVERSUBTERMS)){
		ii=(*((int16_t *)&ptr1[1]));
		if (maxvarnb<ii){
			maxvarnb=ii;
		}
	}

	/* Allocate memory for substitution term auxiliary data and initialize it. */
	if (NULL==(substterms=MYALLOC(ii=sizeof(void *)*(maxvarnb+1)))){
		MYFREE(newterm);
		return(NULL);
	}
	memset(substterms,0,ii);

	/* Initialize set of pointers of substituting terms and */
	/* adjust substitution (see GetSubstTermData() for details). */
	GetSubstTermData(Subst,substterms,varshift);

	/* Apply substitution to generalization term. */
	ptr1=newterm;
	if (NOMEMORY==ApplySubst2Item(&ptr1,(binprefix **)&newterm,&size,term,maxvarnb,substterms)){
		MYFREE(newterm);
		MYFREE(substterms);
		return(NULL);
	}

	/* Copy UNITEND byte to destination buffer */
	/* and update newterm parameters. */
	if (size==(ptr1-newterm)){
		if (NULL==(ptr2=MYREALLOC(newterm,size+1))){
			MYFREE(newterm);
			MYFREE(substterms);
			return(NULL);
		}
		ptr1=ptr2+size;
		newterm=ptr2;
		size++;
	}
	ptr1[0]=UNITEND;
	UpdateParams2(newterm);
	newterm[0]&=(~(MAXIMUM|SELECTED));

	/* Free memory, reallocate newterm to the exact size needed and return. */
	MYFREE(substterms);
	ii=1+(ptr1-newterm);
	if (size!=ii){
		if (NULL==(ptr1=MYREALLOC(newterm,ii))){
			MYFREE(newterm);
			return(NULL);
		}
		newterm=ptr1;
	}
	return(newterm);
} /* ApplyGenrSubst2Term */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if a substitution generates a variant of a term)  OCJ
 *
 *    This function checks if a substitution generates a variant of a term.
 *    A substitution is a variant if it is an injective application from
 *    variables to variables.
 *
 *    IMPORTANT: The check performed by this function is guaranteed to
 *    give a correct result only for substitutions that correspond to
 *    generalizations. Otherwise the result is not guaranteed to be
 *    correct.
 *
 *
 *  ARGUMENTS:
 *
 *    Subst: Pointer to substitution.
 *    maxvarnb: Maximum variable number of "to" variables, that is,
 *              variables that substitute substituted variables.
 *              Care must be taken because Subst may have been
 *              reorganized such that "to" variables have been
 *              shifted (for instance by a call to GetSubstTermData()
 *              function). In this case the variable shift must be
 *              included in the maxvarnb argument.
 *
 *  RETURNS:
 *
 *    1 if substitution generates a variant of a term.
 *    0 if substitution doesn't generate a variant of a term.
 *    NOMEMORY of not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t IsVariantSubst(subst *Subst,int32_t maxvarnb){
	auto int8_t *varmap;                                 /* Pointer to map substituted "to" variables */
	auto uint8_t *ptr1;                                  /* Auxiliary pointers */
	auto int32_t ii;                                     /* Auxiliary */

	/* If substitution is empty then return that the substitution */
	/* generates a variant. */
	if ((Subst->buffer==NULL)||(Subst->substsize==1)){
		return(1);
	}

	/* Allocate and set memory for "to" variables information. */
	/* If varmap[i]==0 then variable i is not a "to" variable */
	/* in the substitution. Otherwise varmap[i]==1. */
	if (NULL==(varmap=MYALLOC(1+maxvarnb))){
		return(NOMEMORY);
	}
	memset(varmap,0,1+maxvarnb);

	/* Loop through substitution items. */
	for (ptr1=Subst->buffer;ptr1[0]!=UNITEND;ptr1=NextItem(&ptr1[3],OVERSUBTERMS)){

		/* If substitution term is not a variable then return */
		/* that the substitution doesn't generate a variant. */
		if (0==(VARIABLE&ptr1[3])){
			MYFREE(varmap);
			return(0);
		}

		/* If the variable in the substitution term has been already assigned */
		/* then return that the substitution doesn't generate a variant. */
		ii=*((int16_t *)&ptr1[4]);
		if (varmap[ii]!=0){
			MYFREE(varmap);
			return(0);
		}

		/* Flag the variable in the substitution term as already assigned. */
		varmap[ii]=1;
	}

	/* Free memory and return that the substitution generates a variant. */
	MYFREE(varmap);
	return(1);
} /* IsVariantSubst */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform forward subsumption resolution)  OCJ
 *
 *    This function tries to simplify the given clause in compiled or binary
 *    format with subsumption resolution inferences by clauses in the indicated
 *    KB by calling SubsResltn() function for every simplifier candidate clause
 *    in the requested KB. It must also be specified the type of clauses that
 *    will be considered, either active and/or passive clauses.
 *
 *    Simplifier candidate clauses are selected from those that have a literal
 *    whose negation is a generalization of a literal in the clause to be
 *    simplified.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in compiled binary form.
 *    pnewcl: Address of pointer where the simplified clause will be placed.
 *    type: PASSIVE if passive clauses are to be checked for
 *          simplifications. Active clauses are always checked.
 *          Any other flag is ignored.
 *
 *  RETURNS:
 *
 *    0 -> Clause was not simplified.
 *    1 -> Clause was simplified.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t FwSubsResltn(cmprefix *clause,cmprefix **pnewcl,int32_t type){
	auto binprefix *binp;                                /* Pointer to candidate binprefix structure */
	auto uint8_t *equality;                              /* Pointer to equality with root terms in reverse order */
	auto uint8_t *eqterms;                               /* Pointer to equality root terms in reverse order */
	auto uint8_t *queryitm;                              /* Pointer to QueryDscTree() query item */
	auto int32_t size;                                   /* Size of eqterms buffer */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5;          /* Auxiliary pointers */
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	auto int32_t ii,jj,kk,mm,nn,rr,ss;                   /* Auxiliary */

	/* Loop through clause literals. */
	eqterms=equality=NULL;
	size=0;
	for (ptr1=&clause->part2.bin->formula[0];ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS)){

		/* Loop for equality symmetry management. */
		ptr4=NextItem(ptr1,IMMED);
		mm=(ptr1[0]&EQUALITY?2:1);
		for (jj=0;jj<mm;jj++){

			/* This is the first iteration. */
			if (jj==0){
				queryitm=ptr1;

			/* This is the second iteration for an equality literal. */
			/* Build the set of root terms in reverse order. */
			} else {
				ptr3=NextItem(ptr4,OVERSUBTERMS);
				rr=ptr3-ptr4;
				ss=NextItem(ptr3,OVERSUBTERMS)-ptr3;
				if (size==0){
					if (NULL==(equality=MYALLOC(size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
						return(NOMEMORY);
					}
					eqterms=&equality[1+sizeof(symbol)];
				} else if (size<(2+sizeof(symbol)+rr+ss)){
					if (NULL==(ptr5=MYREALLOC(equality,size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
						MYFREE(equality);
						return(NOMEMORY);
					}
					equality=ptr5;
					eqterms=&equality[1+sizeof(symbol)];
				}
				memcpy(equality,ptr1,1+sizeof(symbol));
				memcpy(eqterms,ptr3,ss);
				memcpy(&eqterms[ss],ptr4,rr);
				eqterms[rr+ss]=UNITEND;
				queryitm=equality;
			}

			/* Loop through ACTIVE and PASSIVE clauses. ACTIVE clauses have */
			/* selected and non selected literals. */
			for (kk=ACTIVE;kk!=-1;
					kk=(kk==ACTIVE?ACTIVE|SELECTED:(kk==PASSIVE?-1:(type&PASSIVE?PASSIVE:-1)))){

				/* Get pointer to top tree node. */
				switch (kk){
					case ACTIVE:
						if (ptr1[0]&NEGATED){
							ptr3=(((symbol *)&ptr1[1])->symbol)->discr[2];
						} else {
							ptr3=(((symbol *)&ptr1[1])->symbol)->discr[3];
						}
						break;
					case PASSIVE:
						if (ptr1[0]&NEGATED){
							ptr3=(((symbol *)&ptr1[1])->symbol)->discr[4];
						} else {
							ptr3=(((symbol *)&ptr1[1])->symbol)->discr[5];
						}
						break;
					default:
						if (ptr1[0]&NEGATED){
							ptr3=(((symbol *)&ptr1[1])->symbol)->discr[0];
						} else {
							ptr3=(((symbol *)&ptr1[1])->symbol)->discr[1];
						}
						break;
				}

				/* Loop through generalization candidates. */
				nn=0;
				do {

					/* Check timeout and solution by other process. */
					if ((procctl->status==TIMEOUT)||(procctl->status==UNSATISFIABLE)
							||(procctl->status==SATISFIABLE)){
						MYFREE(equality);
						return(TIMEOUT);
					}
					if (procctl->status==NOMEMORY){
						MYFREE(equality);
						return(procctl->status);
					}

					/* Get a candidate negative generalization literal. */
					if (0!=(ii=QueryGenDscTree(queryitm,&ptr2,ptr3,nn))){

						/* Get pointer to candidate binprefix structure. */
						nn=1;
						binp=(binprefix *)(ptr2-(binpsize+((symbol *)&ptr2[1])->offset));

						/* Debug. */
						#ifdef DEBUGCODE
						ptr10=Decompile(binp);
						MYFREE(ptr10);
						#endif

						/* Perform subsumption resolution and check if it is successful. */
						switch (SubsResltn(clause,binp,ptr1,ptr2,pnewcl)){

							/* Not enough memory. */
							case NOMEMORY:
								MYFREE(equality);
								return(NOMEMORY);
								break;

							/* Successful simplification. */
							case 1:

								/* Set locks of simplified clause and asserts of demodulation result. */
								if (NOMEMORY==GetLocksAsserts(binp,clause->part2.bin,(*pnewcl)->part2.bin)){
									if ((*pnewcl)->part2.bin->ovly.asserts!=NULL){
										MYFREE((*pnewcl)->part2.bin->ovly.asserts);
									}
									MYFREE((*pnewcl)->part2.bin);
									MYFREE((*pnewcl));
									MYFREE(equality);
									return(NOMEMORY);
								}

								/* Set type of inference and flags in new clause. */
								(*pnewcl)->inference=FWSUBSRESLTNS;
								(*pnewcl)->flags=UNPROC;

								/* If original clause is not locked then convert it to text form. */
								if (0==(LOCKED&clause->flags)){
									if (NULL==(ptr10=Decompile(clause->part2.bin))){
										if ((*pnewcl)->part2.bin->ovly.asserts!=NULL){
											MYFREE((*pnewcl)->part2.bin->ovly.asserts);
										}
										MYFREE((*pnewcl)->part2.bin);
										MYFREE((*pnewcl));
										MYFREE(equality);
										return(NOMEMORY);
									}
									if (clause->part2.bin->ovly.asserts!=NULL){
										MYFREE(clause->part2.bin->ovly.asserts);
									}
									MYFREE(clause->part2.bin);
									clause->part2.text=ptr10;
								}

								/* Debug. */
								#ifdef DEBUGCODE
								if (LOCKED&clause->flags){
									ptr10=Decompile(clause->part2.bin);
								} else {
									ptr10=NULL;
								}
								#ifdef VERBOSE
								if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
									printf("Subsumption resolution simplification of [%ld]:\n%s\n",clause->number,
											LOCKED&clause->flags?ptr10:clause->part2.text);
									MYFREE(ptr10);
									ptr10=Decompile(binp);
									printf("Subsumption resolution simplification by[%ld]:\n%s\n",binp->clause->number,ptr10);
								}
								#endif
								MYFREE(ptr10);
								ptr10=Decompile((*pnewcl)->part2.bin);
								#ifdef VERBOSE
								if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
									printf("Result [%ld]:\n%s\n",kbset.nformulas,ptr10);
								}
								#endif
								MYFREE(ptr10);
								#endif
								MYFREE(equality);
								return(1);
								break;
						}
					}
				} while(ii);
			}
		}
	}

	/* Free memory and return unsuccessful simplification. */
	MYFREE(equality);
	return(0);
} /* FwSubsResltn */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform forward subsumption resolution for unit clause)  OCJ
 *
 *    This function tries to simplify the given clause in compiled or binary
 *    format with subsumption resolution inferences by clauses in the indicated
 *    KB by calling SubsResltn() function for every simplifier candidate clause
 *    in the requested KB for type 8 problems (unit clause). It must also be
 *    specified the type of clauses that will be considered, either active
 *    and/or passive clauses.
 *
 *    Simplifier candidate clauses are selected from those that have a literal
 *    whose negation is a generalization of a literal in the clause to be
 *    simplified.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in compiled binary form.
 *    pnewcl: Address of pointer where the simplified clause will be placed.
 *    type: PASSIVE if passive clauses are to be checked for
 *          simplifications. Active clauses are always checked.
 *          Any other flag is ignored.
 *
 *  RETURNS:
 *
 *    0 -> Clause was not simplified.
 *    1 -> Clause was simplified.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t FwSubsResltnUEQ(cmprefix *clause,cmprefix **pnewcl,int32_t type){
	auto binprefix *binp;                                /* Pointer to candidate binprefix structure */
	auto uint8_t *equality;                              /* Pointer to equality with root terms in reverse order */
	auto uint8_t *eqterms;                               /* Pointer to equality root terms in reverse order */
	auto uint8_t *queryitm;                              /* Pointer to QueryDscTree() query item */
	auto int32_t size;                                   /* Size of eqterms buffer */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5;          /* Auxiliary pointers */
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	auto int32_t ii,jj,kk,mm,nn,rr,ss;                   /* Auxiliary */

	/* Loop for equality symmetry management. */
	eqterms=equality=NULL;
	size=0;
	ptr1=&clause->part2.bin->formula[0];
	ptr4=NextItem(ptr1,IMMED);
	mm=(ptr1[0]&EQUALITY?2:1);
	for (jj=0;jj<mm;jj++){

		/* This is the first iteration. */
		if (jj==0){
			queryitm=ptr1;

		/* This is the second iteration for an equality literal. */
		/* Build the set of root terms in reverse order. */
		} else {
			ptr3=NextItem(ptr4,OVERSUBTERMS);
			rr=ptr3-ptr4;
			ss=NextItem(ptr3,OVERSUBTERMS)-ptr3;
			if (size==0){
				if (NULL==(equality=MYALLOC(size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
					return(NOMEMORY);
				}
				eqterms=&equality[1+sizeof(symbol)];
			} else if (size<(2+sizeof(symbol)+rr+ss)){
				if (NULL==(ptr5=MYREALLOC(equality,size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
					MYFREE(equality);
					return(NOMEMORY);
				}
				equality=ptr5;
				eqterms=&equality[1+sizeof(symbol)];
			}
			memcpy(equality,ptr1,1+sizeof(symbol));
			memcpy(eqterms,ptr3,ss);
			memcpy(&eqterms[ss],ptr4,rr);
			eqterms[rr+ss]=UNITEND;
			queryitm=equality;
		}

		/* Loop through ACTIVE and PASSIVE clauses. ACTIVE clauses have */
		/* selected and non selected literals. */
		for (kk=ACTIVE;kk!=-1;
				kk=(kk==ACTIVE?ACTIVE|SELECTED:(kk==PASSIVE?-1:(type&PASSIVE?PASSIVE:-1)))){

			/* Get pointer to top tree node. */
			switch (kk){
				case ACTIVE:
					if (ptr1[0]&NEGATED){
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[2];
					} else {
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[3];
					}
					break;
				case PASSIVE:
					if (ptr1[0]&NEGATED){
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[4];
					} else {
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[5];
					}
					break;
				default:
					if (ptr1[0]&NEGATED){
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[0];
					} else {
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[1];
					}
					break;
			}

			/* Loop through generalization candidates. */
			nn=0;
			do {

				/* Check timeout and solution by other process. */
				if ((procctl->status==TIMEOUT)||(procctl->status==UNSATISFIABLE)
						||(procctl->status==SATISFIABLE)){
					MYFREE(equality);
					return(TIMEOUT);
				}
				if (procctl->status==NOMEMORY){
					MYFREE(equality);
					return(procctl->status);
				}

				/* Get a candidate negative generalization literal. */
				if (0!=(ii=QueryGenDscTree(queryitm,&ptr2,ptr3,nn))){

					/* Get pointer to candidate binprefix structure. */
					nn=1;
					binp=(binprefix *)(ptr2-(binpsize+((symbol *)&ptr2[1])->offset));

					/* Debug. */
					#ifdef DEBUGCODE
					ptr10=Decompile(binp);
					MYFREE(ptr10);
					#endif

					/* Perform subsumption resolution and check if it is successful. */
					switch (SubsResltnUEQ(clause,binp,ptr1,ptr2,pnewcl)){

						/* Not enough memory. */
						case NOMEMORY:
							MYFREE(equality);
							return(NOMEMORY);
							break;

						/* Successful simplification. */
						case 1:

							/* Set locks of simplified clause and asserts of demodulation result. */
							(*pnewcl)->part2.bin->ovly.asserts=(*pnewcl)->part2.bin->lockasserts=NULL;

							/* Set type of inference and flags in new clause. */
							(*pnewcl)->inference=FWSUBSRESLTNS;
							(*pnewcl)->flags=UNPROC;

							/* Convert clause to text form. */
							if (NULL==(ptr10=Decompile(clause->part2.bin))){
								MYFREE((*pnewcl)->part2.bin);
								MYFREE((*pnewcl));
								MYFREE(equality);
								return(NOMEMORY);
							}
							MYFREE(clause->part2.bin);
							clause->part2.text=ptr10;

							/* Debug. */
							#ifdef DEBUGCODE
							if (LOCKED&clause->flags){
								ptr10=Decompile(clause->part2.bin);
							} else {
								ptr10=NULL;
							}
							#ifdef VERBOSE
							if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
								printf("Subsumption resolution simplification of [%ld]:\n%s\n",clause->number,
										LOCKED&clause->flags?ptr10:clause->part2.text);
								MYFREE(ptr10);
								ptr10=Decompile(binp);
								printf("Subsumption resolution simplification by[%ld]:\n%s\n",binp->clause->number,ptr10);
							}
							#endif
							MYFREE(ptr10);
							ptr10=Decompile((*pnewcl)->part2.bin);
							#ifdef VERBOSE
							if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
								printf("Result [%ld]:\n%s\n",kbset.nformulas,ptr10);
							}
							#endif
							MYFREE(ptr10);
							#endif
							MYFREE(equality);
							return(1);
							break;
					}
				}
			} while(ii);
		}
	}

	/* Free memory and return unsuccessful simplification. */
	MYFREE(equality);
	return(0);
} /* FwSubsResltnUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform backward subsumption resolution)  OCJ
 *
 *    This function tries to simplify clauses in KB by the given clause in
 *    compiled or binary format with subsumption resolution inferences by
 *    calling SubsResltn() function for every simplifier candidate clause
 *    in the requested KB. It must also be specified the type of clauses that
 *    will be considered, either active and/or passive clauses.
 *
 *    Clauses candidate to be simplified are selected from those that have
 *    a literal whose negation is an instance of a literal in the clause
 *    to be simplified.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in compiled binary form.
 *    type: PASSIVE if passive clauses are to be checked for
 *          simplifications. Active clauses are always checked.
 *          Any other flag is ignored.
 *
 *  RETURNS:
 *
 *    0 -> Process performed and no empty clause without assertions generated.
 *    1 -> Process performed and an empty clause without assertions was generated.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t BkSubsResltn(cmprefix *clause,int32_t type){
	auto cmprefix *newcl;                                /* Pointer to simplified clause cmprefix structure */
	auto binprefix *binp;                                /* Pointer to candidate binprefix structure */
	auto uint8_t *equality;                              /* Pointer to equality with root terms in reverse order */
	auto uint8_t *eqterms;                               /* Pointer to equality root terms in reverse order */
	auto uint8_t *queryitm;                              /* Pointer to QueryDscTree() query item */
	auto int32_t size;                                   /* Size of eqterms buffer */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5;          /* Auxiliary pointers */
	auto int32_t ii,jj,kk,mm,nn,rr,ss;                   /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	#endif

	/* Loop through clause literals. */
	eqterms=equality=NULL;
	size=0;
	for (ptr1=&clause->part2.bin->formula[0];ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS)){

		/* Loop for equality symmetry management. */
		ptr4=NextItem(ptr1,IMMED);
		mm=(ptr1[0]&EQUALITY?2:1);
		for (jj=0;jj<mm;jj++){

			/* This is the first iteration. */
			if (jj==0){
				queryitm=ptr1;

			/* This is the second iteration for an equality literal. */
			/* Build the set of root terms in reverse order. */
			} else {
				ptr3=NextItem(ptr4,OVERSUBTERMS);
				rr=ptr3-ptr4;
				ss=NextItem(ptr3,OVERSUBTERMS)-ptr3;
				if (size==0){
					if (NULL==(equality=MYALLOC(size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
						return(NOMEMORY);
					}
					eqterms=&equality[1+sizeof(symbol)];
				} else if (size<(2+sizeof(symbol)+rr+ss)){
					if (NULL==(ptr5=MYREALLOC(equality,size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
						MYFREE(equality);
						return(NOMEMORY);
					}
					equality=ptr5;
					eqterms=&equality[1+sizeof(symbol)];
				}
				memcpy(equality,ptr1,1+sizeof(symbol));
				memcpy(eqterms,ptr3,ss);
				memcpy(&eqterms[ss],ptr4,rr);
				eqterms[rr+ss]=UNITEND;
				queryitm=equality;
			}

			/* Loop through ACTIVE and PASSIVE clauses. ACTIVE clauses have */
			/* selected and non selected literals. */
			for (kk=ACTIVE;kk!=-1;
					kk=(kk==ACTIVE?ACTIVE|SELECTED:(kk==PASSIVE?-1:(type&PASSIVE?PASSIVE:-1)))){

				/* Get pointer to top tree node. */
				switch (kk){
					case ACTIVE:
						if (ptr1[0]&NEGATED){
							ptr3=(((symbol *)&ptr1[1])->symbol)->discr[2];
						} else {
							ptr3=(((symbol *)&ptr1[1])->symbol)->discr[3];
						}
						break;
					case PASSIVE:
						if (ptr1[0]&NEGATED){
							ptr3=(((symbol *)&ptr1[1])->symbol)->discr[4];
						} else {
							ptr3=(((symbol *)&ptr1[1])->symbol)->discr[5];
						}
						break;
					default:
						if (ptr1[0]&NEGATED){
							ptr3=(((symbol *)&ptr1[1])->symbol)->discr[0];
						} else {
							ptr3=(((symbol *)&ptr1[1])->symbol)->discr[1];
						}
						break;
				}

				/* Loop through candidate instances. */
				nn=0;
				do {

					/* Check timeout and solution by other process. */
					if ((procctl->status==TIMEOUT)||(procctl->status==UNSATISFIABLE)
							||(procctl->status==SATISFIABLE)){
						MYFREE(equality);
						return(TIMEOUT);
					}
					if (procctl->status==NOMEMORY){
						MYFREE(equality);
						return(procctl->status);
					}

					/* Get a candidate negative instance literal. */
					if (0!=(ii=QueryInsDscTree(queryitm,&ptr2,ptr3,nn))){

						/* Get pointer to candidate binprefix structure. */
						nn=1;
						binp=(binprefix *)(ptr2-(binpsize+((symbol *)&ptr2[1])->offset));

						/* Debug. */
						#ifdef DEBUGCODE
						ptr10=Decompile(binp);
						MYFREE(ptr10);
						#endif

						/* Clause is not in disposal queue therefore */
						/* it has not been already simplified. */
						if (0==(binp->clause->flags&INDISPOSALQUEUE)){

							/* Perform subsumption resolution and check if it is successful. */
							switch (SubsResltn(binp->clause,clause->part2.bin,ptr2,ptr1,&newcl)){

								/* Not enough memory. */
								case NOMEMORY:
									MYFREE(equality);
									return(NOMEMORY);
									break;

								/* Successful simplification. */
								case 1:

									/* Set locks of simplified clause and asserts of simplification result. */
									if (NOMEMORY==GetLocksAsserts(clause->part2.bin,binp,newcl->part2.bin)){
										if (newcl->part2.bin->ovly.asserts!=NULL){
											MYFREE(newcl->part2.bin->ovly.asserts);
										}
										MYFREE(newcl->part2.bin);
										MYFREE(newcl);
										MYFREE(equality);
										return(NOMEMORY);
									}

									/* Debug. */
									#ifdef DEBUGCODE
									ptr10=Decompile(clause->part2.bin);
									#ifdef VERBOSE
									if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
										printf("Backward subsumption resolution simplification from [%ld]:\n%s\n",clause->number,ptr10);
										MYFREE(ptr10);
										ptr10=Decompile(binp);
										printf("Backward subsumption resolution simplification to [%ld]:\n%s\n",binp->clause->number,ptr10);
									}
									#endif
									MYFREE(ptr10);
									ptr10=Decompile(newcl->part2.bin);
									#ifdef VERBOSE
									if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
										printf("Result [%ld]:\n%s\n",kbset.nformulas,ptr10);
									}
									#endif
									MYFREE(ptr10);
									#endif

									/* Add original simplified clause to disposal queue for later */
									/* deletion or locking. This queue interconnects clauses from other */
									/* queues. The clause must not be removed from any queue to which */
									/* it is linked. */
									binp->clause->flags|=INDISPOSALQUEUE;
									binp->clause->prevdisp=kbset.lastdisp;
									kbset.lastdisp=binp->clause;
									kbset.prstats.bksubsresltns++;

									/* Add new simplified clause to KB as UNPROC. */
									newcl->flags=UNPROC;
									newcl->inference=BKSUBSRESLTNS;
									if (NOMEMORY==AddBinClause2KB(newcl,&kbset,0)){
										if (newcl->part2.bin->ovly.asserts!=NULL){
											MYFREE(newcl->part2.bin->ovly.asserts);
										}
										MYFREE(newcl->part2.bin);
										MYFREE(newcl);
										MYFREE(equality);
										return(NOMEMORY);
									}

									/* Check empty clause without assertions. */
									if ((newcl->part2.bin->formula[0]==UNITEND)&&(newcl->part2.bin->ovly.asserts==NULL)){
										MYFREE(equality);
										if (NULL==(kbset.frstprfnode=MYALLOC(sizeof(proofnode)))){
											return(NOMEMORY);
										}
										kbset.frstprfnode->type=ACLAUSE;
										kbset.frstprfnode->ptr.Aclause=newcl;
										return(1);
									}
									break;
							}
						}
					}
				} while(ii);
			}
		}
	}

	/* Free memory and return unsuccessful simplification. */
	MYFREE(equality);
	return(0);
} /* BkSubsResltn */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform backward subsumption resolution for unit clause)  OCJ
 *
 *    This function tries to simplify clauses in KB by the given clause in
 *    compiled or binary format with subsumption resolution inferences by
 *    calling SubsResltn() function for every simplifier candidate clause
 *    in the requested KB for type 8 problems (unit equality). It must also
 *    be specified the type of clauses that will be considered, either active
 *    and/or passive clauses.
 *
 *    Clauses candidate to be simplified are selected from those that have
 *    a literal whose negation is an instance of a literal in the clause
 *    to be simplified.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in compiled binary form.
 *    type: PASSIVE if passive clauses are to be checked for
 *          simplifications. Active clauses are always checked.
 *          Any other flag is ignored.
 *
 *  RETURNS:
 *
 *    0 -> Process performed and no empty clause without assertions generated.
 *    1 -> Process performed and an empty clause without assertions was generated.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t BkSubsResltnUEQ(cmprefix *clause,int32_t type){
	auto cmprefix *newcl;                                /* Pointer to simplified clause cmprefix structure */
	auto binprefix *binp;                                /* Pointer to candidate binprefix structure */
	auto uint8_t *equality;                              /* Pointer to equality with root terms in reverse order */
	auto uint8_t *eqterms;                               /* Pointer to equality root terms in reverse order */
	auto uint8_t *queryitm;                              /* Pointer to QueryDscTree() query item */
	auto int32_t size;                                   /* Size of eqterms buffer */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5;          /* Auxiliary pointers */
	auto int32_t ii,jj,kk,mm,nn,rr,ss;                   /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	#endif

	/* Loop for equality symmetry management. */
	eqterms=equality=NULL;
	size=0;
	ptr1=&clause->part2.bin->formula[0];
	ptr4=NextItem(ptr1,IMMED);
	mm=(ptr1[0]&EQUALITY?2:1);
	for (jj=0;jj<mm;jj++){

		/* This is the first iteration. */
		if (jj==0){
			queryitm=ptr1;

		/* This is the second iteration for an equality literal. */
		/* Build the set of root terms in reverse order. */
		} else {
			ptr3=NextItem(ptr4,OVERSUBTERMS);
			rr=ptr3-ptr4;
			ss=NextItem(ptr3,OVERSUBTERMS)-ptr3;
			if (size==0){
				if (NULL==(equality=MYALLOC(size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
					return(NOMEMORY);
				}
				eqterms=&equality[1+sizeof(symbol)];
			} else if (size<(2+sizeof(symbol)+rr+ss)){
				if (NULL==(ptr5=MYREALLOC(equality,size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
					MYFREE(equality);
					return(NOMEMORY);
				}
				equality=ptr5;
				eqterms=&equality[1+sizeof(symbol)];
			}
			memcpy(equality,ptr1,1+sizeof(symbol));
			memcpy(eqterms,ptr3,ss);
			memcpy(&eqterms[ss],ptr4,rr);
			eqterms[rr+ss]=UNITEND;
			queryitm=equality;
		}

		/* Loop through ACTIVE and PASSIVE clauses. ACTIVE clauses have */
		/* selected and non selected literals. */
		for (kk=ACTIVE;kk!=-1;
				kk=(kk==ACTIVE?ACTIVE|SELECTED:(kk==PASSIVE?-1:(type&PASSIVE?PASSIVE:-1)))){

			/* Get pointer to top tree node. */
			switch (kk){
				case ACTIVE:
					if (ptr1[0]&NEGATED){
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[2];
					} else {
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[3];
					}
					break;
				case PASSIVE:
					if (ptr1[0]&NEGATED){
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[4];
					} else {
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[5];
					}
					break;
				default:
					if (ptr1[0]&NEGATED){
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[0];
					} else {
						ptr3=(((symbol *)&ptr1[1])->symbol)->discr[1];
					}
					break;
			}

			/* Loop through candidate instances. */
			nn=0;
			do {

				/* Check timeout and solution by other process. */
				if ((procctl->status==TIMEOUT)||(procctl->status==UNSATISFIABLE)
						||(procctl->status==SATISFIABLE)){
					MYFREE(equality);
					return(TIMEOUT);
				}
				if (procctl->status==NOMEMORY){
					MYFREE(equality);
					return(procctl->status);
				}

				/* Get a candidate negative instance literal. */
				if (0!=(ii=QueryInsDscTree(queryitm,&ptr2,ptr3,nn))){

					/* Get pointer to candidate binprefix structure. */
					nn=1;
					binp=(binprefix *)(ptr2-(binpsize+((symbol *)&ptr2[1])->offset));

					/* Debug. */
					#ifdef DEBUGCODE
					ptr10=Decompile(binp);
					MYFREE(ptr10);
					#endif

					/* Clause is not in disposal queue therefore */
					/* it has not been already simplified. */
					if (0==(binp->clause->flags&INDISPOSALQUEUE)){

						/* Perform subsumption resolution and check if it is successful. */
						switch (SubsResltnUEQ(binp->clause,clause->part2.bin,ptr2,ptr1,&newcl)){

							/* Not enough memory. */
							case NOMEMORY:
								MYFREE(equality);
								return(NOMEMORY);
								break;

							/* Successful simplification. */
							case 1:

								/* Set locks of simplified clause and asserts of simplification result. */
								newcl->part2.bin->ovly.asserts=newcl->part2.bin->lockasserts=NULL;

								/* Debug. */
								#ifdef DEBUGCODE
								ptr10=Decompile(clause->part2.bin);
								#ifdef VERBOSE
								if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
									printf("Backward subsumption resolution simplification from [%ld]:\n%s\n",clause->number,ptr10);
									MYFREE(ptr10);
									ptr10=Decompile(binp);
									printf("Backward subsumption resolution simplification to [%ld]:\n%s\n",binp->clause->number,ptr10);
								}
								#endif
								MYFREE(ptr10);
								ptr10=Decompile(newcl->part2.bin);
								#ifdef VERBOSE
								if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
									printf("Result [%ld]:\n%s\n",kbset.nformulas,ptr10);
								}
								#endif
								MYFREE(ptr10);
								#endif

								/* Add original simplified clause to disposal queue for later */
								/* deletion or locking. This queue interconnects clauses from other */
								/* queues. The clause must not be removed from any queue to which */
								/* it is linked. */
								binp->clause->flags|=INDISPOSALQUEUE;
								binp->clause->prevdisp=kbset.lastdisp;
								kbset.lastdisp=binp->clause;
								kbset.prstats.bksubsresltns++;

								/* Add new simplified clause to KB as UNPROC. */
								newcl->flags=UNPROC;
								newcl->inference=BKSUBSRESLTNS;
								if (NOMEMORY==AddBinClause2KB(newcl,&kbset,0)){
									MYFREE(newcl->part2.bin);
									MYFREE(newcl);
									MYFREE(equality);
									return(NOMEMORY);
								}

								/* Set empty clause. */
								MYFREE(equality);
								if (NULL==(kbset.frstprfnode=MYALLOC(sizeof(proofnode)))){
									return(NOMEMORY);
								}
								kbset.frstprfnode->type=ACLAUSE;
								kbset.frstprfnode->ptr.Aclause=newcl;
								return(1);
								break;
						}
					}
				}
			} while(ii);
		}
	}

	/* Free memory and return unsuccessful simplification. */
	MYFREE(equality);
	return(0);
} /* BkSubsResltnUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform subsumption resolution inference)  OCJ
 *
 *    This function tries to perform subsumption resolution simplification
 *    of a clause by another clause. The simplified clause is placed in
 *    new allocated storage and its parents are set, but no other data is
 *    included in the clause cmprefix.
 *
 *    The check for a valid subsumption resolution is done by negating
 *    literal2 and checking that the clause in binp subsumes the input
 *    clause.
 *
 *    It is the caller responsibility to link and/or unlink the clauses
 *    and set the other prefix fields.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to cmprefix structure of clause to be simplified.
 *    binp: Pointer to binprefix structure of clause that simplifies
 *          input clause.
 *    literal1: Pointer to literal in clause that must be an instance
 *              of the negation of literal2 in binp.
 *    literal2: Pointer to the literal in binp whose negation must be a
 *              generalization of a literal1 in clause.
 *    pnewcl: Address of pointer to cmprefix structure of new clause to be
 *            allocated for the clause that results from simplification.
 *
 *  RETURNS:
 *
 *    0 -> The clause was not simplified.
 *    1 -> The clause was successfully simplified.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t SubsResltn(cmprefix *clause,binprefix *binp,uint8_t *literal1,uint8_t *literal2,cmprefix **pnewcl){
	auto uint8_t *literald;                              /* Pointer to literal that will be deleted in clause */
	auto subst Subst;                                    /* Auxiliary substitution */
	auto binprefix *ptr1;                                /* Auxiliary pointer */
	auto int32_t ii,nn;                                  /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                     /* Auxiliary pointer */

	/* Debug. */
	ptr10=Decompile(clause->part2.bin);
	MYFREE(ptr10);
	ptr10=Decompile(binp);
	MYFREE(ptr10);
	#endif

	/* Initialize substitution. */
	Subst.buffer=NULL;
	Subst.substsize=0;

	/* Check if this is a valid subsumption resolution. */
	literal2[0]^=NEGATED;
	literald=literal2;
	ii=IsSubsumedByClause(&clause->part2.bin->formula[0],clause->part2.bin->literals,
			binp,&Subst,literal2,literal1,&literald);
	MYFREE(Subst.buffer);
	literal2[0]^=NEGATED;
	if (ii!=1){
		return(ii);
	}

	/* Allocate storage for new clause and copy input binary clause. */
	if (NULL==((*pnewcl)=MYALLOC(sizeof(cmprefix)))){
		return(NOMEMORY);
	}
	nn=clause->part2.bin->size+binpsize;
	if (NULL==((*pnewcl)->part2.bin=MYALLOC(nn))){
		MYFREE(*pnewcl);
		return(NOMEMORY);
	}
	memcpy((*pnewcl)->part2.bin,clause->part2.bin,nn);

	/* Delete literal in new clause. */
	DeleteLiteral((*pnewcl)->part2.bin,
			((uint8_t *)(*pnewcl)->part2.bin)+(literald-(uint8_t *)clause->part2.bin));

	/* Reallocate the new clause to the exact length needed. */
	if (NULL==(ptr1=MYREALLOC((*pnewcl)->part2.bin,binpsize+(*pnewcl)->part2.bin->size))){
		MYFREE((*pnewcl)->part2.bin);
		MYFREE(*pnewcl);
		return(NOMEMORY);
	}

	/* Set simplified clause binprefix data. */
	(*pnewcl)->part2.bin=ptr1;
	ptr1->oriented=0;
	ptr1->maxvarnb=clause->part2.bin->maxvarnb;
	ptr1->clause=*pnewcl;

	/* Debug. */
	#ifdef DEBUGCODE
	ptr10=Decompile((*pnewcl)->part2.bin);
	MYFREE(ptr10);
	#endif

	/* Set simplified clause cmprefix data. */
	(*pnewcl)->parent1=clause;
	(*pnewcl)->parent2=binp->clause;
	(*pnewcl)->part2.bin->agedist=clause->part2.bin->agedist;
	if (binp->sinedist<clause->part2.bin->sinedist){
		(*pnewcl)->part2.bin->sinedist=binp->sinedist;
	} else {
		(*pnewcl)->part2.bin->sinedist=clause->part2.bin->sinedist;
	}

	return(1);
} /* SubsResltn */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform subsumption resolution inference for unit clause)  OCJ
 *
 *    This function tries to perform subsumption resolution simplification
 *    of a clause by another clause for type 8 problems (unit equality).
 *    The simplified clause is placed in new allocated storage and its
 *    parents are set, but no other data is included in the clause cmprefix.
 *
 *    The check for a valid subsumption resolution is done by negating
 *    literal2 and checking that the clause in binp subsumes the input
 *    clause.
 *
 *    It is the caller responsibility to link and/or unlink the clauses
 *    and set the other prefix fields.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to cmprefix structure of clause to be simplified.
 *    binp: Pointer to binprefix structure of clause that simplifies
 *          input clause.
 *    literal1: Pointer to literal in clause that must be an instance
 *              of the negation of literal2 in binp.
 *    literal2: Pointer to the literal in binp whose negation must be a
 *              generalization of a literal1 in clause.
 *    pnewcl: Address of pointer to cmprefix structure of new clause to be
 *            allocated for the clause that results from simplification.
 *
 *  RETURNS:
 *
 *    0 -> The clause was not simplified.
 *    1 -> The clause was successfully simplified.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t SubsResltnUEQ(cmprefix *clause,binprefix *binp,uint8_t *literal1,uint8_t *literal2,cmprefix **pnewcl){
	auto subst Subst;                                    /* Auxiliary substitution */
	auto cmprefix *auxcl;                                /* Auxiliary clause to build empty clause */
	auto int32_t ii;                                     /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                     /* Auxiliary pointer */

	/* Debug. */
	ptr10=Decompile(clause->part2.bin);
	MYFREE(ptr10);
	ptr10=Decompile(binp);
	MYFREE(ptr10);
	#endif

	/* Initialize substitution. */
	Subst.buffer=NULL;
	Subst.substsize=0;

	/* Check if this is a valid subsumption resolution. */
	literal2[0]^=NEGATED;
	if (0==(ii=Generalize(&binp->formula[0],&clause->part2.bin->formula[0],&Subst,INITSUBST))){
		ii=SymEquGeneralize(&binp->formula[0],&clause->part2.bin->formula[0],&Subst,INITSUBST);
	}
	MYFREE(Subst.buffer);
	literal2[0]^=NEGATED;
	if (ii!=1){
		return(ii);
	}

	/* Allocate and set new empty clause. */
	if ((auxcl=MYALLOC(sizeof(cmprefix)))==NULL){
		return(NOMEMORY);
	}
	if ((auxcl->part2.bin=MYALLOC(1+binpsize))==NULL){
		MYFREE(auxcl);
		return(NOMEMORY);
	}
	auxcl->parent1=clause;
	auxcl->parent2=binp->clause;
	auxcl->part2.bin->formula[0]=UNITEND;
	auxcl->part2.bin->clause=auxcl;
	auxcl->part2.bin->ovly.asserts=auxcl->part2.bin->lockasserts=NULL;
	auxcl->part2.bin->literals=0;
	auxcl->part2.bin->size=1;
	*pnewcl=auxcl;

	return(1);
} /* SubsResltnUEQ */

#ifdef UNITCLRESOLUTION
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform unit clause resolution inference)  OCJ
 *
 *    This function performs the resolution inference between the
 *    unit clause passed as argument and another matching active
 *    or passive unit clause in the working KB. However, as in this
 *    case a successful resolution may mean solving the problem
 *    or at least generate a contradiction clause this resolution
 *    is exceptionally considered a simplification inference.
 *
 *    It is assumed that the clause is a unit clause but this
 *    is not verified.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause with which inferences will be made.
 *
 *  RETURNS:
 *
 *    0 -> An empty clause without assertions was NOT inferred.
 *    1 -> An empty clause without assertions was inferred.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *
 *--------------------------------------------------------------*/
int32_t UCResolution(cmprefix *clause){
	auto cmprefix *newclause;                            /* Inferred clause */
	auto cmprefix *parent;                               /* Second parent clause */
	auto subst Subst;                                    /* Substitution */
	auto uint8_t *equality;                              /* Pointer to equality with root terms in reverse order */
	auto uint8_t *eqterms;                               /* Pointer to equality root terms in reverse order */
	auto uint8_t *queryitm;                              /* Pointer to QueryDscTree() query item */
	auto int32_t size;                                   /* Size of eqterms buffer */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5;          /* Auxiliary pointers */
	auto int32_t ii,jj,kk,mm,nn,rr,ss;                   /* Auxiliary */

	/* Initialize substitution, stack and equality root terms buffer. */
	Subst.buffer=NULL;
	eqterms=equality=NULL;
	size=0;

	/* Get pointer to appropriate discrimination tree. */
	ptr1=&clause->part2.bin->formula[0];
	if (ptr1[0]&PREDICATE){
		if (ptr1[0]&NEGATED){
			ptr2=(((symbol *)&ptr1[1])->symbol)->discr[7];
		} else {
			ptr2=(((symbol *)&ptr1[1])->symbol)->discr[6];
		}
	} else {
		if (ptr1[0]&NEGATED){
			ptr2=equkey.discr[7];
		} else {
			ptr2=equkey.discr[6];
		}
	}

	/* Loop for equality symmetry management. */
	ptr4=NextItem(ptr1,IMMED);
	mm=(ptr1[0]&EQUALITY?2:1);
	for (kk=0;kk<mm;kk++){

		/* This is the first iteration. */
		if (kk==0){
			queryitm=ptr1;

		/* This is the second iteration for an equality literal. */
		/* Build the set of root terms in reverse order. */
		} else {
			ptr3=NextItem(ptr4,OVERSUBTERMS);
			rr=ptr3-ptr4;
			ss=NextItem(ptr3,OVERSUBTERMS)-ptr3;
			if (size==0){
				if (NULL==(equality=MYALLOC(size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(NOMEMORY);
					eqterms=&equality[1+sizeof(symbol)];
				}
			} else if (size<(2+sizeof(symbol)+rr+ss)){
				if (NULL==(ptr5=MYREALLOC(equality,size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(NOMEMORY);
				}
				equality=ptr5;
				eqterms=&equality[1+sizeof(symbol)];
			}
			memcpy(equality,ptr1,1+sizeof(symbol));
			memcpy(eqterms,ptr3,ss);
			memcpy(&eqterms[ss],ptr4,rr);
			eqterms[rr+ss]=UNITEND;
			ptr4=eqterms;
			queryitm=equality;
		}

		/* Loop through candidates for unification. */
		jj=1;
		nn=0;
		while (jj){

			/* Check timeout and solution by other process. */
			if ((procctl->status==TIMEOUT)||(procctl->status==UNSATISFIABLE)
					||(procctl->status==SATISFIABLE)){
				alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
				alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
				return(TIMEOUT);
			}
			if (procctl->status==NOMEMORY){
				MYFREE(Subst.buffer);
				MYFREE(equality);
				return(procctl->status);
			}

			/* Get candidate for unification. */
			if (0!=(jj=QueryDscTree(queryitm,&ptr3,ptr2,UNIFICATION,nn))){

				/* Check unification. */
				nn=1;
				switch (Unify(queryitm,ptr3,&Subst,ITEM2SEC|NEGATED|INITSUBST)){

					/* Not enough memory. */
					case NOMEMORY:
						MYFREE(Subst.buffer);
						MYFREE(equality);
						return(NOMEMORY);
						break;

					/* Candidate unifies with literal. */
					case 1:

						/* Generate empty clause. */
						if (NULL==(newclause=MYALLOC(sizeof(cmprefix)))){
							return(NOMEMORY);
						}
						if (NULL==((newclause->part2.bin)=MYALLOC(binpsize+1))){
							MYFREE(newclause);
							return(NOMEMORY);
						}
						newclause->part2.bin->formula[0]=UNITEND;
						newclause->part2.bin->clause=newclause;
						newclause->part2.bin->oriented=0;
						parent=((binprefix *)(ptr3-(binpsize+((symbol *)&ptr3[1])->offset)))->clause;
						/*if (NOMEMORY==AddAssertions(clause->part2.bin,parent->part2.bin,newclause->part2.bin)){
							MYFREE(newclause->part2.bin);
							MYFREE(newclause);
							return(NOMEMORY);
						}*/
						newclause->parent1=clause;
						newclause->parent2=parent;
						if (parent->part2.bin->agedist<clause->part2.bin->agedist){
							(*inferred)->part2.bin->agedist=clause->part2.bin->agedist+1;
						} else {
							(*inferred)->part2.bin->agedist=parent->part2.bin->agedist+1;
						}
						if (parent->part2.bin->sinedist<clause->part2.bin->sinedist){
							newclause->part2.bin->sinedist=parent->part2.bin->sinedist;
						} else {
							newclause->part2.bin->sinedist=clause->part2.bin->sinedist;
						}
						newclause->inference=RESOLUTION;

						/* Add empty clause as UNPROC and return. */
						newclause->flags=UNPROC;
						if (NOMEMORY==AddBinClause2KB(newclause,&kbset,0)){
							/*if (newclause->part2.bin->ovly.asserts!=NULL){
								MYFREE(newxlause->part2.bin->ovly.asserts);
							}*/
							MYFREE(newclause->part2.bin);
							MYFREE(newclause);
							return(NOMEMORY);
						}
						if (NULL==(kbset.frstprfnode=MYALLOC(sizeof(proofnode)))){
							/*if (newclause->part2.bin->ovly.asserts!=NULL){
								MYFREE(newxlause->part2.bin->ovly.asserts);
							}*/
							MYFREE(newclause->part2.bin);
							MYFREE(newclause);
							return(NOMEMORY);
						}
						kbset.frstprfnode->type=ACLAUSE;
						kbset.frstprfnode->ptr.Aclause=newclause;
						kbset.status=UNSATISFIABLE;
						return(1);
						break;
				}
			}
		}
	}

	/* Free memory and return. */
	MYFREE(Subst.buffer);
	MYFREE(equality);
	return(0);
} /* UCResolution */
#endif
