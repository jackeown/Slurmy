/*
 ============================================================================
 Name        : ordering.c
 Author      : Oscar Contreras
 Version     :
 Copyright   : Copyright (C) 2015-2016 Oscar Contreras. All rights reserved.
 Description : First order logic theorem prover in C
 ============================================================================
 */

/****************************************************************
*
*                 selecting (selecting functions module)
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
 *    This source contains the literal selection functions of the Drodi package.
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
 *  DESCRIPTION: (Select items in a clause)  OCJ
 *
 *    This function selects the following items in a clause:
 *    - Appropriate literals in active clauses (see below).
 *    - Maximal equality root terms in selected equalities of active
 *      clauses.
 *    - Maximum equality root terms in single literal active and passive
 *      clauses are flagged as MAXIMUM.
 *
 *    Also SELECTED positive equalities in clauses without negative literals
 *    are flagged as PMSELECTED to indicate that they are selected for
 *    paramodulating from them.
 *
 *    This function performs selection tasks common to all literal selection
 *    strategies and then calls the appropriate selecting functions according
 *    to the literal selection currently in use.
 *
 *    See "Selecting the selection" by Krystof Hoder et al. and "The design and
 *    implementation of VAMPIRE" section 2.6 by Alexandre Riazanov and Andrei
 *    Voronkov.
 *
 *    IMPORTANT: This function must never be called for main KB.
 *    The clause must belong to the working KB.
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
int32_t SelectItems(cmprefix *clause){
	auto int32_t firstneg,firstpos;                 /* Literal number of first negative and positive literals */
	auto uint8_t *demodequ;                         /* Pointer to equality suitable for demodulating from if */
	auto char *litmask;                             /* Selected literals mask. */
	auto int32_t eqs;                               /* Number of positive and negative equality literals. */
	auto uint8_t *ptr1,*ptr2,*ptr3;                 /* Auxiliary pointers */
	auto int32_t ii,jj;                             /* Auxiliary */

	/* If clause is not weakly oriented then loop through clause literals. */
	if (0==(WEAKLYORIENTED&clause->flags)){
		for (ptr1=&clause->part2.bin->formula[0],eqs=0,firstneg=firstpos=-1,ii=0;
				(*ptr1)!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){

			/* Deselect literal and equality root terms. Count number of */
			/* equality literals. Equality root terms are deselected only */
			/* if clause is not an oriented positive unit clause, as if */
			/* it is then root terms are correctly selected. */
			ptr1[0]&=(~(SELECTED|PMSELECTED|MAXIMUM));
			if (ptr1[0]&EQUALITY){
				eqs++;
				if ((clause->part2.bin->literals!=1)||(clause->part2.bin->oriented==0)||(ptr1[0]&NEGATED)){
					ptr2=NextItem(ptr1,IMMED);
					ptr2[0]&=(~(SELECTED|MAXIMUM));
					ptr2=NextItem(ptr2,OVERSUBTERMS);
					ptr2[0]&=(~(SELECTED|MAXIMUM));
					clause->part2.bin->oriented=0;
				}
			}

			/* Remember first negative and positive literals. */
			if ((firstneg==-1)&&(ptr1[0]&NEGATED)){
				firstneg=ii;
			}
			if ((firstpos==-1)&&(0==(ptr1[0]&NEGATED))){
				firstpos=ii;
			}
		}

	/* This is just to avoid compiler warnings. */
	} else {
		firstpos=firstneg=eqs=0;
	}

	/* Check if clause is suitable for performing demodulation from it. */
	if ((clause->part2.bin->literals==1)&&(((EQUALITY|NEGATED)&clause->part2.bin->formula[0])==EQUALITY)){

		/* Remember position of equality suitable for demodulating from it. */
		demodequ=&clause->part2.bin->formula[0];

		/* If clause is oriented then equality data has been set, so */
		/* just select the equality literal if appropriate and return. */
		if (clause->part2.bin->oriented==1){
			if (clause->flags&ACTIVE){
				demodequ[0]|=(SELECTED|PMSELECTED);
			}
			return(0);
		}
	} else {
		demodequ=NULL;
	}

	/* Clause is suitable for demodulation from it. */
	if (demodequ!=NULL){

		/* Clause is not weakly oriented. */
		if (0==(WEAKLYORIENTED&clause->flags)){

			/* Get pointers to equality root terms. */
			ptr2=NextItem(demodequ,IMMED);
			ptr3=NextItem(ptr2,OVERSUBTERMS);

			/* Select maximum equality root terms. */
			/* Release equality tree data if the root terms ordering */
			/* is completely defined. */
			ii=CompareEquTerms(clause->part2.bin,ptr2,ptr3);
			clause->part2.bin->oriented=1;
			switch (ii){
				case NOMEMORY:
					return(NOMEMORY);
					break;
				case GREATER:
					ptr2[0]|=MAXIMUM|SELECTED;
					break;
				case LESSER:
					ptr3[0]|=MAXIMUM|SELECTED;
					break;
				case EQUAL:
					break;
				case FAILURE:
					ptr2[0]|=SELECTED;
					ptr3[0]|=SELECTED;
					break;
			}
		}

		/* Select literal if clause is active. */
		if (clause->flags&ACTIVE){
			demodequ[0]|=(SELECTED|PMSELECTED);
		}

		/* Return. */
		return(0);
	}

	/* Active clause or flag is set. */
	litmask=NULL; /* Just to prevent compiler warnings. */
	if (clause->flags&ACTIVE){

		/* Allocate memory for selected literals mask. */
		if (NULL==(litmask=MYALLOC(clause->part2.bin->literals))){
			return(NOMEMORY);
		}

		/* Clause has only one literal. */
		if (clause->part2.bin->literals==1){
			litmask[0]=1;

		/* Clause has more than one literal. */
		} else {

			/* Literal selection is MAXIMAL, SINGLENEG, MULTINEG, SINGLEPOS or MULTIPOS. */
			if (kbset.opts.select&(MAXIMAL|SINGLENEG|MULTINEG|SINGLEPOS|MULTIPOS)){
				memset(litmask,0,clause->part2.bin->literals);
				if (NOMEMORY==SelectLiterals1(clause,litmask,firstneg,firstpos,eqs)){
					MYFREE(litmask);
					return(NOMEMORY);
				}

			/* Literal selection MXSINGLENEG. */
			} else if (kbset.opts.select&MXSINGLENEG){
				memset(litmask,0,clause->part2.bin->literals);
				if (NOMEMORY==SelectLiterals2(clause,litmask,eqs)){
					MYFREE(litmask);
					return(NOMEMORY);
				}

			/* Other types of literal selection. */
			} else {
				memset(litmask,1,clause->part2.bin->literals);
				switch (kbset.opts.select){
					case TYPE1:
						if (NOMEMORY==SelectType1(clause,litmask)){
							MYFREE(litmask);
							return(NOMEMORY);
						}
						break;
					case TYPE2:
						if (NOMEMORY==SelectType2(clause,litmask)){
							MYFREE(litmask);
							return(NOMEMORY);
						}
						break;
					case TYPE3:
						if (NOMEMORY==SelectType3(clause,litmask)){
							MYFREE(litmask);
							return(NOMEMORY);
						}
						break;
					case TYPE4:
						if (NOMEMORY==SelectType4(clause,litmask)){
							MYFREE(litmask);
							return(NOMEMORY);
						}
						break;
					case TYPE5:
						if (NOMEMORY==SelectType5(clause,litmask)){
							MYFREE(litmask);
							return(NOMEMORY);
						}
						break;
					case TYPE6:
						if (NOMEMORY==SelectType6(clause,litmask)){
							MYFREE(litmask);
							return(NOMEMORY);
						}
						break;
					case TYPE1CPL:
						if (NOMEMORY==SelectType14Cpl(SelectType1,clause,litmask,eqs)){
							MYFREE(litmask);
							return(NOMEMORY);
						}
						break;
					case TYPE2CPL:
						if (NOMEMORY==SelectType14Cpl(SelectType2,clause,litmask,eqs)){
							MYFREE(litmask);
							return(NOMEMORY);
						}
						break;
					case TYPE3CPL:
						if (NOMEMORY==SelectType14Cpl(SelectType3,clause,litmask,eqs)){
							MYFREE(litmask);
							return(NOMEMORY);
						}
						break;
					case TYPE4CPL:
						if (NOMEMORY==SelectType14Cpl(SelectType4,clause,litmask,eqs)){
							MYFREE(litmask);
							return(NOMEMORY);
						}
						break;
					case TYPE5CPL:
						if (NOMEMORY==SelectType56Cpl(SelectType5,clause,litmask,eqs)){
							MYFREE(litmask);
							return(NOMEMORY);
						}
						break;
					case TYPE6CPL:
						if (NOMEMORY==SelectType56Cpl(SelectType6,clause,litmask,eqs)){
							MYFREE(litmask);
							return(NOMEMORY);
						}
						break;
				}
			}
		}

		/* Loop through all literals. */
		for (ptr1=&clause->part2.bin->formula[0],ii=0;(*ptr1)!=UNITEND;
				ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){

			/* Literal is selected. */
			if (litmask[ii]){
				ptr1[0]|=SELECTED;

				/* Literal is an equality or inequality. */
				if (ptr1[0]&EQUALITY){

					/* If there are not negative literals then this is a positive equality */
					/* eligible for paramodulating from it. Set flag PMSELECTED. */
					/* The condition SELECTED!=PMSELECTED forces the compiler to */
					/* ignore this code if SELECTED==PMSELECTED. */
					if ((SELECTED!=PMSELECTED)&&(firstneg==-1)){
						ptr1[0]|=PMSELECTED;
					}

					/* Get pointers to equality root terms. */
					ptr2=NextItem(ptr1,IMMED);
					ptr3=NextItem(ptr2,OVERSUBTERMS);

					/* Set equality root terms as appropriate. */
					/* Release equality tree data if the root terms ordering */
					/* is completely defined. */
					jj=CompareEquTerms(clause->part2.bin,ptr2,ptr3);
					clause->part2.bin->oriented=1;
					switch (jj){
						case NOMEMORY:
							MYFREE(litmask);
							return(NOMEMORY);
							break;
						case GREATER:
							ptr2[0]|=(SELECTED|MAXIMUM);
							break;
						case LESSER:
							ptr3[0]|=(SELECTED|MAXIMUM);
							break;
						case EQUAL:
							break;
						case FAILURE:
							ptr2[0]|=SELECTED;
							ptr3[0]|=SELECTED;
							break;
					}
				}
			}
		}
	}

	/* Free memory and return. */
	MYFREE(litmask);
	return(0);
} /* SelectItems */

 /*--------------------------------------------------------------
  *
  *  DESCRIPTION: (Select items in a clause for unit clause)  OCJ
  *
  *    This function selects the following items in a clause for
  *    type 8 problems (unit equality):
  *    - Appropriate literals in active clauses (see below).
  *    - Maximal equality root terms in selected equalities of active
  *      clauses.
  *    - Maximum equality root terms in single literal active and passive
  *      clauses are flagged as MAXIMUM.
  *
  *    Also SELECTED positive equalities in clauses without negative literals
  *    are flagged as PMSELECTED to indicate that they are selected for
  *    paramodulating from them.
  *
  *    This function performs selection tasks common to all literal selection
  *    strategies and then calls the appropriate selecting functions according
  *    to the literal selection currently in use.
  *
  *    See "Selecting the selection" by Krystof Hoder et al. and "The design and
  *    implementation of VAMPIRE" section 2.6 by Alexandre Riazanov and Andrei
  *    Voronkov.
  *
  *    IMPORTANT: This function must never be called for main KB.
  *    The clause must belong to the working KB.
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
 int32_t SelectItemsUEQ(cmprefix *clause){
 	auto uint8_t *demodequ;                         /* Pointer to equality suitable for demodulating from if */
 	auto uint8_t *ptr1,*ptr2,*ptr3;                 /* Auxiliary pointers */
 	auto int32_t ii,jj;                             /* Auxiliary */

 	/* Clause is not weakly oriented. */
 	ptr1=&clause->part2.bin->formula[0];
 	if (0==(WEAKLYORIENTED&clause->flags)){

		/* Deselect literal and equality root terms. Equality root terms */
 		/* are deselected only if clause is not an oriented positive unit clause, */
 		/* as if it is then root terms are correctly selected. */
		ptr1[0]&=(~(SELECTED|PMSELECTED|MAXIMUM));
		if ((clause->part2.bin->oriented==0)||(ptr1[0]&NEGATED)){
			ptr2=NextItem(ptr1,IMMED);
			ptr2[0]&=(~(SELECTED|MAXIMUM));
			ptr2=NextItem(ptr2,OVERSUBTERMS);
			ptr2[0]&=(~(SELECTED|MAXIMUM));
			clause->part2.bin->oriented=0;
		}
 	}

 	/* Check if clause is suitable for performing demodulation from it. */
 	if (((EQUALITY|NEGATED)&ptr1[0])==EQUALITY){

 		/* Remember position of equality suitable for demodulating from it. */
 		demodequ=&clause->part2.bin->formula[0];

 		/* If clause is oriented then equality data has been set, so */
 		/* just select the equality literal if appropriate and return. */
 		if (clause->part2.bin->oriented==1){
 			if (clause->flags&ACTIVE){
 				demodequ[0]|=(SELECTED|PMSELECTED);
 			}
 			return(0);
 		}
 	} else {
 		demodequ=NULL;
 	}

 	/* Clause is suitable for demodulation from it. */
 	if (demodequ!=NULL){

 		/* Clause is not weakly oriented. */
 		if (0==(WEAKLYORIENTED&clause->flags)){

 			/* Get pointers to equality root terms. */
 			ptr2=NextItem(demodequ,IMMED);
 			ptr3=NextItem(ptr2,OVERSUBTERMS);

 			/* Select maximum equality root terms. */
 			/* Release equality tree data if the root terms ordering */
 			/* is completely defined. */
 			ii=CompareEquTerms(clause->part2.bin,ptr2,ptr3);
 			clause->part2.bin->oriented=1;
 			switch (ii){
 				case NOMEMORY:
 					return(NOMEMORY);
 					break;
 				case GREATER:
 					ptr2[0]|=MAXIMUM|SELECTED;
 					break;
 				case LESSER:
 					ptr3[0]|=MAXIMUM|SELECTED;
 					break;
 				case EQUAL:
 					break;
 				case FAILURE:
 					ptr2[0]|=SELECTED;
 					ptr3[0]|=SELECTED;
 					break;
 			}
 		}

 		/* Select literal if clause is active. */
 		if (clause->flags&ACTIVE){
 			demodequ[0]|=(SELECTED|PMSELECTED);
 		}

 		/* Return. */
 		return(0);
 	}

 	/* If we are here then clause is an inequality. */
 	/* Clause is active. */
 	if (clause->flags&ACTIVE){

 		/* Set inequality as selected. */
		ptr1[0]|=SELECTED;

		/* Get pointers to equality root terms. */
		ptr2=NextItem(ptr1,IMMED);
		ptr3=NextItem(ptr2,OVERSUBTERMS);

		/* Set inequality root terms as appropriate. */
		/* Release equality tree data if the root terms ordering */
		/* is completely defined. */
		jj=CompareEquTerms(clause->part2.bin,ptr2,ptr3);
		clause->part2.bin->oriented=1;
		switch (jj){
			case NOMEMORY:
				return(NOMEMORY);
				break;
			case GREATER:
				ptr2[0]|=(SELECTED|MAXIMUM);
				break;
			case LESSER:
				ptr3[0]|=(SELECTED|MAXIMUM);
				break;
			case EQUAL:
				break;
			case FAILURE:
				ptr2[0]|=SELECTED;
				ptr3[0]|=SELECTED;
				break;
		}
 	}

 	/* Free memory and return. */
 	return(0);
 } /* SelectItemsUEQ */

