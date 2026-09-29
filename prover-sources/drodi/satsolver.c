/*
 ============================================================================
 Name        : satsolver.c
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
 *    This source contains the SAT and asserts related functions for Drodi
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

/* Specific includes. */
#include "perfmon.h"

/* Inline functions specific for this module. */
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Remove a symbol from eligible linked list)  OCJ
 *
 *    This function removes a symbol from eligible list.
 *    It is assumed that the symbol is correctly linked to
 *    the list.
 *
 *
 *  ARGUMENTS:
 *
 *    symbol: Pointer to symbol to be removed from eligible list.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
static inline void RemoveFromEligible(subslitrl *symbol){
	if (symbol->nextelig==NULL){
		if (symbol->prevelig==NULL){
			subsctrl.frsteliglit=NULL;
		} else {
			symbol->prevelig->nextelig=NULL;
			symbol->prevelig=NULL;
		}
	} else {
		if (symbol->prevelig==NULL){
			subsctrl.frsteliglit=symbol->nextelig;
			symbol->nextelig->prevelig=NULL;
			symbol->nextelig=NULL;
		} else {
			symbol->prevelig->nextelig=symbol->nextelig;
			symbol->nextelig->prevelig=symbol->prevelig;
			symbol->prevelig=NULL;
			symbol->nextelig=NULL;
		}
	}
	return;
} /* RemoveFromEligible */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform symbol assignment tasks for subsumption
 *                SAT solver)  OCJ
 *
 *    This function performs all the tasks needed for a subsumption
 *    SAT solver symbol assignment, including adjusting SAT clause
 *    parameters and unlinking and chaining SAT clause for clauses
 *    affected by symbol assignment.
 *
 *    IMPORTANT:
 *    - This function must be called only for TRUE (POSITIVE) symbol
 *      assignments and will setup the node for current the decision
 *      level. FALSE (NEGATIVE) symbol assignments are not performed
 *      by this function and may be of two types:
 *      1.- FALSE assignments due to AMO (At Most One) or substitution
 *          incompatibility. For these assignments a node will not be
 *          created.
 *      2.- FALSE assignments as an alternative to a failed TRUE
 *          assignment. For these assignments a node will be created
 *          and will be performed only by the SbBacktrack() function.
 *
 *
 *  ARGUMENTS:
 *
 *    node: Pointer to node corresponding to symbol assignment.
 *
 *  RETURNS:
 *
 *    0 -> There was no contradiction.
 *    1 -> There was a contradiction.
 *
 *--------------------------------------------------------------*/
static inline int32_t SbDoSymbolAssgnmnt(subsnode *node){
	auto subslitrl *ptr1;                                /* Auxiliary pointer */
	auto uint8_t *ptr2;                                  /* Auxiliary pointer */
	auto int32_t ii,jj,kk;                               /* Auxiliary */

	/* Assign node component to TRUE, remove node symbol from the eligible list, */
	/* remove the SAT solver clause from the ordered list and do miscellaneous */
	/* initializations. */
	node->symbol->flags&=~(NEGATIVE|UNDEFINED);
	node->symbol->flags|=POSITIVE;
	node->symbol->satcl->undeflits--;
	node->symbol->satcl->satlits=1;
	RemoveFromEligible(node->symbol);
	SbUnchainSatClause(node->symbol->satcl);
	node->frstpropg=node->frstpropg2=NULL;
	node->varnum=-1;

	/* Loop forward through symbols related to the same main */
	/* (candidate subsumed) clause literal as the node symbol. */
	for (ptr1=node->symbol->prevmain;ptr1!=NULL;ptr1=ptr1->prevmain){

		/* The symbol ptr1 is unassigned. */
		if (UNDEFINED&ptr1->flags){

			/* The symbol ptr1 is the only unassigned symbol in its SAT clause */
			/* and its SAT clause is not satisfied. Return contradiction. */
			if ((ptr1->satcl->undeflits==1)&&(ptr1->satcl->satlits==0)){
				return(1);
			}

			/* The symbol ptr1 is linked to the eligible list. Set it to FALSE, */
			/* link it to the main propagated symbols queue of node and remove it */
			/* from the eligible symbols list. */
			if ((ptr1->nextelig!=ptr1->prevelig)||(subsctrl.frsteliglit==ptr1)){
				ptr1->flags&=~(POSITIVE|UNDEFINED);
				ptr1->flags|=NEGATIVE;
				ptr1->satcl->undeflits--;
				if (ptr1->satcl->undeflits<3){
					SbUnchainSatClause(ptr1->satcl);
					SbChainSatClause(ptr1->satcl);
				}
				ptr1->nextpropg=node->frstpropg;
				node->frstpropg=ptr1;
				RemoveFromEligible(ptr1);
			}
		}
	}

	/* Loop backward through symbols related to the same main */
	/* (candidate subsumed) clause literal as the node symbol. */
	for (ptr1=node->symbol->nextmain;ptr1!=NULL;ptr1=ptr1->nextmain){

		/* The symbol ptr1 is unassigned. */
		if (UNDEFINED&ptr1->flags){

			/* The symbol ptr1 is the only unassigned symbol in its SAT clause */
			/* and its SAT clause is not satisfied. Return contradiction. */
			if ((ptr1->satcl->undeflits==1)&&(ptr1->satcl->satlits==0)){
				return(1);
			}

			/* The symbol ptr1 is linked to the eligible list. Set it to FALSE, */
			/* link it to the main propagated symbols queue of node and remove it */
			/* from the eligible symbols list. */
			if ((ptr1->nextelig!=ptr1->prevelig)||(subsctrl.frsteliglit==ptr1)){
				ptr1->flags&=~(POSITIVE|UNDEFINED);
				ptr1->flags|=NEGATIVE;
				ptr1->satcl->undeflits--;
				if (ptr1->satcl->undeflits<3){
					SbUnchainSatClause(ptr1->satcl);
					SbChainSatClause(ptr1->satcl);
				}
				ptr1->nextpropg=node->frstpropg;
				node->frstpropg=ptr1;
				RemoveFromEligible(ptr1);
			}
		}
	}

	/* Remove all unassigned  symbols in the symbol SAT clause from the eligible list */
	/* and chain them to the main propagated symbol queue except node symbol itself. */
	for (ii=0;ii<node->symbol->satcl->literals;ii++){
		ptr1=&node->symbol->satcl->formula[ii];
		if (ptr1->flags&UNDEFINED){
			RemoveFromEligible(ptr1);
			if (ptr1!=node->symbol){
				ptr1->nextpropg=node->frstpropg;
				node->frstpropg=ptr1;
			}
		}
	}

	/* Select the symbol substitution that will be added to the global substitution. */
	if (0==((SUBST1OFF|SECONDTRUEASSGN)&node->symbol->flags)){
		jj=0;
	} else {
		jj=1;
	}

	/* If the selected symbol substitution is not empty then loop through variables */
	/* in the symbol substitution in normal format. */
	if ((node->symbol->psubst[jj]!=NULL)&&(node->symbol->psubst[jj]->buffer!=NULL)){
		for (ptr2=node->symbol->psubst[jj]->buffer;ptr2[0]!=UNITEND;ptr2=NextItem(&ptr2[3],OVERSUBTERMS)){

			/* Variable not included in the global substitution. */
			ii=*((int16_t *)&ptr2[1]);
			if (subsctrl.glblextsubst[ii]==NULL){

				/* Loop through symbols that have this variable in their substitution. */
				for (ptr1=subsctrl.firstvar[ii];ptr1!=NULL;ptr1=ptr1->varnext[ii]){

					/* Symbol ptr1 is in the eligible list and is not in the same clause than node->symbol. */
					if (((ptr1->nextelig!=ptr1->prevelig)||(subsctrl.frsteliglit==ptr1))&&(node->symbol->satcl!=ptr1->satcl)){

						/* Check compatibility of each symbol substitutions with global substitution. */
						/* Set kk to the updated combination of symbol flags with SUBST1OFF and SUBST2OFF. */
						kk=ptr1->flags&(SUBST1OFF|SUBST2OFF);
						if ((0==(kk&SUBST1OFF))&&(0==IsEqual(&ptr2[3],ptr1->psubst2[0][ii],0))){
							kk|=SUBST1OFF;
						}
						if ((ptr1->psubst2[1][ii]==NULL)||((0==(kk&SUBST2OFF))&&(0==IsEqual(&ptr2[3],ptr1->psubst2[1][ii],0)))){
							kk|=SUBST2OFF;
						}

						/* Symbol ptr1 is not in the second propagation linked list (flags SUBST1OFF and SUBST2OFF */
						/* are off). */
						if (0==((SUBST1OFF|SUBST2OFF)&ptr1->flags)){

							/* Both substitutions are incompatible or the symbol has only one substitution */
							/* and it is incompatible. Add symbol to the main propagation list and unlink it */
							/* from the eligible list. */
							if (kk==(SUBST1OFF|SUBST2OFF)){

								/* The SAT clause of component ptr1 is not satisfied and has */
								/* only one unassigned component (which must be ptr1). */
								/* There is a contradiction. */
								if ((ptr1->satcl->satlits==0)&&(ptr1->satcl->undeflits==1)){
									node->varnum=ii;
									return(1);
								}

								/* Set ptr1 to FALSE, link it to the main propagated symbols queue */
								/* of node and remove it from the eligible symbols list. */
								ptr1->flags&=~(POSITIVE|UNDEFINED);
								ptr1->flags|=NEGATIVE;
								ptr1->satcl->undeflits--;
								ptr1->nextpropg=node->frstpropg;
								node->frstpropg=ptr1;
								RemoveFromEligible(ptr1);

								/* Set the ptr1 flags to indicate that both symbol substitutions */
								/* have been disabled. */
								ptr1->flags|=(SUBST1OFF|SUBST2OFF|BYSUBST1|BYSUBST2);

								/* If ptr1 SAT solver clause has 2 or less unassigned literals */
								/* then re-chain the clause to the ordered queue. */
								if (ptr1->satcl->undeflits<3){
									SbUnchainSatClause(ptr1->satcl);
									SbChainSatClause(ptr1->satcl);
								}

							/* The symbol has two substitutions and one of them has become incompatible. */
							/* Add symbol to the second propagation list and set the flag SUBST1OFF */
							/* or SUBST2OFF as appropriate. */
							} else if ((kk!=0)&&(0==(ptr1->flags&(SUBST1OFF|SUBST2OFF)))&&(ptr1->psubst2[1][ii]!=NULL)){
								ptr1->nextpropg2=node->frstpropg2;
								node->frstpropg2=ptr1;
								ptr1->flags|=kk;
							}

						/* Symbol ptr1 is in the second propagation linked list and the until now compatible */
						/* substitution has become incompatible. */
						} else if (kk==(SUBST1OFF|SUBST2OFF)){

							/* The SAT clause of component ptr1 is not satisfied and has */
							/* only one unassigned component (which must be ptr1). */
							/* There is a contradiction. */
							if ((ptr1->satcl->satlits==0)&&(ptr1->satcl->undeflits==1)){
								node->varnum=ii;
								return(1);
							}

							/* Set ptr1 to FALSE, link it to the main propagated symbols queue */
							/* of node and remove it from the eligible symbols list. */
							ptr1->flags&=~(POSITIVE|UNDEFINED);
							ptr1->flags|=NEGATIVE;
							ptr1->satcl->undeflits--;
							ptr1->nextpropg=node->frstpropg;
							node->frstpropg=ptr1;
							RemoveFromEligible(ptr1);

							/* Set the ptr1 flags to indicate that one of the symbol substitutions */
							/* have been disabled. */
							switch (ptr1->flags&(SUBST1OFF|SUBST2OFF)){
								case SUBST1OFF:
									ptr1->flags|=(SUBST2OFF|BYSUBST2);
									break;
								case SUBST2OFF:
								default:
									ptr1->flags|=(SUBST1OFF|BYSUBST1);
									break;
							}

							/* If ptr1 SAT solver clause has 2 or less unassigned literals */
							/* then re-chain the clause to the ordered queue. */
							if (ptr1->satcl->undeflits<3){
								SbUnchainSatClause(ptr1->satcl);
								SbChainSatClause(ptr1->satcl);
							}
						}
					}
				}

				/* Add the substitution of the variable to the global substitution. */
				subsctrl.glblextsubst[ii]=&ptr2[3];
				subsctrl.glblvarcount[ii]=1;

			/* Variable included in the global substitution. Increase the count of that variable */
			/* in the global substitution. */
			} else {
				subsctrl.glblvarcount[ii]++;
			}
		}

		/* Set varnum field of node to maximum. */
		node->varnum=subsctrl.sidenumvars;
	}

	/* Return no contradiction. */
	return(0);
} /* SbDoSymbolAssgnmnt */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Backtrack from a subsumption SAT solver contradiction)  OCJ
 *
 *    This function identifies a node to which backtracking must be done
 *    after a contradiction due to a TRUE (POSITIVE) assignment is found
 *    by the subsumption SAT solver and backtracks to that node if necessary.
 *
 *    This algorithm is based on the Chaff algorithm, however it is
 *    not a strict implementation of it. See "Validating SAT Solvers
 *    Using an Independent Resolution-Based Checker: Practical
 *    Implementations and Other Applications" by Lintao Zhang and
 *    Sharad Malik.
 *
 *  ARGUMENTS:
 *
 *    pnode: As input this is the address of a pointer to the node in
 *           SAT solver search tree that produced the contradiction.
 *           As output this is the addres of the pointer to the node
 *           in which backtracking stopped. The backtracking includes
 *           the undo of the value assigned by the node. In case of
 *           SAT refutation this is NULL.
 *
 *  RETURNS:
 *
 *    0 if no SAT refutation was found.
 *    1 if a SAT refutation was found.
 *
 *--------------------------------------------------------------*/
static inline int32_t SbBacktrack(subsnode **pnode){
	auto subsnode *node;                                 /* Pointer to node */

	/* Main backtrack loop. */
	node=*pnode;
	do {

		/* The node symbol was set to TRUE (POSITIVE). */
		if (node->symbol->flags&POSITIVE){

			/* Undo symbol assignment. */
			SbUndoSymbolAssgnmnt(node);

			/* The symbol node->symbol has two valid substitutions and its flag field */
			/* doesn't have any of the flags SUBST1OFF, SUBST2OFF and SECONDTRUEASSGN */
			/* so it can be assigned true a second time, this time adding its second */
			/* substitution to the global substitution. */
			if ((node->symbol->psubst[1]!=NULL)&&(0==(node->symbol->flags&(SUBST1OFF|SUBST2OFF|SECONDTRUEASSGN)))){

				/* Chain node->symbol clause to the ordered queue */
				/* and set SECONDTRUEASSGN and POSITIVE flags. */
				SbChainSatClause(node->symbol->satcl);
				node->symbol->flags&=~(UNDEFINED|NEGATIVE);
				node->symbol->flags|=(POSITIVE|SECONDTRUEASSGN);

				/* Do symbol second assignment to true. */
				/* If second true assignment produces a contradiction then */
				/* continue backtracking. */
				if (SbDoSymbolAssgnmnt(node)){
					continue;

				/* If second true assignment doesn't produces a contradiction then */
				/* return no SAT refutation. */
				} else {
					*pnode=node;
					return(0); /* Return no SAT refutation. */
				}

			/* The symbol node->symbol is not eligible for a second true assignment. */
			/* Disable SECONDTRUEASSGN flag. */
			} else {
				node->symbol->flags&=~SECONDTRUEASSGN;
			}

			/* Node symbol clause has 2 or more unassigned literals. Set node symbol */
			/* to FALSE (NEGATIVE), remove node symbol from eligible list, add node */
			/* symbol clause to the ordered list and return that no refutation was found. */
			if (node->symbol->satcl->undeflits>=2){
				node->symbol->flags&=~(POSITIVE|UNDEFINED);
				node->symbol->flags|=NEGATIVE;
				node->symbol->satcl->undeflits--;
				RemoveFromEligible(node->symbol);
				SbChainSatClause(node->symbol->satcl);
				*pnode=node;
				return(0);

			/* Node symbol clause has only 1 unassigned literal which is precisely node->symbol */
			/* so it is already linked to eligible list. Link node symbol clause to the ordered */
			/* list and backtrack. */
			} else {
				SbChainSatClause(node->symbol->satcl);
				node=node->prevnode;
			}

		/* The node symbol was set to FALSE (NEGATIVE). Set node symbol to unassigned, */
		/* link it to eligible list, chain clause to clause ordered list if appropriate */
		/* and backtrack. */
		} else {
			node->symbol->flags&=~(POSITIVE|NEGATIVE);
			node->symbol->flags|=UNDEFINED;
			node->symbol->satcl->undeflits++;
			node->symbol->nextelig=subsctrl.frsteliglit;
			node->symbol->prevelig=NULL;
			if (subsctrl.frsteliglit!=NULL){
				subsctrl.frsteliglit->prevelig=node->symbol;
			}
			subsctrl.frsteliglit=node->symbol;
			if (node->symbol->satcl->undeflits==1){
				SbChainSatClause(node->symbol->satcl);
			}
			node=node->prevnode;
		}
	} while (node!=NULL);

	return(1);
} /* SbBacktrack */

/** Global variables for this module: only those that need initialization in their definition. */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Get maximal split of an A-clause)  OCJ
 *
 *    This function performs a maximal split in components for a
 *    given A-clause. If the clause is successfully split then
 *    it is converted to text and the resulting component A-clause
 *    from the split is added to the SAT queue with SPLIT inference
 *    type. The A-clause must be unlinked from KB.
 *
 *    To be more specific, let D <- A an A-clause. This A-clause can
 *    be split if D can be split or if its only maximal component C
 *    has already an existing component name [C] and it is not a
 *    component clause (of the form C <- [C]) if some additional
 *    conditions are met (see below). A case of splitting a clause
 *    with only one component can be seen in figure 7clause 21 of
 *    document "Better Proof Output for Vampire" by Giles Reger. There
 *    is an exception to this in the case that flag parameter equals 1,
 *    see explanation of flag parameter.
 *
 *    Then if D=C1|...|Cn with n>=1 the clause corresponding to
 *    [C1]|...|[Cn] <- A is built and added to the SAT queue with
 *    SPLIT inference type.
 *
 *    The SAT solver clause format is as follows:
 *
 *      literal_1 literal_2 ... literal_n UNITEND
 *
 *    where:
 *
 *      literal_i: Clause literal.
 *      UNITEND: Byte marker indicating the end of a clause (see define in
 *               global.h).
 *
 *    SAT solver literal format:
 *    ------------------------
 *    The format for SAT solver literals is:
 *
 *        signbyte literal_symbol
 *
 *    where:
 *
 *      signbyte: Byte with values POSITIVE (the literal is an atom) or NEGATIVE (the
 *                literal is a negated atom).
 *      literal_symbol: asymbol structure.
 *
 *    The possible additional conditions for splitting single component
 *    clauses involve the existence of a matching symbol (hashchcmp
 *    structure), existence of assertions, matching polarity of the
 *    corresponding component and being the component false in the model.
 *    The following combinations have been tried:
 *    - A matching symbol already exist. This gives wrong results. See
 *      problems ALG112+1, ALG115+1, ALG116+1, ALG103+1, ALG127+1 and ALG128+1
 *      in AAADebug.txt file.
 *    - A matching symbol already exist and the clause has assertions. This
 *      options works better and is currently being used but still produces
 *      wrong results, for instance same as problem ALG128+1 but with Otter
 *      algorithm.
 *    - A matching symbol already exist, the clause has assertions and the
 *      polarity of the corresponding component matches the polarity of the
 *      clause component. This option also produces wrong results, for
 *      instance problem ALG128+1 as in AAADebug.txt file.
 *    - A matching symbol already exist, the clause has assertions, the
 *      polarity of the corresponding component matches the polarity of the
 *      clause component and the component is false in the model. This option
 *      produces good results in tests performed so far and it is the option
 *      currently in use
 *    Another tested option is same as the last previous combination but
 *    allowing also the component to be undefined in the model. By the moment
 *    the results are the same as with the last previous option.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause to be maximally split in components.
 *    flag: If flag=1 then perform splitting even if the clause must not
 *          be split. This is the case for calling SAT solver alone
 *          without coupling it to the first order prover. If flag=0
 *          then normal splitting rules as outlined in the comments
 *          above will be followed. No other values are allowed.
 *
 *  RETURNS:
 *
 *    0 if clause was not split.
 *    1 if clause was split.
 *    NOMEMORY if not enough memory.
 *
 *
 *
 *--------------------------------------------------------------*/
