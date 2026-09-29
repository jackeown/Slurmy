/*
 ============================================================================
 Name        : randomize.c
 Author      : Oscar Contreras
 Version     :
 Copyright   : Copyright (C) 2015-2016 Oscar Contreras. All rights reserved.
 Description : First order logic theorem prover in C
 ============================================================================
 */
/****************************************************************
*
*                 randomize (Apply randomization to a problem)
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
 *    This module include routines used to shuffle and randomize several
 *    characteristics of the current problem being solved. This is partially
 *    based on the document "Vampire Getting Noisy: Will Random Bits Help
 *    Conquer Chaos?" by Martin Suda.
 *
 *    Drodi is a multi-platform Open Source Artificial Intelligence application.
 *    Current functionality is just a first order logic knowledge base with
 *    a saturation algorithms based mainly in ordered resolution, paramodulation
 *    and factoring.
 *
 *    See Drodi.c for additional details.
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
 *  DESCRIPTION: (Shuffle symbol ids stored in  field)  OCJ
 *
 *    This function shuffles symbol ids stored in  field.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    None.
 *
 *  RETURNS:
 *
 *    0 -> OK
 *    NOMEMORY -> not enough memory
 *    -1 -> consistency check failure.
 *
 *--------------------------------------------------------------*/
int32_t ShuffleSymbols(void){
	auto hashchain **defsymbols;                        /* Pointer to set of pointers to defined symbols */
	auto hashchain **pbmsymbols;                        /* Pointer to set of pointers to problem symbols */
	auto uint32_t numdefsmbls;                          /* Number of defined symbols */
	auto uint32_t numpbmsmbls;                          /* Number of problem symbols */
	auto hashchain *ptr1;                               /* Auxiliary pointer */
	auto uint32_t ii,jj,kk;                             /* Auxiliary */

	/* Allocate memory for defined and problem symbols. */
	if (NULL==(defsymbols=MYALLOC(sizeof(hashchain *)*numsymbols))){
		printf("Not enough memory...\n");
		return(NOMEMORY);
	}
	pbmsymbols=&defsymbols[numskolem+numpdefs];

	/* Initialize defsymbols and pbmsymbols sets. */
	for (ptr1=kb_hash,numdefsmbls=numpbmsmbls=0;ptr1!=NULL;ptr1=ptr1->nextelem){
		if (ptr1->type&(SKOLEM|DEFINEDPRED)){
			defsymbols[numdefsmbls]=ptr1;
			numdefsmbls++;
		} else {
			pbmsymbols[numpbmsmbls]=ptr1;
			numpbmsmbls++;
		}
	}

	/* Consistency check. */
	if ((numdefsmbls!=(numskolem+numpdefs))||(numpbmsmbls!=(numsymbols-numdefsmbls))){
		MYFREE(defsymbols);
		printf("Consistency check failure when shuffling symbols...\n");
		return(-1);
	}

	/* Shuffle defined symbols. */
	for (ii=0;ii<numdefsmbls;ii++){
		jj=((double)rand())*numdefsmbls/((double)1.0+(double)RAND_MAX);
		kk=defsymbols[ii]->occurrence;
		defsymbols[ii]->occurrence=defsymbols[jj]->occurrence;
		defsymbols[jj]->occurrence=kk;
	}

	/* Shuffle problem symbols. */
	for (ii=0;ii<numpbmsmbls;ii++){
		jj=((double)rand())*numpbmsmbls/((double)1.0+(double)RAND_MAX);
		kk=pbmsymbols[ii]->occurrence;
		pbmsymbols[ii]->occurrence=pbmsymbols[jj]->occurrence;
		pbmsymbols[jj]->occurrence=kk;
	}

	/* Free memory and return. */
	MYFREE(defsymbols);
	return(0);
} /* ShuffleSymbols */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Restore shuffled symbol ids)  OCJ
 *
 *    This function restores shuffled symbol ids stored in precedence
 *    field to the original value before shuffling.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    None.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void UnshuffleSymbols(void){
	auto hashchain *ptr1;                               /* Auxiliary pointer */

	/* Loop through symbols. */
	for (ptr1=kb_hash;ptr1!=NULL;ptr1=ptr1->nextelem){
		ptr1->occurrence=ptr1->occurrencebk;
	}
	return;
} /* UnshuffleSymbols */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Shuffle literals and equality root terms in a clause)  OCJ
 *
 *    This function shuffles the literals in a clause. For literals
 *    that are (in)equalities it also shuffles the order of the
 *    (in)equality root terms.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    None.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t ShuffleLiterals(cmprefix *clause){
	auto int32_t *oldorder;                             /* Pointer to original order of literals indexed by new literal order */
	auto int32_t *litpos;                               /* Pointer to original literal positions */
	auto int32_t *litlength;                            /* Pointer to literal lengths */
	auto uint8_t *formula;                              /* Formula with shuffled literals */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5,*ptr6;   /* Auxiliary pointers */
	auto int ii,jj,kk,mm,rr,ss;                         /* Auxiliary */

	/* There is only one literal. */
	if (clause->part2.bin->literals==1){

		/* Return if clause is not an (in)equality). */
		if (0==(EQUALITY&clause->part2.bin->formula[0])){
			return(0);
		}

		/* Root terms must be shuffled. */
		if (rand()>=(RAND_MAX/2)){

			/* Allocate memory for new formula. */
			if (NULL==(formula=MYALLOC(clause->part2.bin->size))){
				return(NOMEMORY);
			}

			/* Swap root terms. */
			ptr3=&clause->part2.bin->formula[0];
			ptr4=NextItem(ptr3,IMMED);
			ptr5=NextItem(ptr4,OVERSUBTERMS);
			ptr6=NextItem(ptr5,OVERSUBTERMS);
			rr=1+sizeof(symbol);
			memcpy(&formula[0],ptr3,rr);
			ss=ptr6-ptr5;
			memcpy(&formula[rr],ptr5,ss);
			memcpy(&formula[rr+ss],ptr4,ptr5-ptr4);
			memcpy(ptr3,formula,ptr6-ptr3);
			UpdateParams2(&clause->part2.bin->formula[0]);
			MYFREE(formula);
		}
		return(0);
	}

	/* Allocate memory for new formula, new literal order, original */
	/* literal positions and literal lengths. */
	if (NULL==(formula=MYALLOC(clause->part2.bin->size))){
		return(NOMEMORY);
	}
	jj=clause->part2.bin->literals;
	if (NULL==(oldorder=MYALLOC(sizeof(oldorder[0])*jj))){
		MYFREE(formula);
		return(NOMEMORY);
	}
	if (NULL==(litpos=MYALLOC(sizeof(litpos[0])*jj))){
		MYFREE(formula);
		MYFREE(oldorder);
		return(NOMEMORY);
	}
	if (NULL==(litlength=MYALLOC(sizeof(litlength[0])*jj))){
		MYFREE(formula);
		MYFREE(oldorder);
		MYFREE(litpos);
		return(NOMEMORY);
	}

	/* Initialize literal positions and lengths. */
	for (ptr1=ptr2=ptr3=&clause->part2.bin->formula[0],ii=0;ptr1[0]!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){
		litpos[ii]=ptr1-ptr2;
		if (ii>0){
			litlength[ii-1]=ptr1-ptr3;
		}
		ptr3=ptr1;
	}
	litlength[ii-1]=ptr1-ptr3;

	/* Initialize and shuffle literal order. If literal j in the original */
	/* clause is now the literal i in new clause then i=oldorder[j]. */
	for (ii=0;ii<jj;ii++){
		oldorder[ii]=ii;
	}
	for (ii=0;ii<jj;ii++){
		kk=((double)rand())*jj/((double)1.0+(double)RAND_MAX);
		mm=oldorder[ii];
		oldorder[ii]=oldorder[kk];
		oldorder[kk]=mm;
	}

	/* Build new formula, copy it to clause formula buffer */
	/* and update clause parameters. */
	for (ii=kk=0;ii<jj;ii++,kk+=mm){

		/* Set auxiliary variables. */
		mm=litlength[oldorder[ii]];
		ptr3=&clause->part2.bin->formula[litpos[oldorder[ii]]];

		/* If literal is an (in)equality) then randomly swap root terms. */
		if (EQUALITY&ptr3[0]){
			if (rand()<(RAND_MAX/2)){
				memcpy(&formula[kk],ptr3,mm);
			} else {
				ptr4=NextItem(ptr3,IMMED);
				ptr5=NextItem(ptr4,OVERSUBTERMS);
				ptr6=NextItem(ptr5,OVERSUBTERMS);
				rr=1+sizeof(symbol);
				memcpy(&formula[kk],ptr3,rr);
				ss=ptr6-ptr5;
				rr+=kk;
				memcpy(&formula[rr],ptr5,ss);
				memcpy(&formula[rr+ss],ptr4,ptr5-ptr4);
			}

		/* Literal is not an (in)equality). */
		} else {
			memcpy(&formula[kk],ptr3,mm);
		}
	}
	memcpy(&clause->part2.bin->formula[0],formula,kk);
	UpdateParams2(&clause->part2.bin->formula[0]);

	/* Free memory and return. */
	MYFREE(formula);
	MYFREE(oldorder);
	MYFREE(litpos);
	MYFREE(litlength);
	return(0);
} /* ShuffleLiterals */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Select and unlink an unprocessed clause)  OCJ
 *
 *    This function selects an unprocessed clause from the UNPROC queue.
 *    If kbset.opts.shuffle is zero then the first clause in the queue
 *    is selected and unlinked. Otherwise a random clause is selected
 *    from the N first or last clauses in the queue, where:
 *      N=MIN(MAXUNPROCSHUFFLING,number of unprocessed clauses)
 *    The selected clause is unlinked and the first or last clause in
 *    the queue is put in the unlinked clause original position if it is
 *    a different clause.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    None.
 *
 *  RETURNS:
 *
 *    Selected clause or NULL if no more clauses.
 *
 *--------------------------------------------------------------*/
