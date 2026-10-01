/*
 ============================================================================
 Name        : clausify.c
 Author      : Oscar Contreras
 Version     :
 Copyright   : Copyright (C) 2015-2016 Oscar Contreras. All rights reserved.
 Description : First order logic theorem prover in C
 ============================================================================
 */
/****************************************************************
*
*                 clausify (convert formulas to CNF)
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
 *    This module include routines necessary to convert input formulas to
 *    conjunctive normal form (CNF). The method followed in this module
 *    is based on the document "Computing small clause normal forms"
 *    by Andreas Nonnengart and Christoph Weidenbach.
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
 *  DESCRIPTION: (Text formula pre-processing)  OCJ
 *
 *    This function pre-processes a text clause. The pre-processing
 *    includes:
 *    - Formula conversion to linked list format.
 *    - Adding redundant brackets so that there is no need to care
 *      for connectives precedence.
 *    - Drop implications, reverse implications, AND, OR, NOR and
 *      NAND connectives.
 *    - Formula renaming.
 *    - Drop equivalences and XOR connectives.
 *    - Move negations inward.
 *    - Miniscoping.
 *    - Skolemization.
 *    - $true and $false simplifications
 *    - Rename variables.
 *    - Remove redundant brackets.
 *    - Convert formula to CNF.
 *
 *    The resulting clause is converted to text again and returned
 *    in the buffer provided as input parameter.
 *
 *    It is assumed that the text clause is an input formula (including
 *    negated conjectures) or a predicate definition formula resulting
 *    from a formula renaming.
 *
 *    Additional text formulas may be generated during the pre-processing
 *    as a result of the intermediate steps. They are added to the main KB
 *    to document the pre-processing inference process.
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to input text formula.
 *    result: Address of pointer to buffer where the result will
 *            be stored.
 *    size: Pointer to buffer size in bytes.
 *    flag: If not zero then perform miniscoping, otherwise don't
 *          perform miniscoping.
 *
 *  RETURNS:
 *
 *    Pointer to cmprefix structure of last formula obtained in the pre-processing
 *    before variable renaming and quantifiers removal or NULL if not enough
 *    memory or timeout.
 *
 *--------------------------------------------------------------*/
cmprefix *PreprocessFormula(cmprefix *formula,char **result,int32_t *size,int32_t flag){
	auto char *ptr1;                                    /* Auxiliary pointer */
	auto cmprefix *ptr2,*ptr4;                          /* Auxiliary pointers */
	auto lkedfitem *ptr3;                               /* Auxiliary pointer */
	auto char *cnvsntnc2;                               /* Pointer to converted formulas */
	auto lkedfitem *topitem;                            /* Pointer to top item of formula in linked list format */
	auto int32_t size2;                                 /* Size in bytes of cnvsntnc2 buffer */
	auto uint32_t ii;                                   /* Auxiliary */
	auto int32_t jj;                                    /* Auxiliary */

	/* Reallocate space for converted formulas if necessary */
	/* and copy formula to buffer. */
	jj=2*strlen(formula->part2.text)+STR_CHUNK_SIZE;
	if (jj>(*size)){
		if (NULL==(ptr1=MYREALLOC(*result,jj))){
			return(NULL);
		}
		*result=ptr1;
		*size=jj;
	}

	/* Allocate auxiliary space for converted formulas. */
	size2=*size;
	if (NULL==(cnvsntnc2=MYALLOC(size2))){
		procctl->status=NOMEMORY;
		return(NULL);
	}

	/* Convert text formula to linked list format. */
	if (NULL==(topitem=Text2LinkedList(formula->part2.text))){
		procctl->status=NOMEMORY;
		MYFREE(cnvsntnc2);
		return(NULL);
	}

	/* Set FROMCONJECTURE, FROMAXIOM and FROMNOTAXIOM flags in clause symbols */
	/* as appropriate. Also initialize sinedist field of symbols in formula. */
	switch (formula->inference){
		case AXIOM:
			ii=FROMAXIOM;
			pbmflags|=EXISTAXIOMLIKE;
			break;
		case HYPOTHESIS:
		case DEFINITION:
		case ASSUMPTION:
		case LEMMA:
		case THEOREM:
		case COROLLARY:
		case PLAIN:
			ii=FROMNOTAXIOM;
			pbmflags|=EXISTAXIOMLIKE;
			break;
		case NEGCONJ:
			ii=FROMCONJECTURE;
			break;
		default:
			ii=0;
			break;
	}
	if (ii){
		for (ptr3=topitem;ptr3!=NULL;ptr3=ptr3->next){
			if (((NAMEDPRED|NAMEDFUNC)&ptr3->flags)&&(0==((TRUEPRED|FALSEPRED)&ptr3->flags))){
				ptr3->ptr.symbol->type|=ii;
				if (ii&FROMCONJECTURE){
					ptr3->ptr.symbol->sinedist=0;
				}
			}
		}
	}

	/* Pre-nnf conversion: eliminate implications, reverse implications, */
	/* NOR and NAND connectives. */
	if (NULL==(ptr2=Convert2PreNNF(&topitem,formula,&cnvsntnc2,&size2))){
		procctl->status=NOMEMORY;
		MYFREE(cnvsntnc2);
		FreeLkedLstFormula(topitem);
		return(NULL);
	}

	/* Loop until no renaming is done. The loop is needed because the equivalence */
	/* connective introduced by renaming may produce the need of additional renaming . */
	/* There is no need to set procctl->status in case of error. */
	do {
		/* Try to rename formula. */
		if (formula->inference==NEGCONJ){
			ptr2->flags|=FROMCONJECTURE;
		} else {
			ptr2->flags|=(FROMCONJECTURE&formula->flags);
		}
		ptr4=ptr2;
		if (NULL==(ptr2=FormulaRenaming(&topitem,ptr2,&cnvsntnc2,&size2))){
			MYFREE(cnvsntnc2);
			FreeLkedLstFormula(topitem);
			return(NULL);
		}
	} while (ptr4!=ptr2);

	/* Perform nnf conversion: eliminate equivalences and XOR connectives, */
	/* indent negations and remove redundant brackets. */
	if (NULL==(ptr2=Convert2NNF(&topitem,ptr2,&cnvsntnc2,&size2))){
		procctl->status=NOMEMORY;
		MYFREE(cnvsntnc2);
		FreeLkedLstFormula(topitem);
		return(NULL);
	}

	/* Miniscope formula. There is no need to set procctl->status */
	/* in case of error. */
	if (flag){
		if (NULL==(ptr2=MiniscopeFormula(&topitem,ptr2,&cnvsntnc2,&size2))){
			MYFREE(cnvsntnc2);
			FreeLkedLstFormula(topitem);
			return(NULL);
		}
	}

	/* Skolemize formula. */
	if (NULL==(ptr2=Skolemize(&topitem,ptr2,&cnvsntnc2,&size2))){
		procctl->status=NOMEMORY;
		MYFREE(cnvsntnc2);
		FreeLkedLstFormula(topitem);
		return(NULL);
	}

	/* Simplify $true and $false in formula and return */
	/* if result is conclusive. */
	ptr2=TFSimplify(&topitem,ptr2,&cnvsntnc2,&size2);
	if ((TRUEPRED|FALSEPRED)&topitem->flags){
		if (TRUEPRED&topitem->flags){
			strcpy(cnvsntnc2,"$true");
		} else {
			strcpy(cnvsntnc2,"$false");
		}
		MYFREE(ptr2->part2.text);
		ptr2->part2.text=cnvsntnc2;
		FreeLkedLstFormula(topitem);
		return(ptr2);
	}

	/* Rename variables. */
	RenameVariables(topitem);

	/* Drop quantifiers. */
	DropQuantifiers(&topitem);

	/* Remove redundant brackets. */
	RemoveRedundantBrackets(&topitem);

	/* Convert formula in linked list format to a text formula. */
	if (NOMEMORY==LinkedList2Text(topitem,&cnvsntnc2,&size2,1)){
		procctl->status=NOMEMORY;
		MYFREE(cnvsntnc2);
		FreeLkedLstFormula(topitem);
		return(NULL);
	}

	/* Free linked list formula. */
	FreeLkedLstFormula(topitem);

	/* Convert to Conjunctive Normal Form. */
	**result=0;
	if (0!=(jj=Convert2CNF(cnvsntnc2,&ptr1,result,size))){
		procctl->status=jj;
		MYFREE(cnvsntnc2);
		return(NULL);
	}

	/* Free memory and return. */
	MYFREE(cnvsntnc2);
	return(ptr2);
} /* PreprocessFormula */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Insert parenthesis according to precedence)  OCJ
 *
 *    This function inserts redundant parenthesis according to precedence
 *    so that next processes don't need to take precedences into account.
 *    Negations are not considered as they affect just the literal following
 *    it. Equalities are not considered as they affect only next term and
 *    are atoms on their own.
 *
 *    See CheckSyntax() function for the precedence of each operator.
 *
 *    For the following a block is defined as a set of consecutive linked
 *    list items that starts with an opening bracket or a literal. If the
 *    block starts with an opening bracket it ends in the corresponding
 *    closing bracket. If the block starts with a literal it ends at the
 *    end of the literal.
 *
 *    Let Q1 be a set of consecutive quantifiers, B1, B2 and B3 blocks
 *    and * a connective that can be an AND, OR, NAND, NOR, XOR,
 *    equivalence, implication or reverse implication. Then this
 *    function makes the following transformations:
 *      Q1 B1 is transformed to (Q1 B1)
 *      B1*B2*B3 is transformed to (B1*B2)*B3
 *
 *
 *
 *  ARGUMENTS:
 *
 *    ptopitem: Address of pointer to top item in formula.
 *
 *  RETURNS:
 *
 *    0 -> Function completed successfully.
 *    NOMEMORY -> No memory available.
 *
 *--------------------------------------------------------------*/
int32_t PutBrackets(lkedfitem **ptopitem){
	auto lkedfitem *item;                               /* Pointer of item being checked */
	auto lkedfitem *openbrkt,*closebrkt;                /* Pointers to new opening and closing brackets */
	auto lkedfitem *insertpt1,*insertpt2;               /* Pointers to brackets insert points. */
	auto lkedfitem *ptr1,*ptr2;                         /* Auxiliary pointers */
	auto int32_t ii;                                    /* Auxiliary */

	/* Loop through formula linked list items. */
	for (item=*ptopitem;item!=NULL;){

		/* Item is a quantifier. */
		if ((UQUANTIFIER|EQUANTIFIER)&item->flags){

			/* Set ptr1 to the start of the block following the set of quantifiers */
			/* and ptr2 to the end of that block. */
			for (ptr1=item->next;(UQUANTIFIER|EQUANTIFIER)&ptr1->flags;ptr1=ptr1->next){
			}
			ptr2=JumpOverBlockOrTerm(ptr1,TOEND);

			/* The set of quantifiers is not yet bracketed. */
			if ((item->prev==NULL)||(0==(OPENBRACKET&item->prev->flags))||(item->prev->ptr.mtchbrckt!=ptr2->next)){
				insertpt1=item->prev;
				insertpt2=ptr2;
				ii=1;

			/* The set of quantifiers is already bracketed. */
			/* Prepare next iteration. */
			} else {
				ii=0;
				item=ptr1;
			}

		/* Item is a block (opening bracket or literal). */
		} else if ((OPENBRACKET|EQUPRED|NAMEDPRED)&item->flags){

			/* Set ptr1 to the item following the literal or the bracket set. */
			ptr1=JumpOverBlockOrTerm(item,PASTEND);

			/* The item at ptr1 is a connective. */
			if ((ptr1!=NULL)&&((OROPER|ANDOPER|NOROPER|NANDOPER|XOROPER|IMPOPER|REVIMPOPER|EQUIVOPER)&ptr1->flags)){

				/* Set ptr2 to the item following the block after ptr1. */
				ptr2=JumpOverBlockOrTerm(ptr1->next,PASTEND);

				/* The item at ptr2 is a connective. Brackets must be added. */
				if ((ptr2!=NULL)&&((OROPER|ANDOPER|NOROPER|NANDOPER|XOROPER|IMPOPER|REVIMPOPER|EQUIVOPER)&ptr2->flags)){
					insertpt1=item->prev;
					insertpt2=ptr2->prev;
					ii=1;

				/* The item at ptr2 is not a connective. */
				/* Prepare next iteration. */
				} else {
					ii=0;
					if (OPENBRACKET&item->flags){
						item=item->next;
					} else {
						item=ptr1;
					}
				}

			/* The item at ptr1 is not a connective. */
			/* Prepare next iteration. */
			} else {
				ii=0;
				if (OPENBRACKET&item->flags){
					item=item->next;
				} else {
					item=ptr1;
				}
			}

		/* Item is neither a quantifier, an opening bracket */
		/* or a literal. Prepare next iteration. */
		} else {
			ii=0;
			item=item->next;
		}

		/* Brackets must be inserted. */
		if (ii){

			/* Allocate memory for the new brackets. */
			if (NULL==(openbrkt=MYALLOC(sizeof(lkedfitem)))){
				return(NOMEMORY);
			}
			if (NULL==(closebrkt=MYALLOC(sizeof(lkedfitem)))){
				MYFREE(openbrkt);
				return(NOMEMORY);
			}

			/* Prepare next iteration. */
			item=openbrkt;

			/* Insert opening and closing brackets. */
			openbrkt->flags=OPENBRACKET;
			openbrkt->ptr.mtchbrckt=closebrkt;
			closebrkt->flags=CLOSEBRACKET;
			closebrkt->ptr.mtchbrckt=openbrkt;
			AddItem2LkdLstF(&openbrkt,ptopitem,&insertpt1);
			AddItem2LkdLstF(&closebrkt,ptopitem,&insertpt2);
		}
	}
	return(0);
} /* PutBrackets */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Append a string safely)  OCJ
 *
 *    This function appends a string to another string in a limited
 *    size buffer. The buffer is reallocated if needed to ensure there
 *    is enough space for appending.
 *
 *    If nchar is negative the whole string is appended. Otherwise
 *    a maximum of nchar characters plus an ending NULL character
 *    are appended.
 *
 *
 *  ARGUMENTS:
 *
 *    deststr: Address of pointer to destination buffer.
 *    buffsize: Pointer to total size of destination buffer in bytes
 *              including the space currently in use.
 *    orgstr: Pointer to string to append.
 *    nchar: Maximum number of characters to append or negative if
 *           appending the whole orgstr.
 *
 *  RETURNS:
 *
 *    0 -> Append completed successfully.
 *    NOMEMORY -> No memory available.
 *
 *--------------------------------------------------------------*/
int32_t SafeAppend(char **deststr,int32_t *buffsize,char *orgstr,int32_t nchar){
	auto char *ptr;                                 /* Auxiliary pointer. */
	auto int32_t ii,jj;                             /* Auxiliary */
	jj=strlen(orgstr);
	ii=strlen(*deststr)+(nchar<0?jj:nchar)+2;
	if (ii>(*buffsize)){
		if (NULL==(ptr=MYREALLOC(*deststr,STR_CHUNK_SIZE+ii))){
			return(NOMEMORY);
		}
		*deststr=ptr;
		*buffsize=STR_CHUNK_SIZE+ii;
	}
	if ((nchar>=0)&&(nchar<jj)){
		strncat(*deststr,orgstr,nchar);
	} else {
		strcat(*deststr,orgstr);
	}
	return(0);
} /* SafeAppend */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform pre-nnf conversion)  OCJ
 *
 *    This function translates the following operators to combinations
 *    of AND (&) and OR (|) operators.
 *      - NAND A~&B is converted to ~A|~B.
 *      - NOR A~|B is converted to ~A&~B.
 *      - Implication A=>B is converted to ~A|B.
 *      - Reverse implication A<=B is converted to A|~B.
 *
 *    This function also calls IndentNegations() function. This is
 *    necessary for the correct behavior of FormulaRenaming(). However
 *    an additional call to IndentNegations() will be necessary also in
 *    Convert2NNF() function because the elimination of equivalences
 *    and XOR's introduces additional negations.
 *
 *    The given formula must be in linked list format.
 *
 *
 *  ARGUMENTS:
 *
 *    ptopitem: Address of pointer to linked list top item of
 *              formula to be converted.
 *    formula: Pointer to cmprefix structure of formula in text
 *             form.
 *    pbuffer: Address of pointer to buffer for formula conversion
 *             to text.
 *    psize: Pointer to buffer size.
 *
 *  RETURNS:
 *
 *    Pointer to cmprefix structure of converted formula in text
 *    form or NULL if not enough memory.
 *
 *--------------------------------------------------------------*/
cmprefix *Convert2PreNNF(lkedfitem **ptopitem,cmprefix *formula,char **pbuffer,int32_t *psize){
	auto lkedfitem *item;                               /* Item in linked list formula */
	auto lkedfitem *ptr1;                               /* Auxiliary pointer */
	auto int32_t ii,jj;                                 /* Auxiliary */

	/* Insert redundant parenthesis according to precedence. */
	if (NOMEMORY==PutBrackets(ptopitem)){
		return(NULL);
	}
	RemoveRedundantBrackets(ptopitem);

	/* Loop through items in linked list formula. */
	for (item=*ptopitem,jj=0;item->next!=NULL;item=item->next){

		/* Set ii to the type of connective if there is a connective */
		/* after the sub-formula. */
		if ((OPENBRACKET|EQUPRED|NAMEDPRED)&item->flags){
			ptr1=JumpOverBlockOrTerm(item,PASTEND);
		} else {
			continue;
		}
		if (ptr1!=NULL){
			ii=(NANDOPER|NOROPER|IMPOPER|REVIMPOPER)&ptr1->flags;
		} else {
			continue;
		}

		/* Process depending on the type of connective. */
		switch (ii){
			case NANDOPER:
				DropNAND(item);
				jj=1;
				break;
			case NOROPER:
				DropNOR(item);
				jj=1;
				break;
			case IMPOPER:
				DropImplication(item);
				jj=1;
				break;
			case REVIMPOPER:
				DropRevImplication(item);
				jj=1;
				break;
		}
	}

	/* Move negations inwards. */
	jj|=IndentNegations(*ptopitem);

	/* If the formula has been converted to pre-nnf form add the resulting */
	/* text formula to main KB and update number of inferences. */
	if (jj){
		pbmstats->prennfxform++;
		return(AddLkdLstFormula2KB(*ptopitem,formula,NULL,PRENNFCONV,pbuffer,psize));
	}

	return(formula);
} /* Convert2PreNNF */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Perform nnf conversion)  OCJ
 *
 *    This function translates the following operators to combinations
 *    of AND (&) and OR (|) operators.
 *      - XOR A<~>B:
 *        - If polarity(A<~>B)=+1 then convert to (A|B)&(~A|~B)
 *        - If polarity(A<~>B)=-1 then convert to (A&~B)|(~A&B)
 *      - Equivalence A<=>B:
 *        - If polarity (A<=>B)=+1 then convert to (~A|B)&(A|~B)
 *        - If polarity (A<=>B)=-1 then convert to (A&B)|(~A&~B)
 *
 *    These rules must be applied at positions with minimal length
 *    to avoid cases in which the polarity is zero. This impedes
 *    making this function recursive.
 *
 *    The given formula must be in linked list format and with
 *    appropriate redundant brackets so that there is no need to
 *    take precedence into account. It is also assumed that the
 *    formula has not NAND, NOR, implications and reverse implication
 *    connectives.
 *
 *
 *  ARGUMENTS:
 *
 *    ptopitem: Address of pointer to linked list top item of
 *              formula to be converted.
 *    formula: Pointer to cmprefix structure of formula in text
 *             form.
 *    pbuffer: Address of pointer to buffer for formula conversion
 *             to text.
 *    psize: Pointer to buffer size.
 *
 *  RETURNS:
 *
 *    Pointer to cmprefix structure of converted formula in text
 *    form or NULL if not enough memory.
 *
 *--------------------------------------------------------------*/
