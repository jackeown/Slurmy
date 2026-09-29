/*
 ============================================================================
 Name        : indexing.c
 Author      : Oscar Contreras
 Version     :
 Copyright   : Copyright (C) 2015-2016 Oscar Contreras. All rights reserved.
 Description : First order logic theorem prover in C
 ============================================================================
 */

/****************************************************************
*
*             indexing (term indexing functions module)
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
 *    This source contains the hashing and term indexing functions
 *    of the Drodi package.
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
  *  DESCRIPTION: (Add locally defined symbols to global symbol list)  OCJ
  *
  *    This function adds locally defined symbols to global symbol list.
  *
  *
  *  ARGUMENTS:
  *
  *    names: structure with local symbol and variable list.
  *
  *  RETURNS:
  *
  *    0 if OK or NOMEMORY.
  *
  *--------------------------------------------------------------*/
int32_t AddGlobalSymbols(nameschain *names){
	auto char *auxchar1;                            /* Auxiliary pointers */
 	auto int32_t ii;                                /* Auxiliary */

 	/* Return if no names buffer allocated. */
 	if (names->name==NULL){
 		return(0);
 	}

 	/* Loop for each symbol. */
 	for (;names!=NULL;names=names->peer){

 		/* Check collision with skolem prefixes. */
 		if (0==strncmp(names->name,SKLPREFIX,strlen(SKLPREFIX))){
 			ii=strtol(&names->name[strlen(SKLPREFIX)],&auxchar1,10);
 			if ((ii>=skolemid)&&(auxchar1>&names->name[strlen(SKLPREFIX)])){
 				skolemid=ii+1;
 			}
 		}

 		/* Check collision with predicate definitions prefix. */
 		if (0==strncmp(names->name,PRDPREFIX,strlen(PRDPREFIX))){
 			ii=strtol(&names->name[strlen(PRDPREFIX)],&auxchar1,10);
 			if ((ii>=pdefid)&&(auxchar1>&names->name[strlen(PRDPREFIX)])){
 				pdefid=ii+1;
 			}
 		}

 		/* Check collision with P-predicate prefix. */
 		if (0==strncmp(names->name,SPLPREFIX,strlen(SPLPREFIX))){
 			ii=strtol(&names->name[strlen(SPLPREFIX)],&auxchar1,10);
 			if ((ii>=splid)&&(auxchar1>&names->name[strlen(SPLPREFIX)])){
 				splid=ii+1;
 			}
 		}

 		/* Check collision with function definitions prefix. */
 		if (0==strncmp(names->name,FNCPREFIX,strlen(FNCPREFIX))){
 			ii=strtol(&names->name[strlen(FNCPREFIX)],&auxchar1,10);
 			if ((ii>=functid)&&(auxchar1>&names->name[strlen(FNCPREFIX)])){
 				functid=ii+1;
 			}
 		}

 		/* If name is a predicate or function symbol then */
 		/* create a symbol entry. */
 		ii=(PREDICATE|FUNCTION)&names->flags;
 		if (ii){
 			if (NULL==CreateGlobalSymbol(names->name,names->data.arity,ii)){
 				return(NOMEMORY);
 			}
 		}
 	}

 	return(0);
} /* AddGlobalSymbols */

/*--------------------------------------------------------------
  *
  *  DESCRIPTION: (Create global symbol entry)  OCJ
  *
  *    This function creates a global symbol entry in the kb_hash chain.
  *
  *
  *  ARGUMENTS:
  *
  *    name: Pointer to symbol name.
  *    arity: Number of arguments of symbol.
  *    type: Type of symbol. Allowed values are:
  *          FUNCTION: The symbol is a function.
  *          PREDICATE: The symbol is a predicate.
  *          Only one of the flags FUNCTION or PREDICATE can be set.
  *          DEFINEDPRED: The symbol is a defined predicate. This flag
  *          can be set only if the PREDICATE flag is set.
  *          SKOLEM: The symbol is a skolem function. This flag can be
  *          set only if the FUNCTION flag is set.
  *
  *  RETURNS:
  *
  *    Pointer to hashchain symbol structure or NULL if NOMEMORY.
  *
  *--------------------------------------------------------------*/
hashchain *CreateGlobalSymbol(char *name,int32_t arity,int32_t type){
	auto hashchain *ptr2;                           /* Auxiliary pointer */
 	auto int32_t hh;                                /* Auxiliary */

	/* Allocate memory for hash symbol element. */
	if (NULL==(ptr2=MYALLOC(strlen(name)+sizeof(hashchain)+1))){
		return(NULL);
	}

	/* Get hash value. Store it in hashchain structure for */
	/* future use in calculating clause components hashes */
	/* when generating P-predicate names. */
	#ifdef MURMUR3
	hh=ptr2->hash=Murmur3(name);
	#else
	hh=ptr2->hash=KRMultHash(name);
	#endif
	hh&=0xffff;

	/* Chain the element to global queue */
	/* and chain the element hash queue.*/
	ptr2->nextelem=kb_hash;
	kb_hash=ptr2;
	ptr2->nextsymbol=hashkeys[hh];
	hashkeys[hh]=ptr2;

	/* Set remaining element fields. */
	ptr2->arity=arity;
	ptr2->type=type;
	if (name[0]=='"'){
		ptr2->type|=DISTINCTOBJECT;
	}
	numsymbols++;
	if (type&PREDICATE){
		ptr2->prednumber=numpreds;
		numpreds++;
	}
	if (type&(SKOLEM|DEFINEDPRED)){
		ptr2->occurrence=ptr2->occurrencebk=numskolem+numpdefs;
	} else {
		ptr2->occurrence=ptr2->occurrencebk=numsymbols;
	}
	ptr2->usecount=0;
	ptr2->sinedist=0xffffffff;
	for (hh=0;hh<DSCTREETYPES;hh++){
		ptr2->discr[hh]=NULL;
		#ifdef DEBUGTREE
		ptr2->numleafs[hh]=0;
		#endif
	}
	#ifdef SEMANTICTAUTOLOGY
	ptr2->posdsctrttlg=ptr2->negdsctrttlg=NULL;
	#endif
	strcpy(((char *)ptr2)+sizeof(hashchain),name);

	/* Update maxarity. */
	if (arity>maxarity){
		maxarity=arity;
	}

 	return(ptr2);
} /* CreateGlobalSymbol */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Murmur3 hash function)  OCJ
 *
 *    This function implements the murmur3 hash function. This has been
 *    taken from Wikipedia murmur3 32 bits sample code.
 *
 *  ARGUMENTS:
 *
 *    key: Key to be hashed. It must be a null ended alphanumeric char string.
 *
 *  RETURNS:
 *
 *    Hashed value.
 *
 *--------------------------------------------------------------*/
#ifdef MURMUR3
uint32_t Murmur3(char *key){
	static const uint32_t c1=0xcc9e2d51;
	static const uint32_t c2=0x1b873593;
	static const uint32_t r1=15;
	static const uint32_t r2=13;
	static const uint32_t mm=5;
	static const uint32_t nn=0xe6546b64;
	static const uint32_t seed=0x9747b28c;
	auto uint32_t len;                                /* Key length */
	auto int32_t nblocks;                             /* Number of 4 bytes blocks in key */
	auto uint32_t *blocks;                            /* Pointer to blocks in key */
	auto uint32_t hash;                               /* 32 bit hash value */
	auto unsigned char *tail;                         /* Pointer to remaining byte in key */
	auto int32_t ii;                                  /* Auxiliary */
	auto uint32_t kk,k1;                              /* Auxiliary */

	/* Initialize and modify hash value for each whole chunk of key. */
	hash=seed;
	len=strlen((char *)key);
	nblocks=len/4;
	blocks=(uint32_t *)key;
	for (ii=0;ii<nblocks;ii++) {
		kk=blocks[ii];
		kk*=c1;
		kk=ROT32(kk,r1);
		kk*=c2;
		hash^=kk;
		hash=ROT32(hash,r2)*mm+nn;
	}

	/* Modify hash value for remaining bytes in key. */
	tail=(unsigned char *)(key+nblocks*4);
	k1=0;
	switch (len&3){
		case 3:
			k1^=tail[2]<<16;
			/* no break */
		case 2:
			k1^=tail[1]<<8;
			/* no break */
		case 1:
			k1^=tail[0];
			k1*=c1;
			k1=ROT32(k1,r1);
			k1*=c2;
			hash^=k1;
	}

	/* Final hash value modifications. */
	hash^=len;
	hash^=(hash>>16);
	hash*=0x85ebca6b;
	hash^=(hash >>13);
	hash*=0xc2b2ae35;
	hash^=(hash>>16);

	return(hash);
} /* Murmur3 */

#else
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Kernighan and Ritchie hash function)  OCJ
 *
 *    This function implements the Kernighan and Ritchie modified
 *    hash function.
 *
 *  ARGUMENTS:
 *
 *    key: Key to be hashed. It must be a null ended alphanumeric
 *         char string.
 *
 *  RETURNS:
 *
 *    Hashed value.
 *
 *--------------------------------------------------------------*/
uint32_t KRMultHash(char *key){
	auto uint32_t hash;                               /* 32 bit hash value */
	auto uint32_t len;                                /* Key length */
	auto int32_t ii;                                  /* Auxiliary */
	hash=0;
	len=strlen((char *)key);
	for(ii=0;ii<len;++ii){
		hash=(hash<<5)-hash+key[ii]-48;
	}
	return(hash);
} /* KRMultHash */
#endif

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if name is globally declared)  OCJ
 *
 *    This function checks if a name is declared.
 *
 *
 *  ARGUMENTS:
 *
 *    name: Symbol name to check.
 *
 *  RETURNS:
 *
 *    Pointer to hashchain structure of the symbol
 *    or NULL if symbol is not globally declared.
 *
 *--------------------------------------------------------------*/
hashchain *CheckGlblDeclared(char *name){
	auto hashchain *ptr1;                           /* Auxiliary pointer */
	auto int32_t hh;                                /* Auxiliary */

	/* Get hash value. */
	#ifdef MURMUR3
	hh=Murmur3(name);
	#else
	hh=KRMultHash(name);
	#endif

	/* Loop through hashchain elements. */
	for (ptr1=hashkeys[hh&0xffff];ptr1!=NULL;ptr1=ptr1->nextsymbol){
		if (0==strcmp(((char *)ptr1)+sizeof(hashchain),name)){
			return(ptr1);
		}
	}

	return(NULL);
} /* CheckGlblDeclared */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Insert discrimination trees for clause)  OCJ
 *
 *    This function performs the discrimination term insertions needed
 *    for literals and terms in a clause. All items added to discrimination
 *    trees must belong to the working KB.
 *
 *    This function must be called only for PASSIVE or ACTIVE clauses
 *    in the working KB but this is not verified.
 *
 *    The description of the discr[] field indices in the haschain
 *    structure are as follows:
 *    0: Pointer to discrimination trees for active clauses. Used for
 *       selected positive literals and maximal root terms of positive
 *       selected equalities. Used by Resolution(), PM2Clause(),
 *       BckSubsuming(), FwSubsResltn() and BkSubsResltn() functions.
 *    1: Pointer to discrimination trees for active clauses. Used for
 *       selected negative literals, terms in selected literals that
 *       are not positive or negative equalities and terms in maximal
 *       equality root terms of positive or negative selected equalities.
 *       Used by PMFromClause(), BckSubsuming(), FwSubsResltn() and
 *       BkSubsResltn() functions.
 *    2: Pointer to discrimination trees for active clauses. Used for
 *       non selected positive literals and all terms. Used by
 *       CheckFwSubsuming(), BckSubsuming(), BckDemodulation(),
 *       FwSubsResltn() and BkSubsResltn() functions.
 *    3: Pointer to discrimination trees for active clauses. Used for
 *       non selected negative literals and maximal root terms of positive
 *       equalities that are suitable for performing demodulation from them.
 *       Used by CheckFwSubsuming(), BckSubsuming(), FwdDemodulation(),
 *       FwSubsResltn() and BkSubsResltn() functions.
 *    4: Pointer to discrimination trees for passive clauses. Used for all
 *       positive literals and maximal root terms of positive equalities
 *       that are suitable for performing demodulation from them. Used by
 *       CheckFwSubsuming(), BckSubsuming(), FwdDemodulation(), FwSubsResltn()
 *       and BkSubsResltn() functions.
 *    5: Pointer to discrimination trees for passive clauses. Used for all
 *       negative literals and all terms. Used by CheckFwSubsuming(),
 *       BckSubsuming(), BckDemodulation(), FwSubsResltn() and BkSubsResltn()
 *       functions.
 *    6: Used only if UNITCLRESOLUTION is defined in global.h file.
 *       Pointer to discrimination trees for active and passive unit clauses.
 *       Used for positive literals. Used by UCResolution() function.
 *    7. Used only if UNITCLRESOLUTION is defined in global.h file.
 *       Pointer to discrimination trees for active and passive unit clauses.
 *       Used for negative literals. Used by UCResolution() function.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *
 *  RETURNS:
 *
 *    0 if successful.
 *    NOMEMORY if not enough memory.
 *    SLICETMOUT if hardware instructions limit exceeded.
 *    TIMEOUT if a timeout condition has occurred.
 *    UNKNOWN if problem solved by another process.
 *
 *--------------------------------------------------------------*/
int32_t AddDiscrTrees(cmprefix *clause){
	auto uint8_t *ptr1,*ptr3,*ptr4,**ptr5;          /* Auxiliary pointers */
	auto symbol *ptr2;                              /* Auxiliary pointer */
	auto int32_t dd,ii,jj,kk,nn;                    /* Auxiliary */

	/* Check if clause is eligible for performing demodulation from it. */
	if (clause->part2.bin->literals!=1){
		dd=0;
	} else {
		dd=1;
	}

	#ifdef UNITCLRESOLUTION
	/* The clause has only one literal. Manage discr[6] and discr[7] trees. */
	/* This is done only for ACTIVE unit clauses without assertions. */
	if ((clause->flags&ACTIVE)&&(clause->part2.bin->literals==1)&&(clause->part2.bin->ovly.asserts==NULL)){

		/* Negative literal. */
		ptr1=&clause->part2.bin->formula[0];
		if (ptr1[0]&NEGATED){

			/* Literal is an equality. */
			if (ptr1[0]&EQUALITY){
				if (0!=(nn=DsTermIndexing(ptr1,&equkey.discr[7]))){
					return(nn);
				}

			/* Literal is a predicate. */
			} else {
				if (0!=(nn=DsTermIndexing(ptr1,&((symbol *)(ptr1+1))->symbol->discr[7]))){
					return(nn);
				}
			}

		/* Positive literal. */
		} else {

			/* Literal is an equality. */
			if (ptr1[0]&EQUALITY){
				if (0!=(nn=DsTermIndexing(ptr1,&equkey.discr[6]))){
					return(nn);
				}

			/* Literal is a predicate. */
			} else {
				if (0!=(nn=DsTermIndexing(ptr1,&((symbol *)(ptr1+1))->symbol->discr[6]))){
					return(nn);
				}
			}
		}
	}
	#endif

	/* Loop through clause items. */
	ii=jj=kk=0; /* Just to avoid compiler warnings. */
	ptr3=ptr4=NULL; /* Just to avoid compiler warnings. */
	for (ptr1=&clause->part2.bin->formula[0];(*ptr1)!=UNITEND;
			ptr1=NextItem(ptr1,IMMED)){

		/* Item is not a variable. */
		if (0==(VARIABLE&ptr1[0])){

			/* Get equality root terms data. */
			/* With this code the following settings are achieved: */
			/* ptr3 = First equality root term if ptr1 is a subterm of positive */
			/*        or negative equality, NULL otherwise. */
			/* ptr4 = Second equality root term if ptr1 is a subterm of positive */
			/*        or negative equality, NULL otherwise. */
			/* ii = 1 if ptr1 is a positive equality or one of its subterms, */
			/* ii = 0 if ptr1 is a negative equality or one of its subterms, */
			/*      undefined otherwise. */
			/* jj = 1 if ptr1 is a selected literal or one of its subterms, */
			/*      0 otherwise. */
			/* kk = 1 if ptr1 is a selected equality root term, */
			/*      0 if ptr1 is an unselected equality root term, */
			/*      undefined otherwise. */
			switch (ptr1[0]&(EQUALITY|PREDICATE|FUNCTION)){
				case EQUALITY:
					ptr3=NextItem(ptr1,IMMED);
					ptr4=NextItem(ptr3,OVERSUBTERMS);
					if (ptr1[0]&NEGATED){
						ii=0;
					} else {
						ii=1;
					}
					if (ptr1[0]&SELECTED){
						jj=1;
					} else {
						jj=0;
					}
					break;
				case PREDICATE:
					ptr3=ptr4=NULL;
					if (ptr1[0]&SELECTED){
						jj=1;
					} else {
						jj=0;
					}
					break;
				case FUNCTION:
					if ((ptr1==ptr3)||(ptr1==ptr4)){
						if (ptr1[0]&SELECTED){
							kk=1;
						} else {
							kk=0;
						}
					}
					break;
			}

			/* Active clause. */
			ptr2=(symbol *)(ptr1+1);
			if (clause->flags&ACTIVE){

				/* Process depending on kind of item. */
				switch (ptr1[0]&(EQUALITY|PREDICATE|NEGATED|FUNCTION)){

					/* Item is a negated predicate. */
					case (PREDICATE|NEGATED):
						if (ptr1[0]&SELECTED){
							if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[1]))){
								return(nn);
							}
						} else {
							if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[3]))){
								return(nn);
							}
						}
						break;

					/* Item is a negated equality. */
					case (EQUALITY|NEGATED):
						if (ptr1[0]&SELECTED){
							if (0!=(nn=DsTermIndexing(ptr1,&equkey.discr[1]))){
								return(nn);
							}
						} else {
							if (0!=(nn=DsTermIndexing(ptr1,&equkey.discr[3]))){
								return(nn);
							}
						}
						break;

					/* Item is a positive predicate. */
					case PREDICATE:
						if (ptr1[0]&SELECTED){
							if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[0]))){
								return(nn);
							}
						} else {
							if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[2]))){
								return(nn);
							}
						}
						break;

					/* Item is a positive equality. */
					case EQUALITY:

						/* Equality is selected. */
						if (ptr1[0]&SELECTED){

							/* Add equality to tree. */
							if (0!=(nn=DsTermIndexing(ptr1,&equkey.discr[0]))){
								return(nn);
							}

							/* If equality is selected for paramodulating from it and it has */
							/* a selected root term that is a variable then add equality */
							/* to vartrmidx field of kbase structure. */
							if ((ptr1[0]&PMSELECTED)&&(((VARIABLE|SELECTED)==((VARIABLE|SELECTED)&ptr3[0]))
									||((VARIABLE|SELECTED)==((VARIABLE|SELECTED)&ptr4[0])))){

								/* Allocate more space for pointers in leave node if needed. */
								if (kbset.vartrmidx.size==kbset.vartrmidx.used){
									if (NULL==(ptr5=MYREALLOC(kbset.vartrmidx.symbol,
											(TREEPTR_CHUNK_SIZE+kbset.vartrmidx.size)*sizeof(void *)))){
										return(NOMEMORY);
									}
									kbset.vartrmidx.size+=TREEPTR_CHUNK_SIZE;
									kbset.vartrmidx.symbol=ptr5;
								}

								/* Set a leave node pointer to item. */
								kbset.vartrmidx.symbol[kbset.vartrmidx.used]=ptr1;
								kbset.vartrmidx.used++;
							}

						/* Equality is not selected. */
						} else {
							if (0!=(nn=DsTermIndexing(ptr1,&equkey.discr[2]))){
								return(nn);
							}
						}
						break;

					/* Item is a function. */
					case FUNCTION:
						if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[2]))){
							return(nn);
						}
						if ((ptr1==ptr3)||(ptr1==ptr4)){
							if (ptr1[0]&SELECTED){
								if (ii){
									if (jj){
										if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[0]))){
											return(nn);
										}
										if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[1]))){
											return(nn);
										}
									}
									if (dd){
										if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[3]))){
											return(nn);
										}
									}
								} else if (jj){
									if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[1]))){
										return(nn);
									}
								}
							}
						} else if (ptr3!=NULL){
							if ((jj)&&(kk)){
								if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[1]))){
									return(nn);
								}
							}
						} else if (jj){
							if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[1]))){
								return(nn);
							}
						}
						break;
				}

			/* Passive clause. */
			} else {

				/* Process depending on kind of item. */
				switch (ptr1[0]&(EQUALITY|PREDICATE|NEGATED|FUNCTION)){

					/* Item is a negated predicate. */
					case (PREDICATE|NEGATED):
						if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[5]))){
							return(nn);
						}
						break;

					/* Item is a negated equality. */
					case (EQUALITY|NEGATED):
						if (0!=(nn=DsTermIndexing(ptr1,&equkey.discr[5]))){
							return(nn);
						}
						break;

					/* Item is a positive predicate. */
					case PREDICATE:
						if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[4]))){
							return(nn);
						}
						break;

					/* Item is a positive equality. */
					case EQUALITY:
						if (0!=(nn=DsTermIndexing(ptr1,&equkey.discr[4]))){
							return(nn);
						}
						break;

					/* Item is a function. */
					case FUNCTION:
						if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[5]))){
							return(nn);
						}
						if (((ptr1==ptr3)||(ptr1==ptr4))&&(ptr1[0]&SELECTED)
								&&(ii)&&(dd)){
							if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[4]))){
								return(nn);
							}
						}
						break;
				}
			}
		}
	}

	return(0);
} /* AddDiscrTrees */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Delete discrimination trees for clause)  OCJ
 *
 *    This function performs the discrimination term editing deletions
 *    needed for literals and terms in a clause. All items deleted
 *    from a discrimination trees must belong to the working KB.
 *
 *    This function must be called only for PASSIVE or ACTIVE clauses
 *    in the working KB but this is not verified.
 *
 *    See AddDiscrTrees() function global comments for a description
 *    of the discr[] field indices in the haschain structure.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *
 *  RETURNS:
 *
 *    0 if successful.
 *    NOMEMORY if not enough memory.
 *    TIMEOUT if a timeout condition has occurred.
 *    UNKNOWN if problem solved by another process.
 *
 *--------------------------------------------------------------*/
