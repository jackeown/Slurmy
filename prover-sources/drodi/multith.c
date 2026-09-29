/*
 ============================================================================
 Name        : multith.c
 Author      : Oscar Contreras
 Version     :
 Copyright   : Copyright (C) 2015-2016 Oscar Contreras. All rights reserved.
 Description : First order logic theorem prover in C
 ============================================================================
 */

/****************************************************************
*
*               multith (Multi-processing functions module)
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
 *    This source contains the multi-processing functions, saturation algorithms
 *    and other related functions of the Drodi package.
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
#define VARTYPE2 /* Indicate that global variables related with performance */
                 /* monitoring will be defined in this module. Otherwise these */
                 /* variables are only declared. VARTYPE2 must be defined only  */
                 /* in one module. */
#define PERFMONIOCTL /* Indicate that myioctl() static inline function must be included. */
#include "perfmon.h"
#include "goaldef.h"

/* Specific static functions for this module. */
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Set symbol precedence according to active strategy)  OCJ
 *
 *    This function set symbol precedence according to active strategy
 *    and the current number of the occurrence field in the symbol
 *    hashchain structure.
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
static inline void SetSymbolPrecedence(void){
	auto hashchain *ptr1;                                /* Pointer to symbol */
	auto uint64_t ii;                                    /* Auxiliary */

	/* Adjust symbol precedence according to kbset.opts.precedence */
	/* and kbset.opts.prprcsymprec. */
	ii=numskolem+numpdefs;
	minterm=NULL;
	switch (kbset.opts.precedence){
		case PRCARITY:
			if (kbset.opts.prprcsymprec==ISPRCLOW){
				for (ptr1=kb_hash;ptr1!=NULL;ptr1=ptr1->nextelem){
					if (ptr1->type&(SKOLEM|DEFINEDPRED)){
						ptr1->precedence=(ii*ptr1->arity)+ptr1->occurrence;
					} else {
						ptr1->precedence=(numsymbols*ptr1->arity)+(ii*(1+maxarity))+ptr1->occurrence;
					}
					if ((ptr1->arity==0)&&((minterm==NULL)||(ptr1->precedence<minterm->precedence))){
						minterm=ptr1;
					}
				}
			} else {
				for (ptr1=kb_hash;ptr1!=NULL;ptr1=ptr1->nextelem){
					if (ptr1->type&(SKOLEM|DEFINEDPRED)){
						ptr1->precedence=(ii*ptr1->arity)+(numsymbols*(1+maxarity))+ptr1->occurrence;
					} else {
						ptr1->precedence=(numsymbols*ptr1->arity)+ptr1->occurrence;
					}
					if ((ptr1->arity==0)&&((minterm==NULL)||(ptr1->precedence<minterm->precedence))){
						minterm=ptr1;
					}
				}
			}
			break;
		case PRCOCCUR:
			if (kbset.opts.prprcsymprec==ISPRCLOW){
				for (ptr1=kb_hash;ptr1!=NULL;ptr1=ptr1->nextelem){
					if (ptr1->type&(SKOLEM|DEFINEDPRED)){
						ptr1->precedence=ptr1->occurrence;
					} else {
						ptr1->precedence=ii+ptr1->occurrence;
					}
					if ((ptr1->arity==0)&&((minterm==NULL)||(ptr1->precedence<minterm->precedence))){
						minterm=ptr1;
					}
				}
			} else {
				for (ptr1=kb_hash;ptr1!=NULL;ptr1=ptr1->nextelem){
					if (ptr1->type&(SKOLEM|DEFINEDPRED)){
						ptr1->precedence=numsymbols+ptr1->occurrence;
					} else {
						ptr1->precedence=ptr1->occurrence;
					}
					if ((ptr1->arity==0)&&((minterm==NULL)||(ptr1->precedence<minterm->precedence))){
						minterm=ptr1;
					}
				}
			}
			break;
		case PRCFREQ:
			if (kbset.opts.prprcsymprec==ISPRCLOW){
				for (ptr1=kb_hash;ptr1!=NULL;ptr1=ptr1->nextelem){
					if (ptr1->type&(SKOLEM|DEFINEDPRED)){
						ptr1->precedence=(ii*(ptr1->usecount-1))+ptr1->occurrence;
					} else {
						ptr1->precedence=(numsymbols*(ptr1->usecount-1))+(ii*maxusecount)+ptr1->occurrence;
					}
					if ((ptr1->arity==0)&&((minterm==NULL)||(ptr1->precedence<minterm->precedence))){
						minterm=ptr1;
					}
				}
			} else {
				for (ptr1=kb_hash;ptr1!=NULL;ptr1=ptr1->nextelem){
					if (ptr1->type&(SKOLEM|DEFINEDPRED)){
						ptr1->precedence=(ii*(ptr1->usecount-1))+(numsymbols*maxusecount)+ptr1->occurrence;
					} else {
						ptr1->precedence=(numsymbols*(ptr1->usecount-1))+ptr1->occurrence;
					}
					if ((ptr1->arity==0)&&((minterm==NULL)||(ptr1->precedence<minterm->precedence))){
						minterm=ptr1;
					}
				}
			}
			break;
		case PRCINVAR:
			if (kbset.opts.prprcsymprec==ISPRCLOW){
				for (ptr1=kb_hash;ptr1!=NULL;ptr1=ptr1->nextelem){
					if (ptr1->type&(SKOLEM|DEFINEDPRED)){
						ptr1->precedence=(ii*(maxarity-ptr1->arity))+ptr1->occurrence;
					} else {
						ptr1->precedence=(numsymbols*(maxarity-ptr1->arity))+(ii*(1+maxarity))+ptr1->occurrence;
					}
					if ((ptr1->arity==0)&&((minterm==NULL)||(ptr1->precedence<minterm->precedence))){
						minterm=ptr1;
					}
				}
			} else {
				for (ptr1=kb_hash;ptr1!=NULL;ptr1=ptr1->nextelem){
					if (ptr1->type&(SKOLEM|DEFINEDPRED)){
						ptr1->precedence=(ii*(maxarity-ptr1->arity))+(numsymbols*(1+maxarity))+ptr1->occurrence;
					} else {
						ptr1->precedence=(numsymbols*(maxarity-ptr1->arity))+ptr1->occurrence;
					}
					if ((ptr1->arity==0)&&((minterm==NULL)||(ptr1->precedence<minterm->precedence))){
						minterm=ptr1;
					}
				}
			}
			break;
		case PRCINVOC:
			if (kbset.opts.prprcsymprec==ISPRCLOW){
				for (ptr1=kb_hash;ptr1!=NULL;ptr1=ptr1->nextelem){
					if (ptr1->type&(SKOLEM|DEFINEDPRED)){
						ptr1->precedence=ii-ptr1->occurrence+1;
					} else {
						ptr1->precedence=ii+numsymbols-ptr1->occurrence+1;
					}
					if ((ptr1->arity==0)&&((minterm==NULL)||(ptr1->precedence<minterm->precedence))){
						minterm=ptr1;
					}
				}
			} else {
				for (ptr1=kb_hash;ptr1!=NULL;ptr1=ptr1->nextelem){
					if (ptr1->type&(SKOLEM|DEFINEDPRED)){
						ptr1->precedence=numsymbols+ii-ptr1->occurrence+1;
					} else {
						ptr1->precedence=numsymbols-ptr1->occurrence+1;
					}
					if ((ptr1->arity==0)&&((minterm==NULL)||(ptr1->precedence<minterm->precedence))){
						minterm=ptr1;
					}
				}
			}
			break;
		case PRCINVFR:
			if (kbset.opts.prprcsymprec==ISPRCLOW){
				for (ptr1=kb_hash;ptr1!=NULL;ptr1=ptr1->nextelem){
					if (ptr1->type&(SKOLEM|DEFINEDPRED)){
						ptr1->precedence=(ii*(maxusecount-ptr1->usecount))+ptr1->occurrence;
					} else {
						ptr1->precedence=(numsymbols*(maxusecount-ptr1->usecount))+(ii*maxusecount)+ptr1->occurrence;
					}
					if ((ptr1->arity==0)&&((minterm==NULL)||(ptr1->precedence<minterm->precedence))){
						minterm=ptr1;
					}
				}
			} else {
				for (ptr1=kb_hash;ptr1!=NULL;ptr1=ptr1->nextelem){
					if (ptr1->type&(SKOLEM|DEFINEDPRED)){
						ptr1->precedence=(ii*(maxusecount-ptr1->usecount))+(numsymbols*maxusecount)+ptr1->occurrence;
					} else {
						ptr1->precedence=(numsymbols*(maxusecount-ptr1->usecount))+ptr1->occurrence;
					}
					if ((ptr1->arity==0)&&((minterm==NULL)||(ptr1->precedence<minterm->precedence))){
						minterm=ptr1;
					}
				}
			}
			break;
	}
	return;
} /* SetSymbolPrecedence */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Select strategy to use in next function cycle)  OCJ
 *
 *    This function selects the strategy to use in the next InferenceProcess()
 *    function cycle.
 *
 *    If SATMODE option is set to OFF the selected strategy is taken from the
 *    standard portfolio if possible, otherwise a random strategy will be used
 *    with enhanced time. The random strategy will be complete or incomplete
 *    depending on the random choice.
 *
 *    If SATMODE option is set to MED there will be processes that will select
 *    the strategy in a similar way as if SATMODE option is set to off. There
 *    will be also other processes that use only complete strategies with
 *    enhanced time and if no more complete portfolio strategies are available
 *    then a random complete strategy will be used .
 *
 *    If SATMODE option is set to ON the selected strategy is taken from standard
 *    portfolio complete strategies to be used with enhanced time if possible,
 *    otherwise a random complete strategy to be used with enhanced time.
 *
 *    The time or HW instruction limit is not set by this function. It is set
 *    by SetInstrLimit() function.
 *
 *
 *  ARGUMENTS:
 *
 *    numstrts: Number of strategies in portfolio.
 *    allstrts: Number of strategies in portfolio plus complete strategies
 *              duplicated.
 *    pstratnb: Pointer to selected strategy position in the portfolio if
 *              it is possible to take a portfolio strategy.
 *    pstratflgs: Pointer to selected strategy flags.
 *
 *  RETURNS:
 *
 *    This function returns the return code from the local call to
 *    SetMTParams() function: 0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
static inline int32_t SelectStrategy(int32_t numstrts,int32_t allstrts,int32_t *pstratnb,
		uint64_t *pstratflgs){
	auto int32_t typeindex;                              /* Problem type index */
	auto int32_t ii;                                     /* Auxiliary */
	auto uint64_t kk,qq;                                 /* Auxiliary */

	/* Set typeindex. Problem type 8 has to sets of strategies in pbmtypestrats[][] */
	/* depending on the type of goal (ground or non ground). */
	if ((pbmtype==8)&&(ueqgoal->part2.bin->maxvarnb>=0)){
		typeindex=9;
	} else {
		typeindex=pbmtype;
	}

	/* There is no user specified strategy. */
	if ((usrparam_mask==0)&&(strategy==0xffffffffffffffff)){

		/* SATMODE option is set to OFF. */
		if (glblopts.satmode==0){

			/* Set strategy. */
			sem_wait(&procctl->strategy);
			*pstratnb=*nextstrat;
			(*nextstrat)++;
			sem_post(&procctl->strategy);
			if (pstratnb[0]>=allstrts){
				pstratflgs[0]=RandomStrategy(0);
			} else {
				pstratflgs[0]=pbmtypestrats[typeindex][*pstratnb];
			}

		/* SATMODE option is set to MED. */
		} else if (glblopts.satmode==2){

			/* This is a process that searches for unsatisfiability and uses */
			/* both complete and incomplete strategies. */
			if (procnb<((int)(0.5+(active_cores*(1.0-SATCORES))))){

				/* Get next portfolio strategy number and account for traversed */
				/* complete strategies. Complete strategies already used by processes */
				/* searching for satisfiability are not used here. */
				sem_wait(&procctl->strategy);
				if (nextstrat[0]<numstrts){
					for (ii=0;ii==0;){
						*pstratnb=*nextstrat;
						if (pstratnb[0]<numstrts){
							(*nextstrat)++;
							kk=pbmtypestrats[typeindex][*pstratnb]&SELECTMASK;
							qq=pbmtypestrats[typeindex][*pstratnb]&DEMODMASK;
							#ifdef REDUNDANCYCOMPLETE
							if (((kk==SINGLENEG_P)||(kk==MULTINEG_P)||(kk==MAXIMAL_P)||(kk==TYPE1CPL_P)||(kk==TYPE2CPL_P)
									||(kk==TYPE3CPL_P)||(kk==TYPE4CPL_P)||(kk==TYPE5CPL_P)||(kk==TYPE6CPL_P))
									&&((pbmtypestrats[typeindex][*pstratnb]&TERMORDRMASK)==TRMORDSTD_P)
									&&((pbmtypestrats[typeindex][*pstratnb]&LITORDRMASK)==LITORDSTD_P)
									&&((0==((UNITEQUALITY|NONUNITEQUALITY)&pbmflags))||(qq==DEMODOFF_P)
											||(qq==DEMODCPL1_P)||(qq==DEMODCPL2_P)
											||(qq==DEMODCPL1ORNT_P)||(qq==DEMODCPL2ORNT_P))){
								(*trvunsstr)++;
								if (trvunsstr[0]>trvsatstr[0]){
									ii=1;
								}
							} else {
								ii=1;
							}
							#else
							if (((kk==SINGLENEG_P)||(kk==MULTINEG_P)||(kk==MAXIMAL_P)||(kk==TYPE1CPL_P)||(kk==TYPE2CPL_P)
									||(kk==TYPE3CPL_P)||(kk==TYPE4CPL_P)||(kk==TYPE5CPL_P)||(kk==TYPE6CPL_P))
									&&((pbmtypestrats[typeindex][*pstratnb]&TERMORDRMASK)==TRMORDSTD_P)
									&&((pbmtypestrats[typeindex][*pstratnb]&LITORDRMASK)==LITORDSTD_P)
									&&((0==((UNITEQUALITY|NONUNITEQUALITY)&pbmflags))||(qq==DEMODOFF_P))){
								trvunsstr[0]++;
								if (trvunsstr[0]>trvsatstr[0]){
									ii=1;
								}
							} else {
								ii=1;
							}
							#endif
						} else {
							ii=1;
						}
					}
				} else {
					*pstratnb=numstrts;
				}
				sem_post(&procctl->strategy);

				/* No more strategies available in the portfolio. Use a random strategy */
				/* regardless its completeness. */
				if (*pstratnb>=numstrts){
					pstratflgs[0]=RandomStrategy(0);

				/* There are strategies available in the portfolio. */
				} else {
					pstratflgs[0]=pbmtypestrats[typeindex][*pstratnb];
				}

			/* This is a process that searches for satisfiability and uses */
			/* only complete strategies. */
			} else {

				/* Get next portfolio complete strategy number and account for traversed */
				/* complete strategies. */
				sem_wait(&procctl->strategy);
				*pstratnb=trvsatstr[0]+numstrts;
				(*trvsatstr)++;
				sem_post(&procctl->strategy);

				/* No more strategies available in the portfolio. Use a random complete strategy. */
				if (*pstratnb>=allstrts){
					pstratflgs[0]=RandomStrategy(1);

				/* There are strategies available in the portfolio. */
				} else {
					pstratflgs[0]=pbmtypestrats[typeindex][*pstratnb];
				}
			}

		/* SATMODE option is set to ON. */
		} else {

			/* Get next portfolio complete strategy number and account for traversed */
			/* complete strategies. */
			sem_wait(&procctl->strategy);
			*pstratnb=trvsatstr[0]+numstrts;
			(*trvsatstr)++;
			sem_post(&procctl->strategy);

			/* No more strategies available in the portfolio. Use a random complete strategy. */
			if (*pstratnb>=allstrts){
				pstratflgs[0]=RandomStrategy(1);

			/* There are strategies available in the portfolio. */
			} else {
				pstratflgs[0]=pbmtypestrats[typeindex][*pstratnb];
			}
		}

	/* There is an user specified strategy. */
	} else {
		if (strategy!=0xffffffffffffffff){
			pstratflgs[0]=strategy;
		} else {
			pstratflgs[0]=0; /* Just to prevent Valgrind displaying an error. */
		}
	}

	/* Set strategy parameters and return. */
	return(SetMTParams(*pstratflgs));
} /* SelectStrategy */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Set instrlimit global variable for InferenceProcess() function)  OCJ
 *
 *    This function sets the instrlimit global variable to use in the next
 *    InferenceProcess() function cycle. See SelectStrategy() function for
 *    more information about the criteria used to select the HW instruction
 *    limit and/or the time limit.
 *
 *
 *  ARGUMENTS:
 *
 *    stratnb: Selected strategy position in the portfolio.
 *    numstrts: Number of strategies in portfolio.
 *    allstrts: Number of strategies in portfolio with complete strategies
 *              duplicated.
 *    instrrate: Estimated HW instructions rate.
 *    tmfactor: Factor to be applied to number of instructions limit.
 *    flag: This parameter is only used if SATMODE option is set to OFF
 *          or MED and stratnb is 0. In this case:
 *          - If flag is 0 then instruction limit instrlimit will be taken
 *            from pbmtypeinstr[][].
 *          - If flag is 1 the instruction limit will be computed to
 *            use half of the remaining time available.
 *          - Otherwise there won't be instruction limit and the process
 *            will use all the remaining time available.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
static inline void SetInstrLimit(int32_t stratnb,int32_t numstrts,int32_t allstrts,
		uint64_t instrrate,float tmfactor,int32_t flag){
	auto struct itimerval alarmtime;                     /* To get remaining time in time alarm */
	auto int32_t typeindex;                              /* Problem type index */
	auto float ff;                                       /* Auxiliary */
	auto int32_t jj;                                     /* Auxiliary */
	auto uint64_t ii;                                    /* Auxiliary */

	/* Set typeindex. Problem type 8 has to sets of strategies in pbmtypeinstr[][] */
	/* depending on the type of goal (ground or non ground). */
	if ((pbmtype==8)&&(ueqgoal->part2.bin->maxvarnb>=0)){
		typeindex=9;
	} else {
		typeindex=pbmtype;
	}

	/* Compute the limit of hardware instructions to execute. */
	/* If there is a user defined strategy the limit is time, not number of instructions. */
	if ((usrparam_mask!=0)||(strategy!=0xffffffffffffffff)){
		instrlimit=0x7fffffffffffffff;

	/* There is not a user defined strategy. */
	} else {

		/* SATMODE option is set to OFF. */
		if (glblopts.satmode==0){

			/* If there is a standard portfolio strategy available then the limit */
			/* is number of instructions assigned to each portfolio strategy */
			/* or a function of available time depending on the value of flag */
			/* (see global comments for details). */
			if (stratnb<numstrts){
				if (flag==0){
					instrlimit=pbmtypeinstr[typeindex][stratnb]*tmfactor;
				} else if (stratnb!=0){
					instrlimit=pbmtypeinstr[typeindex][stratnb]*tmfactor;
				} else if (flag==1){
					getitimer(ITIMER_REAL,&alarmtime);
					ff=alarmtime.it_value.tv_sec+(alarmtime.it_value.tv_usec/1000000);
					instrlimit=0.5+(ff*HWINSTRATE/2.0);
				} else {
					instrlimit=0x7fffffffffffffff;
				}
				jj=0;

			/* When portfolio strategies have been already used then complete portfolio */
			/* strategies are used with an enhanced instruction limit. */
			} else if (stratnb<allstrts){
				if (instrrate==0){
					instrlimit=CPLINSTRLIMIT;
					jj=0;
				} else {
					getitimer(ITIMER_REAL,&alarmtime);
					ff=alarmtime.it_value.tv_sec+(alarmtime.it_value.tv_usec/1000000);
					if (CPLINSTRLIMIT<=ff){
						instrlimit=CPLINSTRLIMIT;
						jj=0;
					} else {
						jj=1;
					}
				}

			/* There are not portfolio strategies available.Use a random strategy */
			/* and set an instruction limit according to time available. */
			} else {
				getitimer(ITIMER_REAL,&alarmtime);
				ff=alarmtime.it_value.tv_sec+(alarmtime.it_value.tv_usec/1000000);
				jj=1;
			}
			if (jj){
				if (ff>=(2*DFLTSTRATTIMEOUT)){
					ff=DFLTSTRATTIMEOUT;
				}
				instrlimit=0.5+(ff*instrrate);
			}

		/* SATMODE option is set to MED or ON. */
		} else {

			/* This is a process that searches for unsatisfiability and uses */
			/* both complete and incomplete strategies. */
			if ((glblopts.satmode==2)&&(procnb<((int)(0.5+(active_cores*(1.0-SATCORES)))))){

				/* If there is a standard portfolio strategy available then the limit */
				/* is number of instructions assigned to each portfolio strategy */
				/* or a function of available time depending on the value of flag */
				/* (see global comments for details). */
				if (stratnb<numstrts){
					if (flag==0){
						instrlimit=pbmtypeinstr[typeindex][stratnb]*tmfactor;
					} else if (stratnb!=0){
						instrlimit=pbmtypeinstr[typeindex][stratnb]*tmfactor;
					} else if (flag==1){
						getitimer(ITIMER_REAL,&alarmtime);
						ff=alarmtime.it_value.tv_sec+(alarmtime.it_value.tv_usec/1000000);
						instrlimit=0.5+(ff*HWINSTRATE/2.0);
					} else {
						instrlimit=0x7fffffffffffffff;
					}

				/* There are not portfolio strategies available and a random strategy */
				/* will be used. Set an instruction limit according to time available. */
				} else {
					getitimer(ITIMER_REAL,&alarmtime);
					ff=alarmtime.it_value.tv_sec+(alarmtime.it_value.tv_usec/1000000);
					if (ff>=(2*DFLTSTRATTIMEOUT)){
						ff=DFLTSTRATTIMEOUT;
					}
					instrlimit=0.5+(ff*instrrate);
				}

			/* This is a process that searches for satisfiability and uses */
			/* only complete strategies. */
			} else {

				/* Set HW instructions rate to be used in ii. */
				if (instrrate==0){
					ii=HWINSTRATE;
				} else {
					ii=instrrate;
				}

				/* There is a standard portfolio strategy available. */
				/* Set an instruction limit according to SATSTRATTIMEOUT time. */
				if (stratnb<(allstrts-numstrts)){
					instrlimit=0.5+(SATSTRATTIMEOUT*ii);

				/* There are not portfolio strategies available and a random strategy */
				/* will be used. Set an instruction limit according to time available. */
				} else {
					getitimer(ITIMER_REAL,&alarmtime);
					ff=alarmtime.it_value.tv_sec+(alarmtime.it_value.tv_usec/1000000);
					if (ff>=(2*DFLTSTRATTIMEOUT)){
						ff/=2.0;
					}
					instrlimit=0.5+(ff*ii);
				}
			}
		}
	}
	return;
} /* SetInstrLimit */

/* Global variables for this module: only those that need initialization in their definition. */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Start saturation algorithms)  OCJ
 *
 *    This function perform the following tasks:
 *    - Converts the input formulas to conjunctive normal form, compiles
 *      the produced clauses and adds them to the main KB.
 *    - Makes copies of the resulting KB.
 *    - Start a saturation algorithm with each KB. Each algorithm is run
 *      in a separate process and there are as many processes as available
 *      cores.
 *    - Prints the resulting output.
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
void Saturate(void){
	auto uint64_t avmemory;                          /* Physical memory available */
	auto clock_t start;                              /* CPU ticks at start of child process */
	auto struct timespec starttime;                  /* To specify wait time for semaphores */
	auto struct timespec endtime;                    /* To specify wait time for semaphores */
	auto int32_t timeleft;                           /* Time left after a proof was found */
	auto struct itimerval alarmtime;                 /* To define a global timeout alarm */
	auto struct mallinfo2 rsinfo;                    /* For mallinfo2() calls */
	auto uint64_t allcdmem;                          /* For maxmemory initialization */
	auto int orgcores;                               /* Original value of active_cores */
	auto char *ptr1,*ptr2;                           /* Auxiliary pointers */
	auto float dd;                                   /* Auxiliary */
	auto double ff;                                  /* Auxiliary */
	auto int32_t ii,jj,kk;                           /* Auxiliary */

	#ifndef DEBUGTREE
	#ifdef DEBUGTREE2
	/* Discrimination trees query check. */
	if (NULL==mainkb.frstunproc){
		CheckDscTQueries();
		return;
	}
	#endif
	#endif

	/* Start CPU time measurement and initialize maximum size in bytes */
	/* of formulas added to discrimination trees. */
	start=clock();
	gettimeofday(&mainkb.time,NULL);
	mxtrfsize=0;

	/* Main KB is empty. */
	if (mainkb.nformulas==0){
		if (pgmmode==NONINTERACTIVE){
			printf("%% SZS status InputError : ");
		}
		printf("There are no input formulas to process.\n");
		return;
	}

	/* Initialize problem used memory and perform pre-processing. */
	ResetStats(pbmstats);
	procctl->solvekb=-1;
	procctl->prctype=1;
	ii=Preprocess(1);
	if (ii){
		if (ii==NOMEMORY){
			if (pgmmode==NONINTERACTIVE){
				printf("%% SZS status MemoryOut : ");
			}
			printf("Not enough memory.\n");
		} else {
			if (pgmmode==NONINTERACTIVE){
				printf("%% SZS status Timeout : ");
			}
			printf("Process timeout\n");
		}
		FreeInfFormulas();
		if ((NOMEMORY==Initialize_KB(&mainkb))&&(pgmmode==INTERACTIVE)){
			printf("No memory available, KB not initialized\n");
		}
		ResetStats(pbmstats);
		procctl->status=STOPPED;
		tot_pbms++;
		procctl->end=clock();
		tot_cpu_time+=((double)(procctl->end-procctl->start))/CLOCKS_PER_SEC;
		return;
	}

	/* Start time measurement after preprocessing. */
	gettimeofday(&mainkb.time,NULL);

	/* Initialize common semaphores. They are initialized in any case because otherwise each */
	/* time a resource in common memory is accessed an additional test would be necessary. */
	sem_init(&procctl->procctl,1,1);
	sem_init(&procctl->statistics,1,1);
	sem_init(&procctl->strategy,1,1);

	/* Problem was not solved in the pre-processing. */
	*secprc_cpu_time=0;
	orgcores=active_cores;
	satcalltype=1; /* Call to SatSolver is not from GoSat(). */
	if (procctl->status==STOPPED){

		/* Set next strategy to first strategy and set traversed complete strategies to zero. */
		/* Initialize prednumber field of equkey needed for subsumption. Also read learning */
		/* if applicable. */
		*nextstrat=0;
		*trvsatstr=0;
		*trvunsstr=0;
		equkey.prednumber=numpreds;
		if (pbmflags&(EXISTCONJ|EXISTNEGCONJ)){
			ReadLearnRecords();
		}

		/* Single process case. */
		procctl->status=RUNNING;
		if ((active_cores==1)||(usrparam_mask!=0)||(strategy!=0xffffffffffffffff)){

			/* Initialize maximum process memory and start inference process. */
			avmemory=GetAvailableMemory();
			rsinfo=mallinfo2();
			allcdmem=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
			if (avmemory>=MEMORYLIMIT){
				maxmemory=allcdmem+avmemory;
			} else {
				procctl->status=NOMEMORY;
				maxmemory=allcdmem;
			}
			if (maxmemory>memorylimit){
				maxmemory=memorylimit;
			}
			procnb=0;
			if (procctl->status==RUNNING){
				if (pbmtype==8){
					InferenceProcessUEQ();
				} else {
					InferenceProcess();
				}
			}

		/* Multi-process case. */
		} else {

			/* Ensure that there are not pending output so that it is not printed */
			/* several times by the children processes. */
			fflush(stdout);

			/* Initialize maximum process memory and and temporarily reduce */
			/* the number of cores if necessary. */
			avmemory=GetAvailableMemory();
			rsinfo=mallinfo2();
			allcdmem=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
			if (avmemory>=MEMORYLIMIT){
				if (allcdmem>0){
					ff=((double)(allcdmem+avmemory))/(2*allcdmem);
					if (ff<orgcores){
						if (ff<1){
							active_cores=1;
						} else {
							active_cores=ff;
						}
					}
				}
				allcdmem+=avmemory;
				if (allcdmem>memorylimit){
					allcdmem=memorylimit;
				}
				*maxprcmemory=allcdmem/active_cores;
			} else {
				procctl->status=NOMEMORY;
				*maxprcmemory=allcdmem;
			}

			/* Initialize process specific semaphores and spawn as many processes */
			/* as number of enabled cores minus one (parent process counts). */
			/* The child process cpu time is set to zero. */
			jj=active_cores-1;
			switch (glblopts.satmode){
				case 0:
				default:
					satcores=0;
					break;
				case 1:
					satcores=active_cores;
					break;
				case 2:
					satcores=0.5+(active_cores*SATCORES);
					break;
			}
			for (procnb=0;procnb<jj;procnb++){
				sem_init(&procsem[procnb],1,0);
				if (0==(pids[procnb]=fork())){
					cpu_time=0;
					break;
				} else if (pids[procnb]==-1){
					procctl->status=NOMEMORY;
					break;
				}
			}

			/* Child process. Start measuring cpu time and set time alarm */
			/* for timeout detection. */
			if (procnb!=jj){
				start=clock();
				alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
				ff=glblopts.timeout-mainkb.prstats.elapsed_time;
				alarmtime.it_value.tv_sec=(int32_t)ff;
				alarmtime.it_value.tv_usec=(int32_t)((ff-(int32_t)ff)*1000000.0);
				alrmstatus=0;
				setitimer(ITIMER_REAL,&alarmtime,NULL);
			}

			/* Start inference process. */
			if (procctl->status==RUNNING){
				maxmemory=*maxprcmemory;
				if (pbmtype==8){
					InferenceProcessUEQ();
				} else {
					InferenceProcess();
				}
			}

			/* If this is the process that solved the problem then indicate */
			/* that proof building and printing is about to start. */
			if (procctl->solvekb==procnb){
				procctl->prfprntinproc=1;
			}
		}

		/* Free learning records memory. */
		if (lrnrecords!=NULL){
			for (jj=0;jj<numlrnrecs;jj++){
				MYFREE(lrnrecords[jj].vector.index);
			}
			MYFREE(lrnrecords);
			lrnrecords=NULL;
		}

	/* Problem was solved in the pre-processing. */
	} else {
		procnb=(active_cores-1);
		pids[0]=0; /* Indicate that there are no children processes now. */
	}

	/* Disable time alarm. */
	alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
	alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
	setitimer(ITIMER_REAL,&alarmtime,NULL);

	/* Print problem output if appropriate. */
	if ((procctl->solvekb==procnb)||(usrparam_mask!=0)||(strategy!=0xffffffffffffffff)
			||((procnb==(active_cores-1))&&((procctl->solvekb<0)||(procctl->solvekb==active_cores)))){

		/* If verbose is active then get semaphore to prevent mixing output */
		/* with output from InferenceProcess() function. */
		if (verbose){
			sem_wait(&procctl->statistics);
		}

		/* Display KB data.*/
		if (prtkbdata){
			PrintKBData();
		}

		/* Display statistics. The PrintStats() function initializes the working KB's. */
		PrintStats(0);

		/* If verbose is active then release semaphore. */
		if (verbose){
			sem_post(&procctl->statistics);
		}
	}

	/* Free feature vector index memory used by GetClauseVector(). */
	MYFREE(fvidx);
	fvidx=NULL;

	/* Collect memory use information for the problem. */
	sem_wait(&procctl->statistics);
	pbmstats->memory+=mainkb.prstats.memory;
	pbmstats->netmemory+=mainkb.prstats.netmemory;
	sem_post(&procctl->statistics);

	/* This is a child process. */
	if ((procnb!=(active_cores-1))&&(usrparam_mask==0)&&(strategy==0xffffffffffffffff)){

		/* Stop and accumulate child process cpu time measurement. */
		procctl->end=clock();
		cpu_time=((double)(clock()-start))/CLOCKS_PER_SEC;
		sem_wait(&procctl->statistics);
		*secprc_cpu_time+=cpu_time;
		sem_post(&procctl->statistics);

		/* Clean for exit and exit. */
		Clean4Exit();
		exit(0);
	}

	/* IF WE ARE HERE WE ARE THE PARENT PROCESS. */
	/* If this execution was multi-pocess then wait with time limit to have a consolidated */
	/* information from all child processes. */
	if ((active_cores>1)&&(usrparam_mask==0)&&(strategy==0xffffffffffffffff)&&(procctl->solvekb!=active_cores)){
		clock_gettime(CLOCK_REALTIME,&starttime);
		gettimeofday(&mainkb.endtime,NULL);
		timeleft=(int32_t)(EXITWAITSECONDS+glblopts.timeout-mainkb.prstats.elapsed_time
				-mainkb.endtime.tv_sec+mainkb.time.tv_sec);
		jj=active_cores-1;
		for (ii=0;ii<jj;ii++){
			endtime=starttime;
			if ((procctl->solvekb==ii)&&(procctl->prfprntinproc)){
				endtime.tv_sec+=timeleft;
			} else {
				endtime.tv_sec+=EXITWAITSECONDS;
			}
			sem_timedwait(&procsem[ii],&endtime);
		}
	}

	/* Stop and accumulate time measurement. */
	procctl->end=clock();
	cpu_time+=*secprc_cpu_time+(((double)(procctl->end-start))/CLOCKS_PER_SEC);
	tot_cpu_time+=cpu_time;
	gettimeofday(&mainkb.endtime,NULL);
	mainkb.prstats.elapsed_time+=(mainkb.endtime.tv_sec-mainkb.time.tv_sec
			+(mainkb.endtime.tv_usec-mainkb.time.tv_usec)/1000000.0);
	pbmstats->elapsed_time=mainkb.prstats.elapsed_time;

	/* Print theorem total statistics. */
	if (pgmmode==NONINTERACTIVE){
		ptr2="% ";
	} else {
		ptr2="";
	}
	if (verbose){
		printf("%s\n%sTheorem statistics:\n",ptr2,ptr2);
		printf("%sNumber of processes %d:\n",ptr2,active_cores);
	}
	PrintStatData(pbmstats,0);
	if (cpuavail){
		printf("%sCPU time: %.6f seconds\n",ptr2,cpu_time);
	}

	/* Print theorem memory use information. */
	dd=FormatMemory(pbmstats->memory,&ptr1);
	printf("%sTotal memory used: %.3f %s\n",ptr2,dd,ptr1);
	dd=FormatMemory(pbmstats->netmemory,&ptr1);
	printf("%sNet memory used: %.3f %s\n",ptr2,dd,ptr1);

	/* Accumulate global statistics. After the PrintStats(0) call */
	/* the main KB has the current problem accumulated statistics. */
	tot_pbms++;
	if ((procctl->status==SATISFIABLE)||(procctl->status==UNSATISFIABLE)){
		solved_pbms++;
	}
	AddInferenceStats(pbmstats,&glblstats);
	glblstats.elapsed_time+=mainkb.prstats.elapsed_time;
	if (glblstats.memory<pbmstats->memory){
		glblstats.memory=pbmstats->memory;
		glblstats.netmemory=pbmstats->netmemory;
	}

	/* Memory problems DEBUG. */
	#ifdef DEBUGMEMORY
	for (ii=0;ii<ndemods;ii++){
		if (memtrack[ii]==1){
			printf("Memory allocation number %d not freed.\n",ii);
		}
	}
	printf("Number of memory allocation calls: %d\n",ndemods);
	#endif

	/* Initialize main KB. */
	FreeInfFormulas();
	if (NOMEMORY==Initialize_KB(&mainkb)){
		printf("No memory available, KB not initialized\n");
	}
	ResetStats(&mainkb.prstats);
	procctl->status=STOPPED;

	/* If this execution was multi-pocess then wait for child processes */
	/* termination and destroy child specific semaphore. */
	if ((active_cores>1)&&(usrparam_mask==0)&&(strategy==0xffffffffffffffff)&&(procctl->solvekb!=active_cores)){
		endtime.tv_sec=0;
		endtime.tv_nsec=10000000;
		jj=active_cores-1;
		for (ii=kk=0;ii<jj;ii++){
			if (kk>KILLWAITITERATIONS){
				if (0==waitpid(pids[ii],NULL,WNOHANG)){
					kill(pids[ii],SIGTERM);
					waitpid(pids[ii],NULL,WNOHANG);
				}
			} else {
				for (;kk<=KILLWAITITERATIONS;kk++){
					if (0!=waitpid(pids[ii],NULL,WNOHANG)){
						break;
					}
					nanosleep(&endtime,NULL);
				}
				if (kk>KILLWAITITERATIONS){
					kill(pids[ii],SIGTERM);
					waitpid(pids[ii],NULL,WNOHANG);
				}
			}
			sem_destroy(&procsem[ii]);
		}
		pids[0]=0; /* Indicate that there are no children processes now. */
	}

	/* Destroy common semaphores and return and restore */
	/* original number of active cores. */
	sem_destroy(&procctl->procctl);
	sem_destroy(&procctl->statistics);
	sem_destroy(&procctl->strategy);
	active_cores=orgcores;
	return;
} /* Saturate */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Start saturation algorithms for strategy tuning)  OCJ
 *
 *    This function is similar to Saturate() function except for the
 *    following:
 *    - A user defined strategy must be specified for this process to run.
 *    - The saturation process is aimed to build the strategy portfolio and
 *      InferenceProcess2() function is called instead InferenceProcess()
 *    - Statistics are not reported.
 *    - Proofs are not reported. Each process may solve the problem so
 *      several solutions may be produced.
 *    - Each process that solves the problem prints information relevant
 *      for the portfolio build process.
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
void Saturate2(void){
	auto uint64_t avmemory;                          /* Physical memory available */
	auto struct timespec waittime1,waittime2;        /* To specify wait time for semaphores */
	auto struct itimerval alarmtime;                 /* To define a global timeout alarm */
	auto struct mallinfo2 rsinfo;                    /* For mallinfo2() calls */
	auto int orgcores;                               /* Original value of active_cores */
	auto uint64_t allcdmem;                          /* For maxmemory initialization */
	auto double ff;                                  /* Auxiliary */
	auto int32_t ii,jj;                              /* Auxiliary */

	/* Start time measurement and initialize maximum size in bytes */
	/* of formulas added to discrimination trees. */
	gettimeofday(&mainkb.time,NULL);
	mxtrfsize=0;

	/* Return if no instruction limit has been specified. */
	if (instrlimit==0xffffffffffffffff){
		printf("HWINSTR parameter has not been specified.\n");
		return;
	}

	/* Return if main KB is empty or there is no user defined strategy. */
	if (mainkb.nformulas==0){
		if (pgmmode==NONINTERACTIVE){
			printf("%% SZS status InputError : ");
		}
		printf("There are no input formulas to process.\n");
		return;
	}
	if (strategy==0xffffffffffffffff){
		if (pgmmode==NONINTERACTIVE){
			printf("%% SZS status InputError : ");
		}
		printf("No user strategy specified with the STRATEGY command/option.\n");
		return;
	}

	/* Initialize problem used memory and perform pre-processing. */
	procctl->prctype=0;
	ii=Preprocess(1);
	if (ii){
		if (ii==NOMEMORY){
			if (pgmmode==NONINTERACTIVE){
				printf("%% SZS status MemoryOut : ");
			}
			printf("Not enough memory.\n");
		} else {
			if (pgmmode==NONINTERACTIVE){
				printf("%% SZS status Timeout : ");
			}
			printf("Process timeout\n");
		}
		FreeInfFormulas();
		if ((NOMEMORY==Initialize_KB(&mainkb))&&(pgmmode==INTERACTIVE)){
			printf("No memory available, KB not initialized\n");
		}
		procctl->status=STOPPED;
		tot_pbms++;
		return;
	}

	/* Start time measurement after preprocessing. */
	gettimeofday(&mainkb.time,NULL);

	/* Initialize common semaphores. They are initialized in any case because otherwise each */
	/* time a resource in common memory is accessed an additional test would be necessary. */
	sem_init(&procctl->procctl,1,1);
	sem_init(&procctl->statistics,1,1);
	sem_init(&procctl->strategy,1,1);

	/* Problem was not solved in the pre-processing. */
	orgcores=active_cores;
	satcalltype=1; /* Call to SatSolver is not from GoSat(). */
	if (procctl->status==STOPPED){

		/* Initialize prednumber field of equkey needed for subsumption. Also read learning */
		/* if applicable. */
		equkey.prednumber=numpreds;
		if ((strategy&LEARN_P)&&(pbmflags&(EXISTCONJ|EXISTNEGCONJ))){
			ReadLearnRecords();
		}

		/* Single process case. */
		procctl->status=RUNNING;
		if (active_cores==1){

			/* Initialize maximum process memory and start inference process. */
			avmemory=GetAvailableMemory();
			rsinfo=mallinfo2();
			allcdmem=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
			if (avmemory>=MEMORYLIMIT){
				maxmemory=allcdmem+avmemory;
			} else {
				procctl->status=NOMEMORY;
				maxmemory=allcdmem;
			}
			if (maxmemory>memorylimit){
				maxmemory=memorylimit;
			}
			procnb=0;
			if (procctl->status==RUNNING){
				if (pbmtype==8){
					InferenceProcess2UEQ();
				} else {
					InferenceProcess2();
				}
			}

		/* Multi-process case. */
		} else {

			/* Ensure that there are not pending output so that it is not printed */
			/* several times by the children processes. */
			fflush(stdout);

			/* Initialize maximum process memory and temporarily reduce */
			/* the number of cores if necessary. */
			avmemory=GetAvailableMemory();
			rsinfo=mallinfo2();
			allcdmem=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
			if (avmemory>=MEMORYLIMIT){
				if (allcdmem>0){
					ff=((double)(allcdmem+avmemory))/(2*allcdmem);
					if (ff<orgcores){
						if (ff<1){
							active_cores=1;
						} else {
							active_cores=ff;
						}
					}
				}
				allcdmem+=avmemory;
				if (allcdmem>memorylimit){
					allcdmem=memorylimit;
				}
				*maxprcmemory=allcdmem/active_cores;
			} else {
				procctl->status=NOMEMORY;
				*maxprcmemory=allcdmem;
			}

			/* Initialize process specific semaphores and spawn as many processes */
			/* as number of enabled cores minus one (parent process counts). */
			/* The child process cpu time is set to zero. */
			jj=active_cores-1;
			for (procnb=0;procnb<jj;procnb++){
				sem_init(&procsem[procnb],1,0);
				if (0==(pids[procnb]=fork())){
					cpu_time=0;
					break;
				} else if (pids[procnb]==-1){
					procctl->status=NOMEMORY;
					break;
				}
			}

			/* Child process. Set time alarm for timeout detection */
			/* and generate a random seed. */
			if (procnb!=jj){
				alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
				ff=glblopts.timeout-mainkb.prstats.elapsed_time;
				alarmtime.it_value.tv_sec=(int32_t)ff;
				alarmtime.it_value.tv_usec=(int32_t)((ff-(int32_t)ff)*1000000.0);
				setitimer(ITIMER_REAL,&alarmtime,NULL);
				alrmstatus=0;
				srand(time(NULL)+(procnb&1?5*procnb:10*procnb));
			}

			/* Start inference process. */
			if (procctl->status==RUNNING){
				maxmemory=*maxprcmemory;
				if (pbmtype==8){
					InferenceProcess2UEQ();
				} else {
					InferenceProcess2();
				}
			}
		}

		/* Free learning records memory. */
		if (lrnrecords!=NULL){
			for (jj=0;jj<numlrnrecs;jj++){
				MYFREE(lrnrecords[jj].vector.index);
			}
			MYFREE(lrnrecords);
			lrnrecords=NULL;
		}

	/* Problem was solved in the pre-processing. */
	} else {
		procnb=(active_cores-1);
		pids[0]=0; /* Indicate that there are no children processes now. */
	}

	/* Disable time alarm. */
	alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
	alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
	setitimer(ITIMER_REAL,&alarmtime,NULL);

	/* Free feature vector index memory used by GetClauseVector(). */
	MYFREE(fvidx);
	fvidx=NULL;

	/* This is a child process. Clean for exit and exit. */
	if (procnb!=(active_cores-1)){
		Clean4Exit();
		exit(0);
	}

	/* IF WE ARE HERE WE ARE THE PARENT PROCESS. */
	/* Initialize main KB. */
	FreeInfFormulas();
	if (NOMEMORY==Initialize_KB(&mainkb)){
		printf("No memory available, KB not initialized\n");
	}
	ResetStats(&mainkb.prstats);
	procctl->status=STOPPED;

	/* If this execution was multi-pocess then wait with time limit to have a consolidated */
	/* information from all child processes. */
	if (active_cores>1){
		clock_gettime(CLOCK_REALTIME,&waittime1);
		gettimeofday(&mainkb.endtime,NULL);
		waittime1.tv_sec+=(int32_t)(glblopts.timeout-mainkb.prstats.elapsed_time
				-mainkb.endtime.tv_sec+mainkb.time.tv_sec);
		jj=active_cores-1;
		for (ii=0;ii<jj;ii++){
			waittime2=waittime1;
			if (sem_timedwait(&procsem[ii],&waittime2)!=0){
				kill(pids[ii],SIGTERM);
			}
			waitpid(pids[ii],NULL,0);
		}
	}

	/* Destroy common semaphores and return and restore */
	/* original number of active cores. */
	sem_destroy(&procctl->procctl);
	sem_destroy(&procctl->statistics);
	sem_destroy(&procctl->strategy);
	active_cores=orgcores;
	return;
} /* Saturate2 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Set strategy specific KB parameters)  OCJ
 *
 *    This function sets strategy specific parameters for a given
 *    strategy.
 *
 *
 *  ARGUMENTS:
 *
 *    stratflgs: Set of flags with the strategy definition.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *
 *--------------------------------------------------------------*/