cmprefix *Convert2NNF(lkedfitem **ptopitem,cmprefix *formula,char **pbuffer,int32_t *psize){
	auto lkedfitem *item;                               /* Item in linked list formula */
	auto lkedfitem *ptr1;                               /* Auxiliary pointer */
	auto int32_t ii;                                    /* Auxiliary */

	/* Loop through items in linked list formula. */
	for (item=*ptopitem,ii=0;item->next!=NULL;item=item->next){

		/* Set ptr1 to the connective if there is a connective */
		/* after the block starting at item. */
		if ((OPENBRACKET|EQUPRED|NAMEDPRED)&item->flags){
			if (NULL==(ptr1=JumpOverBlockOrTerm(item,PASTEND))){
				continue;
			}
		} else {
			continue;
		}

		/* Process depending on the type of connective. */
		switch ((XOROPER|EQUIVOPER)&ptr1->flags){
			case XOROPER:
				if (NULL==(item=DropXOR(item,ptopitem))){
					return(NULL);
				}
				ii=1;
				break;
			case EQUIVOPER:
				if (NULL==(item=DropEquivalence(item,ptopitem))){
					return(NULL);
				}
				ii=1;
				break;
		}
	}

	/* Move negations inwards. */
	ii|=IndentNegations(*ptopitem);

	/* Remove redundant brackets. */
	RemoveRedundantBrackets(ptopitem);

	/* The formula has been converted to nnf form. Add the resulting */
	/* text formula to main KB and update number of inferences. */
	if (ii){
		pbmstats->nnfxform++;
		return(AddLkdLstFormula2KB(*ptopitem,formula,NULL,NNFCONV,pbuffer,psize));
	}
	return(formula);
} /* Convert2NNF */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Translate an implication)  OCJ
 *
 *    This function converts an implication from A=>B to ~A|B.
 *
 *
 *  ARGUMENTS:
 *
 *    item: Pointer to item "A" at the beginning of implication. This
 *          can be a literal or a block starting with an opening bracket.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void DropImplication(lkedfitem *item){
	auto lkedfitem *ptr1;                               /* Auxiliary pointer */

	/* Modify A=>B to ~A|B. */
	item->flags^=(NEGITEM);
	ptr1=JumpOverBlockOrTerm(item,PASTEND);
	ptr1->flags&=(~IMPOPER);
	ptr1->flags|=OROPER;
	return;
} /* DropImplication */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Translate a reverse implication)  OCJ
 *
 *    This function converts an implication from A<=B to A|~B.
 *
 *
 *  ARGUMENTS:
 *
 *    item: Pointer to item "A" at the beginning of reverse implication.
 *          This can be a literal or a block starting with an opening bracket.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void DropRevImplication(lkedfitem *item){
	auto lkedfitem *ptr1;                               /* Auxiliary pointer */

	/* Modify A<=B to A|~B. */
	ptr1=JumpOverBlockOrTerm(item,PASTEND);
	ptr1->flags&=(~REVIMPOPER);
	ptr1->flags|=OROPER;
	ptr1->next->flags^=(NEGITEM);
	return;
} /* DropRevImplication */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Translate an equivalence)  OCJ
 *
 *    This function converts an equivalence A<=>B the following way:
 *      - If polarity (A<=>B)=+1 then convert to (~A|B)&(A|~B)
 *      - If polarity (A<=>B)=-1 then convert to (A&B)|(~A&~B)
 *
 *
 *
 *
 *  ARGUMENTS:
 *
 *    item: Pointer to item "A" to the left of the reverse implication. This
 *          can be a literal or a block starting with an opening bracket.
 *    ptopitem: Address of pointer to linked list top item.
 *
 *  RETURNS:
 *
 *    Pointer to the first opening bracket of the converted formula
 *    or NULL if no memory available.
 *
 *--------------------------------------------------------------*/
lkedfitem *DropEquivalence(lkedfitem *item,lkedfitem **ptopitem){
	auto lkedfitem *ptr1,*ptr2,*ptr3,*ptr4,*ptr5,*ptr6; /* Auxiliary pointers */

	/* Recalculate polarity. */
	SetPolarity(*ptopitem);

	/* Convert A<=>B to (A<=>B. After this ptr1 will */
	/* point to opening bracket. */
	if (NULL==(ptr2=MYALLOC(sizeof(lkedfitem)))){
		return(NULL);
	}
	ptr2->flags=OPENBRACKET;
	if (item->prev!=NULL){
		ptr2->flags|=((POSPOL|NEGPOL|NEUTRALPOL)&item->prev->flags);
	} else {
		ptr2->flags|=POSPOL;
	}
	ptr1=item->prev;
	AddItem2LkdLstF(&ptr2,ptopitem,&ptr1);

	/* Convert (A<=>B to (A<=>B). After this ptr2 will point to */
	/* the closing bracket and ptr3 to the connective. */
	if (NULL==(ptr4=MYALLOC(sizeof(lkedfitem)))){
		return(NULL);
	}
	ptr4->flags=CLOSEBRACKET;
	ptr1->ptr.mtchbrckt=ptr4;
	ptr4->ptr.mtchbrckt=ptr1;
	ptr4->flags|=((POSPOL|NEGPOL|NEUTRALPOL)&ptr1->flags);
	ptr3=JumpOverBlockOrTerm(item,PASTEND);
	ptr2=JumpOverBlockOrTerm(ptr3->next,TOEND);
	AddItem2LkdLstF(&ptr4,ptopitem,&ptr2);

	/* Convert (A<=>B) to (A|B) or (A&B) depending on ptr1 polarity. */
	ptr3->flags&=(~EQUIVOPER);
	if (POSPOL&ptr1->flags){
		ptr3->flags|=OROPER;
	} else {
		ptr3->flags|=ANDOPER;
	}

	/* Make a duplicate of the resulting (A|B) or (A&B). */
	/* Now ptr5 will point to the opening bracket and ptr6 */
	/* will point to the closing bracket in the new */
	/* duplicated block. */
	if (DuplicateBlock(ptr1,ptr2,&ptr5,&ptr6)){
		return(NULL);
	}

	/* Insert new duplicate block to the right of the new connective. */
	InsertBlock(ptr5,ptr6,ptr2);

	/* Add an AND or OR connective to the right of the original block. */
	if (NULL==(ptr4=MYALLOC(sizeof(lkedfitem)))){
		return(NULL);
	}
	if (POSPOL&ptr1->flags){
		ptr4->flags=ANDOPER;
	} else {
		ptr4->flags=OROPER;
	}
	AddItem2LkdLstF(&ptr4,ptopitem,&ptr2);

	/* Modify negation states as appropriate. */
	if (POSPOL&ptr1->flags){
		ptr1->next->flags^=(NEGITEM);
	} else {
		ptr5->next->flags^=(NEGITEM);
	}
	ptr4=JumpOverBlockOrTerm(ptr5->next,PASTEND);
	ptr4->next->flags^=(NEGITEM);

	return(ptr1);
} /* DropEquivalence */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Translate a XOR connective)  OCJ
 *
 *    This function converts a XOR connective A<~>B the following way:
 *      - If polarity(A<~>B)=+1 then convert to (A|B)&(~A|~B)
 *      - If polarity(A<~>B)=-1 then convert to (A&~B)|(~A&B)
 *
 *
 *  ARGUMENTS:
 *
 *    item: Pointer to item "A" to the left of the XOR connective. This can be
 *          a literal or a block starting with an opening bracket.
 *    ptopitem: Address of pointer to linked list top item.
 *
 *  RETURNS:
 *
 *    Pointer to the first opening bracket of the converted formula
 *    or NULLL if no memory available.
 *
 *--------------------------------------------------------------*/
lkedfitem *DropXOR(lkedfitem *item,lkedfitem **ptopitem){
	auto lkedfitem *ptr1,*ptr2,*ptr3,*ptr4,*ptr5,*ptr6; /* Auxiliary pointers */

	/* Recalculate polarity. */
	SetPolarity(*ptopitem);

	/* Convert A<~>B to (A<~>B. After this ptr1 will */
	/* point to opening bracket. */
	if (NULL==(ptr2=MYALLOC(sizeof(lkedfitem)))){
		return(NULL);
	}
	ptr2->flags=OPENBRACKET;
	if (item->prev!=NULL){
		ptr2->flags|=((POSPOL|NEGPOL|NEUTRALPOL)&item->prev->flags);
	} else {
		ptr2->flags|=POSPOL;
	}
	ptr1=item->prev;
	AddItem2LkdLstF(&ptr2,ptopitem,&ptr1);

	/* Convert (A<~>B to (A<~>B). After this ptr2 will point to */
	/* the closing bracket and ptr3 to the connective. */
	if (NULL==(ptr4=MYALLOC(sizeof(lkedfitem)))){
		return(NULL);
	}
	ptr4->flags=CLOSEBRACKET;
	ptr1->ptr.mtchbrckt=ptr4;
	ptr4->ptr.mtchbrckt=ptr1;
	ptr4->flags|=((POSPOL|NEGPOL|NEUTRALPOL)&ptr1->flags);
	ptr3=JumpOverBlockOrTerm(item,PASTEND);
	ptr2=JumpOverBlockOrTerm(ptr3->next,TOEND);
	AddItem2LkdLstF(&ptr4,ptopitem,&ptr2);

	/* Convert (A<~>B) to (A|B) or (A&B) depending on ptr1 polarity. */
	ptr3->flags&=(~XOROPER);
	if (POSPOL&ptr1->flags){
		ptr3->flags|=OROPER;
	} else {
		ptr3->flags|=ANDOPER;
	}

	/* Make a duplicate of the resulting (A|B) or (A&B). */
	/* Now ptr5 will point to the opening bracket and ptr6 */
	/* will point to the closing bracket in the new */
	/* duplicated block. */
	if (DuplicateBlock(ptr1,ptr2,&ptr5,&ptr6)){
		return(NULL);
	}

	/* Insert new duplicate block to the right of the modified original block. */
	InsertBlock(ptr5,ptr6,ptr2);

	/* Add an AND or OR connective to the right of the modified original block. */
	if (NULL==(ptr4=MYALLOC(sizeof(lkedfitem)))){
		return(NULL);
	}
	if (POSPOL&ptr1->flags){
		ptr4->flags=ANDOPER;
	} else {
		ptr4->flags=OROPER;
	}
	AddItem2LkdLstF(&ptr4,ptopitem,&ptr2);

	/* Modify negation states as appropriate. */
	ptr5->next->flags^=(NEGITEM);
	if (POSPOL&ptr1->flags){
		ptr4=JumpOverBlockOrTerm(ptr5->next,PASTEND);
		ptr4->next->flags^=(NEGITEM);
	} else {
		ptr3->next->flags^=(NEGITEM);
	}

	return(ptr1);
} /* DropXOR */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Translate a NAND)  OCJ
 *
 *    This function converts a NAND from A~&B to ~A|~B.
 *
 *
 *  ARGUMENTS:
 *
 *    item: Pointer to item "A" at the beginning of NAND. This can be
 *          a literal or a block starting with an opening bracket.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void DropNAND(lkedfitem *item){
	auto lkedfitem *ptr1;                               /* Auxiliary pointer */

	/* Modify A~&B to ~A|~B. */
	item->flags^=(NEGITEM);
	ptr1=JumpOverBlockOrTerm(item,PASTEND);
	ptr1->flags&=(~NANDOPER);
	ptr1->flags|=OROPER;
	ptr1->next->flags^=(NEGITEM);
	return;
} /* DropNAND */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Convert a NOR)  OCJ
 *
 *    This function converts a NOR from A~|B to ~A&~B.
 *
 *
 *  ARGUMENTS:
 *
 *    item: Pointer to item "A" at the beginning of NOR. This can be
 *          a literal or a block starting with an opening bracket.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void DropNOR(lkedfitem *item){
	auto lkedfitem *ptr1;                               /* Auxiliary pointer */

	/* Modify A~|B to ~A&~B. */
	item->flags^=(NEGITEM);
	ptr1=JumpOverBlockOrTerm(item,PASTEND);
	ptr1->flags&=(~NOROPER);
	ptr1->flags|=ANDOPER;
	ptr1->next->flags^=(NEGITEM);
	return;
} /* DropNOR */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Move negations inwards)  OCJ
 *
 *    This function move negations inwards in a formula in linked list
 *    format. It is assumed that the formula has only OR, AND, XOR and
 *    equivalence operator. These operators are converted as follows:
 *      ~(A|B) is converted to (~A&~B).
 *      ~(A&B) is converted to (~A|~B).
 *      ~(A<=>B) is converted to (A<~>B).
 *      ~(A<~>B) is converted to (A<=>B).
 *
 *
 *
 *  ARGUMENTS:
 *
 *    topitem: Pointer to linked list top item of formula to be translated.
 *
 *  RETURNS:
 *
 *    0 if no negations were moved inwards, 1 otherwise.
 *
 *--------------------------------------------------------------*/
int32_t IndentNegations(lkedfitem *topitem){
	auto lkedfitem *item;                               /* Item in linked list formula */
	auto lkedfitem *ptr1;                               /* Auxiliary pointer */
	auto int32_t ii;                                    /* Auxiliary */

	/* Loop through items in linked list formula. */
	for (item=topitem,ii=0;item!=NULL;item=item->next){

		/* If item is a literal then jump to the end of the item */
		/* and iterate. Also ~$true is converted to $false and */
		/* ~$false is converted to $true. */
		if ((EQUPRED&item->flags)||(NAMEDPRED&item->flags)){
			if (NEGITEM&item->flags){
				if (TRUEPRED&item->flags){
					item->flags&=(~(NEGITEM|TRUEPRED));
					item->flags|=FALSEPRED;
				} else if (FALSEPRED&item->flags){
					item->flags&=(~(NEGITEM|FALSEPRED));
					item->flags|=TRUEPRED;
				}
			}
			item=JumpOverBlockOrTerm(item,TOEND);
			continue;
		}

		/* Item is negated and it is an opening bracket or a quantifier. */
		if ((NEGITEM&item->flags)&&((OPENBRACKET|UQUANTIFIER|EQUANTIFIER)|item->flags)){

			/* Indicate that modifications were done and delete negation state. */
			ii=1;
			item->flags^=NEGITEM;

			/* Item is an opening bracket. */
			if (OPENBRACKET&item->flags){

				/* If next item is an opening bracket then set ptr1 to */
				/* item following the matching closing bracket. */
				if (OPENBRACKET&item->next->flags){
					ptr1=item->next->ptr.mtchbrckt->next;

				/* If next item is a literal then set ptr1 to item */
				/* following literal. */
				} else if ((EQUPRED|NAMEDPRED)&item->next->flags){
					ptr1=JumpOverBlockOrTerm(item->next,PASTEND);

				/* Otherwise set ptr1 to NULL. */
				} else {
					ptr1=NULL;
				}

				/* The pointer ptr1 is not NULL. */
				if (ptr1!=NULL){

					/* If ptr1 is a XOR or equivalence connective then switch connective */
					/* between XOR and equivalence. */
					if ((EQUIVOPER|XOROPER)&ptr1->flags){
						ptr1->flags^=(EQUIVOPER|XOROPER);

					/* If ptr1 is an OR or AND connective then switch connective between */
					/* OR and AND and switch negation state of the operands. */
					} else if ((ANDOPER|OROPER)&ptr1->flags){
						ptr1->flags^=(ANDOPER|OROPER);
						item->next->flags^=NEGITEM;
						ptr1->next->flags^=NEGITEM;

					/* Otherwise transfer negation to next item. */
					} else {
						item->next->flags^=NEGITEM;
					}

				/* If the pointer ptr1 is NULL then transfer negation to next item. */
				} else {
					item->next->flags^=NEGITEM;
				}

			/* If item is a quantifier then transfer negation to next item */
			/* and switch between universal and existential type. */
			} else {
				item->next->flags^=NEGITEM;
				item->flags^=(UQUANTIFIER|EQUANTIFIER);
			}
		}
	}

	return(ii);
} /* IndentNegations */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Delete quantifiers in formula)  OCJ
 *
 *    This function deletes the existential and universal quantifiers
 *    in a formula.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    ptopitem: Address of pointer to linked list top item of formula.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void DropQuantifiers(lkedfitem **ptopitem){
	auto lkedfitem *ptr1,*ptr2;                         /* Auxiliary pointer */

	/* Loop through the formula items. */
	for (ptr1=*ptopitem;ptr1!=NULL;ptr1=ptr2){

		/* Item is a quantifier. Set ptr2 to the first item after the */
		/* set of quantifiers and remove the set of quantifiers. */
		if ((UQUANTIFIER|EQUANTIFIER)&ptr1->flags){
			for (ptr2=ptr1->next;(UQUANTIFIER|EQUANTIFIER)&ptr2->flags;ptr2=ptr2->next){
			}
			RemoveLinkedListChain(ptopitem,ptr1,ptr2->prev);

		/* Item is not a quantifier. */
		} else {
			ptr2=ptr1->next;
		}
	}
	return;
} /* DropQuantifiers */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Remove a chain of linked list items)  OCJ
 *
 *    This function removes a chain of linked list items in a formula.
 *
 *    This function must be called with care in order to ensure that
 *    no free variables are left in the formula after the removal.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    ptopitem: Address of pointer to linked list top item of formula.
 *    first: Pointer to first linked list item in the chain.
 *    last: Pointer to last linked list item in the chain.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void RemoveLinkedListChain(lkedfitem **ptopitem,lkedfitem *first,lkedfitem *last){

	/* Remove the chain of items from the formula. */
	if (last->next!=NULL){
		last->next->prev=first->prev;
	}
	if (first->prev!=NULL){
		first->prev->next=last->next;
	} else {
		*ptopitem=last->next;
	}
	last->next=first->prev=NULL;

	/* Free chain of linked list items. */
	FreeLkedLstFormula(first);
	return;
} /* RemoveLinkedListChain */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Convert formula to CNF)  OCJ
 *
 *    This function converts a formula to conjunctive normal form.
 *    It sets a pointer to the point from which syntax is not
 *    understood and adds the converted part to cnvsntnc. This function
 *    is called recursively.
 *
 *    The given formula is assumed to be syntactically correct,
 *    with appropriate redundant brackets, with negations fully moved
 *    inwards and with no implications nor quantifiers.
 *
 *    The Conjunctive Normal Form representation adopted here encloses
 *    every clause in parenthesis (even if the clause is a single literal)
 *    and separates clauses by conjunctions (AND operator). Clauses are of
 *    course literals separated by disjunctions (OR operators). Literals
 *    are atomic formulas or negated atomic formulas. The result is stored
 *    in the provided buffer in the form (clause1)&(clause2)&...&(clausen),
 *    where clause1, clause2, etc. are of the form:
 *    literal1|literal2|...|literaln
 *
 *    Two CNF formulas separated by an AND operator can be joined
 *    directly into a single CNF. Two CNF formulas separated by an
 *    OR operator require a more sophisticated process to join them
 *    into a single CNF. This last process is performed by CnfOrCnf()
 *    function, which includes comments describing the process.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to string with the formula.
 *    result: Pointer to the point from which syntax is not understood.
 *    cnvsntnc: Pointer to buffer for converted formula. Can be reallocated.
 *    buffsize: Pointer to size in bytes of buffer pointed by *cnvsntc.
 *
 *  RETURNS:
 *
 *    0 -> Function completed successfully.
 *    NOMEMORY -> No memory available.
 *    TIMEOUT -> Timeout condition.
 *
 *--------------------------------------------------------------*/
