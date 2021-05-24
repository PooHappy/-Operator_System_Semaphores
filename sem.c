/*
 * Copyright (c) 2003,2013,2014 Jeffrey K. Hollingsworth <hollings@cs.umd.edu>
 *
 * All rights reserved.
 *
 * This code may not be resdistributed without the permission of the copyright holders.
 * Any student solutions using any of this code base constitute derviced work and may
 * not be redistributed in any form.  This includes (but is not limited to) posting on
 * public forums or web sites, providing copies to (past, present, or future) students
 * enrolled in similar operating systems courses the University of Maryland's CMSC412 course.
 */

#include <geekos/syscall.h>
#include <geekos/errno.h>
#include <geekos/kthread.h>
#include <geekos/int.h>
#include <geekos/elf.h>
#include <geekos/malloc.h>
#include <geekos/screen.h>
#include <geekos/keyboard.h>
#include <geekos/string.h>
#include <geekos/user.h>
#include <geekos/timer.h>
#include <geekos/vfs.h>
#include <geekos/signal.h>
#include <geekos/sem.h>
#include <geekos/projects.h>
#include <geekos/smp.h>
#include <geekos/bitset.h>

/*
 * Create or find a semaphore.
 * Params:
 *   state->ebx - user address of name of semaphore
 *   state->ecx - length of semaphore name
 *   state->edx - initial semaphore count
 * Returns: the global semaphore id
 */
#define MAX_NUM_SEMAPHORES 20
#define MAX_SEMAPHORE_NAME 25
struct Semaphore {
    char name[MAX_SEMAPHORE_NAME+1];
    int count;
    bool use;
    struct Thread_Queue waitingThreads;
};
extern Spin_Lock_t kthreadLock;
extern int Copy_User_String(ulong_t uaddr, ulong_t len, ulong_t maxLen, char **pStr);

struct Semaphore g_Semaphore[MAX_NUM_SEMAPHORES];
 
bool vaildateSID(int sid) {
    if (0 <= sid && sid < MAX_NUM_SEMAPHORES && g_Semaphore[sid].use) 
	return true;
    else 
	return false;
}

int Check(char *N, int length) {
    int ret = -1;

    for(int sid = 0 ; sid < MAX_NUM_SEMAPHORES; sid++) {
	if(strncmp(g_Semaphore[sid].name, N, length)==0) {
	    ret=sid;
	    break;
   	}
    }
    return ret;
}

int Setting() {
    int ret = -1;
    for(int sid = 0 ; sid < MAX_NUM_SEMAPHORES ; sid++) {
    	if(!g_Semaphore[sid].use) {
	    ret = sid;
	    break;
	}
    }
    return ret;
}

int Sys_Open_Semaphore(struct Interrupt_State *state) {
    int sid = 0;
    int ret = -1;
    bool atomic = Begin_Int_Atomic();

    char *N[state->ecx];
    Copy_User_String(state->ebx, state->ecx, MAX_SEMAPHORE_NAME, N);
    int length = state->ecx;
    sid = Check(*N, length);
    if(0 <= sid) {
	ret = sid;
    }
    else {
	sid = Setting();
	if(0 > sid) {
	    ret = -1;
	}
	else {
	    strncpy(g_Semaphore[sid].name, *N, MAX_SEMAPHORE_NAME);
	    g_Semaphore[sid].count = state->edx;
	    g_Semaphore[sid].use = true;
	    ret = sid;
	}
    }
    End_Int_Atomic(atomic);
    
    return ret;
}

/*
 * Acquire a semaphore.
 * Assume that the process has permission to access the semaphore,
 * the call will block until the semaphore count is >= 0.
 * Params:
 *   state->ebx - the semaphore id
 *
 * Returns: 0 if successful, error code (< 0) if unsuccessful
 */
int Sys_P(struct Interrupt_State *state) {
    bool atomic = Begin_Int_Atomic();
    int sid = state->ebx;
    if(!vaildateSID(sid)) {
	return -1;
    }
    if(g_Semaphore[sid].count == 0) {
	if(Interrupts_Enabled()) {
	    Disable_Interrupts();
	    Spin_Lock(&kthreadLock);
	    Wait(&g_Semaphore[sid].waitingThreads);
	    Spin_Unlock(&kthreadLock);
	    Enable_Interrupts();
	}
	else {
	    Wait(&g_Semaphore[sid].waitingThreads);
	}
    }
    g_Semaphore[sid].count--;
    End_Int_Atomic(atomic); 
    return 0;
}

/*
 * Release a semaphore.
 * Params:
 *   state->ebx - the semaphore id
 *
 * Returns: 0 if successful, error code (< 0) if unsuccessful
 */
int Sys_V(struct Interrupt_State *state) {
    bool atomic = Begin_Int_Atomic();
    int sid = state->ebx;
    if(!vaildateSID(sid)) {
	return -1;
    }
    g_Semaphore[sid].count++;
    if(g_Semaphore[sid].count == 1) {
  	if(Interrupts_Enabled()) {
	    Disable_Interrupts();
	    Spin_Lock(&kthreadLock);
	    Wake_Up_One(&g_Semaphore[sid].waitingThreads);
	    Spin_Unlock(&kthreadLock);
	    Enable_Interrupts();
	}
	else {
	    Wake_Up_One(&g_Semaphore[sid].waitingThreads);
	}
    }
    End_Int_Atomic(atomic);
    return 0;
}

/*
 * Destroy our reference to a semaphore.
 * Params:
 *   state->ebx - the semaphore id
 *
 * Returns: 0 if successful, error code (< 0) if unsuccessful
 */
int Sys_Close_Semaphore(struct Interrupt_State *state) {
    int sid = state->ebx;
    char *Null = '\0';
    if(!vaildateSID(sid)) {
	return -1;
    }
    bool atomic = Begin_Int_Atomic();
    strcpy(g_Semaphore[sid].name, Null);
    g_Semaphore[sid].use = false;

    End_Int_Atomic(atomic);
    return 0;
}