int32_t SetMTParams(uint64_t stratflgs){
	auto uint64_t mask1,mask2;                           /* Learning record header masks */
	auto uint64_t mm;                                    /* Auxiliary */

	/* There are user strategy parameters. */
	if (usrparam_mask!=0){

		/* Set user defined parameters that are not learn relevant. */
		if (usrparam_mask&GAMMAMASK){
			kbset.opts.gamma=glblopts.gamma;
		}
		if (usrparam_mask&PRCMASK){
			kbset.opts.precedence=glblopts.precedence;
		}
		if (usrparam_mask&ISPRCMASK){
			kbset.opts.prprcsymprec=glblopts.prprcsymprec;
		}
		if (usrparam_mask&MAXWMASK){
			kbset.opts.mxwghtopt=glblopts.mxwghtopt;
		}
		if (usrparam_mask&CONNECTMASK){
			kbset.opts.connect=glblopts.connect;
		}
		if (usrparam_mask&GRJOINMASK){
			kbset.opts.grjoin=glblopts.grjoin;
		}
		if (usrparam_mask&FACTORINGMASK){
			kbset.opts.factoring=glblopts.factoring;
		}
		if (usrparam_mask&LOOKAHEADMASK){
			kbset.opts.lookahead=glblopts.lookahead;
		}
		if (usrparam_mask&DEMODMASK){
			kbset.opts.demodulation=glblopts.demodulation;
		}
		if (usrparam_mask&LAYERMASK){
			kbset.opts.layer=glblopts.layer;
		}
		if (usrparam_mask&LEARNMASK){
			kbset.opts.learn=glblopts.learn;
		}
		if (usrparam_mask&GOALXFRMMASK){
			if (mainkb.lstunproc->inference!=DEFINTNFOLDING){
				kbset.opts.goalxform=0;
			} else {
				kbset.opts.goalxform=glblopts.goalxform;
			}
		}

		/* Adjust stratflgs with the learn relevant part. */
		stratflgs&=(~usrparam_mask);
		stratflgs|=usrstrat;
	}

	/* Set remaining settings. */
	switch (ALGMASK&stratflgs){
		case OTTER_P:
			kbset.opts.algorithm=OTTER;
			break;
		case DISCOUNT_P:
			kbset.opts.algorithm=DISCOUNT;
			break;
		case UEQDSC_P:
			if (pbmtype==8){
				kbset.opts.algorithm=UEQDSC;
			} else {
				kbset.opts.algorithm=DISCOUNT;
			}
			break;
		case UEQOTT_P:
			if (pbmtype==8){
				kbset.opts.algorithm=UEQOTT;
			} else {
				kbset.opts.algorithm=OTTER;
			}
			break;
	}
	if (UNIFORM_P==(FUNCWMASK&stratflgs)){
		kbset.opts.fweight=UNIFORM;
	} else {
		kbset.opts.fweight=ARITY;
	}
	if (0==(usrparam_mask&GAMMAMASK)){
		switch (GAMMAMASK&stratflgs){
			case GAMMA0_P:
				kbset.opts.gamma=0.0;
				break;
			case GAMMA1_P:
				kbset.opts.gamma=0.1;
				break;
			case GAMMA2_P:
				kbset.opts.gamma=0.2;
				break;
			case GAMMA3_P:
				kbset.opts.gamma=0.3;
				break;
			case GAMMA4_P:
				kbset.opts.gamma=0.4;
				break;
			case GAMMA5_P:
				kbset.opts.gamma=0.5;
				break;
			case GAMMA6_P:
			default:
				kbset.opts.gamma=0.6;
				break;
		}
	}
	if ((LITORDRMASK&stratflgs)==LITORDSTD_P){
		kbset.opts.litord=STANDARD;
	} else {
		kbset.opts.litord=NONRECURSIVE;
	}
	if (TRMORDSTD_P==(TERMORDRMASK&stratflgs)){
		kbset.opts.termord=STANDARD;
	} else {
		kbset.opts.termord=NONRECURSIVE;
	}
	if (0==(usrparam_mask&PRCMASK)){
		switch (PRCMASK&stratflgs){
			case PRCARITY_P:
				kbset.opts.precedence=PRCARITY;
				break;
			case PRCOCCUR_P:
				kbset.opts.precedence=PRCOCCUR;
				break;
			case PRCFREQ_P:
				kbset.opts.precedence=PRCFREQ;
				break;
			case PRCINVAR_P:
				kbset.opts.precedence=PRCINVAR;
				break;
			case PRCINVOC_P:
				kbset.opts.precedence=PRCINVOC;
				break;
			case PRCINVFR_P:
			default:
				kbset.opts.precedence=PRCINVFR;
				break;
		}
	}
	if (0==(usrparam_mask&ISPRCMASK)){
		if (ISPRCLOW_P==(ISPRCMASK&stratflgs)){
			kbset.opts.prprcsymprec=ISPRCLOW;
		} else {
			kbset.opts.prprcsymprec=ISPRCHIGH;
		}
	}
	if ((usrparam_mask==0)&&((glblopts.satmode==1)||((glblopts.satmode==2)
			&&(procnb>=((int)(0.5+(active_cores*(1.0-SATCORES)))))))){
		kbset.opts.maxweight=ABSMAXWEIGHT;
		kbset.opts.mxwghtopt=MXWEIGHTLRSOFF|MXWEIGHTOFF;
	} else {
		if (0==(usrparam_mask&MAXWMASK)){
			switch (MAXWMASK&stratflgs){
				case LRSOTT_P:
					kbset.opts.mxwghtopt=MXWEIGHTLRSOTT|(glblopts.mxwghtopt&(MXWEIGHTNUM|MXWEIGHTOFF));
					break;
				case LRSOFF_P:
					kbset.opts.mxwghtopt=MXWEIGHTLRSOFF|(glblopts.mxwghtopt&(MXWEIGHTNUM|MXWEIGHTOFF));
					break;
				case LRSALL_P:
					kbset.opts.mxwghtopt=MXWEIGHTLRS|(glblopts.mxwghtopt&(MXWEIGHTNUM|MXWEIGHTOFF));
					break;
				default:
					kbset.opts.mxwghtopt=MXWEIGHTLRSDSC|(glblopts.mxwghtopt&(MXWEIGHTNUM|MXWEIGHTOFF));
					break;
			}
		}
		kbset.opts.maxweight=glblopts.maxweight;
	}
	if ((0==(pbmflags&UNITCLAUSE))&&(SPLITON_P==(SPLITMASK&stratflgs))){
		kbset.opts.split=1;
	} else {
		kbset.opts.split=0;
	}
	if (0==(usrparam_mask&CONNECTMASK)){
		if (CONNECTON_P==(CONNECTMASK&stratflgs)){
			kbset.opts.connect=1;
		} else {
			kbset.opts.connect=0;
		}
	}
	if (0==(usrparam_mask&GRJOINMASK)){
		switch (GRJOINMASK&stratflgs){
			case GRJOINON_P:
				kbset.opts.grjoin=1;
				break;
			case GRJOINMED_P:
				kbset.opts.grjoin=2;
				break;
			default:
			case GRJOINOFF_P:
				kbset.opts.grjoin=0;
				break;
		}
	}
	if (0==(usrparam_mask&FACTORINGMASK)){
		if (FACTON_P==(FACTORINGMASK&stratflgs)){
			kbset.opts.factoring=1;
		} else {
			kbset.opts.factoring=0;
		}
	}
	if (0==(usrparam_mask&LOOKAHEADMASK)){
		if (LKAHEADON_P==(LOOKAHEADMASK&stratflgs)){
			kbset.opts.lookahead=1;
		} else {
			kbset.opts.lookahead=0;
		}
	}
	switch (SELECTMASK&stratflgs){
		case SINGLENEG_P:
			kbset.opts.select=SINGLENEG;
			break;
		case MULTINEG_P:
			kbset.opts.select=MULTINEG;
			break;
		case SINGLEPOS_P:
			kbset.opts.select=SINGLEPOS;
			break;
		case MULTIPOS_P:
			kbset.opts.select=MULTIPOS;
			break;
		case MAXIMAL_P:
		default:
			kbset.opts.select=MAXIMAL;
			break;
		case SELECTALL_P:
			kbset.opts.select=SELECTALL;
			break;
		case MXSINGLENEG_P:
			kbset.opts.select=MXSINGLENEG;
			break;
		case TYPE1_P:
			kbset.opts.select=TYPE1;
			break;
		case TYPE2_P:
			kbset.opts.select=TYPE2;
			break;
		case TYPE3_P:
			kbset.opts.select=TYPE3;
			break;
		case TYPE4_P:
			kbset.opts.select=TYPE4;
			break;
		case TYPE5_P:
			kbset.opts.select=TYPE5;
			break;
		case TYPE6_P:
			kbset.opts.select=TYPE6;
			break;
		case TYPE1CPL_P:
			kbset.opts.select=TYPE1CPL;
			break;
		case TYPE2CPL_P:
			kbset.opts.select=TYPE2CPL;
			break;
		case TYPE3CPL_P:
			kbset.opts.select=TYPE3CPL;
			break;
		case TYPE4CPL_P:
			kbset.opts.select=TYPE4CPL;
			break;
		case TYPE5CPL_P:
			kbset.opts.select=TYPE5CPL;
			break;
		case TYPE6CPL_P:
			kbset.opts.select=TYPE6CPL;
			break;
	}
	if (WEAKRWON_P==(WEAKRWMASK&stratflgs)){
		kbset.opts.weakrw=1;
	} else {
		kbset.opts.weakrw=0;
	}
	if (0==(usrparam_mask&DEMODMASK)){
		switch (DEMODMASK&stratflgs){
			case DEMODON_P:
			default:
				kbset.opts.demodulation=DEMODON;
				break;
			case DEMODCPL1_P:
				kbset.opts.demodulation=DEMODCPL1;
				break;
			case DEMODCPL2_P:
				kbset.opts.demodulation=DEMODCPL2;
				break;
			case DEMODOFF_P:
				kbset.opts.demodulation=DEMODOFF;
				break;
			case DEMODONORNT_P:
				kbset.opts.demodulation=DEMODONORNT;
				break;
			case DEMODCPL1ORNT_P:
				kbset.opts.demodulation=DEMODCPL1ORNT;
				break;
			case DEMODCPL2ORNT_P:
				kbset.opts.demodulation=DEMODCPL2ORNT;
				break;
		}
	}
	if (0==(usrparam_mask&LAYERMASK)){
		if (LAYER1_P==(LAYERMASK&stratflgs)){
			kbset.opts.layer=0;
		} else {
			kbset.opts.layer=1;
		}
	}
	if (0==(usrparam_mask&WARMASK)){
		switch (WARMASK&stratflgs){
			case WAR12_P:
				queuerts[0]=1;
				queuerts[1]=2;
				break;
			case WAR11_P:
				queuerts[0]=1;
				queuerts[1]=1;
				break;
			case WAR21_P:
				queuerts[0]=2;
				queuerts[1]=1;
				break;
			case WAR31_P:
				queuerts[0]=3;
				queuerts[1]=1;
				break;
			case WAR41_P:
				queuerts[0]=4;
				queuerts[1]=1;
				break;
			case WAR51_P:
				queuerts[0]=5;
				queuerts[1]=1;
				break;
			case WAR61_P:
			default:
				queuerts[0]=6;
				queuerts[1]=1;
				break;
		}
	}
	if (0==(usrparam_mask&LEARNMASK)){
		if (LEARN_P==(LEARNMASK&stratflgs)){
			kbset.opts.learn=1;
		} else {
			kbset.opts.learn=0;
		}
	}
	if (0==(usrparam_mask&GOALXFRMMASK)){
		if (mainkb.lstunproc->inference!=DEFINTNFOLDING){
			kbset.opts.goalxform=0;
		} else if (GOALXON_P==(GOALXFRMMASK&stratflgs)){
			kbset.opts.goalxform=1;
		} else {
			kbset.opts.goalxform=0;
		}
	}

	/* Initialize selectqueues variable depending on the clause selection */
	/* option and call SetClSelectOrder(). */
	switch (kbset.opts.layerset[kbset.opts.layer]){
		case SIAVLYR:
		case SIHRNLYR:
			selectqueues=6;
			break;
		case AVHRNLYR:
			selectqueues=4;
			break;
		case SINELYR:
			selectqueues=3;
			break;
		case AVLYR:
			selectqueues=2;
			break;
		case HRNLYR:
			selectqueues=2;
			break;
		case NONELYR:
			selectqueues=1;
			break;
	}
	if (NOMEMORY==SetClSelectOrder()){
		return(NOMEMORY);
	}

	/* Disable weak rewriting if there is no minimal term. */
	if (minterm==NULL){
		kbset.opts.weakrw=0;
	}

	/* Set the learning vector, clause selection ratios and gamma parameter. */
	kbset.lrnvector=NULL;
	if ((lrnrecords!=NULL)&&(kbset.opts.learn)){

		/* Set masks for learning record matching. */
		if ((pbmtype==3)||(pbmtype==8)){
			mask1=TERMORDRMASK|FUNCWMASK|WEAKRWMASK|SPLITMASK;
			mask2=ALGMASK|TERMORDRMASK|FUNCWMASK|WEAKRWMASK|SPLITMASK;
		} else {
			mask1=SELECTMASK|TERMORDRMASK|LITORDRMASK|FUNCWMASK|SPLITMASK;
			mask2=ALGMASK|SELECTMASK|TERMORDRMASK|LITORDRMASK|FUNCWMASK|SPLITMASK;
		}

		/* Initialize the learning vector for this process. If no matching record is found */
		/* but all strategy parameters match except the algorithm then take that record */
		/* if such a record exist. */
		for (mm=0;mm<numlrnrecs;mm++){
			if ((stratflgs&mask1)==(mask1&lrnrecords[mm].header)){
				if ((stratflgs&mask2)==(mask2&lrnrecords[mm].header)){
					kbset.lrnvector=&lrnrecords[mm].vector;
					break;
				}
				switch (kbset.opts.algorithm){
					case OTTER:
						if (DISCOUNT_P==(DISCOUNT_P&lrnrecords[mm].header)){
							kbset.lrnvector=&lrnrecords[mm].vector;
						}
						break;
					case DISCOUNT:
						if (OTTER_P==(OTTER_P&lrnrecords[mm].header)){
							kbset.lrnvector=&lrnrecords[mm].vector;
						}
						break;
					case UEQDSC:
						if (UEQOTT_P==(UEQOTT_P&lrnrecords[mm].header)){
							kbset.lrnvector=&lrnrecords[mm].vector;
						}
						break;
					case UEQOTT:
						if (UEQDSC_P==(UEQDSC_P&lrnrecords[mm].header)){
							kbset.lrnvector=&lrnrecords[mm].vector;
						}
						break;
				}
			}
		}
	}

	/* Set remaining options and settings. */
	lkhdcomplete=1;
	return(0);
} /* SetMTParams */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Main inference function for the process for non UEQ)  OCJ
 *
 *    This function controls the inference tasks for the process. There
 *    is an instance of this function per executed process.
 *
 *    If there is a user defined strategy, either by specific commands
 *    like SELECT or ALGORITHM or a full strategy defined by the STRATEGY
 *    command, then that strategy is executed until a solution is found
 *    or time limit expires.
 *
 *    If there is no user defined strategy then the standard portfolio
 *    is executed assigning to each portfolio strategy a number of CPU
 *    hardware instructions.
 *
 *    If no solution is found and there is still some time left then the
 *    complete strategies in the portfolio are executed again with an
 *    increased number of hardware instructions.
 *
 *    If no solution is found or there is not time available to execute
 *    another ground strategy  with the increased number of instructions
 *    but there is still some time left then random strategies are executed
 *    until time limit expires.
 *
 *    The relevant global variables for the process are explained below.
 *    Let:
 *      s -> number of strategies in portfolio for a problem type.
 *      c -> number complete of strategies in portfolio for a problem type.
 *    Then:
 *    - pbmtypestrats[PBMTYPES+1][max(s+c)] contains the portfolio strategies
 *      with learning for each problem type. The first s strategies are all
 *      the portfolio strategies and the next c strategies are those
 *      portfolio strategies that are complete. Each strategy is a 64 bits
 *      number that contain the flags that define the strategy. The first index
 *      is the value of pbmtype global variable. The second index is the
 *      strategy: the first s strategies are all the portfolio strategies and
 *      the next c strategies are the complete portfolio strategies so complete
 *      strategies are duplicated in this variable.
 *    - pbmtypeinstr[PBMTYPES+1][s] contains the number of hardware instructions
 *      assigned to each strategy with learning for each problem type. The
 *      first index is the the same as in pbmtypestrats[][][]. The second
 *      index is the strategy but only for the portfolio strategies without
 *      duplicating the complete ones.
 *    - numstrats[PBMTYPES+1][2] contains the number of portfolio strategies
 *      and complete portfolio strategies. The first index is the the same as
 *      in pbmtypestrats[][]. The second index is 0 for the number of portfolio
 *      strategies and 1 for the number of complete strategies in the portfolio.
 *    - IMPORTANT: Type 8 problems have two sets of strategies depending on the
 *      goal type (ground or non ground). This is the reason for the [PBMTYPES+1]
 *      dimension in the three sets explained above.
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
 *
 *--------------------------------------------------------------*/
void InferenceProcess(void){
	auto struct mallinfo2 rsinfo;                        /* For mallinfo2() calls */
	auto int32_t stratnb;                                /* Strategy number */
	auto int32_t numstrts;                               /* Number of strategies in portfolio */
	auto int32_t allstrts;                               /* Number of strategies in portfolio plus complete strategies duplicated */
	auto uint64_t stratflgs;                             /* Strategy flags */
	auto struct perf_event_attr perfevent;               /* Instruction count performance event */
	auto uint64_t instrrate;                             /* Hardware instructions executed per second */
	auto float tmfactor;                                 /* Factor to be applied to number of instructions limit */
	auto clock_t start;                                  /* CPU ticks to estimate hw instruction rate */
	auto int32_t typeindex;                              /* Problem type index */
	auto cmprefix *ptr1,*ptr2;                           /* Auxiliary pointers */
	auto int32_t ii;                                     /* Auxiliary */
	auto uint64_t jj;                                    /* Auxiliary */

	/* Set typeindex. Problem type 8 has to sets of strategies in numstrats[][] */
	/* depending on the type of goal (ground or non ground). */
	if ((pbmtype==8)&&(ueqgoal->part2.bin->maxvarnb>=0)){
		typeindex=9;
	} else {
		typeindex=pbmtype;
	}

	/* Allocate memory for the weight and variable balance global structure. */
	if (NULL==(wbdata.vb=MYALLOC(VARBALANCE_CHUNK_SIZE*sizeof(*wbdata.vb)))){
		kbset.status=NOMEMORY;
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return;
	}
	wbdata.size=VARBALANCE_CHUNK_SIZE;

	/* Allocate memory for predicate multiset data used for subsumption pruning. */
	if (NULL==(predicateset=MYALLOC(ii=2*(numpreds+1)*sizeof(predicateset[0])))){
		kbset.status=NOMEMORY;
		MYFREE(wbdata.vb);
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return;
	}
	memset(predicateset,0,ii);
	sstimestamp=0;

	/* Initialize allstrts, cplstrats and tmfactor. */
	numstrts=numstrats[typeindex][0];
	allstrts=numstrts+numstrats[typeindex][1];
	tmfactor=foftmfactor*usrtmfactor;

	/* Initialize file descriptor to read number of executed hardware instructions. */
	if (perfmon){
		memset(&perfevent,0,sizeof(perfevent));
		perfevent.type=PERF_TYPE_HARDWARE;
		perfevent.size=sizeof(perfevent);
		perfevent.config=PERF_COUNT_HW_INSTRUCTIONS;
		perfevent.disabled=1;
		perfevent.exclude_kernel=1;
		perfevent.exclude_hv=1;
		filedesc=syscall(SYS_perf_event_open,&perfevent,0,-1,-1,0);
	} else {
		filedesc=-1;
	}

	/* Slice execution loop. */
	kbset.status=RUNNING;
	instrrate=0;
	stratnb=0; /* Just to prevent unnecessary compiler warnings. */
	start=0; /* Just to prevent unnecessary compiler warnings. */
	while (1){

		/* Set strategy. */
		if (NOMEMORY==SelectStrategy(numstrts,allstrts,&stratnb,&stratflgs)){
			kbset.status=NOMEMORY;
			MYFREE(wbdata.vb);
			MYFREE(predicateset);
			sem_wait(&procctl->procctl);
			if (procctl->status==RUNNING){
				procctl->status=NOMEMORY;
			}
			sem_post(&procctl->procctl);
			if (filedesc!=-1){
				close(filedesc);
			}
			return;
		}

		/* Shuffle off/on loop. */
		kbset.opts.shuffle=0;
		while (1){

			/* Set symbol precedence. */
			SetSymbolPrecedence();

			/* Initialize the number of formulas in the working KB with */
			/* the number of text formulas in the main KB. This way the */
			/* formula numbering in both kb's will be compatible after */
			/* loading the working KB with passive binary clauses in the */
			/* main KB. */
			kbset.nformulas=mainkb.nformulas;

			/* Load working KB with unprocessed binary clauses in the main KB. */
			for (ptr1=mainkb.frstunproc;ptr1!=NULL;ptr1=ptr1->next){

				/* Allocate storage for the copied clause. */
				if (NULL==(ptr2=MYALLOC(sizeof(cmprefix)))){
					kbset.status=NOMEMORY;
					break;
				}
				memcpy(ptr2,ptr1,sizeof(cmprefix));
				if (NULL==(ptr2->part2.bin=MYALLOC(binpsize+ptr1->part2.bin->size))){
					kbset.status=NOMEMORY;
					MYFREE(ptr2);
					break;
				}

				/* Copy binary part of clause and clean equality data. */
				memcpy(ptr2->part2.bin,ptr1->part2.bin,binpsize+ptr1->part2.bin->size);
				ptr2->part2.bin->oriented=0;
				ptr2->part2.bin->clause=ptr2;

				/* Split clause if possible. */
				if (NOMEMORY==(ii=SplitClause(ptr2,0))){
					kbset.status=NOMEMORY;
					MYFREE(ptr2->part2.bin);
					MYFREE(ptr2);
					break;
				}

				/* If clause was not split then add clause to KB as unprocessed. */
				if (ii==0){
					if (NOMEMORY==AddBinClause2KB(ptr2,&kbset,0)){
						kbset.status=NOMEMORY;
						MYFREE(ptr2->part2.bin);
						MYFREE(ptr2);
						break;
					}
				}

				/* Check timeout. */
				if (procctl->status==TIMEOUT){
					kbset.status=TIMEOUT;
					MYFREE(wbdata.vb);
					MYFREE(predicateset);
					if (filedesc!=-1){
						close(filedesc);
					}
					return;
				}
			}

			/* If a NOMEMORY condition was detected disable the time alarm, */
			/* set process status and return. */
			if (kbset.status==NOMEMORY){
				MYFREE(wbdata.vb);
				MYFREE(predicateset);
				sem_wait(&procctl->procctl);
				if (procctl->status==RUNNING){
					procctl->status=NOMEMORY;
				}
				sem_post(&procctl->procctl);
				if (filedesc!=-1){
					close(filedesc);
				}
				return;
			}

			/* Compute the limit of hardware instructions to execute. */
			SetInstrLimit(stratnb,numstrts,allstrts,instrrate,tmfactor,
					active_cores<MINCORES4BESTSTRAT?0:kbset.opts.shuffle==0?1:2);

			/* Start instruction limit control. */
			if (instrrate==0){
				if (cpuavail){
					start=clock();
				} else {
					gettimeofday(&kbset.time,NULL);
				}
			}
			myioctl(PERF_EVENT_IOC_RESET);
			myioctl(PERF_EVENT_IOC_ENABLE);

			/* Start process algorithm. */
			switch (kbset.opts.algorithm){
				case OTTER:
					Otter();
					break;
				case DISCOUNT:
					Discount();
					break;
				case UEQOTT:
					UEQOtter();
					break;
				case UEQDSC:
					UEQDscnt();
					break;
			}

			/* Disable instructions count and compute instructions rate if applicable. */
			myioctl(PERF_EVENT_IOC_DISABLE);
			if (instrrate==0){
				myread(&instrrate);
				if (cpuavail){
					start=clock()-start;
					instrrate=0.5+(instrrate/(((double)start)/CLOCKS_PER_SEC));
				} else {
					gettimeofday(&kbset.endtime,NULL);
					instrrate=0.5+(instrrate/(kbset.endtime.tv_sec-kbset.time.tv_sec
							+((kbset.endtime.tv_usec-kbset.time.tv_usec)/1000000.0)));
				}
			}

			/* Adjust process maximum used memory. */
			rsinfo=mallinfo2();
			jj=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
			if (jj>mainkb.prstats.memory){
				mainkb.prstats.memory=jj;
				mainkb.prstats.netmemory=rsinfo.uordblks+rsinfo.hblkhd;
			}

			/* Update problem statistics. */
			sem_wait(&procctl->statistics);
			AddInferenceStats(&kbset.prstats,pbmstats);
			sem_post(&procctl->statistics);

			/* There is a solution or TIMEOUT or NOMEMORY condition. Reinitialize */
			/* data if this is not the solving process and KB data must not be */
			/* printed and exit the function. */
			if ((procctl->status!=RUNNING)&&(procctl->status!=UNKNOWN)&&(procctl->status!=SATQTIMEOUT)
					&&(procctl->status!=SLICETMOUT)){
				if (verbose){
					sem_wait(&procctl->statistics);
					printf("Process %d: strategy=%ld, shuffle=%s, instr. limit=%ld, process terminated with status=",
							procnb,stratflgs,kbset.opts.shuffle?"ON":"OFF",instrlimit);
					switch (kbset.status){
						case UNKNOWN:
							printf("UNKNOWN\n");
							break;
						case TIMEOUT:
							printf("TIMEOUT\n");
							break;
						case NOMEMORY:
							printf("NOMEMORY\n");
							break;
						default:
							printf("SOLVED\n");
							break;
					}
					sem_post(&procctl->statistics);
				}
				if ((procctl->solvekb!=procnb)&&((prtkbdata==0)||(active_cores>1))){
					Initialize_hashcmp();
					Initialize_KB(&kbset);
					ResetStats(&kbset.prstats);
				}
				break;
			} else if (verbose){
				sem_wait(&procctl->statistics);
				printf("Process %d: strategy=%ld, shuffle=%s, instr. limit=%ld, result=FAIL\n",
						procnb,stratflgs,kbset.opts.shuffle?"ON":"OFF",instrlimit);
				sem_post(&procctl->statistics);
			}

			/* Reinitialize working KB and reset statistics. */
			Initialize_hashcmp();
			Initialize_KB(&kbset);
			ResetStats(&kbset.prstats);

			/* Prepare next iteration for the case that shuffle must be enabled. */
			if ((glblopts.shuffle!=0)&&(kbset.opts.shuffle==0)&&(kbset.status!=UNKNOWN)
					&&((glblopts.satmode==0)||((glblopts.satmode==2)&&(procnb<((int)(0.5+(active_cores*(1.0-SATCORES)))))))
					&&(stratnb<numstrts)){
				kbset.opts.shuffle=1;
				#ifdef ADDSTRAT2SEED
				if (stratnb&1){
					srand((RAND_MAX/2)+pbmtype+stratnb);
				} else {
					srand((RAND_MAX/2)+pbmtype-stratnb);
				}
				#else
				if (stratnb&1){
					srand((RAND_MAX/2)+stratnb);
				} else {
					srand((RAND_MAX/2)-stratnb);
				}
				#endif
				if (0!=(ii=ShuffleSymbols())){
					kbset.status=(ii==NOMEMORY?NOMEMORY:UNKNOWN);
					MYFREE(wbdata.vb);
					MYFREE(predicateset);
					sem_wait(&procctl->procctl);
					if (procctl->status==RUNNING){
						procctl->status=kbset.status;
					}
					sem_post(&procctl->procctl);
					printf("Process %d: strategy=%ld, instr. limit=%ld, process terminated with status=%s\n",
							procnb,stratflgs,instrlimit,kbset.status==NOMEMORY?"NOMEMORY":"UNKNOOWN");
					if (filedesc!=-1){
						close(filedesc);
					}
					return;
				}
				kbset.status=RUNNING;

			/* Prepare next iteration for the case that shuffle must not be enabled or must be disabled. */
			} else {
				if (kbset.opts.shuffle){
					UnshuffleSymbols();
					kbset.opts.shuffle=0;
				}
				kbset.status=RUNNING;
				break;
			}
		}

		/* There is a solution or TIMEOUT or NOMEMORY condition. */
		if ((procctl->status!=RUNNING)&&(procctl->status!=UNKNOWN)&&(procctl->status!=SATQTIMEOUT)){
			break;
		}
	}

	/* Close hardware instructions file descriptor and return. */
	MYFREE(wbdata.vb);
	MYFREE(predicateset);
	if (filedesc!=-1){
		close(filedesc);
	}
	return;
} /* InferenceProcess */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Main inference function for the process for UEQ)  OCJ
 *
 *    This function controls the inference tasks for the process. There
 *    is an instance of this function per executed process.
 *
 *    If there is a user defined strategy, either by specific commands
 *    like SELECT or ALGORITHM or a full strategy defined by the STRATEGY
 *    command, then that strategy is executed until a solution is found
 *    or time limit expires.
 *
 *    If there is no user defined strategy then the standard portfolio
 *    is executed assigning to each portfolio strategy a number of CPU
 *    hardware instructions.
 *
 *    If no solution is found and there is still some time left then the
 *    complete strategies in the portfolio are executed again with an
 *    increased number of hardware instructions.
 *
 *    If no solution is found or there is not time available to execute
 *    another ground strategy  with the increased number of instructions
 *    but there is still some time left then random strategies are executed
 *    until time limit expires.
 *
 *    The relevant global variables for the process are explained below.
 *    Let:
 *      s -> number of strategies in portfolio for a problem type.
 *      c -> number complete of strategies in portfolio for a problem type.
 *    Then:
 *    - pbmtypestrats[PBMTYPES][max(s+c)] contains the portfolio strategies
 *      with learning for each problem type. The first s strategies are all
 *      the portfolio strategies and the next c strategies are those
 *      portfolio strategies that are complete. Each strategy is a 64 bits
 *      number that contain the flags that define the strategy. The first index
 *      is the value of pbmtype global variable. The second index is the
 *      strategy: the first s strategies are all the portfolio strategies and
 *      the next c strategies are the complete portfolio strategies so complete
 *      strategies are duplicated in this variable.
 *    - pbmtypeinstr[PBMTYPES][s] contains the number of hardware instructions
 *      assigned to each strategy with learning for each problem type. The
 *      first index is the the same as in pbmtypestrats[][][]. The second
 *      index is the strategy but only for the portfolio strategies without
 *      duplicating the complete ones.
 *    - numstrats[PBMTYPES][2] contains the number of portfolio strategies
 *      and complete portfolio strategies. The first index is the the same as
 *      in pbmtypestrats[][]. The second index is 0 for the number of portfolio
 *      strategies and 1 for the number of complete strategies in the portfolio.
 *    - IMPORTANT: Type 8 problems have two sets of strategies depending on the
 *      goal type (ground or non ground). This is the reason for the [PBMTYPES+1]
 *      dimension in the three sets explained above.
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
 *
 *--------------------------------------------------------------*/