int32_t DelDiscrTrees(cmprefix *clause){
	auto uint8_t *ptr1,*ptr3,*ptr4;                 /* Auxiliary pointers */
	auto symbol *ptr2;                              /* Auxiliary pointer */
	auto int32_t dd,ii,jj,kk,mm;                    /* Auxiliary */

	/* Check if clause is eligible for performing demodulation from it. */
	if (clause->part2.bin->literals!=1){
		dd=0;
	} else {
		dd=1;
	}

	#ifdef UNITCLRESOLUTION
	/* The clause has only one literal. Manage discr[6] and discr[7] trees. */
	/* This is done only for ACTIVE unit clauses without assertions. */
	if ((clause->flags&ACTIVE)&&(clause->part2.bin->literals==1)&&(clause->part2.bin->ovly.asserts==NULL)){

		/* Negative literal. */
		ptr1=&clause->part2.bin->formula[0];
		if (ptr1[0]&NEGATED){

			/* Literal is an equality. */
			if (ptr1[0]&EQUALITY){
				DeleteBranch(ptr1,&equkey.discr[7]);

			/* Literal is a predicate. */
			} else {
				DeleteBranch(ptr1,&((symbol *)(ptr1+1))->symbol->discr[7]);
			}

		/* Positive literal. */
		} else {

			/* Literal is an equality. */
			if (ptr1[0]&EQUALITY){
				DeleteBranch(ptr1,&equkey.discr[6]);

			/* Literal is a predicate. */
			} else {
				DeleteBranch(ptr1,&((symbol *)(ptr1+1))->symbol->discr[6]);
			}
		}
	}
	#endif

	/* Loop through clause items. */
	ii=jj=kk=0; /* Just to avoid compiler warnings. */
	ptr3=ptr4=NULL; /* Just to avoid compiler warnings. */
	for (ptr1=&clause->part2.bin->formula[0];(*ptr1)!=UNITEND;
			ptr1=NextItem(ptr1,IMMED)){

		/* Item is not a variable. */
		if (0==(VARIABLE&ptr1[0])){

			/* Get equality root terms data. */
			/* With this code the following settings are achieved: */
			/* ptr3 = First equality root term if ptr1 is a subterm of positive */
			/*        or negative equality, NULL otherwise. */
			/* ptr4 = Second equality root term if ptr1 is a subterm of positive */
			/*        or negative equality, NULL otherwise. */
			/* ii = 1 if ptr1 is a positive equality or one of its subterms, */
			/* ii = 0 if ptr1 is a negative equality or one of its subterms, */
			/*      undefined otherwise. */
			/* jj = 1 if ptr1 is a selected literal or one of its subterms, */
			/*      0 otherwise. */
			/* kk = 1 if ptr1 is a selected equality root term, */
			/*      0 if ptr1 is an unselected equality root term, */
			/*      undefined otherwise. */
			switch (ptr1[0]&(EQUALITY|PREDICATE|FUNCTION)){
				case EQUALITY:
					ptr3=NextItem(ptr1,IMMED);
					ptr4=NextItem(ptr3,OVERSUBTERMS);
					if (ptr1[0]&NEGATED){
						ii=0;
					} else {
						ii=1;
					}
					if (ptr1[0]&SELECTED){
						jj=1;
					} else {
						jj=0;
					}
					break;
				case PREDICATE:
					ptr3=ptr4=NULL;
					if (ptr1[0]&SELECTED){
						jj=1;
					} else {
						jj=0;
					}
					break;
				case FUNCTION:
					if ((ptr1==ptr3)||(ptr1==ptr4)){
						if (ptr1[0]&SELECTED){
							kk=1;
						} else {
							kk=0;
						}
					}
					break;
			}

			/* Active clause. */
			ptr2=(symbol *)(ptr1+1);
			if (clause->flags&ACTIVE){

				/* Process depending on kind of item. */
				switch (ptr1[0]&(EQUALITY|PREDICATE|NEGATED|FUNCTION)){

					/* Item is a negated predicate. */
					case (PREDICATE|NEGATED):
						if (ptr1[0]&SELECTED){
							DeleteBranch(ptr1,&ptr2->symbol->discr[1]);
						} else {
							DeleteBranch(ptr1,&ptr2->symbol->discr[3]);
						}
						break;

					/* Item is a negated equality. */
					case (EQUALITY|NEGATED):
						if (ptr1[0]&SELECTED){
							DeleteBranch(ptr1,&equkey.discr[1]);
						} else {
							DeleteBranch(ptr1,&equkey.discr[3]);
						}
						break;

					/* Item is a positive predicate. */
					case PREDICATE:
						if (ptr1[0]&SELECTED){
							DeleteBranch(ptr1,&ptr2->symbol->discr[0]);
						} else {
							DeleteBranch(ptr1,&ptr2->symbol->discr[2]);
						}
						break;

					/* Item is a positive equality. */
					case EQUALITY:

						/* Equality is selected. */
						if (ptr1[0]&SELECTED){

							/* Delete equality to/from tree. */
							DeleteBranch(ptr1,&equkey.discr[0]);

							/* If equality is selected for paramodulating from it and it has */
							/* a selected root term that is a variable then delete equality */
							/* in vartrmidx field of kbase structure. */
							if ((ptr1[0]&PMSELECTED)&&(((VARIABLE|SELECTED)==((VARIABLE|SELECTED)&ptr3[0]))
									||((VARIABLE|SELECTED)==((VARIABLE|SELECTED)&ptr4[0])))){
								for (mm=0;mm<kbset.vartrmidx.used;mm++){
									if (ptr1==kbset.vartrmidx.symbol[mm]){
										kbset.vartrmidx.symbol[mm]=
												kbset.vartrmidx.symbol[kbset.vartrmidx.used-1];
										kbset.vartrmidx.used--;
										break;
									}
								}
							}

						/* Equality is not selected. */
						} else {
							DeleteBranch(ptr1,&equkey.discr[2]);
						}
						break;

					/* Item is a function. */
					case FUNCTION:
						DeleteBranch(ptr1,&ptr2->symbol->discr[2]);
						if ((ptr1==ptr3)||(ptr1==ptr4)){
							if (ptr1[0]&SELECTED){
								if (ii){
									if (jj){
										DeleteBranch(ptr1,&ptr2->symbol->discr[0]);
										DeleteBranch(ptr1,&ptr2->symbol->discr[1]);
									}
									if (dd){
										DeleteBranch(ptr1,&ptr2->symbol->discr[3]);
									}
								} else if (jj){
									DeleteBranch(ptr1,&ptr2->symbol->discr[1]);
								}
							}
						} else if (ptr3!=NULL){
							if ((jj)&&(kk)){
								DeleteBranch(ptr1,&ptr2->symbol->discr[1]);
							}
						} else if (jj){
							DeleteBranch(ptr1,&ptr2->symbol->discr[1]);
						}
						break;
				}

			/* Passive clause. */
			} else {

				/* Process depending on kind of item. */
				switch (ptr1[0]&(EQUALITY|PREDICATE|NEGATED|FUNCTION)){

					/* Item is a negated predicate. */
					case (PREDICATE|NEGATED):
						DeleteBranch(ptr1,&ptr2->symbol->discr[5]);
						break;

					/* Item is a negated equality. */
					case (EQUALITY|NEGATED):
						DeleteBranch(ptr1,&equkey.discr[5]);
						break;

					/* Item is a positive predicate. */
					case PREDICATE:
						DeleteBranch(ptr1,&ptr2->symbol->discr[4]);
						break;

					/* Item is a positive equality. */
					case EQUALITY:
						DeleteBranch(ptr1,&equkey.discr[4]);
						break;

					/* Item is a function. */
					case FUNCTION:
						DeleteBranch(ptr1,&ptr2->symbol->discr[5]);
						if (((ptr1==ptr3)||(ptr1==ptr4))&&(ptr1[0]&SELECTED)
								&&(ii)&&(dd)){
							DeleteBranch(ptr1,&ptr2->symbol->discr[4]);
						}
						break;
				}
			}
		}
	}

	return(0);
} /* DelDiscrTrees */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Edit unfailing completion discrimination trees for clause)  OCJ
 *
 *    This function is similar to AddDiscrTrees() and DelDiscrTrees()
 *    functions but for completion without failure strategies (algorithm
 *    set to UEQDSC or UEQOTT) and merging the add and delete options
 *    using the flag parameter.
 *
 *    The clause passed as parameter must be an (in)equality and have
 *    either the ACTIVE (for positive or negative equalities) or the
 *    GROUNDJOINABLE (only for positive equalities) flag set, but of
 *    course not both.
 *
 *    The description of the discr[] field indices in the haschain
 *    structure are as follows:
 *    0: Pointer to discrimination trees for positive active equalities
 *       and selected root terms in positive active equalities. Used by
 *       IsConnected(), GenerateCPTo(), FwdNormalize(), UEQResolution(),
 *       FwUEQSubsum(), BkUEQSubsum() and GJRewrite() functions.
 *    1: Pointer to discrimination trees for ground joinable equalities
 *       and selected root terms in positive passive equalities. Used by
 *       UEQResolution(), FwUEQSubsum(), BkUEQSubsum(), FwdNormalize(),
 *       and GJRewrite() functions.
 *    2: Pointer to discrimination trees for negative active equalities
 *       and selected (in)equality root terms and their sub-terms in
 *       positive and negative active clauses. Used by GenerateCPFrom(),
 *       BckUEQSimplify(), UEQResolution(), FwUEQSubsum() and BkUEQSubsum()
 *       functions.
 *    3: Pointer to discrimination trees for positive passive equalities
 *       and selected (in)equality root terms and their sub-terms in positive
 *       and negative passive clauses. Used by BckUEQSimplify(),
 *       UEQResolution(), FwUEQSubsum() and BkUEQSubsum() functions.
 *    4: Pointer to discrimination trees for negative passive equalities
 *       and non selected (in)equality root terms and their sub-terms in
 *       positive and negative active clauses. Used by BckUEQSimplify(),
 *       UEQResolution(), FwUEQSubsum() and BkUEQSubsum() functions.
 *    5: Pointer to discrimination trees for non selected (in)equality root
 *       terms and their sub-terms in positive and negative passive clauses.
 *       Used by BckUEQSimplify() function.
 *
 *    IMPORTANT: If this function is called with the DELETETREES flag it
 *    is not necessary to check the SLICETMOUT return code in the calling
 *    function.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to positive equality clause in binary form. It must
 *            have either the ACTIVE or the GROUNDJOINABLE flag set, but
 *            of course not both. The flags and positive equality conditions
 *            are supposed to be correct but they are not checked.
 *    flag: ADDTREES or DELETETREES
 *
 *  RETURNS:
 *
 *    0 if successful.
 *    NOMEMORY if not enough memory.
 *    SLICETMOUT if hardware instructions limit exceeded.
 *    TIMEOUT if a timeout condition has occurred.
 *    UNKNOWN if problem solved by another process.
 *
 *--------------------------------------------------------------*/
int32_t EditUEQTrees(cmprefix *clause,int32_t flag){
	auto uint8_t *ptr1,*ptr3,*ptr4;                 /* Auxiliary pointers */
	auto symbol *ptr2;                              /* Auxiliary pointer */
	auto int32_t ii,nn;                             /* Auxiliary */

	/* Process depending on the type of clause. */
	switch ((ACTIVE|PASSIVE|GROUNDJOINABLE)&clause->flags){

		/* Clause is ACTIVE. */
		case ACTIVE:

			/* Update mxtrfsize. */
			if (mxtrfsize<clause->part2.bin->size){
				mxtrfsize=clause->part2.bin->size;
			}

			/* Positive equality. */
			ptr1=&clause->part2.bin->formula[0];
			ptr2=(symbol *)(ptr1+1);
			if (0==(NEGATED&ptr1[0])){

				/* Process equality. */
				if (flag&ADDTREES){
					if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[0]))){
						return(nn);
					}
				} else {
					DeleteBranch(ptr1,&ptr2->symbol->discr[0]);
				}

				/* Check equality root terms. */
				for (ptr1=NextItem(ptr1,IMMED);ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS)){

					/* Iterate if term is a variable. */
					if (VARIABLE&ptr1[0]){
						continue;
					}

					/* Root term is a selected root term. */
					ptr2=(symbol *)(ptr1+1);
					if (SELECTED&ptr1[0]){
						if (flag&ADDTREES){
							if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[0]))){
								return(nn);
							}
						} else {
							DeleteBranch(ptr1,&ptr2->symbol->discr[0]);
						}
					}

					/* Select tree index for selected root terms and sub-terms. */
					if (SELECTED&ptr1[0]){
						ii=2;

					/* Select tree index for non selected root terms and sub-terms. */
					} else {
						ii=4;
					}

					/* Loop through items of root term. */
					for (ptr3=ptr1,ptr4=NextItem(ptr1,OVERSUBTERMS);ptr3<ptr4;ptr3=NextItem(ptr3,IMMED)){

						/* Iterate if term is a variable. */
						if (VARIABLE&ptr3[0]){
							continue;
						}

						/* Add/delete term to appropriate discrimination tree. */
						ptr2=(symbol *)(ptr3+1);
						if (flag&ADDTREES){
							if (0!=(nn=DsTermIndexing(ptr3,&ptr2->symbol->discr[ii]))){
								return(nn);
							}
						} else {
							DeleteBranch(ptr3,&ptr2->symbol->discr[ii]);
						}
					}
				}

			/* Negative equality. */
			} else {

				/* Process inequality. */
				if (flag&ADDTREES){
					if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[2]))){
						return(nn);
					}
				} else {
					DeleteBranch(ptr1,&ptr2->symbol->discr[2]);
				}

				/* Check equality root terms. */
				for (ptr1=NextItem(ptr1,IMMED);ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS)){

					/* Iterate if term is a variable. */
					if (VARIABLE&ptr1[0]){
						continue;
					}

					/* Select tree index for maximum or maximal root terms and sub-terms */
					/* of an oriented or non oriented equation. */
					if (SELECTED&ptr1[0]){
						ii=2;

					/* Select tree index for non selected root terms and sub-terms */
					/* of an oriented equation. */
					} else {
						ii=4;
					}

					/* Loop through items of root term. */
					for (ptr3=ptr1,ptr4=NextItem(ptr1,OVERSUBTERMS);ptr3<ptr4;ptr3=NextItem(ptr3,IMMED)){

						/* Iterate if term is a variable. */
						if (VARIABLE&ptr3[0]){
							continue;
						}

						/* Add/delete term to appropriate discrimination tree. */
						ptr2=(symbol *)(ptr3+1);
						if (flag&ADDTREES){
							if (0!=(nn=DsTermIndexing(ptr3,&ptr2->symbol->discr[ii]))){
								return(nn);
							}
						} else {
							DeleteBranch(ptr3,&ptr2->symbol->discr[ii]);
						}
					}
				}
			}
			break;

		/* Clause is PASSIVE. */
		case PASSIVE:

			/* Update mxtrfsize. */
			if (mxtrfsize<clause->part2.bin->size){
				mxtrfsize=clause->part2.bin->size;
			}

			/* Positive equality. */
			ptr1=&clause->part2.bin->formula[0];
			ptr2=(symbol *)(ptr1+1);
			if (0==(NEGATED&ptr1[0])){

				/* Process equality. */
				if (flag&ADDTREES){
					if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[3]))){
						return(nn);
					}
				} else {
					DeleteBranch(ptr1,&ptr2->symbol->discr[3]);
				}

				/* Check equality root terms. */
				for (ptr1=NextItem(ptr1,IMMED);ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS)){

					/* Iterate if term is a variable. */
					if (VARIABLE&ptr1[0]){
						continue;
					}

					/* Root term is a selected root term. */
					ptr2=(symbol *)(ptr1+1);
					if (SELECTED&ptr1[0]){
						if (flag&ADDTREES){
							if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[1]))){
								return(nn);
							}
						} else {
							DeleteBranch(ptr1,&ptr2->symbol->discr[1]);
						}
					}

					/* Select tree index for selected root terms and sub-terms. */
					if (SELECTED&ptr1[0]){
						ii=3;

					/* Select tree index for non selected root terms and sub-terms. */
					} else {
						ii=5;
					}

					/* Loop through items of root term. */
					for (ptr3=ptr1,ptr4=NextItem(ptr1,OVERSUBTERMS);ptr3<ptr4;ptr3=NextItem(ptr3,IMMED)){

						/* Iterate if term is a variable. */
						if (VARIABLE&ptr3[0]){
							continue;
						}

						/* Add/delete term to appropriate discrimination tree. */
						ptr2=(symbol *)(ptr3+1);
						if (flag&ADDTREES){
							if (0!=(nn=DsTermIndexing(ptr3,&ptr2->symbol->discr[ii]))){
								return(nn);
							}
						} else {
							DeleteBranch(ptr3,&ptr2->symbol->discr[ii]);
						}
					}
				}

			/* Negative equality. */
			} else {

				/* Process inequality. */
				if (flag&ADDTREES){
					if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[4]))){
						return(nn);
					}
				} else {
					DeleteBranch(ptr1,&ptr2->symbol->discr[4]);
				}

				/* Check equality root terms. */
				for (ptr1=NextItem(ptr1,IMMED);ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS)){

					/* Iterate if term is a variable. */
					if (VARIABLE&ptr1[0]){
						continue;
					}

					/* Select tree index for maximum or maximal root terms and sub-terms */
					/* of an oriented or non oriented equation. */
					if (SELECTED&ptr1[0]){
						ii=3;

					/* Select tree index for non selected root terms and sub-terms */
					/* of an oriented equation. */
					} else {
						ii=5;
					}

					/* Loop through items of root term. */
					for (ptr3=ptr1,ptr4=NextItem(ptr1,OVERSUBTERMS);ptr3<ptr4;ptr3=NextItem(ptr3,IMMED)){

						/* Iterate if term is a variable. */
						if (VARIABLE&ptr3[0]){
							continue;
						}

						/* Add/delete term to appropriate discrimination tree. */
						ptr2=(symbol *)(ptr3+1);
						if (flag&ADDTREES){
							if (0!=(nn=DsTermIndexing(ptr3,&ptr2->symbol->discr[ii]))){
								return(nn);
							}
						} else {
							DeleteBranch(ptr3,&ptr2->symbol->discr[ii]);
						}
					}
				}
			}
			break;

		/* Clause is GROUNDJOINABLE. Process equality */
		case GROUNDJOINABLE:
			ptr1=&clause->part2.bin->formula[0];
			ptr2=(symbol *)(ptr1+1);
			if (flag&ADDTREES){
				if (0!=(nn=DsTermIndexing(ptr1,&ptr2->symbol->discr[1]))){
					return(nn);
				}
			} else {
				DeleteBranch(ptr1,&ptr2->symbol->discr[1]);
			}
			break;
	}

	return(0);
} /* EditUEQTrees */

