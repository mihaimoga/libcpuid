/*
 * Copyright 2009  Veselin Georgiev,
 * anrieffNOSPAM @ mgail_DOT.com (convert to gmail)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* freebsd requires _XOPEN_SOURCE 600 for snprintf()
 * for linux it is enough 500 */
#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libcpuid.h"
#include "asm-bits.h"
#include "libcpuid_util.h"
#include "libcpuid_internal.h"
#include "rdtsc.h"

#define MSR_PATH_LEN 32

#if defined (__linux__) || defined (__gnu_linux__)
/* Assuming linux with /dev/cpu/x/msr: */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
struct msr_driver_t { int fd; };
static int rdmsr_supported(void);
static int load_driver(char *msr_path)
{
	const int file_exists   = !access(msr_path, F_OK);
	const int file_readable = !access(msr_path, R_OK);

	if (file_exists && file_readable)
		return 1;
	else if (file_exists && !file_readable)
		return 0;
	else if (getuid() != 0)
		return 0;
	else
		return !system("modprobe msr 2> /dev/null");
}

struct msr_driver_t* cpu_msr_driver_open(void)
{
	return cpu_msr_driver_open_core(0);
}

struct msr_driver_t* cpu_msr_driver_open_core(unsigned core_num)
{
	char msr[MSR_PATH_LEN];
	struct msr_driver_t* handle;
	if (core_num >= cpuid_get_total_cpus()) {
		cpuid_set_error(ERR_INVCNB);
		return NULL;
	}
	if (!rdmsr_supported()) {
		cpuid_set_error(ERR_NO_RDMSR);
		return NULL;
	}
	snprintf(msr, MSR_PATH_LEN, "/dev/cpu/%u/msr", core_num);
	if(!load_driver(msr)) {
		cpuid_set_error(ERR_NO_DRIVER);
		return NULL;
	}
	int fd = open(msr, O_RDONLY);
	if (fd < 0) {
		if (errno == EIO) {
			cpuid_set_error(ERR_NO_RDMSR);
			return NULL;
		}
		cpuid_set_error(ERR_NO_DRIVER);
		return NULL;
	}
	handle = (struct msr_driver_t*) malloc(sizeof(struct msr_driver_t));
	if (!handle) {
		cpuid_set_error(ERR_NO_MEM);
		close(fd);
		return NULL;
	}
	handle->fd = fd;
	return handle;
}

int cpu_rdmsr(struct msr_driver_t* driver, uint32_t msr_index, uint64_t* result)
{
	ssize_t ret;

	if (!driver || driver->fd < 0)
		return cpuid_set_error(ERR_HANDLE);
	ret = pread(driver->fd, result, 8, msr_index);
	if (ret != 8)
		return cpuid_set_error(ERR_INVMSR);
	return 0;
}

int cpu_msr_driver_close(struct msr_driver_t* drv)
{
	if (drv && drv->fd >= 0) {
		close(drv->fd);
		free(drv);
	}
	return 0;
}

/* #endif defined (__linux__) || defined (__gnu_linux__) */

#elif defined (__FreeBSD__) || defined (__DragonFly__)
/* Assuming FreeBSD with /dev/cpuctlX */
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/cpuctl.h>

struct msr_driver_t { int fd; };
static int rdmsr_supported(void);
static int load_driver(char *msr_path)
{
	const int file_exists   = !access(msr_path, F_OK);
	const int file_readable = !access(msr_path, R_OK);

	if (file_exists && file_readable)
		return 1;
	else if (file_exists && !file_readable)
		return 0;
	else if (getuid() != 0)
		return 0;
	else
		return !system("kldload -n cpuctl 2> /dev/null");
}

struct msr_driver_t* cpu_msr_driver_open(void)
{
	return cpu_msr_driver_open_core(0);
}

struct msr_driver_t* cpu_msr_driver_open_core(unsigned core_num)
{
	char msr[MSR_PATH_LEN];
	struct msr_driver_t* handle;
	if (core_num >= cpuid_get_total_cpus()) {
		cpuid_set_error(ERR_INVCNB);
		return NULL;
	}
	if (!rdmsr_supported()) {
		cpuid_set_error(ERR_NO_RDMSR);
		return NULL;
	}
	snprintf(msr, MSR_PATH_LEN, "/dev/cpuctl%u", core_num);
	if(!load_driver(msr)) {
		cpuid_set_error(ERR_NO_DRIVER);
		return NULL;
	}
	int fd = open(msr, O_RDONLY);
	if (fd < 0) {
		if (errno == EIO) {
			cpuid_set_error(ERR_NO_RDMSR);
			return NULL;
		}
		cpuid_set_error(ERR_NO_DRIVER);
		return NULL;
	}
	handle = (struct msr_driver_t*) malloc(sizeof(struct msr_driver_t));
	if (!handle) {
		cpuid_set_error(ERR_NO_MEM);
		return NULL;
	}
	handle->fd = fd;
	return handle;
}

int cpu_rdmsr(struct msr_driver_t* driver, uint32_t msr_index, uint64_t* result)
{
	cpuctl_msr_args_t args;
	args.msr = msr_index;

	if (!driver || driver->fd < 0)
		return cpuid_set_error(ERR_HANDLE);

	if(ioctl(driver->fd, CPUCTL_RDMSR, &args))
		return cpuid_set_error(ERR_INVMSR);

	*result = args.data;
	return 0;
}

int cpu_msr_driver_close(struct msr_driver_t* drv)
{
	if (drv && drv->fd >= 0) {
		close(drv->fd);
		free(drv);
	}
	return 0;
}

/* #endif defined (__FreeBSD__) || defined (__DragonFly__) */

#elif defined (_WIN32)
#include <windows.h>
#include <winioctl.h>
#include <winerror.h>

extern uint8_t cc_x86driver_code[];
extern int cc_x86driver_code_size;
extern uint8_t cc_x64driver_code[];
extern int cc_x64driver_code_size;

struct msr_driver_t {
	char driver_path[MAX_PATH + 1];
	SC_HANDLE scManager;
	volatile SC_HANDLE scDriver;
	HANDLE hhDriver;
	OVERLAPPED ovl;
	int errorcode;
};

static int rdmsr_supported(void);
static int extract_driver(struct msr_driver_t* driver);
static int load_driver(struct msr_driver_t* driver);

struct msr_driver_t* cpu_msr_driver_open(void)
{
	struct msr_driver_t* drv;
	int status;
	if (!rdmsr_supported()) {
		cpuid_set_error(ERR_NO_RDMSR);
		return NULL;
	}

	drv = (struct msr_driver_t*) malloc(sizeof(struct msr_driver_t));
	if (!drv) {
		cpuid_set_error(ERR_NO_MEM);
		return NULL;
	}
	memset(drv, 0, sizeof(struct msr_driver_t));

	if (!extract_driver(drv)) {
		free(drv);
		cpuid_set_error(ERR_EXTRACT);
		return NULL;
	}

