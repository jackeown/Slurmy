/*
 ============================================================================
 Name        : global.h
 Author      : Oscar Contreras
 Version     :
 Copyright   : Copyright (C) 2015-2016 Oscar Contreras. All rights reserved.
 Description : First order logic theorem prover in C
 ============================================================================
 */
/****************************************************************
*
*                 global.h (global include file for Drodi project)
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

#ifndef GLOBAL_H_
#define GLOBAL_H_
/* Include for platform independent modules */

/*--- Standard Includes ----------------------------------------*/
#include <stddef.h>                     /* Standard definitions         */
#include <string.h>                     /* strcpy/memset                */
#include <ctype.h>                      /* isdigit/toupper...           */
#include <stdio.h>                      /* sprintf/fopen...             */
#include <stdlib.h>                     /* malloc/free...               */
#include <time.h>                       /* clock(),...                  */
#include <sys/time.h>                   /* gettimeofday()               */
#include <limits.h>                     /* CHAR_BIT                     */
#include <stdint.h>                     /* intxx_t data types           */
#include <locale.h>                     /* setlocale()                  */
#include <math.h>                       /* sqrt()                       */
#include <signal.h>                     /* signal(),...                 */
#include <errno.h>                      /* strerror()                   */
#include <unistd.h>                     /* getpid(), sysconf()...       */
#include <sys/types.h>                  /* getpid()                     */
#include <sys/resource.h>               /* getrlimit(), setrlimit()     */
#include <sys/mman.h>                   /* mmap()                       */
#include <semaphore.h>                  /* sem_xxx()                    */
#include <sys/wait.h>                   /* wait()                       */
#include <malloc.h>                     /* mallinfo2()                  */

/* Define's for compilation time code generation control. */
//#define DEBUGMEMORY                         /* Generate debug code for memory problems detected by Valgrind.*/
                                            /* See myfree() comments for information. */
//#define DEBUGCODE                           /* Generate additional debug code. */
//#define VERBOSE                             /* Display inferences if DEBUGCODE is defined and only one core enabled. */
//#define DEBUGTREE                             /* Generate debug code for discrimination tree integrity.*/
//#define DEBUGTREE2                            /* Generate debug code for discrimination tree queries. The checks will */
                                              /* only be done if DEBUGTREE is not defined. See ChecDscTQueries() */
                                              /* function global comments for additional information. */
#ifdef DEBUGTREE2
#define NBOFTESTS 3                           /* Number of literals to be tested by ChecDscTQueries() function. */
#endif
//#define SATDEBUGCODE1                       /* Generate additional debug code specific to check coherence */
                                            /* of hashchcmp elements ordered sub-queue. */
//#define SATDEBUGCODE2                       /* Generate additional debug code specific to check coherence */
                                            /* of satclause elements ordered sub-queue. */
//#define VERIFYSATPROOF                      /* Verify that the SAT part of the proof is coherent. */
#define DEBUGKBDATA 0                       /* Set to 0 for normal operations. Set to 1 to force print KB data even in */
                                            /* TIMEOUT or NOMEMORY conditions. */
//#define MURMUR3                             /* Use Murmur3 hash function (default is to use KRM hash function). */
//#define BENCHMARKING                        /* Include code for benchmarking summary output. */
                                            /* VERY IMPORTANT: If BENCHMARKING is defined then the BENCHDIMENSION parameter */
                                            /* below must be adjusted appropriately. */
#ifdef BENCHMARKING
#define BENCHTYPE 3                         /* 1 -> report solving times for problems solved. */
                                            /* 2 -> report THM, SAT, CSA, CAX or UNS for problems solved. */
                                            /* 3 -> report 1 and 2 above for problems solved. */
#define VERIFYSATFINALSTATUS 0              /* If this equals one then SAT final status is verified after a problem solving attempt. */
                                            /* Only if BENCHMARKING is defined this can be set to one, otherwise it must be set */
                                            /* to zero. See VerifySatStatus() function for details. */
#define BENCHDIMENSION 381                  /* Adjust this to the number of problems per run. */
#else
#define VERIFYSATFINALSTATUS 0              /* Don't modify this. See comments above. */
#endif
//#define MEMORYTUNNING                       /* If this is defined some code is generated for memory limit fine tuning and the */
                                            /* SymMalloc() function is defined and called from Initialize_pgm() and Clean4Exit() */
                                            /* functions to simulate huge memory allocation. */
#ifdef MEMORYTUNNING
#define ADDTLMEMORY 4000000000              /* Additional memory to be allocated if MEMORYTUNNING is defined. */
#endif
#define ADDSTRAT2SEED                       /* If this is defined then when shuffling is active if the active strategy is odd */
                                            /* then it is added to the initial shuffling seed and if it is even it is */
                                            /* subtracted. If this is not defined only pure initial seeds are used. */
#define MEMCHECK 1                          /* Defines how to perform the dynamic memory check in Otter(), Discount(), UEQOtter() */
                                            /* and UEQDscnt() to prevent using more memory than the specified limit according to */
                                            /* the following values: */
                                            /* 0 -> Don't perform dynamic memory check. This is the fastest option and can be used */
                                            /*      in CASC competition as StarExec will kill the process if memory is exceeded. */
                                            /* 1 -> Perform dynamic memory check using mallocmn(), reallocmn() and strdupmn functions. */
                                            /*      This is the second faster option. */
                                            /* 2 -> Same as value 1 and also performs dynamic memory check using GetAvailableMemory() */
                                            /*      function. This is safest way to prevent swapping but slows down  the program */
                                            /*      about 1% of elapsed time. */
                                            /* 3 -> Perform dynamic memory check using mallinfo2() function. This is the more accurate */
                                            /*      way to check memory usage. It may be similar than value 2 in terms of time or */
                                            /*      maybe a bit slower. */
//#define SEMANTICTAUTOLOGY               /* If this is defined then semantic tautology detection is enabled. */

/* Other algorithm parameter definitions. These are candidates to be adjusted via command options. The definitions */
/* of some of them may significantly change the code generation. */
//#define SINGLEANY                       /* If this is defined then the SelectItems() function will select the first */
                                        /* negative literal if selection option is set to SINGLENEG or the first positive */
                                        /* literal if selection is set to SINGLEPOS. If this is NOT defined then the */
                                        /* SelectItems() function will select the MAXIMUM (if it exist) or first MAXIMAL */
                                        /* negative (if SINGLENEG) or positive (if SINGLEPOS) literal with respect to all */
                                        /* the negative literals. The results obtained is that it is better to define SINGLEANY. */
#define DEMODPOSITION 1                 /* If this equals 1 then demodulations are done first in FwdSimplify() function. */
                                        /* If this equals 2 then demodulations are done just after FirstSimplify() call. */
                                        /* If this equals 3 then demodulations are done just after SecondSimplify() call. */
                                        /* Value 1 is a bit better than the other values. */
#define KEEPSATMODEL 1                  /* If this equals 1 then each execution of SatSolver() function keeps the previous model */
                                        /* until a node is found without a forced option match. This is a slightly better option. */
                                        /* If this doesn't equal 1 then each execution of SatSolver() function keeps the previous */
                                        /* model until a node is found without a forced option match and there are still some  */
                                        /* forced options available. */
//#define UNITCLRESOLUTION                /* If this is defined then a resolution between a unit clause without assertions being */
                                        /* forward simplified will be attempted with active unit clauses without assertions. */
                                        /* It is much better not to use this, but the code has been left just in case it may be */
                                        /* useful in the future. */
#define REDUNDANCYCOMPLETE              /* If this is defined then the "complete" (by now) version of RedundancyChk() function */
                                        /* is used. Otherwise the Vampire inspired original and not complete version is used. */
#define LKAHDWITHUNIFY                  /* If this is defined the lookahead calls to QueryDscTree() are followed by calls to */
                                        /* Unify() to improve the accuracy of the lookahead estimation at the cost of */
                                        /* slowing down the program. */
#define WEIGHTLKAHDCLAUSES 5            /* Integer greater than 1. If problem type is 3 or 8 then for some weight queues the weight */
                                        /* of the first WEIGHTLKAHDCLAUSES is corrected with the lookahead number of the clause. */
#define LRSWEIGHTLIMIT                  /* If this is defined then Limited Resource Strategy is used to limit maximum clause weight. */
                                        /* With layered clause selection this is not as accurate but anyway it can help. */
//#define ENHANCEDDESTEQRES               /* If this is defined then the destructive equality resolution simplification is enhanced */
                                        /* in DestrEqResolution() function by generating a null clause if the input clause is a */
                                        /* unit clause inequality and both root terms unify. */
#define MAXGRJVARS 5                    /* Maximum number of variables in a clause to activate ground joinability for it. If this */
                                        /* is not defined then no limit will be imposed. */
//#define SUBSUMPRUNEPLUS                 /* If this is defined then additional pruning criteria will be enabled for subsumption */
                                        /* simplification. See IsSubsumedByClause() function for additional details. */
#define STAREXECTUNING                  /* If this is defined then tuning output will be formated for tuning made in StarExec */
                                        /* environment. Otherwise it will be formated for an specific computer environments so that */
                                        /* it will more be directly usable for further processing by using the TMFACTOR option. */
//#define ENABLECHOICEAXIOM               /* If this is defined then a choice axiom is generated and reported in proofs for skolemization */
                                        /* inferences. Otherwise choice axioms are not generated nor reported for skolemizations. */

/* Definitions for parse functions return types. */
#define NEGATION 1                          /* Negation. */
#define UNIVERSALQ 2                        /* Valid universal quantifier. */
#define EXISTENTIALQ 3                      /* Valid existential quantifier. */
#define BRACKET 4                           /* Open parenthesis "(". */
#define VALIDATOMIC 5                       /* Valid atomic sentence. */
#define UNDECLAREDTERM 6                    /* Predicate or function not previously declared. */
#define DECLAREDFUNC 7                      /* Term has been previously declared as a function. */
#define DECLAREDPRED 8                      /* Term has been previously declared as a predicate. */
#define STDVARNAME 9                        /* Standardized variable reserved name. */
#define DECLAREDVAR 10                      /* Declared variable. */
#define VALIDVARLIST 11                     /* Valid variable list. */
#define VALIDSENTENCE 12                    /* Valid sentence. */
#define VALIDNAME 13                        /* Valid name. */

/* Define's for syntax error codes. */
#define NOMEMORY 100000100                  /* No memory. */
#define EMPTYSENTENCE 100000101             /* Empty sentence. */
#define EMPTYBLOCK 100000102                /* Empty parenthesis block. */
#define INVTPTPVARNAME 100000103            /* Invalid TPTP variable name. */
#define INVTPTPNAME 100000104               /* Invalid TPTP predicate or function name. */
#define INVALIDATOMIC 100000105             /* Invalid atomic sentence. */
#define INVALIDVARLIST 100000106            /* Invalid variable list. */
#define INVALIDNAME 100000107               /* Invalid or too long variable, constant, predicate or function name. */
#define INVALIDTERM 100000109               /* Invalid term. */
#define MISSINGARGS 100000110               /* Missing arguments. */
#define ALREADYDECLARED 100000111           /* Variable name already declared in equal scope quantifier. */
#define MISSINGVARINLIST 100000112          /* Missing variable in a variable list. */
#define INVALIDDECLAREDARGS 100000113       /* Predicate or function doesn't match previous declared number of arguments. */
#define INVALIDDECLAREDTYPE 100000114       /* Predicate or function doesn't match previous declared type. */
#define TOOMANYARGS 100000115               /* Too many arguments. */
#define RESERVEDNAME 100000116              /* Use of standard variable name for a function or predicate. */
#define SYNTAXERROR 100000117               /* Generic syntax error. */

/* Definitions for flags field of kbase structure. */
#define RETENTIONUSED 1                     /* LRS or weight limit retention processes have been used and some clauses have */
                                            /* been discarded. The KB now is not complete. */

/* Definitions for flags field of cmprefix structure and type argument of simplify functions. */
/* These flags must be compatible with SELECTED flag and also with HORN flag except FROMNOTAXIOM */
/* that may have the same value as HORN flag. The HORN flag is set in clauses if appropriate but */
/* it is not used by the moment. The flags FROMCONJECTURE, FROMAXIOM, FROMNOTAXIOM and FROMMAINFILE */
/* must be compatible with flags in type field of hashchain structure. */
#define TEXTFORMULA 1                       /* Formula is of text type. */
                                            /* These formulas have their own linked queue. */
#define UNPROC 2                            /* Formula is an unprocessed binary clause. */
                                            /* These formulas have their own linked queue. */
#define PASSIVE 4                           /* Formula is a passive binary clause. */
                                            /* These formulas have their own linked queue. */
#define ACTIVE 8                            /* Formula is an active binary clause. */
                                            /* These formulas have their own linked queue. */
#define LOCKED 16                           /* Formula is a locked binary clause. */
                                            /* These formulas have their own linked queue. */
#define NOFACTORS 32                        /* Clause have no factors. */
#define INDISPOSALQUEUE 128                 /* Clause is in disposal queue for later deletion or locking. */
#define FROMPASSIVE 256                     /* Clause is coming from PASSIVE, so only subsumption must be checked */
                                            /* in FirstSimplify() function. This is only used as a flag argument in */
                                            /* some simplifying functions. This is not used in the flags field of the */
                                            /* cmprefix structure. */
#define NUMBERED 512                        /* Clause has been assigned a number. */
#define INPROOFQUEUE 1024                   /* Clause or hashchcmp component is already in proof queue. This flag must be */
                                            /* compatible with hashchcmp structure flags. */
#define ASSERTSCHAINED 2048                 /* Assertions for this clause has been already chained. Only used for */
                                            /* binary clauses. */
#define FROMCONJECTURE 4096                 /* Formula was obtained from a negated conjecture in the pre-processing. */
                                            /* Checking this flag is performed in ClauseRelevance() and PrintClausifiedFormulas() functions. */
                                            /* Also used in hashchain type field flags for symbols appearing in these formulas. */
#define KEPTCLAUSE 16384                    /* Clause was kept in the relevance filtering pre-processing. */
#define FROMNOTAXIOM 32768                  /* Symbol appears in axiom like formulas hypothesis, definition, assumption, */
                                            /* lemma, theorem and corollary, but not in formulas with axiom role. */
											/* Used only in hashchain type field flags for symbols appearing in these clauses. */
#define FROMMAINFILE 65536                  /* Clause is coming from the main file and can be considered as a conjecture */
                                            /* to the effects of detecting contradictory axioms. */
#define PICKED 131072                       /* Clause has been selected as ACTIVE. Due to AVATAR architecture an active clause */
                                            /* may later be an unprocessed clause so any type of clause may have this flag. */
#define WEIGHTED 262144                     /* Weight, avatar and horn distances have been already assigned to a clause. */
#define FROMAXIOM 524288                    /* Symbol appears in axiom formulas. Used only in hashchain type field flags */
                                            /* for symbols appearing in these clauses. */
#define GROUNDJOINABLE 1048576              /* Formula is a ground joinable binary clause. These formulas have their own */
                                            /* linked queue but use the same top and bottom pointers as UNPROC clauses, that is, */
                                            /* frstunproc and lstunproc fields in kbase structure. */
#define WEAKLYORIENTED 2097152              /* Clause is a binary positive unit equality weakly oriented. */

/* Definitions for inference field of cmprefix structure. */
#define AXIOM 0                             /* Formula is an axiom. */
#define HYPOTHESIS 1                        /* Formula is an hypothesis (axiom like). */
#define DEFINITION 2                        /* Formula is a definition (axiom like). */
#define ASSUMPTION 3                        /* Formula is an assumption (axiom like). */
#define NEGCONJ 4                           /* Formula is a negated conjecture. This is also used as a flag and must be */
                                            /* compatible with FROMMAINFILE above. */
#define CONJECTURE 5                        /* Formula was an input conjecture to the problem. */
#define LEMMA 6                             /* Formula is a lemma (axiom like, redundant). */
#define THEOREM 7                           /* Formula is a theorem (axiom like, redundant). */
#define COROLLARY 8                         /* Formula is a corollary (axiom like, redundant). */
#define PLAIN 9                             /* Formula has a plain role, accepted as an axiom just in case . */
#define CLAUSIFY 10                         /* Formula comes from clausifying an input formula. */
#define RESOLUTION 11                       /* Formula was inferred by resolution. */
#define PARAMODULATION 12                   /* Formula was inferred by paramodulation. */
#define FACTORING 13                        /* Formula was inferred by factoring. */
#define FWDEMODULATION 14                   /* Formula was inferred by forward demodulation. */
#define BKDEMODULATION 15                   /* Formula was inferred by backward demodulation. */
#define REMOVEDUPS 16                       /* Formula is the result of removing duplicate literals from another formula. */
#define TRIVEQRES 17                        /* Formula was inferred by trivial equality resolution. */
#define EQRESOLUTION 18                     /* Formula was inferred by equality resolution. */
#define DESTEQRES 19                        /* Formula was inferred by destructive equality resolution. */
#define EQFACTORING 20                      /* Formula was inferred by equality factoring. */
#define FWSUBSRESLTNS 21                    /* Formula was inferred by forward subsumption resolution. */
#define BKSUBSRESLTNS 22                    /* Formula was inferred by backward subsumption resolution. */
#define SPLIT 23                            /* Formula was inferred by clause splitting. */
#define COMPONENT 24                        /* Formula is a clause component. */
#define CONTRADICTION 25                    /* Formula was inferred by first order solver contradiction. */
#define PRENNFCONV 26                       /* Formula is the result of a pre-nnf conversion. */
#define FORMULARENAMING 27                  /* Formula is the result of renaming a subformula. */
#define PREDICATEDEF 28                     /* Formula is a predicate definition for subformula renaming. */
#define NNFCONV 29                          /* Formula is the result of a full-nnf conversion. */
#define MINISCOPING 30                      /* Formula is the result of miniscoping. */
#define SKOLEMIZATION 31                    /* Formula is the result of skolemization. */
#define TFSIMPLIFICATION 32                 /* Formula is the result of a $true and $false simplification. */
#define EQUALITYSPLIT 33                    /* Formula is the result of a positive unit equality split. */
#define GOALDEFINITION 34                   /* Formula is a definition related to a goal subterm in UEQ problems. */
#define DEFINTNFOLDING 35                   /* Formula is a folding of goal related definitions in UEQ problems. */
#ifdef ENABLECHOICEAXIOM
#define CHOICEAXIOM 36                      /* Formula is an axiom of choice for a skolemization. */
#endif

/* Definitions for byte markers in compiled clauses and discrimination trees, type field of hashchain */
/* structures and type parameter of CreateGlobalSymbol() function. These flags must be compatible */
/* with FROMCONJECTURE, FROMAXIOM, FROMNOTAXIOM and FROMMAINFILE also used in type field of hashchain */
/* structures for learning process. PREDICATE and FUNCION are also used in flags field of nameschain structure. */
#define UNITEND 1                       /* End of clause marker. Only used for byte markers. */
                                        /* This flag must be compatible with flags for Generalize() and */
                                        /* Unify() functions and with flags in hashchcmp and satnode structures */
                                        /* and assertions signbyte (see below). */
#define EQUALITY 2                      /* Item is an equality literal. Also used in UpdateSymbolParms() function flags. */
#define PREDICATE 4                     /* Item is a predicate literal. Also used in nameschain structure flags. */
#define NEGATED 8                       /* Item is a negated literal. Only used for byte markers and in */
                                        /* UpdateSymbolParms() function flags. */
                                        /* This flag must be compatible with flags for Generalize() and Unify() */
                                        /* functions (see below). */
#define VARIABLE 16                     /* Item is a variable. Only used for byte markers and nameschain structure flags. */
#define FUNCTION 32                     /* Item is a function (may be a constant, i.e. arity zero). */
#define SELECTED 64                     /* Item is a selected literal or equality root term. */
                                        /* This flag must be compatible with flags for QueryGeneric() function. */
#define MAXIMUM 128                     /* Item is a maximum equality root term. This must be different than ITEM1SEC, STANDARD */
                                        /* and NONRECURSIVE flags. */
#define PMSELECTED 128                  /* If this is set to the same value as MAXIMUM then a positive equality will be selected */
                                        /* for paramodulating from it only if it is in a clause without negative literals. */
                                        /* If this is set to the same value as SELECTED then the above condition will not */
                                        /* be required. It works a bit better setting it to the same value as MAXIMUM. */
#define DEFINEDPRED 256                 /* Symbol is a defined predicate. Used only in type field of hashchain structure. */
#define RELEVANT 1024                   /* Symbol is in a relevant clause. Used only in type field of hashchain */
                                        /* structure for clause relevance filtering. */
#define SKOLEM 2048                     /* Symbol is a skolem. Used only in type field of hashchain structure . */
#define DISTINCTOBJECT 8192             /* Symbol is a "distinct object". Used only in type field of hashchain structure */
                                        /* and type parameter of CreateGlobalSymbol() function. */

/* Definitions for "type" parameter in NextItem() function. */
#define OVERSUBTERMS 1                  /* Jump to next item that is not a subterm of the given item. */
#define IMMED 2                         /* Jump to item immediately following the given item. */

/* Definitions for Unify() and Generalize() flags. */
/* These flags must be compatible with the UNITEND and NEGATED flags (see above). */
#define LIST 2                          /* Parts to be unified are a list of terms. */
#define ITEM1SEC 16                     /* The first Unify() function argument or the variable in a substitution */
                                        /* belongs to a secondary clause. They belong to the main clause by default. */
                                        /* Also used for flag value of CheckEqRootOrder() function so it must be */
                                        /* different than MAXIMUM, STANDARD and NONRECURSIVE values defined elsewhere. */
#define ITEM2SEC ITEM1SEC<<1            /* The second Unify() function argument or the substituting term in a */
                                        /* substitution  belongs to a secondary clause. They belong to the main */
                                        /* clause by default. This must be ITEM1SEC shifted by 1 bit. */
#define INITSUBST 64                    /* Initialize substitution to null. */
#define DEBUG 128                       /* Call BuildTxtSubstData() function to show results in text form. */

/* Definitions for status fields of kbase and mpctl structures. They must be compatible with NOMEMORY above. */
#define STOPPED 0                       /* Process is stopped. */
#define RUNNING 1                       /* Process is running a (possibly multi-processing) first order solver algorithm */
                                        /* with or without a SAT solver. */
#define RUNNINGSAT 2                    /* Process is running a single process SAT solver algorithm. */
#define SATISFIABLE 3                   /* Formulas in KB are satisfiable, if there are conjectures */
                                        /* they are not refutable. */
#define UNSATISFIABLE 4                 /* Formulas in KB are unsatisfiable, if there are conjectures */
                                        /* they are refutable. */
#define UNKNOWN 5                       /* It is unknown if formulas in KB are satisfiable or unsatisfiable. */
#define TIMEOUT 6                       /* Saturation algorithm timed out. This cannot be 0 or 1. */
#define SATQTIMEOUT 7                   /* A timeout was produced in SatQ2SatSolver() function. */
#define SLICETMOUT 8                    /* Slice timeout or instruction limit reached. */

/* Ordering types definitions. */
/* IMPORTANT: If additional ordering types are added then MAXLEARNRECORDS definition must be changed. */
#define STANDARD 1                      /* Use standard KBO ordering. */
#define NONRECURSIVE 2                  /* Use non recursive KBO ordering. This flag must be compatible  with */
                                        /* MAXIMUM and ITEM1SEC flags defined elsewhere. */

/* Definitions for demodulation and destructive equality resolution options (DEMODULAITON option). */
#define DEMODON 0                       /* Demodulation is enabled without restrictions. */
#define DEMODOFF 1                      /* Demodulation is disabled. */
#define DEMODCPL1 2                     /* Demodulation and destructive equality resolution are restricted */
                                        /* to cases that don't compromise completeness. */
#define DEMODCPL2 3                     /* Similar to DEMODCPL1 but with a more restrictive criteria for demodulation. */
#define DEMODONORNT 4                   /* Demodulation is enabled without restrictions with only oriented equalities. */
#define DEMODCPL1ORNT 5                 /* Demodulation and destructive equality resolution are restricted */
                                        /* to cases that don't compromise completeness. Demodulation is performed */
                                        /* only with oriented equalities. */
#define DEMODCPL2ORNT 6                 /* Similar to DEMODCPL1ORNT but with a more restrictive criteria for demodulation. */

/* Definitions for weight functions. */
/* IMPORTANT: If additional types of weight functions are added then MAXLEARNRECORDS definition must be changed. */
#define UNIFORM 0                       /* Use a uniform weight function assigned a weight=1 to all functions. */
#define ARITY 1                         /* Use weight function = 1 + 10·arity(function). */

/* Definitions for field precedence of options structure. */
#define PRCARITY 0                      /* Symbol precedence is set to arity. */
#define PRCOCCUR 1                      /* Symbol precedence is set to occurrence. */
#define PRCFREQ 2                       /* Symbol precedence is set to frequency. */
#define PRCINVAR 3                      /* Symbol precedence is set to inverse arity. */
#define PRCINVOC 4                      /* Symbol precedence is set to inverse occurrence. */
#define PRCINVFR 5                      /* Symbol precedence is set to inverse frequency. */

/* Definitions for field precedence of options structure. */
#define ISPRCLOW 0                      /* Low precedence for symbols introduced in the pre-processing. */
#define ISPRCHIGH 1                     /* High precedence for symbols introduced in the pre-processing. */

/* Definitions for ordering functions return values and for the result field of the */
/* equpnode structure. See the functions description for the meaning of each one, */
/* as it may differ from one function to another. */
#define EQUAL 1                         /* item1 = item2. */
#define GREATER 2                       /* item1 > item2. */
#define LESSER 4                        /* item2 > item1. */
#define NOGREATER 8                     /* item2 ·> item1 but neither item2 = item1 nor item2 > item1. */
#define NOLESSER 16                     /* item1 ·> item2 but neither item1 = item2 nor item1 > item2. */
#define FAILURE 32                      /* None of the above. */
#define EQUALW 64                       /* weight(item1) = weight(item2). */
#define FAILUREW 128                    /* weight(item1) is not comparable with weight(item2). */

/* Definitions for algorithm field of kbase structure. */
/* IMPORTANT: If additional algorithms are added then MAXLEARNRECORDS definition must be changed. */
#define OTTER 2                         /* Otter algorithm. */
#define DISCOUNT 3                      /* Discount algorithm. */
#define UEQDSC 4                        /* Unfailing completion Discount algorithm. */
#define UEQOTT 5                        /* Unfailing completion Otter algorithm. */

/* Definitions for EditDiscrTrees() function flag parameter. */
#define ADDTREES 1                      /* Add tree branches for specified clause. */
#define DELETETREES 2                   /* Delete tree branches for specified clause. */

/* Definitions for type argument of QueriDscTree() function. */
#define UNIFICATION 0                   /* Search is for an item that unifies with the query term. */
#define INSTANCE 1                      /* Search is for an item that is an instance of the query term. */
#define GENERALIZATION 2                /* Search is for an item that is a generalization of the query term. */

/* Definitions for flags of AddBinClause2KB() and AddBinClause2KBUEQ() functions. */
#define SELECT 1                        /* Appropriate clause items will be selected. */

/* Definitions for flags of JumpOverBlockOrTerm() function. */
#define TOEND 0                         /* Jump to the last item in the block or term. */
#define PASTEND 1                       /* Jump to the item following the last item in the block or term. */

/* Definitions for mxwghtopt field of options structure. */
#define MXWEIGHTOFF 1                   /* Maximum weight option deactivated for algorithms without LRS enabled. */
#define MXWEIGHTLRS 2                   /* Maximum weight calculated using Limited Resource Strategy enabled */
                                        /* for all algorithms. */
#define MXWEIGHTLRSOTT 4                /* Maximum weight calculated using Limited Resource Strategy enabled for Otter */
                                        /* algorithm, disabled for Discount algorithm. */
#define MXWEIGHTLRSDSC 8                /* Maximum weight calculated using Limited Resource Strategy enabled for Discount */
                                        /* algorithm, disabled for Otter algorithm. */
#define MXWEIGHTLRSOFF 16               /* Maximum weight calculated using Limited Resource Strategy id disabled */
                                        /* for all algorithms. */
#define MXWEIGHTNUM 32                  /* Maximum weight selected by user for algorithms without LRS enabled. */