int32_t SplitClause(cmprefix *clause,int32_t flag){
	auto int32_t *literals;                              /* Pointer to array with one value per variable */
	auto int8_t *registered;                             /* Pointer to array with one value per variable */
	auto int32_t *components;                            /* Pointer to array with one value per literal */
	auto int32_t first;                                  /* Current specific value of literals[] */
	auto int32_t comp;                                   /* Specific value of components[first] */
	auto hashchcmp *ppred;                               /* Symbol hashchcmp structure of P-predicate. */
	auto satclause *satcl;                               /* Pointer to SAT clause structure. */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5;          /* Auxiliary pointers */
	auto char *ptr10;                                    /* Pointer to buffer for clause in text form. */
	auto int32_t ii,jj,kk,mm,nn,pp,qq,tt,ss;             /* Auxiliary */

	#ifdef DEBUGCODE
	/* Debug. */
	ptr10=Decompile(clause->part2.bin);
	MYFREE(ptr10);
	#endif

	/* Return if it is a component clause or split is not enabled and flag is zero. */
	if ((clause->inference==COMPONENT)||((kbset.opts.split==0)&&(flag==0))){
		return(0);
	}

	/* The clause has only one literal. */
	if (1==clause->part2.bin->literals){
		jj=1;
		components=NULL;

	/* The clause has more than one literal. */
	} else {

		/* Allocate and initialize memory for literals, components and registered buffers. */
		if (NULL==(components=MYALLOC(clause->part2.bin->literals*sizeof(*components)))){
			return(NOMEMORY);
		}
		if (clause->part2.bin->maxvarnb>=0){
			if (NULL==(literals=MYALLOC((1+clause->part2.bin->maxvarnb)*sizeof(*literals)))){
				MYFREE(components);
				return(NOMEMORY);
			}
			if (NULL==(registered=MYALLOC((1+clause->part2.bin->maxvarnb)*sizeof(*registered)))){
				MYFREE(components);
				MYFREE(literals);
				return(NOMEMORY);
			}
			memset(registered,0,(1+clause->part2.bin->maxvarnb)*sizeof(*registered));
		} else {
			literals=NULL;
			registered=NULL;
		}

		/* First phase for finding maximal split: Loop through literals. */
		for (ptr1=&clause->part2.bin->formula[0],ii=kk=0;ptr1[0]!=UNITEND;ptr1=ptr3,ii++){
			ptr3=NextItem(ptr1,OVERSUBTERMS);

			/* initialize components for this literal. */
			components[ii]=ii;

			/* Loop through variables. */
			if (registered!=NULL){
				for (ptr2=NextItem(ptr1,IMMED);ptr2<ptr3;ptr2=NextItem(ptr2,IMMED)){
					if (ptr2[0]&VARIABLE){

						/* Variable has been occupied earlier. */
						jj=*((int16_t *)&ptr2[1]);
						if (registered[jj]){

							/* Linking loop for components. */
							first=literals[jj];
							while ((components[first]!=first)&&(components[first]!=ii)){
								comp=components[first];
								components[first]=ii;
								first=comp;
							}
							components[first]=ii;

						/* First occurrence of variable. */
						} else {
							literals[jj]=ii;
							registered[jj]=1;
						}
					}
				}
			}
		}

		/* Second phase for finding maximal split: Loop backwards through literals. */
		for (ii=clause->part2.bin->literals-1,jj=0;ii>=0;ii--){
			if (components[ii]>=0){
				if (components[ii]==ii){
					jj++;
				} else {
					components[ii]=components[components[ii]];
				}
			}
		}
	}

	/* The A-clause has only one maximal component. */
	if (jj==1){

		/* Free split auxiliary memory. */
		if (components!=NULL){
			MYFREE(components);
			if (literals!=NULL){
				MYFREE(literals);
				MYFREE(registered);
			}
		}

		/* Return clause not splitable if it has no assertions and flag is zero. */
		if ((clause->part2.bin->ovly.asserts==NULL)&&(flag==0)){
			return(0);
		}

		/* Allocate storage for the hashchcmp element formula. */
		if (NULL==(ptr4=MYALLOC(clause->part2.bin->size))){
			return(NOMEMORY);
		}
		memcpy(ptr4,&clause->part2.bin->formula[0],clause->part2.bin->size);

		/* Get a P-predicate or check if the component has a name */
		/* already assigned, depending on the value of flag. */
		if (NOMEMORY==(ss=GetSplitSymbol(ptr4,clause->part2.bin->size,clause->part2.bin->literals,&ppred,flag))){
			MYFREE(ptr4);
			return(NOMEMORY);
		}

		/* Free hashchcmp element formula if no longer needed. */
		if ((ss==0)||(ss==1)||(ss==4)){
			MYFREE(ptr4);
		}

		/* Initialize sinedist and agedist fields of the P-predicate if applicable. */
		if ((ss==2)||(ss==3)){
			ppred->agedist=clause->part2.bin->agedist;
			ppred->sinedist=clause->part2.bin->sinedist;
		}

		/* The flag is not zero or clause component has a P-predicate name assigned that */
		/* matches the existing component polarity. Build the split clause and return. */
		if ((ppred!=NULL)&&(((ss==0)&&(POSCOMPADDED&ppred->flags)&&(MODELNEGATIVE&ppred->flags))
				||((ss==1)&&(NEGCOMPADDED&ppred->flags)&&(MODELPOSITIVE&ppred->flags))||(flag!=0))){

			/* Allocate storage for the split clause. */
			if (clause->part2.bin->ovly.asserts!=NULL){
				if (NULL==(satcl=MYALLOC(satclsize+sizeof(asymbol)+clause->part2.bin->asize+1))){
					return(NOMEMORY);
				}
			} else {
				if (NULL==(satcl=MYALLOC(satclsize+sizeof(asymbol)+2))){
					return(NOMEMORY);
				}
			}

			/* Chain the split clause to the global SAT queue. */
			satcl->glblnext=NULL;
			if (kbset.lastsatq!=NULL){
				kbset.lastsatq->glblnext=satcl;
			} else {
				kbset.firstsatq=satcl;
			}
			satcl->glblprev=kbset.lastsatq;
			kbset.lastsatq=satcl;

			/* Build the split clause. */
			satcl->formula[0]=(ss&1?NEGATIVE:POSITIVE);
			((asymbol *)&satcl->formula[1])->symbol=ppred;
			((asymbol *)&satcl->formula[1])->part2.satcl=satcl;
			if (clause->part2.bin->ovly.asserts!=NULL){
				memcpy(&satcl->formula[1+sizeof(asymbol)],clause->part2.bin->ovly.asserts,clause->part2.bin->asize);
				for (ptr1=&satcl->formula[1+sizeof(asymbol)],ii=1;ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol)),ii++){
					ptr1[0]^=(POSITIVE|NEGATIVE);
					((asymbol *)&ptr1[1])->part2.satcl=satcl;
				}
			} else {
				ii=1;
				satcl->formula[1+sizeof(asymbol)]=UNITEND;
			}
			satcl->literals=ii;
			kbset.prstats.splits++;
			satcl->parent=clause;
			satcl->inference=SPLIT;

			/* Convert input clause to text. */
			if (NULL==(ptr10=Decompile(clause->part2.bin))){
				return(NOMEMORY);
			}
			if (clause->part2.bin->ovly.asserts!=NULL){
				MYFREE(clause->part2.bin->ovly.asserts);
			}
			MYFREE(clause->part2.bin);
			clause->part2.text=ptr10;
			AddTxtFormula2KB(clause,clause->inference,&kbset);

			/* Assign a number to the split clause. */
			kbset.nformulas++;
			satcl->number=kbset.nformulas;

			/* Debug. */
			#if defined(VERBOSE)&&defined(DEBUGCODE)
			printf("Clause split:\n%ld. %s\n",satcl->number,clause->part2.text);
			#endif
			return(1);

		/* The component has not a component name assigned. */
		/* The clause cannot be split. */
		} else {
			return(0);
		}
	}

	/* Allocate storage for the split clause. */
	if (NULL==(satcl=MYALLOC(satclsize+(jj*(sizeof(asymbol)+1))+(clause->part2.bin->asize==0?1:clause->part2.bin->asize)))){
		MYFREE(components);
		if (literals!=NULL){
			MYFREE(literals);
			MYFREE(registered);
		}
		return(NOMEMORY);
	}

	/* Build split clause. Loop through input clause literals. */
	/* pp = first free byte in formula. */
	pp=0;
	for (ptr1=&clause->part2.bin->formula[0],ii=0;ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){

		/* Literal is part of a component not yet added to split clause. */
		if (components[ii]>=0){

			/* Allocate storage for buffer with clause component. */
			if (NULL==(ptr4=MYALLOC(clause->part2.bin->size))){
				MYFREE(components);
				if (literals!=NULL){
					MYFREE(literals);
					MYFREE(registered);
				}
				return(NOMEMORY);
			}

			/* Loop through literals starting at current literal. */
			/* qq = first free byte in formula of component clause. */
			qq=0;
			for(ptr2=ptr1,kk=components[ii],mm=ii,tt=0;ptr2[0]!=UNITEND;ptr2=ptr3,mm++){
				ptr3=NextItem(ptr2,OVERSUBTERMS);

				/* Literal belongs to the same component. */
				if (components[mm]==kk){
					components[mm]=-2;
					nn=ptr3-ptr2;
					memcpy(&ptr4[qq],ptr2,nn);
					qq+=nn;
					tt++;
				}
			}

			/* Mark end of component clause and resize buffer to exact size. */
			ptr4[qq]=UNITEND;
			qq++;
			if (NULL==(ptr5=MYREALLOC(ptr4,qq))){
				MYFREE(ptr4);
				MYFREE(components);
				if (literals!=NULL){
					MYFREE(literals);
					MYFREE(registered);
				}
				return(NOMEMORY);
			}
			ptr4=ptr5;

			/* Get a P-predicate. */
			UpdateParams2(ptr4);
			if (NOMEMORY==(ss=GetSplitSymbol(ptr4,qq,tt,&ppred,1))){
				MYFREE(components);
				if (literals!=NULL){
					MYFREE(literals);
					MYFREE(registered);
				}
				MYFREE(ptr4);
				return(NOMEMORY);
			}

			/* Free hashchchm element formula if no longer needed. */
			if ((ss==0)||(ss==1)||(ss==4)){
				MYFREE(ptr4);
			}

			/* Initialize sinedist and agedist fields of the P-predicate if applicable. */
			if ((ss==2)||(ss==3)){
				ppred->agedist=clause->part2.bin->agedist;
				ppred->sinedist=clause->part2.bin->sinedist;
			}

			/* Add component item to split clause. */
			satcl->formula[pp]=(ss&1?NEGATIVE:POSITIVE);
			((asymbol *)&satcl->formula[pp+1])->symbol=ppred;
			((asymbol *)&satcl->formula[pp+1])->part2.satcl=satcl;
			pp+=(1+sizeof(asymbol));
		}
	}

	/* Free split auxiliary memory. */
	MYFREE(components);
	if (literals!=NULL){
		MYFREE(literals);
		MYFREE(registered);
	}

	/* Chain the split clause to the global SAT queue. */
	satcl->glblnext=NULL;
	if (kbset.lastsatq!=NULL){
		kbset.lastsatq->glblnext=satcl;
	} else {
		kbset.firstsatq=satcl;
	}
	satcl->glblprev=kbset.lastsatq;
	kbset.lastsatq=satcl;

	/* Complete the build process of the split clause. */
	if (clause->part2.bin->ovly.asserts!=NULL){
		memcpy(&satcl->formula[pp],clause->part2.bin->ovly.asserts,clause->part2.bin->asize);
		for (ptr1=&satcl->formula[pp],ii=jj;ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol)),ii++){
			ptr1[0]^=(POSITIVE|NEGATIVE);
			((asymbol *)&ptr1[1])->part2.satcl=satcl;
		}
	} else {
		ii=jj;
		satcl->formula[pp]=UNITEND;
	}
	satcl->literals=ii;
	kbset.prstats.splits++;
	satcl->parent=clause;
	satcl->inference=SPLIT;

	/* Convert input clause to text. */
	if (NULL==(ptr10=Decompile(clause->part2.bin))){
		return(NOMEMORY);
	}
	if (clause->part2.bin->ovly.asserts!=NULL){
		MYFREE(clause->part2.bin->ovly.asserts);
	}
	MYFREE(clause->part2.bin);
	clause->part2.text=ptr10;
	AddTxtFormula2KB(clause,clause->inference,&kbset);

	/* Assign a number to the split clause. */
	kbset.nformulas++;
	satcl->number=kbset.nformulas;

	/* Debug. */
	#if defined(VERBOSE)&&defined(DEBUGCODE)
	printf("Clause split:\n%ld. %s\n",satcl->number,clause->part2.text);
	#endif

	return(1);
} /* SplitClause */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add assertions from two clauses to a third clause)  OCJ
 *
 *    This function adds the assertions of two A-clauses to a third
 *    A-clause removing duplicate assertions. The asize field of
 *    destination A-clause binprefix is set. The locking assertions
 *    of the third A-clause are set to NULL.
 *
 *    It is assumed that the third clause has no assertions. This
 *    assumption is not checked.
 *
 *    The assertions set format is as follows:
 *
 *      assertion_1 assertion_2 ... assertion_n UNITEND
 *
 *    where:
 *
 *      assertion_i: One item for each assertion literal.
 *      UNITEND: Byte marker indicating the end of a clause (see define in
 *               global.h).
 *
 *    Assertion literal format:
 *    ------------------------
 *    The format for assertion literals is:
 *
 *        signbyte assert_symbol
 *
 *    where:
 *
 *      signbyte: Byte with values POSITIVE (the component corresponding to assertion_data
 *                is true in SAT interpretation model) or NEGATIVE (the component
 *                corresponding to assertion_data is false in SAT interpretation model).
 *      assert_symbol: asymbol structure.
 *
 *
 *  ARGUMENTS:
 *
 *    clausefr1: Pointer to binprefix structure of first "from" A-clause.
 *    clausefr2: Pointer to binprefix structure of second "from" A-clause.
 *               This pointer may be NULL.
 *    clauseto: Pointer to binprefix structure of "to" A-clause.
 *
 *  RETURNS:
 *
 *    0 -> Assertion addition was performed successfully.
 *    NOMEMORY -> Not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t AddAssertions(binprefix *clausefr1,binprefix *clausefr2,binprefix *clauseto){
	auto uint8_t *ptr1,*ptr2;                            /* Auxiliary pointers */
	auto int32_t ii,jj;                                  /* Auxiliary */

	/* Set clauseto lockasserts. */
	clauseto->lockasserts=NULL;
	clauseto->lasize=0;

	/* Second "from" clause is NULL. */
	if (clausefr2==NULL){
		if (clausefr1->ovly.asserts!=NULL){
			if (NULL==(clauseto->ovly.asserts=MYALLOC(clausefr1->asize))){
				return(NOMEMORY);
			}
			memcpy(&clauseto->ovly.asserts[0],clausefr1->ovly.asserts,clausefr1->asize);
			clauseto->asize=clausefr1->asize;
			for (ptr1=&clauseto->ovly.asserts[0];ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol))){
				((asymbol *)&ptr1[1])->part2.binp=clauseto;
			}
		} else {
			clauseto->ovly.asserts=NULL;
			clauseto->asize=0;
		}
		return(0);
	}

	/* There are no assertions to add. */
	if ((clausefr1->ovly.asserts==NULL)&&(clausefr2->ovly.asserts==NULL)){
		clauseto->ovly.asserts=NULL;
		clauseto->asize=0;
		return(0);
	}

	/* Allocate destination assertion buffer. */
	jj=(clausefr1->ovly.asserts!=NULL?clausefr1->asize:1)+(clausefr2->ovly.asserts!=NULL?clausefr2->asize:1)-1;
	if (NULL==(clauseto->ovly.asserts=MYALLOC(jj))){
		return(NOMEMORY);
	}

	/* First "from" clause has no assertions. */
	if (clausefr1->ovly.asserts==NULL){
		memcpy(&clauseto->ovly.asserts[0],clausefr2->ovly.asserts,clausefr2->asize);
		clauseto->asize=clausefr2->asize;
		for (ptr1=&clauseto->ovly.asserts[0];ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol))){
			((asymbol *)&ptr1[1])->part2.binp=clauseto;
		}
		return(0);

	/* Second "from" clause has no assertions. */
	} else if (clausefr2->ovly.asserts==NULL){
		memcpy(&clauseto->ovly.asserts[0],clausefr1->ovly.asserts,clausefr1->asize);
		clauseto->asize=clausefr1->asize;
		for (ptr1=&clauseto->ovly.asserts[0];ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol))){
			((asymbol *)&ptr1[1])->part2.binp=clauseto;
		}
		return(0);
	}

	/* Loop through assertions in first "from" clause. */
	clauseto->asize=0;
	for (ptr1=clausefr1->ovly.asserts;ptr1[0]!=UNITEND;ptr1+=1+sizeof(asymbol)){

		/* Loop through assertions in second "from" clause. */
		for (ptr2=clausefr2->ovly.asserts,ii=1;ptr2[0]!=UNITEND;ptr2+=1+sizeof(asymbol)){

			/* Assertion symbols match. Indicate duplicate assertion and leave the inner loop. */
			if (((asymbol *)&ptr1[1])->symbol==((asymbol *)&ptr2[1])->symbol){
				ii=0;
				break;
			}
		}

		/* Add assertion if it is not a duplicate. */
		if (ii){
			memcpy(&clauseto->ovly.asserts[clauseto->asize],ptr1,1+sizeof(asymbol));
			clauseto->asize+=1+sizeof(asymbol);
		}
	}

	/* Add assertions of second "from" clause including the ending UNTEND */
	/* byte, reallocate buffer to exact size and return. */
	memcpy(&clauseto->ovly.asserts[clauseto->asize],clausefr2->ovly.asserts,clausefr2->asize);
	clauseto->asize+=clausefr2->asize;
	if (clauseto->asize!=jj){
		if (NULL==(ptr1=MYREALLOC(clauseto->ovly.asserts,clauseto->asize))){
			MYFREE(clauseto->ovly.asserts);
			clauseto->ovly.asserts=NULL;
			clauseto->asize=0;
			return(NOMEMORY);
		}
		clauseto->ovly.asserts=ptr1;
	}
	for (ptr1=&clauseto->ovly.asserts[0];ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol))){
		((asymbol *)&ptr1[1])->part2.binp=clauseto;
	}

	return(0);
} /* AddAssertions */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Get simplification assertions and locking assertions)  OCJ
 *
 *    This function gets the locking assertions after the simplification
 *    of clauseto A-clause by clausefr A-clause. The locking assertions
 *    are the assertions of clausefr that are not assertions of claseto.
 *
 *    If there are locking assertions then they are added to clauseto
 *    binprefix and the clause is marked as LOCKED.
 *
 *    This function also gets the assertions to be assigned to the
 *    simplified clausesmp A-clause if clausesmp is not NULL. The
 *    assertions in the new assertion list are linked to the appropriate
 *    (positive or negative) first order prover assertions queue.
 *
 *
 *  ARGUMENTS:
 *
 *    clausefr: Pointer to binprefix structure of simplifying A-clause.
 *    clauseto: Pointer to binprefix structure A-clause to be simplified
 *              by clausefr.
 *    clausesmp: Pointer to binprefix structure of the simplified A-Clause.
 *               If NULL then the assertions of the simplified A-Clause
 *               are not computed.
 *
 *  RETURNS:
 *
 *    0 -> Assertion addition was performed successfully.
 *    NOEMORY -> Not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t GetLocksAsserts(binprefix *clausefr,binprefix *clauseto,binprefix *clausesmp){
	auto uint8_t *ptr1,*ptr2;                            /* Auxiliary pointers */
	auto int32_t ii,jj;                                  /* Auxiliary */

	/* Set clausesmp lockasserts. */
	if (clausesmp!=NULL){
		clausesmp->lockasserts=NULL;
		clausesmp->lasize=0;
	}

	/* The clause clausefr has assertions. */
	if (clausefr->ovly.asserts!=NULL){

		/* Allocate storage for clauseto locking assertions. */
		if (NULL==(clauseto->lockasserts=MYALLOC(clausefr->asize))){
			if (clausesmp!=NULL){
				clausesmp->ovly.asserts=NULL;
				clausesmp->asize=0;
			}
			return(NOMEMORY);
		}
		clauseto->lasize=0;

		/* The clause clauseto has no assertions. */
		if (clauseto->ovly.asserts==NULL){

			/* Get clauseto locking assertions and mark it as LOCKED. */
			memcpy(clauseto->lockasserts,clausefr->ovly.asserts,clausefr->asize);
			clauseto->lasize=clausefr->asize;
			clauseto->clause->flags|=LOCKED;

			/* Allocate storage for clausesmp assertions and get the assertions. */
			if (clausesmp!=NULL){
				if (NULL==(clausesmp->ovly.asserts=MYALLOC(clausefr->asize))){
					MYFREE(clauseto->lockasserts);
					clausesmp->ovly.asserts=clauseto->lockasserts=NULL;
					clausesmp->asize=clauseto->lasize=0;
					return(NOMEMORY);
				}
				memcpy(clausesmp->ovly.asserts,clausefr->ovly.asserts,clausefr->asize);
				clausesmp->asize=clausefr->asize;
			}

		/* The clause clauseto has assertions. */
		} else {

			/* Allocate storage for clausesmp assertions. */
			jj=0; /* Just to prevent compiler warnings. */
			if (clausesmp!=NULL){
				jj=clausefr->asize+clauseto->asize-1;
				if (NULL==(clausesmp->ovly.asserts=MYALLOC(jj))){
					MYFREE(clauseto->lockasserts);
					clausesmp->ovly.asserts=clauseto->lockasserts=NULL;
					clausesmp->asize=clauseto->lasize=0;
					return(NOMEMORY);
				}
				clausesmp->asize=0;
			}

			/* Loop through clausefr assertions. */
			for (ptr1=clausefr->ovly.asserts;ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol))){

				/* Loop through clauseto assertions. */
				for (ptr2=clauseto->ovly.asserts,ii=1;ptr2[0]!=UNITEND;ptr2+=(1+sizeof(asymbol))){

					/* If assertions are equal then indicate so and leave the inner loop. */
					if (((asymbol *)&ptr1[1])->symbol==((asymbol *)&ptr2[1])->symbol){
						ii=0;
						break;
					}
				}

				/* If the assertion of clausefr is not in assertions of clauseto */
				/* then add assertion to clauseto locking assertions and to clausesmp */
				/* assertions. */
				if (ii){
					memcpy(&clauseto->lockasserts[clauseto->lasize],ptr1,1+sizeof(asymbol));
					clauseto->lasize+=1+sizeof(asymbol);
					if (clausesmp!=NULL){
						memcpy(&clausesmp->ovly.asserts[clausesmp->asize],ptr1,1+sizeof(asymbol));
						clausesmp->asize+=1+sizeof(asymbol);
					}
				}
			}

			/* Add clauseto assertions to clausesmp, resize assertions buffer if needed. */
			if (clausesmp!=NULL){
				memcpy(&clausesmp->ovly.asserts[clausesmp->asize],clauseto->ovly.asserts,clauseto->asize);
				clausesmp->asize+=clauseto->asize;
				if (jj!=clausesmp->asize){
					if (NULL==(ptr1=MYREALLOC(clausesmp->ovly.asserts,clausesmp->asize))){
						MYFREE(clauseto->lockasserts);
						MYFREE(clausesmp->ovly.asserts);
						clausesmp->ovly.asserts=clauseto->lockasserts=NULL;
						clausesmp->asize=clauseto->lasize=0;
						return(NOMEMORY);
					}
					clausesmp->ovly.asserts=ptr1;
				}
			}

			/* There are no locking assertions. */
			if (clauseto->lasize==0){
				MYFREE(clauseto->lockasserts);
				clauseto->lockasserts=NULL;

			/* There are locking assertions. Mark the clause as LOCKED, put UNITEND */
			/* marker byte, resize locking assertions buffer if needed and set */
			/* asymbol offsets. */
			} else {
				clauseto->clause->flags|=LOCKED;
				clauseto->lockasserts[clauseto->lasize]=UNITEND;
				clauseto->lasize++;
				if (clauseto->lasize!=clausefr->asize){
					if (NULL==(ptr1=MYREALLOC(clauseto->lockasserts,clauseto->lasize))){
						MYFREE(clauseto->lockasserts);
						clauseto->lockasserts=NULL;
						clauseto->lasize=0;
						clauseto->clause->flags&=(~LOCKED);
						if (clausesmp!=NULL){
							MYFREE(clausesmp->ovly.asserts);
							clausesmp->ovly.asserts=NULL;
							clausesmp->asize=0;
						}
						return(NOMEMORY);
					}
					clauseto->lockasserts=ptr1;
				}
			}
		}

	/* The clause clausefr has no assertions. */
	} else {

		/* Set clauseto locking assertions. */
		clauseto->lockasserts=NULL;
		clauseto->lasize=0;

		/* Set clausesmp assertions. */
		if (clausesmp!=NULL){
			if (clauseto->ovly.asserts!=NULL){
				if (NULL==(clausesmp->ovly.asserts=MYALLOC(clauseto->asize))){
					return(NOMEMORY);
				}
				memcpy(clausesmp->ovly.asserts,clauseto->ovly.asserts,clauseto->asize);
				clausesmp->asize=clauseto->asize;
			} else {
				clausesmp->ovly.asserts=NULL;
				clausesmp->asize=0;
			}
		}
	}

	/* Set the part2 pointers of new assertions and locking assertions. */
	if (clauseto->lockasserts!=NULL){
		for (ptr1=&clauseto->lockasserts[0];ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol))){
			((asymbol *)&ptr1[1])->part2.binp=clauseto;
		}
	}
	if (clausesmp!=NULL){
		if (clausesmp->ovly.asserts!=NULL){
			for (ptr1=&clausesmp->ovly.asserts[0],ii=satclsize;ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol)),ii+=(1+sizeof(asymbol))){
				((asymbol *)&ptr1[1])->part2.binp=clausesmp;
			}
		}
	}

	return(0);
} /* GetLocksAsserts */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Chain assertions to appropriate queue)  OCJ
 *
 *    This function chains a set of assertions to the appropriate
 *    (positive or negative) first order solver queue.
 *
 *    In principle this function must only be called from
 *    AddBinClause2KB() function for ACTIVE and PASSIVE clauses.
 *
 *
 *  ARGUMENTS:
 *
 *    asserts: Set of assertions.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void ChainAsserts(uint8_t *asserts){
	auto uint8_t *ptr1;                                  /* Auxiliary pointers */

	/* There are no assertions to chain. */
	if (asserts==NULL){
		return;
	}

	/* Return if the asserts are already chained. */
	if (((asymbol *)&asserts[1])->part2.binp->clause->flags&ASSERTSCHAINED){
		return;
	}

	/* Loop through assertions and chain appropriately. */
	for (ptr1=asserts;ptr1[0]!=UNITEND;ptr1+=1+sizeof(asymbol)){
		((asymbol *)&ptr1[1])->prev=NULL;
		if (NEGATIVE&ptr1[0]){
			((asymbol *)&ptr1[1])->next=((asymbol *)&ptr1[1])->symbol->frstnegassrt;
			if (((asymbol *)&ptr1[1])->symbol->frstnegassrt!=NULL){
				((asymbol *)&ptr1[1])->symbol->frstnegassrt->prev=(asymbol *)&ptr1[1];
			}
			((asymbol *)&ptr1[1])->symbol->frstnegassrt=(asymbol *)&ptr1[1];
		} else {
			((asymbol *)&ptr1[1])->next=((asymbol *)&ptr1[1])->symbol->frstposassrt;
			if (((asymbol *)&ptr1[1])->symbol->frstposassrt!=NULL){
				((asymbol *)&ptr1[1])->symbol->frstposassrt->prev=(asymbol *)&ptr1[1];
			}
			((asymbol *)&ptr1[1])->symbol->frstposassrt=(asymbol *)&ptr1[1];
		}
	}

	/* Indicate that assertions are chained in clause flags and return. */
	((asymbol *)&asserts[1])->part2.binp->clause->flags|=ASSERTSCHAINED;
	return;
} /* ChainAsserts */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Chain locking assertions to appropriate queue)  OCJ
 *
 *    This function chains a set of locking assertions to the appropriate
 *    first order solver queue.
 *
 *    In principle this function must only be called from
 *    AddBinClause2KB() function for LOCKED clauses.
 *
 *
 *  ARGUMENTS:
 *
 *    lockasserts: Set of locking assertions.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void ChainLocks(uint8_t *lockasserts){
	auto uint8_t *ptr1;                                  /* Auxiliary pointers */

	/* There are no assertions to chain. */
	if (lockasserts==NULL){
		return;
	}

	/* Loop through assertions and chain appropriately. */
	for (ptr1=lockasserts;ptr1[0]!=UNITEND;ptr1+=1+sizeof(asymbol)){
		((asymbol *)&ptr1[1])->prev=NULL;
		((asymbol *)&ptr1[1])->next=((asymbol *)&ptr1[1])->symbol->firstlock;
		if (((asymbol *)&ptr1[1])->symbol->firstlock!=NULL){
			((asymbol *)&ptr1[1])->symbol->firstlock->prev=(asymbol *)&ptr1[1];
		}
		((asymbol *)&ptr1[1])->symbol->firstlock=(asymbol *)&ptr1[1];
	}

	return;
} /* ChainLocks */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Unchain assertions)  OCJ
 *
 *    This function unchains a set of assertions from the appropriate
 *    (positive or negative) first order solver queue.
 *
 *    This function should only be called from Unlink() function.
 *
 *
 *  ARGUMENTS:
 *
 *    asserts: Set of assertions.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void UnchainAsserts(uint8_t *asserts){
	auto uint8_t *ptr1;                                  /* Auxiliary pointers */

	/* There are no assertions to unchain. */
	if (asserts==NULL){
		return;
	}

	/* Return if the asserts are not chained. */
	if (0==(((asymbol *)&asserts[1])->part2.binp->clause->flags&ASSERTSCHAINED)){
		return;
	}

	/* Loop through assertions and unchain appropriately. */
	for (ptr1=asserts;ptr1[0]!=UNITEND;ptr1+=1+sizeof(asymbol)){
		if (((asymbol *)&ptr1[1])->prev!=NULL){
			((asymbol *)&ptr1[1])->prev->next=((asymbol *)&ptr1[1])->next;
		} else {
			if (NEGATIVE&ptr1[0]){
				((asymbol *)&ptr1[1])->symbol->frstnegassrt=((asymbol *)&ptr1[1])->next;
			} else {
				((asymbol *)&ptr1[1])->symbol->frstposassrt=((asymbol *)&ptr1[1])->next;
			}
		}
		if (((asymbol *)&ptr1[1])->next!=NULL){
			((asymbol *)&ptr1[1])->next->prev=((asymbol *)&ptr1[1])->prev;
		}
	}

	/* Indicate that assertions are not chained in clause flags and return. */
	((asymbol *)&asserts[1])->part2.binp->clause->flags&=(~ASSERTSCHAINED);
	return;
} /* UnchainAsserts */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Unchain locking assertions)  OCJ
 *
 *    This function unchains a set of locking assertions from the
 *    appropriate first order solver queue.
 *
 *    In principle this function must only be called when a locked clause
 *    is unlocked.
 *
 *
 *  ARGUMENTS:
 *
 *    lockasserts: Set of locking assertions.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void UnchainLocks(uint8_t *lockasserts){
	auto uint8_t *ptr1;                                  /* Auxiliary pointers */

	/* There are no assertions to chain. */
	if (lockasserts==NULL){
		return;
	}

	/* Loop through locking assertions and unchain appropriately. */
	for (ptr1=lockasserts;ptr1[0]!=UNITEND;ptr1+=1+sizeof(asymbol)){
		if (((asymbol *)&ptr1[1])->prev!=NULL){
			((asymbol *)&ptr1[1])->prev->next=((asymbol *)&ptr1[1])->next;
		} else {
			((asymbol *)&ptr1[1])->symbol->firstlock=((asymbol *)&ptr1[1])->next;
		}
		if (((asymbol *)&ptr1[1])->next!=NULL){
			((asymbol *)&ptr1[1])->next->prev=((asymbol *)&ptr1[1])->prev;
		}
	}

	return;
} /* UnchainLocks */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Get a P-predicate symbol for a clause)  OCJ
 *
 *    This function searchs for an existing hashchcmp structure
 *    associated to the P-predicate name that corresponds to
 *    the split of a specific clause variant. If a clause that is
 *    a variant of the formula passed as parameter already exist
 *    then its corresponding P-predicate name is reused. Otherwise
 *    a new P-predicate name is created. In this context "P-predicate"
 *    is synonym of "component name".
 *
 *    If a new P-predicate name is created then the given formula
 *    buffer is used for it, no new buffer is allocated. Otherwise
 *    it is the caller's responsibility to free the formula
 *    buffer storage.
 *
 *    For the purpose of this splitting a clause is a variant of
 *    another if there exist a substitution that is a bijective
 *    application from variables of one clause into variables of
 *    the other that transforms one clause into the other except
 *    maybe for the order of the literals.
 *
 *    To locate variants of the given formula a hashing algorithm
 *    is used. The hash algorithm takes the formula as key and creates
 *    the same hash value for the same variants according to the
 *    criteria explained above. Two 16 bits hash values are generated
 *    from a 32 bits hash value calculated using each new variant as
 *    key. The haschain elements corresponding to P-predicate name
 *    of variants with the same first hash value are chained together.
 *    The second hash value is stored in each hashchcmp element. This
 *    way it is possible to search for haschain elements with a specific
 *    couple of hash values. Then variant candidates are tested with
 *    the IsVariant() function.
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to component binary formula.
 *    size: Size of formula including the ending UNITEND byte.
 *    literals: The number of literals in formula.
 *    psymbol: Address of pointer to haschcmp element associated to
 *             the P-predicate name (also known as component name)
 *             that corresponds to clause where the created or
 *             existing component will be placed. If flag argument is
 *             zero and a variant of clause is not found then the pointer
 *             will be set to NULL.
 *    flag: If flag is zero then the function will only return an existing
 *          P-predicate in psymbol and if there is not an existing one it will
 *          return NULL in psymbol. If flag is not zero and there is not a
 *          P-predicate that is a variant of the given formula then a new
 *          P-predicate will be created.
 *
 *  RETURNS:
 *
 *    0 -> An existing P-predicate was found and psymbol corresponds to
 *         the existing P-predicate.
 *    1 -> An existing P-predicate was found and psymbol corresponds to
 *         the negation of existing P-predicate. This is only for ground
 *         formula.
 *    2 -> A new P-predicate was created and psymbol corresponds to
 *         the new P-predicate.
 *    3 -> A new P-predicate was created and psymbol corresponds to
 *         the negation of the new P-predicate. This is only for ground
 *         formula.
 *    4 -> No matching P-predicate found .
 *    NOMEMORY -> Not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t GetSplitSymbol(uint8_t *formula,int32_t size,int32_t literals,
		hashchcmp **psymbol,int32_t flag){
	auto uint32_t hash32;                           /* 32 bits hash value for the key clause */
	auto uint16_t hash16;                           /* 16 bits hash value for the key clause */
	auto hashchcmp *ptr1;                           /* Auxiliary pointer */
	auto uint8_t *ptr2;                             /* Auxiliary pointer */
	auto int32_t ii,jj;                             /* Auxiliary */

	/* Check if the formula is ground. Set ii=0 if not ground */
	/* or positive, ii=1 if it is ground and negative. */
	/* Set jj=0 if not ground or GROUND if ground. */
	/* If the formula is a negative ground literal then */
	/* convert it to positive. */
	if (literals==1){
		jj=GROUND;
		for (ptr2=NextItem(formula,IMMED),ii=1;ptr2[0]!=UNITEND;ptr2=NextItem(ptr2,IMMED)){
			if (ptr2[0]&VARIABLE){
				ii=jj=0;
				break;
			}
		}
		if (ii){
			if (formula[0]&NEGATED){
				formula[0]&=(~NEGATED);
			} else {
				ii=0;
			}
		}
	} else {
		ii=jj=0;
	}

	/* Calculate hash value for the clause and scan for a match. */
	hash32=HashClause(formula);
	hash16=hash32&0xffff;
	for (ptr1=hashsplit[hash16];ptr1!=NULL;ptr1=ptr1->nextsymbol){

		/* Check for a match. */
		if (hash32==ptr1->hash){
			switch (IsVariant(formula,literals,ptr1)){

				/* Not enough memory. */
				case NOMEMORY:
					return(NOMEMORY);
					break;

				/* Full match. Set psymbol and return. */
				case 1:
					*psymbol=ptr1;
					return(ii);
					break;
			}
		}
	}

	/* If we are here there is no match. If flag is zero */
	/* then set *psymbol to NULL and return. */
	if (flag==0){
		*psymbol=NULL;
		return(4);
	}

	/* Create a new P-predicate symbol. Allocate memory for hash symbol element. */
	if (NULL==(*psymbol=MYALLOC(sizeof(hashchcmp)))){
		return(NOMEMORY);
	}

	/* Chain the element to global queue */
	/* and chain the element hash queue.*/
	(*psymbol)->nextelem=kb_hashcmp;
	kb_hashcmp=*psymbol;
	(*psymbol)->nextsymbol=hashsplit[hash16];
	hashsplit[hash16]=*psymbol;
	(*psymbol)->hash=hash32;

	/* Set the fields of the hashchcmp structure fields. */
	(*psymbol)->frstposassrt=(*psymbol)->frstnegassrt=(*psymbol)->frstsat=
			(*psymbol)->firstlock=NULL;
	(*psymbol)->number=splid+kbset.splsymbols;
	kbset.splsymbols++;
	(*psymbol)->formula=formula;
	(*psymbol)->size=size;
	(*psymbol)->literals=literals;
	(*psymbol)->flags=jj|UNDEFINED|MODELUNDEFINED;
	(*psymbol)->npositive=(*psymbol)->nnegative=0;
	kbset.nformulas++;
	(*psymbol)->defnumber=kbset.nformulas;
	kbset.nformulas+=2;

	return(ii+2);
} /* GetSplitSymbol */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if clause is a variant of another clause)  OCJ
 *
 *    This function checks if a clause formula is a variant of the
 *    component clause in the formula of the component argument.
 *
 *    For the purpose of this function a clause is a variant of
 *    another if there exist a substitution that is a bijective
 *    application from variables of one clause into variables of
 *    the other that transforms one clause into the other except
 *    maybe for the order of the literals and/or symmetry of
 *    equality root terms.
 *
 *    This algorithm is potentially exponential in execution time. To avoid
 *    hang ups a timeout condition checking is introduced. In case of timeout
 *    it is assumed that the formula is not a variant.  This will affect a little
 *    bit the program performance but it will prevent hang ups and I think that
 *    it preserves completeness. This timeout condition must not be confused
 *    with the global process timeout condition.
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to component binary formula.
 *    literals: The number of literals in formula.
 *    component: Pointer to hashchcmp structure whose formula must be
 *               a variant of the clause argument.
 *
 *  RETURNS:
 *
 *    0 -> formula is not a clause variant of component formula
 *         or a function timeout condition occurred.
 *    1 -> formula is a clause variant of component formula.
 *    NOEMORY -> Not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t IsVariant(uint8_t *formula,int32_t literals,hashchcmp *component){
	auto subst Subst;                                    /* Substitution for calls to LiteralVariant() */
	auto int8_t *lits;                                   /* Pointer to literal assignment map. */
	struct timeval sttime;                               /* Function starting time */
	auto int32_t ii;                                     /* Auxiliary. */

	/* Return if number if not the same number of literals. */
	if (literals!=component->literals){
		return(0);
	}

	/* Initialize substitution for LiteralVariant() calls. */
	Subst.buffer=NULL;
	Subst.substsize=0;

	/* Allocate memory for literal assignment maps. */
	if (NULL==(lits=MYALLOC(literals))){
		return(NOMEMORY);
	}
	memset(lits,0,literals);

	/* Get start time. */
	gettimeofday(&sttime,NULL);

	/* Call LiteralVariant(), free memory and return. */
	ii=LiteralVariant(formula,component,lits,0,&Subst,&sttime);
	if (ii==TIMEOUT){
		ii=0;
	}
	MYFREE(lits);
	if (Subst.buffer!=NULL){
		MYFREE(Subst.buffer);
	}
	return(ii);
} /* IsVariant */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Literal variant check)  OCJ
 *
 *    This function checks if there is a valid corresponding literal in the
 *    formula of the component argument so that the formula is a variant of
 *    clause to which literal belongs.
 *
 *    This function is called recursively.
 *
 *
 *  ARGUMENTS:
 *
 *    literal: Pointer to literal of first clause.
 *    component: Pointer to hashchcmp structure whose formula must have a
 *               literal that is a variant of the literal argument.
 *    lits: Pointer to literal assignment map of second clause.
 *    litnb: Literal number of literal.
 *    Subst: Substitution currently active. This function can add atomic
 *           substitutions to the current substitution but cannot modify
 *           existing atomic substitutions.
 *    sttime: Structure with the function starting time to check function
 *            timeouts. See IsVariant() global comments for details.
 *
 *  RETURNS:
 *
 *    0 -> first clause is not a variant of second clause.
 *    1 -> first clause is a variant of second clause.
 *    NOMEMORY -> Not enough memory.
 *    TIMEOUT -> Function timeout condition
 *
 *--------------------------------------------------------------*/
