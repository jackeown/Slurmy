/*
 ============================================================================
 Name        : Drodi.c
 Author      : Oscar Contreras
 Version     :
 Copyright   : Copyright (C) 2015-2016 Oscar Contreras. All rights reserved.
 Description : First order logic theorem prover in C
 ============================================================================
 */
/****************************************************************
*
*                        Drodi (MAIN module)
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
 *    Entry point for Drodi application. It includes the program initialization
 *    and exit, syntax checking and miscellanea functions.
 *
 *    Drodi is a multi-platform Open Source Artificial Intelligence application.
 *
 *    Current functionality is just a first order logic knowledge base with
 *    saturation algorithms based mainly in ordered resolution, paramodulation
 *    and factoring. This part of the program is based mainly in Vampire theorem
 *    prover, the document "Implementing an efficient theorem prover" by
 *    Alexandre Riazanov and the bibliography cited in that document.
 *
 *
 *  IMPLEMENTATION AND PORTABILITY:
 *
 *    This program is intended to be multi-platform and as portable
 *    as possible. However the following identified restrictions
 *    apply:
 *    - The compiler must be at least C99 compliant.
 *    - The C compiler and target machine must support int16_t,
 *      int32_t, int64_t and their corresponding unsigned types.
 *      Otherwise an error will occur at compile time.
 *    - The char data type must be 8 bits long. Otherwise the
 *      program will report it and terminate at run time.
 *
 *  NOTES:
 *
 *
 *--------------------------------------------------------------*/

/* Include for all C sources */
#define VARTYPE /* Indicate that global variables will be defined in this module. */
                /* Otherwise global variables are only declared. */
#include "global.h"

/* Specific includes. */
#define PERFMONIOCTL /* Indicate that myioctl() static inline function must be included. */
#include "perfmon.h"

/** Global variables for this module: only those that need initialization in their definition. */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Drodi main entry point)  OCJ
 *
 *    Drodi main entry point. Calls the command processing function.
 *
 *    If arguments are passed, the first one must be the path and name
 *    of the KB file to be opened.
 *
 *  ARGUMENTS:
 *
 *    argc: Number of arguments passed, including the program filename.
 *    argv: Pointer to set of pointers to each argument. The first
 *          argument argument is the program filename. The last argument
 *          must be a valid problem filename. The remaining arguments
 *          must be valid options. If no arguments are specified (that is,
 *          argc==1) then the program will enter interactive mode.
 *
 *  RETURNS:
 *
 *    int (return code, 0 ==> no error).
 *
 *--------------------------------------------------------------*/
int main(int argc,char **argv){

	/* Portability check. */
	if (CHAR_BIT!=8){
		printf("Computer architecture or compilation not supported\n");
		return(0);
	}

	/* Initialize program. */
	switch (Initialize_pgm()){
		case NOMEMORY:
			printf("No memory available for initialization\n");
			return(0);
			break;
		case 1:
			printf("Error creating knowledge base\n");
			return(0);
			break;
	}

	/* Call command processor if interactive mode. */
	if (argc==1){
		pgmmode=INTERACTIVE;
		CommandProc();

	/* Call arguments parsing if non-interactive mode. */
	} else {
		pgmmode=NONINTERACTIVE;
		ParseArgs(argc,argv);
	}

	/* Print total statistics, clean program and exit. */
	PrintStats(1);
	#ifdef BENCHMARKING
	auto int32_t bii,bjj;
	auto FILE *filehandle;
	auto char string[16];
	if (benchfname==NULL){
		filehandle=stdout;
	} else {
		filehandle=fopen(benchfname,"a");
	}
	if (BENCHTYPE&1){
		for (bii=0;bii<BENCHDIMENSION;bii++){
			bjj=(int32_t)bresults[0][bii];
			switch (bjj){
				case -1:
					fputs("NOMEMORY ",filehandle);
					break;
				case -2:
					fputs("TIMEOUT ",filehandle);
					break;
				case -3:
					fputs("UNKNOWN ",filehandle);
					break;
				case -4:
					fputs("PREPROC ",filehandle);
					break;
				default:
					sprintf(string,"%.6f ",bresults[0][bii]);
					fputs(string,filehandle);
					break;
			}
		}
		fputs("\n",filehandle);
	}
	if (BENCHTYPE&2){
		for (bii=0;bii<BENCHDIMENSION;bii++){
			bjj=(int32_t)bresults[1][bii];
			switch (bjj){
				case 1:
					fputs("CSA ",filehandle);
					break;
				case 2:
					fputs("SAT ",filehandle);
					break;
				case 3:
					fputs("THM ",filehandle);
					break;
				case 4:
					fputs("CAX ",filehandle);
					break;
				case 5:
					fputs("UNS ",filehandle);
					break;
				default:
					fputs("UK ",filehandle);
					break;
			}
		}
		fputs("\n",filehandle);
	}
	if (VERIFYSATFINALSTATUS){
		for (bii=0;bii<BENCHDIMENSION;bii++){
			bjj=(int32_t)bresults[2][bii];
			fputs("SAT",filehandle);
			if (bjj==SATOK){
				fputs("_OK",filehandle);
			} else if (bjj==SATUNK){
				fputs("_UNKNOWN",filehandle);
			} else {
				if (bjj&SATSAT){
					fputs("_SAT",filehandle);
				}
				if (bjj&SATUNSAT){
					fputs("_UNSAT",filehandle);
				}
				switch (bjj&(SATUNPR1|SATUNPR2|SATUNPR12)){
					case SATUNPR1:
						fputs("_UNPROC1",filehandle);
						break;
					case SATUNPR2:
						fputs("_UNPROC2",filehandle);
						break;
					case SATUNPR12:
						fputs("_UNPROC1+2",filehandle);
						break;
				}
				switch (bjj&(SATPASSIVE1|SATPASSIVE2|SATPASSIVE12)){
					case SATUNPR1:
						fputs("_PASSIVE1",filehandle);
						break;
					case SATUNPR2:
						fputs("_PASSIVE2",filehandle);
						break;
					case SATUNPR12:
						fputs("_PASSIVE1+2",filehandle);
						break;
				}
				switch (bjj&(SATACTIVE1|SATPASSIVE2|SATPASSIVE12)){
					case SATUNPR1:
						fputs("_PASSIVE1",filehandle);
						break;
					case SATUNPR2:
						fputs("_PASSIVE2",filehandle);
						break;
					case SATUNPR12:
						fputs("_PASSIVE1+2",filehandle);
						break;
				}
				switch (bjj&(SATLOCKED1|SATLOCKED2|SATLOCKED12)){
					case SATUNPR1:
						fputs("_LOCKED1",filehandle);
						break;
					case SATUNPR2:
						fputs("_LOCKED2",filehandle);
						break;
					case SATUNPR12:
						fputs("_LOCKED1+2",filehandle);
						break;
				}
			}
			fputs(" ",filehandle);
		}
		fputs("\n",filehandle);
	}
	if (benchfname!=NULL){
		fclose(filehandle);
		MYFREE(benchfname);
	}
	#endif
	Clean4Exit();
	return(0);
} /* main */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Initialize program)  OCJ
 *
 *    This function performs all program initialization tasks.
 *
 *
 *  ARGUMENTS:
 *
 *    None.
 *
 *  RETURNS:
 *
 *    0 --> Initialization was successful.
 *    1 --> Error creating KB.
 *    NOMEMORY --> No memory available.
 *
 *--------------------------------------------------------------*/
int32_t Initialize_pgm(void){
	auto int32_t ii;                                /* Auxiliary */
	auto binprefix binp;                            /* Auxiliary to calculate binpsize */
	auto satclause satcl;                           /* Auxiliary to calculate satclsize */
	#ifdef SEMANTICTAUTOLOGY
	auto ttlgclause ttlgcl;                         /* Auxiliary to calculate ttlgclsize */
	#endif
	auto struct rlimit rslimit;                     /* Auxiliary to get/set memory limits */
	auto struct sigaction act;                      /* Auxiliary for signals handling */
	#ifdef DEBUGMEMORY
	for (ndemods=0;ndemods<DBGMEMELEMS;ndemods++){
		memptrs[ndemods]=NULL;
		memtrack[ndemods]=0;
	}
	ndemods=0;
	auxcounter=0;
	#endif

	/* Ensure the default locale is used. */
	setlocale(LC_ALL,"C");

	/* Disable program core dumps. */
	getrlimit(RLIMIT_CORE,&rslimit);
	rslimit.rlim_cur=0;
	setrlimit(RLIMIT_CORE,&rslimit);

	/* Set the number of cores, initialize mmap region and allocate memory */
	/* for child processes pids. */
	if (0>=(num_cores=sysconf(_SC_NPROCESSORS_ONLN))){
		num_cores=1;
	}
	active_cores=num_cores;
	procnb=active_cores-1;
	if (num_cores>1){
		if (NULL==(pids=MYALLOC((num_cores>20?num_cores-1:19)*sizeof(*pids)))){
			return(NOMEMORY);
		}
		pids[0]=0;
	}
	if (NOMEMORY==Init_mmap()){
		if (active_cores>1){
			MYFREE(pids);
		}
		return(NOMEMORY);
	}
	initavmemory=GetAvailableMemory();

	#ifdef BENCHMARKING
	*bncounter=0;
	benchfname=NULL;
	#endif

	#ifdef MEMORYTUNNING
	/* Memory limiting tuning. */
	if (SymMalloc(ADDTLMEMORY)){
		MYFREE(pids);
		munmap(mmapptr,mmapsize);
		return(NOMEMORY);
	}
	#endif

	/* Miscellanea initializations. */
	binpsize=(((char *)&binp.formula[0])-((char *)&binp));
	satclsize=(((char *)&satcl.formula[0])-((char *)&satcl));
	#ifdef SEMANTICTAUTOLOGY
	ttlgclsize=(((char *)&ttlgcl.formula[0])-((char *)&ttlgcl));
	#endif

	/* Initialize component symbol hash tables. */
	Initialize_hashcmp();

	/* Create KB's. */
	if (0!=Create_KB(&mainkb)){
		if (active_cores>1){
			MYFREE(pids);
		}
		munmap(mmapptr,mmapsize);
		MYFREE(slctordr);
		#ifdef MEMORYTUNNING
		SymMalloc(0);
		#endif
		return(NOMEMORY);
	}
	if (pgmmode==INTERACTIVE){
		printf("Knowledge bases have been initialized\n");
	}

	/* Initialize statistics variables. */
	ResetStats(&glblstats);

	/* Initialize hash values for item positions. */
	srand(0x9747b28c);
	for (ii=0;ii<MAXPOSITIONS;ii++){
		hashpos[ii]=rand();
	}

	/* Initialize CPU time availability. */
	if (-1==clock()){
		cpuavail=0;
	} else {
		cpuavail=1;
	}

	/* Initialize other defaults. */
	/*lookahead y weakrw.*/
	glblopts.timeout=10.0;
	glblopts.mxwghtopt=MXWEIGHTOFF;
	glblopts.maxweight=ABSMAXWEIGHT;
	glblopts.shuffle=1;
	glblopts.satmode=0;
	glblopts.goalxform=1;
	procctl->status=STOPPED;
	procctl->prfprntinproc=0;
	pgmmode=INTERACTIVE;

	/* Catch program exceptions. */
	sigfillset(&act.sa_mask);
	act.sa_handler=CatchSignal1;
	act.sa_flags=0;
	sigaction(SIGFPE,&act,NULL);
	sigaction(SIGILL,&act,NULL);
	sigaction(SIGSEGV,&act,NULL);
	sigaction(SIGBUS,&act,NULL);
	sigaction(SIGABRT,&act,NULL);
	sigaction(SIGIOT,&act,NULL);
	sigaction(SIGTRAP,&act,NULL);
	sigaction(SIGSYS,&act,NULL);
	sigaction(SIGXCPU,&act,NULL);
	sigaction(SIGALRM,&act,NULL);
	sigaction(SIGTERM,&act,NULL);
	sigaction(SIGINT,&act,NULL);
	sigaction(SIGQUIT,&act,NULL);
	sigaction(SIGHUP,&act,NULL);
	return(0);
} /* Initialize_pgm */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Performs the needed cleaning before exit)  OCJ
 *
 *    This function performs any check needed before exit.
 *    If it is OK to exit it frees all memory allocated and
 *    deletes all KB's.
 *
 *    By the moment it just checks if the DB is saved.
 *
 *
 *  ARGUMENTS:
 *
 *    None.
 *
 *  RETURNS:
 *
 *    None
 *
 *--------------------------------------------------------------*/
void Clean4Exit(void){

	/* Clean for exit. */
	if (num_cores>1){
		MYFREE(pids);
	}
	#ifdef MEMORYTUNNING
	/* Memory limiting tuning. */
	SymMalloc(0);
	#endif
	Delete_KB(&mainkb);
	MYFREE(slctordr);
	MYFREE(learnto);
	if (learnfrom!=NULL){
		MYFREE(learnfrom);
	}
	MYFREE(logpath);
	if (procnb<(active_cores-1)){
		sem_post(&procsem[procnb]); /* Indicate that child process is exiting. */
	}
	munmap(mmapptr,mmapsize);
	return;
} /* Clean4Exit */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Confirm answer with user)  OCJ
 *
 *    This function checks if main KB is saved. If not it issues a
 *    warning message and returns with a value depending on the
 *    user answer.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    msg1: Pointer to warning message. It must include a final
 *          question like "exit anyway? [y/N]: "
 *    msg2: Pointer to message issued if user answers NO.
 *
 *  RETURNS:
 *
 *    1 --> User answered "Y".
 *    0 --> User answered "N".
 *
 *--------------------------------------------------------------*/
