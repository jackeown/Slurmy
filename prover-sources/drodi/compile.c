/*
 ============================================================================
 Name        : compile.c
 Author      : Oscar Contreras
 Version     :
 Copyright   : Copyright (C) 2015-2016 Oscar Contreras. All rights reserved.
 Description : First order logic theorem prover in C
 ============================================================================
 */

/****************************************************************
*
*            compile (Source formulas compilation module)
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
 *    This source contains the compilation functions that translate source
 *    formulas to the binary format used by the Drodi package. It also contains
 *    the inverse compilation functions, that take a compiled binary clause
 *    and translate it back to text format.
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
 *  DESCRIPTION: (Compile a CNF text formula)  OCJ
 *
 *    This function translates each clause in a CNF formula to binary
 *    compiled format and adds it to the main KB. The formula is
 *    assumed to be syntactically correct with the form:
 *        "(clause1)&(clause2)&...&(clausen)".
 *
 *    The compiled binary clause is pointed by the part2.bin field of a
 *    cmprefix structure and its format is as follows:
 *
 *      binprefix item1 item2 ... itemn UNITEND
 *
 *    where:
 *
 *      binprefix: Binary clause prefix structure containing global data.
 *      item1,...: One item for each literal and term in the same sequence as
 *                 in the text clause (preorder traversal). For instance,
 *                 for the clause:
 *                    A=x0|B(x0)|C(B(x0),D)
 *                 item1 is an equality literal, item2 is term A, item3 is
 *                 variable x0, item4 is term B, item5 is variable x0 again,
 *                 item6 is term C, and so on.
 *                 Format of items is described below.
 *      UNITEND: Byte marker indicating the end of a clause (see define in
 *               global.h).
 *
 *    Items format:
 *    ------------
 *    The format for items is:
 *
 *        itemtype itemdata
 *
 *    where:
 *
 *      itemtype: Byte marker indicating item type. See defines in global.h
 *      itemdata: Data depending on the itemtype:
 *                - For equalities and symbols (predicates and functions) it is
 *                  a symbol structure.
 *                - For variables it is a 2 byte integer with the variable number.
 *
 *
 *  ARGUMENTS:
 *
 *    formula: pointer to string with CNF formula. See Convert2CNF()
 *             function in Drodi.c for the CNF standard form adopted
 *             in this implementation.
 *    parent: pointer to parent input clause.
 *
 *  RETURNS:
 *
 *    0 -> Function completed successfully.
 *    NOMEMORY -> No memory available.
 *
 *--------------------------------------------------------------*/
int32_t CompileCNF(char *formula,cmprefix *parent){
	auto cmprefix *ptr1;                                 /* Auxiliary pointers */
	auto int32_t ii,jj,kk;                               /* Auxiliary */

	/* Loop through each clause in formula. */
	for (ii=1;formula[ii]!=0;ii+=(jj?kk+2:kk)){

		/* Find and mark end of clause and compile it. */
		for (jj=ii;formula[jj]!=0;jj++){
			if ((formula[jj]=='\'')||(formula[jj]=='"')){
				jj=JumpOverQuotes(&(formula[jj]))-formula;
			} else if (formula[jj]=='&'){
				break;
			}
		}
		if (formula[jj]!=0){
			kk=&formula[jj]-&formula[ii];
			jj=1;
		} else {
			kk=strlen(&formula[ii]);
			jj=0;
		}
		formula[ii+kk-1]=0;
		if (NOMEMORY==CompileClause(&formula[ii],&ptr1,parent)){
			return(NOMEMORY);
		}

		/* Display clause. */
		if ((verbose==2)&&(pgmmode==INTERACTIVE)){
			printf("%s\n",&formula[ii]);
		}

		/* Set clause as UNPROC, initialize sinedist and agedist fields and add it to KB. */
		ptr1->flags=UNPROC;
		ptr1->part2.bin->agedist=0;
		if (parent->flags&FROMCONJECTURE){
			ptr1->part2.bin->sinedist=0;
		} else {
			ptr1->part2.bin->sinedist=0xffffffff;
		}
		if (NOMEMORY==AddBinClause2KB(ptr1,&mainkb,0)){
			MYFREE(ptr1->part2.bin);
			MYFREE(ptr1);
			return(NOMEMORY);
		}
		if ((verbose==2)&&(pgmmode==INTERACTIVE)){
			printf("Clause has been added to main KB.\n");
		}
	}

	return(0);
} /* CompileCNF */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Compile a clause)  OCJ
 *
 *    This function compiles a clause in text form. Memory for the
 *    compiled clause will be allocated. It is responsibility of the
 *    caller to free this memory and to properly flag the clause.
 *
 *    See CompileCNF() for a description of compiled clauses format.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: pointer to string with clause in text form.
 *    buffer: Address where the pointer to compiled clause will be stored.
 *    parent: pointer to parent input clause.
 *
 *  RETURNS:
 *
 *    0 -> Function completed successfully.
 *    NOMEMORY -> No memory available.
 *
 *--------------------------------------------------------------*/
