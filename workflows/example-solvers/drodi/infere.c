/*
 ============================================================================
 Name        : infere.c
 Author      : Oscar Contreras
 Version     :
 Copyright   : Copyright (C) 2015-2016 Oscar Contreras. All rights reserved.
 Description : First order logic theorem prover in C
 ============================================================================
 */

/****************************************************************
*
*               infere (Inference functions module)
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
 *    This source contains the deductive inference and auxiliary functions
 *    of the Drodi package. Simplification inferences are not included here,
 *    they are coded in the simplify module.
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
#include "goaldef.h"

/** Global variables for this module: only those that need initialization in their definition. */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Jump to next item in clause)  OCJ
 *
 *    This function returns a pointer to the item immediately
 *    following a given item in a clause or to the next item that
 *    is not a subterm of the given item, depending on the
 *    value of the type argument.
 *
 *    It is assumed that item parameter is a valid pointer to
 *    an item and that the value of nextoff-offset is correct
 *    if the item is not a variable and type is OVERSUBTERMS.
 *    Otherwise the result is unpredictable. Items copied from
 *    other clauses are valid as far as they are not modified,
 *    i.e. by application of a substitution.
 *
 *
 *  ARGUMENTS:
 *
 *    item: pointer to an item in a clause.
 *    type: If this parameter is OVERSUBTERMS then the function
 *          returns a pointer to next item that is not a subterm
 *          of the given item. If this parameter is IMMED then
 *          the function returns a pointer to the item immediately
 *          following the given item. The default value is IMMED.
 *
 *  RETURNS:
 *
 *    Pointer to next item according to type parameter.
 *
 *--------------------------------------------------------------*/
uint8_t *NextItem(uint8_t *item,int32_t type){

	/* Item is a variable. */
	if (VARIABLE&(*item)){
		return(&item[3]);
	}

	/* Item is not a variable and type is OVERSUBTERMS. */
	if (type==OVERSUBTERMS){
		return(item+((((symbol *)(((char *)item)+1))->nextoff)
				-(((symbol *)(((char *)item)+1))->offset)));
	}

	/* Item is not a variable and type is not OVERSUBTERMS. */
	return(&item[1+sizeof(symbol)]);
} /* NextItem */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if two items are identical)  OCJ
 *
 *    This function checks if two items of the same binary clause
 *    are identical including all subterms, except perhaps for
 *    one item being positive and the other negative.
 *
 *    This function doesn't detect equalities that are equal after
 *    applying equality symmetry.
 *
 *    IMPORTANT: item2 may be placed in a substitution but in any case
 *    it must belong to the same clause as item1. The item1 must
 *    be placed in a clause, not in a substitution.
 *
 *
 *  ARGUMENTS:
 *
 *    item1: pointer to first item in a clause.
 *    item2: pointer to second item in a clause.
 *    type: If this parameter is NEGATED then one item must be
 *          positive and the other negative. Otherwise both items
 *          must be positive or both negative. NEGATED must be used
 *          only if items are literals, otherwise the function
 *          will always return 0.
 *
 *  RETURNS:
 *
 *    1 if both items match, 0 otherwise.
 *
 *--------------------------------------------------------------*/
int32_t IsEqual(uint8_t *item1,uint8_t *item2,int32_t type){
	auto uint8_t *ptr1,*ptr2,*ptr3;                      /* Auxiliary pointers */

	/* Byte markers match. */
	if (((type==NEGATED)&&((((*item1)&(EQUALITY|PREDICATE|VARIABLE|FUNCTION|NEGATED))^NEGATED)
			==((*item2)&(EQUALITY|PREDICATE|VARIABLE|FUNCTION|NEGATED))))
		||((type!=NEGATED)&&(((*item1)&(EQUALITY|PREDICATE|VARIABLE|FUNCTION|NEGATED))
			==((*item2)&(EQUALITY|PREDICATE|VARIABLE|FUNCTION|NEGATED))))){

		/* Loop through items and all subterms in them. */
		for (ptr1=item1,ptr2=item2,ptr3=NextItem(item1,OVERSUBTERMS);
				ptr1<ptr3;ptr1=NextItem(ptr1,IMMED),ptr2=NextItem(ptr2,IMMED)){

			/* Subterm types don't match. */
			if ((ptr1[0]&(VARIABLE|PREDICATE|EQUALITY|FUNCTION))
					!=(ptr2[0]&(VARIABLE|PREDICATE|EQUALITY|FUNCTION))){
				return(0);
			}

			/* Subterms are variables. */
			if ((*ptr1)&VARIABLE){

				/* Variables don't match. */
				if ((*(int16_t *)&ptr1[1])!=(*(int16_t *)&ptr2[1])){
					return(0);
				}

			/* Subterms are literals or functions that don't match. */
			} else if ((((symbol *)&ptr1[1])->symbol)!=(((symbol *)&ptr2[1])->symbol)){
				return(0);
			}
		}
		return(1);
	}

	/* Items don't match. */
	return(0);
} /* IsEqual */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if two items in different clauses are identical)  OCJ
 *
 *    This function is similar to IsEqual() function except that items
 *    are in different clauses, therefore they must be ground terms or
 *    literals in order to be equal.
 *
 *
 *  ARGUMENTS:
 *
 *    item1: pointer to first item in a clause.
 *    item2: pointer to second item in a clause.
 *
 *  RETURNS:
 *
 *    1 if both items match, 0 otherwise.
 *
 *--------------------------------------------------------------*/
int32_t IsEqual2(uint8_t *item1,uint8_t *item2){
	auto uint8_t *ptr1,*ptr2,*ptr3;                      /* Auxiliary pointers */

	/* If one of the items is a variable then return no match. */
	if ((VARIABLE&item1[0])||(VARIABLE&item2[0])){
		return(0);
	}

	/* Byte markers match. */
	if (((*item1)&(EQUALITY|PREDICATE|FUNCTION|NEGATED))
			==((*item2)&(EQUALITY|PREDICATE|FUNCTION|NEGATED))){

		/* Loop through items and all subterms in them. */
		for (ptr1=item1,ptr2=item2,ptr3=NextItem(item1,OVERSUBTERMS);
				ptr1<ptr3;ptr1=NextItem(ptr1,IMMED),ptr2=NextItem(ptr2,IMMED)){

			/* If one of the subterms is a variable the return no math. */
			if ((VARIABLE&ptr1[0])||(VARIABLE&ptr2[0])){
				return(0);
			}

			/* Subterm types don't match. */
			if ((ptr1[0]&(PREDICATE|EQUALITY|FUNCTION))
					!=(ptr2[0]&(PREDICATE|EQUALITY|FUNCTION))){
				return(0);
			}

			/* Subterms are literals or functions that don't match. */
			if ((((symbol *)&ptr1[1])->symbol)!=(((symbol *)&ptr2[1])->symbol)){
				return(0);
			}
		}
		return(1);
	}

	/* Items don't match. */
	return(0);
} /* IsEqual2 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform literal or term unification)  OCJ
 *
 *    This function performs a literal or term unification. If the
 *    unification is successful then a substitution is added to the
 *    substitution structure received.
 *
 *    A substitution is a set of instructions to substitute a set
 *    of variables, each one to be substituted with a term.
 *    The buffer field in the substitution contains the substitution
 *    information that consists in one or more atomic substitutions
 *    one after the other. The atomic substitutions have the following
 *    format:
 *
 *      flags varnum term
 *
 *    where:
 *
 *      flags: is a byte with the following possible flags:
 *             ITEM1SEC -> The variable being substituted belongs to a secondary
 *             clause. It belongs to the main clause by default.
 *             ITEM2SEC -> The substituting term belongs to a secondary
 *             clause. It belongs to the main clause by default.
 *      varnum: 2 byte integer with the number of the variable to be
 *              replaced by the substitution. This together with the ITEM1SEC
 *              completely identifies the variable being substituted.
 *      term: The term that replaces the variable in internal binary format
 *            (see CompileCNF() function for a description of the term format).
 *
 *    The end of the substitution data is marked with a UNITEND byte.
 *
 *    IMPORTANT: This function doesn't build the most general unification mainly
 *    because variable elimination is not fully performed. However the relevant
 *    functions (mainly ApplySubst2Clause() and CheckVarInTerm()) have been
 *    designed to take this into account so that the final effect is as if the
 *    most general unification had been performed. This is not very elegant but
 *    tests performed showed that it is faster than using the function that builds
 *    a most general unification.
 *
 *
 *  ARGUMENTS:
 *
 *    part1: pointer to a literal, term or list of terms. It must be
 *           in the internal binary format.
 *    part2: pointer to a literal, term or list of terms. It must be
 *           in the internal binary format.
 *    Subst: pointer to a substitution structure.
 *    flags: ITEM1SEC -> The part1 argument belongs to a secondary clause.
 *           It belongs to the main clause by default.
 *           ITEM2SEC -> The part2 argument belongs to a secondary clause.
 *           It belongs to the main clause by default.
 *           NEGATED -> part1 must unify with the negation of part2.
 *           This flag is only valid for literals unification, being ignored
 *           for other kinds of unifications.
 *           INITSUBST -> Initialize substitution to NULL (just for this instance,
 *           but not for subsequent recursive calls to Unify()).
 *           DEBUG -> Call BuildTxtSubstData() function to show results in text form.
 *           The call is performed only if DEBUGCODE is defined and only if terms
 *           match. This flag is not propagated to subsequent nested calls.
 *
 *  RETURNS:
 *
 *    0 -> Unification was not successful.
 *    1 -> Unification was successful.
 *    NOMEMORY -> Not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t Unify(uint8_t *part1,uint8_t *part2,subst *Subst,int32_t flags){
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4;                /* Auxiliary pointer */
	auto int32_t ii,jj;                                  /* Auxiliary */
	#ifdef DEBUGCODE
	auto int32_t dd;                                     /* Auxiliary */
	#endif

	/* Initialize substitution if necessary. */
	if (flags&INITSUBST){
		if (Subst->buffer!=NULL){
			Subst->substsize=1;
			Subst->buffer[0]=UNITEND;
		}
		flags&=(~INITSUBST);
	}

	/* Remember address of substitution buffer and check if part1 */
	/* and part2 are inside the substitution. */
	jj=0;
	ptr4=Subst->buffer;
	if (ptr4!=NULL){
		if ((part1>ptr4)&&((part1-ptr4)<Subst->substsize)){
			jj=1;
		}
		if ((part2>ptr4)&&((part2-ptr4)<Subst->substsize)){
			jj|=2;
		}
	}

	/* Remember DEBUG flag and remove it from flags. */
	#ifdef DEBUGCODE
	dd=flags&DEBUG;
	flags&=~DEBUG;
	#endif

	/* Loop through all subterms of both parts. */
	for (ptr1=part1,ptr3=NextItem(part1,OVERSUBTERMS),ptr2=part2;ptr1<ptr3;){

		/* Subterm of part 1 is a variable. */
		if ((*ptr1)&VARIABLE){

			/* Part1 and Part2 are the same variable. */
			if (((*part2)&VARIABLE)&&MATCHFLGS12(flags,flags)
					&&((*(int16_t *)&ptr1[1])==(*(int16_t *)&ptr2[1]))){
				#ifdef DEBUGCODE
				if (dd){
					BuildTxtSubstData(ptr1,ptr2,Subst,flags);
				}
				#endif
				return(1);
			}

			/* Unify variable with ptr2. */
			switch (ii=UnifyVar(ptr1,ptr2,Subst,flags)){
				case NOMEMORY:
				case 0:
					return(ii);
					break;
			}

			/* Update pointers if substitution has been reallocated. */
			if (Subst->buffer!=ptr4){
				if (jj&1){
					ptr1=Subst->buffer+(ptr1-ptr4);
					ptr3=Subst->buffer+(ptr3-ptr4);
				}
				if (jj&2){
					part2=Subst->buffer+(part2-ptr4);
					ptr2=Subst->buffer+(ptr2-ptr4);
				}
				ptr4=Subst->buffer;
			}

			#ifdef DEBUGCODE
			if (dd){
				BuildTxtSubstData(ptr1,ptr2,Subst,flags);
			}
			#endif

			/* Prepare next iteration. */
			ptr1+=3;
			ptr2=NextItem(ptr2,OVERSUBTERMS);

		/* Subterm of part 2 is a variable. Unify variable with subterm of part 1. */
		} else if ((*ptr2)&VARIABLE){

			/* Unify variable with part1. */
			ii=flags;
			if (flags&ITEM1SEC){
				ii|=ITEM2SEC;
			} else {
				ii&=~ITEM2SEC;
			}
			if (flags&ITEM2SEC){
				ii|=ITEM1SEC;
			} else {
				ii&=~ITEM1SEC;
			}
			switch (ii=UnifyVar(ptr2,ptr1,Subst,ii)){
				case NOMEMORY:
				case 0:
					return(ii);
					break;
			}

			/* Update pointers if substitution has been reallocated. */
			if (Subst->buffer!=ptr4){
				if (jj&1){
					ptr1=Subst->buffer+(ptr1-ptr4);
					ptr3=Subst->buffer+(ptr3-ptr4);
				}
				if (jj&2){
					part2=Subst->buffer+(part2-ptr4);
					ptr2=Subst->buffer+(ptr2-ptr4);
				}
				ptr4=Subst->buffer;
			}

			#ifdef DEBUGCODE
			if (dd){
				BuildTxtSubstData(ptr1,ptr2,Subst,flags);
			}
			#endif

			/* Prepare next iteration. */
			ptr1=NextItem(ptr1,OVERSUBTERMS);
			ptr2+=3;

		/* Subterms are non matching literals or functions. */
		} else if (((0==(flags&NEGATED))&&(((*ptr1)&(EQUALITY|PREDICATE|FUNCTION|NEGATED))
					!=((*ptr2)&(EQUALITY|PREDICATE|FUNCTION|NEGATED))))
				||((flags&NEGATED)&&((((*ptr1)&(EQUALITY|PREDICATE|FUNCTION|NEGATED))^NEGATED)
					!=((*ptr2)&(EQUALITY|PREDICATE|FUNCTION|NEGATED))))
				||((((symbol *)&ptr1[1])->symbol)!=(((symbol *)&ptr2[1])->symbol))){
			return(0);

		/* Subterms are matching literals or functions. Prepare */
		/* next iteration and clear NEGATED flag. */
		} else {
			ptr1=NextItem(ptr1,IMMED);
			ptr2=NextItem(ptr2,IMMED);
			flags&=(~NEGATED);
		}
	}

	/* Successful unification. */
	return(1);
} /* Unify */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Unify a variable with a term)  OCJ
 *
 *    This function tries to unify a variable with a term.
 *    If the variable and the term to be unified belong to the
 *    same clause then the variable must not be in the term.
 *    If the unification is successful then a substitution is
 *    added to the received substitution structure. See Unify()
 *    function for a description of the substitution format.
 *
 *
 *  ARGUMENTS:
 *
 *    var: pointer to a term that is a single variable. It must be
 *         in compiled binary format.
 *    term: pointer to a term in compiled binary format.
 *    Subst: pointer to a substitution structure.
 *    flags: ITEM1SEC -> The var function argument belongs to a secondary
 *           clause. It belongs to the main clause by default.
 *           ITEM2SEC -> The term function argument belongs to a secondary
 *           clause. It belongs to the main clause by default.
 *           There may be other flags but they are ignored.
 *
 *  RETURNS:
 *
 *    0 -> Unification was not successful.
 *    1 -> Unification was successful.
 *    NOMEMORY -> Not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t UnifyVar(uint8_t *var,uint8_t *term,subst *Subst,int32_t flags){
	auto uint8_t *ptr1,*ptr4;                            /* Auxiliary pointers */
	auto int16_t *ptr2,*ptr3;                            /* Auxiliary pointers */
	auto int32_t ii;                                     /* Auxiliary */

	/* If term is the same variable as var then return successful unification. */
	/* There is no need to add an atomic substitution in this case. */
	if ((*term)&VARIABLE){
		ptr3=(int16_t *)&var[1];
		ptr2=(int16_t *)&term[1];
		if (MATCHFLGS12(flags,flags)&&((*ptr2)==(*ptr3))){
			return(1);
		}
	}

	/* There are previous substitutions. */
	if ((Subst->buffer!=NULL)&&(Subst->substsize>1)){

		/* If the variable has been previously substituted */
		/* then unify as appropriate. */
		ptr3=(int16_t *)&var[1];
		for (ptr1=Subst->buffer;ptr1[0]!=UNITEND;ptr1=NextItem(&ptr1[3],OVERSUBTERMS)){
			ptr2=(int16_t *)&ptr1[1];
			if (MATCHFLGS11(ptr1[0],flags)&&((*ptr2)==(*ptr3))){
				ii=flags;
				if (ptr1[0]&ITEM2SEC){
					ii|=ITEM1SEC;
				} else {
					ii&=~ITEM1SEC;
				}
				if (flags&ITEM2SEC){
					ii|=ITEM2SEC;
				} else {
					ii&=~ITEM2SEC;
				}
				return(Unify(&ptr1[3],term,Subst,ii));
			}
		}

		/* If the term is a variable that has been previously substituted */
		/* then unify as appropriate. */
		if ((*term)&VARIABLE){
			ptr3=(int16_t *)&term[1];
			for (ptr1=Subst->buffer;ptr1[0]!=UNITEND;ptr1=NextItem(&ptr1[3],OVERSUBTERMS)){
				ptr2=(int16_t *)&ptr1[1];
				if (MATCHFLGS12(ptr1[0],flags)&&((*ptr2)==(*ptr3))){
					ii=flags;
					if (flags&ITEM1SEC){
						ii|=ITEM1SEC;
					} else {
						ii&=~ITEM1SEC;
					}
					if (ptr1[0]&ITEM2SEC){
						ii|=ITEM2SEC;
					} else {
						ii&=~ITEM2SEC;
					}
					return(Unify(var,&ptr1[3],Subst,ii));
				}
			}
		}
	}

	/* If var is part of term then return unsuccessful unification. */
	if (CheckVarInTerm(var,term,Subst,flags)){
		return(0);
	}

	/* Compute space required for substitution. */
	ptr1=NextItem(term,OVERSUBTERMS);
	ii=3+(ptr1-term);

	/* Allocate substitution buffer if necessary. */
	if (Subst->buffer==NULL){
		if (NULL==(Subst->buffer=MYALLOC(ii+SUBST_CHUNK_SIZE))){
			return(NOMEMORY);
		}
		Subst->buffsize=ii+SUBST_CHUNK_SIZE;
		Subst->substsize=1;
		Subst->buffer[0]=UNITEND;

	/* Check free space in substitution buffer. If buffer is reallocated */
	/* then recalculate ptr1 and term pointers if necessary. */
	} else if ((Subst->buffsize-Subst->substsize)<ii){
		if (NULL==(ptr4=MYREALLOC(Subst->buffer,Subst->buffsize+ii+SUBST_CHUNK_SIZE))){
			return(NOMEMORY);
		}
		if ((term>Subst->buffer)&&((term-Subst->buffer)<Subst->substsize)){
			term=ptr4+(term-Subst->buffer);
			ptr1=ptr4+(ptr1-Subst->buffer);
		}
		if ((var>Subst->buffer)&&((var-Subst->buffer)<Subst->substsize)){
			var=ptr4+(var-Subst->buffer);
		}
		Subst->buffer=ptr4;
		Subst->buffsize+=(ii+SUBST_CHUNK_SIZE);
	}

	/* Add substitution of var by term. */
	Subst->buffer[Subst->substsize-1]=flags&(ITEM1SEC|ITEM2SEC);
	*((int16_t *)&Subst->buffer[Subst->substsize])=*((int16_t *)&var[1]);
	ii=ptr1-term;
	memcpy(&Subst->buffer[Subst->substsize+2],term,ii);
	Subst->buffer[Subst->substsize+2+ii]=UNITEND;
	Subst->substsize+=(3+ii);

	/* Return successful unification. */
	return(1);
} /* UnifyVar */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform symmetric equality unification)  OCJ
 *
 *    This function checks for unifications of the symmetric equality
 *    of a given equality. Both part1 and part2 literals must have the
 *    same polarity in order for the unification to be possible.
 *
 *
 *  ARGUMENTS:
 *
 *    part1: pointer to an equality in internal binary format. Its symmetric
 *           equality will be checked for unifying with part2 argument.
 *    part2: pointer to a literal, term or list of terms. It must be in the
 *           internal binary format. It will be checked for unifying with
 *           the symmetric equality of part1 argument.
 *    Subst: pointer to a substitution structure.
 *    flags: ITEM1SEC -> The part1 argument belongs to a secondary clause.
 *           It belongs to the main clause by default.
 *           ITEM2SEC -> The part2 argument belongs to a secondary clause.
 *           It belongs to the main clause by default.
 *           INITSUBST -> Initialize substitution to NULL (just for this instance,
 *           but not for subsequent recursive calls to Unify()).
 *           DEBUG -> Call BuildTxtSubstData() function to show results in text form.
 *           The call is performed only if DEBUGCODE is defined and only if terms
 *           match. This flag is not propagated to subsequent nested calls.
 *
 *  RETURNS:
 *
 *    0 -> Unification was not successful.
 *    1 -> Unification was successful.
 *    NOMEMORY -> Not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t SymEquUnify(uint8_t *part1,uint8_t *part2,subst *Subst,int32_t flags){
	auto uint8_t *rtterm1;                               /* First root term of part1 */
	auto uint8_t *rtterm2;                               /* First root term of part2 */

	/* Return failure if part2 is not an equality of the same polarity. */
	if (((EQUALITY|NEGATED)&part1[0])!=((EQUALITY|NEGATED)&part2[0])){
		return(0);
	}

	/* Check unification of first root term of part1 equality */
	/* with second root term of part2 equality. */
	rtterm1=NextItem(part1,IMMED);
	rtterm2=NextItem(part2,IMMED);
	switch (Unify(rtterm1,NextItem(rtterm2,OVERSUBTERMS),Subst,
			flags&(ITEM1SEC|ITEM2SEC|INITSUBST|DEBUG))){

		/* Not enough memory; */
		case NOMEMORY:
			return(NOMEMORY);
			break;

		/* Valid unification. Check unification of second root term */
		/* of part1 equality with first root term of part2 equality. */
		case 1:
			return(Unify(NextItem(rtterm1,OVERSUBTERMS),rtterm2,Subst,
					flags&(ITEM1SEC|ITEM2SEC|DEBUG)));
			break;
	}

	/* Return invalid generalization. */
	return(0);
} /* SymEquUnify */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform literal, term or list generalization)  OCJ
 *
 *    This function is similar to the Unify() function except
 *    that a generalization is performed instead of an unification.
 *    It checks if part1 argument is a generalization of part2 argument.
 *    Evidently it can also be used to check if part2 is an instance
 *    of part1. It is used mainly for finding equivalent factors and for
 *    checking subsumptions.
 *
 *    A term or literal is a generalization of other term if there
 *    exist a substitution that when applied to the first term it
 *    is made identical to the second term.
 *
 *    IMPORTANT: part1 and part2 arguments must belong to different
 *    clauses. Otherwise results are unpredictable.
 *
 *    See Unify() function for additional details.
 *
 *
 *  ARGUMENTS:
 *
 *    part1: pointer to a literal, term or list of terms. It must be
 *           in the internal binary format. It will be checked for
 *           being a generalization of part2 argument.
 *    part2: pointer to a literal, term or list of terms. It must be
 *           in the internal binary format. It will be checked for
 *           being an instance of part1 argument.
 *    Subst: pointer to a substitution structure.
 *    flags: INITSUBST -> Initialize substitution to NULL (just for this instance,
 *           but not for subsequent recursive calls to Generalize()).
 *           NEGATED -> part1 must be a generalization of the negation of part2.
 *           This flag is only valid for literal generalizations, being ignored
 *           for other kinds of generalizations.
 *           DEBUG -> Call BuildTxtSubstData() function to show results in text form.
 *           The call is performed only if DEBUGCODE is defined and only if terms
 *           match. This flag is not propagated to subsequent nested calls.
 *
 *  RETURNS:
 *
 *    0 -> Generalization was not successful.
 *    1 -> Generalization was successful.
 *    NOMEMORY -> Not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t Generalize(uint8_t *part1,uint8_t *part2,subst *Subst,int32_t flags){
	auto uint8_t *ptr1,*ptr2,*ptr3;                      /* Auxiliary pointer */
	auto int32_t ii;                                     /* Auxiliary */
	#ifdef DEBUGCODE
	auto int32_t dd;                                     /* Auxiliary */
	#endif

	/* Initialize substitution if necessary. */
	if (flags&INITSUBST){
		if (Subst->buffer!=NULL){
			Subst->substsize=1;
			Subst->buffer[0]=UNITEND;
		}
		flags&=(~INITSUBST);
	}

	/* Remember DEBUG flag and remove it from flags. */
	#ifdef DEBUGCODE
	dd=flags&DEBUG;
	flags&=~DEBUG;
	#endif

	/* Loop through all subterms of both parts. */
	for (ptr1=part1,ptr3=NextItem(part1,OVERSUBTERMS),ptr2=part2;ptr1<ptr3;){

		/* Subterm of part 1 is a variable. */
		if ((*ptr1)&VARIABLE){

			/* Generalize variable with part 2. This is done even if both */
			/* variables are the same because it is necessary to detect that */
			/* the variable has been assigned to itself in order to prevent */
			/* other assignment and having the variable substituted in the */
			/* instance term. */
			switch (ii=GeneralizeVar(ptr1,ptr2,Subst)){
				case NOMEMORY:
				case 0:
					return(ii);
					break;
			}
			#ifdef DEBUGCODE
			if (dd){
				BuildTxtSubstData(ptr1,ptr2,Subst,flags);
			}
			#endif

			/* Prepare next iteration. */
			ptr1+=3;
			ptr2=NextItem(ptr2,OVERSUBTERMS);

		/* Part 2 is a variable. */
		} else if ((*ptr2)&VARIABLE){
			return(0);

		/* Subterms are non matching literals or functions. */
		} else if (((0==(flags&NEGATED))&&(((*ptr1)&(EQUALITY|PREDICATE|FUNCTION|NEGATED))
					!=((*ptr2)&(EQUALITY|PREDICATE|FUNCTION|NEGATED))))
				||((flags&NEGATED)&&((((*ptr1)&(EQUALITY|PREDICATE|FUNCTION|NEGATED))^NEGATED)
					!=((*ptr2)&(EQUALITY|PREDICATE|FUNCTION|NEGATED))))
				||((((symbol *)&ptr1[1])->symbol)!=(((symbol *)&ptr2[1])->symbol))){
			return(0);

		/* Subterms are matching literals or functions. Prepare */
		/* next iteration and clear NEGATED flag. */
		} else {
			ptr1=NextItem(ptr1,IMMED);
			ptr2=NextItem(ptr2,IMMED);
			flags&=(~NEGATED);
		}
	}

	/* Successful generalization. */
	return(1);
} /* Generalize */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Generalize a variable with a term)  OCJ
 *
 *    This function is similar to the UnifyVar() function except
 *    that a generalization is performed instead of an unification.
 *    It checks if a variable is a valid generalization of a term
 *    with the constraints of an existing substitution.
 *    See Unify() function for a description of the substitution format.
 *
 *    IMPORTANT: The term argument must belong to a different clause
 *    than the argument var. Otherwise results are unpredictable.
 *
 *
 *  ARGUMENTS:
 *
 *    var: pointer to a term that is a single variable. It must be
 *         in compiled binary format.
 *    term: pointer to a term in compiled binary format.
 *    Subst: pointer to a substitution structure.
 *
 *  RETURNS:
 *
 *    0 -> Generalization was not successful.
 *    1 -> Generalization was successful.
 *    NOMEMORY -> Not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t GeneralizeVar(uint8_t *var,uint8_t *term,subst *Subst){
	auto uint8_t *ptr1,*ptr4;                            /* Auxiliary pointers */
	auto int16_t *ptr2,*ptr3;                            /* Auxiliary pointers */
	auto int32_t ii;                                     /* Auxiliary */

	/* There are previous substitutions. */
	if ((Subst->buffer!=NULL)&&(Subst->substsize>1)){

		/* If the variable has been previously substituted then check */
		/* that the argument term is equal to the term in the substitution. */
		ptr3=(int16_t *)&var[1];
		for (ptr1=Subst->buffer;ptr1[0]!=UNITEND;ptr1=NextItem(&ptr1[3],OVERSUBTERMS)){
			ptr2=(int16_t *)&ptr1[1];
			if ((*ptr2)==(*ptr3)){
				return(IsEqual(term,&ptr1[3],0));
			}
		}
	}

	/* Compute space required for substitution. */
	ptr1=NextItem(term,OVERSUBTERMS);
	ii=3+(ptr1-term);

	/* Allocate substitution buffer if necessary. */
	if (Subst->buffer==NULL){
		if (NULL==(ptr4=MYALLOC(ii+SUBST_CHUNK_SIZE))){
			return(NOMEMORY);
		}
		Subst->buffsize=ii+SUBST_CHUNK_SIZE;
		Subst->substsize=1;
		Subst->buffer=ptr4;
		Subst->buffer[0]=UNITEND;

	/* Check free space in substitution buffer. */
	} else if ((Subst->buffsize-Subst->substsize)<ii){
		if (NULL==(ptr4=MYREALLOC(Subst->buffer,Subst->buffsize+ii+SUBST_CHUNK_SIZE))){
			return(NOMEMORY);
		}
		Subst->buffer=ptr4;
		Subst->buffsize+=(ii+SUBST_CHUNK_SIZE);
	}

	/* Add substitution of var by term. */
	Subst->buffer[Subst->substsize-1]=ITEM2SEC;
	*((int16_t *)&Subst->buffer[Subst->substsize])=*((int16_t *)&var[1]);
	ii=ptr1-term;
	memcpy(&Subst->buffer[Subst->substsize+2],term,ii);
	Subst->buffer[Subst->substsize+2+ii]=UNITEND;
	Subst->substsize+=(3+ii);

	/* Return successful generalization. */
	return(1);
} /* GeneralizeVar */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform symmetric equality generalization)  OCJ
 *
 *    This function checks for generalizations of the symmetric equality
 *    of a given equality. Both part1 and part2 literals must have the
 *    same polarity in order for the generalization to be possible.
 *
 *
 *  ARGUMENTS:
 *
 *    part1: pointer to an equality in internal binary format. Its symmetric
 *           equality will be checked for being a generalization of part2 argument.
 *    part2: pointer to a literal, term or list of terms. It must be in the
 *           internal binary format. It will be checked for being an instance of
 *           the symmetric equality of part1 argument.
 *    Subst: pointer to a substitution structure.
 *    flags: INITSUBST -> Initialize substitution to NULL (just for this instance,
 *           but not for subsequent recursive calls to Generalize()).
 *           DEBUG -> Call BuildTxtSubstData() function to show results in text form.
 *           The call is performed only if DEBUGCODE is defined and only if terms
 *           match. This flag is not propagated to subsequent nested calls.
 *
 *  RETURNS:
 *
 *    0 -> Generalization was not successful.
 *    1 -> Generalization was successful.
 *    NOMEMORY -> Not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t SymEquGeneralize(uint8_t *part1,uint8_t *part2,subst *Subst,int32_t flags){
	auto uint8_t *rtterm1;                               /* First root term of part1 */
	auto uint8_t *rtterm2;                               /* First root term of part2 */

	/* Return failure if part2 is not an equality of the same polarity. */
	if (((EQUALITY|NEGATED)&part1[0])!=((EQUALITY|NEGATED)&part2[0])){
		return(0);
	}

	/* Check if first root term of part1 equality is a valid generalization */
	/* of second root term of part2 equality. */
	rtterm1=NextItem(part1,IMMED);
	rtterm2=NextItem(part2,IMMED);
	switch (Generalize(rtterm1,NextItem(rtterm2,OVERSUBTERMS),Subst,flags&(INITSUBST|DEBUG))){

		/* Not enough memory; */
		case NOMEMORY:
			return(NOMEMORY);
			break;

		/* Valid generalization. Check if second root term of part1 equality */
		/* is a valid generalization of first root term of part2 equality. */
		case 1:
			return(Generalize(NextItem(rtterm1,OVERSUBTERMS),rtterm2,Subst,flags&DEBUG));
			break;
	}

	/* Return invalid generalization. */
	return(0);
} /* SymEquGeneralize */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if a variable occurs in a term)  OCJ
 *
 *    This function checks if a variable occurs in a term, either
 *    directly or indirectly through another optional substitution.
 *    If the substitution pointer is not NULL then it is is assumed
 *    that the substitution is not empty.
 *
 *
 *  ARGUMENTS:
 *
 *    var: pointer to a term that is a single variable. It must be
 *         in compiled binary format. The initial byte is ignored
 *         so this can also be the pointer to a substitution and
 *         the function will take the variable being substituted.
 *    term: pointer to a term in compiled binary format.
 *    Subst: Substitution or NULL if substitution must not be checked.
 *    flags: ITEM1SEC -> The var function argument belongs to a secondary
 *           clause. It belongs to the main clause by default.
 *           ITEM2SEC -> The term function argument belongs to a secondary
 *           clause. It belongs to the main clause by default.
 *
 *  RETURNS:
 *
 *    0 -> var is not part of term.
 *    1 -> var is part of term.
 *
 *--------------------------------------------------------------*/