void InferenceProcessUEQ(void){
	auto cmprefix *trprfclause;                          /* Clause suitable for a trivial proof. */
	auto struct mallinfo2 rsinfo;                        /* For mallinfo2() calls */
	auto int32_t stratnb;                                /* Strategy number */
	auto int32_t numstrts;                               /* Number of strategies in portfolio */
	auto int32_t allstrts;                               /* Number of strategies in portfolio plus complete strategies duplicated */
	auto uint64_t stratflgs;                             /* Strategy flags */
	auto struct perf_event_attr perfevent;               /* Instruction count performance event */
	auto uint64_t instrrate;                             /* Hardware instructions executed per second */
	auto float tmfactor;                                 /* Factor to be applied to number of instructions limit */
	auto clock_t start;                                  /* CPU ticks to estimate hw instruction rate */
	auto int32_t typeindex;                              /* Problem type index */
	auto cmprefix *ptr1,*ptr2;                           /* Auxiliary pointers */
	auto uint8_t *ptr3,*ptr4;                            /* Auxiliary pointers */
	auto int32_t ii;                                     /* Auxiliary */
	auto uint64_t jj;                                    /* Auxiliary */

	/* Set typeindex. Problem type 8 has to sets of strategies in numstrats[][] */
	/* depending on the type of goal (ground or non ground). */
	if ((pbmtype==8)&&(ueqgoal->part2.bin->maxvarnb>=0)){
		typeindex=9;
	} else {
		typeindex=pbmtype;
	}

	/* Allocate memory for the weight and variable balance global structure. */
	if (NULL==(wbdata.vb=MYALLOC(VARBALANCE_CHUNK_SIZE*sizeof(*wbdata.vb)))){
		kbset.status=NOMEMORY;
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return;
	}
	wbdata.size=VARBALANCE_CHUNK_SIZE;

	/* Initialize allstrts, cplstrats and tmfactor. */
	numstrts=numstrats[typeindex][0];
	allstrts=numstrts+numstrats[typeindex][1];
	tmfactor=ueqtmfactor*usrtmfactor;

	/* Initialize file descriptor to read number of executed hardware instructions. */
	if (perfmonstate==0){
		perfmon=0;
		perfmonrate=HWINSTRATE;
	}
	if (perfmon){
		memset(&perfevent,0,sizeof(perfevent));
		perfevent.type=PERF_TYPE_HARDWARE;
		perfevent.size=sizeof(perfevent);
		perfevent.config=PERF_COUNT_HW_INSTRUCTIONS;
		perfevent.disabled=1;
		perfevent.exclude_kernel=1;
		perfevent.exclude_hv=1;
		filedesc=syscall(SYS_perf_event_open,&perfevent,0,-1,-1,0);
	} else {
		filedesc=-1;
	}

	/* Slice execution loop. */
	kbset.status=RUNNING;
	instrrate=0;
	stratnb=0; /* Just to prevent unnecessary compiler warnings. */
	start=0; /* Just to prevent unnecessary compiler warnings. */
	while (1){

		/* Set strategy. */
		if (NOMEMORY==SelectStrategy(numstrts,allstrts,&stratnb,&stratflgs)){
			kbset.status=NOMEMORY;
			MYFREE(wbdata.vb);
			MYFREE(predicateset);
			sem_wait(&procctl->procctl);
			if (procctl->status==RUNNING){
				procctl->status=NOMEMORY;
			}
			sem_post(&procctl->procctl);
			if (filedesc!=-1){
				close(filedesc);
			}
			return;
		}

		/* Shuffle off/on loop. */
		kbset.opts.shuffle=0;
		while (1){

			/* Set symbol precedence. */
			SetSymbolPrecedence();

			/* Initialize the number of formulas in the working KB with */
			/* the number of text formulas in the main KB. This way the */
			/* formula numbering in both kb's will be compatible after */
			/* loading the working KB with passive binary clauses in the */
			/* main KB. */
			kbset.nformulas=mainkb.nformulas;

			/* Algorithm is OTTER or DISCOUNT. */
			if ((kbset.opts.algorithm!=UEQOTT)&&(kbset.opts.algorithm!=UEQDSC)){

				/* Load working KB with unprocessed binary clauses in the main KB. */
				for (ptr1=mainkb.frstunproc;ptr1!=NULL;ptr1=ptr1->next){

					/* If the goal transformation definitions must be loaded then */
					/* don't load the ground unit inequality goal. */
					if (kbset.opts.goalxform){
						if (ptr1==ueqgoal){
							continue;
						}

					/* If the goal transformation definitions must not be loaded */
					/* leave the loop if ptr1 is a goal definition clause. */
					} else if (ptr1->inference==GOALDEFINITION){
						break;
					}

					/* Allocate storage for the copied clause. */
					if (NULL==(ptr2=MYALLOC(sizeof(cmprefix)))){
						kbset.status=NOMEMORY;
						break;
					}
					memcpy(ptr2,ptr1,sizeof(cmprefix));
					if (NULL==(ptr2->part2.bin=MYALLOC(binpsize+ptr1->part2.bin->size))){
						kbset.status=NOMEMORY;
						MYFREE(ptr2);
						break;
					}

					/* Copy binary part of clause and clean equality data. */
					memcpy(ptr2->part2.bin,ptr1->part2.bin,binpsize+ptr1->part2.bin->size);
					ptr2->part2.bin->oriented=0;
					ptr2->part2.bin->clause=ptr2;

					/* If clause inference is GOALDEFINITION then set parent1 of original */
					/* clause pointing to the duplicated clause. This is to allow identifying */
					/* kbset goal transformation clauses by scanning mainkb goal transformation */
					/* clauses that are always at the end of the UNPROC mainkb queue. */
					if (ptr1->inference==GOALDEFINITION){
						ptr1->parent1=ptr2;
					}

					/* Add clause to KB as unprocessed. */
					if (NOMEMORY==AddBinClause2KBUEQ(ptr2,&kbset,0)){
						kbset.status=NOMEMORY;
						MYFREE(ptr2->part2.bin);
						MYFREE(ptr2);
						break;
					}

					/* Check timeout. */
					if (procctl->status==TIMEOUT){
						kbset.status=TIMEOUT;
						MYFREE(wbdata.vb);
						MYFREE(predicateset);
						if (filedesc!=-1){
							close(filedesc);
						}
						return;
					}
				}

				/* If a NOMEMORY condition was detected disable the time alarm, */
				/* set process status and return. */
				if (kbset.status==NOMEMORY){
					MYFREE(wbdata.vb);
					MYFREE(predicateset);
					sem_wait(&procctl->procctl);
					if (procctl->status==RUNNING){
						procctl->status=NOMEMORY;
					}
					sem_post(&procctl->procctl);
					if (filedesc!=-1){
						close(filedesc);
					}
					return;
				}

			/* Algorithm is UEQOTT or UEQDSC. */
			} else {

				/* Load working KB with unprocessed binary clauses in the main KB. */
				trprfclause=NULL; /* No clause for trivial proof exists yet. */
				for (ptr1=mainkb.frstunproc;ptr1!=NULL;ptr1=ptr1->next){

					/* If the goal transformation definitions must be loaded then */
					/* don't load the ground unit inequality goal. */
					if (kbset.opts.goalxform){
						if (ptr1==ueqgoal){
							continue;
						}

					/* If the goal transformation definitions must not be loaded */
					/* leave the loop if ptr1 is a goal definition clause. */
					} else if (ptr1->inference==GOALDEFINITION){
						break;
					}

					/* Allocate storage for the copied clause. */
					if (NULL==(ptr2=MYALLOC(sizeof(cmprefix)))){
						kbset.status=NOMEMORY;
						break;
					}
					memcpy(ptr2,ptr1,sizeof(cmprefix));
					if (NULL==(ptr2->part2.bin=MYALLOC(binpsize+ptr1->part2.bin->size))){
						kbset.status=NOMEMORY;
						MYFREE(ptr2);
						break;
					}

					/* Copy binary part of clause and clean equality data. */
					memcpy(ptr2->part2.bin,ptr1->part2.bin,binpsize+ptr1->part2.bin->size);
					ptr2->part2.bin->oriented=0;
					ptr2->part2.bin->clause=ptr2;

					/* Add clause to KB as UNPROC. */
					if (NOMEMORY==AddUEQClause2KB(ptr2)){
						kbset.status=NOMEMORY;
						MYFREE(ptr2->part2.bin);
						MYFREE(ptr2);
						break;
					}

					/* Clause for trivial proof already exist. */
					if (trprfclause!=NULL){

						/* If we have a goal then build trivial proof and return. */
						if (kbset.negeq!=NULL){
							if (NOMEMORY==MakeTrivialProof(trprfclause)){
								kbset.status=NOMEMORY;
								break;
							} else {
								kbset.status=UNSATISFIABLE;
								MYFREE(wbdata.vb);
								MYFREE(predicateset);
								sem_wait(&procctl->procctl);
								if (procctl->status!=UNSATISFIABLE){
									procctl->status=UNSATISFIABLE;
									procctl->solvekb=procnb;
								}
								sem_post(&procctl->procctl);
								if (filedesc!=-1){
									close(filedesc);
								}
								return;
							}
						}

					/* Clause for trivial proof doesn't exist. */
					} else {

						/* Current clause is suitable for trivial proof because it is of */
						/* the form x ≐ t where x is a variable that is not in term t. */
						ptr3=NextItem(&ptr2->part2.bin->formula[0],IMMED);
						ptr4=NextItem(ptr3,OVERSUBTERMS);
						if (((VARIABLE&ptr3[0])&&(0==CheckVarInTerm2(ptr3,ptr4)))
								||((VARIABLE&ptr4[0])&&(0==CheckVarInTerm2(ptr4,ptr3)))){

							/* Set pointer to clause suitable for trivial proof. */
							trprfclause=ptr2;

							/* If we have a goal then build trivial proof and return. */
							if (kbset.negeq!=NULL){
								if (NOMEMORY==MakeTrivialProof(trprfclause)){
									kbset.status=NOMEMORY;
									break;
								} else {
									kbset.status=UNSATISFIABLE;
									MYFREE(wbdata.vb);
									MYFREE(predicateset);
									sem_wait(&procctl->procctl);
									if (procctl->status!=UNSATISFIABLE){
										procctl->status=UNSATISFIABLE;
										procctl->solvekb=procnb;
									}
									sem_post(&procctl->procctl);
									if (filedesc!=-1){
										close(filedesc);
									}
									return;
								}
							}
						}
					}

					/* Check timeout. */
					if (procctl->status==TIMEOUT){
						kbset.status=TIMEOUT;
						MYFREE(wbdata.vb);
						MYFREE(predicateset);
						if (filedesc!=-1){
							close(filedesc);
						}
						return;
					}
				}

				/* If a NOMEMORY condition was detected then set process status and return. */
				if (kbset.status==NOMEMORY){
					MYFREE(wbdata.vb);
					MYFREE(predicateset);
					sem_wait(&procctl->procctl);
					if (procctl->status==RUNNING){
						procctl->status=NOMEMORY;
					}
					sem_post(&procctl->procctl);
					if (filedesc!=-1){
						close(filedesc);
					}
					return;
				}
			}

			/* Compute the limit of hardware instructions to execute. */
			SetInstrLimit(stratnb,numstrts,allstrts,instrrate,tmfactor,
					active_cores<MINCORES4BESTSTRAT?0:kbset.opts.shuffle==0?1:2);

			/* Start instruction limit control. */
			if (instrrate==0){
				if (cpuavail){
					start=clock();
				} else {
					gettimeofday(&kbset.time,NULL);
				}
			}
			myioctl(PERF_EVENT_IOC_RESET);
			myioctl(PERF_EVENT_IOC_ENABLE);

			/* Start process algorithm. */
			switch (kbset.opts.algorithm){
				case OTTER:
					OtterUEQ();
					break;
				case DISCOUNT:
					DiscountUEQ();
					break;
				case UEQOTT:
					UEQOtter();
					break;
				case UEQDSC:
					UEQDscnt();
					break;
			}

			/* Disable instructions count and compute instructions rate if applicable. */
			myioctl(PERF_EVENT_IOC_DISABLE);
			if (instrrate==0){
				myread(&instrrate);
				if (cpuavail){
					start=clock()-start;
					instrrate=0.5+(instrrate/(((double)start)/CLOCKS_PER_SEC));
				} else {
					gettimeofday(&kbset.endtime,NULL);
					instrrate=0.5+(instrrate/(kbset.endtime.tv_sec-kbset.time.tv_sec
							+((kbset.endtime.tv_usec-kbset.time.tv_usec)/1000000.0)));
				}
			}

			/* Adjust process maximum used memory. */
			rsinfo=mallinfo2();
			jj=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
			if (jj>mainkb.prstats.memory){
				mainkb.prstats.memory=jj;
				mainkb.prstats.netmemory=rsinfo.uordblks+rsinfo.hblkhd;
			}

			/* Update problem statistics. */
			sem_wait(&procctl->statistics);
			AddInferenceStats(&kbset.prstats,pbmstats);
			sem_post(&procctl->statistics);

			/* There is a solution or TIMEOUT or NOMEMORY condition. Reinitialize */
			/* data if this is not the solving process and KB data must not be */
			/* printed and exit the function. */
			if ((procctl->status!=RUNNING)&&(procctl->status!=UNKNOWN)&&(procctl->status!=SATQTIMEOUT)
					&&(procctl->status!=SLICETMOUT)){
				if (verbose){
					sem_wait(&procctl->statistics);
					printf("Process %d: strategy=%ld, shuffle=%s, instr. limit=%ld, process terminated with status=",
							procnb,stratflgs,kbset.opts.shuffle?"ON":"OFF",instrlimit);
					switch (kbset.status){
						case UNKNOWN:
							printf("UNKNOWN\n");
							break;
						case TIMEOUT:
							printf("TIMEOUT\n");
							break;
						case NOMEMORY:
							printf("NOMEMORY\n");
							break;
						default:
							printf("SOLVED\n");
							break;
					}
					sem_post(&procctl->statistics);
				}
				if ((procctl->solvekb!=procnb)&&((prtkbdata==0)||(active_cores>1))){
					Initialize_hashcmp();
					Initialize_KB(&kbset);
					ResetStats(&kbset.prstats);
				}
				break;
			} else if (verbose){
				sem_wait(&procctl->statistics);
				printf("Process %d: strategy=%ld, shuffle=%s, instr. limit=%ld, result=FAIL\n",
						procnb,stratflgs,kbset.opts.shuffle?"ON":"OFF",instrlimit);
				sem_post(&procctl->statistics);
			}

			/* Reinitialize working KB and reset statistics. */
			Initialize_hashcmp();
			Initialize_KB(&kbset);
			ResetStats(&kbset.prstats);

			/* There is a solution or TIMEOUT or NOMEMORY condition. */
			if ((procctl->status!=RUNNING)&&(procctl->status!=UNKNOWN)&&(procctl->status!=SATQTIMEOUT)){
				break;
			}

			/* Prepare next iteration for the case that shuffle must be enabled. */
			if ((glblopts.shuffle!=0)&&(kbset.opts.shuffle==0)&&(kbset.status!=UNKNOWN)
					&&((glblopts.satmode==0)||((glblopts.satmode==2)&&(procnb<((int)(0.5+(active_cores*(1.0-SATCORES)))))))
					&&(stratnb<numstrts)){
				kbset.opts.shuffle=1;
				#ifdef ADDSTRAT2SEED
				if (stratnb&1){
					srand((RAND_MAX/2)+pbmtype+stratnb);
				} else {
					srand((RAND_MAX/2)+pbmtype-stratnb);
				}
				#else
				if (stratnb&1){
					srand((RAND_MAX/2)+stratnb);
				} else {
					srand((RAND_MAX/2)-stratnb);
				}
				#endif
				if (0!=(ii=ShuffleSymbols())){
					kbset.status=(ii==NOMEMORY?NOMEMORY:UNKNOWN);
					MYFREE(wbdata.vb);
					MYFREE(predicateset);
					sem_wait(&procctl->procctl);
					if (procctl->status==RUNNING){
						procctl->status=kbset.status;
					}
					sem_post(&procctl->procctl);
					printf("Process %d: strategy=%ld, instr. limit=%ld, process terminated with status=%s\n",
							procnb,stratflgs,instrlimit,kbset.status==NOMEMORY?"NOMEMORY":"UNKNOOWN");
					if (filedesc!=-1){
						close(filedesc);
					}
					return;
				}
				kbset.status=RUNNING;

			/* Prepare next iteration for the case that shuffle must not be enabled or must be disabled. */
			} else {
				if (kbset.opts.shuffle){
					UnshuffleSymbols();
					kbset.opts.shuffle=0;
				}
				kbset.status=RUNNING;
				break;
			}
		}

		/* There is a solution or TIMEOUT or NOMEMORY condition. */
		if ((procctl->status!=RUNNING)&&(procctl->status!=UNKNOWN)&&(procctl->status!=SATQTIMEOUT)){
			break;
		}
	}

	/* Close hardware instructions file descriptor and return. */
	MYFREE(wbdata.vb);
	MYFREE(predicateset);
	if (filedesc!=-1){
		close(filedesc);
	}
	return;
} /* InferenceProcessUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Main inference function for non UEQ strategy tuning)  OCJ
 *
 *    This function is similar to InferenceProcess() function except
 *    for the following:
 *    - It calls a saturation algorithm only once and only for the
 *      user defined strategy.
 *    - If it is executed by a child process then the input problem
 *      is randomized and randomization is applied through the
 *      saturation process.
 *    - If the problem is solved then the value of the strategy
 *      variable and the number of used instructions are reported
 *      using procctl->statistics semaphore to prevent mixing outputs
 *      from different processes.
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
 *
 *--------------------------------------------------------------*/
void InferenceProcess2(void){
	auto struct mallinfo2 rsinfo;                        /* For mallinfo2() calls */
	auto struct perf_event_attr perfevent;               /* Instruction count performance event */
	auto uint64_t orginstrlimit;                         /* Original instrlimit value. */
	auto cmprefix *ptr1,*ptr2;                           /* Auxiliary pointers */
	auto int32_t ii;                                     /* Auxiliary */
	auto uint64_t jj;                                    /* Auxiliary */
	auto float dd;                                       /* Auxiliary */
	#ifndef STAREXECTUNING
	auto char *ptr5;                                     /* Auxiliary for input file name. */
	#endif

	/* Allocate memory for the weight and variable balance global structure. */
	if (NULL==(wbdata.vb=MYALLOC(VARBALANCE_CHUNK_SIZE*sizeof(*wbdata.vb)))){
		kbset.status=NOMEMORY;
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return;
	}
	wbdata.size=VARBALANCE_CHUNK_SIZE;

	/* Allocate memory for predicate multiset data used for subsumption pruning. */
	if (NULL==(predicateset=MYALLOC(ii=2*(numpreds+1)*sizeof(predicateset[0])))){
		kbset.status=NOMEMORY;
		MYFREE(wbdata.vb);
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return;
	}
	memset(predicateset,0,ii);
	sstimestamp=0;

	/* Set shuffle option and shuffle symbols if appropriate. */
	if (procnb!=(active_cores-1)){
		kbset.opts.shuffle=1;
		if (0!=ShuffleSymbols()){
			MYFREE(wbdata.vb);
			MYFREE(predicateset);
			return;
		}
	} else {
		kbset.opts.shuffle=0;
	}

	/* Initialize file descriptor to read number of executed hardware instructions. */
	if (perfmon){
		memset(&perfevent,0,sizeof(perfevent));
		perfevent.type=PERF_TYPE_HARDWARE;
		perfevent.size=sizeof(perfevent);
		perfevent.config=PERF_COUNT_HW_INSTRUCTIONS;
		perfevent.disabled=1;
		perfevent.exclude_kernel=1;
		perfevent.exclude_hv=1;
		filedesc=syscall(SYS_perf_event_open,&perfevent,0,-1,-1,0);
	} else {
		filedesc=-1;
	}

	/* Set strategy and symbol precedence. */
	kbset.status=RUNNING;
	if (NOMEMORY==SetMTParams(strategy)){
		kbset.status=NOMEMORY;
		MYFREE(wbdata.vb);
		MYFREE(predicateset);
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		if (filedesc!=-1){
			close(filedesc);
		}
		return;
	}
	SetSymbolPrecedence();

	/* Initialize the number of formulas in the working KB with */
	/* the number of text formulas in the main KB. This way the */
	/* formula numbering in both kb's will be compatible after */
	/* loading the working KB with passive binary clauses in the */
	/* main KB. */
	kbset.nformulas=mainkb.nformulas;

	/* Load working KB with unprocessed binary clauses in the main KB. */
	for (ptr1=mainkb.frstunproc;ptr1!=NULL;ptr1=ptr1->next){

		/* Allocate storage for the copied clause. */
		if (NULL==(ptr2=MYALLOC(sizeof(cmprefix)))){
			kbset.status=NOMEMORY;
			break;
		}
		memcpy(ptr2,ptr1,sizeof(cmprefix));
		if (NULL==(ptr2->part2.bin=MYALLOC(binpsize+ptr1->part2.bin->size))){
			kbset.status=NOMEMORY;
			MYFREE(ptr2);
			break;
		}

		/* Copy binary part of clause and clean equality data. */
		memcpy(ptr2->part2.bin,ptr1->part2.bin,binpsize+ptr1->part2.bin->size);
		ptr2->part2.bin->oriented=0;
		ptr2->part2.bin->clause=ptr2;

		/* Split clause if possible. */
		if (NOMEMORY==(ii=SplitClause(ptr2,0))){
			kbset.status=NOMEMORY;
			MYFREE(ptr2->part2.bin);
			MYFREE(ptr2);
			break;
		}

		/* If clause was not split then add clause to KB as unprocessed. */
		if (ii==0){
			if (NOMEMORY==AddBinClause2KB(ptr2,&kbset,0)){
				kbset.status=NOMEMORY;
				MYFREE(ptr2->part2.bin);
				MYFREE(ptr2);
				break;
			}
		}

		/* Check timeout. */
		if (procctl->status==TIMEOUT){
			kbset.status=TIMEOUT;
			MYFREE(wbdata.vb);
			MYFREE(predicateset);
			if (filedesc!=-1){
				close(filedesc);
			}
			return;
		}
	}

	/* If a NOMEMORY condition was detected set process status and return. */
	if (kbset.status==NOMEMORY){
		MYFREE(wbdata.vb);
		MYFREE(predicateset);
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		if (filedesc!=-1){
			close(filedesc);
		}
		return;
	}

	/* Remember the instrlimit value and modify it according to */
	/* foftmfactor and usrtmfactor values. */
	orginstrlimit = instrlimit;
	instrlimit*=foftmfactor*usrtmfactor;

	/* Start instruction limit control. */
	myioctl(PERF_EVENT_IOC_RESET);
	myioctl(PERF_EVENT_IOC_ENABLE);
	gettimeofday(&kbset.time,NULL);

	/* Start process algorithm. */
	switch (kbset.opts.algorithm){
		case OTTER:
			Otter();
			break;
		case DISCOUNT:
			Discount();
			break;
		case UEQOTT:
			UEQOtter();
			break;
		case UEQDSC:
			UEQDscnt();
			break;
	}

	/* Disable instructions count. */
	gettimeofday(&kbset.endtime,NULL);
	myioctl(PERF_EVENT_IOC_DISABLE);

	/* Report results if problem was solved. The number of executed HW instructions */
	/* is readjusted according to foftmfactor and usrtmfactor values (see help */
	/* for TMFACTOR command). */
	dd=(kbset.endtime.tv_sec-kbset.time.tv_sec+((kbset.endtime.tv_usec-kbset.time.tv_usec)/1000000.0))/(foftmfactor*usrtmfactor);
	myread(&jj);
	jj/=(foftmfactor*usrtmfactor);
	if (kbset.status==UNSATISFIABLE){
		MYFREE(kbset.frstprfnode);
		#ifdef STAREXECTUNING
		sem_wait(&procctl->statistics);
		printf("TNGSTRATS SUCCESS =====> strategy=%lu, instructions=%lu, elapsed time=%.6f seconds\n",
				strategy,jj,dd);
		sem_post(&procctl->statistics);
		#else
		if (importfile!=NULL){
			if (NULL==(ptr5=strrchr(importfile,WPATHSEPARATOR))){
				ptr5=importfile;
			} else {
				ptr5++;
			}
		} else {
			ptr5="user_input";
		}
		sem_wait(&procctl->statistics);
		printf("%s strategy=%lu, instructions=%lu, elapsed time=%.6f seconds\n",ptr5,strategy,jj,dd);
		sem_post(&procctl->statistics);
		#endif
	} else {
		#ifdef STAREXECTUNING
		sem_wait(&procctl->statistics);
		printf("TNGSTRATS FAIL =====> strategy=%lu, instructions=%lu, elapsed time=%.6f seconds\n",
				strategy,jj,dd);
		sem_post(&procctl->statistics);
		#endif
	}

	/* Adjust process maximum used memory. */
	rsinfo=mallinfo2();
	jj=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
	if (jj>mainkb.prstats.memory){
		mainkb.prstats.memory=jj;
		mainkb.prstats.netmemory=rsinfo.uordblks+rsinfo.hblkhd;
	}

	/* Reinitialize working KB and reset statistics. */
	Initialize_hashcmp();
	Initialize_KB(&kbset);
	ResetStats(&kbset.prstats);

	/* Restore original instrlimit value just in case that */
	/* a new TMFACTOR command is issued in interactive mode. */
	instrlimit=orginstrlimit;

	/* Close hardware instructions file descriptor and return. */
	MYFREE(wbdata.vb);
	MYFREE(predicateset);
	if (filedesc!=-1){
		close(filedesc);
	}
	return;
} /* InferenceProcess2 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Main inference function for UEQ strategy tuning)  OCJ
 *
 *    This function is similar to InferenceProcessUEQ() function except
 *    for the following:
 *    - It calls a saturation algorithm only once and only for the
 *      user defined strategy.
 *    - If it is executed by a child process then the input problem
 *      is randomized and randomization is applied through the
 *      saturation process.
 *    - If the problem is solved then the value of the strategy
 *      variable and the number of used instructions are reported
 *      using procctl->statistics semaphore to prevent mixing outputs
 *      from different processes.
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
 *
 *--------------------------------------------------------------*/
