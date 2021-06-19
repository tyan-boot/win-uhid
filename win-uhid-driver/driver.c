#include "driver.h"
#include <initguid.h>
#include <usbiodef.h>

NTSTATUS DriverEntry(
	PDRIVER_OBJECT DriverObject,
	PUNICODE_STRING RegistryPath
)
{
	NTSTATUS status;
	WDF_DRIVER_CONFIG config;

	WDF_DRIVER_CONFIG_INIT(&config, UHIDEvtAdd);

	status = WdfDriverCreate(
		DriverObject,
		RegistryPath,
		WDF_NO_OBJECT_ATTRIBUTES,
		&config,
		WDF_NO_HANDLE
	);

	return status;
}

NTSTATUS UHIDEvtAdd(
	WDFDRIVER Driver,
	PWDFDEVICE_INIT DeviceInit
)
{
	UNREFERENCED_PARAMETER(Driver);

	NTSTATUS status;
	WDF_OBJECT_ATTRIBUTES attributes, fileAttributes;
	WDF_FILEOBJECT_CONFIG fileConfig;

	PUHIDDriverContext context;
	WDFDEVICE device;

	// init device name
	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, UHIDDriverContext);
	DECLARE_CONST_UNICODE_STRING(UHIDDeviceName, L"\\Device\\win-uhid-ctl");

	status = WdfDeviceInitAssignName(DeviceInit, &UHIDDeviceName);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	// init file config
	WDF_OBJECT_ATTRIBUTES_INIT(&fileAttributes);
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&fileAttributes, UHIDFileContext);
	fileAttributes.SynchronizationScope = WdfSynchronizationScopeNone;

	WDF_FILEOBJECT_CONFIG_INIT(&fileConfig, EvtWdfDeviceFileCreate, NULL, EvtWdfFileCleanup);
	WdfDeviceInitSetFileObjectConfig(DeviceInit, &fileConfig, &fileAttributes);


	// create device
	status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	status = WdfDeviceCreateDeviceInterface(device, &GUID_UHID_INTERFACE, NULL);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	WDFSTRING string;
	status = WdfStringCreate(NULL, WDF_NO_OBJECT_ATTRIBUTES, &string);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	status = WdfDeviceRetrieveDeviceInterfaceString(device, &GUID_UHID_INTERFACE, NULL, string);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	UNICODE_STRING uString;
	WdfStringGetUnicodeString(string, &uString);

	KdPrint(("a %s\n", uString));

	context = GetUHIDContext(device);
	context->device = device;

	// create io queue
	status = CreateQueue(device, &context->queue);

	return status;
}

NTSTATUS CreateQueue(
	WDFDEVICE Device,
	WDFQUEUE* Queue
)
{
	NTSTATUS status;
	WDF_IO_QUEUE_CONFIG config;
	WDF_OBJECT_ATTRIBUTES attributes;
	WDFQUEUE queue;
	PUHIDQueueContext context;

	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&config, WdfIoQueueDispatchParallel);
	config.EvtIoDeviceControl = EvtIoCtlHandle;
	config.EvtIoWrite = EvtIoWriteHandle;

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, UHIDQueueContext);

	status = WdfIoQueueCreate(
		Device,
		&config,
		&attributes,
		&queue
	);

	context = GetQueueContext(queue);
	context->deviceContext = GetUHIDContext(Device);

	*Queue = queue;

	return status;
}


NTSTATUS OnCreateUHID(WDFDEVICE Device, WDFREQUEST Request, PUHIDCreateReq createReq)
{
	NTSTATUS status;
	VHF_CONFIG vhfConfig;
	VHFHANDLE vhfHandle;
	WDFFILEOBJECT fileObject = WdfRequestGetFileObject(Request);
	PUHIDFileContext fileContext = GetFileContext(fileObject);

	if (fileContext->vhfHandle != WDF_NO_HANDLE) {
		// release previous vhf
		VhfDelete(fileContext->vhfHandle, TRUE);
	}

	VHF_CONFIG_INIT(&vhfConfig, WdfDeviceWdmGetDeviceObject(Device), createReq->len, createReq->descriptor);
	status = VhfCreate(&vhfConfig, &vhfHandle);

	if (!NT_SUCCESS(status))
	{
		return status;
	}

	status = VhfStart(vhfHandle);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	fileContext->vhfHandle = vhfHandle;

	return status;
}