uint32_t CheckVarInTerm(uint8_t *var,uint8_t *term,subst *Subst,int32_t flags){
	auto int16_t *ptr1,*ptr2,*ptr5;                      /* Auxiliary pointers */
	auto uint8_t *ptr3,*ptr4;                            /* Auxiliary pointer */

	/* Check the kind of term. */
	switch ((VARIABLE|FUNCTION)&term[0]){

		/* Term is another variable. */
		case VARIABLE:

			/* Check if the term equals the variable. */
			ptr1=(int16_t *)&var[1];
			ptr2=(int16_t *)&term[1];
			if ((ptr1[0]==ptr2[0])&&MATCHFLGS12(flags,flags)){
				return(1);

			/* Check if the variable is in any term for which the term in being */
			/* substituted. The recursive call to CheckVarInTerm must not be done */
			/* if the variable in substitution is substituted by itself. */
			} else if (NULL!=Subst){
				if (NULL!=Subst->buffer){
					for (ptr3=Subst->buffer;ptr3[0]!=UNITEND;ptr3=NextItem(&ptr3[3],OVERSUBTERMS)){
						ptr1=(int16_t *)&ptr3[1];
						ptr5=(int16_t *)&ptr3[4];
						ptr2=(int16_t *)&term[1];
						if ((ptr1[0]==ptr2[0])&&MATCHFLGS12(ptr3[0],flags)
								&&((0==(VARIABLE&ptr3[3]))||(ptr1[0]!=ptr5[0])||(0==MATCHFLGS12(ptr3[0],ptr3[0])))){
							if (CheckVarInTerm(var,&ptr3[3],Subst,(flags&ITEM1SEC)|(ptr3[0]&ITEM2SEC))){
								return(1);
							}
						}
					}
				}
			}
			break;

		/* If term is a function check that the variable is not in */
		/* any of its arguments. */
		case FUNCTION:
			for (ptr3=NextItem(term,IMMED),ptr4=NextItem(term,OVERSUBTERMS);
					ptr3<ptr4;ptr3=NextItem(ptr3,OVERSUBTERMS)){
				if (CheckVarInTerm(var,ptr3,Subst,flags)){
					return(1);
				}
			}
			break;
	}

	return(0);
} /* CheckVarInTerm */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if a variable occurs in a term)  OCJ
 *
 *    This function is similar to CheckVarInTerm() but for both
 *    variable and term coming from the same clause and with
 *    no substitution to be applied. This is usually the case
 *    when this check is needed in a function other than UnifyVar()
 *    and GeneralizeVar().
 *
 *
 *  ARGUMENTS:
 *
 *    var: pointer to a term that is a single variable. It must be
 *         in compiled binary format. The initial byte is ignored
 *         so this can also be the pointer to a substitution and
 *         the function will take the variable being substituted.
 *    term: pointer to a term in compiled binary format.
 *
 *  RETURNS:
 *
 *    0 -> var is not part of term.
 *    1 -> var is part of term.
 *
 *--------------------------------------------------------------*/
uint32_t CheckVarInTerm2(uint8_t *var,uint8_t *term){
	auto int16_t *ptr1,*ptr2;                            /* Auxiliary pointers */
	auto uint8_t *ptr3,*ptr4;                            /* Auxiliary pointer */
	ptr1=(int16_t *)&var[1];
	for (ptr3=term,ptr4=NextItem(term,OVERSUBTERMS);
			ptr3<ptr4;ptr3=NextItem(ptr3,IMMED)){
		if (VARIABLE&ptr3[0]){
			ptr2=(int16_t *)&ptr3[1];
			if ((*ptr1)==(*ptr2)){
				return(1);
			}
		}
	}
	return(0);
} /* CheckVarInTerm2 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Copy a clause)  OCJ
 *
 *    This function copies a binprefix buffer from a clause to
 *    another. The destination binprefix buffer that may be
 *    reallocated if needed.
 *
 *
 *  ARGUMENTS:
 *
 *    dstclause: Pointer to cmprefix structure of destination clause.
 *    size: Pointer to size of binprefix structure plus aditional space
 *          for formula of destination clause.
 *    orgclause: Pointer to cmprefix structure of clause to be copied.
 *
 *  RETURNS:
 *
 *    0 if OK. NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t CopyClause(cmprefix *dstclause,int32_t *size,cmprefix *orgclause){
	auto binprefix *ptr1;                                /* Auxiliary pointer */
	auto int32_t mm;                                     /* Auxiliary */

	/* Check space available and copy orgclause in */
	/* dstclause buffer. */
	mm=binpsize+orgclause->part2.bin->size;
	if (mm>(*size)){
		if (NULL==(ptr1=MYREALLOC(dstclause->part2.bin,mm+CLAUSE_CHUNK_SIZE))){
			return(NOMEMORY);
		}
		dstclause->part2.bin=ptr1;
		*size=mm+CLAUSE_CHUNK_SIZE;
	}
	memcpy(dstclause->part2.bin,orgclause->part2.bin,mm);
	dstclause->part2.bin->clause=dstclause;
	return(0);
} /* CopyClause */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Deletes a literal in a clause)  OCJ
 *
 *    This function deletes a literal in a compiled binary clause.
 *    It is assumed that the pointer is a valid literal within
 *    the clause.
 *
 *    The binprefix structure buffer of the clause is not reallocated,
 *    so there will be some unused space at the end of it.
 *
 *
 *  ARGUMENTS:
 *
 *    binp: Pointer to binprefix structure of clause.
 *    literal: Pointer to the literal to be deleted.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void DeleteLiteral(binprefix *binp,uint8_t *literal){
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */

	/* Move memory over the deleted literal. */
	ptr1=NextItem(literal,OVERSUBTERMS);
	memmove(literal,ptr1,
			binp->size-(ptr1-&binp->formula[0]));

	/* Update binary clause parameters. */
	UpdateParams(binp);

	return;
} /* DeleteLiteral */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Gets a pointer to literal given its order number)  OCJ
 *
 *    This function gets a pointer to literal given its order number,
 *    that is, 0 for the first literal, 1 for the second and so on.
 *
 *    If the order number is negative the function returns NULL.
 *    If the order number is bigger than the number of literals
 *    minus one the function returns a pointer to the UNITEND
 *    marker byte at the end of the binary formula of the clause.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to cmprefix structure of clause.
 *    literalnb: Order number of the literal within the clause.
 *
 *  RETURNS:
 *
 *    Pointer to literal in the clause or NULL if literal number
 *    is invalid.
 *
 *--------------------------------------------------------------*/
uint8_t *GetLiteralPtr(cmprefix *clause,int32_t literalnb){
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto int32_t ii;                                     /* Auxiliary */
	if (literalnb<0){
		return(NULL);
	}
	for (ptr1=&clause->part2.bin->formula[0],ii=0;
			((ii<literalnb)&&(ptr1[0]!=UNITEND));
			ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){
	}
	return(ptr1);
} /* GetLiteralPtr */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Gets a literal number given a literal pointer)  OCJ
 *
 *    This function gets the literal number corresponding to a literal
 *    pointer.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to binprefix structure of clause.
 *    literal: Pointer to literal in binprefix formula.
 *
 *  RETURNS:
 *
 *    Literal number.
 *
 *--------------------------------------------------------------*/
int32_t GetLiteralNbr(binprefix *binp,uint8_t *literal){
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto int32_t ii;                                     /* Auxiliary */
	for (ptr1=&binp->formula[0],ii=0;ptr1!=literal;
			ptr1=NextItem(ptr1,OVERSUBTERMS),ii++){
	}
	return(ii);
} /* GetLiteralNbr */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Apply substitution to a clause)  OCJ
 *
 *    This function applies a substitution to a clause in compiled
 *    binary form. This function also calls UpdateParams() so the
 *    result has parameters like size, etc. updated.
 *
 *    IMPORTANT: The items in clause coming from items that were
 *    secondary in the call to Unify() function must have the
 *    variables properly shifted (see description of varshift
 *    parameter).
 *
 *    WARNING: The substitution terms may be changed in the substitution
 *    itself if the terms come from a secondary clause and contain
 *    variables, as the variable numbers must be shifted to match the
 *    existing variable number shift in the clause.
 *
 *    WARNING: It is the caller responsibility to reallocate the resulting
 *    binprefix buffer to the exact size needed.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to cmprefix structure of clause. The binprefix
 *            structure buffer of clause is reallocated.
 *    psize: pointer to length of binprefix structure buffer assigned to
 *           clause including the formula space.
 *    Subst: Pointer to substitution.
 *    varshift: Shift for variable numbers belonging to a secondary
 *              merged clause. Variable numbers in substitution
 *              coming from secondary clause increased by varshift
 *              match variable numbers in clause.
 *
 *  RETURNS:
 *
 *    0 -> Substitution was successful applied.
 *    NOMEMORY -> Not enough memory. In this case the content of
 *    clause is unpredictable.
 *
 *
 *--------------------------------------------------------------*/
int32_t ApplySubst2Clause(cmprefix *clause,int32_t *psize,subst *Subst,int32_t varshift){
	auto binprefix *dstclause;                           /* Pointer to destination binprefix structure buffer */
	auto int32_t dstsize;                                /* Size of binprefix structure plus formula buffer for new clause. */
	auto int32_t maxvarnb;                               /* Máximum number assigned to variable in substitution */
	auto uint8_t *ptr1,*ptr2;                            /* Auxiliary pointers */
	auto binprefix *ptr4;                                /* Auxiliary pointer */
	auto uint8_t **substterms;                           /* Address for set of pointers of substituting terms */
	auto int32_t ii,mm;                                  /* Auxiliary */

	/* Update clause parameters. */
	UpdateParams(clause->part2.bin);

	/* Return if substitution is empty. */
	if ((Subst->buffer==NULL)||(Subst->substsize==1)){
		return(0);
	}

	/* Allocate memory for destination binprefix structure. */
	dstsize=2*(binpsize+clause->part2.bin->size);
	if (NULL==(dstclause=MYALLOC(dstsize))){
		return(NOMEMORY);
	}

	/* Get the maximum variable number in substitution. */
	for (ptr1=Subst->buffer,maxvarnb=0;ptr1[0]!=UNITEND;ptr1=NextItem(&ptr1[3],OVERSUBTERMS)){
		ii=(*((int16_t *)&ptr1[1]));
		if (ptr1[0]&ITEM1SEC){
			if (maxvarnb<(ii+varshift)){
				maxvarnb=ii+varshift;
			}
		} else {
			if (maxvarnb<ii){
				maxvarnb=ii;
			}
		}
	}

	/* Allocate memory for substitution term auxiliary data and initialize it. */
	if (NULL==(substterms=MYALLOC(sizeof(void *)*(maxvarnb+1)))){
		MYFREE(dstclause);
		return(NOMEMORY);
	}
	for (ii=0;ii<=maxvarnb;ii++){
		substterms[ii]=NULL;
	}

	/* Initialize set of pointers of substituting terms and */
	/* adjust substitution (see GetSubstTermData() for details). */
	GetSubstTermData(Subst,substterms,varshift);

	/* Loop until no substitutions are applied. This is necessary for */
	/* cases as the following: Unification of predicates P(F(x0,B),x0) */
	/* and P(x3,x4) produces the substitution {x3/F(x0,B)} and {x0/x4}. */
	/* Applying the substitution to the clause Q(x0)|P(x1,x2)|~x1=x3|~x2=x4 */
	/* produces the clause Q(x4)|P(x1,x2)|~x1=F(x0,B)|~x2=x4 which still */
	/* contains the variable x0 that must be substituted, so another pass */
	/* is necessary. */
	do {

		/* Copy clause binprefix structure and loop through literals of clause. */
		/* Variable mm is zero if no changes are made. */
		memcpy(dstclause,clause->part2.bin,binpsize);
		for (ptr1=&clause->part2.bin->formula[0],ptr2=&dstclause->formula[0],mm=0;
				ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,OVERSUBTERMS)){

			/* Perform substitution of the literal. */
			switch (ApplySubst2Item(&ptr2,&dstclause,&dstsize,ptr1,maxvarnb,substterms)){

				/* Not enough memory. */
				case NOMEMORY:
					MYFREE(dstclause);
					MYFREE(substterms);
					return(NOMEMORY);
					break;

				/* Remember if there was a modification. */
				case 1:
					mm=1;
					break;
			}
		}

		/* There was a modification. */
		if (mm){

			/* Copy UNITEND byte to destination clause. */
			if (dstsize==(ptr2-((uint8_t *)dstclause))){
				ii=dstsize+CLAUSE_CHUNK_SIZE;
				if (NULL==(ptr4=MYREALLOC(dstclause,ii))){
					MYFREE(dstclause);
					MYFREE(substterms);
					return(NOMEMORY);
				}
				ptr2=((uint8_t *)ptr4)+dstsize;
				dstclause=ptr4;
				dstsize=ii;
			}
			*ptr2=UNITEND;

			/* Update binary clause parameters. */
			UpdateParams(dstclause);

			/* Swap buffers. */
			ptr4=dstclause;
			dstclause=clause->part2.bin;
			clause->part2.bin=ptr4;
			clause->part2.bin->oriented=0;
			ii=dstsize;
			dstsize=*psize;
			*psize=ii;
		}
	} while (mm);

	/* Free memory. */
	MYFREE(dstclause);
	MYFREE(substterms);
	return(0);
} /* ApplySubst2Clause */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Initialize set of pointers to terms in a substitution)  OCJ
 *
 *    This function initializes a a set of pointers to terms from the
 *    data in a substitution. Each pointer corresponds to the variable
 *    whose number is the index to the set.
 *
 *    It also shifts the variable number of substituted variables
 *    and variables in substituting terms in the substitution itself
 *    when they are coming from a secondary clause, so the content of
 *    the substitution is modified by this function.
 *    This is necessary because the variable numbers must be shifted
 *    to match the existing variable number shift in the clause.
 *
 *    Finally the ITEM1SEC and ITEM2SEC flags are cleared in each
 *    substitution flags byte. This way the same substitution can be
 *    used again by ApplySubst2Clause() function.
 *
 *
 *  ARGUMENTS:
 *
 *    Subst: Pointer to substitution.
 *    substterms: Pointer to set of pointers to substitution terms
 *                in the substitution.
 *    varshift: Shift for variable numbers belonging to a secondary
 *              clause. Variable numbers in substitution coming from
 *              secondary clause increased by varshift match variable
 *              numbers in the secondary clause.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *--------------------------------------------------------------*/