int32_t LiteralVariant(uint8_t *literal,hashchcmp *component,int8_t *lits,
		int32_t litnb,subst *Subst,struct timeval *sttime){
	auto struct timeval endtime;                         /* Function current time */
	auto double timedif;                                 /* Function elapsed time */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4;                /* Auxiliary pointers */
	auto int32_t ii,jj,kk,mm;                            /* Auxiliary */

	/* Remember substitution status. */
	if (Subst->buffer!=NULL){
		kk=Subst->substsize;
	} else {
		kk=-1;
	}

	/* Loop through literals of second clause. */
	for (ptr1=&component->formula[0],ii=0;ptr1[0]!=UNITEND;ptr1=ptr3,ii++){

		/* Literal is free. */
		ptr3=NextItem(ptr1,OVERSUBTERMS);
		if (0==lits[ii]){

			/* Reset substitution to its initial state. */
			if (kk>0){
				Subst->substsize=kk;
				Subst->buffer[kk-1]=UNITEND;
			} else if (Subst->buffer!=NULL){
				Subst->substsize=1;
				Subst->buffer[0]=UNITEND;
			} else {
				Subst->substsize=0;
			}

			/* Check if literal is a valid variant of current literal in clause 2. */
			/* Both literals are equalities or inequalities. */
			if ((literal[0]&EQUALITY)&&((literal[0]&(EQUALITY|NEGATED))==(ptr1[0]&(EQUALITY|NEGATED)))){

				/* Compare first equality root terms. */
				if (NOMEMORY==(jj=ItemVariant(ptr2=NextItem(literal,IMMED),ptr4=NextItem(ptr1,IMMED),Subst))){
					return(NOMEMORY);
				}

				/* The first equality root terms match. */
				if (jj){

					/* Compare second equality root terms. */
					if (NOMEMORY==(jj=ItemVariant(NextItem(ptr2,OVERSUBTERMS),NextItem(ptr4,OVERSUBTERMS),Subst))){
						return(NOMEMORY);
					}

					/* If the second equality root terms don't match then reset substitution */
					/* to its initial state. This is necessary because a second check with root */
					/* terms swapped will be done (see below). */
					if (jj==0){
						if (kk>0){
							Subst->substsize=kk;
							Subst->buffer[kk-1]=UNITEND;
						} else if (Subst->buffer!=NULL){
							Subst->substsize=1;
							Subst->buffer[0]=UNITEND;
						} else {
							Subst->substsize=0;
						}
					}
				}

				/* The first equality root terms don't match or they match but the second root */
				/* terms don't match. This check is necessary because in the following equalities: */
				/* a(X,Y,Z)=a(Y,Z,X) and a(Y,Z,X)=a(X,Y,Z) the first root terms match but the match */
				/* is not compatible with the second root terms match. However swapping root terms */
				/* of one equality produces a valid match. */
				if (jj==0){

					/* Compare first equality root term in literal with */
					/* second equality root term of literal in clause 2. */
					if (NOMEMORY==(jj=ItemVariant(ptr2,NextItem(ptr4,OVERSUBTERMS),Subst))){
						return(NOMEMORY);
					}

					/* Equality root terms match. */
					if (jj){

						/* Compare second equality root term in literal with */
						/* first equality root term of literal in clause 2. */
						if (NOMEMORY==(jj=ItemVariant(NextItem(ptr2,OVERSUBTERMS),ptr4,Subst))){
							return(NOMEMORY);
						}
					}
				}

			/* Literal is not an equality. */
			} else {

				/* Compare literals. */
				if (NOMEMORY==(jj=ItemVariant(literal,ptr1,Subst))){
					return(NOMEMORY);
				}
			}

			/* Literal is a valid variant. Check remaining literals. */
			if (jj){
				lits[ii]=1;
				ptr2=NextItem(literal,OVERSUBTERMS);
				if (ptr2[0]==UNITEND){
					mm=1;
				} else {
					mm=LiteralVariant(ptr2,component,lits,litnb+1,Subst,sttime);
				}
				if (mm){
					return(mm);
				}
				lits[ii]=0;
			}

			/* Check timeout. */
			gettimeofday(&endtime,NULL);
			timedif=endtime.tv_sec-sttime->tv_sec
					+(endtime.tv_usec-sttime->tv_usec)/1000000.0;
			if (timedif>0.0185){
				return(TIMEOUT);
			}
		}
	}

	/* Return not a valid variant. */
	return(0);
} /* LiteralVariant */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Item variant check)  OCJ
 *
 *    This function checks if one item is a valid variant of another
 *    item. Items cannot be equalities (but this is not checked),
 *    however they can be predicates, functions or variables.
 *
 *
 *  ARGUMENTS:
 *
 *    item1: First item to be checked.
 *    item2: First item to be checked.
 *    Subst: Substitution currently active. This function can add atomic
 *           substitutions to the current substitution but cannot modify
 *           existing atomic substitutions.
 *
 *  RETURNS:
 *
 *    0 -> first item is not a variant of second item.
 *    1 -> first item is a variant of second item.
 *    NOMEMORY -> Not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t ItemVariant(uint8_t *item1,uint8_t *item2,subst *Subst){
	auto uint8_t *ptr2,*ptr3,*ptr4,*ptr5;                /* Auxiliary pointers */
	auto int16_t *ptr6,*ptr7;                            /* Auxiliary pointers */
	auto int16_t var1,var2;                              /* Variable numbers */
	auto int32_t jj;                                     /* Auxiliary */

	/* Loop through subitems in item1 and item2. */
	ptr3=NextItem(item1,OVERSUBTERMS);
	for (ptr2=item1,ptr4=item2,jj=1;(jj&&(ptr2<ptr3));ptr2=NextItem(ptr2,IMMED),ptr4=NextItem(ptr4,IMMED)){

		/* Subitem types match. */
		if ((ptr2[0]&(PREDICATE|NEGATED|FUNCTION|VARIABLE))
				==(ptr4[0]&(PREDICATE|NEGATED|FUNCTION|VARIABLE))){

			/* Subitems are variables. */
			if (ptr2[0]&VARIABLE){

				/* Check compatibility with existing substitution. */
				var1=*((int16_t *)&ptr2[1]);
				var2=*((int16_t *)&ptr4[1]);
				jj=2;
				if (Subst->buffer!=NULL){
					for (ptr5=Subst->buffer;ptr5[0]!=UNITEND;ptr5=NextItem(&ptr5[3],OVERSUBTERMS)){
						ptr6=(int16_t *)&ptr5[1];
						ptr7=(int16_t *)&ptr5[4];
						if ((*ptr6)==var1){
							if ((*ptr7)!=var2){
								jj=0;
							} else {
								jj=1;
							}
							break;
						} else {
							if ((*ptr7)==var2){
								jj=0;
								break;
							}
						}
					}
				}

				/* Update substitution if needed. */
				if (jj==2){
					jj=1;

					/* Allocate substitution buffer if necessary. */
					if (Subst->buffer==NULL){
						if (NULL==(ptr5=MYALLOC(6+SUBST_CHUNK_SIZE))){
							return(NOMEMORY);
						}
						Subst->buffsize=6+SUBST_CHUNK_SIZE;
						Subst->substsize=1;
						Subst->buffer=ptr5;
						Subst->buffer[0]=UNITEND;

					/* Check free space in substitution buffer. */
					} else if ((Subst->buffsize-Subst->substsize)<6){
						if (NULL==(ptr5=MYREALLOC(Subst->buffer,Subst->buffsize+6+SUBST_CHUNK_SIZE))){
							return(NOMEMORY);
						}
						Subst->buffer=ptr5;
						Subst->buffsize+=(6+SUBST_CHUNK_SIZE);
					}

					/* Add substitution of var1 by var2. */
					Subst->buffer[Subst->substsize-1]=0;
					*((int16_t *)&Subst->buffer[Subst->substsize])=var1;
					Subst->buffer[Subst->substsize+2]=VARIABLE;
					*((int16_t *)&Subst->buffer[Subst->substsize+3])=var2;
					Subst->buffer[Subst->substsize+5]=UNITEND;
					Subst->substsize+=6;
				}

			/* Subitems are predicate or functions. Check symbols match. */
			} else if ((((symbol *)&ptr2[1])->symbol)==(((symbol *)&ptr4[1])->symbol)){
				jj=1;
			} else {
				jj=0;
			}

		/* Item types don't match. */
		} else {
			jj=0;
		}
	}

	/* Return result. */
	return(jj);
} /* ItemVariant */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Send SAT queue to SAT solver)  OCJ
 *
 *    This function transfer the SAT clauses in the working KB
 *    SAT queue to the SAT solver.
 *
 *  ARGUMENTS:
 *
 *    type: If not zero then the maximum allowed execution time is
 *          SATQMAXTIME. Otherwise execution time is not controlled.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY or TIMEOUT.
 *
 *--------------------------------------------------------------*/