/* Definitions for SYNTAX and PROOF options. */
#define TPTP 1                          /* TPTP mode syntax. */
#define DRODI 2                         /* Drodi mode syntax. */
#define PROOFOFF 3                      /* Don't print proof. */

/* Definitions for program modes (pgmmode global variable). */
#define NONINTERACTIVE 0                /* non interactive mode, entered if some arguments are given to the program. */
#define INTERACTIVE 1                   /* Interactive mode, entered if no arguments are given to the program. */

/* Definitions for select field of options structure. The definitions are set as if they were flags */
/* to speed up some tests, but only one of the values (flags) is allowed. */
/* IMPORTANT: If additional SELECT types are added then MAXLEARNRECORDS definition must be changed. */
#define MAXIMAL 1                       /* All maximal literals are selected. */
#define SINGLENEG 2                     /* If there are negative literals then the first one is selected */
                                        /* and the other literals are not selected. Otherwise all */
                                        /* maximal literals are selected. */
#define MULTINEG 4                      /* If there are negative literals then the negative literals */
                                        /* that are maximal in the set of negative literals are selected */
                                        /* and the other literals are not selected. Otherwise all */
                                        /* maximal literals are selected. */
#define SINGLEPOS 8                     /* If there are positive literals then the first one is selected */
                                        /* and the other literals are not selected. Otherwise all */
                                        /* maximal literals are selected. */
#define MULTIPOS 16                     /* If there are positive literals then the positive literals */
                                        /* that are maximal in the set of positive literals are selected */
                                        /* and the other literals are not selected. Otherwise all */
                                        /* maximal literals are selected. */
#define SELECTALL 32                    /* Select all literals. */
#define MXSINGLENEG 64                  /* Equivalent to strategy 1 of document Selecting the Selection by Hoder. */
#define TYPE1 128                       /* Equivalent to strategy 1002 of document Selecting the Selection by Hoder. */
#define TYPE2 256                       /* Equivalent to strategy 1003 of document Selecting the Selection by Hoder. */
#define TYPE3 512                       /* Equivalent to strategy 1004 of document Selecting the Selection by Hoder. */
#define TYPE4 1024                      /* Equivalent to strategy 1010 of document Selecting the Selection by Hoder. */
#define TYPE5 2048                      /* Equivalent to strategy 1011 of document Selecting the Selection by Hoder. */
#define TYPE6 4096                      /* Equivalent to strategy 1012 of document Selecting the Selection by Hoder. */
#define TYPE1CPL 8192                   /* Complete version of TYPE1. */
#define TYPE2CPL 16384                  /* Complete version of TYPE2. */
#define TYPE3CPL 32768                  /* Complete version of TYPE3. */
#define TYPE4CPL 65536                  /* Complete version of TYPE4. */
#define TYPE5CPL 131072                 /* Complete version of TYPE5. */
#define TYPE6CPL 262144                 /* Complete version of TYPE6. */

/* define for complete selections mask. */
#define COMPLETESELECTIONS (MAXIMAL|SINGLENEG|MULTINEG|TYPE1CPL|TYPE2CPL|TYPE3CPL|TYPE4CPL|TYPE5CPL|TYPE6CPL|SELECTALL)

/* Definitions for flags field in hashchcmp and satnode structures and assertions signbyte */
/* (see AddAssertions() function comments). These definitions must be compatible with UNITEND */
/* that is also used in assertions signbyte and with INPROOFQUEUE cmprefix structure flag */
/* that is also used in hashchcmp flags. */
#define POSITIVE 2                      /* The component is true in SAT interpretation model or in assertion_data. */
#define NEGATIVE 4                      /* The component is false in SAT interpretation model or in assertion_data. */
#define UNDEFINED 8                     /* The component is undefined in SAT interpretation model. This is only used */
                                        /* in flags field of hashchcmp structure. */
#define GROUND 16                       /* The component is ground. This is only used in flags field of hashchcmp structure. */
                                        /* This flag must be compatible with flags for pbmflags global variable and serves */
                                        /* to identify ground problems. */
#define PRPOSITIVE 32                   /* The component was true in previous model of SAT interpretation or in assertion_data. */
                                        /* This is only used in flags field of hashchcmp structure. */
#define PRNEGATIVE 64                   /* The component was false in previous model of SAT interpretation or in assertion_data. */
                                        /* This is only used in flags field of hashchcmp structure. */
#define PRUNDEFINED 128                 /* The component was undefined in previous model of SAT interpretation. This is only used */
                                        /* in flags field of hashchcmp structure. */
#define LASTOPTION 256                  /* The assignment of node is the last possibility available, no other possibility */
                                        /* (true or false) is left or valid to be explored.*/
#define ORDERED 512                     /* The hashchcmp or satclause structure element has been linked to the ordered */
                                        /* ordered sub-queue. */
#define FREENODE 2048                   /* The node is allocated but it is not in use and its data is no longer necessary. */
                                        /* The node can be reused. This way unnecessary memory allocation and freeing is */
                                        /* avoided. This is only used in flags field of satnode structure. */
#define MODELPOSITIVE 4096              /* The component is true in current model of SAT interpretation or in assertion_data. */
                                        /* This is only used in flags field of hashchcmp structure. */
#define MODELNEGATIVE 8192              /* The component is false in current model of SAT interpretation or in assertion_data. */
                                        /* This is only used in flags field of hashchcmp structure. */
#define MODELUNDEFINED 16384            /* The component is undefined in current model of SAT interpretation. This is only used */
                                        /* in flags field of hashchcmp structure. */
#define POSCOMPADDED 32768              /* The component clause C <- [C] has been added to first order solver. This is */
                                        /* only used in flags field of hashchcmp structure. */
#define NEGCOMPADDED 65536              /* The component clause ~C <- ~[C] has been added to first order solver. This is */
                                        /* only used in flags field of hashchcmp structure. */

/* Definitions for flags field in subslitrl structure. These must be compatible with POSITIVE, NEGATIVE and UNDEFINED */
/* because they are also used by this field. */
#define SUBST1OFF 16                    /* The boolean literal owns two substitutions but the first one is disabled because is */
                                        /* incompatible with the global subsumption substitution while the second one is */
                                        /* still compatible. */
#define SUBST2OFF 32                    /* The boolean literal owns two substitutions but the second one is disabled because is */
                                        /* incompatible with the global subsumption substitution while the first one is */
                                        /* still compatible. */
#define SECONDTRUEASSGN 64              /* The boolean literal owns two substitutions, both are compatible with the global */
                                        /* subsumption substitution and the literal has been assigned POSITIVE a second time. */
                                        /* The first time that the literal was assigned POSITIVE its first substitution was */
                                        /* included in the global substitution and now in the second time its second substitution */
                                        /* has been included in the global substitution. */
#define BYSUBST1 128                    /* The boolean literal has been unlinked from the eligible list and linked to the */
                                        /* main propagation list because of incompatibility of its first substitution. */
                                        /* Equalities and inequalities may have two possible substitutions due to symmetry. */
                                        /* This flag is set only if flag SUBST1OFF is also set and is compatible */
                                        /* with BYSUBST2 flag. */
#define BYSUBST2 256                    /* The boolean literal has been unlinked from the eligible list and linked to the */
                                        /* main propagation list because of incompatibility of its second substitution. */
                                        /* Equalities and inequalities may have two possible substitutions due to symmetry. */
                                        /* This flag is set only if flag SUBST2OFF is also set and is compatible */
                                        /* with BYSUBST1 flag. */

/* Definitions for flags in pbmflags global variable. The GROUND flag defined above is also used for pbmflags. */
#define EXISTCONJ 1                     /* A conjecture exist for the current problem. */
#define EXISTNEGCONJ 2                  /* A negated conjecture exist for the current problem. */
#define WITHHORNCLAUSES 4               /* Problem is not horn but there are horn clauses. */
#define UNITEQUALITY 8                  /* Problem has some positive unit equalities. */
#define NONUNITEQUALITY 32              /* Problem has some equalities none of which are positive unit equalities. */
#define UNITCLAUSE 64                   /* All clauses in problem are unit clauses. */
#define NOTCAX 128                      /* The proof includes some formulas from main file and therefore has not */
                                        /* contradictory axioms. */
#define SATREFUTATION 256               /* There was a Sat refutation. */
#define HORN 32768                      /* Problem is horn. This flag must be compatible with flags field */
                                        /* of cmprefix structure. */
#define CNFFORMAT 65536                 /* Problem was read in CNF TPTP format. */
#define EXISTAXIOMFILE 131072           /* Problem included an axiom file. */
#define EXISTAXIOMLIKE 262144           /* Problem has axiom or axiom like formulas. */

/* Definitions for type field in proofnode structure. */
#define SYMBOLDEFNTN 1                  /* Symbol definition. */
#define SATCLAUSE 2                     /* Sat clause. */
#define ACLAUSE 3                       /* A-Clause. */

/* Definitions for flags field in lkedfitem structure. */
#define OPENBRACKET 1                   /* Opening bracket type item. */
#define CLOSEBRACKET 2                  /* Closing bracket type item. */
#define EQUPRED 4                       /* Equality predicate type item. */
#define NAMEDPRED 8                     /* Normal predicate type item. */
#define NAMEDFUNC 16                    /* Function type item. */
#define NAMEDVAR 32                     /* Variable type item. */
#define UQUANTIFIER 64                  /* Universal quantifier type item. */
#define EQUANTIFIER 128                 /* Existential quantifier type item. */
#define ANDOPER 256                     /* AND connective type item. */
#define OROPER 512                      /* OR connective type item. */
#define IMPOPER 1024                    /* Implication connective type item. */
#define REVIMPOPER 2048                 /* Reverse implication connective type item. */
#define EQUIVOPER 4096                  /* Equivalence connective type item. */
#define XOROPER 8192                    /* XOR connective type item. */
#define NOROPER 16384                   /* NOR connective type item. */
#define NANDOPER 32768                  /* NAND connective type item. */
#define NEGITEM 65536                   /* Negated item. */
#define POSPOL 131072                   /* Positive polarity item. */
#define NEGPOL 262144                   /* Negative polarity item. */
#define NEUTRALPOL 524288               /* Neutral polarity item. */
#define RENAMED 1048576                 /* Variable renamed in a quantifier or variable item. */
#define FREEVAR 2097152                 /* The quantifier variable is a free variable in the renamed block. */
#define TRUEPRED 4194304                /* The predicate is a $true predefined predicate. */
#define FALSEPRED 8388608               /* The predicate is a $false predefined predicate. */

/* Definitions for flags parameter of IsConnected() function. */
#define SUBSTITUTION2 1                 /* Use a₁≻a₂≻ ... ≻aₚ ground substitution for variables. */
#define MXTERMCHECKED 2                 /* mxterm argument has been checked for being LESSER than the */
                                        /* original critical pair top term. */
#define MNTERMCHECKED 4                 /* mnterm argument has been checked for being LESSER than the */
                                        /* original critical pair top term. */

/* Definitions for childtype field of dstrnode structure. */
#define NODECHILD 0                     /* Child is a tree node (dstrnode structure). */
#define LEAFCHILD 1                     /* Child is a tree leaf (dstrleaf structure). */

/* Definitions for clause relevance filtering program option (prproflags global variable). */
#define CLFILTERINGRESIGN 1             /* Clause relevance filtering is not performed performed in case of timeout condition. */
#define CLFILTERINGOFF 2                /* No clause relevance filtering is performed. */
#define CLFILTERINGON 4                 /* Clause relevance filtering is always performed. */
#define CLFILTERINGNONEQU 8             /* Clause relevance filtering is performed in problems without equalities only. */
                                        /* Only one of the CLFILTERINGOFF, CLFILTERINGON or CLFILTERINGNONEQU flags can be set. */
                                        /* However CLFILTERINGRESIGN flag is compatible with the other three flags. */
#define CLFILTERINGITERLIMIT 16         /* The threshold in ppthrshld global variable is the maximum number of iterations */
                                        /* allowed in the relevance filtering algorithm. Otherwise the threshold in ppthrshld */
                                        /* global variable is the maximum time in seconds allowed for the algorithm. */
#define CLFILTERINGLIMITREACHED 32      /* A timeout condition occurred or maximum number of iterations reached during */
                                        /* relevance filtering pre-processing before */
                                        /* the filtering process was completed. */
#define CLFILTERINGPRUNNED 64           /* The relevance filtering deleted one or more clauses. */
#define CLFILTERINGTIMEOUT 0.1          /* Default timeout for relevance filtering pre-processing. */

/* Definitions of masks for each type of adjustable parameter. */
#define ALGMASK 0b100000000000000000001L                       /* ALGORITHM parameter mask. */
#define SELECTMASK 0b11001000000000110L                        /* SELECT parameter mask. */
#define LAYERMASK 0b1000L                                      /* Layer parameter mask. */
#define DEMODMASK 0b10000000000000000000100000000010000L       /* Demodulation parameter mask. */
#define FACTORINGMASK 0b100000L                                /* FACTORING parameter mask. */
#define TERMORDRMASK 0b1000000L                                /* TERMORDER parameter mask. */
#define LITORDRMASK 0b110000000L                               /* LITORDER parameter mask. */
#define FUNCWMASK 0b1000000000L                                /* FUNCTIONW parameter mask. */
#define MAXWMASK 0b110000000000L                               /* MAXWEIGHT parameter mask. */
#define LEARNMASK 0b10000000000000L                            /* Learn option mask. */
#define LOOKAHEADMASK 0b100000000000000000L                    /* LOOKAHEAD parameter mask. */
#define GAMMAMASK 0b10000011000000000000000000L                /* GAMMA parameter mask. */
#define GRJOINMASK 0b11000000000000000000000L                  /* GRJOIN parameter mask. */
#define CONNECTMASK 0b100000000000000000000000L                /* CONNECT parameter mask. */
#define WEAKRWMASK 0b1000000000000000000000000L                /* WEAKRW parameter mask. */
#define PRCMASK 0b11100000000000000000000000000L               /* PRECEDENCE parameter mask. */
#define ISPRCMASK 0b100000000000000000000000000000L            /* Introduced symbol precedence mask. */
#define WARMASK   0b111000000000000000000000000000000L         /* SELECTRATIO parameter mask. */
#define SPLITMASK 0b1000000000000000000000000000000000L        /* SPLIT parameter mask. */
#define GOALXFRMMASK 0b100000000000000000000000000000000000L   /* Place holder for future goal transformation parameter mask. */

/* Definitions of values for each type of adjustable parameter. */
#define OTTER_P 0L                                         /* Otter algorithm. */
#define DISCOUNT_P 1L                                      /* Discount algorithm. */
#define UEQDSC_P 0b100000000000000000000L                  /* Unfailing completion Discount algorithm. */
#define UEQOTT_P 0b100000000000000000001L                  /* Unfailing completion Otter algorithm. */
#define SINGLENEG_P 0L                                     /* SINGLENEG SELECT option. */
#define MULTINEG_P 0b10L                                   /* MULTINEG SELECT option. */
#define MAXIMAL_P 0b100L                                   /* MAXIMAL SELECT option. */
#define SINGLEPOS_P 0b110L                                 /* SINGLEPOS SELECT option. */
#define MULTIPOS_P 0b1000000000000L                        /* MULTIPOS SELECT option. */
#define LAYER1_P 0L                                        /* Layer 1 option. */
#define LAYER2_P 0b1000L                                   /* Layer 2 option. */
#define DEMODONORNT_P 0L                                   /* Demodulation is ON and only for oriented equalities. */
#define DEMODOFF_P 0b10000L                                /* Demodulation is OFF. */
#define DEMODCPL1ORNT_P 0b100000000010000L                 /* Demodulation is COMPLETE and only for oriented equalities, option 1. */
#define DEMODCPL2ORNT_P 0b100000000000000L                 /* Demodulation is COMPLETE and only for oriented equalities, option 2. */
#define DEMODON_P 0b10000000000000000000000000000000000L   /* Demodulation is ON. */
#define DEMODCPL1_P 0b10000000000000000000100000000010000L /* Demodulation is COMPLETE, option 1. */
#define DEMODCPL2_P 0b10000000000000000000100000000000000L /* Demodulation is COMPLETE, option 2. */
#define FACTON_P 0L                                        /* ON FACTORING option. */
#define FACTOFF_P 0b100000L                                /* OFF FACTORING option. */
#define TRMORDSTD_P 0L                                     /* STANDARD TERMORDER option. */
#define TRMORDNR_P 0b1000000L                              /* NONRECURSIVE TERMORDER option. */
#define LITORDSTD_P 0L                                     /* STANDARD LITORDER option. */
#define LITORDNR_P 0b10000000L                             /* NONRECURSIVE LITORDER option. */
#define UNIFORM_P 0L                                       /* UNIFORM FUNCTIONW option. */
#define ARITY_P 0b1000000000L                              /* ARITY FUNCTIONW option. */
#define LRSOTT_P 0L                                        /* LRSOTTER MAXWEIGHT option. */
#define LRSOFF_P 0b10000000000L                            /* LRSOFF MAXWEIGHT option. */
#define LRSALL_P 0b100000000000L                           /* LRS MAXWEIGHT option. */
#define LRSDSC_P 0b110000000000L                           /* LRSDISCOUNT MAXWEIGHT option. */
#define NOLEARN_P 0L                                       /* Learning disabled. */
#define LEARN_P 0b10000000000000L                          /* Learning enabled. */
#define SELECTALL_P 0b1000000000010L                       /* ALL SELECT option. */
#define MXSINGLENEG_P 0b1000000000100L                     /* MXSINGLENEG SELECT option. */
#define TYPE1_P 0b1000000000110L                           /* TYPE1 SELECT option. */
#define TYPE2_P 0b1000000000000000L                        /* TYPE2 SELECT option. */
#define TYPE3_P 0b1000000000000010L                        /* TYPE3 SELECT option. */
#define TYPE4_P 0b1000000000000100L                        /* TYPE4 SELECT option. */
#define TYPE5_P 0b1000000000000110L                        /* TYPE5 SELECT option. */
#define TYPE6_P 0b1001000000000000L                        /* TYPE6 SELECT option. */
#define TYPE1CPL_P 0b1001000000000010L                     /* TYPE1CPL SELECT option. */
#define TYPE2CPL_P 0b1001000000000100L                     /* TYPE2CPL SELECT option. */
#define TYPE3CPL_P 0b1001000000000110L                     /* TYPE3CPL SELECT option. */
#define TYPE4CPL_P 0b10000000000000000L                    /* TYPE4CPL SELECT option. */
#define TYPE5CPL_P 0b10000000000000010L                    /* TYPE5CPL SELECT option. */
#define TYPE6CPL_P 0b10000000000000100L                    /* TYPE6CPL SELECT option. */
#define LKAHEADOFF_P 0L                                    /* Lookahead is off. */
#define LKAHEADON_P 0b100000000000000000L                  /* Lookahead is on. */
#define GRJOINOFF_P 0L                                     /* Ground joinability is off. */
#define GRJOINON_P 0b1000000000000000000000L               /* Ground joinability is on. */
#define GRJOINMED_P 0b11000000000000000000000L             /* Ground joinability is set to med. */
#define CONNECTOFF_P 0L                                    /* Connectedness is off. */
#define CONNECTON_P 0b100000000000000000000000L            /* Connectedness is on. */
#define WEAKRWOFF_P 0L                                     /* Weak rewrite is off. */
#define WEAKRWON_P 0b1000000000000000000000000L            /* Weak rewrite is on. */
#define GAMMA0_P 0L                                        /* Set GAMMA to 0. */
#define GAMMA1_P 0b1000000000000000000L                    /* Set GAMMA to 0.1. */
#define GAMMA2_P 0b10000000000000000000L                   /* Set GAMMA to 0.2. */
#define GAMMA3_P 0b11000000000000000000L                   /* Set GAMMA to 0.3. */
#define GAMMA4_P 0b10000000000000000000000000L             /* Set GAMMA to 0.4. */
#define GAMMA5_P 0b10000001000000000000000000L             /* Set GAMMA to 0.5. */
#define GAMMA6_P 0b10000010000000000000000000L             /* Set GAMMA to 0.6. */
#define PRCARITY_P 0L                                      /* Set PRECEDENCE to arity. */
#define PRCOCCUR_P 0b100000000000000000000000000L          /* Set PRECEDENCE to occurrence. */
#define PRCFREQ_P 0b1000000000000000000000000000L          /* Set PRECEDENCE to frequency. */
#define PRCINVAR_P 0b1100000000000000000000000000L         /* Set PRECEDENCE to inverse arity. */
#define PRCINVOC_P 0b10000000000000000000000000000L        /* Set PRECEDENCE to inverse occurrence. */
#define PRCINVFR_P 0b10100000000000000000000000000L        /* Set PRECEDENCE to inverse frequency. */
#define ISPRCLOW_P 0L                                      /* Set introduced symbol precedence low. */
#define ISPRCHIGH_P 0b100000000000000000000000000000L      /* Set introduced symbol precedence high. */
#define WAR12_P 0L                                         /* Set weight:age ratio to 1:2 */
#define WAR11_P 0b1000000000000000000000000000000L         /* Set weight:age ratio to 1:1 */
#define WAR21_P 0b10000000000000000000000000000000L        /* Set weight:age ratio to 2:1 */
#define WAR31_P 0b11000000000000000000000000000000L        /* Set weight:age ratio to 3:1 */
#define WAR41_P 0b100000000000000000000000000000000L       /* Set weight:age ratio to 4:1 */
#define WAR51_P 0b101000000000000000000000000000000L       /* Set weight:age ratio to 5:1 */
#define WAR61_P 0b110000000000000000000000000000000L       /* Set weight:age ratio to 6:1 */
#define SPLITOFF_P 0L                                      /* Disable SPLIT. */
#define SPLITON_P 0b1000000000000000000000000000000000L    /* Enable SPLIT. */
#define GOALXOFF_P 0                                       /* Place holder for future enabling goal transformation for UEQ problems. */
#define GOALXON_P 0b100000000000000000000000000000000000L  /* Disable goal transformation for UEQ problems. */

/* Definitions to be used if VERIFYSATSOLVER is not zero. They are always defined anyway. */
#define SATOK 0                         /* SAT solver status is OK. */
#define SATSAT 1                        /* SAT solver has reported a refutation but all SAT clauses are satisfied. */
#define SATUNSAT 2                      /* SAT solver has reported a model but not all SAT clauses are satisfied. */
#define SATUNPR1 4                      /* There are unprocessed clauses that must be locked. */
#define SATUNPR2 8                      /* There are clauses in unprocessed queue not flagged as unprocessed. */
#define SATUNPR12 16                    /* Conditions SATUNPR1 and SATUNPR2. */
#define SATPASSIVE1 32                  /* There are passive clauses that must be locked. */
#define SATPASSIVE2 64                  /* There are clauses in passive queue not flagged as passive. */
#define SATPASSIVE12 128                /* Conditions SATPASSIVE1 and SATPASSIVE2. */
#define SATACTIVE1 256                  /* There are active clauses that must be locked. */
#define SATACTIVE2 512                  /* There are clauses in active queue not flagged as active. */
#define SATACTIVE12 1024                /* Conditions SATACTIVE1 and SATACTIVE2. */
#define SATLOCKED1 2048                 /* There are active clauses that must be locked. */
#define SATLOCKED2 4096                 /* There are clauses in active queue not flagged as active. */
#define SATLOCKED12 8192                /* Conditions SATLOCKED1 and SATLOCKED2. */
#define SATUNK 16384                    /* SAT solver status is unknown because the problem was not reported as solved. */

/* Definitions related to clause selections. */
#define VARWEIGHT 10                    /* Relative weight of variable symbols for clause weight evaluation. */
#define SYMBOLWEIGHT 20                 /* Relative weight of predicate and function symbols for clause weight evaluation. */
#define SINESEGMENT1 0                  /* End of first segment for SInE selection layer. */
#define SINESEGMENT2 1                  /* End of second segment for SInE selection layer. */
#define AVSEGMENT1 0                    /* End of first segment for Avatar selection layer. */
#define HRNSEGMENT1 0                   /* End of first segment for horn selection layer. */
#define SIAVLYR 0                       /* Specialized layers are SInE and Avatar. This is a possible value of layer */
                                        /* and layerset[0] and layerset[1] fields of options structure. */
#define SIHRNLYR 1                      /* Specialized layers are SInE and Horn. This is a possible value of layer */
                                        /* and layerset[0] and layerset[1] fields of options structure. */
#define AVHRNLYR 2                      /* Specialized layers are Avatar and Horn. This is a possible value of layer, */
                                        /* layerset[0] and layerset[1] fields of options structure. */
#define SINELYR 3                       /* Specialized layer is SInE. This is a possible value of layer, */
                                        /* layerset[0] and layerset[1] fields of options structure. */
#define AVLYR 4                         /* Specialized layer is Avatar. This is a possible value of layer, */
                                        /* layerset[0] and layerset[1] fields of options structure. */
#define HRNLYR 5                        /* Specialized layer is Horn. This is a possible value of layer, */
                                        /* layerset[0] and layerset[1] fields of options structure. */
#define NONELYR 6                       /* No specialized layer. This is a possible value of layer, */
                                        /* layerset[0] and layerset[1] fields of options structure. */

/* Definitions for the automatic learning process. */
#define VECTORLENGTH 78888              /* Number of elements in a clause vector used for learning. */
#define PENALTYPARAMETER 0.1            /* Penalty parameter for learning process. */
#define TOLERANCE1 2.0                  /* Duality gap tolerance used while good and bad clauses accuracy */
                                        /* is unbalanced in dual DCDescentMethod() function. */
#define TOLERANCE2 0.5                  /* Duality gap tolerance used when good and bad clauses accuracy */
                                        /* is balanced in dual DCDescentMethod() function. */
#define MAXRECSITER 0x7fffffff          /* Maximum value of number of iterations times number of records */
                                        /* in dual DCDescentMethod() function. This value should be set */
                                        /* to a value of 84000000 or greater. */
#define LEARNTIMEOUT 90                 /* Timeout in seconds for main loop in DCDescentMethod(). */
#define BADCLAUSEWEIGHT 100             /* Weight penalty if the clause is not classified as good by the learning algorithm. */
#define GOODCLAUSEWEIGHT 1              /* Weight penalty if the clause is classified as good by the learning algorithm. */
#define GENERICSYMBOLS 16               /* Number of generic symbols for both predicate and functions. */
#define ACCBALANCE 0.95                 /* Good/bad clauses accuracy balance threshold to accept a SVM result without */
                                        /* increasing the number of good clauses incorrectly classified. */
#define PROCLEARNMINMEM 400000000       /* Minimum memory in bytes to perform PROCLEARN process by LearnFromProblem() function. */
#define PROCLEARNMINMEM1 100000         /* Minimum memory in bytes to attempt duplicate some misclassified good clauses. */
#define LEARNMEMORYCHUNK 100            /* Number of items to be (re)allocated each time to fvidx global memory. */
#define MAXLEARNRECORDS 912             /* Maximum number of records per problem type. This is the maximum value of numlrnrecs */
                                        /* global variable. */
                                        /* IMPORTANT: This must be adjusted if the number of possible strategy parameters for */
                                        /* ALGORITHM, SELECT, TERMORDER, LITORDER, FUNCTIONW, WEAKRW or SPLIT options changes. */
#define OFFSET1 77778                   /* Offset of maximum symbol depth feature. */
#define OFFSET2 22032                   /* Offset of horizontal features with arity greater than 3. */
#define OFFSET3 22041                   /* Offset of equality horizontal feature. */
#define OFFSET4 22212                   /* Offset of horizontal features with arity 1. */
#define OFFSET5 22374                   /* Offset of horizontal features with arity 2. */
#define OFFSET6 25290                   /* Offset of horizontal features with arity 3. */
#define OFFSET7 78814                   /* Symbol count offset. */
#define OFFSET8 78884                   /* Clause length and literal count offset. */

/* Prefix and postfix definitions for introduced names. */
#define SKLPREFIX "sK"                  /* Skolems prefix. */
#define PRDPREFIX "sP"                  /* Block renaming predicate definitions prefix. */
#define SPLPREFIX "sQ"                  /* Formula splitting predicate definitions prefix. */
#define FNCPREFIX "sF"                  /* Prefix for introduced function definitions. */
#define SKLPOSTFIX "_skl"               /* Skolems postfix. */
#define PRDPOSTFIX "_prd"               /* Block renaming predicate definitions postfix. */
#define SPLPOSTFIX "_spl"               /* Formula splitting predicate definitions postfix. */
#define FNCPOSTFIX "_fnc"               /* Postfix for introduced function definitions. */

