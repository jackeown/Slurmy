/*
 ============================================================================
 Name        : learn.c
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
 *    This source contains the learning related functions for Drodi
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
#include "learn.h"

/** Global variables for this module: only those that need initialization in their definition. */
int32_t fvsize; /* Number of elements allocated to set pointed by fvidx global variable. */
int32_t fvused; /* Number of elements used in the set pointed by fvidx global variable. */
double fvector[VECTORLENGTH]; /* Static feature vector used by DCDescentMethod() and GetClauseVector() functions. */
                              /* It is defined static to save allocation time due to the high rate of use. */

/* Local functions not shared with other modules. */
int LrnCompare(const void *,const void *); /* Compare function for qsort() */
int32_t CheckGCVMemory(int32_t); /* Check and increment fvidx set memory */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Build learn data from output learn file)  OCJ
 *
 *    This function process a learned data file produced by the
 *    LEARNTO command option and generates a learn file usable
 *    by the LEARNFROM command option.
 *
 *    See LearnFromProblem() for a description of the input file.
 *    The output file formats is a set of learn records written
 *    one after the other, with one record for each combination
 *    of type of problem and relevant structures to which the
 *    learn record applies.
 *
 *    The format of a learn record is as follows:
 *
 *      header size indices components
 *
 *    where:
 *      header: 64 bits unsigned integer with the problem type and
 *              relevant strategies to which the learn strategies
 *              to which the learn record applies. See comments
 *              of learnrecord structure in global.h file for
 *              a detailed description of the header.
 *      size: 32 bits integer with the number of non null vector
 *            components.
 *      indices: Set of 32 bits integers with the components that
 *               are not zero in the sparse vector.
 *      components: Set of double precision float numbers with
 *                  the value of the components that are not zero.
 *
 *
 *  ARGUMENTS:
 *
 *    infile: Input file produced by the LEARNTO command option.
 *    outfile: Output file usable by the LEARNFROM command option.
 *
 *  RETURNS:
 *
 *    0 if OK or 1 if error or not enough memory.
 *
 *
 *
 *--------------------------------------------------------------*/