int32_t SatQ2SatSolver(int32_t type){
	auto satclause *satcl;                               /* Pointer to SAT clause */
	auto double time;                                    /* Elapsed time in seconds */
	auto struct timeval timestr1,timestr2;               /* Structures for time measurement */
	auto int32_t itercnt;                                /* Iterations counter */
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto hashchcmp *ptr2;                                /* Auxiliary pointer */
	auto int32_t ii;                                     /* Auxiliary */

	/* Store current time stamp. */
	if (type){
		gettimeofday(&timestr1,NULL);
	}

	/* Loop through clauses in the SAT queue. */
	itercnt=-1;
	for (satcl=kbset.firstsatq;satcl!=NULL;satcl=kbset.firstsatq){

		/* Check timeout and solution by other process. If there is a solution */
		/* by "other process" but the other process is this process then this is */
		/* a call from VerifySatProof() function and it is not really a solution */
		/* by "other process". */
		if (((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE))&&(procnb!=procctl->solvekb)){
			return(TIMEOUT);
		}
		if ((procctl->status==NOMEMORY)||(procctl->status==TIMEOUT)){
			return(procctl->status);
		}
		if (type){
			itercnt++;
			if (itercnt>=SATQITERS){
				itercnt=0;
				gettimeofday(&timestr2,NULL);
				time=timestr2.tv_sec-timestr1.tv_sec+(timestr2.tv_usec-timestr1.tv_usec)/1000000.0;
				if (time>=SATQMAXTIME){
					return(SATQTIMEOUT);
				}
			}
		}

		/* Unlink clause from SAT queue. */
		kbset.firstsatq=satcl->glblnext;
		if (satcl->glblnext!=NULL){
			satcl->glblnext->glblprev=NULL;
		} else {
			kbset.lastsatq=NULL;
		}

		/* Simplify SAT clause. IMPORTANT: some SAT symbols of the clause may become */
		/* orphan if the clause is simplified (and therefore deleted) and the symbols */
		/* are new. The orphan symbols are not deleted to save processing time at the */
		/* cost of a bit of additional memory. They may be reused later. They are */
		/* easily recognized because they have the npositive and nnegative fields */
		/* set to 0. Of course they will be deleted when the global SAT symbol queue */
		/* is depleted. */
		/* First check tautologies and remove duplicate literals. */
		if (NOMEMORY==(ii=SatSimplify(&satcl))){
			return(NOMEMORY);
		} else if (ii){
			continue;
		}

		/* Check subsumptions by and for clause. */
		if (SatSubsumption(satcl)){
			continue;
		}

		/* Set satlits and undeflits fields of satcl. */
		satcl->satlits=0;
		satcl->undeflits=satcl->literals;

		/* Loop through literals. */
		for (ptr1=&satcl->formula[0];ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol))){

			/* Update the npositive, nnegative and absdif fields of the */
			/* corresponding hashchcmp element. */
			ptr2=((asymbol *)&ptr1[1])->symbol;
			if (ptr1[0]&POSITIVE){
				ptr2->npositive++;
			} else {
				ptr2->nnegative++;
			}
			if (ptr2->npositive>ptr2->nnegative){
				ptr2->absdif=ptr2->npositive-ptr2->nnegative;
			} else {
				ptr2->absdif=ptr2->nnegative-ptr2->npositive;
			}

			/* If the corresponding hashchcmp element is not new then remove */
			/* it from the hashchcmp order queue. As at this point we are at */
			/* the root node all existing hashchcmp elements are undefined */
			/* and hence in the ordered sub-queue. It is necessary to remove */
			/* it and link it again to the ordered sub-queue so that it is */
			/* placed in the right position. */
			if (0!=(ptr2->flags&ORDERED)){
				UnchainOrdComponent(ptr2);
			}

			/* Chain the hashchcmp element to the hashchcmp order sub-queue. */
			ChainOrdComponent(ptr2);

			/* Link the literal to the SAT solver chain for literals of the same component. */
			if (ptr2->frstsat!=NULL){
				ptr2->frstsat->prev=(asymbol *)&ptr1[1];
			}
			((asymbol *)&ptr1[1])->next=ptr2->frstsat;
			ptr2->frstsat=(asymbol *)&ptr1[1];
			((asymbol *)&ptr1[1])->prev=NULL;
		}

		/* Add clause to the SAT solver global queue. */
		satcl->glblnext=NULL;
		if (kbset.lastsatglbl!=NULL){
			kbset.lastsatglbl->glblnext=satcl;
		} else {
			kbset.frstsatglbl=satcl;
		}
		satcl->glblprev=kbset.lastsatglbl;
		kbset.lastsatglbl=satcl;

		/* Debug for checking satclause ordered sub-queue coherence. */
		#ifdef SATDEBUGCODE2
		satcl->next=satcl->prev=NULL;
		#endif

		/* Chain clause to the SAT solver ordered queue. */
		ChainSatClause(satcl);
	}

	return(0);
} /* SatQ2SatSolver */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (SAT solver algorithm)  OCJ
 *
 *    This function performs the SAT solver algorithm. The natural
 *    way to build this function would be recursively. However, due
 *    to the potentially very high depth of recursive calls (as many
 *    as the number of hashchcmp different elements, that is,
 *    different components) the recursion may easily cause stack
 *    overflow. Due to this the function is built non recursively.
 *
 *    This way unnecessary memory allocation and freeing is avoided.
 *    Therefore the allocated memory must be freed elsewhere when it
 *    is no longer necessary.
 *
 *    This algorithm is based on the Chaff algorithm, however it is
 *    not a strict implementation of it. See "Validating SAT Solvers
 *    Using an Independent Resolution-Based Checker: Practical
 *    Implementations and Other Applications" by Lintao Zhang and
 *    Sharad Malik.
 *
 *  ARGUMENTS:
 *
 *    None.
 *
 *  RETURNS:
 *
 *    0 -> There is no model available because a refutation was found.
 *    1 -> There is a model available.
 *    NOMEMORY if not enough memory.
 *    TIMEOUT if there is a timeout condition.
 *    SLICETMOUT if maximum number of executed hardware instructions
 *    was reached.
 *
 *--------------------------------------------------------------*/
int32_t SatSolver(void){
	auto satnode *node,*prevnode;                        /* Pointers to current and previous nodes */
	auto satclause *contr;                               /* Pointer to contradiction clause */
	auto uint8_t *ptr1,*ptr5;                            /* Auxiliary pointers */
	auto satnode *ptr2;                                  /* Auxiliary pointer */
	auto hashchcmp *ptr3;                                /* Auxiliary pointer */
	auto satclause *ptr4;                                /* Auxiliary pointer */
	auto int32_t ii,jj;                                  /* Auxiliary */
	auto uint64_t hh;                                    /* Auxiliary */
	#ifdef SATDEBUGCODE1
	auto int64_t jj;
	auto int32_t kk,mm,nn,pp;
	auto hashchcmp *ptr11;
	auto satnode *ptr12;
	jj=0;
	#endif

	/* Debug for checking satclause ordered sub-queue coherence. */
	#ifdef SATDEBUGCODE2
	auto satclause *ptr13;
	auto int64_t qq;
	jj=0;
	#endif

	/* Initialize node pointers. */
	node=kbset.satrootnode;
	prevnode=NULL;

	/* Clear the previous model and save current model as previous. */
	/* As we are starting the SAT solver and it is not running yet */
	/* then all hashchcmp elements are unassigned and so they are */
	/* all in the undefined queue */
	for (ptr3=kbset.frsthshundf;ptr3!=NULL;ptr3=ptr3->nextundef){
		ptr3->flags&=(~(PRPOSITIVE|PRNEGATIVE|PRUNDEFINED));
		switch (ptr3->flags&(MODELPOSITIVE|MODELNEGATIVE|MODELUNDEFINED)){
			case MODELPOSITIVE:
				ptr3->flags|=PRPOSITIVE;
				break;
			case MODELNEGATIVE:
				ptr3->flags|=PRNEGATIVE;
				break;
			case MODELUNDEFINED:
				ptr3->flags|=PRUNDEFINED;
				break;
		}
		ptr3->flags&=(~(MODELPOSITIVE|MODELNEGATIVE|MODELUNDEFINED));
	}

	/* Clear inproof field of SAT clauses. As at this point we are at */
	/* the root node all existing hashchcmp elements are undefined */
	/* and hence in the ordered sub-queue, so the ordered sub-queue */
	/* is used for this loop to be able to call SatSolver() function */
	/* from VerifySatProof() function. */
	for (ptr4=kbset.frstunsats;ptr4!=NULL;ptr4=ptr4->next){
		ptr4->inproof=0;
	}

	/* Main loop. */
	ptr5=NULL; /* Just to avoid compiler warnings. */
	jj=0;
	while (kbset.frstunsats!=NULL){

		/* Check number of executed hardware instructions. */
		if (satcalltype){
			jj++;
			if (jj>CHKCYCLES){
				jj=0;
				myread(&hh);
				if (hh>=instrlimit){
					return(SLICETMOUT);
				}
			}
		}

		/* Check timeout and solution by other process. If there is a solution */
		/* by "other process" but the other process is this process then this is */
		/* a call from VerifySatProof() function and it is not really a solution */
		/* by "other process". */
		if ((procctl->status==TIMEOUT)||(((procctl->status==UNSATISFIABLE)
				||(procctl->status==SATISFIABLE))&&(procnb!=procctl->solvekb))){
			return(TIMEOUT);
		}
		if (procctl->status==NOMEMORY){
			return(procctl->status);
		}

		/* This is an existing branch. */
		if ((node!=NULL)&&(0==(FREENODE&node->flags))){

			/* Check if there are forced options that match the existing branch */
			for (ptr4=kbset.frstunsats,ii=1;(ptr4!=NULL)&&(ptr4->undeflits==1)&&(ii==1);ptr4=ptr4->next){
				for (ptr1=&ptr4->formula[0];;ptr1+=(1+sizeof(asymbol))){
					if (UNDEFINED&((asymbol *)&ptr1[1])->symbol->flags){
						if (((asymbol *)&ptr1[1])->symbol==node->symbol){
							if ((POSITIVE|NEGATIVE)&(ptr1[0]^node->symbol->flags)){
								ii=2;
							} else {
								ii=0;
							}
						}
						break;
					}
				}
			}

			/* If none of the forced options match the existing branch and either */
			/* KEEPSATMODEL equals 1 or there are forced options then cancel the */
			/* existing branch. */
			if ((ii)&&((KEEPSATMODEL==1)||(kbset.frstunsats->undeflits==1))){
				for (ptr2=node;ptr2!=NULL;ptr2=ptr2->nextnode){
					ptr2->flags=FREENODE;
				}
			}
		}

		/* This node is a new branch. */
		if ((node==NULL)||(FREENODE&node->flags)){

			/* Allocate if necessary and initialize node. */
			if (node==NULL){
				if (NULL==(node=MYALLOC(sizeof(satnode)))){
					return(NOMEMORY);
				}
				node->nextnode=NULL;
				if (prevnode!=NULL){
					prevnode->nextnode=node;
					node->number=prevnode->number+1;
				} else {
					kbset.satrootnode=node;
					node->number=0;
				}
				node->prevnode=prevnode;
			}
			node->flags=0;

			/* Select an undefined symbol and the value true or false */
			/* to be assigned to it. First look at the SAT solver clause */
			/* ordered queue for clauses with only one or two undefined */
			/* literals. */
			if (kbset.frstunsats->undeflits<=2){
				node->symbol=NULL;
				node->forcedcl=NULL;
				for (ptr1=&kbset.frstunsats->formula[0];;ptr1+=(1+sizeof(asymbol))){
					if (UNDEFINED&((asymbol *)&ptr1[1])->symbol->flags){
						if (kbset.frstunsats->undeflits==1){
							node->symbol=((asymbol *)&ptr1[1])->symbol;
							node->flags|=LASTOPTION;
							node->forcedcl=kbset.frstunsats;
							node->symbol->satnode=node;
							ptr5=ptr1;
							break;
						}
						if (node->symbol==NULL){
							node->symbol=((asymbol *)&ptr1[1])->symbol;
							node->symbol->satnode=node;
							ptr5=ptr1;
						} else {
							if (((asymbol *)&ptr1[1])->symbol->absdif<=node->symbol->absdif){
								break;
							} else {
								node->symbol=((asymbol *)&ptr1[1])->symbol;
								node->symbol->satnode=node;
								ptr5=ptr1;
								break;
							}
						}
					}
				}
				node->flags|=ptr5[0];

			/* Otherwise take the first symbol in the hashchcmp */
			/* ordered queue. */
			} else {
				node->symbol=kbset.frsthshundf;
				node->symbol->satnode=node;
				node->flags|=(node->symbol->npositive>=node->symbol->nnegative?POSITIVE:NEGATIVE);
				node->forcedcl=NULL;
			}
		}

		/* Debug: Check coherence of ordered haschcmp elements. */
		#ifdef SATDEBUGCODE1
		jj++;
		if (jj==254){
			jj++;
			jj--;
		}
		for (ptr11=kb_hashcmp,kk=mm=0;ptr11!=NULL;ptr11=ptr11->nextelem){
			if (UNDEFINED&ptr11->flags){
				kk++;
			} else {
				mm++;
			}
		}
		for (ptr12=kbset.satrootnode,nn=0;(ptr12!=NULL)&&(0==(FREENODE&ptr12->flags));
				ptr12=ptr12->nextnode,nn++){
		}
		if (nn!=(mm+1)){
			nn++;
			nn--;
		}
		for (ptr11=kbset.frsthshundf,pp=0;ptr11!=NULL;ptr11=ptr11->nextundef,pp++){
		}
		if (pp!=kk){
			pp++;
			pp--;
		}
		#endif

		/* Debug for checking satclause ordered sub-queue coherence. */
		#ifdef SATDEBUGCODE2
		qq++;
		if (qq==259){
			qq++;
			qq--;
		}
		for (ptr13=kbset.frstunsats;ptr13!=NULL;ptr13=ptr13->next){
			if ((ptr13->satlits!=0)||(ptr13->undeflits==0)){
				qq++;
				qq--;
			}
		}
		for (ptr13=kbset.frstsatglbl;ptr13!=NULL;ptr13=ptr13->glblnext){
			if ((ptr13->satlits==0)&&(ptr13->next==NULL)&&(ptr13->prev==NULL)
					&&((ptr13!=kbset.frstunsats)||(ptr13!=kbset.lstunsats))){
				qq++;
				qq--;
			}
		}
		#endif

		/* Perform symbol assignment tasks. */
		while (DoSymbolAssgnmnt(node,&contr)){

			/* If we are here then there is a contradiction. Free branch */
			/* nodes below the current branch. */
			for (ptr2=node->nextnode;ptr2!=NULL;ptr2=ptr2->nextnode){
				ptr2->flags=FREENODE;
			}

			/* Backtrack. */
			switch (Backtrack(contr,&node)){

				/* Not enough memory. */
				case NOMEMORY:
					return(NOMEMORY);
					break;

				/* SAT refutation. Allocate and set the root proof node, */
				/* adjust inproof field of contradicion clause and return */
				/* no model available. */
				case 1:
					if (NULL==(kbset.frstprfnode=MYALLOC(sizeof(proofnode)))){
						return(NOMEMORY);
					}
					kbset.frstprfnode->type=SATCLAUSE;
					kbset.frstprfnode->ptr.satclause=contr;
					contr->inproof=2;
					return(0);
					break;
			}

			/* If we are here we are in a node with a second option. */
			/* Assign second option to the node and iterate. */
			prevnode=node->prevnode;
			node->flags|=LASTOPTION;
			node->flags&=(~FREENODE);
			node->flags^=(POSITIVE|NEGATIVE);
		}

		/* Debug for checking satclause ordered sub-queue coherence. */
		#ifdef SATDEBUGCODE2
		for (ptr13=kbset.frstunsats;ptr13!=NULL;ptr13=ptr13->next){
			if ((ptr13->satlits!=0)||(ptr13->undeflits==0)){
				qq++;
				qq--;
			}
		}
		for (ptr13=kbset.frstsatglbl;ptr13!=NULL;ptr13=ptr13->glblnext){
			if ((ptr13->satlits==0)&&(ptr13->next==NULL)&&(ptr13->prev==NULL)
					&&((ptr13!=kbset.frstunsats)||(ptr13!=kbset.lstunsats))){
				qq++;
				qq--;
			}
		}
		#endif

		/* Debug: Check coherence of ordered haschcmp elements. */
		#ifdef SATDEBUGCODE1
		for (ptr11=kb_hashcmp,kk=mm=0;ptr11!=NULL;ptr11=ptr11->nextelem){
			if (UNDEFINED&ptr11->flags){
				kk++;
			} else {
				mm++;
			}
		}
		for (ptr12=kbset.satrootnode,nn=0;(ptr12!=NULL)&&(FREENODE!=ptr12->flags);
				ptr12=ptr12->nextnode,nn++){
		}
		if (nn!=mm){
			nn++;
			nn--;
		}
		for (ptr11=kbset.frsthshundf,pp=0;ptr11!=NULL;ptr11=ptr11->nextundef,pp++){
		}
		if (pp!=kk){
			pp++;
			pp--;
		}
		#endif

		/* Prepare next iteration and iterate. */
		prevnode=node;
		node=node->nextnode;
	}

	/* If the ordered SAT solver clause queue is empty then all */
	/* the clauses are satisfied and a model is available. */
	/* Save the model. First save the state of undefined components. */
	for (ptr3=kbset.frsthshundf;ptr3!=NULL;ptr3=ptr3->nextundef){
		ptr3->flags|=MODELUNDEFINED;
	}

	/* Save defined components, undo all assignments and return. */
	/* The SAT tree is left as it is to allow incremental model */
	/* calculation. */
	for (node=prevnode;node!=NULL;node=node->prevnode){
		switch ((POSITIVE|NEGATIVE)&node->flags){
			case POSITIVE:
				node->symbol->flags|=MODELPOSITIVE;
				break;
			case NEGATIVE:
				node->symbol->flags|=MODELNEGATIVE;
				break;
		}
		UndoSymbolAssgnmnt(node);
	}
	return(1);
} /* SatSolver */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Chain a hashchcmp element to the ordered sub-queue)  OCJ
 *
 *    This function chains a hashchcmp structure component in the ordered
 *    sub-queue for SAT solver component selection.
 *
 *    The ordering criteria in the sub-queue is as follows:
 *    - Only hashchcmp structure elements that don't have a value true or false
 *      (POSITIVE or NEGATIVE) assigned are in the sub-queue.
 *    - The elements with a single polarity (only positive literals or only
 *      negative literals in SAT solver clauses) go first, starting with those
 *      with the more clauses that have a literal corresponding to the component.
 *    - Next the elements with both polarities are placed, starting with those
 *      with the highest value of the field absdif.
 *
 *    IMPORTANT: This function must never be called for a hashchcmp element
 *    already linked to the ordered sub-queue.
 *
 *    IMPORTANT: This function must never be called for main KB.
 *
 *  ARGUMENTS:
 *
 *    component: Pointer to hashchcmp structure of component to chain.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void ChainOrdComponent(hashchcmp *component){
	auto hashchcmp *ptr3;                                /* Auxiliary pointer */

	/* Component with only one polarity. */
	if ((component->nnegative==0)||(component->npositive==0)){
		for (ptr3=kbset.frsthshundf;ptr3!=NULL;ptr3=ptr3->nextundef){
			if ((ptr3->absdif<=component->absdif)||(ptr3==kbset.frsthunbp)){
				break;
			}
		}
		if (ptr3==NULL){
			if (kbset.frsthshundf==NULL){
				kbset.frsthshundf=component;
			} else {
				kbset.lasthshundf->nextundef=component;
			}
			component->prevundef=kbset.lasthshundf;
			kbset.lasthshundf=component;
			component->nextundef=NULL;
		} else {
			if (NULL!=ptr3->prevundef){
				ptr3->prevundef->nextundef=component;
			} else {
				kbset.frsthshundf=component;
			}
			component->prevundef=ptr3->prevundef;
			ptr3->prevundef=component;
			component->nextundef=ptr3;
		}

	/* Component with both polarities. */
	} else {
		for (ptr3=kbset.frsthunbp;ptr3!=NULL;ptr3=ptr3->nextundef){
			if (ptr3->absdif<=component->absdif){
				break;
			}
		}
		if (ptr3==NULL){
			if (kbset.frsthunbp==NULL){
				kbset.frsthunbp=component;
			}
			if (kbset.frsthshundf==NULL){
				if (kbset.lasthshundf==NULL){
					kbset.frsthshundf=component;
				} else {
					kbset.lasthshundf->nextundef=component;
				}
			} else {
				kbset.lasthshundf->nextundef=component;
			}
			component->prevundef=kbset.lasthshundf;
			kbset.lasthshundf=component;
			component->nextundef=NULL;
		} else {
			if (NULL!=ptr3->prevundef){
				ptr3->prevundef->nextundef=component;
				if (ptr3==kbset.frsthunbp){
					kbset.frsthunbp=component;
				}
			} else {
				kbset.frsthshundf=kbset.frsthunbp=component;
			}
			component->prevundef=ptr3->prevundef;
			ptr3->prevundef=component;
			component->nextundef=ptr3;
		}
	}

	/* Indicate that the component has been ordered. */
	component->flags|=ORDERED;

	return;
} /* ChainOrdComponent */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Remove a hashchcmp element from the ordered sub-queue)  OCJ
 *
 *    This function removes a hashchcmp structure component from the ordered
 *    sub-queue for SAT solver component selection. See ChainOrdComponent()
 *    function for details about this sub-queue.
 *
 *    IMPORTANT: This function must never be called for a hashchcmp element
 *    not linked to the ordered sub-queue.
 *
 *    IMPORTANT: This function must never be called for main KB.
 *
 *  ARGUMENTS:
 *
 *    component: Pointer to hashchcmp structure of component to chain.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void UnchainOrdComponent(hashchcmp *component){

	/* Remove hashchcmp element from the ordered sub-queue. */
	if (component->prevundef!=NULL){
		component->prevundef->nextundef=component->nextundef;
	} else {
		kbset.frsthshundf=component->nextundef;
	}
	if (component->nextundef!=NULL){
		component->nextundef->prevundef=component->prevundef;
	} else {
		kbset.lasthshundf=component->prevundef;
	}
	if (kbset.frsthunbp==component){
		kbset.frsthunbp=component->nextundef;
	}
	component->flags&=(~ORDERED);

	return;
} /* UnchainOrdComponent */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Chain a SAT solver clause to ordered queue)  OCJ
 *
 *    This function chains a SAT solver clause in the SAT solver
 *    ordered queue.
 *
 *    There are two SAT solver queues: the global queue and the ordered
 *    queue. The global queue chains all the SAT solver clauses starting
 *    with the older ones. This function doesn't handles the global queue.
 *
 *    The SAT solver ordered clause queue chains all clauses not yet
 *    satisfied with the following ordering criteria:
 *    - First the not yet satisfied clauses with only one literal.
 *    - Next the not yet satisfied clauses with more than one literal
 *      but only one literal without a value assigned (true or false).
 *    - Next the not yet satisfied clauses with more than one literal
 *      but only two literals without a value assigned (true or false).
 *    - Next all the other not yet satisfied clauses, without any order.
 *      This part of the queue is not ordered. New clauses in this
 *      category are added at the end of the queue.
 *
 *    This function must NOT be called for satisfied clauses, clauses
 *    without undefined literals or clauses already linked to the ordered
 *    queue, otherwise it may cause unpredictable results. Clauses without
 *    undefined literals are either satisfied or cause a contradiction.
 *
 *    IMPORTANT: This function must never be called for main KB.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to the satclause structure of the clause
 *            to be chained.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void ChainSatClause(satclause *clause){
	auto satclause *ptr5;                                /* Auxiliary pointer */

	/* This is the first clause added to the SAT solver ordered queue. */
	if (kbset.frstunsats==NULL){
		kbset.frstunsats=kbset.lstunsats=clause;
		clause->next=clause->prev=NULL;
		if ((clause->undeflits>=2)||(clause->satlits>0)){
			kbset.frstunsats2=clause;
		}

	/* This is not the first clause added to the SAT solver. */
	} else {

		/* The clause has more than two undefined literals. */
		/* Link it to the end of the SAT solver queue. */
		if (clause->undeflits>2){
			kbset.lstunsats->next=clause;
			clause->prev=kbset.lstunsats;
			kbset.lstunsats=clause;
			clause->next=NULL;
			if (kbset.frstunsats2==NULL){
				kbset.frstunsats2=clause;
			}

		/* The clause has one or two undefined literals. Insert it */
		/* in the ordered part of the SAT solver clauses queue. */
		} else {

			/* The clause has only one literal in total, which must be undefined, */
			/* otherwise this function should not be called. */
			if (clause->literals==1){
				clause->next=kbset.frstunsats;
				clause->prev=NULL;
				kbset.frstunsats->prev=clause;
				kbset.frstunsats=clause;

			/* The clause has more than one literal. */
			} else {
				switch (clause->undeflits){

					/* Only one undefined literal. */
					case 1:
						for (ptr5=kbset.frstunsats;
								(ptr5!=NULL)&&(ptr5->literals==1)&&(ptr5->undeflits==1);
								ptr5=ptr5->next){
						}
						if (ptr5==NULL){
							clause->prev=kbset.lstunsats;
							kbset.lstunsats->next=clause;
							clause->next=NULL;
							kbset.lstunsats=clause;
						} else {
							if (ptr5->prev!=NULL){
								ptr5->prev->next=clause;
							} else {
								kbset.frstunsats=clause;
							}
							clause->next=ptr5;
							clause->prev=ptr5->prev;
							ptr5->prev=clause;
						}
						break;

					/* Two undefined literals. */
					case 2:
						if (kbset.frstunsats2==NULL){
							clause->prev=kbset.lstunsats;
							kbset.lstunsats->next=kbset.frstunsats2=clause;
							kbset.lstunsats=clause;
							clause->next=NULL;
						} else {
							if (kbset.frstunsats2->prev!=NULL){
								kbset.frstunsats2->prev->next=clause;
							} else {
								kbset.frstunsats=clause;
							}
							clause->next=kbset.frstunsats2;
							clause->prev=kbset.frstunsats2->prev;
							kbset.frstunsats2->prev=clause;
							kbset.frstunsats2=clause;
						}
						break;
				}
			}
		}
	}

	return;
} /* ChainSatClause */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Remove a satclause element from the ordered sub-queue)  OCJ
 *
 *    This function removes a satclause structure element from the ordered
 *    sub-queue for SAT solver clauses. See ChainSatClause() function
 *    function for details about this sub-queue.
 *
 *    IMPORTANT: This function must never be called for clauses not linked
 *    to the ordered sub-queue.
 *
 *    IMPORTANT: This function must never be called for main KB.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to satclause structure of clause to chain.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void UnchainSatClause(satclause *clause){

	/* Remove clause from the ordered queue. */
	if (clause->prev!=NULL){
		clause->prev->next=clause->next;
	} else {
		kbset.frstunsats=clause->next;
	}
	if (clause->next!=NULL){
		clause->next->prev=clause->prev;
	} else {
		kbset.lstunsats=clause->prev;
	}
	if (kbset.frstunsats2==clause){
		kbset.frstunsats2=clause->next;
	}
	/* Debug for checking satclause ordered sub-queue coherence. */
	#ifdef SATDEBUGCODE2
	clause->next=clause->prev=NULL;
	#endif

	return;
} /* UnchainSatClause */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform symbol assignment tasks)  OCJ
 *
 *    This function performs all the tasks needed for a SAT symbol
 *    assignment, including adjusting SAT clause parameters and
 *    unlinking and chaining SAT clause for clauses affected by
 *    symbol assignment. This function does not (and must not)
 *    change the flags field of node.
 *
 *
 *  ARGUMENTS:
 *
 *    node: Pointer to node corresponding to symbol assignment.
 *    pcontrcl: Address of pointer to contradiction clause. This
 *              will be set if a contradiction is found.
 *
 *  RETURNS:
 *
 *    0 -> There was no contradiction.
 *    1 -> There was a contradiction.
 *
 *--------------------------------------------------------------*/
