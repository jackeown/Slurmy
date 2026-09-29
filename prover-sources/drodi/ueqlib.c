/*
 ============================================================================
 Name        : ueqlib.c
 Author      : Oscar Contreras
 Version     :
 Copyright   : Copyright (C) 2015-2016 Oscar Contreras. All rights reserved.
 Description : First order logic theorem prover in C
 ============================================================================
 */

/****************************************************************
*
*      ueqlib (Specific functions for unfailing completion algorithm)
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
 *    This source contains most of the of the Drodi package specific functions for
 *    the completion without failure algorithm to unit equality type 8 problems.
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

/** Global variables for this module: only those that need initialization in their definition. */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Normalize an equation)  OCJ
 *
 *    This function performs the normalization of an (in)equality for
 *    type 8 problems when using a completion without failure strategy.
 *    The (in)equality) is normalized using the ACTIVE equalities.
 *
 *    The inference rules used used for this normalization are the
 *    following, as described in the document "Completion without failure"
 *    by Leo Bachmair:
 *    - If flag is zero then simplification, composition and collapse
 *      inferences are used.
 *    - If flag is 1 then simplification2, composition2 and collapse2
 *      inferences are also used in addition to the inferences indicated
 *      above.
 *    - Other flag values produce unpredictable results and are not
 *      checked.
 *
 *    This function repeatedly calls FwdNormalize() function until no
 *    simplification is done. Any required memory freeing is performed
 *    by FwdNormalize() function even in the case of error and also
 *    for the memory allocated to *pclause by the caller.
 *
 *    IMPORTANT: The clause addressed by pclause must not be linked
 *    to any KB because it may be possibly decompiled and added to
 *    working KB as text formula.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    pclause: Address of pointer to cmprefix structure of clause
 *             to be simplified.
 *    type: If this is zero then only oriented rules in ACTIVE clauses
 *          are used for simplification. If this is 1 then all ACTIVE
 *          clauses are used for simplification (see comments above).
 *          Other values produce unpredictable results and are not
 *          checked.
 *
 *  RETURNS:
 *
 *    0 -> Process performed successfully.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *    UNKNOWN -> Problem is UNKNOWN because it was solved by another process.
 *
 *
 *--------------------------------------------------------------*/
int32_t Normalize(cmprefix **pclause,int32_t type){
	auto int32_t ii;                                     /* Auxiliary */
	for (ii=1;ii!=0;){
		switch (ii=FwdNormalize(pclause,type)){
			case TIMEOUT:
			case NOMEMORY:
			case UNKNOWN:
				return(ii);
				break;
		}
	}
	return(0);
} /* Normalize */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Forward simplification for UEQOTT and UEQDSC algorithms)  OCJ
 *
 *    This function performs a single forward simplification if possible
 *    of an (in)equality for type 8 problems when using a completion without
 *    failure strategy. The (in)equality is simplified using the ACTIVE
 *    equalities.
 *
 *    The inference rules used used for this simplification are the
 *    following, as described in the document "Completion without failure"
 *    by Leo Bachmair:
 *    - If flag is zero then simplification, composition and collapse
 *      inferences are used.
 *    - If flag is 1 then simplification2, composition2 and collapse2
 *      inferences are also used in addition to the inferences indicated
 *      above.
 *    - Other flag values produce unpredictable results and are not
 *      checked.
 *
 *    Before performing the above inferences a clause orientation is
 *    performed as part of the Knuth-Bendix Completion Procedure as
 *    described in the above document.
 *
 *    Any required memory freeing is performed by this function even in
 *    the case of error and also for the memory allocated to *pclause by
 *    the caller.
 *
 *    IMPORTANT: The clause addressed by pclause must not be linked
 *    to any KB because it may be possible decompiled and added to
 *    working KB as text formula.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    pclause: Address of pointer to cmprefix structure of clause
 *             to be simplified.
 *    type: If this is zero then only oriented rules in ACTIVE clauses
 *          are used for simplification. If this is 1 then all ACTIVE
 *          clauses are used for simplification (see comments above).
 *          Other values produce unpredictable results and are not
 *          checked.
 *
 *  RETURNS:
 *
 *    0 -> Process performed successfully and no simplification was done.
 *    1 -> Process performed successfully and a simplification was done.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *    UNKNOWN -> Problem is UNKNOWN because it was solved by another process.
 *
 *
 *--------------------------------------------------------------*/
int32_t FwdNormalize(cmprefix **pclause,int32_t type){
	auto cmprefix *clause;                               /* Pointer to clause to be simplified */
	auto cmprefix *newclause;                            /* Clause result of simplification */
	auto uint8_t *rtterm1,*rtterm2;                      /* Pointers to root terms in *pclause */
	auto uint8_t *rweq;                                  /* Pointer to binary rewriting equality formula */
	auto binprefix *binprweq;                            /* Pointer to binprefix structure of rewriting equality */
	auto uint8_t *unrttrm;                               /* Pointer to unifying rewriting root term */
	auto uint8_t *dstrttrm;                              /* Pointer to the destination rewriting root term */
	auto subst Subst,Subst2;                             /* Substitutions for Generalize() calls */
	auto uint8_t *ptr1,*ptr2,*ptr3;                      /* Auxiliary pointers */
	auto uint8_t *ptr4,*ptr5,*ptr6;                      /* Auxiliary pointers */
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	auto int32_t ii,kk,mm,nn,oo,pp;                      /* Auxiliary */

	/* Get pointers to clause equality and root terms. */
	clause=*pclause;
	ptr3=&clause->part2.bin->formula[0];
	rtterm1=NextItem(ptr3,IMMED);
	rtterm2=NextItem(rtterm1,OVERSUBTERMS);

	/* Orient (in)equality if it is not oriented as required */
	/* by the Knuth-Bendix Completion Procedure. */
	if ((clause->part2.bin->oriented==0)&&(0==(MAXIMUM&rtterm1[0]))&&(0==(MAXIMUM&rtterm2[0]))){

		/* Reset MAXIMUM and SELECTED flags in equality root terms. */
		rtterm1[0]&=(~(MAXIMUM|SELECTED));
		rtterm2[0]&=(~(MAXIMUM|SELECTED));

		/* Orient (in)equality by setting the SELECTED and MAXIMUM flags */
		/* for equality root terms as appropriate. */
		/* Release equality tree data if the root terms ordering */
		/* is completely defined. */
		ii=CompareEquTerms(clause->part2.bin,rtterm1,rtterm2);
		clause->part2.bin->oriented=1;
		switch (ii){
			case NOMEMORY:
				return(NOMEMORY);
				break;
			case EQUAL:
				break;
			case FAILURE:
				rtterm1[0]|=SELECTED;
				rtterm2[0]|=SELECTED;
				break;
			case GREATER:
				rtterm1[0]|=(SELECTED|MAXIMUM);
				break;
			case LESSER:
				rtterm2[0]|=(SELECTED|MAXIMUM);
				break;
		}
	}

	/* Initialize clause pointer and substitutions for Generalize() calls. */
	Subst.buffer=NULL;
	Subst.substsize=0;
	Subst2.buffer=NULL;
	Subst2.substsize=0;

	/* Debug. */
	#ifdef DEBUGCODE
	ptr10=Decompile(clause->part2.bin);
	MYFREE(ptr10);
	#endif

	/* Loop through clause items. */
	for (ptr1=rtterm1;ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){

		/* Iterate if item is a variable. */
		if (VARIABLE&ptr1[0]){
			continue;
		}

		/* Loop through ACTIVE (always) and PASSIVE (if algorithm is UEQOTT) */
		/* clauses as appropriate. */
		if (kbset.opts.algorithm==UEQOTT){
			ii=2;
		} else {
			ii=1;
		}
		for (kk=0;kk<ii;kk++){

			/* Get pointer to top tree node. */
			ptr2=(((symbol *)&ptr1[1])->symbol)->discr[kk];

			/* Loop through generalization candidates. */
			nn=0;
			do {

				/* Check timeout and solution by other process. */
				if (procctl->status==TIMEOUT){
					MYFREE(clause->part2.bin->ovly.cptopterm);
					MYFREE(clause->part2.bin);
					MYFREE(clause);
					MYFREE(Subst.buffer);
					MYFREE(Subst2.buffer);
					return(TIMEOUT);
				}
				if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
					MYFREE(clause->part2.bin->ovly.cptopterm);
					MYFREE(clause->part2.bin);
					MYFREE(clause);
					MYFREE(Subst.buffer);
					MYFREE(Subst2.buffer);
					return(UNKNOWN);
				}
				if (procctl->status==NOMEMORY){
					MYFREE(clause->part2.bin->ovly.cptopterm);
					MYFREE(clause->part2.bin);
					MYFREE(clause);
					MYFREE(Subst.buffer);
					MYFREE(Subst2.buffer);
					return(procctl->status);
				}

				/* Get a term in a candidate simplifying clause. */
				if (0!=(ii=QueryGenDscTree(ptr1,&unrttrm,ptr2,nn))){
					nn=1; /* Indicate a discrimination tree query has been done. */

					/* Get a pointer to the binary formula of rewriting equation */
					/* and to the destination rewriting root term candidates. */
					rweq=unrttrm-((symbol *)&unrttrm[1])->offset;
					binprweq=(binprefix *)(rweq-binpsize);
					if (unrttrm==(ptr4=NextItem(rweq,IMMED))){
						dstrttrm=NextItem(unrttrm,OVERSUBTERMS);
					} else {
						dstrttrm=ptr4;
					}

					/* Iterate if clause is not oriented or weakly oriented and type is 0. */
					if ((type==0)&&(0==(MAXIMUM&unrttrm[0]))&&(0==(WEAKLYORIENTED&binprweq->clause->flags))){
						continue;
					}

					/* Shift variables of clause being normalized. */
					ShiftUEQVarNbr(clause->part2.bin,binprweq->maxvarnb+1);

					/* Check generalization. */
					switch (Generalize(unrttrm,ptr1,&Subst,INITSUBST)){

						/* Not enough memory. */
						case NOMEMORY:
							MYFREE(clause->part2.bin->ovly.cptopterm);
							MYFREE(clause->part2.bin);
							MYFREE(clause);
							MYFREE(Subst.buffer);
							MYFREE(Subst2.buffer);
							return(NOMEMORY);
							break;

						/* Valid generalization. */
						case 1:

							/* Debug. */
							#ifdef DEBUGCODE
							ptr10=Decompile(binprweq);
							MYFREE(ptr10);
							#endif

							/* Rewriting equation is oriented or weakly oriented. */
							if ((MAXIMUM&unrttrm[0])||(WEAKLYORIENTED&binprweq->clause->flags)){

								/* Check collapse specialization ordering constraint. In order to understand */
								/* how this check is done it must be taken into account that for the check */
								/* to fail it is necessary for the root term to be a generalization of the */
								/* rewriting term. As the rewriting term is a generalization of the current */
								/* item ptr1 then ptr1 itself must be the root term. */
								if (ptr1==rtterm1){
									if (MAXIMUM&rtterm1[0]){
										if (NOMEMORY==(mm=Generalize(ptr1,unrttrm,&Subst2,INITSUBST))){
											MYFREE(clause->part2.bin->ovly.cptopterm);
											MYFREE(clause->part2.bin);
											MYFREE(clause);
											MYFREE(Subst.buffer);
											MYFREE(Subst2.buffer);
											return(NOMEMORY);
										} else if (mm){
											UnshiftUEQVarNbr(clause->part2.bin,binprweq->maxvarnb+1);
											break;
										}
									}
								} else if ((ptr1==rtterm2)&&(MAXIMUM&rtterm2[0])){
									if (NOMEMORY==(mm=Generalize(ptr1,unrttrm,&Subst2,INITSUBST))){
										MYFREE(clause->part2.bin->ovly.cptopterm);
										MYFREE(clause->part2.bin);
										MYFREE(clause);
										MYFREE(Subst.buffer);
										MYFREE(Subst2.buffer);
										return(NOMEMORY);
									} else if (mm){
										UnshiftUEQVarNbr(clause->part2.bin,binprweq->maxvarnb+1);
										break;
									}
								}

								/* Check if a weak rewrite is valid. */
								if (WEAKLYORIENTED&binprweq->clause->flags){
									if (1==(mm=CheckWeakRewrite(ptr1,dstrttrm,&Subst,0))){
										UnshiftUEQVarNbr(clause->part2.bin,binprweq->maxvarnb+1);
										break;
									}
									if (mm==NOMEMORY){
										MYFREE(clause->part2.bin->ovly.cptopterm);
										MYFREE(clause->part2.bin);
										MYFREE(clause);
										MYFREE(Subst.buffer);
										MYFREE(Subst2.buffer);
										return(NOMEMORY);
									}
								}

							/* Rewriting equation is not oriented or weakly oriented. */
							} else {

								/* Check that the rewriting equation is oriented under the substitution. */
								if (NOMEMORY==(mm=CheckEqRootOrder2(unrttrm,dstrttrm,&Subst,MAXIMUM))){
									MYFREE(clause->part2.bin->ovly.cptopterm);
									MYFREE(clause->part2.bin);
									MYFREE(clause);
									MYFREE(Subst.buffer);
									MYFREE(Subst2.buffer);
									return(NOMEMORY);
								} else if (mm==0){
									UnshiftUEQVarNbr(clause->part2.bin,binprweq->maxvarnb+1);
									break;
								}

								/* Check simplification2 and collapse2 additional specialization ordering constraints. */
								/* In order to understand how this check is done it must be taken into account that */
								/* for the check to fail it is necessary for the root term to be a generalization */
								/* of the rewriting term. As the rewriting term is a generalization of the */
								/* current item ptr1 then ptr1 itself must be the root term. */
								if(ptr1>=rtterm2){
									ptr4=rtterm2;
									ptr5=rtterm1;
								} else {
									ptr4=rtterm1;
									ptr5=rtterm2;
								}
								if (((MAXIMUM&ptr4[0])||(0==(MAXIMUM&ptr5[0])))&&(ptr1==ptr4)){
									if (NOMEMORY==(mm=Generalize(ptr1,unrttrm,&Subst2,INITSUBST))){
										MYFREE(clause->part2.bin->ovly.cptopterm);
										MYFREE(clause->part2.bin);
										MYFREE(clause);
										MYFREE(Subst.buffer);
										MYFREE(Subst2.buffer);
										return(NOMEMORY);
									} else if (mm){
										UnshiftUEQVarNbr(clause->part2.bin,binprweq->maxvarnb+1);
										break;
									}
								}
							}

							/* Compute necessary pointers and lengths to build rewritten result. */
							ptr4=NextItem(ptr1,OVERSUBTERMS);
							if (NULL==(ptr5=ApplySubst2Term(dstrttrm,&Subst,0))){
								MYFREE(clause->part2.bin->ovly.cptopterm);
								MYFREE(clause->part2.bin);
								MYFREE(clause);
								MYFREE(Subst.buffer);
								MYFREE(Subst2.buffer);
								return(NOMEMORY);
							}
							mm=ptr1-ptr3;
							oo=NextItem(ptr5,OVERSUBTERMS)-ptr5;
							pp=NextItem(ptr3,OVERSUBTERMS)-ptr4;

							/* Allocate memory for new clause. */
							if (NULL==(newclause=MYALLOC(sizeof(cmprefix)))){
								MYFREE(clause->part2.bin->ovly.cptopterm);
								MYFREE(clause->part2.bin);
								MYFREE(clause);
								MYFREE(Subst.buffer);
								MYFREE(Subst2.buffer);
								return(NOMEMORY);
							}
							if (NULL==((newclause->part2.bin)=MYALLOC(binpsize+1+mm+oo+pp))){
								MYFREE(clause->part2.bin->ovly.cptopterm);
								MYFREE(clause->part2.bin);
								MYFREE(clause);
								MYFREE(newclause);
								MYFREE(Subst.buffer);
								MYFREE(Subst2.buffer);
								return(NOMEMORY);
							}

							/* Initialize new clause cmprefix and binprefix structures. */
							/* Top term is freed as it is no longer necessary, the */
							/* new clause will not be checked for connectedness. */
							newclause->flags=(~NUMBERED)&clause->flags;
							newclause->parent1=binprweq->clause;
							newclause->parent2=clause;
							newclause->prevdisp=NULL;
							if (binprweq->agedist<clause->part2.bin->agedist){
								newclause->part2.bin->agedist=clause->part2.bin->agedist+1;
							} else {
								newclause->part2.bin->agedist=binprweq->agedist+1;
							}
							if (binprweq->sinedist<clause->part2.bin->sinedist){
								newclause->part2.bin->sinedist=binprweq->sinedist;
							} else {
								newclause->part2.bin->sinedist=clause->part2.bin->sinedist;
							}
							newclause->inference=FWDEMODULATION;
							kbset.prstats.fwdemodulations++;
							newclause->part2.bin->literals=1;
							newclause->part2.bin->signature=0;
							newclause->part2.bin->locksignature=0;
							newclause->part2.bin->clause=newclause;
							newclause->part2.bin->maxvarnb=clause->part2.bin->maxvarnb;
							newclause->part2.bin->oriented=0;
							newclause->part2.bin->ovly.cptopterm=NULL;
							MYFREE(clause->part2.bin->ovly.cptopterm);
							clause->part2.bin->ovly.cptopterm=NULL;

							/* Build new clause. */
							ptr6=&newclause->part2.bin->formula[0];
							memcpy(ptr6,ptr3,mm);
							memcpy(&ptr6[mm],ptr5,oo);
							MYFREE(ptr5);
							mm+=oo;
							memcpy(&ptr6[mm],ptr4,pp);
							mm+=pp;
							ptr6[mm]=UNITEND;
							newclause->part2.bin->size=mm+1;

							/* Adjust offset and nextoff fields in new clause terms. */
							UpdateParams2(ptr6);

							/* Unshift variables of new clause and clause being normalized. */
							UnshiftUEQVarNbr(newclause->part2.bin,binprweq->maxvarnb+1);
							UnshiftUEQVarNbr(clause->part2.bin,binprweq->maxvarnb+1);

							/* Reset root terms MAXIMUM and SELECTED flags in new clause. */
							ptr4=NextItem(ptr6,IMMED);
							ptr4[0]&=(~(MAXIMUM|SELECTED));
							ptr5=NextItem(ptr4,OVERSUBTERMS);
							ptr5[0]&=(~(MAXIMUM|SELECTED));

							/* If original simplified clause was oriented and the rewritten */
							/* root term was not the MAXIMUM root term then the inference */
							/* was composition or composition2. Keep the new clause oriented. */
							if (ptr1>=rtterm2){
								if (MAXIMUM&rtterm1[0]){
									ptr4[0]|=(MAXIMUM|SELECTED);
									newclause->part2.bin->oriented=clause->part2.bin->oriented;
								}
							} else if (MAXIMUM&rtterm2[0]){
								ptr5[0]|=(MAXIMUM|SELECTED);
								newclause->part2.bin->oriented=clause->part2.bin->oriented;
							}

							/* Convert original clause to text form and add it to working KB. */
							if (NULL==(ptr10=Decompile(clause->part2.bin))){
								MYFREE(clause->part2.bin);
								MYFREE(clause);
								MYFREE(newclause->part2.bin);
								MYFREE(newclause);
								MYFREE(Subst.buffer);
								MYFREE(Subst2.buffer);
								return(NOMEMORY);
							}
							MYFREE(clause->part2.bin);
							clause->part2.text=ptr10;
							AddTxtFormula2KB(clause,clause->inference,&kbset);

							/* Set normalized clause, free memory and return. */
							*pclause=newclause;
							MYFREE(Subst.buffer);
							MYFREE(Subst2.buffer);
							return(1);
							break;

						/* Not valid generalization, unshift clause variables. */
						default:
							UnshiftUEQVarNbr(clause->part2.bin,binprweq->maxvarnb+1);
							break;
					}
				}
			} while(ii);
		}
	}

	/* Free memory and return clause not simplified. */
	MYFREE(Subst.buffer);
	MYFREE(Subst2.buffer);
	return(0);
} /* FwdNormalize */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if an (in)equality is an equational tautology)  OCJ
 *
 *    This function checks if an (in)equality in internal compiled format
 *    is an equational tautology, that is, for clauses of the form t=t
 *    or "a"="a" or "a"!="b" where t is a term and "a" and "b" are
 *    "distinct objects".
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: pointer to an (in)equality in binary format. If it is not
 *            a pointer to an (in)equality then results are unpredictable.
 *
 *  RETURNS:
 *
 *    0 -> Clause is not a tautology.
 *    1 -> Clause is a tautology.
 *
 *
 *--------------------------------------------------------------*/