void GetSubstTermData(subst *Subst,uint8_t **substterms,int32_t varshift){
	auto uint8_t *ptr1,*ptr2,*ptr3;                      /* Auxiliary pointers */
	auto int32_t jj;                                     /* Auxiliary */

	/* Loop through substitutions. */
	for (ptr1=Subst->buffer;ptr1[0]!=UNITEND;ptr1=NextItem(&ptr1[3],OVERSUBTERMS)){

		/* Shift substituted variable number if appropriate. */
		if (ptr1[0]&ITEM1SEC){
			*((int16_t *)&ptr1[1])+=varshift;
		}
		jj=(*((int16_t *)&ptr1[1]));

		/* Shift the variable numbers that occur in substitution term */
		/* coming from a secondary clause. */
		if (ptr1[0]&ITEM2SEC){
			for (ptr2=&ptr1[3],ptr3=NextItem(&ptr1[3],OVERSUBTERMS);
					ptr2<ptr3;ptr2=NextItem(ptr2,IMMED)){
				if (VARIABLE&ptr2[0]){
					*((int16_t *)&ptr2[1])+=varshift;
				}
			}
		}

		/* Get the address of substitution terms for the substituting variable. */
		/* This is done only if the substitution term is not the same variable */
		/* as the substituted variable, because substitutions generated by the */
		/* Generalize() function may create this kind of substitutions (see */
		/* Generalize() function for details), but they make ApplySubst2Clause() */
		/* function to enter in a loop. */
		if ((ptr1[0]&ITEM2SEC)||(0==(VARIABLE&ptr1[3]))||(jj!=*((int16_t *)&ptr1[4]))){
			substterms[jj]=&ptr1[3];
		}

		/* Clear flags marking secondary clauses. */
		ptr1[0]&=(~(ITEM1SEC|ITEM2SEC));
	}

	return;
} /* GetSubstTermData */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Apply substitution to an item)  OCJ
 *
 *    This function applies a substitution to an item (literal or term)
 *    and all item sub-terms. The item must be in compiled binary form.
 *
 *    This function is called recursively.
 *
 *
 *  ARGUMENTS:
 *
 *    pdstitem: As input parameter this is the address of a pointer to the place
 *              where the result item will be placed. As output this pointer is
 *              set to the first byte not belonging to the substituted term.
 *    pdstclause: Address of a pointer to binprefix structure or buffer of
 *                destination clause. It can be reallocated. In any case it must
 *                be casted to binprefix **.
 *    dstsize: Pointer to length of pdstclause buffer in bytes.
 *    orgitem: Pointer to item to which substitution will be applied.
 *    maxvarnb: Máximum number assigned to variable in substitution.
 *    substterms: Pointer to set of pointers to substituting terms. The set
 *                of pointers is indexed by the substituting variable numbers.
 *
 *  RETURNS:
 *
 *    0 -> Substitution was successfully applied, the original item was not
 *         changed.
 *    1 -> Substitution was successfully applied, the original item was changed.
 *    NOMEMORY -> Not enough memory.
 *
 *
 *--------------------------------------------------------------*/
int32_t ApplySubst2Item(uint8_t **pdstitem,binprefix **pdstclause,int32_t *dstsize,
		uint8_t *orgitem,int32_t maxvarnb,uint8_t **substterms){
	auto uint8_t *ptr1,*ptr2;                            /* Auxiliary pointers */
	auto int32_t ii,jj,kk,vv;                            /* Auxiliary */

	/* Item is a variable. */
	if ((*orgitem)&VARIABLE){

		/* Variable must be substituted. */
		vv=*(int16_t *)&orgitem[1];
		if ((vv<=maxvarnb)&&(NULL!=substterms[vv])){

			/* Check space available. */
			ii=NextItem(substterms[vv],OVERSUBTERMS)-substterms[vv];
			jj=((*pdstitem)-(uint8_t *)(*pdstclause))+ii;
			if (jj>(*dstsize)){
				kk=jj+CLAUSE_CHUNK_SIZE;
				if (NULL==(ptr1=MYREALLOC(*pdstclause,kk))){
					return(NOMEMORY);
				}
				*pdstitem=ptr1+((*pdstitem)-((uint8_t *)(*pdstclause)));
				*pdstclause=(binprefix *)ptr1;
				*dstsize=kk;
			}

			/* Copy the item. */
			memcpy(*pdstitem,substterms[vv],ii);

			/* Update *pdstitem and return  indicating that the original item has changed. */
			*pdstitem+=ii;
			return(1);

		/* Variable must not be substituted. Check space available and copy it as is. */
		} else {
			jj=((*pdstitem)-(uint8_t *)(*pdstclause))+3;
			if (jj>(*dstsize)){
				kk=jj+CLAUSE_CHUNK_SIZE;
				if (NULL==(ptr1=MYREALLOC(*pdstclause,kk))){
					return(NOMEMORY);
				}
				*pdstitem=ptr1+((*pdstitem)-((uint8_t *)(*pdstclause)));
				*pdstclause=(binprefix *)ptr1;
				*dstsize=kk;
			}
			memcpy(*pdstitem,orgitem,3);
			*pdstitem+=3;
		}

	/* Item is a literal or function. */
	} else {

		/* Check space available and copy the item without subterms. */
		if ((*orgitem)&EQUALITY){
			ii=1+sizeof(symbol);
		} else {
			ii=1+sizeof(symbol);
		}
		jj=((*pdstitem)-(uint8_t *)(*pdstclause))+ii;
		if (jj>(*dstsize)){
			kk=jj+CLAUSE_CHUNK_SIZE;
			if (NULL==(ptr1=MYREALLOC(*pdstclause,kk))){
				return(NOMEMORY);
			}
			*pdstitem=ptr1+((*pdstitem)-((uint8_t *)(*pdstclause)));
			*pdstclause=(binprefix *)ptr1;
			*dstsize=kk;
		}
		memcpy(*pdstitem,orgitem,ii);

		/* Update *pdstitem, loop through all subterms in the item */
		/* and return. */
		*pdstitem+=ii;
		for (ptr1=NextItem(orgitem,IMMED),ptr2=NextItem(orgitem,OVERSUBTERMS),ii=0;
				ptr1<ptr2;ptr1=NextItem(ptr1,OVERSUBTERMS)){
			switch (jj=ApplySubst2Item(pdstitem,pdstclause,dstsize,ptr1,
					maxvarnb,substterms)){
				case NOMEMORY:
					return(NOMEMORY);
					break;
				default:
					ii|=jj;
					break;
			}
		}
		return(ii);
	}

	return(0);
} /* ApplySubst2Item */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Update binary clause parameters)  OCJ
 *
 *    This function updates the following parameters in clause in binary
 *    format:
 *    - offset and nextoff fields of all symbol structures.
 *    - literals and size fields of binprefix structure.
 *    - HORN clause flag.
 *
 *    Typical calls to this function are after deleting a literal or
 *    applying a substitution.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    binp: Pointer to binprefix structure.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *--------------------------------------------------------------*/
void UpdateParams(binprefix *binp){
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto int32_t ii,jj;                                  /* Auxiliary */

	/* Loop through literals. */
	for (ptr1=&binp->formula[0],ii=jj=0;ptr1[0]!=UNITEND;ii++){
		if (0==(NEGATED&ptr1[0])){
			jj++;
		}
		ptr1=UpdateSymbolParms(binp,ptr1,NEGATED&ptr1[0]);
	}

	/* Update size, literals and HORN flag. */
	binp->literals=ii;
	binp->size=(ptr1-&binp->formula[0])+1;
	if (jj<=1){
		binp->clause->flags|=HORN;
	}

	return;
} /* UpdateParams */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Update binary formula parameters)  OCJ
 *
 *    This function updates the following parameters in a formula in
 *    binary format:
 *    - offset and nextoff fields of all symbol structures, with
 *      respect to the start of formula.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to formula in binary format.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *--------------------------------------------------------------*/
void UpdateParams2(uint8_t *formula){
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	for (ptr1=formula;ptr1[0]!=UNITEND;ptr1=UpdateSymbolParms2(formula,ptr1)){
	}

	return;
} /* UpdateParams2 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Update binprefix and symbol parameters in an item)  OCJ
 *
 *    This function updates the following parameters in a symbol that
 *    is part of a clause in binary format:
 *    - offset and nextoff fields of symbol structure.
 *
 *    Typical calls to this function are after deleting a literal or
 *    applying a substitution.
 *
 *    VERY IMPORTANT: This function must not be called for clause
 *    with a CLAUSIFY inference.
 *
 *    This function is called recursively.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    binp: Pointer to binprefix structure.
 *    item: Pointer to item containing the symbol in clause formula.
 *    flag: NEGATED if the item belongs to a negative literal.
 *          EQUALITY for positive oriented equalities and their subterms.
 *
 *  RETURNS:
 *
 *    Pointer to next byte after item and all its subterms.
 *
 *
 *--------------------------------------------------------------*/
uint8_t *UpdateSymbolParms(binprefix *binp,uint8_t *item,int32_t flag){
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto int32_t ii,jj;                                  /* Auxiliary */

	/* Item is a variable. */
	if (item[0]&VARIABLE){
		ptr1=&item[3];

	/* Item is not a variable. */
	} else {

		/* Compute symbol arity and offset fields. */
		((symbol *)&item[1])->offset=item-&binp->formula[0];
		if (item[0]&EQUALITY){
			ii=2;
		} else {
			ii=((symbol *)&item[1])->symbol->arity;
		}

		/* Loop through item subterms. */
		for (ptr1=NextItem(item,IMMED),jj=0;jj<ii;jj++){
			ptr1=UpdateSymbolParms(binp,ptr1,flag);
		}

		/* Update symbol nextoff field. */
		((symbol *)&item[1])->nextoff=ptr1-&binp->formula[0];
	}

	return(ptr1);
} /* UpdateSymbolParms */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Update symbol parameters in an item)  OCJ
 *
 *    This function updates offset and nextoff fields of symbol
 *    structure.
 *
 *    This function is similar to UpdateSymbolParms() but it is
 *    used when only the clause formula and not the whole binprefix
 *    structure is available.
 *
 *    This function is called recursively.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to formula in binary format.
 *    item: Pointer to item containing the symbol in clause formula.
 *
 *  RETURNS:
 *
 *    Pointer to next byte after item and all its subterms.
 *
 *
 *--------------------------------------------------------------*/
uint8_t *UpdateSymbolParms2(uint8_t *formula,uint8_t *item){
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto int32_t ii,jj;                                  /* Auxiliary */

	/* Item is a variable. */
	if (item[0]&VARIABLE){
		ptr1=&item[3];

	/* Item is not a variable. */
	} else {

		/* Compute symbol arity and offset fields. */
		((symbol *)&item[1])->offset=item-formula;
		if (item[0]&EQUALITY){
			ii=2;
		} else {
			ii=((symbol *)&item[1])->symbol->arity;
		}

		/* Loop through item subterms. */
		for (ptr1=NextItem(item,IMMED),jj=0;jj<ii;jj++){
			ptr1=UpdateSymbolParms2(formula,ptr1);
		}

		/* Update symbol nextoff field. */
		((symbol *)&item[1])->nextoff=ptr1-formula;
	}

	return(ptr1);
} /* UpdateSymbolParms2 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform saturation inferences)  OCJ
 *
 *    This function performs inferences for a saturation algorithm.
 *    It calls the functions for each deductive inference. Inferred
 *    clauses are added to working KB as unprocessed.
 *
 *    It is assumed that input clause was formerly PASSIVE and it
 *    is unlinked from the KB.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause with which inferences will be made.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *
 *--------------------------------------------------------------*/
void Infere(cmprefix *clause){

	/* Perform resolution inferences except if the problem has only unit */
	/* clauses because in that case the resolution inference is equivalent */
	/* to subsumption resolution simplification. */
	if ((pbmtype!=3)&&(pbmtype!=8)){
		Resolution(clause);
		if (kbset.status!=RUNNING){
			return;
		}
	}

	/* There are equalities. */
	if ((UNITEQUALITY|NONUNITEQUALITY)&pbmflags){

		/* Perform paramodulation to and from clause. */
		PM2Clause(clause);
		if (kbset.status!=RUNNING){
			return;
		}
		PMFromClause(clause);
		if (kbset.status!=RUNNING){
			return;
		}

		/* Perform equality resolution inferences. */
		EqualityResolution(clause);
		if (kbset.status!=RUNNING){
			return;
		}

		/* Perform equality factoring inferences. This is not done if the clause has */
		/* only one literal because this inference cannot be done in this case. */
		if (1<clause->part2.bin->literals){
			EqualityFactoring(clause);
			if (kbset.status!=RUNNING){
				return;
			}
		}
	}

	/* Perform factoring inferences if this option is enabled */
	/* and the clause has more than one literal. */
	if ((kbset.opts.factoring)&&(1<clause->part2.bin->literals)){
		Factor(clause);
		if (kbset.status!=RUNNING){
			return;
		}
	}

	return;
} /* Infere */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform saturation inferences for unit equality)  OCJ
 *
 *    This function performs inferences for a saturation algorithm.
 *    It calls the functions for each deductive inference. Inferred
 *    clauses are added to working KB as unprocessed.
 *
 *    It is assumed that input clause was formerly PASSIVE and it
 *    is unlinked from the KB.
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause with which inferences will be made.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *
 *--------------------------------------------------------------*/
void InfereUEQ(cmprefix *clause){

	/* There are equalities. */
	if ((UNITEQUALITY|NONUNITEQUALITY)&pbmflags){

		/* Perform paramodulation to and from clause. */
		PM2ClauseUEQ(clause);
		if (kbset.status!=RUNNING){
			return;
		}
		PMFromClauseUEQ(clause);
		if (kbset.status!=RUNNING){
			return;
		}

		/* Perform equality resolution inferences. */
		EqualityResolutionUEQ(clause);
		if (kbset.status!=RUNNING){
			return;
		}
	}

	return;
} /* InfereUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform resolution inference)  OCJ
 *
 *    This function performs all the resolution inferences for
 *    the clause passed as argument and all the appropriate
 *    active clauses in the working KB.
 *
 *    Inferred clauses are added to KB as unprocessed.
 *
 *    On return the status field of the KB structure is set with
 *    the values NOMEMORY, UNSATISFIABLE, UNKNOWN or TIMEOUT when
 *    appropriate.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause with which inferences will be made.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *
 *--------------------------------------------------------------*/