/* Miscellanea definitions. */
#define VERSION "Drodi V4.1.1"          /* Program version. */
#define SPREAD 0                        /* Use spread core affinity bind option. */
#define CLOSE 1                         /* Use close core affinity bind option. */
#define LRSTHRESHOLD 200                /* Number of passive clauses moved to active per LRS cycle. */
#define LRSCOEF 1.1                     /* Coefficient to apply to number of LRS reachable clauses. */
#define TIMEOUTCCLDSP 10                /* Number of cycles for timeout check in clause disposal. */
#define INPUT_CHUNK_SIZE 1024           /* Chunk size in bytes for command input. */
#define VAR_CHUNK_SIZE 100              /* Chunk size in bytes for text formula names. */
#define STR_CHUNK_SIZE 200              /* Chunk size in bytes for text buffers. */
                                        /* This must be bigger or equal to 10. */
#define TREEPTR_CHUNK_SIZE 4            /* Pointer set chunk size in tree pointer elements. */
#define SUBST_CHUNK_SIZE 1000           /* Chunk size in bytes for substitution buffer. */
#define CLAUSE_CHUNK_SIZE 200           /* Compiled auxiliary clauses chunk size in bytes. */
#define PNDARGS_CHUNK_SIZE 10           /* Pending arguments elements chunk size in number of elements. */
#define VARRENUM_CHUNK_SIZE 50          /* Variable renumbering buffer size in number of elements. */
#define VARBALANCE_CHUNK_SIZE 100       /* Initial number of elements in vb set of wbparam structure. */
#define ABSMAXWEIGHT 0xffffffff         /* Absolute maximum clause weight. This is to prevent exhausting the stack space by */
                                        /* too many calls to QueryGeneric() for too long clauses. It is rare but it may happen. */
#define MAXWEIGHT 5000                  /* Maximum number of weight queue pointers hold in each weight queue. */
#define SELECTQUEUES 6                  /* Number of clause selection queues. */
#define MAXPOSITIONS 400                /* Maximum number of hash values for item positions. */
#define TUNEDSTRATEGIES 540             /* Maximum number of tuned strategies for a problem type. */
#define ALLSTRATEGIES 577               /* Maximum number of tuned strategies plus tuned complete strategies (duplicated) */
                                        /* for a problem type. */
#define PBMTYPES 9                      /* Number of problem types. Maximum value is 255 due to limitations of the header field */
                                        /* in learnrecord structure. */
#ifdef UNITCLRESOLUTION
#define DSCTREETYPES 8                  /* Number of discr[] set field elements in hashchain structure. */
#else
#define DSCTREETYPES 6                  /* Number of discr[] set field elements in hashchain structure. */
#endif
#define HASHVALUES 0x10000              /* Number of different hash values for symbol names and clause variants. */
#define MAXMINISCOPETIME 1.0            /* Miniscope time limit in seconds. */
#define EXITWAITSECONDS 1               /* Number of seconds to wait for exit of a non solution winner children process. */
                                        /* Child exit is indicated by clearing procsem[] process specific semaphore. */
#define KILLWAITITERATIONS 20           /* Number of iterations to wait for state changes in child processes via waitpid(). */
#define KILLWAITNANOSECS 10000000       /* Number of nanoseconds to wait between iterations for state changes in child */
                                        /* processes via waitpid(). This must be less than 1000000000. */
#define KILLWAITNANOSECS2 100000000     /* Number of nanoseconds to wait before waiting for state changes in child processes */
                                        /* via waitpid() if an interruption occurs. This must be less than 1000000000. */
#define TMOWAITMICROSECS 500000         /* Additional time in microseconds to wait for timeouts not caught by normal means. */
                                        /* This must be less than 1000000. */
#define MAXUNPROCSHUFFLING 100          /* Maximum number of leading or trailing unprocessed clauses to shuffle. */
#define MEMORYLIMIT 5000000             /* Minimum current available memory in bytes for Saturate() to start processing. */
#define MEMORYLIMIT2 16275932000        /* Parameter used to estimate available memory. See GetAvailableMemory() function */
                                        /* for additional information. */
#define MEMORYMARGIN 1700000000         /* Parameter used to estimate available memory. See GetAvailableMemory() function */
                                        /* for additional information. */
#define MEMUSAGE1 0.95                  /* Portion of available memory that the program may use if /proc/meminfo has */
                                        /* MemAvailable information. */
#define MEMUSAGE2 0.75                  /* Portion of physical memory that the program may use if /proc/meminfo has not */
                                        /* MemAvailable information. */
#define MEMORYLIMIT1 600000000          /* Minimum memory in bytes for the program to work in normal mode. */
#define SUBSUMINSTRLIMIT 73874305       /* Maximum number of instructions allowed in each check of clause subsumptions. */
                                        /* See IsSubsumedByClause() function global comments for additional information. */
#define SATQMAXTIME 2                   /* Maximum seconds of execution of SatQ2SatSolver() function when type parameter is not zero. */
#define SATQITERS 50                    /* Number of iterations between time checks in SatQ2SatSolver() function. */
#define CPLINSTRLIMIT 75000000000       /* This must be about twice or so the limit of hardware instructions used in strategy portfolio tuning. */
#define SATSTRATTIMEOUT 30.0            /* Timeout in seconds for satisfiability strategies time slices. */
#define DFLTSTRATTIMEOUT 45.0           /* Default timeout in seconds for random strategy time slices. */
#define HWINSTRATE 1774758056           /* Estimated number of hardware executed instructions per second. See myioctl() function */
                                        /* global comments in multith.c module for additional information. Use 1774758056 for */
                                        /* Miami StarExec and 2258169326 for Iowa allq and long StarExec queues. */
#define CHKCYCLES 20                    /* Number of cycles before checking number of executed hardware instructions. */
#define SATCORES 0.25                   /* Portion of cores that will use complete strategies only if SATMODE is set to MED. */
#define MAXRECURSIVECALLS 30            /* Maximum nested calls to DefinitionFolding() function, */
#define MINCORES4BESTSTRAT 8            /* Minimum number of active cores to reserve one process for the best portfolio strategy. */
#ifdef WINTYPE
#define WPATHSEPARATOR L'\\'            /* Windows type wide char file path separator. */
#else
#define WPATHSEPARATOR L'/'             /* Linux & AIX type wide char file path separator. */
#endif

/* Useful macros */
#ifdef DEBUGMEMORY
#define MYFREE(X) myfree(X,__FILE__,__LINE__)
#define DBGMEMELEMS 2000
#define DBGMEMELEMS 0
#else
#if (MEMCHECK == 1) || (MEMCHECK == 2)
#define MYFREE(X) freemn(X)
#else
#define MYFREE(X) free(X)
#endif
#endif
#if (MEMCHECK == 1) || (MEMCHECK == 2)
#define MYALLOC(X) mallocmn(X)
#define MYREALLOC(X,Y) reallocmn(X,Y)
#define MYSTRDUP(X) strdupmn(X)
#else
#define MYALLOC(X) malloc(X)
#define MYREALLOC(X,Y) realloc(X,Y)
#define MYSTRDUP(X) strdup(X)
#endif
#define MAX(X,Y) ((X)>=(Y)?X:Y)
#define MIN(X,Y) ((X)<=(Y)?X:Y)
#define NUMELEMS(X) sizeof(X)/sizeof(X[0])
#define MATCHFLGS11(X,Y) (((X)&ITEM1SEC)==((Y)&ITEM1SEC))
#define MATCHFLGS12(X,Y) (((X)&ITEM1SEC)==(((Y)&ITEM2SEC)>>1))
#ifdef MURMUR3
#define ROT32(x, y) ((x << y) | (x >> (32 - y)))
#endif

/* STRUCTURES */
/* Structure for chaining names buffers for told sentences. */
typedef struct snameschain {
	struct snameschain *next;               /* Pointer to first nameschain structure of next older level of names. */
	struct snameschain *peer;               /* Pointer to next nameschain structure of the same level. The new names */
	                                        /* at a level are always added at the end of peers chain. */
	struct snameschain *frstpeer;           /* Pointer to first nameschain structure of the same level. */
	struct snameschain *lstpeer;            /* Pointer to last nameschain structure of the same level. Only used for */
	                                        /* first nameschain structure of each level. */
	struct slkedfitem *quantifier;          /* Pointer to quantifier linked list item corresponding to a variable item. */
	char *name;                             /* Pointer to name of predicate, function or variable. */
	int32_t flags;                          /* PREDICATE for predicates, FUNCTION for functions or 0 for variables. */
	union {
		int32_t arity;                      /* Arity of predicate or function. */
		int32_t varnum;                     /* Number assigned to variable or -1 if variable not yet standardized. */
	} data;
	//struct shaschain *type;                 /* Pointer to type or sort assigned to predicate, function or variable. */
} nameschain;

/* Structure for formula common prefix. */
typedef struct scmprefix {
	struct scmprefix *next;             /* Pointer to next formula for formulas of the same kind (TEXT, ACTIVE, PASSIVE, */
	                                    /* UNPROC y GROUNDJOINABLE) in the KB. NULL if last formula of its kind. */
	struct scmprefix *prev;             /* Pointer to previous formula for formulas of the same kind (TEXT, ACTIVE, PASSIVE, */
	                                    /* UNPROC y GROUNDJOINABLE) in the KB. NULL if first formula of its kind. */
	                                    /* This is intended to free formula memory. */
	struct scmprefix *parent1;          /* If ENABLECHOICEAXIOM is defined: */
	                                    /* First parent formula pointer except for axioms of choice for which parent1 */
		                                /* points to the set of the skolemization skldata structure pointers, */
	                                    /* one pointer for each skolemized variable. Then end of the set is marked */
	                                    /* by a NULL pointer. This is necessary to print the CASC required information */
	                                    /* for skolemization inferences in proofs. */
		                                /* For SKOLEMIZATION inferences this points to the axiom of choice. */
                                        /* If ENABLECHOICEAXIOM is NOT defined: */
                                        /* First parent formula pointer except for skolemizations for which parent1 */
                                        /* is reported as the second parent. */
	struct scmprefix *parent2;          /* Second parent formula pointer, NULL if there is no second parent. */
	                                    /* For predicate definition formulas this pointer is used to chain */
	                                    /* the set of predicate definition formulas used in a single formula */
	                                    /* renaming inference. */
	                                    /* If ENABLECHOICEAXIOM is defined: */
                                        /* For skolemization formulas this points to the skolemized formula */
                                        /* which is the real parent of the skolemizations. This is a requirement */
                                        /* for CASC proof checking. */
	                                    /* If ENABLECHOICEAXIOM is NOT defined: */
	                                    /* For skolemization formulas this points to a set of skldata structure */
	                                    /* pointers, one pointer for each skolemized variable. Then end of the set */
	                                    /* is marked by a NULL pointer. This is necessary to print the CASC required */
	                                    /* information for skolemization inferences in proofs. */
	struct scmprefix *prevdisp;         /* Pointer to previous formula in disposal queue. NULL if last formula */
	                                    /* or formula not in disposal queue. This queue interconnects clauses */
	                                    /* from other queues.*/
	union {
		char *text;                     /* Pointer to formula in plain text form. Used for text type formulas. */
		struct sbinprefix *bin;         /* Pointer to binary clause prefix structure. Used for compiled clauses. */
	} part2;
	uint64_t number;                    /* Number assigned to formula. */
	int32_t inference;                  /* Inference from where the formula comes from (see definitions above). */
	uint32_t flags;                     /* See flags above. This must be the last field of this structure. */
} cmprefix;

/* Structure for binary clause prefix. */
typedef struct sbinprefix {
	cmprefix *clause;                     /* Pointer to cmprefix structure of the formula itself. */
	cmprefix *nextselect[2*SELECTQUEUES]; /* Set of pointers to next formula in each clause selection queue. NULL if last formula. */
	cmprefix *prevselect[2*SELECTQUEUES]; /* Set of pointers to previous formula in each clause selection queue. NULL if last formula. */
	union {
		uint8_t *asserts;                 /* Pointer to buffer with clause assertions. */
		uint8_t *cptopterm;               /* Pointer to buffer critical pair top term. */
	} ovly;
	uint8_t *lockasserts;                 /* Pointer to buffer with clause locking assertions. */
	uint64_t signature;                   /* Signature of last inference process. */
	uint64_t locksignature;               /* Signature for lock and unlock processes. */
	uint32_t literals;                    /* Number of literals in clause. */
	uint32_t clweight;                    /* Clause weight. */
	int32_t size;                         /* Size in bytes of formula including ending UNITEND byte. */
	int32_t asize;                        /* Size in bytes of assertions including ending UNITEND byte. */
	int32_t lasize;                       /* Size in bytes of locking assertions assertions including ending UNITEND byte. */
	int32_t maxvarnb;                     /* Maximum variable number, -1 if no variables. This is useful for */
	                                      /* memory allocations in ordering functions. */
	int32_t oriented;                     /* 0 if selected (in)equalities in clause have not been oriented. */
	                                      /* 1 if SELECTED (in)equalities in SELECTED equalities have been oriented. */
	uint32_t sinedist;                    /* SInE distance of the clause to the conjecture for clause selection. */
	uint32_t agedist;                     /* Age distance for clause selection. */
	int32_t avdist;                       /* Avatar distance for clause selection. */
	int32_t hrndist;                      /* Horn distance for clause selection. */
	uint8_t formula[1];                   /* Dummy for start of formula items. This must be the last field of this structure. */
} binprefix;

#ifdef SEMANTICTAUTOLOGY
/* Structure for binary tautology clause. */
typedef struct sttlgclause {
	struct sttlgclause *next;           /* Pointer to next clause for (in)equalities of the same kind (positive or negative), */
	                                    /* NULL if last clause of its type. */
	struct sttlgclause *prev;           /* Pointer to previous clause for (in)equalities of the same kind (positive or negative), */
                                        /* NULL if first clause of its type. */
	uint8_t formula[1];                 /* Dummy for start of formula items. This must be the last field of this structure. */
} ttlgclause;
#endif

/* Structure for SAT clauses. */
typedef struct ssatclause {
	struct ssatclause *glblnext;        /* Pointer to next SAT clause of the same global queue, NULL if last. */
	struct ssatclause *glblprev;        /* Pointer to previous SAT clause of the same global queue, NULL if last. */
	struct ssatclause *next;            /* Pointer to next SAT clause of the SAT solver ordered clauses queue, NULL if last. */
	struct ssatclause *prev;            /* Pointer to previous SAT clause of the SAT solver ordered clauses queue, NULL if first. */
	struct scmprefix *parent;           /* Pointer to parent A-clause. */
	uint64_t number;                    /* Number assigned to the SAT clause. */
	int32_t inference;                  /* Inference from which the SAT clause was obtained. It can be SPLIT or CONTRADICTION. */
	                                    /* See definitions above. */
	int32_t literals;                   /* Number of literals in clause. */
	int32_t satlits;                    /* Number of satisfied literals in clause. */
	int32_t undeflits;                  /* Number of undefined literals in clause. */
	int32_t inproof;                    /* Set to 1 if clause is in the unsatisfiable core but is not the contradiction clause */
	                                    /* that caused the SAT refutation, set to 2 if clause is in the unsatisfiable core and */
	                                    /* it is the contradiction clause that caused the SAT refutation, zero otherwise. */
	uint8_t formula[1];                 /* Dummy for start of formula items. This must be the last field of this structure. */
} satclause;

/* Structure for statistic information. */
/* If new fields are added to this structure the following changes must be made */
/* to the involved functions: */
/* - Search for an existing field and add the appropriate equivalent code */
/*   for the new field. */
/* - Update totals calculation in PrintStatData() function if appropriate. */
/* - Add code for printing proof lines in PrintProof() function if appropriate. */
typedef struct sstats {
	uint64_t tautologies;               /* Number of syntactic tautologies detected. */
	#ifdef SEMANTICTAUTOLOGY
	uint64_t semtautologies;            /* Number of semantic tautologies detected. */
	#endif
	uint64_t fwdsubsum;                 /* Number of forward subsumptions performed. */
	uint64_t bcksubsum;                 /* Number of backward subsumptions performed. */
	uint64_t factors;                   /* Number of factoring inferences. */
	uint64_t removedups;                /* Number of duplicate literal removing simplifications. */
	uint64_t fwdemodulations;           /* Number of forward demodulation inferences. */
	uint64_t bkdemodulations;           /* Number of backward demodulation inferences. */
	uint64_t fwsubsresltns;             /* Number of forward subsumtion resolution inferences. */
	uint64_t bksubsresltns;             /* Number of forward subsumtion resolution inferences. */
	uint64_t triveqres;                 /* Number of trivial equality resolution inferences. */
	uint64_t desteqres;                 /* Number of destructive equality resolution inferences. */
	uint64_t resolutions;               /* Number of resolution inferences. */
	uint64_t paramodulations;           /* Number of paramodulation inferences. */
	uint64_t equresolutions;            /* Number of equality resolution inferences. */
	uint64_t equfactorings;             /* Number of equality factoring inferences. */
	uint64_t splits;                    /* Number of split inferences. */
	uint64_t eqsplits;                  /* Number of equality splits inferences. */
	uint64_t prennfxform;               /* Number of pre-nnf inferences. */
	uint64_t nnfxform;                  /* Number of nnf inferences. */
	uint64_t miniscoping;               /* Number of miniscoping inferences. */
	uint64_t skolemize;                 /* Number of skolemization inferences. */
	uint64_t predicatedef;              /* Number of predicate definitions for sub-formula renaming. */
	uint64_t formularenaming;           /* Number of formula renaming inferences. */
	uint64_t cnfconversion;             /* Number of CNF conversion inferences. */
	uint64_t tfsimplif;                 /* Number of $true and $false simplifications. */
	uint64_t sattautologies;            /* Number of tautologies detected in SAT clauses. */
	uint64_t satremovedups;             /* Number of duplicate literal removing simplifications in SAT clauses. */
	uint64_t satsubsum;                 /* Number of subsumptions performed for SAT clauses. */
	uint64_t nbactive;                  /* Number of active clauses. */
	uint64_t nbpassive;                 /* Number of passive clauses. */
	uint64_t nbunproc;                  /* Number of unprocessed clauses. */
	uint64_t nbgrjnbl;                  /* Number of ground joinable clauses. */
	uint64_t memory;                    /* Memory used including non returnable unused space assigned to the program. */
	uint64_t netmemory;                 /* Memory used excluding non returnable unused space assigned to the program. */
	double elapsed_time;                /* Elapsed time in seconds. */
} stats;

/* Structure for program options. */
typedef struct soptions {
	int32_t fweight;                    /* Type of function weight (UNIFORM or ARITY). */
	int32_t termord;                    /* Terms ordering type (STANDARD or NONRECURSIVE). */
	int32_t litord;                     /* Literals ordering type (STANDARD, NONRECURSIVE or LEXICOGRAPHIC). */
	int32_t select;                     /* Type of literal selection (see defines above). */
	int32_t demodulation;               /* Demodulation options (DEMODON, DEMODOFF, COMPLETE). */
	int32_t factoring;                  /* Factoring is enabled if this is not zero. */
	int32_t algorithm;                  /* Selected algorithm (OTTER or DISCOUNT). */
	int32_t mxwghtopt;                  /* MXWEIGHTxxx (see definitions above). */
	uint32_t maxweight;                 /* Maximum weight allowed. */
	int32_t layer;                      /* Type of specialized layers in use by current strategy, see defines above. */
	int32_t layerset[2];                /* Types of specialized layers allowed for current problem, see defines above. */
	int32_t lookahead;                  /* Use lookahead to modify weight queues for clause clause selection. */
	int32_t split;                      /* Splitting and interface between first order prover and */
	                                    /* SAT solver is enabled if this is not zero. Otherwise split is disabled */
	                                    /* or takes the default value depending on the value of usrparam_mask. */
	int32_t shuffle;                    /* If not zero then include strategies with shuffling and randomization. */
	int32_t grjoin;                     /* Ground joinability setting when UEQDSC saturation algorithm is used. */
	                                    /* 0 -> OFF, 1 -> ON, 2 -> MED. */
	int32_t connect;                    /* Connectedness setting when UEQDSC saturation algorithm is used, 0 -> OFF, 1 -> ON. */
	int32_t learn;                      /* This is 1 if use of learning is enabled in the current strategy or 1 otherwise. */
	int32_t weakrw;                     /* Weak rewrite setting for UEQ problems, 0 -> OFF, 1 -> ON. */
	int32_t goalxform;                  /* Goal transformation for UEQ problems, 0 -> OFF, 1 -> ON. */
	int32_t satmode;                    /* Problem satisfiability search mode: 0 -> OFF, 1 -> ON, 2 -> MED. */
	float timeout;                      /* Timeout setting. */
	float gamma;                        /* Factor that multiplies a non learning weight for learning weight calculation. */
	int32_t precedence;                 /* Symbol precedence settings (see defines above). */
	int32_t prprcsymprec;               /* Precedence of symbols added during pre-processing (see defines above). */
} options;

/* Structure for hash separate symbol chaining. */
typedef struct shashchain {
	struct shashchain *nextelem;        /* Pointer to next hashchain element or NULL if last. This pointer is */
	                                    /* used for freeing memory allocated to hashchain elements. */
	struct shashchain *nextsymbol;      /* Pointer to next hashchain element of the same hash value */
	                                    /* or NULL if last. */
	void *discr[DSCTREETYPES];          /* Set of pointers to discrimination trees. See EditDiscrTrees() */
	                                    /* global function comments for an index description. */
	#ifdef SEMANTICTAUTOLOGY
	void *posdsctrttlg;                 /* Pointer to semantic tautology discrimination tree for functions and positive predicates */
	                                    /* and equalities. */
	void *negdsctrttlg;                 /* Pointer to semantic tautology discrimination tree for negative predicates equalities. */
	#endif
	#ifdef DEBUGTREE
	uint32_t numleafs[DSCTREETYPES];    /* Number of leafs of each discrimination tree. */
	#endif
	int32_t arity;                      /* Symbol arity. */
	int32_t type;                       /* Type flags, see flag definitions above. */
	uint64_t precedence;                /* Symbol precedence value for ordering purposes. */
	uint64_t occurrence;                /* Order of occurrence of symbol. This order of occurrence is adjusted independently for normal */
	                                    /* symbols for one part and for introduced symbols (skolems and defined predicates) for other. */
	uint64_t occurrencebk;              /* Copy of occurrence field for restoring purposes. */
	int32_t usecount;                   /* Number of times the symbol was used in the initial clausification of a problem. This is used for */
	                                    /* SInE distance calculation and for for symbol ordering precedence. */
	uint32_t prednumber;                /* Number assigned to symbols that are predicates. Used for subsumption pruning. */
	uint32_t sinedist;                  /* SInE distance of the symbol to the conjecture. */
	uint32_t hash;                      /* Symbol 32 bits hash value corresponding to symbol name. */
} hashchain;

/* Structure for hash component symbol chaining. */
typedef struct shashchcmp {
	struct shashchcmp *nextelem;        /* Pointer to next hashchcmp element or NULL if last. This pointer is */
	                                    /* used for freeing memory allocated to hashchcmp elements. */
	struct shashchcmp *nextsymbol;      /* Pointer to next hashchcmp element of the same hash value */
	                                    /* or NULL if last. */
	struct shashchcmp *nextundef;       /* Pointer to next undefined hashchcmp element of the same component. */
	struct shashchcmp *prevundef;       /* Pointer to previous undefined hashchcmp element of the same component. */
	struct sasymbol *frstposassrt;      /* Pointer to first positive asymbol in assertions of first order prover. */
	struct sasymbol *frstnegassrt;      /* Pointer to first negative asymbol in assertions of first order prover. */
	struct sasymbol *firstlock;         /* Pointer to first asymbol in locks of first order prover. */
	struct sasymbol *frstsat;           /* Pointer to first asymbol in SAT solver. */
	struct ssatnode *satnode;           /* Pointer to satnode that assigned a value to this component. */
	uint8_t *formula;                   /* Pointer to clause binary formula of component clause associated */
	                                    /* with the P-predicate. */
	int32_t size;                       /* Size in bytes of formula including ending UNITEND byte. */
	int32_t literals;                   /* Number of literals in formula. */
	int32_t number;                     /* Number assigned to component symbol name, the xxx in sQxxx_spl name. */
	uint64_t defnumber;                 /* Number assigned to the symbol definition in the proof. The component */
	                                    /* clause is assigned the number defnumber+1. */
	uint32_t hash;                      /* Symbol 32 bits hash value corresponding to clause component. */
	uint32_t flags;                     /* Flags for this component, see definitions above. */
	uint32_t npositive;                 /* Number of clauses in SAT solver with a positive literal for this component. */
	uint32_t nnegative;                 /* Number of clauses in SAT solver with a negative literal for this component. */
	uint32_t absdif;                    /* Absolute value of npositive field minus nnegative field. */
	uint32_t sinedist;                  /* SInE distance of the clause that originated the component to the conjecture. */
	int32_t agedist;                    /* Age distance of the clause that originated the component to the conjecture. */
} hashchcmp;

/* Structure for weight and balance ordering polynomials. */
typedef struct swbparam {
	int32_t poscnt;                     /* Positive variable balance counter. */
	int32_t negcnt;                     /* Negative variable balance counter. */
	int32_t weight;                     /* Total weight. */
	int32_t *vb;                        /* Pointer to set with variable balances per variable number. */
	int32_t size;                       /* Number of elements in vb set. */
} wbparam;

/* Structure for symbols (literals and functions) items data. */
typedef struct ssymbol {
	int32_t offset;                     /* Offset from beginning of formula field buffer to start of current item. */
	int32_t nextoff;                    /* Offset from beginning of formula field buffer to next item */
	                                    /* that is not a subterm of current item. */
	hashchain *symbol;                  /* Pointer to hashchain structure for the item symbol. */
} symbol;

/* Structure for assertion symbols (P-predicates, component names). */
typedef struct sasymbol {
	hashchcmp *symbol;                  /* Pointer to hashchain structure for the item symbol. */
	struct sasymbol *next;              /* Pointer to next asymbol structure for the same component of the same class. */
	                                    /* (assertion, lock or SAT clause literal). */
	struct sasymbol *prev;              /* Pointer to previous asymbol structure for the same component of the same class. */
	union {
		binprefix *binp;                /* Pointer to A-clause binary formula to which the asymbol belongs if asymbol */
		                                /* belongs to assertions or lock assertions of an A-clause. */
		satclause *satcl;               /* Pointer to SAT clause binary formula to which the asymbol belongs if asymbol */
		                                /* belongs to a SAT clause. */
	} part2;
} asymbol;

/* Discrimination tree node structure. */
typedef struct sdstrnode {
	struct sdstrnode *parent;           /* Pointer to parent node, NULL if first level node. The discr[] field of the */
	                                    /* symbols hashchain structure points to a first level node or a leaf. */
	struct sdstrnode *nxtpeer;          /* Pointer to next peer node. */
	struct sdstrnode *prevpeer;         /* Pointer to previous peer node. */
	union {
		struct sdstrnode *childnode;    /* Pointer to first child node. */
		struct sdstrleaf *childleaf;    /* Pointer to first child leaf. */
	} child;
	hashchain *symbol;                  /* Pointer to node symbol for nodes that are not for variables. Not used */
	                                    /* for nodes that are variables. */
	uint8_t *item;                      /* This is used for temporary storage. DsTermIndexing() function uses it */
	                                    /* to store the pointer to the next clause term that is not a sub-term */
	                                    /* of the clause item matching the node if it has sub-terms or NULL otherwise. */
	                                    /* This is used later to update pndbrkts field. QueryDscTree() function uses it */
	                                    /* to store the pointer to clause item that matches the node or NULL if this is */
	                                    /* a sub-term of a node item that matches a variable. This is used later for */
	                                    /* backtracking. See DsTermIndexing() */
	                                    /* global comments. */
	uint16_t pndbrkts;                  /* Number of brackets to be closed just after the node symbol, see DsTermIndexing() */
	                                    /* global comments. */
	uint8_t nodetype;                   /* VARIABLE if node is for a variable, zero otherwise. */
	int8_t childtype;                   /* Type of child, it can be NODECHILD or LEAFCHILD (see definitions above). */
} dstrnode;