int32_t ProcessLearnData(char *infile,char *outfile){
	auto FILE *filein,*fileout;                              /* Input an output file handles */
	auto learnvector *vectors;                               /* Pointer to feature vectors buffer */
	auto uint8_t *flags;                                     /* Good/bad flags for each feature vector */
	auto int32_t numrecords;                                 /* Number of records allocated for record buffers */
	auto int32_t readrecords;                                /* Number of records used in buffer */
	auto int32_t orgrecords;                                 /* Initial number of records of a certain type */
	auto int32_t recordsnotread;                             /* Not zero if some records were not read due to lack of memory */
	auto uint64_t recordtypes[1+(PBMTYPES*MAXLEARNRECORDS)]; /* Record types fully processed plus an end indicator */
	auto int32_t types;                                      /* Number of record types fully processed */
	auto int64_t offset;                                     /* Current infile read offset */
	auto int64_t nxtoffset;                                  /* Offset of first record of next type of records to be processed. */
	auto learnvector *result;                                /* Resulting vector. */
	auto uint64_t blocklength;                               /* Number of flag/vector entries in block */
	auto uint64_t blockbytes;                                /* Number of bytes occupied by the header, flags and vectors in an input file block */
	auto uint64_t header;                                    /* Block header */
	auto uint64_t filesize;                                  /* Size of input file */
	auto float acctotal,accgood,accbad;                      /* Total, good and bad clauses accuracy. */
	auto float tolerance;                                    /* Tolerance achieved by DCDescentMethod() function */
	auto uint64_t pbmtypemask;                               /* Problem type mask for record header comparison */
	auto struct mallinfo2 rsinfo;                            /* For mallinfo2() calls */
	auto learnvector *ptr1;                                  /* Auxiliary pointer */
	auto uint8_t *ptr2;                                      /* Auxiliary pointer */
	auto char *ptr3;                                         /* Auxiliary pointer */
	auto uint64_t jj,ss;                                     /* Auxiliary */
	auto int32_t kk,mm,pp,qq,rr;                             /* Auxiliary */

	/* Set prefix for printed lines and pbmtypemask. */
	if (pgmmode==NONINTERACTIVE){
		ptr3="% ";
	} else {
		ptr3="";
	}
	pbmtypemask=((uint64_t)(PBMTYPES-1))<<56;

	/* Set maximum memory and check minimum available memory. */
	jj=GetAvailableMemory();
	rsinfo=mallinfo2();
	maxmemory=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost+jj;
	if (jj<PROCLEARNMINMEM){
		printf("\n%sNot enough memory to process input learn file\n",ptr3);
		return(1);
	}

	/* Open input file and get file size. */
	printf("\n%s**********************************************\n",ptr3);
	printf("%sGenerating file %s from file %s\n",ptr3,outfile,infile);
	if (NULL==(filein=fopen(infile,"rb"))){
		printf("%s%s opening input file %s.\n",ptr3,strerror(errno),infile);
		printf("%s**********************************************\n",ptr3);
		return(1);
	}
	fseek(filein,0,SEEK_END);
	filesize=ftell(filein);
	fseek(filein,0,SEEK_SET);

	/* Open output file. */
	if (NULL==(fileout=fopen(outfile,"wb"))){
		fclose(filein);
		printf("%s%s opening output file %s.\n",ptr3,strerror(errno),outfile);
		printf("%s**********************************************\n",ptr3);
		return(1);
	}

	/* Initialize control data. */
	vectors=result=NULL;
	flags=NULL;
	memset(&recordtypes[0],255,sizeof(recordtypes));
	types=0;
	nxtoffset=0;

	/* Record type outer loop. */
	while (1){

		/* Block reading inner loop. */
		readrecords=numrecords=0;
		recordsnotread=0;
		offset=nxtoffset;
		fseek(filein,offset,SEEK_SET);
		while (1){

			/* Read number of sparse vectors in block. */
			if (0==fread(&blocklength,sizeof(blocklength),1,filein)){

				/* Read error. */
				if (ferror(filein)){
					fclose(filein);
					fclose(fileout);
					if (vectors!=NULL){
						for (mm=0;mm<readrecords;mm++){
							MYFREE(vectors[mm].index);
						}
						MYFREE(vectors);
						MYFREE(flags);
					}
					printf("%s%s reading input file %s.\n",ptr3,strerror(errno),infile);
					printf("%s**********************************************\n",ptr3);
					return(1);
				}

				/* End of file, leave block reading loop. */
				break;
			}

			/* Read number of bytes in block. */
			if (0==fread(&blockbytes,sizeof(blockbytes),1,filein)){
				if (ferror(filein)){
					printf("%s%s reading input file %s.\n",ptr3,strerror(errno),infile);
				} else {
					printf("%sInvalid input file %s.\n",ptr3,infile);
				}
				printf("%s**********************************************\n",ptr3);
				fclose(filein);
				fclose(fileout);
				if (vectors!=NULL){
					for (mm=0;mm<readrecords;mm++){
						MYFREE(vectors[mm].index);
					}
					MYFREE(vectors);
					MYFREE(flags);
				}
				return(1);
			}

			/* Validate block length. */
			jj=blockbytes+sizeof(blockbytes)+sizeof(blocklength);
			if (filesize<(jj+offset)){
				fclose(filein);
				fclose(fileout);
				if (vectors!=NULL){
					for (mm=0;mm<readrecords;mm++){
						MYFREE(vectors[mm].index);
					}
					MYFREE(vectors);
					MYFREE(flags);
				}
				printf("%sInvalid input file %s.\n",ptr3,infile);
				printf("%s**********************************************\n",ptr3);
				return(1);
			}

			/* Read block header. */
			if (0==fread(&header,sizeof(header),1,filein)){
				if (ferror(filein)){
					printf("%s%s reading input file %s.\n",ptr3,strerror(errno),infile);
				} else {
					printf("%sInvalid input file %s.\n",ptr3,infile);
				}
				printf("%s**********************************************\n",ptr3);
				fclose(filein);
				fclose(fileout);
				if (vectors!=NULL){
					for (mm=0;mm<readrecords;mm++){
						MYFREE(vectors[mm].index);
					}
					MYFREE(vectors);
					MYFREE(flags);
				}
				return(1);
			}

			/* Validate block header. */
			if (((0xffffffffffffff&header)!=((ALGMASK|SELECTMASK|TERMORDRMASK|
					LITORDRMASK|FUNCWMASK|WEAKRWMASK|SPLITMASK)&header))
					||((0xff00000000000000&header)>pbmtypemask)){
				fclose(filein);
				fclose(fileout);
				if (vectors!=NULL){
					for (mm=0;mm<readrecords;mm++){
						MYFREE(vectors[mm].index);
					}
					MYFREE(vectors);
					MYFREE(flags);
				}
				printf("%sInvalid record header detected at offset %ld.\n",ptr3,offset);
				printf("%s**********************************************\n",ptr3);
				return(1);
			}

			/* Block type is the first type or the type currently in process */
			/* and maximum number of records has not been allocated. */
			if ((((readrecords==0)&&(types==0))||(header==recordtypes[types]))
					&&(recordsnotread==0)){

				/* Allocate memory and initialize recordtype[0] if no records have been read yet. */
				if (numrecords==0){

					/* There is not enough memory to read all flags and learnvector elements. */
					rsinfo=mallinfo2();
					if (maxmemory<((blocklength*(sizeof(flags[0])+sizeof(learnvector)))+rsinfo.uordblks
							+rsinfo.hblkhd)){
						recordsnotread=1;

					/* There is enough memory to read all flags and learnvector elements. */
					} else {
						numrecords=blocklength;
						if (NULL==(vectors=MYALLOC(numrecords*sizeof(learnvector)))){
							fclose(filein);
							fclose(fileout);
							printf("%sNot enough memory.\n",ptr3);
							printf("%s**********************************************\n",ptr3);
							return(1);
						}
						if (NULL==(flags=MYALLOC(numrecords*sizeof(flags[0])))){
							fclose(filein);
							fclose(fileout);
							MYFREE(vectors);
							printf("%sNot enough memory.\n",ptr3);
							printf("%s**********************************************\n",ptr3);
							return(1);
						}
						recordtypes[0]=header;
					}

				/* Reallocate memory if some records have been already read. */
				} else {

					/* There is not enough memory to read all flags and learnvector elements. */
					rsinfo=mallinfo2();
					if (maxmemory<((blocklength*(sizeof(flags[0])+sizeof(learnvector)))+rsinfo.uordblks
							+rsinfo.hblkhd)){
						recordsnotread=1;

					/* There is enough memory to read all flags and learnvector elements. */
					} else {
						numrecords+=blocklength;
						if (NULL==(ptr1=MYREALLOC(vectors,numrecords*sizeof(learnvector)))){
							fclose(filein);
							fclose(fileout);
							for (mm=0;mm<readrecords;mm++){
								MYFREE(vectors[mm].index);
							}
							MYFREE(vectors);
							MYFREE(flags);
							printf("%sNot enough memory.\n",ptr3);
							printf("%s**********************************************\n",ptr3);
							return(1);
						}
						vectors=ptr1;
						if (NULL==(ptr2=MYREALLOC(flags,numrecords*sizeof(flags[0])))){
							fclose(filein);
							fclose(fileout);
							for (mm=0;mm<readrecords;mm++){
								MYFREE(vectors[mm].index);
							}
							MYFREE(vectors);
							MYFREE(flags);
							printf("%sNot enough memory.\n",ptr3);
							printf("%s**********************************************\n",ptr3);
							return(1);
						}
						flags=ptr2;
					}
				}

				/* Additional records can possibly be read. */
				if (recordsnotread==0){

					/* Read block vectors and flags. Loop for each flag/vector pair. */
					for (;readrecords<numrecords;){

						/* Read flag and number of non null vector components. */
						if (0==fread(&flags[readrecords],sizeof(flags[0]),1,filein)){
							if (ferror(filein)){
								printf("%s%s reading input file %s.\n",ptr3,strerror(errno),infile);
							} else {
								printf("%sInvalid input file %s.\n",ptr3,infile);
							}
							printf("%s**********************************************\n",ptr3);
							fclose(filein);
							fclose(fileout);
							for (mm=0;mm<readrecords;mm++){
								MYFREE(vectors[mm].index);
							}
							MYFREE(vectors);
							MYFREE(flags);
							return(1);
						}
						if (0==fread(&vectors[readrecords].size,sizeof(vectors[0].size),1,filein)){
							if (ferror(filein)){
								printf("%s%s reading input file %s.\n",ptr3,strerror(errno),infile);
							} else {
								printf("%sInvalid input file %s.\n",ptr3,infile);
							}
							printf("%s**********************************************\n",ptr3);
							fclose(filein);
							fclose(fileout);
							for (mm=0;mm<readrecords;mm++){
								MYFREE(vectors[mm].index);
							}
							MYFREE(vectors);
							MYFREE(flags);
							return(1);
						}

						/* There is not enough memory for the sparse vector data. */
						/* Indicate that some records were not read and leave the loop. */
						ss=(vectors[readrecords].size*(sizeof(vectors[0].index[0])+sizeof(vectors[0].sparsevect[0])));
						rsinfo=mallinfo2();
						if (maxmemory<(ss+rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost)){
							recordsnotread=1;
							break;

						/* There is enough memory for the sparse vector data. */
						} else {

							/* Allocate memory for block indices and vectors. */
							if (NULL==(vectors[readrecords].index=MYALLOC(ss))){
								fclose(filein);
								fclose(fileout);
								for (mm=0;mm<readrecords;mm++){
									MYFREE(vectors[mm].index);
								}
								MYFREE(vectors);
								MYFREE(flags);
								printf("%sNot enough memory.\n",ptr3);
								printf("%s**********************************************\n",ptr3);
								return(1);
							}
							vectors[readrecords].sparsevect=(double *)&vectors[readrecords].index[vectors[readrecords].size];

							/* Read block indices and vectors. */
							if (0==fread(vectors[readrecords].index,ss,1,filein)){
								if (ferror(filein)){
									printf("%s%s reading input file %s.\n",ptr3,strerror(errno),infile);
								} else {
									printf("%sInvalid input file %s.\n",ptr3,infile);
								}
								printf("%s**********************************************\n",ptr3);
								fclose(filein);
								fclose(fileout);
								for (mm=0;mm<readrecords;mm++){
									MYFREE(vectors[mm].index);
								}
								MYFREE(vectors);
								MYFREE(flags);
								return(1);
							}
							readrecords++;
						}
					}
				}

				/* Not all the records in the block were read. */
				/* Move file pointer to the end of the block. */
				if (recordsnotread){
					fseek(filein,jj+offset,SEEK_SET);
				}

			/* Record type is not the first type nor the type currently in process */
			/* or maximum number of records has been already allocated. */
			} else {

				/* There is not a next record type yet. */
				if (recordtypes[types+1]==0xffffffffffffffff){

					/* Check that the record type has not been already processed. */
					for (kk=mm=0;kk<=types;kk++){
						if (recordtypes[kk]==header){
							mm=1;
							break;
						}
					}

					/* Record type has not been processed yet. */
					if (mm==0){

						/* Set this record type as next record type and remember its position. */
						recordtypes[types+1]=header;
						nxtoffset=offset;

						/* If maximum number of records has been achieved then leave */
						/* the inner loop. */
						if (recordsnotread>0){
							break;
						}
					}
				}

				/* Position the file pointer to the end of the block. */
				/*fseek(filein,jj+offset,SEEK_SET);*/ /* This also serves but may take longer. */
				fseek(filein,jj-sizeof(header)-sizeof(blockbytes)-sizeof(blocklength),SEEK_CUR);
			}

			/* Update current file offset. */
			offset+=jj;
		}

		/* Some records were read. */
		if (readrecords>0){

			/* Do until good clauses accuracy is greater than bad clauses accuracy. */
			printf("%sProcessing records of type %d...\n",ptr3,types+1);
			orgrecords=readrecords;
			while (1){

				/* Process current records. */
				if (NULL==(result=DCDescentMethod(vectors,flags,readrecords,orgrecords,
						&tolerance,&acctotal,&accgood,&accbad,&pp,&qq))){
					fclose(filein);
					fclose(fileout);
					for (mm=0;mm<readrecords;mm++){
						MYFREE(vectors[mm].index);
					}
					MYFREE(vectors);
					MYFREE(flags);
					printf("%sNot enough memory.\n",ptr3);
					printf("%s**********************************************\n",ptr3);
					return(1);
				}

				/* Report progress and accuracy. */
				printf("%sProcessed %d records of type %d, accuracy:",ptr3,readrecords,types+1);
				printf(" %4.1f%% (total)",acctotal);
				printf(", %4.1f%% (good clauses)",accgood);
				printf(", %4.1f%% (bad clauses)",accbad);
				printf(", tolerance=%4.1f\n",tolerance);

				/* Leave loop if good clauses accuracy and bad clauses accuracy are comparable */
				/* or some records were not read due to lack of memory. */
				if (((accgood/accbad)>=ACCBALANCE)||(recordsnotread)){
					break;
				}

				/* If there is not enough memory to add additional flags and learnvector elements */
				/* to duplicate (some) misclassified good clauses then leave the loop. */
				rsinfo=mallinfo2();
				if (maxmemory<((pp*(sizeof(flags[0])+sizeof(learnvector)))+rsinfo.uordblks
						+rsinfo.hblkhd+PROCLEARNMINMEM1)){
					recordsnotread=1;
					break;
				}
				MYFREE(result->index);
				MYFREE(result);

				/* Reallocate memory for vectors structures and flags. */
				numrecords+=pp;
				if (NULL==(ptr1=MYREALLOC(vectors,numrecords*sizeof(learnvector)))){
					fclose(filein);
					fclose(fileout);
					for (mm=0;mm<readrecords;mm++){
						MYFREE(vectors[mm].index);
					}
					MYFREE(vectors);
					MYFREE(flags);
					printf("%sNot enough memory.\n",ptr3);
					printf("%s**********************************************\n",ptr3);
					return(1);
				}
				vectors=ptr1;
				if (NULL==(ptr2=MYREALLOC(flags,numrecords*sizeof(flags[0])))){
					fclose(filein);
					fclose(fileout);
					for (mm=0;mm<readrecords;mm++){
						MYFREE(vectors[mm].index);
					}
					MYFREE(vectors);
					MYFREE(flags);
					printf("%sNot enough memory.\n",ptr3);
					printf("%s**********************************************\n",ptr3);
					return(1);
				}
				flags=ptr2;

				/* Duplicate misclassified good clauses. */
				for (kk=qq,rr=0;rr<pp;kk++){
					if (flags[kk]==2){
						flags[kk]=1;
						rr++;
						if (0==recordsnotread){
							ss=(vectors[kk].size*(sizeof(vectors[0].index[0])+sizeof(vectors[0].sparsevect[0])));
							rsinfo=mallinfo2();
							if (maxmemory<(ss+rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost)){
								recordsnotread=1;
							} else {
								if (NULL==(vectors[readrecords].index=MYALLOC(ss))){
									fclose(filein);
									fclose(fileout);
									for (mm=0;mm<readrecords;mm++){
										MYFREE(vectors[mm].index);
									}
									MYFREE(vectors);
									MYFREE(flags);
									printf("%sNot enough memory.\n",ptr3);
									printf("%s**********************************************\n",ptr3);
									return(1);
								}
								vectors[readrecords].size=vectors[kk].size;
								vectors[readrecords].sparsevect=(double *)&vectors[readrecords].index[vectors[readrecords].size];
								memcpy(vectors[readrecords].index,vectors[kk].index,ss);
								flags[readrecords]=1;
								readrecords++;
							}
						}
					}
				}
			}

			/* Add resulting record to output file, update number of record types fully */
			/* processed, free result and report progress. */
			if (1>fwrite(&recordtypes[types],sizeof(recordtypes[0]),1,fileout)){
				printf("%sError writing to output file %s.\n",ptr3,outfile);
				printf("%s**********************************************\n",ptr3);
				fclose(filein);
				fclose(fileout);
				for (mm=0;mm<readrecords;mm++){
					MYFREE(vectors[mm].index);
				}
				MYFREE(vectors);
				MYFREE(flags);
				return(1);
			}
			if (1>fwrite(&result->size,sizeof(result->size),1,fileout)){
				printf("%sError writing to output file %s.\n",ptr3,outfile);
				printf("%s**********************************************\n",ptr3);
				fclose(filein);
				fclose(fileout);
				for (mm=0;mm<readrecords;mm++){
					MYFREE(vectors[mm].index);
				}
				MYFREE(vectors);
				MYFREE(flags);
				return(1);
			}
			if (1>fwrite(result->index,
					result->size*(sizeof(result->index[0])+sizeof(result->sparsevect[0])),1,fileout)){
				printf("%sError writing to output file %s.\n",ptr3,outfile);
				printf("%s**********************************************\n",ptr3);
				fclose(filein);
				fclose(fileout);
				for (mm=0;mm<readrecords;mm++){
					MYFREE(vectors[mm].index);
				}
				MYFREE(vectors);
				MYFREE(flags);
				return(1);
			}
			if (recordsnotread>0){
				printf("%s===> Maximum records limit exceeded, some input records not processed.\n",ptr3);
			}
			MYFREE(result->index);
			MYFREE(result);
			types++;
		}

		/* Free records memory. */
		for (mm=0;mm<readrecords;mm++){
			MYFREE(vectors[mm].index);
		}
		MYFREE(vectors);
		MYFREE(flags);
		vectors=NULL;

		/* Leave record type loop if maximum number of record types has been reached. */
		if ((PBMTYPES*MAXLEARNRECORDS)<=types){
			printf("%s===> Maximum number of records types exceeded, no more record types processed.\n",ptr3);
			break;
		}

		/* Leave record type loop if no new record type available. */
		if (recordtypes[types]==0xffffffffffffffff){
			break;
		}
	}

	/* Close files and return. */
	fclose(filein);
	fclose(fileout);
	printf("%sFile %s has been created.\n",ptr3,outfile);
	printf("%s**********************************************\n",ptr3);

	return(0);
} /* ProcessLearnData */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Dual Coordinate Descent Method)  OCJ
 *
 *    This function process records from a learned data file
 *    produced by the LEARNTO command option and generates a learn
 *    file usable by the LEARNFROM command option using the Dual
 *    Coordinate Descent Method for Large-Scale Linear SVM with
 *    random permutation for L2-SVM loss function as described
 *    in document "A Dual Coordinate Descent Method for Large-Scale
 *    Linear SVM" by Cho-Jui Hsieh, Kai-Wei Chang, Chih-Jen Lin,
 *    S. Sathiya Keerthi and S. Sundararajan.
 *
 *
 *  ARGUMENTS:
 *
 *    vectors: Pointer to learnvector set of structures.
 *    flags: Pointer to set of flags. Each flag equals 1 or 2 if
 *           the corresponding vector comes from a good clause,
 *           0 otherwise. On entry all flags of good clauses
 *           are set to 1. On exit the flags of mis-classified
 *           good clauses will be set to 2.
 *    numrecords: Number of vectors in vectors set. This may have
 *                been increased from the initial number of records.
 *    orgrecords: Initial number of records in vector set.
 *    ptolerance: Pointer to final tolerance achieved on exit.
 *    acctotal: Pointer to total accuracy achieved on exit.
 *    accgood: Pointer to good clauses accuracy achieved on exit.
 *    accbad: Pointer to bad clauses accuracy achieved on exit.
 *    mcgclause: Pointer to number of misclassified good clauses
 *               on exit.
 *    frstmcgcl: Pointer to index of first misclassified good
 *               clause on exit.
 *
 *  RETURNS:
 *
 *    Pointer to learnvector sparse vector pointer or NULL if
 *    not enough memory. If not NULL then the result must be freed
 *    by the caller.
 *
 *
 *
 *--------------------------------------------------------------*/