void Resolution(cmprefix *clause){
	auto subst Subst;                                    /* Substitution */
	auto uint8_t *equality;                              /* Pointer to equality with root terms in reverse order */
	auto uint8_t *eqterms;                               /* Pointer to equality root terms in reverse order */
	auto uint8_t *queryitm;                              /* Pointer to QueryDscTree() query item */
	auto int32_t size;                                   /* Size of eqterms buffer */
	auto cmprefix *clause2;                              /* Pointer to clause with which inference will be made */
	auto cmprefix *inferred;                             /* Pointer to inferred clause */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5;          /* Auxiliary pointers */
	auto int32_t ii,jj,kk,mm,nn,rr,ss;                   /* Auxiliary */

	/* Initialize substitution, stack and equality root terms buffer. */
	Subst.buffer=NULL;
	eqterms=equality=NULL;
	size=0;

	/* Loop through clause literals. */
	for (ptr1=&clause->part2.bin->formula[0];ptr1[0]!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS)){

		/* Literal is selected. */
		if (ptr1[0]&SELECTED){

			/* Get pointer to appropriate discrimination tree. */
			if (ptr1[0]&NEGATED){
				ptr2=(((symbol *)&ptr1[1])->symbol)->discr[0];
			} else {
				ptr2=(((symbol *)&ptr1[1])->symbol)->discr[1];
			}

			/* Loop for equality symmetry management. */
			ptr4=NextItem(ptr1,IMMED);
			mm=(ptr1[0]&EQUALITY?2:1);
			for (kk=0;kk<mm;kk++){

				/* This is the first iteration. */
				if (kk==0){
					queryitm=ptr1;

				/* This is the second iteration for an equality literal. */
				/* Build the set of root terms in reverse order. */
				} else {
					ptr3=NextItem(ptr4,OVERSUBTERMS);
					rr=ptr3-ptr4;
					ss=NextItem(ptr3,OVERSUBTERMS)-ptr3;
					if (size==0){
						if (NULL==(equality=MYALLOC(size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
							MYFREE(Subst.buffer);
							MYFREE(equality);
							kbset.status=NOMEMORY;
							return;
						}
						eqterms=&equality[1+sizeof(symbol)];
					} else if (size<(2+sizeof(symbol)+rr+ss)){
						if (NULL==(ptr5=MYREALLOC(equality,size=2+sizeof(symbol)+rr+ss+CLAUSE_CHUNK_SIZE))){
							MYFREE(Subst.buffer);
							MYFREE(equality);
							kbset.status=NOMEMORY;
							return;
						}
						equality=ptr5;
						eqterms=&equality[1+sizeof(symbol)];
					}
					memcpy(equality,ptr1,1+sizeof(symbol));
					memcpy(eqterms,ptr3,ss);
					memcpy(&eqterms[ss],ptr4,rr);
					eqterms[rr+ss]=UNITEND;
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
						kbset.status=TIMEOUT;
						return;
					}
					if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
						MYFREE(Subst.buffer);
						MYFREE(equality);
						kbset.status=UNKNOWN;
						return;
					}
					if (procctl->status==NOMEMORY){
						MYFREE(Subst.buffer);
						MYFREE(equality);
						return;
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
								kbset.status=NOMEMORY;
								return;
								break;

							/* Candidate unifies with literal. */
							case 1:

								/* Remember substitution status. */
								if (Subst.buffer!=NULL){
									ss=Subst.substsize;
								} else {
									ss=-1;
								}

								/* Do until a resolution without factoring is performed. It is necessary */
								/* to perform both types of resolution to ensure completeness, at least */
								/* with Avatar architecture. Vampire also does both types of resolution. */
								clause2=((binprefix *)(ptr3-(binpsize+((symbol *)&ptr3[1])->offset)))->clause;
								ii=1;
								do {

									/* Build inferred clause. */
									if (NOMEMORY==(rr=BuildResolvent(clause,clause2,ptr1,ptr3,&inferred,&Subst,ii))){
										MYFREE(Subst.buffer);
										MYFREE(equality);
										kbset.status=NOMEMORY;
										return;
									}
									kbset.prstats.resolutions++;
									ii=0;

									/* Check empty clause without assertions. */
									if ((inferred->part2.bin->formula[0]==UNITEND)&&(inferred->part2.bin->ovly.asserts==NULL)){
										inferred->flags=UNPROC;
										if (NOMEMORY==AddBinClause2KB(inferred,&kbset,0)){
											MYFREE(inferred->part2.bin);
											MYFREE(inferred);
											MYFREE(Subst.buffer);
											MYFREE(equality);
											kbset.status=NOMEMORY;
											return;
										}
										MYFREE(Subst.buffer);
										MYFREE(equality);
										if (NULL==(kbset.frstprfnode=MYALLOC(sizeof(proofnode)))){
											kbset.status=NOMEMORY;
											return;
										}
										kbset.frstprfnode->type=ACLAUSE;
										kbset.frstprfnode->ptr.Aclause=inferred;
										kbset.status=UNSATISFIABLE;
										return;
									}

									/* Add inferred clause to KB as unprocessed. */
									inferred->flags=UNPROC;
									if (NOMEMORY==AddBinClause2KB(inferred,&kbset,0)){
										MYFREE(Subst.buffer);
										MYFREE(equality);
										kbset.status=NOMEMORY;
										return;
									}

									/* If there is another iteration then reset substitution */
									/* to its previous status. */
									if (rr){
										if (ss>0){
											Subst.substsize=ss;
											Subst.buffer[ss-1]=UNITEND;
										} else if (Subst.buffer!=NULL){
											Subst.substsize=1;
											Subst.buffer[0]=UNITEND;
										} else {
											Subst.substsize=0;
										}
									}
								} while (rr);
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
	return;
} /* Resolution */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Build clause inferred by resolution)  OCJ
 *
 *    This function builds a clause inferred by resolution given two
 *    parent clauses in binary format, the literals that unify each
 *    with the negation of the other and a substitution. It is assumed
 *    that the assertions from inferring clauses are compatible.
 *
 *    The pointer to the result is placed in the address passed as
 *    argument.
 *
 *    If the factoring option is disabled then in order to preserve
 *    completeness this resolution process includes factoring of
 *    selected literals that unify with the resolution literal with
 *    a substitution compatible with the main resolution substitution.
 *
 *    IMPORTANT: Neither the signature nor the marking of unifiable
 *    literals can be used to accelerate the factoring process, as
 *    explained below.
 *
 *    The signature cannot be used because there may be literals
 *    that cannot be factored but they can be resolved with the
 *    same literal of the other parent clause in another resolution
 *    inference. For instance, for the main parent clause ~A(x0,x1,x2)
 *    and the secondary parent clause A(C,B,x0)|A(B,x0,C) both literals
 *    of the secondary parent clause can be resolved by the literal
 *    of the primary parent clause, but neither of them can be factored
 *    in the resolution of the other.
 *
 *    Marking factored literals for not checking their resolution
 *    is not possible either. Consider for instance the primary
 *    parent clause A(C,x1,x2)|A(x0,x1,x2) and the second parent
 *    clause ~A(C,B,x0)|~A(B,x0,C). When resolving A(C,x1,x2) with
 *    ~A(C,B,x0) the other literal A(x0,x1,x2) is a factor. However
 *    the literal A(x0,x1,x2) cannot be marked for not being used
 *    in additional resolutions with the secondary parent because
 *    it can be resolved with ~A(B,x0,C) while A(C,x1,x2) doesn't
 *    resolve with it.
 *
 *
 *  ARGUMENTS:
 *
 *    masterprnt: Pointer to first parent clause in binary format.
 *    secparent: Pointer to second parent clause in compiled format.
 *    literal1: Pointer to unifying literal in first parent.
 *    literal2: Pointer to unifying literal in second parent.
 *    inferred: Address of pointer to inferred clause.
 *    Subst: Pointer to the initial substitution of the resolution inference.
 *           It will be upgraded to a compatible factoring substitution.
 *    flag: If 0 then don't perform factoring with resolution. Otherwise
 *          perform factoring with resolution.
 *
 *  RETURNS:
 *
 *    0 if no factors were taken in the resolution inference. This may
 *    happen even if flag is not zero.
 *    1 if factors were taken in the resolution inference. This may only
 *    happen if flag is not zero.
 *    NOMEMORY if not enough memory.
 *
 *--------------------------------------------------------------*/
int32_t BuildResolvent(cmprefix *masterprnt,cmprefix *secparent,uint8_t *literal1,
		uint8_t *literal2,cmprefix **inferred,subst *Subst,int32_t flag){
	auto binprefix *binp;                                /* Auxiliary binary prefix pointer */
	auto binprefix *binpmaster,*binpsec;                 /* Factored master and secondary binprefix structures */
	auto uint8_t *flitmaster,*flitsec;                   /* Pointers to literal1 and literal2 in factored clauses */
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto int32_t ii,jj,kk,nn,rr;                         /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	#endif

	/* Allocate storage for inferred clause. */
	if (NULL==(*inferred=MYALLOC(sizeof(cmprefix)))){
		return(NOMEMORY);
	}
	nn=binpsize+2*(masterprnt->part2.bin->size+secparent->part2.bin->size);
	if (NULL==(((*inferred)->part2.bin)=MYALLOC(nn))){
		MYFREE(*inferred);
		return(NOMEMORY);
	}
	(*inferred)->part2.bin->clause=*inferred;
	(*inferred)->part2.bin->oriented=0;

	/* Factoring must be done. */
	if (flag){

		/* Factor literals with literal1. */
		if (0==(NOFACTORS&masterprnt->flags)){
			if (NOMEMORY==(rr=ResolFactor(masterprnt->part2.bin,literal1,&binpmaster,&flitmaster,Subst,1))){
				MYFREE((*inferred)->part2.bin);
				MYFREE(*inferred);
				return(NOMEMORY);
			}
		} else {
			binpmaster=masterprnt->part2.bin;
			flitmaster=literal1;
			rr=0;
		}

		/* Factor literals with literal2. */
		if (0==(NOFACTORS&secparent->flags)){
			if (NOMEMORY==(ii=ResolFactor(secparent->part2.bin,literal2,&binpsec,&flitsec,Subst,0))){
				MYFREE((*inferred)->part2.bin);
				MYFREE(*inferred);
				if (binpmaster!=masterprnt->part2.bin){
					MYFREE(binpmaster);
				}
				return(NOMEMORY);
			}
			rr|=ii;
		} else {
			binpsec=secparent->part2.bin;
			flitsec=literal2;
		}
	} else {
		binpmaster=masterprnt->part2.bin;
		flitmaster=literal1;
		binpsec=secparent->part2.bin;
		flitsec=literal2;
		rr=0;
	}

	/* Copy formula of first factored parent clause up to and not including literal1. */
	ii=flitmaster-&binpmaster->formula[0];
	memcpy(&(*inferred)->part2.bin->formula[0],&binpmaster->formula[0],ii);

	/* Add literals of first parent clause following literal1. */
	ptr1=NextItem(flitmaster,OVERSUBTERMS);
	jj=binpmaster->size-(ptr1-&binpmaster->formula[0])-1;
	memcpy(&(*inferred)->part2.bin->formula[ii],ptr1,jj);
	ii+=jj;

	/* Add formula of second parent clause up to and not including literal2. */
	jj=flitsec-&binpsec->formula[0];
	memcpy(&(*inferred)->part2.bin->formula[ii],&binpsec->formula[0],jj);
	kk=ii;
	ii+=jj;

	/* Add literals of second parent clause following literal2 */
	/* plus ending UNITEND marker byte. */
	ptr1=NextItem(flitsec,OVERSUBTERMS);
	jj=binpsec->size-(ptr1-&binpsec->formula[0]);
	memcpy(&(*inferred)->part2.bin->formula[ii],ptr1,jj);

	/* Shift the numbers of variables coming from second clause. */
	ii=masterprnt->part2.bin->maxvarnb+1;
	for (ptr1=&(*inferred)->part2.bin->formula[kk];ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){
		if (VARIABLE&ptr1[0]){
			*((uint16_t *)&ptr1[1])+=ii;
		}
	}

	/* Apply substitution to inferred clause. Binary clause */
	/* size field is set by this function, so there is no need */
	/* to call UpdateSize() function here. */
	if (NOMEMORY==ApplySubst2Clause(*inferred,&nn,Subst,ii)){
		MYFREE((*inferred)->part2.bin);
		MYFREE(*inferred);
		if (binpmaster!=masterprnt->part2.bin){
			MYFREE(binpmaster);
		}
		if (binpsec!=secparent->part2.bin){
			MYFREE(binpsec);
		}
		return(NOMEMORY);
	}

	/* Reallocate the formula buffer to the exact length needed. */
	if (NULL==(binp=MYREALLOC((*inferred)->part2.bin,binpsize+(*inferred)->part2.bin->size))){
		MYFREE((*inferred)->part2.bin);
		MYFREE(*inferred);
		if (binpmaster!=masterprnt->part2.bin){
			MYFREE(binpmaster);
		}
		if (binpsec!=secparent->part2.bin){
			MYFREE(binpsec);
		}
		return(NOMEMORY);
	}
	(*inferred)->part2.bin=binp;

	/* Add assertions to inferred clause. */
	if (NOMEMORY==AddAssertions(masterprnt->part2.bin,secparent->part2.bin,(*inferred)->part2.bin)){
		MYFREE((*inferred)->part2.bin);
		MYFREE(*inferred);
		if (binpmaster!=masterprnt->part2.bin){
			MYFREE(binpmaster);
		}
		if (binpsec!=secparent->part2.bin){
			MYFREE(binpsec);
		}
		return(NOMEMORY);
	}

	/* Debug. */
	#ifdef DEBUGCODE
	ptr10=Decompile(secparent->part2.bin);
	#ifdef VERBOSE
	if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
		printf("Resolution parent with:\n%s\n",ptr10);
	}
	#endif
	MYFREE(ptr10);
	ptr10=Decompile((*inferred)->part2.bin);
	#ifdef VERBOSE
	if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
		printf("Result [%ld]:\n%s\n",kbset.nformulas,ptr10);
	}
	#endif
	MYFREE(ptr10);
	#endif

	/* Set clause cmprefix data and free memory. */
	(*inferred)->parent1=masterprnt;
	(*inferred)->parent2=secparent;
	if (secparent->part2.bin->agedist<masterprnt->part2.bin->agedist){
		(*inferred)->part2.bin->agedist=masterprnt->part2.bin->agedist+1;
	} else {
		(*inferred)->part2.bin->agedist=secparent->part2.bin->agedist+1;
	}
	if (secparent->part2.bin->sinedist<masterprnt->part2.bin->sinedist){
		(*inferred)->part2.bin->sinedist=secparent->part2.bin->sinedist;
	} else {
		(*inferred)->part2.bin->sinedist=masterprnt->part2.bin->sinedist;
	}
	(*inferred)->inference=RESOLUTION;
	if (binpmaster!=masterprnt->part2.bin){
		MYFREE(binpmaster);
	}
	if (binpsec!=secparent->part2.bin){
		MYFREE(binpsec);
	}

	return(rr);
} /* BuildResolvent */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform Paramodulation to a clause)  OCJ
 *
 *    This function performs all the paramodulation inferences
 *    from all appropriate active clauses in the given KB to
 *    the given clause.
 *
 *    Inferred clauses are added to KB as unprocessed.
 *
 *    On return the status field of the KB structure is set with
 *    the values NOMEMORY, UNSATISFIABLE, UNKNOWN or TIMEOUT when
 *    appropriate.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause to which paramodulation will be made.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *
 *--------------------------------------------------------------*/
void PM2Clause(cmprefix *clause){
	auto subst Subst;                                    /* Substitution */
	auto cmprefix *clause2;                              /* Pointer to clause with which inference will be made */
	auto uint8_t *eqlit;                                 /* Pointer to "from" clause equality literal */
	auto uint8_t *unrttrm;                               /* Pointer to unifying equality root term */
	auto uint8_t *othrttrm;                              /* Pointer to the other equality root term */
	auto uint8_t *toterm;                                /* Pointer to the "to" term */
	auto cmprefix *inferred;                             /* Pointer to inferred clause */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5,*ptr6;    /* Auxiliary pointers */
	auto int32_t jj,kk,nn,oo,tt;                         /* Auxiliary */

	/* Initialize substitution and stack. */
	Subst.buffer=NULL;

	/* Loop through clause literals. */
	ptr4=othrttrm=eqlit=NULL; /* Just to avoid compiler warnings. */
	clause2=NULL; /* Just to avoid compiler warnings. */
	for (ptr1=&clause->part2.bin->formula[0];ptr1[0]!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS)){

		/* Literal is selected. */
		if (ptr1[0]&SELECTED){

			/* If literal is a positive or negative equality */
			/* take pointers of its root terms: */
			/* ptr3 = First equality root term. */
			/* ptr4 = Second equality root term. */
			if (EQUALITY&ptr1[0]){
				ptr3=NextItem(ptr1,IMMED);
				ptr4=NextItem(ptr3,OVERSUBTERMS);
			} else {
				ptr3=NULL;
			}

			/* Loop through subterms of literal. */
			for (toterm=NextItem(ptr1,IMMED),ptr2=NextItem(ptr1,OVERSUBTERMS);
					toterm<ptr2;toterm=NextItem(toterm,IMMED)){

				/* Iterate if subterm is a variable or is a subterm */
				/* of a non maximal equality root term. Also tt is set to: */
				/* 0 -> toterm is not in a positive or negative equality. */
				/* 1 -> toterm is in the first root term of a positive or negative equality. */
				/* 2 -> toterm is in the second root term of a positive or negative equality. */
				if (VARIABLE&toterm[0]){
					continue;
				}
				if (ptr3!=NULL){
					if ((toterm>=ptr3)&&(toterm<ptr4)){
						if (0==(SELECTED&ptr3[0])){
							continue;
						}
						tt=1;
					} else {
						if (0==(SELECTED&ptr4[0])){
							continue;
						}
						tt=2;
					}
				} else {
					tt=0;
				}

				/* Loop through candidates "from" clauses for paramodulation. */
				kk=-1;
				jj=1;
				nn=0;
				while (jj){

					/* Check timeout and solution by other process. */
					if (procctl->status==TIMEOUT){
						if (Subst.buffer!=NULL){
							MYFREE(Subst.buffer);
						}
						kbset.status=TIMEOUT;
						return;
					}
					if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
						if (Subst.buffer!=NULL){
							MYFREE(Subst.buffer);
						}
						kbset.status=UNKNOWN;
						return;
					}
					if (procctl->status==NOMEMORY){
						if (Subst.buffer!=NULL){
							MYFREE(Subst.buffer);
						}
						kbset.status=procctl->status;
						return;
					}

					/* Search for an unification candidate. First search for */
					/* root terms that are variables. The variable jj is set to 1 */
					/* if a valid unification candidate has been found, otherwise */
					/* it is set to 0 or NOMEMORY as appropriate. */
					/* Case of very first search. */
					if (kk==-1){
						if (kbset.vartrmidx.used>0){
							eqlit=kbset.vartrmidx.symbol[0];
							clause2=((binprefix *)(eqlit-(binpsize+((symbol *)&eqlit[1])->offset)))->clause;
							unrttrm=NextItem(eqlit,IMMED);
							if ((SELECTED|VARIABLE)!=((SELECTED|VARIABLE)&unrttrm[0])){
								othrttrm=unrttrm;
								unrttrm=NextItem(unrttrm,OVERSUBTERMS);
							} else {
								othrttrm=NextItem(unrttrm,OVERSUBTERMS);
							}
							kk=0;
							jj=1;
						} else {
							kk=-2;
							jj=QueryUnfDscTree(toterm,&unrttrm,(((symbol *)&toterm[1])->symbol)->discr[0],nn);
							nn=1;
						}

					/* Subsequent search of root terms that are variables. */
					} else if (kk>=0){

						/* Check the other root term of same end node element. */
						if (unrttrm==NextItem(eqlit,IMMED)){
							othrttrm=unrttrm;
							unrttrm=NextItem(unrttrm,OVERSUBTERMS);
							if ((SELECTED|VARIABLE)==((SELECTED|VARIABLE)&unrttrm[0])){
								jj=1;
							} else {
								kk++;
								if (kbset.vartrmidx.used>kk){
									eqlit=kbset.vartrmidx.symbol[kk];
									clause2=((binprefix *)(eqlit-(binpsize+((symbol *)&eqlit[1])->offset)))->clause;
									unrttrm=NextItem(eqlit,IMMED);
									if ((SELECTED|VARIABLE)!=((SELECTED|VARIABLE)&unrttrm[0])){
										othrttrm=unrttrm;
										unrttrm=NextItem(unrttrm,OVERSUBTERMS);
									} else {
										othrttrm=NextItem(unrttrm,OVERSUBTERMS);
									}
									jj=1;
								} else {
									kk=-2;
									jj=QueryUnfDscTree(toterm,&unrttrm,(((symbol *)&toterm[1])->symbol)->discr[0],nn);
									nn=1;
								}
							}

						/* Check next end node element. */
						} else {
							kk++;
							if (kbset.vartrmidx.used>kk){
								eqlit=kbset.vartrmidx.symbol[kk];
								clause2=((binprefix *)(eqlit-(binpsize+((symbol *)&eqlit[1])->offset)))->clause;
								unrttrm=NextItem(eqlit,IMMED);
								if ((SELECTED|VARIABLE)!=((SELECTED|VARIABLE)&unrttrm[0])){
									othrttrm=unrttrm;
									unrttrm=NextItem(unrttrm,OVERSUBTERMS);
								} else {
									othrttrm=NextItem(unrttrm,OVERSUBTERMS);
								}
								jj=1;
							} else {
								kk=-2;
								jj=QueryUnfDscTree(toterm,&unrttrm,(((symbol *)&toterm[1])->symbol)->discr[0],nn);
								nn=1;
							}
						}

					/* Tree search. */
					} else {
						jj=QueryUnfDscTree(toterm,&unrttrm,(((symbol *)&toterm[1])->symbol)->discr[0],nn);
						nn=1;
					}

					/* Process search result. */
					if (jj){

						/* Don't paramodulate from a term to itself unless the term */
						/* is not MAXIMUM or the other root term is not ground because */
						/* that produces an equational tautology with a literal */
						/* of the form s=s. */
						if (toterm==unrttrm){
							if (MAXIMUM&toterm[0]){
								continue;
							}
							ptr5=(tt==1?NextItem(toterm,OVERSUBTERMS):NextItem(ptr1,IMMED));
							for (ptr6=NextItem(ptr5,OVERSUBTERMS),oo=1;(ptr5<ptr6);ptr5=NextItem(ptr5,IMMED)){
								if (VARIABLE&ptr5[0]){
									oo=0;
									break;
								}
							}
							if (oo){
								continue;
							}
						}

						/* Unifying equality root term is not a variable. */
						if (kk==-2){

							/* Get pointer to the "from" clause. */
							clause2=((binprefix *)(unrttrm-(binpsize+((symbol *)&unrttrm[1])->offset)))->clause;

							/* Get pointers to the "from" clause equality literal and */
							/* the non unifying equality root term. We already have */
							/* these if unifying equality root term is a variable. */
							for (ptr6=&clause2->part2.bin->formula[0];ptr6<unrttrm;
									ptr6=NextItem(eqlit=ptr6,OVERSUBTERMS)){
							}
							ptr6=NextItem(eqlit,IMMED);
							if (ptr6==unrttrm){
								othrttrm=NextItem(ptr6,OVERSUBTERMS);
							} else {
								othrttrm=ptr6;
							}

							/* Check that the "from" equality literal is selected for */
							/* paramodulating from it. */
							if (0==(eqlit[0]&PMSELECTED)){
								continue;
							}

						/* If the "from" term is a variable then don't paramodulate */
						/* to the other equality root term in the "from" equality. */
						} else if (toterm==othrttrm){
							continue;
						}

						/* Check unification. Flag ITEM2SEC is used even if clause==clause2 */
						/* because the paramodulation of a clause in itself is performed as */
						/* a paramodulation of a clause in a duplicate of itself. */
						switch (Unify(toterm,unrttrm,&Subst,ITEM2SEC|INITSUBST)){

							/* Not enough memory. */
							case NOMEMORY:
								if (Subst.buffer!=NULL){
									MYFREE(Subst.buffer);
								}
								kbset.status=NOMEMORY;
								return;
								break;

							/* Candidate unifies with literal. */
							case 1:

								/* Check ordering constraint of root terms in equality */
								/* "from" literal. */
								if (NOMEMORY==(oo=CheckEqRootOrder(unrttrm,othrttrm,&Subst,clause->part2.bin->maxvarnb+1,
										clause->part2.bin->maxvarnb+clause2->part2.bin->maxvarnb+1,ITEM1SEC))){
									if (Subst.buffer!=NULL){
										MYFREE(Subst.buffer);
									}
									kbset.status=NOMEMORY;
									return;
								}

								/* If "to" literal is an equality then check ordering constraint */
								/* of root terms in equality "to" literal. */
								if ((oo)&&(tt)){
									if (NOMEMORY==(oo=CheckEqRootOrder(tt==1?ptr3:ptr4,tt==1?ptr4:ptr3,
											&Subst,clause->part2.bin->maxvarnb+1,
											clause->part2.bin->maxvarnb+clause2->part2.bin->maxvarnb+1,0))){
										if (Subst.buffer!=NULL){
											MYFREE(Subst.buffer);
										}
										kbset.status=NOMEMORY;
										return;
									}
								}

								/* Ordering constraint is OK. */
								if (oo){

									/* Build inferred clause. */
									if (0!=(oo=BuildPMClause(clause,clause2,eqlit,
											unrttrm,othrttrm,toterm,&inferred,&Subst))){
										if (Subst.buffer!=NULL){
											MYFREE(Subst.buffer);
										}
										kbset.status=oo;
										return;
									}
									kbset.prstats.paramodulations++;

									/* Add inferred clause to KB as unprocessed. */
									inferred->flags=UNPROC;
									if (NOMEMORY==AddBinClause2KB(inferred,&kbset,0)){
										if (Subst.buffer!=NULL){
											MYFREE(Subst.buffer);
										}
										kbset.status=NOMEMORY;
										return;
									}
								}
								break;
						}
					}
				}
			}
		}
	}

	/* Free memory and return. */
	if (Subst.buffer!=NULL){
		MYFREE(Subst.buffer);
	}
	return;
} /* PM2Clause */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform Paramodulation to a clause for UEQ)  OCJ
 *
 *    This function performs all the paramodulation inferences
 *    from all appropriate active clauses in the given KB to
 *    the given clause for type 8 problems (unit equality).
 *
 *    Inferred clauses are added to KB as unprocessed.
 *
 *    On return the status field of the KB structure is set with
 *    the values NOMEMORY, UNSATISFIABLE, UNKNOWN or TIMEOUT when
 *    appropriate.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause to which paramodulation will be made.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *
 *--------------------------------------------------------------*/
