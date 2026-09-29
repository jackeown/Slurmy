/*
 ============================================================================
 Name        : parsing.c
 Author      : Oscar Contreras
 Version     :
 Copyright   : Copyright (C) 2015-2016 Oscar Contreras. All rights reserved.
 Description : First order logic theorem prover in C
 ============================================================================
 */
/****************************************************************
*
*                 parsing (parse program options and interactive commands)
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
 *    This module include routines necessary to parse program options
 *    and commands entered in interactivemode.
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
 *  DESCRIPTION: (Command processor)  OCJ
 *
 *    This function process the commands in interactive mode.
 *
 *    Commands are not case insensitive. However the command
 *    arguments may be case sensitive (for instance in the case
 *    of formulas or file names).
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
void CommandProc(void){
	auto char *line;                                /* Input line buffer */
	auto int32_t size;                              /* Size of input line buffer */
	auto char *ptr1;                                /* Auxiliary pointer */
	auto int32_t ii,jj;                             /* Auxiliary */

	/* Initial memory allocation for input line. */
	printf("%s\n",VERSION);
	if (NULL==(line=MYALLOC(INPUT_CHUNK_SIZE))){
		printf("No memory available, exiting program\n");
		return;
	}
	size=INPUT_CHUNK_SIZE;

	/* Read command loop. */
	while (1){

		/* Read line. */
		printf("Enter command: ");
		ii=0;
		do {
			fgets(&line[ii],size-ii,stdin);
			if (line[strlen(line)-1]!='\n'){
				ii=size-1;
				jj=1;
				size+=INPUT_CHUNK_SIZE;
				ptr1=line;
				if (NULL==(ptr1=MYREALLOC(ptr1,size))){
					printf("No memory available, exiting program\n");
					MYFREE(line);
					return;
				}
				line=ptr1;
			} else {
				jj=0;
			}
		} while (jj);
		line[strlen(line)-1]=0;

		/* Parse and execute command in line. */
		ii=ParseCommand(line,&ptr1);
		if (ptr1!=NULL){
			printf("%s\n",ptr1);
		}
		if (ii){
			break;
		}
	}

	/* Free memory, perform cleaning for exit and return. */
	MYFREE(line);
	return;
} /* CommandProc */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Parse program arguments)  OCJ
 *
 *    This function process the arguments passed to the program
 *    when executing it in non-interactive mode.
 *
 *    If more than one of the options -GOSAT, -PBMTYPE, -CLAUSIFY
 *    and -PROCLEARN is specified only the last one will be processed
 *    and the process will be done after processing all the other
 *    options. For other options they will be processed one after the
 *    other even if some of them are repeated.
 *
 *    Options are NOT case sensitive and must be preceded by a "-".
 *    If the option has arguments then they must be enclosed in
 *    parenthesis and separated by commas with no spaces in between
 *    except for program names enclosed in single quotes. For instance:
 *      -relevance(off)
 *      -proclearn('my file name',10)
 *
 *
 *  ARGUMENTS:
 *
 *    argc: Number of arguments passed, including the program filename.
 *    argv: Pointer to set of pointers to each argument. The first
 *          argument argument is the program filename. The last argument
 *          must be a valid problem filename. The remaining arguments
 *          must be valid options. It is assumed than there are some
 *          arguments (that is,argc>1) but this is not checked.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void ParseArgs(int argc,char **argv){
	auto char *argline;                             /* Pointer to copy of a single argument */
	auto int32_t size;                              /* Size of argline buffer */
	auto int32_t exectype;                          /* Type of executing option */
	auto int32_t prclrn;                            /* Set to 1 if PROCLEARN option executed, otherwise 0 */
	auto int32_t help;                              /* Set to 1 if HELP option executed, otherwise 0 */
	auto char *ptr1;                                /* Auxiliary pointer */
	auto int32_t ii,jj,kk;                          /* Auxiliary */

	/* Initial memory allocation for input line. */
	printf("%% %s\n",VERSION);
	if (NULL==(argline=MYALLOC(INPUT_CHUNK_SIZE))){
		printf("No memory available, exiting program\n");
		return;
	}
	size=INPUT_CHUNK_SIZE;

	/* Loop through program parameters. */
	exectype=prclrn=help=0;
	for (ii=1;ii<argc;ii++){

		/* Option not preceded by a "-" character. Leave the loop if last option */
		/* as it could be the target filename, return otherwise. */
		if (*argv[ii]!='-'){
			if (ii<(argc-1)){
				printf("Invalid option %s, exiting program\n",argv[ii]);
				MYFREE(argline);
				return;
			} else {
				break;
			}
		}

		/* Copy option to argline not including the initial dash. */
		jj=strlen(argv[ii]);
		if (jj>size){
			size+=jj;
			ptr1=argline;
			if (NULL==(ptr1=MYREALLOC(ptr1,size))){
				printf("No memory available, exiting program\n");
				MYFREE(argline);
				return;
			}
			argline=ptr1;
		}
		strcpy(argline,&argv[ii][1]);

		/* First option validity check and conversion to interactive command format. */
		/* Last parameter may be a valid filename if not a valid option. */
		if (ChkConvertOption(argline)){
			if (ii<(argc-1)){
				printf("Invalid option %s, exiting program\n",argv[ii]);
				MYFREE(argline);
				return;
			} else {
				break;
			}
		}

		/* Convert first token to upper case. */
		for (jj=0;(argline[jj]!=0)&&(argline[jj]!=' ')&&(argline[jj]!='\t');jj++){
			argline[jj]=toupper(argline[jj]);
		}

		/* Process option.  */
		/* Action options. Remember the last one. Last parameter may be */
		/* a valid filename if not a valid option. */
		kk=0;
		if ((0==strncmp("GOSAT",argline,jj))&&(jj==5)){
			if (argline[jj]!=0){
				if (ii<(argc-1)){
					printf("Invalid option %s, exiting program\n",argv[ii]);
					MYFREE(argline);
					return;
				} else {
					kk=1;
				}
			} else {
				exectype=1;
			}
		} else if ((0==strncmp("PBMTYPE",argline,jj))&&(jj==7)){
			if (argline[jj]!=0){
				if (ii<(argc-1)){
					printf("Invalid option %s, exiting program\n",argv[ii]);
					MYFREE(argline);
					return;
				} else {
					kk=1;
				}
			} else {
				exectype=2;
			}
		} else if ((0==strncmp("CLAUSIFY",argline,jj))&&(jj==8)){
			if (argline[jj]!=0){
				if (ii<(argc-1)){
					printf("Invalid option %s, exiting program\n",argv[ii]);
					MYFREE(argline);
					return;
				} else {
					kk=1;
				}
			} else {
				exectype=3;
			}
		} else if ((0==strncmp("TNGSTRAT",argline,jj))&&(jj==8)){
			if (argline[jj]!=0){
				if (ii<(argc-1)){
					printf("Invalid option %s, exiting program\n",argv[ii]);
					MYFREE(argline);
					return;
				} else {
					kk=1;
				}
			} else {
				exectype=4;
			}

		/* Non action option. Pass it to ParseCommand(). Last parameter */
		/* may be a valid filename if not a valid option. */
		} else {
			if ((0==strncmp("PROCLEARN",argline,jj))&&(jj==5)){
				prclrn=1;
			} else if (0==strncmp("HELP",argline,jj)){
				help=1;
			}
			kk=ParseCommand(argline,&ptr1);
			if (kk){
				if (ii<(argc-1)){
					if (ptr1!=NULL){
						printf("%s in %s\n",ptr1,argv[ii]);
					}
					MYFREE(argline);
					return;
				}
			} else if (help){
				MYFREE(argline);
				return;
			}
		}

		/* If last parameter has been correctly processed and */
		/* there is no PROCLEARN option then there is nothing */
		/* to do. Report status and return. */
		if (ii==(argc-1)){
			if ((prclrn==0)&&(kk==0)){
				printf("No filename specified, nothing to do\n");
				MYFREE(argline);
				return;
			} else {
				if (prclrn==0){
					if (ptr1!=NULL){
						printf("%s in %s\n",ptr1,argv[ii]);
					}
					MYFREE(argline);
					return;
				}
				exectype=5;
			}
		}
	}

	/* Perform the requested action and return. */
	if (exectype!=5){
		syntaxmode=TPTP;
		importfile=argv[argc-1];
		if (0!=Import(importfile,NULL,0,1)){
			Initialize_KB(&mainkb);
			ResetStats(pbmstats);
		} else {
			switch (exectype){
				case 0:
					Saturate();
					break;
				case 1:
					GoSat();
					break;
				case 2:
					GoType();
					break;
				case 3:
					Clausify();
					break;
				case 4:
					Saturate2();
					break;
			}
		}
	}
	MYFREE(argline);
	return;
} /* ParseArgs */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check and convert option to interactive command)  OCJ
 *
 *    This function makes an initial validity check to an option included
 *    in program invocation and converts it to an interactive command.
 *
 *
 *  ARGUMENTS:
 *
 *    argline: Pointer to option as entered included in program invocation
 *             on entry, conversion result on exit.
 *
 *  RETURNS:
 *
 *    0 if OK or 1 if option not valid.
 *
 *--------------------------------------------------------------*/
int32_t ChkConvertOption(char *argline){
	auto int32_t jj,kk,mm;                          /* Auxiliary */


	/* Loop through option characters. */
	for (jj=0,kk=mm=0;argline[jj]!=0;jj++){
		if (argline[jj]=='\''){
			kk^=1;
		} else if (kk==0){
			switch (argline[jj]){
				case '(':
					if (mm==0){
						if ((argline[strlen(argline)-1]!=')')||(argline[jj+1]==')')||(argline[jj+1]==',')){
							return(1);
						}
						mm=1;
						argline[jj]=' ';
					} else {
						return(1);
					}
					break;
				case ')':
					if ((mm==0)||(argline[jj+1]!=0)){
						return(1);
					}
					argline[jj]=0;
					break;
				case ',':
					if ((argline[jj+1]==')')||(argline[jj+1]==',')){
						return(1);
					}
					argline[jj]=' ';
					break;
				case ' ':
					return(1);
					break;
			}
		}
	}
	if (kk){
		return(1);
	}
	return(0);
} /* ChkConvertOption */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Command parser)  OCJ
 *
 *    This function parses the commands entered in interactive
 *    mode or built from options passed as arguments at program
 *    invocation.
 *
 *
 *  ARGUMENTS:
 *
 *    command: Pointer to string with the command.
 *    msg: Address of pointer where an output message will be
 *         stored. It is the calles responsibility to display
 *         this message when appropriate.
 *
 *  RETURNS:
 *
 *    0 if OK or 1 if program must terminate due to an error
 *    or an user request.
 *
 *--------------------------------------------------------------*/