int32_t CompileClause(char *clause,cmprefix **buffer,cmprefix *parent){
	auto int32_t ii,jj;                                  /* Auxiliary */

	/* Calculate space required and allocate storage for compiled clause. */
	if (((*buffer)=MYALLOC(sizeof(cmprefix)))==NULL){
		return(NOMEMORY);
	}
	ii=BytesRequired(clause);
	if ((((*buffer)->part2.bin)=MYALLOC(ii))==NULL){
		MYFREE(*buffer);
		return(NOMEMORY);
	}

	/* Set prefix data. */
	(*buffer)->parent1=parent;
	(*buffer)->parent2=NULL;
	(*buffer)->inference=CLAUSIFY;
	(*buffer)->part2.bin->literals=0;
	(*buffer)->flags=PASSIVE;
	(*buffer)->part2.bin->ovly.asserts=(*buffer)->part2.bin->lockasserts=NULL;
	(*buffer)->part2.bin->asize=(*buffer)->part2.bin->lasize=0;
	(*buffer)->part2.bin->size=ii-binpsize;
	(*buffer)->part2.bin->clause=*buffer;
	(*buffer)->part2.bin->oriented=0;

	/* Loop through each literal in text clause and add the literal */
	/* to the compiled clause. */
	for (ii=jj=0;((ii==0)||(clause[ii-1]!=0));ii++){
		CompileLiteral(clause,&ii,*buffer,&jj);
		(*buffer)->part2.bin->literals++;
	}

	/* Add end of clause marker. */
	(*buffer)->part2.bin->formula[jj]=UNITEND;
	return(0);
} /* CompileClause */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Literal compilation)  OCJ
 *
 *    This function converts the literal starting at character indexed
 *    by *txtidx in clause to compiled form and adds it to the byte
 *    indexed by *binidx in the formula buffer of the compiled clause.
 *    The literal is assumed to be correct and it can be followed by
 *    additional characters belonging to a CNF. The formula buffer
 *    is assumed to have enough space for the literal.
 *
 *    On return *txtidx is set to next character not belonging to the
 *    literal and *binidx is set to the next free byte in the formula
 *    buffer.
 *
 *    See CompileCNF() function for a description of literal format.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: pointer to string with text clause containing the literal.
 *    txtidx: as input this is a pointer to the index to beginning
 *            of literal in clause. As output this is the index of
 *            next character not belonging to the literal in clause.
 *    buffer: cmprefix structure pointer of the compiled formula
 *            where the literal will be added.
 *    binidx: Pointer to index of next free byte in buffer->part2.bin->formula[]
 *            where the compiled literal will be placed. Its content
 *            will be updated by this function.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *--------------------------------------------------------------*/
void CompileLiteral(char *clause,int32_t *txtidx,cmprefix *buffer,int32_t *binidx){
	auto symbol *smbl;                                  /* Symbol pointer */
	auto int32_t ii,jj;                                 /* Auxiliary */

	/* Check if literal is an equality or inequality. */
	for (ii=(*txtidx),jj=0;((clause[ii]!='|')&&(clause[ii]!='&')
			&&(clause[ii]!=0));ii++){
		if ((clause[ii]=='\'')||(clause[ii]=='"')){
			ii=JumpOverQuotes(&(clause[ii]))-clause;
		} else if (clause[ii]=='='){
			if (clause[ii-1]=='!'){
				jj=2;
			} else {
				jj=1;
			}
			break;
		}
	}

	/* Equality or inequality. */
	if (jj){

		/* Add equality marker. */
		if (clause[*txtidx]=='~'){
			(*txtidx)++;
			if (jj==1){
				buffer->part2.bin->formula[*binidx]=EQUALITY|NEGATED;
			} else {
				buffer->part2.bin->formula[*binidx]=EQUALITY;
			}
		} else {
			if (jj==1){
				buffer->part2.bin->formula[*binidx]=EQUALITY;
			} else {
				buffer->part2.bin->formula[*binidx]=EQUALITY|NEGATED;
			}
		}

		/* Initialize equality symbol structure and set equality flag. */
		/* Formula symbolw field is not updated because it depends on */
		/* the weight parameterization option used for each KB. */
		smbl=(symbol *)&buffer->part2.bin->formula[1+(*binidx)];
		smbl->offset=*binidx;
		smbl->symbol=&equkey;

		/* Add equality terms. */
		for (ii=0,(*binidx)+=(1+sizeof(symbol));ii<2;ii++){
			CompileTerm(clause,txtidx,buffer,binidx);
			if (ii==0){
				(*txtidx)+=jj;
			}
		}

		/* Set index of next literal and return. */
		smbl->nextoff=*binidx;
		return;
	}

	/* PREDICATE. */
	/* Add predicate marker. */
	if (clause[*txtidx]=='~'){
		(*txtidx)++;
		buffer->part2.bin->formula[*binidx]=PREDICATE|NEGATED;
	} else {
		buffer->part2.bin->formula[*binidx]=PREDICATE;
	}

	/* Compile symbol. */
	(*binidx)++;
	CompileSymbol(clause,txtidx,buffer,binidx);

	return;
} /* CompileLiteral */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Term compilation)  OCJ
 *
 *    This function converts the term starting at character indexed
 *    by *txtidx in clause to pre-compiled form and adds it to the byte
 *    indexed by *binidx in the formula buffer of the compiled clause.
 *    The term is assumed to be correct and it can be followed by
 *    additional characters belonging to a CNF. The formula buffer
 *    is assumed to have enough space for the term.
 *
 *    On return the index in *txtidx is set to next character not belonging
 *    to the term.
 *
 *    This function calls itself recursively.
 *
 *    See CompileCNF() function for a description of term format.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: pointer to string with text clause containing the term.
 *    txtidx: as input this is a pointer to the index to beginning
 *            of term in clause. As output this is the index of
 *            next character not belonging to the term in clause.
 *    buffer: cmprefix structure pointer of the compiled formula
 *            where the term will be added.
 *    binidx: Pointer to index of next free byte in buffer->part2.bin->formula[]
 *            where the compiled term will be placed. Its content
 *            will be updated by this function.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