int32_t DoSymbolAssgnmnt(satnode *node,satclause **pcontrcl){
	auto satclause *clause;                              /* Pointer to clause */
	auto asymbol *ptr1;                                  /* Auxiliary pointer */
	auto uint8_t *ptr2;                                  /* Auxiliary pointer */
	auto int32_t ii,jj;                                  /* Auxilliary */

	/* Assign polarity to symbol. */
	node->symbol->flags&=(~UNDEFINED);
	node->symbol->flags|=((POSITIVE|NEGATIVE)&node->flags);

	/* Remove hashchcmp element from ordered sub-queue. */
	UnchainOrdComponent(node->symbol);

	/* Loop through literal symbols in clauses matching the assigned symbol. */
	/* The variable jj is set to 0 unless a contradiction is found, in which */
	/* case it is set to 1. */
	jj=0;
	for (ptr1=node->symbol->frstsat;ptr1!=NULL;ptr1=ptr1->next){

		/* Get pointer to literal and clause. */
		ptr2=(uint8_t *)ptr1-1;
		clause=ptr1->part2.satcl;

		/* Update number of undefined and satisfied literals. */
		/* Set ii=1 if clause is satisfied by this assignment, */
		/* set ii=0 otherwise. */
		clause->undeflits--;
		if (0==(ptr2[0]^((POSITIVE|NEGATIVE)&node->flags))){
			clause->satlits++;
			if (clause->satlits==1){
				ii=1;
			} else {
				ii=0;
			}
		} else {
			ii=0;
		}

		/* Clause has been satisfied by this assignment or it is not yet satisfied */
		/* and has two or less undefined literals. */
		if ((ii)||((clause->satlits==0)&&(clause->undeflits<=2))){

			/* Remove clause from the ordered queue. */
			UnchainSatClause(clause);

			/* The clause is not satisfied. */
			if (clause->satlits==0){

				/* If clause has undefined literals then chain it */
				/* again to the ordered queue. */
				if (clause->undeflits>0){
					ChainSatClause(clause);

				/* If clause has not undefined literals then there is a contradiction. */
				/* Update jj and remember contradiction clause. */
				} else if (jj==0){
					jj=1;
					*pcontrcl=clause;
				}
			}
		}
	}

	return(jj);
} /* DoSymbolAssgnmnt */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Undo symbol assignment)  OCJ
 *
 *    This function reverses a symbol assignment, including
 *    adjusting SAT clause parameters and unlinking and
 *    chaining SAT clause for clauses affected by symbol
 *    assignment. This function does not (and must not)
 *    change the flags field of node.
 *
 *
 *  ARGUMENTS:
 *
 *    node: Pointer to node corresponding to symbol assignment.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void UndoSymbolAssgnmnt(satnode *node){
	auto satclause *clause;                              /* Pointer to clause */
	auto asymbol *ptr1;                                  /* Auxiliary pointer. */
	auto uint8_t *ptr2;                                  /* Auxiliary pointer */
	auto int32_t ii;                                     /* Auxiliary */

	/* Set symbol as undefined. */
	node->symbol->flags&=(~((POSITIVE|NEGATIVE)&node->flags));
	node->symbol->flags|=UNDEFINED;

	/* Link symbol to ordered sub-queue. */
	ChainOrdComponent(node->symbol);

	/* Loop through literal symbols in clauses matching the assigned symbol. */
	for (ptr1=node->symbol->frstsat;ptr1!=NULL;ptr1=ptr1->next){

		/* Get pointer to literal and clause. */
		ptr2=(uint8_t *)ptr1-1;
		clause=ptr1->part2.satcl;

		/* Update number of undefined and satisfied literals. The value of ii is */
		/* set as follows: */
		/* ii=1 if the clause was previously unsatisfied and it was not a */
		/*      contradiction and therefore is in the ordered sub-queue. */
		/* ii=2 if the clause was previously satisfied but it is now unsatisfied. */
		/* ii=0 if none of the above cases. */
		clause->undeflits++;
		if (0==(ptr2[0]^((POSITIVE|NEGATIVE)&node->flags))){
			clause->satlits--;
			if (clause->satlits>0){
				ii=0;
			} else {
				ii=2;
			}
		} else if ((clause->satlits==0)&&(clause->undeflits>1)){
			ii=1;
		} else {
			ii=0;
		}

		/* If clause has two or three undefined literals and it was previously */
		/* unsatisfied and it was not a contradiction then remove it from the */
		/* ordered queue. */
		if ((ii==1)&&((clause->undeflits==2)||(clause->undeflits==3))){
			UnchainSatClause(clause);
		}

		/* If clause meets the following conditions then link it to the */
		/* ordered queue: */
		/* - Clause is not satisfied. */
		/*   ... and ... */
		/* - Clause has less than four undefined literals or it was */
		/*   previously satisfied. */
		if ((clause->satlits==0)&&((clause->undeflits<4)||(ii==2))){
			ChainSatClause(clause);
		}
	}

	return;
} /* UndoSymbolAssgnmnt */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Call SAT solver without first order prover)  OCJ
 *
 *    This function calls the SAT solver alone after splitting all
 *    clauses, including those considered non splitable. If the
 *    theorem or theory is ground then this will give a definite
 *    answer. Otherwise it usually will not be able to prove
 *    valid theorems or check the satisfiability of valid theories.
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
void GoSat(void){
	auto char *buffer;                                   /* Buffer for CNF formula */
	auto int32_t size;                                   /* Buffer size in bytes */
	auto struct mallinfo2 rsinfo;                        /* For mallinfo2() calls */
	auto struct itimerval alarmtime;            /* To disable time alarm */
	auto cmprefix *ptr1,*ptr2;                           /* Auxiliary pointers */
    auto char *ptr3;                                     /* Auxiliary pointer */
	auto double dd;                                      /* Auxiliary */
	auto int32_t ii;                                     /* Auxiliary */

	/* Start time measurement. */
	procctl->start=clock();
	gettimeofday(&mainkb.time,NULL);

	/* Main KB is empty. */
	if (mainkb.nformulas==0){
		printf("Main KB has no input formulas to process.\n");
		return;
	}

	/* Allocate space for CNF conversion. */
	if (NULL==(buffer=MYALLOC(2*STR_CHUNK_SIZE))){
		printf("Not enough memory.\n");
		return;
	}
	size=2*STR_CHUNK_SIZE;

	/* Loop through text formulas in the main KB. */
	printf("Converting formulas to CNF...\n");
	for (ptr1=mainkb.firsttxt;ptr1!=NULL;ptr1=ptr1->next){

		/* Formula is of type axiom like, NEGCONJ or PREDICATEDEF. */
		if (((ptr1->inference<=PLAIN)&&(ptr1->inference!=CONJECTURE))||(ptr1->inference==PREDICATEDEF)){

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
				if (NULL!=(mainkb.frstprfnode=MYALLOC(sizeof(proofnode)))){
					mainkb.frstprfnode->type=ACLAUSE;
					mainkb.frstprfnode->ptr.Aclause=ptr2;
				}
				printf("Problem solved in the pre-processing.\n");
				PrintStats(0);
				break;
			}

			/* Compile CNF. */
			if (NOMEMORY==CompileCNF(buffer,ptr2)){
				procctl->status=NOMEMORY;
				break;;
			}
		}
	}

	/* Disable time alarm. */
	alarmtime.it_interval.tv_sec=alarmtime.it_interval.tv_usec=0;
	alarmtime.it_value.tv_sec=alarmtime.it_value.tv_usec=0;
	setitimer(ITIMER_REAL,&alarmtime,NULL);

	/* Free CNF conversion memory. */
	MYFREE(buffer);

	/* Timeout or not enough memory from PreprocessFormula(). */
	if ((procctl->status==TIMEOUT)||(procctl->status==NOMEMORY)){
		if (procctl->status==NOMEMORY){
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
		return;
	}

	/* Accumulate pre-processing elapsed time and initialize */
	/* time field of KB 0 for timeout control in SatSolver(). */
	gettimeofday(&mainkb.endtime,NULL);
	mainkb.prstats.elapsed_time+=(mainkb.endtime.tv_sec-mainkb.time.tv_sec
			+(mainkb.endtime.tv_usec-mainkb.time.tv_usec)/1000000.0);
	mainkb.time.tv_sec=kbset.time.tv_sec=mainkb.endtime.tv_sec;
	mainkb.time.tv_usec=kbset.time.tv_usec=mainkb.endtime.tv_usec;

	/* Print pre-processing statistics. */
	printf("\nTheorem preprocessing statistics:\n");
	printf("Elapsed time: %.6f seconds\n",mainkb.prstats.elapsed_time);

	/* No NOMEMORY or TIMEOUT return in the compilation process */
	/* and the problem was not solved in the pre-processing. */
	satcalltype=0; /* Call to SatSolver is from GoSat(). */
	if (procctl->status==STOPPED){

		/* Initialize the number of formulas in the working KB with */
		/* the number of text formulas in the main KB. This way the */
		/* formula numbering in both kb's will be compatible. */
		kbset.nformulas=mainkb.lasttxt->number;

		/* Split copies of unprocessed binary clauses in the main KB. */
		for (ptr1=mainkb.frstunproc;ptr1!=NULL;ptr1=ptr1->next){

			/* Allocate storage for the copied clause and make a copy of it. */
			if (NULL==(ptr2=MYALLOC(sizeof(cmprefix)))){
				procctl->status=NOMEMORY;
				break;
			}
			memcpy(ptr2,ptr1,sizeof(cmprefix));
			if (NULL==(ptr2->part2.bin=MYALLOC(binpsize+ptr1->part2.bin->size))){
				procctl->status=NOMEMORY;
				break;
			}
			memcpy(ptr2->part2.bin,ptr1->part2.bin,binpsize+ptr1->part2.bin->size);
			ptr2->part2.bin->oriented=0;
			ptr2->part2.bin->clause=ptr2;

			/* Assign a number to clause. */
			kbset.nformulas++;
			ptr2->number=kbset.nformulas;

			/* Split the clause. */
			if (NOMEMORY==SplitClause(ptr2,1)){
				MYFREE(ptr2->part2.bin);
				MYFREE(ptr2);
				procctl->status=NOMEMORY;
				break;
			}
		}

		/* If we had enough memory then load clauses to the SAT solver */
		/* and call the SATSOLVER. */
		procctl->status=RUNNINGSAT;
		kbset.opts.timeout=glblopts.timeout-mainkb.prstats.elapsed_time;
		if (0==(ii=SatQ2SatSolver(1))){
			ii=SatSolver();
		}

		/* Stop time measurement. */
		procctl->end=clock();
		cpu_time+=((double)(procctl->end-procctl->start))/CLOCKS_PER_SEC;
		tot_cpu_time+=cpu_time;
		gettimeofday(&mainkb.endtime,NULL);
		mainkb.prstats.elapsed_time+=(mainkb.endtime.tv_sec-mainkb.time.tv_sec
				+(mainkb.endtime.tv_usec-mainkb.time.tv_usec)/1000000.0);
		printf("\n**************************************\n");

		/* Check return code from SatQ2SatSolver() or SatSolver(). */
		switch (ii){
			case 0:
				procctl->status=kbset.status=UNSATISFIABLE;
				procctl->solvekb=0;
				printf("\nSAT solver results:\n");
				if (pbmflags&EXISTCONJ){
					printf("Theorem is valid\n");
				} else {
					printf("Theory is unsatisfiable\n");
				}
				printf("Refutation found:\n");
				pbmflags|=SATREFUTATION;
				if (NOMEMORY==PrintProof()){
					printf("\n\n==============================================\n");
					printf("Not enough memory to print a complete proof...\n");
					printf("\n\n==============================================\n");
				}
				break;
			case 1:
				procctl->status=kbset.status=SATISFIABLE;
				if ((0==(pbmflags&GROUND))||((UNITEQUALITY|NONUNITEQUALITY)&pbmflags)){
					printf("Problem is not ground and/or has equalities so the following information is not conclusive.\n");
				}
				printf("\nSAT solver results:\n");
				if (pbmflags&EXISTCONJ){
					printf("Theorem is counter-satisfiable\n");
					printf("Conjecture is false\n");
				} else {
					printf("Theory is satisfiable\n");
				}
				printf("Model found:\n");
				if (NOMEMORY==PrintModel()){
					printf("\n\n=======================================\n");
					printf("Not enough memory to print the model...\n");
					printf("\n\n=======================================\n");
				}
				break;
			case NOMEMORY:
				procctl->status=kbset.status=NOMEMORY;
				printf("Not enough memory.\n");
				break;
			case TIMEOUT:
				procctl->status=kbset.status=TIMEOUT;
				printf("Process timeout.\n");
				break;
		}

		/* Display KB data.*/
		if (prtkbdata){
			PrintKBData();
		}
	}

	/* Collect memory use and initialize working KB and */
	/* kb_hashcmp chained elements. */
	rsinfo=mallinfo2();
	pbmstats->memory=rsinfo.arena+rsinfo.hblkhd-rsinfo.keepcost;
	pbmstats->netmemory=rsinfo.uordblks+rsinfo.hblkhd;
	Initialize_hashcmp();
	Initialize_KB(&kbset);

	/* Display memory use and time information. */
	dd=FormatMemory(pbmstats->memory,&ptr3);
	printf("Total memory used: %.3f %s\n",dd,ptr3);
	dd=FormatMemory(pbmstats->netmemory,&ptr3);
	printf("Net memory used: %.3f %s\n",dd,ptr3);
	printf("Elapsed time: %.6f seconds\n",mainkb.prstats.elapsed_time);
	if (cpuavail){
		printf("CPU time: %.6f seconds\n",cpu_time);
	}
	printf("**************************************\n");
	cpu_time=0.0;

	/* Accumulate global statistics. After the PrintStats(0) call */
	/* the main KB has the current problem accumulated statistics. */
	tot_pbms++;
	if ((procctl->status==SATISFIABLE)||(procctl->status==UNSATISFIABLE)){
		solved_pbms++;
	}
	glblstats.elapsed_time+=mainkb.prstats.elapsed_time;
	if (glblstats.memory<kbset.prstats.memory){
		glblstats.memory=kbset.prstats.memory;
		glblstats.netmemory=kbset.prstats.netmemory;
	}

	/* Reset working KB's statistics. */
	ResetStats(&kbset.prstats);

	/* Initialize main KB. */
	FreeInfFormulas();
	if (NOMEMORY==Initialize_KB(&mainkb)){
		printf("No memory available, KB not initialized\n");
	}
	ResetStats(pbmstats);
	procctl->status=STOPPED;

	return;
} /* GoSat */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Apply changes of current interpretation)  OCJ
 *
 *    This function apply the necessary changes corresponding to
 *    changes in current model interpretation:
 *    - Unlock the locked A-clauses unlocked by the new model.
 *    - Lock the unlocked A-clauses locked by the new model.
 *    - Add non existing component clauses for components in
 *      the new model.
 *
 *
 *  ARGUMENTS:
 *
 *    None.
 *
 *  RETURNS:
 *
 *    0 -> Model changes applied.
 *    NOMEMORY -> Not enough memory.
 *    TIMEOUT -> Timeout condition.
 *
 *--------------------------------------------------------------*/
int32_t ApplyModelChanges(void){
	auto hashchcmp *symbol;                              /* Pointer to component symbol */
	auto int32_t ii;                                     /* Auxiliary */

	 /* Update locks signature. */
	kbset.locksignature++;

	/* Loop through all hashchcmp elements for this KB. As SAT solver */
	/* is not running then all hashchcmp elements are unassigned and so */
	/* they are all in the undefined queue. */
	ii=0; /* Just to avoid compiler warnings. */
	for (symbol=kbset.frsthshundf;symbol!=NULL;symbol=symbol->nextundef){

		/* Check if the model has changed for this hashchcmp component. */
		switch ((MODELPOSITIVE|MODELNEGATIVE|MODELUNDEFINED)&symbol->flags){
			case MODELPOSITIVE:
				if (PRPOSITIVE&symbol->flags){
					ii=0;
				} else {
					ii=1;
				}
				break;
			case MODELNEGATIVE:
				if (PRNEGATIVE&symbol->flags){
					ii=0;
				} else {
					ii=1;
				}
				break;
			case MODELUNDEFINED:
				if (PRUNDEFINED&symbol->flags){
					ii=0;
				} else {
					ii=1;
				}
				break;
		}

		/* Model has changed for the component of this node. */
		if (ii){

			/* Remove locks unlocked by model change. */
			if (0!=(ii=RemoveLocks(symbol))){
				return(ii);
			}

			/* Do new locks produced by model change. */
			if (0!=(ii=DoLocks(symbol))){
				return(ii);
			}

			/* Add non existing component clauses for components in */
			/* the new model. */
			if (NOMEMORY==AddCompClause(symbol)){
				return(NOMEMORY);
			}
		}
	}

	return(0);
} /* ApplyModelChanges */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Remove locks unlocked by model change)  OCJ
 *
 *    This function unlocks the locked A-clauses that are unlocked by
 *    the change of the value of a component in the new interpretation.
 *
 *
 *  ARGUMENTS:
 *
 *    component: hashchcmp component.
 *
 *  RETURNS:
 *
 *    0 -> Model changes applied.
 *    TIMEOUT -> Timeout condition.
 *    NOMEMORY -> Not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t RemoveLocks(hashchcmp *component){
	auto asymbol *symbol;                                /* Pointer to asymbol in assertion or locking assertion literal */
	auto binprefix *binp;                                /* Pointer to binpointer owning symbol */
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto asymbol *ptr2;                                  /* Auxiliary pointer */
	auto int32_t ii;                                     /* Auxiliary */

	/* Unlock the A-clauses with lock assertions. */
	/* Loop through A-clause lock assertion literals. */
	for (symbol=component->firstlock;symbol!=NULL;symbol=ptr2){
		ptr2=symbol->next;

		/* Check timeout and solution by other process. If there is a solution */
		/* by "other process" but the other process is this process then this is */
		/* a call from VerifySatProof() function and it is not really a solution */
		/* by "other process". */
		if ((procctl->status==TIMEOUT)||(((procctl->status==UNSATISFIABLE)
				||(procctl->status==SATISFIABLE))&&(procnb!=procctl->solvekb))){
			return(TIMEOUT);
		}

		/* Clause has not been processed before. */
		binp=symbol->part2.binp;
		if (binp->locksignature<kbset.locksignature){

			/* Remove locks from locked A-clause. */
			UnchainLocks(binp->lockasserts);
			MYFREE(binp->lockasserts);
			binp->lockasserts=NULL;
			binp->lasize=0;

			/* Check if the clause must be kept locked by new values of its assertions. */
			if (binp->ovly.asserts!=NULL){
				for (ptr1=&binp->ovly.asserts[0],ii=1;ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol))){
					if (ptr1[0]&POSITIVE){
						if (0==(MODELPOSITIVE&((asymbol *)&ptr1[1])->symbol->flags)){
							ii=0;
							break;
						}
					} else {
						if (0==(MODELNEGATIVE&((asymbol *)&ptr1[1])->symbol->flags)){
							ii=0;
							break;
						}
					}
				}
			} else {
				ii=1;
			}

			/* Clause must be unlocked so unlock it. Unlink() cannot return NOMEMORY */
			/* because locked clauses are not indexed by discrimination trees. */
			if (ii){
				Unlink(binp->clause,&kbset);
				binp->clause->flags&=(~LOCKED);
				binp->clause->flags|=UNPROC;
				if (NOMEMORY==AddBinClause2KB(binp->clause,&kbset,0)){
					MYFREE(binp->clause);
					MYFREE(binp->ovly.asserts);
					MYFREE(binp);
					return(NOMEMORY);
				}

				/* Debug. */
				#ifdef DEBUGCODE
				auto char *ptr10;
				ptr10=Decompile(binp);
				#ifdef VERBOSE
				printf("Clause unlocked:\n%s\n",ptr10);
				#endif
				MYFREE(ptr10);
				#endif
			}

			/* Sign clause. */
			binp->locksignature=kbset.locksignature;
		}
	}

	/* Unlock the A-clauses without lock assertions. */
	/* Loop through A-clause assertion literals. */
	switch ((MODELPOSITIVE|MODELNEGATIVE|MODELUNDEFINED)&component->flags){
		case MODELNEGATIVE:
			symbol=component->frstnegassrt;
			break;
		case MODELPOSITIVE:
			symbol=component->frstposassrt;
			break;
		case MODELUNDEFINED:
			symbol=NULL;
			break;
	}
	for (;symbol!=NULL;symbol=symbol->next){

		/* Check timeout and solution by other process. If there is a solution */
		/* by "other process" but the other process is this process then this is */
		/* a call from VerifySatProof() function and it is not really a solution */
		/* by "other process". */
		if ((procctl->status==TIMEOUT)||(((procctl->status==UNSATISFIABLE)
				||(procctl->status==SATISFIABLE))&&(procnb!=procctl->solvekb))){
			return(TIMEOUT);
		}

		/* Clause has not been processed before and has no locking assertions. */
		if ((symbol->part2.binp->locksignature<kbset.locksignature)
				&&(symbol->part2.binp->lockasserts==NULL)){

			/* Check if the clause must be kept locked by new values of its assertions. */
			if (symbol->part2.binp->ovly.asserts!=NULL){
				for (ptr1=&symbol->part2.binp->ovly.asserts[0],ii=1;ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol))){
					if (ptr1[0]&POSITIVE){
						if (0==(MODELPOSITIVE&((asymbol *)&ptr1[1])->symbol->flags)){
							ii=0;
							break;
						}
					} else {
						if (0==(MODELNEGATIVE&((asymbol *)&ptr1[1])->symbol->flags)){
							ii=0;
							break;
						}
					}
				}
			} else {
				ii=1;
			}

			/* Clause must be unlocked so unlock it. Unlink() cannot return NOMEMORY */
			/* because locked clauses are not indexed by discrimination trees. */
			if (ii){
				Unlink(symbol->part2.binp->clause,&kbset);
				symbol->part2.binp->clause->flags&=(~LOCKED);
				symbol->part2.binp->clause->flags|=UNPROC;
				if (NOMEMORY==AddBinClause2KB(symbol->part2.binp->clause,&kbset,0)){
					MYFREE(symbol->part2.binp->clause);
					MYFREE(symbol->part2.binp->ovly.asserts);
					MYFREE(symbol->part2.binp);
					return(NOMEMORY);
				}

				/* Debug. */
				#ifdef DEBUGCODE
				auto char *ptr10;
				ptr10=Decompile(symbol->part2.binp);
				#ifdef VERBOSE
				printf("Clause unlocked:\n%s\n",ptr10);
				#endif
				MYFREE(ptr10);
				#endif
			}

			/* Sign clause. */
			symbol->part2.binp->locksignature=kbset.locksignature;
		}
	}

	return(0);
} /* RemoveLocks */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Do new locks produced by model change)  OCJ
 *
 *    This function locks the A-clauses that are locked by the change
 *    of the value of a component in the new interpretation.
 *
 *
 *  ARGUMENTS:
 *
 *    component: hashchcmp component.
 *
 *  RETURNS:
 *
 *    0 -> Clauses locked successfully changes applied.
 *    NOMEMORY -> Not enough memory
 *    TIMEOUT -> Timeout condition.
 *
 *--------------------------------------------------------------*/