void InferenceProcess2UEQ(void){
	auto cmprefix *trprfclause;                          /* Clause suitable for a trivial proof. */
	auto struct mallinfo2 rsinfo;                        /* For mallinfo2() calls */
	auto struct perf_event_attr perfevent;               /* Instruction count performance event */
	auto uint64_t orginstrlimit;                         /* Original instrlimit value. */
	auto cmprefix *ptr1,*ptr2;                           /* Auxiliary pointers */
	auto uint8_t *ptr3,*ptr4;                            /* Auxiliary pointers */
	auto uint64_t jj;                                    /* Auxiliary */
	auto float dd;                                       /* Auxiliary */
	#ifndef STAREXECTUNING
	auto char *ptr5;                                     /* Auxiliary for input file name. */
	#endif

	/* Allocate memory for the weight and variable balance global structure. */
	if (NULL==(wbdata.vb=MYALLOC(VARBALANCE_CHUNK_SIZE*sizeof(*wbdata.vb)))){
		kbset.status=NOMEMORY;
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return;
	}
	wbdata.size=VARBALANCE_CHUNK_SIZE;

	/* Set shuffle option and shuffle symbols if appropriate. */
	if (procnb!=(active_cores-1)){
		kbset.opts.shuffle=1;
		if (0!=ShuffleSymbols()){
			MYFREE(wbdata.vb);
			MYFREE(predicateset);
			return;
		}
	} else {
		kbset.opts.shuffle=0;
	}

	/* Initialize file descriptor to read number of executed hardware instructions. */
	if (perfmonstate==0){
		perfmon=0;
		perfmonrate=HWINSTRATE;
	}
	if (perfmon){
		memset(&perfevent,0,sizeof(perfevent));
		perfevent.type=PERF_TYPE_HARDWARE;
		perfevent.size=sizeof(perfevent);
		perfevent.config=PERF_COUNT_HW_INSTRUCTIONS;
		perfevent.disabled=1;
		perfevent.exclude_kernel=1;
		perfevent.exclude_hv=1;
		filedesc=syscall(SYS_perf_event_open,&perfevent,0,-1,-1,0);
	} else {
		filedesc=-1;
	}

	/* Set strategy and symbol precedence. */
	kbset.status=RUNNING;
	if (NOMEMORY==SetMTParams(strategy)){
		kbset.status=NOMEMORY;
		MYFREE(wbdata.vb);
		MYFREE(predicateset);
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		if (filedesc!=-1){
			close(filedesc);
		}
		return;
	}
	SetSymbolPrecedence();

	/* Initialize the number of formulas in the working KB with */
	/* the number of text formulas in the main KB. This way the */
	/* formula numbering in both kb's will be compatible after */
	/* loading the working KB with passive binary clauses in the */
	/* main KB. */
	kbset.nformulas=mainkb.nformulas;

	/* Algorithm is OTTER or DISCOUNT. */
	if ((kbset.opts.algorithm!=UEQOTT)&&(kbset.opts.algorithm!=UEQDSC)){

		/* Load working KB with unprocessed binary clauses in the main KB. */
		for (ptr1=mainkb.frstunproc;ptr1!=NULL;ptr1=ptr1->next){

			/* If the goal transformation definitions must be loaded then */
			/* don't load the ground unit inequality goal. */
			if (kbset.opts.goalxform){
				if (ptr1==ueqgoal){
					continue;
				}

			/* If the goal transformation definitions must not be loaded */
			/* leave the loop if ptr1 is a goal definition clause. */
			} else if (ptr1->inference==GOALDEFINITION){
				break;
			}

			/* Allocate storage for the copied clause. */
			if (NULL==(ptr2=MYALLOC(sizeof(cmprefix)))){
				kbset.status=NOMEMORY;
				break;
			}
			memcpy(ptr2,ptr1,sizeof(cmprefix));
			if (NULL==(ptr2->part2.bin=MYALLOC(binpsize+ptr1->part2.bin->size))){
				kbset.status=NOMEMORY;
				MYFREE(ptr2);
				break;
			}

			/* Copy binary part of clause and clean equality data. */
			memcpy(ptr2->part2.bin,ptr1->part2.bin,binpsize+ptr1->part2.bin->size);
			ptr2->part2.bin->oriented=0;
			ptr2->part2.bin->clause=ptr2;

			/* Add clause to KB as unprocessed. */
			if (NOMEMORY==AddBinClause2KBUEQ(ptr2,&kbset,0)){
				kbset.status=NOMEMORY;
				MYFREE(ptr2->part2.bin);
				MYFREE(ptr2);
				break;
			}

			/* Check timeout. */
			if (procctl->status==TIMEOUT){
				kbset.status=TIMEOUT;
				MYFREE(wbdata.vb);
				MYFREE(predicateset);
				if (filedesc!=-1){
					close(filedesc);
				}
				return;
			}
		}

		/* If a NOMEMORY condition was detected set process status and return. */
		if (kbset.status==NOMEMORY){
			MYFREE(wbdata.vb);
			MYFREE(predicateset);
			sem_wait(&procctl->procctl);
			if (procctl->status==RUNNING){
				procctl->status=NOMEMORY;
			}
			sem_post(&procctl->procctl);
			if (filedesc!=-1){
				close(filedesc);
			}
			return;
		}

	/* Algorithm is UEQOTT or UEQDSC. */
	} else {

		/* Load working KB with unprocessed binary clauses in the main KB. */
		trprfclause=NULL; /* No clause for trivial proof exists yet. */
		for (ptr1=mainkb.frstunproc;ptr1!=NULL;ptr1=ptr1->next){

			/* If the goal transformation definitions must be loaded then */
			/* don't load the ground unit inequality goal. */
			if (kbset.opts.goalxform){
				if (ptr1==ueqgoal){
					continue;
				}

			/* If the goal transformation definitions must not be loaded */
			/* leave the loop if ptr1 is a goal definition clause. */
			} else if (ptr1->inference==GOALDEFINITION){
				break;
			}

			/* Allocate storage for the copied clause. */
			if (NULL==(ptr2=MYALLOC(sizeof(cmprefix)))){
				kbset.status=NOMEMORY;
				break;
			}
			memcpy(ptr2,ptr1,sizeof(cmprefix));
			if (NULL==(ptr2->part2.bin=MYALLOC(binpsize+ptr1->part2.bin->size))){
				kbset.status=NOMEMORY;
				MYFREE(ptr2);
				break;
			}

			/* Copy binary part of clause and clean equality data. */
			memcpy(ptr2->part2.bin,ptr1->part2.bin,binpsize+ptr1->part2.bin->size);
			ptr2->part2.bin->oriented=0;
			ptr2->part2.bin->clause=ptr2;

			/* Add clause to KB as UNPROC. */
			if (NOMEMORY==AddUEQClause2KB(ptr2)){
				kbset.status=NOMEMORY;
				MYFREE(ptr2->part2.bin);
				MYFREE(ptr2);
				break;
			}

			/* Clause for trivial proof already exist. */
			if (trprfclause!=NULL){

				/* If we have a goal then build trivial proof and return. */
				if (kbset.negeq!=NULL){
					if (NOMEMORY==MakeTrivialProof(trprfclause)){
						kbset.status=NOMEMORY;
						break;
					} else {
						kbset.status=UNSATISFIABLE;
						MYFREE(wbdata.vb);
						MYFREE(predicateset);
						sem_wait(&procctl->procctl);
						if (procctl->status!=UNSATISFIABLE){
							procctl->status=UNSATISFIABLE;
							procctl->solvekb=procnb;
						}
						sem_post(&procctl->procctl);
						if (filedesc!=-1){
							close(filedesc);
						}
						return;
					}
				}

			/* Clause for trivial proof doesn't exist. */
			} else {

				/* Current clause is suitable for trivial proof because it is of */
				/* the form x ≐ t where x is a variable that is not in term t. */
				ptr3=NextItem(&ptr2->part2.bin->formula[0],IMMED);
				ptr4=NextItem(ptr3,OVERSUBTERMS);
				if (((VARIABLE&ptr3[0])&&(0==CheckVarInTerm2(ptr3,ptr4)))
						||((VARIABLE&ptr4[0])&&(0==CheckVarInTerm2(ptr4,ptr3)))){

					/* Set pointer to clause suitable for trivial proof. */
					trprfclause=ptr2;

					/* If we have a goal then build trivial proof and return. */
					if (kbset.negeq!=NULL){
						if (NOMEMORY==MakeTrivialProof(trprfclause)){
							kbset.status=NOMEMORY;
							break;
						} else {
							kbset.status=UNSATISFIABLE;
							MYFREE(wbdata.vb);
							MYFREE(predicateset);
							sem_wait(&procctl->procctl);
							if (procctl->status!=UNSATISFIABLE){
								procctl->status=UNSATISFIABLE;
								procctl->solvekb=procnb;
							}
							sem_post(&procctl->procctl);
							if (filedesc!=-1){
								close(filedesc);
							}
							return;
						}
					}
				}
			}

			/* Check timeout. */
			if (procctl->status==TIMEOUT){
				kbset.status=TIMEOUT;
				MYFREE(wbdata.vb);
				MYFREE(predicateset);
				if (filedesc!=-1){
					close(filedesc);
				}
				return;
			}
		}

		/* If a NOMEMORY condition was detected disable the time alarm, */
		/* set process status and return. */
		if (kbset.status==NOMEMORY){
			MYFREE(wbdata.vb);
			MYFREE(predicateset);
			sem_wait(&procctl->procctl);
			if (procctl->status==RUNNING){
				procctl->status=NOMEMORY;
			}
			sem_post(&procctl->procctl);
			if (filedesc!=-1){
				close(filedesc);
			}
			return;
		}
	}

	/* Remember the instrlimit value and modify it according to */
	/* foftmfactor and usrtmfactor values. */
	orginstrlimit = instrlimit;
	instrlimit*=ueqtmfactor*usrtmfactor;

	/* Start instruction limit control. */
	myioctl(PERF_EVENT_IOC_RESET);
	myioctl(PERF_EVENT_IOC_ENABLE);
	gettimeofday(&kbset.time,NULL);

	/* Start process algorithm. */
	switch (kbset.opts.algorithm){
		case OTTER:
			OtterUEQ();
			break;
		case DISCOUNT:
			DiscountUEQ();
			break;
		case UEQOTT:
			UEQOtter();
			break;
		case UEQDSC:
			UEQDscnt();
			break;
	}

	/* Disable instructions count. */
	gettimeofday(&kbset.endtime,NULL);
	myioctl(PERF_EVENT_IOC_DISABLE);

	/* Report results if problem was solved. The number of executed HW instructions */
	/* is readjusted according to foftmfactor and usrtmfactor values (see help */
	/* for TMFACTOR command). */
	dd=(kbset.endtime.tv_sec-kbset.time.tv_sec+((kbset.endtime.tv_usec-kbset.time.tv_usec)/1000000.0))/(ueqtmfactor*usrtmfactor);
	myread(&jj);
	jj/=(ueqtmfactor*usrtmfactor);
	if (kbset.status==UNSATISFIABLE){
		MYFREE(kbset.frstprfnode);
		#ifdef STAREXECTUNING
		sem_wait(&procctl->statistics);
		printf("TNGSTRATS SUCCESS =====> strategy=%lu, instructions=%lu, elapsed time=%.6f seconds\n",
				strategy,jj,dd);
		sem_post(&procctl->statistics);
		#else
		if (importfile!=NULL){
			if (NULL==(ptr5=strrchr(importfile,WPATHSEPARATOR))){
				ptr5=importfile;
			} else {
				ptr5++;
			}
		} else {
			ptr5="user_input";
		}
		sem_wait(&procctl->statistics);
		printf("%s strategy=%lu, instructions=%lu, elapsed time=%.6f seconds\n",ptr5,strategy,jj,dd);
		sem_post(&procctl->statistics);
		#endif
	} else {
		#ifdef STAREXECTUNING
		sem_wait(&procctl->statistics);
		printf("TNGSTRATS FAIL =====> strategy=%lu, instructions=%lu, elapsed time=%.6f seconds\n",
				strategy,jj,dd);
		sem_post(&procctl->statistics);
		#endif
	}

	/* Adjust process maximum used memory. */
	rsinfo=mallinfo2();
	jj=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
	if (jj>mainkb.prstats.memory){
		mainkb.prstats.memory=jj;
		mainkb.prstats.netmemory=rsinfo.uordblks+rsinfo.hblkhd;
	}

	/* Reinitialize working KB and reset statistics. */
	Initialize_hashcmp();
	Initialize_KB(&kbset);
	ResetStats(&kbset.prstats);

	/* Restore original instrlimit value just in case that */
	/* a new TMFACTOR command is issued in interactive mode. */
	instrlimit=orginstrlimit;

	/* Close hardware instructions file descriptor and return. */
	MYFREE(wbdata.vb);
	MYFREE(predicateset);
	if (filedesc!=-1){
		close(filedesc);
	}
	return;
} /* InferenceProcess2UEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Otter saturation algorithm)  OCJ
 *
 *    This function performs the Otter type saturation algorithm.
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
 *
 *--------------------------------------------------------------*/
void Otter(void){
	auto cmprefix *current;                              /* Selected current clause. */
	auto cmprefix *splitcl[3];                           /* Set of pointers to split clauses */
	auto cmprefix *auxcl;                                /* Pointer to auxiliary clauses */
	auto uint64_t instr1,instr2;                         /* For LRS time measurements and timeout checks */
	auto float reachcl;                                  /* Number reachable passive clauses */
	#if MEMCHECK == 3
	auto struct mallinfo2 rsinfo;                        /* For mallinfo2() calls */
	auto uint64_t mem2;                                  /* Current memory measurements */
	#endif
	#if MEMCHECK != 0
	auto uint64_t mem1;                                  /* Previous memory measurements */
	auto int32_t count,countlimit;                       /* Counter and limit for memory check cycles */
	#endif
	auto char *ptr1;                                     /* Auxiliary pointer */
	auto int32_t ii,jj,tt;                               /* Auxiliary */
	auto uint64_t hh;                                    /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	#endif

	/* Initialize instr1 and clause selection parameters. */
	instr1=0;
	kbset.selectcnt=0;

	/* Main loop. This loop is held as far as there are passive or unprocessed */
	/* clauses or SAT queue is not empty. */
	kbset.lrscnt=0;
	kbset.flags&=(~RETENTIONUSED);
	tt=1;
	#if MEMCHECK != 0
	count=-1;countlimit=1;
	mem1=0; /* Just to prevent compiler warnings */
	#if MEMCHECK == 3
	mem2=0; /* Just to prevent compiler warnings */
	#endif
	#endif
	while ((kbset.frstpassive!=NULL)||(kbset.frstunproc!=NULL)
			||(kbset.firstsatq!=NULL)){

		/* Memory limit check. */
		#if (MEMCHECK == 1) || (MEMCHECK == 2)
		count++;
		if (count==0){
			mem1=allcdmemory;
		} else if (count>=countlimit){
			count=0;
			if (maxmemory<=allcdmemory){
				kbset.status=NOMEMORY;
				return;
			}
			if (allcdmemory>mem1){
				countlimit=(((double)(maxmemory-allcdmemory))*countlimit)/(2*(allcdmemory-mem1));
			}
			mem1=allcdmemory;
			#if MEMCHECK == 2
			if (GetAvailableMemory()<=(MEMORYLIMIT1/2)){
				kbset.status=NOMEMORY;
				return;
			}
			#endif
		}
		#else
		#if MEMCHECK == 3
		count++;
		if (count==0){
			rsinfo=mallinfo2();
			mem1=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
		} else if (count>=countlimit){
			count=0;
			rsinfo=mallinfo2();
			mem2=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
			if (maxmemory<=mem2){
				kbset.status=NOMEMORY;
				return;
			}
			if (mem2>mem1){
				countlimit=(((double)(maxmemory-mem2))*countlimit)/(2*(mem2-mem1));
			}
			mem1=mem2;
		}
		#endif
		#endif

		/* SAT queue is not empty. */
		if (kbset.firstsatq!=NULL){

			/* Add SAT clauses in SAT queue to SAT solver and ask SAT solver */
			/* to solve. */
			if (0==(ii=SatQ2SatSolver(tt))){
				ii=SatSolver();
			}
			tt=0;
			switch (ii){

				/* TIMEOUT, SATQTIMEOUT, SLICETMOUT or NOMEMORY condition. */
				case NOMEMORY:
				case TIMEOUT:
				case SATQTIMEOUT:
				case SLICETMOUT:
					kbset.status=ii;
					return;
					break;

				/* Refutation found. */
				case 0:
					kbset.status=UNSATISFIABLE;
					if (procctl->prctype){
						sem_wait(&procctl->procctl);
						if (procctl->status!=UNSATISFIABLE){
							procctl->status=UNSATISFIABLE;
							procctl->solvekb=procnb;
						} else {
							MYFREE(kbset.frstprfnode);
						}
						sem_post(&procctl->procctl);
					}
					return;
					break;

				/* A SAT model is available. */
				default:
				case 1:
					break;
			}

			/* Apply changes of current interpretation. */
			if (0!=(ii=ApplyModelChanges())){
				kbset.status=ii;
				return;
			}
		}

		/* Debug. */
		#ifdef DEBUGCODE
		#ifdef VERBOSE
		if (active_cores==1){
			printf("--------------------------\nStart process of unprocessed clauses\n");
		}
		#endif
		#endif

		/* Loop through unprocessed clauses. */
		for (auxcl=SelectUnprocClause();auxcl!=NULL;auxcl=SelectUnprocClause()){

			/* Check hardware instructions limit. */
			if (instrlimit!=0xffffffffffffffff){
				myread(&hh);
				if (hh>=instrlimit){
					if (auxcl->part2.bin->ovly.asserts!=NULL){
						MYFREE(auxcl->part2.bin->ovly.asserts);
					}
					MYFREE(auxcl->part2.bin);
					MYFREE(auxcl);
					kbset.status=SLICETMOUT;
					return;
				}
			}

			/* Check empty clause. If the clause is empty then it has assertions so */
			/* add contradiction clause to SAT queue and iterate in the main loop. */
			/* Iterating in the main loop is achieved here by leaving the current */
			/* unprocessed clauses loop. */
			if (UNITEND==auxcl->part2.bin->formula[0]){
				if (0==(NUMBERED&auxcl->flags)){
					kbset.nformulas++;
					auxcl->number=kbset.nformulas;
					auxcl->flags|=NUMBERED;
				}
				if (NOMEMORY==BuildContrClause(auxcl)){
					kbset.status=NOMEMORY;
					MYFREE(auxcl->part2.bin->ovly.asserts);
					MYFREE(auxcl->part2.bin);
					MYFREE(auxcl);
					return;
				}
				break;
			}

			/* Split clause if possible. If clause is split then iterate */
			/* in the current loop. In case of NOMEMORY then clause must be */
			/* freed because it is unlinked. */
			if (NOMEMORY==(ii=SplitClause(auxcl,0))){
				kbset.status=NOMEMORY;
				if (auxcl->part2.bin->ovly.asserts!=NULL){
					MYFREE(auxcl->part2.bin->ovly.asserts);
				}
				MYFREE(auxcl->part2.bin);
				MYFREE(auxcl);
				return;
			}
			if (ii){
				continue;
			}

			/* Do simplifications to and from clause. */
			switch (ii=FwdSimplify(&auxcl,PASSIVE)){

				/* Check timeout and no memory. */
				case NOMEMORY:
					kbset.status=NOMEMORY;
					return;
					break;
				case TIMEOUT:
					kbset.status=TIMEOUT;
					return;
					break;

				/* Check empty clause without assertions inferred by simplification. */
				case 1:
					kbset.status=UNSATISFIABLE;
					if (procctl->prctype){
						sem_wait(&procctl->procctl);
						if (procctl->status!=UNSATISFIABLE){
							procctl->status=UNSATISFIABLE;
							procctl->solvekb=procnb;
						} else {
							MYFREE(kbset.frstprfnode);
						}
						sem_post(&procctl->procctl);
					}
					return;
					break;
			}

			/* Clause has not been locked or deleted by simplification. */
			if ((auxcl!=NULL)&&(0==(LOCKED&auxcl->flags))){

				/* Check empty clause. If the clause is empty then it has assertions so */
				/* add contradiction clause to SAT queue and iterate in the main loop. */
				/* Iterating in the main loop is achieved here by leaving the current */
				/* unprocessed clauses loop. */
				if (UNITEND==auxcl->part2.bin->formula[0]){
					if (0==(NUMBERED&auxcl->flags)){
						kbset.nformulas++;
						auxcl->number=kbset.nformulas;
						auxcl->flags|=NUMBERED;
					}
					if (NOMEMORY==BuildContrClause(auxcl)){
						kbset.status=NOMEMORY;
						MYFREE(auxcl->part2.bin->ovly.asserts);
						MYFREE(auxcl->part2.bin);
						MYFREE(auxcl);
						return;
					}
					break;
				}

				/* Retention test checking. */
				switch (Retained(auxcl)){

					/* Not enough memory. */
					case NOMEMORY:
						kbset.status=NOMEMORY;
						if (auxcl->part2.bin->ovly.asserts!=NULL){
							MYFREE(auxcl->part2.bin->ovly.asserts);
						}
						MYFREE(auxcl->part2.bin);
						MYFREE(auxcl);
						return;
						break;

					/* Clause is retained. */
					case 1:

						/* Perform backward simplification. */
						jj=BckSimplify(auxcl,PASSIVE);
						if ((jj==NOMEMORY)||(jj==TIMEOUT)){
							kbset.status=jj;
							if (auxcl->part2.bin->ovly.asserts!=NULL){
								MYFREE(auxcl->part2.bin->ovly.asserts);
							}
							MYFREE(auxcl->part2.bin);
							MYFREE(auxcl);
							return;
						}

						/* Add clause to passive clauses.*/
						auxcl->flags&=(~(ACTIVE|PASSIVE|UNPROC|LOCKED));
						auxcl->flags|=PASSIVE;
						if (0!=(ii=AddBinClause2KB(auxcl,&kbset,SELECT))){
							kbset.status=ii;
							if (auxcl->part2.bin->ovly.asserts!=NULL){
								MYFREE(auxcl->part2.bin->ovly.asserts);
							}
							MYFREE(auxcl->part2.bin);
							MYFREE(auxcl);
							return;
						}

						/* If BckSimplify() produced an empty clause without assertions */
						/* then assign a number to the empty clause and return. */
						if (jj){
							kbset.status=UNSATISFIABLE;
							kbset.nformulas++;
							kbset.frstprfnode->ptr.Aclause->number=kbset.nformulas;
							kbset.frstprfnode->ptr.Aclause->flags|=NUMBERED;
							if (procctl->prctype){
								sem_wait(&procctl->procctl);
								if (procctl->status!=UNSATISFIABLE){
									procctl->status=UNSATISFIABLE;
									procctl->solvekb=procnb;
								} else {
									MYFREE(kbset.frstprfnode);
								}
								sem_post(&procctl->procctl);
							}
							return;
						}
						break;
				}
			}
		}

		/* Disposing of clauses in disposal queue. */
		for (auxcl=kbset.lastdisp,ii=0;auxcl!=NULL;auxcl=kbset.lastdisp,ii++){

			/* Clause is not locked. Dispose of clause. */
			if (0==(LOCKED&auxcl->flags)){
				if (NULL==(ptr1=Decompile(auxcl->part2.bin))){
					kbset.status=NOMEMORY;
					return;
				}
				kbset.lastdisp=auxcl->prevdisp;
				auxcl->prevdisp=NULL;
				auxcl->flags&=(~INDISPOSALQUEUE);
				if (NOMEMORY==Unlink(auxcl,&kbset)){
					kbset.status=NOMEMORY;
					return;
				}
				if (auxcl->part2.bin->ovly.asserts!=NULL){
					MYFREE(auxcl->part2.bin->ovly.asserts);
				}
				MYFREE(auxcl->part2.bin);
				auxcl->part2.text=ptr1;
				AddTxtFormula2KB(auxcl,auxcl->inference,&kbset);

			/* If the clause is marked as LOCKED then there are locking */
			/* assertions. Unlink the clause from current queue and add */
			/* it to the LOCKED queue. If a NOMEMORY condition arises */
			/* after the clause is unlinked then it is completely deleted */
			/* because it cannot be deleted thereafter as it is unlinked. */
			} else {
				kbset.lastdisp=auxcl->prevdisp;
				auxcl->prevdisp=NULL;
				auxcl->flags&=(~(INDISPOSALQUEUE|LOCKED));
				if (NOMEMORY==Unlink(auxcl,&kbset)){
					kbset.status=NOMEMORY;
					return;
				}
				auxcl->flags&=(~(ACTIVE|PASSIVE|UNPROC|LOCKED));
				auxcl->flags|=LOCKED;
				if (NOMEMORY==AddBinClause2KB(auxcl,&kbset,0)){
					MYFREE(auxcl->part2.bin->lockasserts);
					if (NULL!=auxcl->part2.bin->ovly.asserts){
						MYFREE(auxcl->part2.bin->ovly.asserts);
					}
					MYFREE(auxcl->part2.bin);
					MYFREE(auxcl);
					kbset.status=NOMEMORY;
					return;
				}
			}

			/* Check timeout and solution by other process. */
	 		if (ii>=TIMEOUTCCLDSP){
				if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
					kbset.status=UNKNOWN;
					return;
				}
				if (procctl->status==TIMEOUT){
					kbset.status=TIMEOUT;
					return;
				}
				if (procctl->status==NOMEMORY){
					kbset.status=NOMEMORY;
					return;
				}
				ii=-1;
	 		}
		}

		/* If SAT queue is not empty then iterate in the main loop. */
		/* It is important to place this piece of code after processing */
		/* the disposal queue to ensure that clauses marked as locked are */
		/* effectively locked. */
		if (kbset.firstsatq!=NULL){
			continue;
		}

		/* LRS has been selected. */
		if (kbset.opts.mxwghtopt&(MXWEIGHTLRS|MXWEIGHTLRSOTT)){

			/* Threshold has been reached. */
			kbset.lrscnt++;
			if (kbset.lrscnt>=LRSTHRESHOLD){

				/* Get number of executed instructions for last cycle and number of */
				/* executable instructions left. Compute number of reachable passive */
				/* clauses. Update current executed instructions for next cycle*/
				myread(&instr2);
				reachcl=(double)LRSCOEF*LRSTHRESHOLD*(instrlimit-instr2)/(instr2-instr1);
				instr1=instr2;

				/* Not all passive clauses are reachable at the current rate. */
				if (reachcl<kbset.prstats.nbpassive){
					if (0!=(ii=LRSClean((int32_t)reachcl))){
						kbset.status=ii;
						return;
					}
				}

				/* Update LRS count. */
				kbset.lrscnt=0;
			}
		}

		/* Select current clause and unlink it from KB. */
		if (NULL==(current=SelectClause())){
			break;
		}
		if (NOMEMORY==Unlink(current,&kbset)){
			kbset.status=NOMEMORY;
			return;
		}

		/* Debug. */
		#ifdef DEBUGCODE
		if (current->number==dbgnb){
			ptr10=NULL;
		}
		ptr10=Decompile(current->part2.bin);
		#ifdef VERBOSE
		if (active_cores==1){
			printf("--------------------------\nSelected clause [%ld/%ld]:\n%s\n",
					current->number,kbset.nformulas,ptr10);
		}
		#endif
		MYFREE(ptr10);
		#endif

		/* Add current clause to ACTIVE queue. In case of no memory the clause must */
		/* be freed because it is unlinked and will not be freed thereafter. */
		splitcl[0]=current;
		splitcl[1]=NULL;
		current->flags&=(~(ACTIVE|PASSIVE|UNPROC|LOCKED));
		current->flags|=ACTIVE;
		if (pbmtype==3){
			ii=SplitNAdd2Active(&splitcl[0]);
		} else {
			ii=AddBinClause2KB(current,&kbset,SELECT);
		}
		if (ii){
			if (0==(TEXTFORMULA&current->flags)){
				if (current->part2.bin->ovly.asserts!=NULL){
					MYFREE(current->part2.bin->ovly.asserts);
				}
			}
			MYFREE(current->part2.bin);
			MYFREE(current);
			kbset.status=ii;
			return;
		}

		/* Perform deduction inferences. */
		for (ii=0;(ii<3)&&(splitcl[ii]!=NULL);ii++){
			Infere(splitcl[ii]);
			switch (kbset.status){
				case NOMEMORY:
					return;
					break;
				case UNSATISFIABLE:
					if (procctl->prctype){
						sem_wait(&procctl->procctl);
						if (procctl->status!=UNSATISFIABLE){
							procctl->status=UNSATISFIABLE;
							procctl->solvekb=procnb;
						} else {
							MYFREE(kbset.frstprfnode);
						}
						sem_post(&procctl->procctl);
					}
				/* No break */
				case UNKNOWN:
				case TIMEOUT:
				case SLICETMOUT:
					return;
					break;
			}
		}
	}

	/* If we are here then theory is SATISFIABLE if the algorithm used */
	/* is complete or TIMEOUT/UNKNOWN otherwise. */
	#ifdef REDUNDANCYCOMPLETE
	if ((0==(kbset.flags&RETENTIONUSED))&&(glblopts.satmode!=0)&&((0==(prproflags&CLFILTERINGPRUNNED))
			||(((0==((UNITEQUALITY|NONUNITEQUALITY)&pbmflags)))&&(0==(CLFILTERINGLIMITREACHED&prproflags))))
			&&(COMPLETESELECTIONS&kbset.opts.select)&&(lkhdcomplete)
			&&(kbset.opts.termord==STANDARD)&&(kbset.opts.litord==STANDARD)
			&&((0==((UNITEQUALITY|NONUNITEQUALITY)&pbmflags))||(kbset.opts.demodulation==DEMODOFF)
					||(kbset.opts.demodulation==DEMODCPL1)||(kbset.opts.demodulation==DEMODCPL2)
					||(kbset.opts.demodulation==DEMODCPL1ORNT)||(kbset.opts.demodulation==DEMODCPL2ORNT))){
	#else
	if ((0==(kbset.flags&RETENTIONUSED))&&(glblopts.satmode!=0)&&((0==(prproflags&CLFILTERINGPRUNNED))
			||(((0==((UNITEQUALITY|NONUNITEQUALITY)&pbmflags)))&&(0==(CLFILTERINGLIMITREACHED&prproflags))))
			&&((COMPLETESELECTIONS)&kbset.opts.select)&&(lkhdcomplete)
			&&(kbset.opts.termord==STANDARD)&&(kbset.opts.litord==STANDARD)
			&&((0==((UNITEQUALITY|NONUNITEQUALITY)&pbmflags))||(kbset.opts.demodulation==DEMODOFF))){
	#endif
		kbset.status=SATISFIABLE;
		if (procctl->prctype){
			sem_wait(&procctl->procctl);
			if ((procctl->status!=SATISFIABLE)&&(procctl->status!=UNSATISFIABLE)){
				procctl->status=SATISFIABLE;
				procctl->solvekb=procnb;
			}
			sem_post(&procctl->procctl);
		}
	} else {
		if (procctl->status==TIMEOUT){
			kbset.status=TIMEOUT;
		} else {
			kbset.status=UNKNOWN;
		}
	}

	return;
} /* Otter */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Otter saturation algorithm for unit equality)  OCJ
 *
 *    This function performs the Otter type saturation algorithm
 *    for problems type 8 (unit equality).
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
 *
 *--------------------------------------------------------------*/
void OtterUEQ(void){
	auto cmprefix *current;                              /* Selected current clause. */
	auto cmprefix *splitcl[3];                           /* Set of pointers to split clauses */
	auto cmprefix *auxcl;                                /* Pointer to auxiliary clauses */
	auto uint64_t instr1,instr2;                         /* For LRS time measurements and timeout checks */
	auto float reachcl;                                  /* Number reachable passive clauses */
	#if MEMCHECK == 3
	auto struct mallinfo2 rsinfo;                        /* For mallinfo2() calls */
	auto uint64_t mem2;                                  /* Current memory measurements */
	#endif
	#if MEMCHECK != 0
	auto uint64_t mem1;                                  /* Previous memory measurements */
	auto int32_t count,countlimit;                       /* Counter and limit for memory check cycles */
	#endif
	auto char *ptr1;                                     /* Auxiliary pointer */
	auto int32_t ii,jj;                                  /* Auxiliary */
	auto uint64_t hh;                                    /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	#endif

	/* Initialize instr1 and clause selection parameters. */
	instr1=0;
	kbset.selectcnt=0;

	/* Main loop. This loop is held as far as there are passive or unprocessed */
	/* clauses or SAT queue is not empty. */
	kbset.lrscnt=0;
	kbset.flags&=(~RETENTIONUSED);
	#if MEMCHECK != 0
	count=-1;countlimit=1;
	mem1=0; /* Just to prevent compiler warnings */
	#if MEMCHECK == 3
	mem2=0; /* Just to prevent compiler warnings */
	#endif
	#endif
	while ((kbset.frstpassive!=NULL)||(kbset.frstunproc!=NULL)){

		/* Memory limit check. */
		#if (MEMCHECK == 1) || (MEMCHECK == 2)
		count++;
		if (count==0){
			mem1=allcdmemory;
		} else if (count>=countlimit){
			count=0;
			if (maxmemory<=allcdmemory){
				kbset.status=NOMEMORY;
				return;
			}
			if (allcdmemory>mem1){
				countlimit=(((double)(maxmemory-allcdmemory))*countlimit)/(2*(allcdmemory-mem1));
			}
			mem1=allcdmemory;
			#if MEMCHECK == 2
			if (GetAvailableMemory()<=(MEMORYLIMIT1/2)){
				kbset.status=NOMEMORY;
				return;
			}
			#endif
		}
		#else
		#if MEMCHECK == 3
		count++;
		if (count==0){
			rsinfo=mallinfo2();
			mem1=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
		} else if (count>=countlimit){
			count=0;
			rsinfo=mallinfo2();
			mem2=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
			if (maxmemory<=mem2){
				kbset.status=NOMEMORY;
				return;
			}
			if (mem2>mem1){
				countlimit=(((double)(maxmemory-mem2))*countlimit)/(2*(mem2-mem1));
			}
			mem1=mem2;
		}
		#endif
		#endif

		/* Debug. */
		#ifdef DEBUGCODE
		#ifdef VERBOSE
		if (active_cores==1){
			printf("--------------------------\nStart process of unprocessed clauses\n");
		}
		#endif
		#endif

		/* Loop through unprocessed clauses. */
		for (auxcl=SelectUnprocClause();auxcl!=NULL;auxcl=SelectUnprocClause()){

			/* Check hardware instructions limit. */
			if (instrlimit!=0xffffffffffffffff){
				myread(&hh);
				if (hh>=instrlimit){
					MYFREE(auxcl->part2.bin);
					MYFREE(auxcl);
					kbset.status=SLICETMOUT;
					return;
				}
			}

			/* Do simplifications to and from clause. */
			switch (ii=FwdSimplifyUEQ(&auxcl,PASSIVE)){

				/* Check timeout and no memory. */
				case NOMEMORY:
					kbset.status=NOMEMORY;
					return;
					break;
				case TIMEOUT:
					kbset.status=TIMEOUT;
					return;
					break;

				/* Check empty clause. */
				case 1:
					kbset.status=UNSATISFIABLE;
					if (procctl->prctype){
						sem_wait(&procctl->procctl);
						if (procctl->status!=UNSATISFIABLE){
							procctl->status=UNSATISFIABLE;
							procctl->solvekb=procnb;
						} else {
							MYFREE(kbset.frstprfnode);
						}
						sem_post(&procctl->procctl);
					}
					return;
					break;
			}

			/* Clause has not been deleted by simplification. */
			if (auxcl!=NULL){

				/* Retention test checking. */
				switch (Retained(auxcl)){

					/* Not enough memory. */
					case NOMEMORY:
						kbset.status=NOMEMORY;
						MYFREE(auxcl->part2.bin);
						MYFREE(auxcl);
						return;
						break;

					/* Clause is retained. */
					case 1:

						/* Perform backward simplification. */
						jj=BckSimplifyUEQ(auxcl,PASSIVE);
						if ((jj==NOMEMORY)||(jj==TIMEOUT)){
							kbset.status=jj;
							if (auxcl->part2.bin->ovly.asserts!=NULL){
								MYFREE(auxcl->part2.bin->ovly.asserts);
							}
							MYFREE(auxcl->part2.bin);
							MYFREE(auxcl);
							return;
						}

						/* Add clause to passive clauses.*/
						auxcl->flags&=(~(ACTIVE|PASSIVE|UNPROC|LOCKED));
						auxcl->flags|=PASSIVE;
						if (0!=(ii=AddBinClause2KBUEQ(auxcl,&kbset,SELECT))){
							kbset.status=ii;
							if (auxcl->part2.bin->ovly.asserts!=NULL){
								MYFREE(auxcl->part2.bin->ovly.asserts);
							}
							MYFREE(auxcl->part2.bin);
							MYFREE(auxcl);
							return;
						}

						/* If BckSimplify() produced an empty clause then assign a number */
						/* to the empty clause and return. */
						if (jj){
							kbset.status=UNSATISFIABLE;
							kbset.nformulas++;
							kbset.frstprfnode->ptr.Aclause->number=kbset.nformulas;
							kbset.frstprfnode->ptr.Aclause->flags|=NUMBERED;
							if (procctl->prctype){
								sem_wait(&procctl->procctl);
								if (procctl->status!=UNSATISFIABLE){
									procctl->status=UNSATISFIABLE;
									procctl->solvekb=procnb;
								} else {
									MYFREE(kbset.frstprfnode);
								}
								sem_post(&procctl->procctl);
							}
							return;
						}
						break;
				}
			}
		}

		/* Disposing of clauses in disposal queue. */
		for (auxcl=kbset.lastdisp,ii=0;auxcl!=NULL;auxcl=kbset.lastdisp,ii++){

			/* Dispose of clause. */
			if (NULL==(ptr1=Decompile(auxcl->part2.bin))){
				kbset.status=NOMEMORY;
				return;
			}
			kbset.lastdisp=auxcl->prevdisp;
			auxcl->prevdisp=NULL;
			auxcl->flags&=(~INDISPOSALQUEUE);
			if (NOMEMORY==UnlinkUEQ(auxcl,&kbset)){
				kbset.status=NOMEMORY;
				return;
			}
			MYFREE(auxcl->part2.bin);
			auxcl->part2.text=ptr1;
			AddTxtFormula2KB(auxcl,auxcl->inference,&kbset);

			/* Check timeout and solution by other process. */
			if (ii>=TIMEOUTCCLDSP){
				if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
					kbset.status=UNKNOWN;
					return;
				}
				if (procctl->status==TIMEOUT){
					kbset.status=TIMEOUT;
					return;
				}
				if (procctl->status==NOMEMORY){
					kbset.status=NOMEMORY;
					return;
				}
				ii=-1;
			}
		}

		/* LRS has been selected. */
		if (kbset.opts.mxwghtopt&(MXWEIGHTLRS|MXWEIGHTLRSOTT)){

			/* Threshold has been reached. */
			kbset.lrscnt++;
			if (kbset.lrscnt>=LRSTHRESHOLD){

				/* Get number of executed instructions for last cycle and number of */
				/* executable instructions left. Compute number of reachable passive */
				/* clauses. Update current executed instructions for next cycle*/
				myread(&instr2);
				reachcl=(double)LRSCOEF*LRSTHRESHOLD*(instrlimit-instr2)/(instr2-instr1);
				instr1=instr2;

				/* Not all passive clauses are reachable at the current rate. */
				if (reachcl<kbset.prstats.nbpassive){
					if (0!=(ii=LRSClean((int32_t)reachcl))){
						kbset.status=ii;
						return;
					}
				}

				/* Update LRS count. */
				kbset.lrscnt=0;
			}
		}

		/* Select current clause and unlink it from KB. */
		if (NULL==(current=SelectClause())){
			break;
		}
		if (NOMEMORY==UnlinkUEQ(current,&kbset)){
			kbset.status=NOMEMORY;
			return;
		}

		/* Debug. */
		#ifdef DEBUGCODE
		if (current->number==dbgnb){
			ptr10=NULL;
		}
		ptr10=Decompile(current->part2.bin);
		#ifdef VERBOSE
		if (active_cores==1){
			printf("--------------------------\nSelected clause [%ld/%ld]:\n%s\n",
					current->number,kbset.nformulas,ptr10);
		}
		#endif
		MYFREE(ptr10);
		#endif

		/* Add current clause to ACTIVE queue. In case of no memory the clause must */
		/* be freed because it is unlinked and will not be freed thereafter. */
		splitcl[0]=current;
		splitcl[1]=NULL;
		current->flags&=(~(ACTIVE|PASSIVE|UNPROC|LOCKED));
		current->flags|=ACTIVE;
		ii=SplitNAdd2Active(&splitcl[0]);
		if (ii){
			MYFREE(current->part2.bin);
			MYFREE(current);
			kbset.status=ii;
			return;
		}

		/* Perform deduction inferences. */
		for (ii=0;(ii<3)&&(splitcl[ii]!=NULL);ii++){
			InfereUEQ(splitcl[ii]);
			switch (kbset.status){
				case NOMEMORY:
					return;
					break;
				case UNSATISFIABLE:
					if (procctl->prctype){
						sem_wait(&procctl->procctl);
						if (procctl->status!=UNSATISFIABLE){
							procctl->status=UNSATISFIABLE;
							procctl->solvekb=procnb;
						} else {
							MYFREE(kbset.frstprfnode);
						}
						sem_post(&procctl->procctl);
					}
				/* No break */
				case UNKNOWN:
				case TIMEOUT:
				case SLICETMOUT:
					return;
					break;
			}
		}
	}

	/* If we are here then theory is SATISFIABLE if the algorithm used */
	/* is complete or TIMEOUT/UNKNOWN otherwise. */
	#ifdef REDUNDANCYCOMPLETE
	if ((0==(kbset.flags&RETENTIONUSED))&&(glblopts.satmode!=0)&&((0==(prproflags&CLFILTERINGPRUNNED))
			||(((0==((UNITEQUALITY|NONUNITEQUALITY)&pbmflags)))&&(0==(CLFILTERINGLIMITREACHED&prproflags))))
			&&(COMPLETESELECTIONS&kbset.opts.select)&&(lkhdcomplete)
			&&(kbset.opts.termord==STANDARD)&&(kbset.opts.litord==STANDARD)
			&&((0==((UNITEQUALITY|NONUNITEQUALITY)&pbmflags))||(kbset.opts.demodulation==DEMODOFF)
					||(kbset.opts.demodulation==DEMODCPL1)||(kbset.opts.demodulation==DEMODCPL2)
					||(kbset.opts.demodulation==DEMODCPL1ORNT)||(kbset.opts.demodulation==DEMODCPL2ORNT))){
	#else
	if ((0==(kbset.flags&RETENTIONUSED))&&(glblopts.satmode!=0)&&((0==(prproflags&CLFILTERINGPRUNNED))
			||(((0==((UNITEQUALITY|NONUNITEQUALITY)&pbmflags)))&&(0==(CLFILTERINGLIMITREACHED&prproflags))))
			&&((COMPLETESELECTIONS)&kbset.opts.select)&&(lkhdcomplete)
			&&(kbset.opts.termord==STANDARD)&&(kbset.opts.litord==STANDARD)
			&&((0==((UNITEQUALITY|NONUNITEQUALITY)&pbmflags))||(kbset.opts.demodulation==DEMODOFF))){
	#endif
		kbset.status=SATISFIABLE;
		if (procctl->prctype){
			sem_wait(&procctl->procctl);
			if ((procctl->status!=SATISFIABLE)&&(procctl->status!=UNSATISFIABLE)){
				procctl->status=SATISFIABLE;
				procctl->solvekb=procnb;
			}
			sem_post(&procctl->procctl);
		}
	} else {
		if (procctl->status==TIMEOUT){
			kbset.status=TIMEOUT;
		} else {
			kbset.status=UNKNOWN;
		}
	}

	return;
} /* OtterUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Discount saturation algorithm)  OCJ
 *
 *    This function performs the Discount type saturation algorithm.
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
 *
 *--------------------------------------------------------------*/
