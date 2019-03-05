// USBtest2.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>
#include <ntddscsi.h> 
#include <ioapiset.h>
#include "spti.h"

#include<iostream>
#include<winioctl.h>
#include <setupapi.h>
#include <devpkey.h>
#include <map>



BOOL ReadFromScsi(HANDLE fileHandle, int cdbLen, void *cdb, int dataLen, char *data);
BOOL WriteToScsi(HANDLE fileHandle, int cdbLen, void *cdb, int dataLen, char *data);
void ScanDevice();
bool _IsSupportedDevice(SP_DEVINFO_DATA devInfoData, HDEVINFO deviceInfoSet);
std::wstring _GetDeviceHardwareId(SP_DEVINFO_DATA devInfoData, HDEVINFO deviceInfoSet);


std::map<std::wstring, std::wstring> m_SupportedHardwareSpecificsMap = {
	{ L"BuildWinDiskInterface", L"Buildwin" },
	{ L"BuildWin", L"VID_1908" },
	{ L"iPhoneForTest", L"VID_05AC&PID_12A8" }
};

//发命令从Scsi读出
BOOL ReadFromScsi(HANDLE fileHandle, int cdbLen, void *cdb, int dataLen, char *data)
{
	BOOL	status;
	ULONG	length = 0;
	ULONG	returned = 0;
	SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER sptdwb;
	
	int retry_times = 10;
	int lasterr;
	do {
		sptdwb.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
		sptdwb.sptd.PathId = 0;
		sptdwb.sptd.TargetId = 1;
		sptdwb.sptd.Lun = 0;
		sptdwb.sptd.CdbLength = cdbLen;		//CDB命令的长度
		sptdwb.sptd.SenseInfoLength = 26;	//24;
		sptdwb.sptd.DataIn = SCSI_IOCTL_DATA_IN;	//读数据
		sptdwb.sptd.DataTransferLength = dataLen;//sectorSize;	//读取数据的长度
		sptdwb.sptd.TimeOutValue = 200;		//响应超时时间
		sptdwb.sptd.DataBuffer = data;		//读取的数据的存放指针
		sptdwb.sptd.SenseInfoOffset =
			offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);

		memcpy(sptdwb.sptd.Cdb, cdb, cdbLen);
		length = sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER);
		status = DeviceIoControl(fileHandle,
			IOCTL_SCSI_PASS_THROUGH_DIRECT,
			&sptdwb,
			length,
			&sptdwb,
			length,
			&returned,
			FALSE);
		if (status == 0) {
			lasterr = GetLastError();
			if (lasterr == 55) {
				return status;
			}
			Sleep(20);
			if (retry_times-- > 0) {
				continue;
			}
		}
		return status;
	} while (1);
}

//发命令向Scsi写入
BOOL WriteToScsi(HANDLE fileHandle, int cdbLen, void *cdb, int dataLen, char *data)
{
	SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER sptdwb;
	BOOL	status;
	ULONG	length = 0,
		returned = 0;
	int retry_times = 10;
	int lasterr;
	do {
		sptdwb.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
		sptdwb.sptd.PathId = 0;
		sptdwb.sptd.TargetId = 1;
		sptdwb.sptd.Lun = 0;
		sptdwb.sptd.CdbLength = cdbLen;
		sptdwb.sptd.SenseInfoLength = 26;	//24;
		sptdwb.sptd.DataIn = SCSI_IOCTL_DATA_OUT;	//写数据
		sptdwb.sptd.DataTransferLength = dataLen;//sectorSize;
		sptdwb.sptd.TimeOutValue = 200;
		sptdwb.sptd.DataBuffer = data;
		sptdwb.sptd.SenseInfoOffset =
			offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);

		memcpy(sptdwb.sptd.Cdb, cdb, cdbLen);
		length = sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER);
		status = DeviceIoControl(fileHandle,
			IOCTL_SCSI_PASS_THROUGH_DIRECT,
			&sptdwb,
			length,
			&sptdwb,
			length,
			&returned,
			FALSE);
		if (status == 0) {
			lasterr = GetLastError();  //错误号31表示发送阻塞
			if (lasterr == 55) {  //错误号55表示设备已掉
				return status;
			}
			Sleep(20);
			if (retry_times-- > 0) {
				continue;
				//return status;
			}
		}
		return status;
	} while (1);
}