int32_t IsEqTautology(cmprefix *clause){
	auto hashchain *rtterm1s,*rtterm2s;                  /* Pointers to root term symbols in equalities */
	auto uint8_t *ptr1,*ptr2,*ptr3;                      /* Auxiliary pointers */

	/* Detect equality tautologies of identical terms or "distinct objects". */
	ptr1=&clause->part2.bin->formula[0];
	ptr2=NextItem(ptr1,IMMED);
	ptr3=NextItem(ptr2,OVERSUBTERMS);
	if (0==(NEGATED&ptr1[0])){
		if (IsEqual(ptr2,ptr3,0)){
			return(1);
		}
	} else {
		if ((FUNCTION&ptr2[0])&&(FUNCTION&ptr3[0])){
			rtterm1s=((symbol *)&ptr2[1])->symbol;
			rtterm2s=((symbol *)&ptr3[1])->symbol;
			if ((rtterm1s!=rtterm2s)&&(DISTINCTOBJECT&rtterm1s->type)
					&&(DISTINCTOBJECT&rtterm2s->type)){
				return(1);
			}
		}
	}

	return(0);
} /* IsEqTautology */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if a critical pair is connected)  OCJ
 *
 *    This function checks if a critical pair in internal compiled format
 *    is connected as described in section 5 of document "Critical
 *    Pair Criteria for Completion" by Leo Bachmair and in section 3.2
 *    of document "Twee: An Equational Theorem Prover (System Description)"
 *    by Nicholas Smallbone.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: pointer to an equality in binary format that is a critical pair.
 *
 *  RETURNS:
 *
 *    0 -> Critical pair is not connected.
 *    1 -> Critical pair is connected.
 *    TIMEOUT -> Time has expired.
 *    NOMEMORY -> Not enough memory.
 *    UNKNOWN -> Problem is UNKNOWN because it was solved by another process.
 *
 *
 *--------------------------------------------------------------*/
int32_t IsCPConnected(cmprefix *clause){
	auto uint8_t *mxterm;                                /* Pointer to maximum root term under substitution */
	auto uint8_t *mnterm;                                /* Pointer to minimum root term under substitution */
	auto uint8_t *ptr1,*ptr2;                            /* Auxiliary pointers */
	auto int32_t ii,jj;                                  /* Auxiliary */

	/* If critical pair has not top term then return not connected. */
	if (clause->part2.bin->ovly.cptopterm==NULL){
		return(0);
	}

	/* Loop for both types of ground substitution (see global comments */
	/* of IsConnected() function). */
	ptr1=NextItem(&clause->part2.bin->formula[0],IMMED);
	ptr2=NextItem(ptr1,OVERSUBTERMS);
	for (jj=0;jj<2;jj++){

		/* Get maximum and minimum equality root terms for first ground substitution. */
		if (GREATER==ConnectCompare(ptr2,ptr1,clause->part2.bin->maxvarnb,jj)){
			mxterm=ptr2;
			mnterm=ptr1;
		} else {
			mxterm=ptr1;
			mnterm=ptr2;
		}

		/* Check connectedness for current ground substitution. */
		if (0!=(ii=IsConnected(mxterm,mnterm,clause->part2.bin->ovly.cptopterm,
				clause->part2.bin->maxvarnb,jj?SUBSTITUTION2:0))){
			return(ii);
		}
	}

	return(0);
} /* IsCPConnected */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if two terms are connected)  OCJ
 *
 *    This function checks if two terms are connected with respect
 *    to the top term of the original critical pair for a given
 *    ground substitution (see description of flag argument below)
 *    as described in section 5 of document "Critical Pair Criteria
 *    for Completion" by Leo Bachmair and in section 3.2 of document
 *    "Twee: An Equational Theorem Prover (System Description)"
 *    by Nicholas Smallbone.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    mxterm: pointer to maximum term under the ground substitution.
 *    mnterm: pointer to minimum term under the ground substitution.
 *    topterm: Pointer to original critical pair top term.
 *    maxvarnb: Maximum variable number in mxterm, mnterm and topterm.
 *    flags: SUBSTITUTION2: If this flag is set then a₁≻a₂≻ ... ≻aₚ
 *           ground substitution will be used for variables when
 *           checking ordering constraint to ensure rewrite termination.
 *           If this flag is not set then a₁≺a₂≺ ... ≺aₚ ground
 *           substitution is used.
 *           MXTERMCHECKED: If this flag is set then mxterm has been
 *           checked for being LESSER than the original critical pair
 *           top term. This is useful for oriented rewritings because
 *           then it is not necessary to check for connectedness ordering
 *           constraint.
 *           MNTERMCHECKED: Same as MXTERMCHECKED but for mnterm.
 *
 *  RETURNS:
 *
 *    0 -> Terms are not connected.
 *    1 -> Terms are connected.
 *    TIMEOUT -> Time has expired.
 *    NOMEMORY -> Not enough memory.
 *    UNKNOWN -> Problem is UNKNOWN because it was solved by another process.
 *
 *
 *--------------------------------------------------------------*/
int32_t IsConnected(uint8_t *mxterm,uint8_t *mnterm,uint8_t *topterm,int32_t maxvarnb,int32_t flags){
	auto subst Subst;                                    /* Substitution */
	auto uint8_t *rweq;                                  /* Pointer to binary rewriting equality formula */
	auto binprefix *binprweq;                            /* Pointer to binprefix structure of rewriting equality */
	auto uint8_t *unrttrm;                               /* Pointer to unifying rewriting root term */
	auto uint8_t *dstrttrm;                              /* Pointer to the destination rewriting root term */
	auto uint8_t *newmxterm;                             /* Pointer to new maximum root term under substitution */
	auto uint8_t *newmnterm;                             /* Pointer to new minimum root term under substitution */
	auto dstrbktrk backtrk;                              /* Discrimination tree backtrack structure */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4;                /* Auxiliary pointers */
	auto uint8_t *ptr5,*ptr6,*ptr7;                      /* Auxiliary pointers */
	auto int32_t ff,jj,kk,mm,pp,qq;                      /* Auxiliary */

	/* Substitution initialization. */
	Subst.buffer=NULL;

	/* Return if discrimination trees are empty. */
	if (mxtrfsize==0){
		return(0);
	}

	/* Backtrack structure initialization. */
	if (NULL==(backtrk.treeitems=MYALLOC((((mxtrfsize-sizeof(symbol)-2)/3)+1)*sizeof(*backtrk.treeitems)))){
		return(NOMEMORY);
	}

	/* Loop through items in mxterm. */
	for (ptr1=mxterm,ptr2=NextItem(mxterm,OVERSUBTERMS);ptr1<ptr2;ptr1=NextItem(ptr1,IMMED)){

		/* Iterate if item is a variable. */
		if (VARIABLE&ptr1[0]){
			continue;
		}

		/* Select the discrimination tree to be used. */
		ptr7=(((symbol *)&ptr1[1])->symbol)->discr[0];

		/* Loop through generalization candidates for rewriting. */
		jj=1;
		backtrk.lstmatch=NULL;
		while (jj){

			/* Check timeout and solution by other process. */
			if (procctl->status==TIMEOUT){
				MYFREE(Subst.buffer);
				MYFREE(backtrk.treeitems);
				return(TIMEOUT);
			}
			if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
				MYFREE(Subst.buffer);
				MYFREE(backtrk.treeitems);
				return(UNKNOWN);
			}
			if (procctl->status==NOMEMORY){
				MYFREE(Subst.buffer);
				MYFREE(backtrk.treeitems);
				return(procctl->status);
			}

			/* Get candidate for generalization. */
			if (NOMEMORY==(jj=QueryDscTree2(ptr1,&unrttrm,ptr7,&backtrk,GENERALIZATION))){
				MYFREE(Subst.buffer);
				MYFREE(backtrk.treeitems);
				return(NOMEMORY);
			}

			/* We have a candidate generalization term. */
			if (jj){

				/* Get a pointer to the binary formula of rewriting equation */
				/* and to the destination rewriting root term candidates. */
				rweq=unrttrm-((symbol *)&unrttrm[1])->offset;
				binprweq=(binprefix *)(rweq-binpsize);
				if (unrttrm==(ptr3=NextItem(rweq,IMMED))){
					dstrttrm=NextItem(unrttrm,OVERSUBTERMS);
				} else {
					dstrttrm=ptr3;
				}

				/* Shift variables in candidate binary formula. */
				ShiftUEQVarNbr(binprweq,maxvarnb+1);

				/* Check if candidate is a generalization of the term. */
				if (NOMEMORY==(kk=Generalize(unrttrm,ptr1,&Subst,INITSUBST))){
					UnshiftUEQVarNbr(binprweq,maxvarnb+1);
					MYFREE(Subst.buffer);
					MYFREE(backtrk.treeitems);
					return(NOMEMORY);
				}

				/* We have a valid generalization. */
				if (kk){

					/* Apply substitution to rewriting term. */
					if (NULL==(ptr4=ApplySubst2Term(dstrttrm,&Subst,0))){
						UnshiftUEQVarNbr(binprweq,maxvarnb+1);
						MYFREE(Subst.buffer);
						MYFREE(backtrk.treeitems);
						return(NOMEMORY);
					}

					/* Allocate memory for rewritten term. */
					ptr5=NextItem(mxterm,OVERSUBTERMS);
					ptr6=NextItem(ptr1,OVERSUBTERMS);
					mm=ptr1-mxterm;
					pp=NextItem(ptr4,OVERSUBTERMS)-ptr4;
					qq=ptr5-ptr6;
					if (NULL==(ptr3=MYALLOC(mm+pp+qq+1))){
						UnshiftUEQVarNbr(binprweq,maxvarnb+1);
						MYFREE(Subst.buffer);
						MYFREE(backtrk.treeitems);
						MYFREE(ptr4);
						return(NOMEMORY);
					}

					/* Build the resulting term from the rewrite. */
					memcpy(ptr3,mxterm,mm);
					memcpy(&ptr3[mm],ptr4,pp);
					mm+=pp;
					memcpy(&ptr3[mm],ptr6,qq);
					ptr3[mm+qq]=UNITEND;
					UpdateParams2(ptr3);

					/* Return clause connected if rewritten term equals mnterm. */
					if (IsEqual(ptr3,mnterm,0)){
						UnshiftUEQVarNbr(binprweq,maxvarnb+1);
						MYFREE(Subst.buffer);
						MYFREE(backtrk.treeitems);
						MYFREE(ptr4);
						MYFREE(ptr3);
						return(1);
					}

					/* Ordering constraint to ensure rewrite termination is met. */
					if (GREATER==ConnectCompare(ptr1,ptr4,maxvarnb,flags)){

						/* It is not necessary to check connectedness ordering constraint */
						/* because the rewriting equation is oriented and mxterm has been */
						/* checked for being LESSER than the original critical pair top term. */
						if ((MAXIMUM&unrttrm[0])&&(flags&MXTERMCHECKED)){
							kk=1;

						/* Check connectedness ordering constraint. The maximum variable number */
						/* passed to CompareItems() function must be the value in the rewriting */
						/* equality because the resulting term may have some variables from it. */
						} else {

							/* Quick check. If ptr3 has a variable with a number greater than maxvarnb */
							/* then connectedness ordering constraint is not met. This check must be */
							/* done because if full check with CompareItems() function is done and */
							/* ptr3 has such a variable then and comparison would be slower. */
							for (ptr6=ptr3,kk=1;ptr6[0]!=UNITEND;ptr6=NextItem(ptr6,IMMED)){
								if ((VARIABLE&ptr6[0])&&(maxvarnb<(*(int16_t *)&ptr6[1]))){
									kk=0;
									break;
								}
							}

							/* Full check. This is done only if quick check has succeeded. */
							if (kk){
								switch (CompareItems(ptr3,topterm,maxvarnb)){

									/* Not enough memory. */
									case NOMEMORY:
										UnshiftUEQVarNbr(binprweq,maxvarnb+1);
										MYFREE(Subst.buffer);
										MYFREE(backtrk.treeitems);
										MYFREE(ptr4);
										MYFREE(ptr3);
										return(NOMEMORY);
										break;

									/* Connectedness ordering constraint is met. */
									case LESSER:
										kk=1;
										break;

									/* Connectedness ordering constraint is not met. */
									default:
										kk=0;
										break;
								}
							}
						}

						/* Connectedness ordering constraint is met. */
						if (kk){

							/* Get maximum and minimum equality root terms for first ground substitution */
							/* and compute new flags for iterative call. */
							ff=flags&SUBSTITUTION2;
							if (GREATER==ConnectCompare(mnterm,ptr3,maxvarnb,jj)){
								newmxterm=mnterm;
								newmnterm=ptr3;
								ff|=MNTERMCHECKED;
								if (flags&MNTERMCHECKED){
									ff|=MXTERMCHECKED;
								}
							} else {
								newmxterm=ptr3;
								newmnterm=mnterm;
								ff|=MXTERMCHECKED;
								if (flags&MNTERMCHECKED){
									ff|=MNTERMCHECKED;
								}
							}

							/* Check connectedness for ground substitution. */
							if (0!=(kk=IsConnected(newmxterm,newmnterm,topterm,maxvarnb,ff))){
								UnshiftUEQVarNbr(binprweq,maxvarnb+1);
								MYFREE(Subst.buffer);
								MYFREE(backtrk.treeitems);
								MYFREE(ptr4);
								MYFREE(ptr3);
								return(kk);
							}
						}
					}

					/* Free auxiliary terms. */
					MYFREE(ptr4);
					MYFREE(ptr3);
				}

				/* Unshift variables in candidate binary formula. */
				UnshiftUEQVarNbr(binprweq,maxvarnb+1);
			}
		}
	}

	/* Free memory and return. */
	MYFREE(Subst.buffer);
	MYFREE(backtrk.treeitems);
	return(0);
} /* IsConnected */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check ground joinability of an equation)  OCJ
 *
 *    This function checks if a clause that is an equation is
 *    ground joinable. The test is performed based on the
 *    following documents:
 *    - "On using ground joinable equations in equational
 *      theorem proving" by J. Avenhaus, Th. Hillenbrand  and
 *      B. Lochner.
 *    - "Ordered rewriting and confluence" by "Ursula Martin
 *      and Tobias Nipkow.
 *    - "Twee: An Equational Theorem Prover (System Description)"
 *      by Nicholas Smallbone.
 *
 *    First two kind of tests are performed for every permutation
 *    of grounded variable orderings: one for x₁≻x₂≻...≻xₚ and
 *    another for x₁≽x₂≽...≽xₚ (see CheckGJOrdering1() and
 *    CheckGJOrdering2() global comments). If both checks succeed
 *    then the test succeeds for that permutation. If the the first
 *    check fails then equation is not ground joinable. If the second
 *    check fails and GRJOIN is set to MED then equation is reported
 *    as not ground joinable even if it is. However if ground joinability
 *    is set to ON then one (and only one) ≡ ordering relation is
 *    introduced in the x₁≻x₂≻...≻xₚ ordering relation in place
 *    of a ≻ at different positions and the function is called
 *    recursively. If any of the recursive calls fails then the
 *    equation is not ground joinable.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to equation formula in binary format including
 *             an ending UNITEND byte.
 *    size: Number of bytes of formula including the ending UNITEND
 *          byte.
 *    maxvarnb: Maximum variable number in formula.
 *    varidx1: This indicates that for the first ≡ ordering relation
 *             the variable whose number is perm[varidx2] is unified
 *             with the variable perm[varidx1]. This must be 0 for
 *             the first call to this function.
 *    varidx2: See varidx1 comments. This must be 1 for the first
 *             call to this function.
 *
 *  RETURNS:
 *
 *    0 -> Clause is not ground joinable.
 *    1 -> Clause is ground joinable.
 *    TIMEOUT -> Time has expired.
 *    NOMEMORY -> Not enough memory.
 *    UNKNOWN -> Problem is UNKNOWN because it was solved by another process.
 *    SLICETMOUT -> maximum number of executed hardware instructions was reached.
 *
 *
 *--------------------------------------------------------------*/