int32_t CheckAnswer(char *msg1,char *msg2){
	auto char line[40];                             /* Input line buffer */
	auto char *ltkn;                                /* Pointer to token */

	/* Issue warning and get response. */
	printf(msg1);
	fgets(line,NUMELEMS(line),stdin);
	line[strlen(line)-1]=0;

	/* Check response and return user answered "N" if appropriate. */
	ltkn=strtok(line," ");
	if ((ltkn==NULL)||(strlen(ltkn)!=1)||('Y'!=toupper(ltkn[0]))){
		printf("\n");
		printf(msg2);
		printf("\n");
		return(0);
	}

	/* Return user answered "Y". */
	return(1);
} /* CheckSaved */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Create a new KB)  OCJ
 *
 *    This function initializes the storage associated to a knowledge base
 *    (KB for short).
 *
 *    There is one main KB and one working KB per process. The main KB
 *    contains the input formulas (that is, the theory and conjecture)
 *    in text form. The working KB contains the compiled formulas and
 *    the clauses inferred during saturation process.
 *
 *
 *  ARGUMENTS:
 *
 *    pkb: Pointer to KB being created. If this is &mainkb then the main KB
 *         and the working KB will be initialized.
 *
 *  RETURNS:
 *
 *    0: KB's were successfully created.
 *    NOMEMORY: Not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t Create_KB(kbase *pkb){
	pkb->firsttxt=pkb->lasttxt=pkb->frstactive=pkb->lstactive=
			pkb->frstpassive=pkb->lstpassive=
			pkb->frstunproc=pkb->lstunproc=
			pkb->frstgrjoin=pkb->lstgrjoin=pkb->negeq=
			pkb->frstlocked=pkb->lstlocked=pkb->lastdisp=NULL;
	pkb->firstsatq=pkb->lastsatq=pkb->frstsatglbl=pkb->lastsatglbl=
			pkb->frstunsats=pkb->frstunsats2=pkb->lstunsats=NULL;
	pkb->frsthshundf=pkb->frsthunbp=pkb->lasthshundf=NULL;
	pkb->satrootnode=NULL;
	pkb->vartrmidx.symbol=NULL;
	if (NOMEMORY==Initialize_KB(pkb)){
		Delete_KB(pkb);
		return(NOMEMORY);
	}
	ResetStats(&pkb->prstats);
	return(0);
} /* Create_KB */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Initialize a KB)  OCJ
 *
 *    This function frees all memory allocated for a KB and
 *    performs any required initialization.
 *
 *    If KB is the main KB then the working KB and hash tables
 *    will be initialized.
 *
 *    This function doesn't resets KB statistic data. The function
 *    ResetStats() must be called by the calling function when
 *    appropriate.
 *
 *
 *  ARGUMENTS:
 *
 *    pkb: Pointer to KB being created. If this is mainkb then the
 *         main KB and the working KB will be initialized.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t Initialize_KB(kbase *pkb){
	auto satnode *ptr1,*ptr2;                       /* Auxiliary pointers */
	auto int32_t ii;                                /* Auxiliary */

	/* Free formula and tree memory and initialize queue pointers. */
	Free_Formula(pkb);

	/* Miscellanea initializations. */
	pkb->nformulas=0;
	pkb->flags=0;
	pkb->signature=1;
	pkb->locksignature=0;
	pkb->splsymbols=0;

	/* Main KB. */
	if (pkb==&mainkb){

		/* Miscellanea initializations. */
		mainkb.prstats.elapsed_time=0;
		cpu_time=0;
		numskolem=0;
		skolemid=0;
		numpdefs=0;
		pdefid=0;
		numfunc=0;
		functid=0;
		splid=0;
		numsymbols=0;
		numpreds=0;
		maxusecount=0;
		maxarity=0;
		pbmflags=(GROUND|UNITCLAUSE);

		/* Delete working KB. */
		Delete_KB(&kbset);

		/* Initialize hash component symbol tables. */
		Initialize_hash();
		Initialize_hashcmp();

		/* Initialize the equality hash element. */
		equkey.nextelem=equkey.nextsymbol=NULL;
		equkey.arity=2;
		equkey.type=EQUALITY;
		equkey.hash=0x1b873593;
		for (ii=0;ii<DSCTREETYPES;ii++){
			equkey.discr[ii]=NULL;
			#ifdef DEBUGTREE
			equkey.numleafs[ii]=0;
			#endif
		}

		/* Recreate working KB. */
		if (NOMEMORY==Create_KB(&kbset)){
			Delete_KB(&mainkb);
			return(NOMEMORY);
		}

	/* Working KB. Free SAT tree nodes memory and initialize the vartrmidx member */
	/* of kbase structure. This is not necessary for the main KB. */
	} else {
		for (ptr1=pkb->satrootnode;ptr1!=NULL;ptr1=ptr2){
			ptr2=ptr1->nextnode;
			MYFREE(ptr1);
		}
		pkb->satrootnode=NULL;
		if (NULL==(pkb->vartrmidx.symbol=MYALLOC(TREEPTR_CHUNK_SIZE*sizeof(void *)))){
			return(NOMEMORY);
		}
		pkb->vartrmidx.size=TREEPTR_CHUNK_SIZE;
		pkb->vartrmidx.used=0;
	}

	return(0);
} /* Initialize_KB */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Delete a KB)  OCJ
 *
 *    This function fees all memory associated to a KB. If KB is
 *    the main KB then all working KB's will be deleted.
 *
 *
 *  ARGUMENTS:
 *
 *    pkb: Pointer to KB being initialized. If this is mainkb then
 *         the main KB and working KB will be deleted.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void Delete_KB(kbase *pkb){

	/* Get address of pointer to KB being deleted. Also, if this is */
	/* the main KB then delete all working KB's and their hash tables. */
	if (pkb==&mainkb){
		Delete_KB(&kbset);
		kb_hashcmp=NULL;
	}

	/* Free KB definition structure. */
	Free_Formula(pkb);
	if (pkb==&mainkb){
		Initialize_hash();
	}

	return;
} /* Delete_KB */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Free formula and tree memory and initialize queue
 *                pointers)  OCJ
 *
 *    This function frees all memory allocated for a specific KB
 *    and its tree pointers and initializes related queues. It also frees
 *    assertions, locks, SAT queue and SAT solver clauses and tree.
 *
 *    This function must be called only from Initialize_KB() or Delete_KB().
 *
 *
 *  ARGUMENTS:
 *
 *    pkb: Pointer to KB whose memory must be freed.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void Free_Formula(kbase *pkb){
	auto cmprefix *ptr1,*ptr2;                      /* Auxiliary pointers */
	auto satclause *ptr3,*ptr4;                     /* Auxiliary pointer */
	auto satnode *ptr5,*ptr6;                       /* Auxiliary pointers */
	auto int32_t ii,jj;                             /* Auxiliary */

	/* Free formulas memory. */
	for (ptr1=pkb->lasttxt;ptr1!=NULL;ptr1=ptr2){
		ptr2=ptr1->prev;
		#ifdef ENABLECHOICEAXIOM
		if (ptr1->inference==CHOICEAXIOM){
			for (ii=0;((skldata **)ptr1->parent1)[ii]!=NULL;ii++){
				MYFREE(((skldata **)ptr1->parent1)[ii]);
			}
			MYFREE(ptr1->parent1);
		}
		#else
		if (ptr1->inference==SKOLEMIZATION){
			for (ii=0;((skldata **)ptr1->parent2)[ii]!=NULL;ii++){
				MYFREE(((skldata **)ptr1->parent2)[ii]);
			}
			MYFREE(ptr1->parent2);
		}
		#endif
		MYFREE(ptr1->part2.text);
		MYFREE(ptr1);
	}
	for (ptr1=pkb->lstactive;ptr1!=NULL;ptr1=ptr2){
		ptr2=ptr1->prev;
		if (ptr1->part2.bin->ovly.asserts!=NULL){
			MYFREE(ptr1->part2.bin->ovly.asserts);
		}
		if ((ptr1->flags&LOCKED)&&(ptr1->part2.bin->lockasserts!=NULL)){
			MYFREE(ptr1->part2.bin->lockasserts);
		}
		MYFREE(ptr1->part2.bin);
		MYFREE(ptr1);
	}
	for (ptr1=pkb->lstpassive;ptr1!=NULL;ptr1=ptr2){
		ptr2=ptr1->prev;
		if (ptr1->part2.bin->ovly.asserts!=NULL){
			MYFREE(ptr1->part2.bin->ovly.asserts);
		}
		if ((ptr1->flags&LOCKED)&&(ptr1->part2.bin->lockasserts!=NULL)){
			MYFREE(ptr1->part2.bin->lockasserts);
		}
		MYFREE(ptr1->part2.bin);
		MYFREE(ptr1);
	}
	for (ptr1=pkb->lstunproc;ptr1!=NULL;ptr1=ptr2){
		ptr2=ptr1->prev;
		if (ptr1->part2.bin->ovly.asserts!=NULL){
			MYFREE(ptr1->part2.bin->ovly.asserts);
		}
		if ((ptr1->flags&LOCKED)&&(ptr1->part2.bin->lockasserts!=NULL)){
			MYFREE(ptr1->part2.bin->lockasserts);
		}
		MYFREE(ptr1->part2.bin);
		MYFREE(ptr1);
	}
	for (ptr1=pkb->lstgrjoin;ptr1!=NULL;ptr1=ptr2){
		ptr2=ptr1->prev;
		MYFREE(ptr1->part2.bin);
		MYFREE(ptr1);
	}
	for (ptr1=pkb->lstlocked;ptr1!=NULL;ptr1=ptr2){
		ptr2=ptr1->prev;
		if (ptr1->part2.bin->ovly.asserts!=NULL){
			MYFREE(ptr1->part2.bin->ovly.asserts);
		}
		if (ptr1->part2.bin->lockasserts!=NULL){
			MYFREE(ptr1->part2.bin->lockasserts);
		}
		MYFREE(ptr1->part2.bin);
		MYFREE(ptr1);
	}

	/* Free tree memory and memory allocated for variables */
	/* that are maximal root terms of selected positive */
	/* equalities in active clauses. */
	if (pkb==&kbset){
		FreeTreeMemory();
		MYFREE(kbset.vartrmidx.symbol);
		kbset.vartrmidx.symbol=NULL;
	}

	/* Free SAT queue and SAT solver clauses. */
	for (ptr4=pkb->firstsatq;ptr4!=NULL;ptr4=ptr3){
		ptr3=ptr4->glblnext;
		MYFREE(ptr4);
	}
	for (ptr4=pkb->frstsatglbl;ptr4!=NULL;ptr4=ptr3){
		ptr3=ptr4->glblnext;
		MYFREE(ptr4);
	}

	/* Free SAT solver tree. */
	for (ptr5=pkb->satrootnode;ptr5!=NULL;ptr5=ptr6){
		ptr6=ptr5->nextnode;
		MYFREE(ptr5);
	}

	/* Initialize queue pointers. */
	pkb->firsttxt=pkb->lasttxt=pkb->frstactive=pkb->lstactive=
			pkb->frstpassive=pkb->lstpassive=
			pkb->frstunproc=pkb->lstunproc=
			pkb->frstgrjoin=pkb->lstgrjoin=pkb->negeq=
			pkb->frstlocked=pkb->lstlocked=pkb->lastdisp=NULL;
	pkb->firstsatq=pkb->lastsatq=pkb->frstsatglbl=pkb->lastsatglbl=
			pkb->frstunsats=pkb->frstunsats2=pkb->lstunsats=NULL;
	pkb->satrootnode=NULL;
	pkb->frsthshundf=pkb->frsthunbp=pkb->lasthshundf=NULL;
	if (pkb==&kbset){
		for (ii=0;ii<(2*SELECTQUEUES);ii++){
			frstglselect[ii]=lstglselect[ii]=NULL;
			for (jj=0;jj<MAXWEIGHT;jj++){
				lstselect[ii][jj]=frstselect[ii][jj]=NULL;
			}
		}
	}
	return;
} /* Free_Formula */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Free inferred formulas in main KB)  OCJ
 *
 *    This function frees all memory allocated for formulas in the
 *    main KB that were not created in the input process, that is,
 *    inferred formulas either in binary or text format.
 *
 *    Additionally the nformulas field of the main KB will be updated to
 *    reflect only the formulas left after the deletion and the INPROOFQUEUE
 *    flag of input formulas and negated conjecture is cleared.
 *
 *    As opposed to Free_Formula() this function can be called from
 *    anywhere and not only from Initialize_KB() or Delete_KB().
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
void FreeInfFormulas(void){
	auto cmprefix *ptr1,*ptr2,*ptr3;                /* Auxiliary pointers */
	auto int32_t ii;                                /* Auxiliary */

	/* Clear INPROOFQUEUE flag of input formulas and negated conjecture. */
	for (ptr3=mainkb.firsttxt;(ptr3!=NULL)&&(ptr3->inference<=PLAIN);ptr3=ptr3->next){
		ptr3->flags&=(~INPROOFQUEUE);
	}

	/* Free inferred formulas memory. */
	for (ptr3=mainkb.lasttxt;(ptr3!=NULL)&&(ptr3->inference>PLAIN);ptr3=ptr2){
		ptr2=ptr3->prev;
		#ifdef ENABLECHOICEAXIOM
		if (ptr3->inference==CHOICEAXIOM){
			for (ii=0;((skldata **)ptr3->parent1)[ii]!=NULL;ii++){
				MYFREE(((skldata **)ptr3->parent1)[ii]);
			}
			MYFREE(ptr3->parent1);
		}
		#else
		if (ptr3->inference==SKOLEMIZATION){
			for (ii=0;((skldata **)ptr3->parent2)[ii]!=NULL;ii++){
				MYFREE(((skldata **)ptr3->parent2)[ii]);
			}
			MYFREE(ptr3->parent2);
		}
		#endif
		MYFREE(ptr3->part2.text);
		MYFREE(ptr3);
	}
	for (ptr1=mainkb.lstunproc;ptr1!=NULL;ptr1=ptr2){
		ptr2=ptr1->prev;
		if (ptr1->part2.bin->ovly.asserts!=NULL){
			MYFREE(ptr1->part2.bin->ovly.asserts);
		}
		if ((ptr1->flags&LOCKED)&&(ptr1->part2.bin->lockasserts!=NULL)){
			MYFREE(ptr1->part2.bin->lockasserts);
		}
		MYFREE(ptr1->part2.bin);
		MYFREE(ptr1);
	}
	mainkb.signature=1;
	mainkb.locksignature=0;

	/* Initialize queue pointers. */
	mainkb.lasttxt=ptr3;
	if (ptr3!=NULL){
		ptr3->next=NULL;
	} else {
		mainkb.firsttxt=NULL;
	}
	mainkb.frstactive=mainkb.lstactive=mainkb.frstpassive=mainkb.lstpassive=
			mainkb.frstunproc=mainkb.lstunproc=mainkb.frstgrjoin=mainkb.lstgrjoin=
			mainkb.frstlocked=mainkb.lstlocked=mainkb.lastdisp=mainkb.negeq=NULL;
	mainkb.firstsatq=mainkb.lastsatq=mainkb.frstsatglbl=mainkb.lastsatglbl=
			mainkb.frstunsats=mainkb.frstunsats2=mainkb.lstunsats=NULL;
	mainkb.frsthshundf=mainkb.frsthunbp=mainkb.lasthshundf=NULL;
	mainkb.satrootnode=NULL;

	/* Update the nformulas field of the KB to reflect */
	/* only the formulas left after the deletion. */
	mainkb.nformulas=mainkb.lasttxt->number;

	return;
} /* FreeInfFormulas */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Initialize symbol hash tables for names in input formulas)  OCJ
 *
 *    This function initializes the symbol hash tables for global names in
 *    input formulas and frees any memory allocated to them.
 *
 *    This formula doesn't free hash table memory for component names.
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
void Initialize_hash(void){
	auto hashchain *ptr1;                           /* Auxiliary pointer */

	/* Free allocated memory. */
	for (;kb_hash!=NULL;kb_hash=ptr1){
		ptr1=kb_hash->nextelem;
		MYFREE(kb_hash);
	}

	/* Initialize hashkeys. */
	memset(hashkeys,0,sizeof(hashkeys));

	return;
} /* Initialize_hash */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Initialize symbol hash tables for component names)  OCJ
 *
 *    This function initializes the hash table memory for component
 *    names and frees any memory allocated to each component element
 *    for an specific KB. This function doesn't free memory allocated
 *    to the hash table itself, it only frees the chained component
 *    elements memory.
 *
 *    This formula doesn't free hash table memory for global names
 *    in input formulas.
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
void Initialize_hashcmp(void){
	auto hashchcmp *ptr1;                           /* Auxiliary pointer */

	/* Free component symbols allocated memory. */
	for (;kb_hashcmp!=NULL;kb_hashcmp=ptr1){
		ptr1=kb_hashcmp->nextelem;
		MYFREE(kb_hashcmp->formula);
		MYFREE(kb_hashcmp);
	}

	/* Initialize hashsplit. */
	memset(&hashsplit[0],0,HASHVALUES*sizeof(void *));

	return;
} /* Initialize_hashcmp */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add formula to main KB)  OCJ
 *
 *    This function adds an input formula to a KB. The formula is
 *    syntax checked but it is not clausified nor compiled.
 *
 *    The end of the formula must be indicated by a period. This function
 *    will read additional lines if needed until an end of formula
 *    period is found.
 *
 *    It is assumed that mainkb.time structure contains a valid time with
 *    which current time can be compared to detect timeouts.
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to string with the initial part of the formula
 *             to be added.
 *    role: Role of formula.
 *    flag: The following flags are used:
 *          NEGCONJ: formula is a negated conjecture
 *          FROMMAINFILE: formula comes from the main file and can be considered
 *                        a conjecture to the purposes of detecting contradictory
 *                        axioms.
 *
 *  RETURNS:
 *
 *    0--> the formula was not added.
 *    1--> the formula was successfully added.
 *    NOMEMORY--> not enough memory.
 *    TIMEOUT--> timeout and the formula was not added.
 *
 *--------------------------------------------------------------*/