void Discount(void){
	auto cmprefix *current;                              /* Selected current clause. */
	auto cmprefix *splitcl[3];                           /* Set of pointers to split clauses */
	auto cmprefix *auxcl;                                /* Pointers to auxiliary clause */
	auto uint64_t instr1,instr2;                         /* For LRS time measurements and timeout checks */
	auto float reachcl;                                  /* Number reachable passive clauses */
	#if MEMCHECK == 3
	auto struct mallinfo2 rsinfo;                        /* For mallinfo2() calls */
	auto uint64_t mem2;                                  /* Current memory measurements */
	#endif
	#if MEMCHECK != 0
	auto uint64_t mem1;                                  /* Previous memory measurements */
	auto int32_t count,countlimit;                       /* Counter and limit for memory check cycles */
	#endif
	auto char *ptr1;                                     /* Auxiliary pointer */
	auto int32_t ii,jj,kk,tt;                            /* Auxiliary */
	auto uint64_t hh;                                    /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	#endif

	/* Initialize instr1 and clause selection parameters. */
	instr1=0;
	kbset.selectcnt=0;

	/* Main loop. */
	kbset.lrscnt=0;
	kbset.flags&=(~RETENTIONUSED);
	tt=1;
	#if MEMCHECK != 0
	count=-1;countlimit=1;
	mem1=0; /* Just to prevent compiler warnings */
	#if MEMCHECK == 3
	mem2=0; /* Just to prevent compiler warnings */
	#endif
	#endif
	while ((kbset.frstpassive!=NULL)||(kbset.frstunproc!=NULL)
			||(kbset.firstsatq!=NULL)){

		/* Memory limit check. */
		#if (MEMCHECK == 1) || (MEMCHECK == 2)
		count++;
		if (count==0){
			mem1=allcdmemory;
		} else if (count>=countlimit){
			count=0;
			if (maxmemory<=allcdmemory){
				kbset.status=NOMEMORY;
				return;
			}
			if (allcdmemory>mem1){
				countlimit=(((double)(maxmemory-allcdmemory))*countlimit)/(2*(allcdmemory-mem1));
			}
			mem1=allcdmemory;
			#if MEMCHECK == 2
			if (GetAvailableMemory()<=(MEMORYLIMIT1/2)){
				kbset.status=NOMEMORY;
				return;
			}
			#endif
		}
		#else
		#if MEMCHECK == 3
		count++;
		if (count==0){
			rsinfo=mallinfo2();
			mem1=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
		} else if (count>=countlimit){
			count=0;
			rsinfo=mallinfo2();
			mem2=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
			if (maxmemory<=mem2){
				kbset.status=NOMEMORY;
				return;
			}
			if (mem2>mem1){
				countlimit=(((double)(maxmemory-mem2))*countlimit)/(2*(mem2-mem1));
			}
			mem1=mem2;
		}
		#endif
		#endif

		/* SAT queue is not empty. */
		if (kbset.firstsatq!=NULL){

			/* Add SAT clauses in SAT queue to SAT solver and ask SAT solver */
			/* to solve. */
			if (0==(ii=SatQ2SatSolver(tt))){
				ii=SatSolver();
			}
			tt=0;
			switch (ii){

				/* TIMEOUT, SATQTIMEOUT, SLICETMOUT or NOMEMORY condition. */
				case NOMEMORY:
				case TIMEOUT:
				case SATQTIMEOUT:
				case SLICETMOUT:
					kbset.status=ii;
					return;
					break;

				/* Refutation found. */
				case 0:
					kbset.status=UNSATISFIABLE;
					if (procctl->prctype){
						sem_wait(&procctl->procctl);
						if (procctl->status!=UNSATISFIABLE){
							procctl->status=UNSATISFIABLE;
							procctl->solvekb=procnb;
						} else {
							MYFREE(kbset.frstprfnode);
						}
						sem_post(&procctl->procctl);
					}
					return;
					break;

				/* A SAT model is available. */
				default:
				case 1:
					break;
			}

			/* Apply changes of current interpretation. */
			if (0!=(ii=ApplyModelChanges())){
				kbset.status=ii;
				return;
			}
		}

		/* Debug. */
		#ifdef DEBUGCODE
		#ifdef VERBOSE
		if (active_cores==1){
			printf("--------------------------\nStart process of unprocessed clauses\n");
		}
		#endif
		#endif

		/* Loop through unprocessed clauses. */
		for (auxcl=SelectUnprocClause();auxcl!=NULL;auxcl=SelectUnprocClause()){

			/* Check hardware instructions limit. */
			if (instrlimit!=0xffffffffffffffff){
				myread(&hh);
				if (hh>=instrlimit){
					if (auxcl->part2.bin->ovly.asserts!=NULL){
						MYFREE(auxcl->part2.bin->ovly.asserts);
					}
					MYFREE(auxcl->part2.bin);
					MYFREE(auxcl);
					kbset.status=SLICETMOUT;
					return;
				}
			}

			/* Check empty clause. If the clause is empty then it has assertions so */
			/* add contradiction clause to SAT queue and iterate in the main loop. */
			/* Iterating in the main loop is achieved here by leaving the current */
			/* unprocessed clauses loop. */
			if (UNITEND==auxcl->part2.bin->formula[0]){
				if (0==(NUMBERED&auxcl->flags)){
					kbset.nformulas++;
					auxcl->number=kbset.nformulas;
					auxcl->flags|=NUMBERED;
				}
				if (NOMEMORY==BuildContrClause(auxcl)){
					kbset.status=NOMEMORY;
					MYFREE(auxcl->part2.bin->ovly.asserts);
					MYFREE(auxcl->part2.bin);
					MYFREE(auxcl);
					return;
				}
				break;
			}

			/* Split clause if possible. If clause is split then iterate */
			/* in the current loop. In case of NOMEMORY then clause must be */
			/* freed because it is unlinked. */
			if (NOMEMORY==(ii=SplitClause(auxcl,0))){
				kbset.status=NOMEMORY;
				if (auxcl->part2.bin->ovly.asserts!=NULL){
					MYFREE(auxcl->part2.bin->ovly.asserts);
				}
				MYFREE(auxcl->part2.bin);
				MYFREE(auxcl);
				return;
			}
			if (ii){
				continue;
			}

			/* Do simplifications to and from clause. */
			switch (ii=FwdSimplify(&auxcl,0)){

				/* Check timeout and no memory. */
				case NOMEMORY:
					kbset.status=NOMEMORY;
					return;
					break;
				case TIMEOUT:
					kbset.status=TIMEOUT;
					return;
					break;

				/* Check empty clause without assertions inferred by simplification. */
				case 1:
					kbset.status=UNSATISFIABLE;
					if (procctl->prctype){
						sem_wait(&procctl->procctl);
						if (procctl->status!=UNSATISFIABLE){
							procctl->status=UNSATISFIABLE;
							procctl->solvekb=procnb;
						} else {
							MYFREE(kbset.frstprfnode);
						}
						sem_post(&procctl->procctl);
					}
					return;
					break;
			}

			/* Clause has not been locked or deleted by simplification. */
			if ((auxcl!=NULL)&&(0==(LOCKED&auxcl->flags))){

				/* Check empty clause. If the clause is empty then it has assertions so */
				/* add contradiction clause to SAT queue and iterate in the main loop. */
				/* Iterating in the main loop is achieved here by leaving the current */
				/* unprocessed clauses loop. */
				if (UNITEND==auxcl->part2.bin->formula[0]){
					if (0==(NUMBERED&auxcl->flags)){
						kbset.nformulas++;
						auxcl->number=kbset.nformulas;
						auxcl->flags|=NUMBERED;
					}
					if (NOMEMORY==BuildContrClause(auxcl)){
						kbset.status=NOMEMORY;
						MYFREE(auxcl->part2.bin->ovly.asserts);
						MYFREE(auxcl->part2.bin);
						MYFREE(auxcl);
						return;
					}
					break;
				}

				/* Retention test checking. */
				switch (Retained(auxcl)){

					/* Not enough memory. */
					case NOMEMORY:
						kbset.status=NOMEMORY;
						if (auxcl->part2.bin->ovly.asserts!=NULL){
							MYFREE(auxcl->part2.bin->ovly.asserts);
						}
						MYFREE(auxcl->part2.bin);
						MYFREE(auxcl);
						return;
						break;

					/* Clause is retained. Add clause to passive clauses. */
					case 1:
						auxcl->flags&=(~(ACTIVE|PASSIVE|UNPROC|LOCKED));
						auxcl->flags|=PASSIVE;
						if (0!=(ii=AddBinClause2KB(auxcl,&kbset,SELECT))){
							kbset.status=ii;
							if (auxcl->part2.bin->ovly.asserts!=NULL){
								MYFREE(auxcl->part2.bin->ovly.asserts);
							}
							MYFREE(auxcl->part2.bin);
							MYFREE(auxcl);
							return;
						}
						break;
				}
			}
		}

		/* If SAT queue is not empty then iterate in the main loop. */
		/* There is no need to process disposal queue before the */
		/* iteration as the previous processing of unprocessed clauses */
		/* has not put any clause  in disposal queue for disposal or */
		/* locking. This is a difference with Otter() algorithm. */
		if (kbset.firstsatq!=NULL){
			continue;
		}

		/* LRS has been selected. */
		if (kbset.opts.mxwghtopt&(MXWEIGHTLRS|MXWEIGHTLRSDSC)){

			/* Threshold has been reached. */
			kbset.lrscnt++;
			if (kbset.lrscnt>=LRSTHRESHOLD){

				/* Get number of executed instructions for last cycle and number of */
				/* executable instructions left. Compute number of reachable passive */
				/* clauses. Update current executed instructions for next cycle*/
				myread(&instr2);
				reachcl=(double)LRSCOEF*LRSTHRESHOLD*(instrlimit-instr2)/(instr2-instr1);
				instr1=instr2;

				/* Not all passive clauses are reachable at the current rate. */
				if (reachcl<kbset.prstats.nbpassive){
					if (0!=(ii=LRSClean((int32_t)reachcl))){
						kbset.status=ii;
						return;
					}
				}

				/* Update LRS count. */
				kbset.lrscnt=0;
			}
		}

		/* Select current clause and unlink it from KB. */
		if (NULL==(current=SelectClause())){
			break;
		}
		if (NOMEMORY==Unlink(current,&kbset)){
			kbset.status=NOMEMORY;
			return;
		}

		/* Set oriented flag to zero. This is necessary because the clause */
		/* may be simplified in the FwdSimplify() process and then added again to */
		/* ACTIVE clauses in the Infere() process. */
		current->part2.bin->oriented=0;

		/* Debug. */
		#ifdef DEBUGCODE
		if (current->number==dbgnb){
			ptr10=NULL;
		}
		ptr10=Decompile(current->part2.bin);
		#ifdef VERBOSE
		if (active_cores==1){
			printf("--------------------------\nSelected clause [%ld/%ld]:\n%s\n",
					current->number,kbset.nformulas,ptr10);
		}
		#endif
		MYFREE(ptr10);
		#endif

		/* Simplify current clause by and to active clauses */
		/* but of course don't add it to PASSIVE queue. */
		/* Remember origin clause. */
		/* In case of early return the clause is freed by FwdSimplify() */
		/* or one of the functions called by FwdSimplify(). */
		switch (ii=FwdSimplify(&current,FROMPASSIVE)){

			/* Check timeout and no memory. */
			case NOMEMORY:
				kbset.status=NOMEMORY;
				return;
				break;
			case TIMEOUT:
				kbset.status=TIMEOUT;
				return;
				break;

			/* Check empty clause without assertions inferred by simplification. */
			case 1:
				kbset.status=UNSATISFIABLE;
				if (procctl->prctype){
					sem_wait(&procctl->procctl);
					if (procctl->status!=UNSATISFIABLE){
						procctl->status=UNSATISFIABLE;
						procctl->solvekb=procnb;
					} else {
						MYFREE(kbset.frstprfnode);
					}
					sem_post(&procctl->procctl);
				}
				return;
				break;
		}

		/* Current clause has not been locked or deleted by simplification. */
		if ((current!=NULL)&&(0==(LOCKED&current->flags))){

			/* An empty clause with assertions was inferred in current clause */
			/* simplification. Add contradiction clause to SAT queue and set */
			/* current to NULL so that no additional process is performed with it. */
			if (UNITEND==current->part2.bin->formula[0]){
				if (NOMEMORY==BuildContrClause(current)){
					kbset.status=NOMEMORY;
					MYFREE(current->part2.bin->ovly.asserts);
					MYFREE(current->part2.bin);
					MYFREE(current);
					return;
				}
				current=NULL;

			/* No empty clause with assertions was inferred in current clause */
			/* simplification. */
			} else {

				/* Perform backward simplification. */
				jj=BckSimplify(current,0);
				if ((jj==NOMEMORY)||(jj==TIMEOUT)){
					kbset.status=jj;
					if (current->part2.bin->ovly.asserts!=NULL){
						MYFREE(current->part2.bin->ovly.asserts);
					}
					MYFREE(current->part2.bin);
					MYFREE(current);
					return;
				}

				/* Add clause to ACTIVE queue. In case of no memory the clause must be */
				/* freed because it is unlinked and will not be freed thereafter. */
				splitcl[0]=current;
				splitcl[1]=NULL;
				current->flags&=(~(ACTIVE|PASSIVE|UNPROC|LOCKED));
				current->flags|=ACTIVE;
				if (pbmtype==3){
					ii=SplitNAdd2Active(&splitcl[0]);
				} else {
					ii=AddBinClause2KB(current,&kbset,SELECT);
				}
				if (ii){
					if (0==(TEXTFORMULA&current->flags)){
						if (current->part2.bin->ovly.asserts!=NULL){
							MYFREE(current->part2.bin->ovly.asserts);
						}
					}
					MYFREE(current->part2.bin);
					MYFREE(current);
					kbset.status=ii;
					return;
				}

				/* If BckSimplify() produced an empty clause without assertions */
				/* then assign a number to the empty clause and return. */
				if (jj){
					kbset.status=UNSATISFIABLE;
					kbset.nformulas++;
					kbset.frstprfnode->ptr.Aclause->number=kbset.nformulas;
					kbset.frstprfnode->ptr.Aclause->flags|=NUMBERED;
					if (procctl->prctype){
						sem_wait(&procctl->procctl);
						if (procctl->status!=UNSATISFIABLE){
							procctl->status=UNSATISFIABLE;
							procctl->solvekb=procnb;
						} else {
							MYFREE(kbset.frstprfnode);
						}
						sem_post(&procctl->procctl);
					}
					return;
				}
			}
		}

		/* Dispose of clauses in disposal queue. */
		for (auxcl=kbset.lastdisp,kk=0;auxcl!=NULL;auxcl=kbset.lastdisp,kk++){

			/* Clause is not locked. Dispose of clause. */
			if (0==(LOCKED&auxcl->flags)){
				if (NULL==(ptr1=Decompile(auxcl->part2.bin))){
					kbset.status=NOMEMORY;
					return;
				}
				kbset.lastdisp=auxcl->prevdisp;
				auxcl->prevdisp=NULL;
				auxcl->flags&=(~INDISPOSALQUEUE);
				if (NOMEMORY==Unlink(auxcl,&kbset)){
					kbset.status=NOMEMORY;
					return;
				}
				if (auxcl->part2.bin->ovly.asserts!=NULL){
					MYFREE(auxcl->part2.bin->ovly.asserts);
				}
				MYFREE(auxcl->part2.bin);
				auxcl->part2.text=ptr1;
				AddTxtFormula2KB(auxcl,auxcl->inference,&kbset);

			/* If the clause is marked as LOCKED then there are locking */
			/* assertions. Unlink the clause from current queue and add */
			/* it to the LOCKED queue. If a NOMEMORY condition arises */
			/* after the clause is unlinked then it is completely deleted */
			/* because it cannot be deleted thereafter as it is unlinked. */
			} else {
				kbset.lastdisp=auxcl->prevdisp;
				auxcl->prevdisp=NULL;
				auxcl->flags&=(~(INDISPOSALQUEUE|LOCKED));
				if (NOMEMORY==Unlink(auxcl,&kbset)){
					MYFREE(auxcl->part2.bin->lockasserts);
					kbset.status=NOMEMORY;
					return;
				}
				auxcl->flags&=(~(ACTIVE|PASSIVE|UNPROC|LOCKED));
				auxcl->flags|=LOCKED;
				if (NOMEMORY==AddBinClause2KB(auxcl,&kbset,0)){
					MYFREE(auxcl->part2.bin->lockasserts);
					if (NULL!=auxcl->part2.bin->ovly.asserts){
						MYFREE(auxcl->part2.bin->ovly.asserts);
					}
					MYFREE(auxcl->part2.bin);
					MYFREE(auxcl);
					kbset.status=NOMEMORY;
					return;
				}
			}

			/* Check timeout and solution by other process. */
			if (kk>=TIMEOUTCCLDSP){
				if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
					kbset.status=UNKNOWN;
					return;
				}
				if (procctl->status==TIMEOUT){
					kbset.status=TIMEOUT;
					return;
				}
				if (procctl->status==NOMEMORY){
					kbset.status=NOMEMORY;
					return;
				}
				kk=-1;
			}
		}

		/* Current clause has not been locked or deleted by simplification. */
		/* Perform deduction inferences. */
		if ((current!=NULL)&&(0==(LOCKED&current->flags))){
			for (ii=0;(ii<3)&&(splitcl[ii]!=NULL);ii++){
				Infere(splitcl[ii]);
				switch (kbset.status){
					case NOMEMORY:
						return;
						break;
					case UNSATISFIABLE:
						if (procctl->prctype){
							sem_wait(&procctl->procctl);
							if (procctl->status!=UNSATISFIABLE){
								procctl->status=UNSATISFIABLE;
								procctl->solvekb=procnb;
							} else {
								MYFREE(kbset.frstprfnode);
							}
							sem_post(&procctl->procctl);
						}
					/* No break */
					case UNKNOWN:
					case TIMEOUT:
					case SLICETMOUT:
						return;
						break;
				}
			}
		}
	}

	/* If we are here then theory is SATISFIABLE if the algorithm used */
	/* is complete or UNKNOWN otherwise. */
	#ifdef REDUNDANCYCOMPLETE
	if ((0==(kbset.flags&RETENTIONUSED))&&(glblopts.satmode!=0)&&((0==(prproflags&CLFILTERINGPRUNNED))
			||(((0==((UNITEQUALITY|NONUNITEQUALITY)&pbmflags)))&&(0==(CLFILTERINGLIMITREACHED&prproflags))))
			&&(COMPLETESELECTIONS&kbset.opts.select)&&(lkhdcomplete)
			&&(kbset.opts.termord==STANDARD)&&(kbset.opts.litord==STANDARD)
			&&((0==((UNITEQUALITY|NONUNITEQUALITY)&pbmflags))||(kbset.opts.demodulation==DEMODOFF)
					||(kbset.opts.demodulation==DEMODCPL1)||(kbset.opts.demodulation==DEMODCPL2)
					||(kbset.opts.demodulation==DEMODCPL1ORNT)||(kbset.opts.demodulation==DEMODCPL2ORNT))){
	#else
	if ((0==(kbset.flags&RETENTIONUSED))&&(glblopts.satmode!=0)&&((0==(prproflags&CLFILTERINGPRUNNED))
			||(((0==((UNITEQUALITY|NONUNITEQUALITY)&pbmflags)))&&(0==(CLFILTERINGLIMITREACHED&prproflags))))
			&&((COMPLETESELECTIONS)&kbset.opts.select)&&(lkhdcomplete)
			&&(kbset.opts.termord==STANDARD)&&(kbset.opts.litord==STANDARD)
			&&((0==((UNITEQUALITY|NONUNITEQUALITY)&pbmflags))||(kbset.opts.demodulation==DEMODOFF))){
	#endif
		kbset.status=SATISFIABLE;
		if (procctl->prctype){
			sem_wait(&procctl->procctl);
			if ((procctl->status!=SATISFIABLE)&&(procctl->status!=UNSATISFIABLE)){
				procctl->status=SATISFIABLE;
				procctl->solvekb=procnb;
			}
			sem_post(&procctl->procctl);
		}
	} else {
		if (procctl->status==TIMEOUT){
			kbset.status=TIMEOUT;
		} else {
			kbset.status=UNKNOWN;
		}
	}

	return;
} /* Discount */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Discount saturation algorithm for unit equality)  OCJ
 *
 *    This function performs the Discount type saturation algorithm
 *    for problems type 8 (unit equality).
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
 *
 *--------------------------------------------------------------*/
void DiscountUEQ(void){
	auto cmprefix *current;                              /* Selected current clause. */
	auto cmprefix *splitcl[3];                           /* Set of pointers to split clauses */
	auto cmprefix *auxcl;                                /* Pointers to auxiliary clause */
	auto uint64_t instr1,instr2;                         /* For LRS time measurements and timeout checks */
	auto float reachcl;                                  /* Number reachable passive clauses */
	#if MEMCHECK == 3
	auto struct mallinfo2 rsinfo;                        /* For mallinfo2() calls */
	auto uint64_t mem2;                                  /* Current memory measurements */
	#endif
	#if MEMCHECK != 0
	auto uint64_t mem1;                                  /* Previous memory measurements */
	auto int32_t count,countlimit;                       /* Counter and limit for memory check cycles */
	#endif
	auto char *ptr1;                                     /* Auxiliary pointer */
	auto int32_t ii,jj,kk;                               /* Auxiliary */
	auto uint64_t hh;                                    /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	#endif

	/* Initialize instr1 and clause selection parameters. */
	instr1=0;
	kbset.selectcnt=0;

	/* Main loop. */
	kbset.lrscnt=0;
	kbset.flags&=(~RETENTIONUSED);
	#if MEMCHECK != 0
	count=-1;countlimit=1;
	mem1=0; /* Just to prevent compiler warnings */
	#if MEMCHECK == 3
	mem2=0; /* Just to prevent compiler warnings */
	#endif
	#endif
	while ((kbset.frstpassive!=NULL)||(kbset.frstunproc!=NULL)){

		/* Memory limit check. */
		#if (MEMCHECK == 1) || (MEMCHECK == 2)
		count++;
		if (count==0){
			mem1=allcdmemory;
		} else if (count>=countlimit){
			count=0;
			if (maxmemory<=allcdmemory){
				kbset.status=NOMEMORY;
				return;
			}
			if (allcdmemory>mem1){
				countlimit=(((double)(maxmemory-allcdmemory))*countlimit)/(2*(allcdmemory-mem1));
			}
			mem1=allcdmemory;
			#if MEMCHECK == 2
			if (GetAvailableMemory()<=(MEMORYLIMIT1/2)){
				kbset.status=NOMEMORY;
				return;
			}
			#endif
		}
		#else
		#if MEMCHECK == 3
		count++;
		if (count==0){
			rsinfo=mallinfo2();
			mem1=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
		} else if (count>=countlimit){
			count=0;
			rsinfo=mallinfo2();
			mem2=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
			if (maxmemory<=mem2){
				kbset.status=NOMEMORY;
				return;
			}
			if (mem2>mem1){
				countlimit=(((double)(maxmemory-mem2))*countlimit)/(2*(mem2-mem1));
			}
			mem1=mem2;
		}
		#endif
		#endif

		/* Debug. */
		#ifdef DEBUGCODE
		#ifdef VERBOSE
		if (active_cores==1){
			printf("--------------------------\nStart process of unprocessed clauses\n");
		}
		#endif
		#endif

		/* Loop through unprocessed clauses. */
		for (auxcl=SelectUnprocClause();auxcl!=NULL;auxcl=SelectUnprocClause()){

			/* Check hardware instructions limit. */
			if (instrlimit!=0xffffffffffffffff){
				myread(&hh);
				if (hh>=instrlimit){
					MYFREE(auxcl->part2.bin);
					MYFREE(auxcl);
					kbset.status=SLICETMOUT;
					return;
				}
			}

			/* Do simplifications to and from clause. */
			switch (ii=FwdSimplifyUEQ(&auxcl,0)){

				/* Check timeout and no memory. */
				case NOMEMORY:
					kbset.status=NOMEMORY;
					return;
					break;
				case TIMEOUT:
					kbset.status=TIMEOUT;
					return;
					break;

				/* Check empty clause inferred by simplification. */
				case 1:
					kbset.status=UNSATISFIABLE;
					if (procctl->prctype){
						sem_wait(&procctl->procctl);
						if (procctl->status!=UNSATISFIABLE){
							procctl->status=UNSATISFIABLE;
							procctl->solvekb=procnb;
						} else {
							MYFREE(kbset.frstprfnode);
						}
						sem_post(&procctl->procctl);
					}
					return;
					break;
			}

			/* Clause has not been deleted by simplification. */
			if (auxcl!=NULL){

				/* Retention test checking. */
				switch (Retained(auxcl)){

					/* Not enough memory. */
					case NOMEMORY:
						kbset.status=NOMEMORY;
						MYFREE(auxcl->part2.bin);
						MYFREE(auxcl);
						return;
						break;

					/* Clause is retained. Add clause to passive clauses. */
					case 1:
						auxcl->flags&=(~(ACTIVE|PASSIVE|UNPROC|LOCKED));
						auxcl->flags|=PASSIVE;
						if (0!=(ii=AddBinClause2KBUEQ(auxcl,&kbset,SELECT))){
							kbset.status=ii;
							MYFREE(auxcl->part2.bin);
							MYFREE(auxcl);
							return;
						}
						break;
				}

			}
		}

		/* LRS has been selected. */
		if (kbset.opts.mxwghtopt&(MXWEIGHTLRS|MXWEIGHTLRSDSC)){

			/* Threshold has been reached. */
			kbset.lrscnt++;
			if (kbset.lrscnt>=LRSTHRESHOLD){

				/* Get number of executed instructions for last cycle and number of */
				/* executable instructions left. Compute number of reachable passive */
				/* clauses. Update current executed instructions for next cycle*/
				myread(&instr2);
				reachcl=(double)LRSCOEF*LRSTHRESHOLD*(instrlimit-instr2)/(instr2-instr1);
				instr1=instr2;

				/* Not all passive clauses are reachable at the current rate. */
				if (reachcl<kbset.prstats.nbpassive){
					if (0!=(ii=LRSClean((int32_t)reachcl))){
						kbset.status=ii;
						return;
					}
				}

				/* Update LRS count. */
				kbset.lrscnt=0;
			}
		}

		/* Select current clause and unlink it from KB. */
		if (NULL==(current=SelectClause())){
			break;
		}
		if (NOMEMORY==UnlinkUEQ(current,&kbset)){
			kbset.status=NOMEMORY;
			return;
		}

		/* Set oriented flag to zero. This is necessary because the clause */
		/* may be simplified in the FwdSimplify() process and then added again to */
		/* ACTIVE clauses in the Infere() process. */
		current->part2.bin->oriented=0;

		/* Debug. */
		#ifdef DEBUGCODE
		if (current->number==dbgnb){
			ptr10=NULL;
		}
		ptr10=Decompile(current->part2.bin);
		#ifdef VERBOSE
		if (active_cores==1){
			printf("--------------------------\nSelected clause [%ld/%ld]:\n%s\n",
					current->number,kbset.nformulas,ptr10);
		}
		#endif
		MYFREE(ptr10);
		#endif

		/* Simplify current clause by and to active clauses */
		/* but of course don't add it to PASSIVE queue. */
		/* Remember origin clause. */
		/* In case of early return the clause is freed by FwdSimplify() */
		/* or one of the functions called by FwdSimplify(). */
		switch (ii=FwdSimplifyUEQ(&current,FROMPASSIVE)){

			/* Check timeout and no memory. */
			case NOMEMORY:
				kbset.status=NOMEMORY;
				return;
				break;
			case TIMEOUT:
				kbset.status=TIMEOUT;
				return;
				break;

			/* Check empty clause inferred by simplification. */
			case 1:
				kbset.status=UNSATISFIABLE;
				if (procctl->prctype){
					sem_wait(&procctl->procctl);
					if (procctl->status!=UNSATISFIABLE){
						procctl->status=UNSATISFIABLE;
						procctl->solvekb=procnb;
					} else {
						MYFREE(kbset.frstprfnode);
					}
					sem_post(&procctl->procctl);
				}
				return;
				break;
		}

		/* Current clause has not been deleted by simplification. */
		if (current!=NULL){

			/* Perform backward simplification. */
			jj=BckSimplifyUEQ(current,0);
			if ((jj==NOMEMORY)||(jj==TIMEOUT)){
				kbset.status=jj;
				MYFREE(current->part2.bin);
				MYFREE(current);
				return;
			}

			/* Add clause to ACTIVE queue. In case of no memory the clause must be */
			/* freed because it is unlinked and will not be freed thereafter. */
			splitcl[0]=current;
			splitcl[1]=NULL;
			current->flags&=(~(ACTIVE|PASSIVE|UNPROC|LOCKED));
			current->flags|=ACTIVE;
			ii=SplitNAdd2Active(&splitcl[0]);
			if (ii){
				MYFREE(current->part2.bin);
				MYFREE(current);
				kbset.status=ii;
				return;
			}

			/* If BckSimplify() produced an empty clause then assign a number */
			/* to the empty clause and return. */
			if (jj){
				kbset.status=UNSATISFIABLE;
				kbset.nformulas++;
				kbset.frstprfnode->ptr.Aclause->number=kbset.nformulas;
				kbset.frstprfnode->ptr.Aclause->flags|=NUMBERED;
				if (procctl->prctype){
					sem_wait(&procctl->procctl);
					if (procctl->status!=UNSATISFIABLE){
						procctl->status=UNSATISFIABLE;
						procctl->solvekb=procnb;
					} else {
						MYFREE(kbset.frstprfnode);
					}
					sem_post(&procctl->procctl);
				}
				return;
			}
		}

		/* Dispose of clauses in disposal queue. */
		for (auxcl=kbset.lastdisp,kk=0;auxcl!=NULL;auxcl=kbset.lastdisp,kk++){

			/* Dispose of clause. */
			if (NULL==(ptr1=Decompile(auxcl->part2.bin))){
				kbset.status=NOMEMORY;
				return;
			}
			kbset.lastdisp=auxcl->prevdisp;
			auxcl->prevdisp=NULL;
			auxcl->flags&=(~INDISPOSALQUEUE);
			if (NOMEMORY==UnlinkUEQ(auxcl,&kbset)){
				kbset.status=NOMEMORY;
				return;
			}
			MYFREE(auxcl->part2.bin);
			auxcl->part2.text=ptr1;
			AddTxtFormula2KB(auxcl,auxcl->inference,&kbset);

			/* Check timeout and solution by other process. */
			if (kk>=TIMEOUTCCLDSP){
				if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
					kbset.status=UNKNOWN;
					return;
				}
				if (procctl->status==TIMEOUT){
					kbset.status=TIMEOUT;
					return;
				}
				if (procctl->status==NOMEMORY){
					kbset.status=NOMEMORY;
					return;
				}
				kk=-1;
			}
		}

		/* Current clause has not been deleted by simplification. */
		/* Perform deduction inferences. */
		if (current!=NULL){
			for (ii=0;(ii<3)&&(splitcl[ii]!=NULL);ii++){
				InfereUEQ(splitcl[ii]);
				switch (kbset.status){
					case NOMEMORY:
						return;
						break;
					case UNSATISFIABLE:
						if (procctl->prctype){
							sem_wait(&procctl->procctl);
							if (procctl->status!=UNSATISFIABLE){
								procctl->status=UNSATISFIABLE;
								procctl->solvekb=procnb;
							} else {
								MYFREE(kbset.frstprfnode);
							}
							sem_post(&procctl->procctl);
						}
					/* No break */
					case UNKNOWN:
					case TIMEOUT:
					case SLICETMOUT:
						return;
						break;
				}
			}
		}
	}

	/* If we are here then theory is SATISFIABLE if the algorithm used */
	/* is complete or UNKNOWN otherwise. */
	#ifdef REDUNDANCYCOMPLETE
	if ((0==(kbset.flags&RETENTIONUSED))&&(glblopts.satmode!=0)&&((0==(prproflags&CLFILTERINGPRUNNED))
			||(((0==((UNITEQUALITY|NONUNITEQUALITY)&pbmflags)))&&(0==(CLFILTERINGLIMITREACHED&prproflags))))
			&&(COMPLETESELECTIONS&kbset.opts.select)&&(lkhdcomplete)
			&&(kbset.opts.termord==STANDARD)&&(kbset.opts.litord==STANDARD)
			&&((0==((UNITEQUALITY|NONUNITEQUALITY)&pbmflags))||(kbset.opts.demodulation==DEMODOFF)
					||(kbset.opts.demodulation==DEMODCPL1)||(kbset.opts.demodulation==DEMODCPL2)
					||(kbset.opts.demodulation==DEMODCPL1ORNT)||(kbset.opts.demodulation==DEMODCPL2ORNT))){
	#else
	if ((0==(kbset.flags&RETENTIONUSED))&&(glblopts.satmode!=0)&&((0==(prproflags&CLFILTERINGPRUNNED))
			||(((0==((UNITEQUALITY|NONUNITEQUALITY)&pbmflags)))&&(0==(CLFILTERINGLIMITREACHED&prproflags))))
			&&((COMPLETESELECTIONS)&kbset.opts.select)&&(lkhdcomplete)
			&&(kbset.opts.termord==STANDARD)&&(kbset.opts.litord==STANDARD)
			&&((0==((UNITEQUALITY|NONUNITEQUALITY)&pbmflags))||(kbset.opts.demodulation==DEMODOFF))){
	#endif
		kbset.status=SATISFIABLE;
		if (procctl->prctype){
			sem_wait(&procctl->procctl);
			if ((procctl->status!=SATISFIABLE)&&(procctl->status!=UNSATISFIABLE)){
				procctl->status=SATISFIABLE;
				procctl->solvekb=procnb;
			}
			sem_post(&procctl->procctl);
		}
	} else {
		if (procctl->status==TIMEOUT){
			kbset.status=TIMEOUT;
		} else {
			kbset.status=UNKNOWN;
		}
	}

	return;
} /* DiscountUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Otter saturation algorithm for unfailing completion)  OCJ
 *
 *    This function performs the Otter type saturation algorithm
 *    for completion without failure strategies as described in document
 *    "Twee: An Equational Theorem Prover (System Description)"
 *    by Nicholas Smallbone.
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
 *
 *--------------------------------------------------------------*/