void CompileTerm(char *clause,int32_t *txtidx,cmprefix *buffer,int32_t *binidx){
	auto char *ptr1,*ptr2;                              /* Auxiliary pointers */
	auto char tempchar;                                 /* Temporary character */
	auto int16_t *auxptr;                               /* Auxiliary pointer */
	auto int32_t jj,kk;                                 /* Auxiliary */

	/* Parse name. */
	ParseName(&clause[*txtidx],&ptr1);

	/* The term is a variable. */
	/* Formula symbolw field is not updated because it depends on */
	/* the weight parameterization option used for each KB. */
	tempchar=ptr1[0];
	ptr1[0]=0;
	jj=strlen(&clause[*txtidx]);
	kk=strtol(&clause[(*txtidx)+1],&ptr2,10);
	if ((jj<7)&&(jj>1)&&(clause[*txtidx]=='X')&&(kk<32768)&&(ptr2[0]==0)){
		ptr1[0]=tempchar;
		buffer->part2.bin->formula[*binidx]=VARIABLE;
		auxptr=(int16_t *)&buffer->part2.bin->formula[(*binidx)+1];
		*auxptr=kk;
		*txtidx=ptr2-clause;
		(*binidx)+=3;
		return;
	}
	ptr1[0]=tempchar;

	/* The term is a function. Add function marker. */
	buffer->part2.bin->formula[*binidx]=FUNCTION;

	/* Compile symbol. */
	(*binidx)++;
	CompileSymbol(clause,txtidx,buffer,binidx);

	return;
} /* CompileTerm */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Symbol compilation)  OCJ
 *
 *    This function converts the symbol starting at character indexed
 *    by *txtidx in clause to compiled form and adds it to the byte
 *    indexed by *binidx in the formula buffer of the compiled clause.
 *    The symbol is assumed to be correct and it can be followed by
 *    additional characters belonging to a CNF. The formula buffer
 *    is assumed to have enough space for the symbol.
 *
 *    The byte marker corresponding to the symbol must be previously
 *    added to the formula buffer of the compiled clause.
 *
 *    On return *txtidx is set to next character not belonging to the
 *    symbol and *binidx is set to the next free byte in the formula
 *    buffer.
 *
 *    See CompileCNF() function for a description of literal format.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: pointer to string with text clause containing the symbol.
 *    txtidx: as input this is a pointer to the index to beginning
 *            of symbol in clause. As output this is the index of
 *            next character not belonging to the symbol in clause.
 *    buffer: cmprefix structure pointer of the compiled formula
 *            where the symbol will be added.
 *    binidx: Pointer to index of next free byte in buffer->part2.bin->formula[]
 *            where the compiled symbol will be placed. Its content
 *            will be updated by this function.
 *
 *  RETURNS:
 *
 *    None.
 *
 *
 *--------------------------------------------------------------*/
void CompileSymbol(char *clause,int32_t *txtidx,cmprefix *buffer,int32_t *binidx){
	auto char *ptr1;                                    /* Auxiliary pointer */
	auto symbol *smbl;                                  /* Symbol pointer */
	auto char tempchar;                                 /* Temporary char */

	/* Parse name. */
	ParseName(&clause[*txtidx],&ptr1);

	/* Initialize symbol structure. */
	smbl=(symbol *)&buffer->part2.bin->formula[*binidx];
	smbl->offset=(*binidx)-1;
	tempchar=*ptr1;
	*ptr1=0;
	smbl->symbol=CheckGlblDeclared(&clause[*txtidx]);
	smbl->symbol->usecount++;
	if (smbl->symbol->usecount>maxusecount){
		maxusecount=smbl->symbol->usecount;
	}
	*ptr1=tempchar;

	/* Loop through the predicate arguments. */
	(*binidx)+=sizeof(symbol);
	*txtidx=ptr1-clause;
	if (smbl->symbol->arity!=0){
		do {
			(*txtidx)++;
			CompileTerm(clause,txtidx,buffer,binidx);
		} while (clause[*txtidx]==',');
		(*txtidx)++;
	}

	/* Other updates: */
	/* Set offset of next item that is not a subterm of this symbol. */
	/* Formula symbolw field is not updated because it depends on */
	/* the weight parameterization option used for each KB. */
	smbl->nextoff=*binidx;

	return;
} /* CompileSymbol */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Compute the number of bytes needed for a compiled clause)  OCJ
 *
 *    This function calculates the number of bytes needed for a compiled
 *    clause from the clause in text form. The clause must start with a
 *    literal, not with a parenthesis, and is assumed to end with a NULL
 *    character. It is also assumed to be syntactically correct.
 *
 *    See CompileCNF() comments for the format of a compiled clause.
 *
 *    The number of bytes required for a binary compiled clause is:
 *      1+binpsize+3*n_v+(n_s+n_e)*(1+sizeof(symbol))
 *
 *    where:
 *      n_v: Total number of variables in the clause
 *      n_s: Total number of symbols in the clause. This includes predicates
 *           and functions of any arity.
 *      n_e: Total number of equalities and inequalities in the clause.
 *
 *    Variable names and symbols are detected because they start with a letter.
 *    Variable names are detected because they are standardized to the form
 *    "Xnnnnn" where nnn is an integer between 0 and 32767. Equalities and
 *    inequalities are detected counting the number of "=" characters.
 *
 *  ARGUMENTS:
 *
 *    clause: pointer to string with a clause starting with a literal,
 *            not with a parenthesis. See Clausify() function in clausify.c
 *            for the CNF standard form adopted in this implementation.
 *
 *  RETURNS:
 *
 *    Number of bytes required to store the given clause in a KB.
 *
 *--------------------------------------------------------------*/
