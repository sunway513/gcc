/* Copyright (C) 2007-2015 Free Software Foundation, Inc.
   Contributed by Richard Henderson <rth@redhat.com>.

   This file is part of the GNU Offloading and Multi Processing Library
   (libgomp).

   Libgomp is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   Libgomp is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.  */

/* This file handles the maintainence of tasks in response to task
   creation and termination.  */

#include "libgomp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef MTAPI
void (*fn_ptr) (void *);
/*mtapi_job_hndl_t job_hndl;*/
//count of how many tasks creaeted
/*int count=0;*/
//define the action function for task executation
#endif //MTAPI

    /* Called when encountering an explicit task directive.  If IF_CLAUSE is
   false, then we must not delay in executing the task.  If UNTIED is true,
   then the task may be executed by any member of the team.  */

void
GOMP_task (void (*fn) (void *), void *data, void (*cpyfn) (void *, void *),
	   long arg_size, long arg_align, bool if_clause, unsigned flags,
	   void **depend)
{
  printf("Inside task start\n");
  struct gomp_thread *thr = gomp_thread ();
#ifdef MTAPI
  /*assign the function pointer to the TLS storage*/
  fn_ptr = fn;
  mtapi_status_t status;
  /* create action */
  /*printf("MTAPI task create, count %d\n",count);*/
  /*task =*/
  mtapi_task_start(
      MTAPI_TASK_ID_NONE,
      thr->job_hndl,
      data,
      sizeof(data),
      NULL,
      sizeof(NULL),
      MTAPI_DEFAULT_TASK_ATTRIBUTES,
      thr->group_hndl,
      &status
  );
  /*printf("MTAPI task started, count %d\n",count++);*/
  /*printf("mtapi_task_count is %d\n", thr->mtapi_task_count);*/
  /*thr->task_hndl[thr->mtapi_task_count++] = task;*/
//in the current implementation we need the task wait to work around the dependancy
//analysis of the tasks.
  /*mtapi_task_wait(task,MTAPI_INFINITE, &status);*/
  //MTAPI_CHECK_STATUS(status);
#endif
}

/* Called when encountering a taskwait directive.  */

void
GOMP_taskwait (void)
{
  struct gomp_thread *thr = gomp_thread ();
  #ifdef MTAPI
  /*int i;*/
  mtapi_status_t status;
  /*Need to create an array of tasks, stored as TLS, need to send */
    /*all of the tasks to the task wait funciton for explicit waiting.  */
  /*for(i=0;i<thr->mtapi_task_count;i++){*/
     /*mtapi_task_wait(thr->task_hndl[i],MTAPI_INFINITE, &status);*/
  /*}*/
  mtapi_group_wait_all(thr->group_hndl,MTAPI_INFINITE,&status);
  MTAPI_CHECK_STATUS(status);
  #endif //MTAPI

}

void
GOMP_taskyield (void)
{
  /* Nothing at the moment.  */
}

void
GOMP_taskgroup_start (void)
{
}

void
GOMP_taskgroup_end (void)
{
}