void UEQOtter(void){
	auto cmprefix *current;                              /* Selected current clause. */
	auto cmprefix *splitcl[3];                           /* Set of pointers to split clauses */
	auto cmprefix *auxcl,*auxcl1,*auxcl2,*auxcl3;        /* Pointers to auxiliary clause */
	auto uint64_t instr1,instr2;                         /* For LRS time measurements and timeout checks */
	auto float reachcl;                                  /* Number reachable passive clauses */
	#if MEMCHECK == 3
	auto struct mallinfo2 rsinfo;                        /* For mallinfo2() calls */
	auto uint64_t mem2;                                  /* Current memory measurements */
	#endif
	#if MEMCHECK != 0
	auto uint64_t mem1;                                  /* Previous memory measurements */
	auto int32_t count,countlimit;                       /* Counter and limit for memory check cycles */
	#endif
	auto char *ptr1;                                     /* Auxiliary pointer */
	auto int32_t ii,jj;                                  /* Auxiliary */
	auto uint64_t hh;                                    /* Auxiliary */

	/* Initialize instr1 and clause selection parameters. */
	instr1=0;
	kbset.selectcnt=0;

	/* Main loop. */
	kbset.lrscnt=0;
	kbset.flags&=(~RETENTIONUSED);
	#if MEMCHECK != 0
	count=-1;countlimit=1;
	mem1=0; /* Just to prevent compiler warnings */
	#if MEMCHECK == 3
	mem2=0; /* Just to prevent compiler warnings */
	#endif
	#endif
	while ((kbset.frstpassive!=NULL)||(kbset.frstunproc!=NULL)){

		/* Memory limit check. */
		#if (MEMCHECK == 1) || (MEMCHECK == 2)
		count++;
		if (count==0){
			mem1=allcdmemory;
		} else if (count>=countlimit){
			count=0;
			if (maxmemory<=allcdmemory){
				kbset.status=NOMEMORY;
				return;
			}
			if (allcdmemory>mem1){
				countlimit=(((double)(maxmemory-allcdmemory))*countlimit)/(2*(allcdmemory-mem1));
			}
			mem1=allcdmemory;
			#if MEMCHECK == 2
			if (GetAvailableMemory()<=(MEMORYLIMIT1/2)){
				kbset.status=NOMEMORY;
				return;
			}
			#endif
		}
		#else
		#if MEMCHECK == 3
		count++;
		if (count==0){
			rsinfo=mallinfo2();
			mem1=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
		} else if (count>=countlimit){
			count=0;
			rsinfo=mallinfo2();
			mem2=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
			if (maxmemory<=mem2){
				kbset.status=NOMEMORY;
				return;
			}
			if (mem2>mem1){
				countlimit=(((double)(maxmemory-mem2))*countlimit)/(2*(mem2-mem1));
			}
			mem1=mem2;
		}
		#endif
		#endif

		/* Loop through unprocessed clauses. */
		for (current=SelectUnprocClause();current!=NULL;current=SelectUnprocClause()){

			/* Check hardware instructions limit. */
			if (instrlimit!=0xffffffffffffffff){
				myread(&hh);
				if (hh>=instrlimit){
					MYFREE(current->part2.bin);
					MYFREE(current);
					kbset.status=SLICETMOUT;
					return;
				}
			}

			/* Do simplifications to and from clause. */
			auxcl=current;
			switch (ii=FwUEQSimplify(&current)){

				/* Timeout, memory and solution by other process. The clause */
				/* has been freed by FwUEQSimplify() function. */
				case NOMEMORY:
				case TIMEOUT:
				case UNKNOWN:
					kbset.status=ii;
					return;
					break;

				/* Empty clause inferred by simplification. */
				case 1:
					kbset.status=UNSATISFIABLE;
					if (procctl->prctype){
						sem_wait(&procctl->procctl);
						if (procctl->status!=UNSATISFIABLE){
							procctl->status=UNSATISFIABLE;
							procctl->solvekb=procnb;
						} else {
							MYFREE(kbset.frstprfnode);
						}
						sem_post(&procctl->procctl);
					}
					return;
					break;

				/* Tautology. The clause has been freed by FwUEQSimplify() function. */
				case 2:
					continue;
					break;
			}

			/* Retention test checking. */
			if (NOMEMORY==(ii=Retained(current))){
				kbset.status=NOMEMORY;
				MYFREE(current->part2.bin);
				MYFREE(current);
				return;
			} else if (ii!=1){
				continue;
			}

			/* Renumber clause variables. This is necessary because the clause is UNPROC */
			/* and its variables (including the top term) have not yet been renumbered */
			/* and therefore maxvarnb field has not been properly set. */
			if (NOMEMORY==RenumberVars2(current->part2.bin)){
				MYFREE(current->part2.bin->ovly.cptopterm);
				MYFREE(current->part2.bin);
				MYFREE(current);
				kbset.status=NOMEMORY;
				return;
			}

			/* Clause is retained. If current clause is positive and connected then */
			/* free current clause and all intermediate results and iterate. */
			if ((kbset.opts.connect)&&(0==(NEGATED&current->part2.bin->formula[0]))){
				switch (ii=IsCPConnected(current)){
					case NOMEMORY:
					case TIMEOUT:
					case UNKNOWN:
						MYFREE(current->part2.bin->ovly.cptopterm);
						MYFREE(current->part2.bin);
						MYFREE(current);
						kbset.status=ii;
						return;
						break;
					case 1:
						for (auxcl1=current,auxcl2=NULL;auxcl2!=auxcl;auxcl1=auxcl3){
							auxcl2=auxcl1;
							auxcl3=auxcl1->parent2;
							if (TEXTFORMULA&auxcl1->flags){
								UnlinkTxtFormula(auxcl1,&kbset);
								MYFREE(auxcl1->part2.text);
							} else {
								MYFREE(auxcl1->part2.bin->ovly.cptopterm);
								MYFREE(auxcl1->part2.bin);
							}
							MYFREE(auxcl1);
						}
						continue;
						break;
				}
			}

			/* Current clause is subsumed. If original current clause was inferred as */
			/* PARAMODULATION then free current clause and all intermediate results */
			/* and iterate, otherwise convert clause to text form. */
			switch (ii=FwUEQSubsum(current)){
				case NOMEMORY:
				case TIMEOUT:
				case UNKNOWN:
					MYFREE(current->part2.bin);
					MYFREE(current);
					kbset.status=ii;
					return;
					break;
				case 1:
					if (auxcl->inference==PARAMODULATION){
						for (auxcl1=current,auxcl2=NULL;auxcl2!=auxcl;auxcl1=auxcl3){
							auxcl2=auxcl1;
							auxcl3=auxcl1->parent2;
							if (TEXTFORMULA&auxcl1->flags){
								UnlinkTxtFormula(auxcl1,&kbset);
								MYFREE(auxcl1->part2.text);
							} else {
								MYFREE(auxcl1->part2.bin->ovly.cptopterm);
								MYFREE(auxcl1->part2.bin);
							}
							MYFREE(auxcl1);
						}
					} else {
						if (NULL==(ptr1=Decompile(current->part2.bin))){
							kbset.status=NOMEMORY;
							return;
						}
						MYFREE(current->part2.bin);
						current->part2.text=ptr1;
						AddTxtFormula2KB(current,auxcl->inference,&kbset);
					}
					continue;
					break;
			}

			/* Free critical pair top term as it is no longer necessary. */
			MYFREE(current->part2.bin->ovly.cptopterm);
			current->part2.bin->ovly.cptopterm=NULL;

			/* If ground joinability is enabled and current clause is positive and */
			/* ground joinable then add it to ground joinable set, call */
			/* BkUEQSubsum() and iterate. */
			if ((kbset.opts.grjoin)&&(0==(NEGATED&current->part2.bin->formula[0]))){
				switch (ii=IsGrJoinable(&current->part2.bin->formula[0],current->part2.bin->size,
						current->part2.bin->maxvarnb,0,1)){

					/* NOMEMORY, TIMEOUT, SLICETMOUT or RETURN. */
					case NOMEMORY:
					case TIMEOUT:
					case UNKNOWN:
					case SLICETMOUT:
						MYFREE(current->part2.bin);
						MYFREE(current);
						kbset.status=ii;
						return;
						break;

					/* Clause is ground joinable. */
					case 1:

						/* Add clause to ground joinable clauses set. */
						current->flags&=(~(TEXTFORMULA|PASSIVE|ACTIVE|UNPROC));
						current->flags|=GROUNDJOINABLE;
						if (NOMEMORY==AddUEQClause2KB(current)){
							kbset.status=NOMEMORY;
							MYFREE(current->part2.bin);
							MYFREE(current);
							return;
						}

						/* Perform backward subsumption. */
						if (0!=(ii=BkUEQSubsum(current))){
							kbset.status=ii;
							return;
						}

						/* Dispose of clauses in disposal queue and iterate. */
						for (auxcl=kbset.lastdisp,ii=0;auxcl!=NULL;auxcl=kbset.lastdisp,ii++){

							/* Dispose of clause. */
							if (NULL==(ptr1=Decompile(auxcl->part2.bin))){
								kbset.status=NOMEMORY;
								return;
							}
							kbset.lastdisp=auxcl->prevdisp;
							auxcl->prevdisp=NULL;
							auxcl->flags&=(~INDISPOSALQUEUE);
							if (NOMEMORY==UEQUnlink(auxcl)){
								kbset.status=NOMEMORY;
								return;
							}
							MYFREE(auxcl->part2.bin);
							auxcl->part2.text=ptr1;
							AddTxtFormula2KB(auxcl,auxcl->inference,&kbset);

							/* Check timeout and solution by other process. */
							if (ii>=TIMEOUTCCLDSP){
								if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
									kbset.status=UNKNOWN;
									return;
								}
								if (procctl->status==TIMEOUT){
									kbset.status=TIMEOUT;
									return;
								}
								if (procctl->status==NOMEMORY){
									kbset.status=NOMEMORY;
									return;
								}
								ii=-1;
							}
						}
						continue;
						break;
				}
			}

			/* Perform backward simplification. */
			if (0!=(ii=BckUEQSimplify(current))){
				if ((ii==TIMEOUT)||(ii==NOMEMORY)||(ii==UNKNOWN)){
					MYFREE(current->part2.bin);
					MYFREE(current);
				}
				kbset.status=ii;
				return;
			}

			/* Add clause to passive clauses. */
			current->flags&=(~(ACTIVE|PASSIVE|UNPROC|LOCKED));
			current->flags|=PASSIVE;
			if (0!=(ii=AddUEQClause2KB(current))){
				kbset.status=ii;
				MYFREE(current->part2.bin);
				MYFREE(current);
				return;
			}
		}

		/* Dispose of clauses in disposal queue. */
		for (auxcl=kbset.lastdisp,ii=0;auxcl!=NULL;auxcl=kbset.lastdisp,ii++){

			/* Dispose of clause. */
			if (NULL==(ptr1=Decompile(auxcl->part2.bin))){
				kbset.status=NOMEMORY;
				return;
			}
			kbset.lastdisp=auxcl->prevdisp;
			auxcl->prevdisp=NULL;
			auxcl->flags&=(~INDISPOSALQUEUE);
			if (NOMEMORY==UEQUnlink(auxcl)){
				kbset.status=NOMEMORY;
				return;
			}
			MYFREE(auxcl->part2.bin);
			auxcl->part2.text=ptr1;
			AddTxtFormula2KB(auxcl,auxcl->inference,&kbset);

			/* Check timeout and solution by other process. */
			if (ii>=TIMEOUTCCLDSP){
				if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
					kbset.status=UNKNOWN;
					return;
				}
				if (procctl->status==TIMEOUT){
					kbset.status=TIMEOUT;
					return;
				}
				if (procctl->status==NOMEMORY){
					kbset.status=NOMEMORY;
					return;
				}
				ii=-1;
			}
		}

		/* LRS has been selected. */
		if (kbset.opts.mxwghtopt&(MXWEIGHTLRS|MXWEIGHTLRSOTT)){

			/* Threshold has been reached. */
			kbset.lrscnt++;
			if (kbset.lrscnt>=LRSTHRESHOLD){

				/* Get number of executed instructions for last cycle and number of */
				/* executable instructions left. Compute number of reachable passive */
				/* clauses. Update current executed instructions for next cycle*/
				myread(&instr2);
				reachcl=(double)LRSCOEF*LRSTHRESHOLD*(instrlimit-instr2)/(instr2-instr1);
				instr1=instr2;

				/* Not all passive clauses are reachable at the current rate. */
				if (reachcl<kbset.prstats.nbpassive){
					if (0!=(ii=LRSClean((int32_t)reachcl))){
						kbset.status=ii;
						return;
					}
				}

				/* Update LRS count. */
				kbset.lrscnt=0;
			}
		}

		/* Select current clause and unlink it from KB. */
		if (NULL==(current=SelectClause())){
			break;
		}
		if (NOMEMORY==UEQUnlink(current)){
			kbset.status=NOMEMORY;
			return;
		}

		/* Add current clause to working KB as ACTIVE. */
		splitcl[0]=current;
		splitcl[1]=NULL;
		current->flags&=(~(TEXTFORMULA|PASSIVE|ACTIVE|UNPROC));
		current->flags|=ACTIVE;
		if (0!=(ii=SplitNAdd2Active(&splitcl[0]))){
			kbset.status=ii;
			MYFREE(current->part2.bin);
			MYFREE(current);
			return;
		}

		/* Generate and normalize critical pairs. */
		for (jj=ii=0;(jj<3)&&(splitcl[jj]!=NULL)&&(ii==0);jj++){
			switch (ii=GenerateCP(splitcl[jj])){
				case NOMEMORY:
				case TIMEOUT:
				case UNKNOWN:
					kbset.status=ii;
					return;
					break;
			}
		}

		/* Check empty clause. */
		if (ii){
			kbset.status=UNSATISFIABLE;
			if (procctl->prctype){
				sem_wait(&procctl->procctl);
				if (procctl->status!=UNSATISFIABLE){
					procctl->status=UNSATISFIABLE;
					procctl->solvekb=procnb;
				}
				sem_post(&procctl->procctl);
			}
			return;
		}
	}

	/* If we are here then theory is SATISFIABLE if ordering is standard KBO */
	/* and LRS is disabled or UNKNOWN otherwise. */
	if ((kbset.opts.termord==STANDARD)&&(glblopts.satmode!=0)&&(0==(kbset.opts.mxwghtopt&(MXWEIGHTLRS|MXWEIGHTLRSOTT)))
			&&(0==(prproflags&CLFILTERINGPRUNNED))){
		kbset.status=SATISFIABLE;
		if (procctl->prctype){
			sem_wait(&procctl->procctl);
			if ((procctl->status!=SATISFIABLE)&&(procctl->status!=UNSATISFIABLE)){
				procctl->status=SATISFIABLE;
				procctl->solvekb=procnb;
			}
			sem_post(&procctl->procctl);
		}
	} else {
		if (procctl->status==TIMEOUT){
			kbset.status=TIMEOUT;
		} else {
			kbset.status=UNKNOWN;
		}
	}

	return;
} /* UEQOtter */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Discount saturation algorithm for unfailing completion)  OCJ
 *
 *    This function performs the Discount type saturation algorithm
 *    for completion without failure strategies as described in document
 *    "Twee: An Equational Theorem Prover (System Description)"
 *    by Nicholas Smallbone.
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
 *
 *--------------------------------------------------------------*/
void UEQDscnt(void){
	auto cmprefix *current;                              /* Selected current clause. */
	auto cmprefix *splitcl[3];                           /* Set of pointers to split clauses */
	auto cmprefix *auxcl,*auxcl1,*auxcl2,*auxcl3;        /* Pointers to auxiliary clause */
	#if MEMCHECK == 3
	auto struct mallinfo2 rsinfo;                        /* For mallinfo2() calls */
	auto uint64_t mem2;                                  /* Current memory measurements */
	#endif
	#if MEMCHECK != 0
	auto uint64_t mem1;                                  /* Previous memory measurements */
	auto int32_t count,countlimit;                       /* Counter and limit for memory check cycles */
	#endif
	auto char *ptr1;                                     /* Auxiliary pointer */
	auto uint8_t *ptr2,*ptr3;                            /* Auxiliary pointers */
	auto int32_t ii,jj;                                  /* Auxiliary */
	auto uint64_t hh;                                    /* Auxiliary */

	/* Initialize clause selection parameters. */
	kbset.selectcnt=0;

	/* Main loop. */
	#if MEMCHECK != 0
	count=-1;countlimit=1;
	mem1=0; /* Just to prevent compiler warnings */
	#if MEMCHECK == 3
	mem2=0; /* Just to prevent compiler warnings */
	#endif
	#endif
	while ((kbset.frstpassive!=NULL)||(kbset.frstunproc!=NULL)){

		/* Memory limit check. */
		#if (MEMCHECK == 1) || (MEMCHECK == 2)
		count++;
		if (count==0){
			mem1=allcdmemory;
		} else if (count>=countlimit){
			count=0;
			if (maxmemory<=allcdmemory){
				kbset.status=NOMEMORY;
				return;
			}
			if (allcdmemory>mem1){
				countlimit=(((double)(maxmemory-allcdmemory))*countlimit)/(2*(allcdmemory-mem1));
			}
			mem1=allcdmemory;
			#if MEMCHECK == 2
			if (GetAvailableMemory()<=(MEMORYLIMIT1/2)){
				kbset.status=NOMEMORY;
				return;
			}
			#endif
		}
		#else
		#if MEMCHECK == 3
		count++;
		if (count==0){
			rsinfo=mallinfo2();
			mem1=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
		} else if (count>=countlimit){
			count=0;
			rsinfo=mallinfo2();
			mem2=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
			if (maxmemory<=mem2){
				kbset.status=NOMEMORY;
				return;
			}
			if (mem2>mem1){
				countlimit=(((double)(maxmemory-mem2))*countlimit)/(2*(mem2-mem1));
			}
			mem1=mem2;
		}
		#endif
		#endif

		/* Loop through unprocessed clauses. */
		for (auxcl=SelectUnprocClause();auxcl!=NULL;auxcl=SelectUnprocClause()){

			/* Check hardware instructions limit. */
			if (instrlimit!=0xffffffffffffffff){
				myread(&hh);
				if (hh>=instrlimit){
					MYFREE(auxcl->part2.bin);
					MYFREE(auxcl);
					kbset.status=SLICETMOUT;
					return;
				}
			}

			/* Do simplifications to and from clause. */
			switch (ii=FwUEQSimplify(&auxcl)){

				/* Timeout, memory and solution by other process. */
				case NOMEMORY:
				case TIMEOUT:
				case UNKNOWN:
					kbset.status=ii;
					return;
					break;

				/* Empty clause inferred by simplification. */
				case 1:
					kbset.status=UNSATISFIABLE;
					if (procctl->prctype){
						sem_wait(&procctl->procctl);
						if (procctl->status!=UNSATISFIABLE){
							procctl->status=UNSATISFIABLE;
							procctl->solvekb=procnb;
						} else {
							MYFREE(kbset.frstprfnode);
						}
						sem_post(&procctl->procctl);
					}
					return;
					break;

				/* Tautology. */
				case 2:
					continue;
					break;
			}

			/* Retention test checking. */
			switch (Retained(auxcl)){

				/* Not enough memory. */
				case NOMEMORY:
					kbset.status=NOMEMORY;
					MYFREE(auxcl->part2.bin);
					MYFREE(auxcl);
					return;
					break;

				/* Clause is retained. Add clause to passive clauses. */
				case 1:
					auxcl->flags&=(~(ACTIVE|PASSIVE|UNPROC|LOCKED));
					auxcl->flags|=PASSIVE;
					if (0!=(ii=AddUEQClause2KB(auxcl))){
						kbset.status=ii;
						MYFREE(auxcl->part2.bin->ovly.cptopterm);
						MYFREE(auxcl->part2.bin);
						MYFREE(auxcl);
						return;
					}
					break;
			}
		}

		/* Select current clause and unlink it from KB. */
		if (NULL==(current=SelectClause())){
			break;
		}
		if (NOMEMORY==UEQUnlink(current)){
			kbset.status=NOMEMORY;
			return;
		}

		/* If the current clause is a critical pair generated by active clauses */
		/* and some parent is not an ACTIVE clause (that is, if the current */
		/* clause is orphan because one of its parents has been simplified) */
		/* then discard the current clause and iterate. */
		if ((current->inference==PARAMODULATION)&&((TEXTFORMULA&current->parent1->flags)
				||(TEXTFORMULA&current->parent2->flags))){
			MYFREE(current->part2.bin->ovly.cptopterm);
			MYFREE(current->part2.bin);
			MYFREE(current);
			continue;
		}

		/* Normalize current clause using all ACTIVE clauses. */
		auxcl=current;
		if (0!=(ii=Normalize(&current,1))){
			kbset.status=ii;
			return;
		}

		/* Current clause is trivial. If original current clause was inferred as */
		/* PARAMODULATION then free current clause and all intermediate results */
		/* and iterate, otherwise convert clause to text form. */
		if (IsEqTautology(current)){
			if (auxcl->inference==PARAMODULATION){
				for (auxcl1=current,auxcl2=NULL;auxcl2!=auxcl;auxcl1=auxcl3){
					auxcl2=auxcl1;
					auxcl3=auxcl1->parent2;
					if (TEXTFORMULA&auxcl1->flags){
						UnlinkTxtFormula(auxcl1,&kbset);
						MYFREE(auxcl1->part2.text);
					} else {
						MYFREE(auxcl1->part2.bin->ovly.cptopterm);
						MYFREE(auxcl1->part2.bin);
					}
					MYFREE(auxcl1);
				}
			} else {
				if (NULL==(ptr1=Decompile(current->part2.bin))){
					kbset.status=NOMEMORY;
					return;
				}
				MYFREE(current->part2.bin);
				current->part2.text=ptr1;
				AddTxtFormula2KB(current,auxcl->inference,&kbset);
			}
			continue;
		}

		/* Current clause is positive and connected. Free current clause */
		/* and all intermediate results and iterate. */
		if ((kbset.opts.connect)&&(0==(NEGATED&current->part2.bin->formula[0]))){
			switch (ii=IsCPConnected(current)){
				case NOMEMORY:
				case TIMEOUT:
				case UNKNOWN:
					MYFREE(current->part2.bin->ovly.cptopterm);
					MYFREE(current->part2.bin);
					MYFREE(current);
					kbset.status=ii;
					return;
					break;
				case 1:
					for (auxcl1=current,auxcl2=NULL;auxcl2!=auxcl;auxcl1=auxcl3){
						auxcl2=auxcl1;
						auxcl3=auxcl1->parent2;
						if (TEXTFORMULA&auxcl1->flags){
							UnlinkTxtFormula(auxcl1,&kbset);
							MYFREE(auxcl1->part2.text);
						} else {
							MYFREE(auxcl1->part2.bin->ovly.cptopterm);
							MYFREE(auxcl1->part2.bin);
						}
						MYFREE(auxcl1);
					}
					continue;
					break;
			}
		}

		/* Current clause is subsumed. If original current clause was inferred as */
		/* PARAMODULATION then free current clause and all intermediate results */
		/* and iterate, otherwise convert clause to text form. */
		switch (ii=FwUEQSubsum(current)){
			case NOMEMORY:
			case TIMEOUT:
			case UNKNOWN:
				MYFREE(current->part2.bin);
				MYFREE(current);
				kbset.status=ii;
				return;
				break;
			case 1:
				if (auxcl->inference==PARAMODULATION){
					for (auxcl1=current,auxcl2=NULL;auxcl2!=auxcl;auxcl1=auxcl3){
						auxcl2=auxcl1;
						auxcl3=auxcl1->parent2;
						if (TEXTFORMULA&auxcl1->flags){
							UnlinkTxtFormula(auxcl1,&kbset);
							MYFREE(auxcl1->part2.text);
						} else {
							MYFREE(auxcl1->part2.bin->ovly.cptopterm);
							MYFREE(auxcl1->part2.bin);
						}
						MYFREE(auxcl1);
					}
				} else {
					if (NULL==(ptr1=Decompile(current->part2.bin))){
						kbset.status=NOMEMORY;
						return;
					}
					MYFREE(current->part2.bin);
					current->part2.text=ptr1;
					AddTxtFormula2KB(current,auxcl->inference,&kbset);
				}
				continue;
				break;
		}

		/* Free critical pair top term as it is no longer necessary. */
		MYFREE(current->part2.bin->ovly.cptopterm);
		current->part2.bin->ovly.cptopterm=NULL;

		/* If ground joinability is enabled and current clause is positive and */
		/* ground joinable then add it to ground joinable set, call */
		/* BkUEQSubsum() and iterate. */
		if ((kbset.opts.grjoin)&&(0==(NEGATED&current->part2.bin->formula[0]))){
			switch (ii=IsGrJoinable(&current->part2.bin->formula[0],current->part2.bin->size,
					current->part2.bin->maxvarnb,0,1)){

				/* NOMEMORY, TIMEOUT, SLICETMOUT or RETURN. */
				case NOMEMORY:
				case TIMEOUT:
				case UNKNOWN:
				case SLICETMOUT:
					MYFREE(current->part2.bin);
					MYFREE(current);
					kbset.status=ii;
					return;
					break;

				/* Clause is ground joinable. */
				case 1:

					/* Add clause to ground joinable clauses set. */
					current->flags&=(~(TEXTFORMULA|PASSIVE|ACTIVE|UNPROC));
					current->flags|=GROUNDJOINABLE;
					if (NOMEMORY==AddUEQClause2KB(current)){
						kbset.status=NOMEMORY;
						MYFREE(current->part2.bin);
						MYFREE(current);
						return;
					}

					/* Perform backward subsumption. */
					if (0!=(ii=BkUEQSubsum(current))){
						kbset.status=ii;
						return;
					}

					/* Dispose of clauses in disposal queue and iterate. */
					for (auxcl=kbset.lastdisp,ii=0;auxcl!=NULL;auxcl=kbset.lastdisp,ii++){

						/* Dispose of clause. */
						if (NULL==(ptr1=Decompile(auxcl->part2.bin))){
							kbset.status=NOMEMORY;
							return;
						}
						kbset.lastdisp=auxcl->prevdisp;
						auxcl->prevdisp=NULL;
						auxcl->flags&=(~INDISPOSALQUEUE);
						if (NOMEMORY==UEQUnlink(auxcl)){
							kbset.status=NOMEMORY;
							return;
						}
						MYFREE(auxcl->part2.bin);
						auxcl->part2.text=ptr1;
						AddTxtFormula2KB(auxcl,auxcl->inference,&kbset);

						/* Check timeout and solution by other process. */
						if (ii>=TIMEOUTCCLDSP){
							if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
								kbset.status=UNKNOWN;
								return;
							}
							if (procctl->status==TIMEOUT){
								kbset.status=TIMEOUT;
								return;
							}
							if (procctl->status==NOMEMORY){
								kbset.status=NOMEMORY;
								return;
							}
							ii=-1;
						}
					}
					continue;
					break;
			}
		}

		/* Clause is an inequality. */
		if (NEGATED&current->part2.bin->formula[0]){

			/* Simplify clause by equality resolution. */
			if (UEQEqResolution(&current,1)){
				MYFREE(current->part2.bin);
				MYFREE(current);
				kbset.status=TIMEOUT;
				return;
			}

			/* Check empty clause. */
			if (current->part2.bin->formula[0]==UNITEND){
				kbset.status=UNSATISFIABLE;
				if (procctl->prctype){
					sem_wait(&procctl->procctl);
					if (procctl->status!=UNSATISFIABLE){
						procctl->status=UNSATISFIABLE;
						procctl->solvekb=procnb;
					}
					sem_post(&procctl->procctl);
				}
				return;
			}
		}

		/* Simplify active clauses. */
		if (0!=(ii=BckUEQSimplify(current))){
			if ((ii==TIMEOUT)||(ii==NOMEMORY)||(ii==UNKNOWN)){
				MYFREE(current->part2.bin);
				MYFREE(current);
			}
			kbset.status=ii;
			return;
		}

		/* Dispose of clauses in disposal queue. */
		for (auxcl=kbset.lastdisp,ii=0;auxcl!=NULL;auxcl=kbset.lastdisp,ii++){

			/* Dispose of clause. */
			if (NULL==(ptr1=Decompile(auxcl->part2.bin))){
				kbset.status=NOMEMORY;
				return;
			}
			kbset.lastdisp=auxcl->prevdisp;
			auxcl->prevdisp=NULL;
			auxcl->flags&=(~INDISPOSALQUEUE);
			if (NOMEMORY==UEQUnlink(auxcl)){
				kbset.status=NOMEMORY;
				return;
			}
			MYFREE(auxcl->part2.bin);
			auxcl->part2.text=ptr1;
			AddTxtFormula2KB(auxcl,auxcl->inference,&kbset);

			/* Check timeout and solution by other process. */
			if (ii>=TIMEOUTCCLDSP){
				if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
					kbset.status=UNKNOWN;
					return;
				}
				if (procctl->status==TIMEOUT){
					kbset.status=TIMEOUT;
					return;
				}
				if (procctl->status==NOMEMORY){
					kbset.status=NOMEMORY;
					return;
				}
				ii=-1;
			}
		}

		/* Add current clause to working KB as ACTIVE. */
		splitcl[0]=current;
		splitcl[1]=NULL;
		current->flags&=(~(TEXTFORMULA|PASSIVE|ACTIVE|UNPROC));
		current->flags|=ACTIVE;
		if (0!=(ii=SplitNAdd2Active(&splitcl[0]))){
			kbset.status=ii;
			MYFREE(current->part2.bin);
			MYFREE(current);
			return;
		}

		/* Check if current clause can be used to solve immediately the problem. */
		ptr2=NextItem(&splitcl[0]->part2.bin->formula[0],IMMED);
		ptr3=NextItem(ptr2,OVERSUBTERMS);
		if ((0==(NEGATED&splitcl[0]->part2.bin->formula[0]))&&(((VARIABLE&ptr2[0])&&(0==(MAXIMUM&ptr3[0])))
				||((VARIABLE&ptr3[0])&&(0==(MAXIMUM&ptr2[0]))))){
			if (NOMEMORY==MakeTrivialProof(splitcl[0])){
				kbset.status=NOMEMORY;
				return;
			}
			kbset.status=UNSATISFIABLE;
			if (procctl->prctype){
				sem_wait(&procctl->procctl);
				if (procctl->status!=UNSATISFIABLE){
					procctl->status=UNSATISFIABLE;
					procctl->solvekb=procnb;
				}
				sem_post(&procctl->procctl);
			}
			return;
		}

		/* Generate and normalize critical pairs. */
		for (jj=ii=0;(jj<3)&&(splitcl[jj]!=NULL)&&(ii==0);jj++){
			switch (ii=GenerateCP(splitcl[jj])){
				case NOMEMORY:
				case TIMEOUT:
				case UNKNOWN:
					kbset.status=ii;
					return;
					break;
			}
		}

		/* Check empty clause. */
		if (ii){
			kbset.status=UNSATISFIABLE;
			if (procctl->prctype){
				sem_wait(&procctl->procctl);
				if (procctl->status!=UNSATISFIABLE){
					procctl->status=UNSATISFIABLE;
					procctl->solvekb=procnb;
				}
				sem_post(&procctl->procctl);
			}
			return;
		}
	}

	/* If we are here then theory is SATISFIABLE if ordering is standard KBO */
	/* or UNKNOWN otherwise. */
	if ((kbset.opts.termord==STANDARD)&&(glblopts.satmode!=0)){
		kbset.status=SATISFIABLE;
		if (procctl->prctype){
			sem_wait(&procctl->procctl);
			if ((procctl->status!=SATISFIABLE)&&(procctl->status!=UNSATISFIABLE)
					&&(0==(prproflags&CLFILTERINGPRUNNED))){
				procctl->status=SATISFIABLE;
				procctl->solvekb=procnb;
			}
			sem_post(&procctl->procctl);
		}
	} else {
		if (procctl->status==TIMEOUT){
			kbset.status=TIMEOUT;
		} else {
			kbset.status=UNKNOWN;
		}
	}

	return;
} /* UEQDscnt */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Select current clause from passive clauses)  OCJ
 *
 *    This function selects a "current" clause from the set of
 *    passive clauses in the working KB.
 *
 *    The selection algorithm is based on the "age" and "weight" queues
 *    of the KB and on an age to weight ratio that ensures fairness.
 *    See global comments in SetClSelectOrder() function for a
 *    description of how the layered clause selection is implemented.
 *
 *    If shuffling is enabled then the queue ratios are respected
 *    only probabilistically. Otherwise the queues are emptied in a
 *    deterministic order such that the queue ratios are exactly
 *    fulfilled.
 *
 *
 *  ARGUMENTS:
 *
 *    None.
 *
 *  RETURNS:
 *
 *    Pointer to selected "current" clause or NULL if no more passive
 *    clauses available.
 *
 *
 *--------------------------------------------------------------*/