int32_t DoLocks(hashchcmp *component){
	auto asymbol *symbol;                                /* Pointer to asymbol in assertion literal */
	auto asymbol *nxtsymbol;                             /* Pointer to next asymbol in of the same class */

	/* Component is POSITIVE or UNDEFINED in the model. */
	if ((MODELPOSITIVE|MODELUNDEFINED)&component->flags){

		/* Loop through NEGATIVE literals corresponding to the component. */
		for (symbol=component->frstnegassrt;symbol!=NULL;symbol=nxtsymbol){
			nxtsymbol=symbol->next;

			/* Clause has not been processed before and it is not locked. */
			if ((symbol->part2.binp->locksignature<kbset.locksignature)
					&&(0==(LOCKED&symbol->part2.binp->clause->flags))){

				/* Check timeout and solution by other process. If there is a solution */
				/* by "other process" but the other process is this process then this is */
				/* a call from VerifySatProof() function and it is not really a solution */
				/* by "other process". */
				if ((procctl->status==TIMEOUT)||(((procctl->status==UNSATISFIABLE)
						||(procctl->status==SATISFIABLE))&&(procnb!=procctl->solvekb))){
					return(TIMEOUT);
				}

				/* Sign and lock the clause. */
				symbol->part2.binp->locksignature=kbset.locksignature;
				if (NOMEMORY==Unlink(symbol->part2.binp->clause,&kbset)){
					return(NOMEMORY);
				}
				symbol->part2.binp->clause->flags&=(~(UNPROC|ACTIVE|PASSIVE));
				symbol->part2.binp->clause->flags|=LOCKED;
				if (NOMEMORY==AddBinClause2KB(symbol->part2.binp->clause,&kbset,0)){
					MYFREE(symbol->part2.binp->clause);
					MYFREE(symbol->part2.binp->ovly.asserts);
					MYFREE(symbol->part2.binp);
					return(NOMEMORY);
				}

				/* Debug. */
				#ifdef DEBUGCODE
				auto char *ptr10;
				ptr10=Decompile(symbol->part2.binp);
				#ifdef VERBOSE
				printf("Clause locked:\n%s\n",ptr10);
				#endif
				MYFREE(ptr10);
				#endif
			}
		}
	}

	/* Component is NEGATIVE or UNDEFINED in the model. */
	if ((MODELNEGATIVE|MODELUNDEFINED)&component->flags){

		/* Loop through POSITIVE literals corresponding to the component. */
		for (symbol=component->frstposassrt;symbol!=NULL;symbol=nxtsymbol){
			nxtsymbol=symbol->next;

			/* Clause has not been processed before and it is not locked. */
			if ((symbol->part2.binp->locksignature<kbset.locksignature)
					&&(0==(LOCKED&symbol->part2.binp->clause->flags))){

				/* Check timeout and solution by other process. If there is a solution */
				/* by "other process" but the other process is this process then this is */
				/* a call from VerifySatProof() function and it is not really a solution */
				/* by "other process". */
				if ((procctl->status==TIMEOUT)||(((procctl->status==UNSATISFIABLE)
						||(procctl->status==SATISFIABLE))&&(procnb!=procctl->solvekb))){
					return(TIMEOUT);
				}

				/* Sign and lock the clause. */
				symbol->part2.binp->locksignature=kbset.locksignature;
				if (NOMEMORY==Unlink(symbol->part2.binp->clause,&kbset)){
					return(NOMEMORY);
				}
				symbol->part2.binp->clause->flags&=(~(UNPROC|ACTIVE|PASSIVE));
				symbol->part2.binp->clause->flags|=LOCKED;
				if (NOMEMORY==AddBinClause2KB(symbol->part2.binp->clause,&kbset,0)){
					MYFREE(symbol->part2.binp->clause);
					MYFREE(symbol->part2.binp->ovly.asserts);
					MYFREE(symbol->part2.binp);
					return(NOMEMORY);
				}

				/* Debug. */
				#ifdef DEBUGCODE
				auto char *ptr10;
				ptr10=Decompile(symbol->part2.binp);
				#ifdef VERBOSE
				printf("Clause locked:\n%s\n",ptr10);
				#endif
				MYFREE(ptr10);
				#endif
			}
		}
	}

	return(0);
} /* DoLocks */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add non existing component clauses)  OCJ
 *
 *    This function adds non existing component clauses for components
 *    depending on the value of the component in the model. The clause
 *    is added to the UNPROC queue.
 *
 *
 *  ARGUMENTS:
 *
 *    component: hashchcmp component.
 *
 *  RETURNS:
 *
 *    0 -> No errors.
 *    NOMEMORY -> Not enough memory
 *
 *--------------------------------------------------------------*/
int32_t AddCompClause(hashchcmp *component){
	auto cmprefix *clause;                               /* Pointer to component clause */

	/* Return if there is no need to add a component clause. */
	if ((MODELUNDEFINED&component->flags)||((MODELPOSITIVE&component->flags)&&(POSCOMPADDED&component->flags))
			||((MODELNEGATIVE&component->flags)&&((NEGCOMPADDED&component->flags)||(0==(GROUND&component->flags))))){
		return(0);
	}

	/* Allocate storage for the component clause. */
	if (NULL==(clause=MYALLOC(sizeof(cmprefix)))){
		return(NOMEMORY);
	}
	if (NULL==(clause->part2.bin=MYALLOC(binpsize+component->size))){
		MYFREE(clause);
		return(NOMEMORY);
	}
	if (NULL==(clause->part2.bin->ovly.asserts=MYALLOC(2+sizeof(asymbol)))){
		MYFREE(clause->part2.bin);
		MYFREE(clause);
		return(NOMEMORY);
	}

	/* Setup component clause. */
	clause->parent1=(cmprefix *)component;
	clause->parent2=clause->prevdisp=NULL;
	if (MODELPOSITIVE&component->flags){
		clause->number=component->defnumber+1;
	} else {
		clause->number=component->defnumber+2;
	}
	clause->inference=COMPONENT;
	clause->flags=UNPROC|NUMBERED;
	clause->part2.bin->clause=clause;
	clause->part2.bin->lockasserts=NULL;
	clause->part2.bin->agedist=component->agedist;
	clause->part2.bin->sinedist=component->sinedist;
	clause->part2.bin->asize=2+sizeof(asymbol);
	clause->part2.bin->lasize=0;
	clause->part2.bin->oriented=0; /* Just in case. */
	memcpy(&clause->part2.bin->formula[0],component->formula,component->size);
	if (MODELPOSITIVE&component->flags){
		clause->part2.bin->ovly.asserts[0]=POSITIVE;
		component->flags|=POSCOMPADDED;
	} else {
		clause->part2.bin->formula[0]|=NEGATED;
		clause->part2.bin->ovly.asserts[0]=NEGATIVE;
		component->flags|=NEGCOMPADDED;
	}
	((asymbol *)&clause->part2.bin->ovly.asserts[1])->symbol=component;
	((asymbol *)&clause->part2.bin->ovly.asserts[1])->part2.binp=clause->part2.bin;
	clause->part2.bin->ovly.asserts[1+sizeof(asymbol)]=UNITEND;
	UpdateParams(clause->part2.bin);

	/* Add component clause to KB as UNPROC. */
	/* to indexing trees. */
	if (NOMEMORY==AddBinClause2KB(clause,&kbset,0)){
		MYFREE(clause->part2.bin->ovly.asserts);
		MYFREE(clause->part2.bin);
		MYFREE(clause);
		return(NOMEMORY);
	}

	/* Debug. */
	#ifdef DEBUGCODE
	auto char *ptr10;
	ptr10=Decompile(clause->part2.bin);
	#ifdef VERBOSE
	printf("Component added:\n%s\n",ptr10);
	#endif
	MYFREE(ptr10);
	#endif

	return(0);
} /* AddCompClause */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add contradiction clause to SAT queue)  OCJ
 *
 *    This function adds a contradiction clause to the SAT queue
 *    of the working KB. Contradiction clauses are split clauses
 *    generated by empty clauses with assertions. It is assumed
 *    that the input clause comply with these conditions and it
 *    is unlinked, but these conditions are not checked here.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause generating the contradiction clause.
 *
 *  RETURNS:
 *
 *    0 -> No errors.
 *    NOMEMORY -> Not enough memory
 *
 *--------------------------------------------------------------*/
int32_t BuildContrClause(cmprefix *clause){
	auto satclause *satcl;                               /* Pointer to SAT clause structure. */
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto char *ptr10;                                    /* Pointer to buffer for clause in text form. */
	auto int32_t ii;                                     /* Auxiliary */

	/* Allocate storage for the contradiction clause. */
	if (NULL==(satcl=MYALLOC(satclsize+clause->part2.bin->asize))){
		return(NOMEMORY);
	}

	/* Chain the contradiction clause to the global SAT queue. */
	satcl->glblnext=NULL;
	if (kbset.lastsatq!=NULL){
		kbset.lastsatq->glblnext=satcl;
	} else {
		kbset.firstsatq=satcl;
	}
	satcl->glblprev=kbset.lastsatq;
	kbset.lastsatq=satcl;

	/* Build the contradiction clause. */
	memcpy(&satcl->formula[0],clause->part2.bin->ovly.asserts,clause->part2.bin->asize);
	for (ptr1=&satcl->formula[0],ii=0;ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol)),ii++){
		ptr1[0]^=(POSITIVE|NEGATIVE);
		((asymbol *)&ptr1[1])->part2.satcl=satcl;
	}
	satcl->literals=ii;
	satcl->parent=clause;
	satcl->inference=CONTRADICTION;

	/* Convert input clause to text. */
	if (NULL==(ptr10=Decompile(clause->part2.bin))){
		return(NOMEMORY);
	}
	MYFREE(clause->part2.bin->ovly.asserts);
	MYFREE(clause->part2.bin);
	clause->part2.text=ptr10;
	AddTxtFormula2KB(clause,clause->inference,&kbset);

	/* Assign a number to contradiction clause. This is done after the */
	/* generated input clause is added as text formula to ensure that */
	/* the clause numbers are in proper sequence. */
	kbset.nformulas++;
	satcl->number=kbset.nformulas;
	return(0);
} /* BuildContrClause */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check tautologies and remove duplicates in SAT clause)  OCJ
 *
 *    This function checks tautologies and remove duplicates in
 *    a SAT clause. If the clause is a tautology then its storage
 *    is freed.
 *
 *    If there is a tautology or a NOMEMORY error then the SAT clause
 *    is deleted.
 *
 *    IMPORTANT: This function must never be called for main KB.
 *
 *  ARGUMENTS:
 *
 *    psatcl: Address of pointer of SAT clause to be simplified.
 *
 *  RETURNS:
 *
 *    0 if clause is not a tautology and duplicate literals were removed.
 *    1 if clause is a tautology and its storage was freed.
 *    NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t SatSimplify(satclause **psatcl){
	auto uint8_t *ptr1,*ptr2;                            /* Auxiliary pointers */
	auto satclause *ptr5;                                /* Auxiliary pointer */
	auto int32_t ii,jj,kk;                               /* Auxiliary */

	/* Loop through clause literals until next to last literal. */
	for (ptr1=&(*psatcl)->formula[0],ii=jj=0;ptr1[1+sizeof(asymbol)]!=UNITEND;ptr1+=(1+sizeof(asymbol)),jj++){

		/* Loop through literals following current literal. */
		for (ptr2=ptr1+1+sizeof(asymbol),kk=jj+2;ptr2[0]!=UNITEND;ptr2+=(1+sizeof(asymbol)),kk++){

			/* Atoms of both literals are the same. */
			if ((((asymbol *)&ptr1[1])->symbol)==(((asymbol *)&ptr2[1])->symbol)){

				/* If literals have different polarity then this is a tautology. */
				/* Free clause storage and return. */
				if ((ptr1[0]&(POSITIVE|NEGATIVE))!=(ptr2[0]&(POSITIVE|NEGATIVE))){
					kbset.prstats.sattautologies++;
					MYFREE(*psatcl);
					return(1);
				}

				/* Literals are duplicated. Delete second literal. */
				ii=1;
				memmove(ptr2,ptr2+1+sizeof(asymbol),(1+((*psatcl)->literals-kk)*(1+sizeof(asymbol))));
				(*psatcl)->literals--;
				ptr2-=(1+sizeof(asymbol));
				kk--;
			}
		}
	}

	/* Duplicate literals were removed. */
	if (ii){

		/* Reallocate storage to exact size. */
		kbset.prstats.satremovedups++;
		if (NULL==(ptr5=MYREALLOC(*psatcl,satclsize+1+((*psatcl)->literals*(sizeof(asymbol)+1))))){
			MYFREE(*psatcl);
			return(NOMEMORY);
		}

		/* If clause address has changed then update its pointer and the satcl field of */
		/* the asymbol structures in the literals. */
		if (ptr5!=*psatcl){
			*psatcl=ptr5;
			for (ptr1=&(*psatcl)->formula[0];ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol))){
				((asymbol *)&ptr1[1])->part2.satcl=ptr5;
			}
		}
	}

	return(0);
} /* SatSimplify */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform by and to Sat clause subsumptions)  OCJ
 *
 *    This function performs subsumptions of the given Sat clause by
 *    other SAT clauses in the sat solver and subsumptions of sat
 *    solver clauses by the given Sat clause, which must be a clause
 *    not yet added to the Sat solver clauses.
 *
 *    Subsumed clauses are deleted, both the given clause or subsumed
 *    Sat solver clauses.
 *
 *    IMPORTANT: This function must never be called for main KB.
 *
 *  ARGUMENTS:
 *
 *    satcl: Pointer of SAT clause to be checked for subsumptions by and to.
 *
 *  RETURNS:
 *
 *    0 if clause is clause was not subsumed.
 *    1 if clause is clause was subsumed.
 *
 *--------------------------------------------------------------*/
int32_t SatSubsumption(satclause *satcl){
	auto uint8_t *ptr1,*ptr2;                            /* Auxiliary pointers */
	auto asymbol *ptr4,*ptr5,*ptr6;                      /* Auxiliary pointers */

	/* Check if clause is subsumed by some clause in the Sat solver. As all the */
	/* literals in the subsuming clause must match literals in the subsumed clause */
	/* it is only necessary to check matching literals of the first literal in the */
	/* subsumed clause. */
	/* Loop through SAT solver clause literals with the same atom as the first literal. */
	for (ptr4=((asymbol *)&satcl->formula[1])->symbol->frstsat;ptr4!=NULL;ptr4=ptr4->next){

		/* Both literals have the same polarity. */
		ptr2=((uint8_t *)ptr4)-1;
		if ((ptr2[0]&(POSITIVE|NEGATIVE))==(satcl->formula[0]&(POSITIVE|NEGATIVE))){

			/* The number of literals in candidate subsuming clause is less than */
			/* or equal to the number of literals in candidate subsumed clause. */
			if (ptr4->part2.satcl->literals<=satcl->literals){

				/* If clause is subsumed then delete it and return. */
				if (IsSatClSubsumed(ptr4->part2.satcl,satcl,ptr2)){
					kbset.prstats.satsubsum++;
					MYFREE(satcl);
					return(1);
				}
			}
		}
	}

	/* Check if clause subsumes clauses in the Sat solver. */
	/* Loop through clause literals. */
	for (ptr1=&satcl->formula[0];ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol))){

		/* Loop through SAT solver clause literals with the same atom as the current loop literal. */
		for (ptr4=((asymbol *)&ptr1[1])->symbol->frstsat;ptr4!=NULL;ptr4=ptr6){
			ptr6=ptr4->next;

			/* Both literals have the same polarity. */
			ptr2=((uint8_t *)ptr4)-1;
			if ((ptr1[0]&(POSITIVE|NEGATIVE))==(ptr2[0]&(POSITIVE|NEGATIVE))){

				/* The number of literals in candidate subsuming clause is less than */
				/* the number of literals in candidate subsumed clause. */
				if (satcl->literals<ptr4->part2.satcl->literals){

					/* If clause is subsumed then delete it. */
					if (IsSatClSubsumed(satcl,ptr4->part2.satcl,ptr1)){
						kbset.prstats.satsubsum++;

						/* Unlink literal symbols from their queue. */
						/* Loop through subsumed clause literals. */
						for (ptr2=&ptr4->part2.satcl->formula[0];ptr2[0]!=UNITEND;ptr2+=(1+sizeof(asymbol))){
							ptr5=(asymbol *)&ptr2[1];
							if (ptr5->prev!=NULL){
								ptr5->prev->next=ptr5->next;
							} else {
								ptr5->symbol->frstsat=ptr5->next;
							}
							if (ptr5->next!=NULL){
								ptr5->next->prev=ptr5->prev;
							}
						}

						/* Unlink Sat solver clause from the ordered queue. */
						UnchainSatClause(ptr4->part2.satcl);

						/* Unlink Sat solver clause from the global SAT queue */
						/* and free the clause. */
						if (ptr4->part2.satcl->glblprev!=NULL){
							ptr4->part2.satcl->glblprev->glblnext=ptr4->part2.satcl->glblnext;
						} else {
							kbset.frstsatglbl=ptr4->part2.satcl->glblnext;
						}
						if (ptr4->part2.satcl->glblnext!=NULL){
							ptr4->part2.satcl->glblnext->glblprev=ptr4->part2.satcl->glblprev;
						} else {
							kbset.lastsatglbl=ptr4->part2.satcl->glblprev;
						}
						MYFREE(ptr4->part2.satcl);
					}
				}
			}
		}
	}

	return(0);
} /* SatSubsumption */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if a Sat clause subsumes another Sat clause)  OCJ
 *
 *    This function checks if satcl1 Sat clause subsumes satcl2 Sat clause.
 *
 *    It is assumed that the number of literals in satcl1 is less than or
 *    equal to the number of literals in satcl2 but this is not checked.
 *
 *  ARGUMENTS:
 *
 *    satcl1: Pointer to candidate subsuming Sat clause.
 *    satcl2: Pointer to candidate subsumed Sat clause.
 *    literal: Pointer to literal in satcl1 that is guaranteed to match
 *             a literal in satcl2 and therefore a matching literal
 *             must not be searched.
 *
 *  RETURNS:
 *
 *    0 if satcl1 does not subsume satcl2.
 *    1 if satcl1 subsumes satcl2.
 *
 *--------------------------------------------------------------*/