#ifdef SINGLEANY
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Several types of literal selection)  OCJ
 *
 *    This function performs literal selection for MAXIMAL, SINGLENEG,
 *    MULTINEG, SINGLEPOS and MULTIPOS selection options. A literal
 *    mask buffer with as many chars as literals in the clause must be
 *    allocated and filled with 0 before calling this function. The
 *    items corresponding to selected literals will be set to 1 by
 *    this function.
 *
 *    It is assumed that the clause has more than one literal.
 *
 *    The logic of this function is based on the document "The design and
 *    implementation of VAMPIRE" section 2.6 by Alexandre Riazanov and
 *    Andrei Voronkov with one exception for the case when the literal
 *    selection is set to MAXIMAL (see below).
 *
 *    Literals in active clauses are selected as follows:
 *    - If selection is set to MAXIMAL then all maximal literals
 *      are selected. Of course if there is a maximum literal it will be
 *      the only literal selected because it is the only maximal literal.
 *      The difference with the document above is that if there are negative
 *      literals at least one of them will be always selected. To achieve
 *      this if no negative literals are selected yet then any negative
 *      literal will be compared only to other negative literals in order to
 *      decide if it will be selected or not. Once a negative literal is
 *      selected then the remaining negative literals are compared to any
 *      other literal. Positive literal are always compared to any other
 *      literal. This is different from the criteria in the document above
 *      because with the literal ordering of the document the maximal
 *      selection is not complete. As a complete maximal selection is needed
 *      then the criteria explained above is used instead.
 *    - If selection is set to SINGLENEG then:
 *      - If there are negative literals then the first one is selected
 *        and the other literals are not selected.
 *      - Otherwise all maximal literals are selected.
 *    - If selection is set to MULTINEG then:
 *      - If there are negative literals then the negative literals that
 *        are maximal in the set of negative literals are selected and
 *        and the other literals are not selected. Of course if there
 *        is a maximum negative literal that will be the only negative
 *        literal selected because the maximum is the only maximal
 *        literal.
 *      - Otherwise all maximal literals are selected.
 *    - If selection is set to SINGLEPOS then:
 *      - If there are positive literals then the first one is selected
 *        and the other literals are not selected.
 *      - Otherwise all maximal literals are selected.
 *    - If selection is set to MULTIPOS then:
 *      - If there are positive literals then the positive literals that
 *        are maximal in the set of positive literals are selected and
 *        and the other literals are not selected. Of course if there
 *        is a maximum negative literal that will be the only positive
 *        literal selected because the maximum is the only maximal
 *        literal.
 *      - Otherwise all maximal literals are selected.
 *
 *    IMPORTANT: This function must never be called for main KB.
 *    The clause must belong to the working KB.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask. It must
 *             be filled with 0 by the caller but this is not checked.
 *    firstneg: At entry this is the literal number of first negative literal
 *              in clause or -1 if no negative literals.
 *    firstpos: At entry this is the literal number of first positive literal
 *              in clause or -1 if no positive literals.
 *    eqs: Number of equalities and inequalities in clause.
 *
 *  RETURNS:
 *
 *    0 if successful, NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t SelectLiterals1(cmprefix *clause,char *litmask,int32_t firstneg,int32_t firstpos,int32_t eqs){
	auto uint8_t *ptr1,*ptr2;                       /* Auxiliary pointers */
	auto int32_t selneg;                            /* 0 if no negative literals are selected yet, 1 otherwise */
	auto int32_t ii,jj;                             /* Auxiliary */

	/* More than one literal may be potentially selected. */
	if (((kbset.opts.select!=SINGLENEG)||(firstneg==-1))
			&&((kbset.opts.select!=SINGLEPOS)||(firstpos==-1))){

		/* Loop through all literals. Mask items are temporarily set to 2 */
		/* to indicate that the literal is lesser than a previous literal. */
		for (ptr1=&clause->part2.bin->formula[0],ii=selneg=0;(*ptr1)!=UNITEND;
				ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){

			/* Literal is a candidate for selection. */
			if (((ptr1[0]&NEGATED)||(kbset.opts.select!=MULTINEG)||(firstneg==-1))
					&&((0==(ptr1[0]&NEGATED))||(kbset.opts.select!=MULTIPOS)||(firstpos==-1))){

				/* Literal flagged as being lesser than a previous one. Reset literal mask item. */
				if (litmask[ii]==2){
					litmask[ii]=0;

				/* Literal not flagged as being lesser than a previous one. */
				} else {

					/* Loop through literals following current literal. */
					for (ptr2=NextItem(ptr1,OVERSUBTERMS),jj=ii+1,litmask[ii]=1;((*ptr2)!=UNITEND)&&(litmask[ii]);
							ptr2=NextItem(ptr2,OVERSUBTERMS),jj++){

						/* Second loop literal is a candidate for selection. */
						if ((litmask[jj]!=2)
								&&((ptr2[0]&NEGATED)||(kbset.opts.select!=MULTINEG)||(firstneg==-1))
								&&((0==(ptr2[0]&NEGATED))||(kbset.opts.select!=MULTIPOS)||(firstpos==-1))){
							switch (CompareLiterals(ptr1,ptr2)){
								case NOMEMORY:
									return(NOMEMORY);
									break;
								case GREATER:
									if ((selneg)||(NEGATED&ptr1[0])||(0==(NEGATED&ptr2[0]))){
										litmask[jj]=2;
									}
									break;
								case LESSER:
									if ((selneg)||(NEGATED&ptr2[0])||(0==(NEGATED&ptr1[0]))){
										litmask[ii]=0;
									}
									break;
							}

							/* Indicate that a negative literal has been selected. */
							if ((litmask[ii]&&(NEGATED&ptr1[0]))){
								selneg=1;
							}
						}
					}
				}
			}
		}

	/* Only one literal can be selected. Select first negative */
	/* or positive literal as appropriate. */
	} else {
		if (kbset.opts.select==SINGLENEG){
			litmask[firstneg]=1;
		} else {
			litmask[firstpos]=1;
		}
	}

	return(0);
} /* SelectLiterals1 */
#else
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Several types of literal selection)  OCJ
 *
 *    This function performs literal selection for MAXIMAL, SINGLENEG,
 *    MULTINEG, SINGLEPOS and MULTIPOS selection options. A literal
 *    mask buffer with as many chars as literals in the clause must be
 *    allocated and filled with 0 before calling this function. The
 *    items corresponding to selected literals will be set to 1 by
 *    this function.
 *
 *    It is assumed that the clause has more than one literal.
 *
 *    The logic of this function are based on the document "The design and
 *    implementation of VAMPIRE" section 2.6 by Alexandre Riazanov and
 *    Andrei Voronkov with one exception for the case when the literal
 *    selection is set to MAXIMAL (see below).
 *
 *    Literals in active clauses are selected as follows:
 *    - If selection is set to MAXIMAL then all maximal literals
 *      are selected. Of course if there is a maximum literal it will be
 *      the only literal selected because it is the only maximal literal.
 *      The difference with the document above is that if there are negative
 *      literals at least one of them will be always selected. To achieve
 *      this if no negative literals are selected yet then any negative
 *      literal will be compared only to other negative literals in order to
 *      decide if it will be selected or not. Once a negative literal is
 *      selected then the remaining negative literals are compared to any
 *      other literal. Positive literal are always compared to any other
 *      literal. This is different from the criteria in the document above
 *      because with the literal ordering of the document the maximal
 *      selection is not complete. As a complete maximal selection is needed
 *      then the criteria explained above is used instead.
 *    - If selection is set to SINGLENEG then:
 *      - If there are negative literals then the first negative literal
 *        that is maximal with respect to the set of negative literals
 *        will be selected. Of course if there is a negative literal
 *        that is maximum with respect to the set of negative literals that
 *        will be selected as it is the only maximal negative literal.
 *      - Otherwise all maximal literals are selected.
 *    - If selection is set to MULTINEG then:
 *      - If there are negative literals then the negative literals that
 *        are maximal with respect to the set of negative literals are
 *        selected and the other literals are not selected. Of course if
 *        there is a maximum negative literal it will be the only literal
 *        selected because it is the only maximal negative literal.
 *      - Otherwise all maximal literals are selected.
 *    - If selection is set to SINGLEPOS then:
 *      - If there are positive literals then the first positive literal
 *        that is maximal with respect to the set of positive literals
 *        will be selected. Of course if there is a positive literal
 *        that is maximum with respect to the set of positive literals that
 *        will be selected as it is the only maximal positive literal.
 *      - Otherwise all maximal literals are selected.
 *    - If selection is set to MULTIEPOS then:
 *      - If there are positive literals then the positive literals that
 *        are maximal with respect to the set of positive literals are
 *        selected and the other literals are not selected. Of course if
 *        there is a maximum positive literal it will be the only literal
 *        selected because it is the only maximal positive literal.
 *      - Otherwise all maximal literals are selected.
 *
 *    IMPORTANT: This function must never be called for main KB.
 *    The clause must belong to the working KB.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask. It must
 *             be filled with 0 by the caller but this is not checked.
 *    firstneg: At entry this is the literal number of first negative literal
 *              in clause or -1 if no negative literals.
 *    firstpos: At entry this is the literal number of first positive literal
 *              in clause or -1 if no positive literals.
 *    eqs: Number of equalities and inequalities in clause.
 *
 *  RETURNS:
 *
 *    0 if successful, NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t SelectLiterals1(cmprefix *clause,char *litmask,int32_t firstneg,int32_t firstpos,int32_t eqs){
	auto uint8_t *ptr1,*ptr2;                       /* Auxiliary pointers */
	auto uint8_t *lastselected;                     /* Pointer to last selected literal */
	auto int32_t selneg;                            /* 0 if no negative literals are selected yet, 1 otherwise */
	auto int32_t ii,jj;                             /* Auxiliary */

	/* Loop through all literals. Mask items are temporarily set to 2 */
	/* to indicate that the literal is lesser than a previous literal. */
	lastselected=NULL;
	for (ptr1=&clause->part2.bin->formula[0],ii=selneg=0;(*ptr1)!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){

		/* Literal is a candidate for selection. */
		if (((ptr1[0]&NEGATED)||((MAXIMAL|SINGLEPOS|MULTIPOS)&kbset.opts.select)||(firstneg==-1))
				&&((0==(ptr1[0]&NEGATED))||((MAXIMAL|SINGLENEG|MULTINEG)&kbset.opts.select)||(firstpos==-1))){

			/* Literal flagged as being lesser than a previous one. Reset literal mask item. */
			if (litmask[ii]==2){
				litmask[ii]=0;

			/* Literal is not flagged as being lesser than a previous one */
			/* and search for selectable literals must be continued. */
			} else if ((0==((SINGLENEG|SINGLEPOS)&kbset.opts.select))
					||((SINGLENEG&kbset.opts.select)&&(firstneg==-1))
					||((SINGLEPOS&kbset.opts.select)&&(firstpos==-1))||(lastselected==NULL)){

				/* Loop through literals following current literal. */
				for (ptr2=NextItem(ptr1,OVERSUBTERMS),jj=ii+1,litmask[ii]=1;((*ptr2)!=UNITEND)&&(litmask[ii]);
						ptr2=NextItem(ptr2,OVERSUBTERMS),jj++){

					/* Second loop literal is a candidate for selection. */
					if ((2!=litmask[jj])
							&&((ptr2[0]&NEGATED)||((MAXIMAL|SINGLEPOS|MULTIPOS)&kbset.opts.select)||(firstneg==-1))
							&&((0==(ptr2[0]&NEGATED))||((MAXIMAL|SINGLENEG|MULTINEG)&kbset.opts.select)||(firstpos==-1))){
						switch (CompareLiterals(ptr1,ptr2)){
							case NOMEMORY:
								return(NOMEMORY);
								break;
							case GREATER:
								if ((selneg)||(NEGATED&ptr1[0])||(0==(NEGATED&ptr2[0]))){
									litmask[jj]=2;
								}
								break;
							case LESSER:
								if ((selneg)||(NEGATED&ptr2[0])||(0==(NEGATED&ptr1[0]))){
									litmask[ii]=0;
								}
								break;
						}
					}
				}

				/* If literal is selected then update lastselected and selneg. */
				if (litmask[ii]){
					lastselected=ptr1;
					if (NEGATED&ptr1[0]){
						selneg=1;
					}
				}
			}
		}
	}

	return(0);
} /* SelectLiterals1 */
#endif

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (MXSINGLENEG literal selection)  OCJ
 *
 *    This function performs literal selection for MAXSINGLNEG selection
 *    option. A literal mask buffer with as many chars as literals in
 *    the clause must be allocated and filled with 0 before calling
 *    this function. The items corresponding to selected literals will
 *    be set to 1 by this function.
 *
 *    It is assumed that the clause has more than one literal.
 *
 *    A maximal negative literal will be selected if there is a maximal
 *    negative literal, otherwise all maximal literals will be selected
 *    and of course they will be all positive. See SelectLiteral1()
 *    general comments for important information about maximal literal
 *    selection.
 *
 *    IMPORTANT: This function must never be called for main KB.
 *    The clause must belong to the working KB.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask. It must
 *             be filled with 0 by the caller but this is not checked.
 *    eqs: Number of equalities and inequalities in clause.
 *
 *  RETURNS:
 *
 *    0 if successful, NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t SelectLiterals2(cmprefix *clause,char *litmask,int32_t eqs){
	auto uint8_t *ptr1,*ptr2;                       /* Auxiliary pointers */
	auto int32_t selneg;                            /* 0 if no negative literals are selected yet, 1 otherwise */
	auto int32_t ii,jj;                             /* Auxiliary */

	/* Loop through all literals. Mask items are temporarily set to 2 */
	/* to indicate that the literal is lesser than a previous literal. */
	for (ptr1=&clause->part2.bin->formula[0],ii=selneg=0;(*ptr1)!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){

		/* Literal flagged as being lesser than a previous one. Reset literal mask item. */
		if (litmask[ii]==2){
			litmask[ii]=0;

		/* Literal not flagged as being lesser than a previous one. */
		} else {

			/* Loop through literals following current literal. */
			for (ptr2=NextItem(ptr1,OVERSUBTERMS),jj=ii+1,litmask[ii]=1;((*ptr2)!=UNITEND)&&(litmask[ii]);
					ptr2=NextItem(ptr2,OVERSUBTERMS),jj++){

				/* If second literal is not flagged as being lesser than a previous one */
				/* then compare literals. */
				if (litmask[jj]!=2){
					switch (CompareLiterals(ptr1,ptr2)){
						case NOMEMORY:
							return(NOMEMORY);
							break;
						case GREATER:
							if ((selneg)||(NEGATED&ptr1[0])||(0==(NEGATED&ptr2[0]))){
								litmask[jj]=2;
							}
							break;
						case LESSER:
							if ((selneg)||(NEGATED&ptr2[0])||(0==(NEGATED&ptr1[0]))){
								litmask[ii]=0;
							}
							break;
					}
				}
			}

			/* If current literal is selected and it is negative then reset */
			/* all other literals and leave the loop and update selneg. */
			if ((litmask[ii])&&(NEGATED&ptr1[0])){
				memset(litmask,0,clause->part2.bin->literals);
				litmask[ii]=1;
				selneg=1;
				break;
			}
		}
	}

	return(0);
} /* SelectLiterals2 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Auxiliary maximal literal selection)  OCJ
 *
 *    This function select all maximal literals. A literal mask buffer
 *    with as many chars as literals in the clause must be allocated
 *    and filled with 0 before calling this function. The items
 *    corresponding to selected literals will be set to 1 by this
 *    function.
 *
 *    The purpose of this function is to support building complete
 *    versions of TYPE1 to TYPE6 literal selections. Therefore in
 *    addition this function provides the following information:
 *    - Number of maximal negative literals.
 *    - Number of maximal positive literals.
 *    - Number of total negative literals.
 *    See SelectLiteral1() general comments for important information
 *    about maximal literal selection.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask. It must
 *             be filled with 0 by the caller but this is not checked.
 *    eqs: Number of equalities and inequalities in clause.
 *    pselpos: Pointer to number of selected positive literals.
 *    pselneg: Pointer to number of selected negative literals.
 *    pnumneg: Pointer to number of negative literals.
 *
 *  RETURNS:
 *
 *    0 if successful, NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t SelectMaximal(cmprefix *clause,char *litmask,int32_t eqs,int32_t *pselpos,
		int32_t *pselneg,int32_t *pnumneg){
	auto uint8_t *ptr1,*ptr2;                       /* Auxiliary pointers */
	auto int32_t selneg;                            /* 0 if no negative literals are selected yet, 1 otherwise */
	auto int32_t ii,jj;                             /* Auxiliary */

	/* Initialize *pnumneg. */
	*pnumneg=0;
	for (ptr1=&clause->part2.bin->formula[0],jj=0;(*ptr1)!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS)){
		if (ptr1[0]&NEGATED){
			(*pnumneg)++;
		}
	}

	/* Loop through all literals. Mask items are temporarily set to 2 */
	/* to indicate that the literal is lesser than a previous literal. */
	*pselpos=*pselneg=0;
	for (ptr1=&clause->part2.bin->formula[0],ii=selneg=0;(*ptr1)!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){

		/* Literal flagged as being lesser than a previous one. Reset literal mask item. */
		if (litmask[ii]==2){
			litmask[ii]=0;

		/* Literal not flagged as being lesser than a previous one. */
		} else {

			/* Loop through literals following current literal. */
			for (ptr2=NextItem(ptr1,OVERSUBTERMS),jj=ii+1,litmask[ii]=1;((*ptr2)!=UNITEND)&&(litmask[ii]);
					ptr2=NextItem(ptr2,OVERSUBTERMS),jj++){

				/* Compare literals. */
				switch (CompareLiterals(ptr1,ptr2)){
					case NOMEMORY:
						return(NOMEMORY);
						break;
					case GREATER:
						if ((selneg)||(NEGATED&ptr1[0])||(0==(NEGATED&ptr2[0]))){
							litmask[jj]=2;
						}
						break;
					case LESSER:
						if ((selneg)||(NEGATED&ptr2[0])||(0==(NEGATED&ptr1[0]))){
							litmask[ii]=0;
						}
						break;
				}
			}

			/* Update selneg, *pselpos and *pselneg. */
			if (litmask[ii]){
				if (ptr1[0]&NEGATED){
					selneg=1;
					(*pselneg)++;
				} else {
					(*pselpos)++;
				}
			}
		}
	}

	return(0);
} /* SelectMaximal */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Select literals by weight)  OCJ
 *
 *    This function selects the literals with more symbols (including
 *    variables). A literal mask buffer with as many chars as literals
 *    in the clause must be allocated and initialized before calling
 *    this function. Only the items that are not 0 in the mask are
 *    considered for selection. Literals not selected are set to 0
 *    in the mask. Other mask items are not changed.
 *
 *    See "Selecting the selection" by Krystof Hoder et al. for more
 *    details.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask.
 *             It must be initialized by the caller (see comments
 *             above).
 *
 *  RETURNS:
 *
 *    Number of literals selected or NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t SelectByWeight(cmprefix *clause,char *litmask){
	auto int32_t *weights;                          /* Literal weights */
	auto uint8_t *ptr1,*ptr2,*ptr3;                 /* Auxiliary pointers */
	auto int32_t ii,jj,kk;                          /* Auxiliary */

	/* Allocate memory for literal weights. */
	if (NULL==(weights=MYALLOC(clause->part2.bin->literals*sizeof(*weights)))){
		return(NOMEMORY);
	}

	/* Initialize weight of selectable literals. */
	for (ptr1=&clause->part2.bin->formula[0],ii=kk=0;(*ptr1)!=UNITEND;ptr1=ptr3,ii++){
		ptr3=NextItem(ptr1,OVERSUBTERMS);
		if (litmask[ii]){
			kk++;
			weights[ii]=0;
			for (ptr2=(EQUALITY&ptr1[0]?NextItem(ptr1,IMMED):ptr1);ptr2<ptr3;ptr2=NextItem(ptr2,IMMED)){
				weights[ii]++;
			}
		}
	}

	/* Loop through all literals. */
	for (ptr1=&clause->part2.bin->formula[0],ii=0;(*ptr1)!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){

		/* Literal is selectable. */
		if (litmask[ii]){

			/* Loop through literals following current literal. */
			for (ptr2=NextItem(ptr1,OVERSUBTERMS),jj=ii+1;(*ptr2)!=UNITEND;
					ptr2=NextItem(ptr2,OVERSUBTERMS),jj++){

				/* If second literal is selectable then compare literals. */
				if (litmask[jj]){
					if (weights[ii]>weights[jj]){
						litmask[jj]=0;
						kk--;
					} else if (weights[jj]>weights[ii]){
						litmask[ii]=0;
						kk--;
						break;
					}
				}
			}
		}
	}

	/* Free memory and return. */
	MYFREE(weights);
	return(kk);
} /* SelectByWeight */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Select literals by number of variables)  OCJ
 *
 *    This function selects the literals with fewer variables.
 *    A literal mask buffer with as many chars as literals in the
 *    clause must be allocated and initialized before calling this
 *    function. Only the items that are not 0 in the mask are
 *    considered for selection. Literals not selected are set to 0
 *    in the mask. Other mask items are not changed.
 *
 *    It is assumed that clause has some variables but this is NOT
 *    checked.
 *
 *    See "Selecting the selection" by Krystof Hoder et al. for more
 *    details.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask.
 *             It must be initialized by the caller (see comments
 *             above).
 *
 *  RETURNS:
 *
 *    Number of literals selected or NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t SelectByVars(cmprefix *clause,char *litmask){
	auto int32_t *vars;                             /* Number of variables in literals */
	auto uint8_t *ptr1,*ptr2,*ptr3;                 /* Auxiliary pointers */
	auto int32_t ii,jj,kk;                          /* Auxiliary */

	/* Allocate memory for number of variables in literals. */
	if (NULL==(vars=MYALLOC(clause->part2.bin->literals*sizeof(*vars)))){
		return(NOMEMORY);
	}

	/* Initialize number of variables of selectable literals. */
	for (ptr1=&clause->part2.bin->formula[0],ii=kk=0;(*ptr1)!=UNITEND;ptr1=ptr3,ii++){
		ptr3=NextItem(ptr1,OVERSUBTERMS);
		if (litmask[ii]){
			kk++;
			vars[ii]=0;
			for (ptr2=NextItem(ptr1,IMMED);ptr2<ptr3;ptr2=NextItem(ptr2,IMMED)){
				if (VARIABLE&ptr2[0]){
					vars[ii]++;
				}
			}
		}
	}

	/* Loop through all literals. */
	for (ptr1=&clause->part2.bin->formula[0],ii=0;(*ptr1)!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){

		/* Literal is selectable. */
		if (litmask[ii]){

			/* Loop through literals following current literal. */
			for (ptr2=NextItem(ptr1,OVERSUBTERMS),jj=ii+1;(*ptr2)!=UNITEND;
					ptr2=NextItem(ptr2,OVERSUBTERMS),jj++){

				/* If second literal is selectable then compare literals. */
				if (litmask[jj]){
					if (vars[ii]<vars[jj]){
						litmask[jj]=0;
						kk--;
					} else if (vars[jj]<vars[ii]){
						litmask[ii]=0;
						kk--;
						break;
					}
				}
			}
		}
	}

	/* Free memory and return. */
	MYFREE(vars);
	return(kk);
} /* SelectByVars */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Select literals by number of top level variables)  OCJ
 *
 *    This function selects the literals with fewer top level variables.
 *    A literal mask buffer with as many chars as literals in the
 *    clause must be allocated and initialized before calling this
 *    function. Only the items that are not 0 in the mask are
 *    considered for selection. Literals not selected are set to 0
 *    in the mask. Other mask items are not changed.
 *
 *    It is assumed that clause has some variables but this is NOT
 *    checked.
 *
 *    See "Selecting the selection" by Krystof Hoder et al. for more
 *    details.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask.
 *             It must be initialized by the caller (see comments
 *             above).
 *
 *  RETURNS:
 *
 *    Number of literals selected or NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t SelectByTop(cmprefix *clause,char *litmask){
	auto int32_t *vars;                             /* Number of top level variables in literals */
	auto uint8_t *ptr1,*ptr2,*ptr3;                 /* Auxiliary pointers */
	auto int32_t ii,jj,kk;                          /* Auxiliary */

	/* Allocate memory for number of top variables in literals. */
	if (NULL==(vars=MYALLOC(clause->part2.bin->literals*sizeof(*vars)))){
		return(NOMEMORY);
	}

	/* Initialize number of variables of selectable literals. */
	for (ptr1=&clause->part2.bin->formula[0],ii=kk=0;(*ptr1)!=UNITEND;ptr1=ptr3,ii++){
		ptr3=NextItem(ptr1,OVERSUBTERMS);
		if (litmask[ii]){
			kk++;
			vars[ii]=0;
			for (ptr2=NextItem(ptr1,IMMED);ptr2<ptr3;ptr2=NextItem(ptr2,OVERSUBTERMS)){
				if (VARIABLE&ptr2[0]){
					vars[ii]++;
				}
			}
		}
	}

	/* Loop through all literals. */
	for (ptr1=&clause->part2.bin->formula[0],ii=0;(*ptr1)!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){

		/* Literal is selectable. */
		if (litmask[ii]){

			/* Loop through literals following current literal. */
			for (ptr2=NextItem(ptr1,OVERSUBTERMS),jj=ii+1;(*ptr2)!=UNITEND;
					ptr2=NextItem(ptr2,OVERSUBTERMS),jj++){

				/* If second literal is selectable then compare literals. */
				if (litmask[jj]){
					if (vars[ii]<vars[jj]){
						litmask[jj]=0;
						kk--;
					} else if (vars[jj]<vars[ii]){
						litmask[ii]=0;
						kk--;
						break;
					}
				}
			}
		}
	}

	/* Free memory and return. */
	MYFREE(vars);
	return(kk);
} /* SelectByTop */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Select literals by number of distinct variables)  OCJ
 *
 *    This function selects the literals with fewer distinct variables.
 *    A literal mask buffer with as many chars as literals in the
 *    clause must be allocated and initialized before calling this
 *    function. Only the items that are not 0 in the mask are
 *    considered for selection. Literals not selected are set to 0
 *    in the mask. Other mask items are not changed.
 *
 *    It is assumed that clause has some variables but this is NOT
 *    checked.
 *
 *    See "Selecting the selection" by Krystof Hoder et al. for more
 *    details.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask.
 *             It must be initialized by the caller (see comments
 *             above).
 *
 *  RETURNS:
 *
 *    Number of literals selected or NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t SelectByDVars(cmprefix *clause,char *litmask){
	auto int32_t *vars;                             /* Number of distinct variables in literals */
	auto char *usedvars;                            /* 1 if variable has been used, 0 otherwise */
	auto uint8_t *ptr1,*ptr2,*ptr3;                 /* Auxiliary pointers */
	auto int32_t ii,jj,kk;                          /* Auxiliary */

	/* Allocate memory for number of distinct variables in literals. */
	if (NULL==(vars=MYALLOC(clause->part2.bin->literals*sizeof(*vars)))){
		return(NOMEMORY);
	}

	/* Allocate buffer to control distinct variables. */
	if (NULL==(usedvars=MYALLOC(1+clause->part2.bin->maxvarnb))){
		MYFREE(vars);
		return(NOMEMORY);
	}

	/* Initialize number of variables of selectable literals. */
	for (ptr1=&clause->part2.bin->formula[0],ii=kk=0;(*ptr1)!=UNITEND;ptr1=ptr3,ii++){
		ptr3=NextItem(ptr1,OVERSUBTERMS);
		if (litmask[ii]){
			memset(usedvars,0,1+clause->part2.bin->maxvarnb);
			kk++;
			vars[ii]=0;
			for (ptr2=NextItem(ptr1,IMMED);ptr2<ptr3;ptr2=NextItem(ptr2,IMMED)){
				jj=*((int16_t *)&ptr2[1]);
				if ((VARIABLE&ptr2[0])&&(0==usedvars[jj])){
					vars[ii]++;
					usedvars[jj]=1;
				}
			}
		}
	}
	MYFREE(usedvars);

	/* Loop through all literals. */
	for (ptr1=&clause->part2.bin->formula[0],ii=0;(*ptr1)!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){

		/* Literal is selectable. */
		if (litmask[ii]){

			/* Loop through literals following current literal. */
			for (ptr2=NextItem(ptr1,OVERSUBTERMS),jj=ii+1;(*ptr2)!=UNITEND;
					ptr2=NextItem(ptr2,OVERSUBTERMS),jj++){

				/* If second literal is selectable then compare literals. */
				if (litmask[jj]){
					if (vars[ii]<vars[jj]){
						litmask[jj]=0;
						kk--;
					} else if (vars[jj]<vars[ii]){
						litmask[ii]=0;
						kk--;
						break;
					}
				}
			}
		}
	}

	/* Free memory and return. */
	MYFREE(vars);
	return(kk);
} /* SelectByDVars */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Select literals by no positive equalities)  OCJ
 *
 *    This function selects the literals that are not positive
 *    equalities. A literal mask buffer with as many chars as
 *    literals in the clause must be allocated and initialized
 *    before calling this function. Only the items that are not 0
 *    in the mask are considered for selection. Literals not selected
 *    are set to 0 in the mask. Other mask items are not changed.
 *
 *    See "Selecting the selection" by Krystof Hoder et al. for more
 *    details.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask.
 *             It must be initialized by the caller (see comments
 *             above).
 *
 *  RETURNS:
 *
 *    Number of literals selected.
 *
 *--------------------------------------------------------------*/
