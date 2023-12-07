#ifndef _GET_H
#define _GET_H
#include <Windows.h>
#include <vector>
#include <string>
#include <stdint.h>
#include <d3dkmthk.h>
#include <iostream>
#include "phconfig.h"

typedef GUID* PGUID;

typedef struct _ETP_GPU_ADAPTER
{
	_ETP_GPU_ADAPTER()
	{
	}

	LUID AdapterLuid;
	ULONG SegmentCount;
	ULONG NodeCount;
	//ULONG FirstNodeIndex;

} ETP_GPU_ADAPTER, * PETP_GPU_ADAPTER;
union FLOAT_ULONG64
{
	FLOAT float_;
	ULONG64 ulong64_;
};

typedef struct _D3DKMT_QUERYSTATISTICS_SEGMENT_INFORMATION_V1
{
	ULONG CommitLimit;
	ULONG BytesCommitted;
	ULONG BytesResident;
	D3DKMT_QUERYSTATISTICS_MEMORY Memory;
	ULONG Aperture; // boolean
	ULONGLONG TotalBytesEvictedByPriority[D3DKMT_MaxAllocationPriorityClass];
	ULONG64 SystemMemoryEndAddress;

	struct
	{
		ULONG64 PreservedDuringStandby : 1;
		ULONG64 PreservedDuringHibernate : 1;
		ULONG64 PartiallyPreservedDuringHibernate : 1;
		ULONG64 Reserved : 61;
	} PowerFlags;
	ULONG64 Reserved[7];
} D3DKMT_QUERYSTATISTICS_SEGMENT_INFORMATION_V1, * PD3DKMT_QUERYSTATISTICS_SEGMENT_INFORMATION_V1;

typedef struct _RTL_HEAP_PARAMETERS {
	ULONG Length;
	SIZE_T SegmentReserve;
	SIZE_T SegmentCommit;
	SIZE_T DeCommitFreeBlockThreshold;
	SIZE_T DeCommitTotalFreeThreshold;
	SIZE_T MaximumAllocationSize;
	SIZE_T VirtualMemoryThreshold;
	SIZE_T InitialCommit;
	SIZE_T InitialReserve;
	//PRTL_HEAP_COMMIT_ROUTINE CommitRoutine;
	SIZE_T Reserved[2];
} RTL_HEAP_PARAMETERS, * PRTL_HEAP_PARAMETERS;

typedef NTSTATUS
(NTAPI* PRTL_HEAP_COMMIT_ROUTINE)(
	IN PVOID Base,
	IN OUT PVOID* CommitAddress,
	IN OUT PSIZE_T CommitSize
);

typedef LONG(WINAPI* RtlGetVersion) (RTL_OSVERSIONINFOW*);
ULONG getWindowsVersion();

class GpuMonitor {
public:
	GpuMonitor() = delete;
	GpuMonitor(DWORD targetProcessId);
	~GpuMonitor();

	bool start();
	DOUBLE GPU_memory();
	DOUBLE GPU_memory_used();
	void EtpUpdateSystemSegmentInformation();
private:
	bool initializeD3DStatistics();
	bool PhInitializeWindowsVersion();
	BOOLEAN EtCloseAdapterHandle(
		_In_ D3DKMT_HANDLE AdapterHandle
	);
	NTSTATUS EtQueryAdapterInformation(
		_In_ D3DKMT_HANDLE AdapterHandle,
		_In_ KMTQUERYADAPTERINFOTYPE InformationClass,
		_Out_writes_bytes_opt_(InformationLength) PVOID Information,
		_In_ UINT32 InformationLength
	);
private:
	BOOLEAN EtGpuEnabled_ = FALSE;
	BOOLEAN EtGpuSupported_ = FALSE;
	BOOLEAN EtD3DEnabled_ = FALSE;
	DWORD targetProcessId_ = 0;
	ULONG windowsVersion_ = { 0 };
	PVOID PhHeapHandle_ = NULL;
	ULONG64 EtGpuDedicatedLimit_ = { 0 };
	ULONG64 EtGpuDedicatedUsage_ = { 0 };

	std::vector<FLOAT_ULONG64> dataList_;
	std::vector<PETP_GPU_ADAPTER> gpuAdapterList_;
};
#endif