learnvector *DCDescentMethod(learnvector *vectors,uint8_t *flags,int32_t numrecords,int32_t orgrecords,
		float *ptolerance,float *acctotal,float *accgood,float *accbad,int32_t *mcgclause,int32_t *frstmcgcl){
	auto learnvector *result;                            /* Resulting sparse vector */
	auto int32_t size;                                   /* Number of non null components of resulting vector */
	auto double *alpha;                                  /* Working vector dual to result */
	auto int32_t *indices;                               /* Set of random indices */
	auto double gradient;                                /* Gradient of minimized function */
	auto double prgrad;                                  /* Projected gradient */
	auto double bQii;                                    /* Diagonal element of barred Q matrix */
	auto double maxpg;                                   /* Maximum projected gradient */
	auto double minpg;                                   /* Minimum projected gradient */
	auto float tolerance;                                /* Tolerance for current cycle */
	auto int32_t maxiter;                                /* Maximum number of iterations */
	auto struct mallinfo2 rsinfo;                        /* For mallinfo2() calls */
	auto struct timeval time1,time2;                     /* For timeout check */
	auto double timedif;                                 /* For timeout checks */
	auto int32_t ii,jj,kk,nn,pp,ff;                      /* Auxiliary */
	auto uint64_t mm;                                    /* Auxiliary */
	auto double aa,bb;                                   /* Auxiliary */

	/* Check that there is enough memory. */
	mm=numrecords*(sizeof(alpha[0])+sizeof(*indices));
	rsinfo=mallinfo2();
	if (maxmemory<=(rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost+mm)){
		return(NULL);
	}

	/* Allocate memory and initialize. */
	if (NULL==(alpha=MYALLOC(numrecords*sizeof(alpha[0])))){
		return(NULL);
	}
	if (NULL==(indices=MYALLOC(numrecords*sizeof(*indices)))){
		MYFREE(alpha);
		return(NULL);
	}
	memset(fvector,0,VECTORLENGTH*sizeof(fvector[0]));
	memset(alpha,0,numrecords*sizeof(alpha[0]));
	for (ii=0;ii<numrecords;ii++){
		indices[ii]=ii;
	}

	/* Main loop. */
	size=0;
	tolerance=TOLERANCE1;
	maxiter=MAXRECSITER/numrecords;
	gettimeofday(&time1,NULL);
	for (pp=0;;pp++){

		/* Initialize random indices. */
		for (ii=0;ii<numrecords;ii++){
			jj=(((int64_t)numrecords)*((int64_t)rand()))/RAND_MAX;
			if (jj==numrecords){
				jj--;
			}
			kk=indices[ii];
			indices[ii]=indices[jj];
			indices[jj]=kk;
		}

		/* Initialize maximum and minimum projected gradient. */
		maxpg=-1.0e308;
		minpg=1.0e308;

		/* Loop through the random indices. */
		for (ii=0;ii<numrecords;ii++){
			jj=indices[ii];

			/* Calculate gradient and diagonal element of barred Q matrix. */
			for (gradient=0.0,bQii=1/(2.0*PENALTYPARAMETER),kk=0,nn=vectors[jj].size;kk<nn;kk++){
				gradient+=(fvector[vectors[jj].index[kk]]*vectors[jj].sparsevect[kk]);
				bQii+=(vectors[jj].sparsevect[kk]*vectors[jj].sparsevect[kk]);
			}
			if (0==flags[jj]){
				gradient=-gradient;
			}
			gradient+=((alpha[jj]/(2*PENALTYPARAMETER))-1.0);

			/* Calculate projected gradient. */
			if (alpha[jj]==0.0){
				if (gradient<0.0){
					prgrad=gradient;
				} else {
					prgrad=0.0;
				}
			} else {
				prgrad=gradient;
			}

			/* Update maximum and minimum projected gradient. */
			if (maxpg<prgrad){
				maxpg=prgrad;
			}
			if (minpg>prgrad){
				minpg=prgrad;
			}

			/* Projected gradient is not zero. */
			if (prgrad!=0.0){

				/* Update alpha. */
				aa=alpha[jj]; /* Remember old alpha. */
				alpha[jj]-=(gradient/bQii);
				if (alpha[jj]<0.0){
					alpha[jj]=0.0;
				}

				/* Update fvector. */
				bb=alpha[jj]-aa;
				if (flags[jj]){
					for (kk=0,nn=vectors[jj].size;kk<nn;kk++){
						if (fvector[vectors[jj].index[kk]]!=0.0){
							ff=1;
						} else {
							ff=0;
						}
						fvector[vectors[jj].index[kk]]+=(bb*vectors[jj].sparsevect[kk]);
						if (ff){
							if (fvector[vectors[jj].index[kk]]==0.0){
								size--;
							}
						} else {
							if (fvector[vectors[jj].index[kk]]!=0.0){
								size++;
							}
						}
					}
				} else {
					for (kk=0,nn=vectors[jj].size;kk<nn;kk++){
						if (fvector[vectors[jj].index[kk]]!=0.0){
							ff=1;
						} else {
							ff=0;
						}
						fvector[vectors[jj].index[kk]]-=(bb*vectors[jj].sparsevect[kk]);
						if (ff){
							if (fvector[vectors[jj].index[kk]]==0.0){
								size--;
							}
						} else {
							if (fvector[vectors[jj].index[kk]]!=0.0){
								size++;
							}
						}
					}
				}
			}
		}

		/* Check timeout. */
		gettimeofday(&time2,NULL);
		timedif=time2.tv_sec-time1.tv_sec+(time2.tv_usec-time1.tv_usec)/1000000.0;
		if (timedif>LEARNTIMEOUT){
			GetAccuracy(vectors,flags,numrecords,orgrecords,acctotal,accgood,accbad,mcgclause,frstmcgcl);
			*ptolerance=maxpg-minpg;
			break;
		}

		/* Check tolerance and stopping condition. */
		if (((maxpg-minpg)<=tolerance)||(pp>maxiter)){

			/* Get accuracies. */
			GetAccuracy(vectors,flags,numrecords,orgrecords,acctotal,accgood,accbad,mcgclause,frstmcgcl);

			/* If accuracy is not balanced or tolerance is TOLERANCE2 */
			/* or maximum number of iterations are done then leave the loop. */
			if ((((*accgood)/(*accbad))<ACCBALANCE)||(tolerance==TOLERANCE2)||(pp>maxiter)){
				*ptolerance=maxpg-minpg;
				break;
			}

			/* Set tolerance to TOLERANCE2. */
			tolerance=TOLERANCE2;
		}
	}

	/* Free memory. */
	MYFREE(alpha);
	MYFREE(indices);

	/* Check that there is enough memory for result. */
	mm=sizeof(learnvector)+(size*(sizeof(result->index[0])+sizeof(result->sparsevect[0])));
	rsinfo=mallinfo2();
	if (maxmemory<=(rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost+mm)){
		return(NULL);
	}

	/* Allocate memory for result. */
	if (NULL==(result=MYALLOC(sizeof(result[0])))){
		return(NULL);
	}
	if (NULL==(result->index=MYALLOC(size*(sizeof(result->index[0])+sizeof(result->sparsevect[0]))))){
		MYFREE(result);
		return(NULL);
	}
	result->sparsevect=(double *)&result->index[size];

	/* Build result and return. */
	result->size=size;
	for (ii=jj=0;ii<VECTORLENGTH;ii++){
		if (fvector[ii]!=0){
			result->index[jj]=ii;
			result->sparsevect[jj]=fvector[ii];
			jj++;
		}
	}
	return(result);
} /* DCDescentMethod */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Get learning accuracy data)  OCJ
 *
 *    This function learning accuracy data for the learning vector
 *    in fvector global variable and a set of clauses feature vectors
 *    corresponding to good and bad clauses extracted by the
 *    LEARNFROM command. The accuracy is computed over the initial
 *    clause feature vectors that are the first orgrecords in
 *    the vectors argument. However all current numrecords records
 *    are evaluated in order to identify misclassified good clauses
 *    by setting the corresponding flag to 2. Flags of well classified
 *    good clauses are reset to 1 if they had a previous value of 2.
 *
 *
 *  ARGUMENTS:
 *
 *    vectors: Pointer to learnvector set of structures.
 *    flags: Pointer to set of flags. Each flag equals 1 or 2 if
 *           the corresponding vector comes from a good clause,
 *           0 otherwise.
 *    numrecords: Number of vectors in vectors set. This may have
 *                been increased from the initial number of records.
 *    orgrecords: Initial number of records in vector set.
 *    acctotal: Pointer to total accuracy achieved.
 *    accgood: Pointer to good clauses accuracy achieved.
 *    accbad: Pointer to bad clauses accuracy achieved.
 *    mcgclause: Pointer to number of misclassified good clauses
 *               on exit.
 *    frstmcgcl: Pointer to index of first misclassified good
 *               clause on exit.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *
 *--------------------------------------------------------------*/
void GetAccuracy(learnvector *vectors,uint8_t *flags,int32_t numrecords,int32_t orgrecords,
		float *acctotal,float *accgood,float *accbad,int32_t *mcgclause,int32_t *frstmcgcl){
	auto double dd;                                      /* Auxiliary */
	auto int32_t ii,jj,kk,mm,nn,pp,qq,rr,ss;             /* Auxiliary */

	/* Count basic statistical parameters, flags[kk] is temporarily set to 2 */
	/* for misclassified good clauses. */
	/* ii = record number. */
	/* jj = number of correctly classified original good clauses. */
	/* pp = number of original good clauses. */
	/* rr = total number of misclassified good clauses. */
	/* kk = first misclassified good clause. */
	/* mm = number of correctly classified bad clauses. */
	/* ss = number of bad clauses. */
	/* nn and qq = for inner loop control. */
	for (ii=jj=mm=pp=rr=ss=0,kk=-1;ii<numrecords;ii++){
		for (nn=0,qq=vectors[ii].size,dd=0.0;nn<qq;nn++){
			dd+=fvector[vectors[ii].index[nn]]*vectors[ii].sparsevect[nn];
		}
		if (flags[ii]){
			if (ii<orgrecords){
				pp++;
				if (dd>0.0){
					flags[ii]=1;
					jj++;
				} else {
					rr++;
					flags[ii]=2;
					if (kk<0){
						kk=ii;
					}
				}
			} else {
				if (dd>0.0){
					flags[ii]=1;
				} else {
					rr++;
					flags[ii]=2;
				}
			}
		} else {
			ss++;
			if (dd<=0.0){
				mm++;
			}
		}
	}

	/* Compute statistics and return. */
	*acctotal=100.0*((double)(jj+mm))/((double)(pp+ss));
	*accgood=100.0*((double)jj)/((double)pp);
	if (ss>0){
		*accbad=100.0*((double)mm)/((double)ss);
	} else {
		*accbad=100.0;
	}
	*mcgclause=rr;
	*frstmcgcl=kk;
	return;
} /* GetAccuracy */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Build a clause vector)  OCJ
 *
 *    This function builds a vector with clause features used in
 *    the ENIGMA learning process. The following documents describe
 *    the methodology used:
 *    - ENIGMA: Efficient Learning-based Inference Guiding Machine
 *    - Clause Features for Theorem Prover Guidance by Jan Jakubův
 *    - Enhancing ENIGMA Given Clause Guidance
 *    All three documents are by Jan Jakubův and Josef Urban.
 *
 *    The main difference with the Enigma methodology is that no
 *    specific symbols are used for predicates and functions, only
 *    generic classes:
 *    - Those that are only in the conjecture.
 *    - Those that are in the conjecture and in axiom or axiom like
 *      formulas.
 *    - Those that are only in axiom or axiom like formulas.
 *    The exception are equality predicates that have their own symbol.
 *    Also the literal polarity (+/-), skolems and variables are symbols
 *    themselves. Therefore there is a total of 10 different symbols.
 *
 *    The following additional global problem features are included:
 *    - Indication that a conjecture exist.
 *    - Indication that axiom files are used.
 *    - Bias component. This component is important if the hyper-plane that
 *      separates good clause vectors from bad clause vectors does not
 *      contain the origin of the feature vector space.
 *
 *    It is the caller's responsibility to free and fvidx vector memory.
 *
 *    IMPORTANT: This function must be called only in the winning process.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Clause for which the vector will be computed.
 *
 *  RETURNS:
 *
 *    Pointer to sparse vector structure or NULL if not enough memory.
 *
 *
 *
 *--------------------------------------------------------------*/