std::wstring _GetDeviceHardwareId(SP_DEVINFO_DATA devInfoData, HDEVINFO deviceInfoSet)
{
	DEVPROPTYPE propType = 0;
	DWORD bufferSize = 0;
	std::wstring devHardwareIds = L"";
	devHardwareIds.resize(1000);
	//检索设备实例属性
	SetupDiGetDeviceProperty(deviceInfoSet, &devInfoData, &DEVPKEY_Device_HardwareIds, &propType,
		reinterpret_cast<PBYTE>(&devHardwareIds[0]), devHardwareIds.size(), &bufferSize, 0);

	return devHardwareIds;
}

bool _IsSupportedDevice(SP_DEVINFO_DATA devInfoData, HDEVINFO deviceInfoSet)
{
	std::wstring devHardwareId = _GetDeviceHardwareId(devInfoData, deviceInfoSet);
	int a = 1;
	for each (auto item in m_SupportedHardwareSpecificsMap)
	{
		if (devHardwareId.find(item.second) != std::wstring::npos)
		{
			return true;
		}
	}

	return false;
}

void ScanDevice()
{
	//函数返回一个包含本机上所有被请求的设备信息的设备信息集句柄,这里其实就是一个指针
	HDEVINFO hardware_dev_info_set = SetupDiGetClassDevs(&GUID_DEVINTERFACE_DISK, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

	int devidx = 0;

	while (INVALID_HANDLE_VALUE != hardware_dev_info_set)
	{
		//某个设备的信息
		SP_DEVINFO_DATA devInfoData;
		devInfoData.cbSize = sizeof(devInfoData);
		//从 0 序号设备开始遍历 设备信息集 ，返回的信息放在devInfoData中
		if (SetupDiEnumDeviceInfo(hardware_dev_info_set, devidx, &devInfoData))//
		{
			if (_IsSupportedDevice(devInfoData, hardware_dev_info_set))
			{
				SP_DEVICE_INTERFACE_DATA devInterfaceData;
				devInterfaceData.cbSize = sizeof(devInterfaceData);
				//根据devidx 在 hardware_dev_info_set  里面找到对应设备的接口
				SetupDiEnumDeviceInterfaces(hardware_dev_info_set, nullptr, &GUID_DEVINTERFACE_DISK, devidx, &devInterfaceData);

				DWORD interfaceDetailSize = 0;
				SetupDiGetInterfaceDeviceDetail(hardware_dev_info_set, &devInterfaceData, nullptr, interfaceDetailSize,
					&interfaceDetailSize, nullptr);

				PSP_DEVICE_INTERFACE_DETAIL_DATA pDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)new char[interfaceDetailSize];
				pDetailData->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);
				//设备路径 pDetailData->DevicePath 
				SetupDiGetInterfaceDeviceDetail(hardware_dev_info_set, &devInterfaceData, pDetailData, interfaceDetailSize,
					&interfaceDetailSize, nullptr);

				auto device = _AddDevice(pDetailData->DevicePath, devInfoData);
			}
			devidx++;
			continue;
		}
		else
		{
			break;
		}
	}

	if (INVALID_HANDLE_VALUE != hardware_dev_info_set)
	{
		SetupDiDestroyDeviceInfoList(hardware_dev_info_set);
	}
}



int main(int argc, char * argv[])
{
	wchar_t *filename;

	HANDLE m_fileHandle = NULL;
	m_fileHandle = ::CreateFile(filename,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);  //打开设备






	return 0;
}

