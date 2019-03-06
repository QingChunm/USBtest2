// USBtest2.cpp : �������̨Ӧ�ó������ڵ㡣
//
//һ�������������ͷ�ļ��İ���˳��
#include "stdafx.h"

#include <Windows.h>

#include <ntddscsi.h> 
#include <ioapiset.h>
#include "spti.h"
#include <initguid.h>
#include <devpkey.h>
#include<iostream>
#include<winioctl.h>
#include <setupapi.h>

#include <map>
#include <Cfgmgr32.h>
#include <devguid.h>
#include <wiaintfc.h>
#include <algorithm>
#include <functional>


#define		USB_WRITE		true
#define		USB_READ		false

union USB_CMD{
	struct{
		WORD Func1;
		WORD DataAddr;
		WORD Param1;
		WORD Param2;
		DWORD Func2;
		DWORD Param;
	}L3;
	struct{
		DWORD Func1;
		DWORD DataAddr;
		DWORD Func2;
		DWORD Param;
	}L2;
	struct{
		DWORD Func1;
		BYTE  Param;
		BYTE  Ctrl;
		BYTE  SiLen;
		BYTE  Si[5];
	}L1;
};

enum class DeviceEvent
{
	Arrival=0,
	RemoveComplete=1
};

BOOL ReadFromScsi(HANDLE fileHandle, int cdbLen, void *cdb, int dataLen, char *data);
BOOL WriteToScsi(HANDLE fileHandle, int cdbLen, void *cdb, int dataLen, char *data);
void ScanDevice();
bool _IsSupportedDevice(SP_DEVINFO_DATA devInfoData, HDEVINFO deviceInfoSet);
std::wstring _GetDeviceHardwareId(SP_DEVINFO_DATA devInfoData, HDEVINFO deviceInfoSet);
HANDLE _AddDevice(const wchar_t* devSymbolicLink, SP_DEVINFO_DATA devInfoData);
bool InitDebugParam(void);
BOOL UFISPCode(BYTE *pParamBuf, UINT dataLength, char *pDataBuf, BOOL bDataOut2Device);


std::wstring m_devLocation = L"";
std::wstring m_uvcName = L"";
HANDLE m_fileHandle = NULL;
BYTE	ScsiCBW_Buf[16];


std::wstring tCurPath;
std::wstring tTempDebugPath;
std::wstring tTempISPInfoPath;
std::wstring tTempISP1BinPath;
std::wstring tTempISP2BinPath;
std::wstring tTempLCDBinPath;
std::wstring tTempLCDInitBinPath;
std::wstring tTempGamachartBinPath;

std::wstring tTempSensorRegDebugPath;
std::wstring tTempSensorStructDebugPath;

std::wstring tSettingOrderPath;
std::wstring tSettingElfPath;
std::wstring pDownCodePath;
std::wstring tTempDebugBinPath;

int func_Sensor_DebugRegister;
int func_sdk_MemReadWrite;
int func_sdk_MemRead;
int func_sdk_MemWrite;
int func_Get_Sensor_Data;
int func_Set_Sensor_Data;
int func_IspDebugWrite;
int func_IspDebugRead;
int func_UsbCutRaw;
int func_LcdDebugWrite;
int func_LcdDebugRead;
int func_LcdRegDebugToggle;
int func_LcdRegDebugWrite;
int func_IspReadFlash;

int isp_tab_len;
int isp_tab_addr;
int isp_tab_vma;
int sensor_cmd_addr;

int isp_struct_addr;
int isp_struct_len;
int sensor_struct_addr;
int sensor_struct_len;
int sensor_struct_len2;
int lcd_struct_addr;
int lcd_struct_len;
int lcd_init_addr;
int lcd_init_len;

USB_CMD  UsbCmd;
std::map<std::wstring, HANDLE> m_Ax32xxDevMap;
std::function<void(int event,
	const wchar_t* devLocation, const wchar_t* devModel, const wchar_t* uvcInterfaceName)> m_deviceChangeNotifyFunc;
