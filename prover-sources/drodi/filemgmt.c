/*
 ============================================================================
 Name        : filemgmt.c
 Author      : Oscar Contreras
 Version     :
 Copyright   : Copyright (C) 2015-2016 Oscar Contreras. All rights reserved.
 Description : First order logic theorem prover in C
 ============================================================================
 */
/****************************************************************
*
*                        Drodi (Knowledge base management module)
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
 *    This source contains the file I/O management functions for Drodi
 *    package.
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
 *  DESCRIPTION: (Translate a binary clause formula to text format)  OCJ
 *
 *    This function converts a compiled binary clause to text format
 *    and stores the result in a buffer that is allocated by the
 *    function. The buffer size is equal to the text length plus the
 *    ending null character.
 *
 *    It is the caller responsibility to free the allocated memory
 *    when appropriate.
 *
 *
 *  ARGUMENTS:
 *
 *    formula: pointer to the formula in binary format.
 *
 *  RETURNS:
 *
 *    Pointer to allocated buffer with clause in text form
 *    or NULL if no memory available or empty clause.
 *
 *
 *--------------------------------------------------------------*/
char *Decompile(binprefix *binp){
	auto char *buffer;                                  /* Buffer for clause in text format */
	auto int32_t size;                                  /* Size of buffer in bytes */
	auto char *ptr1,*ptr4;                              /* Auxiliary pointer */
	auto uint8_t *ptr2,*ptr3;                           /* Auxiliary pointers */
	auto int32_t ii;                                    /* Auxiliary */

	/* Initial buffer allocation. */
	if (NULL==(buffer=MYALLOC(size=STR_CHUNK_SIZE))){
		return(NULL);
	}
	buffer[0]=0;

	/* Empty clause. */
	if (binp->formula[0]==UNITEND){
		strcpy(buffer,"$false");
		ii=6;

	/* Non empty clause. */
	} else {

		/* Loop through literals in clause. */
		for (ptr2=&binp->formula[0],ii=0;(*ptr2)!=UNITEND;
				ptr2=NextItem(ptr2,OVERSUBTERMS)){

			/* If this is not the first literal then add "OR" operator. */
			if (ii){
				if (NOMEMORY==SafeAppendF(&buffer,&size,"|",&ii)){
					MYFREE(buffer);
					return(NULL);
				}
			}

			/* Negated literal. */
			if ((*ptr2)&NEGATED){
				if (NOMEMORY==SafeAppendF(&buffer,&size,"~",&ii)){
					MYFREE(buffer);
					return(NULL);
				}
			}

			/* Literal is an equality. */
			if ((*ptr2)&EQUALITY){

				/* Add first equality term. */
				ptr3=NextItem(ptr2,IMMED);
				if (NOMEMORY==AddTermText(ptr3,&buffer,&size,&ii)){
					MYFREE(buffer);
					return(NULL);
				}

				/* Add equality sign. */
				if (NOMEMORY==SafeAppendF(&buffer,&size,"=",&ii)){
					MYFREE(buffer);
					return(NULL);
				}

				/* Add second equality term. */
				ptr3=NextItem(ptr3,OVERSUBTERMS);
				if (NOMEMORY==AddTermText(ptr3,&buffer,&size,&ii)){
					MYFREE(buffer);
					return(NULL);
				}

			/* Literal is a predicate. */
			} else {
				if (NOMEMORY==AddTermText(ptr2,&buffer,&size,&ii)){
					MYFREE(buffer);
					return(NULL);
				}
			}
		}
	}

	/* Print assertions if necessary. */
	if ((kbset.opts.algorithm!=UEQOTT)&&(kbset.opts.algorithm!=UEQDSC)&&(binp->ovly.asserts!=NULL)){
		if ((proofmode==DRODI)||(syntaxmode==DRODI)){
			if (NOMEMORY==SafeAppendF(&buffer,&size," <- ",&ii)){
				MYFREE(buffer);
				return(NULL);
			}
		}
		if (NULL==(ptr4=DecompileAsserts(binp->ovly.asserts,(proofmode==DRODI)||(syntaxmode==DRODI)?0:1))){
			MYFREE(buffer);
			return(NULL);
		}
		if (NOMEMORY==SafeAppendF(&buffer,&size,ptr4,&ii)){
			MYFREE(buffer);
			return(NULL);
		}
		MYFREE(ptr4);
	}

	/* Resize buffer to exactly fit the string. */
	if (NULL==(ptr1=MYREALLOC(buffer,ii+1))){
		MYFREE(buffer);
		return(NULL);
	}

	return(ptr1);
} /* Decompile */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Translate a binary hashchcmp formula to text format)  OCJ
 *
 *    This function is similar to Decompile() function except that
 *    it doesn't print the assertions. It is called only to print
 *    the formula field buffer of a hashchcmp structure.
 *
 *    It is the caller responsibility to free the allocated memory
 *    when appropriate.
 *
 *
 *  ARGUMENTS:
 *
 *    formula: pointer to the formula in binary format.
 *
 *  RETURNS:
 *
 *    Pointer to allocated buffer with clause in text form
 *    or NULL if no memory available or empty clause.
 *
 *
 *--------------------------------------------------------------*/
char *Decompile2(uint8_t *formula){
	auto char *buffer;                                  /* Buffer for clause in text format */
	auto int32_t size;                                  /* Size of buffer in bytes */
	auto char *ptr1;                                    /* Auxiliary pointer */
	auto uint8_t *ptr2,*ptr3;                           /* Auxiliary pointers */
	auto int32_t ii;                                    /* Auxiliary */

	/* Initial buffer allocation. */
	if (NULL==(buffer=MYALLOC(size=STR_CHUNK_SIZE))){
		return(NULL);
	}
	buffer[0]=0;

	/* Empty clause. */
	if (formula[0]==UNITEND){
		strcpy(buffer,"$false");
		ii=6;

	/* Non empty clause. */
	} else {

		/* Loop through literals in clause. */
		for (ptr2=&formula[0],ii=0;(*ptr2)!=UNITEND;
				ptr2=NextItem(ptr2,OVERSUBTERMS)){

			/* If this is not the first literal then add "OR" operator. */
			if (ii){
				if (NOMEMORY==SafeAppendF(&buffer,&size,"|",&ii)){
					MYFREE(buffer);
					return(NULL);
				}
			}

			/* Negated literal. */
			if ((*ptr2)&NEGATED){
				if (NOMEMORY==SafeAppendF(&buffer,&size,"~",&ii)){
					MYFREE(buffer);
					return(NULL);
				}
			}

			/* Literal is an equality. */
			if ((*ptr2)&EQUALITY){

				/* Add first equality term. */
				ptr3=NextItem(ptr2,IMMED);
				if (NOMEMORY==AddTermText(ptr3,&buffer,&size,&ii)){
					MYFREE(buffer);
					return(NULL);
				}

				/* Add equality sign. */
				if (NOMEMORY==SafeAppendF(&buffer,&size,"=",&ii)){
					MYFREE(buffer);
					return(NULL);
				}

				/* Add second equality term. */
				ptr3=NextItem(ptr3,OVERSUBTERMS);
				if (NOMEMORY==AddTermText(ptr3,&buffer,&size,&ii)){
					MYFREE(buffer);
					return(NULL);
				}

			/* Literal is a predicate. */
			} else {
				if (NOMEMORY==AddTermText(ptr2,&buffer,&size,&ii)){
					MYFREE(buffer);
					return(NULL);
				}
			}
		}
	}

	/* Resize buffer to exactly fit the string. */
	if (NULL==(ptr1=MYREALLOC(buffer,ii+1))){
		MYFREE(buffer);
		return(NULL);
	}

	return(ptr1);
} /* Decompile2 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Translate a SAT clause formula to text format)  OCJ
 *
 *    This function converts a binary SAT clause to text format
 *    and stores the result in a buffer that is allocated by the
 *    function. The buffer size is equal to the text length plus the
 *    ending null character.
 *
 *    It is the caller responsibility to free the allocated memory
 *    when appropriate.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: pointer to the satclause structure of clause in binary
 *            format.
 *
 *  RETURNS:
 *
 *    Pointer to allocated buffer with clause in text form
 *    or NULL if no memory available or empty clause.
 *
 *
 *--------------------------------------------------------------*/
char *DecompileSat(satclause *clause){
	auto char *buffer;                                  /* Buffer for clause in text format */
	auto int32_t size;                                  /* Size of buffer in bytes */
	auto char name[26];                                 /* Name of P-predicate symbol */
	auto char *ptr1;                                    /* Auxiliary pointer */
	auto uint8_t *ptr2;                                 /* Auxiliary pointers */
	auto int32_t ii;                                    /* Auxiliary */

	/* Initial buffer allocation. */
	if (NULL==(buffer=MYALLOC(size=STR_CHUNK_SIZE))){
		return(NULL);
	}
	buffer[0]=0;

	/* Loop through literals in clause. */
	for (ptr2=&clause->formula[0],ii=0;(*ptr2)!=UNITEND;ptr2+=1+sizeof(asymbol)){

		/* If this is not the first literal then add "OR" operator. */
		if (ii){
			if (NOMEMORY==SafeAppendF(&buffer,&size,"|",&ii)){
				MYFREE(buffer);
				return(NULL);
			}
		}

		/* Negated literal. */
		if ((*ptr2)&NEGATIVE){
			if (NOMEMORY==SafeAppendF(&buffer,&size,"~",&ii)){
				MYFREE(buffer);
				return(NULL);
			}
		}

		/* Print literal. */
		sprintf(name,"%s%d%s",SPLPREFIX,((asymbol *)&ptr2[1])->symbol->number,SPLPOSTFIX);
		if (NOMEMORY==SafeAppendF(&buffer,&size,name,&ii)){
			MYFREE(buffer);
			return(NULL);
		}
	}

	/* Resize buffer to exactly fit the string. */
	if (NULL==(ptr1=MYREALLOC(buffer,ii+1))){
		MYFREE(buffer);
		return(NULL);
	}

	return(ptr1);
} /* DecompileSat */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Translate an A-clause assertions to text format)  OCJ
 *
 *    This function converts assertions in binary format to text format
 *    and stores the result in a buffer that is allocated by the
 *    function. The buffer size is equal to the text length plus the
 *    ending null character.
 *
 *    The result is formated so that it can be appropriately appended
 *    to the decompiled A-clause:
 *    If flag=0 then asserts are decompiled in the form:
 *      {assert1, assert2...}
 *    If flag=1 then asserts are decompiled in the form:
 *      |~assert1|~assert...
 *
 *    It is assumed that the asserts pointer is not NULL, this is NOT
 *    checked.
 *
 *    It is the caller responsibility to free the allocated memory
 *    when appropriate.
 *
 *
 *  ARGUMENTS:
 *
 *    asserts: pointer to the assertions in binary format.
 *    flag: 0 -> decompile asserts in DRODI format
 *          1 -> decompile asserts in TPTP format
 *
 *  RETURNS:
 *
 *    Pointer to allocated buffer with clause in text form
 *    or NULL if no memory available or empty clause.
 *
 *
 *--------------------------------------------------------------*/
char *DecompileAsserts(uint8_t *asserts,int32_t flag){
	auto char *buffer;                                  /* Buffer for clause in text format */
	auto int32_t size;                                  /* Size of buffer in bytes */
	auto char name[26];                                 /* Name of P-predicate symbol */
	auto char *ptr1;                                    /* Auxiliary pointer */
	auto uint8_t *ptr2;                                 /* Auxiliary pointers */
	auto int32_t ii;                                    /* Auxiliary */

	/* Initial buffer allocation. */
	if (NULL==(buffer=MYALLOC(size=STR_CHUNK_SIZE))){
		return(NULL);
	}
	if (flag){
		strcpy(buffer,"|");
	} else {
		strcpy(buffer,"{");
	}

	/* Loop through literals in assertions clause. */
	for (ptr2=asserts,ii=1;(*ptr2)!=UNITEND;ptr2+=1+sizeof(asymbol)){

		/* If this is not the first literal then add separator. */
		if (ii!=1){
			if (NOMEMORY==SafeAppendF(&buffer,&size,flag?"|":", ",&ii)){
				MYFREE(buffer);
				return(NULL);
			}
		}

		/* Negated literal. */
		if ((((*ptr2)&NEGATIVE)&&(flag==0))||((0==((*ptr2)&NEGATIVE))&&(flag!=0))){
			if (NOMEMORY==SafeAppendF(&buffer,&size,"~",&ii)){
				MYFREE(buffer);
				return(NULL);
			}
		}

		/* Print literal. */
		sprintf(name,"%s%d%s",SPLPREFIX,((asymbol *)&ptr2[1])->symbol->number,SPLPOSTFIX);
		if (NOMEMORY==SafeAppendF(&buffer,&size,name,&ii)){
			MYFREE(buffer);
			return(NULL);
		}
	}

	/* Add ending bracket. */
	if (flag==0){
		if (NOMEMORY==SafeAppendF(&buffer,&size,"}",&ii)){
			MYFREE(buffer);
			return(NULL);
		}
	}

	/* Resize buffer to exactly fit the string. */
	if (NULL==(ptr1=MYREALLOC(buffer,ii+1))){
		MYFREE(buffer);
		return(NULL);
	}

	return(ptr1);
} /* DecompileAsserts */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Translate a binary term to text format)  OCJ
 *
 *    This function converts a compiled binary term to text format
 *    and places the result in the buffer whose address has been
 *    passed as argument. The term itself may be a variable, a
 *    constant, a function or even a predicate.
 *
 *    The text is placed with the correct offset according to the
 *    current length of the string in the buffer which is also passed
 *    as an argument. The buffer is reallocated and its size is updated
 *    if necessary. The length of the string is updated.
 *
 *
 *  ARGUMENTS:
 *
 *    term: pointer to compiled binary term.
 *    pbuffer: address of pointer to buffer where the text will be
 *             stored.
 *    psize: address of pointer to size of buffer in bytes.
 *    plength: address of pointer to string length.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *
 *--------------------------------------------------------------*/
int32_t AddTermText(uint8_t *term,char **pbuffer,int32_t *psize,int32_t *plength){
	auto hashchain *ptr1;                               /* Auxiliary pointer */
	auto uint8_t *ptr2;                                 /* Auxiliary pointers */
	auto char auxstr[10];                               /* Auxiliary buffer */
	auto int32_t ii;                                    /* Auxiliary */

	/* Term is a variable. */
	if (VARIABLE&term[0]){
		sprintf(auxstr,"X%d",*((int16_t *)&term[1]));
		if (NOMEMORY==SafeAppendF(pbuffer,psize,auxstr,plength)){
			return(NOMEMORY);
		}

	/* Term is a literal or predicate. */
	} else {

		/* Add function or predicate name. */
		ptr1=((symbol *)&term[1])->symbol;
		if (NOMEMORY==SafeAppendF(pbuffer,psize,
				((char *)ptr1)+(sizeof(hashchain)),plength)){
			return(NOMEMORY);
		}

		/* Arity is not zero. */
		if (ptr1->arity>0){

			/* Add open parenthesis. */
			if (NOMEMORY==SafeAppendF(pbuffer,psize,"(",plength)){
				return(NOMEMORY);
			}

			/* Loop through arguments. */
			for (ptr2=NextItem(term,IMMED),ii=0;ii<ptr1->arity;
					ptr2=NextItem(ptr2,OVERSUBTERMS),ii++){

				/* Add comma separator if applicable. */
				if (ii){
					if (NOMEMORY==SafeAppendF(pbuffer,psize,",",plength)){
						return(NOMEMORY);
					}
				}

				/* Add argument term. */
				if (NOMEMORY==AddTermText(ptr2,pbuffer,psize,plength)){
					return(NOMEMORY);
				}
			}

			/* Add closing parenthesis. */
			if (NOMEMORY==SafeAppendF(pbuffer,psize,")",plength)){
				return(NOMEMORY);
			}
		}
	}

	return(0);
} /* AddTermText */

#ifdef DEBUGCODE
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Build substitution data in text form)  OCJ
 *
 *    This function translates to text form the following items:
 *    - First each term passed as argument to the Unify() calling
 *      function is converted to text form.
 *    - Next the substitution itself is converted to text form.
 *
 *    The three conversions are performed on the same buffer,
 *    so results must be checked step by step while debugging.
 *
 *    It is assumed that this function is called from Unify()
 *    and Generalize() functions for terms unification.
 *
 *    This routine allocates and deallocates the necessary memory.
 *    If no memory is available the function returns without any
 *    text translation.
 *
 *
 *  ARGUMENTS:
 *
 *    term1: Pointer to first substitution term.
 *    term2: Pointer to second substitution term.
 *    Subst: Pointer to substitution.
 *    flags: Flags passed to calling Unify() function.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void BuildTxtSubstData(uint8_t *term1,uint8_t *term2,subst *Subst,int32_t flags){
	auto uint8_t sflags;                                 /* Substitution flags */
	auto int16_t varnum;                                 /* Number of substitution variable */
	auto uint8_t *sterm;                                 /* Pointer to substitution term */
	auto char *textbuffer;                               /* Text buffer */
	auto int32_t size;                                   /* Size of textbuffer in bytes. */
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto char *auxptr;                                   /* Auxiliary pointer */
	auto int32_t ii,jj;                                  /* Auxiliary */

	/* Allocate text buffer memory. */
	if (NULL==(textbuffer=MYALLOC(STR_CHUNK_SIZE))){
		return;
	}
	size=STR_CHUNK_SIZE;

	/* Get terms text. */
	textbuffer[0]=0;
	ii=0;
	if (NOMEMORY==AddTermText(term1,&textbuffer,&size,&ii)){
		MYFREE(textbuffer);
		return;
	}
	textbuffer[0]=0;
	if (NOMEMORY==AddTermText(term2,&textbuffer,&size,&ii)){
		MYFREE(textbuffer);
		return;
	}

	/* Build substitution text. */
	strcpy(textbuffer,"{");
	if (Subst->buffer!=NULL){
		for (ptr1=Subst->buffer,ii=0;ptr1[0]!=UNITEND;ptr1=NextItem(sterm,OVERSUBTERMS),ii=1){

			/* Print substitution separator if needed. */
			if (ii){
				SafeAppend(&textbuffer,&size,", ",-1);
			}

			/* Get substitution data. */
			sflags=*ptr1;
			varnum=*((int16_t *)&ptr1[1]);
			sterm=&ptr1[3];

			/* Print substituted variable. */
			jj=20+strlen(textbuffer);
			if (jj>size){
				if (NULL==(auxptr=MYREALLOC(textbuffer,jj+STR_CHUNK_SIZE))){
					MYFREE(textbuffer);
					return;
				}
				textbuffer=auxptr;
				size=jj+STR_CHUNK_SIZE;
			}
			sprintf(textbuffer+strlen(textbuffer),"X%d",varnum);

			/* Print variable identifier if needed. */
			if (flags&(ITEM1SEC|ITEM2SEC)){
				if (sflags&ITEM1SEC){
					if (flags&ITEM1SEC){
						SafeAppend(&textbuffer,&size,"(1)",-1);
					} else {
						SafeAppend(&textbuffer,&size,"(2)",-1);
					}
				} else {
					if (flags&ITEM1SEC){
						SafeAppend(&textbuffer,&size,"(2)",-1);
					} else {
						SafeAppend(&textbuffer,&size,"(1)",-1);
					}
				}
			}

			/* Print variable-term separator. */
			strcat(textbuffer,"/");

			/* Print substituting term. */
			jj=strlen(textbuffer);
			if (NOMEMORY==AddTermText(sterm,&textbuffer,&size,&jj)){
				MYFREE(textbuffer);
				return;
			}

			/* Print term identifier if needed. */
			jj=10+strlen(textbuffer);
			if (jj>size){
				if (NULL==(auxptr=MYREALLOC(textbuffer,jj+STR_CHUNK_SIZE))){
					MYFREE(textbuffer);
					return;
				}
				textbuffer=auxptr;
				size=jj+STR_CHUNK_SIZE;
			}
			if (flags&(ITEM1SEC|ITEM2SEC)){
				if (sflags&ITEM2SEC){
					if (flags&ITEM1SEC){
						strcat(textbuffer,"(1)");
					} else {
						strcat(textbuffer,"(2)");
					}
				} else {
					if (flags&ITEM1SEC){
						strcat(textbuffer,"(2)");
					} else {
						strcat(textbuffer,"(1)");
					}
				}
			}
		}
	}
	SafeAppend(&textbuffer,&size,"}",-1);
	MYFREE(textbuffer);
	return;
} /* BuildTxtSubstData */
#endif

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Display statistics)  OCJ
 *
 *    This function displays the proof if it exist, statistics related to
 *    number of inferred and discarded clauses, cpu time and elapsed time.
 *    If called for the last theorem results then this function must be
 *    called by the winning process in case of success or by the parent
 *    process otherwise.
 *
 *    When printing last theorem statistics the function Initialize_KB()
 *    and the function Initialize_hashcmp() functions for the winning KB
 *    are called by this function because the PrintProof() function
 *    (called by this function) must be executed before KB initialization
 *    and the KB initialization must be done before displaying memory
 *    statistics as that information must be collected while initializing
 *    the KB.
 *
 *
 *  ARGUMENTS:
 *
 *    flag: If 0 then last theorem statistics are displayed.
 *          otherwise total session statistics are displayed.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *--------------------------------------------------------------*/
void PrintStats(int32_t flag){
	auto struct mallinfo2 rsinfo;                        /* For mallinfo2() calls */
	auto proofnode *prfnode;                             /* Proof node */
	auto kbase *pkb;                                     /* Pointer to KB */
	auto char *ptr1,*ptr2,*ptr3,*ptr4;                   /* Auxiliary pointers */
	auto proofnode *ptr5;                                /* Auxiliary pointer */
	auto float dd;                                       /* Auxiliary */

	/* Print total session statistics. */
	if (pgmmode==NONINTERACTIVE){
		ptr4="% ";
	} else {
		ptr4="";
	}
	if (flag){

		/* Return if no runs have been performed or verbose is off */
		/* and mode is noninteractive. */
		if ((0==tot_pbms)||((verbose==0)&&(pgmmode==NONINTERACTIVE))){
			return;
		}

		/* Print total session information. */
		printf("%sTotal session statistics:\n",ptr4);
		printf("%sProblems processed: %d\n",ptr4,tot_pbms);
		printf("%sProblems solved: %d\n",ptr4,solved_pbms);
		PrintStatData(&glblstats,1);
		if (cpuavail){
			printf("%sCPU time: %.6f seconds\n",ptr4,tot_cpu_time);
		}

		/* Print total session memory use information. */
		dd=FormatMemory(glblstats.memory,&ptr1);
		printf("%sTotal memory used: %.3f %s\n",ptr4,dd,ptr1);
		dd=FormatMemory(glblstats.netmemory,&ptr1);
		printf("%sNet memory used: %.3f %s\n",ptr4,dd,ptr1);

	/* Print last theorem statistics. */
	} else {

		#ifdef DEBUGTREE
		/* Perform discrimination trees integrity checking. */
		if ((strategy!=0xffffffffffffffff)||(usrparam_mask!=0)){
			CheckDscTrees();
		}
		#endif

		/* Build logical data identifier from input file name. */
		if (importfile!=NULL){
			if (NULL==(ptr2=strrchr(importfile,WPATHSEPARATOR))){
				ptr2=importfile;
			} else {
				ptr2++;
			}
			if (NULL!=(ptr3=strrchr(importfile,'.'))){
				if (ptr3>ptr2){
					*ptr3=0;
				} else {
					ptr3=NULL;
				}
			}
		} else {
			ptr2="user input";
			ptr3=NULL;
		}

		#ifdef BENCHMARKING
		#if VERIFYSATFINALSTATUS == 1
		VerifySatStatus();
		#endif
		if (BENCHTYPE&1){
			if (((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE))
					&&(procctl->solvekb>=0)&&(procctl->solvekb!=active_cores)){
				bresults[0][*bncounter]=mainkb.prstats.elapsed_time;
			} else {
				switch (procctl->status){
					case NOMEMORY:
						bresults[0][*bncounter]=-1.0;
						break;
					case TIMEOUT:
						bresults[0][*bncounter]=-2.0;
						break;
					case UNKNOWN:
						bresults[0][*bncounter]=-3.0;
						break;
					case UNSATISFIABLE:
						bresults[0][*bncounter]=-4.0;
						break;
				}
			}
			if (BENCHTYPE==1){
				(*bncounter)++;
			}
		}
		#endif

		/* Get address of pointer to KB. */
		if (procctl->solvekb==active_cores){
			pkb=&mainkb;
		} else {
			pkb=&kbset;
		}

		/* Print theorem results and winning strategy statistics. */
		switch (procctl->status){
			case NOMEMORY:
				if (pgmmode==NONINTERACTIVE){
					printf("%% SZS status MemoryOut for %s: ",ptr2);
				}
				printf("Not enough memory.\n");
				break;
			case TIMEOUT:
				if (pgmmode==NONINTERACTIVE){
					printf("%% SZS status Timeout for %s: ",ptr2);
				}
				printf("Process timeout.\n");
				break;
			case UNKNOWN:
				if (pgmmode==NONINTERACTIVE){
					printf("%% SZS status GaveUp for %s: ",ptr2);
				}
				if (pbmflags&EXISTCONJ){
					printf("Theorem is unknown\n");
				} else {
					printf("Theory is unknown\n");
				}
				break;
			case SATISFIABLE:
				if (EXISTCONJ==(pbmflags&(EXISTCONJ|CNFFORMAT))){
					if (pgmmode==NONINTERACTIVE){
						printf("%% SZS status CounterSatisfiable for %s: ",ptr2);
					}
					printf("Theorem is counter-satisfiable, conjecture is false\n");
				} else {
					if (pgmmode==NONINTERACTIVE){
						printf("%% SZS status Satisfiable for %s: ",ptr2);
					}
					printf("Theory is satisfiable\n");
				}
				if (pgmmode==NONINTERACTIVE){
					printf("%% SZS output start Saturation for %s\n",ptr2);
				} else {
					printf("Start saturation for %s\n",ptr2);
				}
				if (ptr3!=NULL){
					*ptr3='.';
				}
				if (NOMEMORY==BuildSaturation()){
					for (prfnode=pkb->frstprfnode;prfnode!=NULL;prfnode=ptr5){
						ptr5=prfnode->nextnode;
						MYFREE(prfnode);
					}
					pkb->frstprfnode=NULL;
					printf("%s=============================================\n",ptr4);
					printf("%sNot enough memory to print saturation data...\n",ptr4);
					printf("%s=============================================\n",ptr4);
				} else {
					if (NOMEMORY==PrintProof()){
						for (prfnode=pkb->frstprfnode;prfnode!=NULL;prfnode=ptr5){
							ptr5=prfnode->nextnode;
							MYFREE(prfnode);
						}
						pkb->frstprfnode=NULL;
						printf("%s=============================================\n",ptr4);
						printf("%sNot enough memory to print saturation data...\n",ptr4);
						printf("%s=============================================\n",ptr4);
					} else {
						if (NOMEMORY==PrintSatModel()){
							printf("%s==================================================\n",ptr4);
							printf("%sNot enough memory to print the saturation model...\n",ptr4);
							printf("%s==================================================\n",ptr4);
						}
					}
				}
				if (pgmmode==NONINTERACTIVE){
					printf("%% SZS output end Saturation for %s\n",ptr2);
				} else {
					printf("End saturation for %s\n",ptr2);
				}
				if ((procctl->solvekb!=active_cores)&&(verbose)){
					printf("%s\n%sWinning strategy statistics:\n",ptr4,ptr4);
					PrintProcessSettings();
					PrintStatData(&kbset.prstats,0);
				}
				ResetStats(&kbset.prstats);
				procctl->prfprntinproc=0;
				break;
			case UNSATISFIABLE:
				if (pgmmode==NONINTERACTIVE){
					printf("%% ");
				}
				printf("Refutation found\n");
				if (NOMEMORY==BuildProof()){
					for (prfnode=pkb->frstprfnode;prfnode!=NULL;prfnode=ptr5){
						ptr5=prfnode->nextnode;
						MYFREE(prfnode);
					}
					pkb->frstprfnode=NULL;
					if (EXISTCONJ==(pbmflags&(EXISTCONJ|CNFFORMAT))){
						if (pgmmode==NONINTERACTIVE){
							printf("%% SZS status Theorem for %s: ",ptr2);
						}
						printf("Theorem is valid or has contradictory axioms but not enough memory to build proof\n");
					} else {
						if (pgmmode==NONINTERACTIVE){
							printf("%% SZS status Unsatisfiable for %s: ",ptr2);
						}
						printf("Theory is unsatisfiable but not enough memory to build proof\n");
					}
					printf("%s=============================================\n",ptr4);
					printf("%sNot enough memory to build and print proof...\n",ptr4);
					printf("%s=============================================\n",ptr4);
					if (ptr3!=NULL){
						*ptr3='.';
					}
				} else {
					if (EXISTCONJ==(pbmflags&(EXISTCONJ|CNFFORMAT))){
						if (pbmflags&NOTCAX){
							if (pgmmode==NONINTERACTIVE){
								printf("%% SZS status Theorem for %s: ",ptr2);
							}
							printf("Theorem is valid\n");
						} else {
							if (pgmmode==NONINTERACTIVE){
								printf("%% SZS status ContradictoryAxioms for %s: ",ptr2);
							}
							printf("Theorem has contradictory axioms\n");
						}
					} else {
						if (pgmmode==NONINTERACTIVE){
							printf("%% SZS status Unsatisfiable for %s: ",ptr2);
						}
						printf("Theory is unsatisfiable\n");
					}
					if (pgmmode==NONINTERACTIVE){
						printf("%% SZS output start CNFRefutation for %s\n",ptr2);
					}
					if (ptr3!=NULL){
						*ptr3='.';
					}
					if (NOMEMORY==PrintProof()){
						for (prfnode=pkb->frstprfnode;prfnode!=NULL;prfnode=ptr5){
							ptr5=prfnode->nextnode;
							MYFREE(prfnode);
						}
						pkb->frstprfnode=NULL;
						printf("%s=============================================\n",ptr4);
						printf("%sNot enough memory to build and print proof...\n",ptr4);
						printf("%s=============================================\n",ptr4);
					}
					if (pgmmode==NONINTERACTIVE){
						printf("%% SZS output end CNFRefutation for %s\n",ptr2);
					}
					if ((learnto!=NULL)&&(pbmflags&(EXISTCONJ|EXISTNEGCONJ))){
						LearnFromProblem();
					}
				}
				if ((procctl->solvekb!=active_cores)&&(verbose)){
					printf("%s\n%sWinning strategy statistics:\n",ptr4,ptr4);
					PrintProcessSettings();
					PrintStatData(&kbset.prstats,0);
				}
				ResetStats(&kbset.prstats);
				procctl->prfprntinproc=0;
				break;
		}

		#ifdef BENCHMARKING
		if (BENCHTYPE&2){
			switch (procctl->status){
				case NOMEMORY:
					bresults[1][*bncounter]=-1.0;
					break;
				case TIMEOUT:
					bresults[1][*bncounter]=-2.0;
					break;
				case UNKNOWN:
					bresults[1][*bncounter]=-3.0;
					break;
				case SATISFIABLE:
					if (EXISTCONJ==(pbmflags&(EXISTCONJ|CNFFORMAT))){
						bresults[1][*bncounter]=1.0; /* Counter-satisfiable. */
					} else {
						bresults[1][*bncounter]=2.0; /* Satisfiable. */
					}
					break;
				case UNSATISFIABLE:
					if (EXISTCONJ==(pbmflags&(EXISTCONJ|CNFFORMAT))){
						if (pbmflags&NOTCAX){
							bresults[1][*bncounter]=3.0; /* Valid theorem. */
						} else {
							bresults[1][*bncounter]=4.0; /* Contradictory axioms. */
						}
					} else {
						bresults[1][*bncounter]=5.0; /* Unsatisfiable. */
					}
					break;
			}
			(*bncounter)++;
		}
		#endif

		/* If this is the solving process or KB data has been printed then reinitialize working KB. */
		if ((procctl->solvekb==procnb)||((prtkbdata!=0)&&(active_cores==1))){
			Initialize_hashcmp();
			Initialize_KB(&kbset);
		}

		/* Print solving KB memory use information. */
		if (verbose&&(procctl->solvekb==procnb)){
			rsinfo=mallinfo2();
			dd=FormatMemory(rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost,&ptr1);
			printf("%sTotal memory used by solving process: %.3f %s\n",ptr4,dd,ptr1);
			dd=FormatMemory(rsinfo.uordblks+rsinfo.hblkhd,&ptr1);
			printf("%sNet memory used by solving process: %.3f %s\n",ptr4,dd,ptr1);
		}
	}

	return;
} /* PrintStats */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Display statistics data)  OCJ
 *
 *    This function displays the statistics data in a stats structure.
 *
 *
 *  ARGUMENTS:
 *
 *    stts: Pointer to statistics structure.
 *    flag: If not zero then print all data even if VERBOSE option is OFF.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *--------------------------------------------------------------*/
