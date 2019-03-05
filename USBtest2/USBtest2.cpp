// USBtest2.cpp : �������̨Ӧ�ó������ڵ㡣
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

//�������Scsi����
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
		sptdwb.sptd.CdbLength = cdbLen;		//CDB����ĳ���
		sptdwb.sptd.SenseInfoLength = 26;	//24;
		sptdwb.sptd.DataIn = SCSI_IOCTL_DATA_IN;	//������
		sptdwb.sptd.DataTransferLength = dataLen;//sectorSize;	//��ȡ���ݵĳ���
		sptdwb.sptd.TimeOutValue = 200;		//��Ӧ��ʱʱ��
		sptdwb.sptd.DataBuffer = data;		//��ȡ�����ݵĴ��ָ��
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

//��������Scsiд��
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
		sptdwb.sptd.DataIn = SCSI_IOCTL_DATA_OUT;	//д����
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
			lasterr = GetLastError();  //�����31��ʾ��������
			if (lasterr == 55) {  //�����55��ʾ�豸�ѵ�
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
	//�����豸ʵ������
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
	//��������һ���������������б�������豸��Ϣ���豸��Ϣ�����,������ʵ����һ��ָ��
	HDEVINFO hardware_dev_info_set = SetupDiGetClassDevs(&GUID_DEVINTERFACE_DISK, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

	int devidx = 0;

	while (INVALID_HANDLE_VALUE != hardware_dev_info_set)
	{
		//ĳ���豸����Ϣ
		SP_DEVINFO_DATA devInfoData;
		devInfoData.cbSize = sizeof(devInfoData);
		//�� 0 ����豸��ʼ���� �豸��Ϣ�� �����ص���Ϣ����devInfoData��
		if (SetupDiEnumDeviceInfo(hardware_dev_info_set, devidx, &devInfoData))//
		{
			if (_IsSupportedDevice(devInfoData, hardware_dev_info_set))
			{
				SP_DEVICE_INTERFACE_DATA devInterfaceData;
				devInterfaceData.cbSize = sizeof(devInterfaceData);
				//����devidx �� hardware_dev_info_set  �����ҵ���Ӧ�豸�Ľӿ�
				SetupDiEnumDeviceInterfaces(hardware_dev_info_set, nullptr, &GUID_DEVINTERFACE_DISK, devidx, &devInterfaceData);

				DWORD interfaceDetailSize = 0;
				SetupDiGetInterfaceDeviceDetail(hardware_dev_info_set, &devInterfaceData, nullptr, interfaceDetailSize,
					&interfaceDetailSize, nullptr);

				PSP_DEVICE_INTERFACE_DETAIL_DATA pDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)new char[interfaceDetailSize];
				pDetailData->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);
				//�豸·�� pDetailData->DevicePath 
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
		NULL);  //���豸






	return 0;
}

