#include <Windows.h>
#include <vector>
#include <d3dkmthk.h>
#include <iostream>
#include <cfgmgr32.h>
#include <iomanip>
#include <initguid.h>
#include <ntddvdeo.h>
#include "phconfig.h"
#include <cassert>
#include <ntstatus.h>
#include "get.h"
#pragma comment(lib, "cfgmgr32.lib")
#pragma comment(lib, "gdi32.lib")

 

GpuMonitor::GpuMonitor(DWORD targetProcessId) : targetProcessId_(targetProcessId)
{
}

GpuMonitor::~GpuMonitor()
{
}

bool GpuMonitor::start()
{
	PhInitializeWindowsVersion();
	//PhHeapInitialization(0, 0);

	EtGpuSupported_ = windowsVersion_ >= WINDOWS_10_RS4;
	EtD3DEnabled_ = EtGpuSupported_;
	if (initializeD3DStatistics()) {
		EtGpuEnabled_ = true;
	}
	else {
		std::cout << "initializeD3DStatisticsÊ§°Ü" << std::endl;
	}

	return true;
};

DOUBLE GpuMonitor::GPU_memory() {
	return (EtGpuDedicatedLimit_ / 1024 / 1024);
}

DOUBLE GpuMonitor::GPU_memory_used() {
	return (EtGpuDedicatedUsage_ / 1024 / 1024);
}

bool GpuMonitor::PhInitializeWindowsVersion() {
	windowsVersion_ = getWindowsVersion();
	if (windowsVersion_ == 1)
	{
		return false;
	}
	return true;
}

void GpuMonitor::EtpUpdateSystemSegmentInformation()
{
	ULONG i;
	ULONG j;
	PETP_GPU_ADAPTER gpuAdapter;
	D3DKMT_QUERYSTATISTICS queryStatistics;
	ULONG64 dedicatedUsage = 0;
	ULONG64 sharedUsage = 0;
	for (i = 0; i < gpuAdapterList_.size(); i++)
	{
		gpuAdapter = gpuAdapterList_[i];

		for (j = 0; j < gpuAdapter->SegmentCount; j++)
		{
			memset(&queryStatistics, 0, sizeof(D3DKMT_QUERYSTATISTICS));
			queryStatistics.Type = D3DKMT_QUERYSTATISTICS_SEGMENT;
			queryStatistics.AdapterLuid = gpuAdapter->AdapterLuid;
			queryStatistics.QuerySegment.SegmentId = j;
			NTSTATUS status = D3DKMTQueryStatistics(&queryStatistics);
			if (status < 0)
			{
				std::cout << "D3DKMTQueryStatistics failed with status: " << status << std::endl;
				break;
			}

			if (status >= 0)
			{
				ULONG64 bytesCommitted;
				ULONG aperture;

				if (windowsVersion_ >= WINDOWS_8)
				{
					bytesCommitted = queryStatistics.QueryResult.SegmentInformation.BytesResident;
					aperture = queryStatistics.QueryResult.SegmentInformation.Aperture;
				}
				else
				{
					PD3DKMT_QUERYSTATISTICS_SEGMENT_INFORMATION_V1 segmentInfo;
					segmentInfo = (PD3DKMT_QUERYSTATISTICS_SEGMENT_INFORMATION_V1)&queryStatistics.QueryResult;
					bytesCommitted = segmentInfo->BytesResident;
					aperture = queryStatistics.QueryResult.SegmentInformation.Aperture;
				}

				if (aperture) // RtlCheckBit(&gpuAdapter->ApertureBitMap, j)
					sharedUsage += bytesCommitted;
				else
					dedicatedUsage += bytesCommitted;
			}
		}
	}

	EtGpuDedicatedUsage_ = dedicatedUsage;
}