int32_t Convert2CNF(char *formula,char **result,char **cnvsntnc,int32_t *buffsize){
	auto char *ptr1;                                    /* Pointer to next formula chunk */
	auto char *ptr2;                                    /* Auxiliary pointers */
	auto char *auxcnv1,*auxcnv2;                        /* Auxiliary buffers for converted chunks */
	auto int32_t size1,size2;                           /* Size of auxcnv1 and auxcnv2 */
	auto double timedif;                                /* Elapsed time in seconds */
	auto int32_t ii,jj,kk,mm,pp;                        /* Auxiliary */

	/* Check timeout. */
	if (procctl->status==TIMEOUT){
		gettimeofday(&mainkb.endtime,NULL);
		timedif=mainkb.prstats.elapsed_time+mainkb.endtime.tv_sec-mainkb.time.tv_sec
				+(mainkb.endtime.tv_usec-mainkb.time.tv_usec)/1000000.0;
		glblstats.elapsed_time+=timedif;
		return(TIMEOUT);
	}

	/* Allocate space for auxiliary converted chunks. */
	if (NULL==(auxcnv1=MYALLOC(STR_CHUNK_SIZE))){
		return(NOMEMORY);
	}
	auxcnv1[0]=0;
	size1=STR_CHUNK_SIZE;
	if (NULL==(auxcnv2=MYALLOC(STR_CHUNK_SIZE))){
		MYFREE(auxcnv1);
		return(NOMEMORY);
	}
	size2=STR_CHUNK_SIZE;

	/* Parsing loop. Each chunk is converted to CNF, stored */
	/* in auxcnv2 buffer and then added to auxcnv1 buffer */
	/* in a way that depends on the previous binary operator. */
	ptr1=SkipBlanks(formula);
	ii=1; /* Indicate continue the loop. */
	jj=0; /* Indicate no previous negation detected. */
	kk=0; /* 0-> No binary operator pending, 1-> pending AND, 2-> pending OR. */
	while ((ptr1[0]!='\0')&&(ii)){

		/* Parse chunk. */
		switch (*ptr1){

			/* Negations are copied to auxcnv2 preceded by an open parenthesis. */
			case '~':
				strcpy(auxcnv2,"(~");
				jj=1; /* Indicate negation detected. */
				ptr1=SkipBlanks(&ptr1[1]);
				break;

			/* Process parenthesis contents. */
			case '(':
				auxcnv2[0]=0;
				ptr1=SkipBlanks(&ptr1[1]);
				if (0!=(mm=Convert2CNF(ptr1,&ptr1,&auxcnv2,&size2))){
					MYFREE(auxcnv1);
					MYFREE(auxcnv2);
					return(mm);
				}
				ptr1=SkipBlanks(&ptr1[1]);
				ii=0; /* Don't continue the loop unless a binary operator is detected next. */
				break;

			/* Atomic formula. */
			default:

				/* Jump over whole atomic formula until one of the */
				/* following is found: end of string, AND, OR */
				/* or unbalanced closing parenthesis. */
				ptr2=ptr1;
				pp=1; /* Parenthesis balance. */
				for (mm=0;((ptr1[0]!='\0')&&(ptr1[0]!='&')&&(ptr1[0]!='|')&&((ptr1[0]!=')')||(pp)));mm++){
					if ((ptr2[mm]=='\'')||(ptr2[mm]=='"')){
						mm=JumpOverQuotes(&(ptr2[mm]))-ptr2;
					} else {
						ptr1=&ptr2[mm];
						if (ptr1[0]=='('){
							pp++;
						} else if (ptr1[0]==')'){
							pp--;
						}
					}
				}

				/* Place atomic formula in buffer preceded by negation if applicable. */
				if (jj){
					jj=0;
				} else {
					strcpy(auxcnv2,"(");
				}
				if (NOMEMORY==SafeAppend(&auxcnv2,&size2,ptr2,ptr1-ptr2)){
					MYFREE(auxcnv1);
					MYFREE(auxcnv2);
					return(NOMEMORY);
				}
				if (NOMEMORY==SafeAppend(&auxcnv2,&size2,")",-1)){
					MYFREE(auxcnv1);
					MYFREE(auxcnv2);
					return(NOMEMORY);
				}
				jj=0; /* Indicate negation not detected. */
				ii=0; /* Don't continue the loop unless a binary operator is detected next. */
				break;
		}

		/* Add CNF chunk to auxcnv1. */
		if (jj==0){
			switch (kk){

				/* No previous CNF. */
				case 0:
					if (NOMEMORY==SafeAppend(&auxcnv1,&size1,auxcnv2,-1)){
						MYFREE(auxcnv1);
						MYFREE(auxcnv2);
						return(NOMEMORY);
					}
					break;

				/* Separation from previous CNF through an AND operator. */
				/* Simply join both CNF's with an AND operator. */
				case 1:
					if (NOMEMORY==SafeAppend(&auxcnv1,&size1,"&",-1)){
						MYFREE(auxcnv1);
						MYFREE(auxcnv2);
						return(NOMEMORY);
					}
					if (NOMEMORY==SafeAppend(&auxcnv1,&size1,auxcnv2,-1)){
						MYFREE(auxcnv1);
						MYFREE(auxcnv2);
						return(NOMEMORY);
					}
					break;

				/* Separation from previous CNF through an OR operator. */
				/* Call CnfOrCnf() to join both CNF's into a single one. */
				case 2:
					if (NOMEMORY==CnfOrCnf(&auxcnv1,&size1,auxcnv2)){
						MYFREE(auxcnv1);
						MYFREE(auxcnv2);
						return(NOMEMORY);
					}
					break;
			}
		}

		/* Binary operator expected. If so remember it. */
		if (ii==0){
			switch (ptr1[0]){
				case '&':
					kk=1;
					ii=1; /* Indicate continue the loop. */
					ptr1=SkipBlanks(&ptr1[1]);
					break;
				case '|':
					kk=2;
					ii=1; /* Indicate continue the loop. */
					ptr1=SkipBlanks(&ptr1[1]);
					break;
				default:
					break;
			}
		}
	}

	/* Append converted formula to cnvsntnc and update statistics. */
	if (NOMEMORY==SafeAppend(cnvsntnc,buffsize,auxcnv1,-1)){
		MYFREE(auxcnv1);
		MYFREE(auxcnv2);
		return(NOMEMORY);
	}
	pbmstats->cnfconversion++;

	/* Free memory and return the OK. */
	MYFREE(auxcnv1);
	MYFREE(auxcnv2);
	*result=ptr1;
	return(0);
} /* Convert2CNF */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Merge two CNF's separated by an OR operator)  OCJ
 *
 *    This function merges two CNF formulas separated by an OR
 *    operator and put the result in the first CNF, reallocating space
 *    if needed. Both CNF's are assumed to be syntactically correct
 *    and not empty.
 *
 *    The merge is done using the distributive property as follows:
 *    Let:
 *      CNF1=C11&C12&...&C1n the first CNF
 *      CNF2=C21&C22&...&C2m the second CNF
 *      C1i=(L1i1|L1i2|...|L1ip) a clause of first CNF
 *      C2i=(L2i1|L2i2|...|L2iq) a clause of second CNF
 *      L1ij the jth literal of ith clause in first CNF
 *      L2ij the jth literal of ith clause in second CNF
 *    Then:
 *      CNF1|CNF2=(C11|C21)&(C11|C22)&...&(C11|C2m)
 *               &(C12|C21)&(C12|C22)&...&(C12|C2m)
 *                 .
 *                 .
 *                 .
 *               &(C1n|C21)&(C1n|C22)&...&(C1n|C2m)
 *    And:
 *      (C1i|C2j)=(L1i1|L1i2|...|L1ip|L2j1|L2j2|...|L2jq)
 *
 *
 *
 *  ARGUMENTS:
 *
 *    Cnf1: Pointer to buffer with first CNF, where result will be
 *          put. Can be reallocated.
 *    buffsize: Pointer to size in bytes of buffer pointed by *Cnf1.
 *    Cnf2: Pointer to second CNF.
 *
 *  RETURNS:
 *
 *    0 -> Function completed successfully.
 *    NOMEMORY -> No memory available.
 *
 *--------------------------------------------------------------*/
int32_t CnfOrCnf(char **Cnf1,int *buffsize,char *Cnf2){
	auto char *ptr1,*ptr2,*ptr3;                        /* Auxiliary pointer */
	auto char *auxcnv1;                                 /* Auxiliary buffer */
	auto int32_t size1;                                 /* Size of auxcnv1 */
	auto int32_t ii,jj,mm,pp,qq;                        /* Auxiliary */

	/* Allocate auxiliary space. */
	if (NULL==(auxcnv1=MYALLOC(STR_CHUNK_SIZE))){
		return(NOMEMORY);
	}
	auxcnv1[0]=0;
	size1=STR_CHUNK_SIZE;

	/* Set ptr1 to start of first clause in first CNF */
	/* past the open parenthesis. */
	ptr1=SkipBlanks(*Cnf1);
	ptr1=SkipBlanks(&ptr1[1]);

	/* Loop of clauses in first CNF. */
	mm=0; /* No merges so far. */
	pp=0; /* Parenthesis balance. */
	for (ii=0;ptr1[ii]!=0;ii++){

		/* Jump over segments in single or double quotes. */
		if ((ptr1[ii]=='\'')||(ptr1[ii]=='"')){
			ptr3=JumpOverQuotes(&ptr1[ii]);
			if (ptr3[1]==0){
				break;
			}
			ii=&ptr3[1]-ptr1;
		}

		/* Open parenthesis of a function or predicate. */
		if (ptr1[ii]=='('){
			pp++;

		/* Closing parenthesis. */
		} else if (ptr1[ii]==')'){

			/* Closing parenthesis of a function or predicate. */
			if (pp){
				pp--;

			/* End of clause in first CNF. */
			} else {

				/* Set ptr2 to start of first clause in second CNF */
				/* past the open parenthesis. */
				ptr2=SkipBlanks(Cnf2);
				ptr2=SkipBlanks(&ptr2[1]);

				/* Loop of clauses in second CNF. */
				qq=0; /* Parenthesis balance. */
				for (jj=0;ptr2[jj]!='\0';jj++){

					/* Jump over segments in single or double quotes. */
					if ((ptr2[jj]=='\'')||(ptr2[jj]=='"')){
						ptr3=JumpOverQuotes(&ptr2[jj]);
						if (ptr3[1]==0){
							break;
						}
						jj=&ptr3[1]-ptr2;
					}

					/* Open parenthesis of a function or predicate. */
					if (ptr2[jj]=='('){
						qq++;

					/* Closing parenthesis. */
					} else if (ptr2[jj]==')'){

						/* Closing parenthesis of a function or predicate. */
						if (qq){
							qq--;

						/* End of clause in second CNF. */
						/* Add merged clause to result. */
						} else {
							if (mm!=0){
								if (NOMEMORY==SafeAppend(&auxcnv1,&size1,"&",-1)){
									MYFREE(auxcnv1);
									return(NOMEMORY);
								}
							}
							if (NOMEMORY==SafeAppend(&auxcnv1,&size1,"(",-1)){
								MYFREE(auxcnv1);
								return(NOMEMORY);
							}
							if (NOMEMORY==SafeAppend(&auxcnv1,&size1,ptr1,ii)){
								return(NOMEMORY);
							}
							if (NOMEMORY==SafeAppend(&auxcnv1,&size1,"|",-1)){
								MYFREE(auxcnv1);
								return(NOMEMORY);
							}
							if (NOMEMORY==SafeAppend(&auxcnv1,&size1,ptr2,jj)){
								return(NOMEMORY);
							}
							if (NOMEMORY==SafeAppend(&auxcnv1,&size1,")",-1)){
								MYFREE(auxcnv1);
								return(NOMEMORY);
							}

							/* Set ptr2 to start of next clause in second CNF */
							/* past the open parenthesis. */
							ptr2=SkipBlanks(&ptr2[jj+1]);
							if (ptr2[0]!=0){
								ptr2=SkipBlanks(&ptr2[1]);
								ptr2=SkipBlanks(&ptr2[1]);
							}
							jj=-1;
							mm=1;
						}
					}
				}

				/* Set ptr1 to start of next clause in first CNF */
				/* past the open parenthesis. */
				ptr1=SkipBlanks(&ptr1[ii+1]);
				if (ptr1[0]!=0){
					ptr1=SkipBlanks(&ptr1[1]);
					ptr1=SkipBlanks(&ptr1[1]);
				}
				ii=-1;
			}
		}
	}

	/* Put result in Cnf1. */
	**Cnf1=0;
	if (NOMEMORY==SafeAppend(Cnf1,buffsize,auxcnv1,-1)){
		MYFREE(auxcnv1);
		return(NOMEMORY);
	}

	/* Free memory and return the OK. */
	MYFREE(auxcnv1);
	return(0);
} /* CnfOrCnf */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Convert formula text to linked list format)  OCJ
 *
 *    This function converts a formula specified as text to linked list
 *    formula. The formula is assumed to be syntactically correct.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to string with the formula.
 *
 *  RETURNS:
 *
 *    Pointer to first linked list item or NULL if NOMEMORY.
 *
 *--------------------------------------------------------------*/
lkedfitem *Text2LinkedList(char *formula){
	auto lkedfitem *topitem;                            /* Pointer to top item of linked list */
	auto lkedfitem *nextitem;                           /* Pointer to next item of linked list */
	auto lkedfitem *previtem;                           /* Pointer to previous item of linked list in loop iteration */
	auto nameschain *vars;                              /* Pointer to inner active nameschain structure */
	auto int32_t negation;                              /* Pending negation to be applied */
	auto char *ptr1;                                    /* Auxiliary pointer */

	/* Loop through formula characters. */
	topitem=previtem=nextitem=NULL;
	vars=NULL;
	negation=0;
	for (ptr1=formula;ptr1[0]!=0;ptr1++){
		ptr1=SkipBlanks(ptr1);
		if (ptr1[0]==0){
			break;
		}

		/* Allocate storage for next item if necessary. */
		if (nextitem==NULL){
			if (NULL==(nextitem=MYALLOC(sizeof(lkedfitem)))){
				FreeLkedLstFormula(topitem);
				return(NULL);
			}
		}

		/* Process depending on character type. */
		switch (ptr1[0]){

			/* Negation, NOR and NAND connectives. */
			case '~':
				if (ptr1[1]=='&'){
					nextitem->flags=NANDOPER;
					AddItem2LkdLstF(&nextitem,&topitem,&previtem);
					ptr1++;
				} else if (ptr1[1]=='|'){
					nextitem->flags=NOROPER;
					AddItem2LkdLstF(&nextitem,&topitem,&previtem);
					ptr1++;
				} else {
					negation^=NEGITEM;
				}
				break;

			/* Opening bracket. */
			case '(':
				nextitem->flags=negation;
				negation=0;
				if (NOMEMORY==AddOpenBracket(&nextitem,&topitem,&previtem)){
					FreeLkedLstFormula(topitem);
					return(NULL);
				}
				break;

			/* Closing bracket. */
			case ')':
				previtem=previtem->next;
				if ((UQUANTIFIER|EQUANTIFIER)&previtem->ptr.mtchbrckt->next->flags){
					vars=previtem->ptr.mtchbrckt->next->ptr.vardata->next;
				}
				break;

			/* Quantifier. */
			case '!':
			case '?':
				nextitem->flags=negation;
				negation=0;
				if (NOMEMORY==AddQuantifier(ptr1,&ptr1,&nextitem,&topitem,&previtem,&vars)){
					FreeLkedLstFormula(topitem);
					return(NULL);
				}
				break;

			/* AND connective. */
			case '&':
				nextitem->flags=ANDOPER;
				AddItem2LkdLstF(&nextitem,&topitem,&previtem);
				break;

			/* OR connective. */
			case '|':
				nextitem->flags=OROPER;
				AddItem2LkdLstF(&nextitem,&topitem,&previtem);
				break;

			/* Implication connective. */
			case '=':
				nextitem->flags=IMPOPER;
				AddItem2LkdLstF(&nextitem,&topitem,&previtem);
				ptr1++;
				break;

			/* Reverse implication, equivalence or XOR connectives. */
			case '<':
				if (ptr1[1]=='='){
					if (ptr1[2]=='>'){
						nextitem->flags=EQUIVOPER;
						ptr1+=2;
					} else {
						nextitem->flags=REVIMPOPER;
						ptr1++;
					}
				} else {
					nextitem->flags=XOROPER;
					ptr1+=2;
				}
				AddItem2LkdLstF(&nextitem,&topitem,&previtem);
				break;

			case '$':
				nextitem->flags=negation;
				negation=0;
				if (ptr1[1]=='t'){
					nextitem->flags|=(NAMEDPRED|TRUEPRED);
					ptr1=&ptr1[4];
				} else {
					nextitem->flags|=(NAMEDPRED|FALSEPRED);
					ptr1=&ptr1[5];
				}
				AddItem2LkdLstF(&nextitem,&topitem,&previtem);
				break;

			/* None of the above. This is a valid predicate, */
			/* function or variable name. */
			default:
				nextitem->flags=negation;
				negation=0;
				if (NOMEMORY==AddName(ptr1,&ptr1,&nextitem,&topitem,&previtem,vars)){
					FreeLkedLstFormula(topitem);
					return(NULL);
				}
				ptr1--;
				break;
		}
	}

	/* Free unneeded storage and return. */
	if (nextitem!=NULL){
		MYFREE(nextitem);
	}
	return(topitem);
} /* Text2LinkedList */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Free formula storage in linked list format)  OCJ
 *
 *    This frees formula storage in linked list format.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to first linked list item to be freed.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void FreeLkedLstFormula(lkedfitem *firstitem){
	auto lkedfitem *ptr1,*ptr2;                         /* Auxiliary pointers */

	/* Free formula storage. */
	for (ptr1=firstitem;ptr1!=NULL;ptr1=ptr2){
		ptr2=ptr1->next;
		if ((UQUANTIFIER|EQUANTIFIER)&ptr1->flags){
			MYFREE(ptr1->ptr.vardata->name);
			MYFREE(ptr1->ptr.vardata);
		}
		MYFREE(ptr1);
	}
	return;
} /* FreeLkedLstFormula */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Remove redundant brackets)  OCJ
 *
 *    This function remove pairs of redundant brackets. Let L be a literal,
 *    S a sub-function, Q1 and Q2 non empty sets of consecutive quantifiers.
 *    Then the following cases of redundant brackets are detected:
 *
 *    Case 1: (...((S))...)
 *    ------
 *    In this case "(...(" and ")...)" are sets of consecutive opening and
 *    corresponding closing brackets possibly with negations on some of them.
 *    All these brackets are removed and only (S) is left as result.
 *
 *    Case 2: (S)
 *    ------
 *    In this case (S) is the whole global formula and the opening bracket
 *    has not a negation. The bracket pair is removed and only S is left.
 *
 *    Case 3: (L)
 *    ------
 *    In this case the opening bracket may have a negation. The bracket pair
 *    is removed and only L is left.
 *
 *    Case 4: (Q1 (Q2 S))
 *    ------
 *    In this case the opening brackets may have a negation. The bracket pair
 *    starting after Q1 is removed leaving (Q1 Q2 S) as a result.
 *
 *    Case 5: Q1 (Q2 S)
 *    ------
 *    In this case "Q1 (Q2 S)" is the whole formula. The bracket pair is
 *    is removed leaving Q1 Q2 S as a result.
 *
 *    In cases 1, 3 and 4 the negation state of the opening bracket being
 *    removed is merged with the item following it.
 *
 *    Other possibly redundant pairs of brackets are not removed
 *    because they were set by PutBrackets() function.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    ptopitem: Address of pointer to top item in formula.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void RemoveRedundantBrackets(lkedfitem **ptopitem){
	auto lkedfitem *item;                               /* Pointer of item being checked */
	auto lkedfitem *ptr1;                               /* Auxiliary pointers */
	auto int ii;                                        /* Auxiliary */

	/* Loop through formula linked list items. */
	for (item=*ptopitem;item!=NULL;item=item->next){

		/* Item is an opening bracket. */
		if (OPENBRACKET&item->flags){

			/* Case 1: set of adjacent brackets. */
			item=RemoveAdjBrackets(item,ptopitem);

			/* Case 2: bracket pair around the whole formula. */
			if ((item==*ptopitem)&&(0==(NEGITEM&item->flags))&&(item->ptr.mtchbrckt->next==NULL)){

				/* Remove the bracket pair. */
				item->ptr.mtchbrckt->prev->next=NULL;
				item->next->prev=NULL;
				*ptopitem=item->next;
				MYFREE(item->ptr.mtchbrckt);
				MYFREE(item);
				item=*ptopitem;

				/* The formula still starts with an open bracket. Apply case 1 */
				/* again and continue with current iteration. */
				if (OPENBRACKET&item->flags){
					item=RemoveAdjBrackets(item,ptopitem);

				/* The formula doesn't starts now with an open bracket. */
				/* Go to the next iteration in the global loop. */
				} else {
					continue;
				}
			}

			/* Case 3: bracket pair around a literal. */
			if ((EQUPRED|NAMEDPRED)&item->next->flags){

				/* The item pointed by ptr1 is the partner closing bracket */
				/* of the formula opening bracket. Remove set of brackets. */
				if (item->ptr.mtchbrckt==JumpOverBlockOrTerm(item->next,PASTEND)){
					item->next->flags^=(NEGITEM&item->flags);
					item->next->prev=item->prev;
					if (item->prev!=NULL){
						item->prev->next=item->next;
					} else {
						*ptopitem=item->next;
					}
					if (item->ptr.mtchbrckt->next!=NULL){
						item->ptr.mtchbrckt->next->prev=item->ptr.mtchbrckt->prev;
					}
					item->ptr.mtchbrckt->prev->next=item->ptr.mtchbrckt->next;
					MYFREE(item->ptr.mtchbrckt);
					ptr1=item->next;
					MYFREE(item);
					item=ptr1;
				}
			}

		/* Cases 4 and 5. */
		} else if (((UQUANTIFIER|EQUANTIFIER)&item->flags)
				&&((item->prev==NULL)||(OPENBRACKET&item->prev->flags))){

			/* Loop until there are no more 4 or 5 cases. */
			for (ii=1;ii;){

				/* Set ptr1 to next item that is not a quantifier. */
				for (ptr1=item->next;(UQUANTIFIER|EQUANTIFIER)&ptr1->flags;ptr1=ptr1->next){
				}

				/* The pointer ptr1 is an opening bracket. */
				ii=0;
				if (OPENBRACKET&ptr1->flags){

					/* Case 1 again: remove set of adjacent brackets. */
					ptr1=RemoveAdjBrackets(ptr1,ptopitem);

					/* The item after ptr1 is a quantifier. Remove redundant pair */
					/* of brackets and check for case 4 or 5 again. */
					if ((UQUANTIFIER|EQUANTIFIER)&ptr1->next->flags){
						ii=1; /* Check for case 4 or 5 again. */
						item=ptr1->next;
						ptr1->next->flags^=(NEGITEM&ptr1->flags);
						if (ptr1->ptr.mtchbrckt->next!=NULL){
							ptr1->ptr.mtchbrckt->next->prev=ptr1->ptr.mtchbrckt->prev;
						}
						ptr1->ptr.mtchbrckt->prev->next=ptr1->ptr.mtchbrckt->next;
						MYFREE(ptr1->ptr.mtchbrckt);
						ptr1->next->prev=ptr1->prev;
						ptr1->prev->next=ptr1->next;
						MYFREE(ptr1);

					/* If the item after ptr1 is not a quantifier then there is no */
					/* case 4. Prepare next global iteration. */
					} else {
						item=ptr1->prev;
					}

				/* If the pointer at ptr1 is not an opening bracket then */
				/* there is no case 4. Prepare next global iteration. */
				} else {
					item=ptr1->prev;
				}
			}
		}
	}
	return;
} /* RemoveRedundantBrackets */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Remove set of adjacent redundant brackets)  OCJ
 *
 *    This function removes a set of adjacent brackets leaving only
 *    the inner pair of brackets. See case 1 in general comments of
 *    RemoveRedundantBrackets() function.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    ptopitem: Address of pointer to top item in formula.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