int32_t Tell(char *formula,int32_t role,int32_t flag){
	auto char *ptr1;                                    /* Auxiliary pointer */
	auto cmprefix *ptr2;                                /* Auxiliary pointer */
	auto char *cnvsntnc1;                               /* Pointer to full formula */
	auto int32_t size;                                  /* Size in bytes of cnvsntnc1 buffer */
	auto int32_t jj;                                    /* Auxiliary */

	/* Allocate space for full formula and copy formula to buffer. */
	if (formula!=NULL){
		size=(strlen(formula))+INPUT_CHUNK_SIZE;
	} else {
		size=INPUT_CHUNK_SIZE;
	}
	if (NULL==(cnvsntnc1=MYALLOC(size))){
		if (pgmmode==NONINTERACTIVE){
			printf("%% SZS status MemoryOut : ");
		}
		printf("Not enough memory available.\n");
		return(NOMEMORY);
	}
	if (formula!=NULL){
		strcpy(cnvsntnc1,formula);
	} else {
		cnvsntnc1[0]=0;
	}

	/* Input remaining formula lines. */
	if (NOMEMORY==InputSentence(&cnvsntnc1,&size)){
		if (pgmmode==NONINTERACTIVE){
			printf("%% SZS status MemoryOut : ");
		}
		printf("Not enough memory available.\n");
		MYFREE(cnvsntnc1);
		return(NOMEMORY);
	}
	if (cnvsntnc1[0]==0){
		if (pgmmode==INTERACTIVE){
			printf("Empty formula\n");
		}
		MYFREE(cnvsntnc1);
		return(0);
	}

	/* Check syntax. */
	if ((verbose==2)&&(pgmmode==INTERACTIVE)){
		printf("%s\n",cnvsntnc1);
		printf("Checking syntax... ");
	}
	switch (jj=CheckSyntax(&cnvsntnc1,&size,&ptr1,NULL,flag)){

		/* No memory. */
		case NOMEMORY:
			if (pgmmode==NONINTERACTIVE){
				printf("%% SZS status MemoryOut : ");
			}
			printf("No memory available\n");
			MYFREE(cnvsntnc1);
			return(jj);
			break;

		/* Timeout. */
		case -1:
			if (pgmmode==NONINTERACTIVE){
				printf("%% SZS status Timeout: ");
			}
			printf("Timeout parsing formulae\n");
			MYFREE(cnvsntnc1);
			return(TIMEOUT);
			break;

		/* Valid syntax. */
		case VALIDSENTENCE:
			if ((verbose==2)&&(pgmmode==INTERACTIVE)){
				printf("Valid formula\n");
			}
			break;

		/* Syntax errors. */
		default:
			if (pgmmode==NONINTERACTIVE){
				printf("%% SZS status InputError : ");
			}
			switch (jj){
				case EMPTYBLOCK:
					printf("Empty block:\n");
					break;
				case INVALIDDECLAREDARGS:
					printf("Predicate or function doesn't match previously declared number of arguments:\n");
					break;
				case INVALIDDECLAREDTYPE:
					printf("Predicate or function doesn't match previous declared type:\n");
					break;
				case INVTPTPVARNAME:
					printf("Invalid variable name for TPTP syntax:\n");
					break;
				case INVALIDNAME:
					printf("Invalid variable name:\n");
					break;
				case INVTPTPNAME:
					printf("Invalid predicate, function or variable name for TPTP syntax:\n");
					break;
				case INVALIDATOMIC:
					printf("Invalid atomic formula:\n");
					break;
				case EMPTYSENTENCE:
					printf("Empty formula:\n");
					break;
				case MISSINGARGS:
					printf("Missing argument:\n");
					break;
				case ALREADYDECLARED:
					printf("Already declared variable name:\n");
					break;
				case INVALIDVARLIST:
					printf("Invalid variable list:\n");
					break;
				case MISSINGVARINLIST:
					printf("Missing variable in variable list:\n");
					break;
				case TOOMANYARGS:
					printf("Too many arguments in function or predicate:\n");
					break;
				case RESERVEDNAME:
					printf("Reserved variable name used for function or predicate:\n");
					break;
				case SYNTAXERROR:
					printf("Syntax error:\n");
					break;
				default:
					printf("Unknown error:\n");
					break;
				}
			break;
	}

	/* Return if check syntax was not successful. */
	if (jj!=VALIDSENTENCE){
		ptr1[1]=0;
		if (pgmmode==NONINTERACTIVE){
			printf("%% ");
		}
		printf(cnvsntnc1);
		printf("\n");
		MYFREE(cnvsntnc1);
		return(0);
	}

	/* Add formula to main KB. */
	ptr1=SkipBlanks(cnvsntnc1);
	if (NULL==(ptr2=MYALLOC(sizeof(cmprefix)))){
		if (pgmmode==NONINTERACTIVE){
			printf("%% SZS status MemoryOut : ");
		}
		printf("No memory available for more processing.\n");
		MYFREE(cnvsntnc1);
		return(NOMEMORY);
	}
	if (NULL==(ptr2->part2.text=MYALLOC(strlen(ptr1)+1))){
		if (pgmmode==NONINTERACTIVE){
			printf("%% SZS status MemoryOut : ");
		}
		printf("No memory available for more processing.\n");
		MYFREE(cnvsntnc1);
		MYFREE(ptr2);
		return(NOMEMORY);
	}
	if (flag&NEGCONJ){
		ptr2->flags=(FROMCONJECTURE|FROMMAINFILE);
	} else if (flag&FROMMAINFILE){
		ptr2->flags=FROMMAINFILE;
	} else {
		ptr2->flags=0;
	}
	ptr2->parent1=ptr2->parent2=NULL;
	AddTxtFormula2KB(ptr2,role,&mainkb);
	strcpy(ptr2->part2.text,ptr1);

	/* Free allocated memory and return. */
	MYFREE(cnvsntnc1);
	return(1);
} /* Tell */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Input formula lines)  OCJ
 *
 *    This function performs the input process of a formula. A formula
 *    may be composed by several lines. The end of formula is indicated
 *    by a period ".".
 *
 *    Initial spaces or tabs are skipped. If an input line consists only
 *    of spaces and tabs then an explanatory message is shown.
 *
 *    Multiple lines are connected by a single space character.
 *    When the end of formula is detected then the final period is removed.
 *    Leading and trailing spaces and tabs are also removed.
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Address of pointer to string with the initial formula content.
 *             This buffer may be reallocated if required.
 *    size: Address of pointer to size in bytes of formula buffer.
 *
 *  RETURNS:
 *
 *    0 if successful. NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t InputSentence(char **formula,int32_t *size){
	auto char *line;                                 /* Input line buffer */
	auto char *auxptr1,*auxptr2;                     /* Auxiliary pointers */
	auto int32_t linesize;                           /* Input line buffer length */
	auto int32_t ii,jj;                              /* Auxiliary */

	/* Initial memory allocation for input line. */
	if (NULL==(line=MYALLOC(INPUT_CHUNK_SIZE))){
		return(NOMEMORY);
	}
	linesize=INPUT_CHUNK_SIZE;

	/* Strip leading and trailing spaces and tabs in formula. */
	auxptr1=SkipBlanks(*formula);
	memmove(*formula,auxptr1,1+strlen(auxptr1));
	auxptr1=*formula;
	for (ii=strlen(auxptr1)-1;((ii>=0)&&((auxptr1[ii]==' ')||(auxptr1[ii]=='\t')));ii--){
	}
	auxptr1[ii+1]=0;

	/* Process loop. */
	while (1){

		/* Leave loop if end of line. */
		auxptr2=*formula;
		ii=strlen(auxptr2)-1;
		if (auxptr2[ii]=='.'){
			auxptr2[ii]=0;
			break;
		}

		/* Input next line. */
		printf("Input: ");
		ii=0;
		do {
			fgets(&line[ii],linesize-ii,stdin);
			if (line[strlen(line)-1]!='\n'){
				ii=linesize-1;
				jj=1;
				linesize+=INPUT_CHUNK_SIZE;
				if (NULL==(auxptr1=MYREALLOC(line,linesize))){
					MYFREE(line);
					return(NOMEMORY);
				}
				line=auxptr1;
			} else {
				jj=0;
			}
		} while (jj);
		line[strlen(line)-1]=0;

		/* Strip leading and trailing spaces and tabs in line. */
		auxptr1=SkipBlanks(line);
		for (ii=strlen(auxptr1)-1;((ii>=0)&&((auxptr1[ii]==' ')||(auxptr1[ii]=='\t')));ii--){
		}
		auxptr1[ii+1]=0;

		/* Empty string. */
		if (auxptr1[0]==0){
			printf("End line with a period \".\" to terminate input process\n");

		/* Non empty string. */
		} else {

			/* Allocate additional memory if necessary. */
			ii=2+sizeof(auxptr1[0])*(strlen(*formula)+strlen(auxptr1));
			if (ii>*size){
				if (NULL==(auxptr2=MYREALLOC(*formula,ii+INPUT_CHUNK_SIZE))){
					MYFREE(line);
					return(NOMEMORY);
				}
				*formula=auxptr2;
				*size=ii+INPUT_CHUNK_SIZE;
			}

			/* Add line to formula. */
			strcat(*formula," ");
			strcat(*formula,auxptr1);
		}
	}

	/* Free memory and return. */
	MYFREE(line);
	return(0);
} /* InputSentence */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check formula syntax)  OCJ
 *
 *    This function checks the syntax of a given formula. It sets
 *    a pointer to the point from which syntax is incorrect or not
 *    understood. This function is called recursively.
 *
 *    If the syntax is OK and the function is in the root call then
 *    the new symbols are added to the global symbol list.
 *
 *	  formula syntax:
 *	  ==============
 *
 *    There are two syntax modes: DRODI and TPTP. Both are basically
 *    the TPTP syntax only with different naming standards.
 *
 *    Names in DRODI syntax:
 *    ---------------------
 *	  Constants, variables, predicates and functions: Upper and lower case letters
 *	  in any combination, numbers and underscore "_". The first character must be
 *	  a letter. There is no restriction on the use of upper and lower case letters,
 *	  a name is a variable is it is previously declared in a quantifier, otherwise
 *	  it is a predicate or function name.
 *	  The following names are reserved and cannot be used for predicates and
 *	  functions:
 *	  - "Xnnnnn" where nnn is a signed positive integer less than 32768. These names
 *	    conflict with standardized variable names. This is not a restriction for
 *	    TPTP syntax because predicates and functions cannot start with an upper case.
 *
 *    Names in TPTP syntax:
 *    --------------------
 *    Variable names start with upper case. Predicate and function names start with
 *    lower case.
 *
 *    Other syntax rules:
 *    ------------------
 *    The remaining syntax rules (subset of TPTP) are summarized below:
 *
 *	  Formula: Atom , Complex formula
 *	  Atom: Predicate
 *	        Predicate(Term,...)
 *          Term = Term
 *	  Complex formula: (Formula)
 *	                    ~ Formula
 *	                    Formula & Formula
 *	                    Formula | Formula
 *	                    Formula => Formula
 *	                    Formula <= Formula
 *	                    Formula <=> Formula
 *	                    Quantifier [Variable,...] : Formula
 *	  Operators:
 *	    AND: &
 *	    OR: |
 *	    NAND: ~&
 *	    NOR: ~|
 *	    XOR: <~>
 *	    Implication: =>
 *	    Reverse implication: <=
 *	    Equivalence: <=>
 *	    NOT: !
 *	    Equality: =
 *	    Inequality: !=
 *
 *	  Term: Function(Term,...)
 *	        Constant
 *	        Variable
 *
 *	  Universal quantifier: ! [variable, variable...] :
 *
 *	  Existential quantifier: ? [variable, variable...] :
 *
 *	  Variables must be declared in a quantifier before they are used in a formula.
 *	  Each variable declaration is valid only within the scope of the quantifier
 *	  where it is declared. If a quantifier uses the same variable name than a
 *	  previous quantifier with a wider scope then a new instance of the variable
 *	  is created which is valid only in the scope of the inner quantifier.
 *
 *    The maximum number of unskolemized variables is 255 once the formula
 *    has been skolemized in a TELLed formula. However, a compiled clause
 *    in the knowledge base may contain up to 32767 variables to make room
 *    for the joining of clauses in the resolution algorithm.
 *
 *    The maximum number of arguments for predicates and functions is 0x3fffffff.
 *
 *	  A quantifier that uses the same variable name than a previous quantifier
 *	  with the same scope is invalid, as a variable cannot be declared twice at
 *	  the same level. Quantifier scopes are defined with brackets.
 *
 *	  According to the above the  following construction is valid, with the
 *	  variable x of the second quantifier being different than the variable x
 *	  of the first quantifier:
 *	    ! [x] : (formula1) => (? [x] : formula2)
 *	  However the following construction is invalid, as x is declared twice
 *	  in quantifiers with the same scope:
 *	    ! [x] : (formula1) => ? [x] : formula2
 *
 *	  Operator precedence: =, ~, quantifiers, binary operators. All binary operators
 *	  have the same precedence and are executed left to right. Therefore they
 *	  are left associative. "(" and ")" brackets  can be used to modify operators
 *	  order of execution.
 *
 *    It is assumed that mainkb.time structure contains a valid time with
 *    which current time can be compared to detect timeouts.
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Address of pointer to string with the formula.
 *    size: Pointer to size in bytes of formula buffer. Only used in root call
 *          when syntax mode is TPTP.
 *    result: Pointer to the point from which syntax is incorrect or not understood.
 *    vars: Pointer to nameschain structure. It must be null for first call.
 *    flag: The following flags are used:
 *          NEGCONJ: formula is a negated conjecture
 *          FROMMAINFILE: formula comes from the main file and can be considered
 *                        a conjecture to the purposes of detecting contradictory
 *                        axioms.
 *
 *  RETURNS:
 *
 *    VALIDSENTENCE -> Syntax is valid.
 *    -1 -> Timeout.
 *    Other -> A syntax or NOMEMORY error code (see global.h).
 *
 *--------------------------------------------------------------*/