int32_t SelectByNoPosEq(cmprefix *clause,char *litmask){
	auto uint8_t *ptr1;                             /* Auxiliary pointer */
	auto int32_t ii,jj,kk;                          /* Auxiliary */

	/* Check if there are literals that are not positive equalities. */
	for (ptr1=&clause->part2.bin->formula[0],ii=jj=kk=0;(*ptr1)!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){
		if (litmask[ii]){
			kk++;
			if (EQUALITY!=((EQUALITY|NEGATED)&ptr1[0])){
				jj|=1;
			} else {
				jj|=2;
			}
		}
	}

	/* Return if all selectable literals must be selected. */
	if (jj!=3){
		return(kk);
	}

	/* Loop through all literals and uncheck those that are positive equalities. */
	for (ptr1=&clause->part2.bin->formula[0],ii=0;(*ptr1)!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){
		if ((litmask[ii])&&(EQUALITY==((EQUALITY|NEGATED)&ptr1[0]))){
			litmask[ii]=0;
			kk--;
		}
	}

	return(kk);
} /* SelectByNoPosEq */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Select negative equalities)  OCJ
 *
 *    This function selects the literals that are negative literals
 *    over literals that are not equalities. A literal mask buffer
 *    with as many chars as literals in the clause must be allocated
 *    and initialized before calling this function. Only the items
 *    that are not 0 in the mask are considered for selection. Literals
 *    not selected are set to 0 in the mask. Other mask items are
 *    not changed.
 *
 *    See "Selecting the selection" by Krystof Hoder et al. for more
 *    details.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask.
 *             It must be initialized by the caller (see comments
 *             above).
 *
 *  RETURNS:
 *
 *    Number of literals selected.
 *
 *--------------------------------------------------------------*/