int32_t BytesRequired(char *clause){
	auto char *ptr1;                             /* Auxiliary pointers */
	auto int32_t bytesreq;                       /* Number of bytes required */
	auto int32_t vnames;                         /* Number of variable names */
	auto int32_t nonvnames;                      /* Number of non variable names */
	auto int32_t numeqs;                         /* Number of equalities plus inequalities */
	auto int32_t ii;                             /* Auxiliary */

	/* Initialize bytes required. */
	bytesreq=1+binpsize;

	/* Loop through each character in clause. */
	numeqs=nonvnames=vnames=0; /* No detections of these so far. */
	for (ii=0;clause[ii]!=0;ii++){
		switch (clause[ii]){

		    /* Characters not belonging to a name. */
			case '=':
				numeqs++;
				break;
			case '|':
			case '(':
			case ')':
			case '!':
			case ',':
			case '~':
				break;

			/* Name enclosed in single or double quotes. */
			case '\'':
			case '"':
				ii=JumpOverQuotes(&(clause[ii]))-clause;
				nonvnames++;
				break;

			/* Start of a name. Check if it is a variable name */
			/* and count as appropriate. Then jump over the name. */
			default:
				if (STDVARNAME==ParseName(&clause[ii],&ptr1)){
					vnames++;
				} else {
					nonvnames++;
				}
				ii=(ptr1-clause)-1;
				break;
		}
	}

	/* Update number of bytes required and return. */
	bytesreq+=((3*vnames)+((nonvnames+numeqs)*(1+sizeof(symbol))));
	return(bytesreq);
} /* BytesRequired */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add binary clause to a KB)  OCJ
 *
 *    This function adds a clause in internal binary format
 *    to the knowledge base passed as an argument.
 *
 *    Symbols and equalities are added to the term indexing
 *    trees. The clause is chained to the appropriate queues.
 *    Finally a number is assigned to the formula.
 *
 *    If the clause is ACTIVE or PASSIVE then it is assumed that
 *    pkb must points to the working KB but this is not verified.
 *
 *    IMPORTANT: If the clause passed as argument is not ACTIVE
 *    or PASSIVE then it is not necessary to check the SLICETMOUT
 *    return code in the calling function.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to cmprefix structure of clause.
 *    pkb: Pointer to KB being where the clause is to be added.
 *    flags: SELECT: appropriate clause items will be selected.
 *
 *  RETURNS:
 *
 *    0 if successful.
 *    NOMEMORY if not enough memory.
 *    SLICETMOUT if hardware instructions limit exceeded.
 *    TIMEOUT if a timeout condition has occurred.
 *    UNKNOWN if problem solved by another process.
 *
 *
 *--------------------------------------------------------------*/
int32_t AddBinClause2KB(cmprefix *clause,kbase *pkb,int32_t flags){
	auto int32_t *ptr1;                          /* Auxiliary pointer */
	auto int32_t ii;                             /* Auxiliary */

	/* Shuffle literals if clause is active or passive and shuffling is enabled. */
	if ((clause->flags&(ACTIVE|PASSIVE))&&(kbset.opts.shuffle)){
		if (NOMEMORY==ShuffleLiterals(clause)){
			return(NOMEMORY);
		}
	}

	/* Renumber variables to minimize the highest number assigned. */
	if (NOMEMORY==RenumberVars(clause->part2.bin)){
		return(NOMEMORY);
	}

	/* Select items if needed. */
	if (SELECT&flags){
		if (NOMEMORY==SelectItems(clause)){
			return(NOMEMORY);
		}
	}

	/* Add symbol and equalities to term indexing trees. Also increment */
	/* the wb buffer in wvdata global variable if necessary. */
	/* This is always done for ACTIVE clauses, but for PASSIVE */
	/* clauses it is only done for KB's running OTTER algorithm. */
	if ((clause->flags&ACTIVE)||((clause->flags&PASSIVE)
			&&(pkb->opts.algorithm==OTTER))){

		/* Increment the wb buffer in wvdata global variable if necessary. */
		ii=2*(clause->part2.bin->maxvarnb+1);
		if (ii>wbdata.size){
			ii+=VARBALANCE_CHUNK_SIZE;
			if (NULL==(ptr1=MYREALLOC(wbdata.vb,ii*sizeof(*wbdata.vb)))){
				return(NOMEMORY);
			}
			wbdata.vb=ptr1;
			wbdata.size=ii;
		}

		/* Add symbol and equalities to term indexing trees. */
		if (0!=(ii=AddDiscrTrees(clause))){
			return(ii);
		}
	}

	/* Link formula. */
	clause->prevdisp=NULL;
	switch (clause->flags&(ACTIVE|PASSIVE|UNPROC|LOCKED)){

		/* Unprocessed clause. Link clause to UNPROC clauses queue */
		case UNPROC:
			clause->prev=pkb->lstunproc;
			clause->next=NULL;
			if (clause->prev!=NULL){
				clause->prev->next=clause;
			}
			if (pkb->frstunproc==NULL){
				pkb->frstunproc=clause;
			}
			pkb->lstunproc=clause;
			pkb->prstats.nbunproc++;
			ChainAsserts(clause->part2.bin->ovly.asserts);
			break;

		/* Locked clause. Link clause to LOCKED clauses queue. */
		/* Also link clause locks. */
		case LOCKED:
			clause->prev=pkb->lstlocked;
			clause->next=NULL;
			if (clause->prev!=NULL){
				clause->prev->next=clause;
			}
			if (pkb->frstlocked==NULL){
				pkb->frstlocked=clause;
			}
			pkb->lstlocked=clause;
			ChainAsserts(clause->part2.bin->ovly.asserts);
			ChainLocks(clause->part2.bin->lockasserts);
			break;

		/* ACTIVE clause. Link clause to active clauses queue */
		/* and update number of active clauses in KB. Also */
		/* link clause asserts. */
		case ACTIVE:
			clause->prev=pkb->lstactive;
			clause->next=NULL;
			if (clause->prev!=NULL){
				clause->prev->next=clause;
			}
			if (pkb->frstactive==NULL){
				pkb->frstactive=clause;
			}
			pkb->lstactive=clause;
			pkb->prstats.nbactive++;
			ChainAsserts(clause->part2.bin->ovly.asserts);
			break;

		/* PASSIVE clause. */
		case PASSIVE:

			/* Link clause to passive clauses queue. */
			clause->prev=pkb->lstpassive;
			clause->next=NULL;
			if (clause->prev!=NULL){
				clause->prev->next=clause;
			}
			if (pkb->frstpassive==NULL){
				pkb->frstpassive=clause;
			}
			pkb->lstpassive=clause;

			/* Link clause to clause selection queues. */
			LinkSelectQueues(clause);

			/* Update number of passive clauses in KB. */
			pkb->prstats.nbpassive++;

			/* Link clause asserts. */
			ChainAsserts(clause->part2.bin->ovly.asserts);
			break;
	}

	/* Other settings. */
	if (0==(NUMBERED&clause->flags)){
		pkb->nformulas++;
		clause->number=pkb->nformulas;
		clause->flags|=NUMBERED;
	}
	clause->part2.bin->clause=clause;
	clause->part2.bin->signature=0;
	clause->part2.bin->locksignature=0;

	return(0);
} /* AddBinClause2KB */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add binary clause to a KB for unit clause)  OCJ
 *
 *    This function adds a clause in internal binary format
 *    to the knowledge base passed as an argument for type 8
 *    problems (unit equality). It is similar to AddBinClause2KB()
 *    function but specific for type 8 problems.
 *
 *    Symbols and equalities are added to the term indexing
 *    trees. The clause is chained to the appropriate queues.
 *    Finally a number is assigned to the formula.
 *
 *    If the clause is ACTIVE or PASSIVE then it is assumed that
 *    pkb must points to the working KB but this is not verified.
 *
 *    IMPORTANT: If the clause passed as argument is not ACTIVE
 *    or PASSIVE then it is not necessary to check the SLICETMOUT
 *    return code in the calling function.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to cmprefix structure of clause.
 *    pkb: Pointer to KB being where the clause is to be added.
 *    flags: SELECT: appropriate clause items will be selected.
 *
 *  RETURNS:
 *
 *    0 if successful.
 *    NOMEMORY if not enough memory.
 *    SLICETMOUT if hardware instructions limit exceeded.
 *    TIMEOUT if a timeout condition has occurred.
 *    UNKNOWN if problem solved by another process.
 *
 *
 *--------------------------------------------------------------*/