std::map<std::wstring, std::wstring> m_SupportedHardwareSpecificsMap = {//#include <map>
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

BOOL UFISPCode(BYTE *pParamBuf, UINT dataLength, char *pDataBuf, BOOL bDataOut2Device)
{

	BOOL bCmdOptSuccess;

	memset(ScsiCBW_Buf, 0, sizeof(ScsiCBW_Buf));
	ScsiCBW_Buf[0] = 0xCD;

	memcpy(&ScsiCBW_Buf[1], pParamBuf, (sizeof(ScsiCBW_Buf) - 1));

	if (bDataOut2Device)
	{
		bCmdOptSuccess = WriteToScsi(m_fileHandle, sizeof(ScsiCBW_Buf), ScsiCBW_Buf, dataLength, pDataBuf);
	}
	else
	{
		bCmdOptSuccess = ReadFromScsi(m_fileHandle, sizeof(ScsiCBW_Buf), ScsiCBW_Buf, dataLength, pDataBuf);
	}

	return bCmdOptSuccess;
}

bool InitDebugParam(void)
{
	tTempDebugPath = new TCHAR[MAX_PATH];
	tTempISPInfoPath = new TCHAR[MAX_PATH];
	tSettingOrderPath = new TCHAR[MAX_PATH];
	tTempSensorRegDebugPath = new TCHAR[MAX_PATH];
	tTempSensorStructDebugPath = new TCHAR[MAX_PATH];
	func_sdk_MemWrite = 0x0000110;
	func_sdk_MemRead = 0x0000118;
	int addr[11][2] = {
		{ 0x0000140, 0 },
		{ 0x0000150, 0 },
		{ 0x0000160, 0 },
		{ 0x0000170, 0 },
		{ 0x0000180, 0 },
		{ 0x0000190, 0 },
		{ 0x00001a0, 0 },
		{ 0x00001b0, 0 },
		{ 0x00001c0, 0 },
		{ 0x00001d0, 0 },
		{ 0x00001e0, 0 },
	};
	BOOL bSuccess;
	memset((BYTE*)&UsbCmd, 0, 0x10);

	UsbCmd.L2.Func1 = func_sdk_MemRead;
	UsbCmd.L2.Func2 = 0xffffffff;
	UsbCmd.L2.Param = 0xffffffff;

	BYTE temp[0x200] = { 0 };
	UsbCmd.L2.DataAddr = 0x100;  //Ax328X sram :0x100
	bSuccess = UFISPCode((BYTE *)&(UsbCmd), 0x200, (char *)(temp), USB_READ);//����λ����ȡ����
	if (!bSuccess)
	{
		return false;
	}
	memcpy(&isp_tab_addr, temp + 0x30, 4);
	memcpy(&isp_tab_len, temp + 0x34, 4);
	memcpy(&isp_tab_vma, temp + 0x38, 4);
	memcpy(&sensor_cmd_addr, temp + 0x3c, 4);

	memcpy(&func_Sensor_DebugRegister, temp + 0x40, 4);
	memcpy(&func_Get_Sensor_Data, temp + 0x44, 4);
	memcpy(&func_Set_Sensor_Data, temp + 0x48, 4);
	memcpy(&func_IspDebugWrite, temp + 0x4c, 4);
	memcpy(&func_IspDebugRead, temp + 0x50, 4);
	memcpy(&func_UsbCutRaw, temp + 0x54, 4);

	memcpy(&func_LcdDebugWrite, temp + 0x60, 4);
	memcpy(&func_LcdDebugRead, temp + 0x64, 4);
	memcpy(&func_LcdRegDebugToggle, temp + 0x68, 4);
	memcpy(&func_LcdRegDebugWrite, temp + 0x6c, 4);

	memcpy(&func_IspReadFlash, temp + 0x70, 4);

	//isp_tab_len = GetPrivateProfileInt(_T("RAM"), _T("_res_inf_tab_len"), 1, tSettingOrderPath); 
	//isp_tab_addr = GetPrivateProfileInt(_T("RAM"), _T("_res_inf_tab_addr"), 1, tSettingOrderPath); 
	//isp_tab_vma = GetPrivateProfileInt(_T("RAM"), _T("_res_inf_tab_vma"), 1, tSettingOrderPath); 
	//sensor_cmd_addr = GetPrivateProfileInt(_T("RAM"), _T("sensor_cmd"), 1, tSettingOrderPath); 

	return true;
}

HANDLE _AddDevice(const wchar_t* devSymbolicLink, SP_DEVINFO_DATA devInfoData)
{
	wchar_t uvcInterfaceName[MAX_PATH] = L"";

	DEVINST parentDevInst = 0;
	DEVINST tmpDevInst = devInfoData.DevInst;

	CONFIGRET result = CR_FAILURE;
	DEVPROPTYPE tmpPropType = 0;

	std::wstring parentDesc;
	unsigned long tmpBufferSize = 500;
	parentDesc.resize(tmpBufferSize);

	do
	{	//��ȡ���ؼ�����豸����ָ���豸�ڵ㣨tmpDevInst���ĸ��ڵ���豸ʵ����� parentDevInst��
		result = CM_Get_Parent(&parentDevInst, tmpDevInst, NULL);
		if (result != CR_SUCCESS)
		{
			break;
		}

		GUID parentGuid;
		//�ж��ǲ��� USB �豸
		CM_Get_DevNode_Property(parentDevInst, &DEVPKEY_Device_ClassGuid, &tmpPropType, (PBYTE)&parentGuid, &tmpBufferSize, 0);
		if (parentGuid != GUID_DEVCLASS_USB)
		{
			break;
		}
		//���ݷ��ص�tmpBufferSizeȷ���豸������������parentDesc����ȷ��С 
		CM_Get_DevNode_Property(parentDevInst, &DEVPKEY_Device_DeviceDesc, &tmpPropType, (PBYTE)&parentDesc[0], &tmpBufferSize, 0);
		parentDesc.resize(tmpBufferSize / sizeof(wchar_t) - 1);
		//��ȡ �豸������
		CM_Get_DevNode_Property(parentDevInst, &DEVPKEY_Device_DeviceDesc, &tmpPropType, (PBYTE)&parentDesc[0], &tmpBufferSize, 0);
		if (parentDesc == L"USB Composite Device")
		{
			break;
		}
		tmpDevInst = parentDevInst;
	} while (true);


	wchar_t devLocation[MAX_PATH] = { 0 };
	tmpBufferSize = sizeof(devLocation);
	result = CM_Get_DevNode_Property(parentDevInst, &DEVPKEY_Device_LocationInfo, &tmpPropType, (PBYTE)devLocation, &tmpBufferSize, 0);
	if (result != CR_SUCCESS || _tcslen(devLocation) == 0)
	{
		return nullptr;
	}

	DEVINST childDevInst = NULL;
	CM_Get_Child(&childDevInst, parentDevInst, NULL);//�������ؼ�����豸����ָ���豸�ڵ㣨devnode���ĵ�һ���ӽڵ���豸ʵ�������

	if (childDevInst == 0)
	{
		return nullptr;
	}

	while (true)
	{
		GUID guid, guid2;

		tmpBufferSize = sizeof(guid);
		result = CM_Get_DevNode_Property(childDevInst, &DEVPKEY_Device_ClassGuid, &tmpPropType, (PBYTE)&guid,
			(PULONG)&tmpBufferSize, 0);

		// win10��cammera
		auto uuid2Str = L"CA3E7AB9-B4C3-4AE6-8251-579EF933890F";

		UuidFromString((RPC_WSTR)uuid2Str, &guid2);

		// image�ӿڣ�����UVC
		if (guid == GUID_DEVINTERFACE_IMAGE || guid == guid2)
		{
			tmpBufferSize = sizeof(uvcInterfaceName);
			CM_Get_DevNode_Property(childDevInst, &DEVPKEY_Device_FriendlyName, &tmpPropType, (PBYTE)uvcInterfaceName,
				(PULONG)&tmpBufferSize, 0);
		}

		DEVINST nextChildDevInst = 0;
		if (CM_Get_Sibling(&nextChildDevInst, childDevInst, 0) == CR_SUCCESS)
		{
			childDevInst = nextChildDevInst;
		}
		else
		{
			break;
		}
	}

	if (_tcslen(uvcInterfaceName) == 0)
	{
		return nullptr;
	}
	//��ȡ�豸io�Ĳ������
	m_fileHandle = ::CreateFile(devSymbolicLink,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);  //���豸
	int lasterr = GetLastError();
	//����λ����һ�ζ�ȡ����
	InitDebugParam();

	std::wstring devicePath = devSymbolicLink;
	std::transform(devicePath.begin(), devicePath.end(), devicePath.begin(), ::toupper);
	lasterr = GetLastError();
	//�����豸·�����豸io�����ԭ���õ���һ��ax327x�豸�����������ﱣ�����ͺ��ˣ�
	m_Ax32xxDevMap.insert(std::make_pair(devicePath, m_fileHandle));
	lasterr = GetLastError();
	m_deviceChangeNotifyFunc((int)DeviceEvent::Arrival, devLocation, L"AX327X", uvcInterfaceName);
	lasterr = GetLastError();
	return m_fileHandle;
	
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
				//��ȡ��֧���豸�� ʵ����� ����������Ϣ�� ��ȡ �豸io�������ȡ ��λ������
				_AddDevice(pDetailData->DevicePath, devInfoData);
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


	ScanDevice();




	return 0;
}

