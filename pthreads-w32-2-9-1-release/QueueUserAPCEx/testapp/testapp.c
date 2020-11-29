/*
 *      QueueUserAPCEx: Extending APCs on Windows Operating System (version 2.0)
 *      Copyright(C) 2004 Panagiotis E. Hadjidoukas
 *
 *      Contact Email: peh@hpclab.ceid.upatras.gr, xdoukas@ceid.upatras.gr
 *
 *      QueueUserAPCEx is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU Lesser General Public
 *      License as published by the Free Software Foundation; either
 *      version 2 of the License, or (at your option) any later version.
 *
 *      QueueUserAPCEx is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      Lesser General Public License for more details.
 *
 *      You should have received a copy of the GNU Lesser General Public
 *      License along with QueueUserAPCEx in the file COPYING.LIB;
 *      if not, write to the Free Software Foundation, Inc.,
 *      59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/* testapp.c : demo of QueueUserAPCEx */
#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
 
#include "QueueUserApcEx.h"


DWORD WINAPI TestSleep(LPVOID lpParam)
{
	printf("[Thread %4ld] Calling Sleep...\n", GetCurrentThreadId());
	Sleep(INFINITE);
	printf("[Thread %4ld] Exiting!\n", GetCurrentThreadId());

	return 0;
}

DWORD WINAPI TestWait(LPVOID lpParam)
{
	HANDLE hEvent;
	DWORD dwEvent;

	hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	printf("[Thread %4ld] Calling WaitForSingleObject...\n", GetCurrentThreadId());
	dwEvent = WaitForSingleObject(hEvent, INFINITE);
	printf("[Thread %4ld] WaitForSingleObject returned %d\n", GetCurrentThreadId(), dwEvent); /* WAIT_IO_COMPLETION */
	printf("[Thread %4ld] Exiting!\n", GetCurrentThreadId());

	return 0;
}

DWORD WINAPI APCRoutine(DWORD APCParam)
{
	printf("[Thread %4ld] Inside APC routine with argument (%ld)\n", 
		GetCurrentThreadId(), APCParam);
	return 0;
}
 
int main(void)
{
	DWORD   APCData;
	HANDLE  hThread;
	ULONG   id;
	INT     res;
	
	QueueUserAPCEx_Init();
	printf("XXX [Thread %4ld] Starting\n", GetCurrentThreadId());

	/* Test: send an APC to myself */ 
	printf("[Thread %4ld] Sending an APC to myself\n", GetCurrentThreadId());
	APCData = 33;
	res= QueueUserAPCEx((PAPCFUNC) APCRoutine, GetCurrentThread(), APCData); 
	

	hThread= CreateThread(NULL, 0, TestSleep, NULL, 0, &id);
	/* Sleep for a while; then send an APC to hThread */
	Sleep(5000);
	printf("[Thread %4ld] Sending an APC to the thread that called Sleep\n", GetCurrentThreadId());
	APCData = 44;
	res= QueueUserAPCEx((PAPCFUNC) APCRoutine, hThread, APCData); 
	WaitForSingleObject(hThread, INFINITE);
	

	hThread= CreateThread(NULL, 0, TestWait, NULL, 0, &id);
	/* Sleep for a while; then send an APC to hThread */
	Sleep(5000);
	printf("[Thread %4ld] Sending an APC to the thread that called WaitForSingleObject\n", GetCurrentThreadId());
	APCData = 55;
	res= QueueUserAPCEx((PAPCFUNC) APCRoutine, hThread, APCData); 
	WaitForSingleObject(hThread, INFINITE);

	printf("[Thread %4ld] Exiting\n", GetCurrentThreadId());

	QueueUserAPCEx_Fini();

	return 0;
}