void PrintStatData(stats *stts,int32_t flag){
	auto char *ptr1;                                     /* Auxiliary pointer */
	if (pgmmode==NONINTERACTIVE){
		ptr1="% ";
	} else {
		ptr1="";
	}
	if (verbose||flag){
		if (stts->tautologies){
			printf("%sNumber of syntactic tautologies detected: %lu\n",ptr1,stts->tautologies);
		}
		#ifdef SEMANTICTAUTOLOGY
		if (stts->semtautologies){
			printf("%sNumber of semantic tautologies detected: %lu\n",ptr1,stts->semtautologies);
		}
		#endif
		if (stts->fwdsubsum){
			printf("%sNumber of forward subsumptions: %lu\n",ptr1,stts->fwdsubsum);
		}
		if (stts->bcksubsum){
			printf("%sNumber of backward subsumptions: %lu\n",ptr1,stts->bcksubsum);
		}
		if (stts->removedups){
			printf("%sNumber of duplicate literals removal simplifications: %lu\n",ptr1,stts->removedups);
		}
		if (stts->triveqres){
			printf("%sNumber of trivial equality resolution simplifications: %lu\n",ptr1,stts->triveqres);
		}
		if (stts->desteqres){
			printf("%sNumber of destructive equality resolution simplifications: %lu\n",ptr1,stts->desteqres);
		}
		if (stts->fwdemodulations){
			printf("%sNumber of forward demodulation simplifications: %lu\n",ptr1,stts->fwdemodulations);
		}
		if (stts->bkdemodulations){
			printf("%sNumber of backward demodulation simplifications: %lu\n",ptr1,stts->bkdemodulations);
		}
		if (stts->fwsubsresltns){
			printf("%sNumber of forward subsumption resolution simplifications: %lu\n",ptr1,stts->fwsubsresltns);
		}
		if (stts->bksubsresltns){
			printf("%sNumber of backward subsumption resolution simplifications: %lu\n",ptr1,stts->bksubsresltns);
		}
		if (stts->factors){
			printf("%sNumber of factoring inferences: %lu\n",ptr1,stts->factors);
		}
		if (stts->resolutions){
			printf("%sNumber of resolution inferences: %lu\n",ptr1,stts->resolutions);
		}
		if (stts->paramodulations){
			printf("%sNumber of paramodulation inferences: %lu\n",ptr1,stts->paramodulations);
		}
		if (stts->splits){
			printf("%sNumber of split inferences: %lu\n",ptr1,stts->splits);
		}
		if (stts->equresolutions){
			printf("%sNumber of equality resolution inferences: %lu\n",ptr1,stts->equresolutions);
		}
		if (stts->equfactorings){
			printf("%sNumber of equality factoring inferences: %lu\n",ptr1,stts->equfactorings);
		}
		if (stts->eqsplits){
			printf("%sNumber of equality split inferences: %lu\n",ptr1,stts->eqsplits);
		}
		if (stts->prennfxform){
			printf("%sNumber of pre-nnf conversion inferences: %lu\n",ptr1,stts->prennfxform);
		}
		if (stts->nnfxform){
			printf("%sNumber of nnf conversion inferences: %lu\n",ptr1,stts->nnfxform);
		}
		if (stts->miniscoping){
			printf("%sNumber of miniscoping inferences: %lu\n",ptr1,stts->miniscoping);
		}
		if (stts->skolemize){
			printf("%sNumber of skolemization inferences: %lu\n",ptr1,stts->skolemize);
		}
		if (stts->predicatedef){
			printf("%sNumber of predicate definitions: %lu\n",ptr1,stts->predicatedef);
		}
		if (stts->formularenaming){
			printf("%sNumber of formula renaming inferences: %lu\n",ptr1,stts->formularenaming);
		}
		if (stts->cnfconversion){
			printf("%sNumber of CNF conversion inferences: %lu\n",ptr1,stts->cnfconversion);
		}
		if (stts->tfsimplif){
			printf("%sNumber of true and false simplifications: %lu\n",ptr1,stts->tfsimplif);
		}
		if (stts->sattautologies){
			printf("%sNumber of SAT tautologies detected: %lu\n",ptr1,stts->sattautologies);
		}
		if (stts->satremovedups){
			printf("%sNumber of duplicate SAT literals removal simplifications: %lu\n",ptr1,stts->satremovedups);
		}
		if (stts->satsubsum){
			printf("%sNumber of SAT subsumptions: %lu\n",ptr1,stts->satsubsum);
		}
		#ifdef SEMANTICTAUTOLOGY
		printf("%sTotal number of inferences: %lu\n",ptr1,stts->tautologies+stts->semtautologies
				+stts->fwdsubsum+stts->resolutions+stts->paramodulations+stts->fwdemodulations
				+stts->bkdemodulations+stts->removedups+stts->triveqres+stts->desteqres
				+stts->equresolutions+stts->bcksubsum+stts->equfactorings+stts->fwsubsresltns
				+stts->bksubsresltns+stts->splits+stts->prennfxform+stts->nnfxform
				+stts->miniscoping+stts->skolemize+stts->predicatedef+stts->formularenaming
				+stts->cnfconversion+stts->tfsimplif+stts->sattautologies
				+stts->eqsplits+stts->satremovedups+stts->satsubsum);
		#else
		printf("%sTotal number of inferences: %lu\n",ptr1,stts->tautologies
				+stts->fwdsubsum+stts->resolutions+stts->paramodulations+stts->fwdemodulations
				+stts->bkdemodulations+stts->removedups+stts->triveqres+stts->desteqres
				+stts->equresolutions+stts->bcksubsum+stts->equfactorings+stts->fwsubsresltns
				+stts->bksubsresltns+stts->splits+stts->prennfxform+stts->nnfxform
				+stts->miniscoping+stts->skolemize+stts->predicatedef+stts->formularenaming
				+stts->cnfconversion+stts->tfsimplif+stts->sattautologies
				+stts->eqsplits+stts->satremovedups+stts->satsubsum);
		#endif
	}
	printf("%sElapsed time: %.6f seconds\n",ptr1,stts->elapsed_time);
	return;
} /* PrintStatData */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Print process option settings)  OCJ
 *
 *    This function prints the "opts" field settings
 *    of the working KB.
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
void PrintProcessSettings(void){
	auto char *ptr1;                                     /* Auxiliary pointer */
	if (pgmmode==NONINTERACTIVE){
		ptr1="% ";
	} else {
		ptr1="";
	}
	printf("%sAlgorithm: ",ptr1);
	switch (kbset.opts.algorithm){
		case OTTER:
		default:
			printf("OTTER\n");
			break;
		case DISCOUNT:
			printf("DISCOUNT\n");
			break;
		case UEQOTT:
			printf("UEQOTT\n");
			break;
		case UEQDSC:
			printf("UEQDSC\n");
			break;
	}
	printf("%sLiteral selection: ",ptr1);
	switch (kbset.opts.select){
		case MAXIMAL:
			printf("MAXIMAL\n");
			break;
		case SINGLENEG:
			printf("SINGLENEG\n");
			break;
		case MULTINEG:
			printf("MULTINEG\n");
			break;
		case SINGLEPOS:
			printf("SINGLEPOS\n");
			break;
		case MULTIPOS:
			printf("MULTIPOS\n");
			break;
		case SELECTALL:
			printf("ALL\n");
			break;
		case MXSINGLENEG:
			printf("MXSINGLENEG\n");
			break;
		case TYPE1:
			printf("TYPE1\n");
			break;
		case TYPE2:
			printf("TYPE2\n");
			break;
		case TYPE3:
			printf("TYPE3\n");
			break;
		case TYPE4:
			printf("TYPE4\n");
			break;
		case TYPE5:
			printf("TYPE5\n");
			break;
		case TYPE6:
			printf("TYPE6\n");
			break;
		case TYPE1CPL:
			printf("TYPE1CPL\n");
			break;
		case TYPE2CPL:
			printf("TYPE2CPL\n");
			break;
		case TYPE3CPL:
			printf("TYPE3CPL\n");
			break;
		case TYPE4CPL:
			printf("TYPE4CPL\n");
			break;
		case TYPE5CPL:
			printf("TYPE5CPL\n");
			break;
		case TYPE6CPL:
			printf("TYPE5CPL\n");
			break;
		default:
			printf("INVALID SETTING\n");
			break;
	}
	printf("%sLAYER: ",ptr1);
	if (kbset.opts.layer==0){
		printf("1\n");
	} else {
		printf("2\n");
	}
	switch (kbset.opts.algorithm){
		case OTTER:
		case DISCOUNT:
		default:
			printf("%sDemodulation is set to ",ptr1);
			switch (kbset.opts.demodulation){
				case DEMODOFF:
					printf("OFF\n");
					break;
				case DEMODON:
					printf("ON\n");
					break;
				case DEMODCPL1:
					printf("CPL1\n");
					break;
				case DEMODCPL2:
					printf("CPL2\n");
					break;
				case DEMODONORNT:
					printf("ONORNT\n");
					break;
				case DEMODCPL1ORNT:
					printf("CPL1ORNT\n");
					break;
				case DEMODCPL2ORNT:
					printf("CPL2ORNT\n");
					break;
			}
			break;
		case UEQOTT:
		case UEQDSC:
			printf("%sConnectedness is set to %s\n",ptr1,kbset.opts.connect?"ON":"OFF");
			printf("%sGround joinability is set to %s\n",ptr1,kbset.opts.grjoin==1?"ON":
					(kbset.opts.grjoin==0?"OFF":"MED"));
			break;
	}
	printf("%sFactoring is %s\n",ptr1,kbset.opts.factoring?"ENABLED":"DISABLED");
	printf("%sTerm ordering: %s\n",ptr1,kbset.opts.termord==STANDARD?"STANDARD":"NONRECURSIVE");
	printf("%sLiteral ordering: %s\n",ptr1,kbset.opts.litord==STANDARD?"STANDARD":"NONRECURSIVE");
	printf("%sFunction weight: %s\n",ptr1,kbset.opts.fweight==UNIFORM?"UNIFORM":"ARITY");
	if (kbset.opts.lookahead){
		printf("%sLookahead is enabled\n",ptr1);
	} else {
		printf("%sLookahead is disabled\n",ptr1);
	}
	if ((pbmtype==3)||(pbmtype==8)){
		if (kbset.opts.weakrw){
			printf("%sWeak rewriting is enabled\n",ptr1);
		} else {
			printf("%sWeak rewriting is disabled\n",ptr1);
		}
	}
	printf("%sWeight limit ",ptr1);
	if (kbset.opts.mxwghtopt&MXWEIGHTOFF){
		printf("is disabled, ");
	} else {
		printf("is set to %d, ",kbset.opts.maxweight);
	}
	if (kbset.opts.algorithm==OTTER){
		if (kbset.opts.mxwghtopt&(MXWEIGHTLRS|MXWEIGHTLRSOTT)){
			printf("LRS is enabled\n");
		} else {
			printf("LRS is disabled\n");
		}
	} else {
		if (kbset.opts.mxwghtopt&(MXWEIGHTLRS|MXWEIGHTLRSDSC)){
			printf("LRS is enabled\n");
		} else {
			printf("LRS is disabled\n");
		}
	}
	printf("%sLearning was ",ptr1);
	if (kbset.lrnvector==NULL){
		printf("disabled for this strategy\n");
	} else {
		printf("enabled for this strategy\n");
	}
	printf("%sSplitting is %s\n",ptr1,kbset.opts.split?"ENABLED":"DISABLED");
	printf("%sTimeout: %.1f seconds\n",ptr1,kbset.opts.timeout);
	return;
} /* PrintProcessSettings */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Build theorem proof)  OCJ
 *
 *    This function builds the proof found.
 *
 *
 *  ARGUMENTS:
 *
 *    None.
 *
 *  RETURNS:
 *
 *    0 -> Proof built successfully.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t BuildProof(void){
	auto kbase *pkb;                                /* Pointer to KB */

	/* Get address of pointer to KB. */
	if (procctl->solvekb==active_cores){
		pkb=&mainkb;
	} else {
		pkb=&kbset;
	}

	/* Check if proof contains a SAT refutation. */
	if (pkb->frstprfnode->type==SATCLAUSE){
		pbmflags|=SATREFUTATION;
	}

	/* Link proof root node to proof queue. */
	pkb->frstprfnode->nextnode=pkb->frstprfnode->prevnode=NULL;

	/* Proof queue root node is a SAT clause. */
	/* Build the proof queue and return. */
	if (pkb->frstprfnode->type==SATCLAUSE){
		return(AddSatCl2Proof());
	}

	/* Proof queue root node is an A-clause.*/
	/* Build the proof queue and return. */
	pkb->frstprfnode->backptr=pkb->frstprfnode->nextnode=pkb->frstprfnode->prevnode=NULL;
	return(AddAclause2Proof(pkb->frstprfnode));
} /* BuildProof */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Build satisfiable/counter-satisfiable saturation output)  OCJ
 *
 *    This function builds saturation data for satisfiable or
 *    counter-satisfiable problems. Saturation data is a listing of
 *    active clauses, locked clauses and SAT solver global clauses
 *    and all their ancestors.
 *
 *
 *  ARGUMENTS:
 *
 *    None.
 *
 *  RETURNS:
 *
 *    0 -> Saturation data was built successfully.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t BuildSaturation(void){
	auto proofnode *node,*lastnode;                 /* Output queue A-clause nodes */
	auto hashchcmp *component;                      /* Pointer to component */
	auto proofnode *snode,*newsnode;                /* SAT current and new output queue nodes */
	auto cmprefix *ptr1;                            /* Auxliary pointer */
	auto satclause *ptr2;                           /* Auxliary pointer */
	auto uint8_t *ptr3;                             /* Auxliary pointer */

	/* Process active clauses. */
	lastnode=NULL; /* Just to prevent compiler warnings. */
	for (ptr1=kbset.frstactive;ptr1!=NULL;ptr1=ptr1->next){

		/* The clause is not yet in the output queue. */
		if (0==(INPROOFQUEUE&ptr1->flags)){

			/* Allocate and set a new output queue node. */
			if (NULL==(node=MYALLOC(sizeof(proofnode)))){
				kbset.status=NOMEMORY;
				return(NOMEMORY);
			}
			node->type=ACLAUSE;
			node->backptr=NULL;
			node->ptr.Aclause=ptr1;

			/* This is the first item to be added to the output queue. */
			/* Initialize the first node of the output queue. */
			if (ptr1==kbset.frstactive){
				node->nextnode=node->prevnode=NULL;
				kbset.frstprfnode=node;

			/* This is not the first item to be added to the output queue. */
			/* Link the new node to the output queue. */
			} else {
				LinkProofNode(node,lastnode);
			}

			/* Add clause and its ancestors to the output queue. */
			if (NOMEMORY==AddAclause2Proof(node)){
				return(NOMEMORY);
			}
			lastnode=node;
		}
	}

	/* Process locked clauses. */
	for (ptr1=kbset.frstlocked;ptr1!=NULL;ptr1=ptr1->next){

		/* The clause is not yet in the output queue. */
		if (0==(INPROOFQUEUE&ptr1->flags)){

			/* Allocate, set and link a new output queue node. */
			if (NULL==(node=MYALLOC(sizeof(proofnode)))){
				kbset.status=NOMEMORY;
				return(NOMEMORY);
			}
			node->type=ACLAUSE;
			node->backptr=NULL;
			node->ptr.Aclause=ptr1;
			LinkProofNode(node,lastnode);

			/* Add clause and its ancestors to the output queue. */
			if (NOMEMORY==AddAclause2Proof(node)){
				return(NOMEMORY);
			}
			lastnode=node;
		}
	}

	/* Process SAT solver global clauses. */
	/* Loop through SAT solver clauses. */
	for (ptr2=kbset.frstsatglbl,snode=kbset.frstprfnode;ptr2!=NULL;ptr2=ptr2->glblnext){

		/* Allocate a proof node and add it to the proof. */
		if (NULL==(newsnode=MYALLOC(sizeof(proofnode)))){
			return(NOMEMORY);
		}
		newsnode->type=SATCLAUSE;
		newsnode->ptr.satclause=ptr2;
		LinkProofNode(newsnode,snode);
		snode=newsnode;

		/* Parent A-clause is not yet added to the proof queue. */
		if (0==(INPROOFQUEUE&ptr2->parent->flags)){

			/* Allocate, initialize and link parent A-clause node. */
			if (NULL==(node=MYALLOC(sizeof(proofnode)))){
				return(NOMEMORY);
			}
			node->type=ACLAUSE;
			node->backptr=NULL;
			node->ptr.Aclause=ptr2->parent;
			LinkProofNode(node,snode);

			/* Add A-clause to proof queue. */
			if (NOMEMORY==AddAclause2Proof(node)){
				return(NOMEMORY);
			}
		}

		/* Loop through SAT clause literals. */
		for (ptr3=&ptr2->formula[0];ptr3[0]!=UNITEND;ptr3+=(1+sizeof(asymbol))){

			/* SAT clauses containing the component symbol corresponding */
			/* to this literal has not been added to proof queue yet. */
			/* Allocate, initialize and link component symbol definition node. */
			component=((asymbol *)&ptr3[1])->symbol;
			if (0==(INPROOFQUEUE&component->flags)){
				if (NULL==(node=MYALLOC(sizeof(proofnode)))){
					return(NOMEMORY);
				}
				node->type=SYMBOLDEFNTN;
				node->ptr.symbol=component;
				LinkProofNode(node,snode);
				component->flags|=INPROOFQUEUE;
			}
		}
	}

	return(0);
} /* BuildSaturation */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add SAT clause to proof queue)  OCJ
 *
 *    This function adds the SAT clauses to the proof queue being built.
 *
 *
 *  ARGUMENTS:
 *
 *    None.
 *
 *  RETURNS:
 *
 *    0 -> Proof built successfully.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t AddSatCl2Proof(void){
	auto hashchcmp *component;                      /* Pointer to component */
	auto proofnode *snode,*newsnode;                /* SAT current and new proof nodes */
	auto proofnode *anode;                          /* A-clause proof node */
	auto kbase *pkb;                                /* Pointer to KB */
	auto uint8_t *ptr1;                             /* Auxiliary pointer */
	auto satclause *ptr4;                           /* Auxiliary pointer */

	/* Get address of pointer to KB. */
	if (procctl->solvekb==active_cores){
		pkb=&mainkb;
	} else {
		pkb=&kbset;
	}

	/* Loop through SAT solver clauses. */
	for (ptr4=pkb->frstsatglbl,snode=pkb->frstprfnode;ptr4!=NULL;ptr4=ptr4->glblnext){

		/* The SAT clause is part of the proof. */
		if (ptr4->inproof){

			/* If SAT clause is not the contradiction clause that caused the SAT refutation then */
			/* allocate a proof node and add it to the proof. The SAT contradiction clause that */
			/* caused the SAT refutation is already in the proof queue. */
			if (ptr4->inproof==1){
				if (NULL==(newsnode=MYALLOC(sizeof(proofnode)))){
					return(NOMEMORY);
				}
				newsnode->type=SATCLAUSE;
				newsnode->ptr.satclause=ptr4;
				LinkProofNode(newsnode,snode);
				snode=newsnode;
			}

			/* Parent A-clause is not yet added to the proof queue. */
			if (0==(INPROOFQUEUE&ptr4->parent->flags)){

				/* Allocate, initialize and link parent A-clause node. */
				if (NULL==(anode=MYALLOC(sizeof(proofnode)))){
					return(NOMEMORY);
				}
				anode->type=ACLAUSE;
				anode->backptr=NULL;
				anode->ptr.Aclause=ptr4->parent;
				LinkProofNode(anode,snode);

				/* Add A-clause to proof queue. */
				if (NOMEMORY==AddAclause2Proof(anode)){
					return(NOMEMORY);
				}
			}

			/* Loop through SAT clause literals. */
			for (ptr1=&ptr4->formula[0];ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol))){

				/* SAT clauses containing the component symbol corresponding */
				/* to this literal has not been added to proof queue yet. */
				/* Allocate, initialize and link component symbol definition node. */
				component=((asymbol *)&ptr1[1])->symbol;
				if (0==(INPROOFQUEUE&component->flags)){
					if (NULL==(anode=MYALLOC(sizeof(proofnode)))){
						return(NOMEMORY);
					}
					anode->type=SYMBOLDEFNTN;
					anode->ptr.symbol=component;
					LinkProofNode(anode,snode);
					component->flags|=INPROOFQUEUE;
				}
			}
		}
	}

	return(0);
} /* AddSatCl2Proof */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add A-clause to proof queue)  OCJ
 *
 *    This function adds an A-clause and all its ancestors to the proof
 *    queue that is  being built. The corresponding proof node must be
 *    already linked to the proof queue. It is assumed that the clause
 *    has not been added to the proof queue but this is not verified.
 *
 *    The easier way to write this function is making it recursive.
 *    However, in a few cases of potentially very long proofs this
 *    causes segment violation exceptions due to stack overflows.
 *    Due to this the function is non recursive which is almost
 *    as easy in this case.
 *
 *
 *  ARGUMENTS:
 *
 *    node: Pointer to proofnode structure corresponding to the
 *          A-clause to be added to the proof being built.
 *          The node must be already properly linked to the
 *          proof queue.
 *
 *  RETURNS:
 *
 *    0 -> Process performed successfully.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t AddAclause2Proof(proofnode *node){
	auto proofnode *newnode;                        /* New proof node */
	auto proofnode *ptr1;                           /* Auxiliary pointer */
	auto cmprefix *ptr2,*ptr3;                      /* Auxiliary pointers */

	/* Main loop. Here node points to a proof node that is already linked */
	/* to the proof queue but it has not the INPROOFQUEUE flag set yet. */
	while (node!=NULL){

		/* Check if clause comes from main file and therefore there are not */
		/* contradictory axioms. */
		if (node->ptr.Aclause->flags&FROMMAINFILE){
			pbmflags|=NOTCAX;
		}

		/* A-clause is not the conjecture nor an input clause. */
		if ((node->ptr.Aclause->inference>PLAIN)||(node->ptr.Aclause->inference==NEGCONJ)){

			/* A-clause is a COMPONENT clause. */
			if (node->ptr.Aclause->inference==COMPONENT){

				/* Allocate, initialize and link component symbol definition node */
				/* if it is not yet in proof queue. */
				if (0==(INPROOFQUEUE&((hashchcmp *)(node->ptr.Aclause->parent1))->flags)){
					if (NULL==(newnode=MYALLOC(sizeof(proofnode)))){
						return(NOMEMORY);
					}
					newnode->type=SYMBOLDEFNTN;
					newnode->ptr.symbol=(hashchcmp *)(node->ptr.Aclause->parent1);
					LinkProofNode(newnode,node);
					((hashchcmp *)(node->ptr.Aclause->parent1))->flags|=INPROOFQUEUE;
				}

			/* A-clause is not a component clause and it is not the conjecture */
			/* nor an input clause. */
			} else {

				#ifdef ENABLECHOICEAXIOM
				/* First parent is not in the proof queue and the inference is not CHOICEAXIOM. */
				if ((NULL!=(node->ptr.Aclause->parent1))&&(node->ptr.Aclause->inference!=CHOICEAXIOM)
						&&(0==(INPROOFQUEUE&node->ptr.Aclause->parent1->flags))){

					/* Allocate, initialize and link parent1 A-clause node. */
					if (NULL==(newnode=MYALLOC(sizeof(proofnode)))){
						return(NOMEMORY);
					}
					newnode->type=ACLAUSE;
					newnode->backptr=node;
					newnode->ptr.Aclause=node->ptr.Aclause->parent1;
					LinkProofNode(newnode,node);

					/* If parent is a goal definition then set flag INPROOFQUEUE. */
					if (node->ptr.Aclause->parent1->inference==GOALDEFINITION){
						node->ptr.Aclause->parent1->flags|=INPROOFQUEUE;

					/* Otherwise keep traversing. */
					} else {
						node=newnode;
						continue;
					}
				}
				#else
				/* First parent is not in the proof queue. */
				if ((NULL!=(node->ptr.Aclause->parent1))&&(0==(INPROOFQUEUE&node->ptr.Aclause->parent1->flags))){

					/* Allocate, initialize and link parent1 A-clause node. */
					if (NULL==(newnode=MYALLOC(sizeof(proofnode)))){
						return(NOMEMORY);
					}
					newnode->type=ACLAUSE;
					newnode->backptr=node;
					newnode->ptr.Aclause=node->ptr.Aclause->parent1;
					LinkProofNode(newnode,node);

					/* If parent is a goal definition then set flag INPROOFQUEUE. */
					if (node->ptr.Aclause->parent1->inference==GOALDEFINITION){
						node->ptr.Aclause->parent1->flags|=INPROOFQUEUE;

					/* Otherwise keep traversing. */
					} else {
						node=newnode;
						continue;
					}
				}
				#endif

				/* If A-clause inference is DEFINTNFOLDING then add all A-clauses */
				/* with inference GOALDEFINITION to the proof. */
				if (node->ptr.Aclause->inference==DEFINTNFOLDING){
					ptr1=node;
					for (ptr2=mainkb.lstunproc->prev;ptr2->inference==GOALDEFINITION;ptr2=ptr2->prev){
						ptr3=ptr2->parent1;
						if (0==(INPROOFQUEUE&ptr3->flags)){

							/* Allocate, initialize and link goal definition A-clause node. */
							if (NULL==(newnode=MYALLOC(sizeof(proofnode)))){
								return(NOMEMORY);
							}
							newnode->type=ACLAUSE;
							newnode->backptr=node;
							newnode->ptr.Aclause=ptr3;
							LinkProofNode(newnode,node);
							ptr3->flags|=INPROOFQUEUE;

							/* Keep traversing. */
							node=newnode;
						}
					}

					/* Indicate that A-clause has been added to proof queue and backtrack. */
					ptr1->ptr.Aclause->flags|=INPROOFQUEUE;
					node=ptr1->backptr;
					ptr1->backptr=NULL;
					continue;

				/* Otherwise add second parent to the proof if necessary. */
				} else {

					#ifdef ENABLECHOICEAXIOM
					/* There is a second parent and the second parent is not in the proof queue. */
					if ((NULL!=(node->ptr.Aclause->parent2))&&(0==(INPROOFQUEUE&node->ptr.Aclause->parent2->flags))){
					#else
					/* There is a second parent, the inference is not SKOLEMIZATION */
					/* and the second parent is not in the proof queue. */
					if ((NULL!=(node->ptr.Aclause->parent2))&&(node->ptr.Aclause->inference!=SKOLEMIZATION)
							&&(0==(INPROOFQUEUE&node->ptr.Aclause->parent2->flags))){
					#endif

						/* Allocate, initialize and link parent2 A-clause node. */
						if (NULL==(newnode=MYALLOC(sizeof(proofnode)))){
							return(NOMEMORY);
						}
						newnode->type=ACLAUSE;
						newnode->backptr=node;
						newnode->ptr.Aclause=node->ptr.Aclause->parent2;
						LinkProofNode(newnode,node);

						/* If parent is a goal definition then set flag INPROOFQUEUE. */
						if (node->ptr.Aclause->parent2->inference==GOALDEFINITION){
							node->ptr.Aclause->parent2->flags|=INPROOFQUEUE;

						/* Otherwise keep traversing. */
						} else {
							node=newnode;
							continue;
						}
					}
				}
			}
		}

		/* Indicate that A-clause has been added to proof queue. */
		node->ptr.Aclause->flags|=INPROOFQUEUE;

		/* Backtrack. */
		node=(ptr1=node)->backptr;
		ptr1->backptr=NULL;
	}

	return(0);
} /* AddAclause2Proof */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Link node to proof queue)  OCJ
 *
 *    This function links a proof node to the proof queue.
 *
 *
 *  ARGUMENTS:
 *
 *    newnode: Pointer to proofnode to be linked.
 *    refnode: Pointer to a reference node, whose number is suposedly
 *             close to the number of newnode.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *--------------------------------------------------------------*/
