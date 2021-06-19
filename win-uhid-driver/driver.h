
#ifdef _KERNEL_MODE
#include <ntddk.h>
#include <wdf.h>
#include <vhf.h>
DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD UHIDEvtAdd;
EVT_WDF_DEVICE_FILE_CREATE EvtWdfDeviceFileCreate;

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoCtlHandle;
EVT_WDF_IO_QUEUE_IO_WRITE EvtIoWriteHandle;

EVT_WDF_FILE_CLEANUP EvtWdfFileCleanup;
EVT_WDF_FILE_CLOSE EvtWdfFileClose;

/*
* Driver Context
*/
typedef struct _UHIDDriverContext
{
	WDFDEVICE device;
	WDFQUEUE queue;
} UHIDDriverContext, * PUHIDDriverContext;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(UHIDDriverContext, GetUHIDContext);


/*
* I/O Queue Context
*/
typedef struct _UHIDQueueContext
{
	PUHIDDriverContext deviceContext;
} UHIDQueueContext, * PUHIDQueueContext;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(UHIDQueueContext, GetQueueContext);


/*
* WDF File Context
*/
typedef struct _UHIDFileContext
{
	VHFHANDLE vhfHandle;

} UHIDFileContext, * PUHIDFileContext;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(UHIDFileContext, GetFileContext);


NTSTATUS CreateQueue(WDFDEVICE Device, WDFQUEUE* Queue);

#else
#include <windows.h>
#endif

typedef struct _UHIDCreateReq {
	USHORT len;
	UCHAR* descriptor;
} UHIDCreateReq, * PUHIDCreateReq;

// {8B7DE864-23B0-42D7-B42F-FAC7196D2DD2}
static const GUID GUID_UHID_INTERFACE =
{ 0x8b7de864, 0x23b0, 0x42d7, { 0xb4, 0x2f, 0xfa, 0xc7, 0x19, 0x6d, 0x2d, 0xd2 } };

#define IOCTL_UHID_CREATE CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_UHID_DESTORY CTL_CODE(0x8000, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_UHID_INPUT CTL_CODE(0x8000, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)