int32_t IsGrJoinable(uint8_t *formula,int32_t size,int32_t maxvarnb,int32_t varidx1,int32_t varidx2){
	static int32_t *perm;                                /* Pointer to permutation that defines the ordering of variables */
	static int32_t *ctrl;                                /* Pointer to permutation control data */
	static int32_t *auxbuf;                              /* Pointer to auxiliary buffer for ordering checks */
	static int32_t *eqset;                               /* Pointer to set of integers used for variable equalization control */
	auto int32_t idx;                                    /* Pointer to permutation control index */
	auto uint8_t *rtterm1,*rtterm2;                      /* Pointers to root terms in clause */
	auto uint8_t *auxformula;                            /* Pointer to auxiliary formula with substituted variables */
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto int32_t ii,jj,mm,nn;                            /* Auxiliary */

	/* If this is the first call then perform specific tasks */
	/* and memory allocation. */
	if (varidx2==1){

		/* Clause is not ground. */
		if (0<(ii=sizeof(int32_t)*(maxvarnb+1))){

			#ifdef MAXGRJVARS
			/* Check maximum number of variables allowed. For this to work the clause */
			/* must have its variables properly renumbered. See RenumberVars() and */
			/* RenumberVars2() functions global comments. */
			if (maxvarnb>=MAXGRJVARS){
				return(0);
			}
			#endif

			/* Allocate memory for permutation, inverse permutation, control data */
			/* and auxiliary buffer for ordering checks. The permutation defines */
			/* the ordering the following way: if the defined variable order is */
			/* x₁≻x₂≻...≻xₚ or x₁≽x₂≽...≽xₚ then the variable Xn (that is, with */
			/* number n in the internal representation) corresponds to xₘ such */
			/* that perm[n]=m-1.  */
			if (NULL==(perm=MYALLOC(ii))){
				return(NOMEMORY);
			}
			if (NULL==(ctrl=MYALLOC(ii))){
				MYFREE(perm);
				return(NOMEMORY);
			}
			if (NULL==(auxbuf=MYALLOC(ii))){
				MYFREE(perm);
				MYFREE(ctrl);
				return(NOMEMORY);
			}
			if (NULL==(eqset=MYALLOC(ii))){
				MYFREE(perm);
				MYFREE(ctrl);
				MYFREE(auxbuf);
				return(NOMEMORY);
			}

			/* Initialize permutation and control data. */
			memset(ctrl,0,ii);
			for (ii=0;ii<=maxvarnb;ii++){
				perm[ii]=ii;
			}

		/* Clause is ground. */
		} else {
			perm=ctrl=auxbuf=eqset=NULL;
		}
	}
	idx=1; /* Initialized here to avoid compiler warnings. */

	/* Get pointers to equality root terms. */
	rtterm1=NextItem(&formula[0],IMMED);
	rtterm2=NextItem(rtterm1,OVERSUBTERMS);

	/* If GROIN is set to ON then allocate memory for auxiliary */
	/* formula with substituted variables. */
	if (kbset.opts.grjoin==1){
		if (NULL==(auxformula=MYALLOC(size))){
			if (varidx2==1){
				MYFREE(perm);
				MYFREE(ctrl);
				MYFREE(auxbuf);
				MYFREE(eqset);
			}
			return(NOMEMORY);
		}
	} else {
		auxformula=NULL;
	}

	/* Loop for all ordering condition combinations of variables in clause. */
	/* If this is not the first call then the loop is executed only once */
	/* using the current ordering permutation. */
	do {

		/* First root term is MAXIMUM. */
		if (MAXIMUM&rtterm1[0]){
			if (1!=(ii=GJRewrite(perm,auxbuf,eqset,rtterm1,rtterm2,maxvarnb,0))){
				if (varidx2==1){
					MYFREE(perm);
					MYFREE(ctrl);
					MYFREE(auxbuf);
					MYFREE(eqset);
				}
				MYFREE(auxformula);
				return(ii);
			}
			ii=GJRewrite(perm,auxbuf,eqset,rtterm1,rtterm2,maxvarnb,1);

		/* Second root term is MAXIMUM. */
		} else if (MAXIMUM&rtterm2[0]){
			if (1!=(ii=GJRewrite(perm,auxbuf,eqset,rtterm2,rtterm1,maxvarnb,0))){
				if (varidx2==1){
					MYFREE(perm);
					MYFREE(ctrl);
					MYFREE(auxbuf);
					MYFREE(eqset);
				}
				MYFREE(auxformula);
				return(ii);
			}
			ii=GJRewrite(perm,auxbuf,eqset,rtterm2,rtterm1,maxvarnb,1);

		/* First root term is GREATER than second root term under the */
		/* current ordering condition. */
		} else if (CheckGJOrdering1(perm,auxbuf,rtterm1,rtterm2,maxvarnb)){
			if (1!=(ii=GJRewrite(perm,auxbuf,eqset,rtterm1,rtterm2,maxvarnb,0))){
				if (varidx2==1){
					MYFREE(perm);
					MYFREE(ctrl);
					MYFREE(auxbuf);
					MYFREE(eqset);
				}
				MYFREE(auxformula);
				return(ii);
			}
			GJRewrite(perm,auxbuf,eqset,rtterm1,rtterm2,maxvarnb,1);

		/* First root term is not GREATER than second root term under the */
		/* current ordering condition. */
		} else {
			if (1!=(ii=GJRewrite(perm,auxbuf,eqset,rtterm2,rtterm1,maxvarnb,0))){
				if (varidx2==1){
					MYFREE(perm);
					MYFREE(ctrl);
					MYFREE(auxbuf);
					MYFREE(eqset);
				}
				MYFREE(auxformula);
				return(ii);
			}
			GJRewrite(perm,auxbuf,eqset,rtterm2,rtterm1,maxvarnb,1);
		}

		/* If check x₁≽x₂≽...≽xₚ succeeds then iterate to next ordering permutation. */
		if (ii==1){
			continue;
		}

		/* Return if NOMEMORY or UNKNOWN condition. */
		if (ii){
			if (varidx2==1){
				MYFREE(perm);
				MYFREE(ctrl);
				MYFREE(auxbuf);
				MYFREE(eqset);
			}
			MYFREE(auxformula);
			return(ii);
		}

		/* Check x₁≽x₂≽...≽xₚ has failed. If GRJOIN is set to MED */
		/* or there are no more possible positions for ≡ ordering */
		/* relation then return equation not ground joinable. */
		if ((kbset.opts.grjoin==2)||(varidx2>maxvarnb)){
			if (varidx2==1){
				MYFREE(perm);
				MYFREE(ctrl);
				MYFREE(auxbuf);
				MYFREE(eqset);
			}
			MYFREE(auxformula);
			return(0);
		}

		/* Check x₁≽x₂≽...≽xₚ has failed. Loop through all */
		/* possible positions for ≡ ordering relation. */
		for (jj=varidx2;jj<=maxvarnb;jj++){

			/* Get variables to be unified according to the current ≡ position. */
			mm=perm[jj];
			if (jj==varidx2){
				nn=perm[varidx1];
			} else {
				nn=perm[jj-1];
			}

			/* Unify variables corresponding to the current ≡ position. */
			memcpy(auxformula,formula,size);
			for (ptr1=NextItem(auxformula,IMMED);ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){
				if ((VARIABLE&ptr1[0])&&(mm==*((uint16_t *)&ptr1[1]))){
					*((uint16_t *)&ptr1[1])=nn;
				}
			}

			/* Perform recursive call. */
			if (1!=(ii=IsGrJoinable(auxformula,size,maxvarnb,jj==varidx2?varidx1:jj,jj+1))){
				if (varidx2==1){
					MYFREE(perm);
					MYFREE(ctrl);
					MYFREE(auxbuf);
					MYFREE(eqset);
				}
				MYFREE(auxformula);
				return(ii);
			}
		}
	} while ((varidx2==1)&&(perm!=NULL)&&(NextPerm(perm,ctrl,&idx,maxvarnb)));

	/* Free memory and return ground joinable. */
	if (varidx2==1){
		MYFREE(perm);
		MYFREE(ctrl);
		MYFREE(auxbuf);
		MYFREE(eqset);
	}
	MYFREE(auxformula);
	return(1);
} /* IsGrJoinable */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if two terms are ground joinable)  OCJ
 *
 *    This function checks if two terms are ground joinable.
 *    See IsGrJoinable() function global comments for additional
 *    information.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    varorder: Pointer to set with the order of variables in the ground
 *              ordering analyzed. If the variable number i is number k
 *              in the ground ordering analized then varorder[i]==k-1.
 *    auxbuf: Pointer to auxiliary buffer for ordering checking.
 *            This is allocated once in IsGrJoinable() function so that
 *            there is no need to allocate allocate this memory every
 *            time that this function is called.
 *    eqset: Pointer to set of integers used for variable equalization
 *           control. This is allocated once in IsGrJoinable()
 *           function so that there is no need to allocate allocate
 *           this space every time that this function is called.
 *    mxterm: pointer to term that is either maximum, maximal or equal
 *            respect to mnterm under the ground substitution being
 *            considered. This is one of the terms for the ground
 *            joinability test.
 *    mnterm: pointer to the other term for the ground joinability test.
 *    maxvarnb: Maximum variable number in mxterm and mnterm.
 *    flag: If flag is zero then ordering checking is done calling
 *          CheckGJOrdering1() function. Otherwise ordering checking
 *          is done calling CheckGJOrdering2() function.
 *
 *  RETURNS:
 *
 *    0 -> Terms are not ground joinable.
 *    1 -> Terms are ground joinable.
 *    TIMEOUT -> Time has expired.
 *    NOMEMORY -> Not enough memory.
 *    UNKNOWN -> Problem is UNKNOWN because it was solved by another process.
 *    SLICETMOUT -> maximum number of executed hardware instructions was reached.
 *
 *
 *--------------------------------------------------------------*/
int32_t GJRewrite(int32_t *varorder,int32_t *auxbuf,int32_t *eqset,uint8_t *mxterm,
		uint8_t *mnterm,int32_t maxvarnb,int32_t flag){
	auto subst Subst;                                    /* Substitution */
	auto uint8_t *rweq;                                  /* Pointer to binary rewriting equality formula */
	auto binprefix *binprweq;                            /* Pointer to binprefix structure of rewriting equality */
	auto uint8_t *unrttrm;                               /* Pointer to unifying rewriting root term */
	auto uint8_t *dstrttrm;                              /* Pointer to the destination rewriting root term */
	auto dstrbktrk backtrk;                              /* Discrimination tree backtrack structure */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4;                /* Auxiliary pointers */
	auto uint8_t *ptr5,*ptr6,*ptr7;                      /* Auxiliary pointers */
	auto int32_t jj,kk,mm,pp,qq;                         /* Auxiliary */
	auto uint64_t hh;                                    /* Auxiliary */

	/* Substitution initialization. */
	Subst.buffer=NULL;

	/* Return if discrimination trees are empty. */
	if (mxtrfsize==0){
		return(0);
	}

	/* Backtrack structure initialization. */
	if (NULL==(backtrk.treeitems=MYALLOC((((mxtrfsize-sizeof(symbol)-2)/3)+1)*sizeof(*backtrk.treeitems)))){
		return(NOMEMORY);
	}

	/* Loop through items in mxterm. */
	for (ptr1=mxterm,ptr2=NextItem(mxterm,OVERSUBTERMS);ptr1<ptr2;ptr1=NextItem(ptr1,IMMED)){

		/* Iterate if item is a variable. */
		if (VARIABLE&ptr1[0]){
			continue;
		}

		/* Select the discrimination tree to be used. */
		ptr7=(((symbol *)&ptr1[1])->symbol)->discr[0];

		/* Loop through generalization candidates for rewriting. */
		jj=1;
		backtrk.lstmatch=NULL;
		while (jj){

			/* Check number of executed hardware instructions. */
			myread(&hh);
			if (hh>=instrlimit){
				MYFREE(backtrk.treeitems);
				return(SLICETMOUT);
			}

			/* Check timeout and solution by other process. */
			if (procctl->status==TIMEOUT){
				MYFREE(Subst.buffer);
				MYFREE(backtrk.treeitems);
				return(TIMEOUT);
			}
			if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
				MYFREE(Subst.buffer);
				MYFREE(backtrk.treeitems);
				return(UNKNOWN);
			}
			if (procctl->status==NOMEMORY){
				MYFREE(Subst.buffer);
				MYFREE(backtrk.treeitems);
				return(procctl->status);
			}

			/* Get candidate for generalization. */
			if (NOMEMORY==(jj=QueryDscTree2(ptr1,&unrttrm,ptr7,&backtrk,GENERALIZATION))){
				MYFREE(Subst.buffer);
				MYFREE(backtrk.treeitems);
				return(NOMEMORY);
			}

			/* We have a candidate generalization term. */
			if (jj){

				/* Get a pointer to the binary formula of rewriting equation */
				/* and to the destination rewriting root term candidates. */
				rweq=unrttrm-((symbol *)&unrttrm[1])->offset;
				binprweq=(binprefix *)(rweq-binpsize);
				if (unrttrm==(ptr3=NextItem(rweq,IMMED))){
					dstrttrm=NextItem(unrttrm,OVERSUBTERMS);
				} else {
					dstrttrm=ptr3;
				}

				/* Shift variables in candidate binary formula. */
				ShiftUEQVarNbr(binprweq,maxvarnb+1);

				/* Check if candidate is a generalization of the term. */
				if (NOMEMORY==(kk=Generalize(unrttrm,ptr1,&Subst,INITSUBST))){
					UnshiftUEQVarNbr(binprweq,maxvarnb+1);
					MYFREE(Subst.buffer);
					MYFREE(backtrk.treeitems);
					return(NOMEMORY);
				}

				/* We have a valid generalization. */
				if (kk){

					/* Apply substitution to rewriting term. */
					if (NULL==(ptr4=ApplySubst2Term(dstrttrm,&Subst,0))){
						UnshiftUEQVarNbr(binprweq,maxvarnb+1);
						MYFREE(Subst.buffer);
						MYFREE(backtrk.treeitems);
						return(NOMEMORY);
					}

					/* Rewriting equation is not oriented. */
					if (0==(MAXIMUM&unrttrm[0])){

						/* Apply substitution to unification root term. */
						if (NULL==(ptr5=ApplySubst2Term(unrttrm,&Subst,0))){
							UnshiftUEQVarNbr(binprweq,maxvarnb+1);
							MYFREE(Subst.buffer);
							MYFREE(backtrk.treeitems);
							return(NOMEMORY);
						}

						/* Compare root terms of rewriting equation under substitution. */
						if (flag==0){
							kk=CheckGJOrdering1(varorder,auxbuf,ptr5,ptr4,maxvarnb);
						} else {
							if (IsEqual(ptr5,ptr4,0)){
								kk=0;
							} else {
								kk=CheckGJOrdering2FE(varorder,auxbuf,eqset,ptr5,ptr4,maxvarnb);
							}
						}
						MYFREE(ptr5);
						if (kk==0){
							UnshiftUEQVarNbr(binprweq,maxvarnb+1);
							MYFREE(ptr4);
							continue;
						}
					}

					/* Allocate memory for rewritten term. */
					ptr5=NextItem(mxterm,OVERSUBTERMS);
					ptr6=NextItem(ptr1,OVERSUBTERMS);
					mm=ptr1-mxterm;
					pp=NextItem(ptr4,OVERSUBTERMS)-ptr4;
					qq=ptr5-ptr6;
					if (NULL==(ptr3=MYALLOC(mm+pp+qq+1))){
						UnshiftUEQVarNbr(binprweq,maxvarnb+1);
						MYFREE(Subst.buffer);
						MYFREE(backtrk.treeitems);
						MYFREE(ptr4);
						return(NOMEMORY);
					}

					/* Build the resulting term from the rewrite. */
					memcpy(ptr3,mxterm,mm);
					memcpy(&ptr3[mm],ptr4,pp);
					mm+=pp;
					memcpy(&ptr3[mm],ptr6,qq);
					ptr3[mm+qq]=UNITEND;
					MYFREE(ptr4);
					UpdateParams2(ptr3);

					/* Return clause ground joinable if rewritten term equals mnterm. */
					if (IsEqual(ptr3,mnterm,0)){
						UnshiftUEQVarNbr(binprweq,maxvarnb+1);
						MYFREE(Subst.buffer);
						MYFREE(backtrk.treeitems);
						MYFREE(ptr3);
						return(1);
					}

					/* Resulting rewritten term is GREATER than mnterm under the */
					/* current ordering condition. */
					if (CheckGJOrdering1(varorder,auxbuf,ptr3,mnterm,maxvarnb)){
						if (0!=(mm=GJRewrite(varorder,auxbuf,eqset,ptr3,mnterm,maxvarnb,flag))){
							UnshiftUEQVarNbr(binprweq,maxvarnb+1);
							MYFREE(Subst.buffer);
							MYFREE(backtrk.treeitems);
							MYFREE(ptr3);
							return(mm);
						}

					/* Resulting rewritten term is not GREATER than mnterm under the */
					/* current ordering condition. */
					} else {
						if (0!=(mm=GJRewrite(varorder,auxbuf,eqset,mnterm,ptr3,maxvarnb,flag))){
							UnshiftUEQVarNbr(binprweq,maxvarnb+1);
							MYFREE(Subst.buffer);
							MYFREE(backtrk.treeitems);
							MYFREE(ptr3);
							return(mm);
						}
					}

					/* Free auxiliary term. */
					MYFREE(ptr3);
				}

				/* Unshift variables in candidate binary formula. */
				UnshiftUEQVarNbr(binprweq,maxvarnb+1);
			}
		}
	}

	/* Free memory and return. */
	MYFREE(Subst.buffer);
	MYFREE(backtrk.treeitems);
	return(0);
} /* GJRewrite */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if an equality is subsumed)  OCJ
 *
 *    This function checks if an (in)equality in internal compiled format
 *    is subsumed by the set of active (in)equalities or ground joinable
 *    equalities.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: pointer to an (in)equality in binary format. If it is not
 *            a pointer to an (in)equality then results are unpredictable.
 *
 *  RETURNS:
 *
 *    0 -> Clause is not subsumed.
 *    1 -> Clause is subsumed.
 *    TIMEOUT -> Time has expired.
 *    NOMEMORY -> Not enough memory.
 *    UNKNOWN -> Problem is UNKNOWN because it was solved by another process.
 *
 *
 *--------------------------------------------------------------*/
int32_t FwUEQSubsum(cmprefix *clause){
	auto subst Subst;                                    /* Substitution */
	auto uint8_t *equality;                              /* Pointer to (in)equality with root terms in reverse order */
	auto uint8_t *eqterms;                               /* Pointer to (in)equality root terms in reverse order */
	auto uint8_t *queryitm;                              /* Pointer to QueryDscTree2() query item */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4;                /* Auxiliary pointers */
	auto int32_t ii,jj,kk,mm,nn,rr,ss;                   /* Auxiliary */

	/* Initialization. */
	Subst.buffer=NULL;
	eqterms=equality=NULL;
	ptr1=&clause->part2.bin->formula[0];

	/* Loop through ACTIVE and GROUND JOINABLE (if applicable) clauses */
	if ((kbset.opts.grjoin)&&(0==(NEGATED&ptr1[0]))){
		if (kbset.opts.algorithm==UEQOTT){
			mm=3;
		} else {
			mm=2;
		}
	} else {
		if (kbset.opts.algorithm==UEQOTT){
			mm=2;
		} else {
			mm=1;
		}
	}
	for (ii=0;ii<mm;ii++){

		/* Get pointer to discrimination tree. */
		if (0==(NEGATED&ptr1[0])){
			if (kbset.opts.algorithm==UEQOTT){
				switch (ii){
					case 0:
						ptr2=(((symbol *)&ptr1[1])->symbol)->discr[0];
						break;
					case 1:
						ptr2=(((symbol *)&ptr1[1])->symbol)->discr[1];
						break;
					default:
					case 2:
						ptr2=(((symbol *)&ptr1[1])->symbol)->discr[3];
						break;
				}
			} else {
				ptr2=(((symbol *)&ptr1[1])->symbol)->discr[ii];
			}
		} else {
			if (kbset.opts.algorithm==UEQOTT){
				ptr2=(((symbol *)&ptr1[1])->symbol)->discr[2*(ii+1)];
			} else {
				ptr2=(((symbol *)&ptr1[1])->symbol)->discr[2];
			}
		}

		/* Loop for (in)equality symmetry management. */
		for (kk=0;kk<2;kk++){

			/* This is the first iteration. */
			if (kk==0){
				queryitm=ptr1;

			/* This is the second iteration for the (in)equality literal. */
			/* Build an (in)equality with root terms in reverse order. */
			} else {
				if (equality==NULL){
					ptr4=NextItem(ptr1,IMMED);
					ptr3=NextItem(ptr4,OVERSUBTERMS);
					rr=ptr3-ptr4;
					ss=NextItem(ptr3,OVERSUBTERMS)-ptr3;
					if (NULL==(equality=MYALLOC(2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
						MYFREE(Subst.buffer);
						return(NOMEMORY);
					}
					eqterms=&equality[1+sizeof(symbol)];
					memcpy(equality,ptr1,1+sizeof(symbol));
					memcpy(eqterms,ptr3,ss);
					memcpy(&eqterms[ss],ptr4,rr);
					eqterms[rr+ss]=UNITEND;
				}
				queryitm=equality;
			}

			/* Loop through candidates for generalization. */
			jj=1;
			nn=0;
			while (jj){

				/* Check timeout and solution by other process. */
				if (procctl->status==TIMEOUT){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(TIMEOUT);
				}
				if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(UNKNOWN);
				}
				if (procctl->status==NOMEMORY){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(TIMEOUT);
				}

				/* Get candidate for generalization. */
				if (0!=(jj=QueryGenDscTree(queryitm,&ptr3,ptr2,nn))){

					/* Check unification. */
					nn=1;
					switch (Generalize(ptr3,queryitm,&Subst,INITSUBST)){

						/* Not enough memory. */
						case NOMEMORY:
							MYFREE(Subst.buffer);
							MYFREE(equality);
							return(NOMEMORY);
							break;

						/* Candidate is a generalization of clause, return clause subsumed. */
						case 1:
							kbset.prstats.fwdsubsum++;
							MYFREE(Subst.buffer);
							MYFREE(equality);
							return(1);
							break;
					}
				}
			}
		}
	}

	/* Free memory and return. */
	MYFREE(Subst.buffer);
	MYFREE(equality);
	return(0);
} /* FwUEQSubsum */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Backward sumbumption simplifications)  OCJ
 *
 *    This function performs subsumtions of ACTIVE clauses by
 *    a given clause. Subsumed clauses are added to the disposal
 *    queue.
 *
 *    IMPORTANT: The clause passed as argument must not be in
 *    the ACTIVE queue. If this changes in the future then a
 *    check must be added to prevent a clause to subsume itself.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: pointer to an (in)equality in binary format. If it is not
 *            a pointer to an (in)equality then results are unpredictable.
 *
 *  RETURNS:
 *
 *    0 -> Process performed correctly.
 *    TIMEOUT -> Time has expired.
 *    NOMEMORY -> Not enough memory.
 *    UNKNOWN -> Problem is UNKNOWN because it was solved by another process.
 *
 *
 *--------------------------------------------------------------*/
