/*
 ============================================================================
 Name        : perfmon.h
 Author      : Oscar Contreras
 Version     :
 Copyright   : Copyright (C) 2015-2016 Oscar Contreras. All rights reserved.
 Description : First order logic theorem prover in C
 ============================================================================
 */
/****************************************************************
*
*                 perfmon.h (performance monitoring header file)
*
* All of the documentation and software included in the Drodi package
* is copyrighted by Oscar Contreras.
*
* This header file must be included only in sources that make use of
* performance monitoring functions or its emulation.
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

#ifndef PERFMON_H_
#define PERFMON_H_

/*--- Standard Includes ----------------------------------------*/
#include <stddef.h>                     /* Standard definitions         */
#include <linux/perf_event.h>           /* PERF_* constants definition  */
#include <sys/ioctl.h>                  /* ioctl()                      */
#include <sys/syscall.h>                /* SYS_* constants definition   */

/*----------------------------------------------------------------
 *
 *  Global variables shared with other modules that are initialized here.
 *  WARNING: Don't use "static" as then a different variable will be created in
 *  each module with the same name.
 *
 *--------------------------------------------------------------*/
#ifndef VARTYPE2
#define VARTYPE2 extern
extern double hwinstrtime; /* Accumulated time after disabling simulated hardware instruction count. */
extern int32_t ioctlstate; /* Simulated ioctl state (0 if disabled, 1 if enabled). */
#else
double hwinstrtime=0;
int32_t ioctlstate=0;
#endif

/*----------------------------------------------------------------
 *
 *  Global variables shared with other modules that are not initialized here.
 *  WARNING: Don't use "static" as then a different variable will be created in
 *  each module with the same name.
 *
 *--------------------------------------------------------------*/
VARTYPE2 struct timeval ioctlinittime; /* Initial time stamp when simulated ioctl is enabled. */
VARTYPE2 clock_t ioctlinitcputk; /* Initial CPU ticks when simulated ioctl is enabled. */

#ifdef PERFMONIOCTL
/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Front end for ioctl() function)  OCJ
 *
 *    This function is a front end for the ioctl() calls related to
 *    the count of executed hardware instructions. If filedesc
 *    is not -1 then the ioctl() call is performed. Otherwise count
 *    of executed hardware instructions is not possible and then
 *    it is simulated by performing a time control and translating
 *    the results to an estimated number of executed hardware
 *    instructions according to instrate variable. In this later
 *    case CPU time is used if available, otherwise elapsed time
 *    is used.
 *
 *
 *  ARGUMENTS:
 *
 *    request: Same argument as ioctl() function, accepting the
 *             values PERF_EVENT_IOC_RESET, PERF_EVENT_IOC_ENABLE
 *             and PERF_EVENT_IOC_DISABLE
 *
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
static inline void myioctl(unsigned long request){
	auto struct timeval currtime;                     /* Current time */

	/* Real executed hardware instructions are available. */
	if (filedesc!=-1){
		ioctl(filedesc,request,0);
		return;
	}

	/* Real executed hardware instructions are not available. */
	if (request==PERF_EVENT_IOC_RESET){
		hwinstrtime=0;
		if (ioctlstate!=0){
			if (cpuavail){
				ioctlinitcputk=clock();
			} else {
				gettimeofday(&ioctlinittime,NULL);
			}
		}
	} else if (request==PERF_EVENT_IOC_ENABLE){
		if (ioctlstate==0){
			if (cpuavail){
				ioctlinitcputk=clock();
			} else {
				gettimeofday(&ioctlinittime,NULL);
			}
			ioctlstate=1;
		}
	} else {
		if (ioctlstate!=0){
			if (cpuavail){
				hwinstrtime+=((double)(clock()-ioctlinitcputk))/CLOCKS_PER_SEC;
			} else {
				gettimeofday(&currtime,NULL);
				hwinstrtime+=(currtime.tv_sec-ioctlinittime.tv_sec+(currtime.tv_usec-ioctlinittime.tv_usec)/1000000.0);
			}
			ioctlstate=0;
		}
	}
	return;
} /* myioctl */
#endif

/*--------------------------------------------------------------
 *
 *  DESCRIPTION: (Front end for read() function)  OCJ
 *
 *    This function is a front end for the read() calls related to
 *    the count of executed hardware instructions. If filedesc
 *    is not -1 then the read() call is performed. Otherwise count
 *    of executed hardware instructions is not possible and then
 *    it is simulated by performing a time control and translating
 *    the results to an estimated number of executed hardware
 *    instructions according to instrate variable. In this later
 *    case CPU time is used if available, otherwise elapsed time
 *    is used.
 *
 *
 *  ARGUMENTS:
 *
 *    hwinstr: Pointer to executed hardware instructions.
 *
 *  RETURNS:
 *
 *    None.
 *
 *--------------------------------------------------------------*/
static inline void myread(uint64_t *hwinstr){
	auto struct timeval currtime;                     /* Current time */

	/* Real executed hardware instructions are available. */
	if (filedesc!=-1){
		read(filedesc,hwinstr,sizeof(*hwinstr));
		return;
	}

	/* Real executed hardware instructions are not available. */
	if (ioctlstate==0){
		*hwinstr=hwinstrtime*perfmonrate;
	} else {
		if (cpuavail){
			*hwinstr=(hwinstrtime+(((double)(clock()-ioctlinitcputk))/CLOCKS_PER_SEC))*perfmonrate;
		} else {
			gettimeofday(&currtime,NULL);
			*hwinstr=(hwinstrtime+currtime.tv_sec-ioctlinittime.tv_sec+(currtime.tv_usec-ioctlinittime.tv_usec)/1000000.0)*perfmonrate;
		}
	}
	return;
} /* myread */

#endif
