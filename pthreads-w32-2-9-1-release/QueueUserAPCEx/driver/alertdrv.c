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

#include "ntddk.h"

#define FILE_DEVICE_ALERTDRV  0x00008005
#define IOCTL_ALERTDRV_SET_ALERTABLE2 CTL_CODE(FILE_DEVICE_ALERTDRV, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)


#define ALERTDRV_DEVICE_NAME_U     L"\\Device\\Alertdrv"
#define ALERTDRV_DOS_DEVICE_NAME_U L"\\DosDevices\\ALERTDRV"

/*  Debugging macros */
#ifdef DBG
#define AlertDrvKdPrint(_x_) \
                DbgPrint("AlertDrv.sys: ");\
                DbgPrint _x_;
#else
#define AlertDrvKdPrint(_x_)
#endif

/* Function prototypes for APCs */
void
KeInitializeApc(
  PKAPC Apc,
  PKTHREAD Thread,
  CCHAR ApcStateIndex,
  PKKERNEL_ROUTINE KernelRoutine,
  PKRUNDOWN_ROUTINE RundownRoutine,
  PKNORMAL_ROUTINE NormalRoutine,
  KPROCESSOR_MODE ApcMode,
  PVOID NormalContext
);


void
KeInsertQueueApc(
  PKAPC Apc,
  PVOID SystemArgument1,
  PVOID SystemArgument2,
  UCHAR unknown
);


void
KernelApcCallBack(PKAPC Apc, PKNORMAL_ROUTINE NormalRoutine, PVOID NormalContext, PVOID SystemArgument1, PVOID SystemArgument2)
{
  KEVENT event;
  LARGE_INTEGER Timeout;

  AlertDrvKdPrint(("Freeing APC Object\n"));

  ExFreePool(Apc);    /* free the kernel memory */

  Timeout.QuadPart = 0;
  KeDelayExecutionThread(UserMode, TRUE, &Timeout);

  /*
   * Another way for a thread to set itself in alertable state 
   * (MSJ, Nerditorium, July 99):
   *
   * KeInitializeEvent(&event, SynchronizationEvent, FALSE);
   * KeWaitForSingleObject(&event, Executive, UserMode, TRUE, &Timeout);
   */

  return;
}


void
UserApcCallBack(PVOID arg1, PVOID arg2, PVOID arg3)
{
  return;
}

/* Function prototypes */

NTSTATUS DriverEntry(IN PDRIVER_OBJECT  DriverObject,IN PUNICODE_STRING registryPath);
NTSTATUS AlertDrvDispatch(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp);
VOID AlertDrvUnload(IN PDRIVER_OBJECT DriverObject);
NTSTATUS  AlertDrvSendTheSignal(PETHREAD Thread);


NTSTATUS DriverEntry(IN PDRIVER_OBJECT  DriverObject, IN PUNICODE_STRING RegistryPath)
{
  PDEVICE_OBJECT deviceObject = NULL;
  NTSTATUS       status;
  WCHAR          deviceNameBuffer[] = ALERTDRV_DEVICE_NAME_U;
  UNICODE_STRING deviceNameUnicodeString;
  WCHAR          deviceLinkBuffer[] = ALERTDRV_DOS_DEVICE_NAME_U;
  UNICODE_STRING deviceLinkUnicodeString;

  AlertDrvKdPrint (("DriverEntry\n"));

  RtlInitUnicodeString (&deviceNameUnicodeString, deviceNameBuffer);

  status = IoCreateDevice (DriverObject,0,&deviceNameUnicodeString,
                           FILE_DEVICE_ALERTDRV, 0,TRUE,&deviceObject);

  if (!NT_SUCCESS(status))
    {
      AlertDrvKdPrint (("IoCreateDevice failed:%x\n", status));
	  return status;
	}

  DriverObject->MajorFunction[IRP_MJ_CREATE]         =
  DriverObject->MajorFunction[IRP_MJ_CLOSE]          =
  DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = AlertDrvDispatch;
  DriverObject->DriverUnload = AlertDrvUnload;

  RtlInitUnicodeString(&deviceLinkUnicodeString,deviceLinkBuffer);
  status = IoCreateSymbolicLink (&deviceLinkUnicodeString, &deviceNameUnicodeString);
  if (!NT_SUCCESS(status))
    {
      AlertDrvKdPrint (("IoCreateSymbolicLink failed\n"));
      IoDeleteDevice (deviceObject);
    }

  return status;
}