int32_t ParseCommand(char *command,char **msg){
	auto struct itimerval alarmtime;                /* To define a global timeout alarm */
	auto clock_t start,end;                         /* For CPU time measurements */
	auto char *ltkn;                                /* Pointer to token */
	auto char *ptr1,*ptr2,*ptr3,*ptr4;              /* Auxiliary pointers */
	auto struct mallinfo2 rsinfo;                   /* For mallinfo2() calls */
	auto int32_t ii,jj,nn,cc;                       /* Auxiliary */
	auto uint64_t kk;                               /* Auxiliary */
	auto float ff;                                  /* Auxiliary */

	/* Check number of occurrences of ':' char for SELECTRATIO parsing. */
	*msg=NULL;
	for (ptr1=command-1,cc=-1;ptr1!=NULL;ptr1=strchr(ptr1+1,':'),cc++){
	}

	/* Parse first token and convert to upper case. */
	if (NULL==(ltkn=strtok(command," \t"))){
		return(0);
	}
	for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
		ltkn[ii]=toupper(ltkn[ii]);
	}

	/* EXIT command. */
	if (0==strcmp(ltkn,"EXIT")){
		if (pgmmode==NONINTERACTIVE){
			*msg="Invalid option";
			return(1);
		}
		if (NULL==(ltkn=strtok(NULL," \t"))){
			if (CheckAnswer("Exit program? [y/N]: ","")){
				return(1);
			}
		} else {
			printf("Invalid EXIT option: %s\n",ltkn);
			printf("This command has no options\n");
		}

	/* HELP command. */
	} else if (0==strcmp(ltkn,"HELP")){
		Help(ltkn);
		if (pgmmode==NONINTERACTIVE){
			return(1);
		}

	/* NEW command. */
	} else if (0==strcmp(ltkn,"NEW")){
		if (pgmmode==NONINTERACTIVE){
			*msg="Invalid option";
			return(1);
		}
		if (NULL==(ltkn=strtok(NULL," \t"))){
			if (CheckAnswer("Initialize all data? [y/N]: ","KB not initialized")){
				if (NOMEMORY==Initialize_KB(&mainkb)){
					printf("No memory available, exiting program\n");
					return(1);
				}
				ResetStats(pbmstats);
				printf("Knowledge bases have been initialized\n");
			}
		} else {
			printf("Invalid NEW option: %s\n",ltkn);
			printf("This command has no options\n");
		}

	/* TELL command. */
	} else if (0==strcmp(ltkn,"TELL")){
		if (pgmmode==NONINTERACTIVE){
			*msg="Invalid option";
			return(1);
		}
		ltkn=strtok(NULL,"");
		gettimeofday(&mainkb.time,NULL);
		start=clock();
		ii=Tell(ltkn,AXIOM,FROMMAINFILE);
		gettimeofday(&mainkb.endtime,NULL);
		mainkb.prstats.elapsed_time+=(mainkb.endtime.tv_sec-mainkb.time.tv_sec
				+(mainkb.endtime.tv_usec-mainkb.time.tv_usec)/1000000.0);
		end=clock();
		cpu_time+=(((double)(end-start))/CLOCKS_PER_SEC);
		switch (ii){
			case NOMEMORY:
				return(1);
				break;
			case TIMEOUT:
				if (NOMEMORY==Initialize_KB(&mainkb)){
					printf("No memory available, exiting program\n");
					return(1);
				}
				ResetStats(pbmstats);
				printf("Knowledge bases have been initialized\n");
				break;
		}

	/* ASK command. */
	} else if (0==strcmp(ltkn,"ASK")){
		if (pgmmode==NONINTERACTIVE){
			*msg="Invalid option";
			return(1);
		}
		ltkn=strtok(NULL,"");
		gettimeofday(&mainkb.time,NULL);
		start=clock();
		ii=Ask(ltkn);
		gettimeofday(&mainkb.endtime,NULL);
		mainkb.prstats.elapsed_time+=(mainkb.endtime.tv_sec-mainkb.time.tv_sec
				+(mainkb.endtime.tv_usec-mainkb.time.tv_usec)/1000000.0);
		end=clock();
		cpu_time+=(((double)(end-start))/CLOCKS_PER_SEC);
		switch (ii){
			case NOMEMORY:
				return(1);
				break;
			case TIMEOUT:
				if (NOMEMORY==Initialize_KB(&mainkb)){
					printf("No memory available, exiting program\n");
					return(1);
				}
				ResetStats(pbmstats);
				printf("Knowledge bases have been initialized\n");
				break;
		}

	/* GO command. */
	} else if (0==strcmp(ltkn,"GO")){
		if (pgmmode==NONINTERACTIVE){
			*msg="Invalid option";
			return(1);
		}
		if (NULL==(ltkn=strtok(NULL," \t"))){
			alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
			ff=glblopts.timeout-mainkb.prstats.elapsed_time;
			alarmtime.it_value.tv_sec=(int32_t)ff;
			alarmtime.it_value.tv_usec=(int32_t)((ff-(int32_t)ff)*1000000.0);
			alrmstatus=0;
			setitimer(ITIMER_REAL,&alarmtime,NULL);
			Saturate();
		} else {
			printf("Invalid GO option: %s\n",ltkn);
			printf("This command has no options\n");
		}

	/* GOSAT command. */
	} else if (0==strcmp(ltkn,"GOSAT")){
		if (NULL!=(ltkn=strtok(NULL,""))){
			if (NULL==(ptr1=ParseFilename(ltkn,&ptr2))){
				*msg="Invalid filename";
				return(pgmmode==INTERACTIVE?0:1);
			}
			ii=syntaxmode;
			syntaxmode=TPTP;
			importfile=ptr1;
			if (0==Import(ptr1,NULL,0,1)){
				GoSat();
			} else {
				rsinfo=mallinfo2();
				if (glblstats.memory<(rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost)){
					glblstats.memory=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
					glblstats.netmemory=rsinfo.uordblks+rsinfo.hblkhd;
				}
				tot_cpu_time+=cpu_time;
				tot_pbms++;
			}
			syntaxmode=ii;
			importfile=NULL;
		} else {
			alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
			ff=glblopts.timeout-mainkb.prstats.elapsed_time;
			alarmtime.it_value.tv_sec=(int32_t)ff;
			alarmtime.it_value.tv_usec=(int32_t)((ff-(int32_t)ff)*1000000.0);
			alrmstatus=0;
			setitimer(ITIMER_REAL,&alarmtime,NULL);
			GoSat();
		}
		if (NOMEMORY==Initialize_KB(&mainkb)){
			printf("No memory available, exiting program\n");
			return(1);
		}
		ResetStats(pbmstats);
		printf("Knowledge bases have been initialized\n");

	/* TNGSTRAT command. */
	} else if (0==strcmp(ltkn,"TNGSTRAT")){
		if (NULL!=(ltkn=strtok(NULL,""))){
			if (NULL==(ptr1=ParseFilename(ltkn,&ptr2))){
				*msg="Invalid filename";
				return(pgmmode==INTERACTIVE?0:1);
			}
			ii=syntaxmode;
			syntaxmode=TPTP;
			importfile=ptr1;
			if (0==Import(ptr1,NULL,0,1)){
				Saturate2();
			} else {
				rsinfo=mallinfo2();
				if (glblstats.memory<(rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost)){
					glblstats.memory=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
					glblstats.netmemory=rsinfo.uordblks+rsinfo.hblkhd;
				}
				tot_cpu_time+=cpu_time;
				tot_pbms++;
			}
			syntaxmode=ii;
			importfile=NULL;
		} else {
			alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
			ff=glblopts.timeout-mainkb.prstats.elapsed_time;
			alarmtime.it_value.tv_sec=(int32_t)ff;
			alarmtime.it_value.tv_usec=(int32_t)((ff-(int32_t)ff)*1000000.0);
			alrmstatus=0;
			setitimer(ITIMER_REAL,&alarmtime,NULL);
			Saturate2();
		}
		if (NOMEMORY==Initialize_KB(&mainkb)){
			printf("No memory available, exiting program\n");
			return(1);
		}
		ResetStats(pbmstats);
		printf("Knowledge bases have been initialized\n");

	/* PBMTYPE command. */
	} else if (0==strcmp(ltkn,"PBMTYPE")){
		if (NULL!=(ltkn=strtok(NULL,""))){
			if (NULL==(ptr1=ParseFilename(ltkn,&ptr2))){
				*msg="Invalid filename";
				return(pgmmode==INTERACTIVE?0:1);
			}
			procctl->start=clock();
			gettimeofday(&mainkb.time,NULL);
			printf("Reading file %s...\n",ptr1);
			ii=syntaxmode;
			syntaxmode=TPTP;
			if (Import(ptr1,NULL,0,1)){
				printf("      UNIT_EQUALITY UNIT_CLAUSE HORN GROUND NONUNIT_EQU PBM_TYPE\n");
				printf("====>      ?             ?       ?      ?         ?        ?\n");
				rsinfo=mallinfo2();
				if (glblstats.memory<(rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost)){
					glblstats.memory=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
					glblstats.netmemory=rsinfo.uordblks+rsinfo.hblkhd;
				}
				tot_cpu_time+=cpu_time;
				if (NOMEMORY==Initialize_KB(&mainkb)){
					printf("No memory available, exiting program\n");
					return(1);
				}
				ResetStats(pbmstats);
				printf("Knowledge bases have been initialized\n");
				syntaxmode=ii;
				tot_pbms++;
				return(0);
			}
			syntaxmode=ii;
		} else if (pgmmode==NONINTERACTIVE){
			*msg="Missing PBMTYPE parameter";
			return(1);
		}
		GoType();

	/* CLAUSIFY command. */
	} else if (0==strcmp(ltkn,"CLAUSIFY")){
		if (NULL!=(ltkn=strtok(NULL,""))){
			if (NULL==(ptr1=ParseFilename(ltkn,&ptr2))){
				*msg="Invalid filename";
				return(pgmmode==INTERACTIVE?0:1);
			}
			printf("Converting formulas in file %s to CNF format...\n",ptr1);
			ii=syntaxmode;
			syntaxmode=TPTP;
			importfile=ptr1;
			if (0==Import(ptr1,NULL,0,1)){
				Clausify();
			} else {
				rsinfo=mallinfo2();
				if (glblstats.memory<(rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost)){
					glblstats.memory=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
					glblstats.netmemory=rsinfo.uordblks+rsinfo.hblkhd;
				}
				tot_cpu_time+=cpu_time;
				tot_pbms++;
			}
			syntaxmode=ii;
			importfile=NULL;
		} else {
			alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
			ff=glblopts.timeout-mainkb.prstats.elapsed_time;
			alarmtime.it_value.tv_sec=(int32_t)ff;
			alarmtime.it_value.tv_usec=(int32_t)((ff-(int32_t)ff)*1000000.0);
			alrmstatus=0;
			setitimer(ITIMER_REAL,&alarmtime,NULL);
			printf("Converting formulas to CNF format...\n");
			Clausify();
		}
		if (NOMEMORY==Initialize_KB(&mainkb)){
			printf("No memory available, exiting program\n");
			return(1);
		}
		ResetStats(pbmstats);
		printf("Knowledge bases have been initialized\n");

	/* TERMORDER command. */
	} else if (0==strcmp(ltkn,"TERMORDER")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"STANDARD")){
				glblopts.termord=STANDARD;
				usrparam_mask|=TERMORDRMASK;
				usrstrat&=~TERMORDRMASK;
				usrstrat|=TRMORDSTD_P;
				strategy=0xffffffffffffffff;
				*msg="Term ordering has been set to STANDARD";
			} else if (0==strcmp(ltkn,"NONRECURSIVE")){
				glblopts.termord=NONRECURSIVE;
				usrparam_mask|=TERMORDRMASK;
				usrstrat&=~TERMORDRMASK;
				usrstrat|=TRMORDNR_P;
				strategy=0xffffffffffffffff;
				*msg="Term ordering has been set to NONRECURSIVE";
			} else if (0==strcmp(ltkn,"DEFAULT")){
				usrparam_mask&=~TERMORDRMASK;
				usrstrat&=~TERMORDRMASK;
				*msg="Term ordering has been set to DEFAULT";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing TERMORDER parameter";
					return(1);
				}
				if (0==(usrparam_mask&TERMORDRMASK)){
					*msg="Term ordering is set to DEFAULT";
				} else {
					if (glblopts.termord==STANDARD){
						*msg="Term ordering is STANDARD";
					} else {
						*msg="Term ordering is NON RECURSIVE";
					}
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid TERMORDER option, valid options are STANDARD, NONRECURSIVE or DEFAULT";
					return(1);
				}
				printf("Invalid TERMORDER option: %s\n",ltkn);
				printf("Valid options are STANDARD, NONRECURSIVE or DEFAULT\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing TERMORDER parameter";
				return(1);
			}
			if (0==(usrparam_mask&TERMORDRMASK)){
				printf("Term ordering is set to DEFAULT\n");
			} else {
				if (glblopts.termord==STANDARD){
					printf("Term ordering is STANDARD\n");
				} else {
					printf("Term ordering is NON RECURSIVE\n");
				}
			}
		}

	/* LITORDER command. */
	} else if (0==strcmp(ltkn,"LITORDER")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"STANDARD")){
				glblopts.litord=STANDARD;
				usrparam_mask|=LITORDRMASK;
				usrstrat&=~LITORDRMASK;
				usrstrat|=LITORDSTD_P;
				strategy=0xffffffffffffffff;
				*msg="Literal ordering has been set to STANDARD";
			} else if (0==strcmp(ltkn,"NONRECURSIVE")){
				glblopts.litord=NONRECURSIVE;
				usrparam_mask|=LITORDRMASK;
				usrstrat&=~LITORDRMASK;
				usrstrat|=LITORDNR_P;
				strategy=0xffffffffffffffff;
				*msg="Literal ordering has been set to NONRECURSIVE";
			} else if (0==strcmp(ltkn,"LEXICOGRAPHIC")){
				usrparam_mask|=LITORDRMASK;
				usrstrat&=~LITORDRMASK;
				usrstrat|=LITORDNR_P; /* No elegant, but...  */
				strategy=0xffffffffffffffff;
				*msg="Literal ordering has been set to LEXICOGRAPHIC";
			} else if (0==strcmp(ltkn,"DEFAULT")){
				usrparam_mask&=~LITORDRMASK;
				usrstrat&=~LITORDRMASK;
				*msg="Literal ordering has been set to DEFAULT";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing LITORDER parameter";
					return(1);
				}
				if (0==(usrparam_mask&LITORDRMASK)){
					printf("Literal ordering is set to DEFAULT\n");
				} else {
					if (glblopts.litord==STANDARD){
						printf("Literal ordering is STANDARD\n");
					} else {
						printf("Literal ordering is NON RECURSIVE\n");
					}
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid LITORDER option, valid options are STANDARD, NONRECURSIVE, LEXICOGRAPHIC or DEFAULT";
					return(1);
				}
				printf("Invalid LITORDER option: %s\n",ltkn);
				printf("Valid options are STANDARD, NONRECURSIVE, LEXICOGRAPHIC or DEFAULT\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing LITORDER parameter";
				return(1);
			}
			if (0==(usrparam_mask&LITORDRMASK)){
				printf("Literal ordering is set to DEFAULT\n");
			} else {
				if (glblopts.litord==STANDARD){
					printf("Literal ordering is STANDARD\n");
				} else {
					printf("Literal ordering is NON RECURSIVE\n");
				}
			}
		}

	/* FUNCTIONW command. */
	} else if (0==strcmp(ltkn,"FUNCTIONW")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"UNIFORM")){
				glblopts.fweight=UNIFORM;
				usrparam_mask|=FUNCWMASK;
				usrstrat&=~FUNCWMASK;
				usrstrat|=UNIFORM_P;
				strategy=0xffffffffffffffff;
				*msg="Function weight has been set to UNIFORM";
			} else if (0==strcmp(ltkn,"ARITY")){
				glblopts.fweight=ARITY;
				usrparam_mask|=FUNCWMASK;
				usrstrat&=~FUNCWMASK;
				usrstrat|=ARITY_P;
				strategy=0xffffffffffffffff;
				*msg="Function weight has been set to ARITY";
			} else if (0==strcmp(ltkn,"DEFAULT")){
				usrparam_mask&=~FUNCWMASK;
				usrstrat&=~FUNCWMASK;
				*msg="Function weight has been set to DEFAULT";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing FUNCTIONW parameter";
					return(1);
				}
				if (0==(usrparam_mask&FUNCWMASK)){
					printf("Function weight is set to DEFAULT\n");
				} else {
					if (glblopts.fweight==UNIFORM){
						printf("Function weight UNIFORM\n");
					} else {
						printf("Function weight by ARITY\n");
					}
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid FUNCTIONW parameter, valid options are UNIFORM, ARITY or DEFAULT";
					return(1);
				}
				printf("Invalid FUNCTIONW parameter: %s\n",ltkn);
				printf("Valid options are UNIFORM, ARITY or DEFAULT\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing FUNCTIONW parameter";
				return(1);
			}
			if (0==(usrparam_mask&FUNCWMASK)){
				printf("Function weight is set to DEFAULT\n");
			} else {
				if (glblopts.fweight==UNIFORM){
					printf("Function weight UNIFORM\n");
				} else {
					printf("Function weight by ARITY\n");
				}
			}
		}

	/* SELECTRATIO command. */
	} else if (0==strcmp(ltkn,"SELECTRATIO")){
		ltkn=strtok(NULL,": \t");
		if (cc==1){
			if (NULL!=ltkn){
				jj=strtol(ltkn,&ptr1,10);
				if ((jj>=0)&&(ptr1[0]==0)){
					ii=1;
				} else {
					ii=0;
				}
			} else {
				ii=0;
			}
			if ((ii)&&(NULL!=(ltkn=strtok(NULL,": \t")))){
				nn=strtol(ltkn,&ptr1,10);
				if ((nn>0)&&(ptr1[0]==0)){
					queuerts[0]=jj;
					queuerts[1]=nn;
					usrparam_mask|=WARMASK;
					strategy=0xffffffffffffffff;
					if (pgmmode==INTERACTIVE){
						printf("SELECTRATIO value has been set to %d:%d\n",jj,nn);
					}
				} else {
					ii=0;
				}
			}
			if (ii==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="SELECTRATIO parameter missing or invalid.";
					return(1);
				}
				printf("SELECTRATIO parameter missing or invalid.\n");
				printf("Parameter must be a set of two positive integers of the form:\n");
				printf("  weight:age\n");
				printf("The second integer, corresponding to the age queue, must be strictly positive.\n");
			}
		} else if (cc==0){
			if (NULL!=ltkn){
				for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
					ltkn[ii]=toupper(ltkn[ii]);
				}
				if (0==strcmp(ltkn,"DEFAULT")){
					usrparam_mask&=~WARMASK;
					*msg="SELECRATIO has been set to DEFAULT";
				} else {
					if (pgmmode==NONINTERACTIVE){
						*msg="Invalid SELECTRATIO parameter.";
						return(1);
					}
					printf("Invalid SELECTRATIO parameter.\n");
					printf("Parameter must be a set of two positive integers of the form weight:age\n");
					printf("The second integer, corresponding to the age queue, must be strictly positive.\n");
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="SELECTRATIO parameter missing or invalid.";
					return(1);
				}
				if (usrparam_mask&WARMASK){
					printf("Current SELECTRATIO value is %d:%d\n",queuerts[0],queuerts[1]);
				} else {
					printf("Current SELECTRATIO value is set to DEFAULT.\n");
				}
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="SELECTRATIO parameter missing or invalid.";
				return(1);
			}
			printf("SELECTRATIO parameter missing or invalid.\n");
			printf("Parameter must be a set of two positive integers of the form weight:age\n");
			printf("The second integer, corresponding to the age queue, must be strictly positive.\n");
		}

	/* TIMEOUT command. */
	} else if (0==strcmp(ltkn,"TIMEOUT")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			ii=strtol(ltkn,&ptr1,10);
			if ((ii>0)&&(ptr1[0]==0)){
				glblopts.timeout=ii;
				if (pgmmode==INTERACTIVE){
					printf("Timeout value has been set to %.1f seconds\n",glblopts.timeout);
				}
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing timeout parameter";
					return(1);
				}
				printf("Current timeout value is %.1f seconds\n",glblopts.timeout);
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid timeout parameter";
					return(1);
				}
				printf("Invalid timeout parameter: %s\n",ltkn);
				printf("Parameter must be the timeout in seconds given as a strictly positive integer.\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing timeout parameter";
				return(1);
			}
			printf("Current timeout value is %.1f\n",glblopts.timeout);
		}

	/* MAXMEMORY command. */
	} else if (0==strcmp(ltkn,"MAXMEMORY")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			kk=strtoll(ltkn,&ptr1,10)*1000000L;
			if ((kk>0)&&(ptr1[0]==0)){
				memorylimit=kk;
				if (pgmmode==INTERACTIVE){
					printf("MAXMEMORY limit has been set to %d MB\n",ii);
				}
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing MAXMEMORY parameter";
					return(1);
				}
				printf("Current MAXMEMORY value is %lu MB\n",memorylimit/1000000);
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid MAXMEMORY parameter";
					return(1);
				}
				printf("Invalid MAXMEMORY parameter: %s\n",ltkn);
				printf("Parameter must be the memory limit in megabytes given as a strictly positive integer.\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing MAXMEMORY parameter";
				return(1);
			}
			printf("Current MAXMEMORY value is %lu MB\n",memorylimit/1000000);
		}

	/* HWINSTR command. */
	} else if (0==strcmp(ltkn,"HWINSTR")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			kk=strtoll(ltkn,&ptr1,10);
			if ((kk>0)&&(kk<=0x7fffffffffffffff)&&(ptr1[0]==0)){
				instrlimit=kk;
				if (pgmmode==INTERACTIVE){
					printf("HWINSTR limit has been set to %ld\n",kk);
				}
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing HWINSTR parameter";
					return(1);
				}
				if (instrlimit!=0xffffffffffffffff){
					printf("Current HWINSTR value is %lu\n",instrlimit);
				} else {
					printf("HWINSTR parameter has not been specified yet.\n");
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid HWINSTR parameter";
					return(1);
				}
				printf("Invalid HWINSTR parameter: %s\n",ltkn);
				printf("Parameter must maximum number of hardware instructions given as a strictly positive integer\n");
				printf("lesser or equal than %ld.\n",0x7fffffffffffffff);
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing HWINSTR parameter";
				return(1);
			}
			if (instrlimit!=0xffffffffffffffff){
				printf("Current HWINSTR value is %lu\n",instrlimit);
			} else {
				printf("HWINSTR parameter has not been specified yet.\n");
			}
		}

	/* STRATEGY command. */
	} else if (0==strcmp(ltkn,"STRATEGY")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"OFF")){
				usrparam_mask=0;
				strategy=0xffffffffffffffff;
				*msg="User strategy has been disabled";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing STRATEGY parameter";
					return(1);
				}
				PrintStrategy();
			} else {
				kk=strtoll(ltkn,&ptr1,10);
				if ((kk>=0)&&(kk<=0x7fffffffffffffff)&&(ptr1[0]==0)){
					strategy=kk;
					usrparam_mask=0;
					if (pgmmode==INTERACTIVE){
						printf("STRATEGY has been set to %ld\n",kk);
					}
					return(0);
				}
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid STRATEGY parameter";
					return(1);
				}
				printf("Invalid STRATEGY parameter: %s, valid parameters are OFF or a number between 0 and %ld\n",
						ltkn,0x7fffffffffffff);
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing STRATEGY parameter";
				return(1);
			}
			PrintStrategy();
		}

	/* GAMMA command. */
	} else if (0==strcmp(ltkn,"GAMMA")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			ff=strtof(ltkn,&ptr1);
			if ((ff>=0.0)&&(ptr1[0]==0)){
				kbset.opts.gamma=ff;
				usrparam_mask|=GAMMAMASK;
				strategy=0xffffffffffffffff;
				if (pgmmode==INTERACTIVE){
					printf("GAMMA has been set to %f\n",ff);
				}
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing GAMMA parameter";
					return(1);
				}
				if (usrparam_mask&GAMMAMASK){
					printf("Current GAMMA value is %f\n",kbset.opts.gamma);
				} else {
					printf("Current GAMMA value is DEFAULT\n");
				}
			} else {
				for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
					ltkn[ii]=toupper(ltkn[ii]);
				}
				if (0==strcmp(ltkn,"DEFAULT")){
					usrparam_mask&=~MAXWMASK;
					if (pgmmode==INTERACTIVE){
						printf("GAMMA has been set to DEFAULT\n");
					}
				} else {
					if (pgmmode==NONINTERACTIVE){
						*msg="Invalid GAMMMA parameter";
						return(1);
					}
					printf("Invalid GAMMA parameter: %s\n",ltkn);
					printf("Parameter must be a real number greater than or equal to zero.\n");
				}
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing GAMMA parameter";
				return(1);
			}
			if (usrparam_mask&GAMMAMASK){
				printf("Current GAMMA value is %f\n",kbset.opts.gamma);
			} else {
				printf("Current GAMMA value is DEFAULT\n");
			}
		}

	/* TMFACTOR command. */
	} else if (0==strcmp(ltkn,"TMFACTOR")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			ff=strtof(ltkn,&ptr1);
			if ((ff>0.0)&&(ff<=100.0)&&(ptr1[0]==0)){
				usrtmfactor=ff;
				if (pgmmode==INTERACTIVE){
					printf("TMFACTOR has been set to %f\n",ff);
				}
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing TMFACTOR parameter";
					return(1);
				}
				printf("Current TMFACTOR value is %f\n",usrtmfactor);
			} else {
				for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
					ltkn[ii]=toupper(ltkn[ii]);
				}
				if (0==strcmp(ltkn,"DEFAULT")){
					usrtmfactor=1.0;
					if (pgmmode==INTERACTIVE){
						printf("TMFACTOR has been set to DEFAULT value 1.0\n");
					}
				} else {
					if (pgmmode==NONINTERACTIVE){
						*msg="Invalid TMFACTOR parameter";
						return(1);
					}
					printf("Invalid TMFACTOR parameter: %s\n",ltkn);
					printf("Parameter must be a real number greater than 0.0 and less than or equal to 100.0\n");
				}
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing TMFACTOR parameter";
				return(1);
			}
			printf("Current TMFACTOR value is %f\n",usrtmfactor);
		}

	/* RELEVANCE command. */
	} else if (0==strcmp(ltkn,"RELEVANCE")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"OFF")){
				prproflags=CLFILTERINGOFF;
				jj=0;
				*msg="Relevance filtering has been set to OFF";
			} else if (0==strcmp(ltkn,"ON")){
				jj=CLFILTERINGON;
			} else if (0==strcmp(ltkn,"NONEQU")){
				jj=CLFILTERINGNONEQU;
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid RELEVANCE parameter";
					return(1);
				}
				printf("Invalid RELEVANCE parameter: %s\n",ltkn);
				printf("The first parameter must be OFF, ON or NONEQU.\n");
				jj=0;
			}
			if (jj){
				if (NULL!=(ltkn=strtok(NULL," \t"))){
					ff=strtol(ltkn,&ptr1,10);
					if (ptr1[0]==0){
						if (ff>=0){
							jj|=CLFILTERINGITERLIMIT;
						} else {
							if (pgmmode==NONINTERACTIVE){
								*msg="Invalid RELEVANCE threshold parameter";
								return(1);
							}
							printf("Invalid threshold parameter: %s\n",ltkn);
							printf("Threshold must be a positive number in float or integer format.\n");
							jj=0;
						}
					} else {
						ff=strtof(ltkn,&ptr1);
						if (ptr1[0]==0){
							if (ff<0.0){
								if (pgmmode==NONINTERACTIVE){
									*msg="Invalid RELEVANCE threshold parameter";
									return(1);
								}
								printf("Invalid threshold parameter: %s\n",ltkn);
								printf("Threshold must be a positive number in float or integer format.\n");
								jj=0;
							} else if (ff==0){
								jj|=CLFILTERINGITERLIMIT;
							}
						} else {
							if (pgmmode==NONINTERACTIVE){
								if (ltkn[0]==0){
									*msg="Missing threshold value";
								} else {
									*msg="Invalid RELEVANCE threshold parameter";
								}
								return(1);
							}
							jj=0;
							if (ltkn[0]==0){
								printf("Missing threshold value\n");
							} else {
								printf("Invalid threshold parameter: %s\n",ltkn);
								printf("Threshold must be a positive number in float or integer format.\n");
							}
						}
					}
					if (jj){
						if (NULL!=(ltkn=strtok(NULL," \t"))){
							for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
								ltkn[ii]=toupper(ltkn[ii]);
							}
							if (0==strcmp(ltkn,"RESIGN")){
								ppthrshld=ff;
								prproflags=jj|CLFILTERINGRESIGN;
							} else {
								if (pgmmode==NONINTERACTIVE){
									*msg="Invalid parameter. The only optional final parameter is RESIGN";
									return(1);
								}
								printf("Invalid parameter: %s\n",ltkn);
								printf("The only optional final parameter is RESIGN.\n");
								jj=0;
							}
						} else {
							ppthrshld=ff;
							prproflags=jj;
						}
						if ((jj)&&(pgmmode==INTERACTIVE)){
							printf("Relevance filtering has been set to ");
							if (prproflags&CLFILTERINGON){
								printf("ON ");
							} else {
								printf("NONEQU ");
							}
							if (prproflags&CLFILTERINGITERLIMIT){
								printf("%ld",(int64_t)ppthrshld);
							} else {
								printf("%f",ppthrshld);
							}
							if (prproflags&CLFILTERINGRESIGN){
								printf(" RESIGN");
							}
							printf("\n");
						}
					}
				} else {
					*msg="Missing RELEVANCE threshold parameter";
					if (pgmmode==NONINTERACTIVE){
						return(1);
					}
				}
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing RELEVANCE parameters";
				return(1);
			}
			printf("Current relevance filtering setting is ");
			if (prproflags&CLFILTERINGOFF){
				printf("OFF\n");
			} else {
				if (prproflags&CLFILTERINGON){
					printf("ON ");
				} else {
					printf("NONEQU ");
				}
				if (prproflags&CLFILTERINGITERLIMIT){
					printf("%ld",(int64_t)ppthrshld);
				} else {
					printf("%f",ppthrshld);
				}
				if (prproflags&CLFILTERINGRESIGN){
					printf(" RESIGN");
				}
				printf("\n");
			}
		}

	/* CORES command. */
	} else if (0==strcmp(ltkn,"CORES")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			ii=strtol(ltkn,&ptr1,10);
			if ((ii>=0)&&(ptr1[0]==0)){
				if (ii<=num_cores){
					if (ii==0){
						ii=num_cores;
					}
					nn=0;
				} else {
					ii=num_cores;
					nn=1;
				}
				active_cores=ii;
				procnb=active_cores-1;
				if (pgmmode==INTERACTIVE){
					if (nn){
						printf("Only %d core(s) are available.\nOnly %d core(s) enabled\n",num_cores,num_cores);
					} else {
						printf("Now using %d core(s). %d core(s) available.\n",active_cores,num_cores);
					}
				}
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing CORES parameter";
					return(1);
				}
				printf("Using %d cores. %d cores available.\n",active_cores,num_cores);
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid CORES parameter, parameter must be a positive integer";
					return(1);
				}
				printf("Invalid CORES parameter: %s\n",ltkn);
				printf("Parameter must be a positive integer.\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing CORES parameter";
				return(1);
			}
			printf("Using %d cores. %d cores available.\n",active_cores,num_cores);
		}

	/* MAXWEIGHT command. */
	} else if (0==strcmp(ltkn,"MAXWEIGHT")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"OFF")){
				glblopts.mxwghtopt|=MXWEIGHTOFF;
				glblopts.mxwghtopt&=(~MXWEIGHTNUM);
				*msg="MAXWEIGHT has been set to OFF";
				glblopts.maxweight=ABSMAXWEIGHT;
			} else if (0==strcmp(ltkn,"LRS")){
				glblopts.mxwghtopt|=MXWEIGHTLRS;
				glblopts.mxwghtopt&=(~(MXWEIGHTLRSOTT|MXWEIGHTLRSDSC|MXWEIGHTLRSOFF));
				usrparam_mask|=MAXWMASK;
				strategy=0xffffffffffffffff;
				*msg="MAXWEIGHT has been set to LRS";
			} else if (0==strcmp(ltkn,"LRSOTTER")){
				glblopts.mxwghtopt|=MXWEIGHTLRSOTT;
				glblopts.mxwghtopt&=(~(MXWEIGHTLRS|MXWEIGHTLRSDSC|MXWEIGHTLRSOFF));
				usrparam_mask|=MAXWMASK;
				strategy=0xffffffffffffffff;
				*msg="MAXWEIGHT has been set to LRSOTTER";
			} else if (0==strcmp(ltkn,"LRSDISCOUNT")){
				glblopts.mxwghtopt|=MXWEIGHTLRSDSC;
				glblopts.mxwghtopt&=(~(MXWEIGHTLRS|MXWEIGHTLRSOTT|MXWEIGHTLRSOFF));
				usrparam_mask|=MAXWMASK;
				strategy=0xffffffffffffffff;
				*msg="MAXWEIGHT has been set to LRSDISCOUNT";
			} else if (0==strcmp(ltkn,"LRSOFF")){
				glblopts.mxwghtopt|=MXWEIGHTLRSOFF;
				glblopts.mxwghtopt&=(~(MXWEIGHTLRS|MXWEIGHTLRSOTT|MXWEIGHTLRSDSC));
				usrparam_mask|=MAXWMASK;
				strategy=0xffffffffffffffff;
				*msg="MAXWEIGHT has been set to LRSOFF";
			} else if (0==strcmp(ltkn,"DEFAULT")){
				glblopts.mxwghtopt&=(~(MXWEIGHTLRS|MXWEIGHTLRSOTT|MXWEIGHTLRSOFF|MXWEIGHTLRSDSC));
				usrparam_mask&=~MAXWMASK;
				*msg="MAXWEIGHT has been set to DEFAULT";
			} else {
				ii=strtol(ltkn,&ptr1,10);
				if ((ii>0)&&(ptr1[0]==0)){
					glblopts.mxwghtopt|=MXWEIGHTNUM;
					glblopts.mxwghtopt&=(~MXWEIGHTOFF);
					glblopts.maxweight=ii*10;
					if (pgmmode==INTERACTIVE){
						printf("MAXWEIGHT has been set to %d.\n",ii);
					}
				} else if (ltkn[0]==0){
					if (pgmmode==NONINTERACTIVE){
						*msg="Missing MAXWEIGHT parameter";
						return(1);
					}
					if (glblopts.mxwghtopt&MXWEIGHTOFF){
						printf("MAXWEIGHT is set to OFF and ");
					} else {
						printf("MAXWEIGHT is set to %d and ",glblopts.maxweight/10);
					}
					if (0==(usrparam_mask&MAXWMASK)){
						printf("LRS DEFAULT\n");
					} else {
						switch (glblopts.mxwghtopt&(MXWEIGHTLRS|MXWEIGHTLRSOTT|MXWEIGHTLRSDSC|MXWEIGHTLRSOFF)){
							case MXWEIGHTLRS:
								printf("LRS.\n");
								break;
							case MXWEIGHTLRSOFF:
								printf("LRSOFF.\n");
								break;
							case MXWEIGHTLRSOTT:
								printf("LRSOTTER.\n");
								break;
							default:
								printf("LRSDISCOUNT.\n");
								break;
						}
					}
				} else {
					if (pgmmode==NONINTERACTIVE){
						*msg="Invalid MAXWEIGHT parameter";
						return(1);
					}
					printf("Invalid MAXWEIGHT parameter: %s\n",ltkn);
					printf("Parameter must be OFF, LRS, LRSOTTER, LRSDISCOUNT, an unsigned integer or DEFAULT.\n");
				}
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing MAXWEIGHT parameter";
				return(1);
			}
			if (glblopts.mxwghtopt&MXWEIGHTOFF){
				printf("MAXWEIGHT is set to OFF and ");
			} else {
				printf("MAXWEIGHT is set to %d and ",glblopts.maxweight/10);
			}
			if (0==(usrparam_mask&MAXWMASK)){
				printf("LRS DEFAULT\n");
			} else {
				switch (glblopts.mxwghtopt&(MXWEIGHTLRS|MXWEIGHTLRSOTT|MXWEIGHTLRSDSC|MXWEIGHTLRSOFF)){
					case MXWEIGHTLRS:
						printf("LRS.\n");
						break;
					case MXWEIGHTLRSOFF:
						printf("LRSOFF.\n");
						break;
					case MXWEIGHTLRSOTT:
						printf("LRSOTTER.\n");
						break;
					default:
						printf("LRSDISCOUNT.\n");
						break;
				}
			}
		}

	/* DEMODULATION command. */
	} else if (0==strcmp(ltkn,"DEMODULATION")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"ON")){
				glblopts.demodulation=DEMODON;
				usrparam_mask|=DEMODMASK;
				strategy=0xffffffffffffffff;
				*msg="Demodulation has been enabled";
			} else if (0==strcmp(ltkn,"ONORNT")){
				glblopts.demodulation=DEMODONORNT;
				usrparam_mask|=DEMODMASK;
				strategy=0xffffffffffffffff;
				*msg="Demodulation has been enabled only from oriented equalities";
			} else if (0==strcmp(ltkn,"OFF")){
				glblopts.demodulation=DEMODOFF;
				usrparam_mask|=DEMODMASK;
				strategy=0xffffffffffffffff;
				*msg="Demodulation has been disabled";
			} else if (0==strcmp(ltkn,"CPL1")){
				glblopts.demodulation=DEMODCPL1;
				usrparam_mask|=DEMODMASK;
				strategy=0xffffffffffffffff;
				*msg="Encompassment demodulation is enabled and will preserve completeness";
			} else if (0==strcmp(ltkn,"CPL1ORNT")){
				glblopts.demodulation=DEMODCPL1ORNT;
				usrparam_mask|=DEMODMASK;
				strategy=0xffffffffffffffff;
				*msg="Encompassment demodulation is enabled only from oriented equalities and will preserve completeness";
			} else if (0==strcmp(ltkn,"CPL2")){
				glblopts.demodulation=DEMODCPL2;
				usrparam_mask|=DEMODMASK;
				strategy=0xffffffffffffffff;
				*msg="Demodulation is enabled and will preserve completeness";
			} else if (0==strcmp(ltkn,"CPL2ORNT")){
				glblopts.demodulation=DEMODCPL2ORNT;
				usrparam_mask|=DEMODMASK;
				strategy=0xffffffffffffffff;
				*msg="Demodulation is enabled only from oriented equalities and will preserve completeness";
			} else if (0==strcmp(ltkn,"DEFAULT")){
				usrparam_mask&=~DEMODMASK;
				*msg="Demodulation has been set to DEFAULT";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing DEMODULATION parameter";
					return(1);
				}
				if (0==(usrparam_mask&DEMODMASK)){
					printf("Demodulation check is set to DEFAULT\n");
				} else {
					if (glblopts.demodulation==DEMODON){
						printf("Demodulation is set to ON\n");
					} else if (glblopts.demodulation==DEMODONORNT){
						printf("Demodulation is set to ON only from oriented equalities\n");
					} else if (glblopts.demodulation==DEMODCPL1){
						printf("Demodulation is set to encompassment demodulation\n");
					} else if (glblopts.demodulation==DEMODCPL1ORNT){
						printf("Demodulation is set to encompassment demodulation only from oriented equalities\n");
					} else if (glblopts.demodulation==DEMODCPL2){
						printf("Demodulation is set to another complete algorithm\n");
					} else if (glblopts.demodulation==DEMODCPL2ORNT){
						printf("Demodulation is set to another complete algorithm only from oriented equalities\n");
					} else {
						printf("Demodulation is set to OFF\n");
					}
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid DEMODULATION parameter";
					return(1);
				}
				printf("Invalid DEMODULATION option: %s\n",ltkn);
				printf("Valid options are ON, OFF, CPL1, CPL2, ONORNT, CPL1ORNT, CPL2ORNT or DEFAULT\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing DEMODULATION parameter";
				return(1);
			}
			if (0==(usrparam_mask&DEMODMASK)){
				printf("Demodulation check is set to DEFAULT\n");
			} else {
				if (glblopts.demodulation==DEMODON){
					printf("Demodulation is set to ON\n");
				} else if (glblopts.demodulation==DEMODONORNT){
					printf("Demodulation is set to ON only from oriented equalities\n");
				} else if (glblopts.demodulation==DEMODCPL1){
					printf("Demodulation is set to encompassment demodulation\n");
				} else if (glblopts.demodulation==DEMODCPL1ORNT){
					printf("Demodulation is set to encompassment demodulation only from oriented equalities\n");
				} else if (glblopts.demodulation==DEMODCPL2){
					printf("Demodulation is set to another complete algorithm\n");
				} else if (glblopts.demodulation==DEMODCPL2ORNT){
					printf("Demodulation is set to another complete algorithm only from oriented equalities\n");
				} else {
					printf("Demodulation is set to OFF\n");
				}
			}
		}

	/* LOOKAHEAD command. */
	} else if (0==strcmp(ltkn,"LOOKAHEAD")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"ON")){
				glblopts.lookahead=1;
				usrparam_mask|=LOOKAHEADMASK;
				strategy=0xffffffffffffffff;
				*msg="Lookahead has been enabled";
			} else if (0==strcmp(ltkn,"OFF")){
				glblopts.lookahead=0;
				usrparam_mask|=LOOKAHEADMASK;
				strategy=0xffffffffffffffff;
				*msg="Lookahead has been disabled";
			} else if (0==strcmp(ltkn,"DEFAULT")){
				usrparam_mask&=~LOOKAHEADMASK;
				*msg="Lookahead has been set to DEFAULT";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing LOOKAHEAD parameter";
					return(1);
				}
				if (0==(usrparam_mask&LOOKAHEADMASK)){
					printf("Lookahead has been set to DEFAULT\n");
				} else {
					if (glblopts.lookahead==1){
						printf("Lookahead is set to ON\n");
					} else {
						printf("Lookahead is set to OFF\n");
					}
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid LOOKAHEAD parameter";
					return(1);
				}
				printf("Invalid LOOKAHEAD option: %s\n",ltkn);
				printf("Valid options are ON, OFF or DEFAULT\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing LOOKAHEAD parameter";
				return(1);
			}
			if (0==(usrparam_mask&LOOKAHEADMASK)){
				printf("Lookahead has been set to DEFAULT\n");
			} else {
				if (glblopts.lookahead==1){
					printf("Lookahead is set to ON\n");
				} else {
					printf("Lookahead is set to OFF\n");
				}
			}
		}

	/* WEAKRW command. */
	} else if (0==strcmp(ltkn,"WEAKRW")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"ON")){
				glblopts.weakrw=1;
				usrparam_mask|=WEAKRWMASK;
				usrstrat&=~WEAKRWMASK;
				usrstrat|=WEAKRWON_P;
				strategy=0xffffffffffffffff;
				*msg="Weak rewriting has been enabled";
			} else if (0==strcmp(ltkn,"OFF")){
				glblopts.weakrw=0;
				usrparam_mask|=WEAKRWMASK;
				usrstrat&=~WEAKRWMASK;
				usrstrat|=WEAKRWOFF_P;
				strategy=0xffffffffffffffff;
				*msg="Weak rewriting has been disabled";
			} else if (0==strcmp(ltkn,"DEFAULT")){
				usrparam_mask&=~WEAKRWMASK;
				usrstrat&=~WEAKRWMASK;
				*msg="Weak rewriting has been set to DEFAULT";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing WEAKRW parameter";
					return(1);
				}
				if (0==(usrparam_mask&WEAKRWMASK)){
					printf("Weak rewriting has been set to DEFAULT\n");
				} else {
					if (glblopts.weakrw==1){
						printf("Weak rewriting is set to ON\n");
					} else {
						printf("Weak rewriting is set to OFF\n");
					}
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid WEAKRW parameter";
					return(1);
				}
				printf("Invalid WEAKRW option: %s\n",ltkn);
				printf("Valid options are ON, OFF or DEFAULT\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing WEAKRW parameter";
				return(1);
			}
			if (0==(usrparam_mask&WEAKRWMASK)){
				printf("Weak rewriting has been set to DEFAULT\n");
			} else {
				if (glblopts.lookahead==1){
					printf("Weak rewriting is set to ON\n");
				} else {
					printf("Weak rewriting is set to OFF\n");
				}
			}
		}

	/* GRJOIN command. */
	} else if (0==strcmp(ltkn,"GRJOIN")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"ON")){
				glblopts.grjoin=1;
				usrparam_mask|=GRJOINMASK;
				strategy=0xffffffffffffffff;
				*msg="GRJOIN has been enabled";
			} else if (0==strcmp(ltkn,"OFF")){
				glblopts.grjoin=0;
				usrparam_mask|=GRJOINMASK;
				strategy=0xffffffffffffffff;
				*msg="GRJOIN has been disabled";
			} else if (0==strcmp(ltkn,"MED")){
				glblopts.grjoin=2;
				usrparam_mask|=GRJOINMASK;
				strategy=0xffffffffffffffff;
				*msg="GRJOIN has been set to MED";
			} else if (0==strcmp(ltkn,"DEFAULT")){
				usrparam_mask&=~GRJOINMASK;
				*msg="GRJOIN has been set to DEFAULT";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing GRJOIN parameter";
					return(1);
				}
				if (0==(usrparam_mask&GRJOINMASK)){
					printf("GRJOIN has been set to DEFAULT\n");
				} else {
					if (glblopts.grjoin==1){
						printf("GRJOIN is set to ON\n");
					} else if (glblopts.grjoin==2){
						printf("GRJOIN is set to MED\n");
					} else {
						printf("GRJOIN is set to OFF\n");
					}
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid GRJOIN parameter";
					return(1);
				}
				printf("Invalid GRJOIN option: %s\n",ltkn);
				printf("Valid options are ON, MED, OFF or DEFAULT\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing GRJOIN parameter";
				return(1);
			}
			if (0==(usrparam_mask&GRJOINMASK)){
				printf("GRJOIN has been set to DEFAULT\n");
			} else {
				if (glblopts.grjoin==1){
					printf("GRJOIN is set to ON\n");
				} else if (glblopts.grjoin==2){
					printf("GRJOIN is set to MED\n");
				} else {
					printf("GRJOIN is set to OFF\n");
				}
			}
		}

	/* SATMODE command. */
	} else if (0==strcmp(ltkn,"SATMODE")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"ON")){
				glblopts.satmode=1;
				*msg="SATMODE has been set to ON";
			} else if (0==strcmp(ltkn,"OFF")){
				glblopts.satmode=0;
				*msg="SATMODE has been set to OFF";
			} else if (0==strcmp(ltkn,"MED")){
				glblopts.satmode=2;
				*msg="SATMODE has been set to MED";
			} else if (0==strcmp(ltkn,"DEFAULT")){
				glblopts.satmode=0;
				*msg="SATMODE has been set to default (OFF)";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing SATMODE parameter";
					return(1);
				}
				if (glblopts.satmode==1){
					printf("SATMODE is set to ON\n");
				} else if (glblopts.satmode==2){
					printf("SATMODE is set to MED\n");
				} else {
					printf("SATMODE is set to OFF\n");
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid SATMODE parameter";
					return(1);
				}
				printf("Invalid SATMODE option: %s\n",ltkn);
				printf("Valid options are ON, MED, OFF or DEFAULT\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing GRJOIN parameter";
				return(1);
			}
			if (glblopts.satmode==1){
				printf("SATMODE is set to ON\n");
			} else if (glblopts.satmode==2){
				printf("SATMODE is set to MED\n");
			} else {
				printf("SATMODE is set to OFF\n");
			}
		}

	/* CONNECT command. */
	} else if (0==strcmp(ltkn,"CONNECT")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"ON")){
				glblopts.connect=1;
				usrparam_mask|=CONNECTMASK;
				strategy=0xffffffffffffffff;
				*msg="CONNECT has been enabled";
			} else if (0==strcmp(ltkn,"OFF")){
				glblopts.connect=0;
				usrparam_mask|=CONNECTMASK;
				strategy=0xffffffffffffffff;
				*msg="CONNECT has been disabled";
			} else if (0==strcmp(ltkn,"DEFAULT")){
				usrparam_mask&=~CONNECTMASK;
				*msg="CONNECT has been set to DEFAULT";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing CONNECT parameter";
					return(1);
				}
				if (0==(usrparam_mask&GRJOINMASK)){
					printf("CONNECT has been set to DEFAULT\n");
				} else {
					if (glblopts.connect==1){
						printf("CONNECT is set to ON\n");
					} else {
						printf("CONNECT is set to OFF\n");
					}
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid CONNECT parameter";
					return(1);
				}
				printf("Invalid CONNECT option: %s\n",ltkn);
				printf("Valid options are ON, OFF or DEFAULT\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing CONNECT parameter";
				return(1);
			}
			if (0==(usrparam_mask&GRJOINMASK)){
				printf("CONNECT has been set to DEFAULT\n");
			} else {
				if (glblopts.connect==1){
					printf("CONNECT is set to ON\n");
				} else {
					printf("CONNECT is set to OFF\n");
				}
			}
		}

	/* LOADREDUNDANT command. */
	} else if (0==strcmp(ltkn,"LOADREDUNDANT")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"ON")){
				loadredundant=1;
				*msg="LOADREDUNDANT has been set to ON";
			} else if (0==strcmp(ltkn,"OFF")){
				loadredundant=0;
				*msg="LOADREDUNDANT has been set to OFF";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing LOADREDUNDANT parameter";
					return(1);
				}
				if (loadredundant==1){
					printf("LOADREDUNDANT is ON\n");
				} else {
					printf("LOADREDUNDANT is OFF\n");
				}
			} else {
				printf("Invalid LOADREDUNDANT option: %s\n",ltkn);
				printf("Valid options are ON or OFF\n");
				return(pgmmode==INTERACTIVE?0:1);
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing LOADREDUNDANT parameter";
				return(1);
			}
			if (loadredundant==1){
				printf("LOADREDUNDANT is ON\n");
			} else {
				printf("Equivalent factoring is OFF\n");
			}
		}

	/* VERBOSE command. */
	} else if (0==strcmp(ltkn,"VERBOSE")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"ON")){
				verbose=1;
				*msg="VERBOSE has been set to ON";
			} else if (0==strcmp(ltkn,"ALL")){
				verbose=2;
				*msg="VERBOSE has been set to ALL";
			} else if (0==strcmp(ltkn,"OFF")){
				verbose=0;
				*msg="VERBOSE has been set to OFF";
			} else if (ltkn[0]==0){
				switch (verbose){
					case 2:
						printf("VERBOSE is ALL\n");
						break;
					case 0:
						printf("VERBOSE is OFF\n");
						break;
					default:
						printf("VERBOSE is ON\n");
						break;
				}
				return(pgmmode==INTERACTIVE?0:1);
			} else {
				printf("Invalid VERBOSE option: %s\n",ltkn);
				printf("Valid options are ALL, ON or OFF\n");
				return(pgmmode==INTERACTIVE?0:1);
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing VERBOSE parameter";
				return(1);
			}
			switch (verbose){
				case 2:
					printf("VERBOSE is ALL\n");
					break;
				case 0:
					printf("VERBOSE is OFF\n");
					break;
				default:
					printf("VERBOSE is ON\n");
					break;
			}
		}

	/* LEARNTO command. */
	} else if (0==strcmp(ltkn,"LEARNTO")){
		if (NULL!=(ltkn=strtok(NULL,""))){
			if (strlen(ltkn)==3){
				if (('O'==toupper(ltkn[0]))&&('F'==toupper(ltkn[1]))&&('F'==toupper(ltkn[2]))){
					if (learnto!=NULL){
						MYFREE(learnto);
						learnto=NULL;
					}
					*msg="LEARNTO has been set to OFF";
					return(0);
				}
			}
			if (NULL==(ptr1=ParseFilename(ltkn,&ptr2))){
				*msg="Invalid LEARNTO parameter. It must be OFF or a valid filename";
				return(pgmmode==INTERACTIVE?0:1);
			}
			if (NULL!=(ltkn=strtok(ptr2," \t"))){
				if (strlen(ltkn)==3){
					if (('N'==toupper(ltkn[0]))&&('E'==toupper(ltkn[1]))&&('W'==toupper(ltkn[2]))){
						learnnew=1;
					} else {
						printf("Invalid LEARNTO parameter %s\n",ltkn);
						return(pgmmode==INTERACTIVE?0:1);
					}
				} else {
					printf("Invalid LEARNTO parameter %s\n",ltkn);
					return(pgmmode==INTERACTIVE?0:1);
				}
			} else {
				learnnew=0;
			}
			if (NULL==(ptr3=MYSTRDUP(ptr1))){
				*msg="No memory available, exiting program";
				return(1);
			}
			if (learnto!=NULL){
				MYFREE(learnto);
			}
			learnto=ptr3;
			if (pgmmode==INTERACTIVE){
				if (learnnew){
					printf("LEARNTO has been set to %s NEW\n",learnto);
				} else {
					printf("LEARNTO has been set to %s\n",learnto);
				}
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing LEARNTO parameter";
				return(1);
			}
			if (learnto==NULL){
				printf("LEARNTO is OFF\n");
			} else {
				if (learnnew){
					printf("LEARNTO is set to %s NEW\n",learnto);
				} else {
					printf("LEARNTO is set to %s\n",learnto);
				}
			}
		}

	/* LEARNFROM command. */
	} else if (0==strcmp(ltkn,"LEARNFROM")){
		if (NULL!=(ltkn=strtok(NULL,""))){
			if (strlen(ltkn)==3){
				if (('O'==toupper(ltkn[0]))&&('F'==toupper(ltkn[1]))&&('F'==toupper(ltkn[2]))){
					if (learnfrom!=NULL){
						MYFREE(learnfrom);
						learnfrom=NULL;
					}
					*msg="LEARNFROM has been set to OFF";
					return(0);
				}
			}
			if (NULL==(ptr1=ParseFilename(ltkn,&ptr2))){
				*msg="Invalid LEARNFROM parameter. It must be OFF or a valid filename.";
				return(pgmmode==INTERACTIVE?0:1);
			}
			if (NULL==(ptr3=MYSTRDUP(ptr1))){
				*msg="No memory available, exiting program";
				return(1);
			}
			if (learnfrom!=NULL){
				MYFREE(learnfrom);
			}
			learnfrom=ptr3;
			if (pgmmode==INTERACTIVE){
				printf("LEARNFROM has been set to %s\n",learnfrom);
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing LEARNFROM parameter";
				return(1);
			}
			if (learnfrom==NULL){
				printf("LEARNFROM is OFF\n");
			} else {
				printf("LEARNFROM is set to %s\n",learnfrom);
			}
		}

	/* LEARN command. */
	} else if (0==strcmp(ltkn,"LEARN")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"ON")){
				glblopts.learn=1;
				usrparam_mask|=LEARNMASK;
				strategy=0xffffffffffffffff;
				*msg="Learning has been enabled";
			} else if (0==strcmp(ltkn,"OFF")){
				glblopts.learn=0;
				usrparam_mask|=LEARNMASK;
				strategy=0xffffffffffffffff;
				*msg="Learnt has been disabled";
			} else if (0==strcmp(ltkn,"DEFAULT")){
				usrparam_mask&=~LEARNMASK;
				*msg="Learn has been set to DEFAULT";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing LEARN parameter";
					return(1);
				}
				if (glblopts.learn==0){
					if (usrparam_mask&LEARNMASK){
						printf("Learn is disabled.\n");
					} else {
						printf("Learn is set to default value.\n");
					}
				} else {
					printf("Learn is enabled.\n");
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid LEARN parameter";
					return(1);
				}
				printf("Invalid LEARN option: %s\n",ltkn);
				printf("Valid options are ON or OFF.\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing LEARN parameter";
				return(1);
			}
			if (glblopts.learn==0){
				printf("Learn is disabled.\n");
			} else {
				printf("Learn is enabled.\n");
			}
		}

	/* PROCLEARN command. */
	} else if (0==strcmp(ltkn,"PROCLEARN")){
		if (NULL!=(ltkn=strtok(NULL,""))){
			if (NULL==(ptr1=ParseFilename(ltkn,&ptr3))){
				printf("Invalid PROCLEARN input file name: %s\n",ltkn);
				return(pgmmode==INTERACTIVE?0:1);
			}
			if (NULL==(ptr2=ParseFilename(ptr3,&ptr4))){
				printf("Invalid PROCLEARN output file name: %s\n",ptr3);
				return(pgmmode==INTERACTIVE?0:1);
			}
			if (ProcessLearnData(ptr1,ptr2)){
				if (pgmmode==NONINTERACTIVE){
					return(1);
				}
			}
		} else {
			printf("Missing PROCLEARN filename parameter, no process done.\n");
			return(pgmmode==INTERACTIVE?0:1);
		}

	/* SPLIT command. */
	} else if (0==strcmp(ltkn,"SPLIT")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"ON")){
				glblopts.split=1;
				usrparam_mask|=SPLITMASK;
				usrstrat&=~SPLITMASK;
				usrstrat|=SPLITON_P;
				strategy=0xffffffffffffffff;
				*msg="Split has been set to ON";
			} else if (0==strcmp(ltkn,"OFF")){
				glblopts.split=0;
				usrparam_mask|=SPLITMASK;
				usrstrat&=~SPLITMASK;
				usrstrat|=SPLITOFF_P;
				strategy=0xffffffffffffffff;
				*msg="Split has been set to OFF";
			} else if (0==strcmp(ltkn,"DEFAULT")){
				usrparam_mask&=~SPLITMASK;
				usrstrat&=~SPLITMASK;
				*msg="Split has been set to DEFAULT";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing SPLIT parameter";
					return(1);
				}
				if (glblopts.split==0){
					if (usrparam_mask&SPLITMASK){
						printf("Split is OFF, interface between first order prover\n");
						printf("and SAT solver is disabled.\n");
					} else {
						printf("Split is set to default value.\n");
					}
				} else {
					printf("Split is ON, interface between first order prover\n");
					printf("and SAT solver is enabled.\n");
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid SPLIT parameter";
					return(1);
				}
				printf("Invalid SPLIT option: %s\n",ltkn);
				printf("Valid options are ON or OFF.\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing SPLIT parameter";
				return(1);
			}
			if (glblopts.split==0){
				printf("Split is OFF, interface between first order prover\n");
				printf("and SAT solver is disabled.\n");
			} else {
				printf("Split is ON, interface between first order prover\n");
				printf("and SAT solver is enabled.\n");
			}
		}

	/* GOALXFORM command. */
	} else if (0==strcmp(ltkn,"GOALXFORM")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"ON")){
				glblopts.goalxform=1;
				usrparam_mask|=GOALXFRMMASK;
				usrstrat&=~GOALXFRMMASK;
				usrstrat|=GOALXON_P;
				strategy=0xffffffffffffffff;
				*msg="Goal transformation has been set to ON";
			} else if (0==strcmp(ltkn,"OFF")){
				glblopts.goalxform=0;
				usrparam_mask|=GOALXFRMMASK;
				usrstrat&=~GOALXFRMMASK;
				usrstrat|=GOALXOFF_P;
				strategy=0xffffffffffffffff;
				*msg="Goal transformation has been set to OFF";
			} else if (0==strcmp(ltkn,"DEFAULT")){
				usrparam_mask&=~GOALXFRMMASK;
				usrstrat&=~GOALXFRMMASK;
				*msg="Goal transformation has been set to DEFAULT";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing GOALXFORM parameter";
					return(1);
				}
				if (glblopts.goalxform==0){
					if (usrparam_mask&GOALXFRMMASK){
						printf("Goal transformation is OFF.\n");
					} else {
						printf("Goal transformation is set to default value.\n");
					}
				} else {
					printf("Goal transformation is ON.\n");
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid GOALXFORM parameter";
					return(1);
				}
				printf("Invalid GOALXFORM option: %s\n",ltkn);
				printf("Valid options are ON or OFF.\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing GOALXFORM parameter";
				return(1);
			}
			if (glblopts.goalxform==0){
				printf("Goal transformation is OFF.\n");
			} else {
				printf("Goal transformation is ON.\n");
			}
		}

	/* SHUFFLE command. */
	} else if (0==strcmp(ltkn,"SHUFFLE")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0;ii<strlen(ltkn);ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"ON")){
				glblopts.shuffle=1;
				*msg="Shuffling has been enabled";
			} else if (0==strcmp(ltkn,"OFF")){
				glblopts.shuffle=0;
				*msg="Shuffling has been disabled";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing SHUFFLE parameter";
					return(1);
				}
				if (glblopts.shuffle==0){
					printf("Shuffling is disabled\n");
				} else {
					printf("Shuffling is enabled\n");
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid SHUFFLE parameter";
					return(1);
				}
				printf("Invalid SHUFFLE option: %s\n",ltkn);
				printf("Valid options are ON or OFF.\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing SHUFFLE parameter";
				return(1);
			}
			if (glblopts.shuffle==0){
				printf("Shuffling is disabled\n");
			} else {
				printf("Shuffling is enabled\n");
			}
		}

	/* PERFDATA command. */
	} else if (0==strcmp(ltkn,"PERFDATA")){
		GetPerfData();

	/* PERFMON command. */
	} else if (0==strcmp(ltkn,"PERFMON")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0;ii<strlen(ltkn);ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"ON")){
				jj=1;
			} else if (0==strcmp(ltkn,"OFF")){
				jj=0;
			} else {
				jj=-1;
			}
			if (jj!=-1){
				if (NULL!=(ltkn=strtok(NULL," \t"))){
					kk=strtoll(ltkn,&ptr1,10);
					if ((kk>0)&&(kk<=0x7fffffffffffffff)&&(ptr1[0]==0)){
						perfmonrate=kk;
						ii=1;
					} else {
						ii=-1;
						*msg="Invalid PERFMON parameter";
					}
				} else {
					ii=0;
				}
				if (ii>=0){
					perfmonstate=1;
					if (jj){
						perfmon=1;
						if (ii==0){
							*msg="Performance monitoring has been enabled";
						} else {
							*msg="Performance monitoring has been enabled, instruction rate has been set";
						}
					} else {
						perfmon=0;
						if (ii==0){
							*msg="Performance monitoring has been disabled";
						} else {
							*msg="Performance monitoring has been disabled, instruction rate has been set";
						}
					}
				}
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing PERFMON parameter";
					return(1);
				}
				if (perfmon==0){
					printf("Performance monitoring is disabled, instruction rate is set to %ld\n",perfmonrate);
				} else {
					printf("Performance monitoring is enabled, instruction rate is set to %ld\n",perfmonrate);
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid PERFMON parameter";
					return(1);
				}
				printf("Invalid PERFMON option: %s\n",ltkn);
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing PERFMON parameter";
				return(1);
			}
			if (perfmon==0){
				printf("Performance monitoring is disabled, instruction rate is set to %ld\n",perfmonrate);
			} else {
				printf("Performance monitoring is enabled, instruction rate is set to %ld\n",perfmonrate);
			}
		}

	/* FACTORING command. */
	} else if (0==strcmp(ltkn,"FACTORING")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"ON")){
				glblopts.factoring=1;
				usrparam_mask|=FACTORINGMASK;
				strategy=0xffffffffffffffff;
				*msg="Factoring has been set to ON";
			} else if (0==strcmp(ltkn,"OFF")){
				glblopts.factoring=0;
				usrparam_mask|=FACTORINGMASK;
				strategy=0xffffffffffffffff;
				*msg="Factoring has been set to OFF";
			} else if (0==strcmp(ltkn,"DEFAULT")){
				usrparam_mask&=~FACTORINGMASK;
				*msg="Factoring has been set to DEFAULT";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing FACTORING parameter";
					return(1);
				}
				if (0==(usrparam_mask&FACTORINGMASK)){
					printf("Factoring is set to DEFAULT\n");
				} else {
					if (glblopts.factoring==1){
						printf("Factoring is ON\n");
					} else {
						printf("Factoring is OFF\n");
					}
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid FACTORING parameter";
					return(1);
				}
				printf("Invalid FACTORING option: %s\n",ltkn);
				printf("Valid options are ON, OFF or DEFAULT\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing FACTORING parameter";
				return(1);
			}
			if (0==(usrparam_mask&FACTORINGMASK)){
				printf("Factoring is set to DEFAULT\n");
			} else {
				if (glblopts.factoring==1){
					printf("Factoring is ON\n");
				} else {
					printf("Factoring is OFF\n");
				}
			}
		}

	/* SELECT command. */
	} else if (0==strcmp(ltkn,"SELECT")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"MAXIMAL")){
				glblopts.select=MAXIMAL;
				usrparam_mask|=SELECTMASK;
				usrstrat&=~SELECTMASK;
				usrstrat|=MAXIMAL_P;
				strategy=0xffffffffffffffff;
				*msg="Select has been set to MAXIMAL";
			} else if (0==strcmp(ltkn,"SINGLENEG")){
				glblopts.select=SINGLENEG;
				usrparam_mask|=SELECTMASK;
				usrstrat&=~SELECTMASK;
				usrstrat|=SINGLENEG_P;
				strategy=0xffffffffffffffff;
				*msg="Select has been set to SINGLENEG";
			} else if (0==strcmp(ltkn,"MULTINEG")){
				glblopts.select=MULTINEG;
				usrparam_mask|=SELECTMASK;
				usrstrat&=~SELECTMASK;
				usrstrat|=MULTINEG_P;
				strategy=0xffffffffffffffff;
				*msg="Select has been set to MULTINEG";
			} else if (0==strcmp(ltkn,"SINGLEPOS")){
				glblopts.select=SINGLEPOS;
				usrparam_mask|=SELECTMASK;
				usrstrat&=~SELECTMASK;
				usrstrat|=SINGLEPOS_P;
				strategy=0xffffffffffffffff;
				*msg="Select has been set to SINGLEPOS";
			} else if (0==strcmp(ltkn,"MULTIPOS")){
				glblopts.select=MULTIPOS;
				usrparam_mask|=SELECTMASK;
				usrstrat&=~SELECTMASK;
				usrstrat|=MULTIPOS_P;
				strategy=0xffffffffffffffff;
				*msg="Select has been set to MULTIPOS";
			} else if (0==strcmp(ltkn,"MXSINGLENEG")){
				glblopts.select=MXSINGLENEG;
				usrparam_mask|=SELECTMASK;
				usrstrat&=~SELECTMASK;
				usrstrat|=MXSINGLENEG_P;
				strategy=0xffffffffffffffff;
				*msg="Select has been set to MXSINGLENEG";
			} else if (0==strcmp(ltkn,"ALL")){
				glblopts.select=SELECTALL;
				usrparam_mask|=SELECTMASK;
				usrstrat&=~SELECTMASK;
				usrstrat|=SELECTALL_P;
				strategy=0xffffffffffffffff;
				*msg="Select has been set to ALL";
			} else if (0==strcmp(ltkn,"TYPE1")){
				glblopts.select=TYPE1;
				usrparam_mask|=SELECTMASK;
				usrstrat&=~SELECTMASK;
				usrstrat|=TYPE1_P;
				strategy=0xffffffffffffffff;
				*msg="Select has been set to TYPE1";
			} else if (0==strcmp(ltkn,"TYPE2")){
				glblopts.select=TYPE2;
				usrparam_mask|=SELECTMASK;
				usrstrat&=~SELECTMASK;
				usrstrat|=TYPE2_P;
				strategy=0xffffffffffffffff;
				*msg="Select has been set to TYPE2";
			} else if (0==strcmp(ltkn,"TYPE3")){
				glblopts.select=TYPE3;
				usrparam_mask|=SELECTMASK;
				usrstrat&=~SELECTMASK;
				usrstrat|=TYPE3_P;
				strategy=0xffffffffffffffff;
				*msg="Select has been set to TYPE3";
			} else if (0==strcmp(ltkn,"TYPE4")){
				glblopts.select=TYPE4;
				usrparam_mask|=SELECTMASK;
				usrstrat&=~SELECTMASK;
				usrstrat|=TYPE4_P;
				strategy=0xffffffffffffffff;
				*msg="Select has been set to TYPE4";
			} else if (0==strcmp(ltkn,"TYPE5")){
				glblopts.select=TYPE5;
				usrparam_mask|=SELECTMASK;
				usrstrat&=~SELECTMASK;
				usrstrat|=TYPE5_P;
				strategy=0xffffffffffffffff;
				*msg="Select has been set to TYPE5";
			} else if (0==strcmp(ltkn,"TYPE6")){
				glblopts.select=TYPE6;
				usrstrat&=~SELECTMASK;
				usrstrat|=TYPE6_P;
				usrparam_mask|=SELECTMASK;
				strategy=0xffffffffffffffff;
				*msg="Select has been set to TYPE6";
			} else if (0==strcmp(ltkn,"TYPE1CPL")){
				glblopts.select=TYPE1CPL;
				usrparam_mask|=SELECTMASK;
				usrstrat&=~SELECTMASK;
				usrstrat|=TYPE1CPL_P;
				strategy=0xffffffffffffffff;
				*msg="Select has been set to TYPE1CPL";
			} else if (0==strcmp(ltkn,"TYPE2CPL")){
				glblopts.select=TYPE2CPL;
				usrparam_mask|=SELECTMASK;
				usrstrat&=~SELECTMASK;
				usrstrat|=TYPE2CPL_P;
				strategy=0xffffffffffffffff;
				*msg="Select has been set to TYPE2CPL";
			} else if (0==strcmp(ltkn,"TYPE3CPL")){
				glblopts.select=TYPE3CPL;
				usrparam_mask|=SELECTMASK;
				usrstrat&=~SELECTMASK;
				usrstrat|=TYPE3CPL_P;
				strategy=0xffffffffffffffff;
				*msg="Select has been set to TYPE3CPL";
			} else if (0==strcmp(ltkn,"TYPE4CPL")){
				glblopts.select=TYPE4CPL;
				usrparam_mask|=SELECTMASK;
				usrstrat&=~SELECTMASK;
				usrstrat|=TYPE4CPL_P;
				strategy=0xffffffffffffffff;
				*msg="Select has been set to TYPE4CPL";
			} else if (0==strcmp(ltkn,"TYPE5CPL")){
				glblopts.select=TYPE5CPL;
				usrparam_mask|=SELECTMASK;
				usrstrat&=~SELECTMASK;
				usrstrat|=TYPE5CPL_P;
				strategy=0xffffffffffffffff;
				*msg="Select has been set to TYPE5CPL";
			} else if (0==strcmp(ltkn,"TYPE6CPL")){
				glblopts.select=TYPE6CPL;
				usrparam_mask|=SELECTMASK;
				usrstrat&=~SELECTMASK;
				usrstrat|=TYPE6CPL_P;
				strategy=0xffffffffffffffff;
				*msg="Select has been set to TYPE6CPL";
			} else if (0==strcmp(ltkn,"DEFAULT")){
				usrparam_mask&=~SELECTMASK;
				usrstrat&=~SELECTMASK;
				*msg="Select has been set to DEFAULT";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing SELECT parameter";
					return(1);
				}
				if (0==(usrparam_mask&SELECTMASK)){
					printf("Select option is set to DEFAULT\n");
				} else {
					switch (glblopts.select){
						case MAXIMAL:
							printf("Select is MAXIMAL\n");
							break;
						case SINGLENEG:
							printf("Select is SINGLENEG\n");
							break;
						case MULTINEG:
							printf("Select is MULTINEG\n");
							break;
						case SINGLEPOS:
							printf("Select is SINGLEPOS\n");
							break;
						case MULTIPOS:
							printf("Select is MULTIPOS\n");
							break;
						case MXSINGLENEG:
							printf("Select is MXSINGLENEG\n");
							break;
						case SELECTALL:
						default:
							printf("Select is ALL\n");
							break;
						case TYPE1:
							printf("Select is TYPE1\n");
							break;
						case TYPE2:
							printf("Select is TYPE2\n");
							break;
						case TYPE3:
							printf("Select is TYPE3\n");
							break;
						case TYPE4:
							printf("Select is TYPE4\n");
							break;
						case TYPE5:
							printf("Select is TYPE5\n");
							break;
						case TYPE6:
							printf("Select is TYPE6\n");
							break;
						case TYPE1CPL:
							printf("Select is TYPE1CPL\n");
							break;
						case TYPE2CPL:
							printf("Select is TYPE2CPL\n");
							break;
						case TYPE3CPL:
							printf("Select is TYPE3CPL\n");
							break;
						case TYPE4CPL:
							printf("Select is TYPE4CPL\n");
							break;
						case TYPE5CPL:
							printf("Select is TYPE5CPL\n");
							break;
						case TYPE6CPL:
							printf("Select is TYPE6CPL\n");
							break;
					}
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid SELECT parameter";
					return(1);
				}
				printf("Invalid SELECT option: %s\n",ltkn);
				printf("Valid options are MAXIMAL, SINGLENEG, MULTINEG, SINGLEPOS, MULTIPOS, MXSINGLENEG, ALL, TYPEx, TYPExCPL or DEFAULT\n");
				printf("where x is an integer in the range 1 to 6.\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing SELECT parameter";
				return(1);
			}
			if (0==(usrparam_mask&SELECTMASK)){
				printf("Select option is set to DEFAULT\n");
			} else {
				switch (glblopts.select){
					case MAXIMAL:
						printf("Select is MAXIMAL\n");
						break;
					case SINGLENEG:
						printf("Select is SINGLENEG\n");
						break;
					case MULTINEG:
						printf("Select is MULTINEG\n");
						break;
					case SINGLEPOS:
						printf("Select is SINGLEPOS\n");
						break;
					case MULTIPOS:
						printf("Select is MULTIPOS\n");
						break;
					case MXSINGLENEG:
						printf("Select is MXSINGLENEG\n");
						break;
					case SELECTALL:
					default:
						printf("Select is ALL\n");
						break;
					case TYPE1:
						printf("Select is TYPE1\n");
						break;
					case TYPE2:
						printf("Select is TYPE2\n");
						break;
					case TYPE3:
						printf("Select is TYPE3\n");
						break;
					case TYPE4:
						printf("Select is TYPE4\n");
						break;
					case TYPE5:
						printf("Select is TYPE5\n");
						break;
					case TYPE6:
						printf("Select is TYPE6\n");
						break;
					case TYPE1CPL:
						printf("Select is TYPE1CPL\n");
						break;
					case TYPE2CPL:
						printf("Select is TYPE2CPL\n");
						break;
					case TYPE3CPL:
						printf("Select is TYPE3CPL\n");
						break;
					case TYPE4CPL:
						printf("Select is TYPE4CPL\n");
						break;
					case TYPE5CPL:
						printf("Select is TYPE5CPL\n");
						break;
					case TYPE6CPL:
						printf("Select is TYPE6CPL\n");
						break;
				}
			}
		}

	/* PRECEDENCE command. */
	} else if (0==strcmp(ltkn,"PRECEDENCE")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"ARITY")){
				glblopts.precedence=PRCARITY;
				usrparam_mask|=PRCMASK;
				strategy=0xffffffffffffffff;
				*msg="Symbol precedence has been set to arity";
			} else if (0==strcmp(ltkn,"FREQUENCY")){
				glblopts.precedence=PRCFREQ;
				usrparam_mask|=PRCMASK;
				strategy=0xffffffffffffffff;
				*msg="Symbol precedence has been set to frequency";
			} else if (0==strcmp(ltkn,"OCCURRENCE")){
				glblopts.precedence=PRCOCCUR;
				usrparam_mask|=PRCMASK;
				strategy=0xffffffffffffffff;
				*msg="Symbol precedence has been set to occurrence";
			} else if (0==strcmp(ltkn,"INVARITY")){
				glblopts.precedence=PRCINVAR;
				usrparam_mask|=PRCMASK;
				strategy=0xffffffffffffffff;
				*msg="Symbol precedence has been set to inverse arity";
			} else if (0==strcmp(ltkn,"INVFREQ")){
				glblopts.precedence=PRCINVFR;
				usrparam_mask|=PRCMASK;
				strategy=0xffffffffffffffff;
				*msg="Symbol precedence has been set to inverse frequency";
			} else if (0==strcmp(ltkn,"INVOCCUR")){
				glblopts.precedence=PRCINVOC;
				usrparam_mask|=PRCMASK;
				strategy=0xffffffffffffffff;
				*msg="Symbol precedence has been set to inverse occurrence";
			} else if (0==strcmp(ltkn,"DEFAULT")){
				usrparam_mask&=~PRCMASK;
				*msg="Symbol precedence has been set to DEFAULT";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing PRECEDENCE parameter";
					return(1);
				}
				if (0==(usrparam_mask&PRCMASK)){
					printf("Symbol precedence is set to DEFAULT\n");
				} else {
					switch (glblopts.precedence){
						case PRCARITY:
							printf("Symbol precedence is ARITY\n");
							break;
						case PRCFREQ:
							printf("Symbol precedence is FREQUENCY\n");
							break;
						case PRCOCCUR:
							printf("Symbol precedence is OCCURRENCE\n");
							break;
						case PRCINVAR:
							printf("Symbol precedence is INVERSE ARITY\n");
							break;
						case PRCINVFR:
							printf("Symbol precedence is INVERSE FREQUENCY\n");
							break;
						case PRCINVOC:
							printf("Symbol precedence is INVERSE OCCURRENCE\n");
							break;
					}
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid PRECEDENCE parameter";
					return(1);
				}
				printf("Invalid PRECEDENCE option: %s\n",ltkn);
				printf("Valid options are ARITY, FREQUENCY, OCCURRENCE, INVARITY, INVFREQ, INVOCCUR or DEFAULT\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing PRECEDENCE parameter";
				return(1);
			}
			if (0==(usrparam_mask&SELECTMASK)){
				printf("Symbol precedence option is set to DEFAULT\n");
			} else {
				switch (glblopts.precedence){
					case PRCARITY:
						printf("Symbol precedence is ARITY\n");
						break;
					case PRCFREQ:
						printf("Symbol precedence is FREQUENCY\n");
						break;
					case PRCOCCUR:
						printf("Symbol precedence is OCCURRENCE\n");
						break;
					case PRCINVAR:
						printf("Symbol precedence is INVERSE ARITY\n");
						break;
					case PRCINVFR:
						printf("Symbol precedence is INVERSE FREQUENCY\n");
						break;
					case PRCINVOC:
						printf("Symbol precedence is INVERSE OCCURRENCE\n");
						break;
				}
			}
		}

	/* PRPRCSYMPREC command. */
	} else if (0==strcmp(ltkn,"PRPRCSYMPREC")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"HIGH")){
				glblopts.prprcsymprec=ISPRCHIGH;
				usrparam_mask|=ISPRCMASK;
				strategy=0xffffffffffffffff;
				*msg="Introduced symbol precedence has been set to HIGH";
			} else if (0==strcmp(ltkn,"LOW")){
				glblopts.prprcsymprec=ISPRCLOW;
				usrparam_mask|=ISPRCMASK;
				strategy=0xffffffffffffffff;
				*msg="Introduced symbol precedence has been set to LOW";
			} else if (0==strcmp(ltkn,"DEFAULT")){
				usrparam_mask&=~ISPRCMASK;
				*msg="Introduced symbol precedence has been set to DEFAULT";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing PRPRCSYMPREC parameter";
					return(1);
				}
				if (0==(usrparam_mask&PRCMASK)){
					printf("Introduced symbol precedence is set to DEFAULT\n");
				} else {
					if (glblopts.prprcsymprec==ISPRCLOW){
						printf("Introduced symbol precedence is LOW\n");
					} else {
						printf("Introduced symbol precedence is HIGH\n");
					}
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid PRPRCSYMPREC parameter";
					return(1);
				}
				printf("Invalid PRPRCSYMPREC option: %s\n",ltkn);
				printf("Valid options are HIGH, LOW or DEFAULT\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing PRPRCSYMPREC parameter";
				return(1);
			}
			if (0==(usrparam_mask&SELECTMASK)){
				printf("Introduced symbol precedence option is set to DEFAULT\n");
			} else {
				if (glblopts.prprcsymprec==ISPRCLOW){
					printf("Introduced symbol precedence is LOW\n");
				} else {
					printf("Introduced symbol precedence is HIGH\n");
				}
			}
		}

	/* SYNTAX command. */
	} else if (0==strcmp(ltkn,"SYNTAX")){
		if (pgmmode==NONINTERACTIVE){
			*msg="Invalid option";
			return(1);
		}
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"TPTP")){
				syntaxmode=TPTP;
				*msg="Syntax mode has been set to TPTP";
			} else if (0==strcmp(ltkn,"DRODI")){
				if (proofmode==TPTP){
					printf("WARNING: If SYNTAX is set to DRODI and the problem is introduced via TELL and ASK commands the proof\n");
					printf("will always be displayed in DRODI format, because TPTP format is not compatible with DRODI syntax.\n\n");
				}
				syntaxmode=DRODI;
				*msg="Syntax mode has been set to DRODI";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing SYNTAX parameter";
					return(1);
				}
				if (syntaxmode==TPTP){
					printf("Syntax mode is TPTP\n");
				} else {
					printf("Syntax mode is DRODI\n");
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid SYNTAX parameter";
					return(1);
				}
				printf("Invalid SYNTAX option: %s\n",ltkn);
				printf("Valid options are TPTP or DRODI\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing SYNTAX parameter";
				return(1);
			}
			if (syntaxmode==TPTP){
				printf("Syntax mode is set to TPTP\n");
			} else {
				printf("Syntax mode is set to DRODI\n");
			}
		}

	/* PROOF command. */
	} else if (0==strcmp(ltkn,"PROOF")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"TPTP")){
				if ((pgmmode==INTERACTIVE)&&(syntaxmode==DRODI)){
					printf("WARNING: SYNTAX is set to DRODI. If the problem is introduced via TELL and ASK commands the proof\n");
					printf("will always be displayed in DRODI format, because TPTP format is not compatible with DRODI syntax.\n");
				}
				proofmode=TPTP;
				*msg="Proof mode has been set to TPTP";
			} else if (0==strcmp(ltkn,"DRODI")){
				proofmode=DRODI;
				*msg="Proof mode has been set to DRODI";
			} else if (0==strcmp(ltkn,"OFF")){
				proofmode=PROOFOFF;
				*msg="Proof mode has been set to OFF";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing PROOF parameter";
					return(1);
				}
				if (proofmode==TPTP){
					printf("Proof mode is TPTP\n");
				} else if (proofmode==DRODI){
					printf("Proof mode is DRODI\n");
				} else {
					printf("Proof mode is OFF\n");
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid PROOF parameter";
					return(1);
				}
				printf("Invalid PROOF option: %s\n",ltkn);
				printf("Valid options are TPTP or DRODI\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing PROOF parameter";
				return(1);
			}
			if (proofmode==TPTP){
				printf("Proof mode is TPTP\n");
			} else if (proofmode==DRODI){
				printf("Proof mode is DRODI\n");
			} else {
				printf("Proof mode is OFF\n");
			}
		}

	/* PRINTKBDATA command. */
	} else if (0==strcmp(ltkn,"PRINTKBDATA")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"ON")){
				prtkbdata=1;
				*msg="Printkbdata mode has been set to ON";
			} else if (0==strcmp(ltkn,"OFF")){
				prtkbdata=0;
				*msg="Printkbdata mode has been set to OFF";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing PRINTKBDATA parameter";
					return(1);
				}
				if (prtkbdata){
					printf("Printkbdata mode is ON\n");
				} else {
					printf("Printkbdata mode is OFF\n");
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid PRINTKBDATA parameter";
					return(1);
				}
				printf("Invalid PRINTKBDATA option: %s\n",ltkn);
				printf("Valid options are WINNER, ALL or OFF\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing PRINTKBDATA parameter";
				return(1);
			}
			if (prtkbdata){
				printf("Printkbdata mode is ON\n");
			} else {
				printf("Printkbdata mode is OFF\n");
			}
		}

	/* ALGORITHM command. */
	} else if (0==strcmp(ltkn,"ALGORITHM")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"OTTER")){
				glblopts.algorithm=OTTER;
				usrparam_mask|=ALGMASK;
				usrstrat&=~ALGMASK;
				usrstrat|=OTTER_P;
				strategy=0xffffffffffffffff;
				*msg="Selected algorithm has been set to OTTER";
			} else if (0==strcmp(ltkn,"DISCOUNT")){
				glblopts.algorithm=DISCOUNT;
				usrparam_mask|=ALGMASK;
				usrstrat&=~ALGMASK;
				usrstrat|=DISCOUNT_P;
				strategy=0xffffffffffffffff;
				*msg="Selected algorithm has been set to DISCOUNT";
			} else if (0==strcmp(ltkn,"UEQDSC")){
				glblopts.algorithm=UEQDSC;
				usrparam_mask|=ALGMASK;
				usrstrat&=~ALGMASK;
				usrstrat|=UEQDSC_P;
				strategy=0xffffffffffffffff;
				*msg="Selected algorithm has been set to unfailing completion DISCOUNT";
			} else if (0==strcmp(ltkn,"UEQOTT")){
				glblopts.algorithm=UEQOTT;
				usrparam_mask|=ALGMASK;
				usrstrat&=~ALGMASK;
				usrstrat|=UEQOTT_P;
				strategy=0xffffffffffffffff;
				*msg="Selected algorithm has been set to unfailing completion OTTER";
			} else if (0==strcmp(ltkn,"DEFAULT")){
				usrparam_mask&=~ALGMASK;
				usrstrat&=~ALGMASK;
				*msg="Selected algorithm has been set to DEFAULT";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing ALGORITHM parameter";
					return(1);
				}
				if (0==(usrparam_mask&ALGMASK)){
					printf("Selected algorithm is set to DEFAULT\n");
				} else {
					printf("Selected algorithm is \n");
					switch (glblopts.algorithm){
						case OTTER:
							printf("OTTER\n");
							break;
						case DISCOUNT:
							printf("DISCOUNT\n");
							break;
						case UEQOTT:
							printf("unfailing cmpletion OTTER\n");
							break;
						case UEQDSC:
							printf("unfailing cmpletion DISCOUNT\n");
							break;
					}
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid ALGORITHM parameter";
					return(1);
				}
				printf("Invalid ALGORITHM option: %s\n",ltkn);
				printf("Valid options are OTTER, DISCOUNT, UEQOTT, UEQDSC or DEFAULT\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing ALGORITHM parameter";
				return(1);
			}
			if (0==(usrparam_mask&ALGMASK)){
				printf("Selected algorithm is set to DEFAULT\n");
			} else {
				printf("Selected algorithm is \n");
				switch (glblopts.algorithm){
					case OTTER:
						printf("OTTER\n");
						break;
					case DISCOUNT:
						printf("DISCOUNT\n");
						break;
					case UEQOTT:
						printf("unfailing cmpletion OTTER\n");
						break;
					case UEQDSC:
						printf("unfailing cmpletion DISCOUNT\n");
						break;
				}
			}
		}

	/* LAYER command. */
	} else if (0==strcmp(ltkn,"LAYER")){
		if (NULL!=(ltkn=strtok(NULL," \t"))){
			for (ii=0,jj=strlen(ltkn);ii<jj;ii++){
				ltkn[ii]=toupper(ltkn[ii]);
			}
			if (0==strcmp(ltkn,"1")){
				glblopts.layer=0;
				usrparam_mask|=LAYERMASK;
				strategy=0xffffffffffffffff;
				*msg="Clause selection layer type has been set to 1";
			} else if (0==strcmp(ltkn,"2")){
				glblopts.layer=1;
				usrparam_mask|=LAYERMASK;
				strategy=0xffffffffffffffff;
				*msg="Clause selection layer type has been set to 2";
			} else if (0==strcmp(ltkn,"DEFAULT")){
				usrparam_mask&=~LAYERMASK;
				*msg="Clause selection layer type has been set to DEFAULT";
			} else if (ltkn[0]==0){
				if (pgmmode==NONINTERACTIVE){
					*msg="Missing LAYER parameter";
					return(1);
				}
				if (0==(usrparam_mask&LAYERMASK)){
					printf("Clause selection layer type has been set to DEFAULT\n");
				} else {
					if (glblopts.layer==0){
						printf("Clause selection layer type is set to 1\n");
					} else {
						printf("Clause selection layer type is set to 2\n");
					}
				}
			} else {
				if (pgmmode==NONINTERACTIVE){
					*msg="Invalid LAYER parameter";
					return(1);
				}
				printf("Invalid LAYER option: %s\n",ltkn);
				printf("Valid options are 1, 2 or DEFAULT\n");
			}
		} else {
			if (pgmmode==NONINTERACTIVE){
				*msg="Missing LAYER parameter";
				return(1);
			}
			if (0==(usrparam_mask&LAYERMASK)){
				printf("Clause selection layer type has been set to DEFAULT\n");
			} else {
				if (glblopts.layer==0){
					printf("Clause selection layer type is set to 1\n");
				} else {
					printf("Clause selection layer type is set to 2\n");
				}
			}
		}

	/* IMPORT command. */
	} else if (0==strcmp(ltkn,"IMPORT")){
		if (pgmmode==NONINTERACTIVE){
			*msg="Invalid option";
			return(1);
		}
		if (NULL!=(ltkn=strtok(NULL,""))){
			if (NULL==(ptr1=ParseFilename(ltkn,&ptr2))){
				*msg="Invalid IMPORT parameter. It must be a valid filename";
				return(pgmmode==INTERACTIVE?0:1);
			}
			printf("Reading file %s...\n",ptr1);
			ii=syntaxmode;
			syntaxmode=TPTP;
			importfile=ptr1;
			if (0!=(ii=Import(ptr1,NULL,0,1))){
				if (ii!=2){
					if (NOMEMORY==Initialize_KB(&mainkb)){
						printf("No memory available, exiting program\n");
						return(1);
					}
					ResetStats(pbmstats);
					printf("Knowledge bases have been initialized\n");
				}
			} else {
				Saturate();
			}
			syntaxmode=ii;
			importfile=NULL;
		} else {
			printf("Missing file name\n");
		}

	/* LOG command. */
	} else if (0==strcmp(ltkn,"LOG")){
		if (NULL!=(ltkn=strtok(NULL,""))){
			MYFREE(logpath);
			if (strlen(ltkn)==3){
				if (('O'==toupper(ltkn[0]))&&('F'==toupper(ltkn[1]))&&('F'==toupper(ltkn[2]))){
					logpath=NULL;
					*msg="LOG has been set to OFF";
					return(0);
				}
			}
			if (strlen(ltkn)==2){
				if (('O'==toupper(ltkn[0]))&&('N'==toupper(ltkn[1]))){
					if (NULL==(logpath=MYALLOC(1))){
						*msg="No memory available, exiting program";
						return(1);
					}
					logpath[0]=0;
					*msg="LOG has been set to ON";
					return(0);
				}
			}
			if (NULL==(logpath=MYSTRDUP(ltkn))){
				*msg="No memory available, exiting program";
				return(1);
			}
			if (pgmmode==NONINTERACTIVE){
				*msg="LOG has been set to file\n";
			} else {
				printf("LOG has been set to %s\n",ltkn);
			}
		} else {
			*msg="Missing log parameter";
			if (pgmmode==NONINTERACTIVE){
				return(1);
			}
		}

	/* Comment. */
	} else if (ltkn[0]=='#'){
		if (pgmmode==NONINTERACTIVE){
			*msg="Invalid option #";
			return(1);
		}
		printf("\n");

	#ifdef BENCHMARKING
	/* BENCHFILE command. */
	} else if (0==strcmp(ltkn,"BENCHFILE")){
		if (NULL!=(ltkn=strtok(NULL,""))){
			if (NULL==(benchfname=MYALLOC(1+strlen(ltkn)))){
				*msg="Not enough memory for benchmarking output file name";
				return(pgmmode==INTERACTIVE?0:1);
			} else {
				strcpy(benchfname,ltkn);
				if (pgmmode==INTERACTIVE){
					printf("BENCHFILE has been set to %s\n",benchfname);
				}
			}
		} else {
			*msg="Missing file name";
			if (pgmmode==NONINTERACTIVE){
				return(1);
			}
		}
	#endif

	/* Invalid option/command. */
	} else {
		printf("Invalid option %s\n",ltkn);
		return(pgmmode==INTERACTIVE?0:1);
	}
	return(0);
} /*  ParseCommand */