void LinkProofNode(proofnode *newnode,proofnode *refnode){
	auto int32_t ii,jj;                             /* Auxiliary */

	/* Get number corresponding to newnode and refnode. */
	switch (newnode->type){
		case SYMBOLDEFNTN:
			jj=newnode->ptr.symbol->defnumber;
			break;
		case SATCLAUSE:
			jj=newnode->ptr.satclause->number;
			break;
		case ACLAUSE:
		default:
			jj=newnode->ptr.Aclause->number;
			break;
	}
	switch (refnode->type){
		case SYMBOLDEFNTN:
			ii=refnode->ptr.symbol->defnumber;
			break;
		case SATCLAUSE:
			ii=refnode->ptr.satclause->number;
			break;
		case ACLAUSE:
		default:
			ii=refnode->ptr.Aclause->number;
			break;
	}

	/* Initialize link pointers. */
	newnode->nextnode=refnode->nextnode;
	newnode->prevnode=refnode;

	/* Link node to proof queue. Node goes following reference node. */
	if(jj>ii){
		while (newnode->nextnode!=NULL){
			switch (newnode->nextnode->type){
				case SYMBOLDEFNTN:
					ii=newnode->nextnode->ptr.symbol->defnumber;
					break;
				case SATCLAUSE:
					ii=newnode->nextnode->ptr.satclause->number;
					break;
				case ACLAUSE:
				default:
					ii=newnode->nextnode->ptr.Aclause->number;
					break;
			}
			if (jj<ii){
				break;
			}
			newnode->prevnode=newnode->nextnode;
			newnode->nextnode=newnode->nextnode->nextnode;
		}
		newnode->prevnode->nextnode=newnode;
		if (newnode->nextnode!=NULL){
			newnode->nextnode->prevnode=newnode;
		}

	/* Link node to proof queue. Node goes before reference node. */
	} else {
		while (newnode->prevnode!=NULL){
			switch (newnode->prevnode->type){
				case SYMBOLDEFNTN:
					ii=newnode->prevnode->ptr.symbol->defnumber;
					break;
				case SATCLAUSE:
					ii=newnode->prevnode->ptr.satclause->number;
					break;
				case ACLAUSE:
				default:
					ii=newnode->prevnode->ptr.Aclause->number;
					break;
			}
			if (jj>ii){
				break;
			}
			newnode->nextnode=newnode->prevnode;
			newnode->prevnode=newnode->prevnode->prevnode;
		}
		newnode->nextnode->prevnode=newnode;
		if (newnode->prevnode!=NULL){
			newnode->prevnode->nextnode=newnode;
		} else {
			if (procctl->solvekb==active_cores){
				mainkb.frstprfnode=newnode;
			} else {
				kbset.frstprfnode=newnode;
			}
		}
	}

	return;
} /* LinkProofNode */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Print theorem proof or saturation output)  OCJ
 *
 *    This function prints the proof found if the run results are
 *    theorem or unsatisfiable and prints the saturation output
 *    if the run results are satisfiable or counter-satisfiable.
 *    The proof or saturation output must be previously built,
 *    but this in not checked.
 *
 *    Some of the inferences should be reported as status(esa)
 *    in TPTP format proofs. However the proof checking tool
 *    GDV used in CASC competition incorrectly reports FAILURE
 *    for inferences with status(esa) when the inference parent
 *    is a definition. Therefore all inferences that should be
 *    reported as status(esa) are reported here as status(thm)
 *    (which is also correct) except skolemization, which strictly
 *    speaking are not equisatisfiable but again GDV requires them
 *    to be reported as status(esa). The other option is checking
 *    the parent each time an inference must be printed and this
 *    has been discarded by the moment. In any case the correct
 *    status(esa) is kept commented out in the code as documentation.
 *
 *
 *  ARGUMENTS:
 *
 *    None.
 *
 *  RETURNS:
 *
 *    0 -> Proof printed successfully.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t PrintProof(void){
	auto kbase *pkb;                                /* Pointer to KB */
	auto proofnode *prfnode;                        /* Pointer to proof queue node */
	auto char *refutation;                          /* Pointer to string with refutation SAT clause numbers */
	auto int32_t size;                              /* Space allocated for refutation */
	auto char clnum[25];                            /* SAT clause number in text form */
	auto proofnode *ptr1;                           /* Auxiliary pointer */
	auto char *ptr2,*ptr3,*ptr4;                    /* Auxiliary pointers */
	auto int32_t ii,jj;                             /* Auxiliary */

	/* Get address of pointer to KB. */
	if (procctl->solvekb==active_cores){
		pkb=&mainkb;
	} else {
		pkb=&kbset;
	}

	/* If first node of proof is NULL this means that a proof was found */
	/* in the pre-processing but there is not enough memory to build */
	/* the proof. Return NOMEMORY. */
	if (pkb->frstprfnode==NULL){
		return(NOMEMORY);
	}

	/* If it is a proof by contradiction that contains a SAT refutation */
	/* and PROOF option is not OFF then initialize string with refutation */
	/* SAT clause numbers. */
	if ((procctl->status==UNSATISFIABLE)&&(pbmflags&SATREFUTATION)&&(proofmode!=PROOFOFF)){
		if (NULL==(refutation=MYALLOC(STR_CHUNK_SIZE))){
			return(NOMEMORY);
		}
		size=STR_CHUNK_SIZE;
		refutation[0]=0;
		ii=0;
	} else {
		refutation=NULL;
	}

	/* Print proof queue. */
	if (proofmode!=PROOFOFF){
		for (prfnode=pkb->frstprfnode;prfnode!=NULL;prfnode=prfnode->nextnode){
			switch (prfnode->type){

				/* Component symbol definition node. */
				case SYMBOLDEFNTN:
					if (NULL==(ptr2=Decompile2(prfnode->ptr.symbol->formula))){
						MYFREE(refutation);
						return(NOMEMORY);
					}
					if (NULL==(ptr4=ptr3=MYALLOC(42+strlen(ptr2)))){
						MYFREE(refutation);
						MYFREE(ptr2);
						return(NOMEMORY);
					}
					strcpy(ptr3,SPLPREFIX);
					sprintf(&ptr3[strlen(SPLPREFIX)],"%d",prfnode->ptr.symbol->number);
					strcat(ptr3,SPLPOSTFIX);
					jj=strlen(ptr3);
					strcat(ptr3," <=> (");
					strcat(ptr3,ptr2);
					strcat(ptr3,")");
					if (NOMEMORY==Cnf2Fof(&ptr3)){
						MYFREE(refutation);
						MYFREE(ptr2);
						MYFREE(ptr4);
						return(NOMEMORY);
					}
					if ((proofmode==TPTP)&&(syntaxmode==TPTP)){
						printf("fof(f%lu,definition,(\n",prfnode->ptr.symbol->defnumber);
						printf("  %s),\n",ptr3);
						ptr4[jj]=0;
						printf("  introduced(definition,[new_symbols(definition,[%s])],[split_symbol_definition])).\n",ptr4);
					} else {
						printf("%lu. ",prfnode->ptr.symbol->defnumber);
						printf("%s [split symbol definition]\n",ptr3);
					}
					MYFREE(ptr2);
					MYFREE(ptr3);
					MYFREE(ptr4);
					break;

				/* SAT clause node. */
				case SATCLAUSE:

					/* Print SAT clause. */
					if (NOMEMORY==PrintSingleSATClause(prfnode->ptr.satclause)){
						if (refutation!=NULL){
							MYFREE(refutation);
						}
						return(NOMEMORY);
					}
					printf("\n");

					/* If it is a proof by contradiction add the clause number to the refutation list. */
					if (procctl->status==UNSATISFIABLE){
						if (ii){
							if (NOMEMORY==SafeAppend(&refutation,&size,",",-1)){
								MYFREE(refutation);
								return(NOMEMORY);
							}
						} else {
							ii=1;
						}
						if ((proofmode==TPTP)&&(syntaxmode==TPTP)){
							sprintf(clnum,"f%lu",prfnode->ptr.satclause->number);
						} else {
							sprintf(clnum,"%lu",prfnode->ptr.satclause->number);
						}
						if (NOMEMORY==SafeAppend(&refutation,&size,clnum,-1)){
							MYFREE(refutation);
							return(NOMEMORY);
						}
					}
					break;

				/* A-Clause node. */
				case ACLAUSE:
					if (NOMEMORY==PrintSingleAClause(prfnode->ptr.Aclause)){
						if (refutation!=NULL){
							MYFREE(refutation);
						}
						return(NOMEMORY);
					}
					printf("\n");
					break;
			}
		}
	}

	/* If there is a SAT refutation and PROOF option is not OFF then */
	/* print SAT refutation (unsatisfiable core), free proof queue */
	/* and verify selection of SAT solver proof clauses if needed. */
	if ((refutation!=NULL)&&(proofmode!=PROOFOFF)){
		if ((proofmode==TPTP)&&(syntaxmode==TPTP)){
			printf("fof(f%lu,plain,(\n  $false),\n  ",pkb->nformulas+1);
			printf("inference(sat_refutation,[status(thm)],[%s])).\n",refutation);
		} else {
			printf("%lu. $false [sat refutation %s]\n",pkb->nformulas+1,refutation);
		}
		MYFREE(refutation);
		for (prfnode=pkb->frstprfnode;prfnode!=NULL;prfnode=ptr1){
			ptr1=prfnode->nextnode;
			MYFREE(prfnode);
		}
		pkb->frstprfnode=NULL;
		#ifdef VERIFYSATPROOF
		VerifySatProof();
		MYFREE(pkb->frstprfnode);
		pkb->frstprfnode=NULL;
		#endif

	/* If there is not a SAT refutation or PROOF option is OFF */
	/* then just free proof queue. */
	} else {
		for (prfnode=pkb->frstprfnode;prfnode!=NULL;prfnode=ptr1){
			ptr1=prfnode->nextnode;
			MYFREE(prfnode);
		}
		pkb->frstprfnode=NULL;
	}
	return(0);
} /* PrintProof */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Print saturation SAT model output)  OCJ
 *
 *    This function prints the SAT solver model of a satisfiability
 *    or counter-satisfiability result as part of the saturation
 *    output.
 *
 *
 *  ARGUMENTS:
 *
 *    None
 *
 *  RETURNS:
 *
 *    0 -> Successful execution.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t PrintSatModel(void){
	auto hashchcmp *symbol;                         /* Pointer to component symbol */
	auto int32_t ii;                                /* Auxiliary */

	/* Return if there are no hashchcmp elements and therefore there are no */
	/* SAT clauses. */
	if (kb_hashcmp==NULL){
		return(0);
	}

	/* Loop through all hashchcmp elements and print those defined */
	/* in the model. */
	if ((proofmode==TPTP)&&(syntaxmode==TPTP)){
		printf("fof(model,axiom,");
	} else {
		printf("Sat solver model: ");
	}
	for (symbol=kb_hashcmp,ii=0;symbol!=NULL;symbol=symbol->nextelem){
		if ((MODELPOSITIVE|MODELNEGATIVE)&symbol->flags){
			if (ii){
				printf("|");
			} else {
				ii=1;
			}
			if (MODELNEGATIVE&symbol->flags){
				printf("~");
			}
			printf("%s%d%s",SPLPREFIX,symbol->number,SPLPOSTFIX);
		}
	}
	if ((proofmode==TPTP)&&(syntaxmode==TPTP)){
		printf(").");
	}
	printf("\n");

	return(0);
} /* PrintSatModel */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Print model from a GOSAT run)  OCJ
 *
 *    This function prints the model found after executing a GOSAT
 *    user command.
 *
 *
 *  ARGUMENTS:
 *
 *    None
 *
 *  RETURNS:
 *
 *    0 -> Successful execution.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t PrintModel(void){
	auto hashchcmp *symbol;                         /* Pointer to component symbol */
	auto char *ptr2;                                /* Auxiliary pointer */

	/* Loop through all hashchcmp elements and print those defined */
	/* in the model. As this function is only called after a GOSAT */
	/* user command all elements belong to working KB 0. */
	for (symbol=kb_hashcmp;symbol!=NULL;symbol=symbol->nextelem){
		if ((MODELPOSITIVE|MODELNEGATIVE)&symbol->flags){
			printf("%s%d%s <=> ",SPLPREFIX,symbol->number,SPLPOSTFIX);
			if (NULL==(ptr2=Decompile2(symbol->formula))){
				return(NOMEMORY);
			}
			printf("%s: ",ptr2);
			MYFREE(ptr2);
			if (MODELPOSITIVE&symbol->flags){
				printf("TRUE\n");
			} else {
				printf("FALSE\n");
			}
		}
	}

	return(0);
} /* PrintModel */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Import a TPTP problem file)  OCJ
 *
 *    This function imports a TPTP problem file. If filename
 *    doesn't start with a pathsep character then filepath
 *    is prepended to filename.
 *
 *    The import process includes telling and asking appropriate
 *    sentences so that a prove is attempted.
 *
 *    The current main KB is used with the actual contents.
 *
 *    The directive names are not case sensitive.
 *
 *    The TPTP syntax is not fully supported. In addition the syntax
 *    checking on the supported part is not complete by the moment.
 *
 *    Time measurement is not accurately done in this function. Only
 *    time intervals that may become significantly high are measured:
 *    the loop time for reading lines of the same directive and the
 *    Tell() and Ask() execution times that are measured inside the
 *    functions themselves. Also, in case of error sometimes a
 *    significant time interval can go without being measured. This
 *    way the function is simplified at an acceptable cost.
 *
 *
 *  ARGUMENTS:
 *
 *    inpfname: Char pointer to input filename with optional path.
 *    fnprev: Buffer containing formula names. Only formulae with
 *            names included in this list will be imported. Each
 *            name is followed by a 0 byte and a control byte that
 *            is 1 if the name has been already used or 0 otherwise.
 *            This pointer can be NULL.
 *    fnused: Number of bytes used in fnprev buffer including string
 *            terminating and control bytes. This must be 0 if fnprev
 *            is NULL.
 *    firsttime: Non zero if this is the initial call to this function.
 *
 *  RETURNS:
 *
 *    0--> Import successful
 *    1--> There was an error importing file
 *    2--> Import not performed because KB's are not empty
 *    3--> Timeout
 *
 *--------------------------------------------------------------*/