#ifdef SEMANTICTAUTOLOGY
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Edit semantic tautology detection discrimination trees)  OCJ
 *
 *    This function is similar to DelDiscrTrees() and AddDiscrTrees()
 *    functions but for semantic tautology detection and merging the
 *    add and delete options. The main difference is that all clauses
 *    are treated the same way with no distinctions of PASSIVE or ACTIVE
 *    clauses because that has no meaning with this type of single literal
 *    clauses. Finally all clauses are single literal positive or negative
 *    equalities.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to (in)equality clause in binary form.
 *    vleafset: Pointer to set of pointers to leaf nodes indexing
 *              the skolemized variables.
 *    flag: ADDTREES or DELETETREES
 *
 *  RETURNS:
 *
 *    0 if successful, NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t EditTtlgDscTrees(ttlgclause *clause,ttlgdstrleaf **vleafset,int32_t flag){
	auto uint8_t *ptr1;                             /* Auxiliary pointer */

	/* Loop through items in clause. */
	for (ptr1=&clause->formula[0];ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){

		/* Process depending on the type of item. */
		switch ((NEGATED|PREDICATE|EQUALITY|FUNCTION|VARIABLE)&ptr1[0]){
			case EQUALITY:
			case PREDICATE:
				if (flag&ADDTREES){
					if (0!=DsTtlgIndexing(clause,ptr1,&(((symbol *)&ptr1[1])->symbol->posdsctrttlg))){
						return(NOMEMORY);
					}
				} else {
					DeleteTtlgBranch(ptr1,&(((symbol *)&ptr1[1])->symbol->posdsctrttlg));
				}
				break;
			case (NEGATED|PREDICATE):
			case (NEGATED|EQUALITY):
				if (flag&ADDTREES){
					if (0!=DsTtlgIndexing(clause,ptr1,&(((symbol *)&ptr1[1])->symbol->negdsctrttlg))){
						return(NOMEMORY);
					}
				} else {
					DeleteTtlgBranch(ptr1,&(((symbol *)&ptr1[1])->symbol->negdsctrttlg));
				}
				break;
			case FUNCTION:
				if (flag&ADDTREES){
					if (0!=DsTtlgIndexing(clause,ptr1,&(((symbol *)&ptr1[1])->symbol->posdsctrttlg))){
						return(NOMEMORY);
					}
				} else {
					DeleteTtlgBranch(ptr1,&(((symbol *)&ptr1[1])->symbol->posdsctrttlg));
				}
				break;
			case VARIABLE:
				if (flag&ADDTREES){
					if (0!=DsTtlgIndexing(clause,ptr1,(void **)vleafset)){
						return(NOMEMORY);
					}
				} else {
					DeleteTtlgBranch(ptr1,(void **)vleafset);
				}
				break;
		}
	}

	return(0);
} /* EditTtlgDscTrees */
#endif

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add item to a discrimination tree)  OCJ
 *
 *    This function adds an item and all its subterms to a discrimination
 *    tree. The item may be literal or a function.
 *
 *    As usual for discrimination trees each literal or term is described
 *    by a full path from the top node of a discrimination tree down to a
 *    tree leave with a preorder traversal description of the literal or
 *    term, that is, a depth first, left to right inspection of all involved
 *    subterms.
 *
 *    The top node of each discrimination tree is the haschain structure
 *    for the corresponding literal or term. The corresponding discr[x] field
 *    in this structure points to the first child node. The tree is then
 *    made as a linked list of nodes (dstrnode structures) and leafs
 *    (dstrleaf structures). Every item addition to the tree makes use of
 *    existing paths if possible, adds new nodes if necessary and finally
 *    adds a leaf node.
 *
 *    Nodes that are children of the same parent node are forward and backward
 *    linked. Nodes that are a variable are linked at the end of the peer chain
 *    and kept linked in that position. Otherwise nodes that are variables are
 *    linked in the first position of the peer chain and kept there.
 *
 *    Nodes include the pndbrkts field to store the pending brackets to be
 *    closed after the node item not including the bracket opened just after
 *    the item if it exist. This allows to traverse the tree from the current
 *    node to the node of the first item that is not a subterm of the current
 *    node, which is the first node found with the same pndbrkts value. This
 *    is needed  when searching for unifications and instances (see QueryDscTree()
 *    function).
 *
 *    Tree backtracking is done because the node list is also backward chained
 *    and each node stores a pointer to the corresponding item in a clause.
 *    this pointer is used and modified by the query functions.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    item: This is the pointer to a clause item to be fully indexed in a tree.
 *          It cannot be a variable.
 *    topptr: Pointer to the discr[x] field of hashchain symbol structure
 *            corresponding to the discrimination tree where the item must be
 *            indexed.
 *
 *  RETURNS:
 *
 *    0 if item successfully added to the tree,
 *    NOMEMORY if not enough memory.
 *    SLICETMOUT if hardware instructions limit exceeded.
 *    TIMEOUT if a timeout condition has occurred.
 *    UNKNOWN if problem solved by another process.
 *
 *--------------------------------------------------------------*/
int32_t DsTermIndexing(uint8_t *item,void **topptr){
	auto uint8_t *subterm;                          /* Next item sub-term to be added to the tree */
	auto dstrleaf *leaf;                            /* Leaf for the indexed item */
	auto dstrnode *node;                            /* Item tree node */
	auto dstrnode *parent;                          /* Parent node */
	auto dstrnode **pfrstpeer;                      /* Address of pointer in the tree to first peer */
	auto dstrnode *frstpeer;                        /* Pointer to first peer */
	auto uint8_t *ptr1;                             /* Auxiliary pointer */
	auto dstrnode *ptr2,*ptr3;                      /* Auxiliary pointers */
	auto int32_t ii;                                /* Auxiliary */
	auto uint64_t hh;                               /* Auxiliary */

	/* Loop through item sub-terms. */
	parent=NULL;
	node=*(dstrnode **)topptr;
	ptr1=NextItem(item,OVERSUBTERMS);
	for (subterm=NextItem(item,IMMED),ii=0;subterm<ptr1;subterm=NextItem(subterm,IMMED)){

		/* Check hardware instructions limit, timeout */
		/*  and solution by other process. */
		ii++;
		if (ii>CHKCYCLES){
			ii=0;
			myread(&hh);
			if (hh>=instrlimit){
				return(SLICETMOUT);
			}
			switch (procctl->status){
				case TIMEOUT:
					return(TIMEOUT);
					break;
				case UNSATISFIABLE:
				case SATISFIABLE:
					return(UNKNOWN);
					break;
				case NOMEMORY:
					return(NOMEMORY);
					break;
			}
		}

		/* Look for a matching existing node. */
		if (node!=NULL){
			if (node->prevpeer->nodetype==VARIABLE){
				if (VARIABLE&subterm[0]){
					node=node->prevpeer;
				} else {
					for (;(node->nodetype!=VARIABLE)&&(node->symbol!=((symbol *)&subterm[1])->symbol);
							node=node->nxtpeer){
					}
					if ((node->nodetype==VARIABLE)){
						node=NULL;
					}
				}
			} else if (0==(VARIABLE&subterm[0])){
				for (;(node!=NULL)&&(node->symbol!=((symbol *)&subterm[1])->symbol);
						node=node->nxtpeer){
				}
			} else {
				node=NULL;
			}
		}

		/* There is not a matching existing node. */
		if (node==NULL){

			/* Allocate new node. */
			if (NULL==(node=MYALLOC(sizeof(dstrnode)))){
				return(NOMEMORY);
			}

			/* Initialize node nodetype, symbol and childnode fields. */
			if (VARIABLE&subterm[0]){
				node->nodetype=VARIABLE;
			} else {
				node->nodetype=0;
				node->symbol=((symbol *)&subterm[1])->symbol;
			}
			node->child.childnode=NULL;
			node->parent=parent;

			/* Set pndbrkts field and frstpeer and pfrstpeer variables. */
			if (parent==NULL){
				node->pndbrkts=0;
				pfrstpeer=(dstrnode **)topptr;
			} else {
				parent->childtype=NODECHILD;
				if ((parent->nodetype!=VARIABLE)&&(parent->symbol->arity!=0)){
					node->pndbrkts=parent->pndbrkts+1;
				} else {
					for (ptr2=parent,ptr3=NULL;ptr2!=NULL;ptr2=ptr2->parent){
						if (ptr2->item==subterm){
							ptr3=ptr2;
						}
					}
					if (ptr3!=NULL){
						node->pndbrkts=ptr3->pndbrkts;
					} else {
						node->pndbrkts=parent->pndbrkts;
					}
				}
				pfrstpeer=&(parent->child.childnode);
			}
			frstpeer=*pfrstpeer;

			/* Link node. */
			if (NULL==frstpeer){
				*pfrstpeer=(void *)node;
				node->nxtpeer=NULL;
				node->prevpeer=node;
			} else {
				if (node->nodetype!=VARIABLE){
					node->nxtpeer=frstpeer;
					node->prevpeer=frstpeer->prevpeer;
					frstpeer->prevpeer=node;
					*pfrstpeer=(void *)node;
				} else {
					node->nxtpeer=NULL;
					node->prevpeer=frstpeer->prevpeer;
					frstpeer->prevpeer->nxtpeer=node;
					frstpeer->prevpeer=node;
				}
			}
		}

		/* Temporarily store the pointer to the next clause term that is not */
		/* a sub-term of the sub-term in the node item field. */
		if ((node->nodetype!=VARIABLE)&&(node->symbol->arity>0)){
			node->item=NextItem(subterm,OVERSUBTERMS);
		} else {
			node->item=NULL;
		}

		/* Prepare next iteration. */
		parent=node;
		node=node->child.childnode;
	}

	/* Allocate and set the new leaf. */
	if (NULL==(leaf=MYALLOC(sizeof(dstrleaf)))){
		return(NOMEMORY);
	}
	leaf->item=item;
	leaf->parent=parent;
	#ifdef DEBUGTREE
	((symbol *)&item[1])->symbol->numleafs[(void **)topptr-&((symbol *)&item[1])->symbol->discr[0]]++;
	#endif

	/* Link leaf that is a child of the top of the tree. */
	if (parent==NULL){
		if ((*(void **)topptr)==NULL){
			leaf->peer=NULL;
			*(void **)topptr=(void *)leaf;
		} else {
			leaf->peer=*(dstrleaf **)topptr;
			*(void **)topptr=(void *)leaf;
		}

	/* Link leaf that is a child of another node. */
	} else {
		if (parent->child.childleaf==NULL){
			leaf->peer=NULL;
			parent->child.childleaf=leaf;
			parent->childtype=LEAFCHILD;
		} else {
			leaf->peer=parent->child.childleaf;
			parent->child.childleaf=leaf;
		}
	}
	return(0);
} /* DsTermIndexing */

#ifdef SEMANTICTAUTOLOGY
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add item to a semantic tautology discrimination tree)  OCJ
 *
 *    This function adds an item and all its subterms to a semantic
 *    tautology discrimination tree. The item may be literal, a function
 *    or a variable. It is similar to DsTermIndexing. The main difference
 *    is that it accepts terms that are variables treated as skolems.
 *
 *    Backtracking information is not set or used because it is not
 *    relevant for these discrimination trees as no successive calls for
 *    additional matches are performed.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to (in)equality clause containing item.
 *    item: This is the pointer to a semantic tautology clause item to be
 *          fully indexed in a tree.
 *    topptr: Address of pointer to top term of the discrimination tree
 *            where the item must be indexed.
 *
 *  RETURNS:
 *
 *    0 if item successfully added to the tree or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t DsTtlgIndexing(ttlgclause *clause,uint8_t *item,void **topptr){
	auto ttlgdstrleaf **vleafset;                   /* Pointer to set of leaf pointers for skolemized variables */
	auto uint8_t *subterm;                          /* Next item sub-term to be added to the tree */
	auto ttlgdstrleaf *leaf;                        /* Leaf for the indexed item */
	auto ttlgdstrnode *node;                        /* Item tree node */
	auto ttlgdstrnode *parent;                      /* Parent node */
	auto ttlgdstrnode **pfrstpeer;                  /* Address of pointer in the tree to first peer */
	auto ttlgdstrnode *frstpeer;                    /* Pointer to first peer */
	auto uint8_t *ptr1;                             /* Auxiliary pointer */
	auto int32_t ii;                                /* Auxiliary */

	/* item is a variable. */
	if (VARIABLE&item[0]){

		/* Allocate and set the new leaf. */
		if (NULL==(leaf=MYALLOC(sizeof(ttlgdstrleaf)))){
			return(NOMEMORY);
		}
		leaf->item=item;
		leaf->parent=NULL;
		leaf->formula=&clause->formula[0];

		/* Link new leaf to appropriate leaf chain. */
		ii=*(int16_t *)&item[1];
		vleafset=(ttlgdstrleaf **)topptr;
		leaf->peer=vleafset[ii];
		vleafset[ii]=leaf;
		return(0);
	}

	/* Item is not a variable. */
	/* Loop through item sub-terms. */
	parent=NULL;
	node=*(ttlgdstrnode **)topptr;
	ptr1=NextItem(item,OVERSUBTERMS);
	for (subterm=NextItem(item,IMMED);subterm<ptr1;subterm=NextItem(subterm,IMMED)){

		/* Look for a matching existing node. */
		if (node!=NULL){
			if (VARIABLE&subterm[0]){
				ii=*(int16_t *)&subterm[1];
				for (;(node!=NULL)&&((node->nodetype!=VARIABLE)||(node->itemid.varnb!=ii));
						node=node->nxtpeer){
				}
			} else {
				for (;(node!=NULL)&&((node->nodetype==VARIABLE)||(node->itemid.symbol!=((symbol *)&subterm[1])->symbol));
						node=node->nxtpeer){
				}
			}
		}

		/* There is not a matching existing node. */
		if (node==NULL){

			/* Allocate new node. */
			if (NULL==(node=MYALLOC(sizeof(ttlgdstrnode)))){
				return(NOMEMORY);
			}

			/* Initialize node nodetype, itemid and childnode fields. */
			if (VARIABLE&subterm[0]){
				node->nodetype=VARIABLE;
				node->itemid.varnb=*(int16_t *)&subterm[1];
			} else {
				node->nodetype=0;
				node->itemid.symbol=((symbol *)&subterm[1])->symbol;
			}
			node->child.childnode=NULL;
			node->parent=parent;

			/* Set frstpeer and pfrstpeer variables. */
			if (parent==NULL){
				pfrstpeer=(ttlgdstrnode **)topptr;
			} else {
				parent->childtype=NODECHILD;
				pfrstpeer=&(parent->child.childnode);
			}
			frstpeer=*pfrstpeer;

			/* Link node. */
			if (NULL==frstpeer){
				node->nxtpeer=node->prevpeer=NULL;
			} else {
				node->nxtpeer=frstpeer;
				node->prevpeer=NULL;
				frstpeer->prevpeer=node;
			}
			*pfrstpeer=(void *)node;
		}

		/* Prepare next iteration. */
		parent=node;
		node=node->child.childnode;
	}

	/* Allocate and set the new leaf. */
	if (NULL==(leaf=MYALLOC(sizeof(ttlgdstrleaf)))){
		return(NOMEMORY);
	}
	leaf->item=item;
	leaf->formula=&clause->formula[0];
	leaf->parent=parent;

	/* Link leaf that is a child of the top of the tree. */
	if (parent==NULL){
		leaf->peer=*(ttlgdstrleaf **)topptr;
		*(void **)topptr=(void *)leaf;

	/* Link leaf that is a child of another node. */
	} else {
		if (parent->child.childleaf==NULL){
			parent->childtype=LEAFCHILD;
		}
		leaf->peer=parent->child.childleaf;
		parent->child.childleaf=leaf;
	}
	return(0);
} /* DsTtlgIndexing */
#endif

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Delete item in a discrimination tree)  OCJ
 *
 *    This function deletes the discrimination tree data related to
 *    the specified item indexed in the specified tree.
 *
 *    It is assumed that the item is indexed in the tree. If this
 *    is not the case the function will crash.
 *
 *
 *  ARGUMENTS:
 *
 *    item: Pointer to a clause item indexed in a discrimination tree.
 *    topptr: Pointer to the discr[x] field of hashchain symbol
 *            structure corresponding to the discrimination tree
 *            where the item is indexed.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void DeleteBranch(uint8_t *item,void **topptr){
	auto dstrleaf *leaf;                            /* Pointer to leaf of indexed item */
	auto dstrleaf *prevleaf;                        /* Pointer to leaf preceding leaf of indexed item */
	auto dstrnode *node;                            /* Pointer to traversed tree node */
	auto hashchain *trmsymbol;                      /* Pointer to sub-term symbol */
	auto dstrnode *ptr3;                            /* Auxiliary node pointer for bactracking */
	auto uint8_t *ptr1,*ptr2;                       /* Auxiliary pointers */

	/* Return if tree is empty. */
	if (NULL==*topptr){
		return;
	}

	/* Item has no sub-terms. */
	if (((symbol *)&item[1])->symbol->arity==0){
		leaf=*(dstrleaf **)topptr;

	/* Item has sub-terms. Traverse the tree down to the leaf chain */
	/* where the item leaf is placed. */
	} else {
		for (ptr1=NextItem(item,IMMED),ptr2=NextItem(item,OVERSUBTERMS),node=*(dstrnode **)topptr;
				ptr1<ptr2;ptr1=NextItem(ptr1,IMMED),node=node->child.childnode){
			if (0==(VARIABLE&ptr1[0])){
				for (trmsymbol=((symbol *)&ptr1[1])->symbol;(node->symbol!=trmsymbol);node=node->nxtpeer){
				}
			} else {
				node=node->prevpeer;
			}
		}
		leaf=(dstrleaf *)node;
	}

	/* Scan leaf chain. */
	for (prevleaf=NULL;leaf->item!=item;leaf=(prevleaf=leaf)->peer){
	}

	/* Unlink and free item leaf. */
	node=leaf->parent;
	if (prevleaf==NULL){
		if (node==NULL){
			*(dstrleaf **)topptr=leaf->peer;
		} else {
			node->child.childleaf=leaf->peer;
		}
	} else {
		prevleaf->peer=leaf->peer;
	}
	MYFREE(leaf);
	#ifdef DEBUGTREE
	((symbol *)&item[1])->symbol->numleafs[topptr-&((symbol *)&item[1])->symbol->discr[0]]--;
	#endif

	/* Backtrack until a a node with one or more children is found. */
	/* Unlink and free nodes without children. */
	for (;(node!=NULL)&&(node->child.childnode==NULL);node=ptr3){
		ptr3=node->parent;
		if (node->nxtpeer!=NULL){
			node->nxtpeer->prevpeer=node->prevpeer;
		} else {
			if (ptr3==NULL){
				(*(dstrnode **)topptr)->prevpeer=node->prevpeer;
			} else {
				ptr3->child.childnode->prevpeer=node->prevpeer;
			}
		}
		if (ptr3==NULL){
			if (*(dstrnode **)topptr==node){
				*(dstrnode **)topptr=node->nxtpeer;
				if (node->nxtpeer!=NULL){
					node->nxtpeer->prevpeer=node->prevpeer;
				}
			} else {
				node->prevpeer->nxtpeer=node->nxtpeer;
			}
		} else {
			if (node==ptr3->child.childnode){
				ptr3->child.childnode=node->nxtpeer;
				if (node->nxtpeer!=NULL){
					node->nxtpeer->prevpeer=node->prevpeer;
				}
			} else {
				node->prevpeer->nxtpeer=node->nxtpeer;
			}
		}
		MYFREE(node);
	}

	return;
} /* DeleteBranch */