int32_t SelectByNegEq(cmprefix *clause,char *litmask){
	auto uint8_t *ptr1;                             /* Auxiliary pointer */
	auto int32_t ii,jj,kk;                          /* Auxiliary */

	/* Check if there are literals that are not negative equalities. */
	for (ptr1=&clause->part2.bin->formula[0],ii=jj=kk=0;(*ptr1)!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){
		if (litmask[ii]){
			kk++;
			if ((EQUALITY|NEGATED)==((EQUALITY|NEGATED)&ptr1[0])){
				jj|=1;
			} else if (EQUALITY&ptr1[0]){
				jj|=2;
			} else {
				jj|=4;
			}
		}
	}

	/* Return if all selectable literals must be selected. */
	if ((jj!=5)&&(jj!=7)){
		return(kk);
	}

	/* Loop through all literals and uncheck those that are not positive */
	/* or negative equalities. */
	for (ptr1=&clause->part2.bin->formula[0],ii=0;(*ptr1)!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){
		if ((litmask[ii])&&(0==(EQUALITY&ptr1[0]))){
			litmask[ii]=0;
			kk--;
		}
	}

	return(kk);
} /* SelectByNegEq */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Select negative literals)  OCJ
 *
 *    This function selects the literals that are negative over
 *    literals that are not equalities. A literal mask buffer with
 *    as many chars as literals in the clause must be allocated and
 *    initialized before calling this function. Only the items that
 *    are not 0 in the mask are considered for selection. Literals
 *    not selected are set to 0 in the mask. Other mask items are
 *    not changed.
 *
 *    See "Selecting the selection" by Krystof Hoder et al. for more
 *    details.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask.
 *             It must be initialized by the caller (see comments
 *             above).
 *
 *  RETURNS:
 *
 *    Number of literals selected.
 *
 *--------------------------------------------------------------*/