/* Discrimination tree leaf structure. */
typedef struct sdstrleaf {
	struct sdstrnode *parent;           /* Pointer to parent node. */
	struct sdstrleaf *peer;             /* Pointer to next peer (same parent) leaf. */
	uint8_t *item;                      /* Pointer to the tree indexed clause item. */
} dstrleaf;

#ifdef SEMANTICTAUTOLOGY
/* Discrimination tree node structure. */
typedef struct sttlgdstrnode {
	struct sttlgdstrnode *parent;        /* Pointer to parent node, NULL if first level node. The discr[] field of the */
	                                     /* symbols hashchain structure points to a first level node or a leaf. */
	struct sttlgdstrnode *nxtpeer;       /* Pointer to next peer node. */
	struct sttlgdstrnode *prevpeer;      /* Pointer to previous peer node. */
	union {
		struct sttlgdstrnode *childnode; /* Pointer to first child node. */
		struct sttlgdstrleaf *childleaf; /* Pointer to first child leaf. */
	} child;
	union {
		hashchain *symbol;               /* Pointer to node symbol for nodes that are not for variables. Not used */
		                                 /* for nodes that are variables. */
		uint16_t varnb;                  /* Variable number. */
	} itemid;
	uint8_t nodetype;                    /* VARIABLE if node is for a variable, zero otherwise. */
	int8_t childtype;                    /* Type of child, it can be NODECHILD or LEAFCHILD (see definitions above). */
} ttlgdstrnode;

/* Discrimination tree leaf structure. */
typedef struct sttlgdstrleaf {
	struct sttlgdstrnode *parent;        /* Pointer to parent node. */
	struct sttlgdstrleaf *peer;          /* Pointer to next peer (same parent) leaf. */
	uint8_t *item;                       /* Pointer to the tree indexed item. */
	uint8_t *formula;                    /* Pointer to the formula containing the tree indexed item. */
} ttlgdstrleaf;
#endif

/* Discrimination tree backtrack structure for QueryDscTree() function. */
typedef struct sdstrbktrk {
	dstrleaf *lstmatch;                 /* Pointer to last match found in a previous call. */
	uint8_t **treeitems;                /* Pointer to set of tree item pointers. */
	uint32_t nodeidx;                   /* Index of current node in treeitems field set. */
} dstrbktrk;

/* Structure for indexing of positive equalities selected for paramodulating from them */
/* in active clauses that have maximal root terms that are variables. */
typedef struct sdsendnode {
	int32_t size;                       /* Number of total symbol pointers allocated. */
	int32_t used;                       /* Number of used symbol pointers. */
	uint8_t **symbol;                   /* Pointer to set of pointers to item in the clause that matches the tree leave. */
} dsendnode;

/* Structure for SAT solver node. */
typedef struct ssatnode {
	struct ssatnode *nextnode;          /* Pointer to next SAT solver node, NULL if last.*/
	struct ssatnode *prevnode;          /* Pointer to previous SAT solver node, NULL if first.*/
	hashchcmp *symbol;                  /* SAT symbol that was assigned a value true or false in this node. */
	int32_t flags;                      /* Flags for this node: POSITIVE or NEGATIVE and possibly LASTOPTION. */
	satclause *forcedcl;                /* Pointer to unit clause that forced the value of the node symbol field, */
	                                    /* NULL if the node was not forced by a unit clause. */
	uint32_t number;                    /* Sequence number for this node. */
} satnode;

/* Structure for proof node. */
typedef struct sproofnode {
	struct sproofnode *nextnode;        /* Pointer to next proof node, NULL if last.*/
	struct sproofnode *prevnode;        /* Pointer to previous proof node, NULL if first.*/
	struct sproofnode *backptr;         /* This is used by AddAclause2Proof() for backtracking, allowing AddAclause2Proof() */
	                                    /* not to be recursive, because with long ancestors line recursiveness may cause */
	                                    /* stack overflow. */
	int32_t type;                       /* Type of node, see definitions above. */
	union {
		hashchcmp *symbol;              /* Pointer to component symbol. */
		satclause *satclause;           /* Pointer to SAT clause. */
		cmprefix *Aclause;              /* Pointer to A-clause. */
	} ptr;
} proofnode;

/* Structure for vertical features scan node. */
typedef struct svertnode {
	struct svertnode *nextnode;         /* Pointer to next node, NULL if last.*/
	struct svertnode *prevnode;         /* Pointer to previous node, NULL if first.*/
	uint8_t *item;                      /* Pointer to clause item. */
	uint8_t *nextitem;                  /* Next item jumping over subterms of current item. */
	int32_t depth;                      /* Node depth, starting with zero. */
} vertnode;

/* Sparse learning feature vector structure. See GetClauseVector() function for a description */
/* of each feature in vector. */
typedef struct slearnvector {
	int32_t size;                       /* Number of nun null elements in sparse vector. */
	int32_t *index;                     /* Pointer to set of non null vector component indices. */
	double *sparsevect;                 /* Pointer to non null vector components. */
} learnvector;

/* Structure for learning record. */
typedef struct slearnrecord {
	uint64_t header;                    /* Record header with flags indicating the relevant part of the strategy */
	                                    /* to which the learn record applies. The relevant strategy flags are those */
	                                    /* corresponding to ALGMASK, SELECTMASK, TERMORDRMASK, LITORDRMASK and */
	                                    /* FUNCWMASK masks .The most significant byte of the header contains the */
	                                    /* problem type, a value between 0 and PBMTYPES-1. */
	learnvector vector;                 /* Sparse learning feature vector structure. */
} learnrecord;

/* Structure for KB definition. */
typedef struct skbase {
	cmprefix *firsttxt;                 /* Pointer to first text formula, NULL if no text formulas added yet. */
	                                    /* Older formulas are placed first and this pointer points to the oldest */
	                                    /* text formula. */
	cmprefix *lasttxt;                  /* Pointer to last text formula, NULL if no text formulas added yet. */
                                        /* Newer formulas are placed last and this pointer points to the newest */
                                        /* text formula. */
	cmprefix *frstpassive;              /* Pointer to first passive formula, NULL if no passive formulas added yet. */
                                        /* Older formulas are placed first and this pointer points to the oldest */
                                        /* passive formula. */
	cmprefix *lstpassive;               /* Pointer to last passive formula, NULL if no passive formulas added yet. */
                                        /* Newer formulas are placed last and this pointer points to the newest */
                                        /* passive formula. */
	cmprefix *frstactive;               /* Pointer to first active formula, NULL if no active formulas added yet. */
                                        /* Older formulas are placed first and this pointer points to the oldest */
                                        /* active formula. */
	cmprefix *lstactive;                /* Pointer to last active formula, NULL if no active formulas added yet. */
                                        /* Newer formulas are placed last and this pointer points to the newest */
                                        /* active formula. */
	cmprefix *frstunproc;               /* Pointer to first unprocessed formula, NULL if no unprocessed formulas */
                                        /* added yet. Older formulas are placed first and this pointer points */
                                        /* to the oldest unprocessed formula. */
	cmprefix *lstunproc;                /* Pointer to last unprocessed formula, NULL if no unprocessed formulas */
                                        /* added yet. Newer formulas are placed last and this pointer points */
                                        /* to the newest unprocessed formula. */
	cmprefix *frstgrjoin;               /* Pointer to first ground joinable formula, NULL if no ground joinable */
                                        /* formulas added yet. Older formulas are placed first and this pointer */
                                        /* points to the oldest ground joinable formula. */
	cmprefix *lstgrjoin;                /* Pointer to last ground joinable formula, NULL if no ground joinable */
                                        /* formulas added yet. Newer formulas are placed last and this pointer */
                                        /* points to the newest ground joinable formula. */
	cmprefix *frstlocked;               /* Pointer to first locked formula, NULL if no locked formulas added yet. */
                                        /* Older formulas are placed first and this pointer points to the oldest */
                                        /* locked formula. */
	cmprefix *lstlocked;                /* Pointer to last locked formula, NULL if no locked formulas added yet. */
                                        /* Newer formulas are placed last and this pointer points to the newest */
                                        /* locked formula. */
	cmprefix *lastdisp;                 /* Pointer to last formula in disposal queue, NULL if no formulas */
	                                    /* added yet. This queue interconnects clauses from other queues. */
	cmprefix *negeq;                    /* Last negative equality added to UNPROC or PASSIVE clauses when saturation */
	                                    /* algorithm is UEQDscnt(). */
	proofnode *frstprfnode;             /* Pointer to first proof node. */
	satclause *firstsatq;               /* Pointer to first SAT clause in the SAT queue. */
	satclause *lastsatq;                /* Pointer to last SAT clause in the SAT queue. */
	satclause *frstsatglbl;             /* Pointer to first SAT clause in the SAT solver global queue. */
	satclause *lastsatglbl;             /* Pointer to last SAT clause in the SAT solver global queue. */
	satclause *frstunsats;              /* Pointer to first SAT clause in the SAT solver ordered clause queue. */
	satclause *lstunsats;               /* Pointer to last SAT clause in the SAT solver ordered clause queue. */
	satclause *frstunsats2;             /* Pointer to start of the part of SAT solver ordered clause queue with clauses */
	                                    /* that are unsatisfied and have two or more undefined literals. */
	hashchcmp *frsthshundf;             /* Pointer to first undefined hashchcmp element for this KB in the hashchcmp ordered queue. */
	hashchcmp *frsthunbp;               /* Pointer to first undefined hashchcmp element with clauses with negative and */
	                                    /* positive literals (both polarities) for this KB in the hashchcmp ordered queue. */
	hashchcmp *lasthshundf;             /* Pointer to last undefined hashchcmp element for this KB in the hashchcmp ordered queue. */
	satnode *satrootnode;               /* SAT solver root node pointer. */
	learnvector *lrnvector;             /* Pointer to sparse learning vector structure set or NULL if no learning vectors are available. */
	dsendnode vartrmidx;                /* Indexes of positive equalities selected for paramodulating from them in active clauses */
	                                    /* that have maximal root terms that are variables. */
	uint64_t signature;                 /* Last signature used for inference process. */
	uint64_t locksignature;             /* Last signature used for lock and unlock processes. */
	int32_t flags;                      /* KB flags, see definitions above. */
	uint64_t nformulas;                 /* Number of formulas in KB so far. */
	options opts;                       /* Options for KB. */
	int32_t selectcnt;                  /* Number of extractions done so far in the current round-robin clause extraction cycle. */
	int32_t lrscnt;                     /* Count of clauses removed from the passive queue in the current LRS cycle. */
	uint32_t splsymbols;                /* Number of P-predicates for P-predicate name assignment. */
	int32_t status;                     /* KB status (see flags above). */
	struct timeval time;                /* Start time of current saturation algorithm. */
	struct timeval endtime;             /* End time of current saturation algorithm. */
	stats prstats;                      /* KB statistics related to this KB. This depends on the context (process, main or working KB). */
} kbase;

/* Structure for substitutions. See Unify() function for substitution format. */
typedef struct ssubst {
	uint8_t *buffer;                    /* Pointer to buffer with substitution. May be NULL. */
	                                    /* See Unify() function for format description. */
	uint32_t buffsize;                  /* Total buffer size in bytes. */
	uint32_t substsize;                 /* Size in bytes of substitutions including the UNITEND byte. */
} subst;

/* Structure for subsumption SAT solver atoms, also called components. This is equivalent to a merge of hashchcmp and asymbol structures. */
typedef struct ssubslitrl {
	struct ssubslitrl *nextelig;        /* Pointer to next eligible component. */
	struct ssubslitrl *prevelig;        /* Pointer to previous eligible component. */
	struct ssubslitrl *nextmain;        /* Pointer to next component related to the same literal in the main (candidate to be subsumed) */
	                                    /* clause. It is set to NULL if it is the last component in the linked list. */
	struct ssubslitrl *prevmain;        /* Pointer to previous component related to the same literal in the main (candidate to be subsumed) */
	                                    /* clause. It is set to NULL if it is the first component in the linked list. */
	struct ssubslitrl **varnext;        /* Pointer to set of pointers indexed by the number of a variable. Each pointer in the set */
	                                    /* points to the next component that has that variable in the component substitution. */
	struct ssubslitrl *nextpropg;       /* Pointer to next component removed from the eligible list or assigned to FALSE due to AMO */
	                                    /* (At Most One) or substitution incompatibility propagation. NULL if last component in the list. */
	struct ssubslitrl *nextpropg2;      /* Pointer to next component with two valid substitutions one of which is incompatible with the */
	                                    /* global subsumption substitution. */
	uint32_t flags;                     /* Flags for this component, either POSITIVE, NEGATIVE or UNDEFINED, see definitions above. */
	struct ssubsatcl *satcl;            /* Pointer to SAT clause binary formula to which this component belongs. */
	subst *psubst[2];                   /* Set of two pointers, each one points to a component substitution in normal format. */
	uint8_t **psubst2[2];               /* Set of two pointers, each one points to component substitution in extended format. The */
	                                    /* substitution in extended format is a set of pointers indexed by a number of variable each one */
	                                    /* pointing to the term that substitutes the variable. */
	int32_t mainlitnb;                  /* Literal number starting with 0 in the main (subsumed) clause that corresponds to this component. */
} subslitrl;

/* Structure for subsumption SAT solver  clauses. This is equivalent to satclause structure. */
typedef struct ssubsatcl {
	struct ssubsatcl *glblnext;         /* Pointer to next subsumption SAT clause of the global queue, NULL if last. */
	struct ssubsatcl *next;             /* Pointer to next SAT clause of the subsumption SAT solver ordered clauses queue, NULL if last. */
	struct ssubsatcl *prev;             /* Pointer to previous SAT clause of the subsumption SAT solver ordered clauses queue, */
	                                    /* NULL if first. */
	int32_t literals;                   /* Number of literals in clause. */
	int32_t satlits;                    /* Number of satisfied literals in clause. Only one literal can be TRUE in a clause. */
	                                    /* TRUE literals have a POSITIVE flag value. */
	int32_t undeflits;                  /* Number of undefined literals in clause. */
	subslitrl formula[1];               /* Dummy for start of formula literals. This must be the last field of this structure. */
} subsatcl;

/* Structure for subsumption SAT solver  nodes. This is equivalent to satnode structure. */
typedef struct ssubsnode {
	struct ssubsnode *nextnode;         /* Pointer to next subsumption SAT solver node, NULL if last.*/
	struct ssubsnode *prevnode;         /* Pointer to previous subsumption SAT solver node, NULL if first.*/
	subslitrl *symbol;                  /* Subsumption SAT solver component/literal that was assigned in this node. */
	                                    /* FALSE assignments due to "At Most One" or substitution incompatibility don't have */
	                                    /* a node. Only FALSE assignments done as an alternative to a failed TRUE assignment */
	                                    /* have a node. */
	subslitrl *frstpropg;               /* Pointer to first component removed from eligible list or assigned to FALSE due to AMO */
	                                    /* (At Most One) or substitution incompatibility propagation. NULL if list is empty. */
	subslitrl *frstpropg2;              /* Pointer to first component with two valid substitutions one of which is incompatible with */
	                                    /* the global subsumption substitution. */
	int32_t varnum;                     /* Number of first variable not processed as it appears in the substitution in normal format. */
	                                    /* If this is -1 it is the same as it is the number of first variable in the list and */
	                                    /* means that no variables were processed when attempting to do the do the symbol */
	                                    /* assignment. If this is the side clause maxvarnb+1 then all variables were processed. */
} subsnode;

/* Subsumption SAT solver global control structure. */
typedef struct ssbsctrstr {
	int32_t sidenumvars;                /* Number of variables in side (candidate subsuming) clause, computed as maxvarnb+1. */
	subsatcl *frstsat;                  /* Pointer to first SAT clause in the subsumption SAT solver global queue. */
	subsatcl *frstunsats;               /* Pointer to first SAT clause in the subsumption SAT solver ordered clause queue. */
	subsatcl *lstunsats;                /* Pointer to last SAT clause in the subsumption SAT solver ordered clause queue. */
	subsatcl *frstunsats2;              /* Pointer to start of the part of subsumption SAT solver ordered clause queue with */
	                                    /* clauses that are unsatisfied and have two or more undefined literals/components. */
	subslitrl *frsteliglit;             /* Pointer to first component in the eligible subslitrl eligible queue. */
	uint8_t **glblextsubst;             /* Pointer to global substitution in extended format. See psubst2 field in subslitrl */
	                                    /* literal for details. */
	int32_t *glblvarcount;              /* Pointer to set of counters indexed by the number of a variable. Each counter counts */
	                                    /* the number of times that the variable has been referenced in the global substitution */
	                                    /* by the substitution of a component that has been set to TRUE. */
	subslitrl **firstvar;               /* Pointer to set of pointers indexed by the number of variable. Each pointer in the set */
	                                    /* points to the first component for which the variable that indexes the pointer is */
	                                    /* included in the component substitution. This is related to varnext field in subslitrl */
	                                    /* structure. */
	uint8_t *allmemory;                 /* Pointer to block of memory where all setup memory will be placed. */
} sbsctrstr;

/* Structure for linked list formula format. */
typedef struct slkedfitem {
	struct slkedfitem *next;            /* Pointer to next linked list item. */
	struct slkedfitem *prev;            /* Pointer to previous linked list item. */
	union {
		struct slkedfitem *mtchbrckt;   /* Pointer to matching bracket item. */
		nameschain *vardata;            /* Pointer to nameschain structure of a quantifier or variable item. */
		hashchain *symbol;              /* Pointer to hashchain structure of a predicate or function item. */
	} ptr;
	union {
		hashchain *sksymbol;            /* Pointer to hashchain skolem symbol in existential quantifier or variable items. */
		int16_t varnumber;              /* Number assigned to standard name of variable in a quantifier or variable item. */
		struct {
			int32_t posclauses;         /* Lower bound of number of clauses generated for a block for the */
			                            /* open bracket or literal starting the block. */
			int32_t negclauses;         /* Lower bound of number of clauses generated for the negation of a block*/
			                            /* for the open bracket or literal starting the block. */
		} clauses;
	} data;
	struct slkedfitem *dupq;            /* Auxiliary pointer to duplicate quantifier, used when duplicating blocks. */
	int32_t flags;                      /* Item flags. */
} lkedfitem;

/* Structure for multi-processing control. */
typedef struct smpctl {
	clock_t start;                      /* CPU ticks at start of saturation process. */
	clock_t end;                        /* CPU ticks at end of saturation process. */
	int32_t status;                     /* Process status. See definitions above. */
	int32_t prctype;                    /* Set to 1 if Saturate() is in process, 0 if Saturate2() */
	                                    /* is in process, undefined otherwise. */
	int32_t prfprntinproc;              /* Set to non zero if proof printing is in process, zero otherwise. */
	int32_t solvekb;                    /* Process number for problems solved in the saturation stage (see procnb variable), */
	                                    /* active_cores for problems solved in the pre-process, -1 if problem was not solved. */
	sem_t procctl;                      /* Semaphore for process status updates. */
	sem_t statistics;                   /* Semaphore for statistics update. */
	sem_t strategy;                     /* Semaphore for process strategy selection. */
} mpctl;

/* Structure for skolemization data needed for printing the proof. */
typedef struct sskldata {
	uint64_t sklnumber;                 /* Number assigned to skolem symbol. */
	char sklvarnames[1];                /* First element of "hyperstring" consisting in names of variables one after the other */
	                                    /* including their null character at the end. A double null character marks the end */
	                                    /* of the "hyperstring". The first name is the name of the existentially quantified */
	                                    /* variable that has been skolemized. The following variables are the names of the */
	                                    /* universally quantified variables that are arguments of the defined skolem. */
} skldata;

/*----------------------------------------------------------------
 *
 *  Global variables shared with other modules that are initialized here.
 *  WARNING: Don't use "static" as then a different variable will be created in
 *  each module with the same name.
 *
 *--------------------------------------------------------------*/
#ifndef VARTYPE
#define VARTYPE extern
extern double tot_cpu_time; /* Total CPU time used in seconds. */
extern int32_t tot_pbms; /* Total number of problems examined. */
extern int32_t solved_pbms; /* Total number of problems solved. */
extern int32_t queuerts[2]; /* Ratio numbers for weight queues and age queue. */
extern int32_t *slctordr; /* Pointer to clause selection order queue. The I-th clause selection is picking the clause */
                          /* from the slctordr[I] clause selection queue. */
extern hashchain *kb_hash; /* First chained hashchain element. */
extern hashchcmp *kb_hashcmp; /* First chained hashchcmp element. */
extern int32_t syntaxmode; /* Syntax type: TPTP or DRODI. */
extern int32_t proofmode; /* Proof printing type: TPTP or DRODI. */
extern int32_t prtkbdata; /* PRINTKBDATA type: 0->OFF, 1->ON */
extern int32_t verbose; /* Set to 2 if VERBOSE option is ALL, 1 if it is ON, zero otherwise. */
extern char *importfile; /* Pointer to IMPORT file name or NULL. */
extern char *learnto; /* Pointer to LEARNTO file name or NULL. */
extern char *learnfrom; /* Pointer to LEARNFROM file name or NULL. */
extern learnrecord *lrnrecords; /* Learning records with headers and vectors from learning file. */
extern int32_t learnnew; /* If zero then learn-to output will be appended to learn-to file. Otherwise a new file */
                         /* will be created. */
extern int32_t loadredundant; /* If not zero then add formulas with lemma, theorem and corollary roles, */
                              /* otherwise these formulas are not loaded. */
extern double ppthrshld; /* Threshold for the preprocessing clause relevance filtering. */
                         /* See Preprocess() global comments for details. */
extern int32_t prproflags; /* Preprocessing clause filtering option flags. This can be CLFILTERINGOFF, CLFILTERINGON, */
                           /* CLFILTERINGNONEQU and/or CLFILTERINGRESIGN (see definitions above). */
extern uint64_t pbmtypestrats[PBMTYPES+1][ALLSTRATEGIES]; /* Strategies to use for each type of problem. */
                                            /* See InferenceProcess() function global comments for details. */
extern uint64_t pbmtypeinstr[PBMTYPES+1][TUNEDSTRATEGIES]; /* Limit of executed instructions for each strategy in the portfolio. */
                                       /* See InferenceProcess() function global comments for details. */
extern int32_t numstrats[PBMTYPES+1][2]; /* Number of portfolio strategies and complete strategies for each problem type. */
                                          /* See InferenceProcess() function global comments for details. */
extern uint64_t usrparam_mask; /* User parameter options mask. */
extern uint64_t usrstrat; /* User strategy, only used for learning relevant parameters. */
extern uint64_t strategy; /* User defined strategy flags. */
extern volatile sig_atomic_t signalflag; /* Not zero if a program error caught signal process is in progress. */
extern int32_t alrmstatus; /* 0 if a timeout alarm has been programmed but not expired. */
                           /* 1 if an initial programmed alarm has expired and an alarm extension has been programmed. */
                           /* 2 if an extended programmed alarm has expired but program continues because it is */
                           /*   in INTERACTIVE mode. */
#if (MEMCHECK == 1) || (MEMCHECK == 2)
extern uint64_t allcdmemory; /* Currently allocated memory. */
#endif
extern pid_t *pids; /* Pointer to set of child process pids. */
extern uint64_t memorylimit; /* User specified memory limit. */
extern uint64_t initavmemory; /* Available memory at the start of the program. */
extern void *mmapptr; /* Pointer to mmap shared memory. */
extern char *logpath; /* Pointer to path for exceptions logging. */
extern int32_t *fvidx; /* Pointer to non null components of feature vector generated by GetClauseVector() function. */
extern uint64_t instrlimit; /* Maximum number of executed hardware instructions before stopping saturation. */
extern float foftmfactor; /* Factor that multiplies instrlimit to get the final used limit for FOF problems. */
extern float ueqtmfactor; /* Factor that multiplies instrlimit to get the final used limit for UEQ problems. */
extern float usrtmfactor; /* User defined factor that also multiplies instrlimit to account for CPU speed. */
extern uint64_t perfmonrate; /* HW instructions per second used to emulate perf_event_open when controlling the number */
                             /* of executed hardware instructions. This is used when SYS_perf_event_open syscall() */
                             /* is not available or when it is disabled by the user (see PERFMON command). */
extern int32_t perfmon; /* 1 if SYS_perf_event_open syscall() is not disabled by the user. */
                        /* 0 if SYS_perf_event_open syscall() is disabled by the user and must be emulated */
                        /* even if the syscall() is available. */