int32_t AddBinClause2KBUEQ(cmprefix *clause,kbase *pkb,int32_t flags){
	auto int32_t *ptr1;                          /* Auxiliary pointer */
	auto int32_t ii;                             /* Auxiliary */

	/* Renumber variables to minimize the highest number assigned. */
	/* This is not done if the clause inference is GOALDEFINITION */
	/* as it is important to keep the original variable numbering */
	/* until the end of the process. Variable renumbering */
	/* will be done later for these clauses. */
	if (clause->inference!=GOALDEFINITION){
		if (NOMEMORY==RenumberVars(clause->part2.bin)){
			return(NOMEMORY);
		}
	}

	/* Select items if needed. */
	if (SELECT&flags){
		if (NOMEMORY==SelectItemsUEQ(clause)){
			return(NOMEMORY);
		}
	}

	/* Add symbol and equalities to term indexing trees. Also increment */
	/* the wb buffer in wvdata global variable if necessary. */
	/* This is always done for ACTIVE clauses, but for PASSIVE */
	/* clauses it is only done for KB's running OTTER algorithm. */
	if ((clause->flags&ACTIVE)||((clause->flags&PASSIVE)
			&&(pkb->opts.algorithm==OTTER))){

		/* Increment the wb buffer in wvdata global variable if necessary. */
		ii=2*(clause->part2.bin->maxvarnb+1);
		if (ii>wbdata.size){
			ii+=VARBALANCE_CHUNK_SIZE;
			if (NULL==(ptr1=MYREALLOC(wbdata.vb,ii*sizeof(*wbdata.vb)))){
				return(NOMEMORY);
			}
			wbdata.vb=ptr1;
			wbdata.size=ii;
		}

		/* Add symbol and equalities to term indexing trees. */
		if (0!=(ii=AddDiscrTrees(clause))){
			return(ii);
		}
	}

	/* Link formula. */
	clause->prevdisp=NULL;
	switch (clause->flags&(ACTIVE|PASSIVE|UNPROC|LOCKED)){

		/* Unprocessed clause. Link clause to UNPROC clauses queue */
		case UNPROC:
			clause->prev=pkb->lstunproc;
			clause->next=NULL;
			if (clause->prev!=NULL){
				clause->prev->next=clause;
			}
			if (pkb->frstunproc==NULL){
				pkb->frstunproc=clause;
			}
			pkb->lstunproc=clause;
			pkb->prstats.nbunproc++;
			break;

		/* ACTIVE clause. Link clause to active clauses queue */
		/* and update number of active clauses in KB. Also */
		/* link clause asserts. */
		case ACTIVE:
			clause->prev=pkb->lstactive;
			clause->next=NULL;
			if (clause->prev!=NULL){
				clause->prev->next=clause;
			}
			if (pkb->frstactive==NULL){
				pkb->frstactive=clause;
			}
			pkb->lstactive=clause;
			pkb->prstats.nbactive++;
			break;

		/* PASSIVE clause. */
		case PASSIVE:

			/* Link clause to passive clauses queue. */
			clause->prev=pkb->lstpassive;
			clause->next=NULL;
			if (clause->prev!=NULL){
				clause->prev->next=clause;
			}
			if (pkb->frstpassive==NULL){
				pkb->frstpassive=clause;
			}
			pkb->lstpassive=clause;

			/* Link clause to clause selection queues. */
			LinkSelectQueues(clause);

			/* Update number of passive clauses in KB. */
			pkb->prstats.nbpassive++;
			break;
	}

	/* Other settings. */
	if (0==(NUMBERED&clause->flags)){
		pkb->nformulas++;
		clause->number=pkb->nformulas;
		clause->flags|=NUMBERED;
	}
	clause->part2.bin->clause=clause;
	clause->part2.bin->signature=0;
	clause->part2.bin->locksignature=0;

	return(0);
} /* AddBinClause2KBUEQ */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Add binary clause to a KB for unfailing completion)  OCJ
 *
 *    This function is the same as AddBinClause2KB bit specialized
 *    for the case of completion without failure, that is, when
 *    ALGORITHM is set to UEQDSC or UEQOTT.
 *
 *    Weight of clauses to be added to the PASSIVE queue are calculated
 *    and checked from being retained due to weight limits. If the
 *    clause is out of weight limits then it is deleted.
 *
 *    Symbols and equalities of ACTIVE and GROUND JOINABLE clauses
 *    are added to the term indexing trees. The clause is chained
 *    to the appropriate queues. Finally a number is assigned to
 *    the formula.
 *
 *    All clauses are added to the working KB.
 *
 *    IMPORTANT: If the clause passed as argument is not ACTIVE
 *    or PASSIVE then it is not necessary to check the SLICETMOUT
 *    return code in the calling function.
 *
 *
 *  ARGUMENTS:
 *
 *    clause: Pointer to cmprefix structure of clause.
 *
 *  RETURNS:
 *
 *    0 if successful.
 *    NOMEMORY if not enough memory.
 *    SLICETMOUT if hardware instructions limit exceeded.
 *    TIMEOUT if a timeout condition has occurred.
 *    UNKNOWN if problem solved by another process.
 *
 *
 *--------------------------------------------------------------*/