NTSTATUS OnSubmitReport(WDFDEVICE device, WDFREQUEST Request, PHID_XFER_PACKET report)
{
	UNREFERENCED_PARAMETER(device);

	NTSTATUS status;
	VHFHANDLE vhfHandle;
	PUHIDFileContext fileContext;
	WDFFILEOBJECT fileObject;

	fileObject = WdfRequestGetFileObject(Request);
	fileContext = GetFileContext(fileObject);

	vhfHandle = fileContext->vhfHandle;

	if (vhfHandle != WDF_NO_HANDLE)
	{
		status = VhfReadReportSubmit(vhfHandle, report);
		return status;
	}
	else {
		return STATUS_INVALID_HANDLE;
	}
}

void EvtIoCtlHandle(
	_In_
	WDFQUEUE Queue,
	_In_
	WDFREQUEST Request,
	_In_
	size_t OutputBufferLength,
	_In_
	size_t InputBufferLength,
	_In_
	ULONG IoControlCode
)
{
	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(InputBufferLength);
	UNREFERENCED_PARAMETER(IoControlCode);

	size_t length;
	WDFMEMORY memory;
	PVOID inputBuffer;
	NTSTATUS status;
	PUHIDQueueContext queueContext;

	queueContext = GetQueueContext(Queue);

	switch (IoControlCode)
	{
	case IOCTL_UHID_CREATE:
	{
		
		WdfRequestRetrieveInputMemory(Request, &memory);
		inputBuffer = WdfMemoryGetBuffer(memory, &length);

		status = OnCreateUHID(queueContext->deviceContext->device, Request, (PUHIDCreateReq)inputBuffer);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("OnCreateUHID failed %d\n", status));
		}

		WdfRequestComplete(Request, status);
	
		break;
	}
	case IOCTL_UHID_INPUT:
	{
		WdfRequestRetrieveInputMemory(Request, &memory);
		inputBuffer = WdfMemoryGetBuffer(memory, &length);

		status = OnSubmitReport(queueContext->deviceContext->device, Request, (PHID_XFER_PACKET)inputBuffer);

		if (!NT_SUCCESS(status))
		{
			KdPrint(("OnCreateUHID failed %d\n", status));
		}

		WdfRequestComplete(Request, status);
		break;
	}
	default:
		WdfRequestComplete(Request, STATUS_SUCCESS);
		break;
	}

}

void EvtIoWriteHandle(
	_In_ WDFQUEUE Queue,
	_In_ WDFREQUEST Request,
	_In_ size_t Length
)
{
	NTSTATUS status;
	PVOID inputBuffer;
	size_t bufferLength;
	PUHIDQueueContext queueContext;
	KdPrint(("EvtIoWriteHandle %d\n", Length));

	if (Length < sizeof(HID_XFER_PACKET))
	{
		WdfRequestComplete(Request, STATUS_INVALID_BUFFER_SIZE);
		return;
	}

	status = WdfRequestRetrieveInputBuffer(Request, sizeof(HID_XFER_PACKET), &inputBuffer, &bufferLength);
	
	if (!NT_SUCCESS(status))
	{
		WdfRequestComplete(Request, status);
		return;
	}

	queueContext = GetQueueContext(Queue);
	status = OnSubmitReport(queueContext->deviceContext->device, Request, inputBuffer);

	WdfRequestComplete(Request, status);
}


void EvtWdfDeviceFileCreate(
	WDFDEVICE Device,
	WDFREQUEST Request,
	WDFFILEOBJECT FileObject
)
{

	UNREFERENCED_PARAMETER(Device);

	PUHIDFileContext context;
	context = GetFileContext(FileObject);

	WdfRequestComplete(Request, STATUS_SUCCESS);
}

void EvtWdfFileCleanup(
	WDFFILEOBJECT FileObject
)
{
	PUHIDFileContext context;
	context = GetFileContext(FileObject);

	if (context->vhfHandle != WDF_NO_HANDLE)
	{
		VhfDelete(context->vhfHandle, FALSE);
	}
}