extern int32_t perfmonstate; /* 1 if perfmon option has been specified by the user, 0 otherwise. */
#if defined(DEBUGCODE) && defined(VERBOSE)
extern int32_t verbfrom; /* Initial clause number for verbose tracking. */
extern int32_t verbto; /* Final clause number for verbose tracking. */
#endif
#else
double tot_cpu_time=0.0;
int32_t tot_pbms=0;
int32_t solved_pbms=0;
int32_t queuerts[2]={5,1};
int32_t *slctordr=NULL;
hashchain *kb_hash=NULL;
hashchcmp *kb_hashcmp=NULL;
int32_t loadredundant=1;
int32_t syntaxmode=TPTP;
int32_t proofmode=TPTP;
int32_t prtkbdata=0;
int32_t verbose=0;
char *importfile=NULL;
char *learnto=NULL;
int32_t learnnew=0;
char *learnfrom=NULL;
learnrecord *lrnrecords=NULL;
double ppthrshld=CLFILTERINGTIMEOUT;
int32_t prproflags=CLFILTERINGNONEQU;
uint64_t pbmtypestrats[PBMTYPES+1][ALLSTRATEGIES]=
{{14261191181,19462390511,13724320303,10536615974,4127739935,8959352895,11308401385,27481100831,9999532287,26542113830,1845534948,14193574654,9831715053,30669055532,27917615626,12449252079,6778061546,19529487069,26676338730,14227919419,23622873855,14496313391,15301394492,8657584661,12080141009,13086799404,19495739955,2181304524,7080558813,14629816058,5101109469,25937584648,15636918303,11610116351,11274330863,302543102,8690884631,11811774705,29729247999,17213997311,19931381782,33871064,3825533643,8589968111,28790259951,8589984511,235487291,10334806575,906027581,18790748361,14663622691,2148345034,4966146106,9798730487,2752295950,2751546394,23857529387,15771169531,14663623179,15368742646,22850834948,5200957461,32749707837,9395524145,9361744599,1879105595,23958650895,5402575069,3321955019,15670478062,11039973578,5435863754,906061018,269288490,10469073435,28454200840,17851516142,28588650701,22246896331,4564247071,17717338857,30635217663,4563697903,12449014014,11475951843,6577520681,20501786654,235462711,4564271162,5167972907,20938028794,20736930868,9227768524,3959472895,8959108648,4429721831,17449152731,11609907723,2382708957,31407284268,302293741,18019292906,13153391321,28219859709,19327909933,9396101832,12146786523,168055327,30937744109,26441187372,23756846122,5369282781,27514909920,23622385899,29561475135,15770616015,17516266185,15234032700,13421806121,18891683050,27414519833,7281631773,10000024311,201672442,4362371117,10737762523,28722946577,17515463421,1207996655,11005903583,2215149771,15771141324,1174950654,5134172379,26542093344,27950874863,29192405725,2516860991,2516648139,32816547068,18321294075,13422039567,32380379345,17750860493,1410130998,28488065241,3825509420,14093430991,17247778528,32213082828,4026861066,12516147259,10502567440,30971035853,12079677658,20032561381,11475968539,772096059,15569518823,6744516328,17280811035,24528363747,32414171691,13724082746,3792184041,9798190606,29360680991,10368901343,23756595455,15133324320,2818593311,19696750629,9261031630,26843611179,20200300744,26575400991,15401796319,8959049973,15771443722,6778045181,13657190951,13992211694,3590587079,21843956477,9227780127,9261023941,12549632199,10838385391,22079083020,7248323596,9831994064,10234143790,10939310319,2818593535,25769828373,13555996206,21844234987,13489472043,31810478619,18254430251,19126321389,9899106361,26407617077,11442606289,6040404219,26675774510,28789998319,33084719325,28823560238,13556047903,26307199015,13656999642,10737509115,3288671242,10033128187,26407887573,25904829151,27447525578,28991827983,13556858906,23085715689,2416238837,20468487934,31910573100,26575634632,15838019779,26608673319,12952094458,17449156643,33051150062,26608960010,17918885899,739066107,13489218241,10972582097,20603245253,13656974876,32044524075,12180567596,18488795855,1309221603,12415664841,19328152297,27582324949,31239217899,7214252285,28219318506,14260666888,26072061128,19428075068,29897056796,1409287210,9797967915,2483626506,5469675716,22280684091,10368856609,4563973866,31810429966,32279406282,2684706842,9127387700,17884529367,101253835,15703802378,22180016839,12616507630,234926090,2483847399,17348165670,15670534683,19327415038,12985876729,15938921199,30870917851,12550218435,7248294439,9932432094,29024903223,1208575226,18053080810,1241563707,27381257234,26039026886,15804178468,30803304970,9396076599,30937277137,739017413,13724082751,31742542589,11979022547,9428812496,5101113883,32581406779,25904594975,8657084943,31943909947,20502286377,7147684554,14026077916,32916939493,29864260159,22280966341,32916971531,13421863481,9428821712,10200629971,202170911,4999701562,24159740473,13422309928,27078509786,33051381959,4127487514,1811989014,6677423642,19529237036,21274099755,20535325708,28219568157,8825107153,32045051948,19361507050,28253668926,28521866794,134267644,5201281041,23085776906,9898583098,30165979871,28454699069,10738016490,13254320891,15737323725,31441085150,7281894141,9865041094,13522764299,22616273662,5034041370,5133877503,14495598106,27716236302,32346562779,5637234899,4362683610,19998491389,12046303943,31373461034,30333780541,17414816259,23958684165,17381532202,8892491498,18321040121,15033229343,17247531249,30366806219,9127632941,20703138042,12549402661,15367979549,22145984030,13053256255,32280241882,23723311625,14730699780,13254074411,26542092812,30602187007,26844144323,11006669329,29595566825,2147819723,22078825512,22313972265,22280749273,28052291626,2148081709,21609144346,12918801113,6510146270,32414449689,23421609170,21810930207,26474717703,2215158309,31877300938,19394758380,570777657,27414279726,12550190124,14227919419,8657584661,19495739955,235487291,4966146106,2751546394,10469073435,4564271162,27414519833,28722946577,12516147259,11475968539,772096059,17280811035,25769828373,31810478619,9899106361,26407617077,13556858906,22280684091,2684706842,15670534683,27381257234,5101113883,32581406779,31943909947,13421863481,4999701562,24159740473,6677423642,5201281041,9898583098,28454699069,5034041370,14495598106,21609144346,32414449689,570777657},
{2516633106,15367979549,6778290182,3389273124,21676990990,26944496698,22280447014,26541909554,32816291863,8657609734,7181488147,771752454,18522080994,10838688978,27045176326,6006256175,15066006029,22145984030,10469015558,12415985396,23421296678,2517107214,26072393250,11476475936,24327521326,17449156643,31810429966,20737236995,15569813735,7214776340,15133324320,570721802,4429461190,12952870925,28521575968,4127236106,30367327266,7080251939,32011219506,805872326,3758363335,20468748349,9328181812,27112001555,3288671242,6509835298,12985597957,7046472930,14495798494,5469378082,22817804330,11979224126,32782708759,14328311818,1611142696,32213082658,18421420584,22683361830,30098629121,29695701055,32280178707,30334006786,22549422108,18522390579,7315190808,10000024311,10872426498,4530157292,7047226085,4362643168,23958684165,8791581207,14529089587,1410130998,17750872305,6208095424,27448115203,3557074466,6476619824,2416238837,15402012718,15133622525,6711472178,6073967864,17516012035,14026047495,15301124099,28790534671,10536132614,5704843780,6040355047,2382386234,8657842208,23891599902,23220237344,14663639576,23421609170,32312960006,9396060165,5704520738,14663622691,872742923,19462145266,15032922852,24394625075,15032983553,5033730246,29024899088,22348047878,2953355974,21810967094,22817620210,30635470886,671613472,1778943010,6812082182,3557615126,30669291537,23219716112,15133663285,26307543283,1912913950,5200957461,17347908642,13422617117,24327521830,7180977666,22213649109,28152193046,12985606178,7281631773,15334642369,23286817794,21575499785,22280697590,22615762466,21776847890,19931898886,1879852606,20603265042,13522502146,14026019363,11576632530,6677373990,4664115253,32783262261,32581628427,23387496473,24428154914,31843209245,2248458773,6241959954,10368585737,27951386643,20737208334,2886526002,1409287210,6577456684,4429242425,6710919685,22884405298,28051578883,3254829077,1275351255,5537038854,12046303943,31474065609,22380855829,6543684838,10435502274,22045319220,4564271162,33018151938,5436408547,12147523623,10334838819,12147017242,30937993243,2382893575,32414131230,15838527509,29293294311,5705053388,3960223266,30367096835,12650050257,17952236562,32313475085,3423379497,15871295541,22817804805,20099400229,12415443978,15334641672,24461481510,26541823530,20603002934,24495288346,22750494755,23623128114,17448871116,21676212983,2986647586,4899823634,2785022499,29260300853,6979605522,6241406995,13757923379,17985459762,537669826,5537071628,11442111185,20099179010,23756846122,23857529387,24226562054,15938650117,4497122536,26944496698,26541909554,7181488147,18522390579,6476619824,6711472178,14663639576,21810967094,30669291537,15133663285,31843209245,4564271162,17952236562,4899823634,13757923379},
{12415205385,28152170028,10737463849,26508804863,9160364076,14093430991,11610357805,15368758788,9697517817,26139431627,3355795515,13757334077,8926277647,11107050542,20401677019,11409113115,25837187593,9462366235,26307486269,31977660955,26138911236,10334806575,12482536504,31474065609,26474521097,9227780127,25770607129,9731662075,2483643133,27985248489,11878359765,26508800009,27044896991,8657596959,27179123435,29797187629,10838393589,13523009593,10838626519,29293597427,31977918713,15704327724,30602428963,10402226395,29528441553,26071831279,10234675743,9966223407,27447591149,27078701803,26072056329,28152434697,26340491983,9193926895,26910679253,8657584661,12348330191,26340262123,8859222025,11005903583,30937719334,27079008317,10771277028,25837496048,29528186905,10805338673,10838393908,9227534349,9865822244,9227534893,32279400962,26575655449,12180820203,26642259969,10033328857,31742518520,8590496463,27582038729,26072064745,9965937389,12013273131,26441707577,9194738189,29159327244,11408519397,26911490265,11543270105,30064780992,29092237338,11543298813,30601655530,12985876729,13455880220,28219327007,14160576560,29092278480,26474508534,11442083344,27179151611,11006149318,26441728537,27548209881,26575400991,29192671288,30836552732,8926269481,25837748987,11341660719,8959594697,28655747791,10033353431,9731354635,11543577323,28454720015,26407346925,26038505711,11039445743,8959558341,32044533468,26541879831,14160806098,12382126655,26910974173,14764527672,9798734555,14764757524,28487979245,8757998840,10536132614,9798771956,32011797012,11879109872,9127600171,9396076761,11442153202,11442345170,14059364592,26944545321,27179180605,26004713529,8791351313,31676224758,27414532329,27648868381,8824897597,14026077916,11878314697,14562669103,31138525741,14630286896,27548775634,11945468665,11275096639,26508026585,8959619131,28790259951,27716759556,27716298468,27716236302,29327376413,11375235323,13623886347,26843828793,11576616460,28789998319,32213082828,26441725486,9428807917,11308389586,27045151485,25904849101,26844123673,27918135519,31340627140,8690884635,9160433705,11442615006,12146983135,14696882217,27112543272,8724444730,27112551456,9160384574,29126086681,9395294426,28454699069,27078476025,27045659375,10570179115,11006120491,9462358561,9462637309,29058187515,27548229867,9999557321,12482306068,10033087035,8590533353,10200875049,27414250011,27414250205,10502619197,6979596847,10503077919,8824828623,10737714698,26407887573,10771301580,10805375227,28051578883,28152709373,11072975082,29797204701,11476453114,8758235335,12012787406,10267943126,12717457611,9764880955,12113232125,12113429215,12415140399,9932185795,13086781693,13488898068,7147680465,13656736505,14093162210,10200629501,14563442701,29091971289,14629749460,10301261040,10536653539,10871665175,14629816058,29092237371,14630260943,29192405725,14764754680,14495551488,28488271562,32346497599,25770623180,26004955338,11946197709,26877145803,26306695227,26877677786,31742554353,26910744637,26340790831,13421863481,25904594975,5369282781,5402575069,8589946921,13690217193,26910933558,14864892147,26944266303,22549422108,9865322553,10401902841,28488029758,10268467961,11308401385,7013200113,8657912019,8825107153,27380695805,14093701887,21508964413,27615613151,23622873855,27615613661,27212944067,27716788269,27917316309,28219290854,28622226666,28219318506,29192904950,28689065692,29864022553,10267934751,11542733549,11542762223,28051571204,9965699593,10469008604,20971831869,26609205469,30165434378,9261023941,30735881790,3825509420,30938022456,6274724585,30971269677,13958931157,31474086109,26440900801,26441155297,28186530559,31675438330,12348072962,22045319220,14495589067,26071797801,27044945965,26139471887,26911498269,10234663659,11274322637,13723799592,28621963497,31709479163,31742542589,32313192998,32313485030,3355795515,13757334077,11409113115,9462366235,26307486269,31977660955,12482536504,25770607129,8657584661,29528186905,10805338673,14160576560,26541879831,32011797012,27179180605,8791351313,27648868381,8824897597,8959619131,29327376413,8690884635,28454699069,27414250011,10502619197,9764880955,13488898068,26910744637,13421863481,28488029758},
{135029456,1325801480,23958004744,2147501072,218890440,688670416,1090929360,1191207120,1208763096,1762549976,1811939528,1862402248,1862934728,1879851216,2265473752,2533908696,2785436368,2785437392,3221382672,3355722968,3489810136,3759030992,4328801488,4849426640,5117600472,5687764176,7113827024,17331283472,17499316752,18522465304,18589172752,18606743576,18908078096,19160253976,19344942616,19445473296,20116169240,20753957912,21223457808,22146081816,24159330496,2164933848,23623115776,3876078808,6812214272,17733928656,805455056,17733535440,19109406424,22364030464,2147501072,3221382672,17331283472,17499316752,18522465304,18589172752,18606743576,18908078096,19160253976,19344942616,19445473296,20116169240,20753957912,21223457808,22146081816},
{12180292335,17516266185,26542113830,15367979549,26676346911,13724320303,24427909661,1946199050,14563181295,26911498269,31977947391,12046332633,9160364076,11274322637,31944139007,23756595455,6509842652,11542774526,25836962047,14093713451,5704302847,3825533643,14160822989,2751529481,9396101832,12146786523,28823560238,4295288062,20032594147,7080538161,12549632199,18891683050,31373706266,11979227335,30098866743,4966146106,2147750441,24427700747,23153365047,1174942726,11005903583,20938028794,31138579179,11072975082,10469318891,4563973860,9731097846,10435978960,14763999251,235458619,3221267183,23958417980,6711206143,11811513594,9227768524,12449252079,1778417702,23186710755,14730690767,4564271162,6711443695,18857681618,22750442189,19663249658,8691176986,10000060978,12012806707,9697280733,19596656891,268486198,12113461311,18991842318,29293585444,7047308025,806135310,9193972447,14093656590,24293470270,13489472043,4027367455,13053026363,1308714714,23756612137,19495399948,28219327007,604554782,18019275007,2483601631,11945673263,10234143790,9193939984,18052603933,4899791103,2684690665,2952819421,4060119751,26844119615,4295496421,15402012718,32782963423,18791284765,4362613964,14663639576,21206958127,11275108591,25837208301,14193574654,21207220239,7281894141,21810421770,25837187593,6677893645,9227780127,6207652377,2382708957,15838527551,13220538104,5537038854,25904594975,19462390511,5670760703,28790259951,30333560024,27984941777,11845027062,27615854831,8959586559,17448861903,7080551142,20938007273,15301145311,6241181722,25837208111,5033222164,906061018,20032021051,1678025422,6577250844,26508050463,2382676543,25837441581,26038559254,6610538559,28521832687,15368196655,21274330670,28656314054,9496215807,19663474749,14026301686,1208251101,10268492351,30266679325,2148037852,4999934678,11072995535,4631082030,26944266303,23857529387,18253906127,31810429966,10838385391,4530122761,235193087,2516648137,24327521326,30635255030,25836962559,22145984030,13791416855,805865166,23320331782,29058441967,2449498872,15670478062,28219318506,9462965786,3288617689,9496461550,5737838108,22380855829,14026047495,17449152731,31508148423,772309743,10737762523,23287312621,10334806575,11978962462,9697788111,9462371065,15401796319,2483570392,14563410119,839156750,29024621097,20200122106,21240045819,15167193091,269295848,4328604914,15938359311,22280679469,10368901343,19360936154,30602187007,27950874863,2919318233,7214727727,11274864350,2214929608,22649856570,29864022553,23622385899,27548775634,1107903538,202170911,3456663757,21676212983,3590353116,19898057750,873255485,28253668926,17381532202,15033229343,32279422206,2516664537,28018488864,12012537366,31977693214,18455019546,5705053388,15234032700,29595042862,9932432094,9764644078,202146351,2751529673,23152599279,10401907439,12952330303,6979850953,6980161791,18824593647,3020189946,11979485404,22381422104,24226305224,15770616015,17213997311,805872326,30165750486,30601995514,10770977806,21206696138,22683594797,24428176087,5973263103,19696779977,20502286377,11006672935,4563697903,17851516142,15032983553,15301394492,3321888775,26843611179,23454859477,18321318627,2752332537,31273029828,4026807021,26643070976,31205679837,13724082751,6543684838,21475165898,201359918,19428369978,10737714698,33018134559,805889758,2919842521,24159789290,9194750167,17918911039,18555650622,10435993615,17851262190,31273566239,10401902841,23891562513,19662938154,19495739955,335828698,7080014076,5671248407,21542544074,15401504983,3456119336,168055327,5906395389,6174066206,8624046279,235177678,22112977434,11978977826,8657872426,10502841039,6241198081,10368884750,23622352943,28186279949,27078779642,23857243878,5201466924,23924855505,17516019922,27078701803,4999886028,29595067103,20703388174,10469015558,11979514070,2181595183,5167972907,14328309258,12717666345,15100317901,7147446811,4395696681,2885685807,10469278221,1376026823,14126728726,8724447983,15569813735,20938531558,14663622691,24259911903,26139207406,9496471270,18622740026,12080141009,28186304539,5201003241,9697264174,22078821416,7315653832,4563722807,14026047686,13422067917,20132983038,4597231624,6677365295,3254792396,32346825467,14026106074,2483323406,29192904950,29897293871,14026289871,8657609734,24428232955,6509851385,6677423642,14093701887,5905860126,11543577323,26676338730,21140080360,1375822073,28085096962,28186296383,18321310946,9932424434,28521316567,6610559499,32984010959,3489723134,20971812056,20133232669,9463162616,21743272968,26844349176,9193951458,27481399324,4664115775,570499272,14864705264,11509752022,21508933182,17180738107,17314369567,33017594063,21609579049,9965683920,22078825512,29528183326,19025962216,15703826491,4127739935,570467374,21240299770,21475119641,9261852366,21844287736,5436654111,22280749273,10033079850,25770607129,5134172379,4966146106,235458619,4564271162,8691176986,10000060978,24293470270,13053026363,9193939984,18791284765,14663639576,6207652377,20032021051,6610538559,19663474749,9462965786,22649856570,1107903538,873255485,18455019546,22381422104,19428369978,19495739955,22112977434,7147446811,6677423642,17180738107,15703826491,25770607129},
{21005349381,5637234899,22146191397,134280766,8724980263,2986652173,12884960502,23454859477,1141162007,32279397063,18522390579,21542281227,1409376987,772018405,24260452395,19461853245,17951670999,19629916919,12582953990,1845526567,15871829046,2483847399,20166525463,1879097879,18421678823,19932168439,335594199,1376026823,9228034055,2415951879,25837215751,2953347271,2953625655,8959336455,17516249815,2785034257,19093028871,7113855221,33118281969,7147393765,24293986325,26978328583,6744740357,23924888277,23253258757,9798730487,6711202551,10469556791,7147393031,10939318999,7248335415,11073274103,11106829015,31507931671,11375263959,15838011639,11609882839,32548102695,28253388807,28521316567,28085641943,28522103543,28656042023,13724066535,28790276311,13187195399,13723820599,10905223911,11006672935,11039440935,28186279943,28621980183,5536518885,6174315013,6174315045,23354708677,12885501122,27515200199,32447211042,5972770843,32280191687,4462776549,5502980151,5502980823,5503505111,5637984503,6208147511,22146765013,23353937943,4463301127,4597518853,11912388839,22280966341,29193183463,15066243271,12012528327,12348874967,12382150855,30065598663,12146761943,30937227975,31205663463,32346530551,134280766,18522390579,2785034257,24293986325,7248335415,5972770843,5502980151,6208147511,23353937943},
{9261326894,4127236815,13019422735,1611440647,12012814891,11643428879,15938912263,11845284039,5603631847,14194090223,12717171439,4563407049,12952838695,12617057834,15603114697,9261580525,15401517805,7080280615,34144994,15033245738,7080551119,6811550223,15234015271,11475943458,8824848909,8623563307,14596743367,6744776907,11811193069,4664074441,10871964193,787650,2382440140,9328394785,9731375648,14328045807,12348326093,12885196839,3356008460,12952052970,10872205834,9832038955,3758625517,5134393581,15200755722,6543713001,6744735983,4966846701,14092936202,7348723917,2382639852,15703515183,15301124847,14596482602,1410113773,15166636749,9664013322,5604155949,4430013159,11375518407,3590619151,7114334734,4966621391,15167430895,3020235273,15905095727,6811852845,5201207503,3926164167,11274879211,11375509743,14764285962,3758924495,14160790055,14831657707,13120077870,7114358831,7080026151,12650316522,13757620263,10469028359,11375510061,637837839,14831624711,5469937679,11274321935,2215419917,2282029579,14495581738,9496494784,9663980750,15837986855,7348724423,12348850927,15368486951,6275277547,3222077675,12952838383,11342217773,13153370671,14193557543,14663331884,13757392578,10201104591,15636930567,11073815265,6275245094,12952830703,6979920619,13590102253,15938945770,3322446351,11039965199,5101101775,15234015431,8791293965,2919530733,9965961261,1275625519,15234540071,13959463438,11375542305,4966614756,10905748015,6577267435,15300866093,14328037414,4597253318,13958938637,1074299407,4295533101,2416247010,11006418959,11643453474,2415952079,10234638884,13455360014,6543126765,12549923375,2886009058,9865863915,4328826382,15301148875,7047250670,11274847438,3758887661,3892379842,11342258923,5369266735,15167455457,8925524207,8791298094,168362019,12583188012,11275108397,14529372711,67928815,9697800908,302057154,1376288783,15637184711,10268487918,4295034058,11610362597,12382151726,14798069807,10502836430,5168177385,15569813543,5570339021,3926163684,13220460044,1308959435,10570207758,4396230380,369427170,11107115755,15401821194,11475681793,13724025377,5738341056,9697800910,12616532171,13522764842,13120382186,10268222990,15938691073,235450383,2415952623,10268225742,14327783463,14092907212,15938659334,7281385675,4328587275,6006551049,1812243151,9966227470,2215420621,9194177760,10234409006,5637210857,9832047328,14630035687,10234662927,14194090727,9194221582,3993051333,13052740640,6979649771,14059372587,6577225767,6577235468,8959603406,15301648941,6006547470,4362928833,10939310311,15703506983,1845568235,4463075331,9295177226,4362641423,8824853006,15200223936,4597023978,13221265966,5368746699,2416255523,10435997934,671949001,13657252896,15334671047,9932943374,15133115936,15367997162,11609833991,14059872487,14159979712,8758006286,6677627433,3221824235,4530438369,13589585959,14227112461,9160659502,4999618793,4429751020,2886500399,9261322279,15200485930,13522469391,9697566721,8925516014,11107042304,3322184425,6107766795,8926269445,13724320301,8925806634,9361724110,11442106915,335577839,10335625216,5033689312,4127490793,14059872807,1980276751,1879901218,9362255886,12147009070,5603918561,11609870371,9664246316,3959455785,8791830574,14696873991,6442780202,9328431630,4832698925,11912127182,2886242499,7013442087,3356296426,2517172738,9898603246,2349629961,4362076899,1745650415,15401525447,6510391497,10502836428,14563148832,14328013038,12046074062,7180977380,2215150286,4530176747,15402041543,12448761546,9294877390,14663292452,5972730604,9126842375,11509735655,369373389,6643852810,12549661223,9496203790,1275658465,6241420005,5100577487,9966227692,4530405417,671687171,11912381167,15670452934,4597556964,4362896109,8590467265,8757752364,5067276841,15603376327,15938887910,9463148773,10804822055,4496327209,13086826538,15234572811,2751763139,10335072810,3993272333,12616511523,9664504526,7013205508,5067276836,9295167531,10267984424,8657310253,1980277262,1208778799,10905493551,10402435626,9361695425,12616499911,13992763431,1979978469,235413705,5704286253,2953356804,10469019850,14294491182,9462882305,12012814891,12617057834,15033245738,11475943458,8623563307,10871964193,9328394785,9731375648,10872205834,9832038955,15200755722,14092936202,9664013322,3020235273,14764285962,10469028359,2282029579,14495581738,14663331884,11375542305,15300866093,11643453474,8791298094,168362019,10570207758,15401821194,11475681793,13724025377,13522764842,15938691073,235450383,4328587275,9966227470,10234409006,10234662927,9194221582,13052740640,14059372587,4463075331,9295177226,8824853006,2416255523,13657252896,9932943374,15133115936,8758006286,9160659502,9261322279,15200485930,9697566721,11107042304,6107766795,8925806634,11442106915,10335625216,1879901218,9362255886,11609870371,9664246316,8791830574,6442780202,9328431630,4832698925,2517172738,14563148832,9126842375,6643852810,9496203790,671687171,8757752364,13086826538,15234572811,10335072810,12616511523,7013205508,9295167531,10267984424,10402435626,9462882305},
{4932567555,1778680551,12515807757,6241681605,12549661223,738756326,1174438086,8925806634,805348038,302256647,235151393,11307884589,705232930,872452647,2886279394,6476567239,10871964355,13791430342,15871313602,100700685,3288895681,8758296611,6476538565,6644314823,15838019587,772284643,6811854574,6778036941,15905095727,4899283690,7348953615,14865170987,5369504960,335578308,15401558053,13556286181,11342258923,6778323467,11408834595,10301513734,11107053764,3792179207,3993272333,13086818531,5939430950,13623636519,3826024967,2416476710,2953642691,15871782951,11475628773,13556818125,13959443143,14026350597,11476205795,5469700099,13724057797,15367942349,15066014440,5033230883,9496465421,5067309603,15737594565,11307909123,10804822055,4832658630,7080280615,3859329217,9463203562,15200714759,13052715525,7348486851,10502840359,7080280807,15938691117,3926164167,6543179811,12079608325,2516619457,7348715559,12684402733,7147631343,5033722055,13522469391,11039421165,2751770817,6979388099,8892461261,2953613345,6073647655,14294253795,5033988321,268731078,9463148773,10872193255,5737850406,14193557543,134251526,5369008129,14563213315,671391790,11409096898,6006538247,11878834725,13053264395,4597253318,7113605155,2449781473,1611440647,15637184711,6510412291,14227112461,12113215715,4362908193,11610370053,671687171,11274321935,5067276327,1174966465,11106550273,13790916645,14563451085,13153370671,33596142,5201244865,10905748015,6509895909,12348306157,11476434959,12583477285,6677365475,1879085249,3389563950,8859168805,6140506817,15166636749,8724164805,11945947169,14227649025,329442,10468995589,4497117697,302057154,4396230338,13958938637,7348990177,1074541286,3892379842,10502549733,8657371138,13958946861,3758665729,10469049898,5469671649,2416739015,10033049797,6711194305,13590102253,14059372587,7281357537,9831723525,3859059425,10402410725,5939433665,13422592741,11643453474,1409856225,13120320205,15603106309,34144994,7080022017,2148073986,13455340077,13455622893,2517172738,8725013197,10939544103,15066505933,10335617731,14160507406,13187429093,4463305222,11811193069,13120320197,14496313383,6174835430,12348097259,15166898725,14797778983,14496051917,15166898885,15166878247,15871804101,11979522059,15570055373,12046598181,34342,570721478,302264548,11375510061,4932567555,8925806634,235151393,705232930,872452647,100700685,8758296611,15838019587,15401558053,6778323467,11408834595,14026350597,5469700099,5033230883,9496465421,5067309603,11307909123,15938691117,6543179811,14563213315,13053264395,7113605155,6510412291,671687171,13790916645,8657371138,10469049898,14059372587,11643453474,2148073986,2517172738,11979522059},
{41473540097,22901826048,6543508161,21575640064,38957220545,3549560832,58166755344,6610380304,40500462081,40349466625,22683330048,722100752,33833688,1342464016,1376125440,4093779976,4631306432,21744190464,24378212352,35517784592,36015833088,38806889473,41053988360,53251432664,57663694336,57730933248,537797321,41708307480,58838230536,4613751312,36658898448,57630015504,5368971785,6677619728,1157915160,6643794968,17851097088,41030386881,41691652617,55114081280,54761251344,21106394120,58703872008,5503059464,41423093785,58334667281,1845756097,5378416840,23857882128,39796221120,40316068568,57780863488,35107905728,40802869776,24259878937,41686540288,58367943169,23085868752,40047501529,52664361497,58100162752,39980630528,57025906192,7318282752,56204066824,38185075720,23220592649,6706167808,58569261056,7063740417,41507104256,40500864704,23371580416,5085208585,587473417,58653540864,58687365633,7190619137,58670859792,3942794768,6543647232,5133960712,41423478785,7096771080,22247253504,1678403288,3607363585,20669933056,5671364104,6493307072,6291472408,7332446745,22918225936,58335168000,4093903361,23220060160,40634966040,56019797528,23891436560,6257910784,54375506449,6778274328,24495653384,40804155913,4362215424,56103797256,58888307216,2164401664,37171634176,58133970952,41691512833,6815097032,4458283008,39480467976,23639238144,41726517761,251937497,4983759880,58033053696,54291342848,7147365888,41154781185,24411120648,38739133464,37749024272,4496582352,57210979856,3372491776,24226963976,41574089240,58217350680,7248552456,1963082776,39762403849,22633129480,58872045569,3373015552,57026151432,40416329744,7214350872,35785810625,37581497864,56925511696,54963085504,58066468864,5754847936,18052687360,41557443088,3943055896,41490718720,23857750040,20485373960,58804929025,36519544513,6806831616,6292128280,58133856273,56522465296,40081694720,58335166472,58720780993,58268067336,24327644176,41524264968,6442590216,24008213528,38856959176,58855391752,58804404737,3960103448,58787784216,36608163856,1745354752,23337263632,23857751064,58284065297,41183354880,41624560656,7214751960,55298385424,38956843537,17179885584,40282907353,1996898320,58854490137,6778930192,58603602432,55918723585,34611675865,24495267849,5469512192,40399806976,58552762385,57630024216,35243958976,18539234320,2433359880,40114594312,38503866897,57580347929,21508792328,57998976512,38822888664,35074351808,57194447880,7181460504,41507119833,6794912776,37800001552,35135169216,40853045440,637690384,54995723776,40014471697,58015892177,54023184593,6190950104,58619748880,38991176200,54777635352,22247253000,58804405256,1426458112,34597773504,37614796817,40350785544,38504374464,54760833024,53872034824,58183918273,41607898632,39209279489,34673533120,705308168,57697107968,56992858120,54862176793,57982074897,23454557696,54593602776,38285632209,40064671769,7265191104,34732253376,41524133888,56019664912,22616612872,58770612953,22616343040,56992884248,41678021120,34740641984,2198210056,39527530513,55600095744,35148267721,22398238728,58787915288,53838217729,18538825224,23823918088,34933579968,7180781248,57797912072,40349753368,39192511192,58049167553,36541456912,56690352136,40552104128,57009398288,22330613768,53771772936,55901815816,5737832472,56103928832,58872054489,57998836417,38252069592,57797526032,55298113561,23387841544,238035136,1744962560,24394727944,56724422849,6543515649,39342972929,38235807752,53972590801,57579422225,313794752,57731064320,2232181784,24327497416,40823038153,39829790745,56741093392,41054257169,57697002000,372515008,6807093248,58536239105,55683580424,56086495233,58133069841,37715854872,41692168705,22381469704,58519586312,19613368336,380903616,40467325465,41490859728,6594102984,57478768144,56573567168,54912483841,58150363145,37917312728,3373163024,57580340240,235283464,573841600,55835123729,23807016976,57579798528,17498908168,58872037569,4831993872,39577862161,57243861192,38956711952,39393838808,58166755344,6610380304,722100752,1342464016,35517784592,41708307480,4613751312,36658898448,57630015504,6677619728,1157915160,6643794968,54761251344,41423093785,58334667281,23857882128,40802869776,24259878937,52664361497,57025906192,58670859792,3942794768,6291472408,7332446745,22918225936,40634966040,56019797528,23891436560,54375506449,6778274328,58888307216,38739133464,37749024272,57210979856,41574089240,58217350680,1963082776,40416329744,7214350872,56925511696,41557443088,3943055896,23857750040,6292128280,58133856273,56522465296,24327644176,24008213528,3960103448,58787784216,36608163856,23337263632,23857751064,58284065297,41624560656,55298385424,38956843537,17179885584,1996898320,58854490137,6778930192,58552762385,57630024216,18539234320,38503866897,57580347929,7181460504,37800001552,637690384,40014471697,58619748880,54777635352,37614796817,54862176793,57982074897,40064671769,56019664912,56992884248,39527530513,58787915288,40349753368,36541456912,57009398288,5737832472,57797526032,55298113561,57579422225,2232181784,39829790745,56741093392,41054257169,57697002000,58133069841,37715854872,19613368336,40467325465,57478768144,3373163024,57580340240,55835123729,23807016976,4831993872,39577862161,38956711952},
{2701394113,314180289,7249601537,3284992,277873352,6471549440,5386543624,3968869056,38168453649,1654129152,2688426505,3566347265,6756238529,1420961473,4860150977,6705905664,6706167808,6806306816,6806831616,4575199232,6807093248,7140540928,5699281088,1141907969,6192365760,6494093504,6594757312,6596854464,6728974336,6729498624,6773023232,7049715905,6997672640,7066354176,7131365568,7200834048,7226261504,7276864192,7316701696,7335059456,7366246912,615522497,4591714304,4623696384,4892131840,4894228480,5111808512,2215649472,5128855552,5680792064,5437268160,5682364608,6485573824,6603809280,6512846337,6664618496,6687424512,6688997376,6745620480,6771048960,6821380608,7008027136,7056523264,7140418048,7167148032,7249199104,7276986560,5387322056,5388632768,5109719232,5411700736,5422187016,6991511552,5428740096,4156031488,5436866560,5436866760,5454438408,7173710016,5464399880,7175807168,5487461056,5110366400,5915157705,5503975944,5523382464,4625932288,5554577600,4892271104,5562704064,6203252928,6712074241,5571084488,6236021248,5579997184,4590010880,5590221320,5671747784,6795952128,5689049800,7031103680,5707662024,5713952776,5724176384,5724185096,5730992128,5766381760,278012929,5926297608,5965881352,5991047176,5998911688,6057886400,5707539457,6058410688,6066283008,6084886536,6110052872,6117130248,6119227904,2226136769,6169042944,6192366088,6192628424,6200230600,6200763080,6225920520,6242697216,6244532416,6261055680,6292504776,6292775112,3759154177,6295126216,6453985800,6487802376,6512706248,6578504200,6603415752,6628573384,6647186120,6656098824,6118449664,6670516232,6672622280,6705644040,6745751560,6764888776,6779568648,6795829256,6980370440,7000031432,7033323720,7064789000,7073169608,607265280,7089946824,730857473,7106470600,739770881,7133724872,748167360,884220608,7165182664,1217536192,7184057032,1622155969,7184580616,7209493000,1739727553,7276593864,2830770880,7343964872,4975501825,7359955144,7368605896,4298113216,4304404680,4312793088,4348444864,4354744520,4416086720,4447805640,4458283008,4483187392,4491051200,4524876480,4525392392,4558684872,4566811144,4598268424,4623180296,4648337408,4649123840,4657250304,4674028032,4684251840,4835246600,4849934336,4852031680,4852285960,4885586624,4893704904,4933550600,4952424960,4952956936,4958724104,4967366856,4986249928,4992270536,4994900160,5000659136,5017444360,5017698824,5017968832,5053882568,5118099464,5118108360,5120729608,5120991240,5127020552,5187829760,5194130112,38168453649}};
uint64_t pbmtypeinstr[PBMTYPES+1][TUNEDSTRATEGIES]=
{{3105826598,37269919176,3105826598,12423306392,3105826598,3105826598,3105826598,6211653196,3105826598,3105826598,18634959588,3105826598,3105826598,21740786186,9317479794,9317479794,3105826598,9317479794,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,65222358558,3105826598,3105826598,3105826598,3105826598,9317479794,3105826598,3105826598,3105826598,3105826598,27952439382,6211653196,3105826598,3105826598,24846612784,74486869390,12423306392,37269919176,6211653196,6211653196,9317479794,9317479794,15529132990,6211653196,6211653196,9317479794,3105826598,9317479794,6211653196,18634959588,65222358558,3105826598,6211653196,9317479794,9317479794,12423306392,12423306392,12423306392,15529132990,12423306392,71434011754,12423306392,9317479794,15529132990,21740786186,12423306392,59010705362,12423306392,21740786186,18634959588,24846612784,18634959588,21740786186,21740786186,18634959588,21740786186,31058265980,34164092578,18634959588,37269919176,31058265980,71434011754,31058265980,34164092578,34164092578,52799052166,46587398970,65222358558,62116531960,71434011754,34164092578,34164092578,71434011754,71434011754,74486869390,3105826598,3105826598,3105826598,3105826598,3105826598,6211653196,3105826598,3105826598,3105826598,3105826598,6211653196,3105826598,3105826598,49693225568,12423306392,6211653196,3105826598,6211653196,3105826598,3105826598,3105826598,6211653196,3105826598,3105826598,9317479794,6211653196,9317479794,3105826598,3105826598,3105826598,49693225568,3105826598,3105826598,9317479794,18634959588,49693225568,3105826598,3105826598,3105826598,3105826598,9317479794,3105826598,6211653196,3105826598,6211653196,3105826598,3105826598,12423306392,3105826598,3105826598,24846612784,6211653196,6211653196,6211653196,9317479794,3105826598,15529132990,15529132990,24846612784,3105826598,6211653196,43481572372,6211653196,3105826598,27952439382,6211653196,3105826598,15529132990,3105826598,3105826598,74486869390,3105826598,9317479794,27952439382,3105826598,6211653196,18634959588,9317479794,3105826598,3105826598,9317479794,9317479794,3105826598,3105826598,18634959588,3105826598,3105826598,6211653196,3105826598,6211653196,3105826598,3105826598,3105826598,15529132990,3105826598,9317479794,6211653196,43481572372,3105826598,3105826598,3105826598,65222358558,6211653196,9317479794,3105826598,24846612784,40375745774,15529132990,3105826598,34164092578,6211653196,3105826598,3105826598,24846612784,9317479794,6211653196,18634959588,49693225568,6211653196,3105826598,31058265980,6211653196,9317479794,43481572372,12423306392,6211653196,3105826598,3105826598,3105826598,24846612784,3105826598,3105826598,12423306392,3105826598,3105826598,3105826598,6211653196,9317479794,3105826598,21740786186,15529132990,3105826598,24846612784,9317479794,3105826598,3105826598,3105826598,21740786186,74486869390,43481572372,18634959588,12423306392,46587398970,3105826598,3105826598,12423306392,3105826598,12423306392,12423306392,12423306392,6211653196,9317479794,3105826598,59010705362,3105826598,9317479794,3105826598,71434011754,43481572372,6211653196,40375745774,6211653196,9317479794,74486869390,15529132990,3105826598,3105826598,6211653196,24846612784,15529132990,15529132990,9317479794,6211653196,3105826598,6211653196,12423306392,65222358558,3105826598,9317479794,3105826598,59010705362,15529132990,43481572372,12423306392,27952439382,74486869390,6211653196,15529132990,6211653196,9317479794,9317479794,9317479794,34164092578,3105826598,31058265980,12423306392,15529132990,74486869390,3105826598,12423306392,6211653196,6211653196,9317479794,12423306392,6211653196,68328185156,6211653196,49693225568,18634959588,9317479794,15529132990,6211653196,43481572372,43481572372,3105826598,12423306392,15529132990,27952439382,9317479794,15529132990,12423306392,9317479794,31058265980,3105826598,21740786186,27952439382,46587398970,43481572372,52799052166,3105826598,12423306392,40375745774,18634959588,21740786186,3105826598,3105826598,6211653196,3105826598,15529132990,24846612784,24846612784,15529132990,3105826598,3105826598,18634959588,46587398970,21740786186,3105826598,12423306392,52799052166,27952439382,18634959588,6211653196,49693225568,18634959588,12423306392,3105826598,37269919176,18634959588,3105826598,12423306392,9317479794,3105826598,52799052166,27952439382,9317479794,6211653196,27952439382,27952439382,62116531960,12423306392,9317479794,71434011754,43481572372,37269919176,3105826598,15529132990,74486869390,3105826598,43481572372},
{31058265980,3105826598,3105826598,3105826598,3105826598,3105826598,21740786186,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,6211653196,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,6211653196,6211653196,15529132990,3105826598,46587398970,6211653196,24846612784,6211653196,6211653196,6211653196,6211653196,6211653196,6211653196,3105826598,6211653196,74486929404,9317479794,3105826598,3105826598,21740786186,6211653196,15529132990,9317479794,74486929404,18634959588,15529132990,12423306392,74486929404,15529132990,31058265980,15529132990,52799052166,15529132990,18634959588,9317479794,71434011754,9317479794,15529132990,40375745774,74486929404,65222358558,12423306392,74486929404,18634959588,65222358558,65222358558,18634959588,34164092578,59010705362,37269919176,40375745774,71434011754,65222358558,31058265980,52799052166,65222358558,59010705362,71434011754,68328185156,43481572372,52799052166,46587398970,3105826598,71434011754,3105826598,3105826598,3105826598,6211653196,12423306392,6211653196,3105826598,3105826598,62116531960,12423306392,6211653196,37269919176,9317479794,74486929404,12423306392,3105826598,3105826598,12423306392,31058265980,3105826598,3105826598,15529132990,6211653196,3105826598,6211653196,9317479794,9317479794,3105826598,3105826598,24846612784,62116531960,15529132990,40375745774,18634959588,40375745774,6211653196,49693225568,9317479794,34164092578,55904878764,15529132990,46587398970,9317479794,71434011754,21740786186,6211653196,3105826598,43481572372,3105826598,12423306392,74486929404,27952439382,6211653196,18634959588,18634959588,15529132990,9317479794,27952439382,74486929404,31058265980,49693225568,46587398970,74486929404,68328185156,74486929404,68328185156,9317479794,24846612784,71434011754,15529132990,24846612784,49693225568,3105826598,74486929404,74486929404,6211653196,55904878764,18634959588,3105826598,15529132990,34164092578,74486929404,74486929404,12423306392,37269919176,59010705362,21740786186,52799052166,46587398970,3105826598,37269919176,46587398970,21740786186,37269919176,31058265980,18634959588,65222358558,34164092578,3105826598,21740786186,15529132990,43481572372,43481572372,31058265980,74486929404,49693225568,74486929404,62116531960,37269919176,15529132990,27952439382,68328185156,49693225568,68328185156,71434011754,3105826598,43481572372,27952439382,31058265980,24846612784,62116531960,74486929404,74486929404,59010705362,74486929404,49693225568,37269919176,12423306392,46587398970,62116531960,31058265980,40375745774,71434011754},
{3105826598,3105826598,3105826598,6211653196,27952439382,27952439382,3105826598,12423306392,3105826598,21740786186,9317479794,24846612784,3105826598,6211653196,18634959588,3105826598,3105826598,3105826598,3105826598,3105826598,6211653196,6211653196,3105826598,12423306392,3105826598,3105826598,3105826598,3105826598,24846612784,3105826598,3105826598,3105826598,6211653196,3105826598,6211653196,34164092578,9317479794,3105826598,9317479794,31058265980,3105826598,6211653196,34164092578,6211653196,37243895254,6211653196,6211653196,12423306392,6211653196,6211653196,18634959588,6211653196,6211653196,12423306392,6211653196,6211653196,6211653196,6211653196,6211653196,6211653196,3105826598,6211653196,3105826598,6211653196,3105826598,6211653196,3105826598,6211653196,3105826598,6211653196,3105826598,6211653196,3105826598,6211653196,3105826598,6211653196,3105826598,6211653196,3105826598,12423306392,3105826598,3105826598,6211653196,3105826598,6211653196,15529132990,6211653196,3105826598,9317479794,6211653196,3105826598,6211653196,3105826598,6211653196,9317479794,9317479794,9317479794,9317479794,9317479794,3105826598,15529132990,15529132990,3105826598,3105826598,3105826598,9317479794,3105826598,6211653196,9317479794,9317479794,9317479794,12423306392,9317479794,9317479794,9317479794,9317479794,9317479794,3105826598,9317479794,3105826598,9317479794,3105826598,9317479794,3105826598,9317479794,3105826598,9317479794,3105826598,9317479794,3105826598,9317479794,3105826598,9317479794,9317479794,9317479794,9317479794,9317479794,3105826598,9317479794,3105826598,9317479794,6211653196,18634959588,6211653196,9317479794,9317479794,6211653196,9317479794,9317479794,6211653196,15529132990,15529132990,21740786186,34164092578,24846612784,12423306392,12423306392,12423306392,12423306392,3105826598,12423306392,9317479794,12423306392,12423306392,12423306392,3105826598,12423306392,6211653196,12423306392,12423306392,9317479794,12423306392,12423306392,12423306392,12423306392,12423306392,12423306392,12423306392,12423306392,12423306392,3105826598,12423306392,3105826598,12423306392,3105826598,15529132990,15529132990,15529132990,18634959588,15529132990,3105826598,6211653196,15529132990,15529132990,3105826598,15529132990,24846612784,15529132990,6211653196,15529132990,15529132990,3105826598,15529132990,3105826598,15529132990,3105826598,15529132990,3105826598,6211653196,15529132990,15529132990,3105826598,15529132990,3105826598,34164092578,3105826598,18634959588,18634959588,18634959588,18634959588,18634959588,3105826598,34164092578,3105826598,3105826598,34164092578,6211653196,3105826598,18634959588,3105826598,18634959588,6211653196,18634959588,18634959588,18634959588,3105826598,18634959588,3105826598,18634959588,3105826598,21740786186,21740786186,21740786186,3105826598,3105826598,21740786186,3105826598,21740786186,3105826598,21740786186,3105826598,24846612784,31058265980,31058265980,24846612784,24846612784,24846612784,24846612784,3105826598,24846612784,6211653196,24846612784,27952439382,27952439382,27952439382,31058265980,31058265980,27952439382,27952439382,27952439382,6211653196,27952439382,27952439382,3105826598,27952439382,3105826598,27952439382,6211653196,27952439382,27952439382,3105826598,27952439382,3105826598,27952439382,37243895254,31058265980,31058265980,31058265980,31058265980,37243895254,31058265980,31058265980,31058265980,3105826598,34164092578,3105826598,34164092578,3105826598,34164092578,3105826598,34164092578,6211653196,34164092578,34164092578,37243895254,6211653196,37243895254,37243895254,18634959588,18634959588,18634959588,15529132990,15529132990,12423306392,12423306392,12423306392,12423306392,12423306392,12423306392,12423306392,12423306392},
{3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,9317479794,3105826598,3105826598,6211653196,3105826598,3105826598,6211653196,3105826598,9317479794,6211653196,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,18634959588,15529132990,12423306392,12423306392,24846612784,21740786186,21740786186,21740786186,37243072491},
{18634959588,6211653196,3105826598,3105826598,12423306392,24846612784,3105826598,3105826598,74486669327,3105826598,3105826598,3105826598,3105826598,12423306392,6211653196,3105826598,9317479794,3105826598,3105826598,27952439382,3105826598,3105826598,3105826598,15529132990,6211653196,6211653196,6211653196,3105826598,3105826598,3105826598,12423306392,3105826598,3105826598,6211653196,3105826598,3105826598,3105826598,6211653196,9317479794,3105826598,3105826598,3105826598,3105826598,3105826598,12423306392,3105826598,3105826598,3105826598,3105826598,3105826598,3105826598,74486669327,55904878764,18634959588,3105826598,3105826598,6211653196,6211653196,9317479794,3105826598,6211653196,6211653196,40375745774,6211653196,6211653196,6211653196,6211653196,6211653196,6211653196,6211653196,9317479794,12423306392,12423306392,24846612784,6211653196,6211653196,6211653196,62116531960,12423306392,6211653196,15529132990,71434011754,55904878764,24846612784,24846612784,21740786186,21740786186,12423306392,52799052166,24846612784,15529132990,71434011754,34164092578,65222358558,31058265980,46587398970,46587398970,52799052166,62116531960,71434011754,59010705362,59010705362,3105826598,3105826598,3105826598,21740786186,3105826598,6211653196,3105826598,3105826598,3105826598,6211653196,27952439382,3105826598,3105826598,6211653196,3105826598,3105826598,6211653196,68328185156,3105826598,12423306392,3105826598,3105826598,3105826598,3105826598,6211653196,15529132990,9317479794,3105826598,6211653196,6211653196,6211653196,3105826598,3105826598,27952439382,3105826598,3105826598,9317479794,21740786186,3105826598,3105826598,6211653196,3105826598,34164092578,6211653196,3105826598,3105826598,12423306392,3105826598,62116531960,3105826598,3105826598,6211653196,3105826598,31058265980,9317479794,9317479794,21740786186,43481572372,3105826598,6211653196,3105826598,3105826598,68328185156,3105826598,3105826598,3105826598,6211653196,3105826598,24846612784,3105826598,6211653196,9317479794,6211653196,9317479794,9317479794,6211653196,12423306392,21740786186,9317479794,3105826598,9317479794,9317479794,3105826598,9317479794,15529132990,3105826598,9317479794,6211653196,3105826598,9317479794,9317479794,3105826598,12423306392,3105826598,9317479794,31058265980,12423306392,21740786186,3105826598,27952439382,6211653196,6211653196,3105826598,24846612784,9317479794,34164092578,12423306392,9317479794,31058265980,3105826598,3105826598,31058265980,3105826598,21740786186,6211653196,3105826598,3105826598,24846612784,3105826598,37269919176,9317479794,74486669327,9317479794,3105826598,27952439382,6211653196,3105826598,3105826598,9317479794,49693225568,15529132990,3105826598,18634959588,3105826598,3105826598,15529132990,6211653196,12423306392,9317479794,3105826598,34164092578,24846612784,9317479794,9317479794,31058265980,6211653196,3105826598,34164092578,3105826598,9317479794,12423306392,12423306392,3105826598,18634959588,15529132990,15529132990,34164092578,12423306392,6211653196,9317479794,6211653196,18634959588,6211653196,21740786186,9317479794,31058265980,3105826598,31058265980,9317479794,24846612784,52799052166,3105826598,6211653196,3105826598,9317479794,3105826598,71434011754,9317479794,3105826598,3105826598,31058265980,9317479794,3105826598,6211653196,18634959588,15529132990,55904878764,15529132990,3105826598,9317479794,3105826598,6211653196,6211653196,21740786186,6211653196,6211653196,40375745774,3105826598,43481572372,46587398970,21740786186,3105826598,6211653196,18634959588,15529132990,43481572372,18634959588,6211653196,6211653196,12423306392,9317479794,37269919176,6211653196,9317479794,34164092578,3105826598,6211653196,24846612784,9317479794,27952439382,3105826598,3105826598,65222358558,34164092578,6211653196,9317479794,74486669327,43481572372,18634959588,3105826598,15529132990,9317479794,3105826598,12423306392,3105826598,46587398970,21740786186,40375745774,3105826598,3105826598,34164092578,3105826598,9317479794,18634959588,12423306392,6211653196,9317479794,9317479794,3105826598,9317479794,31058265980,3105826598,52799052166,68328185156,31058265980,9317479794,12423306392,12423306392,27952439382,12423306392,3105826598,9317479794,15529132990,37269919176,74486669327,3105826598,9317479794,12423306392,9317479794,9317479794,18634959588,27952439382,40375745774,21740786186,3105826598,37269919176,37269919176,74486669327,3105826598,3105826598,9317479794,9317479794,9317479794,18634959588,34164092578,3105826598,24846612784,6211653196,3105826598,6211653196,9317479794,71434011754,37269919176,62116531960,9317479794,9317479794,24846612784,24846612784,31058265980,27952439382,21740786186,9317479794,3105826598,3105826598,27952439382,6211653196,62116531960,18634959588,34164092578,74486669327},
{3105826598,3105826598,6211653196,3105826598,6211653196,9317479794,6211653196,3105826598,15529132990,6211653196,9317479794,3105826598,3105826598,9317479794,6211653196,12423306392,12423306392,15529132990,6211653196,15529132990,6211653196,15529132990,15529132990,15529132990,15529132990,15529132990,15529132990,15529132990,24846612784,15529132990,24846612784,15529132990,15529132990,27952439382,15529132990,6211653196,15529132990,18634959588,34164092578,18634959588,18634959588,12423306392,21740786186,21740786186,24846612784,15529132990,21740786186,15529132990,21740786186,15529132990,21740786186,15529132990,15529132990,34164092578,15529132990,34164092578,15529132990,37269919176,15529132990,15529132990,24846612784,15529132990,15529132990,27952439382,15529132990,31058265980,31058265980,18634959588,18634959588,18634959588,18634959588,18634959588,21740786186,21740786186,21740786186,21740786186,9317479794,27952439382,9317479794,15529132990,15529132990,21740786186,21740786186,21740786186,21740786186,21740786186,21740786186,21740786186,21740786186,24846612784,24846612784,24846612784,24846612784,24846612784,27952439382,31058265980,31058265980,31058265980,31058265980,34164092578,34164092578,34164092578,37269919176},
{3105826598,21740786186,24846612784,3105826598,3105826598,37269919176,34164092578,3105826598,3105826598,3105826598,37269919176,3105826598,3105826598,9317479794,3105826598,3105826598,37269919176,3105826598,3105826598,3105826598,31058265980,6211653196,18634959588,3105826598,9317479794,3105826598,31058265980,9317479794,12423306392,6211653196,6211653196,3105826598,3105826598,3105826598,3105826598,3105826598,18634959588,12423306392,9317479794,9317479794,27952439382,12423306392,31058265980,37269919176,31058265980,31058265980,34164092578,37269919176,3105826598,31058265980,3105826598,3105826598,18634959588,3105826598,3105826598,3105826598,3105826598,18634959588,3105826598,3105826598,3105826598,3105826598,6211653196,15529132990,3105826598,24846612784,18634959588,3105826598,3105826598,3105826598,37269919176,12423306392,3105826598,34164092578,37269919176,6211653196,3105826598,6211653196,6211653196,24846612784,3105826598,34164092578,3105826598,24846612784,18634959588,37269919176,37269919176,3105826598,12423306392,3105826598,3105826598,34164092578,3105826598,21740786186,34164092578,37269919176,3105826598,3105826598,34164092578,6211653196,34164092578,3105826598,3105826598,37269919176,34164092578,6211653196,3105826598,12423306392,37269919176,6211653196,12423306392,6211653196,24846612784,21740786186,21740786186,37269919176,37269919176,6211653196,24846612784,21740786186,9317479794,27952439382,24846612784,21740786186,37269919176,3105826598,9317479794,3105826598,6211653196,3105826598,31058265980,3105826598,34164092578,6211653196,6211653196,3105826598,27952439382,12423306392,34164092578,3105826598,9317479794,6211653196,37269919176,3105826598,9317479794,31058265980,3105826598,12423306392,9317479794,18634959588,6211653196,31058265980,3105826598,3105826598,34164092578,31058265980,9317479794,31058265980,3105826598,21740786186,34164092578,31058265980,3105826598,6211653196,6211653196,18634959588,31058265980,9317479794,34164092578,27952439382,24846612784,9317479794,31058265980,31058265980,9317479794,12423306392,21740786186,31058265980,18634959588,6211653196,9317479794,31058265980,9317479794,18634959588,34164092578,6211653196,15529132990,6211653196,6211653196,31058265980,34164092578,6211653196,24846612784,37269919176,27952439382,21740786186,6211653196,31058265980,34164092578,6211653196,31058265980,12423306392,9317479794,24846612784,6211653196,31058265980,31058265980,12423306392,18634959588,34164092578,12423306392,12423306392,15529132990,31058265980,21740786186,6211653196,12423306392,6211653196,31058265980,31058265980,9317479794,3105826598,37269919176,31058265980,21740786186,6211653196,27952439382,12423306392,9317479794,31058265980,12423306392,12423306392,31058265980,31058265980,24846612784,12423306392,9317479794,31058265980,12423306392,31058265980,15529132990,37269919176,34164092578,34164092578,21740786186,31058265980,37269919176,31058265980,15529132990,3105826598,31058265980,37269919176,27952439382,31058265980,12423306392,15529132990,9317479794,6211653196,34164092578,6211653196,31058265980,12423306392,15529132990,27952439382,18634959588,15529132990,31058265980,12423306392,6211653196,31058265980,18634959588,18634959588,31058265980,31058265980,18634959588,31058265980,34164092578,9317479794,31058265980,21740786186,9317479794,21740786186,12423306392,9317479794,18634959588,31058265980,21740786186,21740786186,12423306392,31058265980,24846612784,31058265980,18634959588,18634959588,6211653196,21740786186,34164092578,12423306392,34164092578,18634959588,31058265980,18634959588,18634959588,12423306392,6211653196,21740786186,27952439382,34164092578,31058265980,24846612784,27952439382,21740786186,31058265980,27952439382,27952439382,37269919176,21740786186,24846612784,31058265980,6211653196,31058265980,27952439382,31058265980,21740786186,6211653196,6211653196,27952439382,21740786186,27952439382,27952439382,34164092578,31058265980,6211653196,6211653196,27952439382,24846612784,9317479794,34164092578,6211653196,34164092578,12423306392,37269919176,34164092578,6211653196,27952439382,31058265980,31058265980,12423306392,34164092578,27952439382,34164092578,9317479794,6211653196},
{9317479794,12423306392,3105826598,3105826598,3105826598,3105826598,9317479794,3105826598,3105826598,3105826598,3105826598,9317479794,3105826598,3105826598,3105826598,3105826598,49693225568,6211653196,6211653196,9317479794,18634959588,18634959588,49693225568,37269919176,68328185156,3105826598,3105826598,9317479794,3105826598,3105826598,9317479794,3105826598,3105826598,6211653196,6211653196,12423306392,3105826598,6211653196,27952439382,3105826598,12423306392,3105826598,9317479794,71434011754,9317479794,3105826598,18634959588,9317479794,12423306392,3105826598,65222358558,12423306392,3105826598,9317479794,18634959588,59010705362,21740786186,68328185156,24846612784,31058265980,9317479794,9317479794,21740786186,31058265980,37269919176,9317479794,21740786186,40375745774,15529132990,71434011754,21740786186,59010705362,52799052166,21740786186,15529132990,65222358558,59010705362,59010705362,21740786186,24846612784,34164092578,37269919176,74486229209,59010705362,68328185156,24846612784,65222358558,68328185156,24846612784,31058265980,71434011754,24846612784,34164092578,62116531960,37269919176,46587398970,62116531960,34164092578,27952439382,68328185156,34164092578,46587398970,62116531960,18634959588,71434011754,52799052166,59010705362,31058265980,46587398970,74486229209,59010705362,24846612784,71434011754,31058265980,74486229209,74486229209,59010705362,49693225568,34164092578,59010705362,12423306392,12423306392,68328185156,55904878764,52799052166,71434011754,55904878764,62116531960,71434011754,27952439382,65222358558,55904878764,74486229209,65222358558,37269919176,27952439382,55904878764,18634959588,68328185156,6211653196,59010705362,21740786186,6211653196,37269919176,21740786186,68328185156,40375745774,3105826598,68328185156,6211653196,21740786186,24846612784,9317479794,24846612784,31058265980,62116531960,55904878764,34164092578,9317479794,55904878764,55904878764,31058265980,68328185156,34164092578,27952439382,12423306392,74486229209,71434011754,27952439382,9317479794,52799052166,9317479794,74486229209,21740786186,9317479794,65222358558,6211653196,21740786186,9317479794,9317479794,43481572372,46587398970,24846612784,43481572372,6211653196,43481572372,9317479794,12423306392,3105826598,65222358558,12423306392,3105826598,12423306392,59010705362,68328185156,40375745774,9317479794,9317479794,74486229209,68328185156},
{35938850634,11979616878,1996602813,29949042195,1996602813,1996602813,3993205626,5989808439,5989808439,37935453447,7986411252,3993205626,1996602813,1996602813,31945645008,1996602813,1996602813,1996602813,7986411252,1996602813,1996602813,1996602813,1996602813,1996602813,1996602813,1996602813,5989808439,5989808439,31945645008,13976219691,3993205626,3993205626,13976219691,9983014065,5989808439,5989808439,5989808439,5989808439,5989808439,5989808439,7986411252,9983014065,11979616878,13976219691,13976219691,27952439382,31945645008,15972822504,27952439382,17969425317,17969425317,37935453447,19966028130,21962630943,23959233756,23959233756,23959233756,25955836569,25955836569,25955836569,25955836569,27952439382,29949042195,33942247821,35938850634,37935453447,1996602813,1996602813,1996602813,9983014065,3993205626,1996602813,7986411252,1996602813,1996602813,17969425317,1996602813,1996602813,33942247821,1996602813,9983014065,7986411252,37935453447,7986411252,1996602813,3993205626,1996602813,3993205626,19966028130,5989808439,11979616878,33942247821,9983014065,35938850634,1996602813,31945645008,11979616878,13976219691,1996602813,11979616878,15972822504,19966028130,21962630943,1996602813,3993205626,1996602813,37935453447,1996602813,1996602813,13976219691,1996602813,15972822504,1996602813,13976219691,13976219691,1996602813,33942247821,19966028130,11979616878,1996602813,11979616878,33942247821,21962630943,3993205626,29949042195,31945645008,37935453447,5989808439,17969425317,5989808439,33942247821,13976219691,7986411252,33942247821,35938850634,35938850634,5989808439,31945645008,15972822504,37935453447,35938850634,11979616878,23959233756,23959233756,29949042195,7986411252,7986411252,35938850634,27952439382,5989808439,31945645008,23959233756,37935453447,7986411252,1996602813,29949042195,35938850634,11979616878,11979616878,37935453447,37935453447,11979616878,17969425317,5989808439,5989808439,27952439382,25955836569,9983014065,37935453447,9983014065,9983014065,5989808439,11979616878,15972822504,7986411252,31945645008,29949042195,27952439382,17969425317,27952439382,5989808439,15972822504,21962630943,13976219691,37935453447,37935453447,35938850634,33942247821,37935453447,25955836569,19966028130,23959233756,27952439382,37935453447,21962630943,19966028130,13976219691,13976219691,25955836569,15972822504,33942247821,37935453447,19966028130,21962630943,27952439382,13976219691,33942247821,21962630943,9983014065,21962630943,33942247821,5989808439,3993205626,33942247821,37935453447,35938850634,31945645008,33942247821,19966028130,31945645008,15972822504,19966028130,13976219691,23959233756,29949042195,11979616878,37935453447,9983014065,19966028130,37935453447,17969425317,37935453447,23959233756,13976219691,15972822504,11979616878,21962630943,23959233756,19966028130,9983014065,17969425317,33942247821,29949042195,23959233756,7986411252,35938850634,31945645008,35938850634,31945645008,37935453447,33942247821,23959233756,15972822504,27952439382,3993205626,33942247821,15972822504,19966028130,27952439382,35938850634,7986411252,23959233756,35938850634,37935453447,11979616878,27952439382,37935453447,3993205626,7986411252,11979616878,21962630943,23959233756,35938850634,21962630943,27952439382,13976219691,27952439382,31945645008,15972822504,35938850634,35938850634,33942247821,25955836569,19966028130,13976219691,13976219691,37935453447,3993205626,13976219691,33942247821,31945645008,25955836569,13976219691,35938850634,15972822504,7986411252,35938850634,13976219691,29949042195,19966028130,25955836569,1996602813,27952439382,35938850634,23959233756,35938850634,9983014065,35938850634,15972822504,35938850634,21962630943,25955836569,13976219691,11979616878,21962630943,23959233756,35938850634,37935453447,25955836569,15972822504,17969425317,31945645008,25955836569,25955836569,13976219691,15972822504,15972822504,21962630943,31945645008,15972822504,35938850634,35938850634,35938850634,15972822504},
{1996602813,1996602813,1996602813,1996602813,1996602813,33942247821,33942247821,23959233756,1996602813,1996602813,1996602813,1996602813,1996602813,1996602813,3993205626,33942247821,33942247821,33942247821,33942247821,35938850634,33942247821,33942247821,33942247821,3993205626,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,3993205626,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,3993205626,35938850634,35938850634,35938850634,35938850634,35938850634,1996602813,35938850634,35938850634,1996602813,35938850634,35938850634,35938850634,7986411252,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,33942247821,33942247821,5989808439,33942247821,33942247821,19966028130,33942247821,21962630943,33942247821,33942247821,33942247821,25955836569,33942247821,25955836569,33942247821,27952439382,3993205626,33942247821,33942247821,29949042195,33942247821,29949042195,33942247821,29949042195,7986411252,33942247821,29949042195,33942247821,31945645008,33942247821,33942247821,33942247821,33942247821,35938850634,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,5989808439,33942247821,33942247821,33942247821,33942247821,33942247821,5989808439,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,9983014065,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,9983014065,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,11979616878,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,33942247821,1996602813,33942247821,1996602813,33942247821,1996602813,33942247821,1996602813,1996602813,33942247821,1996602813,33942247821,3993205626,33942247821,33942247821,1996602813,33942247821,1996602813,33942247821,15972822504,33942247821,33942247821,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634,35938850634}};
int32_t numstrats[PBMTYPES+1][2]={{393,38},{219,15},{321,29},{50,15},{412,28},{103,9},{353,79},{200,32},{334,104},{223,1}};
uint64_t usrparam_mask=0;
uint64_t usrstrat=0;
uint64_t strategy=0xffffffffffffffff;
volatile sig_atomic_t signalflag=0;
int32_t alrmstatus=2;
#if (MEMCHECK == 1) || (MEMCHECK == 2)
uint64_t allcdmemory=0;
#endif
pid_t *pids=NULL;
uint64_t memorylimit=0xFFFFFFFFFFFFFFFF;
uint64_t initavmemory=0;
void *mmapptr=NULL;
char *logpath=NULL;
int32_t *fvidx=NULL;
uint64_t instrlimit=0xffffffffffffffff;
float foftmfactor=1.0;
float ueqtmfactor=1.08;
float usrtmfactor=1.0;
uint64_t perfmonrate=HWINSTRATE;
int32_t perfmon=1;
int32_t perfmonstate=0;
uint32_t usrseed=0;
#if defined(DEBUGCODE) && defined(VERBOSE)
int32_t verbfrom=0;
int32_t verbto=0x7fffffff;
#endif
#endif