#ifdef SEMANTICTAUTOLOGY
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Query a semantic tautology discrimination tree)  OCJ
 *
 *    This function is similar to QueryDscTree() except that it allows
 *    deletions of items that are variables that conceptually represent
 *    skolems.
 *
 *
 *  ARGUMENTS:
 *
 *    item: Pointer to a fully indexed clause item for which a matching item
 *          will be searched.
 *    topptr: Address of pointer to first level node or leaf of tree to be used for
 *            search. This is only used if the queried item is a function
 *            or (in)equality.
 *
 *  RETURNS:
 *
 *    1 if a valid match was found. The match found is deleted.
 *    0 if no valid match was found. No tree entry is deleted.
 *
 *--------------------------------------------------------------*/
void DeleteTtlgBranch(uint8_t *item,void **topptr){
	auto ttlgdstrleaf **vleafset;                   /* Pointer to set of leaf pointers for skolemized variables */
	auto ttlgdstrleaf *leaf,*leaf2;                 /* Pointers to leafs */
	auto ttlgdstrnode *node,*node2;                 /* Pointers to tree nodes */
	auto uint8_t *ptr1,*ptr2;                       /* Auxiliary pointers */
	auto int32_t ii;                                /* Auxiliary */

	/* The item is a variable. */
	if (VARIABLE&item[0]){
		vleafset=(ttlgdstrleaf **)topptr;
		ii=*(int16_t *)&item[1];
		for (leaf=vleafset[ii],leaf2=NULL;(leaf!=NULL)&&(item!=leaf->item);leaf=leaf->peer){
			leaf2=leaf;
		}
		if (leaf!=NULL){
			if (leaf2==NULL){
				vleafset[ii]=leaf->peer;
			} else {
				leaf2->peer=leaf->peer;
			}
			MYFREE(leaf);
		}
		return;
	}

	/* The item is not a variable. */
	/* The tree has only leafs. */
	if (((symbol *)&item[1])->symbol->arity==0){
		for (leaf=*((ttlgdstrleaf **)topptr),leaf2=NULL;(leaf!=NULL)&&(item!=leaf->item);leaf=leaf->peer){
			leaf2=leaf;
		}
		if (leaf!=NULL){
			if (leaf2==NULL){
				*topptr=leaf->peer;
			} else {
				leaf2->peer=leaf->peer;
			}
			MYFREE(leaf);
		}
		return;
	}

	/* The tree has nodes. */
	/* Loop through item sub-terms. */
	node=*((ttlgdstrnode **)topptr);
	for (ptr1=NextItem(item,IMMED),ptr2=NextItem(item,OVERSUBTERMS);ptr1<ptr2;
			ptr1=NextItem(ptr1,IMMED),node=node->child.childnode){

		/* scan current node for a match. */
		if (VARIABLE&ptr1[0]){
			ii=*(int16_t *)&ptr1[1];
			for (;(node!=NULL)&&((node->nodetype!=VARIABLE)||(node->itemid.varnb!=ii));
					node=node->nxtpeer){
			}
		} else {
			for (;(node!=NULL)&&((node->nodetype==VARIABLE)||(node->itemid.symbol!=((symbol *)&ptr1[1])->symbol));
					node=node->nxtpeer){
			}
		}

		/* No match found. */
		if (node==NULL){
			return;
		}
	}

	/* We have a match. Find leaf and free it. */
	for (leaf=(ttlgdstrleaf *)node,leaf2=NULL;(leaf!=NULL)&&(item!=leaf->item);leaf=leaf->peer){
		leaf2=leaf;
	}
	if (leaf!=NULL){
		node=leaf->parent;
		if (leaf==node->child.childleaf){
			node->child.childleaf=leaf->peer;
		} else {
			leaf2->peer=leaf->peer;
		}
		MYFREE(leaf);
	} else {
		return;
	}

	/* Free parent nodes without children. */
	for (;(node!=NULL)&&(node->child.childnode==NULL);node=node2){
		node2=node->parent;
		if (node->nxtpeer!=NULL){
			node->nxtpeer->prevpeer=node->prevpeer;
		}
		if (node->prevpeer!=NULL){
			node->prevpeer->nxtpeer=node->nxtpeer;
		}
		if (node2!=NULL){
			if (node2->child.childnode==node){
				node2->child.childnode=node->nxtpeer;
			}
		} else {
			if (node==*((ttlgdstrnode **)topptr)){
				(*((ttlgdstrnode **)topptr))=node->nxtpeer;
				if (node->nxtpeer!=NULL){
					MYFREE(node);
					return;
				}
			}
		}
		MYFREE(node);
	}

	return;
} /* DeleteTtlgBranch */
#endif

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Free discrimination tree)  OCJ
 *
 *    This function frees the memory allocated for a discrimination tree.
 *
 *
 *  ARGUMENTS:
 *
 *
 *    topptr: Address of the discr[x] field of the discrimination tree
 *            to be freed.
 *    arity: Arity of the symbol owning the discrimination tree.
 *
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void FreeDiscrBranch(void *topptr,int32_t arity){
	auto dstrnode *node;                            /* Tree node */
	auto dstrleaf *childleaf;                       /* Pointer to child leaf if node is a leaf parent */
	auto dstrleaf *ptr1;                            /* Auxiliary pointer */
	auto dstrnode *ptr2;                            /* Auxiliary pointer */

	/* Tree has only leafs. Free leafs and set to NULL the top of the tree. */
	if (arity==0){
		for (childleaf=*(dstrleaf **)topptr;childleaf!=NULL;childleaf=ptr1){
			ptr1=childleaf->peer;
			MYFREE(childleaf);
		}
		*(void **)topptr=NULL;

	/* Tree has nodes. */
	} else {

		/* Main loop through the tree nodes. */
		node=*(dstrnode **)topptr;
		while (node!=NULL){

			/* The node has children. This is necessary because DsTermIndexing() */
			/* may have been interrupted by the hardware instructions limit check */
			/* leaving the tree addition incomplete. This is not a problem */
			/* for the code but Valgrind reports uninitialised value(s). */
			if (node->child.childnode!=NULL){

				/* Node children are nodes. Prepare next iteration and iterate. */
				if (node->childtype==NODECHILD){
					if (NULL!=node->child.childnode){
						node=node->child.childnode;
						continue;
					}

				/* Node children are leafs, free them. */
				} else  {
					for (childleaf=node->child.childleaf;childleaf!=NULL;childleaf=ptr1){
						ptr1=childleaf->peer;
						MYFREE(childleaf);
					}
				}
			}

			/* If we are here then node had no children or children were leafs */
			/* and have been freed. Prepare next iteration. */
			for (;node!=NULL;node=ptr2){
				if (node->nxtpeer==NULL){
					ptr2=node->parent;
					MYFREE(node);
				} else {
					ptr2=node;
					node=node->nxtpeer;
					MYFREE(ptr2);
					break;
				}
			}
		}

		/* Set to NULL the top of the tree. */
		*(void **)topptr=NULL;
	}

	return;
} /* FreeDiscrBranch */

#ifdef SEMANTICTAUTOLOGY
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Free semantic tautology discrimination tree)  OCJ
 *
 *    This function is similar to FreeDiscrBranch() function but
 *    for semantic tautology discrimination trees.
 *
 *
 *  ARGUMENTS:
 *
 *
 *    topptr: Address of the top pointer of the discrimination tree
 *            to be freed.
 *    arity: Arity of the symbol owning the discrimination tree.
 *
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void FreeTtlgDiscrBranch(void *topptr,int32_t arity){
	auto ttlgdstrnode *node;                        /* Tree node */
	auto ttlgdstrleaf *childleaf;                   /* Pointer to child leaf if node is a leaf parent */
	auto ttlgdstrleaf *ptr1;                        /* Auxiliary pointer */
	auto ttlgdstrnode *ptr2;                        /* Auxiliary pointer */

	/* Tree has only leafs. Free leafs and set to NULL the top of the tree. */
	if (arity==0){
		for (childleaf=*(ttlgdstrleaf **)topptr;childleaf!=NULL;childleaf=ptr1){
			ptr1=childleaf->peer;
			MYFREE(childleaf);
		}
		*(void **)topptr=NULL;

	/* Tree has nodes. */
	} else {

		/* Main loop through the tree nodes. */
		node=*(ttlgdstrnode **)topptr;
		while (node!=NULL){

			/* Node children are nodes. Prepare next iteration and iterate. */
			if (node->childtype==NODECHILD){
				if (NULL!=node->child.childnode){
					node=node->child.childnode;
					continue;
				}

			/* Node children are leafs, free them. */
			} else {
				for (childleaf=node->child.childleaf;childleaf!=NULL;childleaf=ptr1){
					ptr1=childleaf->peer;
					MYFREE(childleaf);
				}
			}

			/* If we are here then node had no children or children were leafs */
			/* and have been freed. Prepare next iteration. */
			for (;node!=NULL;node=ptr2){
				if (node->nxtpeer==NULL){
					ptr2=node->parent;
					MYFREE(node);
				} else {
					ptr2=node;
					node=node->nxtpeer;
					MYFREE(ptr2);
					break;
				}
			}
		}

		/* Set to NULL the top of the tree. */
		*(void **)topptr=NULL;
	}

	return;
} /* FreeTtlgDiscrBranch */
#endif

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Discrimination tree instance Query)  OCJ
 *
 *    This function gets the next candidate item that is an instance
 *    candidate of a given item in a given clause by searching a
 *    specified discrimination tree.
 *
 *    VERY IMPORTANT: This function is not multi-thread safe because:
 *    - the "item" field of dstrnode structure is used for bactracking
 *      when multiple calls are executed to find multiple matches for
 *      the same clause item.
 *    - The static variable lstmatch is used to store the last match
 *      found by the last call.
 *
 *
 *  ARGUMENTS:
 *
 *    item: Pointer to a fully indexed clause item for which a matching item
 *          will be searched. This cannot be a pointer to a variable, but
 *          this is not checked.
 *    pcand: This is the address of pointer to candidate item match. Not used
 *           if no candidate is found.
 *    topnode: Pointer to first level node or leaf of tree to be used for search.
 *             This must be the value of the discr[x] field in the item hashchain
 *             structure.
 *    flag: 0 if an initial search must be performed.
 *          1 if a search for the next match must be performed. If the previous
 *          search was not successful then the search will start as if flag is 0.
 *
 *  RETURNS:
 *
 *    1 if a valid candidate was found.
 *    0 if no valid candidate was found.
 *
 *--------------------------------------------------------------*/
int32_t QueryInsDscTree(uint8_t *item,uint8_t **pcand,void *topnode,int32_t flag){
	auto int32_t status;                            /* 0 if traversing, 1 if backtracking */
	static dstrleaf *lstmatch=NULL;                 /* Last match found in a previous call */
	auto dstrnode *node;                            /* Pointer to current tree node */
	auto uint8_t *term;                             /* Current term in item */
	auto dstrnode *ptr1;                            /* Auxiliary pointer */
	auto int32_t ii;                                /* Auxiliary */

	/* This is a search for next match. */
	node=NULL; /* Just to avoid compiler warnings. */
	if (flag==1){

		/* Last search was successful. */
		if (lstmatch!=NULL){

			/* If there is another leaf available then return full match. */
			if (lstmatch->peer!=NULL){
				lstmatch=lstmatch->peer;
				*pcand=lstmatch->item;
				return(1);
			}

			/* Set search status to backtracking mode. */
			status=1;
			node=lstmatch->parent;

		/* Last search was not successful. Set flag to initial search. */
		} else {
			flag=0;
		}
	}

	/* This is an initial search. */
	if (flag==0){

		/* If tree is empty then return failure. */
		if (topnode==NULL){
			lstmatch=NULL;
			return(0);
		}

		/* The tree has only leafs. Return full match. */
		if (((symbol *)&item[1])->symbol->arity==0){
			lstmatch=(dstrleaf *)topnode;
			*pcand=lstmatch->item;
			return(1);

		/* The tree has nodes. Set search status to traversing mode. */
		} else {
			status=0;
			node=(dstrnode *)topnode;
			term=NextItem(item,IMMED);
		}
	}

	/* Search loop. */
	while (1){

		/* Backtracking status. Now node is the last matching node found. */
		if (status){

			/* Loop through the last matching node available at each iteration. */
			/* Get matching clause term and try to find a new matching node. */
			for (ii=0;(ii==0)&&(node!=NULL);){
				term=node->item;

				/* Node item is not a sub-term of a node matching a clause variable. */
				if (term!=NULL){

					/* Matching clause term is a variable and node has peers so this */
					/* is not a generalization. Any peer node is a valid match. Prepare */
					/* next traversing. */
					if ((VARIABLE&term[0])&&(node->nxtpeer!=NULL)){
						node=node->nxtpeer;
						ii=1;
						status=0;
					}

				/* Node item is a sub-term of a node matching a clause variable. */
				/* If it has peers then follow line to next node that is not a */
				/* sub-term and prepare traversing. */
				} else if (node->nxtpeer!=NULL){

					/* Backtrack to the node matching the clause variable */
					/* and store it in ptr1. */
					for (ptr1=node->parent;ptr1->item==NULL;ptr1=ptr1->parent){
					}

					/* Traverse from node->nxtpeer to the first node that is not a sub-term */
					/* of the ptr1 item or the first node that has leaf children. Traversed */
					/* nodes item field are set to NULL to indicate that are sub-terms of a */
					/* node matching a clause variable. */
					for (node=node->nxtpeer;
							(node->childtype==NODECHILD)&&(node->pndbrkts>ptr1->pndbrkts);
							node=node->child.childnode){
						node->item=NULL;
					}

					/* If node children are leafs and node is a sub-term of the ptr1 item */
					/* then return full match. The node item field is set to NULL as before. */
					if ((node->childtype==LEAFCHILD)&&(node->pndbrkts>ptr1->pndbrkts)){
						node->item=NULL;
						lstmatch=node->child.childleaf;
						*pcand=lstmatch->item;
						return(1);
					}

					/* Prepare traversing. */
					term=NextItem(ptr1->item,OVERSUBTERMS);
					ii=1;
					status=0;
				}

				/* Prepare next backtracking iteration. */
				if (ii==0){
					node=node->parent;
				}
			}

			/* If no candidate for traversing was found then leave the loop. */
			if (ii==0){
				break;
			}

		/* Traversing status. Now node is the first node to be checked for a match and */
		/* term points to the clause term for which a match is being searched. */
		} else {

			/* Clause term is a variable. The node is a valid match. */
			if (VARIABLE&term[0]){

				/* Set node item. */
				node->item=term;

				/* Traverse until the first node that is not a sub-term of the */
				/* matching node or the first node that has leaf children. Traversed */
				/* nodes item field are set to NULL to indicate that are sub-terms of a */
				/* node matching a clause variable. */
				ptr1=node;
				if (node->childtype==NODECHILD){
					for (node=node->child.childnode;
							(node->childtype==NODECHILD)&&(node->pndbrkts>ptr1->pndbrkts);
							node=node->child.childnode){
						node->item=NULL;
					}
				}

				/* If node children are leafs and node is ptr1 or a sub-term of the ptr1 */
				/* item then return full match. If node is a sub-term of ptr1 then the */
				/* node item field is set to NULL to indicate that it is sub-terms of a */
				/* node matching a clause variable. */
				if ((node->childtype==LEAFCHILD)&&((node==ptr1)||(node->pndbrkts>ptr1->pndbrkts))){
					if (node!=ptr1){
						node->item=NULL; /* node is a sub-term of ptr1 */
					}
					lstmatch=node->child.childleaf;
					*pcand=lstmatch->item;
					return(1);
				}

				/* Prepare next traversing. */
				term=NextItem(term,OVERSUBTERMS);

			/* Clause term is not a variable. */
			} else {

				/* Node is a variable. */
				if (node->nodetype==VARIABLE){

					/* This is an instantiation. As node is a variable there are */
					/* no more peers so matching is not possible. Prepare next */
					/* iteration as a backtrack. */
					status=1;
					node=node->parent;

				/* Node is not a variable. */
				} else {

					/* Scan node and its peers for a match. */
					for (ptr1=node;(node!=NULL)&&(node->nodetype!=VARIABLE)&&(((symbol *)&term[1])->symbol!=node->symbol);
							node=node->nxtpeer){
					}

					/* There is a valid match. */
					if ((node!=NULL)&&(node->nodetype!=VARIABLE)){

						/* Set node item. */
						node->item=term;

						/* The node children are leafs. Return full match. */
						if (node->childtype==LEAFCHILD){
							lstmatch=node->child.childleaf;
							*pcand=lstmatch->item;
							return(1);
						}

						/* The node children are not leafs. Prepare next traversing. */
						term=NextItem(term,node->nodetype!=VARIABLE?IMMED:OVERSUBTERMS);
						node=node->child.childnode;

					/* There is no match. Prepare next iteration as a backtrack. */
					} else {
						status=1;
						node=ptr1->parent;
					}
				}
			}
		}
	}

	/* Return candidate not found. */
	lstmatch=NULL;
	return(0);
} /* QueryInsDscTree */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Discrimination tree generalization Query)  OCJ
 *
 *    This function gets the next candidate item that is a generalization
 *    candidate of a given item in a given clause by searching a specified
 *    discrimination tree.
 *
 *    VERY IMPORTANT: This function is not multi-thread safe because:
 *    - the "item" field of dstrnode structure is used for bactracking
 *      when multiple calls are executed to find multiple matches for
 *      the same clause item.
 *    - The static variable lstmatch is used to store the last match
 *      found by the last call.
 *
 *
 *  ARGUMENTS:
 *
 *    item: Pointer to a fully indexed clause item for which a matching item
 *          will be searched. This cannot be a pointer to a variable, but
 *          this is not checked.
 *    pcand: This is the address of pointer to candidate item match. Not used
 *           if no candidate is found.
 *    topnode: Pointer to first level node or leaf of tree to be used for search.
 *             This must be the value of the discr[x] field in the item hashchain
 *             structure.
 *    flag: 0 if an initial search must be performed.
 *          1 if a search for the next match must be performed. If the previous
 *          search was not successful then the search will start as if flag is 0.
 *
 *  RETURNS:
 *
 *    1 if a valid candidate was found.
 *    0 if no valid candidate was found.
 *
 *--------------------------------------------------------------*/