cmprefix *SelectClause(void){
	auto uint64_t minlk;                                 /* Maximum lookahead inferences */
	auto uint64_t lookahead[WEIGHTLKAHDCLAUSES];         /* Lookahead inferences for the first clauses */
	auto cmprefix *clauses[WEIGHTLKAHDCLAUSES+1];        /* Pointer to first WEIGHTLKAHDCLAUSES clauses */
	auto cmprefix *clause;                               /* Pointer to selected lookahead clause */
	auto uint64_t tt1,tt2;                               /* Auxiliary */
	auto int32_t ii,jj,kk;                               /* Auxiliary */

	/* There are passive clauses. */
	if (kbset.frstpassive!=NULL){

		/* Get queue index. If the selected queue is empty then the next */
		/* queue of the same type (weight or age) with clauses is picked. */
		if (kbset.opts.shuffle){
			ii=rand()%slctordrsize;
		} else {
			ii=kbset.selectcnt;
			kbset.selectcnt++;
			if (kbset.selectcnt==slctordrsize){
				kbset.selectcnt=0;
			}
		}
		if (slctordr[ii]<selectqueues){
			for (jj=slctordr[ii],kk=selectqueues-1;frstglselect[jj]==NULL;jj=(jj==kk?0:jj+1)){
			}
		} else {
			for (jj=slctordr[ii],kk=(2*selectqueues)-1;frstglselect[jj]==NULL;jj=(jj==kk?selectqueues:jj+1)){
			}
		}

		/* Lookahead selection (see WEIGHTLKAHDCLAUSES description in global.h). */
		if ((pbmflags&UNITCLAUSE)&&(kbset.opts.lookahead)&&(jj<selectqueues)){

			/* More than 1 passive clause. */
			if (kbset.frstpassive->next!=NULL){

				/* Collect lookahead data of first WEIGHTLKAHDCLAUSES clauses. */
				lookahead[0]=minlk=LookAhead(clauses[0]=frstglselect[jj]);
				for (clauses[1]=frstglselect[jj]->part2.bin->nextselect[jj],kk=1;
						(clauses[kk]!=NULL)&&(kk<WEIGHTLKAHDCLAUSES);kk++){
					if (clauses[kk]->part2.bin->literals==1){
						lookahead[kk]=tt1=LookAhead(clauses[kk]);
						if (tt1<minlk){
							minlk=tt1;
						}
					}
					clauses[kk+1]=clauses[kk]->part2.bin->nextselect[jj];
				}

				/* Select lightest clause with weight corrected by lookahead data. */
				for (clause=clauses[0],tt1=lookahead[0]-minlk,kk=1;
						(clauses[kk]!=NULL)&&(kk<WEIGHTLKAHDCLAUSES);kk++){
					tt2=lookahead[kk]-minlk;
					if ((clause->part2.bin->clweight+tt1)>(clauses[kk]->part2.bin->clweight+tt2)){
						clause=clauses[kk];
						tt1=tt2;
					}
				}
				clause->flags|=PICKED;
				return(clause);

			/* Only one passive clause. */
			} else {
				frstglselect[jj]->flags|=PICKED;
				return(frstglselect[jj]);
			}

		/* Clause selection without lookahead. */
		} else {
			frstglselect[jj]->flags|=PICKED;
			return(frstglselect[jj]);
		}
	}

	return(NULL);
} /* SelectClause */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if clause is retained)  OCJ
 *
 *    This function checks if a clause is retained. The clause is assumed
 *    to be unlinked from KB.
 *
 *    If the clause is not retained then it is converted to text and the
 *    binary (non text) data is freed.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause.
 *
 *  RETURNS:
 *
 *    0 -> Clause was not retained.
 *    1 -> Clause is retained.
 *    NOMEMORY -> Not enough memory
 *
 *
 *--------------------------------------------------------------*/
int32_t Retained(cmprefix *clause){
	auto char *ptr1;                                     /* Auxiliary pointer */

	/* Check clause weight. */
	ClauseWeight(clause);
	if (clause->part2.bin->clweight>=kbset.opts.maxweight){

		/* If split is enabled then convert clause to text form, otherwise delete */
		/* the clause. Due to Avatar architecture the same clause can go as */
		/* unprocessed several times, so an unprocessed clause is not necessarily */
		/* new and therefore can be part of the theorem proof. */
		if (kbset.opts.split){
			if (NULL==(ptr1=Decompile(clause->part2.bin))){
				return(NOMEMORY);
			}
		} else {
			ptr1=NULL; /* Just to prevent compiler warnings. */
		}
		kbset.flags|=RETENTIONUSED;
		if (clause->part2.bin->ovly.asserts!=NULL){
			MYFREE(clause->part2.bin->ovly.asserts);
		}
		MYFREE(clause->part2.bin);
		if (kbset.opts.split){
			clause->part2.text=ptr1;
			AddTxtFormula2KB(clause,clause->inference,&kbset);
		} else {
			MYFREE(clause);
		}
		return(0);
	}

	return(1);
} /* Retained */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Compute clause weight)  OCJ
 *
 *    This function computes the weight of a clause in compiled
 *    binary format. It also computes the Avatar and horn distances
 *    in the avdist and hrndist fields of binprefix structure
 *    respectively.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *--------------------------------------------------------------*/
void ClauseWeight(cmprefix *clause){
	auto learnvector *clvector;                          /* Pointer to clause learning vector */
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto double dd;                                      /* Auxiliary */
	auto int32_t ii,jj,ii1,jj1,iis,jjs;                  /* Auxiliary */

	/* Return if clause weight and Avatar and horn distances have been already set. */
	if (clause->flags&WEIGHTED){
		return;
	}

	/* Loop through clause items. */
	clause->part2.bin->clweight=0;
	ii=0; /* Number of positive literals. */
	for (ptr1=&clause->part2.bin->formula[0];ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){
		switch ((VARIABLE|EQUALITY|PREDICATE|FUNCTION)&ptr1[0]){
			case VARIABLE:
				clause->part2.bin->clweight+=VARWEIGHT;
				break;
			case PREDICATE:
				clause->part2.bin->clweight+=SYMBOLWEIGHT;
				if (0==(NEGATED&ptr1[0])){
					ii++;
				}
				break;
			case FUNCTION:
				clause->part2.bin->clweight+=SYMBOLWEIGHT;
				break;
			case EQUALITY:
				if (0==(NEGATED&ptr1[0])){
					ii++;
				}
				break;
		}
	}

	/* Set horn distance. */
	if (ii<=1){
		clause->part2.bin->hrndist=0;
	} else {
		clause->part2.bin->hrndist=ii-1;
	}

	/* Modify clause weight if learning is enabled. */
	if (NULL!=kbset.lrnvector){
		clause->part2.bin->clweight=0.5+(kbset.opts.gamma*clause->part2.bin->clweight);
		clvector=GetClauseVector(clause);
		if (clvector!=NULL){
			iis=clvector->size;
			jjs=kbset.lrnvector->size;
			for (ii=jj=0,dd=0.0;(ii<iis)&&(jj<jjs);){
				ii1=clvector->index[ii];
				jj1=kbset.lrnvector->index[jj];
				if (ii1>jj1){
					jj++;
				} else if (jj1>ii1){
					ii++;
				} else {
					dd+=(clvector->sparsevect[ii]*kbset.lrnvector->sparsevect[jj]);
					ii++;
					jj++;
				}
			}
			MYFREE(clvector->index);
			MYFREE(clvector);
			if (dd>0.0){
				clause->part2.bin->clweight+=GOODCLAUSEWEIGHT;
			} else {
				clause->part2.bin->clweight+=BADCLAUSEWEIGHT;
			}
		}
	}

	/* Set Avatar distance. */
	if ((clause->part2.bin->ovly.asserts==NULL)||(kbset.opts.algorithm==UEQOTT)||(kbset.opts.algorithm==UEQDSC)){
		clause->part2.bin->avdist=0;
	} else {
		for (ptr1=clause->part2.bin->ovly.asserts,ii=0;ptr1[0]!=UNITEND;ptr1+=1+sizeof(asymbol),ii++){
		}
		clause->part2.bin->avdist=ii;
	}

	/* Indicate that clause weight and Avatar and horn distances */
	/* have been set and return. */
	clause->flags|=WEIGHTED;
	return;
} /* ClauseWeight */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Delete unreachable passive clauses)  OCJ
 *
 *    This function deletes unreachable passive clauses in the working KB
 *    given the number of reachable clauses.
 *
 *
 *  ARGUMENTS:
 *
 *    reachcl: Number of reachable passive clauses.
 *
 *  RETURNS:
 *
 *    0 -> OK
 *    NOMEMORY -> Not enough memory
 *    SLICETMOUT -> maximum number of executed hardware instructions
 *                  was reached.
 *
 *
 *--------------------------------------------------------------*/
int32_t LRSClean(int32_t reachcl){
	auto cmprefix *nxtclause[2*SELECTQUEUES];            /* Pointer to next clause in each weight queue to be signed by weight */
	auto cmprefix *auxcl1,*auxcl2;                       /* Auxiliary clauses */
	auto int32_t selectcnt;                              /* Current number of selected clauses from queue indexed by selectidx */
	auto char *ptr1;                                     /* Auxiliary pointer */
	auto int32_t ii,jj,kk,mm;                            /* Auxiliary */
	auto uint32_t ww;                                    /* Auxiliary */
	auto uint64_t hh;                                    /* Auxiliary */

	/* Indicate that LRS has been used. */
	kbset.flags|=RETENTIONUSED;

	/* Get first clause in clause selection queues. */
	selectcnt=kbset.selectcnt;
	for (ii=0,kk=2*selectqueues;ii<kk;ii++){
		nxtclause[ii]=frstglselect[ii];
	}

	/* Sign the reachable passive clauses. */
	kbset.signature++;
	for (ii=mm=0;ii<reachcl;ii++,mm++){

		/* Check number of executed hardware instructions. */
		if (mm>CHKCYCLES){
			mm=0;
			myread(&hh);
			if (hh>=instrlimit){
				return(SLICETMOUT);
			}
		}

		/* Jump over already signed clauses. If the selected queue is empty */
		/* or all its clauses are signed then the next queue of the same type */
		/* (weight or age) with unsigned clauses is picked. */
		if (slctordr[selectcnt]<selectqueues){
			for (kk=slctordr[selectcnt],jj=selectqueues-1;;kk=(kk==jj?0:kk+1)){
				for (;(nxtclause[kk]!=NULL)&&(nxtclause[kk]->part2.bin->signature==kbset.signature);
						nxtclause[kk]=nxtclause[kk]->part2.bin->nextselect[kk]){
				}
				if (nxtclause[kk]!=NULL){
					break;
				}
			}
		} else {
			for (kk=slctordr[selectcnt],jj=(2*selectqueues)-1;;kk=(kk==jj?selectqueues:kk+1)){
				for (;(nxtclause[kk]!=NULL)&&(nxtclause[kk]->part2.bin->signature==kbset.signature);
						nxtclause[kk]=nxtclause[kk]->part2.bin->nextselect[kk]){
				}
				if (nxtclause[kk]!=NULL){
					break;
				}
			}
		}

		/* Sign clause and update index and counter. */
		nxtclause[kk]->part2.bin->signature=kbset.signature;
		nxtclause[kk]=nxtclause[kk]->part2.bin->nextselect[kk];
		selectcnt++;
		if (selectcnt==slctordrsize){
			selectcnt=0;
		}
	}

	/* Dispose of unsigned clauses. */
	for (auxcl1=kbset.frstpassive,mm=0;auxcl1!=NULL;auxcl1=auxcl2,mm++){

		/* Check number of executed hardware instructions. */
		if (mm>CHKCYCLES){
			mm=0;
			myread(&hh);
			if (hh>=instrlimit){
				return(SLICETMOUT);
			}
		}

		/* Dispose of clause. */
		auxcl2=auxcl1->next;
		if (auxcl1->part2.bin->signature!=kbset.signature){
			if (NULL==(ptr1=Decompile(auxcl1->part2.bin))){
				return(NOMEMORY);
			}
			switch (kbset.opts.algorithm){
				case OTTER:
				case DISCOUNT:
					if (pbmtype==8){
						if (NOMEMORY==UnlinkUEQ(auxcl1,&kbset)){
							return(NOMEMORY);
						}
					} else {
						if (NOMEMORY==Unlink(auxcl1,&kbset)){
							return(NOMEMORY);
						}
					}
					break;
				default:
					if (NOMEMORY==UEQUnlink(auxcl1)){
						return(NOMEMORY);
					}
					break;
			}
			if (auxcl1->part2.bin->ovly.asserts!=NULL){
				MYFREE(auxcl1->part2.bin->ovly.asserts);
			}
			MYFREE(auxcl1->part2.bin);
			auxcl1->part2.text=ptr1;
			AddTxtFormula2KB(auxcl1,auxcl1->inference,&kbset);
		}
	}

	#ifdef LRSWEIGHTLIMIT
	/* Update current maximum clause weight to the maximum existing weight */
	/* in weight queues. */
	for (ww=0,ii=0;ii<selectqueues;ii++){
		if (lstglselect[ii]!=NULL){
			auxcl1=lstglselect[ii];
			if (auxcl1->part2.bin->clweight>ww){
				ww=auxcl1->part2.bin->clweight;
			}
		}
	}
	if (kbset.opts.maxweight>ww){
		kbset.opts.maxweight=ww;
	}
	#endif

	return(0);
} /* LRSClean */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Set select order for clause selection queues)  OCJ
 *
 *    This function initializes the sets pointed by slctordr pointer.
 *    The sets are allocated or reallocated if necessary. They contain
 *    2*selectqueues elements, one for each extraction from the selection
 *    queues during a round-robin clause selection cycle. Each element
 *    indicates from what queue the extraction must be done. See slctordr
 *    comments in global.h for additional details.
 *
 *    The slctordrsize global variable is also initialized.
 *
 *    LAYERED CLAUSE SELECTION IMPLEMENTATION:
 *    =======================================
 *    The current implementation follows the document "Layered Clause
 *    Selection for Saturation-Based Theorem Proving" by Bernhard Gleiss
 *    and Martin Suda.
 *    Currently used layers:
 *    ---------------------
 *    - SInE layer: This layer is available if the problem has a goal
 *      (conjecture or negated conjecture).
 *    - Avatar layer: This layer is available if split is enabled and the
 *      the problem is not unit clause.
 *    - Horn layer: This layer is available if the problem is not horn.
 *    - Weight/Age layer: This layer is always available.
 *    Layer configurations:
 *    --------------------
 *    For each type of problem two possible layer configurations are defined
 *    depending on the strategy in use and layers availability. The two
 *    possible layer configurations are the following:
 *    If SInE, Avatar and Horn are available:
 *    - SInE+Horn
 *    - SInE+Avatar
 *    If SInE and Horn are available:
 *    - SInE+Horn
 *    - SInE
 *    If SInE and Avatar are available:
 *    - SInE+Avatar
 *    - SInE
 *    If Avatar and Horn are available:
 *    - Horn
 *    - Horn+Avatar
 *    Only one specialized layer is available (SInE, Avatar or Horn)
 *    - Use the available specialized layer
 *    - Don't use any specialized layer, only use the weight/Age layer.
 *    Layers order:
 *    ------------
 *    - If SInE layer is enabled then it is always the top layer.
 *    - If Avatar layer is enabled then it is always the next layer. It can
 *      be the first layer if the SInE layer is disabled.
 *    - If horn layer is enabled then it is always the next layer. Therefore
 *      it can be the first layer, second or third layer depending on
 *      SInE and Avatar layers being enabled or not.
 *    - The Weight/Age layer is always enabled and it is always the last
 *      layer. It can be the only layer if all SInE, Avatar and horn layers
 *      are disabled.
 *    Clause selection queues organization:
 *    ------------------------------------
 *    The number of clause selection queues depends on what layers are
 *    enabled. The number of clause selection weight queues is stored in
 *    selectqueues global variable and there are an equal number od clause
 *    selection age queues. Each clause selection queue has an index
 *    assigned to it that corresponds to the indices of nextselect[] and
 *    prevselect[] fields in binprefix structure and frstglselect[],
 *    frstselect[] and lstselect[] global variables. These indices are
 *    assigned as follows. Let's assume that at least one of the SInE,
 *    Avatar and horn layer is enabled and let l1 be the top layer (usually
 *    SInE layer). Let lf be the last of the mentioned layers and sf the
 *    last segment. The first block from index 0 to index selectqueues-1
 *    is assigned to the weight queues in the last layer as follows.
 *      Index 0: l1s1 l2s1
 *      Index 1: l1s2 l2s1
 *      ...
 *      Index i: l1sf l2s1
 *      Index i+1: l1s1 l2s2
 *      Index i+2: l1s2 l2s2
 *      ...
 *      Index j: l1sf l2s2
 *      ...
 *      Index k: l1s1 l2sf
 *      Index k+1: l1s2 l2sf
 *      ...
 *      Index selectqueues-1: l1sf l2s2
 *    The second block from index selectqueues to index 2*selectqueues-1
 *    is assigned to the age queues in the same way as before.
 *
 *    Clause selection order:
 *    ----------------------
 *    Let r0:r1:...:rn be the ratios assigned to each of the 2*selectqueues
 *    clause selection queues corresponding to the queue indices 0, 1, ..., n
 *    as described above. The first r0 clause selections are done from the
 *    queue with index 0, corresponding to the first weight queue. The next ri
 *    selections are done from the queue with index selectqueues, corresponding
 *    to the first age queue. The next r1 selections are done from the queue
 *    queue with index 1, corresponding to the second weight queue. The next
 *    ri+1 selections are done from the queue with index selectqueues+1,
 *    corresponding to the second age queue. This process is continued
 *    alternating weight and age queues until the last weight and age queues
 *    are processed. Then the process is started again from the beginning.
 *    The global variable slctordr is used to identify the queue index of each
 *    clause selection (see description in global.h).
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
 *
 *--------------------------------------------------------------*/
int32_t SetClSelectOrder(void){
	auto int32_t tqueuerts[2*SELECTQUEUES];              /* Clause selection ratios. */
	auto int32_t *ptr1;                                  /* Auxiliary pointer */
	auto int32_t ii,jj,kk,mm,nn;                         /* Auxiliary */

	/* Initialize the clause selection queue ratios. Currently the SInE layer */
	/* ratios are fixed at 1:2:3 for each of its 3 segments, the Avatar layer */
	/* ratios are fixed at 1:1 and the Horn layer ratios are fixed at 1:4. */
	switch (kbset.opts.layerset[kbset.opts.layer]){
		case SIAVLYR:
			tqueuerts[0]=queuerts[0];
			tqueuerts[1]=2*queuerts[0];
			tqueuerts[2]=3*queuerts[0];
			tqueuerts[3]=queuerts[0];
			tqueuerts[4]=2*queuerts[0];
			tqueuerts[5]=3*queuerts[0];
			tqueuerts[SELECTQUEUES]=queuerts[1];
			tqueuerts[SELECTQUEUES+1]=2*queuerts[1];
			tqueuerts[SELECTQUEUES+2]=3*queuerts[1];
			tqueuerts[SELECTQUEUES+3]=queuerts[1];
			tqueuerts[SELECTQUEUES+4]=2*queuerts[1];
			tqueuerts[SELECTQUEUES+5]=3*queuerts[1];
			break;
		case SIHRNLYR:
			tqueuerts[0]=queuerts[0];
			tqueuerts[1]=2*queuerts[0];
			tqueuerts[2]=3*queuerts[0];
			tqueuerts[3]=4*queuerts[0];
			tqueuerts[4]=4*2*queuerts[0];
			tqueuerts[5]=4*3*queuerts[0];
			tqueuerts[SELECTQUEUES]=queuerts[1];
			tqueuerts[SELECTQUEUES+1]=2*queuerts[1];
			tqueuerts[SELECTQUEUES+2]=3*queuerts[1];
			tqueuerts[SELECTQUEUES+3]=4*queuerts[1];
			tqueuerts[SELECTQUEUES+4]=4*2*queuerts[1];
			tqueuerts[SELECTQUEUES+5]=4*3*queuerts[1];
			break;
		case SINELYR:
			tqueuerts[0]=queuerts[0];
			tqueuerts[1]=2*queuerts[0];
			tqueuerts[2]=3*queuerts[0];
			tqueuerts[3]=queuerts[1];
			tqueuerts[4]=2*queuerts[1];
			tqueuerts[5]=3*queuerts[1];
			break;
		case AVHRNLYR:
			tqueuerts[0]=queuerts[0];
			tqueuerts[1]=queuerts[0];
			tqueuerts[2]=4*queuerts[0];
			tqueuerts[3]=4*queuerts[0];
			tqueuerts[4]=queuerts[1];
			tqueuerts[5]=queuerts[1];
			tqueuerts[6]=4*queuerts[1];
			tqueuerts[7]=4*queuerts[1];
			break;
		case AVLYR:
			tqueuerts[0]=queuerts[0];
			tqueuerts[1]=queuerts[0];
			tqueuerts[2]=queuerts[1];
			tqueuerts[3]=queuerts[1];
			break;
		case HRNLYR:
			tqueuerts[0]=queuerts[0];
			tqueuerts[1]=4*queuerts[0];
			tqueuerts[2]=queuerts[1];
			tqueuerts[3]=4*queuerts[1];
			break;
		case NONELYR:
			tqueuerts[0]=queuerts[0];
			tqueuerts[1]=queuerts[1];
			break;
	}

	/* Simplify the ratios. */
	for (ii=0,jj=0x7fffffff,mm=2*selectqueues;ii<mm;ii++){
		if (tqueuerts[ii]<jj){
			jj=tqueuerts[ii];
		}
	}
	for (ii=jj;ii>1;ii=(ii==jj?jj/2:ii-1)){
		if (0==jj%ii){
			for (kk=0;(kk<mm)&&(0==(tqueuerts[kk]%ii));kk++){
			}
			if (kk==mm){
				for (kk=0;kk<mm;kk++){
					tqueuerts[kk]/=ii;
				}
				break;
			}
		}
	}

	/* Count the number of extractions. */
	for (ii=jj=0;ii<mm;ii++){
		jj+=tqueuerts[ii];
	}

	/* Allocate or reallocate slctordr set if necessary */
	/* and set slctordrsize. */
	if (slctordr==NULL){
		if (NULL==(slctordr=MYALLOC(jj*sizeof(*slctordr)))){
			return(NOMEMORY);
		}
		slctordrsize=jj;
	} else if (slctordrsize!=jj){
		if (NULL==(ptr1=MYREALLOC(slctordr,jj*sizeof(*slctordr)))){
			return(NOMEMORY);
		}
		slctordr=ptr1;
		slctordrsize=jj;
	}

	/* Set slctordr elements by depleting each queue one after the other */
	/* alternating weight and age queues. */
	for (ii=jj=0;ii<selectqueues;ii++){
		for (kk=0;kk<tqueuerts[ii];jj++,kk++){
			slctordr[jj]=ii;
		}
		for (kk=0,nn=ii+selectqueues;kk<tqueuerts[nn];jj++,kk++){
			slctordr[jj]=nn;
		}
	}

	return(0);
} /* SetClSelectOrder */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform problem pre-processing)  OCJ
 *
 *    This function performs the pre-processing needed before the
 *    start of the saturation process or SAT stand alone execution:
 *    - Convert formulas to CNF standard form.
 *    - Perform relevance filtering.
 *    - Update the HORN flag of unprocessed clauses and pbmflags
 *      global variable.
 *
 *
 *  ARGUMENTS:
 *
 *    flag: 1 if called from Saturate() or Saturate2(), 2 if called
 *          from Clausify(), function, 0 otherwise.
 *
 *  RETURNS:
 *
 *    NOMEMORY -> Not enough memory
 *    TIMEOUT -> Timeout condition during pre-process
 *    If the function was called from Clausify function (flag==2)
 *    and there was a formula that generated a $false then the
 *    role of that formula is returned, otherwise 0 is returned.
 *
 *
 *--------------------------------------------------------------*/
int32_t Preprocess(int32_t flag){
	auto char *buffer;                               /* Buffer for CNF formula */
	auto int32_t size;                               /* Buffer size in bytes */
	auto struct itimerval alarmtime;                 /* To disable the global timeout alarm */
	#ifdef MEMCHECK
	auto struct mallinfo2 rsinfo;                    /* For mallinfo2() calls */
	auto uint64_t maxmem;                            /* Maximum memory allowed. */
	auto uint64_t mem1,mem2;                         /* Previous and current memory measurements */
	auto int32_t count,countlimit;                   /* Counter and limit for memory check cycles */
	#endif
	auto cmprefix *ptr1,*ptr2;                       /* Auxiliary pointers */
	auto uint8_t *ptr3;                              /* Auxiliary pointer */
	auto uint64_t ii,jj,kk,nn,mm,pp;                 /* Auxiliary */

	#ifdef MEMCHECK
	/* Compute maximum memory allowed. */
	mem1=GetAvailableMemory();
	if (mem1>=MEMORYLIMIT){
		rsinfo=mallinfo2();
		maxmem=mem1+rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
	} else {
		alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
		alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
		setitimer(ITIMER_REAL,&alarmtime,NULL);
		return(NOMEMORY);
	}
	if (maxmem>memorylimit){
		maxmem=memorylimit;
	}
	#endif

	/* Allocate space for CNF conversion. */
	if (NULL==(buffer=MYALLOC(2*STR_CHUNK_SIZE))){
		alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
		alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
		setitimer(ITIMER_REAL,&alarmtime,NULL);
		return(NOMEMORY);
	}
	size=2*STR_CHUNK_SIZE;

	/* Loop through text formulas in the main KB. */
	if ((verbose==2)&&(pgmmode==INTERACTIVE)){
		printf("Converting formulas to CNF...\n");
	}
	pp=ii=0; /* Just to avoid compiler warnings. */
	#ifdef MEMCHECK
	count=-1;countlimit=1;
	mem1=mem2=0; /* Just to prevent compiler warnings */
	#endif
	for (ptr1=mainkb.firsttxt;ptr1!=NULL;ptr1=ptr1->next){

		/* Formula is of type axiom like, NEGCONJ or PREDICATEDEF. */
		if (((PLAIN>=(ii=ptr1->inference))&&(ptr1->inference!=CONJECTURE))||(ptr1->inference==PREDICATEDEF)){

			/* Memory limit check. */
			#ifdef MEMCHECK
			count++;
			if (count==0){
				rsinfo=mallinfo2();
				mem1=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
			} else if (count>=countlimit){
				count=0;
				rsinfo=mallinfo2();
				mem2=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
				if (maxmem<=mem2){
					procctl->status=NOMEMORY;
					break;
				}
				if (mem2>mem1){
					countlimit=(((double)(maxmem-mem2))*countlimit)/(2*(mem2-mem1));
				}
				mem1=mem2;
			}
			#endif

			/* Text formula pre-processing. Pre-processing is repeated in */
			/* case of premature return due to miniscoping time limit */
			/* exceeded. */
			if (NULL==(ptr2=PreprocessFormula(ptr1,&buffer,&size,1))){
				if ((procctl->status!=NOMEMORY)&&(procctl->status!=TIMEOUT)){
					if (NULL==(ptr2=PreprocessFormula(ptr1,&buffer,&size,0))){
						break;
					}
				} else {
					break;
				}
			}

			/* Formula is $false, refutation found. */
			if (0==strcmp("$false",ptr2->part2.text)){
				procctl->status=UNSATISFIABLE;
				procctl->solvekb=active_cores;
				if (flag==1){
					if (NULL!=(mainkb.frstprfnode=MYALLOC(sizeof(proofnode)))){
						mainkb.frstprfnode->type=ACLAUSE;
						mainkb.frstprfnode->ptr.Aclause=ptr2;
					}
				}
				pp=ii;
				break;

			/* Formula is not $false or $true, set FROMCONJECTURE flag */
			/* if appropriate and compile CNF formula. */
			} else if (0!=strcmp("$true",ptr2->part2.text)){
				if (ptr1->inference==NEGCONJ){
					ptr2->flags|=FROMCONJECTURE;
				} else {
					ptr2->flags|=(FROMCONJECTURE&ptr1->flags);
				}
				if (NOMEMORY==CompileCNF(buffer,ptr2)){
					procctl->status=NOMEMORY;
					break;
				}
			}
		}
	}

	/* Free CNF conversion memory. */
	MYFREE(buffer);

	/* Timeout or not enough memory from PreprocessFormula(). */
	if ((procctl->status==TIMEOUT)||(procctl->status==NOMEMORY)){
		alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
		alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
		setitimer(ITIMER_REAL,&alarmtime,NULL);
		return(procctl->status);
	}

	/* There is not a valid type 8 (UEQ) goal by the moment. */
	ueqgoal=NULL;
	ueqgltype=-1;

	/* Update the HORN flag in each clause and UNITEQUALITY, UNITCLAUSE */
	/* and GROUND flags of pbmflags global variable. */
	/* Loop through unprocessed clauses. */
	/* ii=1 if there are horn clauses, otherwise ii=0. */
	/* jj=1 if there are non horn clauses, otherwise jj=0. */
	/* nn=1 if there are equalities, otherwise nn=0. */
	jj=nn=0;
	if (procctl->status==UNSATISFIABLE){
		ii=1;
	} else {
		ii=0;
		for (ptr1=mainkb.frstunproc;ptr1!=NULL;ptr1=ptr1->next){

			/* Update UNITCLAUSE and UNITEQUALITY flags. */
			if (ptr1->part2.bin->literals>1){
				pbmflags&=(~UNITCLAUSE);
			} else if (EQUALITY==((NEGATED|EQUALITY)&ptr1->part2.bin->formula[0])){
				pbmflags|=UNITEQUALITY;
			}

			/* Check inactivation of GROUND flag in pbmflags variable. */
			if (ptr1->part2.bin->maxvarnb>=-1){
				pbmflags&=(~GROUND);
			}

			/* Check if this is a unit inequality that could be a goal for UEQ problems */
			/* and update the ueqgoal and ueqgltype variables. */
			if ((ptr1->part2.bin->literals==1)&&((NEGATED|EQUALITY)==((NEGATED|EQUALITY)&ptr1->part2.bin->formula[0]))){
				ueqgoal=ptr1;
				switch (ueqgltype){
					case -1:
						if (ptr1->part2.bin->maxvarnb==-1){
							ueqgltype=0;
						} else {
							ueqgltype=1;
						}
						break;
					case 0:
						if (ptr1->part2.bin->maxvarnb==-1){
							ueqgltype=2;
						} else {
							ueqgltype=5;
						}
						break;
					case 1:
						if (ptr1->part2.bin->maxvarnb==-1){
							ueqgltype=4;
						} else {
							ueqgltype=3;
						}
						break;
					case 2:
						if (ptr1->part2.bin->maxvarnb>=0){
							ueqgltype=5;
						}
						break;
					case 3:
						if (ptr1->part2.bin->maxvarnb==-1){
							ueqgltype=4;
						}
						break;
					case 4:
						if (ptr1->part2.bin->maxvarnb>=0){
							ueqgltype=5;
						}
						break;
					case 5:
						if (ptr1->part2.bin->maxvarnb==-1){
							ueqgltype=4;
						}
						break;
				}
			}

			/* Loop through clause items. Set kk to the number of positive */
			/* literals. */
			for (ptr3=&ptr1->part2.bin->formula[0],kk=0;(*ptr3)!=UNITEND;
					ptr3=NextItem(ptr3,IMMED)){
				if ((PREDICATE|EQUALITY)&ptr3[0]){
					if (0==(NEGATED&ptr3[0])){
						kk++;
					}
					if (EQUALITY&ptr3[0]){
						nn=1;
					}
				}
			}

			/* Update ii, jj and clause HORN flag if appropriate. */
			if (kk>1){
				jj=1;
			} else {
				ptr1->flags|=HORN;
				ii=1;
			}
		}
	}

	/* Update NONUNITEQUALITY, HORN and WITHHORNCLAUSES in pbmflags global variable. */
	if (nn&&(0==(pbmflags&UNITEQUALITY))){
		pbmflags|=NONUNITEQUALITY;
	}
	if (ii){
		if (jj){
			pbmflags|=WITHHORNCLAUSES;
		} else {
			pbmflags|=HORN;
		}
	}

	/* If the function was called from Clausify() then accumulate pre-processing elapsed time */
	/* (only if a refutation was found) and return. */
	if (flag==2){
		if (procctl->status==UNSATISFIABLE){
			gettimeofday(&mainkb.endtime,NULL);
			mainkb.prstats.elapsed_time+=(mainkb.endtime.tv_sec-mainkb.time.tv_sec
					+(mainkb.endtime.tv_usec-mainkb.time.tv_usec)/1000000.0);
			pbmstats->elapsed_time=mainkb.prstats.elapsed_time;
			alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
			alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
			setitimer(ITIMER_REAL,&alarmtime,NULL);
			return(pp);
		}
		return(0);
	}

	/* Get problem type number. */
	GetPbmTypeNumber();

	/* If problem is type 8 and the last goal detected has at least one of its */
	/* root terms with arity bigger than 0 and at least one of its arguments */
	/* is not a variable then build the goal transformation definitions */
	/* and definition folding. */
	if (pbmtype==8){

		/* Goal is ground. Build the definition folding clause. */
		if (ueqgoal->part2.bin->maxvarnb==-1){
			ptr3=NextItem(&ueqgoal->part2.bin->formula[0],IMMED);
			if (IsDefinable(ptr3)){
				mm=1;
			} else if (IsDefinable(NextItem(ptr3,OVERSUBTERMS))){
				mm=2;
			} else {
				mm=0;
			}
			if (mm){
				if (0!=DefinitionFolding()){
					if (procctl->status==NOMEMORY){
						return(NOMEMORY);
					}
					for (ptr1=mainkb.lstunproc;(ptr1->inference==GOALDEFINITION)||(ptr1->inference==DEFINTNFOLDING);ptr1=ptr1->prev){
						UnlinkUEQ(ptr1,&mainkb);
						MYFREE(ptr1->part2.bin);
						MYFREE(ptr1);
					}
				}
			}

		/* Goal is not ground. */
		} else {

			/* Build the definition folding clause. */
			ptr3=NextItem(&ueqgoal->part2.bin->formula[0],IMMED);
			if (IsDefinable(ptr3)){
				mm=1;
			} else if (IsDefinable(NextItem(ptr3,OVERSUBTERMS))){
				mm=2;
			} else {
				mm=0;
			}
			if (mm){
				if (0!=DefinitionFolding2(mm)){
					if (procctl->status==NOMEMORY){
						return(NOMEMORY);
					}
					for (ptr1=mainkb.lstunproc;(ptr1->inference==GOALDEFINITION)||(ptr1->inference==DEFINTNFOLDING);ptr1=ptr1->prev){
						UnlinkUEQ(ptr1,&mainkb);
						MYFREE(ptr1->part2.bin);
						MYFREE(ptr1);
					}
				}
			}

			/* Renumber the variables with DEFINTNFOLDING or GOALDEFINITION inference. */
			/* They were not renumbered in AddBinClause2KBUEQ() function because */
			/* it was important to keep the original number during the process. */
			for (ptr1=mainkb.lstunproc;;ptr1=ptr1->prev){
				if (ptr1->inference==DEFINTNFOLDING){
					continue;
				}
				if (ptr1->inference!=GOALDEFINITION){
					break;
				}
				if (NOMEMORY==RenumberVars(ptr1->part2.bin)){
					if (procctl->status==NOMEMORY){
						return(NOMEMORY);
					}
					for (ptr1=mainkb.lstunproc;(ptr1->inference==GOALDEFINITION)||(ptr1->inference==DEFINTNFOLDING);ptr1=ptr1->prev){
						UnlinkUEQ(ptr1,&mainkb);
						MYFREE(ptr1->part2.bin);
						MYFREE(ptr1);
					}
					break;
				}
			}
		}
	}

	/* Perform relevance filtering. */
	mm=ClauseRelevance();

	/* Perform SInE processing. */
	if (InitSInEDist()){
		alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
		alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
		setitimer(ITIMER_REAL,&alarmtime,NULL);
		return(TIMEOUT);
	}

	/* Accumulate preprocessing elapsed time. */
	gettimeofday(&mainkb.endtime,NULL);
	mainkb.prstats.elapsed_time+=(mainkb.endtime.tv_sec-mainkb.time.tv_sec
			+(mainkb.endtime.tv_usec-mainkb.time.tv_usec)/1000000.0);
	pbmstats->elapsed_time=mainkb.prstats.elapsed_time;

	/* Print preprocessing statistics. */
	if ((verbose==2)&&(pgmmode==INTERACTIVE)){
		printf("\nPreprocessing statistics:\n");
		PrintStatData(pbmstats,0);
		if (mm){
			printf("%ld clause(s) deleted by relevance filtering\n",mm);
		}
	}
	if (pgmmode==INTERACTIVE){
		if (flag==1){
			printf("Problems processed: %d, problems solved: %d, now working on problem number %d\n",
					tot_pbms,solved_pbms,tot_pbms+1);
		} else {
			printf("Problems processed: %d, now working on problem number %d\n",tot_pbms,tot_pbms+1);
		}
	}

	return(0);
} /* Preprocess */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform relevance filtering)  OCJ
 *
 *    This function select clauses to be used in the saturation process.
 *    Pruning starts with the function and predicate symbols in the goal.
 *    Every axiom that shares such a symbol is considered relevant. Symbols
 *    in relevant axioms become relevant themselves. The process is then
 *    repeated for the time specified in the program options. This relevance
 *    pruning is complete in the non-equational case if allowed to reach a
 *    fixed point. It only provides a relatively coarse measure, however.
 *
 *    If the global variable prproflags has the CLFILTERINGITERLIMIT flag
 *    then the global variable ppthrshld contains the maximun number of
 *    iterations allowed. Otherwise the global variable ppthrshld contains
 *    the maximum allowed time for the algorithm.
 *
 *    A Sine algorithm can be added in the future for a more fine clause
 *    selection.
 *
 *
 *  ARGUMENTS:
 *
 *    None.
 *
 *  RETURNS:
 *
 *    Number of deleted clauses by the relevance filter.
 *
 *
 *--------------------------------------------------------------*/