/*----------------------------------------------------------------
 *
 *  Global variables shared with other modules that are not initialized here.
 *  WARNING: Don't use "static" as then a different variable will be created in
 *  each module with the same name.
 *
 *--------------------------------------------------------------*/
VARTYPE kbase kbset; /* Working KB. */
VARTYPE kbase mainkb; /* Main KB with axioms (theory) being analyzed. */
VARTYPE sbsctrstr subsctrl; /* Control structure for subsumption. */
VARTYPE int num_cores;  /* Number of physical cores available. */
VARTYPE int active_cores;  /* Number of enabled cores */
VARTYPE int32_t satcores; /* Number of cores that whii use complete strategies only. */
VARTYPE int32_t procnb; /* Internal process number of current process. It is in the range 0 to active_cores-2 */
                        /* for child processes and equals active_cores-1 for parent process. */
VARTYPE int proc_affinity; /* Core affinity option. */
VARTYPE uint64_t numskolem; /* Number of skolem functions defined in main KB so far. */
VARTYPE uint32_t skolemid; /* Number to identify unique skolem prefix. */
VARTYPE uint64_t numpdefs; /* Number of predicate definitions in main KB so far. */
VARTYPE uint32_t pdefid; /* Number to identify unique predicate definition prefix. */
VARTYPE uint64_t numfunc; /* Number of  function definitions so far. */
VARTYPE uint32_t functid; /* Number to identify unique function definitions prefix. */
VARTYPE int32_t splid; /* Number to identify unique P-predicate prefix. */
VARTYPE uint32_t numsymbols; /* Number of symbols for symbol weight assignment. */
VARTYPE uint32_t *predicateset; /* Pointer to subsumption pruning predicates multiset data. */
VARTYPE uint32_t numpreds; /* Number of predicates for subsumption pruning. */
VARTYPE uint32_t sstimestamp; /* "Timestamp" for subsumption pruning predicates multiset data. */
VARTYPE uint32_t maxarity; /* Maximum arity. */
VARTYPE uint32_t maxusecount; /* Maximum value of usecount field in hashchain symbol elements. */
VARTYPE hashchain *hashkeys[HASHVALUES]; /* Hash keys indexed by hash values of symbol names. */
VARTYPE hashchcmp *hashsplit[HASHVALUES]; /*Hash keys indexed by hash values of clause variants. */
VARTYPE uint32_t hashpos[MAXPOSITIONS]; /* Hash values for item positions. */
VARTYPE hashchain equkey; /* Key corresponding to equality literal. */
VARTYPE int32_t binpsize; /* Effective size of binprefix structure. */
VARTYPE int32_t satclsize; /* Effective size of satclause structure. */
#ifdef SEMANTICTAUTOLOGY
VARTYPE int32_t ttlgclsize; /* Effective size of ttlgclause structure. */
#endif
VARTYPE int32_t mxtrfsize; /* Maximum formula size in bytes for formulas added to discrimination trees. */
VARTYPE mpctl *procctl; /* Pointer to multi-processing control data to be placed in mmap area. */
VARTYPE double cpu_time; /* Process CPU time used in seconds. */
VARTYPE double *secprc_cpu_time; /* Pointer to total child processes CPU time used in seconds to be placed in mmap area. */
VARTYPE int32_t cpuavail; /* 1 if processor time is available, 0 otherwise. */
VARTYPE sem_t *procsem; /* Pointer to set of process synchronization semaphores to be placed in mmap area. */
VARTYPE stats glblstats; /* GLobal statistics. */
VARTYPE options glblopts; /* Global options. */
VARTYPE stats *pbmstats; /* Pointer to problem global statistics to be placed in mmap area. */
VARTYPE int32_t pbmflags; /* Flags for current problem. See definitions above. */
VARTYPE int32_t pbmtype; /* Problem type number. */
VARTYPE int32_t pgmmode; /* Problem mode: INTERACTIVE or NONINTERACTIVE. */
VARTYPE int32_t slctordrsize; /* Number of elements in slctordr set. */
VARTYPE int32_t selectqueues; /* Effective number of clause selection queues. There are selectqueues weight clause */
                              /* selection queues and the same number of age selection queues. */