int32_t QueryGenDscTree(uint8_t *item,uint8_t **pcand,void *topnode,int32_t flag){
	auto int32_t status;                            /* 0 if traversing, 1 if backtracking */
	static dstrleaf *lstmatch=NULL;                 /* Last match found in a previous call */
	auto dstrnode *node;                            /* Pointer to current tree node */
	auto uint8_t *term;                             /* Current term in item */
	auto dstrnode *ptr1;                            /* Auxiliary pointer */
	auto int32_t ii;                                /* Auxiliary */

	/* This is a search for next match. */
	node=NULL; /* Just to avoid compiler warnings. */
	if (flag==1){

		/* Last search was successful. */
		if (lstmatch!=NULL){

			/* If there is another leaf available then return full match. */
			if (lstmatch->peer!=NULL){
				lstmatch=lstmatch->peer;
				*pcand=lstmatch->item;
				return(1);
			}

			/* Set search status to backtracking mode. */
			status=1;
			node=lstmatch->parent;

		/* Last search was not successful. Set flag to initial search. */
		} else {
			flag=0;
		}
	}

	/* This is an initial search. */
	if (flag==0){

		/* If tree is empty then return failure. */
		if (topnode==NULL){
			lstmatch=NULL;
			return(0);
		}

		/* The tree has only leafs. Return full match. */
		if (((symbol *)&item[1])->symbol->arity==0){
			lstmatch=(dstrleaf *)topnode;
			*pcand=lstmatch->item;
			return(1);

		/* The tree has nodes. Set search status to traversing mode. */
		} else {
			status=0;
			node=(dstrnode *)topnode;
			term=NextItem(item,IMMED);
		}
	}

	/* Search loop. */
	while (1){

		/* Backtracking status. Now node is the last matching node found. */
		if (status){

			/* Loop through the last matching node available at each iteration. */
			/* Get matching clause term and try to find a new matching node. */
			for (ii=0;(ii==0)&&(node!=NULL);){
				term=node->item;

				/* Node item is not a sub-term of a node matching a clause variable. */
				if (term!=NULL){

					/* Matching clause term is a variable and node has peers so this */
					/* is not a generalization. Any peer node is a valid match. Prepare */
					/* next traversing. */
					if ((VARIABLE&term[0])&&(node->nxtpeer!=NULL)){
						node=node->nxtpeer;
						ii=1;
						status=0;

					/* Node has peers so matching clause term is not a variable. */
					} else if (node->nxtpeer!=NULL){

						/* If there is a peer node and last peer node is a variable then */
						/* we have a valid match. Prepare next traversing. */
						if (node->parent==NULL){
							node=((dstrnode *)topnode)->prevpeer;
						} else {
							node=node->parent->child.childnode->prevpeer;
						}
						if (node->nodetype==VARIABLE){
							ii=1;
							status=0;
						}
					}

				/* Node item is a sub-term of a node matching a clause variable. */
				/* If it has peers then follow line to next node that is not a */
				/* sub-term and prepare traversing. */
				} else if (node->nxtpeer!=NULL){

					/* Backtrack to the node matching the clause variable */
					/* and store it in ptr1. */
					for (ptr1=node->parent;ptr1->item==NULL;ptr1=ptr1->parent){
					}

					/* Traverse from node->nxtpeer to the first node that is not a sub-term */
					/* of the ptr1 item or the first node that has leaf children. Traversed */
					/* nodes item field are set to NULL to indicate that are sub-terms of a */
					/* node matching a clause variable. */
					for (node=node->nxtpeer;
							(node->childtype==NODECHILD)&&(node->pndbrkts>ptr1->pndbrkts);
							node=node->child.childnode){
						node->item=NULL;
					}

					/* If node children are leafs and node is a sub-term of the ptr1 item */
					/* then return full match. The node item field is set to NULL as before. */
					if ((node->childtype==LEAFCHILD)&&(node->pndbrkts>ptr1->pndbrkts)){
						node->item=NULL;
						lstmatch=node->child.childleaf;
						*pcand=lstmatch->item;
						return(1);
					}

					/* Prepare traversing. */
					term=NextItem(ptr1->item,OVERSUBTERMS);
					ii=1;
					status=0;
				}

				/* Prepare next backtracking iteration. */
				if (ii==0){
					node=node->parent;
				}
			}

			/* If no candidate for traversing was found then leave the loop. */
			if (ii==0){
				break;
			}

		/* Traversing status. Now node is the first node to be checked for a match and */
		/* term points to the clause term for which a match is being searched. */
		} else {

			/* Clause term is a variable and node item is a variable. The node is a valid match. */
			if ((VARIABLE&term[0])&&(node->nodetype==VARIABLE)){

				/* Set node item. */
				node->item=term;

				/* Traverse until the first node that is not a sub-term of the */
				/* matching node or the first node that has leaf children. Traversed */
				/* nodes item field are set to NULL to indicate that are sub-terms of a */
				/* node matching a clause variable. */
				ptr1=node;
				if (node->childtype==NODECHILD){
					for (node=node->child.childnode;
							(node->childtype==NODECHILD)&&(node->pndbrkts>ptr1->pndbrkts);
							node=node->child.childnode){
						node->item=NULL;
					}
				}

				/* If node children are leafs and node is ptr1 or a sub-term of the ptr1 */
				/* item then return full match. If node is a sub-term of ptr1 then the */
				/* node item field is set to NULL to indicate that it is sub-terms of a */
				/* node matching a clause variable. */
				if ((node->childtype==LEAFCHILD)&&((node==ptr1)||(node->pndbrkts>ptr1->pndbrkts))){
					if (node!=ptr1){
						node->item=NULL; /* node is a sub-term of ptr1 */
					}
					lstmatch=node->child.childleaf;
					*pcand=lstmatch->item;
					return(1);
				}

				/* Prepare next traversing. */
				term=NextItem(term,OVERSUBTERMS);

			/* Clause term is a variable therefore node item is not a variable */
			/* and this is a generalization. */
			} else if (VARIABLE&term[0]){

				/* If there is a peer node and last peer node is a variable then */
				/* we have a valid match. */
				if (node->parent==NULL){
					node=((dstrnode *)topnode)->prevpeer;
				} else {
					node=node->parent->child.childnode->prevpeer;
				}
				if (node->nodetype==VARIABLE){

					/* Set node item. */
					node->item=term;

					/* The node children are leafs. Return full match. */
					if (node->childtype==LEAFCHILD){
						lstmatch=node->child.childleaf;
						*pcand=lstmatch->item;
						return(1);

					/* The node children are not leafs. Prepare next traversing. */
					} else {
						node=node->child.childnode;
						term=NextItem(term,OVERSUBTERMS);
					}

				/* Last peer node is not a variable, match is not possible. */
				/* Prepare next iteration as a backtrack. */
				} else {
					status=1;
					node=node->parent;
				}

			/* Clause term is not a variable. */
			} else {

				/* Node is a variable. We have a match. */
				if (node->nodetype==VARIABLE){

					/* Set node item. */
					node->item=term;

					/* The node children are leafs. Return full match. */
					if (node->childtype==LEAFCHILD){
						lstmatch=node->child.childleaf;
						*pcand=lstmatch->item;
						return(1);

					/* The node children are not leafs. Prepare next traversing. */
					} else {
						node=node->child.childnode;
						term=NextItem(term,OVERSUBTERMS);
					}

				/* Node is not a variable. */
				} else {

					/* Scan node and its peers for a match. */
					for (ptr1=node;(node!=NULL)&&(node->nodetype!=VARIABLE)&&(((symbol *)&term[1])->symbol!=node->symbol);
							node=node->nxtpeer){
					}

					/* There is a valid match. */
					if (node!=NULL){

						/* Set node item. */
						node->item=term;

						/* The node children are leafs. Return full match. */
						if (node->childtype==LEAFCHILD){
							lstmatch=node->child.childleaf;
							*pcand=lstmatch->item;
							return(1);
						}

						/* The node children are not leafs. Prepare next traversing. */
						term=NextItem(term,node->nodetype!=VARIABLE?IMMED:OVERSUBTERMS);
						node=node->child.childnode;

					/* There is no match. Prepare next iteration as a backtrack. */
					} else {
						status=1;
						node=ptr1->parent;
					}
				}
			}
		}
	}

	/* Return candidate not found. */
	lstmatch=NULL;
	return(0);
} /* QueryGenDscTree */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Discrimination tree unification Query)  OCJ
 *
 *    This function gets the next candidate item that is an unification
 *    of a given item in a given clause by searching a specified discrimination tree.
 *
 *    VERY IMPORTANT: This function is not multi-thread safe because:
 *    - the "item" field of dstrnode structure is used for bactracking
 *      when multiple calls are executed to find multiple matches for
 *      the same clause item.
 *    - The static variable lstmatch is used to store the last match
 *      found by the last call.
 *
 *
 *  ARGUMENTS:
 *
 *    item: Pointer to a fully indexed clause item for which a matching item
 *          will be searched. This cannot be a pointer to a variable, but
 *          this is not checked.
 *    pcand: This is the address of pointer to candidate item match. Not used
 *           if no candidate is found.
 *    topnode: Pointer to first level node or leaf of tree to be used for search.
 *             This must be the value of the discr[x] field in the item hashchain
 *             structure.
 *    flag: 0 if an initial search must be performed.
 *          1 if a search for the next match must be performed. If the previous
 *          search was not successful then the search will start as if flag is 0.
 *
 *  RETURNS:
 *
 *    1 if a valid candidate was found.
 *    0 if no valid candidate was found.
 *
 *--------------------------------------------------------------*/
int32_t QueryUnfDscTree(uint8_t *item,uint8_t **pcand,void *topnode,int32_t flag){
	auto int32_t status;                            /* 0 if traversing, 1 if backtracking */
	static dstrleaf *lstmatch=NULL;                 /* Last match found in a previous call */
	auto dstrnode *node;                            /* Pointer to current tree node */
	auto uint8_t *term;                             /* Current term in item */
	auto dstrnode *ptr1;                            /* Auxiliary pointer */
	auto int32_t ii;                                /* Auxiliary */

	/* This is a search for next match. */
	node=NULL; /* Just to avoid compiler warnings. */
	if (flag==1){

		/* Last search was successful. */
		if (lstmatch!=NULL){

			/* If there is another leaf available then return full match. */
			if (lstmatch->peer!=NULL){
				lstmatch=lstmatch->peer;
				*pcand=lstmatch->item;
				return(1);
			}

			/* Set search status to backtracking mode. */
			status=1;
			node=lstmatch->parent;

		/* Last search was not successful. Set flag to initial search. */
		} else {
			flag=0;
		}
	}

	/* This is an initial search. */
	if (flag==0){

		/* If tree is empty then return failure. */
		if (topnode==NULL){
			lstmatch=NULL;
			return(0);
		}

		/* The tree has only leafs. Return full match. */
		if (((symbol *)&item[1])->symbol->arity==0){
			lstmatch=(dstrleaf *)topnode;
			*pcand=lstmatch->item;
			return(1);

		/* The tree has nodes. Set search status to traversing mode. */
		} else {
			status=0;
			node=(dstrnode *)topnode;
			term=NextItem(item,IMMED);
		}
	}

	/* Search loop. */
	while (1){

		/* Backtracking status. Now node is the last matching node found. */
		if (status){

			/* Loop through the last matching node available at each iteration. */
			/* Get matching clause term and try to find a new matching node. */
			for (ii=0;(ii==0)&&(node!=NULL);){
				term=node->item;

				/* Node item is not a sub-term of a node matching a clause variable. */
				if (term!=NULL){

					/* Matching clause term is a variable and node has peers so this */
					/* is not a generalization. Any peer node is a valid match. Prepare */
					/* next traversing. */
					if ((VARIABLE&term[0])&&(node->nxtpeer!=NULL)){
						node=node->nxtpeer;
						ii=1;
						status=0;

					/* Node has peers so matching clause term is not a variable and this */
					/* is not an instantiation. */
					} else if (node->nxtpeer!=NULL){

						/* If there is a peer node and last peer node is a variable then */
						/* we have a valid match. Prepare next traversing. */
						if (node->parent==NULL){
							node=((dstrnode *)topnode)->prevpeer;
						} else {
							node=node->parent->child.childnode->prevpeer;
						}
						if (node->nodetype==VARIABLE){
							ii=1;
							status=0;
						}
					}

				/* Node item is a sub-term of a node matching a clause variable. */
				/* If it has peers then follow line to next node that is not a */
				/* sub-term and prepare traversing. */
				} else if (node->nxtpeer!=NULL){

					/* Backtrack to the node matching the clause variable */
					/* and store it in ptr1. */
					for (ptr1=node->parent;ptr1->item==NULL;ptr1=ptr1->parent){
					}

					/* Traverse from node->nxtpeer to the first node that is not a sub-term */
					/* of the ptr1 item or the first node that has leaf children. Traversed */
					/* nodes item field are set to NULL to indicate that are sub-terms of a */
					/* node matching a clause variable. */
					for (node=node->nxtpeer;
							(node->childtype==NODECHILD)&&(node->pndbrkts>ptr1->pndbrkts);
							node=node->child.childnode){
						node->item=NULL;
					}

					/* If node children are leafs and node is a sub-term of the ptr1 item */
					/* then return full match. The node item field is set to NULL as before. */
					if ((node->childtype==LEAFCHILD)&&(node->pndbrkts>ptr1->pndbrkts)){
						node->item=NULL;
						lstmatch=node->child.childleaf;
						*pcand=lstmatch->item;
						return(1);
					}

					/* Prepare traversing. */
					term=NextItem(ptr1->item,OVERSUBTERMS);
					ii=1;
					status=0;
				}

				/* Prepare next backtracking iteration. */
				if (ii==0){
					node=node->parent;
				}
			}

			/* If no candidate for traversing was found then leave the loop. */
			if (ii==0){
				break;
			}

		/* Traversing status. Now node is the first node to be checked for a match and */
		/* term points to the clause term for which a match is being searched. */
		} else {

			/* Clause term is a variable and either node item is a variable or */
			/* this is not a generalization. The node is a valid match. */
			if (VARIABLE&term[0]){

				/* Set node item. */
				node->item=term;

				/* Traverse until the first node that is not a sub-term of the */
				/* matching node or the first node that has leaf children. Traversed */
				/* nodes item field are set to NULL to indicate that are sub-terms of a */
				/* node matching a clause variable. */
				ptr1=node;
				if (node->childtype==NODECHILD){
					for (node=node->child.childnode;
							(node->childtype==NODECHILD)&&(node->pndbrkts>ptr1->pndbrkts);
							node=node->child.childnode){
						node->item=NULL;
					}
				}

				/* If node children are leafs and node is ptr1 or a sub-term of the ptr1 */
				/* item then return full match. If node is a sub-term of ptr1 then the */
				/* node item field is set to NULL to indicate that it is sub-terms of a */
				/* node matching a clause variable. */
				if ((node->childtype==LEAFCHILD)&&((node==ptr1)||(node->pndbrkts>ptr1->pndbrkts))){
					if (node!=ptr1){
						node->item=NULL; /* node is a sub-term of ptr1 */
					}
					lstmatch=node->child.childleaf;
					*pcand=lstmatch->item;
					return(1);
				}

				/* Prepare next traversing. */
				term=NextItem(term,OVERSUBTERMS);

			/* Clause term is not a variable. */
			} else {

				/* Node is a variable. We have a match. */
				if (node->nodetype==VARIABLE){

					/* Set node item. */
					node->item=term;

					/* The node children are leafs. Return full match. */
					if (node->childtype==LEAFCHILD){
						lstmatch=node->child.childleaf;
						*pcand=lstmatch->item;
						return(1);

					/* The node children are not leafs. Prepare next traversing. */
					} else {
						node=node->child.childnode;
						term=NextItem(term,OVERSUBTERMS);
					}

				/* Node is not a variable. */
				} else {

					/* Scan node and its peers for a match. */
					for (ptr1=node;(node!=NULL)&&(node->nodetype!=VARIABLE)&&(((symbol *)&term[1])->symbol!=node->symbol);
							node=node->nxtpeer){
					}

					/* There is a valid match. */
					if (node!=NULL){

						/* Set node item. */
						node->item=term;

						/* The node children are leafs. Return full match. */
						if (node->childtype==LEAFCHILD){
							lstmatch=node->child.childleaf;
							*pcand=lstmatch->item;
							return(1);
						}

						/* The node children are not leafs. Prepare next traversing. */
						term=NextItem(term,node->nodetype!=VARIABLE?IMMED:OVERSUBTERMS);
						node=node->child.childnode;

					/* There is no match. Prepare next iteration as a backtrack. */
					} else {
						status=1;
						node=ptr1->parent;
					}
				}
			}
		}
	}

	/* Return candidate not found. */
	lstmatch=NULL;
	return(0);
} /* QueryUnfDscTree */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Query a discrimination tree by recursive function)  OCJ
 *
 *    This function is similar to QueryDscTree() with the following
 *    differences:
 *    - It works with recursive functions that may potentially call
 *      this function simultaneously for the same discrimination tree
 *      from several instances of the same recursive function.
 *    - It works only for the GENERALIZATION case.
 *    - It uses KBO ordering only.
 *
 *    To allow calls from recursive functions the "item" field of dstrnode
 *    structure and the lstmatch pointer of original QueryDscTree()
 *    function cannot be used because different backtrack information
 *    must be maintained for each recursive function instance. This is
 *    accomplished by each recursive function instance using its own
 *    own dstrbktrk structure with all the necessary backtrack information.
 *
 *    See QueryDscTree() global comments for additional information.
 *
 *
 *  ARGUMENTS:
 *
 *    item: Pointer to a fully indexed clause item for which a matching item
 *          will be searched. This cannot be a pointer to a variable, but
 *          this is not checked.
 *    pcand: This is the address of pointer to candidate item match. Not used
 *           if no candidate is found.
 *    topnode: Pointer to first level node or leaf of tree to be used for search.
 *             This must be the value of the discr[x] field in the item hashchain
 *             structure.
 *    backtrk: Pointer to dstrbktrk initialized by the calling function. It must
 *             have the lstmatch field set to NULL for the initial search.
 *    type: UNIFICATION: search for an item that unifies with the query term.
 *          INSTANCE: search for an item that is an instance of the query term.
 *          GENERALIZATION: search for an item that is a generalization of the
 *          query term.
 *
 *  RETURNS:
 *
 *    1 if a valid candidate was found.
 *    0 if no valid candidate was found.
 *
 *--------------------------------------------------------------*/