void PM2ClauseUEQ(cmprefix *clause){
	auto subst Subst;                                    /* Substitution */
	auto cmprefix *clause2;                              /* Pointer to clause with which inference will be made */
	auto uint8_t *eqlit;                                 /* Pointer to "from" clause equality literal */
	auto uint8_t *unrttrm;                               /* Pointer to unifying equality root term */
	auto uint8_t *othrttrm;                              /* Pointer to the other equality root term */
	auto uint8_t *toterm;                                /* Pointer to the "to" term */
	auto cmprefix *inferred;                             /* Pointer to inferred clause */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5,*ptr6;    /* Auxiliary pointers */
	auto int32_t jj,kk,nn,oo,tt;                         /* Auxiliary */

	/* Initialize substitution and stack. */
	Subst.buffer=NULL;

	/* Get pointer to clause literal. */
	ptr1=&clause->part2.bin->formula[0];
	ptr4=othrttrm=eqlit=NULL; /* Just to avoid compiler warnings. */
	clause2=NULL; /* Just to avoid compiler warnings. */
	ptr1=&clause->part2.bin->formula[0];

	/* Take pointers of (in)equality root terms: */
	/* ptr3 = First (in)equality root term. */
	/* ptr4 = Second (in)equality root term. */
	ptr3=NextItem(ptr1,IMMED);
	ptr4=NextItem(ptr3,OVERSUBTERMS);

	/* Loop through subterms of literal. */
	for (toterm=NextItem(ptr1,IMMED),ptr2=NextItem(ptr1,OVERSUBTERMS);
			toterm<ptr2;toterm=NextItem(toterm,IMMED)){

		/* Iterate if subterm is a variable or is a subterm */
		/* of a non maximal equality root term. Also tt is set to: */
		/* 1 -> toterm is in the first root term of a positive or negative equality. */
		/* 2 -> toterm is in the second root term of a positive or negative equality. */
		if (VARIABLE&toterm[0]){
			continue;
		}
		if ((toterm>=ptr3)&&(toterm<ptr4)){
			if (0==(SELECTED&ptr3[0])){
				continue;
			}
			tt=1;
		} else {
			if (0==(SELECTED&ptr4[0])){
				continue;
			}
			tt=2;
		}

		/* Loop through candidates "from" clauses for paramodulation. */
		kk=-1;
		jj=1;
		nn=0;
		while (jj){

			/* Check timeout and solution by other process. */
			if (procctl->status==TIMEOUT){
				if (Subst.buffer!=NULL){
					MYFREE(Subst.buffer);
				}
				kbset.status=TIMEOUT;
				return;
			}
			if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
				if (Subst.buffer!=NULL){
					MYFREE(Subst.buffer);
				}
				kbset.status=UNKNOWN;
				return;
			}
			if (procctl->status==NOMEMORY){
				if (Subst.buffer!=NULL){
					MYFREE(Subst.buffer);
				}
				kbset.status=procctl->status;
				return;
			}

			/* Search for an unification candidate. First search for */
			/* root terms that are variables. The variable jj is set to 1 */
			/* if a valid unification candidate has been found, otherwise */
			/* it is set to 0 or NOMEMORY as appropriate. */
			/* Case of very first search. */
			if (kk==-1){
				if (kbset.vartrmidx.used>0){
					eqlit=kbset.vartrmidx.symbol[0];
					clause2=((binprefix *)(eqlit-(binpsize+((symbol *)&eqlit[1])->offset)))->clause;
					unrttrm=NextItem(eqlit,IMMED);
					if ((SELECTED|VARIABLE)!=((SELECTED|VARIABLE)&unrttrm[0])){
						othrttrm=unrttrm;
						unrttrm=NextItem(unrttrm,OVERSUBTERMS);
					} else {
						othrttrm=NextItem(unrttrm,OVERSUBTERMS);
					}
					kk=0;
					jj=1;
				} else {
					kk=-2;
					jj=QueryUnfDscTree(toterm,&unrttrm,(((symbol *)&toterm[1])->symbol)->discr[0],nn);
					nn=1;
				}

			/* Subsequent search of root terms that are variables. */
			} else if (kk>=0){

				/* Check the other root term of same end node element. */
				if (unrttrm==NextItem(eqlit,IMMED)){
					othrttrm=unrttrm;
					unrttrm=NextItem(unrttrm,OVERSUBTERMS);
					if ((SELECTED|VARIABLE)==((SELECTED|VARIABLE)&unrttrm[0])){
						jj=1;
					} else {
						kk++;
						if (kbset.vartrmidx.used>kk){
							eqlit=kbset.vartrmidx.symbol[kk];
							clause2=((binprefix *)(eqlit-(binpsize+((symbol *)&eqlit[1])->offset)))->clause;
							unrttrm=NextItem(eqlit,IMMED);
							if ((SELECTED|VARIABLE)!=((SELECTED|VARIABLE)&unrttrm[0])){
								othrttrm=unrttrm;
								unrttrm=NextItem(unrttrm,OVERSUBTERMS);
							} else {
								othrttrm=NextItem(unrttrm,OVERSUBTERMS);
							}
							jj=1;
						} else {
							kk=-2;
							jj=QueryUnfDscTree(toterm,&unrttrm,(((symbol *)&toterm[1])->symbol)->discr[0],nn);
							nn=1;
						}
					}

				/* Check next end node element. */
				} else {
					kk++;
					if (kbset.vartrmidx.used>kk){
						eqlit=kbset.vartrmidx.symbol[kk];
						clause2=((binprefix *)(eqlit-(binpsize+((symbol *)&eqlit[1])->offset)))->clause;
						unrttrm=NextItem(eqlit,IMMED);
						if ((SELECTED|VARIABLE)!=((SELECTED|VARIABLE)&unrttrm[0])){
							othrttrm=unrttrm;
							unrttrm=NextItem(unrttrm,OVERSUBTERMS);
						} else {
							othrttrm=NextItem(unrttrm,OVERSUBTERMS);
						}
						jj=1;
					} else {
						kk=-2;
						jj=QueryUnfDscTree(toterm,&unrttrm,(((symbol *)&toterm[1])->symbol)->discr[0],nn);
						nn=1;
					}
				}

			/* Tree search. */
			} else {
				jj=QueryUnfDscTree(toterm,&unrttrm,(((symbol *)&toterm[1])->symbol)->discr[0],nn);
				nn=1;
			}

			/* Process search result. */
			if (jj){

				/* Don't paramodulate from a term to itself unless the term */
				/* is not MAXIMUM or the other root term is not ground because */
				/* that produces an equational tautology with a literal */
				/* of the form s=s. */
				if (toterm==unrttrm){
					if (MAXIMUM&toterm[0]){
						continue;
					}
					ptr5=(tt==1?NextItem(toterm,OVERSUBTERMS):NextItem(ptr1,IMMED));
					for (ptr6=NextItem(ptr5,OVERSUBTERMS),oo=1;(ptr5<ptr6);ptr5=NextItem(ptr5,IMMED)){
						if (VARIABLE&ptr5[0]){
							oo=0;
							break;
						}
					}
					if (oo){
						continue;
					}
				}

				/* Unifying equality root term is not a variable. */
				if (kk==-2){

					/* Get pointer to the "from" clause. */
					clause2=((binprefix *)(unrttrm-(binpsize+((symbol *)&unrttrm[1])->offset)))->clause;

					/* Get pointers to the "from" clause equality literal and */
					/* the non unifying equality root term. We already have */
					/* these if unifying equality root term is a variable. */
					for (ptr6=&clause2->part2.bin->formula[0];ptr6<unrttrm;
							ptr6=NextItem(eqlit=ptr6,OVERSUBTERMS)){
					}
					ptr6=NextItem(eqlit,IMMED);
					if (ptr6==unrttrm){
						othrttrm=NextItem(ptr6,OVERSUBTERMS);
					} else {
						othrttrm=ptr6;
					}

					/* Check that the "from" equality literal is selected for */
					/* paramodulating from it. */
					if (0==(eqlit[0]&PMSELECTED)){
						continue;
					}

				/* If the "from" term is a variable then don't paramodulate */
				/* to the other equality root term in the "from" equality. */
				} else if (toterm==othrttrm){
					continue;
				}

				/* Check unification. Flag ITEM2SEC is used even if clause==clause2 */
				/* because the paramodulation of a clause in itself is performed as */
				/* a paramodulation of a clause in a duplicate of itself. */
				switch (Unify(toterm,unrttrm,&Subst,ITEM2SEC|INITSUBST)){

					/* Not enough memory. */
					case NOMEMORY:
						if (Subst.buffer!=NULL){
							MYFREE(Subst.buffer);
						}
						kbset.status=NOMEMORY;
						return;
						break;

					/* Candidate unifies with literal. */
					case 1:

						/* Check ordering constraint of root terms in equality */
						/* "from" literal. */
						if (NOMEMORY==(oo=CheckEqRootOrder(unrttrm,othrttrm,&Subst,clause->part2.bin->maxvarnb+1,
								clause->part2.bin->maxvarnb+clause2->part2.bin->maxvarnb+1,ITEM1SEC))){
							if (Subst.buffer!=NULL){
								MYFREE(Subst.buffer);
							}
							kbset.status=NOMEMORY;
							return;
						}

						/* If "to" literal is an equality then check ordering constraint */
						/* of root terms in equality "to" literal. */
						if (oo){
							if (NOMEMORY==(oo=CheckEqRootOrder(tt==1?ptr3:ptr4,tt==1?ptr4:ptr3,
									&Subst,clause->part2.bin->maxvarnb+1,
									clause->part2.bin->maxvarnb+clause2->part2.bin->maxvarnb+1,0))){
								if (Subst.buffer!=NULL){
									MYFREE(Subst.buffer);
								}
								kbset.status=NOMEMORY;
								return;
							}
						}

						/* Ordering constraint is OK. */
						if (oo){

							/* Build inferred clause. */
							if (0!=(oo=BuildPMClause(clause,clause2,eqlit,
									unrttrm,othrttrm,toterm,&inferred,&Subst))){
								if (Subst.buffer!=NULL){
									MYFREE(Subst.buffer);
								}
								kbset.status=oo;
								return;
							}
							kbset.prstats.paramodulations++;

							/* Add inferred clause to KB as unprocessed. */
							inferred->flags=UNPROC;
							if (NOMEMORY==AddBinClause2KBUEQ(inferred,&kbset,0)){
								if (Subst.buffer!=NULL){
									MYFREE(Subst.buffer);
								}
								kbset.status=NOMEMORY;
								return;
							}
						}
						break;
				}
			}
		}
	}

	/* Free memory and return. */
	if (Subst.buffer!=NULL){
		MYFREE(Subst.buffer);
	}
	return;
} /* PM2ClauseUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform Paramodulation from a clause)  OCJ
 *
 *    This function performs all the paramodulation inferences
 *    from a given clause to all appropriate active clauses in
 *    the given KB.
 *
 *    Inferred clauses are added to KB as unprocessed.
 *
 *    On return the status field of the KB structure is set with
 *    the values NOMEMORY, UNSATISFIABLE, UNKNOWN or TIMEOUT when
 *    appropriate.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause from which paramodulation will be made.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *
 *--------------------------------------------------------------*/
void PMFromClause(cmprefix *clause){
	auto subst Subst;                                    /* Substitution */
	auto cmprefix *clause2;                              /* Pointer to clause with which inference will be made */
	auto uint8_t *eqlit,*eqlit2;                         /* Pointer to "from" and "to" clause equality literal */
	auto uint8_t *unrttrm;                               /* Pointer to unifying equality root term */
	auto uint8_t *othrttrm;                              /* Pointer to the other equality root term */
	auto uint8_t *toterm;                                /* Pointer to the "to" term */
	auto uint8_t *eqtrm1;                                /* Pointer to first equality root term of "to" equality. */
	auto uint8_t *eqtrm2;                                /* Pointer to second equality root term of "to" equality. */
	auto int32_t eqpos;                                  /* This is set to: */
	                                                     /* 0 if toterm is not inside an equality literal. */
	                                                     /* 1 if it is inside the first root term of an equality literal */
	                                                     /* 2 if it is inside the second root term of an equality literal */
	auto cmprefix *inferred;                             /* Pointer to inferred clause */
	auto uint8_t *ptr2,*ptr3;                            /* Auxiliary pointers */
	auto int32_t jj,kk,nn,oo,tt;                         /* Auxiliary */

	/* Initialize substitution and stack. */
	Subst.buffer=NULL;

	/* Loop through clause literals. */
	eqtrm1=eqtrm2=eqlit2=NULL; /* Just to avoid compiler warnings. */
	for (eqlit=&clause->part2.bin->formula[0];eqlit[0]!=UNITEND;
			eqlit=NextItem(eqlit,OVERSUBTERMS)){

		/* Literal is an equality selected for paramodulating from it. */
		/* It is necessary to check that the equality is positive for the */
		/* case that PMSELECTED==SELECTED. */
		if ((PMSELECTED|EQUALITY)==(eqlit[0]&(PMSELECTED|EQUALITY|NEGATED))){

			/* Get pointers to root equality terms. */
			ptr2=NextItem(eqlit,IMMED);
			ptr3=NextItem(ptr2,OVERSUBTERMS);

			/* Loop through root equality terms. */
			for (tt=0;tt<2;tt++){

				/* Identify unifying equality term and the other equality root term. */
				if (tt){
					unrttrm=ptr3;
					othrttrm=ptr2;
				} else {
					unrttrm=ptr2;
					othrttrm=ptr3;
				}

				/* Iterate if root equality term is not maximal. */
				if (0==(SELECTED&unrttrm[0])){
					continue;
				}

				/* Loop through candidates "to" clauses for paramodulation. */
				kk=eqpos=0;
				jj=1;
				nn=0;
				while (jj){

					/* Check timeout and solution by other process. */
					if (procctl->status==TIMEOUT){
						if (Subst.buffer!=NULL){
							MYFREE(Subst.buffer);
						}
						kbset.status=TIMEOUT;
						return;
					}
					if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
						if (Subst.buffer!=NULL){
							MYFREE(Subst.buffer);
						}
						kbset.status=UNKNOWN;
						return;
					}
					if (procctl->status==NOMEMORY){
						if (Subst.buffer!=NULL){
							MYFREE(Subst.buffer);
						}
						kbset.status=procctl->status;
						return;
					}

					/* Search for an unification candidate. The variable jj is set to 1 */
					/* if a valid unification candidate has been found, 0 or NOMEMORY otherwise. */
					/* If root term is a variable then search for any term in a selected literal */
					/* of all active clauses. */
					/* kk = 0 if term search not started. */
					/* kk = 1 if term search in progress. */
					if (VARIABLE&unrttrm[0]){

						/* Initialize pointer to item depending on status of term search. */
						switch (kk){

							/* Term search not started. */
							case 0:
								clause2=kbset.frstactive;
								toterm=&clause2->part2.bin->formula[0];
								kk=1;
								break;

							/* Term search in progress. */
							default:
								toterm=NextItem(toterm,IMMED);
								break;
						}

						/* Search for a valid term: */
						/* - Inside a selected literal. */
						/* - If the literal is a positive or negative equality */
						/*   then the term must be inside a maximal root term. */
						/* - The term must not be a variable. */
						/* The variables eqtrm1, eqtrm2 and eqpos are set as appropriate */
						/* (see comments in the variable definition section). */
						jj=0;
						while ((jj==0)&&(clause2!=NULL)){
							switch (toterm[0]&(UNITEND|VARIABLE|PREDICATE|EQUALITY|FUNCTION)){
								case UNITEND:
									clause2=clause2->next;
									if (clause==clause2){
										clause2=clause2->next;
									}
									if (NULL!=clause2){
										toterm=&clause2->part2.bin->formula[0];
									}
									break;
								case PREDICATE:
									eqpos=0;
									if (SELECTED&toterm[0]){
										toterm=NextItem(toterm,IMMED);
									} else {
										toterm=NextItem(toterm,OVERSUBTERMS);
									}
									break;
								case EQUALITY:
									if (SELECTED&toterm[0]){
										eqlit2=toterm;
										toterm=eqtrm1=NextItem(toterm,IMMED);
										eqtrm2=NextItem(eqtrm1,OVERSUBTERMS);
										if (SELECTED&toterm[0]){
											eqpos=1;
										} else {
											toterm=eqtrm2;
											eqpos=2;
										}
									} else {
										toterm=NextItem(toterm,OVERSUBTERMS);
									}
									break;
								case FUNCTION:
									if ((eqpos==1)&&(toterm==eqtrm2)){
										if (SELECTED&toterm[0]){
											jj=1;
											eqpos=2;
										} else {
											toterm=NextItem(toterm,OVERSUBTERMS);
											eqpos=0;
										}
									} else {
										jj=1;
									}
									break;
								case VARIABLE:
									toterm=NextItem(toterm,IMMED);
									if ((eqpos==1)&&(toterm==eqtrm2)){
										if (SELECTED&toterm[0]){
											jj=1;
											eqpos=2;
										} else {
											toterm=NextItem(toterm,OVERSUBTERMS);
											eqpos=0;
										}
									}
									break;
							}
						}

					/* If root term is not a variable then search tree for unification */
					/* candidates. */
					} else if (1==(jj=QueryUnfDscTree(unrttrm,&toterm,(((symbol *)&unrttrm[1])->symbol)->discr[1],nn))){

						/* We have a candidate. Get pointer to candidate clause and get */
						/* equality related parameters if we are into an equality. */
						nn=1;
						clause2=((binprefix *)(toterm-(binpsize+((symbol *)&toterm[1])->offset)))->clause;
						for (eqlit2=&clause2->part2.bin->formula[0],eqpos=-1;eqpos==-1;){
							eqtrm2=NextItem(eqlit2,OVERSUBTERMS);
							if (eqtrm2>toterm){
								if (EQUALITY&eqlit2[0]){
									eqtrm1=NextItem(eqlit2,IMMED);
									eqtrm2=NextItem(eqtrm1,OVERSUBTERMS);
									if (toterm<eqtrm2){
										eqpos=1;
									} else {
										eqpos=2;
									}
								} else {
									eqpos=0;
								}
							} else {
								eqlit2=eqtrm2;
							}
						}

						/* Don't paramodulate from a clause to itself. This paramodulation */
						/* is valid provided that toterm is not unrttrm but in this function */
						/* PMFromClause() if the clause is not the new current clause moved */
						/* to active then all inferences have been already done and if the */
						/* clause is the new current clause then paramodulation to itself is */
						/* done in PM2Clause() function. By the way this check of not */
						/* paramodulating from a clause to itself makes unnecessary the check */
						/* of paramodulating from a term to itself. */
						if (clause==clause2){
							continue;
						}

						/* Check unification. */
						switch (Unify(toterm,unrttrm,&Subst,ITEM2SEC|INITSUBST)){

							/* Not enough memory. */
							case NOMEMORY:
								if (Subst.buffer!=NULL){
									MYFREE(Subst.buffer);
								}
								kbset.status=NOMEMORY;
								return;
								break;

							/* Candidate unifies with literal. */
							case 1:

								/* Check ordering constraint of root terms in equality */
								/* "from" literal. */
								if (NOMEMORY==(oo=CheckEqRootOrder(unrttrm,othrttrm,&Subst,clause2->part2.bin->maxvarnb+1,
										clause->part2.bin->maxvarnb+clause2->part2.bin->maxvarnb+1,ITEM1SEC))){
									if (Subst.buffer!=NULL){
										MYFREE(Subst.buffer);
									}
									kbset.status=NOMEMORY;
									return;
								}

								/* If "to" literal is an equality then check ordering constraint */
								/* of root terms in equality "to" literal. */
								if ((oo)&&(eqpos)){
									if (NOMEMORY==(oo=CheckEqRootOrder(eqpos==1?eqtrm1:eqtrm2,eqpos==1?eqtrm2:eqtrm1,
											&Subst,clause2->part2.bin->maxvarnb+1,
											clause->part2.bin->maxvarnb+clause2->part2.bin->maxvarnb+1,0))){
										if (Subst.buffer!=NULL){
											MYFREE(Subst.buffer);
										}
										kbset.status=NOMEMORY;
										return;
									}
								}

								/* Ordering constraint is OK. */
								if (oo){

									/* Build inferred clause. */
									if (0!=(oo=BuildPMClause(clause2,clause,eqlit,
											unrttrm,othrttrm,toterm,&inferred,&Subst))){
										if (Subst.buffer!=NULL){
											MYFREE(Subst.buffer);
										}
										kbset.status=oo;
										return;
									}
									kbset.prstats.paramodulations++;

									/* Add inferred clause to KB as unprocessed. */
									inferred->flags=UNPROC;
									if (NOMEMORY==AddBinClause2KB(inferred,&kbset,0)){
										if (Subst.buffer!=NULL){
											MYFREE(Subst.buffer);
										}
										kbset.status=NOMEMORY;
										return;
									}
								}
								break;
						}
					}
				}
			}
		}
	}

	/* Free memory and return. */
	if (Subst.buffer!=NULL){
		MYFREE(Subst.buffer);
	}
	return;
} /* PMFromClause */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform Paramodulation from a clause for unit clause)  OCJ
 *
 *    This function performs all the paramodulation inferences
 *    from a given clause to all appropriate active clauses in
 *    the given KB for type 8 problems (unit equality).
 *
 *    Inferred clauses are added to KB as unprocessed.
 *
 *    On return the status field of the KB structure is set with
 *    the values NOMEMORY, UNSATISFIABLE, UNKNOWN or TIMEOUT when
 *    appropriate.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause from which paramodulation will be made.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *
 *--------------------------------------------------------------*/