int32_t SelectByNegative(cmprefix *clause,char *litmask){
	auto uint8_t *ptr1;                             /* Auxiliary pointer */
	auto int32_t ii,jj,kk;                          /* Auxiliary */

	/* Check if there are literals that are both selectable and non selectable literals. */
	for (ptr1=&clause->part2.bin->formula[0],ii=jj=kk=0;(*ptr1)!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){
		if (litmask[ii]){
			kk++;
			if (EQUALITY&ptr1[0]){
				jj|=1;
			} else {
				if (NEGATED&ptr1[0]){
					jj|=1;
				} else {
					jj|=2;
				}
			}
		}
	}

	/* Return if all selectable literals must be selected. */
	if (jj!=3){
		return(kk);
	}

	/* Loop through all literals and uncheck those that are positive */
	/* non equalities. */
	for (ptr1=&clause->part2.bin->formula[0],ii=0;(*ptr1)!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){
		if ((litmask[ii])&&(0==((EQUALITY|NEGATED)&ptr1[0]))){
			litmask[ii]=0;
			kk--;
		}
	}

	return(kk);
} /* SelectByNegative */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Select literals with minimal lookahead children)  OCJ
 *
 *    This function selects the literals with fewer estimated
 *    children. A literal mask buffer with as many chars as literals
 *    in the clause must be allocated and initialized before calling
 *    this function. Only the items that are not 0 in the mask are
 *    considered for selection. Literals not selected are set to 0
 *    in the mask. Other mask items are not changed.
 *
 *    The number of children of a literal are estimated as the
 *    estimated children by resolution plus the estimated children by
 *    paramodulating to the literal plus the estimated children by
 *    paramodulating from the literal. The equality resolution and
 *    equality factoring inferences are not taken into account.
 *
 *    The resolution children are estimated by matches of the literal
 *    to discr[0] or discr[1] discrimination trees, depending on literal
 *    polarity.
 *
 *    For paramodulation to the literal sub-terms that are variables and
 *    sub-terms of non selected equality root terms are excluded. The number
 *    of children is estimated by matches of the considered literal
 *    sub-terms to discr[0] plus the number of considered sub-terms times
 *    kbset.vartrmidx.used. There is no need to check paramodulations from
 *    a term to itself because the clause is not active.
 *
 *    For paramodulation from literal only positive equality selected root
 *    terms are considered. The number of children is estimated by matches
 *    of the considered non variable equality root terms to discr[1] plus
 *    the number of considered equality root terms that are variables times
 *    the number of active clauses. There is no need to check paramodulations
 *    from a clause to itself because the clause is not active.
 *
 *    See "Selecting the selection" by Krystof Hoder et al. for more
 *    details.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask.
 *             It must be initialized by the caller (see comments
 *             above).
 *
 *  RETURNS:
 *
 *    Number of literals selected or NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t SelectByLMin(cmprefix *clause,char *litmask){
	#ifdef LKAHDWITHUNIFY
	auto subst Subst;                               /* Substitution */
	#endif
	auto uint8_t *equality;                         /* Pointer to equality with root terms in reverse order */
	auto uint8_t *eqterms;                          /* Pointer to equality root terms in reverse order */
	auto uint8_t *queryitm;                         /* Pointer to QueryDscTree() query item */
	auto int32_t size;                              /* Size of eqterms buffer */
	auto int32_t *children;                         /* Number of estimated children of literals */
	auto uint8_t *rttrm1,*rttrm2;                   /* Equality root terms */
	auto uint32_t minchildren;                      /* Minimum number of children so far */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5;     /* Auxiliary pointers */
	auto int32_t ii,kk,mm,nn,pp,rr,ss,tt;           /* Auxiliary */

	#ifdef LKAHDWITHUNIFY
	/* Initialize substitution, stack and equality root terms buffer. */
	Subst.buffer=NULL;
	eqterms=equality=NULL;
	size=0;
	#endif

	/* Allocate memory for number estimated children of literals. */
	if (NULL==(children=MYALLOC(clause->part2.bin->literals*sizeof(*children)))){
		return(NOMEMORY);
	}

	/* Initialize number of estimated children of selectable literals. */
	minchildren=0xffffffff;
	eqterms=equality=NULL;
	size=0;
	for (ptr1=&clause->part2.bin->formula[0],ii=kk=0;(*ptr1)!=UNITEND;ptr1=ptr3,ii++){
		ptr3=NextItem(ptr1,OVERSUBTERMS);
		if (litmask[ii]){
			kk++;
			children[ii]=0;

			/* If literal is an equality get the equality root terms. */
			if (EQUALITY&ptr1[0]){
				rttrm1=NextItem(ptr1,IMMED);
				rttrm2=NextItem(rttrm1,OVERSUBTERMS);
				mm=2;
			} else {
				rttrm1=rttrm2=NULL;
				mm=1;
			}

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
							MYFREE(children);
							return(NOMEMORY);
						}
						eqterms=&equality[1+sizeof(symbol)];
					} else if (size<(2+sizeof(symbol)+rr+ss)){
						if (NULL==(ptr4=MYREALLOC(equality,size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
							#ifdef LKAHDWITHUNIFY
							MYFREE(Subst.buffer);
							#endif
							MYFREE(children);
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
				nn=pp=0;
				while (QueryUnfDscTree(queryitm,&ptr5,ptr4,nn)){
					nn=1;
					#ifdef LKAHDWITHUNIFY
					switch (Unify(queryitm,ptr5,&Subst,ITEM2SEC|NEGATED|INITSUBST)){

						/* Not enough memory. */
						case NOMEMORY:
							MYFREE(Subst.buffer);
							MYFREE(children);
							MYFREE(equality);
							return(NOMEMORY);
							break;

						/* Candidate unifies with literal. */
						case 1:
							children[ii]++;
							if (children[ii]>minchildren){
								pp=1;
								litmask[ii]=0;
								kk--;
							}
							break;
					}
					if (children[ii]>minchildren){
						break;
					}
					#else
					children[ii]++;
					if (children[ii]>minchildren){
						pp=1;
						litmask[ii]=0;
						kk--;
						break;
					}
					#endif
				}
				if (pp){
					break;
				}
			}
			if (pp){
				continue;
			}

			/* Loop trough literal sub-terms. */
			for (ptr2=NextItem(ptr1,IMMED),pp=0;ptr2<ptr3;ptr2=NextItem(ptr2,IMMED)){

				/* Compute estimated children of paramodulation to subterm. */
				if (0==(VARIABLE&ptr2[0])){
					children[ii]+=kbset.vartrmidx.used;
					if (children[ii]>minchildren){
						litmask[ii]=0;
						kk--;
						break;
					}
					ptr4=(((symbol *)&ptr2[1])->symbol)->discr[0];
					nn=pp=0;
					while (QueryUnfDscTree(ptr2,&ptr5,ptr4,nn)){
						nn=1;
						#ifdef LKAHDWITHUNIFY
						switch (Unify(ptr2,ptr5,&Subst,ITEM2SEC|INITSUBST)){

							/* Not enough memory. */
							case NOMEMORY:
								MYFREE(Subst.buffer);
								MYFREE(children);
								MYFREE(equality);
								return(NOMEMORY);
								break;

							/* Candidate unifies with literal. */
							case 1:
								children[ii]++;
								if (children[ii]>minchildren){
									pp=1;
									litmask[ii]=0;
									kk--;
								}
								break;
						}
						if (children[ii]>minchildren){
							break;
						}
						#else
						children[ii]++;
						if (children[ii]>minchildren){
							pp=1;
							litmask[ii]=0;
							kk--;
							break;
						}
						#endif
					}
					if (pp){
						break;
					}
				}

				/* Compute estimated children of paramodulation from subterm. */
				if ((ptr2==rttrm1)||(ptr2==rttrm2)){
					if (VARIABLE&ptr2[0]){
						children[ii]+=kbset.prstats.nbactive;
						if (children[ii]>minchildren){
							litmask[ii]=0;
							kk--;
							break;
						}
					} else {
						ptr4=(((symbol *)&ptr2[1])->symbol)->discr[1];
						nn=pp=0;
						while (QueryUnfDscTree(ptr2,&ptr5,ptr4,nn)){
							nn=1;
							#ifdef LKAHDWITHUNIFY
							switch (Unify(ptr5,ptr2,&Subst,ITEM2SEC|INITSUBST)){

								/* Not enough memory. */
								case NOMEMORY:
									MYFREE(Subst.buffer);
									MYFREE(children);
									MYFREE(equality);
									return(NOMEMORY);
									break;

								/* Candidate unifies with literal. */
								case 1:
									children[ii]++;
									if (children[ii]>minchildren){
										pp=1;
										litmask[ii]=0;
										kk--;
									}
									break;
							}
							if (children[ii]>minchildren){
								break;
							}
							#else
							children[ii]++;
							if (children[ii]>minchildren){
								pp=1;
								litmask[ii]=0;
								kk--;
								break;
							}
							#endif
						}
						if (pp){
							break;
						}
					}
				}
			}

			/* Update minimum number of children. */
			if (children[ii]<minchildren){
				minchildren=children[ii];
			}
		}
	}

	/* Deselect selectable literals with more children than minchildren. */
	/* Loop through all literals. */
	for (ptr1=&clause->part2.bin->formula[0],ii=0;(*ptr1)!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){
		if ((litmask[ii])&&(children[ii]>minchildren)){
			litmask[ii]=0;
			kk--;
		}
	}

	/* Free memory and return. */
	#ifdef LKAHDWITHUNIFY
	MYFREE(Subst.buffer);
	#endif
	MYFREE(children);
	MYFREE(equality);
	return(kk);
} /* SelectByLMin */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Select literals with maximal lookahead children)  OCJ
 *
 *    This function selects the literals with more estimated
 *    children. A literal mask buffer with as many chars as literals
 *    in the clause must be allocated and initialized before calling
 *    this function. Only the items that are not 0 in the mask are
 *    considered for selection. Literals not selected are set to 0
 *    in the mask. Other mask items are not changed.
 *
 *    See SelectByLMin() function for an explanation on how the number
 *    of children of a literal are estimated.
 *
 *    See "Selecting the selection" by Krystof Hoder et al. for more
 *    details.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask.
 *             It must be initialized by the caller (see comments
 *             above).
 *
 *  RETURNS:
 *
 *    Number of literals selected or NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t SelectByLMax(cmprefix *clause,char *litmask){
	#ifdef LKAHDWITHUNIFY
	auto subst Subst;                               /* Substitution */
	#endif
	auto uint8_t *equality;                         /* Pointer to equality with root terms in reverse order */
	auto uint8_t *eqterms;                          /* Pointer to equality root terms in reverse order */
	auto uint8_t *queryitm;                         /* Pointer to QueryDscTree() query item */
	auto int32_t size;                              /* Size of eqterms buffer */
	auto int32_t *children;                         /* Number of estimated children of literals */
	auto uint8_t *rttrm1,*rttrm2;                   /* Equality root terms */
	auto uint32_t maxchildren;                      /* Maximum number of children so far */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5;     /* Auxiliary pointers */
	auto int32_t ii,kk,mm,nn,rr,ss,tt;              /* Auxiliary */

	#ifdef LKAHDWITHUNIFY
	/* Initialize substitution, stack and equality root terms buffer. */
	Subst.buffer=NULL;
	eqterms=equality=NULL;
	size=0;
	#endif

	/* Allocate memory for number estimated children of literals. */
	if (NULL==(children=MYALLOC(clause->part2.bin->literals*sizeof(*children)))){
		return(NOMEMORY);
	}

	/* Initialize number of estimated children of selectable literals. */
	maxchildren=0;
	eqterms=equality=NULL;
	size=0;
	for (ptr1=&clause->part2.bin->formula[0],ii=kk=0;(*ptr1)!=UNITEND;ptr1=ptr3,ii++){
		ptr3=NextItem(ptr1,OVERSUBTERMS);
		if (litmask[ii]){
			kk++;
			children[ii]=0;

			/* If literal is an equality get the equality root terms. */
			if (EQUALITY&ptr1[0]){
				rttrm1=NextItem(ptr1,IMMED);
				rttrm2=NextItem(rttrm1,OVERSUBTERMS);
				mm=2;
			} else {
				rttrm1=rttrm2=NULL;
				mm=1;
			}

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
							MYFREE(children);
							return(NOMEMORY);
						}
						eqterms=&equality[1+sizeof(symbol)];
					} else if (size<(2+sizeof(symbol)+rr+ss)){
						if (NULL==(ptr4=MYREALLOC(equality,size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
							#ifdef LKAHDWITHUNIFY
							MYFREE(Subst.buffer);
							#endif
							MYFREE(children);
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
							MYFREE(children);
							MYFREE(equality);
							return(NOMEMORY);
							break;

						/* Candidate unifies with literal. */
						case 1:
							children[ii]++;
							break;
					}
					#else
					children[ii]++;
					#endif
				}
			}

			/* Loop trough literal sub-terms. */
			for (ptr2=NextItem(ptr1,IMMED);ptr2<ptr3;ptr2=NextItem(ptr2,IMMED)){

				/* Compute estimated children of paramodulation to subterm. */
				if (0==(VARIABLE&ptr2[0])){
					children[ii]+=kbset.vartrmidx.used;
					ptr4=(((symbol *)&ptr2[1])->symbol)->discr[0];
					nn=0;
					while (QueryUnfDscTree(ptr2,&ptr5,ptr4,nn)){
						nn=1;
						#ifdef LKAHDWITHUNIFY
						switch (Unify(ptr2,ptr5,&Subst,ITEM2SEC|INITSUBST)){

							/* Not enough memory. */
							case NOMEMORY:
								MYFREE(Subst.buffer);
								MYFREE(children);
								MYFREE(equality);
								return(NOMEMORY);
								break;

							/* Candidate unifies with literal. */
							case 1:
								children[ii]++;
								break;
						}
						#else
						children[ii]++;
						#endif
					}
				}

				/* Compute estimated children of paramodulation from subterm. */
				if ((ptr2==rttrm1)||(ptr2==rttrm2)){
					if (VARIABLE&ptr2[0]){
						children[ii]+=kbset.prstats.nbactive;
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
									MYFREE(children);
									MYFREE(equality);
									return(NOMEMORY);
									break;

								/* Candidate unifies with literal. */
								case 1:
									children[ii]++;
									break;
							}
							#else
							children[ii]++;
							#endif
						}
					}
				}
			}

			/* Update maximum number of children. */
			if (children[ii]>maxchildren){
				maxchildren=children[ii];
			}
		}
	}

	/* Deselect selectable literals with less children than maxchildren. */
	/* Loop through all literals. */
	for (ptr1=&clause->part2.bin->formula[0],ii=0;(*ptr1)!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){
		if ((litmask[ii])&&(children[ii]<maxchildren)){
			litmask[ii]=0;
			kk--;
		}
	}

	/* Free memory and return. */
	#ifdef LKAHDWITHUNIFY
	MYFREE(Subst.buffer);
	#endif
	MYFREE(children);
	MYFREE(equality);
	return(kk);
} /* SelectByLMax */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Lexical tie break literal selection)  OCJ
 *
 *    This function is a lexical tie break for the case that more than
 *    one has been selected at the end of the main part of TYPE1 to
 *    TYPE2 selections. A literal mask buffer with as many chars as
 *    literals in the clause must be allocated and initialized before
 *    calling this function. Only the items that are not 0 in the mask
 *    are considered for selection. Literals not selected are set to 0
 *    in the mask. Other mask items are not changed.
 *
 *    It is assumed that the clause has not duplicate literals (the clause
 *    has been previously simplified) so a tie is impossible.
 *
 *    See "Selecting the selection" by Krystof Hoder et al. for more
 *    details.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask.
 *             It must be initialized by the caller (see comments
 *             above).
 *    numsel: Number of literals initially selected in litmask.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void SelectByLexic(cmprefix *clause,char *litmask,int32_t numsel){
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4;           /* Auxiliary pointers */
	auto uint8_t *ptr5,*ptr6;                       /* Auxiliary pointers */
	auto int32_t ii,jj,c11,c12,c21,c22;             /* Auxiliary */

	/* Loop through all literals. In this loop all variables are */
	/* considered the same. */
	for (ptr1=&clause->part2.bin->formula[0],ii=0;((*ptr1)!=UNITEND)&&(numsel>1);
			ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){

		/* Literal is selectable. */
		if (litmask[ii]){

			/* Loop through literals following current literal. */
			for (ptr2=NextItem(ptr1,OVERSUBTERMS),jj=ii+1;((*ptr2)!=UNITEND)&&(numsel>1);
					ptr2=NextItem(ptr2,OVERSUBTERMS),jj++){

				/* If second literal is selectable then compare literals. */
				if (litmask[jj]){

					/* First literal is an equality. */
					if (EQUALITY&ptr1[0]){

						/* Second literal is an equality. */
						if (EQUALITY&ptr2[0]){

							/* We compare t1=t2 and t3=t4 (in)equalities. The first case */
							/* is when one root term in one of the (in)equalities is greater */
							/* than both root terms in the other (in)equality. */
							ptr3=NextItem(ptr1,IMMED);
							ptr4=NextItem(ptr3,OVERSUBTERMS);
							ptr5=NextItem(ptr2,IMMED);
							ptr6=NextItem(ptr5,OVERSUBTERMS);
							c11=CompareLexic4Select(ptr3,ptr5);
							c12=CompareLexic4Select(ptr3,ptr6);
							c21=CompareLexic4Select(ptr4,ptr5);
							c22=CompareLexic4Select(ptr4,ptr6);
							if (((c11==GREATER)&&(c12==GREATER))||((c21==GREATER)&&(c22==GREATER))){
								litmask[jj]=0;
								numsel--;
							} else if (((c11==LESSER)&&(c21==LESSER))||((c12==LESSER)&&(c22==LESSER))){
								litmask[ii]=0;
								numsel--;

							/* If we are here then due to ordering transitivity one root term of */
							/* one (in)equality must be equal to one root term of the other */
							/* (in)equality. Next we check three of the four possible cases. */
							} else if (c11==EQUAL){
								if (c22==GREATER){
									litmask[jj]=0;
									numsel--;
								} else if (c22==LESSER){
									litmask[ii]=0;
									numsel--;
								} else {
									printf("%%=====> Assertion error in SelectByLexic\n");
									litmask[jj]=0;
									numsel--;
								}
							} else if (c12==EQUAL){
								if (c21==GREATER){
									litmask[jj]=0;
									numsel--;
								} else if (c21==LESSER){
									litmask[ii]=0;
									numsel--;
								} else {
									printf("%%=====> Assertion error in SelectByLexic\n");
									litmask[jj]=0;
									numsel--;
								}
							} else if (c21==EQUAL){
								if (c12==GREATER){
									litmask[jj]=0;
									numsel--;
								} else if (c12==LESSER){
									litmask[ii]=0;
									numsel--;
								} else {
									printf("%%=====> Assertion error in SelectByLexic\n");
									litmask[jj]=0;
									numsel--;
								}

							/* If we are here then this is the fourth fourth possible case: */
							/* the second root terms of the (in)equalities are equal. */
							} else {
								if (c11==GREATER){
									litmask[jj]=0;
									numsel--;
								} else if (c11==LESSER){
									litmask[ii]=0;
									numsel--;
								} else {
									printf("%%=====> Assertion error in SelectByLexic\n");
									litmask[jj]=0;
									numsel--;
								}
							}

						/* Second literal is not an equality. */
						} else {
							litmask[ii]=0;
							numsel--;
						}

					/* First literal is not an equality and second literal is an equality. */
					} else if (EQUALITY&ptr2[0]){
						litmask[jj]=0;
						numsel--;

					/* None of the literals are equalities. */
					} else {
						switch (CompareLexic4Select(ptr1,ptr2)){
							case GREATER:
								litmask[jj]=0;
								numsel--;
								break;
							case LESSER:
								litmask[ii]=0;
								numsel--;
								break;
							default:
								printf("%%=====> Assertion error in SelectByLexic\n");
								litmask[jj]=0;
								numsel--;
								break;
						}
					}
				}
			}
		}
	}
	return;
} /* SelectByLexic */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Lexical comparison for SelectByLexic function)  OCJ
 *
 *    This function performs a lexical comparison for SelectByLexic()
 *    function. The compared items may be terms or predicates that
 *    are not equalities or inequalities.
 *
 *  ARGUMENTS:
 *
 *    item1: Pointer to first item to be compared.
 *    item2: Pointer to second item to be compared.
 *
 *  RETURNS:
 *
 *    GREATER if item1 is greater than item2.
 *    LESSER if item1 is lesser than item2.
 *    EQUAL otherwise.
 *
 *--------------------------------------------------------------*/