	status = load_driver(drv);
	if (!DeleteFile(drv->driver_path))
		debugf(1, "Deleting temporary driver file failed.\n");
	if (!status) {
		cpuid_set_error(drv->errorcode ? drv->errorcode : ERR_NO_DRIVER);
		free(drv);
		return NULL;
	}
	return drv;
}

struct msr_driver_t* cpu_msr_driver_open_core(unsigned core_num)
{
	warnf("cpu_msr_driver_open_core(): parameter ignored (function is the same as cpu_msr_driver_open)\n");
	return cpu_msr_driver_open();
}

typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
static BOOL is_running_x64(void)
{
	BOOL bIsWow64 = FALSE;

	LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(__TEXT("kernel32")), "IsWow64Process");
	if(NULL != fnIsWow64Process)
		fnIsWow64Process(GetCurrentProcess(), &bIsWow64);
	return bIsWow64;
}


static int extract_driver(struct msr_driver_t* driver)
{
	FILE *f;
	if (!GetTempPath(sizeof(driver->driver_path), driver->driver_path)) return 0;
	strcat(driver->driver_path, "TmpRdr.sys");

	f = fopen(driver->driver_path, "wb");
	if (!f) return 0;
	if (is_running_x64())
		fwrite(cc_x64driver_code, 1, cc_x64driver_code_size, f);
	else
		fwrite(cc_x86driver_code, 1, cc_x86driver_code_size, f);
	fclose(f);
	return 1;
}

static BOOL wait_for_service_state(SC_HANDLE hService, DWORD dwDesiredState, SERVICE_STATUS *lpsrvStatus){
	BOOL fOK = FALSE;
	DWORD dwWaitHint;

	if(hService != NULL){
		while(TRUE){
			fOK = QueryServiceStatus(hService, lpsrvStatus);
			if(!fOK)
				break;
			if(lpsrvStatus->dwCurrentState == dwDesiredState)
				break;

			dwWaitHint = lpsrvStatus->dwWaitHint / 10;    // Poll 1/10 of the wait hint
			if (dwWaitHint <  1000)
				dwWaitHint = 1000;  // At most once per second
			if (dwWaitHint > 10000)
				dwWaitHint = 10000; // At least every 10 seconds
			Sleep(dwWaitHint);
		}
	}

	return fOK;
}