int32_t IsSatClSubsumed(satclause *satcl1,satclause *satcl2,uint8_t *literal){
	auto uint8_t *ptr1,*ptr2;                            /* Auxiliary pointers */
	auto int32_t ii;                                     /* Auxiliary */

	/* Loop through satcl1 literals. */
	for (ptr1=&satcl1->formula[0];ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol))){

		/* A matching literal in satcl2 must be searched. */
		if (ptr1!=literal){

			/* Loop through satcl2 literals. */
			for (ptr2=&satcl2->formula[0],ii=1;ptr2[0]!=UNITEND;ptr2+=(1+sizeof(asymbol))){

				/* Atoms of both literals match. */
				if (((asymbol *)&ptr1[1])->symbol==((asymbol *)&ptr2[1])->symbol){

					/* Both literals have the same polarity. */
					if ((ptr1[0]&(POSITIVE|NEGATIVE))==(ptr2[0]&(POSITIVE|NEGATIVE))){
						ii=0;
						break;
					}
				}
			}

			/* There is no matching literal. Return no subsumption. */
			if (ii){
				return(0);
			}
		}
	}

	/* Return valid subsumption. */
	return(1);
} /* IsSatClSubsumed */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Backtrack from a contradiction)  OCJ
 *
 *    This function identifies a node to which backtracking must
 *    be done after a contradiction is found by the SAT solver and
 *    backtracks to that node if necessary. The function also sets
 *    the field inproof of appropriate SAT clauses to identify
 *    a (non necessarily minimum) unsatisfiable core to be used
 *    in the proof if a SAT refutation is found.
 *
 *    This algorithm is based on the Chaff algorithm, however it is
 *    not a strict implementation of it. See "Validating SAT Solvers
 *    Using an Independent Resolution-Based Checker: Practical
 *    Implementations and Other Applications" by Lintao Zhang and
 *    Sharad Malik.
 *
 *  ARGUMENTS:
 *
 *    contrcl: Contradiction clause, that is, clause with all literals
 *             unsatisfied after the assignment of the value in node.
 *    pnode: As input this is the address of a pointer to the node in
 *           SAT solver search tree that produced the contradiction.
 *           As output this is the addres of the pointer to the node
 *           in which backtracking stopped. The backtracking includes
 *           the undo of the value assigned by the node. In case of
 *           SAT refutation this is NULL.
 *
 *  RETURNS:
 *
 *    0 if no SAT refutation was found.
 *    1 if a SAT refutation was found.
 *    NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t Backtrack(satclause *contrcl,satnode **pnode){
	auto satnode *crnode;                                /* Critical node */
	auto uint8_t *formula;                               /* Conflicting formula */
	auto int32_t literals;                               /* Number of literals in conflicting clause */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr6;                /* Auxiliary pointers */
	auto satnode *ptr4,*ptr5;                            /* Auxiliary pointers */
	auto int32_t ii,jj;                                  /* Auxiliary */

	/* Set inproof field of contradiction clause. */
	contrcl->inproof=1;

	/* Traverse the critical nodes. No backtracking is done yet. */
	crnode=*pnode;
	formula=&contrcl->formula[0];
	literals=contrcl->literals;
	ptr6=NULL;
	ii=0;
	while (1){

		/* The critical node includes a forced clause. */
		if (crnode->forcedcl!=NULL){

			/* Set inproof field of forced clause. */
			crnode->forcedcl->inproof=1;

			/* Allocate memory for new conflicting clause if necessary. */
			jj=literals+crnode->forcedcl->literals-2;
			if (NULL==(ptr1=MYALLOC(1+jj*(1+sizeof(asymbol))))){
				if (ii!=0){
					MYFREE(ptr6);
				}
				return(NOMEMORY);
			}

			/* Build the new conflicting formula by resolution. The new */
			/* conflicting formula will have all its literals assigned and */
			/* unsatisfied. First copy the appropriate literals of current */
			/* conflicting formula. */
			for(ptr2=formula,ptr3=ptr1,jj=0;ptr2[0]!=UNITEND;ptr2+=(1+sizeof(asymbol))){
				if (((asymbol *)&ptr2[1])->symbol!=crnode->symbol){
					memcpy(ptr3,ptr2,1+sizeof(asymbol));
					ptr3+=(1+sizeof(asymbol));
					jj++;
				}
			}

			/* Now copy the appropriate literals of forced clause. */
			for(ptr2=&crnode->forcedcl->formula[0];ptr2[0]!=UNITEND;ptr2+=(1+sizeof(asymbol))){
				if (((asymbol *)&ptr2[1])->symbol!=crnode->symbol){
					memcpy(ptr3,ptr2,1+sizeof(asymbol));
					ptr3+=(1+sizeof(asymbol));
					jj++;
				}
			}
			ptr3[0]=UNITEND;

			/* Free the current conflicting formula storage and update */
			/* formula and number of literals. */
			if (ii==0){
				ii=1;
			} else {
				MYFREE(ptr6);
			}
			formula=ptr6=ptr1;
			literals=jj;

			/* The new conflicting clause is not empty. Get the next */
			/* critical node corresponding to the last assigned literal */
			/* in the new conflicting formula. */
			if (jj){
				crnode=((asymbol *)&formula[1])->symbol->satnode;
				for(ptr2=&formula[1+sizeof(asymbol)];ptr2[0]!=UNITEND;ptr2+=(1+sizeof(asymbol))){
					if (((asymbol *)&ptr2[1])->symbol->satnode->number>crnode->number){
						crnode=((asymbol *)&ptr2[1])->symbol->satnode;
					}
				}
			}

		/* Indicate that critical node doesn't include a forced clause. */
		} else {
			jj=0;
		}

		/* The critical node doesn't include a forced clause or */
		/* the new conflicting clause is empty. Go back until a */
		/* node with another option is found, remember input pnode, */
		/* set pnode and leave the loop. */
		if (jj==0){
			MYFREE(ptr6);
			for (;(crnode!=NULL)&&(LASTOPTION&crnode->flags);crnode=crnode->prevnode){
				if (crnode->forcedcl!=NULL){
					crnode->forcedcl->inproof=1;
				}
			}
			ptr4=*pnode;
			*pnode=crnode;
			break;
		}
	}

	/* If a SAT refutation was found and printing the KB data option */
	/* is enabled or SAT final status must be verified then return */
	/* without backtracking. */
	if (((*pnode)==NULL)&&((prtkbdata)||(VERIFYSATFINALSTATUS))){
		return(1);
	}

	/* Backtrack. Undo symbol assignment and mark the node as free. */
	/* The node assignment flags (POSITIVE or NEGATIVE) are not */
	/* reset because they may be necessary to build the proof. */
	for (ptr5=ptr4;(ptr4!=NULL)&&((ptr4->nextnode!=*pnode)||(ptr4==ptr5));ptr4=ptr4->prevnode){
		UndoSymbolAssgnmnt(ptr4);
		ptr4->flags|=FREENODE;
	}
	return((*pnode)==NULL?1:0);
} /* Backtrack */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Setup subsumption data structures)  OCJ
 *
 *    This function performs all memory allocation and data initialization
 *    and setup needed to run the subsumption SAT Solver.
 *
 *    This function also detects cases in which the subsumption will not
 *    be possible due to a literal in binp2 clause not being a valid
 *    generalization of any literal in formula1.
 *
 *    In order to speed up the subsumption process an additional format
 *    for substitutions will be used. In this format a substitution
 *    is a set of pointers to terms indexed by the variable number.
 *    If the variable is substituted then the pointer points to the
 *    term that substitutes the variable. Otherwise the pointer
 *    is set to NULL.
 *
 *    In case of an early return because NOMEMORY or the subsumption is not
 *    possible the allocated memory so far is also freed in this function.
 *
 *    The function FreeSubsumMem() must be used to free the memory
 *    by other functions once it is no longer needed.
 *
 *  ARGUMENTS:
 *
 *    formula1: Pointer to formula in compiled binary form as it is in the
 *              formula field of binprefix structure od main clause. This
 *              formula will be checked for being subsumed by binp2. It
 *              points to a literal in the clause that is being checked for
 *              being subsumed, but not necessarily to the first literal.
 *              This is useful for forward subsumption, when the first
 *              literals of candidate subsumed clause have been already
 *              checked and found that they cannot participate in the
 *              subsumption.
 *    lits1: Number of literals starting at formula1 pointer.
 *    binp2: Pointer to binprefix structure of side clause in compiled binary
 *           form. This clause will be checked for subsuming formula1.
 *    plit: On entry this is either NULL or the address of a pointer to a
 *          literal in binp2. If it is not NULL on entry and the data setup
 *          is successful then on exit the pointer *plit will be set to the
 *          SAT solver clause corresponding to the original literal pointed
 *          by *plit.
 *
 *  RETURNS:
 *
 *    0 if subsumption is not possible (early pruning).
 *    1 if setup performed OK.
 *    NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t SubsumSetup(uint8_t *formula1,int32_t lits1,binprefix *binp2,uint8_t **plit){
	auto subst **matchset[2];                            /* Set of 2 pointers to substitutions match sets, see comment below */
	auto subst **matches[2];                             /* Set of 2 pointers to sets of substitutions matches */
	auto subslitrl **comptail;                           /* Pointer to set of tail pointers, see comment below */
	auto int32_t components;                             /* Total number of literals in SAT Solver clauses */
	auto size_t memsize;                                 /* Global memory size in bytes */
	auto subst *psubst,*psubst2;                         /* Pointer to substitutions */
	auto subsatcl *solvercl;                             /* Pointer to SAT solver clause */
	auto int32_t sidecls4plit;                           /* Pointer to SAT solver clause number for *plit */
	auto uint8_t *ptr1,*ptr2,*ptr3;                      /* Auxiliary pointers */
	auto int32_t ii,jj,jj1,kk,mm,ss;                     /* Auxiliary */

	/* Allocate and initialize pointer to the substitutions match set matrix. */
	/* Let lm the main clause literal number and ls the side clause literal */
	/* number, both starting at 0. Then matchset[ls*lits1+lm] points to the */
	/* substitution that transforms literal number ls in the side clause into */
	/* literal lm in the main clause. */
	if (NULL==(matchset[0]=calloc(lits1*binp2->literals*sizeof(void *),1))){
		return(NOMEMORY);
	}
	if (NULL==(matchset[1]=calloc(lits1*binp2->literals*sizeof(void *),1))){
		free(matchset[0]);
		return(NOMEMORY);
	}

	/* Allocate and initialize pointer to the substitutions matches. */
	if (NULL==(matches[0]=calloc(lits1*binp2->literals*sizeof(void *),1))){
		free(matchset[0]);
		free(matchset[1]);
		return(NOMEMORY);
	}
	if (NULL==(matches[1]=calloc(lits1*binp2->literals*sizeof(void *),1))){
		free(matchset[0]);
		free(matchset[1]);
		free(matches[0]);
		return(NOMEMORY);
	}

	/* Allocate and initialize pointer to the set of tail pointers. This set */
	/* is indexed by the literal number in main clause (candidate to be subsumed) */
	/* starting at 0. Each pointer in the set points to tail element of the */
	/* linked list of components related to the same main clause literal. */
	if (NULL==(comptail=calloc(lits1*sizeof(void *),1))){
		free(matchset[0]);
		free(matchset[1]);
		free(matches[0]);
		free(matches[1]);
		return(NOMEMORY);
	}

	/* Allocate initial substitutions. */
	if (NULL==(psubst=malloc(sizeof(subst)))){
		free(matchset[0]);
		free(matchset[1]);
		free(matches[0]);
		free(matches[1]);
		free(comptail);
		return(NOMEMORY);
	}
	psubst->buffer=NULL;
	if (NULL==(psubst2=malloc(sizeof(subst)))){
		free(matchset[0]);
		free(matchset[1]);
		free(matches[0]);
		free(matches[1]);
		free(comptail);
		free(psubst);
		return(NOMEMORY);
	}
	psubst2->buffer=NULL;

	/* Build substitution match set. */
	/* Loop for each literal in the side (candidate subsuming) clause. */
	components=0; /* Initialize total number of components. */
	sidecls4plit=-1; /* Indicate no match found of side clause literal. */
	for (ptr1=&binp2->formula[0],ss=0;ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS),ss+=lits1){

		/* Initialize ii to indicate that no matches has been found so far. */
		/* If plit is not NULL then sidecls4plit will be set to the ss counter. */
		ii=0; /* No generalizations found so far. */
		if ((plit!=NULL)&&(ptr1==*plit)){
			sidecls4plit=ss;
		}

		/* Loop for each literal in the main (candidate subsumed) clause. */
		for (ptr2=formula1,mm=0;ptr2[0]!=UNITEND;ptr2=NextItem(ptr2,OVERSUBTERMS),mm++){

			/* Check if side clause literal is a generalization of main clause literal. */
			for (kk=0;(kk==0)||((kk==1)&&(ptr1[0]&EQUALITY));kk++){
				if (kk==0){
					jj=Generalize(ptr1,ptr2,psubst,INITSUBST);
				} else {
					jj1=SymEquGeneralize(ptr1,ptr2,psubst2,INITSUBST);
				}
				switch (kk==0?jj:jj1){

					/* Not enough memory. */
					case NOMEMORY:
						free(matchset[0]);
						free(matchset[1]);
						for (jj=0;jj<2;jj++){
							for (ii=0;ii<components;ii++){
								if (matches[jj][ii]!=NULL){
									MYFREE(matches[jj][ii]->buffer);
									free(matches[jj][ii]);
								}
							}
						}
						free(matches[0]);
						free(matches[1]);
						free(comptail);
						MYFREE(psubst->buffer);
						free(psubst);
						MYFREE(psubst2->buffer);
						free(psubst2);
						return(NOMEMORY);
						break;

					/* Valid generalization. */
					case 1:
						if (kk==0){
							if (psubst->buffer!=NULL){
								psubst->buffer=MYREALLOC(psubst->buffer,psubst->substsize);
								psubst->buffsize=psubst->substsize;
							}
							matchset[0][ss+mm]=matches[0][components]=psubst;
							components++;
							ii=1;
							if (NULL==(psubst=malloc(sizeof(subst)))){
								free(matchset[0]);
								free(matchset[1]);
								for (jj=0;jj<2;jj++){
									for (ii=0;ii<components;ii++){
										if (matches[jj][ii]!=NULL){
											MYFREE(matches[jj][ii]->buffer);
											free(matches[jj][ii]);
										}
									}
								}
								free(matches[0]);
								free(matches[1]);
								free(comptail);
								MYFREE(psubst->buffer);
								free(psubst);
								MYFREE(psubst2->buffer);
								free(psubst2);
								return(NOMEMORY);
							}
							psubst->buffer=NULL;
						} else {
							if (psubst2->buffer!=NULL){
								psubst2->buffer=MYREALLOC(psubst2->buffer,psubst2->substsize);
								psubst2->buffsize=psubst2->substsize;
							}
							if (jj){
								matchset[1][ss+mm]=matches[1][components-1]=psubst2;
							} else {
								matchset[0][ss+mm]=matches[0][components]=psubst2;
								components++;
								ii=1;
							}
							if (NULL==(psubst2=malloc(sizeof(subst)))){
								free(matchset[0]);
								free(matchset[1]);
								for (jj=0;jj<2;jj++){
									for (ii=0;ii<components;ii++){
										if (matches[jj][ii]!=NULL){
											MYFREE(matches[jj][ii]->buffer);
											free(matches[jj][ii]);
										}
									}
								}
								free(matches[0]);
								free(matches[1]);
								free(comptail);
								MYFREE(psubst->buffer);
								free(psubst);
								MYFREE(psubst2->buffer);
								free(psubst2);
								return(NOMEMORY);
							}
							psubst2->buffer=NULL;
						}
						break;
				}
			}
		}

		/* If there is no generalization for this literal in the side clause */
		/* then return that the generalization is impossible. */
		if (ii==0){
			free(matchset[0]);
			free(matchset[1]);
			for (jj=0;jj<2;jj++){
				for (ii=0;ii<components;ii++){
					if (matches[jj][ii]!=NULL){
						MYFREE(matches[jj][ii]->buffer);
						free(matches[jj][ii]);
					}
				}
			}
			free(matches[0]);
			free(matches[1]);
			free(comptail);
			MYFREE(psubst->buffer);
			free(psubst);
			MYFREE(psubst2->buffer);
			free(psubst2);
			return(0);
		}
	}

	/* Initialize new memory independent global control structure. */
	subsctrl.frstsat=subsctrl.frstunsats=subsctrl.lstunsats=subsctrl.frstunsats2=NULL;
	subsctrl.frsteliglit=NULL;
	subsctrl.sidenumvars=binp2->maxvarnb+1;

	/* Allocate and initialize global memory. */
	memsize=subsctrl.sidenumvars*((2+3*(components))*sizeof(void *)+sizeof(*subsctrl.glblvarcount))
		+(binp2->literals)*sizeof(subsatcl)+(components-binp2->literals)*sizeof(subslitrl);
	if (NULL==(subsctrl.allmemory=calloc(memsize,1))){
		free(matchset[0]);
		free(matchset[1]);
		for (jj=0;jj<2;jj++){
			for (ii=0;ii<=components;ii++){
				MYFREE(matches[jj][ii]->buffer);
				free(matches[jj][ii]);
			}
		}
		free(matches[0]);
		free(matches[1]);
		free(comptail);
		MYFREE(psubst->buffer);
		free(psubst);
		MYFREE(psubst2->buffer);
		free(psubst2);
		return(NOMEMORY);
	}

	/* Free unused substitutions and matches buffer. */
	MYFREE(psubst->buffer);
	free(psubst);
	MYFREE(psubst2->buffer);
	free(psubst2);
	free(matches[0]);
	free(matches[1]);

	/* Initialize new memory dependent global control structure. */
	subsctrl.glblextsubst=(uint8_t **)subsctrl.allmemory;
	ptr3=subsctrl.allmemory+(subsctrl.sidenumvars*sizeof(void *));
	subsctrl.glblvarcount=(int32_t *)ptr3;
	ptr3=&ptr3[(subsctrl.sidenumvars*sizeof(int32_t))];
	subsctrl.firstvar=(subslitrl **)ptr3;
	ptr3=&ptr3[(subsctrl.sidenumvars*sizeof(void *))];

	/* Build SAT solver clauses. */
	/* Loop through rows in the match set. */
	for (ss=ii=0;ii<binp2->literals;ii++,ss+=lits1){

		/* Set pointer to SAT solver clause, insert clause in the global queue */
		/* and initialize number of total, undefined and satisfied literals. */
		/* Also if ss is equal to sidecls4plit then *plit will be set to */
		/* a point to this SAT clause. */
		solvercl=(subsatcl *)ptr3;
		solvercl->glblnext=subsctrl.frstsat;
		subsctrl.frstsat=solvercl;
		solvercl->literals=solvercl->satlits=solvercl->undeflits=0;
		if (ss==sidecls4plit){
			*plit=(uint8_t *)solvercl;
		}

		/* Loop through columns (elements) of match set row. */
		for (mm=0;mm<lits1;mm++){

			/* There is a substitution in the match set. */
			if (matchset[0][ss+mm]!=NULL){

				/* Initialize single fields of SAT solver clause. */
				solvercl->formula[solvercl->literals].flags=UNDEFINED;
				solvercl->formula[solvercl->literals].satcl=solvercl;
				solvercl->formula[solvercl->literals].psubst[0]=matchset[0][ss+mm];
				solvercl->formula[solvercl->literals].psubst[1]=matchset[1][ss+mm];
				solvercl->formula[solvercl->literals].mainlitnb=mm;

				/* Insert component in the list of eligible components. */
				solvercl->formula[solvercl->literals].nextelig=subsctrl.frsteliglit;
				solvercl->formula[solvercl->literals].prevelig=NULL;
				if (subsctrl.frsteliglit!=NULL){
					subsctrl.frsteliglit->prevelig=&solvercl->formula[solvercl->literals];
				}
				subsctrl.frsteliglit=&solvercl->formula[solvercl->literals];

				/* Insert component in the list of components of the same main clause literal. */
				/* This is necessary to set AMO (At Most One) constraints. */
				solvercl->formula[solvercl->literals].prevmain=comptail[mm];
				solvercl->formula[solvercl->literals].nextmain=NULL;
				if (comptail[mm]!=NULL){
					comptail[mm]->nextmain=&solvercl->formula[solvercl->literals];
				}
				comptail[mm]=&solvercl->formula[solvercl->literals];

				/* Update number of SAT clause total and undefined literals. */
				solvercl->literals++;
				solvercl->undeflits++;
			}
		}

		/* Update pointer to next global memory unused point. */
		ptr3=&ptr3[sizeof(subsatcl)+((solvercl->literals-1)*sizeof(subslitrl))];

		/* Loop through all literals of new SAT solver clause. */
		for (kk=0;kk<solvercl->literals;kk++){

			/* Initialize varnext and psubst2 fields of each SAT solver clause component. */
			solvercl->formula[kk].varnext=(subslitrl **)ptr3;
			ptr3=&ptr3[(subsctrl.sidenumvars*sizeof(void *))];
			solvercl->formula[kk].psubst2[0]=(uint8_t **)ptr3;
			ptr3=&ptr3[(subsctrl.sidenumvars*sizeof(void *))];
			solvercl->formula[kk].psubst2[1]=(uint8_t **)ptr3;
			ptr3=&ptr3[(subsctrl.sidenumvars*sizeof(void *))];

			/* Loop through both literal substitutions. */
			for (mm=0;mm<2;mm++){

				/* If substitution is not empty then loop through substitution items in normal format. */
				if (((solvercl->formula[kk].psubst[mm]!=NULL))&&(solvercl->formula[kk].psubst[mm]->buffer!=NULL)){
					for (ptr1=solvercl->formula[kk].psubst[mm]->buffer;ptr1[0]!=UNITEND;ptr1=NextItem(&ptr1[3],OVERSUBTERMS)){

						/* Initialize the element of the set pointed by psubst2 corresponding */
						/* to the variable of this substitution item. */
						jj=(*((int16_t *)&ptr1[1]));
						solvercl->formula[kk].psubst2[mm][jj]=&ptr1[3];

						/* Chain the variable in this component to the subsctrl.firstvar linked list. */
						/* Only chain the variable once, even if it is in both substitutions. */
						if ((mm==0)||(solvercl->formula[kk].psubst2[0][jj]==NULL)){
							solvercl->formula[kk].varnext[jj]=subsctrl.firstvar[jj];
							subsctrl.firstvar[jj]=&solvercl->formula[kk];
						}
					}
				}
			}
		}

		/* Insert SAT solver clause in the ordered queue. */
		SbChainSatClause(solvercl);
	}

	/* Free memory and return setup performed OK. */
	free(matchset[0]);
	free(matchset[1]);
	free(comptail);
	return(1);
} /* SubsumSetup */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (SAT solver for subsumption simplification)  OCJ
 *
 *    This function runs the specific SAT solver for sumbumption.
 *    It is specifically designed for checking if a subsumption
 *    is valid, so it is not a general SAT solver. The main
 *    difference is that all solver clauses have positive literals
 *    and that each atom exist only once in only one clause.
 *    The actual clauses are just those to enforce partial
 *    completeness. The multiplicity conservation condition,
 *    also called AMO (At Most One) condition is enforced
 *    directly instead of using solver clauses. The same applies
 *    to substitution compatibility which is also enforced
 *    directly instead of using solver clauses.
 *
 *    This subsumption checking based on a SAT solver follows
 *    the document "SAT solving variants for first-order subsumption"
 *    by Robin Coutelier, Jakob Rath, Michael Rawson, Armin Biere
 *    and Laura Kovács. However, only the part related to subsumption
 *    is implemented. It doen't include subsumption resolution.
 *
 *    This SAT solver algorithm is based on the Chaff algorithm, however
 *    it is not a strict implementation of it. See "Validating SAT
 *    Solvers Using an Independent Resolution-Based Checker: Practical
 *    Implementations and Other Applications" by Lintao Zhang and
 *    Sharad Malik.
 *
 *  ARGUMENTS:
 *
 *    hwinstr: Limit of HW instructions counter value.
 *
 *  RETURNS:
 *
 *    0 if subsumption is not possible or HW instruction limit exceeded
 *      or an error occurred.
 *    1 if valid subsumption.
 *    NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t SbsSatSolver(uint64_t hwinstr){
	auto subsnode *node,*rootnode,*auxnode;              /* Decision level node and root node */
	auto uint64_t hwinstr2;                              /* Current value of HW instructions counter */
	auto int32_t ii,jj;                                  /* Auxiliary */

	/* Allocate first node. */
	if (NULL==(node=rootnode=malloc(sizeof(*node)))){
		return(NOMEMORY);
	}
	node->prevnode=node->nextnode=NULL;

	/* Main SAT solver loop. */
	ii=0;
	while (subsctrl.frstunsats!=NULL){

		/* Check HW instructions limit. */
		ii++;
		if (ii>CHKCYCLES){
			ii=0;
			myread(&hwinstr2);
			if (hwinstr2>=hwinstr){
				return(0);
			}
		}

		/* Select an undefined symbol to be assigned TRUE (POSITIVE). */
		/* First look at the SAT solver clause ordered queue for clauses */
		/* with only one or two undefined literals. */
		if (subsctrl.frstunsats->undeflits<=2){
			for (jj=0;jj<subsctrl.frstunsats->literals;jj++){
				if (UNDEFINED&subsctrl.frstunsats->formula[jj].flags){
					node->symbol=&subsctrl.frstunsats->formula[jj];
					break;
				}
			}
			if (jj==subsctrl.frstunsats->literals){
				printf("=====> ERROR: Unassigned literal not found in subsumption SAT solver clause.\n");
				for (node=rootnode;node!=NULL;node=rootnode){
					rootnode=node->nextnode;
					free(node);
				}
				return(0);
			}

		/* Otherwise take the first symbol in the component ordered queue. */
		} else {
			node->symbol=subsctrl.frsteliglit;
		}

		/* Perform symbol assignment and perform backtrack if there is a contradiction. */
		if (SbDoSymbolAssgnmnt(node)){
			if (SbBacktrack(&node)){
				for (node=rootnode;node!=NULL;node=rootnode){
					rootnode=node->nextnode;
					free(node);
				}
				return(0);
			}
		}

		/* Allocate and link a new node if necessary. */
		if (node->nextnode==NULL){
			if (NULL==(auxnode=malloc(sizeof(*node)))){
				for (node=rootnode;node!=NULL;node=rootnode){
					rootnode=node->nextnode;
					free(node);
				}
				return(NOMEMORY);
			}
			node->nextnode=auxnode;
			auxnode->prevnode=node;
			auxnode->nextnode=NULL;
			node=auxnode;
		} else {
			node=node->nextnode;
		}

	}

	/* Free memory and return valid subsumption */
	for (node=rootnode;node!=NULL;node=rootnode){
		rootnode=node->nextnode;
		free(node);
	}
	return(1);
} /* SbsSatSolver */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Undo symbol assignment for subsumption SAT solver)  OCJ
 *
 *    This function undoes a symbol assignment, including
 *    adjusting SAT clause parameters and unlinking and
 *    chaining SAT clause for clauses affected by symbol
 *    assignment.
 *
 *    IMPORTANT: This function must be called only for TRUE (POSITIVE)
 *    symbol assignments removal. FALSE (NEGATIVE) symbol assignments
 *    removal are done in place when needed. See SbDoSymbolAssgnmnt()
 *    function for additional details.
 *
 *    IMPORTANT: This function doesn't chain the node symbol clause
 *    to the ordered queue. The reason is that this function is
 *    mainly used by SbBacktrack() function and letting SbBacktrack()
 *    do the calls to  SbChainSatClause() when needed saves calls
 *    to SbChainSatClause() and SbUnchainSatClause() functions.
 *
 *
 *  ARGUMENTS:
 *
 *    node: Pointer to node corresponding to symbol assignment to be
 *          reversed.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void SbUndoSymbolAssgnmnt(subsnode *node){
	auto subslitrl *ptr1;                                /* Auxiliary pointer */
	auto uint8_t *ptr2;                                  /* Auxiliary pointer */
	auto int32_t ii,jj;                                  /* Auxiliary */

	/* Set node symbol as undefined, adjust number of undefined and satisfied */
	/* literals and add node symbol it to the eligible list. */
	node->symbol->flags&=~(POSITIVE|NEGATIVE);
	node->symbol->flags|=UNDEFINED;
	node->symbol->satcl->undeflits++;
	node->symbol->satcl->satlits=0;
	node->symbol->nextelig=subsctrl.frsteliglit;
	if (subsctrl.frsteliglit!=NULL){
		subsctrl.frsteliglit->prevelig=node->symbol;
	}
	subsctrl.frsteliglit=node->symbol;

	/* Loop through symbols in the main node propagation list. */
	for (ptr1=node->frstpropg;ptr1!=NULL;ptr1=ptr1->nextpropg){

		/* Add symbol to the eligible list. */
		ptr1->nextelig=subsctrl.frsteliglit;
		if (subsctrl.frsteliglit!=NULL){
			subsctrl.frsteliglit->prevelig=ptr1;
		}
		subsctrl.frsteliglit=ptr1;

		/* The symbol in not undefined. */
		if (0==(ptr1->flags&UNDEFINED)){

			/* Reset the symbol substitution flags that caused the symbol to be added */
			/* to the main propagation list and removed from eligible list. */
			switch (ptr1->flags&(BYSUBST1|BYSUBST2)){
				case BYSUBST1:
					ptr1->flags&=~(SUBST1OFF|BYSUBST1);
					break;
				case BYSUBST2:
					ptr1->flags&=~(SUBST2OFF|BYSUBST2);
					break;
				case BYSUBST1|BYSUBST2:
					ptr1->flags&=~(SUBST1OFF|SUBST2OFF|BYSUBST1|BYSUBST2);
					break;
			}

			/* Set symbol to undefined and re-chain the symbol SAT clause if it is now */
			/* with 3 or less unassigned literals. */
			ptr1->flags&=~(POSITIVE|NEGATIVE);
			ptr1->flags|=UNDEFINED;
			ptr1->satcl->undeflits++;
			if (ptr1->satcl->undeflits<=3){
				SbUnchainSatClause(ptr1->satcl);
				SbChainSatClause(ptr1->satcl);
			}
		}
	}

	/* Loop through symbols in the second node propagation list */
	/* and disable flags SUBST1OFF and SUBST2OFF. */
	for (ptr1=node->frstpropg2;ptr1!=NULL;ptr1=ptr1->nextpropg2){
		ptr1->flags&=~(SUBST1OFF|SUBST2OFF);
	}

	/* Identify the substitution added to the global substitution in the node */
	/* symbol assignment. */
	if (0==((SUBST1OFF|SECONDTRUEASSGN)&node->symbol->flags)){
		jj=0;
	} else {
		jj=1;
	}

	/* For each variable in the symbol substitution decrease its counter in the */
	/* global substitution and cancel that variable if the counter goes to 0. */
	/* If the symbol substitution is not empty then loop through variables in */
	/* the symbol substitution in normal format. */
	if (node->symbol->psubst[jj]->buffer!=NULL){
		for (ptr2=node->symbol->psubst[jj]->buffer;ptr2[0]!=UNITEND;ptr2=NextItem(&ptr2[3],OVERSUBTERMS)){
			ii=*((int16_t *)&ptr2[1]);
			if (ii==node->varnum){
				break;
			}
			subsctrl.glblvarcount[ii]--;
			if (subsctrl.glblvarcount[ii]==0){
				subsctrl.glblextsubst[ii]=NULL;
			}
		}
	}
	return;
} /* SbUndoSymbolAssgnmnt */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Link a subsatcl element to ordered queue)  OCJ
 *
 *    This function links a SAT solver subsumption clause to the SAT
 *    solver ordered queue.
 *
 *    There are two SAT solver queues: the global queue and the ordered
 *    queue. The global queue chains all the SAT solver clauses starting
 *    with the older ones. This function doesn't handles the global queue.
 *
 *    The SAT solver ordered clause queue chains all clauses not yet
 *    satisfied with the following ordering criteria:
 *    - First the not yet satisfied clauses with only one literal.
 *    - Next the not yet satisfied clauses with more than one literal
 *      but only one literal without a value assigned (true or false).
 *    - Next the not yet satisfied clauses with more than one literal
 *      but only two literals without a value assigned (true or false).
 *    - Next all the other not yet satisfied clauses, without any order.
 *      This part of the queue is not ordered. New clauses in this
 *      category are added at the end of the queue.
 *
 *    This function must NOT be called for satisfied clauses, clauses
 *    without undefined literals or clauses already linked to the ordered
 *    queue, otherwise it may cause unpredictable results. Clauses without
 *    undefined literals are either satisfied or cause a contradiction.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to the subsatcl structure of the clause
 *            to be chained.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void SbChainSatClause(subsatcl *clause){
	auto subsatcl *ptr5;                                 /* Auxiliary pointer */

	/* This is the first clause added to the SAT solver ordered queue. */
	if (subsctrl.frstunsats==NULL){
		subsctrl.frstunsats=subsctrl.lstunsats=clause;
		clause->next=clause->prev=NULL;
		if ((clause->undeflits>=2)||(clause->satlits>0)){
			subsctrl.frstunsats2=clause;
		}

	/* This is not the first clause added to the SAT solver. */
	} else {

		/* The clause has more than two undefined literals. */
		/* Link it to the end of the SAT solver queue. */
		if (clause->undeflits>2){
			subsctrl.lstunsats->next=clause;
			clause->prev=subsctrl.lstunsats;
			subsctrl.lstunsats=clause;
			clause->next=NULL;
			if (subsctrl.frstunsats2==NULL){
				subsctrl.frstunsats2=clause;
			}

		/* The clause has one or two undefined literals. Insert it */
		/* in the ordered part of the SAT solver clauses queue. */
		} else {

			/* The clause has only one literal in total, which must be undefined, */
			/* otherwise this function should not be called. */
			if (clause->literals==1){
				clause->next=subsctrl.frstunsats;
				clause->prev=NULL;
				subsctrl.frstunsats->prev=clause;
				subsctrl.frstunsats=clause;

			/* The clause has more than one literal. */
			} else {
				switch (clause->undeflits){

					/* Only one undefined literal. */
					case 1:
						for (ptr5=subsctrl.frstunsats;
								(ptr5!=NULL)&&(ptr5->literals==1)&&(ptr5->undeflits==1);
								ptr5=ptr5->next){
						}
						if (ptr5==NULL){
							clause->prev=subsctrl.lstunsats;
							subsctrl.lstunsats->next=clause;
							clause->next=NULL;
							subsctrl.lstunsats=clause;
						} else {
							if (ptr5->prev!=NULL){
								ptr5->prev->next=clause;
							} else {
								subsctrl.frstunsats=clause;
							}
							clause->next=ptr5;
							clause->prev=ptr5->prev;
							ptr5->prev=clause;
						}
						break;

					/* Two undefined literals. */
					case 2:
						if (subsctrl.frstunsats2==NULL){
							clause->prev=subsctrl.lstunsats;
							subsctrl.lstunsats->next=subsctrl.frstunsats2=clause;
							subsctrl.lstunsats=clause;
							clause->next=NULL;
						} else {
							if (subsctrl.frstunsats2->prev!=NULL){
								subsctrl.frstunsats2->prev->next=clause;
							} else {
								subsctrl.frstunsats=clause;
							}
							clause->next=subsctrl.frstunsats2;
							clause->prev=subsctrl.frstunsats2->prev;
							subsctrl.frstunsats2->prev=clause;
							subsctrl.frstunsats2=clause;
						}
						break;
				}
			}
		}
	}

	return;
} /* SbChainSatClause */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Remove a subsatcl element from the ordered sub-queue)  OCJ
 *
 *    This function removes a subsatcl structure element from the ordered
 *    sub-queue for SAT solver clauses. See SbChainSatClause() function
 *    function for details about this sub-queue.
 *
 *    IMPORTANT: This function must never be called for clauses not linked
 *    to the ordered sub-queue.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to subsatcl structure of clause to chain.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void SbUnchainSatClause(subsatcl *clause){

	/* Remove clause from the ordered queue. */
	if (clause->prev!=NULL){
		clause->prev->next=clause->next;
	} else {
		subsctrl.frstunsats=clause->next;
	}
	if (clause->next!=NULL){
		clause->next->prev=clause->prev;
	} else {
		subsctrl.lstunsats=clause->prev;
	}
	if (subsctrl.frstunsats2==clause){
		subsctrl.frstunsats2=clause->next;
	}

	return;
} /* SbUnchainSatClause */