int32_t QueryDscTree2(uint8_t *item,uint8_t **pcand,void *topnode,dstrbktrk *backtrk,int32_t type){
	auto int32_t status;                            /* 0 if traversing, 1 if backtracking */
	auto dstrnode *node;                            /* Pointer to current tree node */
	auto uint8_t *term;                             /* Current term in item */
	auto dstrnode *ptr1;                            /* Auxiliary pointer */
	auto int32_t ii,jj;                             /* Auxiliary */

	/* This is a search for next match. */
	node=NULL; /* Just to avoid compiler warnings. */
	if (backtrk->lstmatch!=NULL){

		/* If there is another leaf available then return full match. */
		if (backtrk->lstmatch->peer!=NULL){
			backtrk->lstmatch=backtrk->lstmatch->peer;
			*pcand=backtrk->lstmatch->item;
			return(1);
		}

		/* Set search status to backtracking mode. */
		status=1;
		node=backtrk->lstmatch->parent;

	/* This is an initial search. */
	} else {

		/* If tree is empty then return failure. */
		if (topnode==NULL){
			backtrk->lstmatch=NULL;
			return(0);
		}

		/* The tree has only leafs. Return full match. */
		backtrk->nodeidx=0; /* Initialize node index to top node. */
		if (((symbol *)&item[1])->symbol->arity==0){
			backtrk->lstmatch=(dstrleaf *)topnode;
			*pcand=backtrk->lstmatch->item;
			return(1);

		/* The tree has nodes. Set search status to traversing mode. */
		} else {
			status=0;
			node=(dstrnode *)topnode;
			backtrk->nodeidx=0;
			term=NextItem(item,IMMED);
		}
	}

	/* Search loop. */
	while (1){

		/* Backtracking status. Now node is the last matching node found. */
		if (status){

			/* Loop through the last matching node available at each iteration. */
			/* Get matching clause term and try to find a new matching node. */
			for (ii=0;(ii==0)&&(node!=NULL);){
				term=backtrk->treeitems[backtrk->nodeidx];

				/* Node item is not a sub-term of a node matching a clause variable. */
				if (term!=NULL){

					/* Matching clause term is a variable and node has peers so this */
					/* is not a generalization. Any peer node is a valid match. Prepare */
					/* next traversing. */
					if ((VARIABLE&term[0])&&(node->nxtpeer!=NULL)){
						node=node->nxtpeer;
						ii=1;
						status=0;

					/* Node has peers so matching clause term is not a variable and this */
					/* is not an instantiation. */
					} else if ((node->nxtpeer!=NULL)&&(type!=INSTANCE)){

						/* If there is a peer node and last peer node is a variable then */
						/* we have a valid match. Prepare next traversing. */
						if (node->parent==NULL){
							node=((dstrnode *)topnode)->prevpeer;
						} else {
							node=node->parent->child.childnode->prevpeer;
						}
						if (node->nodetype==VARIABLE){
							ii=1;
							status=0;
						}
					}

				/* Node item is a sub-term of a node matching a clause variable. */
				/* If it has peers then follow line to next node that is not a */
				/* sub-term and prepare traversing. */
				} else if (node->nxtpeer!=NULL){

					/* Backtrack to the node matching the clause variable */
					/* and store it in ptr1. */
					for (ptr1=node->parent,jj=backtrk->nodeidx-1;backtrk->treeitems[jj]==NULL;
							ptr1=ptr1->parent,jj--){
					}

					/* Traverse from node->nxtpeer to the first node that is not a sub-term */
					/* of the ptr1 item or the first node that has leaf children. Traversed */
					/* nodes item field are set to NULL to indicate that are sub-terms of a */
					/* node matching a clause variable. */
					for (node=node->nxtpeer;
							(node->childtype==NODECHILD)&&(node->pndbrkts>ptr1->pndbrkts);
							node=node->child.childnode,backtrk->nodeidx++){
						backtrk->treeitems[backtrk->nodeidx]=NULL;
					}

					/* If node children are leafs and node is a sub-term of the ptr1 item */
					/* then return full match. The node item field is set to NULL as before. */
					if ((node->childtype==LEAFCHILD)&&(node->pndbrkts>ptr1->pndbrkts)){
						backtrk->treeitems[backtrk->nodeidx]=NULL;
						backtrk->lstmatch=node->child.childleaf;
						*pcand=backtrk->lstmatch->item;;
						return(1);
					}

					/* Prepare traversing. */
					term=NextItem(backtrk->treeitems[jj],OVERSUBTERMS);
					ii=1;
					status=0;
				}

				/* Prepare next backtracking iteration. */
				if (ii==0){
					node=node->parent;
					backtrk->nodeidx--;
				}
			}

			/* If no candidate for traversing was found then leave the loop. */
			if (ii==0){
				break;
			}

		/* Traversing status. Now node is the first node to be checked for a match and */
		/* term points to the clause term for which a match is being searched. */
		} else {

			/* Clause term is a variable and either node item is a variable or */
			/* this is not a generalization. The node is a valid match. */
			if ((VARIABLE&term[0])&&((node->nodetype==VARIABLE)||(type!=GENERALIZATION))){

				/* Set node item. */
				backtrk->treeitems[backtrk->nodeidx]=term;

				/* Traverse until the first node that is not a sub-term of the */
				/* matching node or the first node that has leaf children. Traversed */
				/* nodes item field are set to NULL to indicate that are sub-terms of a */
				/* node matching a clause variable. */
				ptr1=node;
				if (node->childtype==NODECHILD){
					for (node=node->child.childnode,backtrk->nodeidx++;
							(node->childtype==NODECHILD)&&(node->pndbrkts>ptr1->pndbrkts);
							node=node->child.childnode,backtrk->nodeidx++){
						backtrk->treeitems[backtrk->nodeidx]=NULL;
					}
				}

				/* If node children are leafs and node is ptr1 or a sub-term of the ptr1 */
				/* item then return full match. If node is a sub-term of ptr1 then the */
				/* node item field is set to NULL to indicate that it is sub-terms of a */
				/* node matching a clause variable. */
				if ((node->childtype==LEAFCHILD)&&((node==ptr1)||(node->pndbrkts>ptr1->pndbrkts))){
					if (node!=ptr1){
						backtrk->treeitems[backtrk->nodeidx]=NULL; /* node is a sub-term of ptr1 */
					}
					backtrk->lstmatch=node->child.childleaf;
					*pcand=backtrk->lstmatch->item;
					return(1);
				}

				/* Prepare next traversing. */
				term=NextItem(term,OVERSUBTERMS);

			/* Clause term is a variable therefore node item is not a variable */
			/* and this is a generalization. */
			} else if (VARIABLE&term[0]){

				/* If there is a peer node and last peer node is a variable then */
				/* we have a valid match. */
				if (node->parent==NULL){
					node=((dstrnode *)topnode)->prevpeer;
				} else {
					node=node->parent->child.childnode->prevpeer;
				}
				if (node->nodetype==VARIABLE){

					/* Set node item. */
					backtrk->treeitems[backtrk->nodeidx]=term;

					/* The node children are leafs. Return full match. */
					if (node->childtype==LEAFCHILD){
						backtrk->lstmatch=node->child.childleaf;
						*pcand=backtrk->lstmatch->item;;
						return(1);

					/* The node children are not leafs. Prepare next traversing. */
					} else {
						node=node->child.childnode;
						backtrk->nodeidx++;
						term=NextItem(term,OVERSUBTERMS);
					}

				/* Last peer node is not a variable, match is not possible. */
				/* Prepare next iteration as a backtrack. */
				} else {
					status=1;
					node=node->parent;
					backtrk->nodeidx--;
				}

			/* Clause term is not a variable. */
			} else {

				/* Node is a variable. */
				if (node->nodetype==VARIABLE){

					/* If this is not an instantiation then we have a match. */
					if (type!=INSTANCE){

						/* Set node item. */
						backtrk->treeitems[backtrk->nodeidx]=term;

						/* The node children are leafs. Return full match. */
						if (node->childtype==LEAFCHILD){
							backtrk->lstmatch=node->child.childleaf;
							*pcand=backtrk->lstmatch->item;;
							return(1);

						/* The node children are not leafs. Prepare next traversing. */
						} else {
							node=node->child.childnode;
							backtrk->nodeidx++;
							term=NextItem(term,OVERSUBTERMS);
						}

					/* This is an instantiation. As node is a variable there are */
					/* no more peers so matching is not possible. Prepare next */
					/* iteration as a backtrack. */
					} else {
						status=1;
						node=node->parent;
						backtrk->nodeidx--;
					}

				/* Node is not a variable. */
				} else {

					/* Scan node and its peers for a match. */
					for (ptr1=node;(node!=NULL)&&(node->nodetype!=VARIABLE)&&(((symbol *)&term[1])->symbol!=node->symbol);
							node=node->nxtpeer){
					}

					/* There is a valid match. */
					if ((node!=NULL)&&((node->nodetype!=VARIABLE)||(type!=INSTANCE))){

						/* Set node item. */
						backtrk->treeitems[backtrk->nodeidx]=term;

						/* The node children are leafs. Return full match. */
						if (node->childtype==LEAFCHILD){
							backtrk->lstmatch=node->child.childleaf;
							*pcand=backtrk->lstmatch->item;;
							return(1);
						}

						/* The node children are not leafs. Prepare next traversing. */
						term=NextItem(term,node->nodetype!=VARIABLE?IMMED:OVERSUBTERMS);
						node=node->child.childnode;
						backtrk->nodeidx++;

					/* There is no match. Prepare next iteration as a backtrack. */
					} else {
						status=1;
						node=ptr1->parent;
						backtrk->nodeidx--;
					}
				}
			}
		}
	}

	/* Return candidate not found. */
	backtrk->lstmatch=NULL;
	return(0);
} /* QueryDscTree2 */

#ifdef SEMANTICTAUTOLOGY
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Query a semantic tautology discrimination tree)  OCJ
 *
 *    This function is similar to QueryDscTree() with the following
 *    differences:
 *    - It allows queries for variables that conceptually represent skolems.
 *    - It is called only for one match, no additional matches for the
 *      same query type can be done.
 *    - The item argument itself is not a valid match.
 *
 *
 *  ARGUMENTS:
 *
 *    item: Pointer to a fully indexed clause item for which a matching item
 *          will be searched.
 *    pcand: This is the address of pointer to candidate item match. Not used
 *           if no candidate is found.
 *    pformula: Pointer to formula containing the queried item.
 *    topptr: Address of pointer to first level node or leaf of tree to be used for
 *            search. This is only used if the queried item is a function
 *            or (in)equality.
 *
 *  RETURNS:
 *
 *    1 if a valid match was found. The match found is deleted.
 *    0 if no valid match was found. No tree entry is deleted.
 *
 *--------------------------------------------------------------*/