learnvector *GetClauseVector(cmprefix *clause){
	auto learnvector *result;                            /* Resulting sparse vector */
	auto int32_t ii;                                     /* Auxiliary */

	/* If fvidx is NULL then allocate memory for it and initialize fvsize. */
	if (fvidx==NULL){
		if (NULL==(fvidx=MYALLOC(LEARNMEMORYCHUNK*sizeof(fvidx[0])))){
			return(NULL);
		}
	}
	fvsize=LEARNMEMORYCHUNK;

	/* Initialize fvector and fvused. */
	memset(fvector,0,sizeof(fvector));
	fvused=0;

	/* Set vertical, horizontal and static features. */
	if (NOMEMORY==VerticalFeatures(clause)){
		return(NULL);
	}
	if (NOMEMORY==HorizontalFeatures(clause)){
		return(NULL);
	}
	if (NOMEMORY==StaticFeatures(clause)){
		return(NULL);
	}

	/* Bias. */
	fvector[VECTORLENGTH-1]=1;
	if (NOMEMORY==CheckGCVMemory(VECTORLENGTH-1)){
		return(NULL);
	}

	/* Sort fvidx. */
	qsort(fvidx,fvused,sizeof(fvidx[0]),LrnCompare);

	/* Allocate memory for result. */
	if (NULL==(result=MYALLOC(sizeof(result[0])))){
		return(NULL);
	}
	if (NULL==(result->index=MYALLOC(fvused*(sizeof(result->index[0])+sizeof(result->sparsevect[0]))))){
		MYFREE(result);
		return(NULL);
	}
	result->sparsevect=(double *)&result->index[fvused];

	/* Build result and return. */
	result->size=fvused;
	memcpy(result->index,fvidx,fvused*sizeof(fvidx[0]));
	for (ii=0;ii<fvused;ii++){
		result->sparsevect[ii]=fvector[fvidx[ii]];
	}
	return(result);
} /* GetClauseVector */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Compute vertical features of a clause vector)  OCJ
 *
 *    This function computes the vertical features of clause and
 *    adds them to the clause vector. See GetClauseVector()
 *    function for the methodology and bibliography.
 *
 *    Let N=GENERICSYMBOLS be the number of generic symbols for both
 *    predicate and functions. Each feature is a three terms walk.
 *    The first term cannot be a variable so there are 2*N+4 different
 *    types. The second term cannot be a variable nor a polarity so
 *    there are 2*N+2 different symbols. The third term cannot be a
 *    polarity nor a predicate so there are N+2 different symbols.
 *    Therefore the total number of vertical features is:
 *      (2*N+4)*(2*N+2)*(N+2)
 *
 *    Also the maximum depth of functions, skolems and variables
 *    by polarity of the literal where they appear is recorded as
 *    another "vertical" feature. There are (N+2)*2 additional
 *    "vertical" (depth) features of this kind.
 *
 *    If the way that generic symbols are defined is modified then
 *    the assignment of variable ii must be modified according to the
 *    instructions in the specific comments below.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Clause for which the vector will be computed.
 *    vector: Pointer to vector.
 *
 *  RETURNS:
 *
 *    NOMEMORY if not enough memory or 0 if OK.
 *
 *
 *
 *--------------------------------------------------------------*/
int32_t VerticalFeatures(cmprefix *clause){
	auto vertnode *node;                                 /* Pointer to current vertical feature node */
	auto vertnode *newnode;                              /* Pointer to new vertical feature node */
	auto vertnode *topnode;                              /* Pointer to top vertical feature node */
	auto int32_t vectidx;                                /* Vector index for the three terms walk */
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto uint32_t ii;                                    /* Auxiliary */

	/* Loop through clause items. */
	topnode=node=NULL;
	vectidx=0; /* Just to avoid compile warnings. */
	ii=0; /* Just to avoid compile warnings. */
	for (ptr1=&clause->part2.bin->formula[0];ptr1[0]!=UNITEND;
				ptr1=NextItem(ptr1,IMMED)){

		/* Initialize node: top node or new branch. */
		if ((node==NULL)||(ptr1==node->nextitem)){

			/* Top node. Allocate new node. */
			if (node==NULL){
				if (NULL==(topnode=node=MYALLOC(sizeof(vertnode)))){
					return(NOMEMORY);
				}
				node->depth=0;
				node->nextnode=node->prevnode=NULL;

			/* New branch. Backtrack to branching point. */
			} else {
				for (;(node->prevnode!=NULL)&&(ptr1==node->prevnode->nextitem);node=node->prevnode){
				}
			}

		/* Initialize node: continue in the same branch. */
		} else {

			/* No deeper nodes are available. Allocate new node. */
			if (node->nextnode==NULL){
				if (NULL==(newnode=MYALLOC(sizeof(vertnode)))){
					for (node=topnode;node!=NULL;node=newnode){
						newnode=node->nextnode;
						MYFREE(node);
					}
					return(NOMEMORY);
				}
				node->nextnode=newnode;
				newnode->prevnode=node;
				newnode->nextnode=NULL;
				newnode->depth=1+node->depth;
				node=newnode;

			/* A deeper node is available. */
			} else {
				node=node->nextnode;
			}
		}

		/* Update item and nextitem node fields. */
		node->item=ptr1;
		node->nextitem=NextItem(ptr1,OVERSUBTERMS);

		/* This is not the top node. Compute the vector component index */
		/* for the three terms walk that has this node as third item. */
		if (node!=topnode){

			/* Last node. This corresponds to the third item of the three terms walk. */
			/* This node cannot be a predicate or (in)equality. */
			/* Initialize vector index for vertical feature and update */
			/* maximum depth feature. As there are GENERICSYMBOLS+2 different */
			/* types of nodes at this level the variable ii must be set to a */
			/* value between 0 and GENERICSYMBOLS+1. */
			switch ((VARIABLE|FUNCTION)&node->item[0]){
				case FUNCTION:
					if (0==(SKOLEM&(((symbol *)&node->item[1])->symbol)->type)){
						if ((((symbol *)&node->item[1])->symbol)->sinedist<3){
							ii=4*(((symbol *)&node->item[1])->symbol)->sinedist;
						} else {
							ii=GENERICSYMBOLS-4;
						}
						if ((((symbol *)&node->item[1])->symbol)->arity<3){
							ii+=(((symbol *)&node->item[1])->symbol)->arity;
						} else {
							ii+=3;
						}
					} else {
						ii=GENERICSYMBOLS;
					}
					break;
				case VARIABLE:
					ii=GENERICSYMBOLS+1;
					break;
			}
			vectidx=ii*((2*GENERICSYMBOLS)+4)*((2*GENERICSYMBOLS)+2);
			if (NEGATED&topnode->item[0]){
				if (fvector[OFFSET1+(2*ii)]<node->depth){
					if (fvector[OFFSET1+(2*ii)]==0){
						if (NOMEMORY==CheckGCVMemory(OFFSET1+(2*ii))){
							return(NOMEMORY);
						}
					}
					fvector[OFFSET1+(2*ii)]=node->depth;
				}
			} else {
				if (fvector[OFFSET1+(2*ii)+1]<node->depth){
					if (fvector[OFFSET1+(2*ii)+1]==0){
						if (NOMEMORY==CheckGCVMemory(OFFSET1+(2*ii)+1)){
							return(NOMEMORY);
						}
					}
					fvector[OFFSET1+(2*ii)+1]=node->depth;
				}
			}

			/* Next to last node. This corresponds to the second item of the three terms walk. */
			/* This node cannot be a variable. As there are 2*GENERICSYMBOLS+2 different */
			/* types of nodes at this level the variable ii must be set to a */
			/* value between 0 and 2*GENERICSYMBOLS+1. */
			switch ((PREDICATE|EQUALITY|FUNCTION)&node->prevnode->item[0]){
				case FUNCTION:
					if (0==(SKOLEM&(((symbol *)&node->prevnode->item[1])->symbol)->type)){
						if ((((symbol *)&node->prevnode->item[1])->symbol)->sinedist<3){
							ii=4*(((symbol *)&node->prevnode->item[1])->symbol)->sinedist;
						} else {
							ii=GENERICSYMBOLS-4;
						}
						if ((((symbol *)&node->prevnode->item[1])->symbol)->arity<3){
							ii+=(((symbol *)&node->prevnode->item[1])->symbol)->arity;
						} else {
							ii+=3;
						}
					} else {
						ii=GENERICSYMBOLS;
					}
					break;
				case PREDICATE:
					if ((((symbol *)&node->prevnode->item[1])->symbol)->sinedist<3){
						ii=GENERICSYMBOLS+1+(4*(((symbol *)&node->prevnode->item[1])->symbol)->sinedist);
					} else {
						ii=(2*GENERICSYMBOLS)-3;
					}
					if ((((symbol *)&node->prevnode->item[1])->symbol)->arity<3){
						ii+=(((symbol *)&node->prevnode->item[1])->symbol)->arity;
					} else {
						ii+=3;
					}
					break;
				case EQUALITY:
					ii=(2*GENERICSYMBOLS)+1;
					break;
			}
			vectidx+=ii*((2*GENERICSYMBOLS)+4);
			if (ii<=GENERICSYMBOLS){
				if (NEGATED&topnode->item[0]){
					if (fvector[OFFSET1+(2*ii)]<node->prevnode->depth){
						if (fvector[OFFSET1+(2*ii)]==0){
							if (NOMEMORY==CheckGCVMemory(OFFSET1+(2*ii))){
								return(NOMEMORY);
							}
						}
						fvector[OFFSET1+(2*ii)]=node->prevnode->depth;
					}
				} else {
					if (fvector[OFFSET1+(2*ii)+1]<node->prevnode->depth){
						if (fvector[OFFSET1+(2*ii)+1]==0){
							if (NOMEMORY==CheckGCVMemory(OFFSET1+(2*ii)+1)){
								return(NOMEMORY);
							}
						}
						fvector[OFFSET1+(2*ii)+1]=node->prevnode->depth;
					}
				}
			}

			/* Node before next to last node. This corresponds to the first item of the */
			/* three terms walk. This node cannot be a variable. As there are */
			/* 2*GENERICSYMBOLS+4 different types of nodes at this level the variable ii */
			/* must be set to a value between 0 and 2*GENERICSYMBOLS+3. */
			/* Next to last node is the top node. The first item of three terms walk */
			/* is the literal polarity. */
			if (node->depth==1){
				if (NEGATED&topnode->item[0]){
					vectidx+=((2*GENERICSYMBOLS)+2);
				} else {
					vectidx+=((2*GENERICSYMBOLS)+3);
				}

			/* Next to last node is not the last node. The first item in the*/
			/* three terms walk is not a polarity. */
			} else {
				switch ((PREDICATE|EQUALITY|FUNCTION)&node->prevnode->prevnode->item[0]){
					case FUNCTION:
						if (0==(SKOLEM&(((symbol *)&node->prevnode->prevnode->item[1])->symbol)->type)){
							if ((((symbol *)&node->prevnode->prevnode->item[1])->symbol)->sinedist<3){
								ii=4*(((symbol *)&node->prevnode->prevnode->item[1])->symbol)->sinedist;
							} else {
								ii=GENERICSYMBOLS-4;
							}
							if ((((symbol *)&node->prevnode->prevnode->item[1])->symbol)->arity<3){
								ii+=(((symbol *)&node->prevnode->prevnode->item[1])->symbol)->arity;
							} else {
								ii+=3;
							}
						} else {
							ii=GENERICSYMBOLS;
						}
						break;
					case PREDICATE:
						if ((((symbol *)&node->prevnode->prevnode->item[1])->symbol)->sinedist<3){
							ii=GENERICSYMBOLS+1+(4*(((symbol *)&node->prevnode->prevnode->item[1])->symbol)->sinedist);
						} else {
							ii=(2*GENERICSYMBOLS)-3;
						}
						if ((((symbol *)&node->prevnode->prevnode->item[1])->symbol)->arity<3){
							ii+=(((symbol *)&node->prevnode->prevnode->item[1])->symbol)->arity;
						} else {
							ii+=3;
						}
						break;
					case EQUALITY:
						ii=(2*GENERICSYMBOLS)+1;
						break;
				}
				vectidx+=ii;
				if (ii<=GENERICSYMBOLS){
					if (NEGATED&topnode->item[0]){
						if (fvector[OFFSET1+(2*ii)]<node->prevnode->prevnode->depth){
							if (fvector[OFFSET1+(2*ii)]==0){
								if (NOMEMORY==CheckGCVMemory(OFFSET1+(2*ii))){
									return(NOMEMORY);
								}
							}
							fvector[OFFSET1+(2*ii)]=node->prevnode->prevnode->depth;
						}
					} else {
						if (fvector[OFFSET1+(2*ii)+1]<node->prevnode->prevnode->depth){
							if (fvector[OFFSET1+(2*ii)+1]==0){
								if (NOMEMORY==CheckGCVMemory(OFFSET1+(2*ii)+1)){
									return(NOMEMORY);
								}
							}
							fvector[OFFSET1+(2*ii)+1]=node->prevnode->prevnode->depth;
						}
					}
				}
			}

			/* Update vector component. */
			if (fvector[vectidx]==0){
				if (NOMEMORY==CheckGCVMemory(vectidx)){
					return(NOMEMORY);
				}
			}
			fvector[vectidx]++;
		}
	}

	/* Free memory and return. */
	for (node=topnode;node!=NULL;node=newnode){
		newnode=node->nextnode;
		MYFREE(node);
	}
	return(0);
} /* VerticalFeatures */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Compute horizontal features of a clause vector)  OCJ
 *
 *    This function computes the horizontal features of clause and
 *    adds them to the clause vector. See GetClauseVector()
 *    function for the methodology and bibliography.
 *
 *    Only symbols with an arity bigger than 0 and less than or
 *    equal to 3 are considered. However nine vector indices are
 *    reserved for cases with arity bigger than 3, one for each symbol
 *    type not including polarity symbols, equalities and variables.
 *
 *    Let N=GENERICSYMBOLS be the number of generic symbols for both
 *    predicate and functions. The total number of horizontal features is:
 *      2*N+1 (symbols with arity bigger than 3)
 *      (N+2)+(N+1)+...+1=(N+3)*(N+2)/2 (equalities taking into account
 *                                       the symmetry)
 *      (2N+1)*(N+2) (symbols with arity=1)
 *      (2N+1)*(N+2)*(N+2) (symbols with arity=2)
 *      (2N+1)*(N+2)*(N+2)*(N+2) (symbols with arity=3)
 *
 *    If the way that generic symbols are defined is modified then
 *    the assignment of variables ii and jj must be modified according
 *    to the instructions in the specific comments below.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Clause for which the vector will be computed.
 *    vector: Pointer to vector.
 *
 *  RETURNS:
 *
 *    NOMEMORY if not enough memory or 0 if OK.
 *
 *
 *
 *--------------------------------------------------------------*/