VARTYPE int32_t *nextstrat; /* Pointer to index to next free strategy. */
VARTYPE int32_t *trvsatstr; /* Pointer to number of portfolio complete strategies traversed by processes */
                            /* that that search for satisfiability. */
VARTYPE int32_t *trvunsstr; /* Pointer to number of portfolio complete strategies traversed by processes */
                            /* that that search for unsatisfiability. */
VARTYPE int32_t numlrnrecs; /* Number of record elements in set pointed by learnrecord. */
VARTYPE uint64_t *maxprcmemory; /* Pointer to process memory limit in mmap region to be used for saturation processes. */
VARTYPE uint64_t maxmemory; /* Memory limit to be used for saturation processes. This equals to *maxprcmemory. */
VARTYPE uint64_t mmapsize; /* Size of mmap shared memory. */
VARTYPE int32_t lkhdcomplete; /* Completeness indicator of lookahead literal selections. See SelectType56Cpl() function */
                              /* global comments for details. */
VARTYPE cmprefix *frstglselect[2*SELECTQUEUES]; /* Pointers to first formula in queue for each clause selection queue, */
                                    /* each pointer is NULL if no formulas of that weight added yet. */
                                    /* Only passive clauses are linked to clause selection queues. */
VARTYPE cmprefix *lstglselect[2*SELECTQUEUES]; /* Pointers to last formula in queue for each clause selection queue, */
                                    /* each pointer is NULL if no formulas of that weight added yet. */
                                    /* Only passive clauses are linked to clause selection queues. */
VARTYPE cmprefix *frstselect[2*SELECTQUEUES][MAXWEIGHT]; /* Pointers to first formula in queue for each clause selection */
                                    /* queue and for each weight/age, each pointer is NULL if no formulas of that weight/age */
                                    /* added yet. Only passive clauses are linked to clause selection queues. */
VARTYPE cmprefix *lstselect[2*SELECTQUEUES][MAXWEIGHT]; /* Pointers to last formula in queue for each clause selection */
                                    /* queue and for each weight/age, each pointer is NULL if no formulas of that weight/age */
                                    /* added yet. Only passive clauses are linked to clause selection queues. */
VARTYPE hashchain *minterm; /* Minimal term in term ordering. */
VARTYPE wbparam wbdata; /* Weight and variable balance structure. */
VARTYPE int filedesc; /* File descriptor to read number of executed hardware instuctions. */
VARTYPE int32_t satcalltype; /* 0 if the call to SatSolver() is from GoSat(), 1 otherwise. */
VARTYPE cmprefix *ueqgoal; /* Pointer to goal of UEQ problem, NULL if goal doesn't exist. */
VARTYPE int32_t ueqgltype; /* Goal type for type 8 (UEQ) problems. Its possible values are: */
                           /* -1 if there is not a valid goal. */
                           /* 0 if there is only a goal and it is ground. */
                           /* 1 if there is only a goal and it is not ground. */
                           /* 2 if there are several goals and all of them are ground. */
                           /* 3 if there are several goals and all of them are not ground. */
                           /* 4 if there are several goals some of them ground, some of them not ground and the last one is ground. */
                           /* 5 if there are several goals some of them ground, some of them not ground and the last one is not ground. */
#ifdef DEBUGMEMORY
VARTYPE void *memptrs[DBGMEMELEMS]; /* Pointers to memory allocations that must be tracked when DEBUGMEMORY is defined. */
                            /* The actual size of this set must be adjusted for each case. */
VARTYPE int32_t memtrack[DBGMEMELEMS]; /* Status of each entry in memptrs[]. The actual size of this set must be equal to the */
                               /* size of memptrs[]. The content is: */
                               /* 0 -> Entry not used. */
                               /* 1 -> Entry for memory allocated but not yet freed. */
                               /* 2 -> Entry for memory allocated and freed. */
VARTYPE int32_t ndemods;               /* Number of entries used in the above sets. */
VARTYPE int32_t auxcounter;            /* Auxiliary counter. */
#endif
#ifdef DEBUGCODE
VARTYPE int32_t dbgnb; /* For setting appropriate breakpoint in Otter and Discount functions. */
#endif
#ifdef BENCHMARKING
VARTYPE double *bresults[3]; /* Pointers to benchmarking results to be placed in mmap area. */
VARTYPE int32_t *bncounter; /* Pointer to benchmarking counter to be placed in mmap area. */
VARTYPE char *benchfname; /* Benchmarking output file name. */
#endif

/*----------------------------------------------------------------
 *
 *  Platform independent functions common to all platform independent modules.
 *
 *--------------------------------------------------------------*/
/* Drodi module functions. */
int main(int argc,char *argv[]); /* Entry point. */
int32_t Initialize_pgm(void); /* Initialize program */
void Clean4Exit(void); /* Performs the needed cleaning before exit */
int32_t CheckAnswer(char *,char *); /* Confirm answer with user */
int32_t Create_KB(kbase *); /* Create a new KB */
int32_t Initialize_KB(kbase *); /* Initialize a KB */
void Delete_KB(kbase *); /* Delete a KB */
void Free_Formula(kbase *); /* Free formula and tree memory and initialize queue pointers */
void FreeInfFormulas(void); /* Free inferred formulas in main KB */
void Initialize_hash(void); /* Initialize symbol hash tables for names in input formulas */
void Initialize_hashcmp(void); /* Initialize symbol hash tables for component names */
int32_t Tell(char *,int32_t,int32_t); /* Add formula to main KB */
int32_t InputSentence(char **,int32_t *); /* Input formula lines */
int32_t CheckSyntax(char **,int32_t *,char **,nameschain *,int32_t); /* Check sentence syntax */
char *SkipBlanks(char *); /* Skips space characters */
int32_t ParseSentenceStart(char *,char **,nameschain *,int32_t); /* Get formula start type */
int32_t ParseVariableList(char *string,char **,nameschain *); /* Variable list parser */
int32_t AddNametoChain(char *,nameschain *,int32_t,int32_t); /* Add name to text formulas names chain */
int32_t ParseAtomic(char *,char **,nameschain *,int32_t); /* Atomic formula parser */
int32_t ParseName(char *,char **); /* Parses a variable, constant, function or predicate name */
int32_t ParseTerm(char *,char **,char **,nameschain *,int32_t *,int32_t); /* Parse term */
int32_t CheckDeclared(char *,nameschain *,int32_t,int32_t *,int32_t *); /* Check if name is locally declared */
int32_t Ask(char *); /* Include a negated conjecture in main KB */
void AddTxtFormula2KB(cmprefix *,int32_t,kbase *); /* Add text formula to a KB */
void ResetStats(stats *); /* Reset statistics structure */
void AddInferenceStats(stats *,stats *); /* Add inference statistics */
int32_t TPTPQuantifiers(char **,int32_t *); /* Set scope of quantifiers */
float FormatMemory(float,char **); /* Format memory in KB, MB and GB */
char *JumpOverQuotes(char *); /* Jump over a quoted block */
void CatchSignal1(int); /* Catch program exception signals. */
#if (MEMCHECK == 1) || (MEMCHECK == 2)
char *strdupmn(char *); /* Memory tracking version of strdup() function */
void *mallocmn(size_t); /* Allocate and track total memory */
void *reallocmn(void *,size_t); /* Reallocate and track total memory */
void freemn(void *); /* Free and track total memory */
#endif
uint64_t GetAvailableMemory(void); /* Get available memory */
int32_t Init_mmap(void); /* Initialize mmap common memory area */
void __attribute__((optimize("O0"))) GetPerfData(void); /* Perform performance benchmarking and display results */
#ifdef DEBUGMEMORY
void myfree(void *,char *,int); /* Performs memory debug tasks and calls free() function */
#endif
#ifdef MEMORYTUNNING
int32_t SymMalloc(uint64_t); /* Allocate additional memory for MAXMEMORY tuning */
#endif

/* clausify module functions. */
cmprefix *PreprocessFormula(cmprefix *,char **,int32_t *,int32_t); /* Text formula pre-processing */
int32_t PutBrackets(lkedfitem **ptopitem); /* Insert parenthesis according to precedence */
int32_t SafeAppend(char **,int32_t *,char *,int32_t); /* Append a string safely */
cmprefix *Convert2PreNNF(lkedfitem **,cmprefix *,char **,int32_t *); /* Perform pre-nnf conversion */
cmprefix *Convert2NNF(lkedfitem **,cmprefix *,char **,int32_t *); /* Perform nnf conversion */
void DropImplication(lkedfitem *); /* Translate an implication */
void DropRevImplication(lkedfitem *); /* Translate a reverse implication */
lkedfitem *DropEquivalence(lkedfitem *,lkedfitem **); /* Translate an equivalence */
lkedfitem *DropXOR(lkedfitem *,lkedfitem **); /* Translate a XOR connective */
void DropNAND(lkedfitem *); /* Translate a NAND */
void DropNOR(lkedfitem *); /* Translate a NOR */
int32_t IndentNegations(lkedfitem *); /* Move negations inwards */
void DropQuantifiers(lkedfitem **); /* Delete quantifiers in formula */
void RemoveLinkedListChain(lkedfitem **,lkedfitem *,lkedfitem *); /* Remove a chain of linked list items */
int32_t Convert2CNF(char *,char **,char **,int32_t *); /* Convert sentence to CNF */
int32_t CnfOrCnf(char **,int32_t *,char *); /* Merge two CNF's separated by an OR operator */
lkedfitem *Text2LinkedList(char *); /* Convert formula text to linked list format */
void FreeLkedLstFormula(lkedfitem *); /* Free formula storage in linked list format */
void RemoveRedundantBrackets(lkedfitem **); /* Remove redundant brackets */
lkedfitem *RemoveAdjBrackets(lkedfitem *,lkedfitem **); /* Remove set of adjacent redundant brackets */
void SetPolarity(lkedfitem *); /* Set polarity of linked list items */
void AddItem2LkdLstF(lkedfitem **,lkedfitem **,lkedfitem **); /* Add item to formula in linked list format */
int32_t AddOpenBracket(lkedfitem **,lkedfitem **,lkedfitem **); /* Add open bracket to formula in linked list format */
int32_t AddQuantifier(char *,char **,lkedfitem **,lkedfitem **,lkedfitem **,
		nameschain **); /* Add quantifier to formula in linked list format */
int32_t AddName(char *,char **,lkedfitem **,lkedfitem **,lkedfitem **,
		nameschain *); /* Add name to formula in linked list format */
int32_t AddPredOrFunc(hashchain *,char **,lkedfitem **,lkedfitem **,lkedfitem **,
		nameschain *); /* Add a predicate or function in linked list format */
int32_t AddEquality(char *,char **,lkedfitem **,lkedfitem **,lkedfitem **,
		nameschain *); /* Add an (in)equality in linked list format */
int32_t AddEquRootTerm(char *,char **,lkedfitem **,lkedfitem **,
		nameschain *); /* Add an (in)equality root term in linked list format */
int32_t DuplicateBlock(lkedfitem *,lkedfitem *,lkedfitem **,lkedfitem **); /* Duplicate a block of linked list items */
void InsertBlock(lkedfitem *block,lkedfitem *,lkedfitem *); /* Insert a block of linked list items */
lkedfitem *JumpOverBlockOrTerm(lkedfitem *,int32_t); /* Jump over a block or term */
cmprefix *MiniscopeFormula(lkedfitem **,cmprefix *,char **,int32_t *); /* Miniscope a formula */
int32_t RemoveUnneededQfiers(lkedfitem **); /* Remove redundant quantifiers */
int32_t MiniscopeQuantifier(lkedfitem *,lkedfitem **,struct timeval *); /* Miniscope a quantifier */
int32_t MiniscopeSegment(lkedfitem *,lkedfitem *,lkedfitem **,struct timeval *); /* Miniscope a formula segment */
int32_t MiniscopeBlock(lkedfitem *,lkedfitem *,lkedfitem *,lkedfitem **,struct timeval *); /* Miniscope a formula block */
int32_t IsVarInBlock(lkedfitem *,lkedfitem *); /* Check if a variable exists in a block */
void RelinkVars(lkedfitem *,lkedfitem *,lkedfitem *); /* Rebuild links of variables in a block */
lkedfitem *DuplicateQuantifier(lkedfitem *,lkedfitem **,lkedfitem **,nameschain *); /* Duplicate and insert a quantifier */
cmprefix *Skolemize(lkedfitem **,cmprefix *,char **,int32_t *); /* Skolemize a formula */
cmprefix *TFSimplify(lkedfitem **,cmprefix *,char **,int32_t *); /* Simplify $true and $false predicates */
void RenameVariables(lkedfitem *); /* Rename variables in a formula */
cmprefix * AddLkdLstFormula2KB(lkedfitem *,cmprefix *,cmprefix *,int32_t,char **,
		int32_t *); /* Add linked list formula to main KB in text form */
cmprefix *FormulaRenaming(lkedfitem **,cmprefix *,char **,int32_t *); /* Formula renaming */
void GetClauseNumberBounds(lkedfitem *,int32_t *,int32_t *); /* Get lower bounds of number of clauses in a segment */
void CalcSegmentBounds(int32_t *,int32_t *,int32_t,int32_t,int32_t,int32_t,
		int32_t); /* Calculate lower bounds of number of clauses in a segment */
int32_t SegmentRenaming(lkedfitem **,lkedfitem *,int32_t,int32_t,cmprefix **,
		cmprefix *,nameschain *); /* Rename blocks within a segment */
void GetaABCoefs(int32_t,int32_t,lkedfitem *,lkedfitem *,int32_t,int32_t *,
		int32_t *); /* Get "a" and "b" coefficients of a block */
int32_t RenameBlock(lkedfitem **,lkedfitem *,cmprefix **,cmprefix *,nameschain *); /* Rename a block in a formula */

/* compile module functions. */
int32_t CompileCNF(char *formula,cmprefix *); /* Compile a CNF text formula */
int32_t CompileClause(char *,cmprefix **,cmprefix *); /* Compile a clause */
void CompileLiteral(char *,int32_t *,cmprefix *,int32_t *); /* Literal compilation */
void CompileTerm(char *,int32_t *,cmprefix *,int32_t *); /* Term compilation */
void CompileSymbol(char *,int32_t *,cmprefix *,int32_t *); /* Symbol compilation */
int32_t BytesRequired(char *); /* Compute the number of bytes needed for a compiled clause */
int32_t AddBinClause2KB(cmprefix *,kbase *,int32_t); /* Add binary clause to a KB. */
int32_t AddBinClause2KBUEQ(cmprefix *,kbase *,int32_t); /* Add binary clause to a KB for unit clause */
int32_t AddUEQClause2KB(cmprefix *); /* Add binary clause to a KB for unfailing completion */
int32_t RenumberVars(binprefix *); /* Renumber variables in a binary formula */
int32_t RenumberVars2(binprefix *); /* Renumber variables in critical pairs */
int32_t SafeAppendF(char **,int32_t *,char *,int32_t *); /* Append a string fast and safely */

/* indexing module functions. */
int32_t AddGlobalSymbols(nameschain *); /* Add locally defined symbols to global symbol list */
hashchain *CreateGlobalSymbol(char *,int32_t,int32_t); /* Create global symbol entry */
#ifdef MURMUR3
uint32_t Murmur3(char *); /* Murmur3 hash function */
#else
uint32_t KRMultHash(char *); /* Kernighan and Ritchie hash function */
#endif
hashchain *CheckGlblDeclared(char *); /* Check if a name is globally declared */
int32_t AddDiscrTrees(cmprefix *); /* Insert discrimination trees for clause */
int32_t DelDiscrTrees(cmprefix *); /* Delete discrimination trees for clause */
int32_t EditUEQTrees(cmprefix *,int32_t); /* Edit unfailing completion discrimination trees for clause */
#ifdef SEMANTICTAUTOLOGY
int32_t EditTtlgDscTrees(ttlgclause *,ttlgdstrleaf **,
		int32_t); /* Edit semantic tautology detection discrimination trees */