int32_t CheckSyntax(char **formula,int32_t *size,char **result,nameschain *vars,int32_t flag){
	auto nameschain lclvars;                            /* nameschain for names in this instance */
	auto int32_t empty;                                 /* Empty formula indicator */
	auto char *ptr1;                                    /* Pointer to next formula chunk */
	auto char *ptr2;                                    /* Auxiliary pointer */
	auto nameschain *ptr3,*ptr4;                        /* Auxiliary pointers */
	auto int32_t ii,jj;                                 /* Auxiliary */

	/* Initialize and link local vars data as a first nameschain */
	/* element of a new nameschain level. */
	lclvars.name=NULL; /* No buffer allocated for name field so far. */
	lclvars.next=vars;
	lclvars.peer=NULL;
	lclvars.frstpeer=lclvars.lstpeer=&lclvars;

	/* If this is the root call then set scope of quantifiers. */
	if (vars==NULL){
		if (0!=(ii=TPTPQuantifiers(formula,size))){
			return(ii);
		}
	}

	/* Parsing loop. */
	ptr1=SkipBlanks(*formula);
	empty=EMPTYSENTENCE;
	while (ptr1[0]!=0){

		/* Indicate valid formula. */
		empty=VALIDSENTENCE;

		/* Jump over negations and quantifiers. */
		do {
			switch (ii=ParseSentenceStart(ptr1,&ptr1,&lclvars,flag)){
				case NOMEMORY:
				case -1:
					MYFREE(lclvars.name);
					for (ptr3=lclvars.peer;ptr3!=NULL;ptr3=ptr4){
						ptr4=ptr3->peer;
						MYFREE(ptr3->name);
						MYFREE(ptr3);
					}
					return(ii);
					break;
			}
		} while ((ii==NEGATION)||(ii==UNIVERSALQ)||(ii==EXISTENTIALQ));

		/* Process next chunk. */
		switch (ii){

			/* Open parenthesis. */
			case BRACKET:

				/* Check parenthesis content syntax. */
				jj=CheckSyntax(&ptr1,NULL,&ptr2,&lclvars,flag);

				/* Return if no memory or invalid syntax different than */
				/* generic syntax error. */
				/* Generic syntax errors are real errors only if next character */
				/* is not recognized later in this instance of CheckSyntax(). */
				if (((jj>=NOMEMORY)&&(jj!=SYNTAXERROR))||(jj==-1)){
					MYFREE(lclvars.name);
					for (ptr3=lclvars.peer;ptr3!=NULL;ptr3=ptr4){
						ptr4=ptr3->peer;
						MYFREE(ptr3->name);
						MYFREE(ptr3);
					}
					*result=ptr2;
					return(jj);
				}

				/* Missing right parenthesis. */
				if ((ptr2[0]!=')')&&(ii==BRACKET)){
					MYFREE(lclvars.name);
					for (ptr3=lclvars.peer;ptr3!=NULL;ptr3=ptr4){
						ptr4=ptr3->peer;
						MYFREE(ptr3->name);
						MYFREE(ptr3);
					}
					*result=ptr2;
					return(SYNTAXERROR);
				}

				/* Empty parenthesis block. */
				if (ptr1==ptr2){
					MYFREE(lclvars.name);
					for (ptr3=lclvars.peer;ptr3!=NULL;ptr3=ptr4){
						ptr4=ptr3->peer;
						MYFREE(ptr3->name);
						MYFREE(ptr3);
					}
					*result=ptr1;
					return(EMPTYBLOCK);
				}

				/* Adjust pointer. */
				ptr1=&ptr2[1];
				break;

			/* Valid atomic formula. */
			case VALIDATOMIC:
				break;

			/* Invalid atomic formula. Return the code */
			/* received from ParseSentenceStart(). */
			default:
				MYFREE(lclvars.name);
				for (ptr3=lclvars.peer;ptr3!=NULL;ptr3=ptr4){
					ptr4=ptr3->peer;
					MYFREE(ptr3->name);
					MYFREE(ptr3);
				}
				*result=ptr1;
				return(ii);
				break;
		}

		/* Next non space character should be a binary operator or null. */
		/* Return generic syntax error if not. */
		ptr1=SkipBlanks(ptr1);
		if ((ptr1[0]!=0)){
			if ((ptr1[0]!='&')&&(ptr1[0]!='|')&&(0!=strncmp(ptr1,"~&",2))
					&&(0!=strncmp(ptr1,"~|",2))&&(0!=strncmp(ptr1,"<=",2))
					&&(0!=strncmp(ptr1,"=>",2))&&(0!=strncmp(ptr1,"<~>",3))){
				MYFREE(lclvars.name);
				for (ptr3=lclvars.peer;ptr3!=NULL;ptr3=ptr4){
					ptr4=ptr3->peer;
					MYFREE(ptr3->name);
					MYFREE(ptr3);
				}
				*result=ptr1;
				return(SYNTAXERROR);
			}
		}

		/* Update formula pointer to next non space character before iterate. */
		switch (ptr1[0]){
			case '&':
			case '|':
				ptr1=SkipBlanks(&ptr1[1]);
				break;
			case '~':
			case '=':
				ptr1=SkipBlanks(&ptr1[2]);
				break;
			case '<':
				switch (ptr1[2]){
					case '>':
						ptr1=SkipBlanks(&ptr1[3]);
						break;
					default:
						ptr1=SkipBlanks(&ptr1[2]);
						break;
				}
				break;
		}
	}

	/* If this is the root call to CheckSyntax() and syntax was OK */
	/* then add symbols to global symbol list. */
	if ((vars==NULL)&&(empty==VALIDSENTENCE)){
		if (NOMEMORY==AddGlobalSymbols(&lclvars)){
			return(NOMEMORY);
		}
	}

	/* Free memory and return the empty indicator. */
	MYFREE(lclvars.name);
	for (ptr3=lclvars.peer;ptr3!=NULL;ptr3=ptr4){
		ptr4=ptr3->peer;
		MYFREE(ptr3->name);
		MYFREE(ptr3);
	}
	*result=ptr1;
	return(empty);
} /* CheckSyntax */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Skips spaces characters)  OCJ
 *
 *    This function returns a pointer to the first non space and non tab
 *    character in the argument.
 *
 *  ARGUMENTS:
 *
 *    string: Pointer to string.
 *
 *  RETURNS:
 *
 *    Pointer to the first non space and non tab character in string.
 *
 *--------------------------------------------------------------*/
char *SkipBlanks(char *string){
	auto int32_t ii;                                /* Auxiliary */

	for (ii=0;ii<strlen(string);ii++){
		if ((string[ii]!=' ')&&(string[ii]!='\t')){
			return(&string[ii]);
		}
	}
	return(&string[ii]);
} /* SkipBlanks */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Get formula start type)  OCJ
 *
 *    This function identifies the type of formula start.
 *
 *    If the start is a valid type the function returns a pointer to the next
 *    non space and non tab character after the identified start. If the
 *    start is not valid it returns a pointer the to first non space, non tab
 *    and not recognized character.
 *
 *    Initial spaces or tabs are skipped.
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer string with the formula.
 *    result: Pointer to next non space character that is not recognized.
 *    ltype: Pointer to the type of identified start.
 *    vars: Pointer to nameschain structure.
 *    flag: The following flags are used:
 *          NEGCONJ: formula is a negated conjecture
 *          FROMMAINFILE: formula comes from the main file and can be considered
 *                        a conjecture to the purposes of detecting contradictory
 *                        axioms.
 *
 *  RETURNS:
 *
 *    A parse function return type, syntax error code (see global.h), NOMEMORY
 *    or -1 if timeout.
 *
 *--------------------------------------------------------------*/
int32_t ParseSentenceStart(char *formula,char **result,nameschain *vars,int32_t flag){
	auto int32_t ii;                            /* Auxiliary */

	/* Check start. */
	formula=SkipBlanks(formula);
	switch (formula[0]){

		/* Negation */
		case '~':
			*result=SkipBlanks(&formula[1]);
			return(NEGATION);
			break;

		/* Open parenthesis */
		case '(':
			*result=SkipBlanks(&formula[1]);
			return(BRACKET);
			break;

		/* Universal quantifier. */
		case '!':

			/* If no error in variable list return valid universal quantifier. */
			ii=ParseVariableList(&formula[1],result,vars);
			if (ii==VALIDVARLIST){
				return(UNIVERSALQ);
			}

			/* Else return the error from parsing variable list. */
			return(ii);
			break;

		/* Existential quantifier. */
		case '?':

			/* If no error in variable list return valid existential quantifier. */
			ii=ParseVariableList(&formula[1],result,vars);
			if (ii==VALIDVARLIST){
				return(EXISTENTIALQ);
			}

			/* Else return the error from parsing variable list. */
			return(ii);
			break;
	}

	/* Atomic formula or unknown start. */
	return(ParseAtomic(formula,result,vars,flag));
} /* ParseSentenceStart */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Variable list parser)  OCJ
 *
 *    This function parses a variable list and adds the new variables
 *    detected to the names buffer. The buffer is allocated and/or
 *    reallocated if needed. If the variable list is invalid and
 *    the buffer has been allocated by the function then it will
 *    also be unlinked and deallocated by the function.
 *
 *    If a variable in the list is already declared in the first
 *    nameschain element then the variable is declared twice in quantifiers
 *    with the same scope and the variable list is invalid. See syntax
 *    comments in CheckSyntax() function. See AddNametoChain() function
 *    for a description of the nameschain buffer elements.
 *
 *  ARGUMENTS:
 *
 *    varlist: Pointer to string with a list of variables separated by
 *             commas. Spaces and tabs are ignored.
 *    result: Address of pointer to next non space and non tab character
 *            after a valid variable list or first non space, non tab and
 *            not recognized character for an invalid variable list.
 *    vars: Pointer to nameschain structure.
 *
 *  RETURNS:
 *
 *    A parse function return type, syntax error code or NOMEMORY (see global.h).
 *
 *--------------------------------------------------------------*/
int32_t ParseVariableList(char *varlist,char **result,nameschain *vars){
	auto char *ptr1;                            /* Parse pointer */
	auto char *ptr2;                            /* Auxiliary */
	auto char tempchar;                         /* Temporary character */
	auto int32_t ii,jj;                         /* Auxiliary */

	/* Check open square bracket and jump to first variable. */
	ptr1=SkipBlanks(varlist);
	if (ptr1[0]!='['){
		*result=ptr1;
		return(INVALIDVARLIST);
	}
	ptr1=SkipBlanks(&ptr1[1]);

	/* Parse loop. */
	while (1){

		/* Get the variable name. */
		ii=ParseName(ptr1,&ptr2);

		/* Check that the name has not been used for predicate or function. */
		tempchar=ptr2[0];
		ptr2[0]=0;
		if (CheckDeclared(ptr1,vars,0,NULL,&jj)){
			if (jj){
				ptr2[0]=tempchar; /* Reset temporary mark. */
				*result=ptr1;
				return(ALREADYDECLARED);
			}
		}
		ptr2[0]=tempchar;

		/* It is a valid variable name. */
		if ((ii==VALIDNAME)||(ii==STDVARNAME)){

			/* Temporary mark end of variable. */
			tempchar=ptr2[0];
			ptr2[0]=0;

			/* The variable is declared in the first nameschain element */
			/* so it is already declared at this level. Return error. */
			/* This is not just unnecessary, it is plainly wrong. Repeated */
			/* variables are allowed in TPTP syntax, just the appropriate */
			/* scope must be assigned to each one. The wrong code is left */
			/* commented out just as documentation. */
			/*if (CheckDeclared(ptr1,vars,1,NULL)){
				ptr2[0]=tempchar; //Reset temporary mark.
				*result=ptr1;
				return(ALREADYDECLARED);
			}*/

			/* The variable starts with a lower case and syntax is TPTP. */
			if ((syntaxmode==TPTP)&&(islower(ptr1[0]))){
				ptr2[0]=tempchar; /* Reset temporary mark. */
				*result=ptr1;
				return(INVTPTPVARNAME);
			}

			/* The variable starts with a single or double quote. */
			if ((ptr1[0]=='\'')||(ptr1[0]=='"')){
				ptr2[0]=tempchar; /* Reset temporary mark. */
				*result=ptr1;
				return(INVALIDNAME);
			}

			/* Add name to names chain. */
			if (NOMEMORY==AddNametoChain(ptr1,vars,-1,0)){
				return(NOMEMORY);
			}

			/* Reset temporary mark. */
			ptr2[0]=tempchar;

			/* If next non space character is not a comma then */
			/* there are no more variables. Check the closing square */
			/* bracket and the colon and leave the loop or return error */
			/* as appropriate. */
			ptr1=SkipBlanks(ptr2);
			if (ptr1[0]!=','){
				if (ptr1[0]!=']'){
					*result=ptr1;
					return(INVALIDVARLIST);
				}
				ptr1=SkipBlanks(&ptr1[1]);
				if (ptr1[0]!=':'){
					*result=ptr1;
					return(INVALIDVARLIST);
				}
				*result=SkipBlanks(&ptr1[1]);
				break;
			}

			/* Jump over the comma. */
			ptr1++;

		/* Not a variable. */
		} else {
			*result=ptr1;
			return(MISSINGVARINLIST);
			break;
		}

		/* Jump to next variable. */
		ptr1=SkipBlanks(ptr1);
	}

	return(VALIDVARLIST);
} /* ParseVariableList */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add name to text formulas names chain)  OCJ
 *
 *    This function adds a name and its related information to the names
 *    chain of the formula being parsed.
 *
 *    Variable names are added as the last peer nameschain structure of the
 *    nameschain structure passed as an argument. Predicate or function
 *    names are added as the last peer nameschain structure of the
 *    oldest nameschain structure in the chain.
 *
 *  ARGUMENTS:
 *
 *    name: Pointer to string with the name to add.
 *    vars: Pointer to nameschain structure. This should never be NULL.
 *          If this is NULL then the program will crash.
 *    data: Value of data field of nameschain structure.
 *    nameflgs: Value of flags field of nameschain structure.
 *
 *  RETURNS:
 *
 *    0 -> Function completed successfully.
 *    NOMEMORY -> No memory available.
 *
 *--------------------------------------------------------------*/
int32_t AddNametoChain(char *name,nameschain *vars,int32_t data,int32_t nameflgs){
	auto nameschain *new;                       /* New nameschain element */

	/* If name is a predicate or function name then adjust vars */
	/* to the last peer of oldest nameschain element in the chain. */
	if (nameflgs){
		while (vars->next!=NULL){
			vars=vars->next;
		}
		vars=vars->lstpeer;

	/* If variable name then adjust vars to the last peer of the */
	/* nameschain structure passed as an argument. */
	} else {
		vars=vars->frstpeer->lstpeer;
	}

	/* Allocate and link new nameschain element if needed. */
	if (vars->name!=NULL){
		if (NULL==(new=MYALLOC(sizeof(nameschain)))){
			return(NOMEMORY);
		}
		new->next=vars->next;
		new->frstpeer=vars->frstpeer;
		new->peer=NULL;
		vars->frstpeer->lstpeer->peer=new;
		vars->frstpeer->lstpeer=new;
	} else {
		new=vars;
	}

	/* Allocate buffer for name. This buffer will be freed */
	/* in one of the calling routines. */
	if (NULL==(new->name=MYALLOC(1+strlen(name)))){
		MYFREE(new);
		return(NOMEMORY);
	}

	/* Save name, flags and data fields of nameschain element. */
	strcpy(new->name,name);
	new->flags=nameflgs;
	new->data.arity=data;

	return(0);
} /* AddNametoChain */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Atomic formula parser)  OCJ
 *
 *    This function parses an atomic formula. If a valid atomic
 *    formula is detected the result pointer is adjusted to the
 *    first character not belonging to the atomic formula. If
 *    no valid atomic formula is detected the result pointer is
 *    set to the first unrecognized character.
 *
 *    The formula is assumed to start with a non space character.
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to string supposed to start with an atomic formula.
 *    result: Pointer to next non space and non tab character after a valid
 *            atomic formula. If no valid atomic formula then result
 *            will be set to first unrecognized character.
 *    vars: Pointer to nameschain structure.
 *    flag: The following flags are used:
 *          NEGCONJ: formula is a negated conjecture
 *          FROMMAINFILE: formula comes from the main file and can be considered
 *                        a conjecture to the purposes of detecting contradictory
 *                        axioms.
 *
 *  RETURNS:
 *
 *    A parse function return type, syntax error code (see global.h), NOMEMORY
 *    or -1 if timeout.
 *
 *--------------------------------------------------------------*/