bool GpuMonitor::initializeD3DStatistics()
{
	HMODULE hNtdll = GetModuleHandle(L"ntdll.dll");
	ULONG deviceInterfaceListLength = 0;
	std::vector<void*> deviceAdapterList;
	PWSTR deviceInterfaceList;
	D3DKMT_OPENADAPTERFROMDEVICENAME openAdapterFromDeviceName;
	D3DKMT_QUERYSTATISTICS queryStatistics;

	typedef BOOLEAN(WINAPI* RtlFreeHeap) (PVOID HeapBase, ULONG Flags, PVOID BaseAddress);
	RtlFreeHeap fnRtlFreeHeap;
	typedef void* (__cdecl* RtlAllocateHeap)(void*, unsigned long, unsigned __int64);
	RtlAllocateHeap fnRtlAllocateHeap;
	fnRtlAllocateHeap = (RtlAllocateHeap)GetProcAddress(hNtdll, "RtlAllocateHeap");
	fnRtlFreeHeap = (RtlFreeHeap)GetProcAddress(hNtdll, "RtlFreeHeap");

	PhHeapHandle_ = HeapCreate(0, 0, 0);
	if (PhHeapHandle_ == NULL)
	{
		std::cout << "Failed to create heap." << std::endl;
		return false;
	}

	if (CM_Get_Device_Interface_List_Size(
		&deviceInterfaceListLength,
		(PGUID)&GUID_DISPLAY_DEVICE_ARRIVAL,
		NULL,
		CM_GET_DEVICE_INTERFACE_LIST_PRESENT
	) != CR_SUCCESS)
	{
		std::cout << "CM_Get_Device_Interface_List_Size failed.\n";
		return FALSE;
	}

	deviceInterfaceList = (PWSTR)fnRtlAllocateHeap(PhHeapHandle_, HEAP_GENERATE_EXCEPTIONS, deviceInterfaceListLength * sizeof(WCHAR));
	memset(deviceInterfaceList, 0, deviceInterfaceListLength * sizeof(WCHAR));
	if (CM_Get_Device_Interface_ListW(
		(PGUID)&GUID_DISPLAY_DEVICE_ARRIVAL,
		NULL,
		deviceInterfaceList,
		deviceInterfaceListLength,
		CM_GET_DEVICE_INTERFACE_LIST_PRESENT
	) != CR_SUCCESS)
	{
		fnRtlFreeHeap(PhHeapHandle_, 0, deviceInterfaceList);
		std::cout << "CM_Get_Device_Interface_List failed.\n";
		return FALSE;
	}
	deviceAdapterList.resize(0);


	for (PWSTR deviceInterface = deviceInterfaceList; *deviceInterface; deviceInterface += wcslen(deviceInterface) + 1)
	{
		deviceAdapterList.push_back(deviceInterface);
	}
	for (ULONG i = 0; i < deviceAdapterList.size(); i++)
	{
		memset(&openAdapterFromDeviceName, 0, sizeof(D3DKMT_OPENADAPTERFROMDEVICENAME));
		openAdapterFromDeviceName.pDeviceName = (PCWSTR)deviceAdapterList[i];

		NTSTATUS status = D3DKMTOpenAdapterFromDeviceName(&openAdapterFromDeviceName);
		if (status < 0)
		{
			std::wcout << "fnD3DKMTOpenAdapterFromDeviceName: " << openAdapterFromDeviceName.pDeviceName << "failed" << std::endl;
			continue;
		}

		if (EtGpuSupported_)
		{
			D3DKMT_SEGMENTSIZEINFO segmentInfo;
			memset(&segmentInfo, 0, sizeof(D3DKMT_SEGMENTSIZEINFO));
			NTSTATUS Adapterstatus = EtQueryAdapterInformation(
				openAdapterFromDeviceName.hAdapter,
				KMTQAITYPE_GETSEGMENTSIZE,
				&segmentInfo,
				sizeof(D3DKMT_SEGMENTSIZEINFO)
			);

			if (Adapterstatus >= 0) {
				EtGpuDedicatedLimit_ += segmentInfo.DedicatedVideoMemorySize;
			}
		}
		memset(&queryStatistics, 0, sizeof(D3DKMT_QUERYSTATISTICS));
		queryStatistics.Type = D3DKMT_QUERYSTATISTICS_ADAPTER;
		queryStatistics.AdapterLuid = openAdapterFromDeviceName.AdapterLuid;

		NTSTATUS status1 = D3DKMTQueryStatistics(&queryStatistics);
		if (status1 >= 0) {
			PETP_GPU_ADAPTER gpuAdapter = new ETP_GPU_ADAPTER();
			gpuAdapter->AdapterLuid = openAdapterFromDeviceName.AdapterLuid;
			gpuAdapter->NodeCount = queryStatistics.QueryResult.AdapterInformation.NodeCount;
			gpuAdapter->SegmentCount = queryStatistics.QueryResult.AdapterInformation.NbSegments;
			gpuAdapterList_.push_back(gpuAdapter);
			for (ULONG ii = 0; ii < queryStatistics.QueryResult.AdapterInformation.NbSegments; ii++)
			{
				memset(&queryStatistics, 0, sizeof(D3DKMT_QUERYSTATISTICS));
				queryStatistics.Type = D3DKMT_QUERYSTATISTICS_SEGMENT;
				queryStatistics.AdapterLuid = openAdapterFromDeviceName.AdapterLuid;
				queryStatistics.QuerySegment.SegmentId = ii;
				if (status1 >= 0)
				{
					ULONG64 commitLimit;
					ULONG aperture;

					if (windowsVersion_ >= WINDOWS_8)
					{
						commitLimit = queryStatistics.QueryResult.SegmentInformation.CommitLimit;
						aperture = queryStatistics.QueryResult.SegmentInformation.Aperture;
					}
					else
					{
						PD3DKMT_QUERYSTATISTICS_SEGMENT_INFORMATION_V1 segmentInfo;

						segmentInfo = (PD3DKMT_QUERYSTATISTICS_SEGMENT_INFORMATION_V1)&queryStatistics.QueryResult;
						commitLimit = segmentInfo->CommitLimit;
						aperture = segmentInfo->Aperture;
					}
					if (!EtGpuSupported_ || !EtD3DEnabled_) // Note: Changed to RS4 due to reports of BSODs on LTSB versions of RS3
					{
						if (aperture)
							continue;
						else
							EtGpuDedicatedLimit_ += commitLimit;
					}
				}
			}
		}
		EtCloseAdapterHandle(openAdapterFromDeviceName.hAdapter);
	}

	fnRtlFreeHeap(PhHeapHandle_, 0, deviceInterfaceList);

	return true;
}