lkedfitem *RemoveAdjBrackets(lkedfitem *frstbracket,lkedfitem **ptopitem){
	auto lkedfitem *ptr1,*ptr2;                         /* Auxiliary pointers */

	/* Global loop. */
	for (ptr1=frstbracket;;){

		/* Item after ptr1 is a redundant opening bracket. Remove the */
		/* pair of brackets at ptr1 and prepare next iteration. */
		if ((OPENBRACKET&ptr1->next->flags)&&(ptr1->ptr.mtchbrckt->prev==ptr1->next->ptr.mtchbrckt)){
			ptr1->next->flags^=(NEGITEM&ptr1->flags);
			ptr1->ptr.mtchbrckt->prev->next=ptr1->ptr.mtchbrckt->next;
			if (ptr1->ptr.mtchbrckt->next!=NULL){
				ptr1->ptr.mtchbrckt->next->prev=ptr1->ptr.mtchbrckt->prev;
			}
			MYFREE(ptr1->ptr.mtchbrckt);
			ptr2=ptr1->next;
			ptr2->prev=ptr1->prev;
			if (ptr1->prev!=NULL){
				ptr1->prev->next=ptr2;
			} else {
				*ptopitem=ptr2;
			}
			MYFREE(ptr1);
			ptr1=ptr2;

		/* Item after ptr1 is not a redundant opening bracket. */
		/* Leave the loop. */
		} else {
			break;
		}
	}
	return(ptr1);
} /* RemoveAdjBrackets */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Set polarity of linked list items)  OCJ
 *
 *    This function sets the polarity of each appropriate linked list
 *    item in a formula in linked list format. The polarity setting
 *    procedure described in the document "Computing small clause normal
 *    forms" by Andreas Nonnengart and Christoph Weidenbach is followed.
 *    However that document doesn't indicates how to handle the cases
 *    of NAND, NOR and XOR connectives so the following describes how
 *    that cases are handled here.
 *
 *    The polarity is +-1 depending on the number of effective negations
 *    that act on a sub formula. If the sub formula has both positive and
 *    negative polarity in interconnected parts of the formula then polarity
 *    zero is assigned. For instance, the formula A<=>B is equivalent to
 *    (A&B)|(~A&~B) therefore the items A and B are assigned zero polarity.
 *
 *    For the NAND and NOR cases, as A~&B is equivalent to (~A)&(~B) and
 *    A~|B is equivalent to (~A)|(~B) then the NAND and NOR connectives
 *    cause a change in polarity of A and B operands.
 *
 *    For the XOR case, as A<~>B is equivalent to (A|B)&((~A)|(~B)) then
 *    a zero polarity is assigned to operands A and B.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to string with the formula.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void SetPolarity(lkedfitem *formula){
	auto int32_t polarity;                              /* Current polarity */
	auto lkedfitem *ptr1;                               /* Auxiliary pointer */
	auto int32_t ii,jj,kk;                              /* Auxiliary */

	/* Initialize polarity state. */
	polarity=POSPOL;

	/* Loop through formula linked list items. */
	for (;formula!=NULL;formula=formula->next){

		/* Item is an opening bracket, a predicate or a quantifier. */
		if ((OPENBRACKET|EQUPRED|NAMEDPRED|UQUANTIFIER|EQUANTIFIER)&formula->flags){

			/* If item is an opening bracket then set jj to flags of item */
			/* following the bracket block and set the polarity of the closing */
			/* bracket to the current polarity. */
			if (OPENBRACKET&formula->flags){
				if (formula->ptr.mtchbrckt->next!=NULL){
					jj=formula->ptr.mtchbrckt->next->flags;
				} else {
					jj=0;
				}
				formula->ptr.mtchbrckt->flags=(formula->ptr.mtchbrckt->flags&~(POSPOL|NEGPOL|NEUTRALPOL))|polarity;

			/* If item in not an opening bracket set jj to flags of item */
			/* after the full predicate. */
			} else {
				ptr1=JumpOverBlockOrTerm(formula,PASTEND);
				if (ptr1!=NULL){
					jj=ptr1->flags;
				} else {
					jj=0;
				}
			}

			/* Get flags of previous item. */
			if (formula->prev!=NULL){
				kk=formula->prev->flags;
			} else {
				kk=0;
			}

			/* Item is affected by an equivalence or a XOR connective. */
			if (((EQUIVOPER|XOROPER)&jj)||((EQUIVOPER|XOROPER)&kk)){
				formula->flags=(formula->flags&~(POSPOL|NEGPOL|NEUTRALPOL))|NEUTRALPOL;
				if ((OPENBRACKET|UQUANTIFIER|EQUANTIFIER)&formula->flags){
					polarity=NEUTRALPOL;
				}

			/* Item is not affected by an equivalence or XOR connective. */
			} else {

				/* Set ii to NEGITEM if the combination of the item negation state */
				/* and the effect  of an implication, reverse implication, NAND or NOR */
				/* is negative. Otherwise set ii to zero. */
				ii=NEGITEM&formula->flags;
				if (((IMPOPER|NANDOPER|NOROPER)&jj)||((REVIMPOPER|NANDOPER|NOROPER)&kk)){
					ii^=NEGITEM;
				}

				/* Polarity state must be changed. */
				if (ii){
					switch ((POSPOL|NEGPOL|NEUTRALPOL)&polarity){
						case POSPOL:
							formula->flags=(formula->flags&~(POSPOL|NEGPOL|NEUTRALPOL))|NEGPOL;
							break;
						case NEGPOL:
							formula->flags=(formula->flags&~(POSPOL|NEGPOL|NEUTRALPOL))|POSPOL;
							break;
						case NEUTRALPOL:
							formula->flags=(formula->flags&~(POSPOL|NEGPOL|NEUTRALPOL))|NEUTRALPOL;
							break;
					}
					if ((OPENBRACKET|UQUANTIFIER|EQUANTIFIER)&formula->flags){
						polarity=formula->flags&(POSPOL|NEGPOL|NEUTRALPOL);
					}

				/* Polarity state must not be changed. */
				} else {
					formula->flags=(formula->flags&~(POSPOL|NEGPOL|NEUTRALPOL))|polarity;
				}
			}

		/* Item is a closing bracket. Restore polarity state. */
		} else if (CLOSEBRACKET&formula->flags){
			polarity=formula->flags&(POSPOL|NEGPOL|NEUTRALPOL);
		}
	}
	return;
} /* SetPolarity */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add item to formula in linked list format)  OCJ
 *
 *    This function adds an item to formula in linked list format.
 *    The item is inserted just after the item pointed by pinsertpt.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    pitem: On input this is the address of pointer to item to be
 *           inserted. On output the item pointed by pitem is set to NULL.
 *    ptopitem: Address of pointer to linked list top item.
 *    pinserpt: On input this is the address of pointer to insert point
 *              item, NULL if item must be inserted in the first position.
 *              On output this is the address of the inserted item.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void AddItem2LkdLstF(lkedfitem **pitem,lkedfitem **ptopitem,lkedfitem **pinsertpt){
	auto lkedfitem *insertpt;                           /* Pointer to insert point */
	insertpt=(*pitem)->prev=*pinsertpt;
	if (insertpt==NULL){
		(*pitem)->next=*ptopitem;
		if ((*ptopitem)!=NULL){
			(*ptopitem)->prev=*pitem;
		}
		*ptopitem=*pitem;
	} else {
		(*pitem)->next=insertpt->next;
		if (insertpt->next!=NULL){
			insertpt->next->prev=*pitem;
		}
		insertpt->next=*pitem;
	}
	(*pinsertpt)=*pitem;
	(*pitem)=NULL;
	return;
} /* AddItem2LkdLstF */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add open bracket to formula in linked list format)  OCJ
 *
 *    This function adds an open bracket to formula in linked list format
 *
 *
 *
 *  ARGUMENTS:
 *
 *    pitem: Address of pointer to item for bracket. This must be allocated
 *           before calling this function and must have the negation flag
 *           set when appropriate.
 *    ptopitem: Address of pointer to top item.
 *    pinsertpt: Address of pointer to insert point.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t AddOpenBracket(lkedfitem **pitem,lkedfitem **ptopitem,lkedfitem **pinsertpt){

	/* Add Opening bracket item. */
	(*pitem)->flags|=OPENBRACKET;
	AddItem2LkdLstF(pitem,ptopitem,pinsertpt);

	/* Allocate memory for partner closing bracket item. */
	if (NULL==(*pitem=MYALLOC(sizeof(lkedfitem)))){
		return(NOMEMORY);
	}

	/* Add closing bracket item. */
	(*pitem)->flags=CLOSEBRACKET;
	(*pitem)->ptr.mtchbrckt=*pinsertpt;
	(*pinsertpt)->ptr.mtchbrckt=*pitem;
	AddItem2LkdLstF(pitem,ptopitem,pinsertpt);
	*pinsertpt=(*pinsertpt)->prev;
	return(0);
} /* AddOpenBracket */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add quantifier to formula in linked list format)  OCJ
 *
 *    This function adds universal quantifiers to formula in linked list
 *    format. An universal quantifier will be added for each variable.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    quantifier: Pointer to quantifier in text form.
 *    pnexttext: On output this is the address of pointer to next non space
 *               and non tab character after the quantifier in text form.
 *    pitem: Address of pointer to item for first quantifier. This must be
 *           allocated before calling this function and must have the negation
 *           flag set when appropriate.
 *    ptopitem: Address of pointer to top item.
 *    pinsertpt: Address of pointer to insert point.
 *    pvars: Address of pointer to inner active nameschain structure
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t AddQuantifier(char *quantifier,char **pnexttext,lkedfitem **pitem,lkedfitem **ptopitem,
		lkedfitem **pinsertpt,nameschain **pvars){
	auto nameschain *lclvar;                            /* Pointer to local nameschain structure */
	auto int32_t qtype;                                 /* Quantifier type */
	auto char *ptr1;                                    /* Parse pointer */
	auto char *ptr2;                                    /* Auxiliary */
	auto char tempchar;                                 /* Temporary character */

	/* Get quantifier type. */
	if (quantifier[0]=='!'){
		qtype=UQUANTIFIER;
	} else {
		qtype=EQUANTIFIER;
	}

	/* Set parse pointer to first variable in text form. */
	/* It is assumed that the first non blank character after */
	/* the quantifier is '[', but this is not verified. */
	ptr1=SkipBlanks(SkipBlanks(&quantifier[1])+1);

	/* Parse loop. */
	while (1){

		/* Allocate space for new linked list item if necessary. */
		if (*pitem==NULL){
			if (NULL==(*pitem=MYALLOC(sizeof(lkedfitem)))){
				return(NOMEMORY);
			}
			(*pitem)->flags=0;
		}
		(*pitem)->flags|=qtype;
		(*pitem)->dupq=NULL;

		/* Get the variable name. It is assumed that the name */
		/* is valid because the formula is syntactically correct. */
		ParseName(ptr1,&ptr2);

		/* Temporary mark end of variable. */
		tempchar=ptr2[0];
		ptr2[0]=0;

		/* Create and set new names chain. */
		if (NULL==(lclvar=MYALLOC(sizeof(nameschain)))){
			return(NOMEMORY);
		}
		if (NULL==(lclvar->name=MYALLOC(MAX((1+strlen(ptr1)),18)))){
			MYFREE(lclvar);
			return(NOMEMORY);
		}
		strcpy(lclvar->name,ptr1);
		lclvar->next=*pvars;
		lclvar->quantifier=*pitem;
		*pvars=lclvar;
		(*pitem)->ptr.vardata=lclvar;

		/* Reset temporary mark. */
		ptr2[0]=tempchar;

		/* Add linked list item to chain. */
		AddItem2LkdLstF(pitem,ptopitem,pinsertpt);

		/* If next non space character is not a comma then there are */
		/* no more variables so set pnexttext and leave the loop. */
		ptr1=SkipBlanks(ptr2);
		if (ptr1[0]!=','){
			*pnexttext=SkipBlanks(ptr1+1);
			break;
		}

		/* Jump over the comma. */
		ptr1++;

		/* Jump to next variable. */
		ptr1=SkipBlanks(ptr1);
	}
	return(0);
} /* AddQuantifier */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add name to formula in linked list format)  OCJ
 *
 *    This function adds all the required items to include a name
 *    in a formula in list format. The name may be a predicate,
 *    a function or a variable. If it is a function or variable
 *    then this is the start of an (in)equality literal.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    name: Pointer to name in text form.
 *    pnexttext: On output this is the address of pointer to next
 *               non space and non tab character to be processed.
 *    pitem: Address of pointer to item. This must be allocated before
 *           calling this function and must have the negation flag set
 *           when appropriate.
 *    ptopitem: Address of pointer to top item.
 *    pinsertpt: Address of pointer to insert point.
 *    vars: Pointer to inner active nameschain structure
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t AddName(char *name,char **pnexttext,lkedfitem **pitem,lkedfitem **ptopitem,
		lkedfitem **pinsertpt,nameschain *vars){
	auto hashchain *symbol;                             /* Pointer to hashchain structure of name */
	auto char *ptr2;                                    /* Auxiliary */
	auto char tempchar;                                 /* Temporary character */

	/* Isolate name. */
	ParseName(name,&ptr2);
	tempchar=ptr2[0];
	ptr2[0]=0;

	/*  Get name information and check if the name is a predicate or function. */
	symbol=CheckGlblDeclared(name);
	ptr2[0]=tempchar;

	/* The name is a predicate. */
	if ((symbol!=NULL)&&(symbol->type&PREDICATE)){
		*pnexttext=ptr2;
		return(AddPredOrFunc(symbol,pnexttext,pitem,ptopitem,pinsertpt,vars));
	}

	/* The name is a variable or function. This is the start of an (in)equality. */
	return(AddEquality(name,pnexttext,pitem,ptopitem,pinsertpt,vars));
} /* AddName */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add a predicate or function in linked list format)  OCJ
 *
 *    This function adds a predicate or function to a formula in linked
 *    list format. This function is called recursively.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    symbol: Pointer to the hashchain structure for the symbol corresponding
 *            to the predicate or function name.
 *    pnexttext: On input this is the address of pointer to next character
 *               immediately following the name of the function or predicate.
 *               On output this is the address of pointer to next non space
 *               and non tab character to be processed.
 *    pitem: Address of pointer to item for predicate or function. This must
 *           be allocated before calling this function and must have the negation
 *           flag set when appropriate.
 *    ptopitem: Address of pointer to top item.
 *    pinsertpt: Address of pointer to insert point.
 *    vars: Pointer to inner active nameschain structure
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t AddPredOrFunc(hashchain *symbol,char **pnexttext,lkedfitem **pitem,lkedfitem **ptopitem,
		lkedfitem **pinsertpt,nameschain *vars){
	auto char *ptr1;                                    /* Auxiliary pointer */
	auto nameschain *ptr2;                              /* Auxiliary pointer */
	auto char tempchar;                                 /* Temporary character */
	auto int32_t ii;                                    /* Auxiliary */

	/* Add linked list item for predicate or function. */
	if (symbol->type&PREDICATE){
		(*pitem)->flags|=NAMEDPRED;
	} else {
		(*pitem)->flags=NAMEDFUNC;
	}
	(*pitem)->ptr.symbol=symbol;
	AddItem2LkdLstF(pitem,ptopitem,pinsertpt);

	/* Character after name is an open bracket so there are arguments. */
	if ((**pnexttext)=='('){

		/* Add bracket linked list items. */
		if (NULL==(*pitem=MYALLOC(sizeof(lkedfitem)))){
			return(NOMEMORY);
		}
		(*pitem)->flags=0;
		if (NOMEMORY==AddOpenBracket(pitem,ptopitem,pinsertpt)){
			return(NOMEMORY);
		}

		/* Loop through arguments. */
		*pnexttext=SkipBlanks((*pnexttext)+1);
		while ((**pnexttext)!=')'){

			/* Allocate memory for next linked list item. */
			if (NULL==(*pitem=MYALLOC(sizeof(lkedfitem)))){
				return(NOMEMORY);
			}

			/* Isolate name. */
			ParseName(*pnexttext,&ptr1);
			tempchar=ptr1[0];
			ptr1[0]=0;

			/* Check if this is a variable name by looking at the quantifiers */
			/* chain of nameschain structures in scope. */
			for (ptr2=vars,ii=0;ptr2!=NULL;ptr2=ptr2->next){
				if (0==strcmp(*pnexttext,ptr2->name)){
					ii=1;
					break;
				}
			}

			/* Variable name. */
			if (ii){
				ptr1[0]=tempchar;
				*pnexttext=SkipBlanks(ptr1);
				(*pitem)->flags=NAMEDVAR;
				(*pitem)->ptr.vardata=ptr2;
				AddItem2LkdLstF(pitem,ptopitem,pinsertpt);

			/* The name is a function. Get name information and add */
			/* the function linked list item. */
			} else {
				symbol=CheckGlblDeclared(*pnexttext);
				ptr1[0]=tempchar;
				*pnexttext=ptr1;
				if (AddPredOrFunc(symbol,pnexttext,pitem,ptopitem,pinsertpt,vars)){
					return(NOMEMORY);
				}
			}

			/* Skip a possible comma and go to next non blank character. */
			if ((**pnexttext)==','){
				*pnexttext=SkipBlanks((*pnexttext)+1);
			}
		}

		/* Go to first non blank character after the closing bracket. */
		*pnexttext=SkipBlanks((*pnexttext)+1);

		/* Set insert point after the closing bracket. */
		*pinsertpt=(*pinsertpt)->next;

	/* Name has no arguments. */
	} else {
		*pnexttext=SkipBlanks(*pnexttext);
	}

	/* Free memory and return. */
	if ((*pitem)!=NULL){
		MYFREE(*pitem);
	}
	return(0);
} /* AddPredOrFunc */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add an (in)equality in linked list format)  OCJ
 *
 *    This function adds an (in)equality to a formula in linked
 *    list format.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    name: Pointer to name of first root term of (in)equality in text form.
 *    pnexttext: On output this is the address of pointer to next non space
 *               and non tab character after the equality in text form.
 *    pitem: Address of pointer to item for (in)equality. This must be allocated
 *           before calling this function and must have the negation flag set
 *           when appropriate.
 *    ptopitem: Address of pointer to top item.
 *    pinsertpt: Address of pointer to insert point.
 *    vars: Pointer to inner active nameschain structure
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t AddEquality(char *name,char **pnexttext,lkedfitem **pitem,lkedfitem **ptopitem,
		lkedfitem **pinsertpt,nameschain *vars){
	auto lkedfitem *equitem;                            /* Pointer to linkedlist item for (in)equality */

	/* Add (in)equality linked list item. */
	(*pitem)->flags|=EQUPRED;
	equitem=*pitem;
	AddItem2LkdLstF(pitem,ptopitem,pinsertpt);

	/* Add first (in)equality root term. */
	if (AddEquRootTerm(name,pnexttext,ptopitem,pinsertpt,vars)){
		return(NOMEMORY);
	}

	/* Update negation flag if necessary. */
	if ((**pnexttext)=='!'){
		equitem->flags^=NEGITEM;
		(*pnexttext)+=2;
	} else {
		(*pnexttext)++;
	}

	/* Add second (in)equality root term and return. */
	name=SkipBlanks(*pnexttext);
	return(AddEquRootTerm(name,pnexttext,ptopitem,pinsertpt,vars));
} /* AddEquality */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add an (in)equality root term in linked list format)  OCJ
 *
 *    This function adds an (in)equality root term to a formula in linked
 *    list format.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    name: Pointer to name of root term of (in)equality in text form.
 *    pnexttext: On output this is the address of pointer to next non space
 *               and non tab character after the root term in text form.
 *    pitem: Address of pointer to item for (in)equality root term. This
 *           must be allocated before calling this function.
 *    ptopitem: Address of pointer to top item.
 *    pinsertpt: Address of pointer to insert point.
 *    vars: Pointer to inner active nameschain structure
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t AddEquRootTerm(char *name,char **pnexttext,lkedfitem **ptopitem,
		lkedfitem **pinsertpt,nameschain *vars){
	auto lkedfitem *item;                               /* Pointer to linked list item */
	auto hashchain *symbol;                             /* Pointer to symbol hashchain structure */
	auto char *ptr1;                                    /* Parse pointer */
	auto nameschain *ptr2;                              /* Auxiliary pointer */
	auto char tempchar;                                 /* Temporary character */
	auto int32_t ii;                                    /* Auxiliary */

	/* Allocate storage for next linked list item. */
	if (NULL==(item=MYALLOC(sizeof(lkedfitem)))){
		return(NOMEMORY);
	}
	item->flags=0;

	/* Isolate name. */
	ParseName(name,&ptr1);
	tempchar=ptr1[0];
	ptr1[0]=0;

	/* Check if this is a variable name by looking at the quantifiers */
	/* chain of nameschain structures in scope. */
	for (ptr2=vars,ii=0;ptr2!=NULL;ptr2=ptr2->next){
		if (0==strcmp(name,ptr2->name)){
			ii=1;
			break;
		}
	}

	/* Variable name. */
	if (ii){
		ptr1[0]=tempchar;
		*pnexttext=SkipBlanks(ptr1);
		item->flags=NAMEDVAR;
		item->ptr.vardata=ptr2;
		AddItem2LkdLstF(&item,ptopitem,pinsertpt);

	/* The name is a function. Get name information and add */
	/* the function linked list item. */
	} else {
		symbol=CheckGlblDeclared(name);
		ptr1[0]=tempchar;
		*pnexttext=ptr1;
		if (AddPredOrFunc(symbol,pnexttext,&item,ptopitem,pinsertpt,vars)){
			return(NOMEMORY);
		}
	}
	return(0);
} /* AddEquRootTerm */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Duplicate a block of linked list items)  OCJ
 *
 *    This function builds a duplicate of a sequence of linked list
 *    items that are part of a formula in linked list format.
 *
 *    The last item in the block result of the duplication must
 *    have its next field set to NULL. This is achieved by using
 *    the function AddItem2LkdLstF() to link the new items.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    frstorg: Pointer to first item of block to be duplicated.
 *    lastorg: Pointer to last item of block to be duplicated.
 *    pfrstdst: Address of pointer to first item of block built
 *              by the duplication.
 *    plastdst: Address of pointer to last item of block built
 *              by the duplication.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t DuplicateBlock(lkedfitem *frstorg,lkedfitem *lastorg,lkedfitem **pfrstdst,lkedfitem **plastdst){
	auto lkedfitem *orgitem,*dstitem;                   /* Pointers to origin and destination items */
	auto lkedfitem *insertpt;                           /* Insert point in destination block */
	auto lkedfitem *newq;                               /* Pointer to new quantifier */
	auto nameschain *vars;                              /* Pointer to inner active nameschain structure */

	/* Loop through items in block to be duplicated. */
	vars=NULL;
	orgitem=NULL;
	*pfrstdst=NULL;
	do {

		/* Update pointers for next iteration. */
		if (orgitem==NULL){
			orgitem=frstorg;
			insertpt=dstitem=NULL;
		} else {
			orgitem=orgitem->next;
		}

		/* Allocate memory for the destination block if necessary. */
		if (dstitem==NULL){
			if (NULL==(dstitem=MYALLOC(sizeof(lkedfitem)))){
				FreeLkedLstFormula(*pfrstdst);
				return(NOMEMORY);
			}
		}

		/* Process depending on the destination item. */
		dstitem->flags=orgitem->flags;
		switch ((OPENBRACKET|CLOSEBRACKET|UQUANTIFIER|EQUANTIFIER|NAMEDVAR|NAMEDPRED|NAMEDFUNC)&dstitem->flags){

			/* Opening bracket. */
			case OPENBRACKET:
				if (NOMEMORY==AddOpenBracket(&dstitem,pfrstdst,&insertpt)){
					FreeLkedLstFormula(*pfrstdst);
					return(NOMEMORY);
				}
				if (insertpt->ptr.mtchbrckt->next==NULL){
					*plastdst=insertpt->ptr.mtchbrckt;
				}
				break;

			/* Closing bracket. */
			case CLOSEBRACKET:
				insertpt=insertpt->next;
				if ((UQUANTIFIER|EQUANTIFIER)&insertpt->ptr.mtchbrckt->next->flags){
					vars=insertpt->ptr.mtchbrckt->next->ptr.vardata->next;
				}
				break;

			/* Quantifier. */
			case UQUANTIFIER:
			case EQUANTIFIER:
				newq=DuplicateQuantifier(orgitem,&insertpt,pfrstdst,vars);
				orgitem->dupq=newq;
				vars=newq->ptr.vardata;
				break;

			/* Variable. */
			case NAMEDVAR:
				if (orgitem->ptr.vardata->quantifier->dupq==NULL){
					dstitem->ptr.vardata=orgitem->ptr.vardata;
				} else {
					dstitem->ptr.vardata=orgitem->ptr.vardata->quantifier->dupq->ptr.vardata;
				}
				AddItem2LkdLstF(&dstitem,pfrstdst,&insertpt);
				break;

			/* Predicate or function. */
			case NAMEDPRED:
			case NAMEDFUNC:
				dstitem->ptr.symbol=orgitem->ptr.symbol;
				AddItem2LkdLstF(&dstitem,pfrstdst,&insertpt);
				break;

			/* Remaining items. */
			default:
				AddItem2LkdLstF(&dstitem,pfrstdst,&insertpt);
				break;
		}

		/* Update pointer to last item of block built by duplication. */
		if (insertpt->next==NULL){
			*plastdst=insertpt;
		}
	} while (orgitem!=lastorg);

	/* Free unused memory. */
	if (dstitem!=NULL){
		MYFREE(dstitem);
	}

	/* Reset original quantifier dupq fields to NULL. */
	orgitem=NULL;
	do {
		if (orgitem==NULL){
			orgitem=frstorg;
		} else {
			orgitem=orgitem->next;
		}
		if ((UQUANTIFIER|EQUANTIFIER)&orgitem->flags){
			orgitem->dupq=NULL;
		}
	} while (orgitem!=lastorg);

	return(0);
} /* DuplicateBlock */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Insert a block of linked list items)  OCJ
 *
 *    This function inserts a block of linked list items after
 *    an insert point in a formula in linked list format.
 *
 *    The last item in the block to be inserted must have the
 *    next field set to NULL.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    blockstart: Pointer to first linked item in block.
 *    blockstart: Pointer to last linked item in block.
 *    insertpt: Pointer to linked item in formula after which the
 *              block will be inserted. This pointer cannot be NULL.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