int32_t Import(char *inpfname,char *fnprev,int32_t fnused,int32_t firsttime){
	auto FILE *fhndl;                                    /* File handle. */
	auto char *line;                                     /* Line buffer */
	auto char *pending;                                  /* List of pending parentheses and brackets */
	auto int32_t pathlength;                             /* Path length in inpfname */
	auto int32_t pathtype;                               /* Type of path of include file */
	auto int32_t sizel;                                  /* Size in bytes of line buffer */
	auto int32_t sizep;                                  /* Size in bytes of pending buffer */
	auto char *fnames;                                   /* Formula names */
	auto int32_t fnsize;                                 /* Size in bytes of fnames buffer */
	auto int32_t comas[4];                               /* Position of argument separating commas */
	auto int32_t conjinput;                              /* Set to 1 if a valid conjecture was processed, otherwise 0. */
	auto struct itimerval alarmtime;                     /* To define a global timeout alarm */
	auto clock_t start,end;                              /* For CPU time measurements */
	auto double time;                                    /* Elapsed time in seconds */
	auto char *ptr1,*auxptr1,*auxptr2,*auxptr3,*auxptr4; /* Auxiliary pointers */
	auto int32_t cc,ii,jj,kk,mm,nn,pp,qq;                /* Auxiliary */

	/* Root call. Return if KB's are not empty, initialize start time */
	/* and set alarm time. */
	if (firsttime){
		if (mainkb.lasttxt!=NULL){
			printf("KB's are not empty, process stopped.\n");
			return(2);
		}
		gettimeofday(&mainkb.time,NULL);
		start=clock();
		alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
		alarmtime.it_value.tv_sec=(int32_t)glblopts.timeout;
		alarmtime.it_value.tv_usec=(int32_t)((glblopts.timeout-(int32_t)glblopts.timeout)*1000000.0);
		alrmstatus=0;
		setitimer(ITIMER_REAL,&alarmtime,NULL);

	/* Not a root call. */
	} else {
		start=0;
	}

	/* Allocate memory for buffers. */
	sizel=INPUT_CHUNK_SIZE;
	if (NULL==(line=MYALLOC(sizel))){
		printf("%% SZS status MemoryOut : ");
		printf("Not enough memory, process stopped\n");
		if (firsttime){
			alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
			alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
			setitimer(ITIMER_REAL,&alarmtime,NULL);
		}
		return(1);
	}
	fnsize=INPUT_CHUNK_SIZE+fnused;
	if (NULL==(fnames=MYALLOC(fnsize))){
		printf("%% SZS status MemoryOut : ");
		printf("Not enough memory, process stopped\n");
		MYFREE(line);
		if (firsttime){
			alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
			alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
			setitimer(ITIMER_REAL,&alarmtime,NULL);
		}
		return(1);
	}
	if (fnused!=0){
		memcpy(fnames,fnprev,fnused);
	}
	sizep=INPUT_CHUNK_SIZE;
	if (NULL==(pending=MYALLOC(sizep))){
		printf("%% SZS status MemoryOut : ");
		printf("Not enough memory, process stopped\n");
		MYFREE(line);
		MYFREE(fnames);
		if (firsttime){
			alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
			alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
			setitimer(ITIMER_REAL,&alarmtime,NULL);
		}
		return(1);
	}

	/* Strip leading and trailing spaces and tabs in filename. */
	inpfname=SkipBlanks(inpfname);
	for (ii=strlen(inpfname)-1;((ii>=0)&&((inpfname[ii]==' ')||(inpfname[ii]=='\t')));ii--){
	}
	inpfname[ii+1]=0;

	/* Get the path length in inpfname. */
	if (NULL==(auxptr1=strrchr(inpfname,WPATHSEPARATOR))){
		pathlength=0;
	} else {
		pathlength=auxptr1-inpfname+1;
	}

	/* Open file. */
	if (NULL==(fhndl=fopen(inpfname,"r"))){
		printf("Error opening file \"%s\"\n",inpfname);
		MYFREE(line);
		MYFREE(fnames);
		MYFREE(pending);
		if (firsttime){
			alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
			alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
			setitimer(ITIMER_REAL,&alarmtime,NULL);
		}
		return(1);
	}

	/* Directive read loop. */
	conjinput=0; /* No valid conjecture processed so far. */
	while (1!=(ii=ReadDirective(&line,&sizel,fhndl))){

		/* Handle file read error and NOMEMORY conditions. */
		switch (ii){
			case 2:
				printf("%s error reading file %s, process stopped.\n",strerror(errno),inpfname);
				fclose(fhndl);
				MYFREE(line);
				MYFREE(fnames);
				MYFREE(pending);
				if (firsttime){
					alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
					alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
					setitimer(ITIMER_REAL,&alarmtime,NULL);
				}
				return(1);
				break;
			case NOMEMORY:
				printf("%% SZS status MemoryOut : ");
				printf("Not enough memory, process stopped\n");
				fclose(fhndl);
				MYFREE(line);
				MYFREE(fnames);
				MYFREE(pending);
				if (firsttime){
					alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
					alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
					setitimer(ITIMER_REAL,&alarmtime,NULL);
				}
				return(1);
				break;
		}

		/* Check timeout. Some files are very long. */
		if (procctl->status==TIMEOUT){
			if (pgmmode==NONINTERACTIVE){
				printf("%% SZS status Timeout for %s: ",inpfname);
			}
			printf("Timeout reading file.\n");
			fclose(fhndl);
			MYFREE(line);
			MYFREE(fnames);
			MYFREE(pending);
			if (firsttime){
				alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
				alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
				setitimer(ITIMER_REAL,&alarmtime,NULL);
				gettimeofday(&mainkb.endtime,NULL);
				time=mainkb.prstats.elapsed_time+mainkb.endtime.tv_sec-mainkb.time.tv_sec
						+(mainkb.endtime.tv_usec-mainkb.time.tv_usec)/1000000.0;
				mainkb.prstats.elapsed_time=time;
				glblstats.elapsed_time+=mainkb.prstats.elapsed_time;
				end=clock();
				cpu_time+=(((double)(end-start))/CLOCKS_PER_SEC);
			}
			return(3);
		}

		/* Include directive. */
		/* OJO: PASAR A MINÚSCULA. */
		if ((0==strncmp(line,"include ",8))||(0==strncmp(line,"include(",8))){
			mm=0;

		/* FOF directive. */
		} else if ((0==strncmp(line,"fof ",4))||(0==strncmp(line,"fof(",4))){
			mm=1;

		/* CNF directive. */
		} else if ((0==strncmp(line,"cnf ",4))||(0==strncmp(line,"cnf(",4))){
			pbmflags|=CNFFORMAT;
			mm=2;

		/* TFF or THF directive. */
		} else if ((0==strncmp(line,"thf ",4))||(0==strncmp(line,"thf(",4))
				||(0==strncmp(line,"tff ",4))||(0==strncmp(line,"tff(",4))){
			printf("THF and TFF formulae not supported, process stopped\n");
			fclose(fhndl);
			MYFREE(line);
			MYFREE(fnames);
			MYFREE(pending);
			if (firsttime){
				alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
				alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
				setitimer(ITIMER_REAL,&alarmtime,NULL);
			}
			return(1);

		/* Unknown directive. */
		} else {
			printf("Unknown directive:\n%s\nImport stopped\n",line);
			fclose(fhndl);
			MYFREE(line);
			MYFREE(fnames);
			MYFREE(pending);
			if (firsttime){
				alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
				alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
				setitimer(ITIMER_REAL,&alarmtime,NULL);
			}
			return(1);
		}

		/* Check other errors from ReadDirective(). */
		switch (ii){
			case 3:
				printf("Syntax error in line:\n%s\nmissing single or double quotes, process stopped\n",line);
				fclose(fhndl);
				MYFREE(line);
				MYFREE(fnames);
				MYFREE(pending);
				if (firsttime){
					alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
					alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
					setitimer(ITIMER_REAL,&alarmtime,NULL);
				}
				return(1);
				break;
			case 4:
				printf("Syntax error in line:\n%s\nMissing final dot. Process stopped\n",line);
				fclose(fhndl);
				MYFREE(line);
				MYFREE(fnames);
				MYFREE(pending);
				if (firsttime){
					alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
					alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
					setitimer(ITIMER_REAL,&alarmtime,NULL);
				}
				return(1);
				break;
		}

		/* Jump over first open parenthesis. Pointer to first open parenthesis */
		/* is stored in auxptr1 for later. */
		auxptr1=SkipBlanks(&line[mm?3:7]);
		if ((*auxptr1)!='('){
			printf("Syntax error in line:\n%s\nmissing open parenthesis. Process stopped\n",line);
			fclose(fhndl);
			MYFREE(line);
			MYFREE(fnames);
			MYFREE(pending);
			if (firsttime){
				alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
				alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
				setitimer(ITIMER_REAL,&alarmtime,NULL);
			}
			return(1);
		}

		/* Check parenthesis and brackets balance. */
		auxptr2=auxptr1;
		if (0!=(ii=CheckBrackets(&pending,&sizep,&auxptr2))){
			if (ii==NOMEMORY){
				printf("%% SZS status MemoryOut : ");
				printf("Not enough memory, process stopped\n");
				fclose(fhndl);
				MYFREE(line);
				MYFREE(fnames);
				MYFREE(pending);
				if (firsttime){
					alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
					alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
					setitimer(ITIMER_REAL,&alarmtime,NULL);
				}
				return(1);
			} else {
				auxptr2[1]=0;
				printf("Syntax error in line:\n%s\nParenthesis or square bracket mismatch, process stopped\n",line);
				fclose(fhndl);
				MYFREE(line);
				MYFREE(fnames);
				MYFREE(pending);
				if (firsttime){
					alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
					alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
					setitimer(ITIMER_REAL,&alarmtime,NULL);
				}
				return(1);
			}
		}

		/* Check dot after final parenthesis. */
		if (auxptr2[0]!='.'){
			printf("Syntax error in line:\n%s\nMissing dot after last parenthesis. Process stopped\n",line);
			fclose(fhndl);
			MYFREE(line);
			MYFREE(fnames);
			MYFREE(pending);
			if (firsttime){
				alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
				alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
				setitimer(ITIMER_REAL,&alarmtime,NULL);
			}
			return(1);
		}

		/* Check number of arguments. */
		/* cc = number of commas detected. */
		for (nn=(mm?3:7),cc=pp=qq=0;line[nn]!=0;nn++){
			switch (line[nn]){
				case '\'':
				case '"':
					ptr1=JumpOverQuotes(&(line[nn]));
					nn=ptr1-line;
					break;
				case '(':
					pp++;
					break;
				case ')':
					pp--;
					break;
				case '[':
					qq++;
					break;
				case ']':
					qq--;
					break;
				case ',':
					if ((pp==1)&&(qq==0)){
						if (cc>=4){
							printf("Syntax error in line:\n%s\nToo many arguments, process stopped\n",line);
							fclose(fhndl);
							MYFREE(line);
							MYFREE(fnames);
							MYFREE(pending);
							if (firsttime){
								alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
								alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
								setitimer(ITIMER_REAL,&alarmtime,NULL);
							}
							return(1);
						}
						comas[cc]=nn;
						cc++;
					}
					break;
			}
		}

		/* Display directive. */
		if ((verbose==2)&&(pgmmode==INTERACTIVE)){
			printf("%s\n",line);
		}

		/* Process directive. */
		switch (mm){

			/* Include directive. */
			case 0:

				/* Check number of arguments. */
				if (cc>1){
					printf("Syntax error: Too many arguments. Process stopped\n");
					fclose(fhndl);
					MYFREE(line);
					MYFREE(fnames);
					MYFREE(pending);
					if (firsttime){
						alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
						alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
						setitimer(ITIMER_REAL,&alarmtime,NULL);
					}
					return(1);
				}

				/* Get file name in auxptr1. */
				auxptr1=SkipBlanks(&auxptr1[1]);
				if ((*auxptr1)!='\''){
					printf("Syntax error: Missing opening single quotes. Process stopped\n");
					fclose(fhndl);
					MYFREE(line);
					MYFREE(fnames);
					MYFREE(pending);
					if (firsttime){
						alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
						alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
						setitimer(ITIMER_REAL,&alarmtime,NULL);
					}
					return(1);
				}
				auxptr1++;
				auxptr2=strchr(auxptr1,'\''); /* Import() enforces closing single quotes in the same line. */
				*auxptr2=0;

				/* Get optional list of formula names. */
				auxptr2=SkipBlanks(&auxptr2[1]);
				if ((*auxptr2)==','){

					/* Second argument is "all". */
					if ((0==strncmp("all",&auxptr2[1],3))&&(0==isalnum(auxptr2[4]))){
						auxptr2=SkipBlanks(&auxptr2[4]);
						ii=0;

					/* Second argument is not "all". */
					} else {

						/* Second argument is a list of formulae names. */
						/* Put the names in fnames buffer. */
						auxptr2=SkipBlanks(&auxptr2[1]);
						if ((*auxptr2)=='['){
							auxptr2=SkipBlanks(&auxptr2[1]);
							for (auxptr4=strchr(auxptr2,']'),ii=fnused,kk=1;
									kk!=2;auxptr2=SkipBlanks(&auxptr3[1])){
								if (NULL!=(auxptr3=strchr(auxptr2,','))){
									if (auxptr3<auxptr4){
										kk=1;
									} else {
										auxptr3=auxptr4;
										kk=2;
									}
								} else {
									auxptr3=auxptr4;
									kk=2;
								}
								if (auxptr2!=auxptr3){
									*auxptr3=0;
									jj=strlen(auxptr2);
									if ((ii+jj+2)>fnsize){
										fnsize=ii+jj+2+INPUT_CHUNK_SIZE;
										if (NULL==(auxptr4=MYREALLOC(fnames,fnsize))){
											printf("%% SZS status MemoryOut : ");
											printf("Not enough memory, process stopped\n");
											fclose(fhndl);
											MYFREE(line);
											MYFREE(fnames);
											MYFREE(pending);
											if (firsttime){
												alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
												alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
												setitimer(ITIMER_REAL,&alarmtime,NULL);
											}
											return(1);
										}
										fnames=auxptr4;
									}
									strcpy(&fnames[ii],auxptr2);
									fnames[ii+jj+1]=0;
									ii+=(jj+2);
								} else {
									printf("Syntax error: missing formula name. Process stopped\n");
									fclose(fhndl);
									MYFREE(line);
									MYFREE(fnames);
									MYFREE(pending);
									if (firsttime){
										alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
										alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
										setitimer(ITIMER_REAL,&alarmtime,NULL);
									}
									return(1);
								}
							}

						/* Second argument is not a list of formulae names. */
						/* Syntax error. */
						} else {
							printf("Syntax error: invalid second argument. Process stopped\n");
							fclose(fhndl);
							MYFREE(line);
							MYFREE(fnames);
							MYFREE(pending);
							if (firsttime){
								alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
								alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
								setitimer(ITIMER_REAL,&alarmtime,NULL);
							}
							return(1);
						}
					}

				/* There are not formula names. */
				} else {
					ii=0;
				}

				/* Check line termination. */
				if ((*auxptr2)!=')'){
					printf("Syntax error: Closing parenthesis expected in second argument. Process stopped\n");
					fclose(fhndl);
					MYFREE(line);
					MYFREE(fnames);
					MYFREE(pending);
					if (firsttime){
						alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
						alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
						setitimer(ITIMER_REAL,&alarmtime,NULL);
					}
					return(1);
				}
				auxptr2=SkipBlanks(&auxptr2[1]);
				if ((*auxptr2)!='.'){
					printf("Syntax error: Missing ending dot. Process stopped\n");
					fclose(fhndl);
					MYFREE(line);
					MYFREE(fnames);
					MYFREE(pending);
					if (firsttime){
						alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
						alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
						setitimer(ITIMER_REAL,&alarmtime,NULL);
					}
					return(1);
				}

				/* Build the final file name in auxptr1. The name specified by the */
				/* include directive is already in auxptr1. */
				/* The possible values of pathtype are: */
				/* 0 -> no additional path characters were added at the beginning of */
				/*      file name and no additional storage was allocated. */
				/* 1 -> additional path characters added at the beginning of file name */
				/*      either with the path of current file or with the path in $TPTP */
				/*      environment variable and specific storage that must be freed */
				/*      was allocated for the result. */
				/* Absolute path specified. */
				if (auxptr1[0]==WPATHSEPARATOR){
					if (FileExist(auxptr1)){
						pathtype=0;
					} else {
						printf("Included file %s not found.\n",auxptr1);
						fclose(fhndl);
						MYFREE(line);
						MYFREE(fnames);
						MYFREE(pending);
						if (firsttime){
							alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
							alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
							setitimer(ITIMER_REAL,&alarmtime,NULL);
						}
						return(1);
					}

				/* Relative path specified. */
				} else {

					/* No additional path to append from current file. */
					if (pathlength==0){
						pathtype=0;
						auxptr2=auxptr1;

					/* Additional path to append from current file. */
					} else {
						pathtype=1;
						if (NULL==(auxptr2=MYALLOC(pathlength+strlen(auxptr1)+1))){
							printf("%% SZS status MemoryOut : ");
							printf("Not enough memory, process stopped\n");
							fclose(fhndl);
							MYFREE(line);
							MYFREE(fnames);
							MYFREE(pending);
							if (firsttime){
								alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
								alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
								setitimer(ITIMER_REAL,&alarmtime,NULL);
							}
							return(1);
						}
						memcpy(auxptr2,inpfname,pathlength);
						strcpy(&auxptr2[pathlength],auxptr1);
					}

					/* The file doesn't exist, try with $TPTP path. */
					if (!FileExist(auxptr2)){
						if (NULL!=(auxptr3=getenv("TPTP"))){
							if (pathtype==1){
								if (NULL==(auxptr4=MYREALLOC(auxptr2,strlen(auxptr1)+strlen(auxptr3)+2))){
									printf("Not enough memory, process stopped\n");
									fclose(fhndl);
									MYFREE(line);
									MYFREE(fnames);
									MYFREE(pending);
									if (firsttime){
										alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
										alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
										setitimer(ITIMER_REAL,&alarmtime,NULL);
									}
									return(1);
								}
								auxptr2=auxptr4;
							} else {
								if (NULL==(auxptr2=MYALLOC(strlen(auxptr1)+strlen(auxptr3)+2))){
									printf("Not enough memory, process stopped\n");
									fclose(fhndl);
									MYFREE(line);
									MYFREE(fnames);
									MYFREE(pending);
									if (firsttime){
										alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
										alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
										setitimer(ITIMER_REAL,&alarmtime,NULL);
									}
									return(1);
								}
								pathtype=1;
							}
							strcpy(auxptr2,auxptr3);
							kk=strlen(auxptr2);
							if ((kk>0)&&(auxptr2[kk-1]!=WPATHSEPARATOR)){
								auxptr2[kk]=WPATHSEPARATOR;
								auxptr2[kk+1]=0;
							}
							strcat(auxptr2,auxptr1);
							if (!FileExist(auxptr2)){
								printf("Included file %s not found.\n",auxptr1);
								MYFREE(auxptr2);
								fclose(fhndl);
								MYFREE(line);
								MYFREE(pending);
								MYFREE(fnames);
								if (firsttime){
									alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
									alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
									setitimer(ITIMER_REAL,&alarmtime,NULL);
								}
								return(1);
							}
						} else {
							printf("Included file %s not found.\n",auxptr1);
							if (pathtype==1){
								MYFREE(auxptr2);
							}
							fclose(fhndl);
							MYFREE(line);
							MYFREE(fnames);
							MYFREE(pending);
							if (firsttime){
								alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
								alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
								setitimer(ITIMER_REAL,&alarmtime,NULL);
							}
							return(1);
						}
					}
					auxptr1=auxptr2;
				}

				/* Include the file and free file name memory if necessary. */
				jj=Import(auxptr1,ii==0?NULL:fnames,ii,0);
				if (pathtype==1){
					MYFREE(auxptr1);
				}
				if (0!=jj){
					fclose(fhndl);
					MYFREE(line);
					MYFREE(fnames);
					MYFREE(pending);
					if (firsttime){
						alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
						alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
						setitimer(ITIMER_REAL,&alarmtime,NULL);
					}
					return(jj);
				}
				pbmflags|=EXISTAXIOMFILE;

				/* Check that all formula names in scope of the include file have been found. */
				for (jj=fnused;jj<ii;jj+=(2+kk)){
					kk=strlen(&fnames[jj]);
					if (fnames[jj+kk+1]==0){
						printf("Formula name %s not found. Process stopped\n",&fnames[jj]);
						fclose(fhndl);
						MYFREE(line);
						MYFREE(fnames);
						MYFREE(pending);
						if (firsttime){
							alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
							alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
							setitimer(ITIMER_REAL,&alarmtime,NULL);
						}
						return(1);
					}
				}

				/* Update formula names in scope of current file being imported. */
				memcpy(fnprev,fnames,fnused);
				break;

			/* FOF or CNF directive. */
			case 1:
			case 2:

				/* Check number of arguments. */
				if (cc<2){
					printf("Syntax error: Too few arguments. Process stopped\n");
					fclose(fhndl);
					MYFREE(line);
					MYFREE(fnames);
					MYFREE(pending);
					if (firsttime){
						alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
						alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
						setitimer(ITIMER_REAL,&alarmtime,NULL);
					}
					return(1);
				}

				/* Check formula name. */
				if (fnprev!=NULL){
					auxptr1=SkipBlanks(&auxptr1[1]);
					line[comas[0]]=0;
					for (ii=jj=0;jj<fnused;jj+=(2+kk)){
						kk=strlen(&fnprev[jj]);
						if (0==strcmp(auxptr1,&fnprev[jj])){
							if (fnprev[jj+kk+1]==0){
								fnprev[jj+kk+1]='\1';
								ii=1;
								jj=fnused;
							} else {
								printf("Formula name \"%s\" used more than once. Process stopped\n",auxptr1);
								fclose(fhndl);
								MYFREE(line);
								MYFREE(fnames);
								MYFREE(pending);
								if (firsttime){
									alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
									alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
									setitimer(ITIMER_REAL,&alarmtime,NULL);
								}
								return(1);
							}
						}
					}
				} else {
					ii=1;
				}

				/* Formula must be read. */
				if (ii){

					/* Get role of formula. */
					auxptr1=SkipBlanks(&line[1+comas[0]]);
					line[comas[1]]=0;
					if (0==strcmp(auxptr1,"axiom")){
						ii=AXIOM;
					} else if (0==strcmp(auxptr1,"hypothesis")){
						/*if (0==firsttime){
							printf("Warning: not axiom role formula in include file.\n");
						}*/
						ii=HYPOTHESIS;
					} else if (0==strcmp(auxptr1,"definition")){
						ii=DEFINITION;
					} else if (0==strcmp(auxptr1,"assumption")){
						ii=ASSUMPTION;
					} else if (0==strcmp(auxptr1,"negated_conjecture")){
						if (0==firsttime){
							printf("Negated conjectures are not accepted in include files, process stopped\n");
							fclose(fhndl);
							MYFREE(line);
							MYFREE(fnames);
							MYFREE(pending);
							if (firsttime){
								alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
								alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
								setitimer(ITIMER_REAL,&alarmtime,NULL);
							}
							return(1);
						}
						ii=NEGCONJ;
					} else if (0==strcmp(auxptr1,"conjecture")){
						if (0==firsttime){
							printf("Conjectures are not accepted in include files, process stopped\n");
							fclose(fhndl);
							MYFREE(line);
							MYFREE(fnames);
							MYFREE(pending);
							if (firsttime){
								alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
								alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
								setitimer(ITIMER_REAL,&alarmtime,NULL);
							}
							return(1);
						}
						if (mm==2){
							printf("Conjectures are not accepted in CNF formulas, process stopped\n");
							fclose(fhndl);
							MYFREE(line);
							MYFREE(fnames);
							MYFREE(pending);
							if (firsttime){
								alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
								alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
								setitimer(ITIMER_REAL,&alarmtime,NULL);
							}
							return(1);
						}
						if (conjinput){
							printf("Only one conjecture is accepted in a problem, process stopped\n");
							fclose(fhndl);
							MYFREE(line);
							MYFREE(fnames);
							MYFREE(pending);
							if (firsttime){
								alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
								alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
								setitimer(ITIMER_REAL,&alarmtime,NULL);
							}
							return(1);
						}
						ii=CONJECTURE;
					} else if (0==strcmp(auxptr1,"lemma")){
						ii=LEMMA;
					} else if (0==strcmp(auxptr1,"theorem")){
						ii=THEOREM;
					} else if (0==strcmp(auxptr1,"corollary")){
						ii=COROLLARY;
					} else if (0==strcmp(auxptr1,"plain")){ /* Plain role is accepted although not sure this is correct. */
						ii=PLAIN;
					} else if (0==strcmp(auxptr1,"type")){
						printf("Formula role \"type\" not accepted, process stopped\n");
						fclose(fhndl);
						MYFREE(line);
						MYFREE(fnames);
						MYFREE(pending);
						if (firsttime){
							alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
							alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
							setitimer(ITIMER_REAL,&alarmtime,NULL);
						}
						return(1);
					} else {
						printf("Invalid formula role, process stopped\n");
						fclose(fhndl);
						MYFREE(line);
						MYFREE(fnames);
						MYFREE(pending);
						if (firsttime){
							alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
							alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
							setitimer(ITIMER_REAL,&alarmtime,NULL);
						}
						return(1);
					}

					/* Formula is not redundant or redundant formulas are accepted. */
					if ((ii<LEMMA)||(ii>COROLLARY)||(loadredundant==1)){

						/* Isolate formula. */
						auxptr1=SkipBlanks(&line[1+comas[1]]);
						if (cc>2){
							jj=comas[2]-(auxptr1-line);
						} else {
							for (jj=strlen(auxptr1)-2;
									((jj>=0)&&((auxptr1[jj]==' ')||(auxptr1[jj]=='\t')));
									jj--){
							}
							if ((jj<0)||(auxptr1[jj]!=')')){
								printf("Unexpected syntax error or program bug. Process stopped\n");
								fclose(fhndl);
								MYFREE(line);
								MYFREE(fnames);
								MYFREE(pending);
								if (firsttime){
									alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
									alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
									setitimer(ITIMER_REAL,&alarmtime,NULL);
								}
								return(1);
							}
						}
						auxptr1[jj]=0;

						/* If formula is CNF then convert it to FOF. */
						if (mm==2){
							if (NOMEMORY==Cnf2Fof(&auxptr1)){
								printf("%% SZS status MemoryOut : ");
								printf("Not enough memory, process stopped\n");
								fclose(fhndl);
								MYFREE(line);
								MYFREE(fnames);
								MYFREE(pending);
								if (firsttime){
									alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
									alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
									setitimer(ITIMER_REAL,&alarmtime,NULL);
								}
								return(1);
							}
						}

						/* Process conjecture. */
						strcat(auxptr1,".");
						if (ii==CONJECTURE){
							switch (Ask(auxptr1)){
								case 0:
								case NOMEMORY:
									if (pgmmode==INTERACTIVE){
										printf("Process stopped\n");
									}
									fclose(fhndl);
									MYFREE(line);
									MYFREE(fnames);
									MYFREE(pending);
									if (firsttime){
										alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
										alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
										setitimer(ITIMER_REAL,&alarmtime,NULL);
									}
									return(1);
									break;
								case TIMEOUT:
									if (pgmmode==INTERACTIVE){
										printf("Process stopped\n");
									}
									fclose(fhndl);
									MYFREE(line);
									MYFREE(fnames);
									MYFREE(pending);
									if (firsttime){
										alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
										alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
										setitimer(ITIMER_REAL,&alarmtime,NULL);
										gettimeofday(&mainkb.endtime,NULL);
										mainkb.prstats.elapsed_time+=(mainkb.endtime.tv_sec-mainkb.time.tv_sec
												+(mainkb.endtime.tv_usec-mainkb.time.tv_usec)/1000000.0);
										end=clock();
										cpu_time+=(((double)(end-start))/CLOCKS_PER_SEC);
									}
									return(3);
									break;
							}
							conjinput=1;

						/* Process negated conjecture, axiom or axiom like. */
						} else {
							switch (Tell(auxptr1,ii,(ii==NEGCONJ?NEGCONJ:0)|(firsttime?FROMMAINFILE:0))){
								case 0:
								case NOMEMORY:
									if (pgmmode==INTERACTIVE){
										printf("Process stopped\n");
									}
									fclose(fhndl);
									MYFREE(line);
									MYFREE(fnames);
									MYFREE(pending);
									if (firsttime){
										alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
										alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
										setitimer(ITIMER_REAL,&alarmtime,NULL);
									}
									return(1);
									break;
								case TIMEOUT:
									if (pgmmode==INTERACTIVE){
										printf("Process stopped\n");
									}
									fclose(fhndl);
									MYFREE(line);
									MYFREE(fnames);
									MYFREE(pending);
									if (firsttime){
										gettimeofday(&mainkb.endtime,NULL);
										mainkb.prstats.elapsed_time+=(mainkb.endtime.tv_sec-mainkb.time.tv_sec
												+(mainkb.endtime.tv_usec-mainkb.time.tv_usec)/1000000.0);
										end=clock();
										cpu_time+=(((double)(end-start))/CLOCKS_PER_SEC);
									}
									if (firsttime){
										alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
										alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
										setitimer(ITIMER_REAL,&alarmtime,NULL);
									}
									return(3);
									break;
									break;
							}
							if (ii==NEGCONJ){
								pbmflags|=EXISTNEGCONJ;
							}
						}

						/* If formula is CNF then free memory of converted formula. */
						if (mm==2){
							MYFREE(auxptr1);
						}

					/* Redundant formula not loaded. */
					} else {
						printf("Redundant formula not loaded.\n");
					}

				/* Formula must not be read. This message is not really necessary. */
				/*} else {
					printf("Formula %s not loaded because formula name not specified.\n",auxptr1);*/
				}
				break;
		}
	}

	/* Accumulate time statistics. */
	if (firsttime){
		gettimeofday(&mainkb.endtime,NULL);
		mainkb.prstats.elapsed_time+=(mainkb.endtime.tv_sec-mainkb.time.tv_sec
				+(mainkb.endtime.tv_usec-mainkb.time.tv_usec)/1000000.0);
		end=clock();
		cpu_time+=(((double)(end-start))/CLOCKS_PER_SEC);
	}

	/* Close file, free memory and return. */
	fclose(fhndl);
	MYFREE(line);
	MYFREE(fnames);
	MYFREE(pending);
	return(0);
} /* Import */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Read a directive from a TPTP file)  OCJ
 *
 *    This function reads a directive (include or formula) from a TPTP
 *    file. Read starts from current file position and ends when an
 *    uncommented "." character is read or end of file is reached.
 *    Comments, end of line and leading and trailing spaces and tab
 *    characters are removed. Quotes are enforced to be closed in the
 *    same line that they are opened. The remaining text should be a
 *    directive but no syntax checking is performed.
 *
 *    The buffer where result is placed is reallocated if additional
 *    space is required.
 *
 *    It is assumed that the input file has no null characters.
 *
 *
 *  ARGUMENTS:
 *
 *    pbuffer: Address of pointer to buffer where the line will be placed.
 *             May be reallocated.
 *    psize: Address of size of buffer.
 *    fhndl: File handle.
 *
 *  RETURNS:
 *
 *    0-> Read was successful.
 *    1-> Nothing was read because end of file was reached.
 *    2-> An error occurred reading the file.
 *    3-> Unclosed quote in line read.
 *    4-> Missing dot at end of directive.
 *    5-> Non critical error found. Specifically the directive is correct
 *        except for a dot found before the real end but between
 *        brackets. This is not allowed in theory but is not a critical
 *        error and the directive can be processed.
 *    NOMEMORY-> No memory available.
 *
 *--------------------------------------------------------------*/
int32_t ReadDirective(char **pbuffer,int32_t *psize,FILE *fhndl){
	auto int32_t used;                                   /* Used bytes in *pbuffer so far */
	auto char *buffer;                                   /* Pointer to buffer */
	auto int32_t opncomment;                             /* Non zero if an unclosed block comment */
	auto int32_t opnbrkt;                                /* Number of open brackets pending */
	auto long filepos;                                   /* Position to start reading next directive */
	auto int32_t eof;                                    /* End of file indicator */
	auto int32_t rc;                                     /* Non critical return code (0 or 5) */
	auto int32_t ii,jj,kk,ll,mm,nn,pp,qq,rr,cc;          /* Auxiliary */
	auto char *auxptr;                                   /* Auxiliary pointer */

	/* Main loop. */
	rc=0;
	used=0;
	buffer=*pbuffer;
	buffer[0]=0;
	opncomment=0;
	mm=1;
	opnbrkt=0;
	eof=0;
	do {

		/* Read lines until line is not empty and it is not a %-comment. */
		do {

			/* Read line: read until new line character or end of file. */
			jj=used;
			do {

				/* Check available space. */
				if (((*psize)-jj)<INPUT_CHUNK_SIZE){
					*psize+=INPUT_CHUNK_SIZE;
					if (NULL==(auxptr=MYREALLOC(buffer,*psize))){
						return(NOMEMORY);
					}
					*pbuffer=buffer=auxptr;
				}

				/* Read chunk of text. */
				if (NULL==fgets(&buffer[jj],INPUT_CHUNK_SIZE,fhndl)){
					if (feof(fhndl)){
						if (jj==0){
							return(1);
						}
						ii=strlen(buffer)-1;
						eof=1;
						break;
					}
					return(2);
				}
				jj=strlen(buffer);
			} while (buffer[ii=jj-1]!='\n');

			/* Get file position and check end of file. */
			filepos=ftell(fhndl);
			if (eof){
				break;
			}

			/* Reach first non carriage return, non commented string, non space */
			/* and non tab character. */
			for (kk=used,nn=0;nn==0;kk++){
				switch (buffer[kk]){
					case '\n':
						kk--;
						nn=1;
						break;
					case '/':
						if (opncomment==0){
							if (buffer[kk+1]=='*'){
								opncomment=1;
								kk++;
							} else {
								kk--;
								nn=1;
							}
						}
						break;
					case '*':
						if (opncomment!=0){
							if (buffer[kk+1]=='/'){
								opncomment=0;
								kk++;
							}
						} else {
							nn=1;
						}
						break;
					case '\r':
					case '\t':
					case ' ':
						break;
					default:
						if (opncomment==0){
							kk--;
							nn=1;
						}
						break;
				}
			}
		} while ((buffer[kk]=='%')&&(eof==0));

		/* Leave the main loop if end of file was reached. */
		if (eof){
			break;
		}

		/* If there is an effective dot in the line read then adjust filepos */
		/* just after the dot. It is assumed that the string includes the */
		/* line feed '\n' character read by fgets() function. */
		ll=strlen(&buffer[kk]);
		for (qq=kk,nn=rr=0,pp=opnbrkt,cc=opncomment;nn==0;qq++,rr++){
			switch (buffer[qq]){
				case '\n':
					nn=1;
					break;
				case '/':
					if ((cc==0)&&(buffer[qq+1]=='*')){
						cc=1;
						qq++;
						rr++;
					}
					break;
				case '*':
					if ((cc!=0)&&(buffer[qq+1]=='/')){
						cc=0;
						qq++;
						rr++;
					}
					break;
				case '[':
					if (cc==0){
						pp++;
					}
					break;
				case ']':
					if (cc==0){
						pp--;
					}
					break;
				case '\'':
				case '"':
					if (cc==0){
						if (NULL==(auxptr=JumpOverQuotes(&buffer[qq]))){
							nn=1;
						} else {
							jj=auxptr-&buffer[qq];
							rr+=jj;
							qq+=jj;
						}
					}
					break;
				case '.':
					if ((cc==0)&&(pp==0)){
						filepos-=(ll-rr-1);
						nn=1;
					}
					break;
			}
		}

		/* Remove trailing line feed and carriage return characters. */
		if ((buffer[ii]=='\n')||(buffer[ii]=='\r')){
			buffer[ii]=0;
			ii--;
			if ((ii>=0)&&((buffer[ii]=='\n')||(buffer[ii]=='\r'))){
				buffer[ii]=0;
				ii--;
			}
		}

		/* Remove trailing space and tab characters. */
		for (;(ii>=used)&&((buffer[ii]==' ')||(buffer[ii]=='\t'));ii--){
		}
		buffer[ii+1]=0;

		/* Text transfer loop. */
		for (ii=kk;(buffer[ii]!=0)&&(mm);ii++){
			switch (buffer[ii]){
				case '(':
					if (opncomment==0){
						buffer[used]=buffer[ii];
						used++;
					}
					break;
				case ')':
					if (opncomment==0){
						buffer[used]=buffer[ii];
						used++;
					}
					break;
				case '[':
					if (opncomment==0){
						opnbrkt++;
						buffer[used]=buffer[ii];
						used++;
					}
					break;
				case ']':
					if (opncomment==0){
						opnbrkt--;
						buffer[used]=buffer[ii];
						used++;
					}
					break;
				case '\'':
				case '"':
					if (opncomment==0){
						if (NULL==(auxptr=JumpOverQuotes(&buffer[ii]))){
							jj=1+strlen(&buffer[ii]);
							memmove(&buffer[used],&buffer[ii],jj);
							buffer[used+jj]=0;
							return(3);
						}
						jj=1+(auxptr-&buffer[ii]);
						memmove(&buffer[used],&buffer[ii],jj);
						used+=jj;
						ii+=(jj-1);
					}
					break;
				case '/':
					if ((opncomment==0)&&(buffer[ii+1]=='*')){
						opncomment=1;
						ii++;
					} else {
						buffer[used]=buffer[ii];
						used++;
					}
					break;
				case '*':
					if ((opncomment!=0)&&(buffer[ii+1]=='/')){
						opncomment=0;
						ii++;
					} else {
						buffer[used]=buffer[ii];
						used++;
					}
					break;
				case '%':
					if (opncomment==0){
						ii+=(strlen(&buffer[ii])-1);
					}
					break;
				case '.':
					if (opncomment==0){
						if (opnbrkt>0){
							rc=5; /* Set return code to non critical error. */
						} else {
							buffer[used]=buffer[ii];
							used++;
							mm=0; /* Leave both loops. */
						}
					}
					break;
				default:
					if (opncomment==0){
						buffer[used]=buffer[ii];
						used++;
					}
					break;
			}
		}
	} while (mm);
	jj=buffer[used];
	buffer[used]=0;

	/* Check final dot character. */
	if (buffer[used-1]!='.'){
		return(4);
	}

	/* Reposition file just after the dot. */
	if (jj!=0){
		fseek(fhndl,filepos,SEEK_SET);
	}

	return(rc);
} /* ReadDirective */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check brackets and parenthesis balance)  OCJ
 *
 *    This function check brackets and parenthesis balance in a TPTP
 *    directive. It also detects the cases where the open brackets
 *    match the closed ones but there they are not coherent, for
 *    instance something like (...[...)...].
 *
 *    It is assumed that the directive has been read by ReadDirective()
 *    function with no errors, so error checking from calls to
 *    JumpOverQuotes() function is not performed and it is assumed that
 *    a dot in *pbufffer exist.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    pbuffer: Address of pointer to buffer for pending parentheses
 *             and brackets list.
 *    psize: Address of size of buffer.
 *    pbracket: On input this is the address of pointer to the string
 *              to be checked. On exit *pbracket points to the first
 *              non space and non tab character after the last parenthesis
 *              if no error is found or to the first offending character
 *              if an error is found.
 *
 *  RETURNS:
 *
 *    0-> No error found.
 *    1-> An error was found.
 *    NOMEMORY-> Not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t CheckBrackets(char **pbuffer,int32_t *psize,char **pbracket){
	auto char *ptr1,*ptr2,*ptr3,*ptr4;                   /* Auxiliary pointers */
	auto int32_t ii;                                     /* Auxiliary */

	/* Main loop. */
	ptr1=*pbuffer;
	ii=-1;
	ptr4=*pbracket;
	for (ptr2=strpbrk(*pbracket,"\"'()[]");ptr2!=NULL;ptr2=strpbrk(ptr4=&ptr2[1],"\"'()[]")){
		switch (*ptr2){
			case '\'':
			case '"':
				ptr2=JumpOverQuotes(ptr2);
				break;
			case '(':
				ii++;
				if ((*psize)<=ii){
					*psize+=INPUT_CHUNK_SIZE;
					if (NULL==(ptr3=MYREALLOC(*pbuffer,*psize))){
						return(NOMEMORY);
					}
					*pbuffer=ptr3;
				}
				ptr1[ii]='(';
				break;
			case '[':
				ii++;
				if ((*psize)<=ii){
					*psize+=INPUT_CHUNK_SIZE;
					if (NULL==(ptr3=MYREALLOC(*pbuffer,*psize))){
						return(NOMEMORY);
					}
					*pbuffer=ptr3;
				}
				ptr1[ii]='[';
				break;
			case ')':
				if ((ii<0)||(ptr1[ii]!='(')){
					*pbracket=ptr2;
					return(1);
				} else {
					ii--;;
				}
				break;
			case ']':
				if ((ii<0)||(ptr1[ii]!='[')){
					*pbracket=ptr2;
					return(1);
				} else {
					ii--;;
				}
				break;
		}
	}

	/* Set *pbracket and return. */
	for (;(ptr4[0]==' ')||(ptr4[0]=='\t');ptr4++){
	}
	*pbracket=ptr4;
	return(0);
} /* CheckBrakets */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Convert a CNF formula to FOF)  OCJ
 *
 *    This function converts a CNF formula to FOF by adding an
 *    universal quantifier at the beginning of it with all variables
 *    found. New storage is allocated for converted formula and the
 *    pointer is passed back in the original pointer to caller function.
 *    This storage must be freed by the caller function.
 *
 *    To identify variables the criteria is different if the current
 *    syntax is TPTP or DRODI:
 *    - If the current syntax is TPTP the formula is scanned and names
 *      starting with upper case are identified as variables.
 *    - If the current syntax is DRODI names of the form Xnnnn where
 *      nnnn is a valid integer are identified as variables.
 *    No other syntax considerations are taken into account so any syntax
 *    errors must be detected afterwards by the calling function if
 *    applicable.
 *
 *    Names are identified because they start with an alphabetic
 *    character not preceded by an alphanumeric character and
 *    end with a non alphanumeric character.
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Address of pointer to formula. Pointer to formula will
 *             be set to allocated storage on return.
 *
 *  RETURNS:
 *
 *    0-> Conversion was successful.
 *    NOMEMORY-> No memory available.
 *
 *--------------------------------------------------------------*/