int32_t BkUEQSubsum(cmprefix *clause){
	auto subst Subst;                                    /* Substitution */
	auto uint8_t *equality;                              /* Pointer to (in)equality with root terms in reverse order */
	auto uint8_t *eqterms;                               /* Pointer to (in)equality root terms in reverse order */
	auto uint8_t *queryitm;                              /* Pointer to QueryDscTree2() query item */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4;                /* Auxiliary pointers */
	auto binprefix *ptr5;                                /* Auxiliary pointer */
	auto int32_t ii,jj,kk,mm,nn,rr,ss;                   /* Auxiliary */

	/* Initialization. */
	Subst.buffer=NULL;
	eqterms=equality=NULL;
	ptr1=&clause->part2.bin->formula[0];

	/* Loop through discrimination trees. */
	if (kbset.opts.algorithm==UEQOTT){
		mm=2;
	} else {
		mm=1;
	}
	for (ii=0;ii<mm;ii++){

		/* Get pointer to discrimination tree. */
		if (0==(NEGATED&ptr1[0])){
			if (kbset.opts.algorithm==UEQOTT){
				if (ii){
					ptr2=(((symbol *)&ptr1[1])->symbol)->discr[3];
				} else {
					ptr2=(((symbol *)&ptr1[1])->symbol)->discr[0];
				}
			} else {
				ptr2=(((symbol *)&ptr1[1])->symbol)->discr[0];
			}
		} else {
			if (kbset.opts.algorithm==UEQOTT){
				ptr2=(((symbol *)&ptr1[1])->symbol)->discr[2*(ii+1)];
			} else {
				ptr2=(((symbol *)&ptr1[1])->symbol)->discr[2];
			}
		}

		/* Loop for (in)equality symmetry management. */
		for (kk=0;kk<2;kk++){

			/* This is the first iteration. */
			if (kk==0){
				queryitm=ptr1;

			/* This is the second iteration for the (in)equality literal. */
			/* Build an (in)equality with root terms in reverse order. */
			} else {
				if (ii==0){
					ptr4=NextItem(ptr1,IMMED);
					ptr3=NextItem(ptr4,OVERSUBTERMS);
					rr=ptr3-ptr4;
					ss=NextItem(ptr3,OVERSUBTERMS)-ptr3;
					if (NULL==(equality=MYALLOC(2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
						MYFREE(Subst.buffer);
						MYFREE(equality);
						return(NOMEMORY);
					}
					eqterms=&equality[1+sizeof(symbol)];
					memcpy(equality,ptr1,1+sizeof(symbol));
					memcpy(eqterms,ptr3,ss);
					memcpy(&eqterms[ss],ptr4,rr);
					eqterms[rr+ss]=UNITEND;
				}
				queryitm=equality;
			}

			/* Loop through candidates for instantiation. */
			jj=1;
			nn=0;
			while (jj){

				/* Check timeout and solution by other process. */
				if (procctl->status==TIMEOUT){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(TIMEOUT);
				}
				if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(UNKNOWN);
				}
				if (procctl->status==NOMEMORY){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(TIMEOUT);
				}

				/* Get candidate for instantiation. */
				if (0!=(jj=QueryInsDscTree(queryitm,&ptr3,ptr2,nn))){

					/* Candidate is not INDISPOSALQUEUE. */
					nn=1;
					ptr5=(binprefix *)(ptr3-binpsize);
					if (0==(INDISPOSALQUEUE&ptr5->clause->flags)){

						/* Check unification. */
						switch (Generalize(queryitm,ptr3,&Subst,INITSUBST)){

							/* Not enough memory. */
							case NOMEMORY:
								MYFREE(Subst.buffer);
								MYFREE(equality);
								return(NOMEMORY);
								break;

							/* Candidate is an instance of clause, add candidate */
							/* to disposal queue. */
							case 1:
								kbset.prstats.bcksubsum++;
								ptr5->clause->flags|=INDISPOSALQUEUE;
								ptr5->clause->prevdisp=kbset.lastdisp;
								kbset.lastdisp=ptr5->clause;
								break;
						}
					}
				}
			}
		}
	}

	/* Free memory and return. */
	MYFREE(Subst.buffer);
	MYFREE(equality);
	return(0);
} /* BkUEQSubsum */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Generate critical pairs)  OCJ
 *
 *    This function generates all critical pairs between a given
 *    equality and the active clauses. The given clause must
 *    have the root term flags SELECTED and MAXIMUM set if appropriate
 *    and the equality data updated by a call to CompareEquTerms()
 *    function.
 *
 *    This function just calls GenerateCPTo() and GenerateCPFrom()
 *    functions.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: pointer to an equality in binary format. If it is not
 *            a pointer to an equality then results are unpredictable.
 *
 *  RETURNS:
 *
 *    0 -> Process performed without errors and a $false clause was not generated.
 *    1 -> Process performed without errors and a $false clause was generated.
 *    TIMEOUT -> Time has expired.
 *    NOMEMORY -> Not enough memory.
 *    UNKNOWN -> Problem is UNKNOWN because it was solved by another process.
 *
 *
 *--------------------------------------------------------------*/
int32_t GenerateCP(cmprefix *clause){
	auto int32_t ii;                                     /* Auxiliary */

	/* Call GenerateCPTo() and GenerateCPFrom() functions. */
	if (0!=(ii=GenerateCPTo(clause))){
		return(ii);
	}
	if (0==(NEGATED&clause->part2.bin->formula[0])){
		return(GenerateCPFrom(clause));
	}
	return(0);
} /* GenerateCP */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Generate critical pairs to clause)  OCJ
 *
 *    This function generates all critical pairs paramodulating with
 *    the superposition version from the active clauses to a given
 *    equality or equivalently using deduction and deduction2 rules
 *    as described in the document "Completion without failure" by Leo
 *    Bachmair. The given clause must have the root term flags SELECTED
 *    and MAXIMUM set if appropriate and the equality data updated by a
 *    call to CompareEquTerms() function.
 *
 *    The generated critical pairs are added to KB as UNPROC.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: pointer to an equality in binary format. If it is not
 *            a pointer to an equality then results are unpredictable.
 *
 *  RETURNS:
 *
 *    0 -> Process performed without errors and a $false clause was not generated.
 *    1 -> Process performed without errors and a $false clause was generated.
 *    TIMEOUT -> Time has expired.
 *    NOMEMORY -> Not enough memory.
 *    UNKNOWN -> Problem is UNKNOWN because it was solved by another process.
 *
 *
 *--------------------------------------------------------------*/
int32_t GenerateCPTo(cmprefix *clause){
	auto uint8_t *rttrm1,*rttrm2;                        /* Pointers to clause equality root terms */
	auto binprefix *frbinp;                              /* Pointer to binprefix structure of "from" equality */
	auto binprefix *dupbinp;                             /* Duplicate of clause binprefix structure */
	auto uint8_t *unrttrm;                               /* Pointer to unifying root term */
	auto uint8_t *othrttrm;                              /* Pointer to the other (non unifying) root term */
	auto cmprefix *cpclause;                             /* Critical pair clause pointer */
	auto subst Subst;                                    /* Substitution */
	auto dstrbktrk backtrk;                              /* Discrimination tree backtrack structure */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4;                /* Auxiliary pointers */
	auto uint8_t *ptr5,*ptr6,*ptr7;                      /* Auxiliary pointers */
	auto binprefix *ptr8;                                /* Auxiliary pointer */
	auto int32_t jj,kk,mm,pp,qq,rr;                      /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	#endif

	/* Initialize substitution and equality root terms. */
	Subst.buffer=NULL;
	rttrm1=NextItem(&clause->part2.bin->formula[0],IMMED);
	rttrm2=NextItem(rttrm1,OVERSUBTERMS);

	/* Return if discrimination trees are empty. */
	if (mxtrfsize==0){
		return(0);
	}

	/* Backtrack structure initialization. */
	if (NULL==(backtrk.treeitems=MYALLOC((((mxtrfsize-sizeof(symbol)-2)/3)+1)*sizeof(*backtrk.treeitems)))){
		return(NOMEMORY);
	}

	/* Create a duplicate of clause binprefix structure. This is necessary */
	/* for the rare cases in which a clause forms critical pairs with itself. */
	if (NULL==(dupbinp=MYALLOC(jj=binpsize+clause->part2.bin->size))){
		MYFREE(backtrk.treeitems);
		return(NOMEMORY);
	}
	memcpy(dupbinp,clause->part2.bin,jj);

	/* Debug. */
	#ifdef DEBUGCODE
	ptr10=Decompile(clause->part2.bin);
	MYFREE(ptr10);
	#endif

	/* Loop through equality root terms. */
	for (ptr1=rttrm1;ptr1!=NULL;ptr1=(ptr1==rttrm1?rttrm2:NULL)){

		/* Iterate if root term is not SELECTED. */
		if (0==(SELECTED&ptr1[0])){
			continue;
		}

		/* Loop through items of root term. */
		for (ptr2=ptr1,ptr3=NextItem(ptr1,OVERSUBTERMS);ptr2<ptr3;ptr2=NextItem(ptr2,IMMED)){

			/* Iterate if term is a variable. */
			if (VARIABLE&ptr2[0]){
				continue;
			}

			/* Select the discrimination tree to be used. */
			ptr4=(((symbol *)&ptr2[1])->symbol)->discr[0];

			/* Loop through unification candidates for rewriting. */
			jj=1;
			backtrk.lstmatch=NULL;
			while (jj){

				/* Check timeout and solution by other process. */
				if (procctl->status==TIMEOUT){
					MYFREE(Subst.buffer);
					MYFREE(backtrk.treeitems);
					MYFREE(dupbinp);
					return(TIMEOUT);
				}
				if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
					MYFREE(Subst.buffer);
					MYFREE(backtrk.treeitems);
					MYFREE(dupbinp);
					return(UNKNOWN);
				}
				if (procctl->status==NOMEMORY){
					MYFREE(Subst.buffer);
					MYFREE(backtrk.treeitems);
					MYFREE(dupbinp);
					return(procctl->status);
				}

				/* Get candidate for unification. */
				if (NOMEMORY==(jj=QueryDscTree2(ptr2,&unrttrm,ptr4,&backtrk,UNIFICATION))){
					MYFREE(Subst.buffer);
					MYFREE(backtrk.treeitems);
					MYFREE(dupbinp);
					return(NOMEMORY);
				}

				/* We have a candidate unification term. */
				if (jj){

					/* Get a pointer to the binary formula of rewriting */
					/* equation and to the destination rewriting root term candidates. */
					/* Iterate if ptr2 is one of the candidate clause root terms. */
					/* Use duplicate binprefix structure if appropriate. */
					frbinp=(binprefix *)(unrttrm-(binpsize+((symbol *)&unrttrm[1])->offset));
					if (unrttrm==(ptr5=NextItem(&frbinp->formula[0],IMMED))){
						othrttrm=NextItem(unrttrm,OVERSUBTERMS);
					} else {
						othrttrm=ptr5;
					}
					if ((ptr2==unrttrm)||(ptr2==othrttrm)){
						continue;
					}
					if (frbinp==clause->part2.bin){
						unrttrm=(uint8_t *)dupbinp+(unrttrm-(uint8_t *)frbinp);
						othrttrm=(uint8_t *)dupbinp+(othrttrm-(uint8_t *)frbinp);
						frbinp=dupbinp;
					}

					/* Debug. */
					#ifdef DEBUGCODE
					ptr10=Decompile(frbinp);
					MYFREE(ptr10);
					#endif

					/* Shift variables in candidate binary formula. */
					ShiftUEQVarNbr(frbinp,clause->part2.bin->maxvarnb+1);

					/* Check if candidate unifies the term. */
					if (NOMEMORY==(kk=Unify(unrttrm,ptr2,&Subst,INITSUBST))){
						UnshiftUEQVarNbr(frbinp,clause->part2.bin->maxvarnb+1);
						MYFREE(Subst.buffer);
						MYFREE(backtrk.treeitems);
						MYFREE(dupbinp);
						return(NOMEMORY);
					}

					/* We have a valid unification. */
					if (kk){

						/* Check ordering constraints. */
						switch (CheckEqRootOrder2(unrttrm,othrttrm,&Subst,0)){
							case NOMEMORY:
								UnshiftUEQVarNbr(frbinp,clause->part2.bin->maxvarnb+1);
								MYFREE(Subst.buffer);
								MYFREE(backtrk.treeitems);
								MYFREE(dupbinp);
								return(NOMEMORY);
								break;
							case 0:
								UnshiftUEQVarNbr(frbinp,clause->part2.bin->maxvarnb+1);
								continue;
								break;
						}
						switch (CheckEqRootOrder(ptr1,ptr1==rttrm1?rttrm2:rttrm1,&Subst,0,
								frbinp->maxvarnb+clause->part2.bin->maxvarnb+1,0)){
							case NOMEMORY:
								UnshiftUEQVarNbr(frbinp,clause->part2.bin->maxvarnb+1);
								MYFREE(Subst.buffer);
								MYFREE(backtrk.treeitems);
								MYFREE(dupbinp);
								return(NOMEMORY);
								break;
							case 0:
								UnshiftUEQVarNbr(frbinp,clause->part2.bin->maxvarnb+1);
								continue;
								break;
						}

						/* Debug. */
						#ifdef DEBUGCODE
						ptr10=Decompile(frbinp);
						MYFREE(ptr10);
						#endif

						/* Compute necessary pointers and lengths to build critical pair. */
						kk=rttrm1-&clause->part2.bin->formula[0];
						ptr5=(ptr1==rttrm1?rttrm2:rttrm1);
						mm=NextItem(ptr5,OVERSUBTERMS)-ptr5;
						pp=ptr2-ptr1;
						qq=NextItem(othrttrm,OVERSUBTERMS)-othrttrm;
						ptr6=NextItem(ptr2,OVERSUBTERMS);
						rr=NextItem(ptr1,OVERSUBTERMS)-ptr6;

						/* Allocate memory for critical pair. */
						if (NULL==(cpclause=MYALLOC(sizeof(cmprefix)))){
							UnshiftUEQVarNbr(frbinp,clause->part2.bin->maxvarnb+1);
							MYFREE(Subst.buffer);
							MYFREE(backtrk.treeitems);
							MYFREE(dupbinp);
							return(NOMEMORY);
						}
						if (NULL==((cpclause->part2.bin)=MYALLOC(binpsize+1+kk+mm+pp+qq+rr))){
							UnshiftUEQVarNbr(frbinp,clause->part2.bin->maxvarnb+1);
							MYFREE(Subst.buffer);
							MYFREE(cpclause);
							MYFREE(backtrk.treeitems);
							MYFREE(dupbinp);
							return(NOMEMORY);
						}

						/* Initialize critical pair cmprefix and binprefix structures. */
						cpclause->flags=UNPROC;
						cpclause->parent1=frbinp->clause;
						cpclause->parent2=clause;
						cpclause->prevdisp=NULL;
						if (frbinp->agedist<clause->part2.bin->agedist){
							cpclause->part2.bin->agedist=clause->part2.bin->agedist+1;
						} else {
							cpclause->part2.bin->agedist=frbinp->agedist+1;
						}
						if (frbinp->sinedist<clause->part2.bin->sinedist){
							cpclause->part2.bin->sinedist=frbinp->sinedist;
						} else {
							cpclause->part2.bin->sinedist=clause->part2.bin->sinedist;
						}
						cpclause->inference=PARAMODULATION;
						kbset.prstats.paramodulations++;
						cpclause->part2.bin->signature=0;
						cpclause->part2.bin->locksignature=0;
						cpclause->part2.bin->clause=cpclause;
						cpclause->part2.bin->oriented=0;

						/* Build critical pair without substitution. */
						ptr7=&cpclause->part2.bin->formula[0];
						memcpy(ptr7,&clause->part2.bin->formula[0],kk);
						memcpy(&ptr7[kk],ptr5,mm);
						kk+=mm;
						memcpy(&ptr7[kk],ptr1,pp);
						kk+=pp;
						memcpy(&ptr7[kk],othrttrm,qq);
						kk+=qq;
						memcpy(&ptr7[kk],ptr6,rr);
						kk+=rr;
						ptr7[kk]=UNITEND;
						kk++;

						/* Apply substitution to critical pair and set length to the exact size. */
						if (NOMEMORY==ApplySubst2Clause(cpclause,&kk,&Subst,0)){
							UnshiftUEQVarNbr(frbinp,clause->part2.bin->maxvarnb+1);
							MYFREE(Subst.buffer);
							MYFREE(cpclause->part2.bin);
							MYFREE(cpclause);
							MYFREE(backtrk.treeitems);
							MYFREE(dupbinp);
							return(NOMEMORY);
						}
						if (NULL==(ptr8=MYREALLOC(cpclause->part2.bin,cpclause->part2.bin->size+binpsize))){
							UnshiftUEQVarNbr(frbinp,clause->part2.bin->maxvarnb+1);
							MYFREE(Subst.buffer);
							MYFREE(cpclause->part2.bin);
							MYFREE(cpclause);
							MYFREE(backtrk.treeitems);
							MYFREE(dupbinp);
							return(NOMEMORY);
						}
						cpclause->part2.bin=ptr8;

						/* If input clause is positive then add critical pair top term. */
						if (0==(NEGATED&clause->part2.bin->formula[0])){
							if (NULL==(ptr7=ApplySubst2Term(ptr1,&Subst,0))){
								UnshiftUEQVarNbr(frbinp,clause->part2.bin->maxvarnb+1);
								MYFREE(Subst.buffer);
								MYFREE(cpclause->part2.bin);
								MYFREE(cpclause);
								MYFREE(backtrk.treeitems);
								MYFREE(dupbinp);
								return(NOMEMORY);
							}
							cpclause->part2.bin->ovly.cptopterm=ptr7;

						/* If input clause is negative then initialize top term to NULL. */
						} else {
							cpclause->part2.bin->ovly.cptopterm=NULL;
						}

						/* Initialize equality ordering data and reset */
						/* root terms MAXIMUM and SELECTED flags. */
						ptr7=&cpclause->part2.bin->formula[0];
						ptr7=NextItem(ptr7,IMMED);
						ptr7[0]&=(~(MAXIMUM|SELECTED));
						ptr7=NextItem(ptr7,OVERSUBTERMS);
						ptr7[0]&=(~(MAXIMUM|SELECTED));

						/* Add critical pair to UNPROC clauses. */
						if (NOMEMORY==AddUEQClause2KB(cpclause)){
							UnshiftUEQVarNbr(frbinp,clause->part2.bin->maxvarnb+1);
							MYFREE(cpclause->part2.bin->ovly.cptopterm);
							MYFREE(cpclause->part2.bin);
							MYFREE(cpclause);
							MYFREE(Subst.buffer);
							MYFREE(backtrk.treeitems);
							MYFREE(dupbinp);
							return(NOMEMORY);
						}
					}

					/* Unshift variables in candidate binary formula. */
					UnshiftUEQVarNbr(frbinp,clause->part2.bin->maxvarnb+1);
				}
			}
		}
	}

	/* Free memory and return. */
	MYFREE(Subst.buffer);
	MYFREE(backtrk.treeitems);
	MYFREE(dupbinp);
	return(0);
} /* GenerateCPTo */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Generate critical pairs from clause)  OCJ
 *
 *    This function generates all critical pairs paramodulating with
 *    the superposition version from a given equality to the active
 *    clauses or equivalently using deduction and deduction2 rules as
 *    described in the document "Completion without failure" by Leo
 *    Bachmair. The given clause must have the root term flags SELECTED
 *    and MAXIMUM set if appropriate and the equality data updated by a
 *    call to CompareEquTerms() function.
 *
 *    The generated critical pairs are added to UNPROC clauses.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: pointer to an equality in binary format. If it is not
 *            a pointer to an equality then results are unpredictable.
 *
 *  RETURNS:
 *
 *    0 -> Process performed without errors and a $false clause was not generated.
 *    1 -> Process performed without errors and a $false clause was generated.
 *    TIMEOUT -> Time has expired.
 *    NOMEMORY -> Not enough memory.
 *    UNKNOWN -> Problem is UNKNOWN because it was solved by another process.
 *
 *
 *--------------------------------------------------------------*/