uint64_t ClauseRelevance(void){
	auto struct timeval sttime,ctime;                    /* Function start and current times */
	auto double timedif;                                 /* Function elapsed time */
	auto cmprefix *ptr1,*ptr2;                           /* Auxiliary pointers */
	auto uint8_t *ptr3;                                  /* Auxiliary pointer */
	auto int32_t ii,jj,mm,nn;                            /* Auxiliary */
	auto uint64_t kk;                                    /* Auxiliary */

	/* Clear CLFILTERINGLIMITREACHED and CLFILTERINGPRUNNED flag from previous jobs. */
	prproflags&=~(CLFILTERINGLIMITREACHED|CLFILTERINGPRUNNED);

	/* Return if no clause filtering must be done. */
	if ((0==(pbmflags&(EXISTCONJ|EXISTNEGCONJ)))||(prproflags&CLFILTERINGOFF)
			||((prproflags&CLFILTERINGNONEQU)&&((UNITEQUALITY|NONUNITEQUALITY)&pbmflags))){
		return(0);
	}

	/* Get start time if necessary. */
	if ((ppthrshld>0.0)&&(0==(prproflags&CLFILTERINGITERLIMIT))){
		gettimeofday(&sttime,NULL);
	}

	/* Set ptr2 to first main KB unprocessed clause and loop until */
	/* no more clauses are flagged as kept or a timeout occurs. */
	ptr2=mainkb.frstunproc;
	mm=0; /* No clause coming from negated conjecture has been processed yet. */
	for (ii=nn=1;ii;nn++){
		ii=0;

		/* Loop through main KB unprocessed clauses. The pointer ptr2 */
		/* is set to first non kept clause in each iteration. */
		for (ptr1=ptr2;ptr1!=NULL;ptr1=ptr1->next){
			if (ptr1==ptr2){
				ptr2=NULL;
			}

			/* Clause is not flagged as kept. */
			if (0==(KEPTCLAUSE&ptr1->flags)){

				/* Clause is coming from a negated conjecture. */
				if(ptr1->parent1->flags&FROMCONJECTURE){
					jj=mm=1;

				/* Clause is not coming from a negated conjecture. */
				} else {

					/* At least one clause coming from conjecture has been processed. */
					if (mm){

						/* Loop through clause items and check if any of its */
						/* predicate or function symbols are flagged as RELEVANT. */
						for (ptr3=&ptr1->part2.bin->formula[0],jj=0;(jj==0)&&(ptr3[0]!=UNITEND);ptr3=NextItem(ptr3,IMMED)){
							if (((PREDICATE|FUNCTION)&ptr3[0])&&(RELEVANT&((symbol *)&ptr3[1])->symbol->type)){
								jj=1;
							}
						}

					/* No clause coming from negated conjecture has been processed yet. */
					} else {
						jj=0;
					}
				}

				/* Clause must be flagged as kept. Flag the clause */
				/* and all its predicates and functions. Also indicate */
				/* that main loop must be repeated. */
				if (jj){
					ptr1->flags|=KEPTCLAUSE;
					for (ptr3=&ptr1->part2.bin->formula[0];ptr3[0]!=UNITEND;ptr3=NextItem(ptr3,IMMED)){
						if ((PREDICATE|FUNCTION)&ptr3[0]){
							((symbol *)&ptr3[1])->symbol->type|=RELEVANT;
						}
					}
					ii=1;

				/* Clause must not be flagged as kept. Set ptr2 if not already set. */
				} else {
					if (ptr2==NULL){
						ptr2=ptr1;
					}
				}
			}
		}

		/* Check timeout or number of iterations. */
		if (ppthrshld>0.0){
			if (prproflags&CLFILTERINGITERLIMIT){
				if (nn>=ppthrshld){
					if (ii){
						prproflags|=CLFILTERINGLIMITREACHED;
					}
					break;
				}
			} else {
				gettimeofday(&ctime,NULL);
				timedif=ctime.tv_sec-sttime.tv_sec
						+(ctime.tv_usec-sttime.tv_usec)/1000000.0;
				if (timedif>ppthrshld){
					if (ii){
						prproflags|=CLFILTERINGLIMITREACHED;
					}
					break;
				}
			}
		}
	}

	/* Delete non flagged clauses. Unlink() function cannot */
	/* return NOMEMORY because all the clauses are unprocessed. */
	/* None of the clauses have equality data nor asserts so */
	/* there is no need to free that storage. */
	if ((0==(prproflags&CLFILTERINGLIMITREACHED))||(0==(prproflags&CLFILTERINGRESIGN))){
		for (ptr1=mainkb.frstunproc,kk=0;ptr1!=NULL;ptr1=ptr2){
			ptr2=ptr1->next;
			if (0==(KEPTCLAUSE&ptr1->flags)){
				Unlink(ptr1,&mainkb);
				MYFREE(ptr1->part2.bin);
				MYFREE(ptr1);
				kk++;
				prproflags|=CLFILTERINGPRUNNED;
			} else {
				ptr1->flags&=~(KEPTCLAUSE);
			}
		}
	} else {
		kk=0;
	}

	return(kk);
} /* ClauseRelevance */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform SInE processing)  OCJ
 *
 *    This function initializes the sinedist field of binprefix structure
 *    of current unprocessed binary clauses and sinedist field of hashchain
 *    symbol structures. It should be called when the only unprocessed
 *    clauses are the result of the clausification of an input problem.
 *
 *    This function also initializes the kbset.opts.layerset[] set field
 *    depending on the split settings and problem properties.
 *
 *
 *  ARGUMENTS:
 *
 *    None.
 *
 *  RETURNS:
 *
 *    0 if OK or TIMEOUT.
 *
 *
 *--------------------------------------------------------------*/
int32_t InitSInEDist(void){
	auto struct timeval ctime;                           /* Function start and current times */
	auto double timedif;                                 /* Function elapsed time */
	auto cmprefix *lastclause;                           /* Last clause in unselected clauses queue */
	auto int32_t minusecount;                            /* Minimum usecount value in a clause */
	auto cmprefix *ptr1,*ptr2,*ptr4,*ptr5;               /* Auxiliary pointers */
	auto uint8_t *ptr3,*ptr6;                            /* Auxiliary pointers */
	auto int32_t ii,jj,kk;                               /* Auxiliary */

	/* Initialize kbset.opts.layerset[]. */
	/* Return if SInE process cannot be done. */
	if (0==(pbmflags&(EXISTCONJ|EXISTNEGCONJ))){
		if ((0==(pbmflags&UNITCLAUSE))&&(glblopts.split==1)){
			if (0==(pbmflags&HORN)){
				kbset.opts.layerset[0]=HRNLYR;
				kbset.opts.layerset[1]=AVHRNLYR;
			} else {
				kbset.opts.layerset[0]=AVLYR;
				kbset.opts.layerset[1]=NONELYR;
			}
		} else if (0==(pbmflags&HORN)){
			kbset.opts.layerset[0]=HRNLYR;
			kbset.opts.layerset[1]=NONELYR;
		} else {
			kbset.opts.layerset[0]=NONELYR;
			kbset.opts.layerset[1]=NONELYR;
		}
		return(0);
	} else {
		if ((0==(pbmflags&UNITCLAUSE))&&(glblopts.split==1)){
			if (0==(pbmflags&HORN)){
				kbset.opts.layerset[0]=SIHRNLYR;
				kbset.opts.layerset[1]=SIAVLYR;
			} else {
				kbset.opts.layerset[0]=SIAVLYR;
				kbset.opts.layerset[1]=SINELYR;
			}
		} else if (0==(pbmflags&HORN)){
			kbset.opts.layerset[0]=SIHRNLYR;
			kbset.opts.layerset[1]=SINELYR;
		} else {
			kbset.opts.layerset[0]=SINELYR;
			kbset.opts.layerset[1]=NONELYR;
		}
	}

	/* Initialize queue with unselected clauses. Clauses coming from the */
	/* conjecture are preselected. The prevdisp field of cmprefix structure */
	/* is temporarily used to index the unselected clauses queue. */
	/* Loop through main KB unprocessed clauses. */
	for (ptr1=mainkb.frstunproc,lastclause=NULL;ptr1!=NULL;ptr1=ptr1->next){
		if (ptr1->part2.bin->sinedist!=0){
			ptr1->prevdisp=lastclause;
			lastclause=ptr1;
		}
	}

	/* Main loop. This is repeated until no additional clauses are removed */
	/* from the unselected clauses queue. */
	for (ii=kk=1;kk;ii++){

		/* Loop through unselected clauses. */
		kk=0; /* No clauses selected yet. */
		for (ptr1=lastclause,ptr5=NULL;ptr1!=NULL;ptr1=ptr2,ptr5=ptr4){

			/* Prepare next iteration. */
			ptr2=ptr1->prevdisp;
			ptr4=ptr1;

			/* Loop through clause items and calculate the minimum usecount value. */
			/* Also set jj=1 if clause has no symbols, 0 otherwise. */
			for (ptr3=&ptr1->part2.bin->formula[0],minusecount=0x7fffffff,jj=1;
					ptr3[0]!=UNITEND;ptr3=NextItem(ptr3,IMMED)){
				if (ptr3[0]&(FUNCTION|PREDICATE)){
					jj=0;
					if ((0==(ptr3[0]&SKOLEM))&&(((symbol *)&ptr3[1])->symbol->usecount<minusecount)){
						minusecount=((symbol *)&ptr3[1])->symbol->usecount;
					}
				}
			}

			/* If the clause has no symbols then remove the clause from unselected queue */
			/* and set its sinedist to 0. */
			if (jj){
				kk=1; /* Some clauses have been selected in this main loop iteration. */
				if (lastclause==ptr1){
					lastclause=ptr2;
				} else {
					ptr5->prevdisp=ptr1->prevdisp;
				}
				ptr4=ptr5;
				ptr1->part2.bin->sinedist=0;

			/* The clause has symbols. */
			} else {

				/* Loop through clause items. */
				for (ptr3=&ptr1->part2.bin->formula[0];ptr3[0]!=UNITEND;ptr3=NextItem(ptr3,IMMED)){

					/* This item qualifies clause for being selected. */
					if ((ptr3[0]&(FUNCTION|PREDICATE))&&(((symbol *)&ptr3[1])->symbol->sinedist<ii)
							&&(((symbol *)&ptr3[1])->symbol->usecount==minusecount)){

						/* Remove the clause from unselected queue and set its sinedist */
						/* to the current counter value. */
						kk=1; /* Some clauses have been selected in this main loop iteration. */
						if (lastclause==ptr1){
							lastclause=ptr2;
						} else {
							ptr5->prevdisp=ptr1->prevdisp;
						}
						ptr4=ptr5;
						ptr1->part2.bin->sinedist=ii;

						/* Loop through clause items. */
						for (ptr6=&ptr1->part2.bin->formula[0];ptr6[0]!=UNITEND;ptr6=NextItem(ptr6,IMMED)){
							if ((ptr6[0]&(FUNCTION|PREDICATE))&&(0==(ptr6[0]&SKOLEM))
									&&(((symbol *)&ptr6[1])->symbol->sinedist==0xffffffff)){
								((symbol *)&ptr6[1])->symbol->sinedist=ii;
							}
						}

						/* Leave current items loop. */
						break;
					}
				}
			}

			/* Check timeout. */
			if (procctl->status==TIMEOUT){
				gettimeofday(&ctime,NULL);
				timedif=mainkb.prstats.elapsed_time+ctime.tv_sec-mainkb.time.tv_sec
						+(ctime.tv_usec-mainkb.time.tv_usec)/1000000.0;
				glblstats.elapsed_time+=timedif;
				return(TIMEOUT);
			}
		}
	}

	return(0);
} /* InitSInEDist */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Print problem type flags)  OCJ
 *
 *    This function prints the problem type flags in pbmflags global
 *    variable. See global.h for the flags in pbmflags.
 *
 *    In order to do so the problem must be pre-processed.
 *
 *    All main KB (and therefore all KB's data) will be initialized
 *    at the end of the execution of this function.
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
void GoType(void){
	auto char *uniteq,*unitcl,*horn,*ground,*eq;         /* To store "Y" or "N" for the different simple problem types */
	auto struct mallinfo2 rsinfo;                        /* For mallinfo2() calls */
	auto struct itimerval alarmtime;                     /* To define a global timeout alarm */
	auto int32_t ii;                                     /* Auxiliary */

	/* Main KB is empty. */
	if (mainkb.nformulas==0){
		printf("Main KB has no input formulas to process.\n");
		return;
	}

	/* Perform pre-processing. */
	ii=Preprocess(0);
	if (ii){

		/* Print error information. */
		if (ii==NOMEMORY){
			printf("Not enough memory.\n");
		} else {
			printf("Process timeout\n");
		}
		printf("\n      UNIT_EQUALITY UNIT_CLAUSE HORN GROUND NONUNIT_EQU PBM_TYPE\n");
		printf("====>      ?             ?       ?      ?         ?        ?\n");
		FreeInfFormulas();
		if (NOMEMORY==Initialize_KB(&mainkb)){
			printf("No memory available, KB not initialized\n");
		}
		glblstats.elapsed_time+=mainkb.prstats.elapsed_time;
		ResetStats(pbmstats);
		procctl->status=STOPPED;
		tot_pbms++;

		/* Stop time measurement, collect CPU time and return. */
		procctl->end=clock();
		tot_cpu_time+=((double)(procctl->end-procctl->start))/CLOCKS_PER_SEC;
		return;
	}

	/* Disable time alarm. */
	alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
	alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
	setitimer(ITIMER_REAL,&alarmtime,NULL);

	/* Display results. */
	if (pbmflags&UNITEQUALITY){
		uniteq="Y";
	} else {
		uniteq="N";
	}
	if (pbmflags&UNITCLAUSE){
		unitcl="Y";
	} else {
		unitcl="N";
	}
	if (pbmflags&HORN){
		horn="Y";
	} else {
		horn="N";
	}
	if (pbmflags&GROUND){
		ground="Y";
	} else {
		ground="N";
	}
	if (pbmflags&NONUNITEQUALITY){
		eq="Y";
	} else {
		eq="N";
	}
	printf("      UNIT_EQUALITY UNIT_CLAUSE HORN GROUND NONUNIT_EQU PBM_TYPE\n");
	printf("====>      %s             %s       %s      %s         %s        %d\n",
			uniteq,unitcl,horn,ground,eq,1+pbmtype);
	if (pbmtype==8){
		/*    0 si hay un solo goal y es ground
		1 si hay un solo goal y no es ground
		2 si hay varios goals todos ground
		3 si hay varios goals todos no ground
		4 si hay varios goals algunos ground y otros no ground y el último goal es ground
		5 si hay varios goals algunos ground y otros no ground y el último goal es no ground*/
		switch (ueqgltype){
			case 0:
				printf("----> There is only one goal and it is ground.\n");
				break;
			case 1:
				printf("----> There is only one goal and it is not ground.\n");
				break;
			case 2:
				printf("----> There are several goals and all of them are ground.\n");
				break;
			case 3:
				printf("----> There are several goals and none of them are ground.\n");
				break;
			case 4:
				printf("----> There are several goals ground and not ground and the last one is ground.\n");
				break;
			case 5:
				printf("----> There are several goals ground and not ground and the last one is not ground.\n");
				break;
		}
	}

	/* Collect memory use information. */
	rsinfo=mallinfo2();
	if (glblstats.memory<(rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost)){
		glblstats.memory=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
		glblstats.netmemory=rsinfo.uordblks+rsinfo.hblkhd;
	}

	/* Collect inference statistics and initialize main KB. */
	AddInferenceStats(pbmstats,&glblstats);
	FreeInfFormulas();
	if (NOMEMORY==Initialize_KB(&mainkb)){
		printf("No memory available, KB not initialized\n");
	}
	gettimeofday(&mainkb.endtime,NULL);
	mainkb.prstats.elapsed_time=mainkb.endtime.tv_sec-mainkb.time.tv_sec
			+(mainkb.endtime.tv_usec-mainkb.time.tv_usec)/1000000.0;
	glblstats.elapsed_time+=mainkb.prstats.elapsed_time;
	ResetStats(pbmstats);
	tot_pbms++;

	/* Stop time measurement, collect CPU time and return. */
	procctl->status=STOPPED;
	procctl->end=clock();
	tot_cpu_time+=((double)(procctl->end-procctl->start))/CLOCKS_PER_SEC;

	return;
} /* GoType */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Convert problem formulas to CNF and print them in TPTP format)  OCJ
 *
 *    This function converts problem formulas to CNF and prints them
 *    in TPTP format.
 *
 *    In order to do so the problem must be pre-processed.
 *
 *    All main KB (and therefore all KB's data) will be initialized
 *    at the end of the execution of this function.
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
void Clausify(void){
	auto struct mallinfo2 rsinfo;                        /* For mallinfo2() calls */
	auto struct itimerval alarmtime;                     /* To disable the global timeout alarm */
	auto int32_t ii;                                     /* Auxiliary */

	/* Start time measurement. */
	procctl->start=clock();
	gettimeofday(&mainkb.time,NULL);
	kbset.time=mainkb.time;

	/* Main KB is empty. */
	if (mainkb.nformulas==0){
		printf("Main KB has no input formulas to process.\n");
		return;
	}

	/* Perform pre-processing. */
	ii=Preprocess(2);
	if ((ii==NOMEMORY)||(ii==TIMEOUT)){

		/* Print error information. */
		if (ii==NOMEMORY){
			printf("Not enough memory.\n");
		} else {
			printf("Process timeout\n");
		}
		FreeInfFormulas();
		if (NOMEMORY==Initialize_KB(&mainkb)){
			printf("No memory available, KB not initialized\n");
		}
		ResetStats(pbmstats);
		procctl->status=STOPPED;
		tot_pbms++;

		/* Stop time measurement, collect CPU time and return. */
		procctl->end=clock();
		tot_cpu_time+=((double)(procctl->end-procctl->start))/CLOCKS_PER_SEC;
		return;
	}

	/* Disable timeout. */
	alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
	alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
	setitimer(ITIMER_REAL,&alarmtime,NULL);

	/* Display results. */
	if (NOMEMORY==PrintClausifiedFormulas(ii)){
		printf("No memory available, printed information is not complete.\n");
	}

	/* Collect memory use information. */
	rsinfo=mallinfo2();
	if (glblstats.memory<(rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost)){
		glblstats.memory=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
		glblstats.netmemory=rsinfo.uordblks+rsinfo.hblkhd;
	}

	/* Collect inference statistics and initialize main KB. */
	AddInferenceStats(pbmstats,&glblstats);
	FreeInfFormulas();
	if (NOMEMORY==Initialize_KB(&mainkb)){
		printf("No memory available, KB not initialized\n");
	}
	ResetStats(pbmstats);
	tot_pbms++;

	/* Stop time measurement and collect elapsed and CPU times. */
	procctl->end=clock();
	tot_cpu_time+=((double)(procctl->end-procctl->start))/CLOCKS_PER_SEC;
	gettimeofday(&mainkb.endtime,NULL);
	glblstats.elapsed_time+=(mainkb.endtime.tv_sec-mainkb.time.tv_sec
			+(mainkb.endtime.tv_usec-mainkb.time.tv_usec)/1000000.0);
	procctl->status=STOPPED;

	return;
} /* Clausify */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Calculate problem type number)  OCJ
 *
 *    This function calculates a problem type number according to
 *    the following:
 *    LEGEND:
 *    ------
 *    UE: Problem has some positive unit equalities.
 *    NUE: Problem has some equalities none of which are positive unit
 *         equalities.
 *    UC: All clauses in problem are unit clauses.
 *    H: Problem is horn, that is, all clauses contain at most only
 *       one positive literal.
 *    G: Problem is ground, there are no variables in the clauses.
 *
 *    The problem type number is stored in pbmtype global variable.
 *
 *    PROBLEM TYPE NUMBER:
 *    -------------------
 *    0 -> UE&~UC&~H&~G
 *    1 -> UE&~UC&H
 *    2 -> UE&~UC&~H&G
 *    3 -> UE&UC and (numpreds!=0 or ueqgoal==NULL)
 *    4 -> NUE&~UC&~H y NUE&UC
 *    5 -> NUE&~UC&H
 *    6 -> ~UE&~NUE&~UC&~H y ~UE&~NUE&UC
 *    7 -> ~UE&~NUE&~UC&H
 *    8 -> UE&UC and numpreds==0 and ueqgoal!=NULL
 *
 *  ARGUMENTS:
 *
 *    None.
 *
 *  RETURNS:
 *
 *    Problem type number.
 *
 *
 *--------------------------------------------------------------*/
void GetPbmTypeNumber(void){

	/* Calculate problem type number. */
	/* 0 -> UE&~UC&~H&~G */
	/* 1 -> UE&~UC&H */
	/* 2 -> UE&~UC&~H&G */
	/* 3 -> UE&UC and (numpreds!=0 or ueqgoal==NULL).  */
	/* 4 -> NUE&~UC&~H y NUE&UC */
	/* 5 -> NUE&~UC&H */
	/* 6 -> ~UE&~NUE&~UC&~H y ~UE&~NUE&UC */
	/* 7 -> ~UE&~NUE&~UC&H */
	/* 8 -> UE&UC and numpreds==0 and ueqgoal!=NULL.  */
	switch (pbmflags&(UNITEQUALITY|NONUNITEQUALITY|UNITCLAUSE|HORN|GROUND)){
		case UNITEQUALITY:
			pbmtype=0;
			break;
		case UNITEQUALITY|HORN|GROUND:
		case UNITEQUALITY|HORN:
			pbmtype=1;
			break;
		case UNITEQUALITY|GROUND:
			pbmtype=2;
			break;
		case UNITEQUALITY|UNITCLAUSE:
		case UNITEQUALITY|UNITCLAUSE|HORN:
		case UNITEQUALITY|UNITCLAUSE|GROUND:
		case UNITEQUALITY|UNITCLAUSE|HORN|GROUND:
			if ((numpreds==0)&&(ueqgoal!=NULL)){
				pbmtype=8;
			} else {
				pbmtype=3;
			}
			break;
		case NONUNITEQUALITY:
		case NONUNITEQUALITY|GROUND:
		case NONUNITEQUALITY|UNITCLAUSE:
		case NONUNITEQUALITY|UNITCLAUSE|HORN:
		case NONUNITEQUALITY|UNITCLAUSE|GROUND:
		case NONUNITEQUALITY|UNITCLAUSE|HORN|GROUND:
			pbmtype=4;
			break;
		case NONUNITEQUALITY|HORN:
		case NONUNITEQUALITY|HORN|GROUND:
			pbmtype=5;
			break;
		case 0:
		case GROUND:
		case UNITCLAUSE:
		case UNITCLAUSE|HORN:
		case UNITCLAUSE|GROUND:
		case UNITCLAUSE|HORN|GROUND:
			pbmtype=6;
			break;
		/* The following cases are redundant. They are left just for documentation. */
		/* The compiler optimization will remove the redundance. */
		case HORN:
		case HORN|GROUND:
		default:
			pbmtype=7;
			break;
	}
	/* The following is the same as above written in terms of conditionals. */
	/* It is commented out as documentation and clarification. */
	/*if (pbmflags&UNITEQUALITY){
		if (0==(pbmflags&UNITCLAUSE)){
			if (pbmflags&HORN){
				pbmtype=1;
			} else if (pbmflags&GROUND){
				pbmtype=2;
			} else {
				pbmtype=0;
			}
		} else {
			pbmtype=3;
		}
	} else if (pbmflags&NONUNITEQUALITY){
		if ((0==(pbmflags&UNITCLAUSE))&&(pbmflags&HORN)){
			pbmtype=5;
		} else {
			pbmtype=4;
		}
	} else if ((0==(pbmflags&UNITCLAUSE))&&(pbmflags&HORN)){
		pbmtype=7;
	} else {
		pbmtype=6;
	}*/

	return;
} /* GetPbmTypeNumber */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Estimate number of inferences of a given clause)  OCJ
 *
 *    This function gets the estimated number of non simplification
 *    inferences that a clause will generate if added to ACTIVE
 *    clauses.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *
 *  RETURNS:
 *
 *    Lookahead number of inferences.
 *
 *--------------------------------------------------------------*/
uint32_t LookAhead(cmprefix *clause){
	#ifdef LKAHDWITHUNIFY
	auto subst Subst;                                 /* Substitution */
	#endif
	auto uint8_t *equality;                           /* Pointer to equality with root terms in reverse order */
	auto uint8_t *eqterms;                            /* Pointer to equality root terms in reverse order */
	auto uint8_t *queryitm;                           /* Pointer to QueryDscTree() query item */
	auto int32_t size;                                /* Size of eqterms buffer */
	auto uint32_t children;                           /* Number of estimated children of literals */
	auto uint8_t *rttrm1,*rttrm2;                     /* Equality root terms */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5,*ptr6; /* Auxiliary pointers */
	auto int32_t mm,nn,rr,ss,tt;                      /* Auxiliary */

	#ifdef LKAHDWITHUNIFY
	/* Initialize substitution, stack and equality root terms buffer. */
	Subst.buffer=NULL;
	eqterms=equality=NULL;
	size=0;
	#endif

	/* Initialize number of estimated children of selectable literals. */
	eqterms=equality=NULL;
	size=0;
	ptr1=&clause->part2.bin->formula[0];
	ptr3=NextItem(ptr1,OVERSUBTERMS);
	children=0;

	/* If literal is an (in)equality get the equality root terms. */
	if (EQUALITY&ptr1[0]){
		rttrm1=NextItem(ptr1,IMMED);
		rttrm2=NextItem(rttrm1,OVERSUBTERMS);
		mm=2;
	} else {
		rttrm1=rttrm2=NULL;
		mm=1;
	}

	/* Algorithm is not UEQOTT or UEQDSC. */
	if ((kbset.opts.algorithm!=UEQOTT)&&(kbset.opts.algorithm!=UEQDSC)){

		/* Loop for equality symmetry management. */
		for (tt=0;tt<mm;tt++){

			/* This is the first iteration. */
			if (tt==0){
				queryitm=ptr1;

			/* This is the second iteration for an equality literal. */
			/* Build the set of root terms in reverse order. */
			} else {
				rr=rttrm2-rttrm1;
				ss=ptr3-rttrm2;
				if (size==0){
					if (NULL==(equality=MYALLOC(size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
						#ifdef LKAHDWITHUNIFY
						MYFREE(Subst.buffer);
						#endif
						return(NOMEMORY);
					}
					eqterms=&equality[1+sizeof(symbol)];
				} else if (size<(2+sizeof(symbol)+rr+ss)){
					if (NULL==(ptr4=MYREALLOC(equality,size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
						#ifdef LKAHDWITHUNIFY
						MYFREE(Subst.buffer);
						#endif
						MYFREE(equality);
						return(NOMEMORY);
					}
					equality=ptr4;
					eqterms=&equality[1+sizeof(symbol)];
				}
				memcpy(equality,ptr1,1+sizeof(symbol));
				memcpy(eqterms,rttrm2,ss);
				memcpy(&eqterms[ss],rttrm1,rr);
				eqterms[rr+ss]=UNITEND;
				queryitm=equality;
			}

			/* Compute estimated resolution children. */
			if (ptr1[0]&NEGATED){
				ptr4=(((symbol *)&ptr1[1])->symbol)->discr[0];
			} else {
				ptr4=(((symbol *)&ptr1[1])->symbol)->discr[1];
			}
			nn=0;
			while (QueryUnfDscTree(queryitm,&ptr5,ptr4,nn)){
				nn=1;
				#ifdef LKAHDWITHUNIFY
				switch (Unify(queryitm,ptr5,&Subst,ITEM2SEC|NEGATED|INITSUBST)){

					/* Not enough memory. */
					case NOMEMORY:
						MYFREE(Subst.buffer);
						MYFREE(equality);
						return(NOMEMORY);
						break;

					/* Candidate unifies with literal. */
					case 1:
						children++;
						break;
				}
				#else
				children++;
				#endif
			}
		}

		/* Loop trough literal sub-terms. */
		for (ptr2=NextItem(ptr1,IMMED);ptr2<ptr3;ptr2=NextItem(ptr2,IMMED)){

			/* Compute estimated children of paramodulation to subterm. */
			if ((0==(VARIABLE&ptr2[0]))&&((rttrm1==NULL)||((ptr2<rttrm2)&&(SELECTED&rttrm1[0]))
					||((ptr2>=rttrm2)&&(SELECTED&rttrm2[0])))){
				children+=kbset.vartrmidx.used;
				ptr4=(((symbol *)&ptr2[1])->symbol)->discr[0];
				nn=0;
				while (QueryUnfDscTree(ptr2,&ptr5,ptr4,nn)){
					nn=1;
					#ifdef LKAHDWITHUNIFY
					switch (Unify(ptr2,ptr5,&Subst,ITEM2SEC|INITSUBST)){

						/* Not enough memory. */
						case NOMEMORY:
							MYFREE(Subst.buffer);
							MYFREE(equality);
							return(NOMEMORY);
							break;

						/* Candidate unifies with literal. */
						case 1:
							children++;
							break;
					}
					#else
					children++;
					#endif
				}
			}

			/* Compute estimated children of paramodulation from subterm. */
			if ((0==(NEGATED&ptr1[0]))&&((ptr2==rttrm1)||(ptr2==rttrm2))&&(SELECTED&ptr2[0])){
				if (VARIABLE&ptr2[0]){
					children+=kbset.prstats.nbactive;
				} else {
					ptr4=(((symbol *)&ptr2[1])->symbol)->discr[1];
					nn=0;
					while (QueryUnfDscTree(ptr2,&ptr5,ptr4,nn)){
						nn=1;
						#ifdef LKAHDWITHUNIFY
						switch (Unify(ptr5,ptr2,&Subst,ITEM2SEC|INITSUBST)){

							/* Not enough memory. */
							case NOMEMORY:
								MYFREE(Subst.buffer);
								MYFREE(equality);
								return(NOMEMORY);
								break;

							/* Candidate unifies with literal. */
							case 1:
								children++;
								break;
						}
						#else
						children++;
						#endif
					}
				}
			}
		}

	/* Algorithm is UEQOTT or UEQDSC. */
	} else {

		/* Loop through equality root terms. */
		for (ptr2=rttrm1;ptr2!=NULL;ptr2=(ptr2==rttrm1?rttrm2:NULL)){

			/* Iterate if root term is not SELECTED. */
			if (0==(SELECTED&ptr2[0])){
				continue;
			}

			/* Estimate number of critical pairs built to (sub)terms in root term. */
			/* Loop through items of root term. */
			for (ptr6=ptr2,ptr3=NextItem(ptr2,OVERSUBTERMS);ptr6<ptr3;ptr6=NextItem(ptr6,IMMED)){

				/* Iterate if term is a variable. */
				if (VARIABLE&ptr6[0]){
					continue;
				}

				/* Select the discrimination tree to be used. */
				ptr4=(((symbol *)&ptr6[1])->symbol)->discr[0];

				/* Loop through "from" candidates. */
				nn=0;
				while (QueryUnfDscTree(ptr6,&ptr5,ptr4,nn)){
					nn=1;
					#ifdef LKAHDWITHUNIFY
					switch (Unify(ptr6,ptr5,&Subst,ITEM2SEC|INITSUBST)){

						/* Not enough memory. */
						case NOMEMORY:
							MYFREE(Subst.buffer);
							return(NOMEMORY);
							break;

						/* Candidate unifies with literal. */
						case 1:
							children++;
							break;
					}
					#else
					children++;
					#endif
				}
			}

			/* Estimate number of critical pairs built from root term. */
			/* Root term cannot be a variable because it is selected. */
			if (0==(NEGATED&ptr1[0])){
				ptr4=(((symbol *)&ptr2[1])->symbol)->discr[2];
				nn=0;
				while (QueryUnfDscTree(ptr2,&ptr5,ptr4,nn)){
					nn=1;
					#ifdef LKAHDWITHUNIFY
					switch (Unify(ptr5,ptr2,&Subst,ITEM2SEC|INITSUBST)){

						/* Not enough memory. */
						case NOMEMORY:
							MYFREE(Subst.buffer);
							return(NOMEMORY);
							break;

						/* Candidate unifies with literal. */
						case 1:
							children++;
							break;
					}
					#else
					children++;
					#endif
				}
			}
		}
	}

	/* Free memory and return. */
	#ifdef LKAHDWITHUNIFY
	MYFREE(Subst.buffer);
	#endif
	MYFREE(equality);
	return(children);
} /* LookAhead */