int32_t ParseAtomic(char *formula,char **result,nameschain *vars,int32_t flag){
	auto char *ptr1,*ptr2;                      /* Auxiliary pointer */
	auto char tempchar;                         /* Temporary char */
	auto int32_t ii,jj;                         /* Auxiliary */

	/* Parse beginning of formula. */
	switch (ii=ParseTerm(formula,&ptr1,&ptr2,vars,&jj,flag)){
		case INVALIDTERM:
			*result=ptr1;
			return(INVALIDATOMIC);
			break;
		case -1: /* Timeout. */
		case NOMEMORY:
		case INVTPTPVARNAME:
		case INVTPTPNAME:
		case MISSINGARGS:
		case INVALIDDECLAREDARGS:
		case INVALIDDECLAREDTYPE:
		case TOOMANYARGS:
		case RESERVEDNAME:
			*result=ptr1;
			return(ii);
			break;
		default:
			break;
	}

	/* Equality or inequality. */
	if (((ptr1[0]=='=')&&(ptr1[1]!='>'))||((ptr1[0]=='!')&&(ptr1[1]=='='))){

		/* If the term was declared as a predicate the new declaration */
		/* is invalid. */
		if (ii==DECLAREDPRED){
			*result=ptr1;
			return(INVALIDDECLAREDTYPE);
		}

		/* If undeclared term add name to names chain as a function. */
		if (ii==UNDECLAREDTERM){
			tempchar=ptr2[0];
			ptr2[0]=0;
			if (NOMEMORY==AddNametoChain(formula,vars,jj,FUNCTION)){
				ptr2[0]=tempchar;
				return(NOMEMORY);
			}
			ptr2[0]=tempchar;
		}

		/* Parse second term and add name to names chain as a function */
		/* if it is undeclared. */
		ptr1=SkipBlanks(&ptr1[ptr1[0]=='='?1:2]);
		if (-1==(ii=ParseTerm(ptr1,result,&ptr2,vars,&jj,flag))){ /* Timeout. */
			return(-1);
		}

		/* If the term was declared as a predicate the new declaration */
		/* is invalid. */
		if (ii==DECLAREDPRED){
			return(INVALIDDECLAREDTYPE);
		}

		/* Add name to names chain as a function if it is undeclared. */
		if (ii==UNDECLAREDTERM){
			tempchar=ptr2[0];
			ptr2[0]=0;
			if (NOMEMORY==AddNametoChain(ptr1,vars,jj,FUNCTION)){
				ptr2[0]=tempchar;
				return(NOMEMORY);
			}
			ptr2[0]=tempchar;
		}

		/* Remaining checks for second term in equality. */
		switch (ii){
			case INVALIDTERM:
				return(INVALIDATOMIC);
				break;
			case NOMEMORY:
			case MISSINGARGS:
			case INVTPTPVARNAME:
			case INVTPTPNAME:
			case INVALIDDECLAREDARGS:
			case INVALIDDECLAREDTYPE:
			case RESERVEDNAME:
				return(ii);
				break;
			default:
				return(VALIDATOMIC);
				break;
		}
	}

	/* If we are here it is not an equality. */
	/* Check that the name is not a "distinct object". */
	if (formula[0]=='"'){
		*result=JumpOverQuotes(formula);
		return(INVALIDATOMIC);
	}

	/* If undeclared term add name to names chain as a predicate. */
	if (ii==UNDECLAREDTERM){
		tempchar=ptr2[0];
		ptr2[0]=0;
		if (NOMEMORY==AddNametoChain(formula,vars,jj,PREDICATE)){
			ptr2[0]=tempchar;
			return(NOMEMORY);
		}
		ptr2[0]=tempchar;
	}

	/* Check remaining cases. */
	*result=ptr1; /* Set result to returned value from first ParseTerm() above. */
	switch (ii){

		/* If it is a variable then it is an invalid atomic formula. */
		case INVALIDTERM:
		case DECLAREDVAR:
			return(INVALIDATOMIC);
			break;

		/* Declared functions cannot be used as predicates. */
		case DECLAREDFUNC:
			return(INVALIDDECLAREDTYPE);
			break;

		/* Remaining cases are valid atomic. */
		default:
			break;
	}

	return(VALIDATOMIC);
} /* ParseAtomic */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Parses a variable, constant, function or predicate name)  OCJ
 *
 *    This function parses a variable, constant, function or predicate
 *    name. If a valid name is detected the result pointer is adjusted to the
 *    first character not belonging to the name.
 *
 *    If invalid name is detected the result pointer is set to the beginning
 *    of formula. If a skolem function or standardized variable name is
 *    detected then the result pointer may be set to the first character
 *    not belonging to the name or to the beginning of formula, depending
 *    on the value of flag argument.
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to string supposed to start with
 *             a variable, constant or predicate name.
 *    result: Address of pointer to next character after a valid name (even
 *            if it is a space character). If invalid name then result will
 *            be set to start of formula.
 *
 *  RETURNS:
 *
 *    A parse function return type or syntax error code (see global.h).
 *
 *--------------------------------------------------------------*/
int32_t ParseName(char *formula,char **result){
	auto char auxstr[14];                       /* Auxiliary string */
	auto char *ptr1;                            /* Auxiliary string pointer */
	auto int32_t ii,jj;                         /* Auxiliary */

	/* Name in single or double quotes. */
	if ((formula[0]=='\'')||(formula[0]=='"')){
		if (NULL==(ptr1=JumpOverQuotes(formula))){
			*result=formula;
			return(INVALIDNAME);
		}
		*result=&ptr1[1];
		return(VALIDNAME);
	}

	/* If first character is not a letter then return invalid. */
	if (0==isalpha(formula[0])){
		*result=formula;
		return(INVALIDNAME);
	}

	/* Get index to first character not in the name. */
	for (ii=1;(isalnum(formula[ii])||(formula[ii]=='_'));ii++){
	}

	/* Check for reserved names. */
	if (ii<12){

		/* Check for standardized variable names. */
		strncpy(auxstr,formula,ii);
		auxstr[ii]=0;
		jj=strtol(&auxstr[1],&ptr1,10);
		if ((auxstr[0]=='X')&&(strlen(auxstr)>1)&&(jj<32768)&&(ptr1[0]==0)){
			*result=&formula[ii];
			return(STDVARNAME);
		}
	}

	/* Return a valid name. */
	*result=&formula[ii];
	return(VALIDNAME);
} /* ParseName */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Parse a term)  OCJ
 *
 *    This function parses a term. If a valid term is detected the
 *    result pointer is adjusted to the first character not belonging
 *    to the term. If invalid term is detected the result pointer
 *    is set to the first unrecognized character.
 *
 *    The term is assumed to start with a non space character.
 *
 *    If this function returns that a valid construction term has been
 *    detected it doesn't mean that it is really a term. This is because
 *    a predicate has the same syntax as some other constructions so the
 *    final meaning depends on the context, which needs to be considered
 *    in the calling function. For instance, a predicate without arguments
 *    has the same syntax as a constant. Another example: a predicate
 *    with arguments has the same syntax as a function.
 *
 *    It is assumed that mainkb.time structure contains a valid time with
 *    which current time can be compared to detect timeouts.
 *
 *    This function is called recursively.
 *
 *  ARGUMENTS:
 *
 *    term: Pointer to string.
 *    result: Pointer to next non space character after a valid term. If invalid
 *            term then result will be set to first unrecognized character.
 *    endofname: Pointer to first character not belonging to name of term.
 *    vars: Pointer to nameschain structure.
 *    arity: Pointer to number of arguments if term is a function or predicate.
 *    flag: The following flags are used:
 *          NEGCONJ: formula is a negated conjecture
 *          FROMMAINFILE: formula comes from the main file and can be considered
 *                        a conjecture to the purposes of detecting contradictory
 *                        axioms.
 *
 *  RETURNS:
 *
 *    A parse function return type, syntax error code (see global.h),
 *    NOMEMORY or -1 if timeout.
 *
 *--------------------------------------------------------------*/
int32_t ParseTerm(char *term,char **result,char **endofname,nameschain *vars,int32_t *arity,int32_t flag){
	auto char *ptr1,*ptr2,*ptr3;                /* Auxiliary pointers */
	auto hashchain *ptr4;                       /* Auxiliary pointer */
	auto int32_t declared;                      /* Declared variables flag */
	auto double time;                           /* Elapsed time in seconds */
	auto char tempchar;                         /* Temporary char */
	auto int32_t ii,jj,nn,kk,ff;                /* Auxiliary */

	/* Check timeout. */
	gettimeofday(&mainkb.endtime,NULL);
	time=mainkb.prstats.elapsed_time+mainkb.endtime.tv_sec-mainkb.time.tv_sec
			+(mainkb.endtime.tv_usec-mainkb.time.tv_usec)/1000000.0;
	if (((pgmmode==NONINTERACTIVE)&&(procctl->status==TIMEOUT))
			||((pgmmode!=NONINTERACTIVE)&&(time>=glblopts.timeout))){
		glblstats.elapsed_time+=time;
		return(-1);
	}

	/* Term starts with "$". */
	if (term[0]=='$'){

		/* Parse name and check if it is $false or $true. */
		ii=ParseName(&term[1],&ptr1);
		if (ii!=VALIDNAME){
			*result=term;
			return(INVALIDTERM);
		}
		if ((0!=strncmp("true",&term[1],ptr1-&term[1]))&&(0!=strncmp("false",&term[1],ptr1-&term[1]))){
			*result=ptr1;
			return(INVALIDTERM);
		}

		/* Return declared predicate. */
		*endofname=ptr1;
		*result=SkipBlanks(ptr1);
		return(DECLAREDPRED);

	/* Term doesn't start with "$". Parse name and get pointer to */
	/* next non space character. */
	} else {
		ii=ParseName(term,&ptr1);
		if ((ii!=VALIDNAME)&&(ii!=STDVARNAME)){
			*result=term;
			return(INVALIDTERM);
		}
		*endofname=ptr1;
		ptr2=SkipBlanks(ptr1);
	}

	/* Check if name has been previously declared */
	/* in the formula. */
	tempchar=ptr1[0];
	ptr1[0]=0;
	declared=CheckDeclared(term,vars,0,&nn,&ff);

	/* Declared variable name. */
	if (declared&&(0==ff)){
		ptr1[0]=tempchar;
		*result=ptr2;
		return(DECLAREDVAR);
	}

	/* If syntax is TPTP  and name starts with an upper case */
	/* then return error. */
	if ((syntaxmode==TPTP)&&(isupper(term[0]))){
		*result=term;
		return(INVTPTPNAME);
	}

	/* Check if name has been globally declared if necessary. */
	if (declared==0){
		if (NULL!=(ptr4=CheckGlblDeclared(term))){
			declared=1;
			nn=ptr4->arity;
			ff=(FUNCTION|PREDICATE)&ptr4->type;
		}
	}

	/* If name is undeclared and is a reserved variable name then */
	/* return error. */
	jj=strlen(term);
	kk=strtol(&term[1],&ptr3,10);
	if ((jj<7)&&(jj>1)&&(term[0]=='X')&&(kk<32768)&&(ptr3[0]==0)){
		ptr1[0]=tempchar;
		*result=ptr1;
		return(RESERVEDNAME);
	}
	ptr1[0]=tempchar;

	/* Function or predicate with arguments. */
	*arity=0; /* No arguments so far. */
	if (ptr2[0]==L'('){

		/* If it is a "distinct object" then this is not a valid term because */
		/* in this case arguments are not allowed. */
		if (term[0]=='"'){
			*result=ptr2;
			return(INVALIDTERM);
		}

		/* Loop through the arguments. */
		ptr3=ptr2=SkipBlanks(&ptr2[1]);
		ii=0; /* No new argument. */
		while (INVALIDTERM!=(jj=ParseTerm(ptr2,&ptr2,&ptr1,vars,&kk,flag))){

			/* Timeout. */
			if (jj==-1){
				*result=ptr2;
				return(-1);
			}

			/* Update argument counter and ii flag. */
			if ((*arity)==0x1fffffff){
				*result=ptr2;
				return(TOOMANYARGS);
			}
			(*arity)++;
			ii=1;

			/* Error cases. */
			if (jj==DECLAREDPRED){
				*result=ptr2;
				return(INVALIDDECLAREDTYPE);
			}
			if (jj>=NOMEMORY){
				*result=ptr2;
				return(jj);
			}

			/* Undeclared term. */
			if (jj==UNDECLAREDTERM){

				/* If syntax is TPTP and name starts with an upper case */
				/* then return error. */
				if ((syntaxmode==TPTP)&&(isupper(ptr2[0]))){
					*result=ptr2;
					return(INVTPTPNAME);
				}

				/* Add the name as a function name. */
				tempchar=ptr1[0];
				ptr1[0]=0;
				if (NOMEMORY==AddNametoChain(ptr3,vars,kk,FUNCTION)){
					ptr1[0]=tempchar;
					return(NOMEMORY);
				}
				ptr1[0]=tempchar;
			}

			/* End of list. */
			if (ptr2[0]!=','){
				break;
			}

			/* Get next argument. */
			ptr3=ptr2=SkipBlanks(&ptr2[1]);
			ii=0;
		}

		/* Missing closing parenthesis. */
		*result=ptr2;
		if (ptr2[0]!=')'){
			return(INVALIDTERM);
		}

		/* Missing arguments. */
		if (ii==0){
			return(MISSINGARGS);
		}
		*result=SkipBlanks(&ptr2[1]);

	/* Predicate without arguments or constant. If syntax is TPTP */
	/* and name starts with an upper case then return error, else */
	/* set result pointer. */
	} else {
		*result=ptr2;
	}

	/* Return as appropriate. */
	if (declared){
		if ((*arity)!=nn){
			return(INVALIDDECLAREDARGS);
		}
		if (ff&FUNCTION){
			return(DECLAREDFUNC);
		}
		return(DECLAREDPRED);
	}
	return(UNDECLAREDTERM);
} /* ParseTerm */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if name is locally declared)  OCJ
 *
 *    This function checks if a name has been previously declared
 *    in the formula being parsed.
 *
 *    See AddNametoChain() function for a description of the nameschain
 *    buffer elements.
 *
 *  ARGUMENTS:
 *
 *    name: Variable or symbol name to check.
 *    vars: Pointer to nameschain structure.
 *    flag: If 0 then nameschain elements of all levels will be explored.
 *          If 1 only the nameschain elements at same level as vars will
 *          be explored.
 *    arity: Pointer to arity of name for PREDICATE or FUNCTION names.
 *           Not used if NULL.
 *    nameflg: Pointer to integer to store flags field of nameschain
 *             structure of name.
 *
 *  RETURNS:
 *
 *    1 -> Name has been declared.
 *    0 -> Name is undeclared.
 *
 *--------------------------------------------------------------*/
int32_t CheckDeclared(char *name,nameschain *vars,int32_t flag,int32_t *arity,int32_t *nameflg){
	auto nameschain *chainlvl;								/* Pointer to first nameschain element of current level */
	auto nameschain *chainpeer;                             /* Pointer to peer nameschain element of current level */

	/* Loop through each nameschain level. */
	for (chainlvl=vars;chainlvl!=NULL;chainlvl=(flag?NULL:chainlvl->next)){

		/* Loop through each nameschain element at this level. */
		for (chainpeer=chainlvl;chainpeer!=NULL;chainpeer=chainpeer->peer){
			if ((chainpeer->name!=NULL)&&(0==strcmp(chainpeer->name,name))){
				*nameflg=chainpeer->flags;
				if (arity!=NULL){
					*arity=chainpeer->data.arity;
				}
				return(1);
			}
		}
	}

	/* Return undeclared. */
	return(0);
} /* CheckDeclared */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Include a negated conjecture in main KB)  OCJ
 *
 *    This function includes an input conjecture and its negation
 *    in the main KB. So if thereafter the KB is proven unsatisfiable then
 *    the negated conjecture is refuted and therefore the conjecture
 *    is proven. The end of the formula must be indicated by a period.
 *    This function will read additional lines if needed until an end of
 *    formula period is found.
 *
 *    This function calls Tell() function for the formula and its
 *    negation in order to include both formulas in the main KB.
 *
 *    The conjecture is assigned as parent of the negated conjecture.
 *
 *    It is assumed that mainkb.time structure contains a valid time with
 *    which current time can be compared to detect timeouts.
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to string with the initial input of the formula
 *             content.
 *
 *  RETURNS:
 *
 *    0--> the formula or its negation was not added.
 *    1--> the formula and its negation were successfully added.
 *    NOMEMORY--> not enough memory.
 *    TIMEOUT--> timeout and the formula or its negation was not added.
 *
 *--------------------------------------------------------------*/