void PMFromClauseUEQ(cmprefix *clause){
	auto subst Subst;                                    /* Substitution */
	auto cmprefix *clause2;                              /* Pointer to clause with which inference will be made */
	auto uint8_t *eqlit,*eqlit2;                         /* Pointer to "from" and "to" clause equality literal */
	auto uint8_t *unrttrm;                               /* Pointer to unifying equality root term */
	auto uint8_t *othrttrm;                              /* Pointer to the other equality root term */
	auto uint8_t *toterm;                                /* Pointer to the "to" term */
	auto uint8_t *eqtrm1;                                /* Pointer to first equality root term of "to" equality. */
	auto uint8_t *eqtrm2;                                /* Pointer to second equality root term of "to" equality. */
	auto int32_t eqpos;                                  /* This is set to: */
	                                                     /* 0 if toterm is not inside an equality literal. */
	                                                     /* 1 if it is inside the first root term of an equality literal */
	                                                     /* 2 if it is inside the second root term of an equality literal */
	auto cmprefix *inferred;                             /* Pointer to inferred clause */
	auto uint8_t *ptr2,*ptr3;                            /* Auxiliary pointers */
	auto int32_t jj,kk,nn,oo,tt;                         /* Auxiliary */

	/* Initialize substitution and stack. */
	Subst.buffer=NULL;

	/* Get pointer to clause literal. */
	eqtrm1=eqtrm2=NULL; /* Just to avoid compiler warnings. */
	eqlit=&clause->part2.bin->formula[0];

	/* Literal is an equality selected for paramodulating from it. */
	/* It is necessary to check that the equality is positive for the */
	/* case that PMSELECTED==SELECTED. */
	if ((PMSELECTED|EQUALITY)==(eqlit[0]&(PMSELECTED|EQUALITY|NEGATED))){

		/* Get pointers to root equality terms. */
		ptr2=NextItem(eqlit,IMMED);
		ptr3=NextItem(ptr2,OVERSUBTERMS);

		/* Loop through root equality terms. */
		for (tt=0;tt<2;tt++){

			/* Identify unifying equality term and the other equality root term. */
			if (tt){
				unrttrm=ptr3;
				othrttrm=ptr2;
			} else {
				unrttrm=ptr2;
				othrttrm=ptr3;
			}

			/* Iterate if root equality term is not maximal. */
			if (0==(SELECTED&unrttrm[0])){
				continue;
			}

			/* Loop through candidates "to" clauses for paramodulation. */
			kk=eqpos=0;
			jj=1;
			nn=0;
			while (jj){

				/* Check timeout and solution by other process. */
				if (procctl->status==TIMEOUT){
					if (Subst.buffer!=NULL){
						MYFREE(Subst.buffer);
					}
					kbset.status=TIMEOUT;
					return;
				}
				if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
					if (Subst.buffer!=NULL){
						MYFREE(Subst.buffer);
					}
					kbset.status=UNKNOWN;
					return;
				}
				if (procctl->status==NOMEMORY){
					if (Subst.buffer!=NULL){
						MYFREE(Subst.buffer);
					}
					kbset.status=procctl->status;
					return;
				}

				/* Search for an unification candidate. The variable jj is set to 1 */
				/* if a valid unification candidate has been found, 0 or NOMEMORY otherwise. */
				/* If root term is a variable then search for any term in a selected literal */
				/* of all active clauses. */
				/* kk = 0 if term search not started. */
				/* kk = 1 if term search in progress. */
				if (VARIABLE&unrttrm[0]){

					/* Initialize pointer to item depending on status of term search. */
					switch (kk){

						/* Term search not started. */
						case 0:
							clause2=kbset.frstactive;
							toterm=&clause2->part2.bin->formula[0];
							kk=1;
							break;

						/* Term search in progress. */
						default:
							toterm=NextItem(toterm,IMMED);
							break;
					}

					/* Search for a valid term: */
					/* - Inside a selected literal. */
					/* - If the literal is a positive or negative equality */
					/*   then the term must be inside a maximal root term. */
					/* - The term must not be a variable. */
					/* The variables eqtrm1, eqtrm2 and eqpos are set as appropriate */
					/* (see comments in the variable definition section). */
					jj=0;
					while ((jj==0)&&(clause2!=NULL)){
						switch (toterm[0]&(UNITEND|VARIABLE|PREDICATE|EQUALITY|FUNCTION)){
							case UNITEND:
								clause2=clause2->next;
								if (clause==clause2){
									clause2=clause2->next;
								}
								if (NULL!=clause2){
									toterm=&clause2->part2.bin->formula[0];
								}
								break;
							case EQUALITY:
								if (SELECTED&toterm[0]){
									eqlit2=toterm;
									toterm=eqtrm1=NextItem(toterm,IMMED);
									eqtrm2=NextItem(eqtrm1,OVERSUBTERMS);
									if (SELECTED&toterm[0]){
										eqpos=1;
									} else {
										toterm=eqtrm2;
										eqpos=2;
									}
								} else {
									toterm=NextItem(toterm,OVERSUBTERMS);
								}
								break;
							case FUNCTION:
								if ((eqpos==1)&&(toterm==eqtrm2)){
									if (SELECTED&toterm[0]){
										jj=1;
										eqpos=2;
									} else {
										toterm=NextItem(toterm,OVERSUBTERMS);
										eqpos=0;
									}
								} else {
									jj=1;
								}
								break;
							case VARIABLE:
								toterm=NextItem(toterm,IMMED);
								if ((eqpos==1)&&(toterm==eqtrm2)){
									if (SELECTED&toterm[0]){
										jj=1;
										eqpos=2;
									} else {
										toterm=NextItem(toterm,OVERSUBTERMS);
										eqpos=0;
									}
								}
								break;
						}
					}

				/* If root term is not a variable then search tree for unification */
				/* candidates. */
				} else if (1==(jj=QueryUnfDscTree(unrttrm,&toterm,(((symbol *)&unrttrm[1])->symbol)->discr[1],nn))){

					/* We have a candidate. Get pointer to candidate clause and get */
					/* equality related parameters. */
					nn=1;
					clause2=((binprefix *)(toterm-(binpsize+((symbol *)&toterm[1])->offset)))->clause;
					for (eqlit2=&clause2->part2.bin->formula[0],eqpos=-1;eqpos==-1;){
						eqtrm2=NextItem(eqlit2,OVERSUBTERMS);
						if (eqtrm2>toterm){
							eqtrm1=NextItem(eqlit2,IMMED);
							eqtrm2=NextItem(eqtrm1,OVERSUBTERMS);
							if (toterm<eqtrm2){
								eqpos=1;
							} else {
								eqpos=2;
							}
						} else {
							eqlit2=eqtrm2;
						}
					}

					/* Don't paramodulate from a clause to itself. This paramodulation */
					/* is valid provided that toterm is not unrttrm but in this function */
					/* PMFromClause() if the clause is not the new current clause moved */
					/* to active then all inferences have been already done and if the */
					/* clause is the new current clause then paramodulation to itself is */
					/* done in PM2Clause() function. By the way this check of not */
					/* paramodulating from a clause to itself makes unnecessary the check */
					/* of paramodulating from a term to itself. */
					if (clause==clause2){
						continue;
					}

					/* Check unification. */
					switch (Unify(toterm,unrttrm,&Subst,ITEM2SEC|INITSUBST)){

						/* Not enough memory. */
						case NOMEMORY:
							if (Subst.buffer!=NULL){
								MYFREE(Subst.buffer);
							}
							kbset.status=NOMEMORY;
							return;
							break;

						/* Candidate unifies with literal. */
						case 1:

							/* Check ordering constraint of root terms in equality */
							/* "from" literal. */
							if (NOMEMORY==(oo=CheckEqRootOrder(unrttrm,othrttrm,&Subst,clause2->part2.bin->maxvarnb+1,
									clause->part2.bin->maxvarnb+clause2->part2.bin->maxvarnb+1,ITEM1SEC))){
								if (Subst.buffer!=NULL){
									MYFREE(Subst.buffer);
								}
								kbset.status=NOMEMORY;
								return;
							}

							/* Check ordering constraint of root terms in (in)equality "to" literal. */
							if (oo){
								if (NOMEMORY==(oo=CheckEqRootOrder(eqpos==1?eqtrm1:eqtrm2,eqpos==1?eqtrm2:eqtrm1,
										&Subst,clause2->part2.bin->maxvarnb+1,
										clause->part2.bin->maxvarnb+clause2->part2.bin->maxvarnb+1,0))){
									if (Subst.buffer!=NULL){
										MYFREE(Subst.buffer);
									}
									kbset.status=NOMEMORY;
									return;
								}
							}

							/* Ordering constraint is OK. */
							if (oo){

								/* Build inferred clause. */
								if (0!=(oo=BuildPMClause(clause2,clause,eqlit,
										unrttrm,othrttrm,toterm,&inferred,&Subst))){
									if (Subst.buffer!=NULL){
										MYFREE(Subst.buffer);
									}
									kbset.status=oo;
									return;
								}
								kbset.prstats.paramodulations++;

								/* Add inferred clause to KB as unprocessed. */
								inferred->flags=UNPROC;
								if (NOMEMORY==AddBinClause2KBUEQ(inferred,&kbset,0)){
									if (Subst.buffer!=NULL){
										MYFREE(Subst.buffer);
									}
									kbset.status=NOMEMORY;
									return;
								}
							}
							break;
					}
				}
			}
		}
	}

	/* Free memory and return. */
	if (Subst.buffer!=NULL){
		MYFREE(Subst.buffer);
	}
	return;
} /* PMFromClauseUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Build clause inferred by paramodulation)  OCJ
 *
 *    This function builds a clause inferred by paramodulation given
 *    two parent clauses in binary format and the pointers to the
 *    other relevant components of the paramodulation (see arguments
 *    description below. It is assumed that assertions of inferring
 *    clauses are compatible.
 *
 *    The pointer to the result is placed in the address passed as
 *    argument.
 *
 *
 *  ARGUMENTS:
 *
 *    toparent: Pointer to "to" parent clause in binary format.
 *    frparent: Pointer to "from" parent clause in compiled format.
 *    eqlit: Pointer to equality literal in "from" clause.
 *    unrttrm: Pointer to unifying equality root term.
 *    othttrm: Pointer to the other equality root term.
 *    toterm: Pointer to the "to" term.
 *    inferred: Address of pointer to inferred clause.
 *    Subst: Pointer to the substitution of unification of "from"
 *           and "to" terms. The "from" term must be secondary
 *           in the substitution.
 *
 *  RETURNS:
 *
 *    0 -> OK
 *    NOMEMORY -> Not enough memory
 *    SLICETMOUT -> maximum number of executed hardware instructions
 *                  was reached.
 *
 *--------------------------------------------------------------*/
int32_t BuildPMClause(cmprefix *toparent,cmprefix *frparent,uint8_t *eqlit,uint8_t *unrttrm,
		uint8_t *othrttrm,uint8_t *toterm,cmprefix **inferred,subst *Subst){
	auto binprefix *binp;                                /* Auxiliary binary prefix pointer */
	auto uint8_t *ptr1,*ptr2;                            /* Auxiliary pointers */
	auto int32_t ii,jj,kk,mm,nn;                         /* Auxiliary */
	auto uint64_t hh;                                    /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Buffer for clause in text form. */

	ptr10=Decompile(frparent->part2.bin);
	MYFREE(ptr10);
	ptr10=Decompile(toparent->part2.bin);
	MYFREE(ptr10);
	#endif

	/* Allocate storage for inferred clause. */
	if (NULL==(*inferred=MYALLOC(sizeof(cmprefix)))){
		return(NOMEMORY);
	}
	nn=binpsize+2*(toparent->part2.bin->size+frparent->part2.bin->size);
	if (NULL==(((*inferred)->part2.bin)=MYALLOC(nn))){
		MYFREE(*inferred);
		return(NOMEMORY);
	}
	(*inferred)->part2.bin->clause=*inferred;
	(*inferred)->part2.bin->oriented=0;

	/* Copy formula of "to" parent clause up to and not including toterm. */
	ii=toterm-&toparent->part2.bin->formula[0];
	memcpy(&(*inferred)->part2.bin->formula[0],&toparent->part2.bin->formula[0],ii);

	/* Add othrttrm term of "from" parent clause. */
	ptr1=NextItem(othrttrm,OVERSUBTERMS);
	jj=ptr1-othrttrm;
	memcpy(&(*inferred)->part2.bin->formula[mm=ii],othrttrm,jj);
	ii+=jj;

	/* Shift the numbers of variables coming from othrttrm term. */
	kk=toparent->part2.bin->maxvarnb+1;
	for (ptr2=&(*inferred)->part2.bin->formula[mm];
			ptr2<&(*inferred)->part2.bin->formula[ii];
			ptr2=NextItem(ptr2,IMMED)){
		if (VARIABLE&ptr2[0]){
			*((uint16_t *)&ptr2[1])+=kk;
		}
	}

	/* Add remaining items of "to" parent clause not including the UNITEND marker byte. */
	ptr1=NextItem(toterm,OVERSUBTERMS);
	jj=toparent->part2.bin->size-(ptr1-&toparent->part2.bin->formula[0])-1;
	memcpy(&(*inferred)->part2.bin->formula[ii],ptr1,jj);
	ii+=jj;

	/* Add formula of "from" parent clause up to and not including eqlit. */
	jj=eqlit-&frparent->part2.bin->formula[0];
	memcpy(&(*inferred)->part2.bin->formula[ii],&frparent->part2.bin->formula[0],jj);
	mm=ii;
	ii+=jj;

	/* Add literals of "from" parent clause following eqlit */
	/* plus ending UNITEND marker byte. */
	ptr1=NextItem(eqlit,OVERSUBTERMS);
	jj=frparent->part2.bin->size-(ptr1-&frparent->part2.bin->formula[0]);
	memcpy(&(*inferred)->part2.bin->formula[ii],ptr1,jj);

	/* Shift the numbers of variables coming from "from" clause. */
	for (ptr1=&(*inferred)->part2.bin->formula[mm];ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){
		if (VARIABLE&ptr1[0]){
			*((uint16_t *)&ptr1[1])+=kk;
		}
	}

	/* Apply substitution to inferred clause. Binary clause */
	/* size field is set by this function, so there is no need */
	/* to call UpdateSize() function here. */
	if (NOMEMORY==ApplySubst2Clause(*inferred,&nn,Subst,kk)){
		MYFREE((*inferred)->part2.bin);
		MYFREE(*inferred);
		return(NOMEMORY);
	}

	/* Reallocate the formula buffer to the exact length needed. */
	if (NULL==(binp=MYREALLOC((*inferred)->part2.bin,binpsize+(*inferred)->part2.bin->size))){
		MYFREE((*inferred)->part2.bin);
		MYFREE(*inferred);
		return(NOMEMORY);
	}
	(*inferred)->part2.bin=binp;

	/* Add assertions to inferred clause. */
	if (kbset.opts.split){
		if (NOMEMORY==AddAssertions(toparent->part2.bin,frparent->part2.bin,(*inferred)->part2.bin)){
			MYFREE((*inferred)->part2.bin);
			MYFREE(*inferred);
			return(NOMEMORY);
		}
	} else {
		(*inferred)->part2.bin->ovly.asserts=(*inferred)->part2.bin->lockasserts=NULL;
	}

	/* Debug. */
	#ifdef DEBUGCODE
	ptr10=Decompile(frparent->part2.bin);
	#ifdef VERBOSE
	if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
		printf("Paramodulation from: %s\n",ptr10);
	}
	#endif
	MYFREE(ptr10);
	#ifdef VERBOSE
	if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
		ptr10=Decompile(toparent->part2.bin);
		printf("To: %s\n",ptr10);
		MYFREE(ptr10);
	}
	#endif
	ptr10=Decompile((*inferred)->part2.bin);
	#ifdef VERBOSE
	if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
		printf("Result [%ld]:\n%s\n",kbset.nformulas,ptr10);
	}
	#endif
	MYFREE(ptr10);
	#endif

	/* Set clause cmprefix data. */
	(*inferred)->parent1=frparent;
	(*inferred)->parent2=toparent;
	if (frparent->part2.bin->agedist<toparent->part2.bin->agedist){
		(*inferred)->part2.bin->agedist=toparent->part2.bin->agedist+1;
	} else {
		(*inferred)->part2.bin->agedist=frparent->part2.bin->agedist+1;
	}
	if (frparent->part2.bin->sinedist<toparent->part2.bin->sinedist){
		(*inferred)->part2.bin->sinedist=frparent->part2.bin->sinedist;
	} else {
		(*inferred)->part2.bin->sinedist=toparent->part2.bin->sinedist;
	}
	(*inferred)->inference=PARAMODULATION;

	/* Check number of executed hardware instructions. */
	myread(&hh);
	if (hh>=instrlimit){
		return(SLICETMOUT);
	}
	return(0);
} /* BuildPMClause */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform equality resolution inference)  OCJ
 *
 *    This function performs all the equality resolution inferences
 *    for the clause passed as argument.
 *
 *    Inferred clauses are added to KB as unprocessed.
 *
 *    On return the status field of the KB structure is set with
 *    the values NOMEMORY, UNSATISFIABLE, UNKNOWN or TIMEOUT when
 *    appropriate.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause with which inferences will be made.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *
 *--------------------------------------------------------------*/
void EqualityResolution(cmprefix *clause){
	auto cmprefix *auxcl;                                /* Pointer to auxiliary clause */
	auto int32_t size;                                   /* Size of auxiliary clause binary buffer in bytes */
	auto subst Subst;                                    /* Substitution */
	auto binprefix *binp;                                /* Auxiliary binprefix pointer */
	auto uint8_t *ptr1,*ptr2;                            /* Auxiliary pointers */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	#endif

	/* Loop through clause literals. */
	Subst.buffer=NULL;
	for (ptr1=&clause->part2.bin->formula[0];ptr1[0]!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS)){

		/* Literal is a selected negative equality. */
		if ((SELECTED|NEGATED|EQUALITY)==(ptr1[0]&(SELECTED|NEGATED|EQUALITY))){

			/* Check timeout and solution by other process. */
			if (procctl->status==TIMEOUT){
				if (Subst.buffer!=NULL){
					MYFREE(Subst.buffer);
				}
				kbset.status=TIMEOUT;
				return;
			}
			if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
				if (Subst.buffer!=NULL){
					MYFREE(Subst.buffer);
				}
				kbset.status=UNKNOWN;
				return;
			}
			if (procctl->status==NOMEMORY){
				if (Subst.buffer!=NULL){
					MYFREE(Subst.buffer);
				}
				kbset.status=procctl->status;
				return;
			}

			/* Check unification of equality root terms. */
			ptr2=NextItem(ptr1,IMMED);
			switch (Unify(ptr2,NextItem(ptr2,OVERSUBTERMS),&Subst,INITSUBST)){

				/* Not enough memory. */
				case NOMEMORY:
					if (Subst.buffer!=NULL){
						MYFREE(Subst.buffer);
					}
					kbset.status=NOMEMORY;
					return;
					break;

				/* Equality root terms unify. */
				case 1:

					/* Allocate storage for auxiliary clause and copy */
					/* clause into the auxiliary clause. */
					size=clause->part2.bin->size+binpsize;
					if ((auxcl=MYALLOC(sizeof(cmprefix)))==NULL){
						if (Subst.buffer!=NULL){
							MYFREE(Subst.buffer);
						}
						kbset.status=NOMEMORY;
						return;
					}
					if ((auxcl->part2.bin=MYALLOC(size))==NULL){
						MYFREE(auxcl);
						if (Subst.buffer!=NULL){
							MYFREE(Subst.buffer);
						}
						kbset.status=NOMEMORY;
						return;
					}
					memcpy(auxcl->part2.bin,clause->part2.bin,size);
					auxcl->part2.bin->clause=auxcl;
					auxcl->part2.bin->oriented=0;

					/* Delete equality literal. */
					DeleteLiteral(auxcl->part2.bin,&auxcl->part2.bin->formula[0]+(ptr1-&clause->part2.bin->formula[0]));
					auxcl->parent1=clause;
					auxcl->part2.bin->agedist=clause->part2.bin->agedist+1;
					auxcl->part2.bin->sinedist=clause->part2.bin->sinedist;
					auxcl->parent2=NULL;
					auxcl->inference=EQRESOLUTION;
					kbset.prstats.equresolutions++;

					/* Apply substitution to inferred clause. Binary clause */
					/* size field is set by this function, so there is no need */
					/* to call UpdateSize() function here. */
					if (NOMEMORY==ApplySubst2Clause(auxcl,&size,&Subst,0)){
						MYFREE(auxcl->part2.bin);
						MYFREE(auxcl);
						if (Subst.buffer!=NULL){
							MYFREE(Subst.buffer);
						}
						kbset.status=NOMEMORY;
						return;
					}

					/* Reallocate the formula buffer to the exact length needed. */
					if (NULL==(binp=MYREALLOC(auxcl->part2.bin,binpsize+auxcl->part2.bin->size))){
						MYFREE(auxcl->part2.bin);
						MYFREE(auxcl);
						if (Subst.buffer!=NULL){
							MYFREE(Subst.buffer);
						}
						kbset.status=NOMEMORY;
						return;
					}
					auxcl->part2.bin=binp;

					/* Add assertions to inferred clause. */
					if (NOMEMORY==AddAssertions(clause->part2.bin,NULL,auxcl->part2.bin)){
						MYFREE(auxcl->part2.bin);
						MYFREE(auxcl);
						if (Subst.buffer!=NULL){
							MYFREE(Subst.buffer);
						}
						kbset.status=NOMEMORY;
						return;
					}

					/* Check empty clause without assertions. */
					if ((auxcl->part2.bin->formula[0]==UNITEND)&&(auxcl->part2.bin->ovly.asserts==NULL)){
						auxcl->flags=UNPROC;
						if (NOMEMORY==AddBinClause2KB(auxcl,&kbset,0)){
							if (auxcl->part2.bin->ovly.asserts!=NULL){
								MYFREE(auxcl->part2.bin->ovly.asserts);
							}
							MYFREE(auxcl->part2.bin);
							MYFREE(auxcl);
							if (Subst.buffer!=NULL){
								MYFREE(Subst.buffer);
							}
							kbset.status=NOMEMORY;
							return;
						}
						if (Subst.buffer!=NULL){
							MYFREE(Subst.buffer);
						}
						if (NULL==(kbset.frstprfnode=MYALLOC(sizeof(proofnode)))){
							kbset.status=NOMEMORY;
							return;
						}
						kbset.frstprfnode->type=ACLAUSE;
						kbset.frstprfnode->ptr.Aclause=auxcl;
						kbset.status=UNSATISFIABLE;
						return;
					}

					/* Debug. */
					#ifdef DEBUGCODE
					ptr10=Decompile(auxcl->part2.bin);
					#ifdef VERBOSE
					if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
						printf("Equality resolution result [%ld]:\n%s\n",kbset.nformulas,ptr10);
					}
					#endif
					MYFREE(ptr10);
					#endif

					/* Add inferred clause to KB as unprocessed. */
					auxcl->flags=UNPROC;
					if (NOMEMORY==AddBinClause2KB(auxcl,&kbset,0)){
						if (Subst.buffer!=NULL){
							MYFREE(Subst.buffer);
						}
						kbset.status=NOMEMORY;
						return;
					}
					break;
			}
		}
	}

	/* Free substitution memory and return. */
	if (Subst.buffer!=NULL){
		MYFREE(Subst.buffer);
	}
	return;
} /* EqualityResolution */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform equality resolution inference for unit clause)  OCJ
 *
 *    This function performs all the equality resolution inferences
 *    for the clause passed as argument for type 8 problems (unit
 *    equality).
 *
 *    Inferred clauses are added to KB as unprocessed.
 *
 *    On return the status field of the KB structure is set with
 *    the values NOMEMORY, UNSATISFIABLE, UNKNOWN or TIMEOUT when
 *    appropriate.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause with which inferences will be made.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *
 *--------------------------------------------------------------*/
void EqualityResolutionUEQ(cmprefix *clause){
	auto cmprefix *auxcl;                                /* Pointer to auxiliary clause */
	auto int32_t size;                                   /* Size of auxiliary clause binary buffer in bytes */
	auto subst Subst;                                    /* Substitution */
	auto uint8_t *ptr1,*ptr2;                            /* Auxiliary pointers */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	#endif

	/* Get pointer to literal. */
	Subst.buffer=NULL;
	ptr1=&clause->part2.bin->formula[0];

	/* Literal is a selected negative equality. */
	if ((SELECTED|NEGATED|EQUALITY)==(ptr1[0]&(SELECTED|NEGATED|EQUALITY))){

		/* Check timeout and solution by other process. */
		if (procctl->status==TIMEOUT){
			if (Subst.buffer!=NULL){
				MYFREE(Subst.buffer);
			}
			kbset.status=TIMEOUT;
			return;
		}
		if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
			if (Subst.buffer!=NULL){
				MYFREE(Subst.buffer);
			}
			kbset.status=UNKNOWN;
			return;
		}
		if (procctl->status==NOMEMORY){
			if (Subst.buffer!=NULL){
				MYFREE(Subst.buffer);
			}
			kbset.status=procctl->status;
			return;
		}

		/* Check unification of equality root terms. */
		ptr2=NextItem(ptr1,IMMED);
		switch (Unify(ptr2,NextItem(ptr2,OVERSUBTERMS),&Subst,INITSUBST)){

			/* Not enough memory. */
			case NOMEMORY:
				if (Subst.buffer!=NULL){
					MYFREE(Subst.buffer);
				}
				kbset.status=NOMEMORY;
				return;
				break;

			/* Equality root terms unify. */
			case 1:

				/* Allocate storage for auxiliary empty clause. */
				size=1+binpsize;
				if ((auxcl=MYALLOC(sizeof(cmprefix)))==NULL){
					if (Subst.buffer!=NULL){
						MYFREE(Subst.buffer);
					}
					kbset.status=NOMEMORY;
					return;
				}
				if ((auxcl->part2.bin=MYALLOC(size))==NULL){
					MYFREE(auxcl);
					if (Subst.buffer!=NULL){
						MYFREE(Subst.buffer);
					}
					kbset.status=NOMEMORY;
					return;
				}
				auxcl->part2.bin->formula[0]=UNITEND;
				auxcl->part2.bin->clause=auxcl;
				auxcl->parent1=clause;
				auxcl->part2.bin->agedist=clause->part2.bin->agedist+1;
				auxcl->part2.bin->sinedist=clause->part2.bin->sinedist;
				auxcl->parent2=NULL;
				auxcl->part2.bin->ovly.asserts=auxcl->part2.bin->lockasserts=NULL;
				auxcl->inference=EQRESOLUTION;
				kbset.prstats.equresolutions++;

				/* Debug. */
				#ifdef DEBUGCODE
				ptr10=Decompile(auxcl->part2.bin);
				#ifdef VERBOSE
				if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
					printf("Equality resolution result [%ld]:\n%s\n",kbset.nformulas,ptr10);
				}
				#endif
				MYFREE(ptr10);
				#endif

				/* Add empty clause to KB. */
				auxcl->flags=UNPROC;
				if (NOMEMORY==AddBinClause2KBUEQ(auxcl,&kbset,0)){
					MYFREE(auxcl->part2.bin);
					MYFREE(auxcl);
					if (Subst.buffer!=NULL){
						MYFREE(Subst.buffer);
					}
					kbset.status=NOMEMORY;
					return;
				}
				if (NULL==(kbset.frstprfnode=MYALLOC(sizeof(proofnode)))){
					kbset.status=NOMEMORY;
					return;
				}
				kbset.frstprfnode->type=ACLAUSE;
				kbset.frstprfnode->ptr.Aclause=auxcl;
				kbset.status=UNSATISFIABLE;
				break;
		}
	}

	/* Free substitution memory and return. */
	if (Subst.buffer!=NULL){
		MYFREE(Subst.buffer);
	}
	return;
} /* EqualityResolutionUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform equality factoring inference)  OCJ
 *
 *    This function performs all the equality factoring inferences
 *    for the clause passed as argument.
 *
 *    Inferred clauses are added to KB as unprocessed.
 *
 *    On return the status field of the KB structure is set with
 *    the values NOMEMORY, UNSATISFIABLE, UNKNOWN or TIMEOUT when
 *    appropriate.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause with which inferences will be made.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *
 *--------------------------------------------------------------*/
