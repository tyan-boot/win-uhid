#include <windows.h>
#include <iostream>
#include <vhf.h>
#include <SetupAPI.h>
#include <initguid.h>
#include <usbiodef.h>

#include "driver.h"


using std::cout;
using std::wcout;
using std::endl;

// \Device\0000009d

UCHAR G_DefaultReportDescriptor[] =
{
   0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
   0x09, 0x06,                    // USAGE (Keyboard)
   0xa1, 0x01,                    // COLLECTION (Application)
   0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
   0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)
   0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)
   0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
   0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
   0x75, 0x01,                    //   REPORT_SIZE (1)
   0x95, 0x08,                    //   REPORT_COUNT (8)
   0x81, 0x02,                    //   INPUT (Data,Var,Abs)
   0x95, 0x01,                    //   REPORT_COUNT (1)
   0x75, 0x08,                    //   REPORT_SIZE (8)
   0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)
   0x95, 0x05,                    //   REPORT_COUNT (5)
   0x75, 0x01,                    //   REPORT_SIZE (1)
   0x05, 0x08,                    //   USAGE_PAGE (LEDs)
   0x19, 0x01,                    //   USAGE_MINIMUM (Num Lock)
   0x29, 0x05,                    //   USAGE_MAXIMUM (Kana)
   0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)
   0x95, 0x01,                    //   REPORT_COUNT (1)
   0x75, 0x03,                    //   REPORT_SIZE (3)
   0x91, 0x03,                    //   OUTPUT (Cnst,Var,Abs)
   0x95, 0x06,                    //   REPORT_COUNT (6)
   0x75, 0x08,                    //   REPORT_SIZE (8)
   0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
   0x25, 0x65,                    //   LOGICAL_MAXIMUM (101)
   0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
   0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))
   0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application)
   0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
   0xc0                           // END_COLLECTION
};


int main()
{
	
	HDEVINFO hDevinfo;
	DWORD deviceIndex = 0;
	BOOL bResult = FALSE;
	SP_DEVINFO_DATA devinfoData{};
	SP_DEVICE_INTERFACE_DATA interfaceData{};
	PSP_DEVICE_INTERFACE_DETAIL_DATA detailData;

	hDevinfo = SetupDiGetClassDevsEx(&GUID_UHID_INTERFACE, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE, NULL, NULL, NULL);
	ZeroMemory(&devinfoData, sizeof(devinfoData));
	devinfoData.cbSize = sizeof(devinfoData);

	interfaceData.cbSize = sizeof(interfaceData);
	bResult = SetupDiEnumDeviceInterfaces(hDevinfo, NULL, &GUID_UHID_INTERFACE, 0, &interfaceData);
	DWORD requiredLength;

	bResult = SetupDiGetDeviceInterfaceDetail(hDevinfo, &interfaceData, NULL, 0, &requiredLength, NULL);
	detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LMEM_FIXED, requiredLength);
	detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
	
	bResult = SetupDiGetDeviceInterfaceDetail(hDevinfo, &interfaceData, detailData, requiredLength, &requiredLength, NULL);
	
	wcout << detailData->DevicePath << endl;

	
	HANDLE hDevice;

	//L"\\\\.\\GLOBALROOT\\Device\\win-uhid-ctl"

	hDevice = CreateFile(detailData->DevicePath,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (hDevice == INVALID_HANDLE_VALUE) {
		DWORD e = GetLastError();
		
		cout << "open device failed" << endl;
		return -1;
	}

	DWORD junk = 0;


	UHIDCreateReq req;

	req.len = sizeof(G_DefaultReportDescriptor);
	req.descriptor = G_DefaultReportDescriptor;

	bResult = DeviceIoControl(hDevice, IOCTL_UHID_CREATE, &req, sizeof(req), NULL, 0, &junk, NULL);
	if (!bResult) {
		DWORD e = GetLastError();

		cout << "DeviceIoControl failed" << endl;
		return -1;
	}

	UCHAR testReport[] = {
		0x00,	// modifier
		0x00,	

		0x04,	// key a
		0x00,
		0x00,

		0x00,
		0x00,
		0x00
	};


	HID_XFER_PACKET report;
	report.reportId = 1;
	report.reportBuffer = testReport;
	report.reportBufferLen = sizeof(testReport);

	bResult = WriteFile(hDevice, &report, sizeof(report), NULL, NULL);
	if (!bResult) {
		DWORD e = GetLastError();

		cout << "WriteFile failed " << e << endl;
		return -1;
	}

	Sleep(100);
	testReport[2] = 0x00;

	bResult = WriteFile(hDevice, &report, sizeof(report), NULL, NULL);
	if (!bResult) {
		DWORD e = GetLastError();

		cout << "WriteFile failed " << e << endl;
		return -1;
	}

	return 0;
}