int32_t AddUEQClause2KB(cmprefix *clause){
	auto int32_t *ptr1;                                  /* Auxiliary pointer */
	auto uint8_t *ptr2,*ptr3,*ptr4;                      /* Auxiliary pointers */
	auto int32_t ii;                                     /* Auxiliary */

	/* If the clause must be added to PASSIVE queue then compute */
	/* clause weight and delete it if it is out of weight limits. */
	if (PASSIVE&clause->flags){
		ClauseWeight(clause);
		if (clause->part2.bin->clweight>=kbset.opts.maxweight){
			MYFREE(clause->part2.bin->ovly.cptopterm);
			MYFREE(clause->part2.bin);
			MYFREE(clause);
			return(0);
		}
	}

	/* Shuffle (in)equality root terms if clause is active */
	/* or passive and shuffling is enabled. */
	if ((clause->flags&(ACTIVE|PASSIVE))&&(kbset.opts.shuffle)){
		if (NOMEMORY==ShuffleLiterals(clause)){
			return(NOMEMORY);
		}
	}

	/* Renumber variables to minimize the highest number assigned. */
	if (NOMEMORY==RenumberVars2(clause->part2.bin)){
		return(NOMEMORY);
	}

	/* Increment the wb buffer in wvdata global variable if necessary. */
	/* This is always done for ACTIVE clauses, but for PASSIVE */
	/* clauses it is only done for KB's running OTTER algorithm. */
	if ((clause->flags&ACTIVE)||((clause->flags&PASSIVE)
			&&(kbset.opts.algorithm==OTTER))){
		ii=2*(clause->part2.bin->maxvarnb+1);
		if (ii>wbdata.size){
			ii+=VARBALANCE_CHUNK_SIZE;
			if (NULL==(ptr1=MYREALLOC(wbdata.vb,ii*sizeof(*wbdata.vb)))){
				return(NOMEMORY);
			}
			wbdata.vb=ptr1;
			wbdata.size=ii;
		}
	}

	/* Orient equality if it is not oriented or weakly oriented. This is */
	/* necessary because if literals or (in)equality root terms have been */
	/* shuffled then equality data has been freed as variable renumbering */
	/* makes it unusable. */
	if ((0==(WEAKLYORIENTED&clause->flags))&&(clause->part2.bin->oriented==0)&&((clause->flags&ACTIVE)
			||((clause->flags&PASSIVE)&&(kbset.opts.algorithm==UEQOTT)))){
		ptr2=NextItem(ptr4=&clause->part2.bin->formula[0],IMMED);
		ptr3=NextItem(ptr2,OVERSUBTERMS);
		if ((0==(MAXIMUM&ptr2[0]))&&(0==(MAXIMUM&ptr3[0]))){

			/* Reset MAXIMUM and SELECTED flags in equality root terms. */
			ptr2[0]&=(~(MAXIMUM|SELECTED));
			ptr3[0]&=(~(MAXIMUM|SELECTED));

			/* Orient (in)equality by setting the SELECTED and MAXIMUM flags */
			/* for equality root terms as appropriate. */
			/* Release equality tree data if the root terms ordering */
			/* is completely defined. */
			ii=CompareEquTerms(clause->part2.bin,ptr2,ptr3);
			clause->part2.bin->oriented=1;
			switch (ii){
				case NOMEMORY:
					return(NOMEMORY);
					break;
				case EQUAL:
					break;
				case FAILURE:
					ptr2[0]|=SELECTED;
					ptr3[0]|=SELECTED;
					break;
				case GREATER:
					ptr2[0]|=(SELECTED|MAXIMUM);
					break;
				case LESSER:
					ptr3[0]|=(SELECTED|MAXIMUM);
					break;
			}
		}
	}

	/* Add symbols and equalities to term indexing trees. */
	/* This is done for ACTIVE and GROUNDJOINABLE clauses */
	/* and if algorithm is UEQOTT also for PASSIVE clauses. */
	if ((clause->flags&(ACTIVE|GROUNDJOINABLE))||((kbset.opts.algorithm==UEQOTT)&&(clause->flags&PASSIVE))){
		if (0!=(ii=EditUEQTrees(clause,ADDTREES))){
			return(ii);
		}
	}

	/* Link formula. */
	clause->prevdisp=NULL;
	switch (clause->flags&(ACTIVE|PASSIVE|GROUNDJOINABLE|UNPROC)){

		/* Ground joinable clause. Link clause to GROUNDJOINABLE clauses queue */
		case GROUNDJOINABLE:
			clause->prev=kbset.lstgrjoin;
			clause->next=NULL;
			if (clause->prev!=NULL){
				clause->prev->next=clause;
			}
			if (kbset.frstgrjoin==NULL){
				kbset.frstgrjoin=clause;
			}
			kbset.lstgrjoin=clause;
			kbset.prstats.nbgrjnbl++;
			break;

		/* Unprocessed clause. Link clause to UNPROC clauses queue. */
		/* Also if clause is negative then set kbset.negeq field. */
		case UNPROC:
			clause->prev=kbset.lstunproc;
			clause->next=NULL;
			if (clause->prev!=NULL){
				clause->prev->next=clause;
			}
			if (kbset.frstunproc==NULL){
				kbset.frstunproc=clause;
			}
			kbset.lstunproc=clause;
			kbset.prstats.nbunproc++;
			if (NEGATED&clause->part2.bin->formula[0]){
				kbset.negeq=clause;
			}
			break;

		/* ACTIVE clause. Link clause to active clauses queue */
		/* and update number of active clauses in KB. */
		case ACTIVE:
			clause->prev=kbset.lstactive;
			clause->next=NULL;
			if (clause->prev!=NULL){
				clause->prev->next=clause;
			}
			if (kbset.frstactive==NULL){
				kbset.frstactive=clause;
			}
			kbset.lstactive=clause;
			kbset.prstats.nbactive++;
			if (NEGATED&clause->part2.bin->formula[0]){
				kbset.negeq=clause;
			}
			break;

		/* PASSIVE clause. */
		case PASSIVE:

			/* Link clause to passive clauses queue. */
			clause->prev=kbset.lstpassive;
			clause->next=NULL;
			if (clause->prev!=NULL){
				clause->prev->next=clause;
			}
			if (kbset.frstpassive==NULL){
				kbset.frstpassive=clause;
			}
			kbset.lstpassive=clause;

			/* Link clause to clause selection queues. */
			LinkSelectQueues(clause);

			/* Update number of passive clauses in KB. */
			kbset.prstats.nbpassive++;

			/* If clause is negative then set kbset.negeq field. */
			if (NEGATED&clause->part2.bin->formula[0]){
				kbset.negeq=clause;
			}
			break;
	}

	/* Other settings. */
	if (0==(NUMBERED&clause->flags)){
		kbset.nformulas++;
		clause->number=kbset.nformulas;
		clause->flags|=NUMBERED;
	}
	clause->part2.bin->clause=clause;
	clause->part2.bin->signature=0;
	clause->part2.bin->locksignature=0;

	return(0);
} /* AddUEQClause2KB */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Renumber variables in a binary formula)  OCJ
 *
 *    This function renumbers all variables in a binary formula
 *    such that the lowest variable is assigned number zero and
 *    all the variable numbers are consecutive.
 *
 *    This way the  formula is kept away from high variable numbers.
 *
 *    This function also sets the maxvarnb field of clause binprefix
 *    structure.
 *
 *
 *  ARGUMENTS:
 *
 *    binp: Pointer to formula binprefix.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *
 *--------------------------------------------------------------*/