NTSTATUS GpuMonitor::EtQueryAdapterInformation(D3DKMT_HANDLE AdapterHandle, KMTQUERYADAPTERINFOTYPE InformationClass, PVOID Information, UINT32 InformationLength)
{
	D3DKMT_QUERYADAPTERINFO queryAdapterInfo;

	memset(&queryAdapterInfo, 0, sizeof(D3DKMT_QUERYADAPTERINFO));
	queryAdapterInfo.hAdapter = AdapterHandle;
	queryAdapterInfo.Type = InformationClass;
	queryAdapterInfo.pPrivateDriverData = Information;
	queryAdapterInfo.PrivateDriverDataSize = InformationLength;

	return D3DKMTQueryAdapterInfo(&queryAdapterInfo);
}

BOOLEAN GpuMonitor::EtCloseAdapterHandle(D3DKMT_HANDLE AdapterHandle)
{
	return BOOLEAN();
}

RtlGetVersion fnRtlGetVersion;

void* getFuncFromNtdll(HMODULE dllHandle, const char* funName)
{
	void* func = nullptr;
	func = GetProcAddress(dllHandle, funName);
	if (func == nullptr) {
		std::cout << "getFuncFromNtdll Error.\n";
		return nullptr;
	}

	return func;
}

ULONG getWindowsVersion() {
	ULONG WindowsVersion = 0;
	HMODULE hNtdll = LoadLibrary(L"ntdll.dll");
	LONG ntStatus;
	ULONG    majorVersion = 0;
	ULONG    minorVersion = 0;
	ULONG    buildVersion = 0;
	RTL_OSVERSIONINFOW VersionInformation = { 0 };
	if (hNtdll == nullptr) {
		std::cout << "LoadLibrary Error: " << GetLastError() << std::endl;
		return 1;
	}

	fnRtlGetVersion = (RtlGetVersion)getFuncFromNtdll(hNtdll, "RtlGetVersion");
	if (fnRtlGetVersion == NULL) {
		std::cout << "Get fnRtlGetVersion Error.\n";
		return false;
	}

	if (fnRtlGetVersion == nullptr) {
		std::cout << "fnRtlGetVersion is nullptr." << std::endl;
		return 1;
	}

	VersionInformation.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
	ntStatus = fnRtlGetVersion(&VersionInformation);
	if (ntStatus != 0) {
		std::cout <<ntStatus << "fnRtlGetVersion Error.\n";
		return 1;
	}

	majorVersion = VersionInformation.dwMajorVersion;
	minorVersion = VersionInformation.dwMinorVersion;
	buildVersion = VersionInformation.dwBuildNumber;

	if (majorVersion == 6 && minorVersion < 1 || majorVersion < 6)
	{
		WindowsVersion = WINDOWS_ANCIENT;
	}
	// Windows 7, Windows Server 2008 R2
	else if (majorVersion == 6 && minorVersion == 1)
	{
		WindowsVersion = WINDOWS_7;
	}
	// Windows 8, Windows Server 2012
	else if (majorVersion == 6 && minorVersion == 2)
	{
		WindowsVersion = WINDOWS_8;
	}
	// Windows 8.1, Windows Server 2012 R2
	else if (majorVersion == 6 && minorVersion == 3)
	{
		WindowsVersion = WINDOWS_8_1;
	}
	// Windows 10, Windows Server 2016
	else if (majorVersion == 10 && minorVersion == 0)
	{
		if (buildVersion >= 22500)
		{
			WindowsVersion = WINDOWS_11_22H1;
		}
		else if (buildVersion >= 22000)
		{
			WindowsVersion = WINDOWS_11;
		}
		else if (buildVersion >= 19044)
		{
			WindowsVersion = WINDOWS_10_21H2;
		}
		else if (buildVersion >= 19043)
		{
			WindowsVersion = WINDOWS_10_21H1;
		}
		else if (buildVersion >= 19042)
		{
			WindowsVersion = WINDOWS_10_20H2;
		}
		else if (buildVersion >= 19041)
		{
			WindowsVersion = WINDOWS_10_20H1;
		}
		else if (buildVersion >= 18363)
		{
			WindowsVersion = WINDOWS_10_19H2;
		}
		else if (buildVersion >= 18362)
		{
			WindowsVersion = WINDOWS_10_19H1;
		}
		else if (buildVersion >= 17763)
		{
			WindowsVersion = WINDOWS_10_RS5;
		}
		else if (buildVersion >= 17134)
		{
			WindowsVersion = WINDOWS_10_RS4;
		}
		else if (buildVersion >= 16299)
		{
			WindowsVersion = WINDOWS_10_RS3;
		}
		else if (buildVersion >= 15063)
		{
			WindowsVersion = WINDOWS_10_RS2;
		}
		else if (buildVersion >= 14393)
		{
			WindowsVersion = WINDOWS_10_RS1;
		}
		else if (buildVersion >= 10586)
		{
			WindowsVersion = WINDOWS_10_TH2;
		}
		else if (buildVersion >= 10240)
		{
			WindowsVersion = WINDOWS_10;
		}
		else
		{
			WindowsVersion = WINDOWS_10;
		}
	}
	else
	{
		WindowsVersion = WINDOWS_NEW;
	}

	return WindowsVersion;
}