void InsertBlock(lkedfitem *blockstart,lkedfitem *blockend,lkedfitem *insertpt){
	blockstart->prev=insertpt;
	if (insertpt->next!=NULL){
		insertpt->next->prev=blockend;
	}
	blockend->next=insertpt->next;
	insertpt->next=blockstart;
	return;
} /* InsertBlock */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Jump over a block or term)  OCJ
 *
 *    This function jumps over a block or term. A block is defined
 *    as a single literal or a sub-formula enclosed in brackets that
 *    contains at least one literal.
 *
 *    The jump may be done to the last item of the block or literal
 *    or to the item following the last item of the block or literal,
 *    depending on the value of the flags parameter.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    item: Pointer to first linked item in block or term.
 *    flags: TOEND if jump must be done to the last item of block or term.
 *           PASTEND if jump must be done to the item following the last
 *           item of block or term.
 *
 *  RETURNS:
 *
 *    Pointer to the appropriate item at the end of the block or term.
 *
 *--------------------------------------------------------------*/
lkedfitem *JumpOverBlockOrTerm(lkedfitem *item,int32_t flags){

	/* Jump over equality or block starting with a bracket. */
	/* Jump over block starting with a bracket and return. */
	if (OPENBRACKET&item->flags){
		if (flags==TOEND){
			return(item->ptr.mtchbrckt);
		} else {
			return(item->ptr.mtchbrckt->next);
		}

	/* Jump over block starting with an equality and return. */
	} else if (EQUPRED&item->flags){
		return(JumpOverBlockOrTerm(JumpOverBlockOrTerm(item->next,PASTEND),flags));
	}

	/* Jump over named predicate, named function or variable and return. */
	if ((item->next!=NULL)&&(OPENBRACKET&item->next->flags)){
		if (flags==TOEND){
			return(item->next->ptr.mtchbrckt);
		} else {
			return(item->next->ptr.mtchbrckt->next);
		}
	}
	if (flags==TOEND){
		return(item);
	}
	return(item->next);
} /* JumpOverBlockOrTerm */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Miniscope a formula)  OCJ
 *
 *    This function miniscopes all the quantifiers in a formula. The
 *    miniscoping is performed from right to left, that is, quantifiers
 *    to the right are miniscoped first.
 *
 *    It is assumed that the PutBrackets() function has been called
 *    before and that RemoveRedundantBracktes() function has been
 *    called just before calling this function. This is needed by
 *    MiniscopeSegment() function which is indirectly called by
 *    this function.
 *
 *    Miniscoping may be stopped and/or cancelled if it takes too long.
 *    This may happen in some rare cases, see for instances problem
 *    HWV061+1.
 *
 *    This function may set procctl->status to NOMEMORY.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    ptopitem: Address of pointer to linked list top item of
 *              formula to be miniscoped.
 *    formula: Pointer to cmprefix structure of formula in text
 *             form.
 *    pbuffer: Address of pointer to buffer for formula conversion
 *             to text.
 *    psize: Pointer to buffer size.
 *
 *  RETURNS:
 *
 *    Pointer to cmprefix structure of converted formula in text
 *    form or NULL if not enough memory or timeout.
 *
 *--------------------------------------------------------------*/
cmprefix *MiniscopeFormula(lkedfitem **ptopitem,cmprefix *formula,char **pbuffer,int32_t *psize){
	auto nameschain *lastn;                             /* Last active quantifier nameschain structure */
	auto struct timeval inittime;                       /* Time when miniscoping was started. */
	auto lkedfitem *ptr1,*ptr2;                         /* Auxiliary pointers */
	auto int32_t ii,jj;                                 /* Auxiliary */

	/* Remove unneeded quantifiers. */
	ii=RemoveUnneededQfiers(ptopitem);

	/* Initialize initial miniscoping time. */
	gettimeofday(&inittime,NULL);

	/* Get the rightmost quantifier in formula. Return if there are no quantifiers. */
	for (ptr1=*ptopitem,ptr2=NULL;ptr1!=NULL;ptr1=ptr1->next){
		if ((UQUANTIFIER|EQUANTIFIER)&ptr1->flags){
			ptr2=ptr1;
		}
	}
	if (ptr2==NULL){
		return(formula);
	}

	/* Loop through quantifiers from right to left. */
	for (;ptr2!=NULL;ptr2=ptr1){
		ptr1=ptr2->prev;
		if ((UQUANTIFIER|EQUANTIFIER)&ptr2->flags){
			switch (jj=MiniscopeQuantifier(ptr2,ptopitem,&inittime)){
				case NOMEMORY:
					procctl->status=NOMEMORY;
				/* No break. */
				case 2:
					if (ii==0){
						return(NULL);
					}
					break;
			}
			ii|=jj;
		}
	}

	/* Re-link quantifiers vardata fields. */
	for (ptr1=*ptopitem,lastn=NULL;ptr1!=NULL;ptr1=ptr1->next){
		if ((UQUANTIFIER|EQUANTIFIER)&ptr1->flags){
			ptr1->ptr.vardata->next=lastn;
			lastn=ptr1->ptr.vardata;
		} else if ((CLOSEBRACKET&ptr1->flags)&&((UQUANTIFIER|EQUANTIFIER)&ptr1->ptr.mtchbrckt->next->flags)){
			lastn=ptr1->ptr.mtchbrckt->next->ptr.vardata->next;
		}
	}

	/* The formula has been miniscoped. Add the resulting text */
	/* formula to main KB and update number of inferences. */
	if (ii){
		pbmstats->miniscoping++;
		RemoveRedundantBrackets(ptopitem);
		if (NULL==(formula=AddLkdLstFormula2KB(*ptopitem,formula,NULL,MINISCOPING,pbuffer,psize))){
			procctl->status=NOMEMORY;
			return(NULL);
		}
	}

	return(formula);
} /* MiniscopeFormula */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Remove redundant quantifiers)  OCJ
 *
 *    This function removes quantifiers that are not needed. This may
 *    happen if the same variable name is repeated in a chain of
 *    quantifiers or a quantified variable is not used within its
 *    scope. Although these conditions are rare they may happen
 *    (see for instance ruleX8 formula in GEO012+0.ax TPTP axiom
 *    file).
 *
 *
 *
 *
 *  ARGUMENTS:
 *
 *    ptopitem: Address of pointer to linked list top item of
 *              formula to be miniscoped.
 *
 *  RETURNS:
 *
 *    0 -> No quantifiers were removed.
 *    1 -> Some quantifiers were removed.
 *
 *--------------------------------------------------------------*/
int32_t RemoveUnneededQfiers(lkedfitem **ptopitem){
	auto lkedfitem *ptr1,*ptr2,*ptr3,*ptr4;             /* Auxiliary pointers */
	auto int32_t ii;                                    /* Auxiliary */

	/* Loop through linked list items and remove quantifiers */
	/* that are not needed. */
	for (ptr1=*ptopitem,ii=0;ptr1!=NULL;ptr1=ptr2){

		/* Item is a quantifier. */
		if ((UQUANTIFIER|EQUANTIFIER)&ptr1->flags){

			/* Get the start of the block in scope. */
			for (ptr2=ptr1->next;;ptr2=ptr2->next){
				if (0==((UQUANTIFIER|EQUANTIFIER)&ptr2->flags)){
					break;
				}
			}

			/* Loop through the chain of quantifiers. */
			for (ptr3=ptr1;ptr3!=ptr2;ptr3=ptr4){

				/* Check if the item in variable is in block in scope */
				/* and remove the quantifier if the variable is not found. */
				ptr4=ptr3->next;
				if (!IsVarInBlock(ptr2,ptr3)){
					ii=1;
					RemoveLinkedListChain(ptopitem,ptr3,ptr3);
				}
			}
			ptr2=ptr2->next; /* Prepare next iteration. */

		/* Prepare next iteration. */
		} else {
			ptr2=ptr1->next;
		}
	}

	return(ii);
} /* RemoveUnneededQfiers */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Miniscope a quantifier)  OCJ
 *
 *    This function miniscopes one quantifier in a formula. The
 *    quantifier is deleted if miniscoping is successful or the
 *    quantified variable doesn't exist in the quantified block.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    quantifier: Pointer to quantifier.
 *    ptopitem: Address of pointer to top item in formula.
 *    pinittime: Pointer to timeval structure with miniscoping
 *               initial time.
 *
 *  RETURNS:
 *
 *    0 -> The segment cannot be miniscoped.
 *    1 -> The segment was successfully miniscoped.
 *    2 -> Miniscope time limit exceeded.
 *    NOMEMORY -> No memory available.
 *
 *--------------------------------------------------------------*/
int32_t MiniscopeQuantifier(lkedfitem *quantifier,lkedfitem **ptopitem,
		struct timeval *pinittime){
	auto lkedfitem *block;                              /* Pointer to quantified block */
	auto int32_t ii;                                    /* Auxiliary */

	/* Skip quantifiers following the current quantifier and reach */
	/* the quantified block. Check if there is a quantifier of different */
	/* type in the quantifiers following the current quantifier. */
	/* If there is a quantifier of different type following the current */
	/* quantifier then miniscoping is not possible. This is because */
	/* quantifiers of different types don't commute. For instance */
	/* the formula (![X]: ?[Y]: sum(X,Y)=zero) is true but the formula */
	/* (?[Y]: ![X]: sum(X,Y)=ZERO) is false. */
	for (block=quantifier->next;(UQUANTIFIER|EQUANTIFIER)&block->flags;block=block->next){
		if (((UQUANTIFIER|EQUANTIFIER)&quantifier->flags)!=((UQUANTIFIER|EQUANTIFIER)&block->flags)){
			return(0);
		}
	}

	/* The quantified variable exists in the block. */
	if (IsVarInBlock(block,quantifier)){

		/* Return if the quantified block is a single literal. */
		if ((EQUPRED|NAMEDPRED)&block->flags){
			return(0);
		}

		/* Return if miniscoping is not possible inside the quantified block */
		/* or time limit exceeded or a NOMEMORY condition arises. */
		if (1!=(ii=MiniscopeSegment(quantifier,block->next,ptopitem,pinittime))){
			return (ii);
		}
	}

	/* Quantifier has been miniscoped or variable doesn't exist */
	/* in the quantifier block. Delete quantifier and its */
	/* nameschain structure and return. */
	MYFREE(quantifier->ptr.vardata->name);
	MYFREE(quantifier->ptr.vardata);
	quantifier->next->prev=quantifier->prev;
	if (quantifier->prev!=NULL){
		quantifier->prev->next=quantifier->next;
	} else {
		*ptopitem=quantifier->next;
	}
	MYFREE(quantifier);
	return(1);
} /* MiniscopeQuantifier */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Miniscope a formula segment)  OCJ
 *
 *    This function miniscopes a formula segment. A segment is
 *    defined as the sub formula that ends at the end of the
 *    formula or the first unmatched closing bracket from the
 *    start of the segment, whichever occurs first. It is assumed
 *    that the miniscoped variable exist in the segment.
 *
 *    This function assumes that the PutBrackets() function has
 *    been called before and that RemoveRedundantBracktes()
 *    function has been called just before calling MiniscopeFormula().
 *
 *    Let Q be a quantifier, B1 and B2 blocks and * an AND or OR
 *    connective. See general comments in PutBrackets() function
 *    for the definition of a block. Due to the redundant brackets
 *    added by PutBrackets() function and the subsequent removal
 *    of redundant brackets by RemoveRedundantBrackets() function
 *    the segment can be only in the forms "Q B1" or "B1*B2".
 *
 *    Miniscoping is successful if the segment is in the form "Q B1"
 *    or the miniscoping can be applied either to B1, B2 or both.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    quantifier: Pointer to quantifier.
 *    segment: Pointer to start of segment.
 *    ptopitem: Address of pointer to top item in formula.
 *    pinittime: Pointer to timeval structure with miniscoping
 *               initial time.
 *
 *  RETURNS:
 *
 *    0 -> The segment cannot be miniscoped.
 *    1 -> The segment was successfully miniscoped.
 *    2 -> Miniscope time limit exceeded.
 *    NOMEMORY -> No memory available.
 *
 *--------------------------------------------------------------*/