int32_t Ask(char *formula){
	auto char *negsntnc;                             /* Negated formula */
	auto int32_t size;                               /* Size of negated formula buffer */
	auto cmprefix *ptr1;                             /* Auxiliary pointer */
	auto int32_t ii;                                 /* Auxiliary */

	/* Allocate memory. */
	size=4+strlen(formula)+INPUT_CHUNK_SIZE;
	if (NULL==(negsntnc=MYALLOC(size))){
		if (pgmmode==NONINTERACTIVE){
			printf("%% SZS status MemoryOut : ");
		}
		printf("No memory available\n");
		return(NOMEMORY);
	}

	/* Input remaining formula lines. */
	strcpy(negsntnc,formula);
	if (NOMEMORY==InputSentence(&negsntnc,&size)){
		if (pgmmode==NONINTERACTIVE){
			printf("%% SZS status MemoryOut : ");
		}
		printf("Not enough memory available.\n");
		MYFREE(negsntnc);
		return(NOMEMORY);
	}
	if ((negsntnc[0]==' ')&&(negsntnc[1]==0)){
		if (pgmmode==INTERACTIVE){
			printf("Empty formula, nothing to prove.\n");
		}
		MYFREE(negsntnc);
		return(0);
	}

	/* Call Tell() function to check syntax and add formula */
	/* to main KB. */
	strcat(negsntnc,".");
	if (1!=(ii=Tell(negsntnc,CONJECTURE,0))){
		MYFREE(negsntnc);
		return(ii);
	}
	ptr1=mainkb.lasttxt;
	ptr1->inference=CONJECTURE;

	/* Build negated formula. */
	memmove(&negsntnc[2],negsntnc,1+strlen(negsntnc));
	memcpy(negsntnc,"~(",2);
	strcpy(&negsntnc[strlen(negsntnc)-1],").");

	/* Call Tell() function to check syntax and add negated */
	/* formula to main KB. */
	if ((verbose==2)&&(pgmmode==INTERACTIVE)){
		printf("Checking negated conjecture...\n");
	}
	if (1==(ii=Tell(negsntnc,NEGCONJ,NEGCONJ|FROMMAINFILE))){
		pbmflags|=EXISTCONJ;
		mainkb.lasttxt->inference=NEGCONJ;
		mainkb.lasttxt->parent1=ptr1;
		mainkb.lasttxt->parent2=NULL;
	}

	/* Free memory and return. */
	MYFREE(negsntnc);
	return(ii);
} /* Ask */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add text formula to a KB)  OCJ
 *
 *    This function adds a text formula to the knowledge base
 *    passed as an argument.
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to cmprefix structure of formula.
 *    inference: Inference that produced the formula. See
 *               defines in global.h.
 *    pkb: Pointer to KB being where the clause is to be added.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *--------------------------------------------------------------*/
void AddTxtFormula2KB(cmprefix *formula,int32_t inference,kbase *pkb){

	/* Add text formula to KB. */
	formula->prev=pkb->lasttxt;
	formula->next=NULL;
	pkb->lasttxt=formula;
	if (pkb->firsttxt==NULL){
		pkb->firsttxt=formula;
	}
	if (formula->prev!=NULL){
		formula->prev->next=formula;
	}
	formula->flags&=(~(ACTIVE|PASSIVE|UNPROC|LOCKED));
	formula->flags|=TEXTFORMULA;
	formula->inference=inference;
	if (0==(NUMBERED&formula->flags)){
		pkb->nformulas++;
		formula->number=pkb->nformulas;
		formula->flags|=NUMBERED;
	}
	return;
} /* AddTxtFormula2KB */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Reset statistics structure)  OCJ
 *
 *    This function resets the data of a statistics structure.
 *
 *
 *  ARGUMENTS:
 *
 *    stts: Pointer to statistics structure.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *--------------------------------------------------------------*/
void ResetStats(stats *stts){
	stts->elapsed_time=0.0;
	stts->removedups=0;
	stts->triveqres=0;
	stts->desteqres=0;
	stts->factors=0;
	stts->fwdsubsum=0;
	stts->bcksubsum=0;
	stts->resolutions=0;
	stts->paramodulations=0;
	stts->splits=0;
	stts->equresolutions=0;
	stts->equfactorings=0;
	stts->fwdemodulations=0;
	stts->bkdemodulations=0;
	stts->fwsubsresltns=0;
	stts->bksubsresltns=0;
	stts->tautologies=0;
	stts->eqsplits=0;
	#ifdef SEMANTICTAUTOLOGY
	stts->semtautologies=0;
	#endif
	stts->prennfxform=0;
	stts->nnfxform=0;
	stts->miniscoping=0;
	stts->skolemize=0;
	stts->predicatedef=0;
	stts->formularenaming=0;
	stts->cnfconversion=0;
	stts->tfsimplif=0;
	stts->sattautologies=0;
	stts->satremovedups=0;
	stts->satsubsum=0;
	stts->nbactive=stts->nbpassive=stts->nbunproc=stts->nbgrjnbl=0;
	stts->memory=0;
	stts->netmemory=0;
	return;
} /* ResetStats */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add inference statistics)  OCJ
 *
 *    This function adds the inference statistics of a stats structure
 *    to another stats structure. Statistics not related to inferences
 *    are not added.
 *
 *
 *  ARGUMENTS:
 *
 *    stfrom: Pointer to statistics structure of statistics to be added.
 *    stto: Pointer to statistics structure to which statistics will be
 *          added.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *--------------------------------------------------------------*/
void AddInferenceStats(stats *stfrom,stats *stto){
	stto->fwdemodulations+=stfrom->fwdemodulations;
	stto->bkdemodulations+=stfrom->bkdemodulations;
	stto->fwsubsresltns+=stfrom->fwsubsresltns;
	stto->bksubsresltns+=stfrom->bksubsresltns;
	stto->factors+=stfrom->factors;
	stto->fwdsubsum+=stfrom->fwdsubsum;
	stto->bcksubsum+=stfrom->bcksubsum;
	stto->triveqres+=stfrom->triveqres;
	stto->desteqres+=stfrom->desteqres;
	stto->resolutions+=stfrom->resolutions;
	stto->paramodulations+=stfrom->paramodulations;
	stto->splits+=stfrom->splits;
	stto->tautologies+=stfrom->tautologies;
	#ifdef SEMANTICTAUTOLOGY
	stto->semtautologies+=stfrom->semtautologies;
	#endif
	stto->removedups+=stfrom->removedups;
	stto->equresolutions+=stfrom->equresolutions;
	stto->equfactorings+=stfrom->equfactorings;
	stto->eqsplits+=stfrom->eqsplits;
	stto->prennfxform+=stfrom->prennfxform;
	stto->nnfxform+=stfrom->nnfxform;
	stto->miniscoping+=stfrom->miniscoping;
	stto->skolemize+=stfrom->skolemize;
	stto->predicatedef+=stfrom->predicatedef;
	stto->formularenaming+=stfrom->formularenaming;
	stto->cnfconversion+=stfrom->cnfconversion;
	stto->tfsimplif+=stfrom->tfsimplif;
	stto->sattautologies+=stfrom->sattautologies;
	stto->satremovedups+=stfrom->satremovedups;
	stto->satsubsum+=stfrom->satsubsum;
	return;
} /* AddInferenceStats */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Set scope of quantifiers)  OCJ
 *
 *    This function sets the scope of quantifiers to the sub-formula
 *    immediately following the quantifier. This sub-formula is the
 *    minimum piece that can be interpreted as a formula an can be
 *    a single literal. This is done because in TPTP syntax the
 *    quantifiers have higher precedence than binary connectives.
 *
 *    To set the appropriate scope this function scans the formula for
 *    start of quantifiers, that is, characters "!" or "?". When it
 *    detects one of these an opening parenthesis is inserted and
 *    a closing parenthesis is inserted when one of the following
 *    is detected:
 *    - the first occurrence of a binary connective that happens with
 *      no unbalanced parenthesis for the parenthesis set following
 *      the quantifier.
 *    - first unbalanced closing parenthesis for the parenthesis set
 *      following the quantifier.
 *
 *    This function assumes that the input formula is correct except
 *    for items between single quotes. Any additional syntax errors
 *    must be detected afterwards.
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Address of pointer to text formula.
 *    size: Pointer to size in bytes of formula buffer.
 *
 *  RETURNS:
 *
 *    NOMEMORY, SYNTAXERROR or 0 if success.
 *
 *
 *--------------------------------------------------------------*/
int32_t TPTPQuantifiers(char **formula,int32_t *size){
	auto int32_t length;                                 /* Length of formula */
	auto char *ptr1;                                     /* Auxiliary pointer */
	auto int ii,jj,kk,pp;                                /* Auxiliary */

	/* Scan formula. */
	length=strlen(*formula);
	for (ii=0;ii<length;ii++){

		/* Jump over segments in single or double quotes. */
		if (((*formula)[ii]=='\'')||((*formula)[ii]=='"')){
			if (NULL==(ptr1=JumpOverQuotes(&((*formula)[ii])))){
				return(SYNTAXERROR);
			}
			if (ptr1[1]==0){
				break;
			}
			ii=&ptr1[1]-(*formula);
		}

		/* Start of quantifier. */
		if (((*formula)[ii]=='?')||(((*formula)[ii]=='!')&&((*formula)[ii+1]!='='))){

			/* Insert opening parenthesis. */
			if ((*size)==(length+1)){
				*size+=INPUT_CHUNK_SIZE;
				if (NULL==(ptr1=MYREALLOC(*formula,*size))){
					return(NOMEMORY);
				}
				*formula=ptr1;
			}
			memmove(&(*formula)[ii+1],&(*formula)[ii],length-ii+1);
			(*formula)[ii]='(';
			length++;

			/* Scan for position to insert closing parenthesis. Care must be taken */
			/* to jump over over segments in single or double quotes. */
			for (jj=ii+1,kk=pp=0;(jj<=length)&&(kk==0);jj++){

				/* Set kk=1 if this is the insertion point. */
				switch ((*formula)[jj]){
					case '\'':
					case '"':
						if (NULL==(ptr1=JumpOverQuotes(&((*formula)[jj])))){
							return(SYNTAXERROR);
						}
						jj=ptr1-(*formula);
						continue;
						break;
					case '~':
						if ((pp==0)&&(((*formula)[jj+1]=='&')||((*formula)[jj+1]=='|'))){
							kk=1;
						}
						break;
					case '=':
						if ((pp==0)&&(0==strncmp(&(*formula)[jj],"=>",2))){
							kk=1;
						}
						break;
					case '<':
					case '|':
					case '&':
					case 0:
						if (pp==0){
							kk=1;
						}
						break;
					case ')':
						if (pp==0){
							kk=1;
						} else {
							pp--;
						}
						break;
					case '(':
						pp++;
						break;
				}

				/* Insertion point has been detected. */
				if (kk){
					if ((*size)==(length+1)){
						*size+=INPUT_CHUNK_SIZE;
						if (NULL==(ptr1=MYREALLOC(*formula,*size))){
							return(NOMEMORY);
						}
						*formula=ptr1;
					}
					memmove(&(*formula)[jj+1],&(*formula)[jj],length-jj+1);
					(*formula)[jj]=')';
					length++;
				}
			}

			/* Prepare next loop iteration. */
			ii++;
		}
	}

	return(0);
} /* TPTPQuantifiers */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Format memory in KB, MB and GB)  OCJ
 *
 *    This function converts a number of bytes in kilobytes,
 *    megabytes or gigabytes and stores the appropriate
 *    units in a string.
 *
 *  ARGUMENTS:
 *
 *    memory: Number of bytes to be converted.
 *    string: Pointer to characters with the appropriate units.
 *            (output).
 *
 *  RETURNS:
 *
 *    Memory converted to the appropriate units.
 *
 *
 *--------------------------------------------------------------*/
float FormatMemory(float memory,char **string){
	auto float dd;                                       /* Auxiliary */

	/* Perform conversion. */
	if (memory>=1000000000.0){
		dd=memory/1000000000.0;
		*string="GB";
	} else if (memory>=1000000.0){
		dd=memory/1000000.0;
		*string="MB";
	} else {
		dd=memory/1000.0;
		*string="KB";
	}
	return(dd);
} /* FormatMemory */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Jump over a quoted block)  OCJ
 *
 *    This function jumps over a single or double quoted block
 *    There may be escape characters for quotes inside the block.
 *    For instance, '\'' is a valid predicate or function name
 *    and "\"" is a valid "distinct object" name.
 *
 *  ARGUMENTS:
 *
 *   block: pointer to beginning of block (a single or double
 *          quote character).
 *
 *  RETURNS:
 *
 *    Pointer to end of block (a single or double quote character)
 *    or NULL if no end of block is found.
 *
 *
 *--------------------------------------------------------------*/
char *JumpOverQuotes(char *block){
	auto char *ptr1;                                     /* Auxiliary */
	auto char ii;                                        /* Auxiliary */

	/* Search for end of block and return. */
	ptr1=block;
	ii=block[0];
	do {
		ptr1=strchr(&(ptr1[1]),ii);
	} while ((ptr1!=NULL)&&(*(ptr1-1)=='\\'));
	return(ptr1);
} /* JumpOverQuotes */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Catch program exception signals)  OCJ
 *
 *    This function catches some program exception signals and
 *    if enabled it also creates the file ~/drodi.log with
 *    information of the failure.
 *
 *  ARGUMENTS:
 *
 *    sig: Signal number
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *--------------------------------------------------------------*/
void CatchSignal1(int sig){
	auto char *signalname;                               /* Signal name */
	auto struct sigaction act;                           /* Auxiliary for signals handling */
	auto char *ptr1;                                     /* Auxiliary pointer */
	auto int32_t ii,jj;                                  /* Auxiliary */
	auto FILE *filehandle;                               /* File handle for signal log file */
	auto char string[200];                               /* Auxiliary string for log file management */
	auto char *logname;                                  /* Pointer to log file name */
	auto int32_t log;                                    /* Not zero if interruption must be logged */
	auto struct timespec wtime;                          /* To specify wait time for children */
	auto struct itimerval alarmtime;                     /* To disable the global timeout alarm */

	/* Another signal process is in progress. */
	if (signalflag){
		raise(sig);
	}

	/* Process programmed timer. */
	if ((sig==SIGALRM)&&(alrmstatus==0)){
		alrmstatus=1;
		alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
		alarmtime.it_value.tv_sec=EXITWAITSECONDS;
		alarmtime.it_value.tv_usec=TMOWAITMICROSECS;
		procctl->status=TIMEOUT;
		setitimer(ITIMER_REAL,&alarmtime,NULL);
		return;
	}

	/* Disable alarm for global timeouts detection to prevent */
	/* interference with external interruptions. */
	alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
	alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
	setitimer(ITIMER_REAL,&alarmtime,NULL);

	/* If signal is SIGALRM, mode is interactive and alrmflg is zero */
	/* then report received signal and return. */
	if ((sig==SIGALRM)&&(pgmmode==INTERACTIVE)&&(alrmstatus==1)){
		alrmstatus=2;
		printf("Timeout or SIGALRM received but the program is busy so it will be ignored this time\n");
		return;
	}

	/* Parent process. */
	if (procnb==(active_cores-1)){

		/* Kill child processes if appropriate. */
		if ((pids!=NULL)&&(pids[0]!=0)){
			jj=active_cores-1;
			for (ii=0;ii<jj;ii++){
				kill(pids[ii],SIGTERM);
			}
		}

	/* Child process. Clear process specific semaphore. */
	} else {
		sem_post(&procsem[procnb]);
	}

	/* Indicate that a signal is in process and get signal name. */
	signalflag=1;
	switch (sig){
		case SIGFPE:
			log=1;
			signalname="SIGFPE";
			break;
		case SIGILL:
			log=1;
			signalname="SIGILL";
			break;
		case SIGSEGV:
			log=1;
			signalname="SIGSEGV";
			break;
		case SIGBUS:
			log=1;
			signalname="SIGBUS";
			break;
		default:
		case SIGABRT:
			log=1;
			signalname="SIGABRT/SIGIOT";
			break;
		/*case SIGTRAP:
			log=1;
			signalname="SIGTRAP";
			break;*/
		case SIGSYS:
			log=1;
			signalname="SIGSYS";
			break;
		case SIGTERM:
			log=0;
			signalname="SIGTERM";
			break;
		case SIGINT:
			log=0;
			signalname="SIGINT";
			break;
		case SIGALRM:
			log=0;
			signalname="SIGALRM";
			break;
		case SIGXCPU:
			log=0;
			signalname="SIGXCPU";
			break;
		case SIGQUIT:
			log=0;
			signalname="SIGQUIT";
			break;
		case SIGHUP:
			log=0;
			signalname="SIGHUP";
			break;
	}

	/* Report exception to log. */
	if ((log!=0)&&(logpath!=NULL)){
		if (logpath[0]==0){
			filehandle=stdout;
		} else {
			if (importfile!=NULL){
				if (NULL==(ptr1=strrchr(importfile,WPATHSEPARATOR))){
					ptr1=importfile;
				} else {
					ptr1++;
				}
				if (NULL==(logname=MYALLOC(strlen(logpath)+11+strlen(ptr1)))){
					filehandle=stdout;
				} else {
					strcpy(logname,logpath);
					ii=strlen(logname);
					if (logname[ii-1]!=WPATHSEPARATOR){
						logname[ii]=WPATHSEPARATOR;
						logname[ii+1]=0;
					}
					sprintf(&string[0],"%s_%d.log",ptr1,getpid());
					strcat(logname,&string[0]);
					if (NULL==(filehandle=fopen(logname,"w"))){
						filehandle=stdout;
					}
					MYFREE(logname);
				}
			} else {
				filehandle=stdout;
			}
		}
		if (procnb==(active_cores-1)){
			fprintf(filehandle,"Signal %s while processing problem number %d, pid=%d\n",
					signalname,tot_pbms+1,getpid());
		} else {
			fprintf(filehandle,"Signal %s while processing problem number %d, child=%d pid=%d\n",
					signalname,tot_pbms+1,procnb,getpid());
		}
		glblopts=kbset.opts;
		usrparam_mask=ALGMASK|FUNCWMASK|GAMMAMASK|LITORDRMASK|TERMORDRMASK|PRCMASK|ISPRCMASK;
		usrparam_mask|=(MAXWMASK|SPLITMASK|CONNECTMASK|GRJOINMASK|FACTORINGMASK|LOOKAHEADMASK);
		usrparam_mask|=(SELECTMASK|WEAKRWMASK|DEMODMASK|LAYERMASK);
		PrintStrategy();
		if (filehandle!=stdout){
			fclose(filehandle);
		} else {
			fprintf(filehandle,"\n");
		}
	}

	/* Report interruption as appropriate. */
	if (procnb==(active_cores-1)){
		if (importfile!=NULL){
			if (NULL==(ptr1=strrchr(importfile,WPATHSEPARATOR))){
				ptr1=importfile;
			} else {
				ptr1++;
			}
		} else {
			ptr1="user input";
		}
		switch(sig){
			case SIGALRM:
			case SIGXCPU:
				if (pgmmode==NONINTERACTIVE){
					printf("%% SZS status Timeout for %s: ",ptr1);
				}
				printf("Process timeout. ");
				if (sig==SIGALRM){
					//printf("Wall clock time limit exceeded (SIGALRM)\n");
					printf("\n");
				} else {
					printf("CPU time limit exceeded (SIGXCPU)\n");
				}
				break;
			default:
				if (pgmmode==NONINTERACTIVE){
					printf("\n%% SZS status Unknown for %s: %s\n",ptr1,signalname);
				} else {
					printf("\nProgram aborted by signal %s\n",signalname);
				}
				break;
		}
	}
	fflush(stdout);

	/* Parent process. Wait for child processes if appropriate. */
	if (procnb==(active_cores-1)){
		if ((pids!=NULL)&&(pids[0]!=0)){
			jj=active_cores-1;
			wtime.tv_sec=0;
			wtime.tv_nsec=KILLWAITNANOSECS2;
			nanosleep(&wtime,NULL);
			for (ii=0;ii<jj;ii++){
				waitpid(pids[ii],NULL,WNOHANG);
			}
		}
	}

	/* Remove common mmap memory. */
	if (mmapptr!=NULL){
		munmap(mmapptr,mmapsize);
	}

	/* Reactivate the signal’s default handling and re-raise the signal. */
	sigemptyset(&act.sa_mask);
	act.sa_handler=SIG_DFL;
	act.sa_flags=0;
	sigaction(sig,&act,NULL);
	raise(sig);

	return;
} /* CatchSignal1 */