int32_t GenerateCPFrom(cmprefix *clause){
	auto uint8_t *rttrm1,*rttrm2;                        /* Pointers to clause equality root terms */
	auto binprefix *tobinp;                              /* Pointer to binprefix structure of "To" equality */
	auto uint8_t *unifterm;                              /* Pointer to unifying term */
	auto uint8_t *unrttrm;                               /* Pointer to root term of unifying term */
	auto uint8_t *othrttrm;                              /* Pointer to the other (non unifying) root term */
	auto cmprefix *cpclause;                             /* Critical pair clause pointer */
	auto subst Subst;                                    /* Substitution */
	auto dstrbktrk backtrk;                              /* Discrimination tree backtrack structure */
	auto binprefix *ptr3;                                /* Auxiliary pointer */
	auto uint8_t *ptr1,*ptr2,*ptr4;                      /* Auxiliary pointers */
	auto uint8_t *ptr5,*ptr6;                            /* Auxiliary pointers */
	auto int32_t jj,kk,mm,pp,qq,rr;                      /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	#endif

	/* Initialize substitution and equality root terms. */
	Subst.buffer=NULL;
	rttrm1=NextItem(&clause->part2.bin->formula[0],IMMED);
	rttrm2=NextItem(rttrm1,OVERSUBTERMS);

	/* Return if discrimination trees are empty. */
	if (mxtrfsize==0){
		return(0);
	}

	/* Backtrack structure initialization. */
	if (NULL==(backtrk.treeitems=MYALLOC((((mxtrfsize-sizeof(symbol)-2)/3)+1)*sizeof(*backtrk.treeitems)))){
		return(NOMEMORY);
	}

	/* Debug. */
	#ifdef DEBUGCODE
	ptr10=Decompile(clause->part2.bin);
	MYFREE(ptr10);
	#endif

	/* Loop through equality root terms. */
	for (ptr1=rttrm1;ptr1!=NULL;ptr1=(ptr1==rttrm1?rttrm2:NULL)){

		/* Iterate if root term is not SELECTED. */
		if (0==(SELECTED&ptr1[0])){
			continue;
		}

		/* Select the discrimination tree to be used. */
		ptr4=(((symbol *)&ptr1[1])->symbol)->discr[2];

		/* Loop through unification candidates for rewriting. */
		jj=1;
		backtrk.lstmatch=NULL;
		while (jj){

			/* Check timeout and solution by other process. */
			if (procctl->status==TIMEOUT){
				MYFREE(Subst.buffer);
				MYFREE(backtrk.treeitems);
				return(TIMEOUT);
			}
			if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
				MYFREE(Subst.buffer);
				MYFREE(backtrk.treeitems);
				return(UNKNOWN);
			}
			if (procctl->status==NOMEMORY){
				MYFREE(Subst.buffer);
				MYFREE(backtrk.treeitems);
				return(procctl->status);
			}

			/* Get candidate for unification. */
			if (NOMEMORY==(jj=QueryDscTree2(ptr1,&unifterm,ptr4,&backtrk,UNIFICATION))){
				MYFREE(Subst.buffer);
				MYFREE(backtrk.treeitems);
				return(NOMEMORY);
			}

			/* We have a candidate unification term. */
			if (jj){

				/* Get a pointer to the binary "to" formula equation */
				/* and to its root terms. Iterate if the candidate clause is the same */
				/* as the "from" clause, because in this case the valid critical */
				/* pairs have been already generated by GenerateCPTo() function. */
				tobinp=(binprefix *)(unifterm-(binpsize+((symbol *)&unifterm[1])->offset));
				if (clause==tobinp->clause){
					continue;
				}
				unrttrm=NextItem(&tobinp->formula[0],IMMED);
				othrttrm=NextItem(unrttrm,OVERSUBTERMS);
				if (unifterm>=othrttrm){
					ptr2=unrttrm;
					unrttrm=othrttrm;
					othrttrm=ptr2;
				}

				/* Shift variables in candidate binary formula. */
				ShiftUEQVarNbr(tobinp,clause->part2.bin->maxvarnb+1);

				/* Check if candidate unifies the term. */
				if (NOMEMORY==(kk=Unify(unifterm,ptr1,&Subst,INITSUBST))){
					UnshiftUEQVarNbr(tobinp,clause->part2.bin->maxvarnb+1);
					MYFREE(Subst.buffer);
					MYFREE(backtrk.treeitems);
					return(NOMEMORY);
				}

				/* We have a valid unification. */
				if (kk){

					/* Check ordering constraints. */
					switch (CheckEqRootOrder2(unrttrm,othrttrm,&Subst,0)){
						case NOMEMORY:
							UnshiftUEQVarNbr(tobinp,clause->part2.bin->maxvarnb+1);
							MYFREE(Subst.buffer);
							MYFREE(backtrk.treeitems);
							return(NOMEMORY);
							break;
						case 0:
							UnshiftUEQVarNbr(tobinp,clause->part2.bin->maxvarnb+1);
							continue;
							break;
					}
					switch (CheckEqRootOrder(ptr1,ptr1==rttrm1?rttrm2:rttrm1,&Subst,0,
							tobinp->maxvarnb+clause->part2.bin->maxvarnb+1,0)){
						case NOMEMORY:
							UnshiftUEQVarNbr(tobinp,clause->part2.bin->maxvarnb+1);
							MYFREE(Subst.buffer);
							MYFREE(backtrk.treeitems);
							return(NOMEMORY);
							break;
						case 0:
							UnshiftUEQVarNbr(tobinp,clause->part2.bin->maxvarnb+1);
							continue;
							break;
					}

					/* Debug. */
					#ifdef DEBUGCODE
					ptr10=Decompile(tobinp);
					MYFREE(ptr10);
					#endif

					/* Compute necessary pointers and lengths to build critical pair. */
					kk=rttrm1-&clause->part2.bin->formula[0];
					mm=NextItem(othrttrm,OVERSUBTERMS)-othrttrm;
					pp=unifterm-unrttrm;
					ptr5=(ptr1==rttrm1?rttrm2:rttrm1);
					qq=NextItem(ptr5,OVERSUBTERMS)-ptr5;
					ptr6=NextItem(unifterm,OVERSUBTERMS);
					rr=NextItem(unrttrm,OVERSUBTERMS)-ptr6;

					/* Allocate memory for critical pair. */
					if (NULL==(cpclause=MYALLOC(sizeof(cmprefix)))){
						UnshiftUEQVarNbr(tobinp,clause->part2.bin->maxvarnb+1);
						MYFREE(Subst.buffer);
						MYFREE(backtrk.treeitems);
						return(NOMEMORY);
					}
					if (NULL==((cpclause->part2.bin)=MYALLOC(binpsize+1+kk+mm+pp+qq+rr))){
						UnshiftUEQVarNbr(tobinp,clause->part2.bin->maxvarnb+1);
						MYFREE(Subst.buffer);
						MYFREE(cpclause);
						MYFREE(backtrk.treeitems);
						return(NOMEMORY);
					}

					/* Initialize critical pair cmprefix and binprefix structures. */
					cpclause->flags=UNPROC;
					cpclause->parent1=clause;
					cpclause->parent2=tobinp->clause;
					cpclause->prevdisp=NULL;
					if (tobinp->agedist<clause->part2.bin->agedist){
						cpclause->part2.bin->agedist=clause->part2.bin->agedist+1;
					} else {
						cpclause->part2.bin->agedist=tobinp->agedist+1;
					}
					if (tobinp->sinedist<clause->part2.bin->sinedist){
						cpclause->part2.bin->sinedist=tobinp->sinedist;
					} else {
						cpclause->part2.bin->sinedist=clause->part2.bin->sinedist;
					}
					cpclause->inference=PARAMODULATION;
					kbset.prstats.paramodulations++;
					cpclause->part2.bin->signature=0;
					cpclause->part2.bin->locksignature=0;
					cpclause->part2.bin->clause=cpclause;
					cpclause->part2.bin->oriented=0;

					/* Build critical pair without substitution. */
					ptr2=&cpclause->part2.bin->formula[0];
					memcpy(ptr2,&tobinp->formula[0],kk);
					memcpy(&ptr2[kk],othrttrm,mm);
					kk+=mm;
					memcpy(&ptr2[kk],unrttrm,pp);
					kk+=pp;
					memcpy(&ptr2[kk],ptr5,qq);
					kk+=qq;
					memcpy(&ptr2[kk],ptr6,rr);
					kk+=rr;
					ptr2[kk]=UNITEND;
					kk++;

					/* Apply substitution to critical pair, set length to the exact size */
					/* and add critical pair top term. */
					if (NOMEMORY==ApplySubst2Clause(cpclause,&kk,&Subst,0)){
						UnshiftUEQVarNbr(tobinp,clause->part2.bin->maxvarnb+1);
						MYFREE(Subst.buffer);
						MYFREE(cpclause->part2.bin);
						MYFREE(cpclause);
						MYFREE(backtrk.treeitems);
						return(NOMEMORY);
					}
					if (NULL==(ptr3=MYREALLOC(cpclause->part2.bin,cpclause->part2.bin->size+binpsize))){
						UnshiftUEQVarNbr(tobinp,clause->part2.bin->maxvarnb+1);
						MYFREE(Subst.buffer);
						MYFREE(cpclause->part2.bin);
						MYFREE(cpclause);
						MYFREE(backtrk.treeitems);
						return(NOMEMORY);
					}
					cpclause->part2.bin=ptr3;

					/* If candidate clause is positive generate critical pair top term. */
					if (0==(NEGATED&tobinp->formula[0])){
						if (NULL==(ptr2=ApplySubst2Term(unrttrm,&Subst,0))){
							UnshiftUEQVarNbr(tobinp,clause->part2.bin->maxvarnb+1);
							MYFREE(Subst.buffer);
							MYFREE(cpclause->part2.bin);
							MYFREE(cpclause);
							MYFREE(backtrk.treeitems);
							return(NOMEMORY);
						}
						cpclause->part2.bin->ovly.cptopterm=ptr2;

					/* If input clause is negative then initialize top term to NULL. */
					} else {
						cpclause->part2.bin->ovly.cptopterm=NULL;
					}

					/* Initialize equality ordering data and reset */
					/* root terms MAXIMUM and SELECTED flags. */
					ptr2=&cpclause->part2.bin->formula[0];
					ptr2=NextItem(ptr2,IMMED);
					ptr2[0]&=(~(MAXIMUM|SELECTED));
					ptr2=NextItem(ptr2,OVERSUBTERMS);
					ptr2[0]&=(~(MAXIMUM|SELECTED));

					/* Debug. */
					#ifdef DEBUGCODE
					ptr10=Decompile(cpclause->part2.bin);
					MYFREE(ptr10);
					#endif

					/* Add critical pair to UNPROC clauses. */
					if (NOMEMORY==AddUEQClause2KB(cpclause)){
						UnshiftUEQVarNbr(tobinp,clause->part2.bin->maxvarnb+1);
						MYFREE(cpclause->part2.bin->ovly.cptopterm);
						MYFREE(cpclause->part2.bin);
						MYFREE(cpclause);
						MYFREE(Subst.buffer);
						MYFREE(backtrk.treeitems);
						return(NOMEMORY);
					}
				}

				/* Unshift variables in candidate binary formula. */
				UnshiftUEQVarNbr(tobinp,clause->part2.bin->maxvarnb+1);
			}
		}
	}

	/* Free memory and return. */
	MYFREE(Subst.buffer);
	MYFREE(backtrk.treeitems);
	return(0);
} /* GenerateCPFrom */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Generate a trivial proof for UEQ problem)  OCJ
 *
 *    This function generates a trivial proof for an UEQ problem
 *    for which we have a goal (kbset.ueqgoal is not NULL) using
 *    a clause of the form x ≐ t where x is a variable that is not
 *    in term t.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: clause of the form x ≐ t where x is a variable
 *            that is not in term t.
 *
 *  RETURNS:
 *
 *    0 -> Process performed without errors.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t MakeTrivialProof(cmprefix *clause){
	auto symbol *smbl;                                   /* Symbol pointer */
	auto int16_t *auxptr;                                /* Auxiliary pointer */
	auto cmprefix *ptr1,*ptr2;                           /* Auxiliary pointers */

	/* Allocate memory for additional clauses. */
	if ((ptr1=MYALLOC(sizeof(cmprefix)))==NULL){
		return(NOMEMORY);
	}
	if ((ptr2=MYALLOC(sizeof(cmprefix)))==NULL){
		MYFREE(ptr1);
		return(NOMEMORY);
	}
	if ((ptr1->part2.bin=MYALLOC(binpsize+8+sizeof(symbol)))==NULL){
		MYFREE(ptr1);
		MYFREE(ptr2);
		return(NOMEMORY);
	}
	if ((ptr2->part2.bin=MYALLOC(binpsize+1))==NULL){
		MYFREE(ptr1->part2.bin);
		MYFREE(ptr1);
		MYFREE(ptr2);
		return(NOMEMORY);
	}

	/* Build new clause ![X0,X1]: (X0=X1) with argument clause as both parents */
	/* and inference PARAMODULATION. */
	ptr1->flags=UNPROC;
	ptr1->inference=PARAMODULATION;
	kbset.prstats.paramodulations++;
	ptr1->parent1=clause;
	ptr1->parent2=clause;
	ptr1->part2.bin->agedist=clause->part2.bin->agedist;
	ptr1->part2.bin->sinedist=clause->part2.bin->sinedist;
	ptr1->part2.bin->clause=ptr1;
	ptr1->part2.bin->ovly.cptopterm=NULL;
	ptr1->part2.bin->lockasserts=NULL;
	ptr1->part2.bin->literals=1;
	ptr1->part2.bin->size=binpsize+8+sizeof(symbol);
	ptr1->part2.bin->maxvarnb=1;
	ptr1->part2.bin->oriented=0;
	ptr1->part2.bin->formula[0]=EQUALITY;
	smbl=(symbol *)&ptr1->part2.bin->formula[1];
	smbl->symbol=&equkey;
	ptr1->part2.bin->formula[1+sizeof(symbol)]=VARIABLE;
	auxptr=(int16_t *)&ptr1->part2.bin->formula[2+sizeof(symbol)];
	*auxptr=0;
	ptr1->part2.bin->formula[4+sizeof(symbol)]=VARIABLE;
	auxptr=(int16_t *)&ptr1->part2.bin->formula[5+sizeof(symbol)];
	*auxptr=1;
	ptr1->part2.bin->formula[7+sizeof(symbol)]=UNITEND;
	UpdateParams2(&ptr1->part2.bin->formula[0]);
	if (NOMEMORY==AddUEQClause2KB(ptr1)){
		MYFREE(ptr1->part2.bin);
		MYFREE(ptr1);
		MYFREE(ptr2->part2.bin);
		MYFREE(ptr2);
		return(NOMEMORY);
	}

	/* Build new empty clause with negeq negative equality and previous */
	/* new clause as parents and inference BKSUBSRESLTNS. */
	ptr2->flags=UNPROC;
	ptr2->inference=BKSUBSRESLTNS;
	kbset.prstats.bksubsresltns++;
	ptr2->parent1=kbset.negeq;
	ptr2->parent2=ptr1;
	ptr2->part2.bin->agedist=clause->part2.bin->agedist;
	ptr2->part2.bin->sinedist=0;
	ptr2->part2.bin->clause=ptr2;
	ptr2->part2.bin->ovly.cptopterm=NULL;
	ptr2->part2.bin->lockasserts=NULL;
	ptr2->part2.bin->literals=0;
	ptr2->part2.bin->size=1;
	ptr2->part2.bin->oriented=0;
	ptr2->part2.bin->formula[0]=UNITEND;
	if (NOMEMORY==AddUEQClause2KB(ptr2)){
		MYFREE(ptr2->part2.bin);
		MYFREE(ptr2);
		return(NOMEMORY);
	}

	/* Set first node for the proof and return. */
	if (NULL==(kbset.frstprfnode=MYALLOC(sizeof(proofnode)))){
		return(NOMEMORY);
	} else {
		kbset.frstprfnode->type=ACLAUSE;
		kbset.frstprfnode->ptr.Aclause=ptr2;
	}
	return(0);
} /* MakeTrivialProof */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform forward simplifications of UNPROC clause)  OCJ
 *
 *    This function performs forward simplifications of a clause extracted
 *    from the UNPROC queue before the clauses are added to KB as PASSIVE.
 *    The clause must be unlinked.
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
 *    pclause: Address of pointer to clause to be simplified in binary
 *             format.
 *
 *  RETURNS:
 *
 *    0 -> Process performed and no empty clause was obtained.
 *    1 -> Process performed and an empty clause was obtained.
 *    2 -> Clause was freed because it was simplified to a tautology.
 *    TIMEOUT -> Time has expired.
 *    NOMEMORY -> Not enough memory.
 *    UNKNOWN -> Problem is UNKNOWN because it was solved by another process.
 *
 *
 *--------------------------------------------------------------*/