cmprefix *SelectUnprocClause(void){
	auto cmprefix *clause;                              /* Pointer to clause */
	auto cmprefix *prvclause,*nxtclause;                /* Pointers to previous and next clauses */
	auto cmprefix *tipclause;                           /* Pointer to first or last clause in queue */
	auto int32_t ii,jj,kk;                              /* Auxiliary */

	/* Shuffling is disabled. */
	if (kbset.opts.shuffle==0){

		/* Select and unlink clause. Unlink cannot return NOMEMORY because */
		/* unprocessed clauses are not indexed by discrimination trees. */
		clause=kbset.frstunproc;
		if (clause!=NULL){
			switch (kbset.opts.algorithm){
				case OTTER:
				case DISCOUNT:
					if (pbmtype==8){
						UnlinkUEQ(clause,&kbset);
					} else {
						Unlink(clause,&kbset);
					}
					break;
				default:
					UEQUnlink(clause);
					break;
			}
		}

	/* Shuffling is enabled. */
	} else {

		/* There are no clauses in the queue. */
		if (kbset.prstats.nbunproc==0){
			clause=NULL;

		/* There is only one clause in the queue. */
		} else if (kbset.prstats.nbunproc==1){
			clause=kbset.frstunproc;
			switch (kbset.opts.algorithm){
				case OTTER:
				case DISCOUNT:
					if (pbmtype==8){
						UnlinkUEQ(clause,&kbset);
					} else {
						Unlink(clause,&kbset);
					}
					break;
				default:
					UEQUnlink(clause);
					break;
			}

		/* There are two or more clauses in the queue. */
		} else {

			/* Get random numbers: ii to decide if the selection will be done */
			/* from the top or bottom of the queue and jj the clause number. */
			ii=rand();
			if (kbset.prstats.nbunproc<(2*MAXUNPROCSHUFFLING)){
				kk=kbset.prstats.nbunproc/2;
			} else {
				kk=MAXUNPROCSHUFFLING;
			}
			jj=((double)rand())*kk/((double)1.0+(double)RAND_MAX);

			/* Select from first clauses in queue. Unlink cannot return NOMEMORY because */
			/* unprocessed clauses are not indexed by discrimination trees. */
			prvclause=nxtclause=tipclause=NULL; /* Just to prevent compiler warnings. */
			if (ii<=(RAND_MAX/2)){
				for (clause=kbset.frstunproc,kk=0;kk<jj;clause=clause->next,kk++){
				}
				if (jj!=0){
					tipclause=kbset.frstunproc;
					kbset.frstunproc=tipclause->next;
					tipclause->next->prev=NULL;
					prvclause=clause->prev;
					nxtclause=clause->next;
				}
				switch (kbset.opts.algorithm){
					case OTTER:
					case DISCOUNT:
						if (pbmtype==8){
							UnlinkUEQ(clause,&kbset);
						} else {
							Unlink(clause,&kbset);
						}
						break;
					default:
						UEQUnlink(clause);
						break;
				}
				if (jj!=0){
					tipclause->prev=prvclause;
					tipclause->next=nxtclause;
					if (tipclause->prev==NULL){
						kbset.frstunproc=tipclause;
					}
					if (prvclause!=NULL){
						prvclause->next=tipclause;
					}
					nxtclause->prev=tipclause;
				}

			/* Select from last clauses in queue. Unlink cannot return NOMEMORY because */
			/* unprocessed clauses are not indexed by discrimination trees. */
			} else {
				for (clause=kbset.lstunproc,kk=0;kk<jj;clause=clause->prev,kk++){
				}
				if (jj!=0){
					tipclause=kbset.lstunproc;
					kbset.lstunproc=tipclause->prev;
					tipclause->prev->next=NULL;
					prvclause=clause->prev;
					nxtclause=clause->next;
				}
				switch (kbset.opts.algorithm){
					case OTTER:
					case DISCOUNT:
						if (pbmtype==8){
							UnlinkUEQ(clause,&kbset);
						} else {
							Unlink(clause,&kbset);
						}
						break;
					default:
						UEQUnlink(clause);
						break;
				}
				if (jj!=0){
					tipclause->prev=prvclause;
					tipclause->next=nxtclause;
					if (tipclause->next==NULL){
						kbset.lstunproc=tipclause;
					}
					if (nxtclause!=NULL){
						nxtclause->prev=tipclause;
					}
					prvclause->next=tipclause;
				}

			}
		}
	}
	return(clause);
} /* SelectUnprocClause */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Generate a random strategy)  OCJ
 *
 *    This function generates a random strategy and returns the
 *    strategy setting flags.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    type: 0 to generate a complete or incomplete strategy
 *          1 to generate only a complete strategy
 *
 *  RETURNS:
 *
 *    64 bit integer with strategy flags.
 *
 *--------------------------------------------------------------*/