void EqualityFactoring(cmprefix *clause){
	auto uint8_t *mxtrm;                                 /* Selected equality maximal root term. */
	auto uint8_t *nonmxtrm;                              /* Selected equality other root term. */
	auto uint8_t *untrm;                                 /* Unifying root term in the other equality. */
	auto uint8_t *othtrm;                                /* The other root term in the other equality. */
	auto cmprefix *auxcl;                                /* Pointer to auxiliary clause */
	auto int32_t size;                                   /* Size of auxiliary clause binary buffer in bytes */
	auto subst Subst;                                    /* Substitution */
	auto binprefix *binp;                                /* Auxiliary binprefix pointer */
	auto symbol *smbl1,*smbl2;                           /* Symbol pointers */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4,*ptr5,*ptr6;    /* Auxiliary pointers */
	auto int32_t ii,jj,kk,oo;                            /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	#endif

	/* Loop through clause literals. */
	Subst.buffer=NULL;
	for (ptr1=&clause->part2.bin->formula[0],ptr2=NULL;ptr1!=NULL;ptr1=ptr6){

		/* Literal is not a positive equality. */
		/* Prepare next iteration and iterate. */
		if ((EQUALITY)!=(ptr1[0]&(NEGATED|EQUALITY))){
			ptr6=NextItem(ptr1,OVERSUBTERMS);
			if (ptr6[0]==UNITEND){
				ptr6=NULL;
			}
			continue;

		/* Literal is a positive equality. */
		} else {

			/* Remember first positive equality literal. */
			if (ptr2==NULL){
				ptr2=ptr1;
			}

			/* Equality is not selected. Prepare next iteration and iterate. */
			if (0==(ptr1[0]&SELECTED)){
				ptr6=NextItem(ptr1,OVERSUBTERMS);
				if (ptr6[0]==UNITEND){
					ptr6=NULL;
				}
				continue;
			}

			/* Get the maximal and the equality root terms. */
			mxtrm=NextItem(ptr1,IMMED);
			nonmxtrm=NextItem(mxtrm,OVERSUBTERMS);
			if (0==(SELECTED&mxtrm[0])){
				if (SELECTED&nonmxtrm[0]){
					ptr3=nonmxtrm;
					nonmxtrm=mxtrm;
					mxtrm=ptr3;
				} else {
					mxtrm=NULL;
				}
			}

			/* Loop through maximal equality root terms. */
			ptr6=NULL;
			while (mxtrm!=NULL){

				/* Check timeout and solution by other process. */
				if (procctl->status==TIMEOUT){
					if (Subst.buffer!=NULL){
						MYFREE(Subst.buffer);
					}
					kbset.status=TIMEOUT;
					return;
				}
				if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
					if (Subst.buffer!=NULL){
						MYFREE(Subst.buffer);
					}
					kbset.status=UNKNOWN;
					return;
				}
				if (procctl->status==NOMEMORY){
					if (Subst.buffer!=NULL){
						MYFREE(Subst.buffer);
					}
					kbset.status=procctl->status;
					return;
				}

				/* Loop through positive equality literals. */
				for (ptr4=ptr2;ptr4[0]!=UNITEND;ptr4=NextItem(ptr4,OVERSUBTERMS)){

					/* Iterate if literal is not a positive equality */
					/* or if it is equal to the selected literal ptr1. */
					if ((EQUALITY!=(ptr4[0]&(NEGATED|EQUALITY)))||(ptr4==ptr1)){
						continue;
					}

					/* Prepare next iteration of main loop if appropriate. */
					if ((ptr6==NULL)&&(ptr4>ptr1)&&(ptr4[0]&SELECTED)){
						ptr6=ptr4;
					}

					/* Loop through equality root terms. */
					untrm=NextItem(ptr4,IMMED);
					othtrm=NextItem(untrm,OVERSUBTERMS);
					while (untrm!=NULL){

						/* Check unification of equality root terms. */
						switch (Unify(mxtrm,untrm,&Subst,INITSUBST)){

							/* Not enough memory. */
							case NOMEMORY:
								if (Subst.buffer!=NULL){
									MYFREE(Subst.buffer);
								}
								kbset.status=NOMEMORY;
								return;
								break;

							/* Equality root terms unify. */
							case 1:

								/* Check ordering constraint of root terms in equality */
								/* "from" literal. */
								if (NOMEMORY==(oo=CheckEqRootOrder(mxtrm,nonmxtrm,&Subst,0,clause->part2.bin->maxvarnb,0))){
									if (Subst.buffer!=NULL){
										MYFREE(Subst.buffer);
									}
									kbset.status=NOMEMORY;
									return;
								}

								/* Ordering constraint is OK. */
								if (oo){

									/* Allocate storage for auxiliary clause. */
									/* Size is overestimated. */
									size=(2*clause->part2.bin->size)+binpsize;
									if ((auxcl=MYALLOC(sizeof(cmprefix)))==NULL){
										if (Subst.buffer!=NULL){
											MYFREE(Subst.buffer);
										}
										kbset.status=NOMEMORY;
										return;
									}
									if ((auxcl->part2.bin=MYALLOC(size))==NULL){
										MYFREE(auxcl);
										if (Subst.buffer!=NULL){
											MYFREE(Subst.buffer);
										}
										kbset.status=NOMEMORY;
										return;
									}

									/* Copy the clause into the auxiliary clause not including */
									/* the equalities and the UNITEND byte marker. */
									ptr3=(ptr1<ptr4?ptr1:ptr4);
									ii=ptr3-&clause->part2.bin->formula[0];
									memcpy(auxcl->part2.bin,clause->part2.bin,binpsize+ii);
									auxcl->part2.bin->clause=auxcl;
									ptr5=NextItem(ptr3,OVERSUBTERMS);
									ptr3=(ptr3==ptr1?ptr4:ptr1);
									jj=ptr3-ptr5;
									memcpy(&auxcl->part2.bin->formula[ii],ptr5,jj);
									ii+=jj;
									jj=clause->part2.bin->size-1-(NextItem(ptr3,OVERSUBTERMS)-&clause->part2.bin->formula[0]);
									memcpy(&auxcl->part2.bin->formula[ii],NextItem(ptr3,OVERSUBTERMS),jj);
									ii+=jj;

									/* Add negative equality. */
									auxcl->part2.bin->formula[ii]=(NEGATED|EQUALITY);
									smbl1=(symbol *)&auxcl->part2.bin->formula[ii+1];
									smbl1->symbol=&equkey;
									smbl1->offset=0;
									ii+=(1+sizeof(symbol));
									if (FUNCTION&nonmxtrm[0]){
										smbl2=(symbol *)&nonmxtrm[1];
										jj=smbl1->nextoff=smbl2->nextoff-smbl2->offset;
										memcpy(&auxcl->part2.bin->formula[ii],nonmxtrm,jj);
										ii+=jj;
										if (FUNCTION&othtrm[0]){
											smbl2=(symbol *)&othtrm[1];
											kk=smbl2->nextoff-smbl2->offset;
											smbl1->nextoff+=kk;
											memcpy(&auxcl->part2.bin->formula[ii],othtrm,kk);
											ii+=kk;
										} else {
											smbl1->nextoff+=3;
											memcpy(&auxcl->part2.bin->formula[ii],othtrm,3);
											ii+=3;
										}
									} else {
										memcpy(&auxcl->part2.bin->formula[ii],nonmxtrm,3);
										smbl1->nextoff=3;
										ii+=3;
										if (FUNCTION&othtrm[0]){
											smbl2=(symbol *)&othtrm[1];
											kk=smbl2->nextoff-smbl2->offset;
											smbl1->nextoff+=kk;
											memcpy(&auxcl->part2.bin->formula[ii],othtrm,kk);
											ii+=kk;
										} else {
											smbl1->nextoff+=3;
											memcpy(&auxcl->part2.bin->formula[ii],othtrm,3);
											ii+=3;
										}
									}

									/* Add positive equality and UNITEND byte marker. */
									auxcl->part2.bin->formula[ii]=EQUALITY;
									smbl1=(symbol *)&auxcl->part2.bin->formula[ii+1];
									smbl1->symbol=&equkey;
									smbl1->offset=0;
									ii+=(1+sizeof(symbol));
									if (FUNCTION&mxtrm[0]){
										smbl2=(symbol *)&mxtrm[1];
										jj=smbl1->nextoff=smbl2->nextoff-smbl2->offset;
										memcpy(&auxcl->part2.bin->formula[ii],mxtrm,jj);
										ii+=jj;
										if (FUNCTION&othtrm[0]){
											smbl2=(symbol *)&othtrm[1];
											kk=smbl2->nextoff-smbl2->offset;
											smbl1->nextoff+=kk;
											memcpy(&auxcl->part2.bin->formula[ii],othtrm,kk);
											ii+=kk;
										} else {
											smbl1->nextoff+=3;
											memcpy(&auxcl->part2.bin->formula[ii],othtrm,3);
											ii+=3;
										}
									} else {
										memcpy(&auxcl->part2.bin->formula[ii],mxtrm,3);
										smbl1->nextoff=3;
										ii+=3;
										if (FUNCTION&othtrm[0]){
											smbl2=(symbol *)&othtrm[1];
											kk=smbl2->nextoff-smbl2->offset;
											smbl1->nextoff+=kk;
											memcpy(&auxcl->part2.bin->formula[ii],othtrm,kk);
											ii+=kk;
										} else {
											smbl1->nextoff+=3;
											memcpy(&auxcl->part2.bin->formula[ii],othtrm,3);
											ii+=3;
										}
									}
									auxcl->part2.bin->formula[ii]=UNITEND;

									/* Apply substitution to inferred clause. Binary clause */
									/* size field is set by this function, so there is no need */
									/* to call UpdateSize() function here. */
									if (NOMEMORY==ApplySubst2Clause(auxcl,&size,&Subst,0)){
										MYFREE(auxcl->part2.bin);
										MYFREE(auxcl);
										if (Subst.buffer!=NULL){
											MYFREE(Subst.buffer);
										}
										kbset.status=NOMEMORY;
										return;
									}

									/* Reallocate the formula buffer to the exact length needed. */
									if (NULL==(binp=MYREALLOC(auxcl->part2.bin,binpsize+auxcl->part2.bin->size))){
										MYFREE(auxcl->part2.bin);
										MYFREE(auxcl);
										if (Subst.buffer!=NULL){
											MYFREE(Subst.buffer);
										}
										kbset.status=NOMEMORY;
										return;
									}
									auxcl->part2.bin=binp;
									auxcl->part2.bin->oriented=0;
									auxcl->parent1=clause;
									auxcl->parent2=NULL;
									auxcl->part2.bin->agedist=clause->part2.bin->agedist+1;
									auxcl->part2.bin->sinedist=clause->part2.bin->sinedist;
									auxcl->inference=EQFACTORING;
									kbset.prstats.equfactorings++;

									/* Add assertions to inferred clause. */
									if (NOMEMORY==AddAssertions(clause->part2.bin,NULL,auxcl->part2.bin)){
										MYFREE(auxcl->part2.bin);
										MYFREE(auxcl);
										if (Subst.buffer!=NULL){
											MYFREE(Subst.buffer);
										}
										kbset.status=NOMEMORY;
										return;
									}

									/* Debug. */
									#ifdef DEBUGCODE
									ptr10=Decompile(auxcl->part2.bin);
									#ifdef VERBOSE
									if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
										printf("Equality factoring result [%ld]:\n%s\n",kbset.nformulas,ptr10);
									}
									#endif
									MYFREE(ptr10);
									#endif

									/* Add inferred clause to KB as unprocessed. */
									auxcl->flags=UNPROC;
									if (NOMEMORY==AddBinClause2KB(auxcl,&kbset,0)){
										if (Subst.buffer!=NULL){
											MYFREE(Subst.buffer);
										}
										kbset.status=NOMEMORY;
										return;
									}
								}
								break;
						}

						/* Prepare next iteration. */
						if (untrm<othtrm){
							ptr3=untrm;
							untrm=othtrm;
							othtrm=ptr3;
						} else {
							untrm=NULL;
						}
					}
				}

				/* Prepare next iteration. */
				if (mxtrm>nonmxtrm){
					mxtrm=NULL;
				} else if (SELECTED&nonmxtrm[0]){
						ptr3=nonmxtrm;
						nonmxtrm=mxtrm;
						mxtrm=ptr3;
				} else {
					mxtrm=NULL;
				}
			}
		}
	}

	/* Free substitution memory eand return. */
	if (Subst.buffer!=NULL){
		MYFREE(Subst.buffer);
	}
	return;
} /* EqualityFactoring */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Infer factors of a clause)  OCJ
 *
 *    This function infers as many full factors as possible from
 *    a given clause. Only selected literals are factored but each
 *    selected literal is factored with as many other literals as
 *    possible.
 *
 *    On return the status field of the KB structure is set with
 *    the values NOMEMORY, UNSATISFIABLE, UNKNOWN or TIMEOUT when
 *    appropriate.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to clause with which inferences will be made.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *
 *--------------------------------------------------------------*/
void Factor(cmprefix *clause){
	auto subst Subst;                                    /* Substitution */
	auto cmprefix *inferred;                             /* Pointer to inferred clause */
	auto binprefix *binp;                                /* Auxiliary binary prefix pointer */
	auto uint8_t *ptr1,*ptr2;                            /* Auxiliary pointers */
	auto int32_t ii,jj,kk,mm,nn;                         /* Auxiliary */
	#ifdef DEBUGCODE
	auto char *ptr10;                                    /* Buffer for clause in text form. */
	#endif

	/* Initialize substitution. */
	Subst.buffer=NULL;

	/* Allocate storage for inferred clause and copy original clause. */
	if (NULL==(inferred=MYALLOC(sizeof(cmprefix)))){
		kbset.status=NOMEMORY;
	}
	nn=binpsize+clause->part2.bin->size+CLAUSE_CHUNK_SIZE;
	if (NULL==((inferred->part2.bin)=MYALLOC(nn))){
		MYFREE(inferred);
		kbset.status=NOMEMORY;
	}
	memcpy(inferred->part2.bin,clause->part2.bin,binpsize+clause->part2.bin->size);
	inferred->part2.bin->clause=inferred;
	inferred->part2.bin->oriented=0;

	/* Loop through clause literals. */
	for (ptr1=&inferred->part2.bin->formula[0],ii=jj=0;ptr1[0]!=UNITEND;
			ptr1=NextItem(ptr1,OVERSUBTERMS),jj++){

		/* Literal is selected. */
		if (ptr1[0]&SELECTED){

			/* Check timeout and solution by other process. */
			if (procctl->status==TIMEOUT){
				if (Subst.buffer!=NULL){
					MYFREE(Subst.buffer);
				}
				MYFREE(inferred->part2.bin);
				MYFREE(inferred);
				kbset.status=TIMEOUT;
				return;
			}
			if ((procctl->status==UNSATISFIABLE)||(procctl->status==SATISFIABLE)){
				if (Subst.buffer!=NULL){
					MYFREE(Subst.buffer);
				}
				MYFREE(inferred->part2.bin);
				MYFREE(inferred);
				kbset.status=UNKNOWN;
				return;
			}
			if (procctl->status==NOMEMORY){
				if (Subst.buffer!=NULL){
					MYFREE(Subst.buffer);
				}
				MYFREE(inferred->part2.bin);
				MYFREE(inferred);
				kbset.status=procctl->status;
				return;
			}

			/* Loop through literals following ptr1 literal. */
			for (ptr2=NextItem(ptr1,OVERSUBTERMS),kk=jj+1;ptr2[0]!=UNITEND;
					ptr2=(mm?ptr2:NextItem(ptr2,OVERSUBTERMS)),kk=(mm?kk:kk+1)){
				mm=0;

				/* Literal is selected. */
				if (ptr2[0]&SELECTED){

					/* Check unification. */
					switch (Unify(ptr1,ptr2,&Subst,INITSUBST)){

						/* Not enough memory. */
						case NOMEMORY:
							if (Subst.buffer!=NULL){
								MYFREE(Subst.buffer);
							}
							MYFREE(inferred->part2.bin);
							MYFREE(inferred);
							kbset.status=NOMEMORY;
							return;
							break;

						/* Literals unify. */
						case 1:

							/* Delete second literal and apply substitution. */
							DeleteLiteral(inferred->part2.bin,ptr2);
							if (NOMEMORY==ApplySubst2Clause(inferred,&nn,&Subst,0)){
								if (Subst.buffer!=NULL){
									MYFREE(Subst.buffer);
								}
								MYFREE(inferred->part2.bin);
								MYFREE(inferred);
								kbset.status=NOMEMORY;
								return;
							}

							/* Indicate that a factor was obtained, update ptr2 */
							/* and indicate to stay in the same literal of inner loop. */
							ii=1;
							ptr1=GetLiteralPtr(inferred,jj);
							ptr2=GetLiteralPtr(inferred,kk);
							mm=1;
							break;
					}
				}
			}
		}
	}

	/* There was a factor. */
	if (ii){

		/* Reallocate clause to exact size. */
		if (NULL==(binp=MYREALLOC(inferred->part2.bin,binpsize+inferred->part2.bin->size))){
			if (Subst.buffer!=NULL){
				MYFREE(Subst.buffer);
			}
			MYFREE(inferred->part2.bin);
			MYFREE(inferred);
			kbset.status=NOMEMORY;
			return;
		}
		inferred->part2.bin=binp;

		/* Add assertions to inferred clause. */
		if (NOMEMORY==AddAssertions(clause->part2.bin,NULL,inferred->part2.bin)){
			if (Subst.buffer!=NULL){
				MYFREE(Subst.buffer);
			}
			MYFREE(inferred->part2.bin);
			MYFREE(inferred);
			kbset.status=NOMEMORY;
			return;
		}

		/* Debug. */
		#ifdef DEBUGCODE
		ptr10=Decompile(inferred->part2.bin);
		#ifdef VERBOSE
		if ((active_cores==1)&&(verbfrom<=kbset.nformulas)&&(verbto>=kbset.nformulas)){
			printf("Factoring result [%ld]:\n%s\n",kbset.nformulas,ptr10);
		}
		#endif
		MYFREE(ptr10);
		#endif

		/* Update statistics and set clause inference information. */
		kbset.prstats.factors++;
		inferred->parent1=clause;
		inferred->parent2=NULL;
		inferred->part2.bin->agedist=clause->part2.bin->agedist+1;
		inferred->part2.bin->sinedist=clause->part2.bin->sinedist;
		inferred->inference=FACTORING;

		/* Add inferred clause to KB as unprocessed without factors. */
		inferred->flags=UNPROC|NOFACTORS;
		if (NOMEMORY==AddBinClause2KB(inferred,&kbset,0)){
			kbset.status=NOMEMORY;
		}

	/* If there was no factor then free inferred clause storage. */
	} else {
		MYFREE(inferred->part2.bin);
		MYFREE(inferred);
	}

	/* Free substitution memory and return. */
	if (Subst.buffer!=NULL){
		MYFREE(Subst.buffer);
	}
	return;
} /* Factor */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Build goal transformation clause for ground goals)  OCJ
 *
 *    This function builds the definition folding clause based
 *    on the definitions made by GoalDefinition() function for
 *    unit equality problems that have one ground negated
 *    conjecture that is an inequality.
 *
 *    This is based on the Vampire implementation of the goal
 *    transformation described in the document "Twee: An Equational
 *    Theorem Prover (System Description)" by Nicholas Smallbone.
 *
 *
 *  ARGUMENTS: None
 *
 *
 *  RETURNS:
 *
 *    0 -> Process completed successfully.
 *    1 -> There was a NOMEMORY condition or the maximum recursive
 *         calls depth was reached in GoalDefinition() function.
 *         If a NOMEMORY condition is produced then procctl->status
 *         is set to NOMEMORY, otherwise it is not changed.
 *
 *--------------------------------------------------------------*/