static int load_driver(struct msr_driver_t* drv)
{
	LPTSTR		lpszInfo = __TEXT("RDMSR Executor Driver");
	USHORT		uLen = 0;
	SERVICE_STATUS srvStatus = {0};
	BOOL		fRunning = FALSE;
	DWORD		dwLastError;
	LPTSTR		lpszDriverServiceName = __TEXT("TmpRdr");
	TCHAR		lpszDriverName[] = __TEXT("\\\\.\\Global\\TmpRdr");

	if((LPVOID)(drv->scManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) != NULL) {
		drv->scDriver = CreateService(drv->scManager, lpszDriverServiceName, lpszInfo, SERVICE_ALL_ACCESS,
		                              SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
				                      drv->driver_path, NULL, NULL, NULL, NULL, NULL);
		if(drv->scDriver == NULL){
			switch(dwLastError = GetLastError()){
				case ERROR_SERVICE_EXISTS:
				case ERROR_SERVICE_MARKED_FOR_DELETE:{
					LPQUERY_SERVICE_CONFIG lpqsc;
					DWORD dwBytesNeeded;

					drv->scDriver = OpenService(drv->scManager, lpszDriverServiceName, SERVICE_ALL_ACCESS);
					if(drv->scDriver == NULL){
						debugf(1, "Error opening service: %d\n", GetLastError());
						break;
					}

					QueryServiceConfig(drv->scDriver, NULL, 0, &dwBytesNeeded);
					if((dwLastError = GetLastError()) == ERROR_INSUFFICIENT_BUFFER){
						lpqsc = calloc(1, dwBytesNeeded);
						if(!QueryServiceConfig(drv->scDriver, lpqsc, dwBytesNeeded, &dwBytesNeeded)){
							free(lpqsc);
							debugf(1, "Error query service config(adjusted buffer): %d\n", GetLastError());
							goto clean_up;
						}
						else{
							free(lpqsc);
						}
					}
					else{
						debugf(1, "Error query service config: %d\n", dwLastError);
						goto clean_up;
					}

					break;
				}
				case ERROR_ACCESS_DENIED:
					drv->errorcode = ERR_NO_PERMS;
					break;
				default:
					debugf(1, "Create driver service failed: %d\n", dwLastError);
					break;
			}
		}
		if(drv->scDriver != NULL){
			if(StartService(drv->scDriver, 0, NULL)){
				if(!wait_for_service_state(drv->scDriver, SERVICE_RUNNING, &srvStatus)){
					debugf(1, "Driver load failed.\n");
					DeleteService(drv->scDriver);
					CloseServiceHandle(drv->scManager);
					drv->scDriver = NULL;
					goto clean_up;
				} else {
					fRunning = TRUE;
				}
			} else{
				if((dwLastError = GetLastError()) == ERROR_SERVICE_ALREADY_RUNNING)
					fRunning = TRUE;
				else{
					debugf(1, "Driver start failed.\n");
					DeleteService(drv->scDriver);
					CloseServiceHandle(drv->scManager);
					drv->scDriver = NULL;
					goto clean_up;
				}

			}
			if(fRunning)
				debugf(1, "Driver already running.\n");
			else
				debugf(1, "Driver loaded.\n");
			CloseServiceHandle(drv->scManager);
			drv->hhDriver = CreateFile(lpszDriverName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
			drv->ovl.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
			return 1;
		}
	} else {
		debugf(1, "Open SCM failed: %d\n", GetLastError());
	}

clean_up:
	if(drv->scManager != NULL){
		CloseServiceHandle(drv->scManager);
		drv->scManager = 0; // pointless
	}
	if(drv->scDriver != NULL){
		if(!DeleteService(drv->scDriver))
			debugf(1, "Delete driver service failed: %d\n", GetLastError());
		CloseServiceHandle(drv->scDriver);
		drv->scDriver = 0;
	}

	return 0;
}

#define FILE_DEVICE_UNKNOWN             0x00000022
#define IOCTL_UNKNOWN_BASE              FILE_DEVICE_UNKNOWN
#define IOCTL_PROCVIEW_RDMSR		CTL_CODE(IOCTL_UNKNOWN_BASE, 0x0803, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

int cpu_rdmsr(struct msr_driver_t* driver, uint32_t msr_index, uint64_t* result)
{
	DWORD dwBytesReturned;
	__int64 msrdata;
	SERVICE_STATUS srvStatus = {0};

	if (!driver)
		return cpuid_set_error(ERR_HANDLE);
	DeviceIoControl(driver->hhDriver, IOCTL_PROCVIEW_RDMSR, &msr_index, sizeof(int), &msrdata, sizeof(__int64), &dwBytesReturned, &driver->ovl);
	GetOverlappedResult(driver->hhDriver, &driver->ovl, &dwBytesReturned, TRUE);
	*result = msrdata;
	return 0;
}

int cpu_msr_driver_close(struct msr_driver_t* drv)
{
	SERVICE_STATUS srvStatus = {0};
	if (drv == NULL) return 0;
	if(drv->scDriver != NULL){
		if (drv->hhDriver) CancelIo(drv->hhDriver);
		if(drv->ovl.hEvent != NULL)
			CloseHandle(drv->ovl.hEvent);
		if (drv->hhDriver) CloseHandle(drv->hhDriver);
		drv->hhDriver = NULL;
		drv->ovl.hEvent = NULL;
		if (ControlService(drv->scDriver, SERVICE_CONTROL_STOP, &srvStatus)){
			if (wait_for_service_state(drv->scDriver, SERVICE_STOPPED, &srvStatus)){
				DeleteService(drv->scDriver);
			}
		}
	}
	return 0;
}

/* endif defined (_WIN32) */

#else /* Unsupported OS */
/* On others OS (i.e., Darwin), we still do not support RDMSR, so supply dummy struct
   and functions */

#define RDMSR_UNSUPPORTED_OS

struct msr_driver_t { int dummy; };
struct msr_driver_t* cpu_msr_driver_open(void)
{
	cpuid_set_error(ERR_NOT_IMP);
	return NULL;
}

struct msr_driver_t* cpu_msr_driver_open_core(unsigned core_num)
{
	UNUSED(core_num);
	cpuid_set_error(ERR_NOT_IMP);
	return NULL;
}

int cpu_rdmsr(struct msr_driver_t* driver, uint32_t msr_index, uint64_t* result)
{
	UNUSED(driver);
	UNUSED(msr_index);
	UNUSED(result);
	return cpuid_set_error(ERR_NOT_IMP);
}

int cpu_msr_driver_close(struct msr_driver_t* driver)
{
	UNUSED(driver);
	return cpuid_set_error(ERR_NOT_IMP);
}

int cpu_rdmsr_range(struct msr_driver_t* handle, uint32_t msr_index, uint8_t highbit,
                    uint8_t lowbit, uint64_t* result)
{
	UNUSED(handle);
	UNUSED(msr_index);
	UNUSED(highbit);
	UNUSED(lowbit);
	UNUSED(result);
	return cpuid_set_error(ERR_NOT_IMP);
}

int cpu_msrinfo(struct msr_driver_t* driver, cpu_msrinfo_request_t which)
{
	UNUSED(driver);
	UNUSED(which);
	return cpuid_set_error(ERR_NOT_IMP);
}

int msr_serialize_raw_data(struct msr_driver_t* handle, const char* filename)
{
	UNUSED(handle);
	UNUSED(filename);
	return cpuid_set_error(ERR_NOT_IMP);
}

#endif /* Unsupported OS */

#ifndef RDMSR_UNSUPPORTED_OS

/* Useful links for hackers:
- AMD MSRs:
  AMD BIOS and Kernel Developer’s Guide (BKDG)
  * AMD Family 10h Processors
  http://support.amd.com/TechDocs/31116.pdf
  * AMD Family 11h Processors
  http://support.amd.com/TechDocs/41256.pdf
  * AMD Family 12h Processors
  http://support.amd.com/TechDocs/41131.pdf
  * AMD Family 14h Processors
  http://support.amd.com/TechDocs/43170_14h_Mod_00h-0Fh_BKDG.pdf
  * AMD Family 15h Processors
  http://support.amd.com/TechDocs/42301_15h_Mod_00h-0Fh_BKDG.pdf
  http://support.amd.com/TechDocs/42300_15h_Mod_10h-1Fh_BKDG.pdf
  http://support.amd.com/TechDocs/49125_15h_Models_30h-3Fh_BKDG.pdf
  http://support.amd.com/TechDocs/50742_15h_Models_60h-6Fh_BKDG.pdf
  http://support.amd.com/TechDocs/55072_AMD_Family_15h_Models_70h-7Fh_BKDG.pdf
  * AMD Family 16h Processors
  http://support.amd.com/TechDocs/48751_16h_bkdg.pdf
  http://support.amd.com/TechDocs/52740_16h_Models_30h-3Fh_BKDG.pdf

  AMD Processor Programming Reference (PPR)
  * AMD Family 17h Processors
  Models 00h-0Fh: https://www.amd.com/system/files/TechDocs/54945-ppr-family-17h-models-00h-0fh-processors.zip
  Models 01h, 08h, Revision B2: https://www.amd.com/system/files/TechDocs/54945_3.03_ppr_ZP_B2_pub.zip
  Model 71h, Revision B0: https://www.amd.com/system/files/TechDocs/56176_ppr_Family_17h_Model_71h_B0_pub_Rev_3.06.zip
  Model 60h, Revision A1: https://www.amd.com/system/files/TechDocs/55922-A1-PUB.zip
  Model 18h, Revision B1: https://www.amd.com/system/files/TechDocs/55570-B1-PUB.zip
  Model 20h, Revision A1: https://www.amd.com/system/files/TechDocs/55772-A1-PUB.zip
  Model 31h, Revision B0 Processors https://www.amd.com/system/files/TechDocs/55803-ppr-family-17h-model-31h-b0-processors.pdf
  Models A0h-AFh, Revision A0: https://www.amd.com/system/files/TechDocs/57243-A0-PUB.zip
  * AMD Family 19h Processors
  Model 01h, Revision B1: https://www.amd.com/system/files/TechDocs/55898_B1_pub_0.50.zip
  Model 21h, Revision B0: https://www.amd.com/system/files/TechDocs/56214-B0-PUB.zip
  Model 51h, Revision A1: https://www.amd.com/system/files/TechDocs/56569-A1-PUB.zip
  Model 11h, Revision B1: https://www.amd.com/system/files/TechDocs/55901_0.25.zip
  Model 61h, Revision B1: https://www.amd.com/system/files/TechDocs/56713-B1-PUB_3.04.zip
  Model 70h, Revision A0: https://www.amd.com/system/files/TechDocs/57019-A0-PUB_3.00.zip

- Intel MSRs:
  Intel® 64 and IA-32 Architectures Software Developer’s Manual
  * Volume 3 (3A, 3B, 3C & 3D): System Programming Guide
  http://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-system-programming-manual-325384.pdf
*/

/* AMD MSRs addresses */
#define MSR_PSTATE_L           0xC0010061
#define MSR_PSTATE_S           0xC0010063
#define MSR_PSTATE_0           0xC0010064
#define MSR_PSTATE_1           0xC0010065
#define MSR_PSTATE_2           0xC0010066
#define MSR_PSTATE_3           0xC0010067
#define MSR_PSTATE_4           0xC0010068
#define MSR_PSTATE_5           0xC0010069
#define MSR_PSTATE_6           0xC001006A
#define MSR_PSTATE_7           0xC001006B
static const uint32_t amd_msr[] = {
	MSR_PSTATE_L,
	MSR_PSTATE_S,
	MSR_PSTATE_0,
	MSR_PSTATE_1,
	MSR_PSTATE_2,
	MSR_PSTATE_3,
	MSR_PSTATE_4,
	MSR_PSTATE_5,
	MSR_PSTATE_6,
	MSR_PSTATE_7,
	CPU_INVALID_VALUE
};

/* Intel MSRs addresses */
#define IA32_MPERF             0xE7
#define IA32_APERF             0xE8
#define IA32_PERF_STATUS       0x198
#define IA32_THERM_STATUS      0x19C
#define MSR_EBL_CR_POWERON     0x2A
#define MSR_TURBO_RATIO_LIMIT  0x1AD
#define MSR_TEMPERATURE_TARGET 0x1A2
#define MSR_PERF_STATUS        0x198
#define MSR_PLATFORM_INFO      0xCE
static const uint32_t intel_msr[] = {
	IA32_MPERF,
	IA32_APERF,
	IA32_PERF_STATUS,
	IA32_THERM_STATUS,
	MSR_EBL_CR_POWERON,
	MSR_TURBO_RATIO_LIMIT,
	MSR_TEMPERATURE_TARGET,
	MSR_PERF_STATUS,
	MSR_PLATFORM_INFO,
	CPU_INVALID_VALUE
};

struct msr_info_t {
	int cpu_clock;
	struct msr_driver_t *handle;
	struct cpu_id_t *id;
	struct internal_id_info_t *internal;
};

static int rdmsr_supported(void)
{
	struct cpu_id_t* id = get_cached_cpuid();
	return id->flags[CPU_FEATURE_MSR];
}

static int perfmsr_measure(struct msr_driver_t* handle, int msr)
{
	int err;
	uint64_t a, b;
	uint64_t x, y;
	err = cpu_rdmsr(handle, msr, &x);
	if (err) return CPU_INVALID_VALUE;
	sys_precise_clock(&a);
	busy_loop_delay(10);
	cpu_rdmsr(handle, msr, &y);
	sys_precise_clock(&b);
	if (a >= b || x > y) return CPU_INVALID_VALUE;
	return (int) ((y - x) / (b - a));
}

static int msr_platform_info_supported(struct msr_info_t *info)
{
	int i;
	static int supported = -1;

	/* Return cached result */
	if(supported >= 0)
		return supported;

	/* List of microarchitectures that provide both "Maximum Non-Turbo Ratio" and "Maximum Efficiency Ratio" values
	Please note Silvermont does not report "Maximum Efficiency Ratio" */
	const struct { int32_t ext_family; int32_t ext_model; } msr_platform_info[] = {
		/* Table 2-12. MSRs in Intel Atom® Processors Based on Goldmont Microarchitecture */
		{ 6, 92 },
		{ 6, 122 },
		/* Table 2-15. MSRs in Processors Based on Nehalem Microarchitecture */
		{ 6, 26 },
		{ 6, 30 },
		{ 6, 37 },
		{ 6, 44 },
		/* Table 2-20. MSRs Supported by Intel® Processors Based on Sandy Bridge Microarchitecture */
		{ 6, 42 },
		{ 6, 45 },
		/* Table 2-25. Additional MSRs Supported by 3rd Generation Intel® CoreTM Processors Based on Ivy Bridge Microarchitecture */
		{ 6, 58 },
		/* Table 2-26. MSRs Supported by the Intel® Xeon® Processor E5 v2 Product Family (Ivy Bridge-E Microarchitecture) */
		{ 6, 62 },
		/* Table 2-29. Additional MSRs Supported by Processors Based on the Haswell and Haswell-E Microarchitectures */
		{ 6, 60 },
		{ 6, 63 },
		{ 6, 69 },
		{ 6, 70 },
		/* Table 2-36. Additional MSRs Common to the Intel® Xeon® Processor D and the Intel® Xeon® Processor E5 v4 Family Based on Broadwell Microarchitecture */
		{ 6, 61 },
		{ 6, 71 },
		{ 6, 79 },
		/* Table 2-39. Additional MSRs Supported by the 6th—13th Generation Intel® CoreTM Processors, 1st—5th Generation Intel® Xeon® Scalable Processor Families, Intel® CoreTM Ultra 7 Processors, 8th Generation Intel® CoreTM i3 Processors, and Intel® Xeon® E Processors */
		/* ==> Skylake */
		{ 6, 78 },
		{ 6, 85 },
		{ 6, 94 },
		/* ==> Kaby Lake */
		{ 6, 142 },
		{ 6, 158 },
		/* ==> Coffee Lake */
		{ 6, 102 },
		{ 6, 142 },
		{ 6, 158 },
		/* ==> Cascade Lake */
		{ 6, 85 },
		/* ==> Comet Lake */
		{ 6, 142 },
		{ 6, 165 },
		/* ==> Ice Lake */
		{ 6, 106 },
		{ 6, 108 },
		{ 6, 126 },
		/* ==> Rocket Lake */
		{ 6, 167 },
		/* ==> Tremont */
		{ 6, 138 },
		{ 6, 150 },
		{ 6, 156 },
		/* ==> Tiger Lake */
		{ 6, 140 },
		/* ==> Alder Lake */
		{ 6, 151 },
		{ 6, 154 },
		{ 6, 190 },
		/* ==> Raptor Lake */
		{ 6, 183 },
		{ 6, 186 },
		{ 6, 191 },
		/* ==> Sapphire Rapids */
		{ 6, 143 },
		/* ==> Emerald Rapids */
		{ 6, 207 },
		/* ==> Meteor Lake */
		{ 6, 170 },
		/* ==> Arrow Lake */
		{ 6, 198 },
		/* ==> Lunar Lake */
		{ 6, 189 },
		/* Table 2-50. MSRs Supported by the Intel® Xeon® Scalable Processor Family with a CPUID Signature DisplayFamily_DisplayModel Value of 06_55H */
		{ 0x6, 0x55 },
		/* Table 2-56. Selected MSRs Supported by Intel® Xeon PhiTM Processors with a CPUID Signature DisplayFamily_DisplayModel Value of 06_57H or 06_85H */
		{ 0x6, 0x57 },
		{ 0x6, 0x85 },
	};

	if(info->id->vendor == VENDOR_INTEL) {
		for(i = 0; i < COUNT_OF(msr_platform_info); i++) {
			if((info->id->x86.ext_family == msr_platform_info[i].ext_family) && (info->id->x86.ext_model == msr_platform_info[i].ext_model)) {
				debugf(2, "Intel CPU with CPUID signature %02X_%02XH supports MSR_PLATFORM_INFO.\n", info->id->x86.ext_family, info->id->x86.ext_model);
				supported = 1;
				return supported;
			}
		}
		debugf(2, "Intel CPU with CPUID signature %02X_%02XH does not support MSR_PLATFORM_INFO.\n", info->id->x86.ext_family, info->id->x86.ext_model);
	}

	supported = 0;
	return supported;
}

static int msr_intel_core_supported(struct msr_info_t *info)
{
	int i;
	static int supported = -1;

	/* Return cached result */
	if(supported >= 0)
		return supported;

	/* List of microarchitectures that provide "Core Voltage" values */
	const struct { int32_t ext_family; int32_t ext_model; } msr_perf_status[] = {
		/* Table 2-20. MSRs Supported by Intel® Processors Based on Sandy Bridge Microarchitecture */
		{ 6, 42 },
		{ 6, 45 },
		/* ==> Ivy Bridge */
		{ 6, 58 },
		{ 6, 62 },
		/* ==> Haswell */
		{ 6, 60 },
		{ 6, 63 },
		{ 6, 69 },
		{ 6, 70 },
		/* ==> Broadwell */
		{ 6, 61 },
		{ 6, 71 },
		{ 6, 79 },
		/* ==> Skylake */
		{ 6, 78 },
		{ 6, 85 },
		{ 6, 94 },
		/* ==> Kaby Lake */
		{ 6, 142 },
		{ 6, 158 },
		/* ==> Coffee Lake */
		{ 6, 102 },
		{ 6, 142 },
		{ 6, 158 },
		/* ==> Cascade Lake */
		{ 6, 85 },
		/* ==> Comet Lake */
		{ 6, 142 },
		{ 6, 165 },
		/* ==> Ice Lake */
		{ 6, 106 },
		{ 6, 108 },
		{ 6, 126 },
		/* ==> Rocket Lake */
		{ 6, 167 },
		/* ==> Tremont */
		{ 6, 138 },
		{ 6, 150 },
		{ 6, 156 },
		/* ==> Tiger Lake */
		{ 6, 140 },
		/* ==> Alder Lake */
		{ 6, 151 },
		{ 6, 154 },
		{ 6, 190 },
		/* ==> Raptor Lake */
		{ 6, 183 },
		{ 6, 186 },
		{ 6, 191 },
		/* ==> Sapphire Rapids */
		{ 6, 143 },
		/* ==> Emerald Rapids */
		{ 6, 207 },
		/* ==> Meteor Lake */
		{ 6, 170 },
		/* Table 2-50. MSRs Supported by the Intel® Xeon® Scalable Processor Family with a CPUID Signature DisplayFamily_DisplayModel Value of 06_55H */
		{ 0x6, 0x55 },
		/* Table 2-56. Selected MSRs Supported by Intel® Xeon PhiTM Processors with a CPUID Signature DisplayFamily_DisplayModel Value of 06_57H or 06_85H */
		{ 0x6, 0x57 },
		{ 0x6, 0x85 },
	};

	if(info->id->vendor == VENDOR_INTEL) {
		for(i = 0; i < COUNT_OF(msr_perf_status); i++) {
			if((info->id->x86.ext_family == msr_perf_status[i].ext_family) && (info->id->x86.ext_model == msr_perf_status[i].ext_model)) {
				debugf(2, "Intel CPU with CPUID signature %02X_%02XH supports MSR_PERF_STATUS.\n", info->id->x86.ext_family, info->id->x86.ext_model);
				supported = 1;
				return supported;
			}
		}
		debugf(2, "Intel CPU with CPUID signature %02X_%02XH does not support MSR_PERF_STATUS.\n", info->id->x86.ext_family, info->id->x86.ext_model);
	}

	supported = 0;
	return supported;
}

static int get_amd_multipliers(struct msr_info_t *info, uint32_t pstate, double *multiplier)
{
	int i, err;
	uint64_t CpuFid, CpuDid, CpuDidLSD;

	/* Constant values needed for 12h family */
	const struct { uint64_t did; double divisor; } divisor_t[] = {
		{ 0x0,    1   },
		{ 0x1,    1.5 },
		{ 0x2,    2   },
		{ 0x3,    3   },
		{ 0x4,    4   },
		{ 0x5,    6   },
		{ 0x6,    8   },
		{ 0x7,    12  },
		{ 0x8,    16  },
	};
	const int num_dids = (int) COUNT_OF(divisor_t);

	/* Constant values for common families */
	const int magic_constant = (info->id->x86.ext_family == 0x11) ? 0x8 : 0x10;
	const int is_apu = (strstr(info->id->brand_str, "APU") != NULL) || (strstr(info->id->brand_str, "Radeon ") != NULL);
	const double divisor = is_apu ? 1.0 : 2.0;

	/* Check if P-state is valid */
	if (pstate < MSR_PSTATE_0 || MSR_PSTATE_7 < pstate)
		return 1;

	/* Overview of AMD CPU microarchitectures: https://en.wikipedia.org/wiki/List_of_AMD_CPU_microarchitectures#Nomenclature */
	switch (info->id->x86.ext_family) {
		case 0x12: /* K10 (Llano) / K12 */
			/* BKDG 12h, page 469
			MSRC001_00[6B:64][8:4] is CpuFid
			MSRC001_00[6B:64][3:0] is CpuDid
			CPU COF is (100MHz * (CpuFid + 10h) / (divisor specified by CpuDid))
			Note: This family contains only APUs */
			err  = cpu_rdmsr_range(info->handle, pstate, 8, 4, &CpuFid);
			err += cpu_rdmsr_range(info->handle, pstate, 3, 0, &CpuDid);
			i = 0;
			while (i < num_dids && divisor_t[i].did != CpuDid)
				i++;
			if (i < num_dids)
				*multiplier = (double) ((CpuFid + magic_constant) / divisor_t[i].divisor);
			else
				err++;
			break;
		case 0x14: /* Bobcat */
			/* BKDG 14h, page 430
			MSRC001_00[6B:64][8:4] is CpuDidMSD
			MSRC001_00[6B:64][3:0] is CpuDidLSD
			PLL COF is (100 MHz * (D18F3xD4[MainPllOpFreqId] + 10h))
			Divisor is (CpuDidMSD + (CpuDidLSD * 0.25) + 1)
			CPU COF is (main PLL frequency specified by D18F3xD4[MainPllOpFreqId]) / (core clock divisor specified by CpuDidMSD and CpuDidLSD)
			Note: This family contains only APUs */
			err  = cpu_rdmsr_range(info->handle, pstate, 8, 4, &CpuDid);
			err += cpu_rdmsr_range(info->handle, pstate, 3, 0, &CpuDidLSD);
			*multiplier = (double) (((info->cpu_clock + 5) / 100 + magic_constant) / (CpuDid + CpuDidLSD * 0.25 + 1));
			break;
		case 0x10: /* K10 */
			/* BKDG 10h, page 429
			MSRC001_00[6B:64][8:6] is CpuDid
			MSRC001_00[6B:64][5:0] is CpuFid
			CPU COF is (100 MHz * (CpuFid + 10h) / (2^CpuDid))
			Note: This family contains only CPUs */
			/* Fall through */
		case 0x11: /* K8 & K10 "hybrid" */
			/* BKDG 11h, page 236
			MSRC001_00[6B:64][8:6] is CpuDid
			MSRC001_00[6B:64][5:0] is CpuFid
			CPU COF is ((100 MHz * (CpuFid + 08h)) / (2^CpuDid))
			Note: This family contains only CPUs */
			/* Fall through */
		case 0x15: /* Bulldozer / Piledriver / Steamroller / Excavator */
			/* BKDG 15h, page 570/580/635/692 (00h-0Fh/10h-1Fh/30h-3Fh/60h-6Fh)
			MSRC001_00[6B:64][8:6] is CpuDid
			MSRC001_00[6B:64][5:0] is CpuFid
			CoreCOF is (100 * (MSRC001_00[6B:64][CpuFid] + 10h) / (2^MSRC001_00[6B:64][CpuDid]))
			Note: This family contains BOTH CPUs and APUs */
			/* Fall through */
		case 0x16: /* Jaguar / Puma */
			/* BKDG 16h, page 549/611 (00h-0Fh/30h-3Fh)
			MSRC001_00[6B:64][8:6] is CpuDid
			MSRC001_00[6B:64][5:0] is CpuFid
			CoreCOF is (100 * (MSRC001_00[6B:64][CpuFid] + 10h) / (2^MSRC001_00[6B:64][CpuDid]))
			Note: This family contains only APUs */
			err  = cpu_rdmsr_range(info->handle, pstate, 8, 6, &CpuDid);
			err += cpu_rdmsr_range(info->handle, pstate, 5, 0, &CpuFid);
			*multiplier = ((double) (CpuFid + magic_constant) / (1ull << CpuDid)) / divisor;
			break;
		case 0x17: /* Zen / Zen+ / Zen 2 */
			/* PPR 17h, pages 30 and 138-139
			MSRC001_00[6B:64][13:8] is CpuDfsId
			MSRC001_00[6B:64][7:0]  is CpuFid
			CoreCOF is (Core::X86::Msr::PStateDef[CpuFid[7:0]] / Core::X86::Msr::PStateDef[CpuDfsId]) * 200 */
			/* Fall through */
		case 0x18: /* Hygon Dhyana */
			/* Note: Dhyana is "mostly a re-branded Zen CPU for the Chinese server market"
			https://www.phoronix.com/news/Hygon-Dhyana-AMD-China-CPUs */
			/* Fall through */
		case 0x19: /* Zen 3 / Zen 3+ / Zen 4 */
			/* PPR for AMD Family 19h Model 70h A0, pages 37 and 206-207
			MSRC001_006[4...B][13:8] is CpuDfsId
			MSRC001_006[4...B][7:0]  is CpuFid
			CoreCOF is (Core::X86::Msr::PStateDef[CpuFid[7:0]]/Core::X86::Msr::PStateDef[CpuDfsId]) *200 */
			err  = cpu_rdmsr_range(info->handle, pstate, 13, 8, &CpuDid);
			err += cpu_rdmsr_range(info->handle, pstate,  7, 0, &CpuFid);
			*multiplier = ((double) CpuFid / CpuDid) * 2;
			break;
		default:
			warnf("get_amd_multipliers(): unsupported CPU extended family: %xh\n", info->id->x86.ext_family);
			err = 1;
			break;
	}

	return err;
}

static uint32_t get_amd_last_pstate_addr(struct msr_info_t *info)
{
	static uint32_t last_addr = 0x0;
	uint64_t reg = 0x0;

	/* The result is cached, need to be computed once */
	if(last_addr != 0x0)
		return last_addr;

	/* Refer links above
	MSRC001_00[6B:64][63] is PstateEn
	PstateEn indicates if the rest of the P-state information in the register is valid after a reset */
	last_addr = MSR_PSTATE_7 + 1;
	while((reg == 0x0) && (last_addr > MSR_PSTATE_0)) {
		last_addr--;
		cpu_rdmsr_range(info->handle, last_addr, 63, 63, &reg);
	}
	return last_addr;
}

static double get_info_min_multiplier(struct msr_info_t *info)
{
	int err;
	double mult;
	uint32_t addr;
	uint64_t reg;

	if(msr_platform_info_supported(info)) {
		/* Refer links above
		Table 35-12.  MSRs in Next Generation Intel Atom Processors Based on the Goldmont Microarchitecture
		Table 35-13.  MSRs in Processors Based on Intel® Microarchitecture Code Name Nehalem
		Table 35-18.  MSRs Supported by Intel® Processors based on Intel® microarchitecture code name Sandy Bridge (Contd.)
		Table 35-23.  Additional MSRs Supported by 3rd Generation Intel® Core™ Processors (based on Intel® microarchitecture code name Ivy Bridge)
		Table 35-24.  MSRs Supported by Intel® Xeon® Processors E5 v2 Product Family (based on Ivy Bridge-E microarchitecture)
		Table 35-27.  Additional MSRs Supported by Processors based on the Haswell or Haswell-E microarchitectures
		Table 35-34.  Additional MSRs Common to Intel® Xeon® Processor D and Intel Xeon Processors E5 v4 Family Based on the Broadwell Microarchitecture
		Table 35-40.  Selected MSRs Supported by Next Generation Intel® Xeon Phi™ Processors with DisplayFamily_DisplayModel Signature 06_57H
		MSR_PLATFORM_INFO[47:40] is Maximum Efficiency Ratio
		Maximum Efficiency Ratio is the minimum ratio that the processor can operates */
		err = cpu_rdmsr_range(info->handle, MSR_PLATFORM_INFO, 47, 40, &reg);
		if (!err) return (double) reg;
	}
	else if(info->id->vendor == VENDOR_AMD || info->id->vendor == VENDOR_HYGON) {
		/* N.B.: Find the last P-state
		get_amd_last_pstate_addr() returns the last P-state, MSR_PSTATE_0 <= addr <= MSR_PSTATE_7 */
		addr = get_amd_last_pstate_addr(info);
		err  = get_amd_multipliers(info, addr, &mult);
		if (!err) return mult;
	}

	return (double) CPU_INVALID_VALUE / 100;
}

static double get_info_cur_multiplier(struct msr_info_t *info)
{
	int err;
	double mult;
	uint64_t reg;

	if(info->id->vendor == VENDOR_INTEL) {
		if(!msr_intel_core_supported(info)) {
			err = cpu_rdmsr(info->handle, MSR_EBL_CR_POWERON, &reg);
			if (!err) return (double) ((reg>>22) & 0x1f);
		}
		else {
			/* Refer links above
			Table 35-2.  IA-32 Architectural MSRs (Contd.)
			IA32_PERF_STATUS[15:0] is Current performance State Value
			[7:0] is 0x0, [15:8] looks like current ratio */
			err = cpu_rdmsr_range(info->handle, IA32_PERF_STATUS, 15, 8, &reg);
			if (!err) return (double) reg;
		}
	}
	else if(info->id->vendor == VENDOR_AMD || info->id->vendor == VENDOR_HYGON) {
		/* Refer links above
		MSRC001_0063[2:0] is CurPstate */
		err  = cpu_rdmsr_range(info->handle, MSR_PSTATE_S, 2, 0, &reg);
		err += get_amd_multipliers(info, MSR_PSTATE_0 + (uint32_t) reg, &mult);
		if (!err) return mult;
	}

	return (double) CPU_INVALID_VALUE / 100;
}

static double get_info_max_multiplier(struct msr_info_t *info)
{
	int err;
	double mult;
	uint64_t reg;

	if(info->id->vendor == VENDOR_INTEL) {
		if(!msr_intel_core_supported(info)) {
			err = cpu_rdmsr(info->handle, IA32_PERF_STATUS, &reg);
			if (!err) return (double) ((reg >> 40) & 0x1f);
		}
		else {
			/* Refer links above
			Table 35-10.  Specific MSRs Supported by Intel® Atom™ Processor C2000 Series with CPUID Signature 06_4DH
			Table 35-12.  MSRs in Next Generation Intel Atom Processors Based on the Goldmont Microarchitecture (Contd.)
			Table 35-13.  MSRs in Processors Based on Intel® Microarchitecture Code Name Nehalem (Contd.)
			Table 35-14.  Additional MSRs in Intel® Xeon® Processor 5500 and 3400 Series
			Table 35-16.  Additional MSRs Supported by Intel Processors (Based on Intel® Microarchitecture Code Name Westmere)
			Table 35-19.  MSRs Supported by 2nd Generation Intel® Core™ Processors (Intel® microarchitecture code name Sandy Bridge)
			Table 35-21.  Selected MSRs Supported by Intel® Xeon® Processors E5 Family (based on Sandy Bridge microarchitecture)
			Table 35-28.  MSRs Supported by 4th Generation Intel® Core™ Processors (Haswell microarchitecture) (Contd.)
			Table 35-30.  Additional MSRs Supported by Intel® Xeon® Processor E5 v3 Family
			Table 35-33.  Additional MSRs Supported by Intel® Core™ M Processors and 5th Generation Intel® Core™ Processors
			Table 35-34.  Additional MSRs Common to Intel® Xeon® Processor D and Intel Xeon Processors E5 v4 Family Based on the Broadwell Microarchitecture
			Table 35-37.  Additional MSRs Supported by 6th Generation Intel® Core™ Processors Based on Skylake Microarchitecture
			Table 35-40.  Selected MSRs Supported by Next Generation Intel® Xeon Phi™ Processors with DisplayFamily_DisplayModel Signature 06_57H
			MSR_TURBO_RATIO_LIMIT[7:0] is Maximum Ratio Limit for 1C */
			err = cpu_rdmsr_range(info->handle, MSR_TURBO_RATIO_LIMIT, 7, 0, &reg);
			if (!err) return (double) reg;
		}
	}
	else if(info->id->vendor == VENDOR_AMD || info->id->vendor == VENDOR_HYGON) {
		/* Refer links above
		MSRC001_0064 is Pb0
		Pb0 is the highest-performance boosted P-state */
		err = get_amd_multipliers(info, MSR_PSTATE_0, &mult);
		if (!err) return mult;
	}

	return (double) CPU_INVALID_VALUE / 100;
}

static int get_info_temperature(struct msr_info_t *info)
{
	int err;
	uint64_t DigitalReadout, ReadingValid, TemperatureTarget;

	if(msr_intel_core_supported(info)) {
		/* Refer links above
		Table 35-2.   IA-32 Architectural MSRs
		IA32_THERM_STATUS[22:16] is Digital Readout
		IA32_THERM_STATUS[31]    is Reading Valid

		Table 35-6.   MSRs Common to the Silvermont Microarchitecture and Newer Microarchitectures for Intel® Atom
		Table 35-13.  MSRs in Processors Based on Intel® Microarchitecture Code Name Nehalem (Contd.)
		Table 35-18.  MSRs Supported by Intel® Processors based on Intel® microarchitecture code name Sandy Bridge (Contd.)
		Table 35-24.  MSRs Supported by Intel® Xeon® Processors E5 v2 Product Family (based on Ivy Bridge-E microarchitecture) (Contd.)
		Table 35-34.  Additional MSRs Common to Intel® Xeon® Processor D and Intel Xeon Processors E5 v4 Family Based on the Broadwell Microarchitecture
		Table 35-40.  Selected MSRs Supported by Next Generation Intel® Xeon Phi™ Processors with DisplayFamily_DisplayModel Signature 06_57H
		MSR_TEMPERATURE_TARGET[23:16] is Temperature Target */
		err  = cpu_rdmsr_range(info->handle, IA32_THERM_STATUS,      22, 16, &DigitalReadout);
		err += cpu_rdmsr_range(info->handle, IA32_THERM_STATUS,      31, 31, &ReadingValid);
		err += cpu_rdmsr_range(info->handle, MSR_TEMPERATURE_TARGET, 23, 16, &TemperatureTarget);
		if(!err && ReadingValid) return (int) (TemperatureTarget - DigitalReadout);
	}

	return CPU_INVALID_VALUE;
}

static double get_info_voltage(struct msr_info_t *info)
{
	int err;
	double VIDStep;
	uint64_t reg, CpuVid;

	if(msr_intel_core_supported(info)) {
		/* Refer links above
		Table 35-18.  MSRs Supported by Intel® Processors based on Intel® microarchitecture code name Sandy Bridge (Contd.)
		MSR_PERF_STATUS[47:32] is Core Voltage
		P-state core voltage can be computed by MSR_PERF_STATUS[37:32] * (float) 1/(2^13). */
		err = cpu_rdmsr_range(info->handle, MSR_PERF_STATUS, 47, 32, &reg);
		if (!err) return (double) reg / (1 << 13);
	}
	else if(info->id->vendor == VENDOR_AMD || info->id->vendor == VENDOR_HYGON) {
		/* Refer links above
		MSRC001_00[6B:64][15:9]  is CpuVid (Jaguar and before)
		MSRC001_00[6B:64][21:14] is CpuVid (Zen)
		MSRC001_0063[2:0] is P-state Status
		BKDG 10h, page 49: voltage = 1.550V - 0.0125V * SviVid (SVI1)
		BKDG 15h, page 50: Voltage = 1.5500 - 0.00625 * Vid[7:0] (SVI2)
		SVI2 since Piledriver (Family 15h, 2nd-gen): Models 10h-1Fh Processors */
		VIDStep = ((info->id->x86.ext_family < 0x15) || ((info->id->x86.ext_family == 0x15) && (info->id->x86.ext_model < 0x10))) ? 0.0125 : 0.00625;
		err = cpu_rdmsr_range(info->handle, MSR_PSTATE_S, 2, 0, &reg);
		if(info->id->x86.ext_family < 0x17)
			err += cpu_rdmsr_range(info->handle, MSR_PSTATE_0 + (uint32_t) reg, 15, 9, &CpuVid);
		else
			err += cpu_rdmsr_range(info->handle, MSR_PSTATE_0 + (uint32_t) reg, 21, 14, &CpuVid);
		if (!err && MSR_PSTATE_0 + (uint32_t) reg <= MSR_PSTATE_7) return 1.550 - VIDStep * CpuVid;
	}

	return (double) CPU_INVALID_VALUE / 100;
}

static double get_info_bus_clock(struct msr_info_t *info)
{
	int err;
	double mult;
	uint32_t addr;
	uint64_t reg;

	if(msr_platform_info_supported(info)) {
		/* Refer links above
		Table 35-12.  MSRs in Next Generation Intel Atom Processors Based on the Goldmont Microarchitecture
		Table 35-13.  MSRs in Processors Based on Intel® Microarchitecture Code Name Nehalem
		Table 35-18.  MSRs Supported by Intel® Processors based on Intel® microarchitecture code name Sandy Bridge (Contd.)
		Table 35-23.  Additional MSRs Supported by 3rd Generation Intel® Core™ Processors (based on Intel® microarchitecture code name Ivy Bridge)
		Table 35-24.  MSRs Supported by Intel® Xeon® Processors E5 v2 Product Family (based on Ivy Bridge-E microarchitecture)
		Table 35-27.  Additional MSRs Supported by Processors based on the Haswell or Haswell-E microarchitectures
		Table 35-40.  Selected MSRs Supported by Next Generation Intel® Xeon Phi™ Processors with DisplayFamily_DisplayModel Signature 06_57H
		MSR_PLATFORM_INFO[15:8] is Maximum Non-Turbo Ratio */
		err = cpu_rdmsr_range(info->handle, MSR_PLATFORM_INFO, 15, 8, &reg);
		if (!err) return (double) info->cpu_clock / reg;
	}
	else if(info->id->vendor == VENDOR_AMD || info->id->vendor == VENDOR_HYGON) {
		/* Refer links above
		MSRC001_0061[6:4] is PstateMaxVal
		PstateMaxVal is the the lowest-performance non-boosted P-state */
		addr = get_amd_last_pstate_addr(info);
		err  = cpu_rdmsr_range(info->handle, MSR_PSTATE_L, 6, 4, &reg);
		err += get_amd_multipliers(info, addr - (uint32_t) reg, &mult);
		if (!err) return (double) info->cpu_clock / mult;
	}

	return (double) CPU_INVALID_VALUE / 100;
}

int cpu_rdmsr_range(struct msr_driver_t* handle, uint32_t msr_index, uint8_t highbit,
                    uint8_t lowbit, uint64_t* result)
{
	int err;
	const uint8_t bits = highbit - lowbit + 1;

	if(highbit > 63 || lowbit > highbit)
		return cpuid_set_error(ERR_INVRANGE);

	err = cpu_rdmsr(handle, msr_index, result);

	if(!err && bits < 64) {
		/* Show only part of register */
		*result >>= lowbit;
		*result &= (1ULL << bits) - 1;
	}

	return err;
}

int cpu_msrinfo(struct msr_driver_t* handle, cpu_msrinfo_request_t which)
{
	static int err = 0, init = 0;
	struct cpu_raw_data_t raw;
	static struct cpu_id_t id;
	static struct internal_id_info_t internal;
	static struct msr_info_t info;

	if (handle == NULL) {
		cpuid_set_error(ERR_HANDLE);
		return CPU_INVALID_VALUE;
	}

	info.handle = handle;
	if (!init) {
		err  = cpuid_get_raw_data(&raw);
		err += cpu_ident_internal(&raw, &id, &internal);
		info.cpu_clock = cpu_clock_measure(250, 1);
		info.id = &id;
		info.internal = &internal;
		init = 1;
	}

	if (err)
		return CPU_INVALID_VALUE;

	switch (which) {
		case INFO_MPERF:
			return perfmsr_measure(handle, IA32_MPERF);
		case INFO_APERF:
			return perfmsr_measure(handle, IA32_APERF);
		case INFO_MIN_MULTIPLIER:
			return (int) (get_info_min_multiplier(&info) * 100);
		case INFO_CUR_MULTIPLIER:
			return (int) (get_info_cur_multiplier(&info) * 100);
		case INFO_MAX_MULTIPLIER:
			return (int) (get_info_max_multiplier(&info) * 100);
		case INFO_TEMPERATURE:
			return get_info_temperature(&info);
		case INFO_THROTTLING:
			return CPU_INVALID_VALUE;
		case INFO_VOLTAGE:
			return (int) (get_info_voltage(&info) * 100);
		case INFO_BCLK:
		case INFO_BUS_CLOCK:
			return (int) (get_info_bus_clock(&info) * 100);
		default:
			return CPU_INVALID_VALUE;
	}
}

int msr_serialize_raw_data(struct msr_driver_t* handle, const char* filename)
{
	int i, j;
	FILE *f;
	uint64_t reg;
	const uint32_t *msr;
	struct cpu_id_t* id;
	static int cpu_clock = 0;

	/* Check if MSR driver is initialized */
	if (handle == NULL)
		return cpuid_set_error(ERR_HANDLE);

	/* Open file descriptor */
	f = ((filename == NULL) || !strcmp(filename, "")) ? stdout : fopen(filename, "wt");
	if (!f)
		return cpuid_set_error(ERR_OPEN);

	/* Get cached decoded CPUID information */
	id = get_cached_cpuid();
	if (id->vendor == VENDOR_UNKNOWN) {
		fclose(f);
		return cpuid_get_error();
	}

	/* Get CPU stock speed */
	if (cpu_clock == 0)
		cpu_clock = cpu_clock_measure(250, 1);

	/* Check if CPU vendor is supported */
	fprintf(f, "vendor_str=%s\nbrand_str=%s\ncpu_clock_measure=%dMHz\n", id->vendor_str, id->brand_str, cpu_clock);
	switch (id->vendor) {
		case VENDOR_HYGON:
		case VENDOR_AMD:   msr = amd_msr;   break;
		case VENDOR_INTEL: msr = intel_msr; break;
		default: fclose(f); return cpuid_set_error(ERR_CPU_UNKN);
	}

	/* Print raw MSR values */
	for (i = 0; msr[i] != CPU_INVALID_VALUE; i++) {
		cpu_rdmsr(handle, msr[i], &reg);
		fprintf(f, "msr[%#08x]=", msr[i]);
		for (j = 56; j >= 0; j -= 8)
			fprintf(f, "%02x ", (int) (reg >> j) & 0xff);
		fprintf(f, "\n");
	}

	/* Close file descriptor */
	if (f != stdout)
		fclose(f);
	return cpuid_set_error(ERR_OK);
}

#endif // RDMSR_UNSUPPORTED_OS