int32_t RenumberVars(binprefix *binp){
	auto int32_t size;                                   /* Size if buffer in number of elements */
	auto int16_t *buffer;                                /* Auxiliary pointer */
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto int16_t *ptr2;                                  /* Auxiliary pointer */
	auto uint32_t ii,jj,kk;                              /* Auxiliary */

	/* Allocate storage for sorting variables. */
	ii=VARRENUM_CHUNK_SIZE*sizeof(int16_t);
	if (NULL==(buffer=MYALLOC(ii))){
		return(NOMEMORY);
	}
	memset(buffer,0xff,ii);
	size=VARRENUM_CHUNK_SIZE;

	/* Compute maxvarnb and the new number assigned to each variable. */
	/* Loop through items in clause. */
	/* At the end of this loop if kk is not zero then an effective */
	/* variable renumbering has been done. */
	binp->maxvarnb=-1;
	for (ptr1=&binp->formula[0],kk=0;ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){

		/* Item is a variable. */
		if (VARIABLE&ptr1[0]){

			/* Reallocate buffer space if necessary. */
			jj=*((int16_t *)&ptr1[1]);
			if (jj>=size){
				ii=jj+VARRENUM_CHUNK_SIZE;
				if (NULL==(ptr2=MYREALLOC(buffer,ii*sizeof(int16_t)))){
					MYFREE(buffer);
					return(NOMEMORY);
				}
				buffer=ptr2;
				memset(&buffer[size],0xff,(ii-size)*sizeof(int16_t));
				size=ii;
			}

			/* If variable new number is not yet assigned then assign a new number. */
			/* Update kk as necessary (see comments above). */
			if (buffer[jj]==-1){
				binp->maxvarnb++;
				buffer[jj]=binp->maxvarnb;
				if (jj!=binp->maxvarnb){
					*((int16_t *)&ptr1[1])=binp->maxvarnb;
					kk=1;
				}

			/* If variable number has been already assigned but it doesn't match */
			/* the current variable number then renumber it and update kk. */
			} else if (jj!=buffer[jj]){
				*((int16_t *)&ptr1[1])=buffer[jj];
				kk=1;
			}
		}
	}

	/* Set the oriented field to zero. This is necessary because as the variables */
	/* were renumbered then the equality data will be unusable. */
	if (kk){
		binp->oriented=0;
	}

	/* Free memory and return. */
	MYFREE(buffer);
	return(0);
} /* RenumberVars */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Renumber variables in critical pairs)  OCJ
 *
 *    This function is similar to RenumberVars() but if the
 *    formula has the field ovly->cptopterm of binprefix
 *    structure is not NULL then its variables are also
 *    renumbered and taken into account for computing maxvarnb
 *    of the clause binprefix structure.
 *
 *
 *  ARGUMENTS:
 *
 *    binp: Pointer to formula binprefix.
 *
 *  RETURNS:
 *
 *    0 if OK or NOMEMORY.
 *
 *
 *--------------------------------------------------------------*/