int32_t Cnf2Fof(char **formula){
	auto char *auxptr1;                                  /* Pointer to converted formula buffer */
	auto int32_t size;                                   /* Size of converted formula buffer */
	auto char *auxptr4;                                  /* Pointer to variable names buffer */
	auto char *auxptr2,*auxptr3,*auxptr5;                /* Auxiliary pointers */
	auto int32_t ii,jj,kk,mm,nn,vv;                      /* Auxiliary */

	/* Allocate space for converted formula. */
	auxptr2=*formula;
	jj=strlen(auxptr2);
	size=jj+INPUT_CHUNK_SIZE;
	if (NULL==(auxptr1=MYALLOC(size))){
		return(NOMEMORY);
	}

	/* Allocate and initialize space for variable names. */
	if (NULL==(auxptr4=MYALLOC((2*jj)+2))){
		MYFREE(auxptr1);
		return(NOMEMORY);
	}
	strcpy(auxptr4,"#");

	/* Allocate and initialize auxiliary space. */
	if (NULL==(auxptr5=MYALLOC((2*jj)+2))){
		MYFREE(auxptr1);
		MYFREE(auxptr4);
		return(NOMEMORY);
	}
	strcpy(auxptr5,"#");

	/* Initialize quantifier. */
	strcpy(auxptr1,"![");
	nn=jj+9;

	/* Scan formula. */
	for (ii=kk=0;ii<jj;ii++){

		/* Start of a name. */
		if ((isalpha(auxptr2[ii]))&&((ii==0)||(!isalnum(auxptr2[ii-1])))){

			/* Get name length. */
			for (mm=ii+1;(isalnum(auxptr2[mm])||(auxptr2[mm]=='_'));mm++){
			}
			mm-=ii;

			/* Set vv to 1 if the name is a variable. Otherwise set it to 0 */
			if (syntaxmode==TPTP){
				if (isupper(auxptr2[ii])){
					vv=1;
				} else {
					vv=0;
				}
			} else {
				if (auxptr2[ii]=='X'){
					vv=strtol(&auxptr2[ii+1],&auxptr3,10);
					if ((vv>=0)&&((auxptr3-&auxptr2[ii+1])<mm)){
						vv=1;
					} else {
						vv=0;
					}
				} else {
					vv=0;
				}
			}

			/* Name is a variable.  */
			if (vv){

				/* Variable has not been added yet, add it to quantifier. */
				auxptr5[1]=0;
				strncat(auxptr5,&auxptr2[ii],mm);
				strcat(auxptr5,"#");
				if (NULL==strstr(auxptr4,auxptr5)){
					if ((nn+mm)>size){
						size=nn+mm+INPUT_CHUNK_SIZE;
						auxptr3=auxptr1;
						if (NULL==(auxptr3=MYREALLOC(auxptr3,size))){
							MYFREE(auxptr1);
							MYFREE(auxptr4);
							MYFREE(auxptr5);
							return(NOMEMORY);
						}
						auxptr1=auxptr3;
					}
					if (kk){
						strcat(auxptr1,",");
						nn++;
					}
					kk=1;
					strncat(auxptr1,&auxptr2[ii],mm);
					nn+=mm;
					strcat(auxptr4,&auxptr5[1]);
				}
			}

			/* Skip name. */
			ii+=mm;
		}
	}

	/* If quantifier is not empty then add remaining quantifier */
	/* characters and original formula. */
	if (nn>(jj+9)){
		strcat(auxptr1,"]: (");
		strcat(auxptr1,auxptr2);
		strcat(auxptr1,")");

	/* If quantifier is empty then copy original formula. */
	} else {
		strcpy(auxptr1,auxptr2);
	}

	/* Set formula pointer to converted formula. */
	*formula=auxptr1;

	/* Free memory and return. */
	MYFREE(auxptr4);
	MYFREE(auxptr5);
	return(0);
} /* Cnf2Fof */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if a file exist)  OCJ
 *
 *    This function checks if a file exist and can be accessed
 *    in read mode by the user by trying to open it in read mode.
 *
 *
 *  ARGUMENTS:
 *
 *    filename: Pointer to file name.
 *
 *  RETURNS:
 *
 *    0-> File doesn't exist or cannot be accessed in read mode
 *        by the user.
 *    1-> File exist and can be accessed by the user.
 *
 *--------------------------------------------------------------*/
int32_t FileExist(char *filename){
	auto FILE *filehndl;                                 /* File handle */
	if (NULL==(filehndl=fopen(filename,"r"))){
		return(0);
	}
	fclose(filehndl);
	return(1);
} /* ExistFile */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Print current data of a knowledge base)  OCJ
 *
 *    This function prints the following data of the working KB:
 *    - KB status.
 *    - All A-Clauses: text, unprocessed, active and passive.
 *    - Component symbol definitions and  their values in the
 *      last model found or in the last assignement before
 *      a refutation was found.
 *    - All SAT clauses.
 *
 *    IMPORTANT: This function must be called before VerifySatStatus()
 *    and VerifySatProof().
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
void PrintKBData(void){
	auto satnode *node;                                  /* Pointer to SAT node */

	/* Print KB status. */
	printf("\nKB DATA DUMP\n");
	printf("============\n");
	switch (kbset.status){
		case STOPPED:
			printf("Stopped with no results obtained\n");
			break;
		case SATISFIABLE:
			printf("Stopped and satisfiable\n");
			break;
		case UNSATISFIABLE:
			printf("Stopped and unsatisfiable\n");
			break;
		case UNKNOWN:
			printf("Stopped and problem is unknown\n");
			break;
		case TIMEOUT:
			printf("Stopped because a timeout condition\n");
			break;
		case NOMEMORY:
			printf("Stopped because a no memory condition\n");
			break;
	}

	/* Print KB data and return. */
	if (NOMEMORY==PrintAClauses()){
		printf("\n... printing stopped due to lack of memory.\n");
	} else if (NOMEMORY==PrintCompSymbols()){
		printf("\n... printing stopped due to lack of memory.\n");
	} else if (NOMEMORY==PrintSATClauses()){
		printf("\n... printing stopped due to lack of memory.\n");
	}

	/* If SAT final status must not be verified and problem had a SAT refutation */
	/* then undo all SAT assignments for the winning process KB. This is necessary */
	/* in order for VerifySatProof() and GetMinUnsatCore() functions to work properly. */
	if ((VERIFYSATFINALSTATUS!=1)&&(procctl->status==UNSATISFIABLE)
			&&(kbset.frstprfnode->type==SATCLAUSE)){

		/* Get last used node. As there was a sat refutation and print KB data is */
		/* enabled then there must be some used SAT nodes. */
		for (node=kbset.satrootnode;(node->nextnode!=NULL)&&(0==(node->nextnode->flags&FREENODE));
				node=node->nextnode){
		}

		/* Undo all SAT assignments. */
		for (;node!=NULL;node=node->prevnode){
			UndoSymbolAssgnmnt(node);
		}
	}
	return;
} /* PrintKBData */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Print A-clauses of a knowledge base)  OCJ
 *
 *    This function prints all A-Clauses of the working KB: text,
 *    unprocessed, active and passive.
 *
 *
 *  ARGUMENTS:
 *
 *    None.
 *
 *  RETURNS:
 *
 *    0 -> Process completed.
 *    NOMEMORY -> Not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t PrintAClauses(void){
	auto cmprefix *ptr1;                                 /* Auxiliary pointer */
	auto char *ptr2;                                     /* Auxiliary pointer */

	/* Print text clauses of main KB. */
	printf("\nText clauses:\n");
	printf("------------\n");
	for (ptr1=mainkb.firsttxt;ptr1!=NULL;ptr1=ptr1->next){
		if (NOMEMORY==PrintSingleAClause(ptr1)){
			return(NOMEMORY);
		}
		printf("\n");
	}

	/* Print text clauses of argument KB. */
	for (ptr1=kbset.firsttxt;ptr1!=NULL;ptr1=ptr1->next){
		if (NOMEMORY==PrintSingleAClause(ptr1)){
			return(NOMEMORY);
		}
		printf("\n");
	}

	/* Print unprocessed clauses. */
	printf("\nUnprocessed clauses:\n");
	printf("-------------------\n");
	if (kbset.frstunproc!=NULL){
		for (ptr1=kbset.frstunproc;ptr1!=NULL;ptr1=ptr1->next){
			if (NOMEMORY==PrintSingleAClause(ptr1)){
				return(NOMEMORY);
			}
			printf("\n");
		}
	} else {
		printf("None\n");
	}

	/* Print passive clauses. */
	printf("\nPassive clauses:\n");
	printf("---------------\n");
	if (kbset.frstpassive!=NULL){
		for (ptr1=kbset.frstpassive;ptr1!=NULL;ptr1=ptr1->next){
			if (NOMEMORY==PrintSingleAClause(ptr1)){
				return(NOMEMORY);
			}
			printf("\n");
		}
	} else {
		printf("None\n");
	}

	/* Print active clauses. */
	printf("\nActive clauses:\n");
	printf("--------------\n");
	if (kbset.frstactive!=NULL){
		for (ptr1=kbset.frstactive;ptr1!=NULL;ptr1=ptr1->next){
			if (NOMEMORY==PrintSingleAClause(ptr1)){
				return(NOMEMORY);
			}
			printf("\n");
		}
	} else {
		printf("None\n");
	}

	/* Print locked clauses. */
	printf("\nLocked clauses:\n");
	printf("--------------\n");
	if (kbset.frstlocked!=NULL){
		for (ptr1=kbset.frstlocked;ptr1!=NULL;ptr1=ptr1->next){
			if (NOMEMORY==PrintSingleAClause(ptr1)){
				return(NOMEMORY);
			}
			printf(" locks = ");
			if (ptr1->part2.bin->lockasserts!=NULL){
				if (NULL==(ptr2=DecompileAsserts(ptr1->part2.bin->lockasserts,0))){
					return(NOMEMORY);
				}
				printf("%s\n",ptr2);
				MYFREE(ptr2);
			} else {
				printf("{}\n");
			}
		}
	} else {
		printf("None\n");
	}
	return(0);
} /* PrintAClauses */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Print a single A-clause)  OCJ
 *
 *    This function prints the A-Clause passed as argument.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to cmprefix structure of the clause
 *            to be printed.
 *
 *  RETURNS:
 *
 *    0 -> Process completed.
 *    NOMEMORY -> Not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t PrintSingleAClause(cmprefix *clause){
	auto cmprefix *ptr1;                                 /* Auxiliary pointer */
	auto char *ptr2,*ptr3,*ptr4;                         /* Auxiliary pointers */
	auto int32_t ii;                                     /* Auxiliary */
	auto char jj,kk;                                     /* Auxiliary */

	/* Print first part with clause number. */
	ptr4=NULL;
	if (NUMBERED&clause->flags){
		if ((proofmode==TPTP)&&(syntaxmode==TPTP)){
			printf("fof(f%lu,",clause->number);
		} else {
			printf("%lu. ",clause->number);
		}
	} else {
		if ((proofmode==TPTP)&&(syntaxmode==TPTP)){
			printf("fof(f??,");
		} else {
			printf("N/A. ");
		}
	}
	if ((proofmode==TPTP)&&(syntaxmode==TPTP)){
		switch (clause->inference){
			case CONJECTURE:
				printf("conjecture,(\n  ");
				break;
			case AXIOM:
			#ifdef ENABLECHOICEAXIOM
			case CHOICEAXIOM:
			#endif
				printf("axiom,(\n  ");
				break;
			case HYPOTHESIS:
				printf("hypothesis,(\n  ");
				break;
			case ASSUMPTION:
				printf("assumption,(\n  ");
				break;
			case LEMMA:
				printf("lemma,(\n  ");
				break;
			case THEOREM:
				printf("theorem,(\n  ");
				break;
			case COROLLARY:
				printf("corollary,(\n  ");
				break;
			case NEGCONJ:
				printf("negated_conjecture,(\n  ");
				break;
			case DEFINITION:
			case PREDICATEDEF:
			case GOALDEFINITION:
				printf("definition,(\n  ");
				break;
			default:
				printf("plain,(\n  ");
				break;
		}
	}

	/* Print text A-clause. */
	if (clause->flags&(TEXTFORMULA)){
		switch (clause->inference){
			case CONJECTURE:
			case AXIOM:
			case HYPOTHESIS:
			case DEFINITION:
			case ASSUMPTION:
			case LEMMA:
			case THEOREM:
			case COROLLARY:
			case PLAIN:
			case NEGCONJ:
			case PRENNFCONV:
			case FORMULARENAMING:
			case PREDICATEDEF:
			case NNFCONV:
			case MINISCOPING:
			case SKOLEMIZATION:
			case TFSIMPLIFICATION:
			#ifdef ENABLECHOICEAXIOM
			case CHOICEAXIOM:
			#endif
				printf("%s",clause->part2.text);
				break;
			default:
				ptr4=ptr3=clause->part2.text;
				if (NOMEMORY==Cnf2Fof(&ptr3)){
					return(NOMEMORY);
				}
				printf("%s",ptr3);
				MYFREE(ptr3);
				break;

		}

	/* Print binary A-Clause. */
	} else {
		if (NULL==(ptr3=Decompile(clause->part2.bin))){
			return(NOMEMORY);
		}
		ptr4=ptr3;
		if (NOMEMORY==Cnf2Fof(&ptr3)){
			MYFREE(ptr4);
			return(NOMEMORY);
		}
		printf("%s",ptr3);
		MYFREE(ptr3);
	}

	/* Print inference and parent clause numbers. */
	if ((proofmode==TPTP)&&(syntaxmode==TPTP)){
		printf("),\n  ");
		switch (clause->inference){
			case CONJECTURE:
			case AXIOM:
			case HYPOTHESIS:
			case DEFINITION:
			case ASSUMPTION:
			case LEMMA:
			case THEOREM:
			case COROLLARY:
			case PLAIN:
				if (importfile!=NULL){
					printf("file('%s'",importfile);
				} else {
					printf("file(stdin");
				}
				break;
			case NEGCONJ:
				if (clause->parent1!=NULL){
					printf("inference(negated_conjecture,[status(cth)],[f%lu]",clause->parent1->number);
				} else if (importfile!=NULL){
					printf("file('%s'",importfile);
				} else {
					printf("file(stdin");
				}
				break;
			case CLAUSIFY:
				printf("inference(cnf_transformation,[status(thm)],[f%lu]",clause->parent1->number);
				break;
			case RESOLUTION:
				printf("inference(resolution,[status(thm)],[f%lu,f%lu]",clause->parent1->number,
						clause->parent2->number);
				break;
			case FACTORING:
				printf("inference(factoring,[status(thm)],[f%lu]",clause->parent1->number);
				break;
			case PARAMODULATION:
				printf("inference(paramodulation,[status(thm)],[f%lu,f%lu]",clause->parent1->number,
						clause->parent2->number);
				break;
			case FWDEMODULATION:
				printf("inference(forward_demodulation,[status(thm)],[f%lu,f%lu]",clause->parent1->number,
						clause->parent2->number);
				break;
			case BKDEMODULATION:
				printf("inference(backward_demodulation,[status(thm)],[f%lu,f%lu]",clause->parent1->number,
						clause->parent2->number);
				break;
			case REMOVEDUPS:
				printf("inference(duplicate_literals_removal,[status(thm)],[f%lu]",clause->parent1->number);
				break;
			case TRIVEQRES:
				printf("inference(trivial_equality_resolution,[status(thm)],[f%lu]",clause->parent1->number);
				break;
			case EQRESOLUTION:
				printf("inference(equality_resolution,[status(thm)],[f%lu]",clause->parent1->number);
				break;
			case DESTEQRES:
				printf("inference(destructive_equality_resolution,[status(thm)],[f%lu]",clause->parent1->number);
				break;
			case EQFACTORING:
				printf("inference(equality_factoring,[status(thm)],[f%lu]",clause->parent1->number);
				break;
			case FWSUBSRESLTNS:
				printf("inference(forward_subsumption_resolution,[status(thm)],[f%lu,f%lu]",clause->parent1->number,
						clause->parent2->number);
				break;
			case BKSUBSRESLTNS:
				printf("inference(backward_subsumption_resolution,[status(thm)],[f%lu,f%lu]",clause->parent1->number,
						clause->parent2->number);
				break;
			case COMPONENT:
				printf("inference(component_clause,[status(thm)],[f%lu]",((hashchcmp *)(clause->parent1))->defnumber);
				break;
			case PRENNFCONV:
				//printf("inference(pre_NNF_transformation,[status(esa)],[f%lu]",clause->parent1->number);
				printf("inference(pre_NNF_transformation,[status(thm)],[f%lu]",clause->parent1->number);
				break;
			case PREDICATEDEF:
				/* The commented out code is left for documentation. The reason */
				/* is the same explained in RenameBlock() function for other */
				/* commented out code. */
				/*ptr3=strchr(clause->part2.text,'=');
				if ('<'==*(ptr3-1)){
					ptr3--;
				}*/
				/* It is important to note that universal quantifiers and its open parenthesis */
				/* must be removed from predicate name. */
				ptr3=strstr(clause->part2.text,PRDPOSTFIX)+strlen(PRDPOSTFIX);
				jj=ptr3[0];
				ptr3[0]=0;
				if (NULL==(ptr2=strchr(clause->part2.text,'('))){
					ptr2=clause->part2.text;
				} else {
					ptr2++;
				}
				printf("introduced(definition,[new_symbols(definition,[%s])],[]",ptr2);
				ptr3[0]=jj;
				break;
			case NNFCONV:
				//printf("inference(NNF_transformation,[status(esa)],[f%lu]",clause->parent1->number);
				printf("inference(NNF_transformation,[status(thm)],[f%lu]",clause->parent1->number);
				break;
			case MINISCOPING:
				//printf("inference(miniscoping,[status(esa)],[f%lu]",clause->parent1->number);
				printf("inference(miniscoping,[status(thm)],[f%lu]",clause->parent1->number);
				break;
			case SKOLEMIZATION:

				#ifdef ENABLECHOICEAXIOM
				/* For this case clause->parent1->parent1 is used to store a pointer to a set */
				/* of skldata structure pointers instead of the parent2 clause. */
				/* Print skolems list. */
				printf("inference(skolemize,[status(esa),new_symbols(skolem,[");
				for (ii=0;((skldata **)clause->parent1->parent1)[ii]!=NULL;ii++){
					if (ii>0){
						printf(",");
					}
					printf("%s%lu%s",SKLPREFIX,((skldata **)clause->parent1->parent1)[ii]->sklnumber,SKLPOSTFIX);
				}
				printf("])");

				/* Print skolemize records. */
				for (ii=0;((skldata **)clause->parent1->parent1)[ii]!=NULL;ii++){
					printf(",skolemize(%s,%s%lu%s",ptr2=&((skldata **)clause->parent1->parent1)[ii]->sklvarnames[0],
							SKLPREFIX,((skldata **)clause->parent1->parent1)[ii]->sklnumber,SKLPOSTFIX);
					for (ptr2=&ptr2[1+strlen(ptr2)],kk=0;ptr2[0]!=0;ptr2=&ptr2[1+strlen(ptr2)]){
						if (kk==0){
							printf("(");
							kk=1;
						} else {
							printf(",");
						}
						printf("%s",ptr2);
					}
					if (kk){
						printf(")");
					}
					printf(")");
				}

				/* Print parents. */
				printf("],[f%lu,f%lu]",clause->parent1->number,clause->parent2->number);

				#else
				/* For this case clause->parent2 is used to store a pointer to a set */
				/* of skldata structure pointers instead of the parent2 clause. */
				/* Print skolems list. */
				printf("inference(skolemize,[status(esa),new_symbols(skolem,[");
				for (ii=0;((skldata **)clause->parent2)[ii]!=NULL;ii++){
					if (ii>0){
						printf(",");
					}
					printf("%s%lu%s",SKLPREFIX,((skldata **)clause->parent2)[ii]->sklnumber,SKLPOSTFIX);
				}
				printf("])");

				/* Print skolemize records. */
				for (ii=0;((skldata **)clause->parent2)[ii]!=NULL;ii++){
					printf(",skolemize(%s,%s%lu%s",ptr2=&((skldata **)clause->parent2)[ii]->sklvarnames[0],
							SKLPREFIX,((skldata **)clause->parent2)[ii]->sklnumber,SKLPOSTFIX);
					for (ptr2=&ptr2[1+strlen(ptr2)],kk=0;ptr2[0]!=0;ptr2=&ptr2[1+strlen(ptr2)]){
						if (kk==0){
							printf("(");
							kk=1;
						} else {
							printf(",");
						}
						printf("%s",ptr2);
					}
					if (kk){
						printf(")");
					}
					printf(")");
				}

				/* Print parent. */
				printf("],[f%lu]",clause->parent1->number);
				#endif
				break;
			#ifdef ENABLECHOICEAXIOM
			case CHOICEAXIOM:
				printf("introduced(choice_axiom,[],[]");
				break;
			#endif
			case TFSIMPLIFICATION:
				//printf("inference(true_and_false_simplification,[status(esa)],[f%lu]",clause->parent1->number);
				printf("inference(true_and_false_simplification,[status(thm)],[f%lu]",clause->parent1->number);
				break;
			case FORMULARENAMING:
				printf("inference(formula_renaming,[status(thm)],[f%lu",clause->parent1->number);
				for (ptr1=clause->parent2;ptr1!=NULL;ptr1=ptr1->parent2){
					printf(",f%lu",ptr1->number);
				}
				printf("]");
				break;
			case EQUALITYSPLIT:
				//printf("inference(equality_split,[status(esa)],[f%lu]",clause->parent1->number);
				printf("inference(equality_split,[status(thm)],[f%lu]",clause->parent1->number);
				break;
			case GOALDEFINITION:
				ptr3=strchr(ptr4,'=');
				ptr3[0]=0;
				printf("introduced(definition,[new_symbols(definition,[%s])],[function_definition]",ptr4);
				break;
			case DEFINTNFOLDING:
				printf("inference(definition_folding,[],[f%lu",clause->parent1->number);
				for (ptr1=mainkb.lstunproc;;ptr1=ptr1->prev){
					if (ptr1->inference==DEFINTNFOLDING){
						continue;
					}
					if (ptr1->inference!=GOALDEFINITION){
						break;
					}
					printf(",f%lu",ptr1->number);
				}
				printf("]");
				break;
		}
		printf(")).");
	} else {
		printf(" [");
		switch (clause->inference){
			case CONJECTURE:
				printf("conjecture");
				break;
			case AXIOM:
			case HYPOTHESIS:
			case DEFINITION:
			case ASSUMPTION:
			case LEMMA:
			case THEOREM:
			case COROLLARY:
			case PLAIN:
				printf("input");
				break;
			case NEGCONJ:
				printf("negated conjecture %lu",clause->parent1->number);
				break;
			case CLAUSIFY:
				printf("cnf transformation %lu",clause->parent1->number);
				break;
			case RESOLUTION:
				printf("resolution %lu, %lu",clause->parent1->number,
						clause->parent2->number);
				break;
			case FACTORING:
				printf("factoring %lu",clause->parent1->number);
				break;
			case PARAMODULATION:
				printf("paramodulation from %lu into %lu",clause->parent1->number,
						clause->parent2->number);
				break;
			case FWDEMODULATION:
				printf("forward demodulation from %lu into %lu",clause->parent1->number,
						clause->parent2->number);
				break;
			case BKDEMODULATION:
				printf("backward demodulation from %lu into %lu",clause->parent1->number,
						clause->parent2->number);
				break;
			case REMOVEDUPS:
				printf("duplicate literals removal %lu",clause->parent1->number);
				break;
			case TRIVEQRES:
				printf("trivial equality resolution %lu",clause->parent1->number);
				break;
			case EQRESOLUTION:
				printf("equality resolution %lu",clause->parent1->number);
				break;
			case DESTEQRES:
				printf("destructive equality resolution %lu",clause->parent1->number);
				break;
			case EQFACTORING:
				printf("equality factoring %lu",clause->parent1->number);
				break;
			case FWSUBSRESLTNS:
				printf("forward subsumption resolution %lu, %lu",clause->parent1->number,
						clause->parent2->number);
				break;
			case BKSUBSRESLTNS:
				printf("backward subsumption resolution %lu, %lu",clause->parent1->number,
						clause->parent2->number);
				break;
			case COMPONENT:
				printf("component clause %lu",((hashchcmp *)(clause->parent1))->defnumber);
				break;
			case PRENNFCONV:
				printf("pre NNF transformation %lu",clause->parent1->number);
				break;
			case PREDICATEDEF:
				printf("predicate definition %lu",clause->parent1->number);
				break;
			case NNFCONV:
				printf("NNF transformation %lu",clause->parent1->number);
				break;
			case MINISCOPING:
				printf("miniscoping %lu",clause->parent1->number);
				break;
			case SKOLEMIZATION:
				#ifdef ENABLECHOICEAXIOM
				printf("skolemize %lu %lu",clause->parent1->number,clause->parent2->number);
				#else
				printf("skolemize %lu %lu",clause->parent1->number,clause->prev->number);
				#endif
				break;
			#ifdef ENABLECHOICEAXIOM
			case CHOICEAXIOM:
				printf("choice axiom");
				break;
			#endif
			case TFSIMPLIFICATION:
				printf("true and false simplification %lu",clause->parent1->number);
				break;
			case FORMULARENAMING:
				printf("formula renaming %lu",clause->parent1->number);
				for (ptr1=clause->parent2;ptr1!=NULL;ptr1=ptr1->parent2){
					printf(", %lu",ptr1->number);
				}
				break;
			case EQUALITYSPLIT:
				printf("equality split %lu",clause->parent1->number);
				break;
			case GOALDEFINITION:
				ptr3=strchr(ptr4,'=');
				ptr3[0]=0;
				printf("function symbol definition %s",ptr4);
				break;
			case DEFINTNFOLDING:
				printf("definition folding %lu",clause->parent1->number);
				for (ptr1=mainkb.lstunproc;;ptr1=ptr1->prev){
					if (ptr1->inference==DEFINTNFOLDING){
						continue;
					}
					if (ptr1->inference!=GOALDEFINITION){
						break;
					}
					printf(", %lu",ptr1->number);
				}
				break;
		}
		printf("]");
	}
	if (ptr4!=clause->part2.text){
		MYFREE(ptr4);
	}
	return(0);
} /* PrintSingleAClause */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Print the component symbols of a knowledge base)  OCJ
 *
 *    This function prints all component symbols of a given KB.
 *
 *
 *  ARGUMENTS:
 *
 *    None.
 *
 *  RETURNS:
 *
 *    0 -> Process completed.
 *    NOMEMORY -> Not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t PrintCompSymbols(void){
	auto hashchcmp *symbol;                              /* Pointer to component symbol */
	auto char *ptr1;                                     /* Auxiliary pointer */

	/* Loop through all hashchcmp elements and print them. */
	printf("\nComponent symbols:\n");
	printf("-----------------\n");
	if (kb_hashcmp!=NULL){
		for (symbol=kb_hashcmp;symbol!=NULL;symbol=symbol->nextelem){
			printf("%lu. ",symbol->defnumber);
			printf("%s%d%s <=> ",SPLPREFIX,symbol->number,SPLPOSTFIX);
			if (NULL==(ptr1=Decompile2(symbol->formula))){
				return(NOMEMORY);
			}
			printf("%s [",ptr1);
			MYFREE(ptr1);
			if (procctl->status==UNSATISFIABLE){
				switch ((POSITIVE|NEGATIVE|UNDEFINED)&symbol->flags){
					case POSITIVE:
						printf("TRUE]\n");
						break;
					case NEGATIVE:
						printf("FALSE]\n");
						break;
					case UNDEFINED:
						printf("UNDEFINED]\n");
						break;
				}
			} else {
				switch ((MODELPOSITIVE|MODELNEGATIVE|MODELUNDEFINED)&symbol->flags){
					case MODELPOSITIVE:
						printf("TRUE]\n");
						break;
					case MODELNEGATIVE:
						printf("FALSE]\n");
						break;
					case MODELUNDEFINED:
						printf("UNDEFINED]\n");
						break;
				}
			}
		}
	} else {
		printf("None\n");
	}
	return(0);
} /* PrintCompSymbols */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Print SAT clauses of a knowledge base)  OCJ
 *
 *    This function prints all SAT clauses of the working KB.
 *
 *
 *  ARGUMENTS:
 *
 *    None.
 *
 *  RETURNS:
 *
 *    0 -> Process completed.
 *    NOMEMORY -> Not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t PrintSATClauses(void){
	auto satclause *ptr1;                                /* Auxiliary pointer */
	auto uint8_t *ptr2;                                  /* Auxiliary pointer */
	auto int32_t ii;                                     /* Auxiliary */

	/* Loop through SAT clauses in SAT queue. */
	printf("\nSAT clauses in SAT queue:\n");
	printf("------------------------\n");
	if (kbset.firstsatq==NULL){
		printf("None\n");
	} else {
		for (ptr1=kbset.firstsatq;ptr1!=NULL;ptr1=ptr1->glblnext){

			/* Print SAT clause and its status. */
			if (NOMEMORY==PrintSingleSATClause(ptr1)){
				return(NOMEMORY);
			}
			printf("\n");
		}
	}

	/* Loop through SAT clauses in SAT solver. */
	printf("\nSAT clauses in SAT solver:\n");
	printf("-------------------------\n");
	if (kbset.frstsatglbl==NULL){
		printf("None\n");
	} else {
		for (ptr1=kbset.frstsatglbl;ptr1!=NULL;ptr1=ptr1->glblnext){

			/* Print SAT clause. */
			if (NOMEMORY==PrintSingleSATClause(ptr1)){
				return(NOMEMORY);
			}

			/* Print SAT clause status if there is a refutation. */
			if (procctl->status==UNSATISFIABLE){
				if (ptr1->satlits>0){
					printf(" [SATISFIED]\n");
				} else if (ptr1->undeflits>0){
					printf(" [UNDEFINED]\n");
				} else {
					printf(" [==> NOT SATISFIED]\n");
				}

			/* Print SAT clause status if there is a model. */
			} else {
				for (ptr2=&ptr1->formula[0],ii=0;ptr2[0]!=UNITEND;ptr2+=(1+sizeof(asymbol))){
					switch ((MODELPOSITIVE|MODELNEGATIVE|MODELUNDEFINED)&((asymbol *)&ptr2[1])->symbol->flags){
						case MODELPOSITIVE:
							if (POSITIVE&ptr2[0]){
								ii=2;
							}
							break;
						case MODELNEGATIVE:
							if (NEGATIVE&ptr2[0]){
								ii=2;
							}
							break;
						case MODELUNDEFINED:
						default:
							ii=1;
							break;
					}
					if (ii==2){
						break;
					}
				}
				switch (ii){
					case 0:
						printf(" [==> NOT SATISFIED]\n");
						break;
					case 1:
						printf(" [==> UNDEFINED]\n");
						break;
					case 2:
						printf(" [SATISFIED]\n");
						break;
				}
			}
		}
	}
	return(0);
} /* PrintSATClauses */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Print a single SAT clause)  OCJ
 *
 *    This function prints the SAT clause passed as argument.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to satclause structure of the clause
 *            to be printed.
 *
 *  RETURNS:
 *
 *    0 -> Process completed.
 *    NOMEMORY -> Not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t PrintSingleSATClause(satclause *clause){
	auto char *ptr1;                                     /* Auxiliary pointer */
	auto uint8_t *ptr2;                                  /* Auxiliary pointer */

	/* Get SAT clause. */
	if (NULL==(ptr1=DecompileSat(clause))){
		return(NOMEMORY);
	}

	/* Print SAT clause. */
	if ((proofmode==TPTP)&&(syntaxmode==TPTP)){
		printf("fof(f%lu,plain,(\n  %s),\n  inference(",clause->number,ptr1);
		if (clause->inference==SPLIT){
			printf("split_clause,[status(thm)],[f%lu",clause->parent->number);
			for (ptr2=&clause->formula[0];ptr2[0]!=UNITEND;ptr2+=1+sizeof(asymbol)){
				printf(",f%lu",((asymbol *)&ptr2[1])->symbol->defnumber);
			}
			printf("])).");
		} else {
			printf("contradiction_clause,[status(thm)],[f%lu])).",clause->parent->number);
		}
	} else {
		printf("%lu. %s [",clause->number,ptr1);
		if (clause->inference==SPLIT){
			printf("split clause %lu",clause->parent->number);
			for (ptr2=&clause->formula[0];ptr2[0]!=UNITEND;ptr2+=1+sizeof(asymbol)){
				printf(", %lu",((asymbol *)&ptr2[1])->symbol->defnumber);
			}
			printf("]");
		} else {
			printf("contradiction clause %lu]",clause->parent->number);
		}
	}
	MYFREE(ptr1);
	return(0);
} /* PrintSingleSATClause */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Convert formula in linked list format to text)  OCJ
 *
 *    This function converts a formula in linked list format to
 *    text to linked.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to first linked list item in formula.
 *    pdeststr: Address of pointer to destination buffer.
 *    psize: Pointer to total size of destination buffer in bytes
 *           including the space currently in use.
 *    flag: If zero then don't print the existential quantifiers
 *          and the corresponding enclosing brackets, otherwise
 *          print them. This is used to correctly convert formulas
 *          resulting from skolemization when the quantifiers have
 *          not been removed yet.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t LinkedList2Text(lkedfitem *formula,char **pdeststr,int32_t *psize,int32_t flag){
	auto int32_t ii,jj;                                 /* Auxiliary */

	/* Loop through linked list items. */
	**pdeststr=0;
	jj=0;
	ii=OPENBRACKET|CLOSEBRACKET|EQUPRED|NAMEDPRED|UQUANTIFIER|EQUANTIFIER;
	ii|=ANDOPER|OROPER|IMPOPER|REVIMPOPER|EQUIVOPER|XOROPER|NOROPER|NANDOPER;
	for (;formula!=NULL;formula=(formula!=NULL?formula->next:NULL)){

		/* Add negation. */
		if (NEGITEM&formula->flags){
			if (SafeAppendF(pdeststr,psize,"~",&jj)){
				return(NOMEMORY);
			}
		}

		/* Process depending on the kind of item. */
		switch (ii&formula->flags){
			case OPENBRACKET:
				if ((flag!=0)||(0==(EQUANTIFIER&formula->next->flags))){
					if (SafeAppendF(pdeststr,psize,"(",&jj)){
						return(NOMEMORY);
					}
				}
				break;
			case CLOSEBRACKET:
				if ((flag!=0)||(0==(EQUANTIFIER&formula->ptr.mtchbrckt->next->flags))){
					if (SafeAppendF(pdeststr,psize,")",&jj)){
						return(NOMEMORY);
					}
				}
				break;
			case EQUPRED:
				if (Equ2Text(&formula,pdeststr,psize,&jj)){
					return(NOMEMORY);
				}
				if (formula!=NULL){
					formula=formula->prev;
				}
				break;
			case NAMEDPRED:
				if (PredOrFunc2Text(&formula,pdeststr,psize,&jj)){
					return(NOMEMORY);
				}
				if (formula!=NULL){
					formula=formula->prev;
				}
				break;
			case UQUANTIFIER:
				if (Quantifier2Text(&formula,pdeststr,psize,&jj)){
					return(NOMEMORY);
				}
				if (formula!=NULL){
					formula=formula->prev;
				}
				break;
			case EQUANTIFIER:
				if (flag!=0){
					if (Quantifier2Text(&formula,pdeststr,psize,&jj)){
						return(NOMEMORY);
					}
					if (formula!=NULL){
						formula=formula->prev;
					}
				}
				break;
			case ANDOPER:
				if (SafeAppendF(pdeststr,psize,"&",&jj)){
					return(NOMEMORY);
				}
				break;
			case OROPER:
				if (SafeAppendF(pdeststr,psize,"|",&jj)){
					return(NOMEMORY);
				}
				break;
			case IMPOPER:
				if (SafeAppendF(pdeststr,psize,"=>",&jj)){
					return(NOMEMORY);
				}
				break;
			case REVIMPOPER:
				if (SafeAppendF(pdeststr,psize,"<=",&jj)){
					return(NOMEMORY);
				}
				break;
			case EQUIVOPER:
				if (SafeAppendF(pdeststr,psize,"<=>",&jj)){
					return(NOMEMORY);
				}
				break;
			case XOROPER:
				if (SafeAppendF(pdeststr,psize,"<~>",&jj)){
					return(NOMEMORY);
				}
				break;
			case NOROPER:
				if (SafeAppendF(pdeststr,psize,"~|",&jj)){
					return(NOMEMORY);
				}
				break;
			case NANDOPER:
				if (SafeAppendF(pdeststr,psize,"~&",&jj)){
					return(NOMEMORY);
				}
				break;
		}
	}

	return(0);
} /* LinkedList2Text */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Convert (in)equality in linked list formula to text)  OCJ
 *
 *    This function converts an (in)equality in linked list formula to text.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    pformula: On input this is the address of pointer to linked list (in)equality
 *              item. On output this is the address of pointer to linked list item
 *              after the (in)equality block.
 *    pdeststr: Address of pointer to destination buffer.
 *    psize: Pointer to total size of destination buffer in bytes
 *           including the space currently in use.
 *    pstrlen: Address of current length of *pformula.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t Equ2Text(lkedfitem **pformula,char **pdeststr,int32_t *psize,int32_t *pstrlen){

	/* Print first equality root term. */
	(*pformula)=(*pformula)->next;
	if (Term2Text(pformula,pdeststr,psize,pstrlen)){
		return(NOMEMORY);
	}

	/* Print equality connective. */
	if (SafeAppendF(pdeststr,psize,"=",pstrlen)){
		return(NOMEMORY);
	}

	/* Print second equality root term. */
	if (Term2Text(pformula,pdeststr,psize,pstrlen)){
		return(NOMEMORY);
	}
	return(0);
} /* Equ2Text */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Convert predicate or function in linked list formula to text)  OCJ
 *
 *    This function converts a predicate or function in linked list
 *    formula to text.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    pformula: On input this is the address of pointer to linked list predicate
 *              or function item. On output this is the address of pointer to
 *              linked list item after the predicate or function block.
 *    pdeststr: Address of pointer to destination buffer.
 *    psize: Pointer to total size of destination buffer in bytes
 *           including the space currently in use.
 *    pstrlen: Address of current length of *pformula.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t PredOrFunc2Text(lkedfitem **pformula,char **pdeststr,int32_t *psize,int32_t *pstrlen){

	/* Print predicate or function name. */
	if (TRUEPRED&(*pformula)->flags){
		if (SafeAppendF(pdeststr,psize,"$true",pstrlen)){
			return(NOMEMORY);
		}
		(*pformula)=(*pformula)->next;
		return(0);
	} else if (FALSEPRED&(*pformula)->flags){
		if (SafeAppendF(pdeststr,psize,"$false",pstrlen)){
			return(NOMEMORY);
		}
		(*pformula)=(*pformula)->next;
		return(0);
	}
	if (SafeAppendF(pdeststr,psize,
			((char *)((*pformula)->ptr.symbol))+(sizeof(hashchain)),pstrlen)){
		return(NOMEMORY);
	}

	/* Set pformula to next linked list item. */
	(*pformula)=(*pformula)->next;

	/* Predicate or function has arguments. */
	if (((*pformula)!=NULL)&&(OPENBRACKET&(*pformula)->flags)){

		/* Print opening bracket. */
		if (SafeAppendF(pdeststr,psize,"(",pstrlen)){
			return(NOMEMORY);
		}

		/* Set pformula to next linked list item. */
		(*pformula)=(*pformula)->next;

		/* Loop through arguments. */
		while (0==(CLOSEBRACKET&(*pformula)->flags)){

			/* Print comma if necessary. */
			if (0==(OPENBRACKET&(*pformula)->prev->flags)){
				if (SafeAppendF(pdeststr,psize,",",pstrlen)){
					return(NOMEMORY);
				}
			}

			/* Print term. */
			if (Term2Text(pformula,pdeststr,psize,pstrlen)){
				return(NOMEMORY);
			}
		}

		/* Print closing bracket. */
		if (SafeAppendF(pdeststr,psize,")",pstrlen)){
			return(NOMEMORY);
		}

		/* Set pformula to next linked list item. */
		(*pformula)=(*pformula)->next;
	}

	return(0);
} /* PredOrFunc2Text */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Convert term in linked list formula to text)  OCJ
 *
 *    This function converts a term in linked list formula to text.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    pformula: On input this is the address of pointer to linked list term
 *              item. On output this is the address of pointer to linked list
 *              item after the term block.
 *    pdeststr: Address of pointer to destination buffer.
 *    psize: Pointer to total size of destination buffer in bytes
 *           including the space currently in use.
 *    pstrlen: Address of current length of *pformula.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t Term2Text(lkedfitem **pformula,char **pdeststr,int32_t *psize,int32_t *pstrlen){
	auto char varname[10];                              /* Renamed variable name */

	/* Term is a variable. */
	if (NAMEDVAR&(*pformula)->flags){

		/* The variable has been renamed. */
		if (RENAMED&(*pformula)->flags){
			varname[0]='X';
			sprintf(&varname[1],"%d",(*pformula)->data.varnumber);
			if (SafeAppendF(pdeststr,psize,varname,pstrlen)){
				return(NOMEMORY);
			}

		/* The variable has not been renamed. */
		} else {
			if (SafeAppendF(pdeststr,psize,(*pformula)->ptr.vardata->name,pstrlen)){
				return(NOMEMORY);
			}
		}

		/* Set pformula and return. */
		(*pformula)=(*pformula)->next;
		return(0);
	}

	/* If we are here then the term is a function. Print it. */
	if (PredOrFunc2Text(pformula,pdeststr,psize,pstrlen)){
		return(NOMEMORY);
	}
	return(0);
} /* Term2Text */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Convert a quantifier in linked list formula to text)  OCJ
 *
 *    This function converts a quantifier and all immediately following
 *    quantifiers of the same kind in linked list formula to text.
 *
 *    The TPTP syntax to include several variables in a single quantifier
 *    is followed. For instance: ![X,Y]: ... instead of ![X]: ![Y]: ...-
 *
 *
 *
 *  ARGUMENTS:
 *
 *    pformula: On input this is the address of pointer to linked list quantifier
 *              item. On output this is the address of pointer to linked list
 *              item after the quantifier block.
 *    pdeststr: Address of pointer to destination buffer.
 *    psize: Pointer to total size of destination buffer in bytes
 *           including the space currently in use.
 *    pstrlen: Address of current length of *pformula.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t Quantifier2Text(lkedfitem **pformula,char **pdeststr,int32_t *psize,int32_t *pstrlen){
	auto int32_t ii,jj;                                 /* Auxiliary */

	/* Print quantifier type. */
	ii=(UQUANTIFIER|EQUANTIFIER)&(*pformula)->flags;
	if (ii==UQUANTIFIER){
		if (SafeAppendF(pdeststr,psize,"![",pstrlen)){
			return(NOMEMORY);
		}
	} else {
		if (SafeAppendF(pdeststr,psize,"?[",pstrlen)){
			return(NOMEMORY);
		}
	}

	/* Loop through quantifiers. */
	jj=0;
	while (ii==((UQUANTIFIER|EQUANTIFIER)&(*pformula)->flags)){

		/* Print comma separator of variable if needed. */
		if (jj){
			if (SafeAppendF(pdeststr,psize,",",pstrlen)){
				return(NOMEMORY);
			}
		} else {
			jj=1;
		}

		/* Print name of the variable. */
		if (SafeAppendF(pdeststr,psize,(*pformula)->ptr.vardata->name,pstrlen)){
			return(NOMEMORY);
		}

		/* Set pformula to next linked list item. */
		(*pformula)=(*pformula)->next;
	}

	/* Print end of quantifier variable list. */
	if (SafeAppendF(pdeststr,psize,"]: ",pstrlen)){
		return(NOMEMORY);
	}

	return(0);
} /* Quantifier2Text */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Print main KB formulas in TPTP CNF format)  OCJ
 *
 *    This function prints main KB unprocessed binary formulas in
 *    TPTP CNF format. It is assumed that the only unprocessed
 *    binary formulas in main KB are the result of pre-processing
 *    and are therefore in CNF format.
 *
 *
 *  ARGUMENTS:
 *
 *    role: Role of formula that generated a $false during pre-process,
 *          not used otherwise.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t PrintClausifiedFormulas(int32_t role){
	auto cmprefix *ptr1,*ptr3,*ptr4;                     /* Auxiliary pointers */
	auto char *ptr2;                                     /* Auxiliary pointer */
	auto uint64_t ii;                                    /* Auxiliary */

	/* Problem solved in pre-processing. */
	if (procctl->status==UNSATISFIABLE){
		printf("cnf(ax1,");
		switch (role){
			case AXIOM:
				printf("axiom,");
				break;
			case HYPOTHESIS:
				printf("hypothesis,");
				break;
			case DEFINITION:
				printf("definition,");
				break;
			case ASSUMPTION:
				printf("assumption,");
				break;
			case LEMMA:
				printf("lemma,");
				break;
			case THEOREM:
				printf("theorem,");
				break;
			case COROLLARY:
				printf("corollary,");
				break;
			default:
				printf("plain,");
				break;
		}
		printf("$false).\n");

	/* Problem not solved in pre-processing. */
	} else {

		/* Print main KB unprocessed clauses. */
		printf("\n");
		if (mainkb.frstunproc!=NULL){
			for (ptr1=mainkb.frstunproc,ii=1;ptr1!=NULL;ptr1=ptr4,ii++){

				/* Prepare next iteration. */
				ptr4=ptr1->next;

				/* Get oldest ancestor of formula. */
				for (ptr3=ptr1->parent1;ptr3->parent1!=NULL;ptr3=ptr3->parent1){
				}

				/* Unlink and simplify formula. Unlink() function cannot */
				/* return NOMEMORY because all the clauses are unprocessed. */
				/* FirstSimplify() cannot return TIMEOUT because kbset.opts.timeout */
				/* is set to a very high value. */
				Unlink(ptr1,&mainkb);
				switch (FirstSimplify(&ptr1,0)){

					/* Not enough memory. */
					case NOMEMORY:
						MYFREE(ptr1->part2.bin);
						MYFREE(ptr1);
						return(NOMEMORY);
						break;

					/* Printable clause. */
					case 0:
					case 1:

						/* Print formula. */
						if (NULL==(ptr2=Decompile2(&ptr1->part2.bin->formula[0]))){
							return(NOMEMORY);
						}
						if(ptr1->parent1->flags&FROMCONJECTURE){
							printf("cnf(co%lu,negated_conjecture,%s).\n",ii,ptr2);
						} else {
							printf("cnf(ax%lu,",ii);
							switch (ptr3->inference){
								case AXIOM:
									printf("axiom,");
									break;
								case HYPOTHESIS:
									printf("hypothesis,");
									break;
								case DEFINITION:
									printf("definition,");
									break;
								case ASSUMPTION:
									printf("assumption,");
									break;
								case LEMMA:
									printf("lemma,");
									break;
								case THEOREM:
									printf("theorem,");
									break;
								case COROLLARY:
									printf("corollary,");
									break;
								default:
									printf("plain,");
									break;
							}
							printf("%s).\n",ptr2);
						}
						MYFREE(ptr2);

						/* Free formula. */
						MYFREE(ptr1->part2.bin);
						MYFREE(ptr1);
						break;
				}
			}

		/* No clauses in the KB. */
		} else {
			printf("No CNF formulas generated.\n");
		}
	}

	return(0);
} /* PrintClausifiedFormulas */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Parse and build a filename)  OCJ
 *
 *    This function parses a string and tries to get and build a
 *    candidate to a filename from the first first characters in
 *    the given string. If the first non space character in the
 *    string is not a single quote then the file is interpreted
 *    to be the first token with no spaces or tabs. If the first
 *    non space and non tab character is a single quote then the
 *    file is interpreted to be the content between the single
 *    quotes. If the initial single quote has not a closing single
 *    quote then the filename is invalid.
 *
 *    If a valid filename candidate is found then the function will
 *    return a pointer to the start of the name without quotes.
 *    A \0 character will be placed at the end of the name without
 *    quotes. Otherwise the function will return a NULL pointer.
 *
 *
 *  ARGUMENTS:
 *
 *    string: String to be parsed. It will be modified if a valid filename
 *            candidate is found.
 *    pnext: If not NULL and a valid filename candidate is found then
 *           *pnext will be set to the character immediately following
 *           the \0 character after the candidate filename built.
 *
 *  RETURNS:
 *
 *    NOMEMORY if not enough memory or 0 otherwise.
 *
 *--------------------------------------------------------------*/