int32_t MiniscopeSegment(lkedfitem *quantifier,lkedfitem *segment,lkedfitem **ptopitem,
		struct timeval *pinittime){
	auto lkedfitem *block1,*block2;                     /* Pointer to blocks in segment */
	auto lkedfitem *connective;                         /* Connective in segment */
	auto int32_t nvar1,nvar2;                           /* Set to 1 if quantified variable exist in blocks 1 and 2, 0 otherwise. */
	auto double time;                                   /* Elapsed time in seconds */
	auto int32_t ii,jj;                                 /* Auxiliary */

	/* Check miniscope time limit. */
	gettimeofday(&mainkb.endtime,NULL);
	time=mainkb.endtime.tv_sec-pinittime->tv_sec
			+(mainkb.endtime.tv_usec-pinittime->tv_usec)/1000000.0;
	if (time>=MAXMINISCOPETIME){
		return(2);
	}

	/* Skip quantifiers at the start of the segment and reach the first block */
	/* in the segment. Check if there is a quantifier of different */
	/* type in the quantifiers following the current quantifier. */
	/* If there is a quantifier of different type following the current */
	/* quantifier then miniscoping is not possible. This is because */
	/* quantifiers of different types don't commute. For instance */
	/* the formula (![X]: ?[Y]: sum(X,Y)=zero) is true but the formula */
	/* (?[Y]: ![X]: sum(X,Y)=ZERO) is false. */
	for (block1=segment;(UQUANTIFIER|EQUANTIFIER)&block1->flags;block1=block1->next){
		if (((UQUANTIFIER|EQUANTIFIER)&quantifier->flags)!=((UQUANTIFIER|EQUANTIFIER)&block1->flags)){
			return(0);
		}
	}

	/* Check if quantified variable exist in first block. */
	nvar1=IsVarInBlock(block1,quantifier);

	/* The segment is in the form "Q B1". Also segments of the type B1 are */
	/* checked just in case, although they cannot happen here in theory.*/
	connective=JumpOverBlockOrTerm(block1,PASTEND);
	if ((block1!=segment)||(connective==NULL)||(0==((ANDOPER|OROPER)&connective->flags))){
		return(MiniscopeBlock(quantifier,block1,segment,ptopitem,pinittime));
	}

	/* The segment is in the form "B1*B2". Check if quantified variable */
	/* exist in second block. */
	block2=connective->next;
	nvar2=IsVarInBlock(block2,quantifier);

	/* Miniscope the first block if possible. */
	ii=0;
	jj=(UQUANTIFIER&quantifier->flags)&&(ANDOPER&connective->flags);
	jj=(jj)||((EQUANTIFIER&quantifier->flags)&&(OROPER&connective->flags));
	if (nvar1&&((nvar2==0)||jj)){
		if (1!=(ii=MiniscopeBlock(quantifier,block1,block1,ptopitem,pinittime))){
			return(ii);
		}
	}

	/* Miniscope the second block if possible and return. */
	if (nvar2&&((nvar1==0)||jj)){
		if (1!=(jj=MiniscopeBlock(quantifier,block2,block2,ptopitem,pinittime))){
			return(jj);
		}
		ii=1;
	}
	return(ii);
} /* MiniscopeSegment */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Miniscope a formula block)  OCJ
 *
 *    This function miniscopes a formula block. See general comments
 *    in PutBrackets() function for the definition of a block. It is
 *    assumed that the block is miniscopable.
 *
 *    The quantifier will be inserted just before the insert point
 *    passed as argument. If the insert point is the start of the
 *    block to be miniscoped then a bracket set will also be added
 *    enclosing from the new quantifier to the end of the block.
 *    If the insert point in not the start of the block then it is
 *    assumed that the insert point is a valid quantifier set and
 *    no bracket set will be added.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    quantifier: Pointer to quantifier.
 *    block: Pointer to start of block.
 *    insertpt: Pointer to item. The new quantifier will be inserted
 *              just before this item.
 *    ptopitem: Address of pointer to top item in formula.
 *    pinittime: Pointer to timeval structure with miniscoping
 *               initial time.
 *
 *  RETURNS:
 *
 *    1 -> Function completed successfully.
 *    2 -> Miniscope time limit exceeded.
 *    NOMEMORY -> No memory available.
 *
 *--------------------------------------------------------------*/
int32_t MiniscopeBlock(lkedfitem *quantifier,lkedfitem *block,lkedfitem *insertpt,
		lkedfitem **ptopitem,struct timeval *pinittime){
	auto lkedfitem *newq;                               /* New miniscoped quantifier pointer */
	auto lkedfitem *openbrkt,*closebrkt;                /* Pointers to new opening and closing brackets */
	auto lkedfitem *ptr1;                               /* Auxiliary pointer */
	auto int32_t ii;                                    /* Auxiliary */

	/* If the block is not a single literal then try to miniscope the */
	/* quantifier inside the block. Due to RemoveRedundantBrackets() */
	/* a single literal cannot be enclosed between brackets and */
	/* due to PutBrackets if the block is not a single literal it */
	/* must be enclosed into brackets. */
	if (OPENBRACKET&block->flags){
		if (0!=(ii=MiniscopeSegment(quantifier,block->next,ptopitem,pinittime))){
			return(ii);
		}
	}

	/* Quantifier cannot be further miniscoped inside the block. */
	/* Miniscope the block. First insert brackets if necessary. */
	if (block==insertpt){

		/* Allocate memory for the new brackets. */
		if (NULL==(openbrkt=MYALLOC(sizeof(lkedfitem)))){
			return(NOMEMORY);
		}
		if (NULL==(closebrkt=MYALLOC(sizeof(lkedfitem)))){
			MYFREE(openbrkt);
			return(NOMEMORY);
		}

		/* Insert opening and closing brackets. */
		openbrkt->flags=OPENBRACKET;
		openbrkt->ptr.mtchbrckt=closebrkt;
		closebrkt->flags=CLOSEBRACKET;
		closebrkt->ptr.mtchbrckt=openbrkt;
		ptr1=insertpt->prev;
		AddItem2LkdLstF(&openbrkt,ptopitem,&ptr1);
		ptr1=JumpOverBlockOrTerm(block,TOEND);
		AddItem2LkdLstF(&closebrkt,ptopitem,&ptr1);
	}

	/* Insert the new quantifier, re-link variables and return. */
	if (NULL==(newq=DuplicateQuantifier(quantifier,&insertpt->prev,ptopitem,NULL))){
		return(NOMEMORY);
	}
	RelinkVars(block,quantifier,newq);
	return(1);
} /* MiniscopeBlock */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if a variable exists in a block)  OCJ
 *
 *    This function checks if a variable exists in a block. See
 *    general comments in PutBrackets() function for the definition
 *    of a block.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    block: Pointer to start of block.
 *    quantifier: Pointer to quantifier.
 *
 *  RETURNS:
 *
 *    0 -> The variable doesn't exist in the block.
 *    1 -> The variable exist in the block.
 *
 *--------------------------------------------------------------*/
int32_t IsVarInBlock(lkedfitem *block,lkedfitem *quantifier){
	auto lkedfitem *ptr1,*ptr2;                         /* Auxiliary pointer */

	/* Loop through the block items. */
	ptr2=JumpOverBlockOrTerm(block,PASTEND);
	for (ptr1=block;ptr1!=ptr2;ptr1=ptr1->next){
		if ((NAMEDVAR&ptr1->flags)&&(ptr1->ptr.vardata==quantifier->ptr.vardata)){
			return(1);
		}
	}
	return(0);
} /* IsVarInBlock */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Rebuild links of variables in a block)  OCJ
 *
 *    This function scans a block for variables linked to an old
 *    quantifier and links those variables to a new quantifier.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    block: Pointer to start of block.
 *    oldq: Pointer to old quantifier.
 *    newq: Pointer to new quantifier.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void RelinkVars(lkedfitem *block,lkedfitem *oldq,lkedfitem *newq){
	auto lkedfitem *ptr1,*ptr2;                         /* Auxiliary pointer */

	/* Loop through the block items. */
	ptr2=JumpOverBlockOrTerm(block,TOEND);
	for (ptr1=block;ptr1!=ptr2->next;ptr1=ptr1->next){
		if ((NAMEDVAR&ptr1->flags)&&(ptr1->ptr.vardata==oldq->ptr.vardata)){
			ptr1->ptr.vardata=newq->ptr.vardata;
		}
	}
	return;
} /* RelinkVars */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Duplicate and insert a quantifier)  OCJ
 *
 *    This function duplicates a given quantifier and inserts it
 *    after a given insert point.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    quantifier: Pointer to quantifier.
 *    pinsertpt: Address of pointer to insert point. The new quantifier
 *               will be inserted just after this item.
 *    ptopitem: Address of pointer to top item in formula.
 *    vars: Pointer to nameschain structure to which the new quantifier
 *          nameschain structure will be linked. If this pointer is NULL
 *          the new quantifier nameschain structure will be linked to the
 *          same nameschain structure as the old quantifier nameschain
 *          structure.
 *
 *  RETURNS:
 *
 *    Pointer to new quantifier or NULL if not enough memory.
 *
 *--------------------------------------------------------------*/
lkedfitem *DuplicateQuantifier(lkedfitem *quantifier,lkedfitem **pinsertpt,lkedfitem **ptopitem,
		nameschain *vars){
	auto lkedfitem *newq;                               /* Pointer to new quantifier */

	/* Allocate memory for the new quantifier and its nameschain structure. */
	if (NULL==(newq=MYALLOC(sizeof(lkedfitem)))){
		return(NULL);
	}
	if (NULL==(newq->ptr.vardata=MYALLOC(sizeof(nameschain)))){
		MYFREE(newq);
		return(NULL);
	}
	if (NULL==(newq->ptr.vardata->name=MYALLOC(1+strlen(quantifier->ptr.vardata->name)))){
		MYFREE(newq->ptr.vardata);
		MYFREE(newq);
		return(NULL);
	}

	/* Build the quantifier data and insert new quantifier. */
	newq->flags=quantifier->flags;
	strcpy(newq->ptr.vardata->name,quantifier->ptr.vardata->name);
	newq->ptr.vardata->quantifier=newq;
	newq->dupq=NULL;
	if (vars==NULL){
		newq->ptr.vardata->next=quantifier->ptr.vardata->next;
	} else {
		newq->ptr.vardata->next=vars;
	}
	AddItem2LkdLstF(&newq,ptopitem,pinsertpt);
	return(*pinsertpt);
} /* DuplicateQuantifier */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Skolemize a formula)  OCJ
 *
 *    This function skolemizes existential quantifiers and variables
 *    in a formula.
 *
 *    The skolem function name created for a variable declared in an
 *    existential qualifier is built by appending nnn to skfprefix
 *    where nnn is the next skolem function number to be assigned
 *    according to the numskolem parameter.
 *
 *    Due to CASC requirements each skolemization inference must contain
 *    one skolemize record per each skolemized variable, see
 *    https://tptp.org/UserDocs/QuickGuide/Derivations.html for details.
 *    In order to do this the parent2 pointer in the cmprefix structure
 *    points to a set of pointers to skldata structures. For more
 *    information see cmprefix and skldata structures comments
 *    in global.h include file.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    ptopitem: Address of pointer to linked list top item of
 *              formula to be skolemized.
 *    formula: Pointer to cmprefix structure of formula in text
 *             form.
 *    pbuffer: Address of pointer to buffer for formula conversion
 *             to text.
 *    psize: Pointer to buffer size.
 *
 *  RETURNS:
 *
 *    Pointer to cmprefix structure of converted formula in text
 *    form or NULL if not enough memory.
 *
 *--------------------------------------------------------------*/
cmprefix *Skolemize(lkedfitem **ptopitem,cmprefix *formula,char **pbuffer,int32_t *psize){
	auto lkedfitem *openbrkt,*closebrkt;                /* Pointers to new opening and closing brackets */
	auto lkedfitem *varitem;                            /* Pointer to variable item that is argument of skolem function */
	auto skldata **skldataptr;                          /* Pointer to set of pointers to skldata structures */
	auto int32_t sklsize;                               /* Number of elements allocated for set of skldata pointers */
	auto char skname[40];                               /* Skolem name */
	#ifdef ENABLECHOICEAXIOM
	auto cmprefix *choiceaxiom;                         /* Pointer to axiom of choice formula */
	#endif
	auto lkedfitem *ptr1,*ptr3;                         /* Auxiliary pointers */
	auto nameschain *ptr2;                              /* Auxiliary pointer */
	auto skldata **ptr4;                                /* Auxiliary pointer */
	auto char *ptr5;                                    /* Auxiliary pointer */
	auto uint64_t ii;                                   /* Auxiliary */
	auto int32_t jj,kk,mm,ss;                           /* Auxiliary */

	/* Initialize pointer to set of pointers to skldata structures. */
	skldataptr=NULL;

	/* Loop through the formula items. */
	kk=0;
	for (ptr1=*ptopitem;ptr1!=NULL;ptr1=ptr1->next){

		/* Item is an existential quantifier. */
		if (EQUANTIFIER&ptr1->flags){

			/* Allocate initial memory for set of pointers to skldata structures. */
			kk++;
			if (skldataptr==NULL){
				sklsize=10;
				if (NULL==(skldataptr=MYALLOC(sklsize*sizeof(*skldataptr)))){
					return(NULL);
				}
				strcpy(skname,SKLPREFIX);

			/* Increase the size of skldata pointers if necessary. */
			} else if (kk>=sklsize){
				sklsize+=10;
				if (NULL==(ptr4=MYREALLOC(skldataptr,sklsize*sizeof(*skldataptr)))){
					for (jj=0;skldataptr[jj]!=NULL;jj++){
						MYFREE(skldataptr[jj]);
					}
					MYFREE(skldataptr);
					return (NULL);
				}
				skldataptr=ptr4;
			}

			/* Create skolem name and update skolem number identifications. */
			/* Set ii to the skolem number assigned to new skolem. */
			sprintf(&skname[strlen(SKLPREFIX)],"%lu",ii=numskolem+skolemid);
			strcat(skname,SKLPOSTFIX);
			numskolem++;

			/* Get skolem symbol arity and set variable ii to the sum of lengths */
			/* of names in sklvarnames field of skldata structure. */
			ss=strlen(ptr1->ptr.vardata->name);
			for (jj=0,ptr2=ptr1->ptr.vardata->next;ptr2!=NULL;ptr2=ptr2->next){
				if (UQUANTIFIER&ptr2->quantifier->flags){
					ss+=strlen(ptr2->name);
					jj++;
				}
			}

			/* Create skolem symbol and adjust symbol SKOLEM flag. */
			if (NULL==(ptr1->data.sksymbol=CreateGlobalSymbol(skname,jj,FUNCTION|SKOLEM))){
				for (jj=0;skldataptr[jj]!=NULL;jj++){
					MYFREE(skldataptr[jj]);
				}
				MYFREE(skldataptr);
				return(NULL);
			}
			ptr1->data.sksymbol->type|=SKOLEM;

			/* Allocate and set new skldata structure and add pointer to skldataptr set. */
			mm=sizeof(skldata); /* Temporal, para comprobación. */
			if (NULL==(skldataptr[kk-1]=MYALLOC(mm+ss+jj))){
				for (jj=0;skldataptr[jj]!=NULL;jj++){
					MYFREE(skldataptr[jj]);
				}
				MYFREE(skldataptr);
				return(NULL);
			}
			skldataptr[kk-1]->sklnumber=ii;
			skldataptr[kk]=NULL;
			strcpy(ptr5=&skldataptr[kk-1]->sklvarnames[0],ptr1->ptr.vardata->name);
			for (ptr2=ptr1->ptr.vardata->next,ptr5=&ptr5[1+strlen(ptr5)];ptr2!=NULL;ptr2=ptr2->next){
				if (UQUANTIFIER&ptr2->quantifier->flags){
					ptr5=strcpy(ptr5,ptr2->name);
					ptr5=&ptr5[1+strlen(ptr5)];
				}
			}
			ptr5[0]=0;

		/* Item is an existentially quantified variable. */
		} else if ((NAMEDVAR&ptr1->flags)&&(EQUANTIFIER&ptr1->ptr.vardata->quantifier->flags)){

			/* Transform variable item in function item. */
			ptr1->flags=NAMEDFUNC;
			ptr2=ptr1->ptr.vardata;
			ptr1->ptr.symbol=ptr2->quantifier->data.sksymbol;

			/* Set ptr2 to next nameschain that is an universal quantifier. */
			for (ptr2=ptr2->next;(ptr2!=NULL)&&(EQUANTIFIER&ptr2->quantifier->flags);
					ptr2=ptr2->next){
			}

			/* If skolem arity is not zero then add skolem arguments. */
			if (ptr2!=NULL){

				/* Add brackets. */
				if (NULL==(openbrkt=MYALLOC(sizeof(lkedfitem)))){
					for (jj=0;skldataptr[jj]!=NULL;jj++){
						MYFREE(skldataptr[jj]);
					}
					MYFREE(skldataptr);
					return(NULL);
				}
				if (NULL==(closebrkt=MYALLOC(sizeof(lkedfitem)))){
					for (jj=0;skldataptr[jj]!=NULL;jj++){
						MYFREE(skldataptr[jj]);
					}
					MYFREE(skldataptr);
					MYFREE(openbrkt);
					return(NULL);
				}
				openbrkt->flags=OPENBRACKET;
				openbrkt->ptr.mtchbrckt=closebrkt;
				closebrkt->flags=CLOSEBRACKET;
				closebrkt->ptr.mtchbrckt=openbrkt;
				ptr3=ptr1;
				AddItem2LkdLstF(&openbrkt,ptopitem,&ptr3);
				AddItem2LkdLstF(&closebrkt,ptopitem,&ptr3);
				ptr3=ptr3->ptr.mtchbrckt;

				/* Add arguments. */
				for (;ptr2!=NULL;ptr2=ptr2->next){
					if (UQUANTIFIER&ptr2->quantifier->flags){
						if (NULL==(varitem=MYALLOC(sizeof(lkedfitem)))){
							for (jj=0;skldataptr[jj]!=NULL;jj++){
								MYFREE(skldataptr[jj]);
							}
							MYFREE(skldataptr);
							return(NULL);
						}
						varitem->flags=NAMEDVAR;
						varitem->ptr.vardata=ptr2;
						varitem->data.sksymbol=NULL;
						AddItem2LkdLstF(&varitem,ptopitem,&ptr3);
					}
				}
			}
		}
	}

	#ifdef ENABLECHOICEAXIOM
	/* The formula has been skolemized. */
	if (kk){

		/* Set skldataptr set size to the real space used. */
		if (sklsize>(kk+1)){
			if (NULL==(ptr4=MYREALLOC(skldataptr,(kk+1)*sizeof(*skldataptr)))){
				for (jj=0;skldataptr[jj]!=NULL;jj++){
					MYFREE(skldataptr[jj]);
				}
				MYFREE(skldataptr);
				return (NULL);
			}
			skldataptr=ptr4;
		}

		/* Allocate and initializa the know part of the axiom of choice for the skolemization. */
		if (NULL==(choiceaxiom=MYALLOC(sizeof(cmprefix)))){
			for (jj=0;skldataptr[jj]!=NULL;jj++){
				MYFREE(skldataptr[jj]);
			}
			MYFREE(skldataptr);
			return(NULL);
		}
		if (NULL==(choiceaxiom->part2.text=MYALLOC(ss=7+(3*(mm=strlen(formula->part2.text)))))){
			for (jj=0;skldataptr[jj]!=NULL;jj++){
				MYFREE(skldataptr[jj]);
			}
			MYFREE(skldataptr);
			MYFREE(choiceaxiom);
			return(NULL);
		}
		choiceaxiom->parent1=(cmprefix *)skldataptr;
		choiceaxiom->parent2=NULL;
		choiceaxiom->flags=0;
		strcpy(choiceaxiom->part2.text,"(");
		strcat(choiceaxiom->part2.text,formula->part2.text);
		strcat(choiceaxiom->part2.text,")=>(");

		/* Add the resulting skolemized text formula to main KB and update number of inferences. */
		/* The skolemized formula is assigned to formula variable. */
		pbmstats->skolemize++;
		RemoveRedundantBrackets(ptopitem);
		formula=AddLkdLstFormula2KB(*ptopitem,choiceaxiom,formula,SKOLEMIZATION,pbuffer,psize);
		if (formula==NULL){
			for (jj=0;skldataptr[jj]!=NULL;jj++){
				MYFREE(skldataptr[jj]);
			}
			MYFREE(skldataptr);
			MYFREE(choiceaxiom->part2.text);
			MYFREE(choiceaxiom);
			return(NULL);
		}

		/* Set the choice axiom formula text to its final size and add the remaining */
		/* text to the choice axiom formula. */
		if (ss!=(mm=(7+mm+strlen(formula->part2.text)))){
			if (NULL==(ptr5=MYREALLOC(choiceaxiom->part2.text,mm))){
				MYFREE(choiceaxiom->part2.text);
				MYFREE(choiceaxiom);
				for (jj=0;skldataptr[jj]!=NULL;jj++){
					MYFREE(skldataptr[jj]);
				}
				MYFREE(skldataptr);
				return(NULL);
			}
			choiceaxiom->part2.text=ptr5;
		}
		strcat(choiceaxiom->part2.text,formula->part2.text);
		strcat(choiceaxiom->part2.text,")");

		/* Add choice axiom formula to KB. */
		AddTxtFormula2KB(choiceaxiom,CHOICEAXIOM,&mainkb);
	}
	#else
	/* The formula has been skolemized. */
	if (kk){

		/* Set skldataptr set size to the real space used. */
		if (sklsize>(kk+1)){
			if (NULL==(ptr4=MYREALLOC(skldataptr,(kk+1)*sizeof(*skldataptr)))){
				for (jj=0;skldataptr[jj]!=NULL;jj++){
					MYFREE(skldataptr[jj]);
				}
				MYFREE(skldataptr);
				return (NULL);
			}
			skldataptr=ptr4;
		}

		/* Add the resulting skolemized text formula to main KB and update number of inferences. */
		/* The skolemized formula is assigned to formula variable. */
		pbmstats->skolemize++;
		RemoveRedundantBrackets(ptopitem);
		formula=AddLkdLstFormula2KB(*ptopitem,formula,(cmprefix *)skldataptr,SKOLEMIZATION,pbuffer,psize);
		if (formula==NULL){
			for (jj=0;skldataptr[jj]!=NULL;jj++){
				MYFREE(skldataptr[jj]);
			}
			MYFREE(skldataptr);
			return(NULL);
		}
	}
	#endif
	return(formula);
} /* Skolemize */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Simplify $true and $false predicates)  OCJ
 *
 *    This function simplify $true and $false predicates in a formula.
 *    The formula is assumed to be converted to nnf form and miniscoped.
 *    The end result is a formula with a single $true or a single $false
 *    or no $true or $false predicates.
 *
 *
 *  ARGUMENTS:
 *
 *    ptopitem: Address of pointer to linked list top item of
 *              formula to be simplified.
 *    formula: Pointer to cmprefix structure of formula in text
 *             form.
 *    pbuffer: Address of pointer to buffer for formula conversion
 *             to text.
 *    psize: Pointer to buffer size.
 *
 *  RETURNS:
 *
 *    Pointer to cmprefix structure of converted formula in text
 *    form.
 *
 *--------------------------------------------------------------*/