int32_t QueryTtlgDscTree(uint8_t *item,uint8_t **pcand,uint8_t **pformula,void **topptr){
	auto ttlgdstrleaf **vleafset;                   /* Pointer to set of leaf pointers for skolemized variables */
	auto ttlgdstrleaf *leaf;                        /* Pointers to leafs */
	auto ttlgdstrnode *node;                        /* Pointers to tree nodes */
	auto uint8_t *ptr1,*ptr2;                       /* Auxiliary pointers */
	auto int32_t ii;                                /* Auxiliary */

	/* The queried item is a variable. */
	if (VARIABLE&item[0]){

		/* Queried skolemized variable has a match that is not item itself. */
		vleafset=(ttlgdstrleaf **)topptr;
		ii=*(int16_t *)&item[1];
		if (vleafset[ii]!=NULL){
			if (item!=vleafset[ii]->item){
				leaf=vleafset[ii];
				*pcand=leaf->item;
				*pformula=leaf->formula;
				return(1);
			} else if (vleafset[ii]->peer!=NULL){
				leaf=vleafset[ii]->peer;
				*pcand=leaf->item;
				*pformula=leaf->formula;
				return(1);
			}
		}

		/* Return not valid match found. */
		return(0);
	}

	/* The queried item is not a variable. In this case the tree cannot be empty */
	/* as it has at least the item itself. */
	/* If the tree has only leafs and there is a match that is not the item itself */
	/* then remove leaf of valid matched item and return full match. Otherwise */
	/* return not valid match found. */
	if (((symbol *)&item[1])->symbol->arity==0){
		leaf=*((ttlgdstrleaf **)topptr);
		if (leaf!=NULL){
			if (item!=leaf->item){
				*pcand=leaf->item;
				*pformula=leaf->formula;
				return(1);
			}
			if (leaf->peer!=NULL){
				leaf=leaf->peer;
				*pcand=leaf->item;
				*pformula=leaf->formula;
				return(1);
			}
		}
		return(0);
	}

	/* The tree has nodes. */
	/* Loop through item sub-terms. */
	node=*((ttlgdstrnode **)topptr);
	for (ptr1=NextItem(item,IMMED),ptr2=NextItem(item,OVERSUBTERMS);ptr1<ptr2;
			ptr1=NextItem(ptr1,IMMED),node=node->child.childnode){

		/* scan current node for a match. */
		if (VARIABLE&ptr1[0]){
			ii=*(int16_t *)&ptr1[1];
			for (;(node!=NULL)&&((node->nodetype!=VARIABLE)||(node->itemid.varnb!=ii));
					node=node->nxtpeer){
			}
		} else {
			for (;(node!=NULL)&&((node->nodetype==VARIABLE)||(node->itemid.symbol!=((symbol *)&ptr1[1])->symbol));
					node=node->nxtpeer){
			}
		}

		/* No match found. */
		if (node==NULL){
			return(0);
		}
	}

	/* We have a match. */
	/* The match is the item itself. */
	leaf=(ttlgdstrleaf *)node;
	if (item==leaf->item){

		/* There are no more leafs, return not valid match. */
		if (leaf->peer==NULL){
			return(0);
		}

		/* There are more leafs, return valid match. */
		*pcand=leaf->peer->item;
		*pformula=leaf->peer->formula;
		return(1);
	}

	/* The match is not the item itself. Set match data. */
	*pcand=leaf->item;
	*pformula=leaf->formula;

	return(1);
} /* QueryTtlgDscTree */
#endif

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Link binary clause to clause selection queues)  OCJ
 *
 *    This function links a passive clause in binary to all clause
 *    the selection queues.
 *
 *    See global comments in SetClSelectOrder() function for a
 *    description of how the layered clause selection is implemented.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void LinkSelectQueues(cmprefix *clause){
	auto int32_t queueidx[6];                            /* Set of indices of queues to link to */
	auto cmprefix *ptr1;                                 /* Auxiliary pointer */
	auto int32_t ii,jj,kk,mm,qq;                         /* Auxiliary */

	/* Identify the indices of queues to link to. */
	switch (kbset.opts.layerset[kbset.opts.layer]){
		case SIAVLYR:
			if (clause->part2.bin->sinedist<=SINESEGMENT1){
				if (clause->part2.bin->avdist<=AVSEGMENT1){
					queueidx[0]=0;
					queueidx[1]=1;
					queueidx[2]=2;
					queueidx[3]=SELECTQUEUES;
					queueidx[4]=SELECTQUEUES+1;
					queueidx[5]=SELECTQUEUES+2;
				} else {
					queueidx[0]=3;
					queueidx[1]=4;
					queueidx[2]=5;
					queueidx[3]=SELECTQUEUES+3;
					queueidx[4]=SELECTQUEUES+4;
					queueidx[5]=SELECTQUEUES+5;
				}
				qq=3;
			} else if (clause->part2.bin->sinedist<=SINESEGMENT2){
				if (clause->part2.bin->avdist<=AVSEGMENT1){
					queueidx[0]=1;
					queueidx[1]=2;
					queueidx[2]=SELECTQUEUES+1;
					queueidx[3]=SELECTQUEUES+2;
				} else {
					queueidx[0]=4;
					queueidx[1]=5;
					queueidx[2]=SELECTQUEUES+4;
					queueidx[3]=SELECTQUEUES+5;
				}
				qq=2;
			} else {
				if (clause->part2.bin->avdist<=AVSEGMENT1){
					queueidx[0]=2;
					queueidx[1]=SELECTQUEUES+2;
				} else {
					queueidx[0]=5;
					queueidx[1]=SELECTQUEUES+5;
				}
				qq=1;
			}
			break;
		case SIHRNLYR:
			if (clause->part2.bin->sinedist<=SINESEGMENT1){
				if (clause->part2.bin->hrndist<=HRNSEGMENT1){
					queueidx[0]=0;
					queueidx[1]=1;
					queueidx[2]=2;
					queueidx[3]=SELECTQUEUES;
					queueidx[4]=SELECTQUEUES+1;
					queueidx[5]=SELECTQUEUES+2;
				} else {
					queueidx[0]=3;
					queueidx[1]=4;
					queueidx[2]=5;
					queueidx[3]=SELECTQUEUES+3;
					queueidx[4]=SELECTQUEUES+4;
					queueidx[5]=SELECTQUEUES+5;
				}
				qq=3;
			} else if (clause->part2.bin->sinedist<=SINESEGMENT2){
				if (clause->part2.bin->hrndist<=HRNSEGMENT1){
					queueidx[0]=1;
					queueidx[1]=2;
					queueidx[2]=SELECTQUEUES+1;
					queueidx[3]=SELECTQUEUES+2;
				} else {
					queueidx[0]=4;
					queueidx[1]=5;
					queueidx[2]=SELECTQUEUES+4;
					queueidx[3]=SELECTQUEUES+5;
				}
				qq=2;
			} else {
				if (clause->part2.bin->hrndist<=HRNSEGMENT1){
					queueidx[0]=2;
					queueidx[1]=SELECTQUEUES+2;
				} else {
					queueidx[0]=5;
					queueidx[1]=SELECTQUEUES+5;
				}
				qq=1;
			}
			break;
		case SINELYR:
			if (clause->part2.bin->sinedist<=SINESEGMENT1){
				queueidx[0]=0;
				queueidx[1]=1;
				queueidx[2]=2;
				queueidx[3]=3;
				queueidx[4]=4;
				queueidx[5]=5;
				qq=3;
			} else if (clause->part2.bin->sinedist<=SINESEGMENT2){
				queueidx[0]=1;
				queueidx[1]=2;
				queueidx[2]=4;
				queueidx[3]=5;
				qq=2;
			} else {
				queueidx[0]=2;
				queueidx[1]=5;
				qq=1;
			}
			break;
		case AVHRNLYR:
			qq=1;
			if (clause->part2.bin->avdist<=AVSEGMENT1){
				if (clause->part2.bin->hrndist<=HRNSEGMENT1){
					queueidx[0]=0;
					queueidx[1]=4;
				} else {
					queueidx[0]=2;
					queueidx[1]=6;
				}
			} else {
				if (clause->part2.bin->hrndist<=HRNSEGMENT1){
					queueidx[0]=1;
					queueidx[1]=5;
				} else {
					queueidx[0]=3;
					queueidx[1]=7;
				}
			}
			break;
		case AVLYR:
			qq=1;
			if (clause->part2.bin->avdist<=AVSEGMENT1){
				queueidx[0]=0;
				queueidx[1]=2;
			} else {
				queueidx[0]=1;
				queueidx[1]=3;
			}
			break;
		case HRNLYR:
			qq=1;
			if (clause->part2.bin->hrndist<=HRNSEGMENT1){
				queueidx[0]=0;
				queueidx[1]=2;
			} else {
				queueidx[0]=1;
				queueidx[1]=3;
			}
			break;
		case NONELYR:
		default: /* Just to prevent compiler warnings. */
			qq=1;
			queueidx[0]=0;
			queueidx[1]=1;
			break;
	}

	/* Link clause to weight queues. */
	for (mm=0;mm<qq;mm++){
		kk=queueidx[mm];
		ii=(clause->part2.bin->clweight>(MAXWEIGHT-2)?MAXWEIGHT-1:clause->part2.bin->clweight);
		if (frstglselect[kk]==NULL){
			clause->part2.bin->prevselect[kk]=clause->part2.bin->nextselect[kk]=NULL;
		} else {
			if ((ii==(MAXWEIGHT-1))&&(lstselect[kk][MAXWEIGHT-1]!=NULL)){
				for (ptr1=lstselect[kk][MAXWEIGHT-1];(ptr1!=NULL)&&(ptr1->part2.bin->clweight>clause->part2.bin->clweight);
						ptr1=ptr1->part2.bin->prevselect[kk]){
				}
				clause->part2.bin->prevselect[kk]=ptr1;
				if (ptr1!=NULL){
					clause->part2.bin->nextselect[kk]=ptr1->part2.bin->nextselect[kk];
				} else {
					clause->part2.bin->nextselect[kk]=frstselect[kk][MAXWEIGHT-1];
				}
			} else {
				if ((ii==0)||(lstselect[kk][ii]!=NULL)){
					clause->part2.bin->prevselect[kk]=lstselect[kk][ii];
				} else {
					if (ii==(MAXWEIGHT-1)){
						clause->part2.bin->prevselect[kk]=lstglselect[kk];
					} else {
						for (jj=ii-1;(jj>0)&&(lstselect[kk][jj]==NULL);jj--){
						}
						clause->part2.bin->prevselect[kk]=lstselect[kk][jj];
					}
				}
				if (ii==(MAXWEIGHT-1)){
					clause->part2.bin->nextselect[kk]=NULL;
				} else {
					if (clause->part2.bin->prevselect[kk]!=NULL){
						clause->part2.bin->nextselect[kk]=clause->part2.bin->prevselect[kk]->part2.bin->nextselect[kk];
					} else {
						clause->part2.bin->nextselect[kk]=frstglselect[kk];
					}
				}
			}
		}
		if (clause->part2.bin->nextselect[kk]!=NULL){
			clause->part2.bin->nextselect[kk]->part2.bin->prevselect[kk]=clause;
		}
		if (clause->part2.bin->prevselect[kk]!=NULL){
			clause->part2.bin->prevselect[kk]->part2.bin->nextselect[kk]=clause;
		}
		if ((frstselect[kk][ii]==NULL)
				||((ii==(MAXWEIGHT-1))&&((clause->part2.bin->prevselect[kk]==NULL)
						||(clause->part2.bin->prevselect[kk]->part2.bin->clweight<=(MAXWEIGHT-2))))){
			frstselect[kk][ii]=clause;
		}
		if ((ii<(MAXWEIGHT-1))||(clause->part2.bin->nextselect[kk]==NULL)){
			lstselect[kk][ii]=clause;
		}
		if (clause->part2.bin->prevselect[kk]==NULL){
			frstglselect[kk]=clause;
		}
		if (clause->part2.bin->nextselect[kk]==NULL){
			lstglselect[kk]=clause;
		}
	}

	/* Link clause to age queues. */
	qq*=2;
	for (;mm<qq;mm++){
		kk=queueidx[mm];
		ii=(clause->part2.bin->agedist>(MAXWEIGHT-2)?MAXWEIGHT-1:clause->part2.bin->agedist);
		if (frstglselect[kk]==NULL){
			clause->part2.bin->prevselect[kk]=clause->part2.bin->nextselect[kk]=NULL;
		} else {
			if ((ii==(MAXWEIGHT-1))&&(lstselect[kk][MAXWEIGHT-1]!=NULL)){
				for (ptr1=lstselect[kk][MAXWEIGHT-1];(ptr1!=NULL)&&(ptr1->part2.bin->agedist>clause->part2.bin->agedist);
						ptr1=ptr1->part2.bin->prevselect[kk]){
				}
				clause->part2.bin->prevselect[kk]=ptr1;
				if (ptr1!=NULL){
					clause->part2.bin->nextselect[kk]=ptr1->part2.bin->nextselect[kk];
				} else {
					clause->part2.bin->nextselect[kk]=frstselect[kk][MAXWEIGHT-1];
				}
			} else {
				if ((ii==0)||(lstselect[kk][ii]!=NULL)){
					clause->part2.bin->prevselect[kk]=lstselect[kk][ii];
				} else {
					if (ii==(MAXWEIGHT-1)){
						clause->part2.bin->prevselect[kk]=lstglselect[kk];
					} else {
						for (jj=ii-1;(jj>0)&&(lstselect[kk][jj]==NULL);jj--){
						}
						clause->part2.bin->prevselect[kk]=lstselect[kk][jj];
					}
				}
				if (ii==(MAXWEIGHT-1)){
					clause->part2.bin->nextselect[kk]=NULL;
				} else {
					if (clause->part2.bin->prevselect[kk]!=NULL){
						clause->part2.bin->nextselect[kk]=clause->part2.bin->prevselect[kk]->part2.bin->nextselect[kk];
					} else {
						clause->part2.bin->nextselect[kk]=frstglselect[kk];
					}
				}
			}
		}
		if (clause->part2.bin->nextselect[kk]!=NULL){
			clause->part2.bin->nextselect[kk]->part2.bin->prevselect[kk]=clause;
		}
		if (clause->part2.bin->prevselect[kk]!=NULL){
			clause->part2.bin->prevselect[kk]->part2.bin->nextselect[kk]=clause;
		}
		if ((frstselect[kk][ii]==NULL)
				||((ii==(MAXWEIGHT-1))&&((clause->part2.bin->prevselect[kk]==NULL)
						||(clause->part2.bin->prevselect[kk]->part2.bin->agedist<=(MAXWEIGHT-2))))){
			frstselect[kk][ii]=clause;
		}
		if ((ii<(MAXWEIGHT-1))||(clause->part2.bin->nextselect[kk]==NULL)){
			lstselect[kk][ii]=clause;
		}
		if (clause->part2.bin->prevselect[kk]==NULL){
			frstglselect[kk]=clause;
		}
		if (clause->part2.bin->nextselect[kk]==NULL){
			lstglselect[kk]=clause;
		}
	}

	return;
} /* LinkSelectQueues */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Unlink binary clause to clause selection queues)  OCJ
 *
 *    This function unlinks a passive clause in binary from all clause
 *    the selection queues.
 *
 *    See global comments in SetClSelectOrder() function for a
 *    description of how the layered clause selection is implemented.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void UnlinkSelectQueues(cmprefix *clause){
	auto int32_t queueidx[6];                            /* Set of indices of queues to link to */
	auto int32_t ii,jj,kk,mm,qq;                         /* Auxiliary */

	/* Identify the indices of queues to link to. */
	switch (kbset.opts.layerset[kbset.opts.layer]){
		case SIAVLYR:
			if (clause->part2.bin->sinedist<=SINESEGMENT1){
				if (clause->part2.bin->avdist<=AVSEGMENT1){
					queueidx[0]=0;
					queueidx[1]=1;
					queueidx[2]=2;
					queueidx[3]=SELECTQUEUES;
					queueidx[4]=SELECTQUEUES+1;
					queueidx[5]=SELECTQUEUES+2;
				} else {
					queueidx[0]=3;
					queueidx[1]=4;
					queueidx[2]=5;
					queueidx[3]=SELECTQUEUES+3;
					queueidx[4]=SELECTQUEUES+4;
					queueidx[5]=SELECTQUEUES+5;
				}
				qq=3;
			} else if (clause->part2.bin->sinedist<=SINESEGMENT2){
				if (clause->part2.bin->avdist<=AVSEGMENT1){
					queueidx[0]=1;
					queueidx[1]=2;
					queueidx[2]=SELECTQUEUES+1;
					queueidx[3]=SELECTQUEUES+2;
				} else {
					queueidx[0]=4;
					queueidx[1]=5;
					queueidx[2]=SELECTQUEUES+4;
					queueidx[3]=SELECTQUEUES+5;
				}
				qq=2;
			} else {
				if (clause->part2.bin->avdist<=AVSEGMENT1){
					queueidx[0]=2;
					queueidx[1]=SELECTQUEUES+2;
				} else {
					queueidx[0]=5;
					queueidx[1]=SELECTQUEUES+5;
				}
				qq=1;
			}
			break;
		case SIHRNLYR:
			if (clause->part2.bin->sinedist<=SINESEGMENT1){
				if (clause->part2.bin->hrndist<=HRNSEGMENT1){
					queueidx[0]=0;
					queueidx[1]=1;
					queueidx[2]=2;
					queueidx[3]=SELECTQUEUES;
					queueidx[4]=SELECTQUEUES+1;
					queueidx[5]=SELECTQUEUES+2;
				} else {
					queueidx[0]=3;
					queueidx[1]=4;
					queueidx[2]=5;
					queueidx[3]=SELECTQUEUES+3;
					queueidx[4]=SELECTQUEUES+4;
					queueidx[5]=SELECTQUEUES+5;
				}
				qq=3;
			} else if (clause->part2.bin->sinedist<=SINESEGMENT2){
				if (clause->part2.bin->hrndist<=HRNSEGMENT1){
					queueidx[0]=1;
					queueidx[1]=2;
					queueidx[2]=SELECTQUEUES+1;
					queueidx[3]=SELECTQUEUES+2;
				} else {
					queueidx[0]=4;
					queueidx[1]=5;
					queueidx[2]=SELECTQUEUES+4;
					queueidx[3]=SELECTQUEUES+5;
				}
				qq=2;
			} else {
				if (clause->part2.bin->hrndist<=HRNSEGMENT1){
					queueidx[0]=2;
					queueidx[1]=SELECTQUEUES+2;
				} else {
					queueidx[0]=5;
					queueidx[1]=SELECTQUEUES+5;
				}
				qq=1;
			}
			break;
		case SINELYR:
			if (clause->part2.bin->sinedist<=SINESEGMENT1){
				queueidx[0]=0;
				queueidx[1]=1;
				queueidx[2]=2;
				queueidx[3]=3;
				queueidx[4]=4;
				queueidx[5]=5;
				qq=3;
			} else if (clause->part2.bin->sinedist<=SINESEGMENT2){
				queueidx[0]=1;
				queueidx[1]=2;
				queueidx[2]=4;
				queueidx[3]=5;
				qq=2;
			} else {
				queueidx[0]=2;
				queueidx[1]=5;
				qq=1;
			}
			break;
		case AVHRNLYR:
			qq=1;
			if (clause->part2.bin->avdist<=AVSEGMENT1){
				if (clause->part2.bin->hrndist<=HRNSEGMENT1){
					queueidx[0]=0;
					queueidx[1]=4;
				} else {
					queueidx[0]=2;
					queueidx[1]=6;
				}
			} else {
				if (clause->part2.bin->hrndist<=HRNSEGMENT1){
					queueidx[0]=1;
					queueidx[1]=5;
				} else {
					queueidx[0]=3;
					queueidx[1]=7;
				}
			}
			break;
		case AVLYR:
			qq=1;
			if (clause->part2.bin->avdist<=AVSEGMENT1){
				queueidx[0]=0;
				queueidx[1]=2;
			} else {
				queueidx[0]=1;
				queueidx[1]=3;
			}
			break;
		case HRNLYR:
			qq=1;
			if (clause->part2.bin->hrndist<=HRNSEGMENT1){
				queueidx[0]=0;
				queueidx[1]=2;
			} else {
				queueidx[0]=1;
				queueidx[1]=3;
			}
			break;
		case NONELYR:
		default: /* Just to prevent compiler warningss. */
			qq=1;
			queueidx[0]=0;
			queueidx[1]=1;
			break;
	}

	/* Unlink clause from weight queues. */
	for (mm=0;mm<qq;mm++){
		kk=queueidx[mm];
		ii=(clause->part2.bin->clweight>(MAXWEIGHT-2)?MAXWEIGHT-1:clause->part2.bin->clweight);
		if (frstselect[kk][ii]==clause){
			if (clause->part2.bin->nextselect[kk]!=NULL){
				jj=(clause->part2.bin->nextselect[kk]->part2.bin->clweight>(MAXWEIGHT-2)?
						MAXWEIGHT-1:clause->part2.bin->nextselect[kk]->part2.bin->clweight);
				if (ii==jj){
					frstselect[kk][ii]=clause->part2.bin->nextselect[kk];
				} else {
					frstselect[kk][ii]=NULL;
				}
			} else {
				frstselect[kk][ii]=NULL;
			}
		}
		if (lstselect[kk][ii]==clause){
			if (clause->part2.bin->prevselect[kk]!=NULL){
				jj=(clause->part2.bin->prevselect[kk]->part2.bin->clweight>(MAXWEIGHT-2)?
						MAXWEIGHT-1:clause->part2.bin->prevselect[kk]->part2.bin->clweight);
				if (ii==jj){
					lstselect[kk][ii]=clause->part2.bin->prevselect[kk];
				} else {
					lstselect[kk][ii]=NULL;
				}
			} else {
				lstselect[kk][ii]=NULL;
			}
		}
		if (clause->part2.bin->nextselect[kk]!=NULL){
			clause->part2.bin->nextselect[kk]->part2.bin->prevselect[kk]=clause->part2.bin->prevselect[kk];
		} else {
			lstglselect[kk]=clause->part2.bin->prevselect[kk];
		}
		if (clause->part2.bin->prevselect[kk]!=NULL){
			clause->part2.bin->prevselect[kk]->part2.bin->nextselect[kk]=clause->part2.bin->nextselect[kk];
		} else {
			frstglselect[kk]=clause->part2.bin->nextselect[kk];
		}
	}

	/* Unlink clause from age queues. */
	qq*=2;
	for (;mm<qq;mm++){
		kk=queueidx[mm];
		ii=(clause->part2.bin->agedist>(MAXWEIGHT-2)?MAXWEIGHT-1:clause->part2.bin->agedist);
		if (frstselect[kk][ii]==clause){
			if (clause->part2.bin->nextselect[kk]!=NULL){
				jj=(clause->part2.bin->nextselect[kk]->part2.bin->agedist>(MAXWEIGHT-2)?
						MAXWEIGHT-1:clause->part2.bin->nextselect[kk]->part2.bin->agedist);
				if (ii==jj){
					frstselect[kk][ii]=clause->part2.bin->nextselect[kk];
				} else {
					frstselect[kk][ii]=NULL;
				}
			} else {
				frstselect[kk][ii]=NULL;
			}
		}
		if (lstselect[kk][ii]==clause){
			if (clause->part2.bin->prevselect[kk]!=NULL){
				jj=(clause->part2.bin->prevselect[kk]->part2.bin->agedist>(MAXWEIGHT-2)?
						MAXWEIGHT-1:clause->part2.bin->prevselect[kk]->part2.bin->agedist);
				if (ii==jj){
					lstselect[kk][ii]=clause->part2.bin->prevselect[kk];
				} else {
					lstselect[kk][ii]=NULL;
				}
			} else {
				lstselect[kk][ii]=NULL;
			}
		}
		if (clause->part2.bin->nextselect[kk]!=NULL){
			clause->part2.bin->nextselect[kk]->part2.bin->prevselect[kk]=clause->part2.bin->prevselect[kk];
		} else {
			lstglselect[kk]=clause->part2.bin->prevselect[kk];
		}
		if (clause->part2.bin->prevselect[kk]!=NULL){
			clause->part2.bin->prevselect[kk]->part2.bin->nextselect[kk]=clause->part2.bin->nextselect[kk];
		} else {
			frstglselect[kk]=clause->part2.bin->nextselect[kk];
		}
	}

	return;
} /* UnlinkSelectQueues */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Unlink binary clause from KB)  OCJ
 *
 *    This function unlinks a clause in binary form from all
 *    queues of a KB, with the exception of the disposal queue.
 *    This function doesn't remove the clause from the disposal
 *    queue. In fact it is assumed that the clause is not linked
 *    to the disposal queue.
 *
 *    If the clause is ACTIVE or PASSIVE then it is also removed
 *    from discrimination trees. In this case it is assumed that
 *    the clause is in the working KB but this is not verified.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    pkb: Pointer to KB to which the clause belongs.
 *
 *  RETURNS:
 *
 *    0 if successful, NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t Unlink(cmprefix *clause,kbase *pkb){

	/* Remove clause from discrimination trees if it is ACTIVE or */
	/* it is PASSIVE. This is always done for ACTIVE clauses, but */
	/* for PASSIVE clauses it is only done for KB's running OTTER */
	/* algorithm. */
	if ((clause->flags&ACTIVE)||((clause->flags&PASSIVE)
			&&(pkb->opts.algorithm==OTTER))){
		if (NOMEMORY==DelDiscrTrees(clause)){
			return(NOMEMORY);
		}
	}

	/* Unchain clause assertions. */
	UnchainAsserts(clause->part2.bin->ovly.asserts);

	/* Unlink formula. */
	switch (clause->flags&(ACTIVE|PASSIVE|UNPROC|LOCKED)){

		/* Unprocessed clause. Unlink clause from UNPROC queue. */
		case UNPROC:
			if (pkb->lstunproc==clause){
				pkb->lstunproc=clause->prev;
			}
			if (pkb->frstunproc==clause){
				pkb->frstunproc=clause->next;
			}
			pkb->prstats.nbunproc--;
			break;

		/* Locked clause. Unlink clause from LOCKED queue. */
		case LOCKED:
			if (pkb->lstlocked==clause){
				pkb->lstlocked=clause->prev;
			}
			if (pkb->frstlocked==clause){
				pkb->frstlocked=clause->next;
			}
			break;

		/* ACTIVE clause. Unlink clause from active queue */
		/* and update number of active clauses in KB. */
		case ACTIVE:
			if (pkb->lstactive==clause){
				pkb->lstactive=clause->prev;
			}
			if (pkb->frstactive==clause){
				pkb->frstactive=clause->next;
			}
			pkb->prstats.nbactive--;
			break;

		/* PASSIVE clause. */
		case PASSIVE:

			/* Unlink clause from passive queue. */
			if (pkb->lstpassive==clause){
				pkb->lstpassive=clause->prev;
			}
			if (pkb->frstpassive==clause){
				pkb->frstpassive=clause->next;
			}

			/* Unlink clause from clause selection queue. */
			UnlinkSelectQueues(clause);

			/* Update number of passive clauses in KB. */
			pkb->prstats.nbpassive--;
			break;
	}

	/* Common unlink tasks. */
	if (clause->prev!=NULL){
		clause->prev->next=clause->next;
	}
	if (clause->next!=NULL){
		clause->next->prev=clause->prev;
	}

	return(0);
} /* Unlink */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Unlink binary clause from KB for unit clause)  OCJ
 *
 *    This function unlinks a clause in binary form from all
 *    queues of a KB, with the exception of the disposal queue,
 *    for type 8 problems (unit equality).
 *    This function doesn't remove the clause from the disposal
 *    queue. In fact it is assumed that the clause is not linked
 *    to the disposal queue.
 *
 *    If the clause is ACTIVE or PASSIVE then it is also removed
 *    from discrimination trees. In this case it is assumed that
 *    the clause is in the working KB but this is not verified.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    pkb: Pointer to KB to which the clause belongs.
 *
 *  RETURNS:
 *
 *    0 if successful, NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t UnlinkUEQ(cmprefix *clause,kbase *pkb){

	/* Remove clause from discrimination trees if it is ACTIVE or */
	/* it is PASSIVE. This is always done for ACTIVE clauses, but */
	/* for PASSIVE clauses it is only done for KB's running OTTER */
	/* algorithm. */
	if ((clause->flags&ACTIVE)||((clause->flags&PASSIVE)
			&&(pkb->opts.algorithm==OTTER))){
		if (NOMEMORY==DelDiscrTrees(clause)){
			return(NOMEMORY);
		}
	}

	/* Unlink formula. */
	switch (clause->flags&(ACTIVE|PASSIVE|UNPROC|LOCKED)){

		/* Unprocessed clause. Unlink clause from UNPROC queue. */
		case UNPROC:
			if (pkb->lstunproc==clause){
				pkb->lstunproc=clause->prev;
			}
			if (pkb->frstunproc==clause){
				pkb->frstunproc=clause->next;
			}
			pkb->prstats.nbunproc--;
			break;

		/* ACTIVE clause. Unlink clause from active queue */
		/* and update number of active clauses in KB. */
		case ACTIVE:
			if (pkb->lstactive==clause){
				pkb->lstactive=clause->prev;
			}
			if (pkb->frstactive==clause){
				pkb->frstactive=clause->next;
			}
			pkb->prstats.nbactive--;
			break;

		/* PASSIVE clause. */
		case PASSIVE:

			/* Unlink clause from passive queue. */
			if (pkb->lstpassive==clause){
				pkb->lstpassive=clause->prev;
			}
			if (pkb->frstpassive==clause){
				pkb->frstpassive=clause->next;
			}

			/* Unlink clause from clause selection queue. */
			UnlinkSelectQueues(clause);

			/* Update number of passive clauses in KB. */
			pkb->prstats.nbpassive--;
			break;
	}

	/* Common unlink tasks. */
	if (clause->prev!=NULL){
		clause->prev->next=clause->next;
	}
	if (clause->next!=NULL){
		clause->next->prev=clause->prev;
	}

	return(0);
} /* UnlinkUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Unlink binary clause from KB for unfailing completion)  OCJ
 *
 *    This function is similar to the Unlink() function but for
 *    completion without failure strategies (algorithm set to UEQDSC
 *    or UEQOTT).
 *
 *    The clause must be either PASSIVE or ACTIVE and must be in
 *    the working KB.
 *
 *    If the clause is ACTIVE then it is also removed from discrimination
 *    trees.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *
 *  RETURNS:
 *
 *    0 if successful, NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t UEQUnlink(cmprefix *clause){

	/* Remove clause from discrimination trees if it is ACTIVE */
	/* or the algorithm is UEQOTT and clause is PASSIVE. */
	if ((clause->flags&ACTIVE)||((kbset.opts.algorithm==UEQOTT)&&(clause->flags&PASSIVE))){
		if (NOMEMORY==EditUEQTrees(clause,DELETETREES)){
			return(NOMEMORY);
		}
	}

	/* Unlink formula. */
	switch (clause->flags&(ACTIVE|PASSIVE|UNPROC)){

		/* Unprocessed clause. Unlink clause from UNPROC queue. */
		case UNPROC:
			if (kbset.lstunproc==clause){
				kbset.lstunproc=clause->prev;
			}
			if (kbset.frstunproc==clause){
				kbset.frstunproc=clause->next;
			}
			kbset.prstats.nbunproc--;
			break;

		/* ACTIVE clause. Unlink clause from active queue */
		/* and update number of active clauses in KB. */
		case ACTIVE:
			if (kbset.lstactive==clause){
				kbset.lstactive=clause->prev;
			}
			if (kbset.frstactive==clause){
				kbset.frstactive=clause->next;
			}
			kbset.prstats.nbactive--;
			break;

		/* PASSIVE clause. */
		case PASSIVE:

			/* Unlink clause from passive queue. */
			if (kbset.lstpassive==clause){
				kbset.lstpassive=clause->prev;
			}
			if (kbset.frstpassive==clause){
				kbset.frstpassive=clause->next;
			}

			/* Unlink clause from clause selection queue. */
			UnlinkSelectQueues(clause);

			/* Update number of passive clauses in KB. */
			kbset.prstats.nbpassive--;
			break;
	}

	/* Common unlink tasks. */
	if (clause->prev!=NULL){
		clause->prev->next=clause->next;
	}
	if (clause->next!=NULL){
		clause->next->prev=clause->prev;
	}

	return(0);
} /* UnlinkUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Unlink text formula from KB)  OCJ
 *
 *    This function unlinks a text formula from a KB. It is assumed
 *    that the formula is in text format but this is not verified.
 *    If it is not in text format then unpredictable results will
 *    be produced.
 *
 *    This function is mainly used by UEQDscnt() saturation algorithm
 *    function.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to formula in text format.
 *    pkb: Pointer to KB to which the clause belongs.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void UnlinkTxtFormula(cmprefix *formula,kbase *pkb){

	/* Unlink formula. */
	if (pkb->lasttxt==formula){
		pkb->lasttxt=formula->prev;
	}
	if (pkb->firsttxt==formula){
		pkb->firsttxt=formula->next;
	}
	if (formula->prev!=NULL){
		formula->prev->next=formula->next;
	}
	if (formula->next!=NULL){
		formula->next->prev=formula->prev;
	}
	return;
} /* UnlinkTxtFormula */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Free KB tree memory)  OCJ
 *
 *    This function frees all memory allocated for the trees of
 *    the working KB.
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
void FreeTreeMemory(void){
	auto hashchain *ptr3;                           /* Auxiliary pointer */
	auto int32_t ii;                                /* Auxiliary */

	/* Free tree memory. */
	for (ptr3=kb_hash;ptr3!=NULL;ptr3=ptr3->nextelem){
		for (ii=0;ii<DSCTREETYPES;ii++){
			FreeDiscrBranch(&ptr3->discr[ii],ptr3->arity);
		}
	}
	for (ii=0;ii<DSCTREETYPES;ii++){
		FreeDiscrBranch(&equkey.discr[ii],equkey.arity);
	}

	return;
} /* FreeTreeMemory */