char *ParseFilename(char *string,char **pnext){
	auto char *ptr1;                                     /* Auxiliary pointers */

	/* Check that string is not empty. */
	ptr1=SkipBlanks(string);
	if (ptr1[0]==0){
		return(NULL);
	}

	/* The string starts with a single quote. */
	if (ptr1[0]=='\''){
		ptr1++;
		if (NULL==(*pnext=strchr(ptr1,'\''))){
			return(NULL);
		}
		**pnext=0;

	/* The string doesn't start with a single quote. */
	} else {
		if (NULL==(*pnext=strchr(ptr1,' '))){
			if (NULL==(*pnext=strchr(ptr1,'\t'))){
				*pnext=&ptr1[strlen(ptr1)-1];
			} else {
				**pnext=0;
			}
		} else {
			**pnext=0;
		}
	}
	(*pnext)++;
	return(ptr1);
} /* ParseFilename */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Print user strategy information)  OCJ
 *
 *    This function prints user strategy information set either
 *    by the PRECEDENCE command/option or by a subset of the
 *    following command options: ALGORITHM, CONNECT, DEMODULATION,
 *    FACTORING, FUNCTIONW, GAMMA, GRJOIN, LAYER, LEARNFROM,
 *    LITORDER, LOOKAHEAD, MAXWEIGHT, PRECEDENCE, PRPRCSYMPREC,
 *    SELECT, SELECTRATIO, SPLIT, TERMORDER, WEAKRW and LEARN.
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
void PrintStrategy(void){
	auto uint64_t ii;                                    /* Auxiliary */

	/* Strategy defined with STRATEGY command/option. */
	if (strategy!=0xffffffffffffffff){
		printf("Strategy %ld defined by STRATEGY command/option:\n",strategy);
		printf("ALGORITHM has been set to ");
		switch (ALGMASK&strategy){
			case OTTER_P:
				printf("OTTER\n");
				break;
			case DISCOUNT_P:
				printf("DISCOUNT\n");
				break;
			case UEQDSC_P:
				printf("UEQDSC\n");
				break;
			case UEQOTT_P:
				printf("UEQOTT\n");
				break;
		}
		printf("FUNCTIONW has been set to ");
		if (UNIFORM_P==(FUNCWMASK&strategy)){
			printf("UNIFORM\n");
		} else {
			printf("ARITY\n");
		}
		printf("GAMMA has been set to ");
		switch (GAMMAMASK&strategy){
			case GAMMA0_P:
				printf("%f\n",0.0);
				break;
			case GAMMA1_P:
				printf("%f\n",0.1);
				break;
			case GAMMA2_P:
				printf("%f\n",0.2);
				break;
			case GAMMA3_P:
				printf("%f\n",0.3);
				break;
			case GAMMA4_P:
				printf("%f\n",0.4);
				break;
			case GAMMA5_P:
				printf("%f\n",0.5);
				break;
			case GAMMA6_P:
				printf("%f\n",0.6);
				break;
		}
		printf("LITORDER has been set to ");
		switch (LITORDRMASK&strategy){
			case LITORDSTD_P:
				printf("STANDARD\n");
				break;
			case LITORDNR_P:
				printf("NONRECURSIVE\n");
				break;
			default:
				printf("LEXICOGRAPHIC\n");
				break;
		}
		printf("TERMORDER has been set to ");
		if (TRMORDSTD_P==(TERMORDRMASK&strategy)){
			printf("STANDARD\n");
		} else {
			printf("NONRECURSIVE\n");
		}
		printf("PRECEDENCE has been set to ");
		switch (PRCMASK&strategy){
			case PRCARITY_P:
				printf("ARITY\n");
				break;
			case PRCOCCUR_P:
				printf("OCCURRENCE\n");
				break;
			case PRCFREQ_P:
				printf("FREQUENCY\n");
				break;
			case PRCINVAR_P:
				printf("INVERSE ARITY\n");
				break;
			case PRCINVOC_P:
				printf("INVERSE OCCURRENCE\n");
				break;
			case PRCINVFR_P:
				printf("INVERSE FREQUENCY\n");
				break;
		}
		printf("PRPRCSYMPREC has been set to ");
		if (ISPRCLOW_P==(ISPRCMASK&strategy)){
			printf("LOW\n");
		} else {
			printf("HIGH\n");
		}
		printf("MAXWEIGHT has been set to ");
		switch (MAXWMASK&strategy){
			case LRSOTT_P:
				printf("LRSOTTER\n");
				break;
			case LRSOFF_P:
				printf("LRSOFF\n");
				break;
			case LRSALL_P:
				printf("LRS\n");
				break;
			default:
				printf("LRSDISCOUNT\n");
				break;
		}
		printf("SPLIT has been set to ");
		if (SPLITON_P==(SPLITMASK&strategy)){
			printf("ON\n");
		} else {
			printf("OFF\n");
		}
		printf("CONNECT has been set to ");
		if (CONNECTON_P==(CONNECTMASK&strategy)){
			printf("ON\n");
		} else {
			printf("OFF\n");
		}
		printf("GRJOIN has been set to ");
		switch (GRJOINMASK&strategy){
			case GRJOINON_P:
				printf("ON\n");
				break;
			case GRJOINMED_P:
				printf("MED\n");
				break;
			default:
			case GRJOINOFF_P:
				printf("OFF\n");
				break;
		}
		printf("FACTORING has been set to ");
		if (FACTON_P==(FACTORINGMASK&strategy)){
			printf("ON\n");
		} else {
			printf("OFF\n");
		}
		printf("LOOKAHEAD has been set to ");
		if (LKAHEADON_P==(LOOKAHEADMASK&strategy)){
			printf("ON\n");
		} else {
			printf("OFF\n");
		}
		printf("SELECT has been set to ");
		switch (SELECTMASK&strategy){
			case SINGLENEG_P:
				printf("SINGLENEG\n");
				break;
			case MULTINEG_P:
				printf("MULTINEG\n");
				break;
			case SINGLEPOS_P:
				printf("SINGLEPOS\n");
				break;
			case MULTIPOS_P:
				printf("MULTIPOS\n");
				break;
			case MAXIMAL_P:
				printf("MAXIMAL\n");
				break;
			case SELECTALL_P:
				printf("ALL\n");
				break;
			case MXSINGLENEG_P:
				printf("MXSINGLENEG\n");
				break;
			case TYPE1_P:
				printf("TYPE1\n");
				break;
			case TYPE2_P:
				printf("TYPE2\n");
				break;
			case TYPE3_P:
				printf("TYPE3\n");
				break;
			case TYPE4_P:
				printf("TYPE4\n");
				break;
			case TYPE5_P:
				printf("TYPE5\n");
				break;
			case TYPE6_P:
				printf("TYPE6\n");
				break;
			case TYPE1CPL_P:
				printf("TYPE1CPL\n");
				break;
			case TYPE2CPL_P:
				printf("TYPE2CPL\n");
				break;
			case TYPE3CPL_P:
				printf("TYPE3CPL\n");
				break;
			case TYPE4CPL_P:
				printf("TYPE4CPL\n");
				break;
			case TYPE5CPL_P:
				printf("TYPE5CPL\n");
				break;
			case TYPE6CPL_P:
			default:
				printf("TYPE6CPL\n");
				break;
		}
		printf("WEAKRW has been set to ");
		if (WEAKRWON_P==(WEAKRWMASK&strategy)){
			printf("ON\n");
		} else {
			printf("OFF\n");
		}
		printf("DEMODULATION has been set to ");
		if (DEMODON_P==(DEMODMASK&strategy)){
			printf("ON\n");
		} else if (DEMODOFF_P==(DEMODMASK&strategy)){
			printf("OFF\n");
		} else if (DEMODCPL1_P==(DEMODMASK&strategy)){
			printf("CPL1\n");
		} else if (DEMODCPL2_P==(DEMODMASK&strategy)){
			printf("CPL2\n");
		} else if (DEMODONORNT_P==(DEMODMASK&strategy)){
			printf("ONORNT\n");
		} else if (DEMODCPL1ORNT_P==(DEMODMASK&strategy)){
			printf("CPL1ORNT\n");
		} else {
			printf("CPL2ORNT\n");
		}
		printf("LAYER has been set to ");
		if (LAYER1_P==(LAYERMASK&strategy)){
			printf("1\n");
		} else {
			printf("2\n");
		}
		printf("Weight:Age SELECTRATIO has been set to ");
		switch (WARMASK&strategy){
			case WAR12_P:
				printf("1:2\n");
				break;
			case WAR11_P:
				printf("1:1\n");
				break;
			case WAR21_P:
				printf("2:1\n");
				break;
			case WAR31_P:
				printf("3:1\n");
				break;
			case WAR41_P:
				printf("4:1\n");
				break;
			case WAR51_P:
				printf("5:1\n");
				break;
			case WAR61_P:
				printf("6:1\n");
				break;
		}
		printf("LEARN has been ");
		if ((LEARN_P&LEARNMASK)==LEARN_P){
			printf("enabled\n");
		} else {
			printf("disabled\n");
		}
		printf("LEARNFROM has been set to ");
		if ((learnfrom!=NULL)&&(LEARN_P==(LEARNMASK&strategy))){
			printf("%s\n",learnfrom);
		} else {
			printf("OFF\n");
		}

	/* Strategy defined by other means (see comments above). */
	} else if (usrparam_mask!=0){
		ii=0;
		printf("ALGORITHM has been set to ");
		if (usrparam_mask&ALGMASK){
			switch (glblopts.algorithm){
				case OTTER:
					printf("OTTER\n");
					ii|=OTTER_P;
					break;
				case DISCOUNT:
					printf("DISCOUNT\n");
					ii|=DISCOUNT_P;
					break;
				case UEQDSC:
					printf("UEQDSC\n");
					ii|=UEQOTT_P;
					break;
				case UEQOTT:
					printf("UEQOTT\n");
					ii|=UEQDSC_P;
					break;
			}
		}
		if (usrparam_mask&FUNCWMASK){
			printf("FUNCTIONW has been set to ");
			if (glblopts.fweight==UNIFORM){
				printf("UNIFORM\n");
				ii|=UNIFORM_P;
			} else {
				printf("ARITY\n");
				ii|=ARITY_P;
			}
		}
		if (usrparam_mask&GAMMAMASK){
			printf("GAMMA has been set to %f\n",glblopts.gamma);
		}
		if (usrparam_mask&LITORDRMASK){
			printf("LITORDER has been set to ");
			if (glblopts.litord==STANDARD){
				printf("STANDARD\n");
				ii|=LITORDSTD_P;
			} else {
				printf("NONRECURSIVE\n");
				ii|=LITORDNR_P;
			}
		}
		if (usrparam_mask&TERMORDRMASK){
			printf("TERMORDER has been set to ");
			if (glblopts.termord==STANDARD){
				printf("STANDARD\n");
				ii|=TRMORDSTD_P;
			} else {
				printf("NONRECURSIVE\n");
				ii|=TRMORDNR_P;
			}
		}
		if (usrparam_mask&PRCMASK){
			printf("PRECEDENCE has been set to ");
			switch (glblopts.precedence){
				case PRCARITY:
					printf("ARITY\n");
					ii|=PRCARITY_P;
					break;
				case PRCFREQ:
					printf("FREQUENCY\n");
					ii|=PRCFREQ_P;
					break;
				case PRCOCCUR:
					printf("OCCURRENCE\n");
					ii|=PRCOCCUR_P;
					break;
				case PRCINVAR:
					printf("INVERSE ARITY\n");
					ii|=PRCINVAR_P;
					break;
				case PRCINVFR:
					printf("INVERSE FREQUENCY\n");
					ii|=PRCINVFR_P;
					break;
				case PRCINVOC:
					printf("INVERSE OCCURRENCE\n");
					ii|=PRCINVOC_P;
					break;
			}
		}
		if (usrparam_mask&ISPRCMASK){
			printf("PRPRCSYMPREC has been set to ");
			if (glblopts.prprcsymprec==ISPRCLOW){
				printf("LOW\n");
			} else {
				printf("HIGH\n");
			}
		}
		if (usrparam_mask&MAXWMASK){
			printf("MAXWEIGHT has been set to ");
			switch (glblopts.mxwghtopt&(MXWEIGHTLRS|MXWEIGHTLRSOTT|MXWEIGHTLRSDSC|MXWEIGHTLRSOFF)){
				case MXWEIGHTLRS:
					printf("LRS.\n");
					ii|=LRSALL_P;
					break;
				case MXWEIGHTLRSOFF:
					printf("LRSOFF.\n");
					ii|=LRSOFF_P;
					break;
				case MXWEIGHTLRSOTT:
					printf("LRSOTTER.\n");
					ii|=LRSOTT_P;
					break;
				default:
					printf("LRSDISCOUNT.\n");
					ii|=LRSDSC_P;
					break;
			}
		}
		if (usrparam_mask&SPLITMASK){
			printf("SPLIT has been set to ");
			if (glblopts.split==0){
				printf("OFF\n");
				ii|=SPLITOFF_P;
			} else {
				printf("ON\n");
				ii|=SPLITON_P;
			}
		}
		if (usrparam_mask&CONNECTMASK){
			printf("CONNECT has been set to ");
			if (glblopts.connect==1){
				printf("ON\n");
				ii|=CONNECTON_P;
			} else {
				printf("OFF\n");
				ii|=CONNECTOFF_P;
			}
		}
		if (usrparam_mask&GRJOINMASK){
			printf("GRJOIN has been set to ");
			if (glblopts.grjoin==1){
				printf("ON\n");
				ii|=GRJOINON_P;
			} else if (glblopts.grjoin==2){
				printf("MED\n");
				ii|=GRJOINMED_P;
			} else {
				printf("OFF\n");
				ii|=GRJOINOFF_P;
			}
		}
		if (usrparam_mask&FACTORINGMASK){
			printf("FACTORING has been set to ");
			if (glblopts.factoring==1){
				printf("ON\n");
			} else {
				printf("OFF\n");
			}
		}
		if (usrparam_mask&LOOKAHEADMASK){
			printf("LOOKAHEAD has been set to ");
			if (glblopts.lookahead==1){
				printf("ON\n");
				ii|=FACTON_P;
			} else {
				printf("OFF\n");
				ii|=FACTOFF_P;
			}
		}
		if (usrparam_mask&SELECTMASK){
			printf("SELECT has been set to ");
			switch (glblopts.select){
				case MAXIMAL:
					printf("MAXIMAL\n");
					ii|=MAXIMAL_P;
					break;
				case SINGLENEG:
					printf("SINGLENEG\n");
					ii|=SINGLENEG_P;
					break;
				case MULTINEG:
					printf("MULTINEG\n");
					ii|=MULTINEG_P;
					break;
				case SINGLEPOS:
					printf("SINGLEPOS\n");
					ii|=SINGLEPOS_P;
					break;
				case MULTIPOS:
					printf("MULTIPOS\n");
					ii|=MULTIPOS_P;
					break;
				case MXSINGLENEG:
					printf("MXSINGLENEG\n");
					ii|=MXSINGLENEG_P;
					break;
				case SELECTALL:
				default:
					printf("ALL\n");
					ii|=SELECTALL_P;
					break;
				case TYPE1:
					printf("TYPE1\n");
					ii|=TYPE1_P;
					break;
				case TYPE2:
					printf("TYPE2\n");
					ii|=TYPE2_P;
					break;
				case TYPE3:
					printf("TYPE3\n");
					ii|=TYPE3_P;
					break;
				case TYPE4:
					printf("TYPE4\n");
					ii|=TYPE4_P;
					break;
				case TYPE5:
					printf("TYPE5\n");
					ii|=TYPE5_P;
					break;
				case TYPE6:
					printf("TYPE6\n");
					ii|=TYPE6_P;
					break;
				case TYPE1CPL:
					printf("TYPE1CPL\n");
					ii|=TYPE1CPL_P;
					break;
				case TYPE2CPL:
					printf("TYPE2CPL\n");
					ii|=TYPE2CPL_P;
					break;
				case TYPE3CPL:
					printf("TYPE3CPL\n");
					ii|=TYPE3CPL_P;
					break;
				case TYPE4CPL:
					printf("TYPE4CPL\n");
					ii|=TYPE4CPL_P;
					break;
				case TYPE5CPL:
					printf("TYPE5CPL\n");
					ii|=TYPE5CPL_P;
					break;
				case TYPE6CPL:
					printf("TYPE6CPL\n");
					ii|=TYPE6CPL_P;
					break;
			}
		}
		if (usrparam_mask&WEAKRWMASK){
			printf("WEAKRW has been set to ");
			if (glblopts.weakrw==1){
				printf("ON\n");
				ii|=WEAKRWON_P;
			} else {
				ii|=WEAKRWOFF_P;
			}
		}
		if (usrparam_mask&DEMODMASK){
			printf("DEMODULATION has been set to ");
			if (glblopts.demodulation==DEMODON){
				printf("ON\n");
				ii|=DEMODON_P;
			} else if (glblopts.demodulation==DEMODCPL1){
				printf("CPL1\n");
				ii|=DEMODCPL1_P;
			} else if (glblopts.demodulation==DEMODCPL2){
				printf("CPL2\n");
				ii|=DEMODCPL2_P;
			} else if (glblopts.demodulation==DEMODONORNT){
				printf("ONORNT\n");
				ii|=DEMODONORNT_P;
			} else if (glblopts.demodulation==DEMODCPL1ORNT){
				printf("CPL1ORNT\n");
				ii|=DEMODCPL1ORNT_P;
			} else if (glblopts.demodulation==DEMODCPL2ORNT){
				printf("CPL2ORNT\n");
				ii|=DEMODCPL2ORNT_P;
			} else {
				printf("OFF\n");
				ii|=DEMODOFF_P;
			}
		}
		if (usrparam_mask&LAYERMASK){
			printf("LAYER has been set to ");
			if (glblopts.layer==0){
				printf("1\n");
				ii|=LAYER1_P;
			} else {
				printf("2\n");
				ii|=LAYER2_P;
			}
		}
		if (usrparam_mask&LEARNMASK){
			printf("LEARN has been set to ");
			if (glblopts.learn){
				printf("ON\n");
				ii|=LEARN_P;
			} else {
				printf("OFF\n");
				ii|=NOLEARN_P;
			}
		}
		printf("LEARNFROM has been set to ");
		if (learnfrom!=NULL){
			printf("%s\n",learnfrom);
			ii|=LEARN_P;
		} else {
			printf("OFF\n");
		}
		printf("Strategy %ld defined by multiple commands/options\n",ii);

	/* There is no user defined strategy. */
	} else {
		printf("There is no user defined strategy.\n");
	}
	return;
} /* PrintStrategy */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Display general help or a help topic)  OCJ
 *
 *    This function displays a general help screen or a help topic
 *    referring to a specific command option.
 *
 *
 *  ARGUMENTS:
 *
 *    token: Name of command option or NULL pointer for
 *           general help.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *--------------------------------------------------------------*/