int32_t FwUEQSimplify(cmprefix **pclause){
	auto cmprefix *clause;                               /* Clause to be simplified */
	auto uint8_t *ptr1,*ptr2;                            /* Auxiliary pointers */
	auto int32_t ii;                                     /* Auxiliary */

	/* Normalize clause. If algorithm is UEQDSC use only oriented ACTIVE clauses. */
	switch (ii=Normalize(pclause,kbset.opts.algorithm==UEQOTT?1:0)){
		case NOMEMORY:
		case TIMEOUT:
		case UNKNOWN:
			return(ii);
			break;
	}

	/* Return critical pair is an equational tautology. */
	clause=*pclause;
	if (IsEqTautology(clause)){
		MYFREE(clause->part2.bin->ovly.cptopterm);
		MYFREE(clause->part2.bin);
		MYFREE(clause);
		return(2);
	}

	/* Clause is positive. */
	if (0==(NEGATED&clause->part2.bin->formula[0])){

		/* Clause suitable for trivial proof because it is of the */
		/* form x ≐ t where x is a variable that is not in term t. */
		/* The clause is added again to KB as UNPROC. */
		ptr1=NextItem(&clause->part2.bin->formula[0],IMMED);
		ptr2=NextItem(ptr1,OVERSUBTERMS);
		if (((VARIABLE&ptr1[0])&&(0==CheckVarInTerm2(ptr1,ptr2)))
				||((VARIABLE&ptr2[0])&&(0==CheckVarInTerm2(ptr2,ptr1)))){
			if (NOMEMORY==MakeTrivialProof(clause)){
				return(NOMEMORY);
			}
			clause->flags&=(~(UNPROC|PASSIVE|ACTIVE));
			clause->flags|=UNPROC;
			if (NOMEMORY==AddUEQClause2KB(clause)){
				MYFREE(clause->part2.bin->ovly.cptopterm);
				MYFREE(clause->part2.bin);
				MYFREE(clause);
				return(NOMEMORY);
			}
			return(1);
		}

	/* Inferred clause is negative. */
	} else {

		/* Call UEQEqResolution() for critical pair. */
		if (0!=(ii=UEQEqResolution(pclause,1))){
			MYFREE(clause->part2.bin->ovly.cptopterm);
			MYFREE(clause->part2.bin);
			MYFREE(clause);
			return(ii);
		}

		/* Check empty clause. */
		if ((*pclause)->part2.bin->formula[0]==UNITEND){
			return(1);
		}
	}

	/* Perform resolution "simplification". */
	if (0==(ii=UEQResolution(pclause))){

		/* Check empty clause. */
		if ((*pclause)->part2.bin->formula[0]==UNITEND){
			return(1);
		}

	/* TIMEOUT, NOMEMORY or UNKNOWN in resolution "simplification". */
	} else {
		MYFREE(clause->part2.bin->ovly.cptopterm);
		MYFREE(clause->part2.bin);
		MYFREE(clause);
		return(ii);
	}
	return(0);
} /* FwUEQSimplify */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Backward simplify active clauses)  OCJ
 *
 *    This function simplifies active clauses by a given clause.
 *
 *    The inference rules used used for this simplification are
 *    simplification, simplification2, composition, composition2,
 *    collapse and collapse2 described in the document "Completion
 *    without failure" by Leo Bachmair.
 *
 *    Additionally the clause is used for backward subsumption
 *    and resolution. Resolution is not a simplification strictly
 *    speaking but it is cheap in unit equation problems and
 *    if successful it immediately solves the problem.
 *
 *    Simplified clauses are flagged as INDISPOSALQUEUE and
 *    are not subject to additional simplifications.
 *
 *    IMPORTANT: The clause passed as argument must not be in
 *    the ACTIVE queue. If this changes in the future then a
 *    check must be added in this function and in BkUEQSubsum()
 *    function to prevent a clause to simplify or subsume itself.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: pointer to equality in binary format that will be used
 *            to backwards simplify ACTIVE clauses.
 *
 *  RETURNS:
 *
 *    0 -> Process performed without errors.
 *    UNSATISFIABLE -> Problem is unsatisfiable.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *    UNKNOWN -> Problem is UNKNOWN because it was solved by another process.
 *
 *
 *--------------------------------------------------------------*/
int32_t BckUEQSimplify(cmprefix *clause){
	auto cmprefix *newclause;                            /* Clause result of simplification */
	auto uint8_t *rtterm1,*rtterm2;                      /* Pointers to root terms in *pclause */
	auto binprefix *smpbinp;                             /* Pointer to binprefix structure of simplified clause */
	auto uint8_t *unifterm;                              /* Pointer to unifying term */
	auto uint8_t *unrttrm;                               /* Pointer to unifying root term */
	auto uint8_t *othrttrm;                              /* Pointer to the non unifying root term */
	auto subst Subst,Subst2;                             /* Substitutions for Generalize() calls */
	auto uint8_t *ptr1,*ptr2,*ptr3;                      /* Auxiliary pointers */
	auto uint8_t *ptr4,*ptr5,*ptr6;                      /* Auxiliary pointers */
	auto int32_t ii,jj,kk,mm,nn,oo,pp;                   /* Auxiliary */

	/* Perform backward subsumption. */
	if (0!=(ii=BkUEQSubsum(clause))){
		return(ii);
	}

	/* Perform resolution. */
	if (0!=(ii=UEQResolution(&clause))){
		return(ii);
	}

	/* Check empty clause. */
	if (clause->part2.bin->formula[0]==UNITEND){
		sem_wait(&procctl->procctl);
		if (procctl->status!=UNSATISFIABLE){
			procctl->status=UNSATISFIABLE;
			procctl->solvekb=procnb;
		}
		sem_post(&procctl->procctl);
		return(UNSATISFIABLE);
	}

	/* Return if clause is negative. */
	if (NEGATED&clause->part2.bin->formula[0]){
		return(0);
	}

	/* Orient equality if it is not oriented. */
	ptr2=NextItem(ptr4=&clause->part2.bin->formula[0],IMMED);
	ptr3=NextItem(ptr2,OVERSUBTERMS);
	if ((0==(MAXIMUM&ptr2[0]))&&(0==(MAXIMUM&ptr3[0]))){

		/* Reset MAXIMUM and SELECTED flags in equality root terms. */
		ptr2[0]&=(~(MAXIMUM|SELECTED));
		ptr3[0]&=(~(MAXIMUM|SELECTED));

		/* Orient (in)equality by setting the SELECTED and MAXIMUM flags */
		/* for equality root terms as appropriate. */
		/* Release equality tree data if the root terms ordering */
		/* is completely defined. */
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

	/* Initialize clause pointer and substitutions for Generalize() calls. */
	Subst.buffer=NULL;
	Subst.substsize=0;
	Subst2.buffer=NULL;
	Subst2.substsize=0;

	/* Get pointers to clause root terms. */
	rtterm1=NextItem(&clause->part2.bin->formula[0],IMMED);
	rtterm2=NextItem(rtterm1,OVERSUBTERMS);

	/* Debug. */
	#ifdef DEBUGCODE
	auto char *ptr10;
	ptr10=Decompile(clause->part2.bin);
	MYFREE(ptr10);
	#endif

	/* Loop through maximal root terms. */
	for (ptr1=(rtterm1[0]&SELECTED?rtterm1:rtterm2);ptr1!=NULL;
			ptr1=(ptr1==rtterm2?NULL:(rtterm2[0]&SELECTED?rtterm2:NULL))){

		/* Loop through discrimination trees of terms and sub-terms of */
		/* selected and non selected root terms of positive and negative */
		/* equalities. */
		ptr3=(ptr1==rtterm1?rtterm2:rtterm1); /* Pointer to the other root term. */
		if (kbset.opts.algorithm==UEQOTT){
			jj=5;
		} else {
			jj=3;
		}
		for (kk=1;kk<jj;kk++){

			/* Get pointer to top tree node. */
			if (kbset.opts.algorithm==UEQOTT){
				ptr2=(((symbol *)&ptr1[1])->symbol)->discr[kk+1];
			} else {
				ptr2=(((symbol *)&ptr1[1])->symbol)->discr[2*kk];
			}

			/* Loop through generalization candidates. */
			nn=0;
			do {

				/* Check timeout and solution by other process. */
				if (procctl->status==TIMEOUT){
					MYFREE(Subst.buffer);
					MYFREE(Subst2.buffer);
					return(TIMEOUT);
				}
				if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
					MYFREE(Subst.buffer);
					MYFREE(Subst2.buffer);
					return(UNKNOWN);
				}
				if (procctl->status==NOMEMORY){
					MYFREE(Subst.buffer);
					MYFREE(Subst2.buffer);
					return(procctl->status);
				}

				/* Get a term in a candidate simplifying clause. */
				if (0!=(ii=QueryInsDscTree(ptr1,&unifterm,ptr2,nn))){
					nn=1; /* Indicate a discrimination tree query has been done. */

					/* Get a pointer to the binary formula of candidate clause */
					/* and iterate if the clause is in disposal queue. */
					smpbinp=(binprefix *)(unifterm-(((symbol *)&unifterm[1])->offset+binpsize));
					if (INDISPOSALQUEUE&smpbinp->clause->flags){
						continue;
					}

					/* Get a pointer to the root terms of candidate clause */
					/* to be simplified. */
					ptr4=NextItem(&smpbinp->formula[0],IMMED);
					ptr5=NextItem(ptr4,OVERSUBTERMS);
					if (unifterm>=ptr5){
						unrttrm=ptr5;
						othrttrm=ptr4;
					} else {
						unrttrm=ptr4;
						othrttrm=ptr5;
					}

				/* If no candidate found then leave the loop. */
				} else {
					break;
				}

				/* Shift variables of clause being simplified. */
				ShiftUEQVarNbr(smpbinp,clause->part2.bin->maxvarnb+1);

				/* Check generalization. */
				switch (Generalize(ptr1,unifterm,&Subst,INITSUBST)){

					/* Not enough memory. */
					case NOMEMORY:
						MYFREE(Subst.buffer);
						MYFREE(Subst2.buffer);
						return(NOMEMORY);
						break;

					/* Not valid generalization, unshift clause variables and iterate. */
					case 0:
						UnshiftUEQVarNbr(smpbinp,clause->part2.bin->maxvarnb+1);
						continue;
						break;
				}

				/* Debug. */
				#ifdef DEBUGCODE
				auto char *ptr10;
				ptr10=Decompile(smpbinp);
				MYFREE(ptr10);
				#endif

				/* If rewriting equation is oriented check collapse specialized ordering constraint. */
				/* In order to understand how this check is done it must be taken into account that */
				/* for the check to fail it is necessary for the root term to be a generalization */
				/* of the rewriting term. As the rewriting term is a generalization of the */
				/* rewritten term unifterm then unifterm itself must be the root term. */
				if (MAXIMUM&ptr1[0]){
					if ((unifterm==unrttrm)&&(MAXIMUM&unrttrm[0])){
						if (NOMEMORY==(mm=Generalize(unrttrm,ptr1,&Subst2,INITSUBST))){
							MYFREE(Subst.buffer);
							MYFREE(Subst2.buffer);
							return(NOMEMORY);
						} else if (mm){
							UnshiftUEQVarNbr(smpbinp,clause->part2.bin->maxvarnb+1);
							continue;
						}
					}

				/* Rewriting equation is not oriented. */
				} else {

					/* Check that the rewriting equation is oriented under the substitution. */
					if (NOMEMORY==(mm=CheckEqRootOrder(ptr1,ptr3,&Subst,0,smpbinp->maxvarnb+clause->part2.bin->maxvarnb+1,MAXIMUM))){
						MYFREE(Subst.buffer);
						MYFREE(Subst2.buffer);
						return(NOMEMORY);
					} else if (mm==0){
						UnshiftUEQVarNbr(smpbinp,clause->part2.bin->maxvarnb+1);
						continue;
					}

					/* Check simplification2 and collapse2 specialized ordering constraints. */
					/* In order to understand how this check is done it must be taken into account that */
					/* for the check to fail it is necessary for the root term to be a generalization */
					/* of the rewriting term. As the rewriting term is a generalization of the */
					/* rewritten term unifterm then unifterm itself must be the root term. */
					if (((MAXIMUM&unrttrm[0])||(0==(MAXIMUM&othrttrm[0])))&&(unifterm==unrttrm)){
						if (NOMEMORY==(mm=Generalize(unrttrm,ptr1,&Subst2,INITSUBST))){
							MYFREE(Subst.buffer);
							MYFREE(Subst2.buffer);
							return(NOMEMORY);
						} else if (mm){
							UnshiftUEQVarNbr(smpbinp,clause->part2.bin->maxvarnb+1);
							continue;
						}
					}
				}

				/* Compute necessary pointers and lengths to build rewritten result. */
				ptr4=NextItem(unifterm,OVERSUBTERMS);
				if (NULL==(ptr5=ApplySubst2Term(ptr3,&Subst,0))){
					MYFREE(Subst.buffer);
					MYFREE(Subst2.buffer);
					return(NOMEMORY);
				}
				mm=unifterm-&smpbinp->formula[0];
				oo=NextItem(ptr5,OVERSUBTERMS)-ptr5;
				pp=NextItem(&smpbinp->formula[0],OVERSUBTERMS)-ptr4;

				/* Allocate memory for new clause. */
				if (NULL==(newclause=MYALLOC(sizeof(cmprefix)))){
					MYFREE(ptr5);
					MYFREE(Subst.buffer);
					MYFREE(Subst2.buffer);
					return(NOMEMORY);
				}
				if (NULL==((newclause->part2.bin)=MYALLOC(binpsize+1+mm+oo+pp))){
					MYFREE(newclause);
					MYFREE(ptr5);
					MYFREE(Subst.buffer);
					MYFREE(Subst2.buffer);
					return(NOMEMORY);
				}

				/* Initialize new clause cmprefix and binprefix structures. */
				newclause->parent1=clause;
				newclause->parent2=smpbinp->clause;
				newclause->prevdisp=NULL;
				if (clause->part2.bin->agedist<smpbinp->agedist){
					newclause->part2.bin->agedist=smpbinp->agedist+1;
				} else {
					newclause->part2.bin->agedist=clause->part2.bin->agedist+1;
				}
				if (clause->part2.bin->sinedist<smpbinp->sinedist){
					newclause->part2.bin->sinedist=clause->part2.bin->sinedist;
				} else {
					newclause->part2.bin->sinedist=smpbinp->sinedist;
				}
				newclause->inference=BKDEMODULATION;
				kbset.prstats.bkdemodulations++;
				newclause->part2.bin->literals=1;
				newclause->part2.bin->signature=0;
				newclause->part2.bin->locksignature=0;
				newclause->part2.bin->clause=newclause;
				newclause->part2.bin->ovly.cptopterm=NULL;

				/* Build new clause. */
				ptr6=&newclause->part2.bin->formula[0];
				memcpy(ptr6,&smpbinp->formula[0],mm);
				memcpy(&ptr6[mm],ptr5,oo);
				MYFREE(ptr5);
				mm+=oo;
				memcpy(&ptr6[mm],ptr4,pp);
				mm+=pp;
				ptr6[mm]=UNITEND;
				newclause->part2.bin->size=mm+1;

				/* Adjust offset and nextoff fields in new clause terms. */
				UpdateParams2(ptr6);

				/* Unshift variables of clause being simplified. */
				UnshiftUEQVarNbr(smpbinp,clause->part2.bin->maxvarnb+1);

				/* Original simplified clause was oriented and the rewritten */
				/* root term was not the MAXIMUM root term then the inference */
				/* was composition or composition2. Keep the new clause oriented. */
				ptr4=NextItem(ptr6,IMMED);
				ptr5=NextItem(ptr4,OVERSUBTERMS);
				if (MAXIMUM&othrttrm[0]){
					newclause->part2.bin->oriented=smpbinp->oriented;
					if (unrttrm==NextItem(&smpbinp->formula[0],IMMED)){
						ptr4[0]&=(~(MAXIMUM|SELECTED));
						ptr5[0]|=(MAXIMUM|SELECTED);
					} else {
						ptr5[0]&=(~(MAXIMUM|SELECTED));
						ptr4[0]|=(MAXIMUM|SELECTED);
					}

				/* Original simplified clause was not oriented or the rewritten */
				/* root term was the MAXIMUM root term. Reset root terms MAXIMUM */
				/* and SELECTED flags in new clause. */
				} else {
					ptr4[0]&=(~(MAXIMUM|SELECTED));
					ptr5[0]&=(~(MAXIMUM|SELECTED));
					newclause->part2.bin->oriented=0;
				}

				/* Add original simplified clause to disposal queue. */
				smpbinp->clause->flags|=INDISPOSALQUEUE;
				smpbinp->clause->prevdisp=kbset.lastdisp;
				kbset.lastdisp=smpbinp->clause;

				/* Add the new simplified clause to UNPROC. */
				newclause->flags=UNPROC;
				if (NOMEMORY==AddUEQClause2KB(newclause)){
					MYFREE(newclause->part2.bin);
					MYFREE(newclause);
					MYFREE(Subst.buffer);
					MYFREE(Subst2.buffer);
					return(NOMEMORY);
				}
			} while(ii);
		}
	}

	/* Free memory and return. */
	MYFREE(Subst.buffer);
	MYFREE(Subst2.buffer);
	return(0);
} /* BckUEQSimplify */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Equality resolution inference for unfailing completion)  OCJ
 *
 *    This function performs equality resolution simplification for
 *    a negative clause in completion without failure algorithm.
 *
 *    If an equality resolution can be done then a refutation
 *    has been found.
 *
 *
 *  ARGUMENTS:
 *
 *    pclause: Address of pointer to negative equality clause to be
 *             simplified. On exit this points to the address of
 *             the generated empty clause.
 *    flag: If this is not zero then the clause pclause is not linked
 *          so it will be decompiled and added to working KB
 *          as text clause. If pclause is linked to a KB then
 *          flag must be zero.
 *
 *  RETURNS:
 *
 *    0 -> Process performed without errors.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *
 *--------------------------------------------------------------*/