NTSTATUS AlertDrvDispatch(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
  PIO_STACK_LOCATION irpStack;
  PVOID              ioBuffer;
  ULONG              inputBufferLength;
  ULONG              outputBufferLength;
  ULONG              ioControlCode;
  NTSTATUS           ntStatus;

  PHANDLE				ph = NULL;
  PETHREAD			uThread = NULL;

  Irp->IoStatus.Status      = STATUS_SUCCESS;
  Irp->IoStatus.Information = 0;

  irpStack = IoGetCurrentIrpStackLocation(Irp);

  ioBuffer           = Irp->AssociatedIrp.SystemBuffer;
  inputBufferLength  = irpStack->Parameters.DeviceIoControl.InputBufferLength;
  outputBufferLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

  switch (irpStack->MajorFunction)
    {
    case IRP_MJ_CREATE:
      AlertDrvKdPrint (("IRP_MJ_CREATE\n"));
      break;

	case IRP_MJ_CLOSE:
      AlertDrvKdPrint (("IRP_MJ_CLOSE\n"));
      break;

    case IRP_MJ_DEVICE_CONTROL:
      ioControlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;

      switch (ioControlCode)
        {
        case IOCTL_ALERTDRV_SET_ALERTABLE2:
          if (inputBufferLength >= sizeof(PVOID))
            {
              ph = (PHANDLE) ioBuffer;
              Irp->IoStatus.Status = ObReferenceObjectByHandle(*((PHANDLE)ph),THREAD_ALL_ACCESS,NULL,UserMode,&uThread,NULL);

              if (NT_ERROR(Irp->IoStatus.Status))
                {
                  AlertDrvKdPrint (("ObReferenceObjectByHandle Failed (%ld)\n", Irp->IoStatus.Status));
                }
              else
                {
                  AlertDrvKdPrint (("uThread = 0x%lx\n", uThread));

                  Irp->IoStatus.Status = AlertDrvSendTheSignal(uThread);
                  ObDereferenceObject((PVOID) uThread);
                }
             }
          else
            {
              Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
              AlertDrvKdPrint (("Invalid parameter passed!\n"));
            }
          break;

        default:
          AlertDrvKdPrint (("Unknown IRP_MJ_DEVICE_CONTROL\n"));
          Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
          break;

        }

      break;
    }

  ntStatus = Irp->IoStatus.Status;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);

  return ntStatus;
}


VOID
AlertDrvUnload(IN PDRIVER_OBJECT DriverObject)
{
  WCHAR                  deviceLinkBuffer[]  = ALERTDRV_DOS_DEVICE_NAME_U;
  UNICODE_STRING         deviceLinkUnicodeString;

  RtlInitUnicodeString (&deviceLinkUnicodeString, deviceLinkBuffer);
  IoDeleteSymbolicLink (&deviceLinkUnicodeString);
  IoDeleteDevice (DriverObject->DeviceObject);

  AlertDrvKdPrint (("Driver has been unloaded\n"));

  return;
}


NTSTATUS
AlertDrvSendTheSignal(PETHREAD	uThread)
{
  NTSTATUS   ntStatus = STATUS_SUCCESS;
  PKAPC      kApc;

  /* Allocate an KAPC structure from NonPagedPool */
  kApc = ExAllocatePool(NonPagedPool, sizeof(KAPC));
  if (kApc == NULL)
    {
      AlertDrvKdPrint (("ExAllocatePool returned NULL\n"));
      return !ntStatus;
    }

  KeInitializeApc(kApc,
                  (PKTHREAD) uThread,
                  0,
                  (PKKERNEL_ROUTINE) &KernelApcCallBack,
                  0,
                  (PKNORMAL_ROUTINE) &UserApcCallBack,
                  KernelMode,
                  NULL);

  KeInsertQueueApc (kApc, NULL, NULL, 0);

  return ntStatus;
}