#if (MEMCHECK == 1) || (MEMCHECK == 2)
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Memory tracking version of strdup() function)  OCJ
 *
 *    This function is the memory tracking version of strdup() function.
 *
 *  ARGUMENTS:
 *
 *    string: Pointer to string to be duplicated.
 *
 *  RETURNS:
 *
 *    Pointer to new allocated string or NULL if not enough memory.
 *
 *
 *--------------------------------------------------------------*/
char *strdupmn(char *string){
	auto char *newstring;                                /* New allocated string */

	/* Allocate memory for string and copy it. */
	if (NULL==(newstring=mallocmn(strlen(string)+1))){
		return(NULL);
	}
	strcpy(newstring,string);
	return(newstring);
} /* strdupmn */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Allocate and track total memory)  OCJ
 *
 *    This function allocates and tracks total  memory. It allocates
 *    an uitn64_t integer space in addition to the requested memory
 *    to store the total size of the memory allocated.
 *
 *  ARGUMENTS:
 *
 *    size: Size of block requested by the user.
 *
 *  RETURNS:
 *
 *    Pointer to allocated memory or NULL if not enough memory.
 *
 *
 *--------------------------------------------------------------*/
void *mallocmn(size_t size){
	auto uint64_t *allocdmem;                            /* Real pointer to allocated memory */

	/* Allocate memory, set its size field and update total allocated memory. */
	size+=sizeof(uint64_t);
	if (NULL==(allocdmem=malloc(size))){
		return(NULL);
	}
	*allocdmem=size;
	allcdmemory+=size;
	return((void *)(((char *)allocdmem)+sizeof(uint64_t)));
} /* mallocmn */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Reallocate and track total memory)  OCJ
 *
 *    This function reallocates and tracks total  memory. It reallocates
 *    an uitn64_t integer space in addition to the requested memory
 *    to store the total size of the memory reallocated.
 *
 *  ARGUMENTS:
 *
 *    addr: Pointer to original allocated memory.
 *    size: New size of block requested by the user.
 *
 *  RETURNS:
 *
 *    Pointer to reallocated memory or NULL if not enough memory.
 *
 *
 *--------------------------------------------------------------*/
void *reallocmn(void *addr,size_t size){
	auto uint64_t orgsize;                               /* Original size of allocated memory */
	auto uint64_t *allocdmem;                            /* Real pointer to reallocated memory */

	/* Allocate memory, set its size field and update total allocated memory. */
	orgsize=*((uint64_t *)(((char *)addr)-sizeof(uint64_t)));
	size+=sizeof(uint64_t);
	if (NULL==(allocdmem=realloc(((char *)addr)-sizeof(uint64_t),size))){
		return(NULL);
	}
	allcdmemory+=(size-orgsize);
	*allocdmem=size;
	return((void *)(((char *)allocdmem)+sizeof(uint64_t)));
} /* reallocmn */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Free and track total memory)  OCJ
 *
 *    This function frees and tracks total  memory. See allocmn()
 *    function for additional details.
 *
 *  ARGUMENTS:
 *
 *    addr: Pointer to allocated memory.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *--------------------------------------------------------------*/
void freemn(void *addr){
	auto int64_t size;                                   /* Size of allocated memory */

	/* Free memory and update total allocated memory. */
	if (addr==NULL){
		return;
	}
	size=*((uint64_t *)(((char *)addr)-sizeof(uint64_t)));
	free(((char *)addr)-sizeof(uint64_t));
	allcdmemory-=size;
	return;
} /* myfree */
#endif

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Get available memory)  OCJ
 *
 *    This function gets the maximum memory to be dynamically
 *    allocated. First an attempt is made to get available memory
 *    from /proc/meminfo. If this is not successful then the
 *    sysconf() function is used.
 *
 *    The available memory is computed as follows:
 *
 *    - If memory available can be obtained from /proc/meminfo
 *      (MemAvailable token) then it is reported as available memory
 *      after applying MEMUSAGE1 security factor.
 *
 *    - If memory available cannot be obtained from /proc/meminfo then
 *      first the physical memory is obtained using sysconf() calls.
 *
 *      If physical memory is greater than MEMORYLIMIT2 and this is
 *      the first call to GetAvailableMemory() (which is detected
 *      because intavmemory is zero) then the reported available
 *      memory is the physical memory after applying MEMUSAGE2
 *      security factor.
 *
 *      If physical memory is NOT greater than MEMORYLIMIT2 and this is
 *      the first call to GetAvailableMemory() then the reported
 *      available memory is the physical memory minus MEMORYMARGIN
 *      or zero if physical memory is lesser than MEMORYMARGIN.
 *
 *      If this is NOT the first call to GetAvailableMemory() then the
 *      reported available memory is initavmemory minus the memory
 *      currently in use computed as arena+hblkhd-keepcost members of
 *      mallinfo2 structure obtained using the mallinfo2() call.
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
uint64_t GetAvailableMemory(void){
	auto FILE *memfile;                                  /* Input an output file handles */
	auto uint64_t avmemory;                              /* Available memory to be returned */
	auto char line[256];                                 /* Line read from /proc/meminfo */
	auto char *ltkn;                                     /* Pointer to token */
	auto struct mallinfo2 rsinfo;                        /* For mallinfo2() calls */
	auto char *ptr1;                                     /* Auxiliary */
	auto int32_t ii,jj;                                  /* Auxiliary */

	/* Try to get available memory from /proc/meminfo. */
	ii=1;
	avmemory=0;
	if (NULL!=(memfile=fopen("/proc/meminfo","r"))){

		/*  Loop to read lines from /proc/meminfo. */
		while (1){

			/* Read lines from /proc/meminfo. */
			if (NULL==fgets(line,sizeof(line),memfile)){
				break;
			}
			if (line[strlen(line)-1]!='\n'){
				break;
			}
			line[strlen(line)-1]=0;

			/* Parse first token and convert to upper case. */
			if (NULL==(ltkn=strtok(line," \t"))){
				continue;
			}
			for (jj=0;jj<strlen(ltkn);jj++){
				ltkn[jj]=toupper(ltkn[jj]);
			}

			/* MemAvailable line. Extract amount of available memory. */
			if (0==strcmp(ltkn,"MEMAVAILABLE:")){
				if (NULL!=(ltkn=strtok(NULL," \t"))){
					avmemory=strtoul(ltkn,&ptr1,10);
					if ((avmemory>0)&&(ptr1[0]==0)){
						if (NULL!=(ltkn=strtok(NULL," \t"))){
							for (jj=0;jj<strlen(ltkn);jj++){
								ltkn[jj]=toupper(ltkn[jj]);
							}
							if (0==strcmp(ltkn,"KB")){
								avmemory*=1000;
							} else if (0==strcmp(ltkn,"MB")){
								avmemory*=1000000;
							} else if (0==strcmp(ltkn,"GB")){
								avmemory*=1000000000;
							} else if (0!=strcmp(ltkn,"B")){
								break;
							}
							avmemory=(avmemory*MEMUSAGE1)-mmapsize;
							ii=0;
							break;
						} else {
							break;
						}
					} else {
						break;
					}
				} else {
					break;
				}
			}
		}
		fclose(memfile);
	}

	/* Available memory could not be obtained from /proc/meminfo. */
	/* Get available memory from sysconf() calls. */
	if (ii){
		if (initavmemory==0){
			avmemory=(sysconf(_SC_PAGESIZE)*sysconf(_SC_AVPHYS_PAGES))-mmapsize;
			if (avmemory>=MEMORYLIMIT2){
				avmemory*=MEMUSAGE2;
			} else if (avmemory>MEMORYMARGIN){
				avmemory-=MEMORYMARGIN;
			} else {
				avmemory=0;
			}
		} else {
			rsinfo=mallinfo2();
			avmemory=initavmemory+rsinfo.keepcost-rsinfo.arena-rsinfo.hblkhd;
		}
	}

	return(avmemory);
} /* GetAvailableMemory */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Initialize mmap common memory area)  OCJ
 *
 *    This function initializes common memory area to be shared by
 *    all processes using mmap() function. It also initializes the
 *    size of mmap area and the pointers needed to make use of that
 *    memory.
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
int32_t Init_mmap(void){
	auto uint64_t ii;                                    /* Auxiliary */

	/* Allocate mmap memory. */
	#ifdef BENCHMARKING
	mmapsize=sizeof(*trvsatstr)+sizeof(*trvunsstr)+sizeof(*procctl)+sizeof(*pbmstats)
			+sizeof(*nextstrat)+sizeof(*maxprcmemory)+sizeof(*secprc_cpu_time)
			+((num_cores-1)*sizeof(*procsem))+(BENCHDIMENSION*3*sizeof(*bresults[0]))
			+sizeof(*bncounter);
	#else
	mmapsize=sizeof(*trvsatstr)+sizeof(*trvunsstr)+sizeof(*procctl)+sizeof(*pbmstats)
			+sizeof(*nextstrat)+sizeof(*maxprcmemory)+sizeof(*secprc_cpu_time)
			+((num_cores-1)*sizeof(*procsem));
	#endif
	ii=sysconf(_SC_PAGESIZE);
	mmapsize=ii*((mmapsize/ii)+((mmapsize%ii)?1:0));
	if (MAP_FAILED==(mmapptr=mmap(NULL,mmapsize,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0))){
		mmapptr=NULL;
		return(NOMEMORY);
	}

	/* Set pointers to mmap memory. */
	trvsatstr=mmapptr;
	trvunsstr=&trvsatstr[1];
	procctl=(mpctl *)&trvunsstr[1];
	pbmstats=(stats *)&procctl[1];
	nextstrat=(int32_t *)&pbmstats[1];
	maxprcmemory=(uint64_t *)&nextstrat[1];
	secprc_cpu_time=(double *)&maxprcmemory[1];
	procsem=(sem_t *)&secprc_cpu_time[1];
	#ifdef BENCHMARKING
	bresults[0]=(double *)&procsem[num_cores-1];
	bresults[1]=&bresults[0][BENCHDIMENSION];
	bresults[2]=&bresults[1][BENCHDIMENSION];
	bncounter=(int32_t *)&bresults[2][BENCHDIMENSION];
	#endif

	/* Initialize problem statistics and return. */
	ResetStats(pbmstats);
	return(0);
} /* Init_mmap */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform performance benchmarking and display results)  OCJ
 *
 *    This function performs performance benchmarking and displays
 *    the results. It is currently under development.
 *
 *    IMPORTANT: This function must not be optimized.
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
void __attribute__((optimize("O0"))) GetPerfData(void){
	auto clock_t cputicks;                               /* CPU ticks */
	auto struct timeval time1,time2;                     /* Current time to be used if no CPU ticks are available */
	auto cmprefix *clauses[5];                           /* Set of pointers to clauses */
	auto symbol *smbl;                                   /* Symbol pointer */
	auto struct perf_event_attr perfevent;               /* Instruction count performance event */
	auto int orgfiledesc;                                /* Original value of file descriptor */
	auto int32_t orgperfmon;                             /* Original value of perfmon global variable */
	auto int16_t *auxptr;                                /* Auxiliary pointer */
	auto float ff;                                       /* Auxiliary */
	auto int32_t ii,jj,kk,tt;                            /* Auxiliary */
	auto uint64_t ll1,mm1;                               /* Auxiliary */

	/* Performance monitor is available. */
	orgperfmon=perfmon;
	orgfiledesc=filedesc;
	perfmon=1;
	memset(&perfevent,0,sizeof(perfevent));
	perfevent.type=PERF_TYPE_HARDWARE;
	perfevent.size=sizeof(perfevent);
	perfevent.config=PERF_COUNT_HW_INSTRUCTIONS;
	perfevent.disabled=1;
	perfevent.exclude_kernel=1;
	perfevent.exclude_hv=1;
	filedesc=syscall(SYS_perf_event_open,&perfevent,0,-1,-1,0);
	if (filedesc!=-1){

		/* First performance monitor experiment (arithmetic calculations. */
		myioctl(PERF_EVENT_IOC_RESET);
		myioctl(PERF_EVENT_IOC_ENABLE);
		for (ii=0;ii<10000000;ii++){
			for (jj=0;jj<5;jj++){
				ff=3.25*jj;
				ff/=(jj*3.0);
				ff+=(jj*2.1);
				if ((4&rand())|(int)ff){
					ff+=(rand()&(int)ff);
				} else {
					ff-=(rand()|(int)ff);
				}
			}
		}
		if (ff!=0.0){ /* To avoid "variable set but no used" compiler warning. */
			printf("\n");
		}

		/* Second performance monitor experiment (arithmetic calculations. */
		for (ii=0;(ii<1000000)&&(jj!=-1);ii++){

			/* Creation clause. */
			for (jj=0;jj<5;jj++){

				/* Allocate clause memory. */
				if ((clauses[jj]=MYALLOC(sizeof(cmprefix)))==NULL){
					printf("Not enough memory to perform second CPU experiment\n");
					for (kk=0;kk<jj;kk++){
						MYFREE(clauses[kk]->part2.bin);
						MYFREE(clauses[kk]);
					}
					jj=-1;
					break;
				}
				if ((clauses[jj]->part2.bin=MYALLOC(binpsize+8+sizeof(symbol)))==NULL){
					printf("Not enough memory to perform second CPU experiment\n");
					for (kk=0;kk<jj;kk++){
						MYFREE(clauses[kk]);
					}
					jj=-1;
					break;
				}

				/* Set clause. */
				clauses[jj]->flags=UNPROC;
				clauses[jj]->inference=PARAMODULATION;
				kbset.prstats.paramodulations++;
				clauses[jj]->parent1=NULL;
				clauses[jj]->parent2=NULL;
				clauses[jj]->part2.bin->agedist=0;
				clauses[jj]->part2.bin->sinedist=1;
				clauses[jj]->part2.bin->clause=clauses[jj];
				clauses[jj]->part2.bin->ovly.cptopterm=NULL;
				clauses[jj]->part2.bin->lockasserts=NULL;
				clauses[jj]->part2.bin->literals=1;
				clauses[jj]->part2.bin->size=binpsize+8+sizeof(symbol);
				clauses[jj]->part2.bin->maxvarnb=1;
				clauses[jj]->part2.bin->oriented=0;
				clauses[jj]->part2.bin->formula[0]=EQUALITY;
				smbl=(symbol *)&clauses[jj]->part2.bin->formula[1];
				smbl->symbol=&equkey;
				clauses[jj]->part2.bin->formula[1+sizeof(symbol)]=VARIABLE;
				auxptr=(int16_t *)&clauses[jj]->part2.bin->formula[2+sizeof(symbol)];
				*auxptr=0;
				clauses[jj]->part2.bin->formula[4+sizeof(symbol)]=VARIABLE;
				auxptr=(int16_t *)&clauses[jj]->part2.bin->formula[5+sizeof(symbol)];
				*auxptr=1;
				clauses[jj]->part2.bin->formula[7+sizeof(symbol)]=UNITEND;
				UpdateParams2(&clauses[jj]->part2.bin->formula[0]);
			}

			/* Free clause memory. */
			for (jj=0;jj<5;jj+=2){
				MYFREE(clauses[jj]->part2.bin);
				MYFREE(clauses[jj]);
			}
			for (jj=1;jj<5;jj+=2){
				MYFREE(clauses[jj]->part2.bin);
				MYFREE(clauses[jj]);
			}
		}

		/* Print final results. */
		myioctl(PERF_EVENT_IOC_DISABLE);
		myread(&mm1);
		printf("Performance monitor is available.\n");
		printf("Performance monitor test: %lu HW instructions\n",mm1);
		printf("Recommended time_factor_value parameter in TMFACTOR command/option when using performance monitor is %f\n\n",mm1/10423777629.0);
		printf("If this factor is significantly different than 1.0 then using PERFMON OFF command/option is recommended\n");

	/* Performance monitor is not available. */
	} else {
		printf("Performance monitor is not available.\n\n");
	}

	/* Flush stdout to prevent flushing from occurring in the next code */
	/* and increasing so the CPU time. */
	fflush(stdout);

	/* Five CPU ticks tests are done to compute the average because */
	/* the results always show some variation. */
	for (tt=0,ll1=0;tt<5;tt++){

		/* First CPU ticks experiment (arithmetic calculations). */
		if (cpuavail){
			cputicks=clock();
		} else {
			gettimeofday(&time1,NULL);
		}
		for (ii=0;ii<10000000;ii++){
			for (jj=0;jj<5;jj++){
				ff=3.25*jj;
				ff/=(jj*3.0);
				ff+=(jj*2.1);
				if ((4&rand())|(int)ff){
					ff+=(rand()&(int)ff);
				} else {
					ff-=(rand()|(int)ff);
				}
			}
		}
		if (ff==0.0){ /* To avoid "variable set but no used" compiler warning. */
			printf("\n");
		}

		/* Second CPU ticks experiment (clause creation). */
		for (ii=0;(ii<1000000)&&(jj!=-1);ii++){

			/* Creation clause. */
			for (jj=0;jj<5;jj++){

				/* Allocate clause memory. */
				if ((clauses[jj]=MYALLOC(sizeof(cmprefix)))==NULL){
					printf("Not enough memory to perform second CPU experiment\n");
					for (kk=0;kk<jj;kk++){
						MYFREE(clauses[kk]->part2.bin);
						MYFREE(clauses[kk]);
					}
					jj=-1;
					break;
				}
				if ((clauses[jj]->part2.bin=MYALLOC(binpsize+8+sizeof(symbol)))==NULL){
					printf("Not enough memory to perform second CPU experiment\n");
					for (kk=0;kk<jj;kk++){
						MYFREE(clauses[kk]);
					}
					jj=-1;
					break;
				}

				/* Set clause. */
				clauses[jj]->flags=UNPROC;
				clauses[jj]->inference=PARAMODULATION;
				kbset.prstats.paramodulations++;
				clauses[jj]->parent1=NULL;
				clauses[jj]->parent2=NULL;
				clauses[jj]->part2.bin->agedist=0;
				clauses[jj]->part2.bin->sinedist=1;
				clauses[jj]->part2.bin->clause=clauses[jj];
				clauses[jj]->part2.bin->ovly.cptopterm=NULL;
				clauses[jj]->part2.bin->lockasserts=NULL;
				clauses[jj]->part2.bin->literals=1;
				clauses[jj]->part2.bin->size=binpsize+8+sizeof(symbol);
				clauses[jj]->part2.bin->maxvarnb=1;
				clauses[jj]->part2.bin->oriented=0;
				clauses[jj]->part2.bin->formula[0]=EQUALITY;
				smbl=(symbol *)&clauses[jj]->part2.bin->formula[1];
				smbl->symbol=&equkey;
				clauses[jj]->part2.bin->formula[1+sizeof(symbol)]=VARIABLE;
				auxptr=(int16_t *)&clauses[jj]->part2.bin->formula[2+sizeof(symbol)];
				*auxptr=0;
				clauses[jj]->part2.bin->formula[4+sizeof(symbol)]=VARIABLE;
				auxptr=(int16_t *)&clauses[jj]->part2.bin->formula[5+sizeof(symbol)];
				*auxptr=1;
				clauses[jj]->part2.bin->formula[7+sizeof(symbol)]=UNITEND;
				UpdateParams2(&clauses[jj]->part2.bin->formula[0]);
			}

			/* Free clause memory. */
			for (jj=0;jj<5;jj+=2){
				MYFREE(clauses[jj]->part2.bin);
				MYFREE(clauses[jj]);
			}
			for (jj=1;jj<5;jj+=2){
				MYFREE(clauses[jj]->part2.bin);
				MYFREE(clauses[jj]);
			}
		}
		if (cpuavail){
			ll1+=(clock()-cputicks);
		} else {
			gettimeofday(&time2,NULL);
			ll1+=(time2.tv_sec-time1.tv_sec+(time2.tv_usec-time1.tv_usec)/1000000.0)*CLOCKS_PER_SEC;
		}
	}
	ll1/=5;

	/* Print final results. */
	if (!cpuavail){
		printf("CPU time is not available and will be estimated using elapsed time.\n");
	}
	printf("Average CPU ticks test: %lu ticks\n",ll1);
	printf("Recommended time_factor_value parameter in TMFACTOR command/option when using CPU ticks is %f\n",ll1/3165992.6);

	/* Restore original perfmon and filedesc variables. */
	perfmon=orgperfmon;
	filedesc=orgfiledesc;
	return;
} /* GetPerfData */