#endif
int32_t DsTermIndexing(uint8_t *,void **); /* Add item to a discrimination tree */
#ifdef SEMANTICTAUTOLOGY
int32_t DsTtlgIndexing(ttlgclause *,uint8_t *,void **); /* Add item to a semantic tautology discrimination tree */
#endif
void DeleteBranch(uint8_t *,void **); /* Delete item in a discrimination tree */
#ifdef SEMANTICTAUTOLOGY
void DeleteTtlgBranch(uint8_t *,void **); /* Query a semantic tautology discrimination tree */
#endif
void FreeDiscrBranch(void *,int32_t); /* Free discrimination tree */
#ifdef SEMANTICTAUTOLOGY
void FreeTtlgDiscrBranch(void *,int32_t); /* Free semantic tautology discrimination tree */
#endif
int32_t QueryInsDscTree(uint8_t *,uint8_t **,void *,int32_t); /* Discrimination tree instance Query */
int32_t QueryGenDscTree(uint8_t *,uint8_t **,void *,int32_t); /* Discrimination tree generalization Query */
int32_t QueryUnfDscTree(uint8_t *,uint8_t **,void *,int32_t); /* Discrimination tree unification Query */
int32_t QueryDscTree2(uint8_t *,uint8_t **,void *,dstrbktrk *,
		int32_t); /* Query a discrimination tree by recursive function */
#ifdef SEMANTICTAUTOLOGY
int32_t QueryTtlgDscTree(uint8_t *,uint8_t **,uint8_t **,void **); /* Query a semantic tautology discrimination tree */
#endif
void LinkSelectQueues(cmprefix *); /* Link binary clause to clause selection queues */
void UnlinkSelectQueues(cmprefix *); /* Unlink binary clause from clause selection queues */
int32_t Unlink(cmprefix *,kbase *); /* Unlink binary clause from KB */
int32_t UnlinkUEQ(cmprefix *,kbase *); /* Unlink binary clause from KB for unit clause */
int32_t UEQUnlink(cmprefix *); /* Unlink binary clause from KB for unfailing completion */
void UnlinkTxtFormula(cmprefix *,kbase *); /* Unlink text formula from KB */
void FreeTreeMemory(void); /* Free KB tree memory */
#ifdef SEMANTICTAUTOLOGY
void FreeTtlgTreeMemory(ttlgdstrleaf **,int32_t); /* Free semantic tautologies tree memory */
#endif
uint32_t HashClause(uint8_t *); /* Calculate 32 bits hash value for a clause key */
#ifdef DEBUGTREE
void CheckDscTrees(void); /* Discrimination trees integrity check */
#else
#ifdef DEBUGTREE2
void CheckDscTQueries(void); /* Discrimination trees queries check */
#endif

/* infere module functions. */
uint8_t *NextItem(uint8_t *,int32_t); /* Jump to next item in clause */
int32_t IsEqual(uint8_t *,uint8_t *,int32_t); /* Check if two items are identical */
int32_t IsEqual2(uint8_t *,uint8_t *); /* Check if two items in different clauses are identical */
int32_t Unify(uint8_t *,uint8_t *,subst *,int32_t); /* Perform literal or term unification */
int32_t UnifyVar(uint8_t *,uint8_t *,subst *,int32_t); /* Unify a variable with a term */
int32_t SymEquUnify(uint8_t *,uint8_t *,subst *,int32_t); /* Perform symmetric equality unification */
int32_t Generalize(uint8_t *,uint8_t *,subst *,int32_t); /* Perform literal, term or list generalization */
int32_t GeneralizeVar(uint8_t *,uint8_t *,subst *); /* Generalize a variable with a term */
int32_t SymEquGeneralize(uint8_t *,uint8_t *,subst *,int32_t); /* Perform symmetric equality generalization */
uint32_t CheckVarInTerm(uint8_t *,uint8_t *,subst *,
		int32_t); /* Check if a variable occurs in a term with substitution */
uint32_t CheckVarInTerm2(uint8_t *,uint8_t *); /* Check if a variable occurs in a term */
int32_t CopyClause(cmprefix *,int32_t *,cmprefix *); /* Copy a clause */
void DeleteLiteral(binprefix *,uint8_t *); /* Deletes a literal in a clause */
uint8_t *GetLiteralPtr(cmprefix *,int32_t); /* Gets a pointer to literal given its order number */
int32_t GetLiteralNbr(binprefix *,uint8_t *); /* Gets a literal number given a literal pointer */
int32_t ApplySubst2Clause(cmprefix *,int32_t *,subst *,int32_t); /* Apply substitution to a clause */
void GetSubstTermData(subst *,uint8_t **,int32_t); /* Initialize set of pointers to terms in a substitution */
int32_t ApplySubst2Item(uint8_t **,binprefix **,int32_t *,uint8_t *,
		int32_t,uint8_t **); /* Apply substitution to an item */
void UpdateParams(binprefix *); /* Update binary clause parameters */
void UpdateParams2(uint8_t *); /* Update binary formula parameters */
uint8_t *UpdateSymbolParms(binprefix *,uint8_t *,int32_t); /* Update binprefix and symbol parameters in an item */
uint8_t *UpdateSymbolParms2(uint8_t *,uint8_t *); /* Update symbol parameters in an item */
void Infere(cmprefix *); /* Perform saturation inferences */
void InfereUEQ(cmprefix *); /* Perform saturation inferences for unit clause */
void Resolution(cmprefix *); /* Perform resolution inference */
int32_t BuildResolvent(cmprefix *,cmprefix *,uint8_t *,uint8_t *,
		cmprefix **,subst *,int32_t); /* Build clause inferred by resolution */
void PM2Clause(cmprefix *); /* Perform Paramodulation to a clause */
void PM2ClauseUEQ(cmprefix *); /* Perform Paramodulation to a clause for unit clause */
void PMFromClause(cmprefix *); /* Perform Paramodulation from a clause */
void PMFromClauseUEQ(cmprefix *); /* Perform Paramodulation from a clause for unit clause */
int32_t BuildPMClause(cmprefix *,cmprefix *,uint8_t *,uint8_t *,uint8_t *,uint8_t *,
		cmprefix **,subst *); /* Build clause inferred by paramodulation */
void EqualityResolution(cmprefix *); /* Perform equality resolution inference */
void EqualityResolutionUEQ(cmprefix *); /* Perform equality resolution inference for unit clause */
void EqualityFactoring(cmprefix *); /* Perform equality factoring inference */
void Factor(cmprefix *); /* Infer factors of a clause */
int32_t DefinitionFolding(void); /* Build goal transformation clause for ground goals */
int32_t DefinitionFolding2(int32_t); /* Build goal transformation clause for non ground goals */
uint8_t *GoalDefinition(uint8_t *,int32_t); /* Build goal transformation definitions for ground goals */
binprefix *GoalDefinition2(uint8_t *,uint8_t *,int32_t); /* Build goal transformation definitions for non ground goals */

/* simplify module functions. */
int32_t IsTautology(cmprefix *); /* Check if clause is a tautology. */
int32_t RemoveDuplicates(cmprefix *,cmprefix **,int32_t); /* Remove literal duplicates from clause */
int32_t TrivEqResolution(cmprefix *,cmprefix **); /* Trivial equality resolution inference */
int32_t TrivEqResolutionUEQ(cmprefix *,cmprefix **); /* Trivial equality resolution inference for unit clause */
int32_t DestrEqResolution(cmprefix *,cmprefix **); /* Destructive equality resolution inference */
int32_t DestrEqResolutionUEQ(cmprefix *,cmprefix **); /* Destructive equality resolution inference for unit clause */
int32_t FwSubsuming(cmprefix *,int32_t); /* Check if clause is subsumed */
int32_t FwSubsumingUEQ(cmprefix *,int32_t); /* Check if clause is subsumed for unit clause */
int32_t BckSubsuming(cmprefix *,int32_t); /* Perform backward subsumption inference */
int32_t BckSubsumingUEQ(cmprefix *,int32_t); /* Perform backward subsumption inference for unit clause */
int32_t IsSubsumedByClause(uint8_t *,int32_t,binprefix *,subst *,uint8_t *,
		uint8_t *,uint8_t **); /* Check if clause is subsumed by another clause */
int32_t FwdSimplify(cmprefix **,int32_t); /* Forward and backward simplifications */
int32_t FwdSimplifyUEQ(cmprefix **,int32_t); /* Forward and backward simplifications for unit clause */
int32_t FirstSimplify(cmprefix **,int32_t); /* First clause simplification */
int32_t FirstSimplifyUEQ(cmprefix **,int32_t); /* First clause simplification for unit clause */
int32_t SecondSimplify(cmprefix **,int32_t); /* Forward clause simplification */
int32_t SecondSimplifyUEQ(cmprefix **,int32_t); /* Forward clause simplification for unit clause */
int32_t BckSimplify(cmprefix *,int32_t); /* Backward simplification */
int32_t BckSimplifyUEQ(cmprefix *,int32_t); /* Backward simplification for unit clause */
int32_t ResolFactor(binprefix *,uint8_t *,binprefix **,uint8_t **,
		subst *,int32_t); /* Factoring for resolution inferences */
int32_t FwdDemodulation(cmprefix **,int32_t); /* Perform forward demodulation */
int32_t BckDemodulation(cmprefix *,int32_t); /* Perform backward demodulation */
int32_t Demodulate(cmprefix *,uint8_t *,binprefix *,uint8_t *,
		cmprefix **,subst *); /* Demodulate a clause */
uint8_t *ApplyGenrSubst2Term(uint8_t *,subst *,int32_t); /* Apply a generalization substitution to a term */
int32_t IsVariantSubst(subst *,int32_t); /* Check if a substitution generates a variant of a term */
int32_t FwSubsResltn(cmprefix *,cmprefix **,int32_t); /* Perform forward subsumption resolution */
int32_t FwSubsResltnUEQ(cmprefix *,cmprefix **,int32_t); /* Perform forward subsumption resolution for unit clause */
int32_t BkSubsResltn(cmprefix *,int32_t); /* Perform backward subsumption resolution */
int32_t BkSubsResltnUEQ(cmprefix *,int32_t); /* Perform backward subsumption resolution for unit clause */
int32_t SubsResltn(cmprefix *,binprefix *,uint8_t *,uint8_t *,
		cmprefix **); /* Perform subsumption resolution inference */
int32_t SubsResltnUEQ(cmprefix *,binprefix *,uint8_t *,uint8_t *,
		cmprefix **); /* Perform subsumption resolution inference for unit clause */
#ifdef UNITCLRESOLUTION
int32_t UCResolution(cmprefix *); /* Perform unit clause resolution inference */
#endif

/* ueqlib module functions. */
int32_t Normalize(cmprefix **,int32_t); /* Normalize an equation */
int32_t FwdNormalize(cmprefix **,int32_t); /* Forward simplification for UEQOTT and UEQDSC algorithms */
int32_t IsEqTautology(cmprefix *); /* Check if an (in)equality is an equational tautology */
int32_t IsCPConnected(cmprefix *); /* Check if a critical pair is connected */
int32_t IsConnected(uint8_t *,uint8_t *,uint8_t *,int32_t,int32_t); /* Check if two terms are connected */
int32_t IsGrJoinable(uint8_t *,int32_t,int32_t,int32_t,int32_t); /* Check if an equality is ground joinable */
int32_t GJRewrite(int32_t *,int32_t *,int32_t *,uint8_t *,uint8_t *,
		int32_t,int32_t); /* Check if two terms are ground joinable */
int32_t FwUEQSubsum(cmprefix *); /* Check if an equality is subsumed */
int32_t BkUEQSubsum(cmprefix *); /* Backward sumbumption simplifications */
int32_t GenerateCP(cmprefix *); /* Generate critical pairs */
int32_t GenerateCPTo(cmprefix *); /* Generate critical pairs to clause */
int32_t GenerateCPFrom(cmprefix *); /* Generate critical pairs from clause */
int32_t MakeTrivialProof(cmprefix *); /* Generate a trivial proof for UEQ problem */
int32_t FwUEQSimplify(cmprefix **); /* Perform forward simplifications of UNPROC clause */
int32_t BckUEQSimplify(cmprefix *); /* Backward simplify active clauses */
int32_t UEQEqResolution(cmprefix **,int32_t); /* Equality resolution inference for unfailing completion */
int32_t UEQResolution(cmprefix **); /* Resolution goal simplification for unfailing completion */
uint8_t *ApplySubst2Term(uint8_t *,subst *,int32_t); /* Apply an instance substitution to a term */
void ShiftUEQVarNbr(binprefix *,int32_t); /* Shift unit equality variable numbers */
void UnshiftUEQVarNbr(binprefix *,int32_t); /* Unshift unit equality variable numbers */
int32_t NextPerm(int32_t *,int32_t *,int32_t *,int32_t); /* Generate next permutation */
int32_t SplitNAdd2Active(cmprefix **); /* Split unit equalities and add to ACTIVE */
uint8_t *ApplyFastSubst(uint8_t *,int32_t *,int32_t *); /* Apply a special fast substitution to a term */
int32_t CheckWeakRewrite(uint8_t *,uint8_t *,subst *,int32_t); /* Check if a weak rewrite is valid */

/* ordering module functions. */
int32_t CompareLiterals(uint8_t *,uint8_t *); /* Literals comparison */
int32_t CompareEqualities(uint8_t *,uint8_t *); /* Equality literals comparison */
int32_t CompareItems(uint8_t *,uint8_t *,int32_t); /* Standard and non recursive KBO items comparison front end */
int32_t KBOCompare(uint8_t *,uint8_t *); /* Standard KBO comparison */
int32_t NROCompare(uint8_t *,uint8_t *); /* Non recursive version of KBO comparison */
int32_t KBOLex(uint8_t *,uint8_t *); /* KBO lexicographic comparison */
int32_t NROLex(uint8_t *,uint8_t *); /* Non recursive KBO lexicographic comparison control function */
int32_t NROLex2(uint8_t *,uint8_t *); /* Non recursive KBO lexicographic comparison perform function */
int32_t UpdateVWB1(uint8_t *,int32_t,int32_t); /* Update weight and variable balance of a term and sub-terms */
                                               /* and check if a variable is in them. */
void UpdateVWB2(uint8_t *,uint8_t *,int32_t); /* Update weight and variable balance of a list of terms */
void UpdateVWB3(uint8_t *,int32_t); /* Update weight and variable balance of a term */
int32_t CompareEquTerms(binprefix *,uint8_t *,uint8_t *); /* Equality root terms comparison */
int32_t ConnectCompare(uint8_t *,uint8_t *,int32_t,int32_t); /* Terms comparison specific for connectedness check */
int32_t CheckGJOrdering1(int32_t *,int32_t *,uint8_t *,uint8_t *,
		int32_t); /* Check term ordering condition for ground joinability */
int32_t CheckGJOrdering2FE(int32_t *,int32_t *,int32_t *,uint8_t *,uint8_t *,
		int32_t); /* Front end for CheckGJOrdering2() function */
int32_t CheckGJOrdering2(int32_t *,int32_t *,int32_t *,uint8_t *,uint8_t *,
		int32_t); /* Check term ordering condition for ground joinability */
void Equalize(int32_t,int32_t,int32_t *,int32_t *,
		int32_t); /* Equalize two variables for ground joinability checking */
#ifdef SEMANTICTAUTOLOGY
int32_t TtlgCompare(uint8_t *,uint8_t *); /* Term comparison for semantic tautology detection */
#endif
int32_t SecondKBOCheck(uint8_t *,uint8_t *,int32_t); /* Perform second KBO check */
int32_t ApplySbst2TwoItm(uint8_t *,uint8_t *,uint8_t **,uint8_t **,cmprefix **,
		subst *,int32_t,int32_t); /* Apply substitution to two terms */
int32_t CheckEqRootOrder(uint8_t *,uint8_t *,subst *,int32_t,int32_t,
		int32_t); /* Check ordering constraints in equality root terms */
int32_t CheckEqRootOrder2(uint8_t *,uint8_t *,subst *,int32_t); /* Check ordering constraints in equality root terms */
uint8_t *UpdateOffsets(binprefix *,uint8_t *); /* Update item offsets */

/* selecting module functions. */
int32_t SelectItems(cmprefix *); /* Select items in a clause */
int32_t SelectItemsUEQ(cmprefix *); /* Select items in a clause for unit clause */
int32_t SelectLiterals1(cmprefix *,char *,int32_t,int32_t,int32_t); /* Several types of literal selection */
int32_t SelectLiterals2(cmprefix *,char *,int32_t); /* MXSINGLENEG literal selection */
int32_t SelectMaximal(cmprefix *,char *,int32_t,int32_t *,int32_t *,int32_t *); /* Auxiliary maximal literal selection */
int32_t SelectByWeight(cmprefix *,char *); /* Select literals by weight */
int32_t SelectByVars(cmprefix *,char *); /* Select literals by number of variables */
int32_t SelectByTop(cmprefix *,char *); /* Select literals by number of top level variables */
int32_t SelectByDVars(cmprefix *,char *); /* Select literals by number of distinct variables */
int32_t SelectByNoPosEq(cmprefix *,char *); /* Select literals by no positive equalities */
int32_t SelectByNegEq(cmprefix *,char *); /* Select negative equalities */
int32_t SelectByNegative(cmprefix *,char *); /* Select negative literals */
int32_t SelectByLMin(cmprefix *,char *); /* Select literals with minimal lookahead children */
int32_t SelectByLMax(cmprefix *,char *); /* Select literals with maximal lookahead children */
void SelectByLexic(cmprefix *,char *,int32_t); /* Lexical tie break literal selection */
int32_t CompareLexic4Select(uint8_t *,uint8_t *); /* Lexical comparison for SelectByLexic function */
int32_t SelectType1(cmprefix *,char *); /* Type 1 literal selection */
int32_t SelectType2(cmprefix *,char *); /* Type 2 literal selection */
int32_t SelectType3(cmprefix *,char *); /* Type 3 literal selection */
int32_t SelectType4(cmprefix *,char *); /* Type 4 literal selection */
int32_t SelectType5(cmprefix *,char *); /* Type 5 literal selection */
int32_t SelectType6(cmprefix *,char *); /* Type 6 literal selection */
int32_t SelectType14Cpl(int32_t (*selectfunc)(cmprefix *,char *),cmprefix *,
		char *,int32_t); /* Complete version of Type 1 to 4 literal selection */
int32_t SelectType56Cpl(int32_t (*selectfunc)(cmprefix *,char *),cmprefix *,
		char *,int32_t); /* Complete version of Type 5 and 6 literal selection */

/* multith module functions. */
void Saturate(void); /* Start normal saturation algorithms */
void Saturate2(void); /* Start saturation algorithms for strategy tuning */
int32_t SetMTParams(uint64_t); /* Set strategy specific KB parameters */
void InferenceProcess(void); /* Main inference function for the process for non UEQ */
void InferenceProcessUEQ(void); /* Main inference function for the process for UEQ */
void InferenceProcess2(void); /* Main inference function for non UEQ strategy tuning */
void InferenceProcess2UEQ(void); /* Main inference function for UEQ strategy tuning */
void Otter(void); /* Otter saturation algorithm */
void OtterUEQ(void); /* Otter saturation algorithm for unit equality */
void Discount(void); /* Discount saturation algorithm */
void DiscountUEQ(void); /* Discount saturation algorithm for unit equality */
void UEQOtter(void); /* Otter saturation algorithm for unfailing completion */
void UEQDscnt(void); /* Discount saturation algorithm for unfailing completion */
cmprefix *SelectClause(void); /* Select current clause from passive clauses */
int32_t Retained(cmprefix *); /* Check if clause is retained */
void ClauseWeight(cmprefix *); /* Compute clause weight */
int32_t LRSClean(int32_t); /* Delete unreachable passive clauses */
int32_t SetClSelectOrder(void); /* Set select order for clause selection queues */
int32_t Preprocess(int32_t); /* Perform problem pre-processing */
uint64_t ClauseRelevance(void); /* Perform relevance filtering */
int32_t InitSInEDist(void); /* Perform SInE processing */
void GoType(void); /* Print problem type flags */
void Clausify(void); /* Convert problem formulas to CNF and print them in TPTP format */
void GetPbmTypeNumber(void); /* Calculate problem type number */
uint32_t LookAhead(cmprefix *); /* Estimate number of inferences of a given clause */

/* filemgmt module functions. */
char *Decompile(binprefix *); /* Translate a binary clause formula to text format */
char *Decompile2(uint8_t *); /* Translate a binary hashchcmp formula to text format */
char *DecompileSat(satclause *); /* Translate a SAT clause formula to text format */
char *DecompileAsserts(uint8_t *,int32_t); /* Translate an A-clause assertions to text format */
int32_t AddTermText(uint8_t *,char **,int32_t *,int32_t *); /* Translate a binary term to text format */
#ifdef DEBUGCODE
void BuildTxtSubstData(uint8_t *,uint8_t *,subst *,int32_t); /* Build substitution data in text form */
#endif
void PrintStats(int32_t); /* Display statistics */
void PrintStatData(stats *,int32_t); /* Display statistics data */
void PrintProcessSettings(void); /* Print process option settings */
int32_t BuildProof(void); /* Print theorem proof */
int32_t AddSatCl2Proof(void); /* Add SAT clauses to proof queue */
int32_t AddAclause2Proof(proofnode *); /* Add A-clause to proof queue */
void LinkProofNode(proofnode *,proofnode *); /* Link node to proof queue */
int32_t PrintProof(void); /* Print theorem proof or saturation output */
int32_t BuildSaturation(void); /* Build satisfiable/counter-satisfiable saturation output */
int32_t PrintSatModel(void); /* Print saturation SAT model output */
int32_t PrintModel(void); /* Print model from a GOSAT run */
int32_t Import(char *,char *,int32_t,int32_t); /* Import a TPTP problem file */
int32_t ReadDirective(char **,int32_t *,FILE *); /* Read a directive from a TPTP file */
int32_t CheckBrackets(char **,int32_t *,char **); /* Check brackets and parenthesis balance */
int32_t Cnf2Fof(char **); /* Convert a CNF formula to FOF */
int32_t FileExist(char *); /* Check if a file exist */
void PrintKBData(void); /* Print current data of a knowledge base */
int32_t PrintAClauses(void); /* Print A-clauses of a knowledge base */
int32_t PrintSingleAClause(cmprefix *); /* Print a single A-clause */
int32_t PrintCompSymbols(void); /* Print the component symbols of a knowledge base */
int32_t PrintSATClauses(void); /* Print SAT clauses of a knowledge base */
int32_t PrintSingleSATClause(satclause *); /* Print a single SAT clause */
int32_t LinkedList2Text(lkedfitem *,char **,int32_t *,int32_t); /* Convert formula in linked list format to text */
int32_t Equ2Text(lkedfitem **,char **,int32_t *,int32_t *); /* Convert (in)equality in linked list formula to text */
int32_t PredOrFunc2Text(lkedfitem **,char **,int32_t *,
		int32_t *); /* Convert predicate or function in linked list formula to text */
int32_t Term2Text(lkedfitem **,char **,int32_t *,int32_t *); /* Convert term in linked list formula to text */
int32_t Quantifier2Text(lkedfitem **,char **,int32_t *,int32_t *); /* Convert a quantifier in linked list formula to text */
int32_t PrintClausifiedFormulas(int32_t); /* Print main KB formulas in TPTP CNF format */
char *ParseFilename(char *,char **); /* Parse and build a filename */
void PrintStrategy(void); /* Print user strategy information */
void Help(char*); /* Display general help or a help topic */

/* satsolver module functions. */
int32_t SplitClause(cmprefix *,int32_t); /* Get maximal split of an A-clause */
int32_t AddAssertions(binprefix *,binprefix *,binprefix *); /* Add assertions from two clauses to a third clause */
int32_t GetLocksAsserts(binprefix *,binprefix *,binprefix *); /* Get simplification assertions and locking assertions */
void ChainAsserts(uint8_t *); /* Chain assertions to appropriate queue */
void ChainLocks(uint8_t *); /* Chain locking assertions to appropriate queue */
void UnchainAsserts(uint8_t *); /* Unchain assertions */
void UnchainLocks(uint8_t *); /* Unchain locking assertions */
int32_t GetSplitSymbol(uint8_t *,int32_t,int32_t,hashchcmp **,int32_t); /* Get a P-predicate symbol for a clause */
int32_t IsVariant(uint8_t *,int32_t,hashchcmp *); /* Check if clause is a variant of another clause */
int32_t LiteralVariant(uint8_t *,hashchcmp *,int8_t *,int32_t,subst *,struct timeval *); /* Literal variant check */
int32_t ItemVariant(uint8_t *,uint8_t *,subst *); /* Item variant check */
int32_t SatQ2SatSolver(int32_t); /* Send SAT queue to SAT solver */
int32_t SatSolver(void); /* SAT solver algorithm */
void ChainOrdComponent(hashchcmp *); /* Chain a hashchcmp element to the ordered sub-queue */
void UnchainOrdComponent(hashchcmp *); /* Remove a hashchcmp element from the ordered sub-queue */
void ChainSatClause(satclause *); /* Chain a SAT solver clause to ordered sub-queue */
void UnchainSatClause(satclause *); /* Remove a satclause element from the ordered sub-queue */
int32_t DoSymbolAssgnmnt(satnode *,satclause **); /* Perform symbol assignment tasks */
void UndoSymbolAssgnmnt(satnode *); /* Undo symbol assignment */
void GoSat(void); /* Call SAT solver without first order prover */
int32_t ApplyModelChanges(void); /* Apply changes of current interpretation */
int32_t RemoveLocks(hashchcmp *); /* Remove locks unlocked by model change */
int32_t DoLocks(hashchcmp *); /* Do new locks produced by model change */
int32_t AddCompClause(hashchcmp *); /* Add non existing component clauses */
int32_t BuildContrClause(cmprefix *); /* Add contradiction clause to SAT queue */
int32_t SatSimplify(satclause **); /* Check tautologies and remove duplicates in SAT clause */
int32_t SatSubsumption(satclause *); /* Perform by and to Sat clause subsumptions */
int32_t IsSatClSubsumed(satclause *,satclause *,uint8_t *); /* Check if a Sat clause subsumes another Sat clause */
int32_t Backtrack(satclause *,satnode **); /* Backtrack from a contradiction */
int32_t SubsumSetup(uint8_t *,int32_t,binprefix *,uint8_t **); /* Setup subsumption data structures */
int32_t SbsSatSolver(uint64_t); /* SAT solver for subsumption simplification */
void SbUndoSymbolAssgnmnt(subsnode *); /* Undo symbol assignment for subsumption SAT solver */
void SbChainSatClause(subsatcl *); /* Link a subsatcl element to ordered queue */
void SbUnchainSatClause(subsatcl *); /* Remove a subsatcl element from the ordered sub-queue */
#ifdef VERIFYSATPROOF
void VerifySatProof(void); /* Verify SAT part of the proof */
#endif
#if VERIFYSATFINALSTATUS == 1
void VerifySatStatus(void); /* Verify SAT final status after a problem solving run */
#endif

/* learn module functions. */
int32_t ProcessLearnData(char *,char *); /* Build learn data from output learn file */
learnvector *DCDescentMethod(learnvector *,uint8_t *,int32_t,int32_t,float *,float *,
		float *,float *,int32_t *,int32_t *); /* Dual Coordinate Descent Method */
void GetAccuracy(learnvector *,uint8_t *,int32_t,int32_t,float *,float *,
		float *,int32_t *,int32_t *); /* Get learning accuracy data */
learnvector *GetClauseVector(cmprefix *); /* Build a clause vector */
int32_t VerticalFeatures(cmprefix *); /* Compute vertical features of a clause vector */
int32_t  HorizontalFeatures(cmprefix *); /* Compute horizontal features of a clause vector */
int32_t  StaticFeatures(cmprefix *clause); /* Compute static features of a clause vector */
void LearnFromProblem(); /* Extract learning information from solved problem */
void ReadLearnRecords(void); /* Read learn records generated by PROCLEARN command */

/* parsing module functions. */
void CommandProc(void); /* Command processor */
void ParseArgs(int,char **); /* Parse program arguments */
int32_t ChkConvertOption(char *); /* Check and convert option to interactive command */
int32_t ParseCommand(char *,char **); /* Command parser */
#endif /* GLOBAL_H_ */

/* Shuffling and randomizing functions. */
int32_t ShuffleSymbols(void); /* Shuffle symbol ids stored in precedence field */
void UnshuffleSymbols(void); /* Restore shuffled symbol ids */
int32_t ShuffleLiterals(cmprefix *); /* Shuffle literals and equality root terms in a clause */
cmprefix *SelectUnprocClause(void); /* Select and unlink an unprocessed clause */
uint64_t RandomStrategy(int32_t); /* Generate a random strategy */

#endif