int32_t UEQEqResolution(cmprefix **pclause,int32_t flag){
	auto cmprefix *clause;                               /* Pointer to goal */
	auto cmprefix *auxcl;                                /* Pointer to auxiliary clause */
	auto subst Subst;                                    /* Substitution */
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	auto uint8_t *ptr1;                                  /* Auxiliary pointers */
	auto int32_t ii;                                     /* Auxiliary */

	/* Initialization. */
	Subst.buffer=NULL;
	clause=*pclause;

	/* Check unification of equality root terms. */
	ptr1=NextItem(&clause->part2.bin->formula[0],IMMED);
	ii=Unify(ptr1,NextItem(ptr1,OVERSUBTERMS),&Subst,INITSUBST);
	MYFREE(Subst.buffer);
	switch (ii){

		/* Not enough memory. */
		case NOMEMORY:
			return(NOMEMORY);
			break;

		/* Equality root terms unify. */
		case 1:

			/* Allocate storage for new empty clause and initialize it. */
			if ((auxcl=MYALLOC(sizeof(cmprefix)))==NULL){
				return(NOMEMORY);
			}
			if ((auxcl->part2.bin=MYALLOC(binpsize+1))==NULL){
				MYFREE(auxcl);
				return(NOMEMORY);
			}
			auxcl->flags=UNPROC;
			auxcl->inference=EQRESOLUTION;
			kbset.prstats.equresolutions++;
			auxcl->parent1=clause;
			auxcl->parent2=NULL;
			auxcl->part2.bin->agedist=clause->part2.bin->agedist;
			auxcl->part2.bin->sinedist=clause->part2.bin->sinedist;
			auxcl->part2.bin->clause=auxcl;
			auxcl->part2.bin->ovly.cptopterm=NULL;
			auxcl->part2.bin->lockasserts=NULL;
			auxcl->part2.bin->literals=0;
			auxcl->part2.bin->size=1;
			auxcl->part2.bin->oriented=0;
			auxcl->part2.bin->formula[0]=UNITEND;

			/* If flag is not zero then convert old negative clause */
			/* to text form and add it to text clauses. */
			if (flag){
				if (NULL==(ptr10=Decompile(clause->part2.bin))){
					MYFREE(auxcl->part2.bin);
					MYFREE(auxcl);
					return(NOMEMORY);
				}
				MYFREE(clause->part2.bin);
				clause->part2.text=ptr10;
				AddTxtFormula2KB(clause,clause->inference,&kbset);
			}

			/* Add new empty clause to working KB.  */
			*pclause=auxcl;
			if (NOMEMORY==AddUEQClause2KB(auxcl)){
				MYFREE(auxcl->part2.bin);
				MYFREE(auxcl);
				return(NOMEMORY);
			}

			/* Set first node for the proof. */
			if (NULL==(kbset.frstprfnode=MYALLOC(sizeof(proofnode)))){
				return(NOMEMORY);
			} else {
				kbset.frstprfnode->type=ACLAUSE;
				kbset.frstprfnode->ptr.Aclause=auxcl;
			}
			break;
	}

	return(0);
} /* UEQEqResolution */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Resolution goal simplification for unfailing completion)  OCJ
 *
 *    This function performs resolution simplification for an (in)equality
 *    and all the appropriate active clauses in the working KB for
 *    completion without failure algorithm.
 *
 *    If a resolution can be done then a refutation has been found.
 *
 *    IMPORTANT: The clause addressed by pclause must not be linked
 *    to any KB because it may be possible decompiled and added to
 *    working KB as text formula.
 *
 *
 *  ARGUMENTS:
 *
 *    pclause: Address of pointer to (in)equality to be used for
 *             performing resolution. On exit this points to the
 *             address of the generated empty clause.
 *
 *  RETURNS:
 *
 *    0 -> Process performed without errors.
 *    TIMEOUT -> Time has expired or other process has solved the problem.
 *    NOMEMORY -> Not enough memory.
 *    UNKNOWN -> Problem is UNKNOWN because it was solved by another process.
 *
 *
 *
 *--------------------------------------------------------------*/
int32_t UEQResolution(cmprefix **pclause){
	auto cmprefix *clause;                               /* Pointer to goal */
	auto subst Subst;                                    /* Substitution */
	auto uint8_t *equality;                              /* Pointer to equality with root terms in reverse order */
	auto uint8_t *eqterms;                               /* Pointer to equality root terms in reverse order */
	auto uint8_t *queryitm;                              /* Pointer to QueryDscTree2() query item */
	auto cmprefix *inferred;                             /* Pointer to inferred clause */
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4;                /* Auxiliary pointers */
	auto int32_t ii,jj,kk,mm,nn,rr,ss;                   /* Auxiliary */

	/* Initialization. */
	Subst.buffer=NULL;
	eqterms=equality=NULL;
	clause=*pclause;
	ptr1=&clause->part2.bin->formula[0];

	/* Loop through discrimination trees. */
	if (kbset.opts.algorithm==UEQOTT){
		mm=2;
	} else {
		mm=1;
	}
	for (ii=0;ii<mm;ii++){

		/* Get pointer to discrimination tree. */
		if (0==(NEGATED&ptr1[0])){
			if (kbset.opts.algorithm==UEQOTT){
				ptr2=(((symbol *)&ptr1[1])->symbol)->discr[2*(ii+1)];
			} else {
				ptr2=(((symbol *)&ptr1[1])->symbol)->discr[2];
			}
		} else {
			if (kbset.opts.algorithm==UEQOTT){
				ptr2=(((symbol *)&ptr1[1])->symbol)->discr[2*(ii+1)];
				if (ii){
					ptr2=(((symbol *)&ptr1[1])->symbol)->discr[3];
				} else {
					ptr2=(((symbol *)&ptr1[1])->symbol)->discr[0];
				}
			} else {
				ptr2=(((symbol *)&ptr1[1])->symbol)->discr[0];
			}
		}

		/* Loop for equality symmetry management. */
		for (kk=0;kk<2;kk++){

			/* This is the first iteration. */
			if (kk==0){
				queryitm=ptr1;

			/* This is the second iteration for the equality literal. */
			/* Build an equality with root terms in reverse order. */
			} else {
				if (ii==0){
					ptr4=NextItem(ptr1,IMMED);
					ptr3=NextItem(ptr4,OVERSUBTERMS);
					rr=ptr3-ptr4;
					ss=NextItem(ptr3,OVERSUBTERMS)-ptr3;
					if (NULL==(equality=MYALLOC(2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
						MYFREE(Subst.buffer);
						MYFREE(equality);
						return(NOMEMORY);
					}
					eqterms=&equality[1+sizeof(symbol)];
					memcpy(equality,ptr1,1+sizeof(symbol));
					memcpy(eqterms,ptr3,ss);
					memcpy(&eqterms[ss],ptr4,rr);
					eqterms[rr+ss]=UNITEND;
				}
				queryitm=equality;
			}

			/* Loop through candidates for unification. */
			jj=1;
			nn=0;
			while (jj){

				/* Check timeout and solution by other process. */
				if (procctl->status==TIMEOUT){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(TIMEOUT);
				}
				if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(UNKNOWN);
				}
				if (procctl->status==NOMEMORY){
					MYFREE(Subst.buffer);
					MYFREE(equality);
					return(procctl->status);
				}

				/* Get candidate for unification. */
				if (0!=(jj=QueryUnfDscTree(queryitm,&ptr3,ptr2,nn))){

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

							/* Allocate storage for new empty clause and initialize it. */
							if ((inferred=MYALLOC(sizeof(cmprefix)))==NULL){
								MYFREE(Subst.buffer);
								MYFREE(equality);
								return(NOMEMORY);
							}
							if ((inferred->part2.bin=MYALLOC(binpsize+1))==NULL){
								MYFREE(inferred);
								MYFREE(Subst.buffer);
								MYFREE(equality);
								return(NOMEMORY);
							}
							inferred->flags=UNPROC;
							inferred->inference=RESOLUTION;
							kbset.prstats.resolutions++;
							inferred->parent1=clause;
							inferred->parent2=((binprefix *)(ptr3-binpsize))->clause;
							inferred->part2.bin->agedist=clause->part2.bin->agedist;
							inferred->part2.bin->sinedist=clause->part2.bin->sinedist;
							inferred->part2.bin->clause=inferred;
							inferred->part2.bin->ovly.cptopterm=NULL;
							inferred->part2.bin->lockasserts=NULL;
							inferred->part2.bin->literals=0;
							inferred->part2.bin->size=1;
							inferred->part2.bin->oriented=0;
							inferred->part2.bin->formula[0]=UNITEND;

							/* Convert original clause to text form and add it to text clauses. */
							if (NULL==(ptr10=Decompile(clause->part2.bin))){
								MYFREE(inferred->part2.bin);
								MYFREE(inferred);
								MYFREE(Subst.buffer);
								MYFREE(equality);
								return(NOMEMORY);
							}
							MYFREE(clause->part2.bin->ovly.cptopterm);
							MYFREE(clause->part2.bin);
							clause->part2.text=ptr10;
							AddTxtFormula2KB(clause,clause->inference,&kbset);

							/* Add empty clause to the proof. */
							*pclause=inferred;
							if (NOMEMORY==AddUEQClause2KB(inferred)){
								MYFREE(inferred->part2.bin);
								MYFREE(inferred);
								MYFREE(Subst.buffer);
								MYFREE(equality);
								return(NOMEMORY);
							}

							/* Set first node for the proof. */
							if (NULL==(kbset.frstprfnode=MYALLOC(sizeof(proofnode)))){
								MYFREE(Subst.buffer);
								MYFREE(equality);
								return(NOMEMORY);
							} else {
								kbset.frstprfnode->type=ACLAUSE;
								kbset.frstprfnode->ptr.Aclause=inferred;
							}

							/* Free memory and return. */
							MYFREE(Subst.buffer);
							MYFREE(equality);
							return(0);
							break;
					}
				}
			}
		}
	}

	/* Free memory and return. */
	MYFREE(Subst.buffer);
	MYFREE(equality);
	return(0);
} /* UEQResolution */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Apply an instance substitution to a term)  OCJ
 *
 *    This function is similar to ApplySubst2Clause() function with
 *    the following differences:
 *    - A buffer for the resulting term is allocated by the function.
 *    - It returns a pointer to the buffer allocated for the resulting
 *      new term.
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
uint8_t *ApplySubst2Term(uint8_t *term,subst *Subst,int32_t varshift){
	auto uint8_t *newterm;                               /* Pointer to resulting new term of each iteration */
	auto uint8_t *inputterm;                             /* Pointer to input term in each iteration */
	auto int32_t *psize;                                 /* Pointer to size of *pnewterm buffer */
	auto uint8_t *buffer1,*buffer2;                      /* Pointers to auxiliary buffers for resulting new term */
	auto int32_t size1,size2;                            /* Sizes of buffer1 and buffer2 buffers */
	auto int32_t maxvarnb;                               /* Maximum number assigned to variable in substitution */
	auto uint8_t **substterms;                           /* Address for set of pointers of substituting terms */
	auto uint8_t *ptr1,*ptr2;                            /* Auxiliary pointer */
	auto int32_t ii,jj,kk;                               /* Auxiliary */

	/* If substitution is empty then duplicate term and return. */
	if ((Subst->buffer==NULL)||(Subst->substsize==1)){
		size1=NextItem(term,OVERSUBTERMS)-term;
		if (NULL!=(newterm=MYALLOC(size1+1))){
			memcpy(newterm,term,size1);
			newterm[0]&=(~(MAXIMUM|SELECTED));
			newterm[size1]=UNITEND;
		}
		return(newterm);
	}

	/* Allocate memory for auxiliary buffers. */
	size1=size2=3*(NextItem(term,OVERSUBTERMS)-term);
	if (NULL==(buffer1=MYALLOC(size1))){
		return(NULL);
	}
	if (NULL==(buffer2=MYALLOC(size2))){
		MYFREE(buffer1);
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
		MYFREE(buffer1);
		MYFREE(buffer2);
		return(NULL);
	}
	memset(substterms,0,ii);

	/* Initialize set of pointers of substituting terms and */
	/* adjust substitution (see GetSubstTermData() for details). */
	GetSubstTermData(Subst,substterms,varshift);

	/* Loop until no substitution is applied. */
	inputterm=term;
	newterm=buffer1;
	psize=&size1;
	jj=1;
	do {

		/* Perform substitution of the term. */
		ptr1=newterm;
		if (NOMEMORY==(ii=ApplySubst2Item(&ptr1,(binprefix **)&newterm,psize,
				inputterm,maxvarnb,substterms))){
			if (jj==1){
				MYFREE(buffer2);
			} else {
				MYFREE(buffer1);
			}
			MYFREE(newterm);
			MYFREE(substterms);
			return(NULL);
		}

		/* Copy UNITEND byte to destination buffer */
		/* and update newterm parameters. */
		if ((*psize)==(ptr1-newterm)){
			kk=(*psize)+CLAUSE_CHUNK_SIZE;
			if (NULL==(ptr2=MYREALLOC(newterm,kk))){
				if (jj==1){
					MYFREE(buffer2);
				} else {
					MYFREE(buffer1);
				}
				MYFREE(newterm);
				MYFREE(substterms);
				return(NULL);
			}
			ptr1=ptr2+*psize;
			newterm=ptr2;
			*psize=kk;
		}
		ptr1[0]=UNITEND;
		UpdateParams2(newterm);

		/* Prepare next iteration. */
		if (ii){
			if (jj==1){
				buffer1=inputterm=newterm;
				newterm=buffer2;
				psize=&size2;
				jj=2;
			} else {
				buffer2=inputterm=newterm;
				newterm=buffer1;
				psize=&size1;
				jj=1;
			}
		} else {
			MYFREE(substterms);
			if (jj==1){
				MYFREE(buffer2);
			} else {
				MYFREE(buffer1);
			}
			break;
		}
	} while (1);

	/* Reallocate newterm to the exact size needed and return. */
	if (NULL==(ptr1=MYREALLOC(newterm,1+(ptr1-newterm)))){
		MYFREE(newterm);
		return(NULL);
	}
	newterm=ptr1;
	newterm[0]&=(~(MAXIMUM|SELECTED));
	return(ptr1);
} /* ApplySubst2Term */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Shift unit equality variable numbers)  OCJ
 *
 *    This function shifts variable numbers in a unit equality with
 *    possibly a top term if it is a critical pair.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    binp: Pointer to binprefix structure of equality or critical
 *          pair whose variable numbers will be shifted.
 *    shift: Shift to apply to variables in item.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *--------------------------------------------------------------*/
void ShiftUEQVarNbr(binprefix *binp,int32_t shift){
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	for (ptr1=&binp->formula[0];ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){
		if (VARIABLE&ptr1[0]){
			*((int16_t *)&ptr1[1])+=shift;
		}
	}
	if (binp->ovly.cptopterm!=NULL){
		for (ptr1=binp->ovly.cptopterm;ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){
			if (VARIABLE&ptr1[0]){
				*((int16_t *)&ptr1[1])+=shift;
			}
		}
	}
	binp->maxvarnb+=shift;
	return;
} /* ShiftUEQVarNbr */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Unshift unit equality variable numbers)  OCJ
 *
 *    This function shifts variable numbers in a unit equality with
 *    possibly a top term if it is a critical pair.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    binp: Pointer to binprefix structure of equality or critical
 *          pair whose variable numbers will be unshifted.
 *    unshift: Unshift to apply to variables in item.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *--------------------------------------------------------------*/
void UnshiftUEQVarNbr(binprefix *binp,int32_t unshift){
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	for (ptr1=&binp->formula[0];ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){
		if (VARIABLE&ptr1[0]){
			*((int16_t *)&ptr1[1])-=unshift;
		}
	}
	if (binp->ovly.cptopterm!=NULL){
		for (ptr1=binp->ovly.cptopterm;ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){
			if (VARIABLE&ptr1[0]){
				*((int16_t *)&ptr1[1])-=unshift;
			}
		}
	}
	binp->maxvarnb-=unshift;
	return;
} /* UnshiftUEQVarNbr */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Generate next permutation)  OCJ
 *
 *    This function generates a permutation taking as parameter
 *    another permutation using the non recursive version of
 *    Heap's algorithm.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    perm: Pointer to permutation. On input it contains the
 *          current permutation. On output it contains the
 *          permutation following the current permutation.
 *          The first call permutation must be initialized by
 *          by the calling function. If the permutation is not
 *          valid then results are unpredictable.
 *    ctrl: Pointer to control set of size elements of non recursive
 *          Heap's algorithm. It must be initialized to zeroes by
 *          the calling function for the first call to this function
 *          and don't modify it thereafter.
 *    pidx: Pointer to control index non recursive Heap's
 *          algorithm. The control index must be initialized to 1
 *          by the calling function for the first call to this
 *          function and don't modify it thereafter.
 *    size: Number of elements of permutation.

 *
 *  RETURNS:
 *
 *    1 if a new permutation has been generated.
 *    0 if there are no more permutations.
 *
 *
 *--------------------------------------------------------------*/
int32_t NextPerm(int32_t *perm,int32_t *ctrl,int32_t *pidx,int32_t size){
	auto int32_t ii;                                       /* Auxiliary */

	/*Main loop. */
	while ((*pidx)<size){

		/* New permutation must be built.. */
		if (ctrl[*pidx]<(*pidx)){

			/* Build new permutation. */
			if (1&(*pidx)){
				ii=perm[ctrl[*pidx]];
				perm[ctrl[*pidx]]=perm[*pidx];
				perm[*pidx]=ii;
			} else {
				ii=perm[0];
				perm[0]=perm[*pidx];
				perm[*pidx]=ii;
			}

			/* Adjust control data and return. */
			ctrl[*pidx]++;
			*pidx=1;
			return(1);

		/* Adjust control. */
		} else {
			ctrl[*pidx]=0;
			(*pidx)++;
		}
	}

	return(0);
} /* NextPerm */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Split unit equalities and add to ACTIVE)  OCJ
 *
 *    This function is a front end to add ACTIVE clauses to working KB.
 *    It checks if a clause is a positive unit equality where both
 *    sides have one or more variables possibly with several instances
 *    not occurring on the other side. In that case the equality is split
 *    as described in section 2 of document "Twee: An Equational Theorem
 *    Prover (System Description)" by Nicholas Smallbone. Otherwise the
 *    clause is just added to working KB.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    pclause: Pointer to set of pointers to clause. On entry this set
 *             has the input clause as its only element. On exit if
 *             the clause has not been split then its content is left
 *             without change but if the clause has been split it
 *             contains the set of clauses resulting from the split.
 *
 *  RETURNS:
 *
 *    0 if successful.
 *    NOMEMORY if not enough memory.
 *    SLICETMOUT if hardware instructions limit exceeded.
 *    TIMEOUT if a timeout condition has occurred.
 *    UNKNOWN if problem solved by another process.
 *
 *
 *--------------------------------------------------------------*/