#ifdef DEBUGMEMORY
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Memory free function for debugging purposes)  OCJ
 *
 *    This function is an interface to free() and myfree() functions.
 *    In addition to calling free() or myfree() functions it performs
 *    some additional tasks before and/or after it. See variables
 *    defined in global.h when DEBUGMEMORY is defined for more
 *    information.
 *
 *    The purpose of DEBUGMEMORY code is to help debug non freed
 *    memory problems reported by Valgrind as memory lost. They
 *    usually are due to non freed allocated memory. The following
 *    steps are a guide to the proposed methodology:
 *    - First find an interval of program execution where the
 *      problem is produced. This is done by placing appropriate
 *      checking checking code of the auxcounter variable in the
 *      appropriate places.
 *    - Place appropriate code after the MYALLOC() or MYREALLOC() call
 *      reported by Valgrind where the non freed memory was allocated
 *    - Checking the output of the program it can be determined the
 *      exact MYALLOC() or MYREALLOC() call that corresponds to the
 *      non freed memory and so track and fix the problem.
 *
 *      Example of code after a MYALLOC() call:
 *			auxcounter++;
 *			if (auxcounter>=50){ //Adjust number in this line.
 *				if (ndemods<DBGMEMELEMS){
 *					memtrack[ndemods]=1;
 *					memptrs[ndemods]=ptr1; //Adjust ptr1 in this line
 *					ndemods++;
 *				} else if (ndemods<(DBGMEMELEMS+1)){
 *					printf("==> Memory debug stack overflow...\n");
 *					ndemods++;
 *				}
 *			}
 *
 *      Example of code after a MYREALLOC() call:
 *			auxcounter++;
 *			if (auxcounter>=50){ //Adjust number in this line.
 *				auto int32_t ii,jj;
 *				for (ii=ndemods-1,jj=0;ii>=0;ii--){
 *					if (memptrs[ii]==chunk){ //Adjust chunk here to the original memory pointer.
 *						if (memtrack[ii]==1){
 *							jj=1;
 *							break;
 *						} else {
 *							printf("Reallocation attempt without a previous allocation, number %d.",ii);
 *							break;
 *						}
 *					}
 *				}
 *				if (jj){
 *					if (ndemods<DBGMEMELEMS){
 *						memptrs[ndemods]=ptr5; //Adjust ptr5 here to the new memory pointer
 *						ndemods++;
 *					} else if (ndemods<(DBGMEMELEMS+1)){
 *						printf("==> Memory debug stack overflow...\n");
 *						ndemods++;
 *					}
 *				}
 *			}
 *
 *  ARGUMENTS:
 *
 *    ptr: Pointer to previously allocated block of memory.
 *    file: Pointer to name of file where the myfree() call was located.
 *    linenb: Line number where the myfree() call was located.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *--------------------------------------------------------------*/
void myfree(void *ptr,char *file,int linenb){
	auto int ii,jj,kk;                                   /* Auxiliary */

	/* Check watched pointers previously allocated. */
	for (ii=jj=0;ii<ndemods;ii++){
		if (memptrs[ii]==ptr){
			if (memtrack[ii]==1){
				jj++;
				switch (jj){
					case 1:
						kk=ii;
						break;
					case 2:
						printf("==> ERROR: Memory allocated in a previously used memory location\n");
						printf("==> ERROR:            free call file %s\n",file);
						printf("==> ERROR:            free call line %d\n",linenb);
						printf("==> ERROR:            Initial allocation number %d\n",kk);
					/* No break */
					default:
						printf("==> ERROR:            Additional allocation number %d\n",ii);
						break;
				}
				memtrack[ii]=2;
			}
		}
	}

	/* Free memory and return. */
	#if MEMCHECK == 3
	free(ptr);
	#else
	freemn(ptr);
	#endif
	return;
} /* myfree */
#endif

#ifdef MEMORYTUNNING
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Allocate additional memory for MAXMEMORY tuning)  OCJ
 *
 *    If memory argument is not zero this function will allocate that
 *    amount of memory in three types of chunks:
 *    - Chunks with 16 to 25 bytes
 *    - Chunks with 76 to 125 bytes
 *    - Chunks with 376 to 625 bytes
 *
 *    If memory argument is zero then this function will free the
 *    allocated memory.
 *
 *  ARGUMENTS:
 *
 *    memory: Total number of bytes of memory to be allocated.
 *
 *  RETURNS:
 *
 *    0 if OK, NOMEMORY if not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t SymMalloc(uint64_t memory){
	static int64_t number;                               /* Number of chunks of each type to be allocated */
	static char **memptr1,**memptr2,**memptr3;           /* Pointers to sets of pointers */
	auto int64_t ii,jj,kk,mm;                            /* Auxiliary */

	if (memory!=0){
		number=memory/(20+100+500+(3*sizeof(void *)));
		if (NULL==(memptr1=MYALLOC(number*sizeof(char *)))){
			return(NOMEMORY);
		}
		if (NULL==(memptr2=MYALLOC(number*sizeof(char *)))){
			MYFREE(memptr1);
			return(NOMEMORY);
		}
		if (NULL==(memptr3=MYALLOC(number*sizeof(char *)))){
			MYFREE(memptr1);
			MYFREE(memptr2);
			return(NOMEMORY);
		}
		mm=3*number*sizeof(char *);
		memset(memptr1,0,number*sizeof(char *));
		memset(memptr2,0,number*sizeof(char *));
		memset(memptr3,0,number*sizeof(char *));
		for (ii=0;;ii++){
			kk=16+(rand()%10);
			if ((kk+mm)>=memory){
				kk=memory-mm;
			}
			if (NULL==(memptr1[ii]=MYALLOC(16+(rand()%10)))){
				for (jj=0;jj<ii;jj++){
					MYFREE(memptr1[jj]);
					MYFREE(memptr2[jj]);
					MYFREE(memptr3[jj]);
				}
				MYFREE(memptr1);
				MYFREE(memptr2);
				MYFREE(memptr3);
				return(NOMEMORY);
			}
			mm+=kk;
			if (mm>=memory){
				break;
			}
			kk=76+(rand()%50);
			if ((kk+mm)>=memory){
				kk=memory-mm;
			}
			if (NULL==(memptr2[ii]=MYALLOC(76+(rand()%50)))){
				for (jj=0;jj<ii;jj++){
					MYFREE(memptr1[jj]);
					MYFREE(memptr2[jj]);
					MYFREE(memptr3[jj]);
				}
				MYFREE(memptr1[ii]);
				MYFREE(memptr1);
				MYFREE(memptr2);
				MYFREE(memptr3);
				return(NOMEMORY);
			}
			mm+=kk;
			if (mm>=memory){
				break;
			}
			if (ii<(number-1)){
				kk=376+(rand()%250);
				if ((kk+mm)>=memory){
					kk=memory-mm;
				}
			} else {
				kk=memory-mm;
			}
			if (NULL==(memptr3[ii]=MYALLOC(376+(rand()%250)))){
				for (jj=0;jj<ii;jj++){
					MYFREE(memptr1[jj]);
					MYFREE(memptr2[jj]);
					MYFREE(memptr3[jj]);
				}
				MYFREE(memptr1[ii]);
				MYFREE(memptr2[ii]);
				MYFREE(memptr1);
				MYFREE(memptr2);
				MYFREE(memptr3);
				return(NOMEMORY);
			}
			mm+=kk;
			if (mm>=memory){
				break;
			}
		}
	} else {
		for (jj=0;jj<number;jj++){
			if (memptr1[jj]!=NULL){
				MYFREE(memptr1[jj]);
			} else {
				break;
			}
			if (memptr2[jj]!=NULL){
				MYFREE(memptr2[jj]);
			} else {
				break;
			}
			if (memptr3[jj]!=NULL){
				MYFREE(memptr3[jj]);
			} else {
				break;
			}
		}
		MYFREE(memptr1);
		MYFREE(memptr2);
		MYFREE(memptr3);
	}
	return(0);
}
#endif