int32_t CompareLexic4Select(uint8_t *item1,uint8_t *item2){
	auto uint8_t *ptr1,*ptr2,*ptr3;                 /* Auxiliary pointers */
	auto int32_t ii;                                /* Auxiliary */
	auto uint64_t jj,kk;                            /* Auxiliary */

	/* Loop through items sub-terms. */
	ptr3=NextItem(item1,OVERSUBTERMS);
	for (ptr1=item1,ptr2=item2,ii=EQUAL;ptr1<ptr3;
			ptr1=NextItem(ptr1,IMMED),ptr2=NextItem(ptr2,IMMED)){
		if (VARIABLE&ptr1[0]){
			if (0==(VARIABLE&ptr2[0])){
				return(LESSER);
			} else if (ii==EQUAL){
				if ((*((int16_t *)&ptr1[1]))>(*((int16_t *)&ptr2[1]))){
					ii=GREATER;
				} else if ((*((int16_t *)&ptr1[1]))<(*((int16_t *)&ptr2[1]))){
					ii=LESSER;
				}
			}
		} else if (VARIABLE&ptr2[0]){
			return(GREATER);
		} else {
			jj=((symbol *)&ptr1[1])->symbol->precedence;
			kk=((symbol *)&ptr2[1])->symbol->precedence;
			if (jj>kk){
				return(GREATER);
			} else if (kk>jj){
				return(LESSER);
			}
		}
	}

	/* There was a tie in previous check. Items are equal up to variable numbers. */
	/* Use variable number for comparison. */
	return(ii);
} /* CompareLexic4Select */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Type 1 literal selection)  OCJ
 *
 *    This function performs type 1 selection. The following selections
 *    are performed until only one literal is selected:
 *    - Select by weight
 *    - Select by lexicon
 *
 *    A literal mask buffer with as many chars as literals in the clause
 *    must be allocated and initialized before calling this function. Only
 *    the items that are not 0 in the mask are considered for selection.
 *    Literals not selected are set to 0 in the mask. Other mask items
 *    are not changed.
 *
 *    See "Selecting the selection" by Krystof Hoder et al. for more
 *    details.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask.
 *             It must be initialized by the caller (see comments
 *             above).
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t SelectType1(cmprefix *clause,char *litmask){
	auto int32_t ii;                                /* Auxiliary */
	ii=SelectByWeight(clause,litmask);
	if (ii==NOMEMORY){
		return(NOMEMORY);
	} else  if (ii==1){
		return(0);
	}
	SelectByLexic(clause,litmask,ii);
	return(0);
} /* SelectType1 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Type 2 literal selection)  OCJ
 *
 *    This function performs type 2 selection. The following selections
 *    are performed until only one literal is selected:
 *    - Select by no positive equalities
 *    - Select by number of top level variables
 *    - Select by number of distinct variables
 *    - Select by lexicon
 *
 *    A literal mask buffer with as many chars as literals in the clause
 *    must be allocated and initialized before calling this function. Only
 *    the items that are not 0 in the mask are considered for selection.
 *    Literals not selected are set to 0 in the mask. Other mask items
 *    are not changed.
 *
 *    See "Selecting the selection" by Krystof Hoder et al. for more
 *    details.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask.
 *             It must be initialized by the caller (see comments
 *             above).
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t SelectType2(cmprefix *clause,char *litmask){
	auto int32_t ii;                                /* Auxiliary */
	ii=0;
	if (pbmtype<6){
		ii=SelectByNoPosEq(clause,litmask);
		if (ii==NOMEMORY){
			return(NOMEMORY);
		} else  if (ii==1){
			return(0);
		}
	}
	if (clause->part2.bin->maxvarnb>=0){
		ii=SelectByTop(clause,litmask);
		if (ii==NOMEMORY){
			return(NOMEMORY);
		} else  if (ii==1){
			return(0);
		}
		ii=SelectByDVars(clause,litmask);
		if (ii==NOMEMORY){
			return(NOMEMORY);
		} else  if (ii==1){
			return(0);
		}
	}
	if (ii==0){
		ii=clause->part2.bin->literals;
		memset(litmask,1,ii);
	}
	SelectByLexic(clause,litmask,ii);
	return(0);
} /* SelectType2 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Type 3 literal selection)  OCJ
 *
 *    This function performs type 3 selection. The following selections
 *    are performed until only one literal is selected:
 *    - Select by no positive equalities
 *    - Select by number of top level variables
 *    - Select by number of variables
 *    - Select by weight
 *    - Select by lexicon
 *
 *    A literal mask buffer with as many chars as literals in the clause
 *    must be allocated and initialized before calling this function. Only
 *    the items that are not 0 in the mask are considered for selection.
 *    Literals not selected are set to 0 in the mask. Other mask items
 *    are not changed.
 *
 *    See "Selecting the selection" by Krystof Hoder et al. for more
 *    details.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask.
 *             It must be initialized by the caller (see comments
 *             above).
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t SelectType3(cmprefix *clause,char *litmask){
	auto int32_t ii;                                /* Auxiliary */
	if (pbmtype<6){
		ii=SelectByNoPosEq(clause,litmask);
		if (ii==NOMEMORY){
			return(NOMEMORY);
		} else  if (ii==1){
			return(0);
		}
	}
	if (clause->part2.bin->maxvarnb>=0){
		ii=SelectByTop(clause,litmask);
		if (ii==NOMEMORY){
			return(NOMEMORY);
		} else  if (ii==1){
			return(0);
		}
		ii=SelectByVars(clause,litmask);
		if (ii==NOMEMORY){
			return(NOMEMORY);
		} else  if (ii==1){
			return(0);
		}
	}
	ii=SelectByWeight(clause,litmask);
	if (ii==NOMEMORY){
		return(NOMEMORY);
	} else  if (ii==1){
		return(0);
	}
	SelectByLexic(clause,litmask,ii);
	return(0);
} /* SelectType3 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Type 4 literal selection)  OCJ
 *
 *    This function performs type 4 selection. The following selections
 *    are performed until only one literal is selected:
 *    - Select by negative equalities
 *    - Select by weight
 *    - Select negative literals
 *    - Select by lexicon
 *
 *    A literal mask buffer with as many chars as literals in the clause
 *    must be allocated and initialized before calling this function. Only
 *    the items that are not 0 in the mask are considered for selection.
 *    Literals not selected are set to 0 in the mask. Other mask items
 *    are not changed.
 *
 *    See "Selecting the selection" by Krystof Hoder et al. for more
 *    details.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask.
 *             It must be initialized by the caller (see comments
 *             above).
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t SelectType4(cmprefix *clause,char *litmask){
	auto int32_t ii;                                /* Auxiliary */
	if (pbmtype<6){
		ii=SelectByNegEq(clause,litmask);
		if (ii==NOMEMORY){
			return(NOMEMORY);
		} else  if (ii==1){
			return(0);
		}
	}
	ii=SelectByWeight(clause,litmask);
	if (ii==NOMEMORY){
		return(NOMEMORY);
	} else  if (ii==1){
		return(0);
	}
	ii=SelectByNegative(clause,litmask);
	if (ii==NOMEMORY){
		return(NOMEMORY);
	} else  if (ii==1){
		return(0);
	}
	SelectByLexic(clause,litmask,ii);
	return(0);
} /* SelectType4 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Type 5 literal selection)  OCJ
 *
 *    This function performs type 5 selection. The following selections
 *    are performed until only one literal is selected:
 *    - Minimal lookahead children selection
 *    - Type 2 selection
 *
 *    A literal mask buffer with as many chars as literals in the clause
 *    must be allocated and initialized before calling this function. Only
 *    the items that are not 0 in the mask are considered for selection.
 *    Literals not selected are set to 0 in the mask. Other mask items
 *    are not changed.
 *
 *    See "Selecting the selection" by Krystof Hoder et al. for more
 *    details.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask.
 *             It must be initialized by the caller (see comments
 *             above).
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t SelectType5(cmprefix *clause,char *litmask){
	auto int32_t ii;                                /* Auxiliary */
	ii=SelectByLMin(clause,litmask);
	if (ii==NOMEMORY){
		return(NOMEMORY);
	} else if (ii==1){
		return(0);
	}
	return(SelectType2(clause,litmask));
} /* SelectType5 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Type 6 literal selection)  OCJ
 *
 *    This function performs type 6 selection. The following selections
 *    are performed until only one literal is selected:
 *    - Maximal lookahead children selection
 *    - Type 2 selection
 *
 *    A literal mask buffer with as many chars as literals in the clause
 *    must be allocated and initialized before calling this function. Only
 *    the items that are not 0 in the mask are considered for selection.
 *    Literals not selected are set to 0 in the mask. Other mask items
 *    are not changed.
 *
 *    See "Selecting the selection" by Krystof Hoder et al. for more
 *    details.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask.
 *             It must be initialized by the caller (see comments
 *             above).
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t SelectType6(cmprefix *clause,char *litmask){
	auto int32_t ii;                                /* Auxiliary */
	ii=SelectByLMax(clause,litmask);
	if (ii==NOMEMORY){
		return(NOMEMORY);
	} else if (ii==1){
		return(0);
	}
	return(SelectType2(clause,litmask));
} /* SelectType6 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Complete version of Type 1 to 4 literal selection)  OCJ
 *
 *    This function performs a complete type 1 to 4 selection. See general
 *    comments of SelectTypex() functions (x=1 to 4) for details on types 1
 *    to 4 selections.
 *
 *    A literal mask buffer with as many chars as literals in the clause
 *    must be allocated and filled with 1 before calling this function.
 *
 *    See "Selecting the selection" by Krystof Hoder et al. for details
 *    details on how to make the selection complete.
 *
 *  ARGUMENTS:
 *
 *    selectfunc: Pointer to appropriate selection function.
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask.
 *             It must be initialized by the caller (see comments
 *             above).
 *    eqs: Number of equalities and inequalities in clause.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t SelectType14Cpl(int32_t (*selectfunc)(cmprefix *,char *),cmprefix *clause,char *litmask,int32_t eqs){
	auto char *selectable;                          /* Selectable literals mask. */
	auto char *maxlits;                             /* Maximal literals */
	auto int32_t selpos;                            /* Number of maximal positive literals */
	auto int32_t selneg;                            /* Number of maximal negative literals */
	auto int32_t numneg;                            /* Number of negative literals */
	auto int32_t selectednum;                       /* Number of literal selected by selectfunc() function */
	auto uint8_t *ptr1;                             /* Auxiliary pointer */
	auto int32_t ii;                                /* Auxiliary */

	/* Allocate and initialize memory for selectable literals mask. */
	if (NULL==(selectable=MYALLOC(clause->part2.bin->literals))){
		return(NOMEMORY);
	}
	memset(selectable,1,clause->part2.bin->literals);

	/* Main loop. */
	maxlits=NULL;
	while (1){

		/* Make a selection. */
		if (NOMEMORY==selectfunc(clause,litmask)){
			MYFREE(selectable);
			MYFREE(maxlits);
			return(NOMEMORY);
		}

		/* Check if the selected literal is negative. */
		/* Set selectednum to the literal number of selected literal. */
		for (ptr1=&clause->part2.bin->formula[0],selectednum=0;litmask[selectednum]==0;
				ptr1=NextItem(ptr1,OVERSUBTERMS),selectednum++){
		}
		if (NEGATED&ptr1[0]){
			MYFREE(selectable);
			MYFREE(maxlits);
			return(0);
		}

		/* Allocate memory for maxlits mask an initialize it if necessary. */
		if (maxlits==NULL){
			if (NULL==(maxlits=MYALLOC(clause->part2.bin->literals))){
				MYFREE(selectable);
				return(NOMEMORY);
			}
			memset(maxlits,0,clause->part2.bin->literals);
			if (NOMEMORY==SelectMaximal(clause,maxlits,eqs,&selpos,&selneg,&numneg)){
				MYFREE(selectable);
				MYFREE(maxlits);
				return(NOMEMORY);
			}
		}

		/* All maximal literals are positive. */
		if (selneg==0){

			/* If selected literal is maximal then select all maximal literals */
			/* and leave the loop. */
			if (maxlits[selectednum]){
				memcpy(litmask,maxlits,clause->part2.bin->literals);
				break;
			}

			/* Remove selected literal from selectable literals. */
			selectable[selectednum]=0;

		/* If some maximal literals are negative set selectable literals mask */
		/* to the set of all maximal negative literals. */
		} else {
			for (ptr1=&clause->part2.bin->formula[0],ii=0;ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){
				if (maxlits[ii]){
					if (NEGATED&ptr1[0]){
						selectable[ii]=1;
					} else {
						selectable[ii]=0;
					}
				} else {
					selectable[ii]=0;
				}
			}
		}

		/* Copy selectable literals to litmask, */
		memcpy(litmask,selectable,clause->part2.bin->literals);
	}

	/* Free memory and return. */
	MYFREE(selectable);
	MYFREE(maxlits);
	return(0);
} /* SelectType14Cpl */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Complete version of Type 5 and 6 literal selection)  OCJ
 *
 *    This function performs a complete type 5 and 6 selection. See general
 *    comments of SelectTypex() functions (x=5 or 6) for details on types 5
 *    and 6 selections. There are some cases that this selection is not
 *    complete. In that cases the lkhdcomplete global variable is set to 0.
 *    See general comments of SelectLiterals1() function for additional
 *    details of completeness in this context.
 *
 *    A literal mask buffer with as many chars as literals in the clause
 *    must be allocated and filled with 1 before calling this function.
 *
 *    See "Selecting the selection" by Krystof Hoder et al. for details
 *    details on how to make the selection complete.
 *
 *  ARGUMENTS:
 *
 *    selectfunc: Pointer to appropriate selection function.
 *    clause: Pointer to clause in binary form.
 *    litmask: Pointer to buffer with caller provided literal mask.
 *             It must be initialized by the caller (see comments
 *             above).
 *    eqs: Number of equalities and inequalities in clause.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t SelectType56Cpl(int32_t (*selectfunc)(cmprefix *,char *),cmprefix *clause,char *litmask,int32_t eqs){
	auto char *maxlits;                             /* Maximal literals mask */
	auto int32_t selpos;                            /* Number of maximal positive literals */
	auto int32_t selneg;                            /* Number of maximal negative literals */
	auto int32_t numneg;                            /* Number of negative literals */
	auto uint8_t *ptr1;                             /* Auxiliary pointer */
	auto int32_t ii;                                /* Auxiliary */


	/* Allocate memory for maxlits mask and initialize it if necessary. */
	if (NULL==(maxlits=MYALLOC(clause->part2.bin->literals))){
		return(NOMEMORY);
	}
	memset(maxlits,0,clause->part2.bin->literals);
	if (NOMEMORY==SelectMaximal(clause,maxlits,eqs,&selpos,&selneg,&numneg)){
		MYFREE(maxlits);
		return(NOMEMORY);
	}

	/* If there are not negative literals then select all maximal literals. */
	if (numneg==0){
		memcpy(litmask,maxlits,clause->part2.bin->literals);
		MYFREE(maxlits);
		return(0);
	}

	/* Set selectable literals to all negative literals and a single maximal */
	/* positive literal (if there is only one) and perform selection. */
	for (ptr1=&clause->part2.bin->formula[0],ii=0;ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){
		if ((NEGATED&ptr1[0])||((maxlits[ii])&&(selpos==1))){
			litmask[ii]=1;
		} else {
			litmask[ii]=0;
		}
	}
	MYFREE(maxlits);
	if (NOMEMORY==selectfunc(clause,litmask)){
		return(NOMEMORY);
	}

	/* Set lkhdcomplete global variable to 0 if the selection is not complete. */
	if (selpos==1){
		for (ptr1=&clause->part2.bin->formula[0],ii=0;ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){
			if (litmask[ii]){
				if (0==(NEGATED&ptr1[0])){
					lkhdcomplete=0;
				}
				break;
			}
		}
	}
	return(0);
} /* SelectType56Cpl */