int32_t HorizontalFeatures(cmprefix *clause){
	auto uint8_t *ptr1,*ptr2;                            /* Auxiliary pointers */
	auto int32_t vectidx;                                /* Vector index for the horizontal feature */
	auto uint32_t ii;                                    /* Auxiliary */
	auto int32_t jj,kk,mm,pp,qq;                         /* Auxiliary */
	static int16_t offsets[3]={OFFSET4,OFFSET5,OFFSET6}; /* Offsets by arity */

	/* Loop through clause items. */
	ii=0; /* Just to avoid compile warnings. */
	for (ptr1=&clause->part2.bin->formula[0];ptr1[0]!=UNITEND;
				ptr1=NextItem(ptr1,IMMED)){

		/* This item is not a variable and its arity is bigger than zero. */
		if (0==(VARIABLE&ptr1[0])&&((kk=(((symbol *)&ptr1[1])->symbol)->arity)>0)){

			/* The variable ii must be set to a number between zero */
			/* and the number of elements with this arity minus 1. */
			/* This is a number between 0 and GENERICSYMBOLS/2. */
			switch ((PREDICATE|FUNCTION)&ptr1[0]){
				case PREDICATE:
					if ((((symbol *)&ptr1[1])->symbol)->sinedist<3){
						ii=(((symbol *)&ptr1[1])->symbol)->sinedist;
					} else {
						ii=3;
					}
					break;
				case FUNCTION:
					if (0==(SKOLEM&(((symbol *)&ptr1[1])->symbol)->type)){
						if ((((symbol *)&ptr1[1])->symbol)->sinedist<3){
							ii=(GENERICSYMBOLS/4)+(((symbol *)&ptr1[1])->symbol)->sinedist;
						} else {
							ii=(GENERICSYMBOLS/4)+3;
						}
					} else {
						ii=GENERICSYMBOLS/2;
					}
					break;
			}

			/* Arity is bigger than three. Update vector component. */
			if (kk>3){
				vectidx=OFFSET2+ii;
				if (fvector[vectidx]==0){
					if (NOMEMORY==CheckGCVMemory(vectidx)){
						return(NOMEMORY);
					}
				}
				fvector[vectidx]++;

			/* Arity is less than or equal to three. */
			} else {

				/* Loop through first level arguments. */
				jj=pp=qq=0; /* Just to prevent compiler warnings. */
				for (vectidx=ii,mm=0,ptr2=NextItem(ptr1,IMMED);mm<kk;
						mm++,ptr2=NextItem(ptr2,OVERSUBTERMS)){

					/* The variable jj must be set to a number between zero */
					/* and the number of possible first level arguments types */
					/* minus 1. This is a number between 0 and GENERICSYBOLS+1. */
					switch ((FUNCTION|VARIABLE)&ptr2[0]){
						case FUNCTION:
							if (0==(SKOLEM&(((symbol *)&ptr2[1])->symbol)->type)){
								if ((((symbol *)&ptr2[1])->symbol)->sinedist<3){
									jj=4*(((symbol *)&ptr2[1])->symbol)->sinedist;
								} else {
									jj=GENERICSYMBOLS-4;
								}
								if ((((symbol *)&ptr2[1])->symbol)->arity<3){
									jj+=(((symbol *)&ptr2[1])->symbol)->arity;
								} else {
									jj+=3;
								}
							} else {
								jj=GENERICSYMBOLS;
							}
							break;
						case VARIABLE:
							jj=GENERICSYMBOLS+1;
							break;
					}

					/* The ptr1 item is an equality, initialize pp and qq. */
					if (EQUALITY&ptr1[0]){
						if (mm==0){
							pp=jj+1;
						} else {
							qq=jj+1;
						}

					/* The ptr1 item is not an equality, update vector index. */
					} else {
						vectidx=(vectidx*(GENERICSYMBOLS+2))+jj;
					}
				}

				/* Finish vector index calculation. */
				if (EQUALITY&ptr1[0]){
					if (pp>=qq){
						vectidx=OFFSET3-1+qq+((pp*(pp-1))/2);
					} else {
						vectidx=OFFSET3-1+pp+((qq*(qq-1))/2);
					}
				} else {
					vectidx+=offsets[kk-1];
				}

				/* Update vector component. */
				if (fvector[vectidx]==0){
					if (NOMEMORY==CheckGCVMemory(vectidx)){
						return(NOMEMORY);
					}
				}
				fvector[vectidx]++;
			}
		}
	}
	return(0);
} /* HorizontalFeatures */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Compute static features of a clause vector)  OCJ
 *
 *    This function computes the static features of clause and
 *    adds them to the clause vector. See GetClauseVector()
 *    function for the methodology and bibliography.
 *
 *    Let N=GENERICSYMBOLS be the number of generic symbols for both
 *    predicate and functions. The following features are calculated:
 *    - Symbol count by polarity ((2N+3)x2 features).
 *    - Clause length and number of positive and negative literals
 *      (3 features).
 *
 *    If the way that generic symbols are defined is modified then
 *    the assignment of variable ii must be modified according to the
 *    instructions in the specific comments below.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Clause for which the vector will be computed.
 *    vector: Pointer to vector.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *
 *--------------------------------------------------------------*/
int32_t StaticFeatures(cmprefix *clause){
	auto uint8_t *ptr1;                                 /* Auxiliary pointer */
	auto int32_t ii,jj;                                 /* Auxiliary */

	/* Loop through clause items. */
	jj=0; /* To prevent compiler warnings. */
	for (ptr1=&clause->part2.bin->formula[0];ptr1[0]!=UNITEND;
				ptr1=NextItem(ptr1,IMMED)){


		/* Clause length. */
		if (fvector[OFFSET8]==0){
			if (NOMEMORY==CheckGCVMemory(OFFSET8)){
				return(NOMEMORY);
			}
		}
		fvector[OFFSET8]++;

		/* Symbol and literal count. */
		switch ((EQUALITY|PREDICATE|FUNCTION|VARIABLE)&ptr1[0]){
			case EQUALITY:

				/* Symbol and literal count, negative polarity. */
				if (NEGATED&ptr1[0]){
					jj=1;
					if (fvector[OFFSET7]==0){
						if (NOMEMORY==CheckGCVMemory(OFFSET7)){
							return(NOMEMORY);
						}
					}
					fvector[OFFSET7]++;
					if (fvector[OFFSET8+1]==0){
						if (NOMEMORY==CheckGCVMemory(OFFSET8+1)){
							return(NOMEMORY);
						}
					}
					fvector[OFFSET8+1]++;

				/* Symbol and literal count, positive polarity. */
				} else {
					jj=0;
					if (fvector[OFFSET7+1]==0){
						if (NOMEMORY==CheckGCVMemory(OFFSET7+1)){
							return(NOMEMORY);
						}
					}
					fvector[OFFSET7+1]++;
					if (fvector[OFFSET8+2]==0){
						if (NOMEMORY==CheckGCVMemory(OFFSET8+2)){
							return(NOMEMORY);
						}
					}
					fvector[OFFSET8+2]++;
				}
				break;
			case PREDICATE:

				/* Set variable ii according to the type of predicate */
				/* to a value between 0 and GENERICSYMBOLS-1. */
				if ((((symbol *)&ptr1[1])->symbol)->sinedist<3){
					ii=4*(((symbol *)&ptr1[1])->symbol)->sinedist;
				} else {
					ii=GENERICSYMBOLS-4;
				}
				if ((((symbol *)&ptr1[1])->symbol)->arity<3){
					ii+=(((symbol *)&ptr1[1])->symbol)->arity;
				} else {
					ii+=3;
				}

				/* Symbol and literal count, negative polarity. */
				if (NEGATED&ptr1[0]){
					jj=1;
					if (fvector[OFFSET7+2+ii]==0){
						if (NOMEMORY==CheckGCVMemory(OFFSET7+2+ii)){
							return(NOMEMORY);
						}
					}
					fvector[OFFSET7+2+ii]++;
					if (fvector[OFFSET8+1]==0){
						if (NOMEMORY==CheckGCVMemory(OFFSET8+1)){
							return(NOMEMORY);
						}
					}
					fvector[OFFSET8+1]++;

				/* Symbol and literal count, positive polarity. */
				} else {
					jj=0;
					if (fvector[OFFSET7+GENERICSYMBOLS+2+ii]==0){
						if (NOMEMORY==CheckGCVMemory(OFFSET7+GENERICSYMBOLS+2+ii)){
							return(NOMEMORY);
						}
					}
					fvector[OFFSET7+GENERICSYMBOLS+2+ii]++;
					if (fvector[OFFSET8+2]==0){
						if (NOMEMORY==CheckGCVMemory(OFFSET8+2)){
							return(NOMEMORY);
						}
					}
					fvector[OFFSET8+2]++;
				}
				break;
			case FUNCTION:

				/* Set variable ii according to the type of function */
				/* to a value between 0 and GENERICSYMBOLS. */
				if (0==(SKOLEM&(((symbol *)&ptr1[1])->symbol)->type)){
					if ((((symbol *)&ptr1[1])->symbol)->sinedist<3){
						ii=4*(((symbol *)&ptr1[1])->symbol)->sinedist;
					} else {
						ii=GENERICSYMBOLS-4;
					}
					if ((((symbol *)&ptr1[1])->symbol)->arity<3){
						ii+=(((symbol *)&ptr1[1])->symbol)->arity;
					} else {
						ii+=3;
					}
				} else {
					ii=GENERICSYMBOLS;
				}

				/* Symbol count, negative polarity. */
				if (jj){
					if (fvector[OFFSET7+(2*GENERICSYMBOLS)+2+ii]==0){
						if (NOMEMORY==CheckGCVMemory(OFFSET7+(2*GENERICSYMBOLS)+2+ii)){
							return(NOMEMORY);
						}
					}
					fvector[OFFSET7+(2*GENERICSYMBOLS)+2+ii]++;

				/* Symbol count, positive polarity. */
				} else {
					if (fvector[OFFSET7+(3*GENERICSYMBOLS)+3+ii]==0){
						if (NOMEMORY==CheckGCVMemory(OFFSET7+(3*GENERICSYMBOLS)+3+ii)){
							return(NOMEMORY);
						}
					}
					fvector[OFFSET7+(3*GENERICSYMBOLS)+3+ii]++;
				}
				break;
			case VARIABLE:

				/* Symbol count, negative polarity. */
				if (jj){
					if (fvector[OFFSET7+(4*GENERICSYMBOLS)+4]==0){
						if (NOMEMORY==CheckGCVMemory(OFFSET7+(4*GENERICSYMBOLS)+4)){
							return(NOMEMORY);
						}
					}
					fvector[OFFSET7+(4*GENERICSYMBOLS)+4]++;

				/* Symbol count, positive polarity. */
				} else {
					if (fvector[OFFSET7+(4*GENERICSYMBOLS)+5]==0){
						if (NOMEMORY==CheckGCVMemory(OFFSET7+(4*GENERICSYMBOLS)+5)){
							return(NOMEMORY);
						}
					}
					fvector[OFFSET7+(4*GENERICSYMBOLS)+5]++;
				}
				break;
		}
	}
	return(0);
} /* StaticFeatures */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Extract learning information from solved problem)  OCJ
 *
 *    This function extracts learning information from a solved problem
 *    and adds it to the file specified by the LEARNTO command. If the
 *    file already exist then the learning information is just appended
 *    to the file without verifying that it is a valid LEARNTO file.
 *    It is assumed that a problem has been just solved and that a proof
 *    has been built so that formula used in the proof is marked with
 *    INPROOFQUEUE flag.
 *
 *    The function scans text, passive, active, unprocessed and locked
 *    clauses. For every clause with the PICKED flag a record is built
 *    and added to the learnto file.
 *
 *    LEARNTO FILE FORMAT:
 *    -------------------
 *      block1 block2 ... blockn
 *
 *    where:
 *      blocki: A block corresponding to records of a specific type.
 *
 *    BLOCK FORMAT:
 *    ------------
 *      size length header f_1 s_1 i_1 c_1 f_2 s_2 i_2 c_2 ... f_n s_n i_n c_n
 *
 *    where:
 *      size: 64 bit unsigned integer with the number of vectors in block.
 *      length: 64 bit unsigned integer with the number of bytes occupied
 *              by the header, flags and vectors.
 *      header: 64 bit unsigned integer with the type of block. See header field
 *              of learnrecord structure for details about types of block.
 *      f_i: 8 bit integer with one if feature vector i belongs to
 *           a good clause, 0 otherwise.
 *      s_i: 32 bit integer with the number of sparse vector non null components.
 *      i_i: Set of s_i 32 bit indices of non null components in sparse vector.
 *      c_i: Set of s_i double precision non null components of sparse vector.
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
 *
 *--------------------------------------------------------------*/