cmprefix *TFSimplify(lkedfitem **ptopitem,cmprefix *formula,char **pbuffer,int32_t *psize){
	auto lkedfitem *item;                               /* Item in linked list formula */
	auto lkedfitem *ptr1,*ptr2;                         /* Auxiliary pointers */
	auto int32_t ii,jj;                                 /* Auxiliary */

	/* Loop until no simplifications are done. */
	for (ii=0,jj=1;jj;){

		/* Remove redundant brackets. */
		RemoveRedundantBrackets(ptopitem);

		/* Loop through items in linked list formula. */
		for (item=*ptopitem,jj=0;item!=NULL;item=item->next){

			/* Item is the start of a set of quantifiers. */
			if ((UQUANTIFIER|EQUANTIFIER)&item->flags){

				/* Set ptr1 to the start of the quantified block */
				/* of this set of quantifiers. */
				for (ptr1=item->next;(UQUANTIFIER|EQUANTIFIER)&ptr1->flags;ptr1=ptr1->next){
				}

				/* If the quantified block is a single $true or $false then */
				/* delete the set of quantifiers. */
				if ((TRUEPRED|FALSEPRED)&ptr1->flags){
					ii=jj=1;
					RemoveLinkedListChain(ptopitem,item,ptr1->prev);
				}


				/* Set item to ptr1. */
				item=ptr1;
			}

			/* Set ptr1 to the connective if there is a connective */
			/* after the block starting at item. */
			if ((OPENBRACKET|EQUPRED|NAMEDPRED)&item->flags){
				if (NULL==(ptr1=JumpOverBlockOrTerm(item,PASTEND))){
					if ((EQUPRED|NAMEDPRED)&item->flags){
						item=JumpOverBlockOrTerm(item,TOEND);
					}
					continue;
				}
			} else {
				continue;
			}

			/* Connective is an AND. */
			if (ANDOPER&ptr1->flags){

				/* First operand is a $true predicate or second operand is a $false */
				/* predicate. Delete the connective and the first operand. */
				if ((TRUEPRED&item->flags)||(FALSEPRED&ptr1->next->flags)){
					ii=jj=1;
					ptr2=ptr1->next;
					RemoveLinkedListChain(ptopitem,item,ptr1);
					item=ptr2;

				/* First operand is a $false predicate or second operand is a $true */
				/* predicate. Delete the connective and the second operand. */
				} else if ((FALSEPRED&item->flags)||(TRUEPRED&ptr1->next->flags)){
					ii=jj=1;
					RemoveLinkedListChain(ptopitem,ptr1,JumpOverBlockOrTerm(ptr1->next,TOEND));
				}

			/* Connective is an OR. */
			} else if (OROPER&ptr1->flags){

				/* First operand is a $false predicate or second operand is a $true */
				/* predicate. Delete the connective and the first operand. */
				if ((FALSEPRED&item->flags)||(TRUEPRED&ptr1->next->flags)){
					ii=jj=1;
					ptr2=ptr1->next;
					RemoveLinkedListChain(ptopitem,item,ptr1);
					item=ptr2;

				/* First operand is a $true predicate or second operand is a $false */
				/* predicate. Delete the connective and the second operand. */
				} else if ((TRUEPRED&item->flags)||(FALSEPRED&ptr1->next->flags)){
					ii=jj=1;
					RemoveLinkedListChain(ptopitem,ptr1,JumpOverBlockOrTerm(ptr1->next,TOEND));
				}
			}

			/* If item is a predicate set item to last item in predicate. */
			if ((EQUPRED|NAMEDPRED)&item->flags){
				item=JumpOverBlockOrTerm(item,TOEND);
			}
		}
	}

	/* The formula has been simplified. Add the resulting text */
	/* formula to main KB and update number of inferences. */
	if (ii){
		pbmstats->tfsimplif++;
		return(AddLkdLstFormula2KB(*ptopitem,formula,NULL,TFSIMPLIFICATION,pbuffer,psize));
	}
	return(formula);
} /* TFSimplify */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Rename variables in a formula)  OCJ
 *
 *    This function renames variables declared in the universal
 *    quantifiers of a formula. The variables are assigned standard
 *    names of the form Xnnnn where nnnn is the next variable number
 *    to be assigned, starting at 0.
 *
 *    Both the quantifier and variable items are modified according
 *    to the following:
 *    - The variable number is stored in the data.varnumver field
 *      of the item slkedfitem structure.
 *    - The flag RENAMED is added to the flags field of the item
 *      slkedfitem structure.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to top item in formula.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void RenameVariables(lkedfitem *formula){
	auto int32_t varnumber;                             /* Next variable number ot be assigned */
	auto lkedfitem *ptr1;                               /* Auxiliary pointer */

	/* Loop through the formula items. */
	varnumber=0;
	for (ptr1=formula;ptr1!=NULL;ptr1=ptr1->next){

		/* Item is an universal quantifier. */
		if (UQUANTIFIER&ptr1->flags){
			ptr1->data.varnumber=varnumber;
			ptr1->flags|=RENAMED;
			varnumber++;

		/* Item is an existentially quantified variable. */
		} else if ((NAMEDVAR&ptr1->flags)&&(UQUANTIFIER&ptr1->ptr.vardata->quantifier->flags)){
			ptr1->data.varnumber=ptr1->ptr.vardata->quantifier->data.varnumber;
			ptr1->flags|=RENAMED;
		}
	}
	return;
} /* RenameVariables */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add linked list formula to main KB in text form)  OCJ
 *
 *    This function converts a linked list formula to text form and
 *    adds it to main KB.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    formula: Pointer to top linked list item in formula.
 *    parent1: Pointer to first parent.
 *    parent2: Pointer to second parent.
 *    inference: Inference where formula is coming from.
 *    pbuffer: Address of pointer of auxiliary buffer to be used
 *             for formula conversion to text. This way an already
 *             allocated buffer can be reused. If NULL then a new
 *             buffer will be allocated.
 *    pbuffsize: Address of *pbuffer size.
 *
 *  RETURNS:
 *
 *    Pointer to new cmprefix structure of converted formula
 *    or NULL if not enough memory.
 *
 *--------------------------------------------------------------*/
cmprefix * AddLkdLstFormula2KB(lkedfitem *formula,cmprefix *parent1,cmprefix *parent2,int32_t inference,
		char **pbuffer,int32_t *pbuffsize){
	auto char *buffer;                                  /* Local buffer if no buffer is provided */
	auto int32_t size;                                  /* Local buffer size in bytes */
	auto cmprefix *ptr1;                                /* Auxiliary pointer */
	auto int32_t ii;                                    /* Auxiliary */

	/* Allocate new buffer if necessary. */
	if (pbuffer==NULL){
		if (NULL==(buffer=MYALLOC(STR_CHUNK_SIZE))){
			return(NULL);
		}
		size=STR_CHUNK_SIZE;
		pbuffer=&buffer;
		pbuffsize=&size;
		ii=1;
	} else {
		ii=0;
	}

	/* Convert linked list formula to text. If the inference is skolemization */
	/* then don't include the existential quantifiers and the corresponding */
	/* enclosing brackets in the conversion. This is necessary because in */
	/* this case the quantifiers have not been removed yet. */
	if (NOMEMORY==LinkedList2Text(formula,pbuffer,pbuffsize,inference==SKOLEMIZATION?0:1)){
		return(NULL);
	}

	/* Allocate storage for new text formula. */
	if (NULL==(ptr1=MYALLOC(sizeof(cmprefix)))){
		return(NULL);
	}
	if (NULL==(ptr1->part2.text=MYALLOC(1+strlen(*pbuffer)))){
		MYFREE(ptr1);
		return(NULL);
	}

	/* Set new text clause and add it to main KB. */
	ptr1->parent1=parent1;
	ptr1->parent2=parent2;
	ptr1->flags=0;
	strcpy(ptr1->part2.text,*pbuffer);
	AddTxtFormula2KB(ptr1,inference,&mainkb);

	/* Free local buffer if necessary. */
	if (ii){
		MYFREE(buffer);
	}

	return(ptr1);
} /* AddLkdLstFormula2KB */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Formula renaming)  OCJ
 *
 *    This function renames subformulas in a given formula to
 *    obtain the minimal number of clauses in the standard CNF
 *    conversion performed thereafter.
 *
 *    The concepts of block and segment are widely used in this function
 *    and the called functions. See general comments in PutBrackets()
 *    function for the definition of a block. See general comments
 *    in MiniscopeSegment() function for the definition of a segment.
 *
 *    As most functions in this module the methodology followed
 *    here is mainly based in the document "Computing small clause
 *    normal forms" by Andreas Nonnengart and Christoph Weidenbach.
 *
 *    This function set procctl->status in case of NOMEMORY or
 *    TIMEOUT conditions.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    ptopitem: Address of pointer to linked list top item of
 *              formula to be processed for renaming.
 *    formula: Pointer to cmprefix structure of formula in text
 *             form.
 *    pbuffer: Address of pointer to buffer for formula conversion
 *             to text.
 *    psize: Pointer to buffer size.
 *
 *  RETURNS:
 *
 *    Pointer to cmprefix structure of converted formula in text
 *    form or NULL if not enough memory or timeout.
 *
 *--------------------------------------------------------------*/
cmprefix *FormulaRenaming(lkedfitem **ptopitem,cmprefix *formula,char **pbuffer,int32_t *psize){
	auto cmprefix *newformula;                          /* Pointer to cmprefix structure of renamed formula */
	auto cmprefix *lastpdef;                            /* Pointer to cmprefix structure of last predicate definition */
	auto int32_t ii;                                    /* Auxiliary */

	/* Set polarity. */
	SetPolarity(*ptopitem);

	/* Calculate bounds of the number of clauses generated */
	/* for each block in the formula. */
	GetClauseNumberBounds(*ptopitem,NULL,NULL);

	/* Apply renaming to the whole formula considered as a segment. */
	lastpdef=NULL;
	switch (ii=SegmentRenaming(ptopitem,*ptopitem,1,0,&lastpdef,formula,NULL)){
		case NOMEMORY:
		case TIMEOUT:
			procctl->status=ii;
			return(NULL);
			break;
	}

	/* Some sub-formulas were renamed. Add the resulting text formula to */
	/* main KB and update number of inferences. */
	if (ii){
		pbmstats->formularenaming++;
		RemoveRedundantBrackets(ptopitem);
		if (NULL!=(newformula=AddLkdLstFormula2KB(*ptopitem,formula,NULL,FORMULARENAMING,pbuffer,psize))){
			newformula->parent2=lastpdef;
		}
		formula=newformula;
	}
	return(formula);
} /* FormulaRenaming */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Get lower bounds of number of clauses in a segment)  OCJ
 *
 *    This function gets lower bounds of the number of clauses generated
 *    in a segment of a formula using a standard CNF conversion. Let F be
 *    the sub formula in the segment. This function calculates bounds
 *    for the number of clauses generated by both F and ~F. Table 2 of the
 *    "Computing small clause normal forms" by Andreas Nonnengart and
 *    Christoph Weidenbach is used for this calculation. The polarity
 *    of the segment itself is not taken into account because the calculation
 *    is done for both F and ~F.
 *
 *    The concepts of block and segment are widely used in this function
 *    and the called functions. See general comments in PutBrackets()
 *    function for the definition of a block. See general comments
 *    in MiniscopeSegment() function for the definition of a segment.
 *
 *    Let Q be a possibly empty set of quantifiers, B1 and B2 blocks and *
 *    an AND, OR, XOR or equivalence connective. Then, due to the previous
 *    pre-processing there are only two possible segment configurations:
 *    - Configuration Q B1
 *    - Configuration B1 * B2
 *
 *    If any or both blocks in the segment are enclosed in brackets then
 *    the function is called recursively for the segment inside each block.
 *    The results are stored in the ptr.clauses.posclauses and
 *    ptr.clauses.posclauses fields of the lkedfitem structure of the
 *    linked list item of the enclosing opening bracket.
 *
 *    For blocks that are single literals then the number of clauses is one
 *    for both positive and negative literals.
 *
 *    Once the number of clauses in all the blocks of the segment are known
 *    the number of clauses of the segment is calculated using the table 2
 *    mentioned above. This table doesn't include the case of a XOR operation.
 *    However the results for a XOR operation are the same as those for the
 *    equivalence operation after switching the p(psi) and p(~psi) columns.
 *
 *    See general comments in MiniscopeSegment() function for the definition
 *    of a segment.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    segment: Pointer to first  linked list item of the segment.
 *    pposclauses: Pointer to integer where the lower bound of the number of
 *                 clauses for the positive version of the segment will be stored.
 *                 This may have the values 1, 2, 3 or 4. Values 1 to 3 are exact,
 *                 value 4 is a lower bound. Not used if NULL.
 *    pnegclauses: Pointer to integer where the lower bound of the number of
 *                 clauses for the negative version of the segment will be stored.
 *                 This may have the values 1, 2, 3 or 4. Values 1 to 3 are exact,
 *                 value 4 is a lower bound. Not used if pposclauses is NULL.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void GetClauseNumberBounds(lkedfitem *segment,int32_t *pposclauses,int32_t *pnegclauses){
	auto lkedfitem *block1,*block2;                     /* Pointers to blocks in segment */
	auto int32_t posclbl1,negclbl1,posclbl2,negclbl2;   /* Bounds of number of clauses in each block */
	auto int32_t ii;                                    /* Auxiliary */

	/* Skip quantifiers at the start of the segment and reach the first block */
	/* in the segment. */
	for (block1=segment;(UQUANTIFIER|EQUANTIFIER)&block1->flags;block1=block1->next){
	}

	/* Get number of clauses for positive and negative version */
	/* of first block. */
	if (OPENBRACKET&block1->flags){
		GetClauseNumberBounds(block1->next,&posclbl1,&negclbl1);
		if (NEGITEM&block1->flags){
			ii=posclbl1;
			posclbl1=negclbl1;
			negclbl1=ii;
		}
	} else {
		posclbl1=negclbl1=1;
	}
	block1->data.clauses.posclauses=posclbl1;
	block1->data.clauses.negclauses=negclbl1;

	/* If there is a second block in segment get number of clauses for */
	/* positive and negative version of second block. */
	block2=JumpOverBlockOrTerm(block1,PASTEND);
	if ((block2!=NULL)
			&&((ANDOPER|OROPER|XOROPER|EQUIVOPER)&block2->flags)){
		block2=block2->next;
		if (OPENBRACKET&block2->flags){
			GetClauseNumberBounds(block2->next,&posclbl2,&negclbl2);
			if (NEGITEM&block2->flags){
				ii=posclbl2;
				posclbl2=negclbl2;
				negclbl2=ii;
			}
		} else {
			posclbl2=negclbl2=1;
		}
		block2->data.clauses.posclauses=posclbl2;
		block2->data.clauses.negclauses=negclbl2;
	} else {
		block2=NULL;
	}

	/* The posclauses parameter is not NULL. */
	if (pposclauses!=NULL){
		if (block2!=NULL){
			CalcSegmentBounds(pposclauses,pnegclauses,posclbl1,negclbl1,posclbl1,
					negclbl1,(ANDOPER|OROPER|XOROPER|EQUIVOPER)&block2->prev->flags);
		} else {
			*pposclauses=posclbl1;
			*pnegclauses=negclbl1;
		}
	}
	return;
} /* GetClauseNumberBounds */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Calculate lower bounds of number of clauses in a segment)  OCJ
 *
 *    This function calculates the lower bounds of the number of clauses
 *    generated in a segment of a formula with the configuration B1 * B2
 *    (see general comments of GetClauseNumberBounds() function for a
 *    description of this configuration and how the bounds are calculated).
 *
 *    The concepts of block and segment are widely used in this function
 *    and the called functions. See general comments in PutBrackets()
 *    function for the definition of a block. See general comments
 *    in MiniscopeSegment() function for the definition of a segment.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    pposclauses: Pointer to integer where the lower bound of the number of
 *                 clauses for the positive version of the segment will be stored.
 *                 This may have the values 1, 2, 3 or 4. Values 1 to 3 are exact,
 *                 value 4 is a lower bound. Not used if NULL.
 *    pnegclauses: Pointer to integer where the lower bound of the number of
 *                 clauses for the negative version of the segment will be stored.
 *                 This may have the values 1, 2, 3 or 4. Values 1 to 3 are exact,
 *                 value 4 is a lower bound. Not used if pposclauses is NULL.
 *    posclbl1: Positive bound of first operand.
 *    negclbl1: Negative bound of first operand.
 *    posclbl2: Positive bound of second operand.
 *    negclbl2: Negative bound of second operand.
 *    connective: Connective of operands. May be ANDOPER, OROPER, XOROPER or EQUIVOPER.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void CalcSegmentBounds(int32_t *pposclauses,int32_t *pnegclauses,int32_t posclbl1,int32_t negclbl1,
		int32_t posclbl2,int32_t negclbl2,int32_t connective){

	/* Calculate number of clauses. */
	switch (connective){
		case ANDOPER:
			*pposclauses=posclbl1+posclbl2;
			*pnegclauses=negclbl1*negclbl2;
			break;
		case OROPER:
			*pposclauses=posclbl1*posclbl2;
			*pnegclauses=negclbl1+negclbl2;
			break;
		case EQUIVOPER:
			*pposclauses=(posclbl1*negclbl2)+(negclbl1*posclbl2);
			*pnegclauses=(posclbl1*posclbl2)+(negclbl1*negclbl2);
			break;
		case XOROPER:
			*pposclauses=(posclbl1*posclbl2)+(negclbl1*negclbl2);
			*pnegclauses=(posclbl1*negclbl2)+(negclbl1*posclbl2);
			break;
	}

	/* Set lower bounds. */
	if ((*pposclauses)>4){
		*pposclauses=4;
	}
	if ((*pnegclauses)>4){
		*pnegclauses=4;
	}
	return;
} /* CalcSegmentBounds */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Rename blocks within a segment)  OCJ
 *
 *    This function renames blocks within a segment. First a renaming of
 *    inner blocks is attempted by means of a recursive call. The a and b
 *    coefficients for the recursive call are computed calling the function
 *    GetaABCoefs(). The needed p(phi_i) functions have been previously
 *    calculated by GetClauseNumberBounds() function.
 *
 *    The concepts of block and segment are widely used in this function
 *    and the called functions. See general comments in PutBrackets()
 *    function for the definition of a block. See general comments
 *    in MiniscopeSegment() function for the definition of a segment.
 *
 *    If the renaming at an inner level was not performed then a renaming at the
 *    current level is attempted by checking the bound constraints indicated
 *    in the document mentioned above.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    ptopitem: Address of pointer to linked list top item of
 *              formula to be processed for renaming.
 *    segment: Pointer to first linked list item in the segment.
 *    acoef: Coefficient "a" that determines how often the segment sub-formula is
 *           duplicated in the course of a standard CNF translation.
 *    bcoef: Coefficient "b" that determines how often the negation of the segment
 *           sub-formula is duplicated in the course of a standard CNF translation.
 *    lastpdef: Address of pointer where the address of last predicate definition
 *              cmprefix structure will be placed.
 *    formula: Pointer to cmprefix structure of whole formula that is being renamed
 *             in text format.
 *    vars: Pointer to inner active nameschain structure.
 *
 *  RETURNS:
 *
 *    0 if no renaming was performed.
 *    1 if some renaming was performed at the current or inner level.
 *    NOMEMORY if not enough memory.
 *    TIMEOUT if timeout.
 *
 *--------------------------------------------------------------*/