int32_t SplitNAdd2Active(cmprefix **pclause){
	auto cmprefix *clause;                                 /* Pointer to original clause */
	auto int32_t *vardata;                                 /* Pointer to set of variable data */
	auto uint8_t *rttrm1,*rttrm2;                          /* Pointers to equality root terms */
	auto uint8_t *sbrttrm1,*sbrttrm2;                      /* Pointers to equality root terms with substitution applied */
	auto int32_t length1,length2;                          /* Length of equality root terms with substitution applied */
	auto cmprefix *auxcl1;                                 /* Auxiliary pointer for clauses resulting from equality split */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4;                  /* Auxiliary pointers */
	auto char *ptr10;                                      /* Auxiliary pointer */
	auto int32_t ii,jj,kk,pp;                              /* Auxiliary */

	/* If weak rewriting is not enabled or the clause is not a positive equality */
	/* unit clause or has no variables then just add it to working kb as ACTIVE. */
	clause=pclause[0];
	if ((kbset.opts.weakrw==0)||(clause->part2.bin->literals!=1)
			||(EQUALITY!=((NEGATED|EQUALITY)&clause->part2.bin->formula[0]))
			||(clause->part2.bin->maxvarnb==-1)||(minterm==NULL)){
		clause->flags&=(~WEAKLYORIENTED);
		switch (kbset.opts.algorithm){
			case OTTER:
			case DISCOUNT:
				if (pbmtype==8){
					return(AddBinClause2KBUEQ(clause,&kbset,SELECT));
				} else {
					return(AddBinClause2KB(clause,&kbset,SELECT));
				}
				break;
			default:
				return(AddUEQClause2KB(clause));
				break;
		}
	}

	/* Allocate and set temporary memory to store variable data. */
	kk=clause->part2.bin->maxvarnb+1;
	if (NULL==(vardata=MYALLOC(ii=kk*sizeof(*vardata)))){
		return(NOMEMORY);
	}
	memset(vardata,0,ii);

	/* Check if both sides of equality have variables not occurring */
	/* in the other side. Variable jj is set to 3 if test is positive. */
	rttrm1=NextItem(&clause->part2.bin->formula[0],IMMED);
	rttrm2=NextItem(rttrm1,OVERSUBTERMS);
	for (ptr1=rttrm1;ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){
		if (VARIABLE&ptr1[0]){
			ii=*((uint16_t *)&ptr1[1]);
			if (ptr1<rttrm2){
				vardata[ii]|=1;
			} else {
				vardata[ii]|=2;
			}
		}
	}
	for (ii=jj=0;ii<kk;ii++){
		if (vardata[ii]!=3){
			jj|=vardata[ii];
		}
	}

	/* If there is at least one side of the equality with all its variables */
	/* occurring in the other side then add clause to working kb as ACTIVE */
	/* and return. */
	if (jj!=3){
		clause->flags&=(~WEAKLYORIENTED);
		MYFREE(vardata);
		switch (kbset.opts.algorithm){
			case OTTER:
			case DISCOUNT:
				if (pbmtype==8){
					return(AddBinClause2KBUEQ(clause,&kbset,SELECT));
				} else {
					return(AddBinClause2KB(clause,&kbset,SELECT));
				}
				break;
			default:
				return(AddUEQClause2KB(clause));
				break;
		}
	}

	/* Generate new root terms with each variable not occurring in the */
	/* other root term replaced by the minimal term in the term ordering. */
	if (NULL==(sbrttrm1=ApplyFastSubst(rttrm1,vardata,&length1))){
		MYFREE(vardata);
		return(NOMEMORY);
	}
	if (NULL==(sbrttrm2=ApplyFastSubst(rttrm2,vardata,&length2))){
		MYFREE(vardata);
		MYFREE(sbrttrm1);
		return(NOMEMORY);
	}
	MYFREE(vardata); /* No longer required. */

	/* Compare root terms with substitution applied. */
	rttrm1[0]&=(~MAXIMUM);
	rttrm2[0]&=(~MAXIMUM);
	switch (CompareItems(sbrttrm1,sbrttrm2,clause->part2.bin->maxvarnb)){
		case GREATER:
			sbrttrm1[0]|=MAXIMUM;
			ii=1;
			break;
		case LESSER:
			sbrttrm2[0]|=MAXIMUM;
			ii=1;
			break;
		default:
			ii=0;
			break;
	}

	/* One of the root terms is MAXIMUM with substitution applied. */
	if (ii){

		/* Allocate storage for oriented equality. */
		if (NULL==(auxcl1=MYALLOC(sizeof(cmprefix)))){
			MYFREE(sbrttrm1);
			MYFREE(sbrttrm2);
			return(NOMEMORY);
		}
		if (sbrttrm1[0]&MAXIMUM){
			sbrttrm1[0]&=(~MAXIMUM);
			ii=NextItem(rttrm1,OVERSUBTERMS)-rttrm1;
			ptr2=rttrm1;
			jj=length2;
			ptr3=sbrttrm2;
			pp=NextItem(rttrm2,OVERSUBTERMS)-rttrm2;
			ptr4=rttrm2;
		} else {
			sbrttrm2[0]&=(~MAXIMUM);
			ii=NextItem(rttrm2,OVERSUBTERMS)-rttrm2;
			ptr2=rttrm2;
			jj=length1;
			ptr3=sbrttrm1;
			pp=NextItem(rttrm1,OVERSUBTERMS)-rttrm1;
			ptr4=rttrm1;
		}
		kk=2+sizeof(symbol)+ii+jj;
		if (NULL==((auxcl1->part2.bin)=MYALLOC(binpsize+kk))){
			MYFREE(auxcl1);
			MYFREE(sbrttrm1);
			MYFREE(sbrttrm2);
			return(NOMEMORY);
		}

		/* Build oriented equality. */
		auxcl1->flags=ACTIVE|HORN;
		auxcl1->parent1=clause;
		auxcl1->parent2=NULL;
		auxcl1->prevdisp=NULL;
		auxcl1->part2.bin->agedist=clause->part2.bin->agedist+1;
		auxcl1->part2.bin->sinedist=clause->part2.bin->sinedist;
		auxcl1->inference=EQUALITYSPLIT;
		auxcl1->part2.bin->signature=0;
		auxcl1->part2.bin->locksignature=0;
		auxcl1->part2.bin->clause=auxcl1;
		auxcl1->part2.bin->oriented=0;
		auxcl1->part2.bin->ovly.asserts=auxcl1->part2.bin->lockasserts=NULL;
		auxcl1->part2.bin->literals=1;
		auxcl1->part2.bin->size=kk;
		ptr1=&auxcl1->part2.bin->formula[0];
		ptr1[0]=EQUALITY|SELECTED;
		((symbol *)&ptr1[1])->symbol=&equkey;
		ptr1=&ptr1[1+sizeof(symbol)];
		memcpy(ptr1,ptr2,ii);
		ptr1[0]|=(MAXIMUM|SELECTED);
		memcpy(&ptr1[ii],ptr3,jj);
		ptr1[ii+jj]=UNITEND;
		UpdateParams2(&auxcl1->part2.bin->formula[0]);
		pclause[0]=auxcl1;

		/* Allocate storage for weakly oriented equality. */
		if (NULL==(auxcl1=MYALLOC(sizeof(cmprefix)))){
			MYFREE(sbrttrm1);
			MYFREE(sbrttrm2);
			MYFREE(pclause[0]->part2.bin);
			MYFREE(pclause[0]);
			return(NOMEMORY);
		}
		kk=2+sizeof(symbol)+pp+jj;
		if (NULL==((auxcl1->part2.bin)=MYALLOC(binpsize+kk))){
			MYFREE(sbrttrm1);
			MYFREE(sbrttrm2);
			MYFREE(auxcl1);
			MYFREE(pclause[0]->part2.bin);
			MYFREE(pclause[0]);
			return(NOMEMORY);
		}

		/* Build weakly oriented equality. */
		auxcl1->flags=ACTIVE|HORN|WEAKLYORIENTED;
		auxcl1->parent1=clause;
		auxcl1->parent2=NULL;
		auxcl1->prevdisp=NULL;
		auxcl1->part2.bin->agedist=clause->part2.bin->agedist+1;
		auxcl1->part2.bin->sinedist=clause->part2.bin->sinedist;
		auxcl1->inference=EQUALITYSPLIT;
		auxcl1->part2.bin->signature=0;
		auxcl1->part2.bin->locksignature=0;
		auxcl1->part2.bin->clause=auxcl1;
		auxcl1->part2.bin->oriented=0;
		auxcl1->part2.bin->ovly.asserts=auxcl1->part2.bin->lockasserts=NULL;
		auxcl1->part2.bin->literals=1;
		auxcl1->part2.bin->size=kk;
		ptr1=&auxcl1->part2.bin->formula[0];
		ptr1[0]=EQUALITY|SELECTED;
		((symbol *)&ptr1[1])->symbol=&equkey;
		ptr1=&ptr1[1+sizeof(symbol)];
		memcpy(ptr1,ptr4,pp);
		ptr1[0]|=SELECTED;
		memcpy(&ptr1[pp],ptr3,jj);
		ptr1[pp+jj]=UNITEND;
		UpdateParams2(&auxcl1->part2.bin->formula[0]);
		pclause[1]=auxcl1;
		pclause[2]=NULL;

	/*None of the root terms is MAXIMUM with substitution applied. */
	} else {

		/* Allocate storage for first weakly oriented equality. */
		if (NULL==(auxcl1=MYALLOC(sizeof(cmprefix)))){
			MYFREE(sbrttrm1);
			MYFREE(sbrttrm2);
			return(NOMEMORY);
		}
		ii=NextItem(rttrm1,OVERSUBTERMS)-rttrm1;
		jj=NextItem(rttrm2,OVERSUBTERMS)-rttrm2;
		kk=2+sizeof(symbol)+ii+length1;
		if (NULL==((auxcl1->part2.bin)=MYALLOC(binpsize+kk))){
			MYFREE(sbrttrm1);
			MYFREE(sbrttrm2);
			MYFREE(auxcl1);
			return(NOMEMORY);
		}

		/* Build first weakly oriented equality. */
		auxcl1->flags=ACTIVE|HORN|WEAKLYORIENTED;
		auxcl1->parent1=clause;
		auxcl1->parent2=NULL;
		auxcl1->prevdisp=NULL;
		auxcl1->part2.bin->agedist=clause->part2.bin->agedist+1;
		auxcl1->part2.bin->sinedist=clause->part2.bin->sinedist;
		auxcl1->inference=EQUALITYSPLIT;
		auxcl1->part2.bin->signature=0;
		auxcl1->part2.bin->locksignature=0;
		auxcl1->part2.bin->clause=auxcl1;
		auxcl1->part2.bin->oriented=0;
		auxcl1->part2.bin->ovly.asserts=auxcl1->part2.bin->lockasserts=NULL;
		auxcl1->part2.bin->literals=1;
		auxcl1->part2.bin->size=kk;
		ptr1=&auxcl1->part2.bin->formula[0];
		ptr1[0]=EQUALITY|SELECTED;
		((symbol *)&ptr1[1])->symbol=&equkey;
		ptr1=&ptr1[1+sizeof(symbol)];
		memcpy(ptr1,rttrm1,ii);
		ptr1[0]|=SELECTED;
		memcpy(&ptr1[ii],sbrttrm1,length1);
		ptr1[ii+length1]=UNITEND;
		UpdateParams2(&auxcl1->part2.bin->formula[0]);
		pclause[0]=auxcl1;

		/* Allocate storage for second weakly oriented equality. */
		if (NULL==(auxcl1=MYALLOC(sizeof(cmprefix)))){
			MYFREE(sbrttrm1);
			MYFREE(sbrttrm2);
			MYFREE(pclause[0]->part2.bin);
			MYFREE(pclause[0]);
			return(NOMEMORY);
		}
		kk=2+sizeof(symbol)+jj+length2;
		if (NULL==((auxcl1->part2.bin)=MYALLOC(binpsize+kk))){
			MYFREE(sbrttrm1);
			MYFREE(sbrttrm2);
			MYFREE(auxcl1);
			MYFREE(pclause[0]->part2.bin);
			MYFREE(pclause[0]);
			return(NOMEMORY);
		}

		/* Build second oriented equality. */
		auxcl1->flags=ACTIVE|HORN|WEAKLYORIENTED;
		auxcl1->parent1=clause;
		auxcl1->parent2=NULL;
		auxcl1->prevdisp=NULL;
		auxcl1->part2.bin->agedist=clause->part2.bin->agedist+1;
		auxcl1->part2.bin->sinedist=clause->part2.bin->sinedist;
		auxcl1->inference=EQUALITYSPLIT;
		auxcl1->part2.bin->signature=0;
		auxcl1->part2.bin->locksignature=0;
		auxcl1->part2.bin->clause=auxcl1;
		auxcl1->part2.bin->oriented=0;
		auxcl1->part2.bin->ovly.asserts=auxcl1->part2.bin->lockasserts=NULL;
		auxcl1->part2.bin->literals=1;
		auxcl1->part2.bin->size=kk;
		ptr1=&auxcl1->part2.bin->formula[0];
		ptr1[0]=EQUALITY|SELECTED;
		((symbol *)&ptr1[1])->symbol=&equkey;
		ptr1=&ptr1[1+sizeof(symbol)];
		memcpy(ptr1,rttrm2,jj);
		ptr1[0]|=SELECTED;
		memcpy(&ptr1[jj],sbrttrm2,length2);
		ptr1[jj+length2]=UNITEND;
		UpdateParams2(&auxcl1->part2.bin->formula[0]);
		pclause[1]=auxcl1;

		/* Allocate storage for non oriented equality. */
		if (NULL==(auxcl1=MYALLOC(sizeof(cmprefix)))){
			MYFREE(sbrttrm1);
			MYFREE(sbrttrm2);
			MYFREE(pclause[0]->part2.bin);
			MYFREE(pclause[0]);
			MYFREE(pclause[1]->part2.bin);
			MYFREE(pclause[1]);
			return(NOMEMORY);
		}
		kk=2+sizeof(symbol)+length1+length2;
		if (NULL==((auxcl1->part2.bin)=MYALLOC(binpsize+kk))){
			MYFREE(sbrttrm1);
			MYFREE(sbrttrm2);
			MYFREE(auxcl1);
			MYFREE(pclause[0]->part2.bin);
			MYFREE(pclause[0]);
			MYFREE(pclause[1]->part2.bin);
			MYFREE(pclause[1]);
			return(NOMEMORY);
		}

		/* Build non oriented equality. */
		auxcl1->flags=ACTIVE|HORN;
		auxcl1->parent1=clause;
		auxcl1->parent2=NULL;
		auxcl1->prevdisp=NULL;
		auxcl1->part2.bin->agedist=clause->part2.bin->agedist+1;
		auxcl1->part2.bin->sinedist=clause->part2.bin->sinedist;
		auxcl1->inference=EQUALITYSPLIT;
		auxcl1->part2.bin->signature=0;
		auxcl1->part2.bin->locksignature=0;
		auxcl1->part2.bin->clause=auxcl1;
		auxcl1->part2.bin->oriented=0;
		auxcl1->part2.bin->ovly.asserts=auxcl1->part2.bin->lockasserts=NULL;
		auxcl1->part2.bin->literals=1;
		auxcl1->part2.bin->size=kk;
		ptr1=&auxcl1->part2.bin->formula[0];
		ptr1[0]=EQUALITY|SELECTED;
		((symbol *)&ptr1[1])->symbol=&equkey;
		ptr1=&ptr1[1+sizeof(symbol)];
		memcpy(ptr1,sbrttrm1,length1);
		memcpy(&ptr1[length1],sbrttrm2,length2);
		ptr1[length1+length2]=UNITEND;
		UpdateParams2(&auxcl1->part2.bin->formula[0]);
		pclause[2]=auxcl1;
	}
	kbset.prstats.eqsplits++;

	/* Convert input clause to text and add it to working KB as TEXTFORMULA. */
	if (NULL==(ptr10=Decompile(clause->part2.bin))){
		for (ii=0;(ii<3)&&(pclause[ii]!=NULL);ii++){
			MYFREE(pclause[ii]->part2.bin);
			MYFREE(pclause[ii]);
		}
		return(NOMEMORY);
	}
	MYFREE(clause->part2.bin);
	clause->part2.text=ptr10;
	AddTxtFormula2KB(clause,clause->inference,&kbset);

	/* Add split clauses to working KB. */
	MYFREE(sbrttrm1);
	MYFREE(sbrttrm2);
	if ((kbset.opts.algorithm==OTTER)||(kbset.opts.algorithm==DISCOUNT)){
		for (ii=0;(ii<3)&&(pclause[ii]!=NULL);ii++){
			if (pbmtype==8){
				jj=AddBinClause2KBUEQ(pclause[ii],&kbset,SELECT);
			} else {
				jj=AddBinClause2KB(pclause[ii],&kbset,SELECT);
			}
			if (0!=jj){
				for (;(ii<3)&&(pclause[ii]!=NULL);ii++){
					MYFREE(pclause[ii]->part2.bin);
					MYFREE(pclause[ii]);
				}
				return(jj);
			}
		}
	} else {
		for (ii=0;(ii<3)&&(pclause[ii]!=NULL);ii++){
			if (0!=(jj=AddUEQClause2KB(pclause[ii]))){
				for (;(ii<3)&&(pclause[ii]!=NULL);ii++){
					MYFREE(pclause[ii]->part2.bin);
					MYFREE(pclause[ii]);
				}
				return(jj);
			}
		}
	}

	return(0);
} /* SplitNAdd2Active */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Apply a special fast substitution to a term)  OCJ
 *
 *    This function generates a new term by applying a substitution to it
 *    The substituted variables are those for which the element indexed
 *    by the variable number in the vardata set is either 1 or 2. All
 *    substituted variables are replaced by the minimal term the in term
 *    ordering.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    term: Pointer to term to which a substitution will be applied.
 *    vardata: Pointer to set indexed by variable number. Only
 *             variables for which vardata[variable_number] is 1 or 2
 *             are substituted. All substituted variables are replaced
 *             by the minimal term in term ordering.
 *    length: Length of term wit the substitution applied.

 *
 *  RETURNS:
 *
 *    Pointer to new term with substitution applied or NULL if NOMEMORY.
 *
 *
 *--------------------------------------------------------------*/
uint8_t *ApplyFastSubst(uint8_t *term,int32_t *vardata,int32_t *length){
	auto uint8_t *newterm;                                 /* Pointer to new term */
	auto uint8_t *ptr1,*ptr2;                              /* Auxiliary pointers */
	auto int32_t ii,jj;                                    /* Auxiliary */

	/* Allocate temporary memory for new term. The amount of memory allocated */
	/* is an upper bound of the memory needed. There is no need to reallocate */
	/* this to the exact length because the memory is short lived and will be */
	/* freed by the calling function. */
	ptr2=NextItem(term,OVERSUBTERMS);
	if (NULL==(newterm=MYALLOC(2+((1+sizeof(symbol))*(ptr2-term)/3)))){
		return(NULL);
	}

	/* Loop through term items and build new term. */
	for (ptr1=term,jj=0;ptr1<ptr2;ptr1=NextItem(ptr1,IMMED)){
		if (VARIABLE&ptr1[0]){
			ii=*((uint16_t *)&ptr1[1]);
			if ((vardata[ii]==1)||(vardata[ii]==2)){
				newterm[jj]=FUNCTION;
				((symbol *)&newterm[jj+1])->symbol=minterm;
				jj+=(1+sizeof(symbol));
			} else {
				memcpy(&newterm[jj],ptr1,3);
				jj+=3;
			}
		} else {
			memcpy(&newterm[jj],ptr1,1+sizeof(symbol));
			jj+=(1+sizeof(symbol));
		}
	}
	newterm[0]&=(~(MAXIMUM|SELECTED));

	/* Add a final UNITEND and update offset and nextoff fields. */
	newterm[jj]=UNITEND;
	UpdateParams2(newterm);


	/* Set term length and return. */
	*length=jj;
	return(newterm);
} /* ApplyFastSubst */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if a weak rewrite is valid)  OCJ
 *
 *    This function checks if a weak rewrite is valid according to
 *    the term rewrite description in document "Twee: An Equational
 *    Theorem Prover (System Description)" by Nicholas Smallbone.
 *    A term is built by applying a substitution to term2. If the
 *    result is a term equal to term1 then the rewrite is not valid.
 *    Otherwise it is valid.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    term1: Pointer to rewritten term.
 *    term2: Pointer to term to replace term1 after applying a
 *           substitution to it.
 *    Subst: Pointer to substitution to be applied to term2.
 *    varshift: Shift to apply to variables numbers in substitution
 *              term.
 *
 *  RETURNS:
 *
 *    0 if rewrite is OK, 1 if rewrite is not allowed or NOMEMORY.
 *
 *
 *--------------------------------------------------------------*/
int32_t CheckWeakRewrite(uint8_t *term1,uint8_t *term2,subst *Subst,int32_t varshift){
	auto uint8_t *ptr1,*ptr2;                              /* auxiliary pointers */

	/* Build term with substitution applied. */
	if (NULL==(ptr1=ApplySubst2Term(term2,Subst,varshift))){
		return(NOMEMORY);
	}

	/* If varshift is not zero then unshift variables of term in ptr1. */
	if (varshift!=0){
		for (ptr2=ptr1;ptr2[0]!=UNITEND;ptr2=NextItem(ptr2,IMMED)){
			if (VARIABLE&ptr2[0]){
				*((int16_t *)&ptr2[1])-=varshift;
			}
		}
	}

	/* Compare substituted term with term1 and return. */
	if (IsEqual(term1,ptr1,0)){
		MYFREE(ptr1);
		return(1);
	}

	/* Free memory and return. */
	MYFREE(ptr1);
	return(0);
} /* CheckWeakRewrite */