#ifdef SEMANTICTAUTOLOGY
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Free semantic tautologies tree memory)  OCJ
 *
 *    This function is similar to FreeTreeMemory() but for
 *    semantic tautology detection discrimination trees./
 *
 *
 *  ARGUMENTS:
 *
 *    varleafs: Pointer to set of initial leafs for each skolemized
 *              variable
 *    maxvarnb: Maximum variable number. The dimension of varleafs
 *              set is maxvarnb+1.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void FreeTtlgTreeMemory(ttlgdstrleaf **varleafs,int32_t maxvarnb){
	auto ttlgdstrleaf *leaf,*leaf2;                 /* Pointers to leafs */
	auto hashchain *ptr3;                           /* Auxiliary pointer */
	auto int32_t ii;                                /* Auxiliary */

	/* Free skolemized variables leafs memory. */
	for (ii=0;ii<=maxvarnb;ii++){
		if (varleafs[ii]!=NULL){
			for (leaf=varleafs[ii];leaf!=NULL;leaf=leaf2){
				leaf2=leaf->peer;
				MYFREE(leaf);
			}
		}
	}
	MYFREE(varleafs);

	/* Free tree memory. */
	for (ptr3=kb_hash;ptr3!=NULL;ptr3=ptr3->nextelem){
		if (ptr3->posdsctrttlg!=NULL){
			FreeTtlgDiscrBranch(&ptr3->posdsctrttlg,ptr3->arity);
		}
		if (ptr3->negdsctrttlg!=NULL){
			FreeTtlgDiscrBranch(&ptr3->negdsctrttlg,ptr3->arity);
		}
	}
	if (equkey.posdsctrttlg!=NULL){
		FreeTtlgDiscrBranch(&equkey.posdsctrttlg,2);
	}
	FreeTtlgDiscrBranch(&equkey.negdsctrttlg,2);

	return;
} /* FreeTtlgTreeMemory */
#endif

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Calculate 32 bits hash value for a clause key)  OCJ
 *
 *    This function calculates a couple of hash values for a clause
 *    formula.
 *
 *    The hash value is guaranteed to be the same for variants of the
 *    same clause as defined in GetSplitSymbol() function.
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to component binary formula.
 *
 *  RETURNS:
 *
 *    32 bits hash value.
 *
 *--------------------------------------------------------------*/
uint32_t HashClause(uint8_t *formula){
	auto uint32_t hash;                                  /* 32 bits hash value for the key clause */
	auto uint8_t *ptr1,*ptr2,*ptr3;                      /* Auxiliary pointers */
	auto uint32_t ii;                                    /* Auxiliary */
	auto int32_t jj;                                     /* Auxiliary */

	/* Generate hash values for each literal in clause. */
	/* These values are added together to generate the */
	/* clause hash values. */
	/* Loop through literals. */
	hash=0;
	for (ptr1=formula;(*ptr1)!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS)){

		/* Literal is an equality or inequality. */
		if (ptr1[0]&EQUALITY){

			/* Add the equality hash value for the equality itself. */
			ii=((symbol *)&ptr1[1])->symbol->hash;
			if (ptr1[0]&NEGATED){
				ii^=0xe6546b64;
			}
			hash+=(ii^hashpos[0]);

			/* Calculate hash value for first equality root term. This hash value is */
			/* calculated by xoring the hash value of each item with the hash value */
			/* corresponding to the item position in the root term and adding the results. */
			/* Loop through first equality root term items. */
			for (ptr3=NextItem(ptr2=NextItem(ptr1,IMMED),OVERSUBTERMS),jj=0;
					ptr2<ptr3;ptr2=NextItem(ptr2,IMMED),jj++){
				if (ptr2[0]&FUNCTION){
					ii=((symbol *)&ptr2[1])->symbol->hash;
				} else {
					ii=0xcc9e2d51;
				}
				hash+=(ii^hashpos[jj<MAXPOSITIONS?jj:MAXPOSITIONS-1]);
			}

			/* Calculate hash value for second equality root term. This hash value is */
			/* calculated by xoring the hash value of each item with the hash value */
			/* corresponding to the item position in the root term and adding the results. */
			/* Loop through second equality root term items. */
			for (ptr3=NextItem(ptr2=ptr3,OVERSUBTERMS),jj=0;
					ptr2<ptr3;ptr2=NextItem(ptr2,IMMED),jj++){
				if (ptr2[0]&FUNCTION){
					ii=((symbol *)&ptr2[1])->symbol->hash;
				} else {
					ii=0xcc9e2d51;
				}
				hash+=(ii^hashpos[jj<MAXPOSITIONS?jj:MAXPOSITIONS-1]);
			}

		/* Literal is a predicate. */
		/* Calculate hash value for literal. This hash value is calculated */
		/* by xoring the hash value of each item with the hash value corresponding */
		/* to the item position in the literal and adding the results. */
		/* Loop through literal items. */
		} else {
			for (ptr2=ptr1,ptr3=NextItem(ptr1,OVERSUBTERMS),jj=0;ptr2<ptr3;ptr2=NextItem(ptr2,IMMED),jj++){
				if (ptr2[0]&(PREDICATE|EQUALITY|FUNCTION)){
					ii=((symbol *)&ptr2[1])->symbol->hash;
				} else {
					ii=0xcc9e2d51;
				}
				if (ptr2[0]&NEGATED){
					ii^=0xe6546b64;
				}
				hash+=(ii^hashpos[jj<MAXPOSITIONS?jj:MAXPOSITIONS-1]);
			}
		}
	}

	return(hash);
} /* HashClause */

#ifdef DEBUGTREE
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Discrimination trees integrity check)  OCJ
 *
 *    This function traverses all discrimination trees of all symbols
 *    up to all leafs and checks the following:
 *    - The number of leafs must be equal to the corresponding numleafs[]
 *      symbol field.
 *    - The item pointed by the leaf must be found by calls to QueryDiscTree()
 *      function with types GENERALIZATION and INSTANCE.
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
void CheckDscTrees(void){
	auto hashchain *symbol;                         /* Pointer to symbol hashchain structure */
	auto dstrnode *node;                            /* Pointer to tree node */
	auto dstrleaf *leaf;                            /* Pointer to tree leaf */
	auto uint32_t numleafs;                         /* Number of leafs */
	auto char *buffer;                              /* Buffer for decompiled item */
	auto int32_t size;                              /* Size of buffer in bytes */
	auto uint8_t *ptr1;                             /* Auxiliary pointer */
	auto int32_t ii,jj,kk,mm;                       /* Auxiliary */

	/* Allocate memory for decompiled item. */
	/* Initial buffer allocation. */
	if (NULL==(buffer=MYALLOC(size=STR_CHUNK_SIZE))){
		printf("====> Not enough memory for discrimination trees checking\n");
		return;
	}

	/* Loop through symbols and equality. */
	for (symbol=kb_hash,kk=0;symbol!=NULL;
			symbol=(symbol==&equkey?NULL:(symbol->nextelem!=NULL?symbol->nextelem:&equkey))){

		/* Loop through symbol discrimination trees. */
		for (numleafs=ii=0;(ii<DSCTREETYPES)&&(symbol->discr[ii]!=NULL);ii++,numleafs=0){

			/* Loop through tree leafs. */
			if (symbol->arity>0){
				node=(dstrnode *)symbol->discr[ii];
				leaf=NULL;
			} else {
				leaf=(dstrleaf *)symbol->discr[ii];
				node=NULL;
			}
			while (1){

				/* We are in a node. Traverse tree to the leafs. */
				if (node!=NULL){
					for (;node->childtype==NODECHILD;node=node->child.childnode){
					}
					leaf=node->child.childleaf;
					node=NULL;

				/* We are in a leaf.. */
				} else {

					/* Loop through the leafs at this level. */
					while (1){
						numleafs++;

						/* Check queries. */
						mm=0;
						while (1){
							if (0==QueryDscTree(leaf->item,&ptr1,symbol->discr[ii],INSTANCE,mm)){
								kk=1;
								jj=buffer[0]=0;
								if (NOMEMORY==AddTermText(leaf->item,&buffer,&size,&jj)){
									MYFREE(buffer);
									printf("====> Not enough memory for discrimination trees checking\n");
									return;
								}
								printf("====> Instance query failed for term %s\n",buffer);
								break;
							}
							if (leaf->item==ptr1){
								break;
							}
							mm=1;
						}
						mm=0;
						while (1){
							if (0==QueryDscTree(leaf->item,&ptr1,symbol->discr[ii],GENERALIZATION,mm)){
								kk=1;
								jj=buffer[0]=0;
								if (NOMEMORY==AddTermText(leaf->item,&buffer,&size,&jj)){
									MYFREE(buffer);
									printf("====> Not enough memory for discrimination trees checking\n");
									return;
								}
								printf("====> Generalization query failed for term %s\n",buffer);
								break;
							}
							if (leaf->item==ptr1){
								break;
							}
							mm=1;
						}

						/* Prepare next iteration. */
						if (leaf->peer==NULL){
							break;
						}
						leaf=leaf->peer;
					}

					/* Backtrack to next traversing point. */
					for (node=leaf->parent;(node!=NULL)&&(node->nxtpeer==NULL);node=node->parent){
					}

					/* Leave the leafs loop if there are no more leafs. */
					if (node==NULL){
						break;
					}
					node=node->nxtpeer;
					leaf=NULL;
				}
			}

			/* Check number of leafs. */
			if (numleafs!=symbol->numleafs[ii]){
				kk=1;
				printf("====> Number of leafs mismatch for tree %d, symbol %s\n",ii,
						symbol==&equkey?"equality":((char *)symbol)+(sizeof(hashchain)));
			}
		}
	}

	/* Report no errors. */
	if (kk==0){
		printf("====> No errors found in discrimination trees check\n");
	}

	/* Free memory and return. */
	MYFREE(buffer);
	return;
} /* CheckDscTrees */
#else

#ifdef DEBUGTREE2
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Discrimination trees queries check)  OCJ
 *
 *    This function check discrimination trees queries for debug purposes.
 *    The performed steps are:
 *    - Add the first NBOFTESTS literals that are not (in)equalities found
 *      in the main KB binary unprocessed clauses to discr[0] discrimination
 *      trees.
 *    - For each (in)equality literal found perform UNIFICATION, INSTANCE
 *      and GENERALIZATION discrimination tree queries and report the results.
 *    - Free the allocated discrimination tree memory.
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
void CheckDscTQueries(void){
	auto cmprefix *clause;                          /* Auxiliary pointer */
	auto uint8_t *cliteral;                         /* Pointer to first literal in clause */
	auto uint8_t *qliteral;                         /* Pointer to found literal */
	auto char *buffer;                              /* Buffer for decompiled literal */
	auto int32_t size;                              /* Size of buffer in bytes */
	auto float timeout;                             /* To save global timeout */
	auto int32_t ii,jj,kk,mm,nn;                    /* Auxiliary */

	/* Main KB is empty. */
	if (mainkb.nformulas==0){
		printf("Main KB has no input formulas to process.\n");
		return;
	}

	/* Start time measurement. */
	timeout=glblopts.timeout;
	glblopts.timeout=9999999;
	procctl->start=clock();
	gettimeofday(&mainkb.time,NULL);

	/* Perform pre-processing. */
	ii=Preprocess(0);
	glblopts.timeout=timeout;
	if (ii){

		/* Print error information. */
		if (ii==NOMEMORY){
			printf("====> Not enough memory for discrimination trees checking\n");
		} else {
			printf("Process timeout\n");
		}
		FreeInfFormulas();
		if (NOMEMORY==Initialize_KB(&mainkb)){
			printf("No memory available, KB not initialized\n");
		}
		glblstats.elapsed_time+=mainkb.prstats.elapsed_time;
		ResetStats(pbmstats);
		procctl->status=STOPPED;

		/* Stop time measurement, collect CPU time and return. */
		procctl->end=clock();
		tot_cpu_time+=((double)(procctl->end-procctl->start))/CLOCKS_PER_SEC;
		return;
	}

	/* Allocate memory for decompiled literal. */
	/* Initial buffer allocation. */
	if (NULL==(buffer=MYALLOC(size=STR_CHUNK_SIZE))){
		printf("====> Not enough memory for discrimination trees checking\n");
		return;
	}

	/* Add first literal of unprocessed clauses to discr[0] discrimination tree. */
	for (clause=mainkb.frstunproc,ii=0;(clause!=NULL)&&(ii<NBOFTESTS);clause=clause->next){
		for (cliteral=&clause->part2.bin->formula[0];cliteral[0]!=UNITEND;cliteral=NextItem(cliteral,OVERSUBTERMS)){
			if (PREDICATE&cliteral[0]){
				ii++;
				if (0!=DsTermIndexing(cliteral,&((symbol *)&cliteral[1])->symbol->discr[0])){
					printf("====> Not enough memory. Discrimination tree query check not performed.\n");
					return;
				}
			}
		}
	}

	/* Loop through first literal of first NBOFTESTS binary unprocessed clauses. */
	for (clause=mainkb.frstunproc,ii=0;(clause!=NULL)&&(ii<NBOFTESTS);clause=clause->next){
		for (cliteral=&clause->part2.bin->formula[0];cliteral[0]!=UNITEND;cliteral=NextItem(cliteral,OVERSUBTERMS)){
			if (PREDICATE&cliteral[0]){

				/* Print literal information. */
				ii++;
				mm=buffer[0]=0;
				if (NOMEMORY==AddTermText(cliteral,&buffer,&size,&mm)){
					MYFREE(buffer);
					printf("====> Not enough memory. Discrimination tree query check not completed.\n");
					return;
				}
				printf("====> Checking queries for literal: %s\n",buffer);

				/* Perform INSTANCE queries. */
				jj=kk=nn=0;
				do {
					if (0!=(jj=QueryDscTree(cliteral,&qliteral,((symbol *)&cliteral[1])->symbol->discr[0],INSTANCE,nn))){
						mm=buffer[0]=0;
						if (NOMEMORY==AddTermText(qliteral,&buffer,&size,&mm)){
							MYFREE(buffer);
							printf("====> Not enough memory. Discrimination tree query check not completed.\n");
							return;
						}
						printf("  ====> INSTANCE found: %s\n",buffer);
						kk=nn=1;
					}
				} while (jj);

				/* Perform GENERALIZATION queries. */
				jj=nn=0;
				do {
					if (0!=(jj=QueryDscTree(cliteral,&qliteral,((symbol *)&cliteral[1])->symbol->discr[0],GENERALIZATION,nn))){
						mm=buffer[0]=0;
						if (NOMEMORY==AddTermText(qliteral,&buffer,&size,&mm)){
							MYFREE(buffer);
							printf("====> Not enough memory. Discrimination tree query check not completed.\n");
							return;
						}
						printf("  ====> GENERALIZATION found: %s\n",buffer);
						kk=nn=1;
					}
				} while (jj);

				/* Perform UNIFICATION queries. */
				jj=nn=0;
				do {
					if (0!=(jj=QueryDscTree(cliteral,&qliteral,((symbol *)&cliteral[1])->symbol->discr[0],UNIFICATION,nn))){
						mm=buffer[0]=0;
						if (NOMEMORY==AddTermText(qliteral,&buffer,&size,&mm)){
							MYFREE(buffer);
							printf("====> Not enough memory. Discrimination tree query check not completed.\n");
							return;
						}
						printf("  ====> UNIFICATION found: %s\n",buffer);
						kk=nn=1;
					}
				} while (jj);

				/* No matches found for this literal. */
				if (kk==0){
					printf("  ====> No matches found.\n");
				}
			}
		}
	}

	/* Free memory and return. */
	MYFREE(buffer);
	FreeInfFormulas();
	if (NOMEMORY==Initialize_KB(&mainkb)){
		printf("No memory available, KB not initialized\n");
	}
	glblstats.elapsed_time+=mainkb.prstats.elapsed_time;
	ResetStats(pbmstats);
	procctl->status=STOPPED;

	/* Stop time measurement, collect CPU time and return. */
	procctl->end=clock();
	tot_cpu_time+=((double)(procctl->end-procctl->start))/CLOCKS_PER_SEC;
	return;
} /* CheckDscTQueries */
#endif
#endif