int32_t RenumberVars2(binprefix *binp){
	auto int32_t size;                                   /* Size if buffer in number of elements */
	auto int16_t *buffer;                                /* Auxiliary pointer */
	auto uint8_t *ptr1;                                  /* Auxiliary pointer */
	auto int16_t *ptr2;                                  /* Auxiliary pointer */
	auto uint32_t ii,jj,kk;                              /* Auxiliary */

	/* Allocate storage for sorting variables. */
	ii=VARRENUM_CHUNK_SIZE*sizeof(int16_t);
	if (NULL==(buffer=MYALLOC(ii))){
		return(NOMEMORY);
	}
	memset(buffer,0xff,ii);
	size=VARRENUM_CHUNK_SIZE;

	/* Compute maxvarnb and the new number assigned to each variable. */
	/* Loop through items in clause. */
	/* At the end of this loop if kk is not zero then an effective */
	/* variable renumbering has been done for the clause. */
	binp->maxvarnb=-1;
	for (ptr1=&binp->formula[0],kk=0;ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){

		/* Item is a variable. */
		if (VARIABLE&ptr1[0]){

			/* Reallocate buffer space if necessary. */
			jj=*((int16_t *)&ptr1[1]);
			if (jj>=size){
				ii=jj+VARRENUM_CHUNK_SIZE;
				if (NULL==(ptr2=MYREALLOC(buffer,ii*sizeof(int16_t)))){
					MYFREE(buffer);
					return(NOMEMORY);
				}
				buffer=ptr2;
				memset(&buffer[size],0xff,(ii-size)*sizeof(int16_t));
				size=ii;
			}

			/* If variable new number is not yet assigned then assign a new number. */
			/* Update kk as necessary (see comments above). */
			if (buffer[jj]==-1){
				binp->maxvarnb++;
				buffer[jj]=binp->maxvarnb;
				if (jj!=binp->maxvarnb){
					*((int16_t *)&ptr1[1])=binp->maxvarnb;
					kk=1;
				}

			/* If variable number has been already assigned but it doesn't match */
			/* the current variable number then renumber it and update kk. */
			} else if (jj!=buffer[jj]){
				*((int16_t *)&ptr1[1])=buffer[jj];
				kk=1;
			}
		}
	}

	/* Update maxvarnb and the new number assigned to each variable */
	/* for variables in the top term. Loop through items in top term. */
	if (binp->ovly.cptopterm!=NULL){
		for (ptr1=&binp->ovly.cptopterm[0];ptr1[0]!=UNITEND;ptr1=NextItem(ptr1,IMMED)){

			/* Item is a variable. */
			if (VARIABLE&ptr1[0]){

				/* Reallocate buffer space if necessary. */
				jj=*((int16_t *)&ptr1[1]);
				if (jj>=size){
					ii=(jj+VARRENUM_CHUNK_SIZE)*sizeof(int16_t);
					if (NULL==(ptr2=MYREALLOC(buffer,ii*sizeof(int16_t)))){
						MYFREE(buffer);
						return(NOMEMORY);
					}
					buffer=ptr2;
					memset(&buffer[size],0xff,(jj+VARRENUM_CHUNK_SIZE-size)*sizeof(int16_t));
					size=jj+VARRENUM_CHUNK_SIZE;
				}

				/* If variable new number is not yet assigned then assign a new number. */
				if (buffer[jj]==-1){
					binp->maxvarnb++;
					buffer[jj]=binp->maxvarnb;
					if (jj!=binp->maxvarnb){
						*((int16_t *)&ptr1[1])=binp->maxvarnb;
					}

				/* If variable number has been already assigned but it doesn't match */
				/* the current variable number then renumber it. */
				} else if (jj!=buffer[jj]){
					*((int16_t *)&ptr1[1])=buffer[jj];
				}
			}
		}
	}

	/* Set the oriented field to zero. This is necessary because as the variables */
	/* were renumbered then the equality data will be unusable. */
	if (kk){
		binp->oriented=0;
	}

	/* Free memory and return. */
	MYFREE(buffer);
	return(0);
} /* RenumberVars2 */

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Append a string fast and safely)  OCJ
 *
 *    This function is basically a slightly faster version of the
 *    SafeAppend() function in clausify module. The increase in
 *    speed is achieved by keeping track of the length of the string
 *    so that a search for the end of destination string and a
 *    potentially long call to strlen() function is avoided. This
 *    function doesn't have the option of appending a limited number
 *    of characters.
 *
 *    A new function is created to be used during the inference process
 *    so that a marginal improve in speed is achieved. The original
 *    SafeAppend() function is kept to avoid having to modify all calls
 *    in the clausify module.
 *
 *
 *  ARGUMENTS:
 *
 *    deststr: Address of pointer to destination buffer.
 *    buffsize: Pointer to total size of destination buffer in bytes.
 *              including the space currently in use.
 *    orgstr: Pointer to string to append.
 *    length: Pointer to current length of string. This is updated by
 *            this function.
 *
 *  RETURNS:
 *
 *    0 -> Append completed successfully.
 *    NOMEMORY -> No memory available.
 *
 *--------------------------------------------------------------*/
int32_t SafeAppendF(char **deststr,int32_t *buffsize,char *orgstr,int32_t *length){
	auto char *ptr;                                 /* Auxiliary pointer. */
	auto int32_t ii,jj,kk;                          /* Auxiliary */
	kk=*length;
	*length+=(jj=strlen(orgstr));
	ii=(*length)+1;
	if (ii>(*buffsize)){
		if (NULL==(ptr=MYREALLOC(*deststr,STR_CHUNK_SIZE+ii))){
			return(NOMEMORY);
		}
		*deststr=ptr;
		*buffsize=STR_CHUNK_SIZE+ii;
	}
	memcpy((*deststr)+kk,orgstr,jj+1);
	return(0);
} /* SafeAppendF */