uint64_t RandomStrategy(int32_t type){
	auto uint64_t ss;                                   /* Auxiliary */

	/* Randomly set unconditional strategies */
	/* that don't depend on the problem type. */
	ss=0;
	if (1&rand()){
		ss|=ARITY_P;
	} else {
		ss|=UNIFORM_P;
	}
	switch (rand()%7){
		case 0:
			ss|=GAMMA0_P;
			break;
		case 1:
			ss|=GAMMA1_P;
			break;
		case 2:
			ss|=GAMMA2_P;
			break;
		case 3:
			ss|=GAMMA3_P;
			break;
		case 4:
			ss|=GAMMA4_P;
			break;
		case 5:
			ss|=GAMMA5_P;
			break;
		case 6:
			ss|=GAMMA6_P;
			break;
	}
	if (1&rand()){
		ss|=LEARN_P;
	}
	if (type){
		ss|=(TRMORDSTD_P|LITORDSTD_P);
	} else {
		if (1&rand()){
			ss|=TRMORDSTD_P;
		} else {
			ss|=TRMORDNR_P;
		}
		if (1&rand()){
			ss|=LITORDSTD_P;
		} else {
			ss|=LITORDNR_P;
		}
	}
	switch (rand()%6){
		case 0:
			ss|=PRCARITY_P;
			break;
		case 1:
			ss|=PRCOCCUR_P;
			break;
		case 2:
			ss|=PRCFREQ_P;
			break;
		case 3:
			ss|=PRCINVAR_P;
			break;
		case 4:
			ss|=PRCINVOC_P;
			break;
		case 5:
			ss|=PRCINVFR_P;
			break;
	}
	if (1&rand()){
		ss|=ISPRCLOW_P;
	} else {
		ss|=ISPRCHIGH_P;
	}
	switch (rand()%7){
		case 0:
			ss|=WAR12_P;
			break;
		case 1:
			ss|=WAR11_P;
			break;
		case 2:
			ss|=WAR21_P;
			break;
		case 3:
			ss|=WAR31_P;
			break;
		case 4:
			ss|=WAR41_P;
			break;
		case 5:
			ss|=WAR51_P;
			break;
		case 6:
			ss|=WAR61_P;
			break;
	}

	/* Randomly set unconditional strategies */
	/* that depend on the problem type. */
	if (pbmtype==8){
		switch (rand()%4){
			case 0:
				ss|=OTTER_P;
				break;
			case 1:
				ss|=DISCOUNT_P;
				break;
			case 02:
				ss|=UEQOTT_P;
				break;
			case 3:
				ss|=UEQDSC_P;
				break;
		}
		switch (ss&ALGMASK){
			case UEQOTT_P:
			case UEQDSC_P:
				if (1&rand()){
					ss|=CONNECTON_P;
				} else {
					ss|=CONNECTOFF_P;
				}
				if (1&rand()){
					ss|=GRJOINON_P;
				} else {
					ss|=GRJOINOFF_P;
				}
				if (1&rand()){
					ss|=WEAKRWON_P;
				} else {
					ss|=WEAKRWOFF_P;
				}
				break;
			case OTTER_P:
			case DISCOUNT_P:
				if (type){
					#ifdef REDUNDANCYCOMPLETE
					switch (rand()%5){
						case 0:
							ss|=DEMODOFF_P;
							break;
						case 1:
							ss|=DEMODCPL1_P;
							break;
						case 2:
							ss|=DEMODCPL2_P;
							break;
						case 3:
							ss|=DEMODCPL1ORNT_P;
							break;
						case 4:
							ss|=DEMODCPL2ORNT_P;
							break;
					}
					#else
					ss|=DEMODOFF_P;
					#endif
				} else {
					switch (rand()%7){
						case 0:
							ss|=DEMODOFF_P;
							break;
						case 1:
							ss|=DEMODON_P;
							break;
						case 2:
							ss|=DEMODCPL1_P;
							break;
						case 3:
							ss|=DEMODCPL2_P;
							break;
						case 4:
							ss|=DEMODONORNT_P;
							break;
						case 5:
							ss|=DEMODCPL1ORNT_P;
							break;
						case 6:
							ss|=DEMODCPL2ORNT_P;
							break;
					}
				}
				break;
		}
		if (1&rand()){
			ss|=GOALXON_P;
		} else {
			ss|=GOALXOFF_P;
		}
	} else {
		if (1&rand()){
			ss|=OTTER_P;
		} else {
			ss|=DISCOUNT_P;
		}
		if ((pbmtype!=6)&&(pbmtype!=7)){
			if (type){
				#ifdef REDUNDANCYCOMPLETE
				switch (rand()%5){
					case 0:
						ss|=DEMODOFF_P;
						break;
					case 1:
						ss|=DEMODCPL1_P;
						break;
					case 2:
						ss|=DEMODCPL2_P;
						break;
					case 3:
						ss|=DEMODCPL1ORNT_P;
						break;
					case 4:
						ss|=DEMODCPL2ORNT_P;
						break;
				}
				#else
				ss|=DEMODOFF_P;
				#endif
			} else {
				switch (rand()%7){
					case 0:
						ss|=DEMODOFF_P;
						break;
					case 1:
						ss|=DEMODON_P;
						break;
					case 2:
						ss|=DEMODCPL1_P;
						break;
					case 3:
						ss|=DEMODCPL2_P;
						break;
					case 4:
						ss|=DEMODONORNT_P;
						break;
					case 5:
						ss|=DEMODCPL1ORNT_P;
						break;
					case 6:
						ss|=DEMODCPL2ORNT_P;
						break;
				}
			}
		}
	}
	if ((pbmtype==3)||(pbmtype==8)){
		if (1&rand()){
			ss|=LKAHEADON_P;
		} else {
			ss|=LKAHEADOFF_P;
		}
		ss|=SPLITOFF_P;
	} else {
		if (1&rand()){
			ss|=FACTON_P;
		} else {
			ss|=FACTOFF_P;
		}
		if (1&rand()){
			ss|=SPLITON_P;
		} else {
			ss|=SPLITOFF_P;
		}
		if (type){
			switch (rand()%8){
				case 0:
					ss|=SINGLENEG_P;
					break;
				case 1:
					ss|=MULTINEG_P;
					break;
				case 2:
					ss|=MAXIMAL_P;
					break;
				case 3:
					ss|=TYPE1CPL_P;
					break;
				case 4:
					ss|=TYPE2CPL_P;
					break;
				case 5:
					ss|=TYPE3CPL_P;
					break;
				case 6:
					ss|=TYPE4CPL_P;
					break;
				case 7:
					ss|=SELECTALL_P;
					break;
			}
		} else {
			switch (rand()%19){
				case 0:
					ss|=SINGLENEG_P;
					break;
				case 1:
					ss|=MULTINEG_P;
					break;
				case 2:
					ss|=MAXIMAL_P;
					break;
				case 3:
					ss|=SINGLEPOS_P;
					break;
				case 4:
					ss|=MULTIPOS_P;
					break;
				case 5:
					ss|=TYPE1_P;
					break;
				case 6:
					ss|=TYPE2_P;
					break;
				case 7:
					ss|=TYPE3_P;
					break;
				case 8:
					ss|=TYPE4_P;
					break;
				case 9:
					ss|=TYPE5_P;
					break;
				case 10:
					ss|=TYPE6_P;
					break;
				case 11:
					ss|=TYPE1CPL_P;
					break;
				case 12:
					ss|=TYPE2CPL_P;
					break;
				case 13:
					ss|=TYPE3CPL_P;
					break;
				case 14:
					ss|=TYPE4CPL_P;
					break;
				case 15:
					ss|=TYPE5CPL_P;
					break;
				case 16:
					ss|=TYPE6CPL_P;
					break;
				case 17:
					ss|=SELECTALL_P;
					break;
				case 18:
					ss|=MXSINGLENEG_P;
					break;
			}
		}
	}

	/* Randomly set conditional strategies */
	/* that don't depend on the problem type. */
	switch (ss&ALGMASK){
		case OTTER_P:
		case UEQOTT_P:
			if (1&rand()){
				ss|=LRSOTT_P;
			} else {
				ss|=LRSOFF_P;
			}
			break;
	}

	/* Randomly set conditional strategies */
	/* that depend on the problem type. */
	if ((pbmflags&(EXISTCONJ|EXISTNEGCONJ))||(0==(pbmflags&HORN))||(SPLITON_P==(ss&SPLITMASK))){
		if (1&rand()){
			ss|=LAYER1_P;
		} else {
			ss|=LAYER2_P;
		}
	}

	return(ss);
} /* RandomStrategy */
