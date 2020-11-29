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

#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <winioctl.h>

#define FILE_DEVICE_ALERTDRV  0x00008005
#define IOCTL_ALERTDRV_SET_ALERTABLE2 CTL_CODE(FILE_DEVICE_ALERTDRV, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)


static HANDLE hDevice = INVALID_HANDLE_VALUE;


BOOL
QueueUserAPCEx_Init(VOID)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      This function initializes QueueUserAPCEx by opening a
      *      handle to the kernel-mode device driver.
      *
      * PARAMETERS
      *      None
      *
      * DESCRIPTION
      *      This function initializes QueueUserAPCEx by opening a
      *      handle to the kernel-mode device driver.
      *
      * RESULTS
      * 	 1    Success
      *      0    Failure: Error values can be retrieved by calling GetLastError
      * ------------------------------------------------------
      */
{
  if ((hDevice = CreateFile("\\\\.\\Global\\ALERTDRV",
                    GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,NULL)) == INVALID_HANDLE_VALUE)
    {
      printf ("QueueUserAPCEx_Init failed: Can't get a handle to the ALERT driver\n");
      return 0;
    }

  return 1;
}


BOOL
QueueUserAPCEx_Fini(VOID)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      This function shutdowns QueueUserAPCEx by closing the
      *      handle to the kernel-mode device driver.
      *
      * PARAMETERS
      *      None
      *
      * DESCRIPTION
      *      This function shutdowns QueueUserAPCEx by closing the
      *      handle to the kernel-mode device driver.
      *
      * RESULTS
      * 	 1    Success
      *      0    Failure: Error values can be retrieved by calling GetLastError
      * ------------------------------------------------------
      */
{
  return CloseHandle(hDevice);
}

DWORD QueueUserAPCEx(PAPCFUNC pfnApc, HANDLE hThread, DWORD dwData)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      Adds a user-mode asynchronous procedure call (APC) object
      *      to the APC queue of the specified thread AND sets this
      *      thread in alertarte state.
      *
      * PARAMETERS
      *      Uses the same parameters as QueueUserAPC.
      *
      * DESCRIPTION
      *      Adds a user-mode asynchronous procedure call (APC) object
      *      to the APC queue of the specified thread AND sets this
      *      thread in alertarte state.
      *
      * RESULTS
	  *		 1    Success
	  *      0    Failure
      * ------------------------------------------------------
      */
{
  DWORD cbReturned;

  /* trivial case */
  if (hThread == GetCurrentThread())
    {
      if (!QueueUserAPC(pfnApc, hThread, dwData))
        {
	      return 0;
        }

      SleepEx(0, TRUE);
      return 1;
    }

  if (INVALID_HANDLE_VALUE == hDevice
      /* && !QueueUserAPCEx_Init() */
      )
    {
      printf ("Can't get a handle to the ALERT driver\n");

      return 0;
    }

  /* probably not necessary */
  if (SuspendThread(hThread) == -1)
    {
      return 0;
    }

  /* Send the APC */
  if (!QueueUserAPC(pfnApc, hThread, dwData))
    {
      return 0;
    }

  /* Ensure the execution of the APC */
  if (DeviceIoControl (hDevice, (DWORD)IOCTL_ALERTDRV_SET_ALERTABLE2, &hThread, sizeof(HANDLE),
		NULL, 0, &cbReturned, 0))
    {
	}
  else
    {
      printf ("DeviceIoControl failed\n");
      return 0;
    }

  /* Here, we could even cancel suspended threads */
  ResumeThread(hThread);

  return 1;
}