int32_t DefinitionFolding(){
	auto cmprefix *newcl;                             /* Pointer to clause to be build */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4;             /* Auxiliary pointers */

	/* Allocate storage for inferred clause. */
	if (NULL==(newcl=MYALLOC(sizeof(cmprefix)))){
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return(1);
	}
	if ((newcl->part2.bin=MYALLOC(binpsize+1+(3*(1+sizeof(symbol)))))==NULL){
		MYFREE(newcl);
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return(1);
	}

	/* Build the known part of newcl cmprefix and binprefix structures. */
	newcl->flags=UNPROC;
	newcl->inference=DEFINTNFOLDING;
	newcl->parent1=ueqgoal;
	newcl->parent2=NULL;
	newcl->flags=UNPROC|HORN;
	newcl->part2.bin->agedist=0;
	newcl->part2.bin->sinedist=0;
	newcl->part2.bin->clause=newcl;
	newcl->part2.bin->ovly.asserts=NULL;
	newcl->part2.bin->lockasserts=NULL;
	newcl->part2.bin->literals=1;
	newcl->part2.bin->size=1+(3*(1+sizeof(symbol)));
	newcl->part2.bin->oriented=0;

	/* Build the known part of the definition folding formula. */
	/* This is just the negated equality symbol. */
	newcl->part2.bin->formula[0]=(EQUALITY|NEGATED);
	((symbol *)&newcl->part2.bin->formula[1])->symbol=&equkey;
	ptr3=NextItem(&newcl->part2.bin->formula[0],IMMED);

	/* Loop through the inequality root terms of the original ground goal. */
	for (ptr1=NextItem(&ueqgoal->part2.bin->formula[0],IMMED),ptr2=NextItem(&ueqgoal->part2.bin->formula[0],OVERSUBTERMS);
			ptr1<ptr2;ptr1=NextItem(ptr1,OVERSUBTERMS)){

		/* The term has arity 0. Add the term to newcl. */
		if (((symbol *)&ptr1[1])->symbol->arity==0){
			ptr3[0]=FUNCTION;
			((symbol *)&ptr3[1])->symbol=((symbol *)&ptr1[1])->symbol;
			ptr3=NextItem(ptr3,IMMED);

		/* The term has arity bigger than 0. */
		} else {

			/* Build the definition for this root term. */
			if (NULL==(ptr4=GoalDefinition(ptr1,0))){
				MYFREE(newcl->part2.bin);
				MYFREE(newcl);
				return(1);
			}

			/* Add the ptr4 symbol to the goal inequality being built. */
			ptr3[0]=FUNCTION;
			((symbol *)&ptr3[1])->symbol=((symbol *)&ptr4[1])->symbol;
			ptr3=NextItem(ptr3,IMMED);
		}
	}

	/* Set end of formula byte and update formula parameters. */
	ptr3[0]=UNITEND;
	UpdateParams2(&newcl->part2.bin->formula[0]);

	/* Add definition folding clause to mainkb as UNPROC and return. */
	if (NOMEMORY==AddBinClause2KBUEQ(newcl,&mainkb,0)){
		MYFREE(newcl->part2.bin);
		MYFREE(newcl);
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return(1);
	}
	return(0);
} /* DefinitionFolding */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Build goal transformation clause for non ground goals)  OCJ
 *
 *    This function builds the definition folding clause based
 *    on the definitions made by GoalDefinition2() function for
 *    unit equality problems that have one non ground negated
 *    conjecture that is an inequality.
 *
 *    This is based on the Vampire implementation of the goal
 *    transformation described in the document "Twee: An Equational
 *    Theorem Prover (System Description)" by Nicholas Smallbone.
 *
 *
 *  ARGUMENTS:
 *
 *    type: 1 if first ueqgoal root term is definable and possibly also
 *          the second root term.
 *    type: 2 if first ueqgoal root term is not definable but the second
 *          root term is definable.
 *
 *  RETURNS:
 *
 *    0 -> Process completed successfully.
 *    1 -> There was a NOMEMORY condition or the maximum recursive
 *         calls depth was reached in GoalDefinition() function.
 *         If a NOMEMORY condition is produced then procctl->status
 *         is set to NOMEMORY, otherwise it is not changed.
 *
 *--------------------------------------------------------------*/
int32_t DefinitionFolding2(int32_t type){
	auto cmprefix *newcl;                             /* Pointer to clause to be build */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4;             /* Auxiliary pointers */
	auto binprefix *ptr5;                             /* Auxiliary pointer */
	auto int32_t ii;                                  /* Auxiliary */

	/* Allocate storage for inferred clause. */
	if (NULL==(newcl=MYALLOC(sizeof(cmprefix)))){
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return(1);
	}
	if ((newcl->part2.bin=MYALLOC(binpsize+ueqgoal->part2.bin->size))==NULL){
		MYFREE(newcl);
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return(1);
	}

	/* Build the known part of newcl cmprefix and binprefix structures. */
	newcl->flags=UNPROC;
	newcl->inference=DEFINTNFOLDING;
	newcl->parent1=ueqgoal;
	newcl->parent2=NULL;
	newcl->flags=UNPROC|HORN;
	newcl->part2.bin->agedist=0;
	newcl->part2.bin->sinedist=0;
	newcl->part2.bin->clause=newcl;
	newcl->part2.bin->ovly.asserts=NULL;
	newcl->part2.bin->lockasserts=NULL;
	newcl->part2.bin->literals=1;
	newcl->part2.bin->size=1+(3*(1+sizeof(symbol)));
	newcl->part2.bin->oriented=0;

	/* Build the known part of the definition folding formula. */
	/* This is just the negated equality symbol. */
	newcl->part2.bin->formula[0]=(EQUALITY|NEGATED);
	((symbol *)&newcl->part2.bin->formula[1])->symbol=&equkey;
	ptr3=NextItem(&newcl->part2.bin->formula[0],IMMED);

	/* Loop through the inequality root terms of the original non ground goal. */
	for (ptr2=NextItem(ptr1=NextItem(&ueqgoal->part2.bin->formula[0],IMMED),OVERSUBTERMS),ii=0;ii<2;ptr1=ptr2,ii++){

		/* The term is not definable. Copy it to the new clause. */
		if (((ii==0)&&(type==2))||((ii==type)&&(0==IsDefinable(ptr1)))){
			memcpy(ptr3,ptr1,(ii==0?ptr2:NextItem(ptr1,OVERSUBTERMS))-ptr1);
			ptr3=NextItem(ptr3,OVERSUBTERMS);

		/* The term is definable. */
		} else {

			/* Build the definition for this root term. */
			if (NULL==(ptr5=GoalDefinition2(ptr1,NULL,0))){
				MYFREE(newcl->part2.bin);
				MYFREE(newcl);
				return(1);
			}

			/* Add the first root term in ptr5 equality to the goal inequality being built. */
			ptr4=NextItem(&ptr5->formula[0],IMMED);
			memcpy(ptr3,ptr4,(NextItem(ptr4,OVERSUBTERMS))-ptr4);
			ptr3=NextItem(ptr3,OVERSUBTERMS);
		}
	}

	/* Set end of formula byte and update formula parameters. */
	ptr3[0]=UNITEND;
	UpdateParams2(&newcl->part2.bin->formula[0]);

	/* Set binprefix size field in new clause and adjust the formula size to the exact size. */
	newcl->part2.bin->size=1+(ptr3-&newcl->part2.bin->formula[0]);
	if (NULL==(ptr5=MYREALLOC(newcl->part2.bin,binpsize+newcl->part2.bin->size))){
		MYFREE(newcl->part2.bin);
		MYFREE(newcl);
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return(1);
	}
	newcl->part2.bin=ptr5;

	/* Add definition folding clause to mainkb as UNPROC and return. */
	if (NOMEMORY==AddBinClause2KBUEQ(newcl,&mainkb,0)){
		MYFREE(newcl->part2.bin);
		MYFREE(newcl);
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return(1);
	}
	return(0);
} /* DefinitionFolding2 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Build goal transformation definitions for ground goals)  OCJ
 *
 *    This function builds the goal transformation definitions
 *    for unit equality problems that have one ground negated
 *    conjecture that is an inequality.
 *
 *    This is based on the Vampire implementation of the goal
 *    transformation described in the document "Twee: An Equational
 *    Theorem Prover (System Description)" by Nicholas Smallbone.
 *
 *    This call is recursive. To prevent excessive nested calls
 *    the maximum recursive depth is limited to MAXRECURSIVECALLS.
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to a sub-term of ground inequality negated
 *             conjecture with arity bigger than 0.
 *    depth: Depth of the call: 0 for first call and so on.
 *
 *  RETURNS:
 *
 *    - NULL in case of a NOMEMORY condition or if the maximum
 *      recursive depth is reached. If a NOMEMORY condition
 *      is produced then procctl->status is set to NOMEMORY.
 *    - Otherwise this function returns a pointer to the first
 *      root term of the equality generated for the definition.
 *      This root term is just an arity 0 function.
 *
 *--------------------------------------------------------------*/
uint8_t *GoalDefinition(uint8_t *formula,int32_t depth){
	auto cmprefix *newcl;                             /* Pointer to clause to be build */
	auto char smbname[40];                            /* New defined symbol name */
	auto hashchain *defsymbol;                        /* Pointer to new defined symbol */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4;             /* Auxiliary pointers */
	auto binprefix *ptr5;                             /* Auxiliary pointer */
	auto cmprefix *ptr6;                              /* Auxiliary pointer */

	/* Check if an equal formula has been already processed. */
	for (ptr6=mainkb.lstunproc;;ptr6=ptr6->prev){
		if (ptr6->inference==DEFINTNFOLDING){
			continue;
		}
		if (ptr6->inference!=GOALDEFINITION){
			break;
		}
		ptr1=NextItem(&ptr6->part2.bin->formula[0],IMMED);
		ptr2=NextItem(ptr1,OVERSUBTERMS);
		if (IsEqual2(formula,ptr2)){
			return(ptr1);
		}
	}

	/* Return if maximum depth has been reached. */
	if (depth>MAXRECURSIVECALLS){
		return(NULL);
	}

	/* Allocate storage for inferred clause. */
	if (NULL==(newcl=MYALLOC(sizeof(cmprefix)))){
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return(NULL);
	}
	if ((newcl->part2.bin=MYALLOC(binpsize+1+(2*(1+sizeof(symbol)))+(NextItem(formula,OVERSUBTERMS)-formula)))==NULL){
		MYFREE(newcl);
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return(NULL);
	}

	/* Build name of new symbol defined in this function instance. */
	strcpy(smbname,FNCPREFIX);
	sprintf(&smbname[strlen(FNCPREFIX)],"%lu",numfunc+functid);
	strcat(smbname,FNCPOSTFIX);
	numfunc++;

	/* Create defined function symbol, set FROMCONJECTURE flag in */
	/* the symbol type field and set symbol agedist field to 0.  */
	if (NULL==(defsymbol=CreateGlobalSymbol(smbname,0,FUNCTION))){
		MYFREE(newcl);
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return(NULL);
	}
	defsymbol->type|=FROMCONJECTURE;
	defsymbol->sinedist=0;

	/* Build the known part of newcl cmprefix and binprefix. */
	newcl->flags=UNPROC;
	newcl->inference=GOALDEFINITION;
	newcl->parent1=newcl->parent2=NULL;
	newcl->flags=UNPROC|HORN;
	newcl->part2.bin->agedist=0;
	newcl->part2.bin->sinedist=0;
	newcl->part2.bin->clause=newcl;
	newcl->part2.bin->ovly.asserts=NULL;
	newcl->part2.bin->lockasserts=NULL;
	newcl->part2.bin->literals=1;
	newcl->part2.bin->oriented=0;

	/* Build the known part of the definition formula. This is just */
	/* the equality symbol, the first root term corresponding to the */
	/* the defined function symbol created above and the second root */
	/* term which is the term in formula but without its arguments. */
	/* This second root term has arguments. */
	ptr1=&newcl->part2.bin->formula[0];
	ptr1[0]=EQUALITY;
	((symbol *)&ptr1[1])->symbol=&equkey;
	ptr1=NextItem(ptr1,IMMED);
	ptr1[0]=FUNCTION;
	((symbol *)&ptr1[1])->symbol=defsymbol;
	ptr1=NextItem(ptr1,IMMED);
	ptr1[0]=FUNCTION;
	((symbol *)&ptr1[1])->symbol=((symbol *)&formula[1])->symbol;
	ptr3=NextItem(&ptr1[0],IMMED);

	/* Loop through each term that is an argument of the main term in formula. */
	for (ptr1=NextItem(formula,IMMED),ptr2=NextItem(formula,OVERSUBTERMS);
			ptr1<ptr2;ptr1=NextItem(ptr1,OVERSUBTERMS)){

		/* The term has arity 0. Add the term to newcl. */
		if (((symbol *)&ptr1[1])->symbol->arity==0){
			ptr3[0]=FUNCTION;
			((symbol *)&ptr3[1])->symbol=((symbol *)&ptr1[1])->symbol;
			ptr3=NextItem(ptr3,IMMED);

		/* The term has arity bigger than 0. */
		} else {

			/* Perform recursive call for the corresponding definition */
			/* related to current term. */
			if (NULL==(ptr4=GoalDefinition(ptr1,depth+1))){
				MYFREE(newcl->part2.bin);
				MYFREE(newcl);
				return(NULL);
			}

			/* Add the ptr4 sub-term to the equality being built. */
			ptr3[0]=FUNCTION;
			((symbol *)&ptr3[1])->symbol=((symbol *)&ptr4[1])->symbol;
			ptr3=NextItem(ptr3,IMMED);
		}
	}

	/* Set end of formula byte and update formula parameters. */
	ptr3[0]=UNITEND;
	ptr1=&newcl->part2.bin->formula[0];
	UpdateParams2(ptr1);

	/* Adjust the formula size to the exact size. */
	newcl->part2.bin->size=1+(NextItem(ptr1,OVERSUBTERMS)-ptr1);
	if (NULL==(ptr5=MYREALLOC(newcl->part2.bin,binpsize+newcl->part2.bin->size))){
		MYFREE(newcl->part2.bin);
		MYFREE(newcl);
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return(NULL);
	}
	newcl->part2.bin=ptr5;

	/* Add clause to mainkb as UNPROC and return. */
	if (NOMEMORY==AddBinClause2KBUEQ(newcl,&mainkb,0)){
		MYFREE(newcl->part2.bin);
		MYFREE(newcl);
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return(NULL);
	}
	return(NextItem(&newcl->part2.bin->formula[0],IMMED));
} /* GoalDefinition */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Build goal transformation definitions for non ground goals)  OCJ
 *
 *    This function builds the goal transformation definitions
 *    for unit equality problems that have one non ground negated
 *    conjecture that is an inequality.
 *
 *    This is based on the Vampire implementation of the goal
 *    transformation described in the document "Twee: An Equational
 *    Theorem Prover (System Description)" by Nicholas Smallbone.
 *
 *    This call is recursive. To prevent excessive nested calls
 *    the maximum recursive depth is limited to MAXRECURSIVECALLS.
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to a sub-term of ground inequality negated
 *             conjecture with arity bigger than 0 and at least
 *             one argument that is not a variable.
 *    varflags: Pointer to a set of integers. This is NULL for the
 *              first (depth==0) call and allocated in that instance.
 *              Each item varflags[ii] is set to 1 if the ii variable
 *              is a sub-term of the term pointed by formula or 0
 *              otherwise. This is used when building the first root
 *              term of the definition equality.
 *    depth: Depth of the call: 0 for first call and so on.
 *
 *  RETURNS:
 *
 *    - NULL in case of a NOMEMORY condition or if the maximum
 *      recursive depth is reached. If a NOMEMORY condition
 *      is produced then procctl->status is set to NOMEMORY.
 *    - Otherwise this function returns a pointer to the binprefix
 *      structure of the equality generated for the definition.
 *
 *--------------------------------------------------------------*/
binprefix *GoalDefinition2(uint8_t *formula,uint8_t *varflags,int32_t depth){
	auto cmprefix *newcl;                             /* Pointer to clause to be build */
	auto char smbname[40];                            /* New defined symbol name */
	auto hashchain *defsymbol;                        /* Pointer to new defined symbol */
	auto uint8_t *ptr1,*ptr2,*ptr3,*ptr4;             /* Auxiliary pointers */
	auto binprefix *ptr5;                             /* Auxiliary pointer */
	auto cmprefix *ptr6;                              /* Auxiliary pointer */
	auto int32_t ii,jj,kk;                            /* Auxiliary */

	/* Check if an equal formula has been already processed. */
	for (ptr6=mainkb.lstunproc;;ptr6=ptr6->prev){
		if (ptr6->inference==DEFINTNFOLDING){
			continue;
		}
		if (ptr6->inference!=GOALDEFINITION){
			break;
		}
		ptr1=NextItem(&ptr6->part2.bin->formula[0],IMMED);
		ptr2=NextItem(ptr1,OVERSUBTERMS);
		if (IsEqual(formula,ptr2,0)){
			return(ptr6->part2.bin);
		}
	}

	/* Return if maximum depth has been reached. */
	if (depth>MAXRECURSIVECALLS){
		return(NULL);
	}

	/* Allocate storage for varflags set. */
	if (depth==0){
		if (NULL==(varflags=malloc(1+ueqgoal->part2.bin->maxvarnb))){
			sem_wait(&procctl->procctl);
			if (procctl->status==RUNNING){
				procctl->status=NOMEMORY;
			}
			sem_post(&procctl->procctl);
			return(NULL);
		}
	}

	/* Allocate storage for inferred clause. */
	if (NULL==(newcl=MYALLOC(sizeof(cmprefix)))){
		if (depth==0){
			free(varflags);
		}
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return(NULL);
	}
	if ((newcl->part2.bin=MYALLOC(binpsize+2+sizeof(symbol)+(2*(NextItem(formula,OVERSUBTERMS)-formula))))==NULL){
		if (depth==0){
			free(varflags);
		}
		MYFREE(newcl);
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return(NULL);
	}

	/* Build name of new symbol defined in this function instance. */
	strcpy(smbname,FNCPREFIX);
	sprintf(&smbname[strlen(FNCPREFIX)],"%lu",numfunc+functid);
	strcat(smbname,FNCPOSTFIX);
	numfunc++;

	/* Create defined function symbol, set FROMCONJECTURE flag in */
	/* the symbol type field and set symbol agedist field to 0.  */
	if (NULL==(defsymbol=CreateGlobalSymbol(smbname,0,FUNCTION))){
		if (depth==0){
			free(varflags);
		}
		MYFREE(newcl);
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return(NULL);
	}
	defsymbol->type|=FROMCONJECTURE;
	defsymbol->sinedist=0;

	/* Build the known part of newcl cmprefix and binprefix. */
	newcl->flags=UNPROC;
	newcl->inference=GOALDEFINITION;
	newcl->parent1=newcl->parent2=NULL;
	newcl->flags=UNPROC|HORN;
	newcl->part2.bin->agedist=0;
	newcl->part2.bin->sinedist=0;
	newcl->part2.bin->clause=newcl;
	newcl->part2.bin->ovly.asserts=NULL;
	newcl->part2.bin->lockasserts=NULL;
	newcl->part2.bin->literals=1;
	newcl->part2.bin->oriented=0;

	/* Add the known part of the definition formula. This is just */
	/* the equality symbol and the symbol of the first root term */
	/* created above. This first root term has arguments that */
	/* are variables. */
	ptr1=&newcl->part2.bin->formula[0];
	ptr1[0]=EQUALITY;
	((symbol *)&ptr1[1])->symbol=&equkey;
	ptr1=NextItem(ptr1,IMMED);
	ptr1[0]=FUNCTION;
	((symbol *)&ptr1[1])->symbol=defsymbol;
	ptr1=NextItem(ptr1,IMMED);

	/* Set the varflags items (see description in global comments). */
	memset(varflags,0,kk=1+ueqgoal->part2.bin->maxvarnb);
	for (ptr2=NextItem(&formula[0],IMMED),ptr3=NextItem(&formula[0],OVERSUBTERMS);ptr2<ptr3;ptr2=NextItem(ptr2,IMMED)){
		if (VARIABLE&ptr2[0]){
			varflags[*((uint16_t *)&ptr2[1])]=1;
		}
	}

	/* Add the arguments of the first root term, all of them are variables. */
	/* These arguments are the variables with varflags[] set to 1. */
	/* Set defsymbol arity (which is the first root symbol arity) after all */
	/* this is done. */
	for (ii=jj=0;ii<kk;ii++){
		if (varflags[ii]){
			jj++;
			ptr1[0]=VARIABLE;
			*((uint16_t *)&ptr1[1])=ii;
			ptr1+=3; /* ===> BE CAREFULL WITH THIS. It is faster than NextItem but may not be valid if architecture changes. */
		}
	}
	defsymbol->arity=jj;

	/* Add the known part of the second root term of the definition equality. */
	/* This is just the first symbol in formula. */
	ptr1[0]=FUNCTION;
	((symbol *)&ptr1[1])->symbol=((symbol *)&formula[1])->symbol;
	ptr1=NextItem(&ptr1[0],IMMED);

	/* Loop through each term that is an argument of the main term in formula. */
	for (ptr2=NextItem(formula,IMMED),ptr3=NextItem(formula,OVERSUBTERMS);
			ptr2<ptr3;ptr2=NextItem(ptr2,OVERSUBTERMS)){

		/* The term is a variable. Add the term to newcl. */
		if (VARIABLE&ptr2[0]){
			memcpy(ptr1,ptr2,3);
			ptr1+=3; /* ===> BE CAREFULL WITH THIS. It is faster than NextItem but may not be valid if architecture changes. */

		/* The term is not a variable and it is definable. */
		} else if (IsDefinable(ptr2)){

			/* Perform recursive call for the corresponding definition */
			/* related to current term. */
			if (NULL==(ptr5=GoalDefinition2(ptr2,varflags,depth+1))){
				if (depth==0){
					free(varflags);
				}
				MYFREE(newcl->part2.bin);
				MYFREE(newcl);
				return(NULL);
			}

			/* Add the first root term in the ptr5 equality to newcl. */
			ptr4=NextItem(&ptr5->formula[0],IMMED);
			memcpy(ptr1,ptr4,NextItem(ptr4,OVERSUBTERMS)-ptr4);
			ptr1=NextItem(ptr1,OVERSUBTERMS);

		/* The term is not definable. Just add it to the equality being built */
		} else {
			memcpy(ptr1,ptr2,NextItem(ptr2,OVERSUBTERMS)-ptr2);
			ptr1=NextItem(ptr1,OVERSUBTERMS);
		}
	}

	/* Free varflags set. */
	if (depth==0){
		free(varflags);
	}

	/* Set end of formula byte and update formula parameters. */
	ptr1[0]=UNITEND;
	ptr1=&newcl->part2.bin->formula[0];
	UpdateParams2(ptr1);

	/* Adjust the formula size to the exact size. */
	newcl->part2.bin->size=1+(NextItem(ptr1,OVERSUBTERMS)-ptr1);
	if (NULL==(ptr5=MYREALLOC(newcl->part2.bin,binpsize+newcl->part2.bin->size))){
		MYFREE(newcl->part2.bin);
		MYFREE(newcl);
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return(NULL);
	}
	newcl->part2.bin=ptr5;

	/* Add clause to mainkb as UNPROC and return. */
	if (NOMEMORY==AddBinClause2KBUEQ(newcl,&mainkb,0)){
		if (depth==0){
			free(varflags);
		}
		MYFREE(newcl->part2.bin);
		MYFREE(newcl);
		sem_wait(&procctl->procctl);
		if (procctl->status==RUNNING){
			procctl->status=NOMEMORY;
		}
		sem_post(&procctl->procctl);
		return(NULL);
	}
	return(newcl->part2.bin);
} /* GoalDefinition2 */