void LearnFromProblem(){
	auto FILE *file;                                    /* Learn to file handle */
	auto uint64_t header;                               /* Common part of the header */
	auto learnvector *vector;                           /* Pointer to clause vector */
	auto cmprefix *tmpclause;                           /* Temporary binary clause built from a text clause. */
	auto cmprefix *ptr1;                                /* Auxiliary pointer */
	auto char *ptr2,*ptr3,*ptr4,*ptr5;                  /* Auxiliary pointers */
	auto char *ptr6,*ptr7,*ptr8;                        /* Auxiliary pointers */
	auto uint64_t ii,jj,mm,nn;                          /* Auxiliary */
	auto int32_t pp;                                    /* Auxiliary */
	auto int8_t kk;                                     /* Auxiliary */

	/* Set prefix for printed lines. */
	if (pgmmode==NONINTERACTIVE){
		ptr4="% ";
	} else {
		ptr4="";
	}

	/* No learning is extracted if the problem was solved during pre-processing. */
	if (procctl->solvekb==active_cores){
		return;
	}

	/* Build the header according to the strategy parameters of the winning */
	/* process and the type of problem. */
	header=0;
	switch (kbset.opts.algorithm){
		case OTTER:
			header|=OTTER_P;
			break;
		case UEQDSC:
			header|=UEQDSC_P;
			break;
		case UEQOTT:
			header|=UEQOTT_P;
			break;
		case DISCOUNT:
		default:
			header|=DISCOUNT_P;
			break;
	}
	switch (kbset.opts.select){
		case SINGLENEG:
			header|=SINGLENEG_P;
			break;
		case MULTINEG:
			header|=MULTINEG_P;
			break;
		case MAXIMAL:
			header|=MAXIMAL_P;
			break;
		case SINGLEPOS:
			header|=SINGLEPOS_P;
			break;
		case MULTIPOS:
			header|=MULTIPOS_P;
			break;
		case SELECTALL:
			header|=SELECTALL_P;
			break;
		case MXSINGLENEG:
			header|=MXSINGLENEG_P;
			break;
		case TYPE1:
			header|=TYPE1_P;
			break;
		case TYPE2:
			header|=TYPE2_P;
			break;
		case TYPE3:
			header|=TYPE3_P;
			break;
		case TYPE4:
			header|=TYPE4_P;
			break;
		case TYPE5:
			header|=TYPE5_P;
			break;
		case TYPE6:
			header|=TYPE6_P;
			break;
		case TYPE1CPL:
			header|=TYPE1CPL_P;
			break;
		case TYPE2CPL:
			header|=TYPE2CPL_P;
			break;
		case TYPE3CPL:
			header|=TYPE3CPL_P;
			break;
		case TYPE4CPL:
			header|=TYPE4CPL_P;
			break;
		case TYPE5CPL:
			header|=TYPE5CPL_P;
			break;
		case TYPE6CPL:
		default:
			header|=TYPE6CPL_P;
			break;
	}
	if (kbset.opts.termord==NONRECURSIVE){
		header|=TRMORDNR_P;
	} else {
		header|=TRMORDSTD_P;
	}
	if (kbset.opts.litord==STANDARD){
		header|=LITORDSTD_P;
	} else {
		header|=LITORDNR_P;
	}
	if (kbset.opts.fweight==ARITY){
		header|=ARITY_P;
	} else {
		header|=UNIFORM_P;
	}
	if (kbset.opts.split){
		header|=SPLITON_P;
	} else {
		header|=SPLITOFF_P;
	}
	if (kbset.opts.weakrw){
		header|=WEAKRWON_P;
	} else {
		header|=WEAKRWOFF_P;
	}
	ii=pbmtype;
	ii<<=56;
	header|=ii;

	/* Open learnto file. */
	printf("%s**********************************************\n",ptr4);
	printf("%sExtracting learning information from problem...\n",ptr4);
	if (learnnew){
		if (NULL==(file=fopen(learnto,"w+b"))){
			printf("%s%s opening file %s.\n",ptr4,strerror(errno),learnto);
			printf("%sLearning extraction not completed.\n",ptr4);
			printf("%s**********************************************\n",ptr4);
			return;
		}
	} else {
		if (NULL==(file=fopen(learnto,"r+b"))){
			if (NULL==(file=fopen(learnto,"w+b"))){
				printf("%s%s opening file %s.\n",ptr4,strerror(errno),learnto);
				printf("%sLearning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
		}
	}

	/* Get file length. */
	fseek(file,0,SEEK_END);
	mm=ftell(file);

	/* Loop through text clauses. */
	ii=0; /* Number of vectors added to block (value of size field in the new block). */
	nn=sizeof(header); /* Value of length field in new block. */
	jj=0; /* Just to avoid compiler warnings. */
	for (ptr1=kbset.firsttxt;ptr1!=NULL;ptr1=ptr1->next){
		if (PICKED&ptr1->flags){

			/* Write size, length and header fields if first record. */
			/* Update number of written records. */
			if (ii==0){
				if (1>fwrite(&ii,sizeof(ii),1,file)){
					fclose(file);
					printf("%sError writing to file %s.\n",ptr4,learnto);
					printf("%sLearning extraction not performed.\n",ptr4);
					printf("%s**********************************************\n",ptr4);
					return;
				}
				if (1>fwrite(&ii,sizeof(ii),1,file)){
					fclose(file);
					printf("%sError writing to file %s.\n",ptr4,learnto);
					printf("%sLearning extraction not performed.\n",ptr4);
					printf("%s**********************************************\n",ptr4);
					return;
				}
				if (1>fwrite(&header,sizeof(header),1,file)){
					fclose(file);
					printf("%sError writing to file %s.\n",ptr4,learnto);
					printf("%sLearning extraction not performed.\n",ptr4);
					printf("%s**********************************************\n",ptr4);
					return;
				}
			}
			ii++;

			/* Remove the possible clause assertions. */
			/* First jump over all single and double quotes. */
			ptr2=ptr1->part2.text;
			while (1){
				ptr3=strchr(ptr2,'\'');
				ptr8=strchr(ptr2,'"');
				if (ptr3==NULL){
					if (ptr8==NULL){
						break;
					}
					ptr2=ptr8+1;
				} else {
					if ((ptr8==NULL)||(ptr3>ptr8)){
						ptr2=ptr3+1;
					} else {
						ptr2=ptr8+1;
					}
				}
			}

			/* Clause is in Drodi style. */
			if ((proofmode==DRODI)||(syntaxmode==DRODI)){
				if (NULL!=(ptr3=strchr(ptr2,'<'))){
					jj=*(ptr3-1);
					*(ptr3-1)=0;
				}

			/* Clause is in TPTP format. */
			} else {

				/* Loop through OR operators. */
				while (NULL!=(ptr3=strchr(ptr2,'|'))){

					/* Set ptr7 pointing to character preceding the atom */
					/* (it can be an OR operator or a negation). */
					if (ptr3[1]=='~'){
						ptr7=ptr3+2;
					} else {
						ptr7=ptr3+1;
					}

					/* Check if the atom is an introduced split symbol. */
					/* The second if and the second condition of the third if */
					/* are redundants but they are left just in case. */
					if ((strlen(ptr7)>=(1+strlen(SPLPREFIX)+strlen(SPLPOSTFIX)))
							&&(0==strncmp(SPLPREFIX,&ptr7[0],strlen(SPLPREFIX)))
							&&(NULL!=(ptr5=strstr(&ptr7[strlen(SPLPREFIX)+1],SPLPOSTFIX)))){
						if ((ptr5[strlen(SPLPOSTFIX)]==0)||(ptr5[strlen(SPLPOSTFIX)]=='|')){
							pp=strtol(&ptr7[strlen(SPLPREFIX)],&ptr6,10);
							if ((ptr5==ptr6)&&(pp<(splid+kbset.splsymbols))){
								ptr3[0]=0;
								break;
							}
						}
					}
					ptr2=ptr3+1;
				}
			}

			/* Compile text clause. Variables must be renumbered as this is needed */
			/* because SelectItems() will be called by GetClauseVector(). */
			if (NOMEMORY==(CompileClause(ptr1->part2.text,&tmpclause,NULL))){
				fclose(file);
				printf("%sNot enough memory, learning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			tmpclause->flags=ptr1->flags;
			RenumberVars(tmpclause->part2.bin);

			/* Restore the possibly removed clause assertions. */
			if (ptr3!=NULL){
				if ((proofmode==DRODI)||(syntaxmode==DRODI)){
					*(ptr3-1)=jj;
				} else {
					ptr3[0]='|';
				}
			}

			/* Save learn record and increment nn with the number of bytes */
			/* added to the block. */
			if (INPROOFQUEUE&ptr1->flags){
				kk=1;
			} else {
				kk=0;
			}
			if (1>fwrite(&kk,sizeof(kk),1,file)){
				fclose(file);
				MYFREE(tmpclause->part2.bin);
				MYFREE(tmpclause);
				printf("%sError writing to file %s.\n",ptr4,learnto);
				printf("%sLearning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			if (NULL==(vector=GetClauseVector(tmpclause))){
				fclose(file);
				MYFREE(tmpclause->part2.bin);
				MYFREE(tmpclause);
				printf("%sNot enough memory, learning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			nn+=sizeof(vector->size)+(sizeof(kk)+(vector->size*(sizeof(*vector->index)+sizeof(*vector->sparsevect))));
			if (1>fwrite(&vector->size,sizeof(vector->size),1,file)){
				fclose(file);
				MYFREE(tmpclause->part2.bin);
				MYFREE(tmpclause);
				MYFREE(vector);
				printf("%sError writing to file %s.\n",ptr4,learnto);
				printf("%sLearning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			if (vector->size>fwrite(vector->index,sizeof(*vector->index)+sizeof(*vector->sparsevect),
					vector->size,file)){
				fclose(file);
				MYFREE(tmpclause->part2.bin);
				MYFREE(tmpclause);
				MYFREE(vector);
				printf("%sError writing to file %s.\n",ptr4,learnto);
				printf("%sLearning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			MYFREE(tmpclause->part2.bin);
			MYFREE(tmpclause);
			MYFREE(vector->index);
			MYFREE(vector);
		}
	}

	/* Loop through unprocessed clauses and save learn records. */
	for (ptr1=kbset.frstunproc;ptr1!=NULL;ptr1=ptr1->next){
		if (PICKED&ptr1->flags){

			/* Write size, length and header fields if first record. */
			/* Update number of written records. */
			if (ii==0){
				if (1>fwrite(&ii,sizeof(ii),1,file)){
					fclose(file);
					printf("%sError writing to file %s.\n",ptr4,learnto);
					printf("%sLearning extraction not performed.\n",ptr4);
					printf("%s**********************************************\n",ptr4);
					return;
				}
				if (1>fwrite(&ii,sizeof(ii),1,file)){
					fclose(file);
					printf("%sError writing to file %s.\n",ptr4,learnto);
					printf("%sLearning extraction not performed.\n",ptr4);
					printf("%s**********************************************\n",ptr4);
					return;
				}
				if (1>fwrite(&header,sizeof(header),1,file)){
					fclose(file);
					printf("%sError writing to file %s.\n",ptr4,learnto);
					printf("%sLearning extraction not performed.\n",ptr4);
					printf("%s**********************************************\n",ptr4);
					return;
				}
			}
			ii++;

			/* Save learn record. */
			if (INPROOFQUEUE&ptr1->flags){
				kk=1;
			} else {
				kk=0;
			}
			if (1>fwrite(&kk,sizeof(kk),1,file)){
				fclose(file);
				printf("%sError writing to file %s.\n",ptr4,learnto);
				printf("%sLearning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			if (NULL==(vector=GetClauseVector(ptr1))){
				fclose(file);
				printf("%sNot enough memory, learning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			nn+=sizeof(vector->size)+(sizeof(kk)+(vector->size*(sizeof(*vector->index)+sizeof(*vector->sparsevect))));
			if (1>fwrite(&vector->size,sizeof(vector->size),1,file)){
				fclose(file);
				MYFREE(tmpclause->part2.bin);
				MYFREE(tmpclause);
				MYFREE(vector);
				printf("%sError writing to file %s.\n",ptr4,learnto);
				printf("%sLearning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			if (vector->size>fwrite(vector->index,sizeof(*vector->index)+sizeof(*vector->sparsevect),
					vector->size,file)){
				fclose(file);
				MYFREE(tmpclause->part2.bin);
				MYFREE(tmpclause);
				MYFREE(vector);
				printf("%sError writing to file %s.\n",ptr4,learnto);
				printf("%sLearning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			MYFREE(vector->index);
			MYFREE(vector);
		}
	}

	/* Loop through passive clauses and save learn records. */
	for (ptr1=kbset.frstpassive;ptr1!=NULL;ptr1=ptr1->next){
		if (PICKED&ptr1->flags){

			/* Write size, length and header fields if first record. */
			/* Update number of written records. */
			if (ii==0){
				if (1>fwrite(&ii,sizeof(ii),1,file)){
					fclose(file);
					printf("%sError writing to file %s.\n",ptr4,learnto);
					printf("%sLearning extraction not performed.\n",ptr4);
					printf("%s**********************************************\n",ptr4);
					return;
				}
				if (1>fwrite(&ii,sizeof(ii),1,file)){
					fclose(file);
					printf("%sError writing to file %s.\n",ptr4,learnto);
					printf("%sLearning extraction not performed.\n",ptr4);
					printf("%s**********************************************\n",ptr4);
					return;
				}
				if (1>fwrite(&header,sizeof(header),1,file)){
					fclose(file);
					printf("%sError writing to file %s.\n",ptr4,learnto);
					printf("%sLearning extraction not performed.\n",ptr4);
					printf("%s**********************************************\n",ptr4);
					return;
				}
			}
			ii++;

			/* Save learn record. */
			if (INPROOFQUEUE&ptr1->flags){
				kk=1;
			} else {
				kk=0;
			}
			if (1>fwrite(&kk,sizeof(kk),1,file)){
				fclose(file);
				printf("%sError writing to file %s.\n",ptr4,learnto);
				printf("%sLearning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			if (NULL==(vector=GetClauseVector(ptr1))){
				fclose(file);
				printf("%sNot enough memory, learning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			nn+=sizeof(vector->size)+(sizeof(kk)+(vector->size*(sizeof(*vector->index)+sizeof(*vector->sparsevect))));
			if (1>fwrite(&vector->size,sizeof(vector->size),1,file)){
				fclose(file);
				MYFREE(tmpclause->part2.bin);
				MYFREE(tmpclause);
				MYFREE(vector);
				printf("%sError writing to file %s.\n",ptr4,learnto);
				printf("%sLearning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			if (vector->size>fwrite(vector->index,sizeof(*vector->index)+sizeof(*vector->sparsevect),
					vector->size,file)){
				fclose(file);
				MYFREE(tmpclause->part2.bin);
				MYFREE(tmpclause);
				MYFREE(vector);
				printf("%sError writing to file %s.\n",ptr4,learnto);
				printf("%sLearning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			MYFREE(vector->index);
			MYFREE(vector);
		}
	}

	/* Loop through active clauses and save learn records. */
	for (ptr1=kbset.frstactive;ptr1!=NULL;ptr1=ptr1->next){
		if (PICKED&ptr1->flags){

			/* Write size, length and header fields if first record. */
			/* Update number of written records. */
			if (ii==0){
				if (1>fwrite(&ii,sizeof(ii),1,file)){
					fclose(file);
					printf("%sError writing to file %s.\n",ptr4,learnto);
					printf("%sLearning extraction not performed.\n",ptr4);
					printf("%s**********************************************\n",ptr4);
					return;
				}
				if (1>fwrite(&ii,sizeof(ii),1,file)){
					fclose(file);
					printf("%sError writing to file %s.\n",ptr4,learnto);
					printf("%sLearning extraction not performed.\n",ptr4);
					printf("%s**********************************************\n",ptr4);
					return;
				}
				if (1>fwrite(&header,sizeof(header),1,file)){
					fclose(file);
					printf("%sError writing to file %s.\n",ptr4,learnto);
					printf("%sLearning extraction not performed.\n",ptr4);
					printf("%s**********************************************\n",ptr4);
					return;
				}
			}
			ii++;

			/* Save learn record. */
			if (INPROOFQUEUE&ptr1->flags){
				kk=1;
			} else {
				kk=0;
			}
			if (1>fwrite(&kk,sizeof(kk),1,file)){
				fclose(file);
				printf("%sError writing to file %s.\n",ptr4,learnto);
				printf("%sLearning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			if (NULL==(vector=GetClauseVector(ptr1))){
				fclose(file);
				printf("%sNot enough memory, learning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			nn+=sizeof(vector->size)+(sizeof(kk)+(vector->size*(sizeof(*vector->index)+sizeof(*vector->sparsevect))));
			if (1>fwrite(&vector->size,sizeof(vector->size),1,file)){
				fclose(file);
				MYFREE(tmpclause->part2.bin);
				MYFREE(tmpclause);
				MYFREE(vector);
				printf("%sError writing to file %s.\n",ptr4,learnto);
				printf("%sLearning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			if (vector->size>fwrite(vector->index,sizeof(*vector->index)+sizeof(*vector->sparsevect),
					vector->size,file)){
				fclose(file);
				MYFREE(tmpclause->part2.bin);
				MYFREE(tmpclause);
				MYFREE(vector);
				printf("%sError writing to file %s.\n",ptr4,learnto);
				printf("%sLearning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			MYFREE(vector->index);
			MYFREE(vector);
		}
	}

	/* Loop through locked clauses and save learn records. */
	for (ptr1=kbset.frstlocked;ptr1!=NULL;ptr1=ptr1->next){
		if (PICKED&ptr1->flags){

			/* Write size, length and header fields if first record. */
			/* Update number of written records. */
			if (ii==0){
				if (1>fwrite(&ii,sizeof(ii),1,file)){
					fclose(file);
					printf("%sError writing to file %s.\n",ptr4,learnto);
					printf("%sLearning extraction not performed.\n",ptr4);
					printf("%s**********************************************\n",ptr4);
					return;
				}
				if (1>fwrite(&ii,sizeof(ii),1,file)){
					fclose(file);
					printf("%sError writing to file %s.\n",ptr4,learnto);
					printf("%sLearning extraction not performed.\n",ptr4);
					printf("%s**********************************************\n",ptr4);
					return;
				}
				if (1>fwrite(&header,sizeof(header),1,file)){
					fclose(file);
					printf("%sError writing to file %s.\n",ptr4,learnto);
					printf("%sLearning extraction not performed.\n",ptr4);
					printf("%s**********************************************\n",ptr4);
					return;
				}
			}
			ii++;

			/* Save learn record. */
			if (INPROOFQUEUE&ptr1->flags){
				kk=1;
			} else {
				kk=0;
			}
			if (1>fwrite(&kk,sizeof(kk),1,file)){
				fclose(file);
				printf("%sError writing to file %s.\n",ptr4,learnto);
				printf("%sLearning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			if (NULL==(vector=GetClauseVector(ptr1))){
				fclose(file);
				printf("%sNot enough memory, learning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			nn+=sizeof(vector->size)+(sizeof(kk)+(vector->size*(sizeof(*vector->index)+sizeof(*vector->sparsevect))));
			if (1>fwrite(&vector->size,sizeof(vector->size),1,file)){
				fclose(file);
				MYFREE(tmpclause->part2.bin);
				MYFREE(tmpclause);
				MYFREE(vector);
				printf("%sError writing to file %s.\n",ptr4,learnto);
				printf("%sLearning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			if (vector->size>fwrite(vector->index,sizeof(*vector->index)+sizeof(*vector->sparsevect),
					vector->size,file)){
				fclose(file);
				MYFREE(tmpclause->part2.bin);
				MYFREE(tmpclause);
				MYFREE(vector);
				printf("%sError writing to file %s.\n",ptr4,learnto);
				printf("%sLearning extraction not completed.\n",ptr4);
				printf("%s**********************************************\n",ptr4);
				return;
			}
			MYFREE(vector->index);
			MYFREE(vector);
		}
	}

	/* Update block size and length fields if some records were written. */
	if (ii){
		fseek(file,mm,SEEK_SET);
		if (1>fwrite(&ii,sizeof(ii),1,file)){
			fclose(file);
			printf("%sError writing to file %s.\n",ptr4,learnto);
			printf("%sLearning extraction not completed.\n",ptr4);
			printf("%s**********************************************\n",ptr4);
			return;
		}
		if (1>fwrite(&nn,sizeof(nn),1,file)){
			fclose(file);
			printf("%sError writing to file %s.\n",ptr4,learnto);
			printf("%sLearning extraction not completed.\n",ptr4);
			printf("%s**********************************************\n",ptr4);
			return;
		}
	}

	/* Close file and return. */
	fclose(file);
	printf("%s%ld records added to file %s\n",ptr4,ii,learnto);
	printf("%s**********************************************\n",ptr4);
	learnnew=0;
	return;
} /* LearnFromProblem */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Read learn records generated by PROCLEARN command)  OCJ
 *
 *    This function reads learn records generated by PROCLEARN command
 *    and initializes lrnrecords and numlrnrecs global variables. Only
 *    learn records with a header that matches the current problem type
 *    will be loaded. See ProcessLearnData() global comments for a
 *    description of the file generated by PROCLEARN command that is
 *    the input to this function.
 *
 *    If a learning file has been specified with the command LEARNFROM
 *    and therefore learnfrom is not NULL then the learning data will
 *    be read from file. Otherwise the internal learning data in learn[]
 *    global variable will be used.
 *
 *    If the file pointed by learnfrom is not valid then learnfrom
 *    global variable is freed and set to NULL, thus setting the
 *    LEARNFROM option to OFF and internal learning data will be used.
 *
 *    It is the caller's responsibility to free memory allocated to
 *    lrnrecords.
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
 *
 *--------------------------------------------------------------*/
void ReadLearnRecords(void){
	auto FILE *filein;                                   /* Input file handle */
	auto learnrecord *ptr1;                              /* Auxiliary pointer */
	auto uint64_t pbmtypemask;                           /* Problem type mask for record header comparison */
	auto char *ptr2;                                     /* Auxiliary pointer */
	auto int32_t *ptr3;                                  /* Auxiliary pointer */
	auto uint64_t ii;                                    /* Auxiliary */
	auto int32_t jj,kk;                                  /* Auxiliary */

	/* Set prefix for printed lines and pbmtypemask. */
	if (pgmmode==NONINTERACTIVE){
		ptr2="% ";
	} else {
		ptr2="";
	}
	pbmtypemask=((uint64_t)(PBMTYPES-1))<<56;

	/* Allocate space for lrnrecords and initialize numlrnrecs. */
	/* The space allocated for lrnrecords is enough to fit all */
	/* possible records of one problem type. */
	if (NULL==(ptr1=MYALLOC(MAXLEARNRECORDS*sizeof(learnrecord)))){
		printf("%sNot enough memory for learning data.\n",ptr2);
		MYFREE(learnfrom);
		learnfrom=NULL;
		numlrnrecs=0;
		return;
	}
	lrnrecords=ptr1;

	/* A learning file has been specified. */
	if (NULL!=learnfrom){

		/* Open input file. */
		if (verbose){
			printf("%sReading learning file %s\n",ptr2,learnfrom);
		}

		/* Error opening learning file. */
		if (NULL==(filein=fopen(learnfrom,"rb"))){
			printf("%sLEARNFROM error: %s opening input file %s.\n",ptr2,strerror(errno),learnfrom);
			MYFREE(learnfrom);
			learnfrom=NULL;

		/* Read learning file. */
		} else {

			/* Read and validate learning records. */
			for (numlrnrecs=0;numlrnrecs<MAXLEARNRECORDS;){

				/* Read learning record header and sparse vector size. */
				if (0==fread(&lrnrecords[numlrnrecs].header,sizeof(lrnrecords[0].header),1,filein)){
					if (ferror(filein)){
						printf("%sLEARNFROM error: %s error reading input file %s.\n",ptr2,strerror(errno),learnfrom);
						for (jj=0;jj<numlrnrecs;jj++){
							MYFREE(lrnrecords[jj].vector.index);
						}
						learnfrom=NULL;
					}
					break;
				}
				if (0==fread(&lrnrecords[numlrnrecs].vector.size,sizeof(lrnrecords[0].vector.size),1,filein)){
					if (ferror(filein)){
						printf("%sLEARNFROM error: %s error reading input file %s.\n",ptr2,strerror(errno),learnfrom);
					} else {
						printf("%sLEARNFROM error: invalid %s input file.\n",ptr2,learnfrom);
					}
					for (jj=0;jj<numlrnrecs;jj++){
						MYFREE(lrnrecords[jj].vector.index);
					}
					learnfrom=NULL;
					break;
				}

				/* Validate learning record. */
				if (((0xffffffffffffff&lrnrecords[numlrnrecs].header)!=((ALGMASK|SELECTMASK|TERMORDRMASK|
						LITORDRMASK|FUNCWMASK|WEAKRWMASK|SPLITMASK)&lrnrecords[numlrnrecs].header))
						||((0xff00000000000000&lrnrecords[numlrnrecs].header)>pbmtypemask)){
					printf("%sLEARNFROM error: invalid %s input file.\n",ptr2,learnfrom);
					for (jj=0;jj<numlrnrecs;jj++){
						MYFREE(lrnrecords[jj].vector.index);
					}
					learnfrom=NULL;
					break;
				}

				/* Check problem type match. Problem types 3 and 8 match each other. */
				ii=lrnrecords[numlrnrecs].header>>56;
				if ((ii!=pbmtype)&&((pbmtype!=3)||(ii!=8))&&((pbmtype!=8)||(ii!=3))){
					fseek(filein,lrnrecords[numlrnrecs].vector.size*
							(sizeof(lrnrecords[0].vector.index[0])+sizeof(lrnrecords[0].vector.sparsevect[0])),
							SEEK_CUR);
					continue;
				}

				/* Allocate memory for sparse vector data and read the data. */
				if (NULL==(ptr3=MYALLOC(lrnrecords[numlrnrecs].vector.size*
						(sizeof(lrnrecords[0].vector.index[0])+sizeof(lrnrecords[0].vector.sparsevect[0]))))){
					printf("%sLEARNFROM error: not enough memory.\n",ptr2);
					for (jj=0;jj<numlrnrecs;jj++){
						MYFREE(lrnrecords[jj].vector.index);
					}
					learnfrom=NULL;
					break;
				}
				lrnrecords[numlrnrecs].vector.index=ptr3;
				lrnrecords[numlrnrecs].vector.sparsevect=
						(double *)&lrnrecords[numlrnrecs].vector.index[lrnrecords[numlrnrecs].vector.size];
				if (0==fread(lrnrecords[numlrnrecs].vector.index,lrnrecords[numlrnrecs].vector.size*
						(sizeof(lrnrecords[0].vector.index[0])+sizeof(lrnrecords[0].vector.sparsevect[0])),
						1,filein)){
					if (ferror(filein)){
						printf("%sLEARNFROM error: %s error reading input file %s.\n",ptr2,strerror(errno),learnfrom);
					} else {
						printf("%sLEARNFROM error: invalid %s input file.\n",ptr2,learnfrom);
					}
					for (jj=0;jj<numlrnrecs;jj++){
						MYFREE(lrnrecords[jj].vector.index);
					}
					learnfrom=NULL;
					break;
				}
				numlrnrecs++;
			}

			/* Close file. */
			fclose(filein);
		}
	}

	/* No learning file available. Use internal learning data. */
	if (NULL==learnfrom){

		/* Read and validate learning records. */
		for (numlrnrecs=jj=0;(numlrnrecs<MAXLEARNRECORDS)&&(jj<learn_len);){

			/* Get learning record header and sparse vector size. */
			memcpy(&lrnrecords[numlrnrecs].header,&learn[jj],sizeof(lrnrecords[0].header));
			jj+=sizeof(lrnrecords[0].header);
			memcpy(&lrnrecords[numlrnrecs].vector.size,&learn[jj],sizeof(lrnrecords[0].vector.size));
			jj+=sizeof(lrnrecords[0].vector.size);

			/* Validate learning record. */
			if (((0xffffffffffffff&lrnrecords[numlrnrecs].header)!=((ALGMASK|SELECTMASK|TERMORDRMASK|
					LITORDRMASK|FUNCWMASK|WEAKRWMASK|SPLITMASK)&lrnrecords[numlrnrecs].header))
					||((0xff00000000000000&lrnrecords[numlrnrecs].header)>pbmtypemask)){
				printf("%sLearn data error: invalid header in record.\n",ptr2);
				for (jj=0;jj<numlrnrecs;jj++){
					MYFREE(lrnrecords[jj].vector.index);
				}
				numlrnrecs=0;
				break;
			}

			/* Check problem type match. Problem types 3 and 8 match each other. */
			ii=lrnrecords[numlrnrecs].header>>56;
			if ((ii!=pbmtype)&&((pbmtype!=3)||(ii!=8))&&((pbmtype!=8)||(ii!=3))){
				jj+=lrnrecords[numlrnrecs].vector.size*(sizeof(lrnrecords[0].vector.index[0])
						+sizeof(lrnrecords[0].vector.sparsevect[0]));
				if (jj>learn_len){
					printf("%sLearn error: invalid learn data length.\n",ptr2);
					for (jj=0;jj<numlrnrecs;jj++){
						MYFREE(lrnrecords[jj].vector.index);
					}
					numlrnrecs=0;
					break;
				}
				continue;
			}

			/* Allocate memory for sparse vector data and read the data. */
			if (NULL==(ptr3=MYALLOC(lrnrecords[numlrnrecs].vector.size*
					(sizeof(lrnrecords[0].vector.index[0])+sizeof(lrnrecords[0].vector.sparsevect[0]))))){
				printf("%sLearn error: not enough memory.\n",ptr2);
				for (jj=0;jj<numlrnrecs;jj++){
					MYFREE(lrnrecords[jj].vector.index);
				}
				numlrnrecs=0;
				break;
			}
			lrnrecords[numlrnrecs].vector.index=ptr3;
			lrnrecords[numlrnrecs].vector.sparsevect=
					(double *)&lrnrecords[numlrnrecs].vector.index[lrnrecords[numlrnrecs].vector.size];
			kk=lrnrecords[numlrnrecs].vector.size*(sizeof(lrnrecords[0].vector.index[0])
					+sizeof(lrnrecords[0].vector.sparsevect[0]));
			if ((jj+kk)>learn_len){
				printf("%sLearn error: invalid learn data length.\n",ptr2);
				for (jj=0;jj<numlrnrecs;jj++){
					MYFREE(lrnrecords[jj].vector.index);
				}
				numlrnrecs=0;
				break;
			}
			memcpy(lrnrecords[numlrnrecs].vector.index,&learn[jj],kk);
			jj+=kk;
			numlrnrecs++;
		}
	}

	/* Reallocate lrnrecords to exact size and return. */
	if (numlrnrecs==0){
		MYFREE(lrnrecords);
		lrnrecords=NULL;
	} else {
		if (NULL!=(ptr1=MYREALLOC(lrnrecords,numlrnrecs*sizeof(learnrecord)))){
			lrnrecords=ptr1;
		}
	}
	return;
} /* ReadLearnRecords */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Compare function for qsort())  OCJ
 *
 *    This function compares to integers.
 *
 *
 *  ARGUMENTS:
 *
 *    val1: Pointer to first integer to compare.
 *    val2: Pointer to second integer to compare.
 *
 *  RETURNS:
 *
 *    Integer less than, equal to, or greater than zero
 *    corresponding to whether (int *)*val1 is considered
 *    less than, equal to, or greater than (int *)val2.
 *
 *
 *
 *--------------------------------------------------------------*/
int LrnCompare(const void *val1,const void *val2){
	return((*(int *)val1>*(int *)val2)-(*(int *)val1<*(int *)val2));
} /* LrnCompare */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check and increment fvidx set memory)  OCJ
 *
 *    This function checks and increments if necessary the memory
 *    pointed by fvidx global variable. The number of elements
 *    allocated to the fvidx set is in fvsize global variable.
 *    The number of elements used is in fvused global variable.
 *    If fvused is equal or greater than fvsize then the set
 *    is incremented by additional LEARNMEMORYCHUNK elements.
 *
 *    Once the memory is checked and incremented if necessary
 *    fvidx[fvused] is set to index and fvused is incremented.
 *
 *    The fvused and fvsize global variables are specific for
 *    this module learn.c.
 *
 *
 *  ARGUMENTS:
 *
 *    val1: Pointer to first integer to compare.
 *    val2: Pointer to second integer to compare.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *
 *
 *--------------------------------------------------------------*/
int32_t CheckGCVMemory(int32_t index){
	auto int32_t *ptr1;                                  /* Auxiliary pointer */
	if (fvused>=fvsize){
		if (NULL==(ptr1=MYREALLOC(fvidx,(fvsize+LEARNMEMORYCHUNK)*sizeof(fvidx[0])))){
			return(NOMEMORY);
		}
		fvidx=ptr1;
		fvsize+=LEARNMEMORYCHUNK;
	}
	fvidx[fvused]=index;
	fvused++;
	return(0);
} /* CheckGCVMemory */