#ifdef VERIFYSATPROOF
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Verify SAT part of the proof)  OCJ
 *
 *    This function checks if the selected SAT clauses for the
 *    proof are complete, that is, a SAT refutation is found
 *    with only that subset of clauses. The check is performed
 *    by deleting the SAT solver clauses that are not part of
 *    the proof and calling SatSolver() function again. If the
 *    function returns a refutation then the SAT clauses selected
 *    for the proof are a complete subset.
 *
 *    It is assumed that a proof has been found and that the proof
 *    includes SAT solver clauses.
 *
 *    IMPORTANT: This function must be called after PrintKBData() and
 *    VerifySatStatus().
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
void VerifySatProof(void){
	auto satclause *satcl;                               /* Pointer to SAT clause */
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto hashchcmp *ptr2;                                /* Auxiliary pointer */
	auto satnode *ptr3;                                  /* Auxiliary pointer */
	auto int32_t ii,jj;                                  /* Auxiliary */
	auto float ff;                                       /* Auxiliary */

	/* Print output heading. */
	printf("\n******************************\nSAT refutation checking: ");

	/* Loop through clauses in the SAT solver. */
	ii=0; /* Total number of SAT solver clauses. */
	jj=0; /* Total number of SAT solver clauses in proof queue. */
	for (satcl=kbset.frstsatglbl;satcl!=NULL;satcl=satcl->glblnext){
		ii++;

		/* Clause is not part of the proof so it must be deleted. */
		if (0==satcl->inproof){

			/* Loop through literals. */
			for (ptr1=&satcl->formula[0];ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol))){

				/* Update the npositive, nnegative and absdif fields of the */
				/* corresponding hashchcmp element. */
				ptr2=((asymbol *)&ptr1[1])->symbol;
				if (ptr1[0]&POSITIVE){
					ptr2->npositive--;
				} else {
					ptr2->nnegative--;
				}
				if (ptr2->npositive>ptr2->nnegative){
					ptr2->absdif=ptr2->npositive-ptr2->nnegative;
				} else {
					ptr2->absdif=ptr2->nnegative-ptr2->npositive;
				}

				/* Remove the hashchcmp element from the hashchcmp ordered queue. */
				/* It is necessary to remove it and link it again to the ordered */
				/* sub-queue so that it is placed in the right position. */
				UnchainOrdComponent(ptr2);

				/* If there are some literals corresponding to the hashchcmp element */
				/* then link it again to the hashchcmp order sub-queue. */
				if (ptr2->npositive||ptr2->nnegative){
					ChainOrdComponent(ptr2);
				}

				/* Unlink the literal from the SAT solver chain for literals of the same component. */
				if (((asymbol *)&ptr1[1])->next!=NULL){
					((asymbol *)&ptr1[1])->next->prev=((asymbol *)&ptr1[1])->prev;
				}
				if (((asymbol *)&ptr1[1])->prev!=NULL){
					((asymbol *)&ptr1[1])->prev->next=((asymbol *)&ptr1[1])->next;
				} else {
					ptr2->frstsat=((asymbol *)&ptr1[1])->next;
				}
			}

			/* Debug for checking satclause ordered sub-queue coherence. */
			#ifdef SATDEBUGCODE2
			satcl->next=satcl->prev=NULL;
			#endif

			/* Unchain clause from the SAT solver ordered queue. */
			UnchainSatClause(satcl);

		/* Clause is part of the proof. */
		} else {
			jj++;
		}
	}

	/* Print total number of clauses and number of clauses in proof. */
	printf("%d of %d SAT solver clauses are in proof queue, ",jj,ii);

	/* Cancel the existing branch, call SatSolver() function */
	/* and print results. */
	for (ptr3=kbset.satrootnode;ptr3!=NULL;ptr3=ptr3->nextnode){
		ptr3->flags=FREENODE;
	}
	ff=kbset.opts.timeout;
	kbset.opts.timeout=9999999.0;
	switch (SatSolver()){

		/* NOMEMORY condition. TIMEOUT is not possible as timeout */
		/* has been set to a very high value. */
		case NOMEMORY:
			printf("not enough memory, no additional data is available.");
			break;

		/* Refutation found. */
		case 0:
			printf("proof clauses are consistent with refutation.");
			break;

		/* A SAT model is available. */
		default:
		case 1:
			printf("proof clauses are NOT consistent with refutation.");
			break;
	}

	/* Restore timeout and return. */
	kbset.opts.timeout=ff;
	printf("\n******************************\n");
	return;
} /* VerifySatProof */
#endif

#if VERIFYSATFINALSTATUS == 1
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Verify SAT final status after a problem solving run)  OCJ
 *
 *    This function perform the following integrity checks related to
 *    SAT data after a problem solving run:
 *    - Unprocessed, passive and active clauses are checked for not
 *      having to be locked due to their assertions, that is, all their
 *      assertions must be true.
 *    - Locked clauses are checked for being properly locked according
 *      to their assertions and lock assertions. For that at least one
 *      of the following conditions must be true:
 *      - One or more of its assertions is false.
 *      - All its lock assertions are true.
 *    - Satisfiability status of SAT clauses and coherence with the status
 *      reported by SatSolver(). The check is done examining each literal
 *      of each SAT clause, not examining the satlits field of the clause.
 *      This way possible bugs may be detected.
 *
 *    The result is stored in bresults[2][] global variable.
 *
 *    IMPORTANT: This function must be called after PrintKBData() and
 *    before VerifySatProof().
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
void VerifySatStatus(void){
	auto satclause *satcl;                               /* Pointer to SAT clause */
	auto satnode *node;                                  /* Pointer to SAT node */
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto hashchcmp *ptr2;                                /* Auxiliary pointer */
	auto cmprefix *ptr3;                                 /* Auxiliary pointer */
	auto int32_t ii,jj,kk;                               /* Auxiliary */

	/* If problem was not solved then set result to SATUNK and return. */
	if (procctl->solvekb<0){
		bresults[2][*bncounter]=SATUNK;
		return;

	/* If the problem was solved in the pre-processing set result to SATOK and return. */
	} else if (procctl->solvekb==active_cores){
		bresults[2][*bncounter]=SATOK;
		return;
	}

	/* Initialize status flags. */
	kk=SATOK;

	/* Check assertions and locked assertions in A-clauses. This is */
	/* done only if SAT solver has not reported a refutation because */
	/* otherwise there is no model available. */
	if ((procctl->status==SATISFIABLE)||(kbset.frstprfnode->type!=SATCLAUSE)){

		/* Loop through unprocessed clauses. */
		if (kbset.frstunproc!=NULL){
			for (ptr3=kbset.frstunproc,ii=0;ptr3!=NULL;ptr3=ptr3->next){

				/* Loop through literals in clause assertions. */
				if (ptr3->part2.bin->ovly.asserts!=NULL){
					for (ptr1=ptr3->part2.bin->ovly.asserts;(*ptr1)!=UNITEND;ptr1+=1+sizeof(asymbol)){

						/* Check if the assertion is false. */
						ptr2=((asymbol *)&ptr1[1])->symbol;
						if (((0==((MODELPOSITIVE)&ptr2->flags))&&(POSITIVE&ptr1[0]))
								||((0==((MODELNEGATIVE)&ptr2->flags))&&(NEGATIVE&ptr1[0]))){
							ii|=1;
							break;
						}
					}
				}

				/* Check clause type. */
				if (0==(ptr3->flags&UNPROC)){
					ii|=2;
				}
			}

			/* Problems with unprocessed clauses. */
			switch (ii){
				case 1:
					kk|=SATUNPR1;
					break;
				case 2:
					kk|=SATUNPR2;
					break;
				case 3:
					kk|=SATUNPR12;
					break;
			}
		}

		/* Loop through passive clauses. */
		if (kbset.frstpassive!=NULL){
			for (ptr3=kbset.frstpassive,ii=0;ptr3!=NULL;ptr3=ptr3->next){

				/* Loop through literals in clause assertions. */
				if (ptr3->part2.bin->ovly.asserts!=NULL){
					for (ptr1=ptr3->part2.bin->ovly.asserts;(*ptr1)!=UNITEND;ptr1+=1+sizeof(asymbol)){

						/* Check if the assertion is false. */
						ptr2=((asymbol *)&ptr1[1])->symbol;
						if (((0==((MODELPOSITIVE)&ptr2->flags))&&(POSITIVE&ptr1[0]))
								||((0==((MODELNEGATIVE)&ptr2->flags))&&(NEGATIVE&ptr1[0]))){
							ii|=1;
							break;
						}
					}
				}

				/* Check clause type. */
				if (0==(ptr3->flags&PASSIVE)){
					ii|=2;
				}
			}

			/* Problems with passive clauses. */
			switch (ii){
				case 1:
					kk|=SATPASSIVE1;
					break;
				case 2:
					kk|=SATPASSIVE2;
					break;
				case 3:
					kk|=SATPASSIVE12;
					break;
			}
		}

		/* Loop through active clauses. */
		if (kbset.frstactive!=NULL){
			for (ptr3=kbset.frstactive,ii=0;ptr3!=NULL;ptr3=ptr3->next){

				/* Loop through literals in clause assertions. */
				if (ptr3->part2.bin->ovly.asserts!=NULL){
					for (ptr1=ptr3->part2.bin->ovly.asserts;(*ptr1)!=UNITEND;ptr1+=1+sizeof(asymbol)){

						/* Check if the assertion is false. */
						ptr2=((asymbol *)&ptr1[1])->symbol;
						if (((0==((MODELPOSITIVE)&ptr2->flags))&&(POSITIVE&ptr1[0]))
								||((0==((MODELNEGATIVE)&ptr2->flags))&&(NEGATIVE&ptr1[0]))){
							ii|=1;
							break;
						}
					}
				}

				/* Check clause type. */
				if (0==(ptr3->flags&ACTIVE)){
					ii|=2;
				}
			}

			/* Problems with active clauses. */
			switch (ii){
				case 1:
					kk|=SATACTIVE1;
					break;
				case 2:
					kk|=SATACTIVE2;
					break;
				case 3:
					kk|=SATACTIVE12;
					break;
			}
		}

		/* Loop through locked clauses. */
		if (kbset.frstlocked!=NULL){
			for (ptr3=kbset.frstlocked,jj=0;ptr3!=NULL;ptr3=ptr3->next){

				/* Loop through literals in clause assertions. */
				ii=1;
				if (ptr3->part2.bin->ovly.asserts!=NULL){
					for (ptr1=ptr3->part2.bin->ovly.asserts;(*ptr1)!=UNITEND;ptr1+=1+sizeof(asymbol)){

						/* Check if the assertion is false. */
						ptr2=((asymbol *)&ptr1[1])->symbol;
						if (((0==((MODELPOSITIVE)&ptr2->flags))&&(POSITIVE&ptr1[0]))
								||((0==((MODELNEGATIVE)&ptr2->flags))&&(NEGATIVE&ptr1[0]))){
							ii=0;
							break;
						}
					}
				}

				/* None of the assertions are false. Loop through literals in clause lock assertions. */
				if (ii){
					ii=0;
					if (ptr3->part2.bin->lockasserts!=NULL){
						for (ptr1=ptr3->part2.bin->lockasserts;(*ptr1)!=UNITEND;ptr1+=1+sizeof(asymbol)){

							/* Check if the lock assertion is false. */
							ptr2=((asymbol *)&ptr1[1])->symbol;
							if (((0==((MODELPOSITIVE)&ptr2->flags))&&(POSITIVE&ptr1[0]))
									||((0==((MODELNEGATIVE)&ptr2->flags))&&(NEGATIVE&ptr1[0]))){
								ii=1;
								break;
							}
						}
					}
				}

				/* Check clause type. */
				jj|=ii;
				if (0==(ptr3->flags&LOCKED)){
					jj|=2;
				}
			}

			/* Problems with locked clauses. */
			switch (jj){
				case 1:
					kk|=SATLOCKED1;
					break;
				case 2:
					kk|=SATLOCKED2;
					break;
				case 3:
					kk|=SATLOCKED12;
					break;
			}
		}
	}

	/* Loop through clauses in the SAT solver. */
	for (satcl=kbset.frstsatglbl,jj=1;satcl!=NULL;satcl=satcl->glblnext){

		/* Loop through SAT clause literals. */
		for (ptr1=&satcl->formula[0],ii=0;ptr1[0]!=UNITEND;ptr1+=(1+sizeof(asymbol))){
			ptr2=((asymbol *)&ptr1[1])->symbol;

			/* SAT solver has reported a refutation. */
			if ((procctl->status!=SATISFIABLE)&&(kbset.frstprfnode->type==SATCLAUSE)){

				/* Literal is satisfied. */
				if (((POSITIVE|NEGATIVE)&ptr2->flags)==((POSITIVE|NEGATIVE)&ptr1[0])){
					ii=1;
					break;
				}

			/* SAT solver has not reported a refutation. */
			} else {

				/* Literal is satisfied. */
				if ((((MODELPOSITIVE)&ptr2->flags)&&(POSITIVE&ptr1[0]))
						||(((MODELNEGATIVE)&ptr2->flags)&&(NEGATIVE&ptr1[0]))){
					ii=1;
					break;
				}
			}
		}

		/* Clause is not satisfied. Set results and return or leave the loop as appropriate. */
		if (ii==0){
			jj=0;
			if (procctl->status==SATISFIABLE){
				kk|=SATUNSAT;
				return;
			}
			break;
		}
	}

	/* If all SAT clauses are satisfied then set results as appropriate. */
	if ((jj)&&(procctl->status!=SATISFIABLE)){
		kk|=SATSAT;
	}
	bresults[2][*bncounter]=kk;

	/* If the problem had a SAT refutation then undo all SAT assignments for the winning */
	/* process KB. This is necessary in order for VerifySatProof() and GetMinUnsatCore() */
	/* functions to work properly. */
	if ((procctl->status==UNSATISFIABLE)&&(kbset.frstprfnode->type==SATCLAUSE)){

		/* Get last used node. As there was a sat refutation and this function is active */
		/* then there must be some used SAT nodes. */
		for (node=kbset.satrootnode;(node->nextnode!=NULL)&&(0==(node->nextnode->flags&FREENODE));
				node=node->nextnode){
		}

		/* Undo all SAT assignments. */
		for (;node!=NULL;node=node->prevnode){
			UndoSymbolAssgnmnt(node);
		}
	}
	return;
} /* VerifySatStatus */
#endif