void Help(char *token){
	auto int ii,jj,kk;                                   /* Auxiliary */

	/* Topic specific help. */
	if ((token!=NULL)&&(NULL!=(token=strtok(NULL," \t")))){
		for (ii=0;ii<strlen(token);ii++){
			token[ii]=toupper(token[ii]);
		}
		if (0==strcmp(token,"ALL")){
			Help(NULL);
			printf("\n\nInteractive commands and program options in alphabetical order\n");
			printf("==============================================================\n");
			jj=1;
		} else {
			jj=0;
			kk=1;
		}
		printf("\n");
		if ((jj)||(0==strcmp(token,"ALGORITHM"))){
			printf("ALGORITHM command format is:\n");
			printf("    ALGORITHM [OTTER|DISCOUNT|UEQOTT|UEQDSC|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -algorithm(winner|discount|ueqott|ueqdsc|default)\n");
			printf("Default value: DEFAULT\n\n");
			printf("In interactive mode if no parameter is included then this command displays the default algorithm\n");
			printf("type used by the program in the proof process. If algorithm type is specified then the default\n");
			printf("algorithm is changed accordingly. If CORES option is set to more than one core then other algorithms\n");
			printf("can be used simultaneously in order to increase the chance of finding a proof.\n");
			printf("Valid algorithm types are OTTER, DISCOUNT, UEQOTT, UEQDSC and DEFAULT. If DEFAULT is selected\n");
			printf("the algorithm type will be set to the best choice for the problem type being solved.\n");
			printf("Algorithms UEQDSC and UEQOTT are unfailing completion Discount and Otter algorithms to be used\n");
			printf("for problems with all clauses being unit equalities except a negated conjecture that is an inequality.\n");
			printf("If this algorithm is specified but the problem is not of this type then the normal OTTER or DISCOUNT\n");
			printf("algorithm will be used.\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("DEFAULT parameter can be specified in non-interactive option but it is redundant.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"ASK"))){
			printf("ASK command format is:\n");
			printf("    ASK conjecture_sentence\n");
			printf("There is not an equivalent program option for this command.\n\n");
			printf("Inputs a new conjecture. Conjecture sentences must end with a period \".\". If the\n");
			printf("sentence is too long the sentence text can be introduced in several lines keying\n");
			printf("ENTER after each line and including a period in the last line to indicate the end\n");
			printf("of sentence.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"CLAUSIFY"))){
			printf("CLAUSIFY command format is:\n");
			printf("    CLAUSIFY [filename]\n");
			printf("Program option format is:\n");
			printf("    -clausify\n\n");
			printf("Converts problem formulas to CNF and prints them in TPTP format. In interactive mode if filename\n");
			printf("is not specified then problem formulas must be introduced by the user before invoking CLAUSIFY.\n");
			printf("If filename is specified then formulas in the file in CNF or FOF format will be converted to CNF.\n");
			printf("In this case program data must be previously initialized with the NEW command. If filename is\n");
			printf("specified then any additional parameters will be ignored.\n");
			printf("In non-interactive mode a filename must be specified as the final parameter in program invocation.\n");
			printf("All problem data will be initialized after command execution.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"CONNECT"))){
			printf("CONNECT command format is:\n");
			printf("    CONNECT [ON|OFF|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -connect(on|off|default)\n");
			printf("Default value: DEFAULT\n\n");
			printf("This option affects only when saturation algorithm in use is UEQOTT or UEQDSC (see ALGORITHM command option).\n");
			printf("In interactive mode if no parameter is included then this command displays connect settings.\n");
			printf("If OFF parameter is specified then connectedness setting is disabled. If ON is specified then\n");
			printf("connectedness is enabled. If DEFAULT is specified then connectedness will be set to the best\n");
			printf("choice for the problem type being solved.\n");
			printf("DEFAULT parameter can be specified in non-interactive but it is redundant.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"CORES"))){
			printf("CORES command format is:\n");
			printf("    CORES [number_of_processes]\n");
			printf("Program option format is:\n");
			printf("    -cores(number_of_processes)\n");
			printf("Default value: all available cores\n\n");
			printf("In interactive mode if no parameter is included then this command displays the number of processes\n");
			printf("that will be started simultaneously to find a proof. Number of processes must be a positive integer.\n");
			printf("If zero is specified then all available cores will be used. If more cores than available are specified\n");
			printf("then then all available cores will be used. Any additional parameters will be ignored.\n");
			printf("IMPORTANT: if a strategy is specified with the STRATEGY command or using some of the commands ALGORITHM\n");
			printf("CONNECT, DEMODULATION, FACTORING, FUNCTIONW, GAMMA, GRJOIN, LAYER, LITORDER, LOOKAHEAD, MAXWEIGHT, PRECEDENCE,\n");
			printf("PRPRCSYMPREC, SELECT, SELECTRATIO, SPLIT, TERMORDER, WEAKRW or LEARN then only one core will be used in\n");
			printf("a normal problem solving run regardless the setting specified with the CORES option.\n");
			printf("WARNING: this program assumes that it has not CPU affinity restrictions. It will assume that all\n");
			printf("cores available in the system can be used and will try to use them all by default. If this is not\n");
			printf("the case then program performance may not be optimal.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"DEMODULATION"))){
			printf("DEMODULATION command format is:\n");
			printf("    DEMODULATION [ON|OFF|CPL1|CPL2|ONORNT|CPL1ORNT|CPL2ORNT|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -demodulation(on|off|cpl1|cpl2|onornt|cpl1ornt|cpl2ornt|default)\n");
			printf("Default value: DEFAULT\n\n");
			printf("In interactive mode if no parameter is included then this command displays demodulation settings.\n");
			printf("If OFF parameter is specified then demodulation simplification is disabled. If ON then demodulation\n");
			printf("simplifications are enabled without restrictions. If CPL1 parameter is specified then demodulation\n");
			printf("is restricted to the non completeness compromising \"encompassment demodulation\" and destructive equality\n");
			printf("resolution is also restricted to cases that don't compromise completeness.\n");
			printf("CPL2 is the same as CPL1 but a more restrictive algorithm is used to limit demodulation cases\n");
			printf("so it is more restrictive and therefore less efficient in principle. In any case destructive equality resolution\n");
			printf("is always enabled, with or without restrictions. If DEFAULT is specified then demodulation will be set to the best\n");
			printf("choice for the problem type being solved. DEFAULT parameter can be specified in non-interactive\n");
			printf("mode but it is redundant.\n");
			printf("ONORNT, CPL1ORNT and CPL2ORNT are the same as ON, CPL1 and CPL2 but in addition the demodulations\n");
			printf("are restricted to be done only with oriented equalities\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"EXIT"))){
			printf("EXIT command format is:\n");
			printf("    EXIT\n");
			printf("There is not an equivalent program option for this command.\n\n");
			printf("This command exits the program. The program asks for a confirmation before exiting\n");
			printf("This command has no parameters\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"FACTORING"))){
			printf("FACTORING command format is:\n");
			printf("    FACTORING [ON|OFF|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -factoring(on|off|default)\n");
			printf("Default value: DEFAULT\n\n");
			printf("In interactive mode if no parameter is included then this command displays if factoring inferences\n");
			printf("are enabled or disabled. If ON parameter is specified then factoring inferences are enabled.\n");
			printf("If OFF parameter is specified then factoring inferences are disabled.\n");
			printf("If DEFAULT is specified then factoring inferences will be set to the best choice for the problem.\n");
			printf("type being solved. DEFAULT parameter can be specified in non-interactive option but it is redundant.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"FUNCTIONW"))){
			printf("FUNCTIONW command format is:\n");
			printf("    FUNCTIONW [UNIFORM|ARITY|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -functionw(uniform|arity|default)\n");
			printf("Default value: DEFAULT\n\n");
			printf("In interactive mode if no parameter is included then this command displays the default\n");
			printf("weight type used. If weight type is specified then the default weight comparison is changed\n");
			printf("accordingly. If CORES option is set to more than one process then other weight types can be used\n");
			printf("simultaneously in order to increase the chance of finding a proof.\n");
			printf("Valid weight types are UNIFORM, ARITY or DEFAULT. ARITY depends on the number of\n");
			printf("arguments of a function or predicate. If DEFAULT is selected the weight type will be set\n");
			printf("to the best choice for the problem type being solved.\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("DEFAULT parameter can be specified in non-interactive option but it is redundant.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"GAMMA"))){
			printf("GAMMA command format is:\n");
			printf("    GAMMA gamma_value|DEFAULT\n");
			printf("Program option format is:\n");
			printf("    -gamma(gamma_value|default)\n");
			printf("Sets the factor to be applied to pure weight for learning weight calculation.This is a positive\n");
			printf("or zero floating point number. The higher it is the more pure weight will be included in\n");
			printf("learning weight calculation. If DEFAULT is selected the weight type will be set to the best\n");
			printf("choice for the problem type being solved.\n\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"GO"))){
			printf("GO command format is:\n");
			printf("    GO\n");
			printf("There is not an equivalent program option for this command.\n\n");
			printf("Initiates an inference process to try to prove the current theorem.\n\n");
			printf("All problem data will be initialized after command execution. This command has no parameters\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"GOALXFORM"))){
			printf("GOALXFORM command format is:\n");
			printf("    GOALXFORM [ON|OFF|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -goalxform(on|off)\n");
			printf("Default value: DEFAULT\n\n");
			printf("In interactive mode if no parameter is included then this command displays if goal transformation is enabled\n");
			printf("or disabled for unit equality problems. If ON parameter is specified then goal transformation is enabled.\n");
			printf("If OFF parameter is specified then goal transformation is disabled.\n");
			printf("If DEFAULT parameter is specified then goal transformation is set to the best choice for the problem type being solved.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"GOSAT"))){
			printf("GOSAT command format is:\n");
			printf("    GOSAT [filename]\n");
			printf("Program option format is:\n");
			printf("    -gosat\n\n");
			printf("Calls the SAT solver to try to prove a problem without using the first order\n");
			printf("prover. If the problem is ground and has no (in)equalities this will givea definite answer.\n");
			printf("Otherwise it will usually be unable to prove valid theorems.\n");
			printf("In interactive mode if filename is not specified then the current problem introduced with\n");
			printf("TELL and ASK commands is analyzed. If filename is specified then the file will be loaded and\n");
			printf("analyzed. In this case the program status must be previously initialized with the NEW command\n");
			printf("if some previous formulae was introduced by hand. If filename is specified then any additional\n");
			printf("parameters will be ignored.\n");
			printf("In non-interactive mode a filename must be specified as the final parameter in program invocation.\n");
			printf("All problem data will be initialized after command execution.\n");
			printf("===========================\n");

			kk=0;
		}
		if ((jj)||(0==strcmp(token,"GRJOIN"))){
			printf("GRJOIN command format is:\n");
			printf("    GRJOIN [ON|MED|OFF|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -grjoin(on|med|off|default)\n");
			printf("Default value: DEFAULT\n\n");
			printf("This option affects only when saturation algorithm in use is UEQOTT or UEQDSC (see ALGORITHM command option).\n");
			printf("In interactive mode if no parameter is included then this command displays grjoin settings.\n");
			printf("If OFF parameter is specified then ground joinability is disabled. If ON is specified then ground\n");
			printf("joinability is enabled. If MED is specified then a light version of ground joinability is used.\n");
			printf("In this case some ground joinable equations are not detected in exchange of speed improvement.\n");
			printf("If DEFAULT is specified then grjoin will be set to the best choice for the problem type\n");
			printf("being solved.\n");
			printf("DEFAULT parameter can be specified in non-interactive but it is redundant.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"HELP"))){
			printf("HELP command format is:\n");
			printf("    HELP [command|ALL]\n\n");
			printf("Program option format is:\n");
			printf("    -help[(option|all)]\n\n");
			printf("If no parameter is included then this command displays the list of command/program options\n");
			printf("that are available. If a command or option is specified then help for that specific command\n");
			printf("or option will be displayed. If ALL is specified then all available help will be displayed.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"HWINSTR"))){
			printf("HWINSTR command format is:\n");
			printf("    HWINSTR [number_of_HW_instructions]\n");
			printf("Program option format is:\n");
			printf("    -hwinstr(number_of_HW_instructions)\n");
			printf("In interactive mode if no parameter is included then this command displays the current parameter value.\n");
			printf("If number of hardware instructions limit is specified then the parameter value will be changed accordingly.\n");
			printf("The parameter number_of_HW_instructions must be a strictly positive integer that corresponds to the\n");
			printf("maximum number of hardware instructions allowed during the saturation process executed. The limit is applicable\n");
			printf("only when TNGSTRAT command option is used and will be reset if a normal non strategy tuning solving process is executed.\n");
			printf("Any strictly positive integer will be accepted. There is not a default value of maximum number of instructions.\n");
			printf("A value must be specified before executing the TNGSTRAT command.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"IMPORT"))){
			printf("IMPORT command format is:\n");
			printf("    IMPORT filename\n");
			printf("There is not an equivalent program option for this command.\n\n");
			printf("This command imports a TPTP problem file in TPTP format and will try to solve it. TPTP syntax\n");
			printf("will be used regardless of the SYNTAX command setting in use. If filename doesn't include a full\n");
			printf("path then the given path is supposed to start in the program executable current directory.\n");
			printf("If there is a space in the file or path specifications then the whole filename must be enclosed\n");
			printf("in single quotes. Otherwise the enclosing single quotes are optional. If there is problem data\n");
			printf("in memory then the NEW command must be used to initialize the program, otherwise the file import\n");
			printf("will fail with an error message.\n");
			printf("In non-interactive if none of the GOSAT, PBMTYPE or CLAUSIFY program options are specified.\n");
			printf("then the program will try to solve problem specified in the filename parameter.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"LAYER"))){
			printf("LAYER command format is:\n");
			printf("    LAYER [1|2|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -layer(1|2|default)\n\n");
			printf("Default value: DEFAULT\n\n");
			printf("In interactive mode if no parameter is included then this command displays the default layer selection.\n");
			printf("Drodi uses a layered clause selection with some specific on top layers if possible and a bottom\n");
			printf("weight-age layer described in the SELECTRATIO command help. Two types of layer schemes will be available\n");
			printf("depending on the problem type and user settings. It is a bit complex to describe the details\n");
			printf("here but feel free to experiment for your specific case. Anyway setting this to DEFAULT is\n");
			printf("usually the best choice. If DEFAULT is selected the layer selection type will be set to the best choice\n");
			printf("for the problem type being solved.\n");
			printf("DEFAULT parameter can be specified in non-interactive option but it is redundant.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"LEARN"))){
			printf("LEARN command format is:\n");
			printf("    LEARN [ON|OFF|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -learn(on|off)\n");
			printf("Default value: DEFAULT\n\n");
			printf("In interactive mode if no parameter is included then this command displays if use of learning data\n");
			printf("is enabled or disabled. If ON parameter is specified then learning is enabled.\n");
			printf("If OFF parameter is specified then learning is disabled.\n");
			printf("If DEFAULT parameter is specified then learning is set to the best choice for the problem type being solved.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"LEARNFROM"))){
			printf("LEARNFROM command format is:\n");
			printf("    LEARNFROM [OFF|filename]\n");
			printf("Program option format is:\n");
			printf("    -learnfrom(off|filename)\n\n");
			printf("In interactive mode if no parameter is included then this command displays the current settings.\n");
			printf("If OFF parameter is used then using a file with learned knowledge from successful proofs is disabled\n");
			printf("and the internal learning data will be used instead.\n");
			printf("If a filename with optional path is specified then the specified file with learned knowledge created\n");
			printf("with PROCLEARN command will be used. Learned knowledge will only be used by an strategy if there is a record\n");
			printf("in the file with a type that matches the strategy settings. If no match is found then the performance\n");
			printf("may be a bit worst than with LEARNFROM OFF.\n");
			printf("If there is a space in the file or path specifications then the whole filename must be enclosed\n");
			printf("in single quotes. Otherwise the enclosing single quotes are optional.\n");
			printf("Learning will not be enabled for problems without a conjecture or negated conjecture.\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option.\n");
			printf("See LEARNTO and PROCLEARN command options for additional details. Any additional parameters will be ignored.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"LEARNTO"))){
			printf("LEARNTO command format is:\n");
			printf("    LEARNTO [OFF]\n");
			printf("    LEARNTO filename [NEW]\n");
			printf("Program option format is:\n");
			printf("    -learnto(off)\n");
			printf("    -learnto(filename[,new])\n\n");
			printf("In interactive mode if no parameter is included then this command displays the current settings.\n");
			printf("If OFF parameter is used then adding knowledge in a file from successful proofs is disabled.\n");
			printf("If a filename with optional path is specified then knowledge learned from successful proofs will\n");
			printf("be added to the specified file. The file will be created if it doesn't exist. If NEW parameter\n");
			printf("is specified and the file already exists then its previous content will be destroyed. NEW option\n");
			printf("will be disabled after the first problem successfully processed with the NEW option in interactive\n");
			printf("mode. Several files created with the LEARNTO command can be jointed together using for example\n");
			printf("\"cat infile1 infile2 > outfile\" in Linux of \"copy infile1+infile2 outfile\" in Windows.\n");
			printf("Each record added to the file has a type assigned depending on the ALGORITHM, SELECT, TERMORDER,\n");
			printf("LITORDER and FUNCTIONW command settings and on the problem type (see PBMTYPE option), so there are\n");
			printf("the corresponding number of different record types.\n");
			printf("If there is a space in the file or path specifications then the whole filename must be enclosed\n");
			printf("in single quotes. Otherwise the enclosing single quotes are optional.\n");
			printf("WARNING: If many problems are processed with this option enabled then make sure that you have plenty.\n");
			printf("of disk space. The output file may be very big.\n");
			printf("Learning information will not be generated for problems without a conjecture or negated conjecture.\n");
			printf("See LEARNFROM and PROCLEARN command options for additional details.\n");
			printf("If NEW parameter is specified then any additional parameters will be ignored.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"LITORDER"))){
			printf("LITORDER command format is:\n");
			printf("    LITORDER [STANDARD|NONRECURSIVE|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -litorder(standard|nonrecursive|default)\n");
			printf("Default value: DEFAULT\n\n");
			printf("In interactive mode if no parameter is included then this command displays the default literal\n");
			printf("ordering type used. If literal ordering type is specified then the default literal ordering is\n");
			printf("changed accordingly. If CORES option is set to more than one process then other literal ordering\n");
			printf("types can be used simultaneously in order to increase the chance of finding a proof.\n");
			printf("Valid term ordering types are STANDARD for Knuth-Bendix standard ordering, NONRECURSIVE for\n");
			printf("a Knuth-Bendix non recursive ordering version or DEFAULT. If DEFAULT is selected the literal\n");
			printf("ordering type will be set to the best choice for the problem type being solved.\n");
			printf("DEFAULT parameter can be specified in non-interactive option but it is redundant.\n");
			printf("IMPORTANT: If LITORDER and TERMORDER settings are different then algorithm completeness\n");
			printf("may be compromised.\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"LOADREDUNDANT"))){
			printf("LOADREDUNDANT command format is:\n");
			printf("    LOADREDUNDANT [ON|OFF]\n");
			printf("Program option format is:\n");
			printf("    -loadredundant(on|off)\n");
			printf("Default value: ON\n\n");
			printf("The LOADREDUNDANT command changes the option of loading or not loading redundant formulas from problem files.\n");
			printf("In interactive mode if no parameter is included then this command displays current LOADREDUNDANT setting.\n");
			printf("If ON parameter is specified then loading redundant formulas is enabled.\n");
			printf("If OFF parameter is specified then loading redundant formulas is disabled.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"LOG"))){
			printf("LOG command format is:\n");
			printf("    LOG [ON|path|OFF]\n");
			printf("Program option format is:\n");
			printf("    -log(on|path|off)\n");
			printf("Default value: OFF\n\n");
			printf("The LOG command enables or disables program exceptions logging. The logged program exceptions are SIGFPE, SIGILL,\n");
			printf("SIGSEGV, SIGBUS, SIGTRAP, SIGSYS and SIGABRT/SIGIOT. OFF disables exception logging, ON enables exception logging\n");
			printf("and reports the exception data to stdout. Any other argument is interpreted as an absolute or relative path where a log file\n");
			printf("with the exception data will be created. The log file name is pbm_xxxxx.log where pbm is the problem file name or \"drodi\"\n");
			printf("if the problem was introduced in interactive mode and xxxxx is a 16 bits unsigned integer with encoded program settings\n");
			printf("that must be interpreted by the developer. Any string will be accepted as a path, if the path is not valid then the\n");
			printf("exception data will be reported to stdout.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"LOOKAHEAD"))){
			printf("LOOKAHEAD command format is:\n");
			printf("    LOOKAHEAD [ON|OFF|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -lookahead(on|off|default)\n");
			printf("Default value: DEFAULT\n\n");
			printf("This option affects only to unit clause problems.\n");
			printf("In interactive mode if no parameter is included then this command displays lookahead settings.\n");
			printf("If OFF parameter is specified then lookahead is disabled for clause selection. If ON then lookahead\n");
			printf("is enabled for clause selection and the number of estimated inferences of a few of the best clauses\n");
			printf("in the weight queues will be taken into account to prime those that generate less inferences.\n");
			printf("If DEFAULT is specified then lookahead will be set to the best choice for the problem type being solved.\n");
			printf("DEFAULT parameter can be specified in non-interactive but it is redundant.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"MAXMEMORY"))){
			printf("MAXMEMORY command format is:\n");
			printf("    MAXMEMORY [memory_limit_in_megabytes]\n");
			printf("Program option format is:\n");
			printf("    -maxmemory(memory_limit_in_megabytes)\n");
			printf("Default value: Depends on available physical memory\n\n");
			printf("In interactive mode if no parameter is included then this command displays the current memory limit.\n");
			printf("If memory limit is specified then the memory limit will be changed accordingly.\n");
			printf("The parameter memory_limit_in_megabytes must be a strictly positive integer that corresponds to the\n");
			printf("memory limit in megabytes. The memory limit is applicable only to the saturation process dynamically\n");
			printf("allocated memory, which is the memory use reported by the program. It is not applicable to static data\n");
			printf(" memory or to the memory used to store the program. Also it is not applicable to other types of processing\n");
			printf("(see PROCLEARN, CLAUSIFY, PBMTYPE and GOSAT options). Any positive integer will be accepted. However,\n");
			printf("if the real available memory when the saturation process starts is less than the specified limit then\n");
			printf("the real available memory limit will be used.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"MAXWEIGHT"))){
			printf("MAXWEIGHT command format is:\n");
			printf("    MAXWEIGHT [LRS|LRSOTTER|LRSDISCOUNT|LRSOFF|DEFAULT|OFF|positive_number]\n");
			printf("Program option format is:\n");
			printf("    -maxweight(lrs|lrsotter|lrsdiscount|lrsoff|default|off|positive_number)\n");
			printf("Default value: DEFAULT\n\n");
			printf("In interactive mode f no parameter is included then this command displays the maximum weight\n");
			printf("criteria used to retain inferred clauses. If maximum weight criteria is specified then the maximum\n");
			printf("weight criteria is changed accordingly. The valid criteria are:\n");
			printf("LRS: Use a limited resource strategy algorithm to delete clauses that are not reachable\n");
			printf("     in the time available for both Otter and Discount algorithms.\n");
			printf("LRSOTTER: Use a limited resource strategy algorithm to delete clauses that are not reachable\n");
			printf("     in the time available only for Otter algorithm.\n");
			printf("LRSDISCOUNT: Use a limited resource strategy algorithm to delete clauses that are not reachable\n");
			printf("     in the time available only for Discount algorithm.\n");
			printf("LRSOFF: Don't use the limited resource strategy algorithm to delete clauses that are not reachable\n");
			printf("     in the time available.\n");
			printf("DEFAULT: Use the limited resource strategy according to the best choice for the problem type being solved.\n");
			printf("DEFAULT parameter can be specified in non-interactive option but it is redundant.\n");
			printf("OFF: Do not use the maximum weight criteria to discard clauses.\n");
			printf("Positive_number: Clauses with weight greater than the number specified are deleted. The weight is roughly\n");
			printf("equal to the number of symbols in the clause.\n\n");
			printf("Specifying a maximum weight is compatible with the use of LRS. If both are enabled\n");
			printf("then both will be applied for retaining and discarding clauses.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("If SATMODE option is set to MED or OFF then the limited resources strategy algorithm may be disabled\n");
			printf("for some or all saturation processes in order not to compromise completeness.\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"NEW"))){
			printf("NEW command format is:\n");
			printf("    NEW\n");
			printf("There is not an equivalent program option for this command.\n\n");
			printf("NEW command initializes data for a new theorem. All previously introduced sentences and conjectures\n");
			printf("are deleted. The program asks for a confirmation before initializing.\n");
			printf("This command has no parameters\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"PBMTYPE"))){
			printf("PBMTYPE command format is:\n");
			printf("    PBMTYPE [filename]\n");
			printf("Program option format is:\n");
			printf("    -pbmtype\n\n");
			printf("Prints the problem type flags:\n");
			printf("UNIT_EQUALITY if the problem has some positive unit equalities.\n");
			printf("UNIT_CLAUSE if all the clauses are unit clauses.\n");
			printf("HORN if all the clauses are horn.\n");
			printf("GROUND if all the clauses are ground.\n");
			printf("NONUNIT_EQU if there are some equalities none of which are positive unit equalities.\n");
			printf("This option also prints the problem type according to the problem flags. There are 9 valid\n");
			printf("flag combinations so there are 9 problem types identified by a number in the range 1-9.\n");
			printf("In interactive mode if filename is not specified then the current problem introduced with\n");
			printf("TELL and ASK commands is analyzed. If filename is specified then the file will be loaded and\n");
			printf("analyzed. In this case the program status must be previously initialized with the NEW command\n");
			printf("if some previous formulae was introduced by hand. If filename is specified then any additional\n");
			printf("parameters will be ignored.\n");
			printf("In non-interactive mode a filename must be specified as the final parameter in program invocation.\n");
			printf("All problem data will be initialized after command execution.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"PRECEDENCE"))){
			printf("PRECEDENCE command format is:\n");
			printf("    PRECEDENCE [precedence_criteria|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -precedence(precedence_criteria|default)\n");
			printf("Default value: DEFAULT\n\n");
			printf("In interactive mode if no parameter is included then this command displays the symbol precedence\n");
			printf("criteria used. If precedence criteria is specified then the symbol precedence criteria is changed\n");
			printf("accordingly for ordering purposes. Valid symbol precedence criteria are:\n");
			printf("DEFAULT: symbol precedence will be done according to the best choice for the problem type being solved.\n");
			printf("DEFAULT parameter can be specified in non-interactive option but it is redundant.\n");
			printf("ARITY: Symbol precedence is set according to symbol arity.\n");
			printf("FREQUENCY: Symbol precedence is set according to symbol frequency in problem formula.\n");
			printf("OCCURRENCE: Symbol precedence is set according to symbol order of appearance in problem formula.\n");
			printf("INVARITY: Symbol precedence is set in reverse order with respect to symbol arity.\n");
			printf("INVFREQ: Symbol precedence is set in reverse order with respect to symbol frequency in problem formula.\n");
			printf("INVOCCUR: Symbol precedence is set in reverse order with respect to symbol order of appearance in problem formula.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"PERFDATA"))){
			printf("PERFDATA command format is:\n");
			printf("    PERFDATA\n");
			printf("Program option format is:\n");
			printf("    -perfdata\n");
			printf("This command performs quick benchmarks to evaluate the processor performance and speed and displays the results.\n");
			printf("This command is currently in development state. See TMFACTOR and HWINSTR commands for additional information.\n");
			printf("This command has no parameters. Any specified parameters will be ignored.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"PERFMON"))){
			printf("PERFMON command format is:\n");
			printf("    PERFMON [ON|OFF [instructions_rate]]\n");
			printf("Program option format is:\n");
			printf("    -perfmon(ON|OFF[,instructions_rate])\n");
			printf("In interactive mode if no parameter is included then this command displays the performance monitoring\n");
			printf("state and the hardware instructions per second used to emulate the performance monitor if the real thing\n");
			printf("is not available or it is disabled by the user. If ON is specified then performance monitoring is enabled.\n");
			printf("If OFF is specified then performance monitoring is disabled.\n");
			printf("If performance monitoring is enabled and a valid CAP_PERFORM authorization exist then the number of\n");
			printf("executed hardware instructions is monitored to control the amount of resources assigned to each strategy.\n");
			printf("Otherwise this control is emulated using elapsed time.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"PRINTKBDATA"))){
			printf("PRINTKBDATA command format is:\n");
			printf("    PRINTKBDATA [ON|OFF]\n");
			printf("Program option format is:\n");
			printf("    -printkbdata(on|off)\n");
			printf("Default value: OFF\n\n");
			printf("In interactive mode if no parameter is included then this command displays the current\n");
			printf("option selected for printing knowledge base data at the end of a run. If a parameter\n");
			printf("is specified then the option is changed accordingly.\n");
			printf("If ON is specified then the data of the winner strategy KB will be printed. If there is no winner strategy\n");
			printf("then data will be printed only if just one core is enabled.\n");
			printf("If OFF is specified then no data will be printed.\n");
			printf("Any additional parameters will be ignored.\n");
			printf("This option is mainly intended for debugging purposes.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"PROCLEARN"))){
			printf("PROCLEARN command format is:\n");
			printf("    PROCLEARN infilename outfilename\n");
			printf("Program option format is:\n");
			printf("    -proclearn(infilename,outfilename)\n\n");
			printf("This command will process infilename file created or updated when the LEARNTO option is enabled and will\n");
			printf("create outfilename file with learned knowledge that will be used when LEARNFROM option is enabled. The infilename\n");
			printf("file must exist.\n");
			printf("Records of each type in input file will be processed together and a single output record will be built and stored\n");
			printf("in the output file.\n");
			printf("This option uses a lot of memory so it will use all free physical memoy available. It will not be limited by the\n");
			printf("MAXMEMORY setting.\n");
			printf("The file outfilename will be replaced if it already exist. See LEARNTO and PROCLEARN command options for\n");
			printf("additional details.\n");
			printf("Several files created with the PROCLEARN command can be jointed together using for example\n");
			printf("\"cat infile1 infile2 > outfile\" in Linux of \"copy infile1+infile2 outfile\" in Windows.\n");
			printf("If there is a space in the file or path specifications then the whole infilename or outfilename must be enclosed\n");
			printf("in single quotes. Otherwise the enclosing single quotes are optional.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"PROOF"))){
			printf("PROOF command format is:\n");
			printf("    PROOF [DRODI|TPTP|OFF]\n");
			printf("Program option format is:\n");
			printf("    -proof(drodi|tptp|off)\n");
			printf("Default value: DRODI\n\n");
			printf("In interactive mode if no parameter is included then this command displays the type\n");
			printf("of proof currently in use. If proof type is specified then the proof type is changed.\n");
			printf("accordingly. Valid proof types are TPTP, DRODI and OFF. TPTP type is the proof mode used in\n");
			printf("CASC TPTP problems. DRODI type is default and it is a more legible mode for humans.\n");
			printf("OFF type prevents printing a proof. Any additional parameters will be ignored.\n\n");
			printf("WARNING: If SYNTAX is set to DRODI and the problem is introduced via TELL and ASK commands the proof\n");
			printf("will always be displayed in DRODI format, because TPTP format is not compatible with DRODI syntax.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"PRPRCSYMPREC"))){
			printf("PRPRCSYMPREC command format is:\n");
			printf("    PRPRCSYMPREC [HIGH|LOW|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -prprcsymprec(high|low|default)\n");
			printf("Default value: DEFAULT\n\n");
			printf("In interactive mode if no parameter is included then this command displays the introduced symbols precedence\n");
			printf("criteria used. If introduced symbols precedence criteria is specified then the precedence of symbol introduced\n");
			printf("during pre-processing such as skolems or predicate definitions is changed accordingly.\n");
			printf("Valid symbol precedence criteria are:\n");
			printf("DEFAULT: introduced symbol precedence will be done according to the best choice for the problem type being solved.\n");
			printf("DEFAULT parameter can be specified in non-interactive option but it is redundant.\n");
			printf("HIGH: Introduced symbols will have higher preference than normal symbols for ordering purposes.\n");
			printf("LOW: Introduced symbols will have lower preference than normal symbols for ordering purposes.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"RELEVANCE"))){
			printf("RELEVANCE command format is:\n");
			printf("    RELEVANCE [OFF]\n");
			printf("    RELEVANCE ON|NONEQU threshold [RESIGN]\n");
			printf("Program option format is:\n");
			printf("    -relevance(off)\n");
			printf("    -relevance(on|nonequ,threshold[,resign])\n");
			printf("Default value: NONEQU, threshold 0.1 seconds\n\n");
			printf("In interactive mode if no parameter is included then this command displays the current clause relevance\n");
			printf("filtering settings. OFF will disable relevance filtering, ON will enable it for all problems and NONEQU\n");
			printf("will enable it only for problems without equality. The threshold is a positive number. If a decimal\n");
			printf("separator \".\" is included then the threshold is the maximum time in seconds for the filtering\n");
			printf("algorithm. Otherwise the threshold is the maximum number of iterations allowed in in the filtering\n");
			printf("algorithm. If threshold is zero then the filtering algorithm is run without time or iteration limits.\n");
			printf("If RESIGN is specified then if the threshold is reached no filtering will be applied and all input\n");
			printf("clauses will be selected.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"SATMODE"))){
			printf("SATMODE command format is:\n");
			printf("    SATMODE [ON|MED|OFF|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -satmode(on|med|off|default)\n");
			printf("Default value: OFF\n\n");
			printf("This option defines how search for satisfiability will be performed when trying to solve a problem.\n");
			printf("If OFF parameter is specified then Drodi will search mainly for theorem or unsatisfiable status.\n");
			printf("Satisfiability or counter-satisfiability may be eventually found but will not be reported and complete\n");
			printf("strategies will not be necessarily used.\n");
			printf("If MED is specified then Drodi will search for both theorem or unsatisfiable and satisfiable \n");
			printf("or counter-satisfiable status. Both complete and incomplete strategies will be used in the search.\n");
			printf("If ON parameter is specified then Drodi will search mainly for theorem or satisfiable or counter-\n");
			printf("satisfiable status. No incomplete strategies will be used.\n");
			printf("If DEFAULT is specified then SATMODE will be set to OFF.\n");
			printf("DEFAULT parameter can be specified in non-interactive but it is redundant.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"SELECT"))){
			printf("SELECT command format is:\n");
			printf("    SELECT [selection_criteria|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -select(selection_criteria|default)\n");
			printf("Default value: DEFAULT\n\n");
			printf("In interactive mode if no parameter is included then this command displays the literal selection\n");
			printf("criteria used. If literal selection criteria is specified then the selection criteria is changed\n");
			printf("accordingly. Valid selection criteria are:\n");
			printf("DEFAULT: literal selection will be done according to the best choice for the problem type being solved.\n");
			printf("DEFAULT parameter can be specified in non-interactive option but it is redundant.\n");
			printf("MAXIMAL: Select maximal literals.\n");
			printf("SINGLENEG: Select a negative literal if possible, otherwise proceed as in MAXIMAL.\n");
			printf("MXSINGLENEG: Select a maximal negative literal if there is a maximal negative literal, otherwise proceed\n");
			printf("as in MAXIMAL.\n");
			printf("MULTINEG: Select the maximal negative literals if possible, otherwise proceed as in MAXIMAL.\n");
			printf("SINGLEPOS: Select the first positive literal if possible, otherwise proceed as in MAXIMAL.\n");
			printf("MULTIPOS: Select the maximal positive literals if possible, otherwise proceed as in MAXIMAL.\n");
			printf("ALL: Select all literals.\n");
			printf("The following selection criteria are defined in the document \"Selecting the selection \" by Hoder:\n");
			printf("TYPE1: selection 1002 of the document.\n");
			printf("TYPE2: selection 1003 of the document.\n");
			printf("TYPE3: selection 1004 of the document.\n");
			printf("TYPE4: selection 1010 of the document.\n");
			printf("TYPE5: selection 1011 of the document.\n");
			printf("TYPE6: selection 1012 of the document.\n");
			printf("TYPE1CPL: Complete version of TYPE1.\n");
			printf("TYPE2CPL: Complete version of TYPE2.\n");
			printf("TYPE3CPL: Complete version of TYPE3.\n");
			printf("TYPE4CPL: Complete version of TYPE4.\n");
			printf("TYPE5CPL: Complete version of TYPE5.\n");
			printf("TYPE6CPL: Complete version of TYPE6.\n");
			printf("Selections SINGLEPOS, MULTIPOS and TYPE1 to TYPE6 don't preserve completeness.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"SELECTRATIO"))){
			printf("SELECTRATIO command format is:\n");
			printf("    SELECTRATIO [weight:age|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -selectratio(weight:age|default)\n");
			printf("Default value: DEFAULT\n\n");
			printf("In interactive mode if no parameter is included then this command displays the weight and age\n");
			printf("ratios used for clause selection. If weight and age ratios are specified then the selection ratios\n");
			printf("are changed accordingly. The ratios must be a set of two positive integer numbers that specify\n");
			printf("the number of clauses selected from each queue (weight and age) in each round-robin cycle.\n");
			printf("The weight queue selects lighter clauses first.\n");
			printf("The age queue selects older clauses first. Its ratio number must be strictly positive.\n");
			printf("DEFAULT: weight to age ratio will be set according to the best choice for the problem type being solved.\n");
			printf("DEFAULT parameter can be specified in non-interactive option but it is redundant.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"SHUFFLE"))){
			printf("SHUFFLE command format is:\n");
			printf("    SHUFFLE [ON|OFF]\n");
			printf("Program option format is:\n");
			printf("    -shuffle(on|off)\n");
			printf("Default value: ON\n\n");
			printf("In interactive mode if no parameter is included then this command displays if shuffling is enabled\n");
			printf("or disabled. If ON parameter is specified then shuffling is enabled. If OFF parameter is specified then\n");
			printf("shuffling is disabled. When shuffling is enabled the following actions are sometimes performed when\n");
			printf("trying to solve a problem:\n");
			printf("- Shuffling the order in which generated clauses are put into the passive set.\n");
			printf("- reordering literals in a newly generated clause and in each given clause before activation.\n");
			printf("- randomization of function and term symbol precedence used for literal and term ordering.\n");
			printf("- age-weight ratios are only probabilistically respected.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"SPLIT"))){
			printf("SPLIT command format is:\n");
			printf("    SPLIT [ON|OFF|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -split(on|off)\n");
			printf("Default value: DEFAULT\n\n");
			printf("In interactive mode if no parameter is included then this command displays if splitting is enabled\n");
			printf("or disabled. If ON parameter is specified then splitting is enabled and the interface between\n");
			printf("first order prover and SAT solver is enabled (AVATAR architecture).\n");
			printf("If OFF parameter is specified then splitting is disabled and the interface between first order\n");
			printf("prover and SAT solver is disabled.\n");
			printf("If DEFAULT parameter is specified then splitting is set to the best choice for the problem type being solved.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"STRATEGY"))){
			printf("STRATEGY command format is:\n");
			printf("    STRATEGY [strategy_flags|OFF]\n");
			printf("Program option format is:\n");
			printf("    -split(strategy_flags|off)\n");
			printf("In interactive mode if no parameter is included then this command displays user strategy status\n");
			printf("and settings. If OFF parameter is specified then user strategy is disabled. If strategy_flags is\n");
			printf("specified it must be a number between 0 and %ld containing the strategy flags",0x7fffffffffffffff);
			printf("of the desired strategy. If used with a parameter this command disables any previous strategy related settings\n");
			printf("specified with  the commands ALGORITHM, CONNECT, DEMODULATION, FACTORING, FUNCTIONW, GAMMA, GRJOIN, LAYER,\n");
			printf("LITORDER, LOOKAHEAD, MAXWEIGHT, PRECEDENCE, PRPRCSYMPREC, SELECT, SELECTRATIO, SPLIT, TERMORDER,\n");
			printf("WEAKRW or LEARN.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"SYNTAX"))){
			printf("SYNTAX command format is:\n");
			printf("    SYNTAX [DRODI|TPTP]\n");
			printf("There is not an equivalent program option for this command.\n\n");
			printf("Default value: TPTP\n\n");
			printf("In interactive mode if no parameter is included then this command displays the type of syntax\n");
			printf("currently in use. If syntax type is specified then the syntax type is changed accordingly.\n\n");
			printf("Valid syntax types are TPTP and DRODI. TPTP type the default and it is the syntax used in CASC\n");
			printf("TPTP problems. DRODI type is an internal syntax whose basic difference with TPTP is that variables\n");
			printf("doesn't need to be in upper case but they must always be quantified.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("WARNING: If SYNTAX is set to DRODI and the problem is introduced via TELL and ASK commands the proof\n");
			printf("will be displayed in DRODI format, because TPTP format is not compatible with DRODI syntax.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"TELL"))){
			printf("TELL command format is:\n");
			printf("    TELL sentence\n");
			printf("There is not an equivalent program option for this command.\n\n");
			printf("Inputs a new sentence. Sentences must end with a period \".\". If the sentence\n");
			printf("is too long the sentence text can be introduced in several lines keying ENTER \n");
			printf("after each line and including a period in the last line to indicate the end of\n");
			printf("the sentence.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"TERMORDER"))){
			printf("TERMORDER command format is:\n");
			printf("    TERMORDER [STANDARD|NONRECURSIVE|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -termorder(standard|nonrecursive|default)\n");
			printf("Default value: DEFAULT\n\n");
			printf("In interactive mode if no parameter is included then this command displays the default term ordering\n");
			printf("type used. If term ordering type is specified then the default term ordering is changed accordingly.\n");
			printf("If CORES option is set to more than one process then other term ordering types can be used simultaneously\n");
			printf("in order to increase the chance of finding a proof. The term ordering used is always the Knuth-Bendix,\n");
			printf("but two versions of it are available.\n");
			printf("Valid term ordering types are STANDARD, NONRECURSIVE and DEFAULT. If DEFAULT is selected the\n");
			printf("term ordering type will be set to the best choice for the problem type being solved.\n");
			printf("DEFAULT parameter can be specified in non-interactive option but it is redundant.\n");
			printf("IMPORTANT: If LITORDER and TERMORDER settings are different then algorithm completeness\n");
			printf("may be compromised.\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"TIMEOUT"))){
			printf("TIMEOUT command format is:\n");
			printf("    TIMEOUT [number_of_seconds]\n");
			printf("Program option format is:\n");
			printf("    -timeout(number_of_seconds)\n");
			printf("Default value: 10 seconds\n\n");
			printf("In interactive mode if no parameter is included then this command displays the maximum number\n");
			printf("of seconds allowed to find a proof. If number of seconds is specified then the timeout value will\n");
			printf("be changed accordingly. The parameter number_of_seconds must be an unsigned integer.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"TMFACTOR"))){
			printf("TMFACTOR command format is:\n");
			printf("    TMFACTOR [time_factor_value|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -tmfactor(time_factor_value|default)\n");
			printf("Sets the factor to be applied to strategy slice instructions or time to account for CPU speed. This is a strictly\n");
			printf("positive floating point number less than or equal to 100.0. If DEFAULT is selected its value will be set to 1.0\n");
			printf("If nothing is specified in interactive mode the current TMFACTOR value will be displayed.\n");
			printf("This command is used if the program is executed in a computer with different speed than the computer\n");
			printf("with which the strategy portfolio was generated and can be used both for normal processes and in strategy tuning processes.\n");
			printf("However in normal processes the real number of executed HW instructions will be reported if VERBOSE option is not OFF\n");
			printf("while in strategy tuning processes the readjusted number of executed HW instructions will be reported so that the results\n");
			printf("can be used to complete or enhance tuning performed in a different computer.\n");
			printf("See PERFDATA command help for hints for an appropriate time_factor_value parameter.\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"TNGSTRAT"))){
			printf("TNGSTRAT command format is:\n");
			printf("    TNGSTRAT [filename]\n");
			printf("Program option format is:\n");
			printf("    -tngstrat\n\n");
			printf("This option evaluates a specific strategy on a problem by running a saturation process against several\n");
			printf("versions of the problem with and without randomization. Several things are important in order for this option to work: \n");
			printf("- A valid user strategy must be previously defined with the STRATEGY option.\n");
			printf("- No additional settings must be specified after the STRATEGY option using the following commands: ALGORITHM, CONNECT,\n");
			printf("  DEMODULATION, FACTORING, FUNCTIONW, GAMMA, GRJOIN, LAYER, LITORDER, LOOKAHEAD, MAXWEIGHT, PRECEDENCE, PRPRCSYMPREC,\n");
			printf("  SELECT, SELECTRATIO, SPLIT, TERMORDER, WEAKRW and LEARN.\n");
			printf("- A limit of hardware instructions must be previously specified with the HWINSTR command and a bigger enough timeout\n");
			printf("  must be specified with the TIMEOUT command to allow all instructions to be executed.\n");
			printf("In interactive mode if filename is not specified then the current problem introduced with\n");
			printf("TELL and ASK commands is analyzed. If filename is specified then the file will be loaded and\n");
			printf("analyzed. In this case the program status must be previously initialized with the NEW command\n");
			printf("if some previous formulae was introduced by hand. If filename is specified then any additional\n");
			printf("parameters will be ignored.\n");
			printf("In non-interactive mode a filename must be specified as the final parameter in program invocation.\n");
			printf("All problem data will be initialized after command execution.\n");
			printf("===========================\n");

			kk=0;
		}
		if ((jj)||(0==strcmp(token,"VERBOSE"))){
			printf("VERBOSE command format is:\n");
			printf("    VERBOSE [ALL|ON|OFF]\n");
			printf("Program option format is:\n");
			printf("    -verbose(all|on|off)\n");
			printf("Default value: OFF\n\n");
			printf("In interactive mode if no parameter is included then this command displays the current VERBOSE option.\n");
			printf("setting. If ON parameter is specified then statistic and other relevant information will be displayed.\n");
			printf("setting. If ALL parameter is specified then problem input pre-processing information will be displayed\n");
			printf("in addition to the information displayed with the ON setting.\n");
			printf("If OFF parameter is specified then only basic information is displayed.\n");
			printf("Proof information is not affected by VERBOSE setting, it is displayed according to the PROOF command setting.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("===========================\n");
			kk=0;
		}
		if ((jj)||(0==strcmp(token,"WEAKRW"))){
			printf("WEAKRW command format is:\n");
			printf("    WEAKRW [ON|OFF|DEFAULT]\n");
			printf("Program option format is:\n");
			printf("    -weakrw(on|off|default)\n");
			printf("Default value: DEFAULT\n\n");
			printf("This option affects only to unit equality problems.\n");
			printf("In interactive mode if no parameter is included then this command displays weak rewriting settings.\n");
			printf("If OFF parameter is specified then weak rewriting is disabled for clause selection. If ON then weak rewriting\n");
			printf("is enabled and positive equalities with both members having variables not in the other member will\n");
			printf("be split in several better behaved equalities.\n");
			printf("If DEFAULT is specified then weak rewriting will be set to the best choice for the problem type being solved.\n");
			printf("DEFAULT parameter can be specified in non-interactive but it is redundant.\n");
			printf("Any additional parameters will be ignored.\n\n");
			printf("Except if DEFAULT is specified this command disables any previous strategy specified with the STRATEGY\n");
			printf("command or option. See CORES and TNGSTRAT commands for additional relevant information.\n");
			printf("===========================\n");
			kk=0;
		}
		if (kk){
			if (pgmmode==INTERACTIVE){
				printf("%s is not a valid command option, type HELP for a list of valid options\n",token);
			} else {
				printf("%s is not a valid option, use -help option without parameters for a list of valid options\n",token);
			}
		}

	/* General help. */
	} else {
		printf("\nCopyright (C) 2015-2016 Oscar Contreras. All rights reserved.\n");
		printf("Program invocation:\n  Drodi [options] [filename]\n");
		printf("Drodi has two modes of operation. If options or filename are entered then the program\n");
		printf("enters in non-interactive mode and tries to perform the requested action, usually\n");
		printf("solving a problem. If no options nor a filename are entered then the program enters\n");
		printf("the interactive mode and the user is requested to enter commands.\n");
		printf("NON INTERACTIVE MODE:\n");
		printf("====================\n");
		printf("Options are preceded by a \"-\" and are NOT case sensitive. They may contain parameters\n");
		printf("enclosed in parenthesis and separated by commas, for instance:\n");
		printf(" -gosat -verbose(on)\n");
		printf("If the filename contains a space in the file or path specifications then the whole filename must be\n");
		printf("enclosed in single quotes. Otherwise the enclosing single quotes are optional.\n");
		printf("INTERACTIVE MODE:\n");
		printf("================\n");
		printf("Input lines are interpreted as commands unless they start with a \"#\" character,\n");
		printf("in which case the line is considered a comment and it is ignored. This is useful\n");
		printf("for including comments in input files for batch jobs. Commands and parameters are\n");
		printf("NOT case sensitive except for filenames or formulas.\n");
		printf("\nCommand format is:\n");
		printf("   command_option [parameters]\n\n");
		printf("Most commands have an equivalent program option with an identical name, except that the program option\n");
		printf("must be preceded by a \"-\". For instance VERBOSE ON is an interactive command and -verbose(on)\n");
		printf("is the equivalent program invocation option.");
		printf("Program options/User commands:\n");
		printf("ALGORITHM: Change default inference algorithm to be used.\n");
		printf("ASK: Input a conjecture.\n");
		printf("CLAUSIFY: Convert problem formulas to CNF and print them in TPTP format.\n");
		printf("CONNECT: Enable, disable or set default of connectedness for UEQDSC and UEQOTT algorithms.\n");
		printf("CORES: Change the number of processes to be started for proving a theorem.\n");
		printf("DEMODULATION: Set or disable the demodulation and destructive equality resolutions settings.\n");
		printf("EXIT: Exit the program.\n");
		printf("FACTORING: Enable or disable factoring inferences.\n");
		printf("FUNCTIONW: Change weight function type to be used.\n");
		printf("GAMMA: Adjust parameter to compute learning weight.\n");
		printf("GO: Try to prove the theorem.\n");
		printf("GOLXFORM: Enable or disable goal transformatio option for unit equality problems.\n");
		printf("GOSAT: Try to prove the theorem using only the SAT solver.\n");
		printf("GRJOIN: Enable (ON or MED), disable or set default of ground joinability for UEQDSC and UEQOTT algorithms.\n");
		printf("HELP: Display this help.\n");
		printf("HWINSTR: Set the maximum number of executable hardware instruction for TNGSTRAT command option.\n");
		printf("IMPORT: Import and try to solve a theorem file in TPTP format.\n");
		printf("LAYER: Define the layers to use in layered clause selection.\n");
		printf("LEARN: Enable or disable use of learning data.\n");
		printf("LEARNFROM: Enable or disable the use of a file with previously learned knowledge.\n");
		printf("LEARNTO: Enable or disable appending knowledge learned from successful proofs to a file.\n");
		printf("LITORDER: Change literal ordering type to be used.\n");
		printf("LOADREDUNDANT: Change option for loading redundant formula from problem files.\n");
		printf("LOG: Log program exceptions.\n");
		printf("LOOKAHEAD: Enable, disable or set default of lookahead option for unit clause problems.\n");
		printf("MAXMEMORY: Change the memory limit.\n");
		printf("MAXWEIGHT: Change the criteria to be used to decide the maximum weight allowed for clauses.\n");
		printf("NEW: Initialize data for a new problem.\n");
		printf("PBMTYPE: Print problem type flags.\n");
		printf("PERFDATA: Get processor performance information.\n");
		printf("PERFMON: Enable or disable hardware instructions performance monitoring.\n");
		printf("PRECEDENCE: Specify symbol precedence for ordering purposes.\n");
		printf("PRPRCSYMPREC: Specify introduced symbols precedence for ordering purposes.\n");
		printf("PRINTKBDATA: Change the option for printing KB data at the end of a problem run.\n");
		printf("PROCLEARN: Process a file created with LEARNTO option and create a file usable by LEARNFROM option.\n");
		printf("PROOF: Set the proof output format.\n");
		printf("RELEVANCE: Set the relevance clause filtering parameters.\n");
		printf("SATMODE: Set the way to search for satisfiability when solving a problem.\n");
		printf("SELECT: Change literal selection criteria.\n");
		printf("SELECTRATIO: Change weight and age ratios to be used for selecting the current clause when LEARNTO is disabled.\n");
		printf("SHUFFLE: Enable or disable problem shuffling and randomization.\n");
		printf("SPLIT: Enable or disable splitting and interface between first order prover and SAT solver.\n");
		printf("STRATEGY: Set or query user defined strategy.\n");
		printf("SYNTAX: Change type of syntax to be used.\n");
		printf("TELL: Input a sentence.\n");
		printf("TERMORDER: Change term ordering type to be used.\n");
		printf("TIMEOUT: Change maximum time allowed for solving a problem.\n");
		printf("TMFACTOR: Factor to be applied to strategy slice time to account for CPU speed.\n");
		printf("TNGSTRAT: Evaluate a specific strategy on several problem versions with and without randomization.\n");
		printf("VERBOSE: Enable or disable VERBOSE option.\n");
		printf("WEAKRW: Enable, disable or set default of weak rewriting option.\n");
		printf("To get help in interactive mode for a specific command type:\n");
		printf("  HELP [command_name]\n");
		printf("To get help in non-interactive mode for a specific option use the following program option:\n");
		printf("  -help(option_name).\n");
		printf("To get the complete help text in interactive mode type:\n");
		printf("  HELP ALL\n");
		printf("To get the complete help text in non-interactive mode use the following program option:\n");
		printf("  -help(all).\n");
	}

	return;
} /* Help */
