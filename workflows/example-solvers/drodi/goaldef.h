/*
 ============================================================================
 Name        : goaldef.h
 Author      : Oscar Contreras
 Version     :
 Copyright   : Copyright (C) 2015-2016 Oscar Contreras. All rights reserved.
 Description : First order logic theorem prover in C
 ============================================================================
 */
/****************************************************************
*
*                 goaldef.h (IsDefinable() static function header file)
*
* All of the documentation and software included in the Drodi package
* is copyrighted by Oscar Contreras.
*
* This header file must be included only in sources that make use of
* the IsDefinable() static inline function.
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

#ifndef GOALDEF_H_
#define GOALDEF_H_

/*--- Standard Includes ----------------------------------------*/
#include "global.h" /* This is included to prevent Eclipse showing errors when editing. */

/*----------------------------------------------------------------
 *
 *  Global variables shared with other modules that are initialized here.
 *  WARNING: Don't use "static" as then a different variable will be created in
 *  each module with the same name.
 *
 *--------------------------------------------------------------*/

/*----------------------------------------------------------------
 *
 *  Global variables shared with other modules that are not initialized here.
 *  WARNING: Don't use "static" as then a different variable will be created in
 *  each module with the same name.
 *
 *--------------------------------------------------------------*/

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Check if a term is definable)  OCJ
 *
 *    This function checks if a term must be defined in the goal
 *    transformation process for type 8 (UEQ) problems.
 *
 *    A term is definable if its arity is bigger than zero and
 *    at least one of its arguments is not a variable.
 *
 *
 *  ARGUMENTS:
 *
 *    term: Pointer to term to be checked.
 *
 *  RETURNS:
 *
 *    0 if term in not definable.
 *    1 if term is definable.
 *
 *--------------------------------------------------------------*/
static inline int32_t IsDefinable(uint8_t *term){
	auto uint8_t *ptr1,*ptr2;                         /* Auxiliary pointers */

	if ((VARIABLE&term[0])||(((symbol *)&term[1])->symbol->arity==0)){
	  return(0);
	} else {
		for (ptr1=NextItem(term,IMMED),ptr2=NextItem(term,OVERSUBTERMS);ptr1<ptr2;ptr1=NextItem(ptr1,IMMED)){
			if (0==(VARIABLE&ptr1[0])){
				return(1);
			}
		}
	}
	return(0);
} /* IsDefinable */

#endif