int32_t SegmentRenaming(lkedfitem **ptopitem,lkedfitem *segment,int32_t acoef,int32_t bcoef,
		cmprefix **lastpdef,cmprefix *formula,nameschain *vars){
	auto lkedfitem *block1,*block2;                     /* Pointers to blocks in segment */
	auto lkedfitem *block1prev;                         /* Item preceding block1 */
	auto lkedfitem *block;                              /* Pointer to current block in segment */
	auto int32_t acoefbl,bcoefbl;                       /* "a" and "b" coefficients in block */
	auto double timedif;                                /* Elapsed time in seconds */
	auto int32_t ii,jj,kk;                              /* Auxiliary */

	/* Skip quantifiers at the start of the segment and reach the first block */
	/* in the segment. */
	for (block1=segment;(UQUANTIFIER|EQUANTIFIER)&block1->flags;block1=block1->next){
		vars=block1->ptr.vardata;
	}
	block1prev=block1->prev;

	/* Get pointer to second block in segment. */
	block2=JumpOverBlockOrTerm(block1,PASTEND);
	if ((block2!=NULL)&&((ANDOPER|OROPER|XOROPER|EQUIVOPER)&block2->flags)){
		block2=block2->next;
	} else {
		block2=NULL;
	}

	/* Loop through blocks in segment. */
	acoefbl=bcoefbl=jj=0; /* Just to avoid compiler warnings. */
	for (block=block1,ii=0,kk=1;block!=NULL;block=(block==block1?block2:NULL),kk++){

		/* Block is not a literal. */
		if (OPENBRACKET&block->flags){

			/* Check condition for renaming current block. In addition to the */
			/* condition of minimizing the number of clauses the block will not */
			/* be renamed if it starts with quantifiers. */
			GetaABCoefs(acoef,bcoef,block1,block2,kk,&acoefbl,&bcoefbl);
			if ((UQUANTIFIER|EQUANTIFIER)&block->next->flags){
				jj=0;
			} else {
				switch ((POSPOL|NEGPOL|NEUTRALPOL)&block->ptr.mtchbrckt->flags){
					case POSPOL:
						if (((acoefbl-1)*(block->data.clauses.posclauses-1))>=1){
							jj=1;
						} else {
							jj=0;
						}
						break;
					case NEGPOL:
						if (((bcoefbl-1)*(block->data.clauses.negclauses-1))>=1){
							jj=1;
						} else {
							jj=0;
						}
						break;
					case NEUTRALPOL:
						if ((((acoefbl-1)*(block->data.clauses.posclauses-1))
								+((bcoefbl-1)*(block->data.clauses.negclauses-1)))>=2){
							jj=1;
						} else {
							jj=0;
						}
						break;
				}
			}

			/* Current block must be renamed. */
			if (jj){

				/* Rename block. */
				if (NOMEMORY==RenameBlock(ptopitem,block,lastpdef,formula,vars)){
					return(NOMEMORY);
				}
				ii|=jj;

				/* Recalculate bounds of the number of clauses generated */
				/* for each block in the formula. */
				GetClauseNumberBounds(*ptopitem,NULL,NULL);

				/* If this is the iteration for first block then update pointer to first block. */
				if (kk==1){
					if (block1prev==NULL){
						block1=*ptopitem;
					} else {
						block1=block1prev->next;
					}
				}
			}

			/* Inner renaming failed or was not performed, try to rename */
			/* an inner sub-formula in the block. */
			if (jj==0){
				if (NEGITEM&block->flags){
					jj=SegmentRenaming(ptopitem,block->next,bcoefbl,acoefbl,lastpdef,formula,vars);
					if ((jj==NOMEMORY)||(jj==TIMEOUT)){
						return(jj);
					}
				} else {
					jj=SegmentRenaming(ptopitem,block->next,acoefbl,bcoefbl,lastpdef,formula,vars);
					if ((jj==NOMEMORY)||(jj==TIMEOUT)){
						return(jj);
					}
				}
				ii|=jj;
			}

			/* Check timeout. */
			if (procctl->status==TIMEOUT){
				gettimeofday(&mainkb.endtime,NULL);
				timedif=mainkb.prstats.elapsed_time+mainkb.endtime.tv_sec-mainkb.time.tv_sec
						+(mainkb.endtime.tv_usec-mainkb.time.tv_usec)/1000000.0;
				glblstats.elapsed_time+=timedif;
				return(TIMEOUT);
			}
		}
	}
	return(ii);
} /* SegmentRenaming */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Get "a" and "b" coefficients of a block)  OCJ
 *
 *    This function calculates the "a" and "b" coefficients of a block in
 *    a sub-formula as a function of the "a" and "b" coefficients of
 *    sub-formula. The sub-formula may be a single block or B1*B2 where B1 and
 *    B2 are blocks, one of which is the block for which the coefficients
 *    are being calculated, and * is an AND, OR, XOR or equivalence connective.
 *    See general comments of GetClauseNumberBounds() function for information
 *    about the methodology and concepts of this calculations.
 *
 *    The concepts of block and segment are widely used in this function
 *    and the called functions. See general comments in PutBrackets()
 *    function for the definition of a block. See general comments
 *    in MiniscopeSegment() function for the definition of a segment.
 *
 *    The a and b coefficients for the recursive call are computed using table 3
 *    of the document "Computing small clause normal forms" by Andreas Nonnengart
 *    and Christoph Weidenbach. The table 3 doesn't include the case of a XOR
 *    operation. However the results for a XOR operation are the same as those
 *    for the equivalence operation after switching the a and b columns.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    acoef: "a" coefficient of the sub-formula.
 *    bcoef: "b" coefficient of the sub-formula.
 *    block1: Pointer to linked list item of first block of the sub-formula.
 *    block2: Pointer to linked list item of second block of the sub-formula.
 *    position: position of the block for which the coefficients are being
 *              calculated within the sub-formula. It may be 1 or 2.
 *    acoefbl: Pointer to integer where the lower bound of the number of
 *             coefficient "a" of the requested block will be stored. This
 *             may have the values 0, 1, 2, 3 or 4. Values 0 to 3 are exact,
 *             value 4 is a lower bound.
 *    bcoefbl: Pointer to integer where the lower bound of the number of
 *             coefficient "b" of the requested block will be stored. This
 *             may have the values 0, 1, 2, 3 or 4. Values 0 to 3 are exact,
 *             value 4 is a lower bound.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void GetaABCoefs(int32_t acoef,int32_t bcoef,lkedfitem *block1,lkedfitem *block2,int32_t position,
		int32_t *acoefbl,int32_t *bcoefbl){
	auto lkedfitem *connective;                         /* Pointer to connective linked list item */

	/* If block2 is NULL then the first block is not directly */
	/* operated with another block. */
	if (block2==NULL){
		*acoefbl=acoef;
		*bcoefbl=bcoef;
		 return;
	}

	/* Get pointer to connective. */
	connective=JumpOverBlockOrTerm(block1,PASTEND);

	/* Get the number of clauses generated by a standard CNF */
	/* Process depending on the type of connective for block */
	/* at position 1. */
	if (position==1){
		switch ((ANDOPER|OROPER|XOROPER|EQUIVOPER)&connective->flags){
			case ANDOPER:
				*acoefbl=acoef;
				*bcoefbl=bcoef*block2->data.clauses.negclauses;
				break;
			case OROPER:
				*acoefbl=acoef*block2->data.clauses.posclauses;
				*bcoefbl=bcoef;
				break;
			case EQUIVOPER:
				*acoefbl=(acoef*block2->data.clauses.negclauses)+(bcoef*block2->data.clauses.posclauses);
				*bcoefbl=(acoef*block2->data.clauses.posclauses)+(bcoef*block2->data.clauses.negclauses);
				break;
			case XOROPER:
				*acoefbl=(acoef*block2->data.clauses.posclauses)+(bcoef*block2->data.clauses.negclauses);
				*bcoefbl=(acoef*block2->data.clauses.negclauses)+(bcoef*block2->data.clauses.posclauses);
				break;
		}

	/* Process depending on the type of connective for block */
	/* at position 2. */
	} else {
		switch ((ANDOPER|OROPER|XOROPER|EQUIVOPER)&connective->flags){
			case ANDOPER:
				*acoefbl=acoef;
				*bcoefbl=bcoef*block1->data.clauses.negclauses;
				break;
			case OROPER:
				*acoefbl=acoef*block1->data.clauses.posclauses;
				*bcoefbl=bcoef;
				break;
			case EQUIVOPER:
				*acoefbl=(acoef*block1->data.clauses.negclauses)+(bcoef*block1->data.clauses.posclauses);
				*bcoefbl=(acoef*block1->data.clauses.posclauses)+(bcoef*block1->data.clauses.negclauses);
				break;
			case XOROPER:
				*acoefbl=(acoef*block1->data.clauses.posclauses)+(bcoef*block1->data.clauses.negclauses);
				*bcoefbl=(acoef*block1->data.clauses.negclauses)+(bcoef*block1->data.clauses.posclauses);
				break;
		}
	}

	/* Set lower bounds. */
	if ((*acoefbl)>4){
		*acoefbl=4;
	}
	if ((*bcoefbl)>4){
		*bcoefbl=4;
	}
	return;
} /* GetaABCoefs */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Rename a block in a formula)  OCJ
 *
 *    This function renames a block in a formula in order to minimize the
 *    number of clauses generated by the CNF transformation. See general
 *    comments of GetClauseNumberBounds() function for information about
 *    the methodology and concepts of this calculations.
 *
 *    To do so a predicate definition formula is created and the block is
 *    replaced by the new predicate created. The block must start with an
 *    opening bracket and have connectives.
 *
 *    The concepts of block and segment are widely used in this function
 *    and the called functions. See general comments in PutBrackets()
 *    function for the definition of a block. See general comments
 *    in MiniscopeSegment() function for the definition of a segment.
 *
 *
 *
 *  ARGUMENTS:
 *
 *    ptopitem: Address of pointer to linked list top item of
 *              formula being processed for renaming.
 *    block: Pointer to first linked list item of block to be renamed.
 *    lastpdef: On input this is the address of pointer of last predicate
 *              definition cmprefix structure that has been already added
 *              for this renaming cycle. On output this is the address of
 *              predicate definition added after renaming this block.
 *    formula: Pointer to cmprefix structure of whole formula that is being renamed
 *             in text format.
 *    vars: Pointer to inner active nameschain structure.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *--------------------------------------------------------------*/
int32_t RenameBlock(lkedfitem **ptopitem,lkedfitem *block,cmprefix **lastpdef,cmprefix *formula,nameschain *vars){
	auto lkedfitem *predargs1,*predargs2;               /* Pointers to two copies of new predicate with arguments */
	auto lkedfitem *insertpt;                           /* Insert point of new quantifiers or new predicate in formula */
	auto lkedfitem **qlist;                             /* Pointer to list of quantifier linked list items. */
	auto nameschain *lclvars;                           /* Pointer to nameschain structure of last quantifier added */
	auto char prname[32];                               /* New predicate name */
	auto lkedfitem *ptr1,*ptr3,*ptr4,*ptr5;             /* Auxiliary pointers */
	auto nameschain *ptr2;                              /* Auxiliary pointer */
	auto cmprefix *ptr6;                                /* Auxiliary pointer */
	auto int32_t ii,jj;                                 /* Auxiliary */

	/* Remove block from the formula. */
	insertpt=block->prev;
	if (block->prev!=NULL){
		block->prev->next=block->ptr.mtchbrckt->next;
	} else {
		*ptopitem=block->ptr.mtchbrckt->next;
	}
	if (block->ptr.mtchbrckt->next!=NULL){
		block->ptr.mtchbrckt->next->prev=block->prev;
	}
	block->prev=block->ptr.mtchbrckt->next=NULL;

	/* Set ii=1 if there are free variables in the block, */
	/* ii=0 otherwise. Free variables are those not included */
	/* in the vars chain of nameschain structures. */
	for (ptr1=block->next,ii=0;(ptr1!=NULL)&&(ii==0);ptr1=ptr1->next){
		if (NAMEDVAR&ptr1->flags){
			for (ptr2=vars;ptr2!=NULL;ptr2=ptr2->next){
				if (ptr2==ptr1->ptr.vardata){
					ii=1;
					break;
				}
			}
		}
	}

	/* Build set of arguments for new predicate in linked list format. */
	/* The set of arguments must be enclosed in brackets. The variable */
	/* ii is updated to the number of arguments of new predicate, that */
	/* is the number of free variables in the block. */
	/* Two copies are made, one for the predicate definition and other */
	/* for the formula renaming. */
	predargs1=predargs2=NULL;
	if (ii){

		/* Set up enclosing brackets. */
		if (NULL==(ptr1=MYALLOC(sizeof(lkedfitem)))){
			FreeLkedLstFormula(block);
			return(NOMEMORY);
		}
		ptr1->flags=0;
		ptr3=NULL;
		if (NOMEMORY==AddOpenBracket(&ptr1,&predargs1,&ptr3)){
			FreeLkedLstFormula(block);
			FreeLkedLstFormula(predargs1);
			return(NOMEMORY);
		}
		if (NULL==(ptr1=MYALLOC(sizeof(lkedfitem)))){
			FreeLkedLstFormula(block);
			FreeLkedLstFormula(predargs1);
			return(NOMEMORY);
		}
		ptr1->flags=0;
		ptr5=NULL;
		if (NOMEMORY==AddOpenBracket(&ptr1,&predargs2,&ptr5)){
			FreeLkedLstFormula(block);
			FreeLkedLstFormula(predargs1);
			FreeLkedLstFormula(predargs2);
			return(NOMEMORY);
		}

		/* Loop through quantifiers that are active before the */
		/* beginning of the block. */
		for (ptr2=vars,ii=0;ptr2!=NULL;ptr2=ptr2->next){

			/* Loop through items in block. */
			for (ptr1=block->next;ptr1!=NULL;ptr1=ptr1->next){

				/* Item is a free variable corresponding to the quantifier being */
				/* examined. Add it to arguments and leave the inner loop. */
				if ((NAMEDVAR&ptr1->flags)&&(ptr1->ptr.vardata==ptr2)){
					ii++;
					if (NULL==(ptr4=MYALLOC(sizeof(lkedfitem)))){
						FreeLkedLstFormula(block);
						FreeLkedLstFormula(predargs1);
						FreeLkedLstFormula(predargs2);
						return(NOMEMORY);
					}
					ptr4->flags=NAMEDVAR;
					ptr4->ptr.vardata=ptr1->ptr.vardata;
					AddItem2LkdLstF(&ptr4,&predargs1,&ptr3);
					if (NULL==(ptr4=MYALLOC(sizeof(lkedfitem)))){
						FreeLkedLstFormula(block);
						FreeLkedLstFormula(predargs1);
						FreeLkedLstFormula(predargs2);
						return(NOMEMORY);
					}
					ptr4->flags=NAMEDVAR;
					ptr4->ptr.vardata=ptr1->ptr.vardata;
					AddItem2LkdLstF(&ptr4,&predargs2,&ptr5);
					ptr2->quantifier->flags|=FREEVAR;
					break;
				}
			}
		}
	}

	/* Create linked list item, name and symbol for new predicate */
	/* and add linked list item. */
	/* Two copies are made, one for the predicate definition and other */
	/* for the formula renaming. */
	strcpy(prname,PRDPREFIX);
	sprintf(&prname[strlen(PRDPREFIX)],"%lu",numpdefs+pdefid);
	strcat(prname,PRDPOSTFIX);
	numpdefs++;
	if (NULL==(ptr4=MYALLOC(sizeof(lkedfitem)))){
		FreeLkedLstFormula(block);
		FreeLkedLstFormula(predargs1);
		FreeLkedLstFormula(predargs2);
		return(NOMEMORY);
	}
	if (NULL==(ptr4->ptr.symbol=CreateGlobalSymbol(prname,ii,PREDICATE|DEFINEDPRED))){
		FreeLkedLstFormula(block);
		FreeLkedLstFormula(predargs1);
		FreeLkedLstFormula(predargs2);
		return(NOMEMORY);
	}
	ptr3=ptr5=NULL;
	ptr4->flags=NAMEDPRED;
	AddItem2LkdLstF(&ptr4,&predargs1,&ptr3);
	if (NULL==(ptr4=MYALLOC(sizeof(lkedfitem)))){
		FreeLkedLstFormula(block);
		FreeLkedLstFormula(predargs1);
		FreeLkedLstFormula(predargs2);
		return(NOMEMORY);
	}
	ptr4->ptr.symbol=ptr3->ptr.symbol;
	ptr4->flags=NAMEDPRED;
	AddItem2LkdLstF(&ptr4,&predargs2,&ptr5);

	/* Insert first copy of new predicate in formula. */
	/* This builds the renamed formula. */
	ptr1=JumpOverBlockOrTerm(predargs1,TOEND);
	if (insertpt!=NULL){
		ptr1->next=insertpt->next;
		if (insertpt->next!=NULL){
			insertpt->next->prev=ptr1;
		}
		predargs1->prev=insertpt;
		insertpt->next=predargs1;
	} else {
		ptr1->next=*ptopitem;
		(*ptopitem)->prev=ptr1;
		predargs1->prev=NULL;
		*ptopitem=predargs1;
	}

	/* Build the predicate definition with block item and second copy of predicate. */
	/* Also save last item of predicate definition for later. The commented out code */
	/* is the one described in the paper "Computing small clause normal forms" */
	/* by Andreas Nonnengart and Christoph Weidenbach but it is wrong because */
	/* the clause resulting from the renaming is not a theorem of its two parents */
	/* (the original clause and the predicate definition). It is left commented out */
	/* just for documentation. */
	ptr1=JumpOverBlockOrTerm(predargs2,TOEND);
	if (NULL==(ptr4=MYALLOC(sizeof(lkedfitem)))){
		FreeLkedLstFormula(block);
		FreeLkedLstFormula(predargs2);
		return(NOMEMORY);
	}
	/*switch ((POSPOL|NEGPOL|NEUTRALPOL)&block->flags){
		case POSPOL:
			ptr4->flags=IMPOPER;
			break;
		case NEGPOL:
			ptr4->flags=REVIMPOPER;
			break;
		case NEUTRALPOL:
			ptr4->flags=EQUIVOPER;
			break;
	}*/
	ptr4->flags=EQUIVOPER;
	ptr5=NULL;
	AddItem2LkdLstF(&ptr4,&block,&ptr5);
	ptr1->next=block;
	block->prev=ptr1;
	ptr1=JumpOverBlockOrTerm(block,TOEND); /* Save last item for later. */
	block=predargs2;

	/* There are free variables in the block. */
	if (ii){

		/* Enclose predicate definition in brackets. */
		if (NULL==(ptr3=MYALLOC(sizeof(lkedfitem)))){
			FreeLkedLstFormula(block);
			return(NOMEMORY);
		}
		ptr3->flags=CLOSEBRACKET;
		AddItem2LkdLstF(&ptr3,&block,&ptr1);
		if (NULL==(ptr3=MYALLOC(sizeof(lkedfitem)))){
			FreeLkedLstFormula(block);
			return(NOMEMORY);
		}
		ptr3->flags=OPENBRACKET;
		ptr4=NULL;
		AddItem2LkdLstF(&ptr3,&block,&ptr4);
		ptr4->ptr.mtchbrckt=ptr1;
		ptr1->ptr.mtchbrckt=ptr4;

		/* Create list of quantifiers to be added to predicate definition. */
		/* Allocate storage for the list and loop through nameschain structures. */
		if (NULL==(qlist=MYALLOC(ii*sizeof(lkedfitem *)))){
			FreeLkedLstFormula(block);
			return(NOMEMORY);
		}
		for (ptr2=vars,jj=ii-1;ptr2!=NULL;ptr2=ptr2->next){
			if (FREEVAR&ptr2->quantifier->flags){
				qlist[jj]=ptr2->quantifier;
				ptr2->quantifier->flags&=~(FREEVAR);
				jj--;
			}
		}

		/* Add appropriate quantifiers to predicate definition formula. */
		/* Loop through quantifiers in list. */
		insertpt=NULL;
		lclvars=NULL;
		for (jj=0;jj<ii;jj++){

			/* Add quantifier to predicate definition formula. */
			/* All quantifiers are converted to universal. */
			if (NULL==(insertpt=DuplicateQuantifier(qlist[jj],&insertpt,&block,lclvars))){
				FreeLkedLstFormula(block);
				MYFREE(qlist);
				return(NOMEMORY);
			}
			if (lclvars==NULL){
				insertpt->ptr.vardata->next=NULL;
				lclvars=insertpt->ptr.vardata;
			}
			insertpt->flags&=~EQUANTIFIER;
			insertpt->flags|=UQUANTIFIER;

			/* Adjust vardata pointers of variables for this quantifier. */
			/* Loop through predicate definition variables. */
			for (ptr1=block->next;ptr1!=NULL;ptr1=ptr1->next){
				if ((NAMEDVAR&ptr1->flags)&&(qlist[jj]->ptr.vardata==ptr1->ptr.vardata)){
					ptr1->ptr.vardata=insertpt->ptr.vardata;
				}
			}
		}
		MYFREE(qlist);
	}

	/* Convert predicate definition linked list formula to text, */
	/* create a text formula in main KB, update number of inferences */
	/* and free linked list predicate definition formula. */
	if (NULL==(ptr6=AddLkdLstFormula2KB(block,formula,*lastpdef,PREDICATEDEF,NULL,NULL))){
		FreeLkedLstFormula(block);
		return(NOMEMORY);
	}
	ptr6->flags|=((FROMCONJECTURE|FROMMAINFILE)&formula->flags);
	*lastpdef=ptr6;
	pbmstats->predicatedef++;
	FreeLkedLstFormula(block);
	return(0);
} /* RenameBlock */
