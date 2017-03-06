#include "stdafx.h"

#include <comip.h>
#include <comdef.h>

#include <dshow.h>

#include <algorithm>

#include "it35.h"

_COM_SMARTPTR_TYPEDEF(IKsPropertySet, __uuidof(IKsPropertySet));

/*
 * ITE_EXTENSION
 */
static const GUID KSPROPSETID_IteExtension = { 0xc6efe5eb, 0x855a, 0x4f1b,{ 0xb7, 0xaa, 0x87, 0xb5, 0xe1, 0xdc, 0x41, 0x13 } };

enum KSPROPERTY_ITE_EXTENSION {
	KSPROPERTY_ITE_EX_BULK_DATA = 0,					// get/put
	KSPROPERTY_ITE_EX_BULK_DATA_NB,						// get/put
	KSPROPERTY_ITE_EX_PID_FILTER_ON_OFF,				// put only
	KSPROPERTY_ITE_EX_BAND_WIDTH,						// get/put
	KSPROPERTY_ITE_EX_MERCURY_DRIVER_INFO,				// get only
	KSPROPERTY_ITE_EX_MERCURY_DEVICE_INFO,				// get only
	KSPROPERTY_ITE_EX_TS_DATA,							// get/put
	KSPROPERTY_ITE_EX_OVL_CNT,							// get only
	KSPROPERTY_ITE_EX_FREQ,								// get/put
	KSPROPERTY_ITE_EX_RESET_USB,						// put only
	KSPROPERTY_ITE_EX_MERCURY_REG,						// get/put
	KSPROPERTY_ITE_EX_MERCURY_PVBER,					// get/put
	KSPROPERTY_ITE_EX_MERCURY_REC_LEN,					// get/put
	KSPROPERTY_ITE_EX_MERCURY_EEPROM,					// get/put
	KSPROPERTY_ITE_EX_MERCURY_IR,						// get only	(ERROR_NOT_SUPORTED)
	KSPROPERTY_ITE_EX_MERCURY_SIGNAL_STRENGTH,			// get only
	KSPROPERTY_ITE_EX_CHANNEL_MODULATION = 99,			// get only
};

/*
 * DIB-S IO control Property
 */
static const GUID KSPROPSETID_DvbsIoCtl = { 0xf23fac2d, 0xe1af, 0x48e0,{ 0x8b, 0xbe, 0xa1, 0x40, 0x29, 0xc9, 0x2f, 0x21 } };

enum KSPROPERTY_DVBS_IO_CTL {
	KSPROPERTY_DVBS_IO_LNB_POWER = 0,					// get/put
	KSPROPERTY_DVBS_IO_DiseqcLoad,						// put only
};

/*
 * Ext IO Control Property
 */
static const GUID KSPROPSETID_ExtIoCtl = { 0xf23fac2d, 0xe1af, 0x48e0,{ 0x8b, 0xbe, 0xa1, 0x40, 0x29, 0xc9, 0x2f, 0x11 } };

enum KSPROPERTY_EXT_IO_CTL {
	KSPROPERTY_EXT_IO_DRV_DATA = 0,						// get/put
	KSPROPERTY_EXT_IO_DEV_IO_CTL,						// get/put
	KSPROPERTY_EXT_IO_UNUSED50 = 50,					// not used
	KSPROPERTY_EXT_IO_UNUSED51 = 51,					// not used
	KSPROPERTY_EXT_IO_ISDBT_IO_CTL = 200,				// put only
};

/* function code for KSPROPERTY_EXT_IO_DRV_DATA */
enum DRV_DATA_FUNC_CODE {
	DRV_DATA_FUNC_GET_DRIVER_INFO = 1,					// Read
};

/* structure for KSPROPERTY_EXT_IO_DRV_DATA */
#pragma pack(1)
struct DrvDataDataSet {
	struct {
		DWORD DriverPID;
		DWORD DriverVersion;
		DWORD FwVersion_LINK;
		DWORD FwVersion_OFDM;
		DWORD TunerID;
	} DriverInfo;
};
#pragma pack()

/* function code for KSPROPERTY_EXT_IO_DEV_IO_CTL */
enum DEV_IO_CTL_FUNC_CODE {
	DEV_IO_CTL_FUNC_READ_OFDM_REG = 0,					// Read
	DEV_IO_CTL_FUNC_WRITE_OFDM_REG,						// Write
	DEV_IO_CTL_FUNC_READ_LINK_REG,						// Read
	DEV_IO_CTL_FUNC_WRITE_LINK_REG,						// Write
	DEV_IO_CTL_FUNC_AP_CTRL,							// Write
	DEV_IO_CTL_FUNC_READ_RAW_IR = 7,					// Read
	DEV_IO_CTL_FUNC_GET_UART_DATA = 10,					// Read
	DEV_IO_CTL_FUNC_SENT_UART,							// Write
	DEV_IO_CTL_FUNC_SET_UART_BAUDRATE,					// Write
	DEV_IO_CTL_FUNC_CARD_DETECT,						// Read
	DEV_IO_CTL_FUNC_GET_ATR,							// Read
	DEV_IO_CTL_FUNC_AES_KEY,							// Write
	DEV_IO_CTL_FUNC_AES_ENABLE,							// Write
	DEV_IO_CTL_FUNC_RESET_SMART_CARD,					// Write
	DEV_IO_CTL_FUNC_IS_UART_READY,						// Read
	DEV_IO_CTL_FUNC_SET_ONE_SEG,						// Write
	DEV_IO_CTL_FUNC_GET_BOARD_INPUT_POWER = 25,			// Read
	DEV_IO_CTL_FUNC_SET_DECRYP,							// Write
	DEV_IO_CTL_FUNC_UNKNOWN99 = 99,						// Read
	DEV_IO_CTL_FUNC_GET_RX_DEVICE_ID,					// Read
	DEV_IO_CTL_FUNC_UNKNOWN101,							// Write
	DEV_IO_CTL_FUNC_IT930X_EEPROM_READ = 300,			// Read
	DEV_IO_CTL_FUNC_IT930X_EEPROM_WRITE,				// Write
	DEV_IO_CTL_FUNC_READ_GPIO,							// Read
	DEV_IO_CTL_FUNC_WRITE_GPIO,							// Write
};

/* structure for KSPROPERTY_EXT_IO_DEV_IO_CTL */
#pragma pack(1)
struct DevIoCtlDataSet {
	union {
		struct {
			DWORD Addr;
			BYTE WriteData;
		} Reg;
		struct {
			DWORD Key;
			BYTE Enable;
		} Decryp;
		struct {
			BYTE Length;
			BYTE Buffer[3 + 254 + 2];
		} UartData;
		DWORD DeviceId;
		DWORD PowerOn;
	};
	union {
		DWORD CardDetected;
		DWORD UartReady;
		DWORD AesEnable;
	};
	BYTE Reserved1[2];							// -28h (-40)
	WORD UartBaudRate;							// -26h (-38)
	BYTE ATR[13];								// -24h (-36)
	BYTE AesKey[16];							// -17h (-23)
	BYTE Reserved2[7];							// -07h (-7)
	DevIoCtlDataSet(void) {
		memset(this, 0, sizeof(*this));
	};
};
#pragma pack()

/*
 * Private IO Control property
 */
static const GUID KSPROPSETID_PrivateIoCtl = { 0xede22531, 0x92e8, 0x4957,{ 0x9d, 0x5, 0x6f, 0x30, 0x33, 0x73, 0xe8, 0x37 } };

enum KSPROPERTY_PRIVATE_IO_CTL {
	KSPROPERTY_PRIVATE_IO_DIGIBEST_TUNER = 0,			// put only
};

/* function code for KSPROPERTY_PRIVATE_IO_DIGIBEST_TUNER */
enum PRIVATE_IO_CTL_FUNC_CODE {
	PRIVATE_IO_CTL_FUNC_PROTECT_TUNER_POWER = 0,
	PRIVATE_IO_CTL_FUNC_UNPROTECT_TUNER_POWER,
	PRIVATE_IO_CTL_FUNC_SET_TUNER_POWER_ON,
	PRIVATE_IO_CTL_FUNC_SET_TUNER_POWER_OFF,
};

//
// ITE 拡張プロパティセット用関数
//
// チューニング帯域幅取得
HRESULT it35_GetBandWidth(IKsPropertySetPtr &props, WORD &data)
{
	DWORD bytes;
	BYTE buf[sizeof(data)];

	HRESULT hr = props->Get(KSPROPSETID_IteExtension, KSPROPERTY_ITE_EX_BAND_WIDTH, NULL, 0, buf, sizeof(buf), &bytes);
	if (FAILED(hr))
		return hr;

	data = *(WORD*)buf;

	return hr;
}

// チューニング帯域幅設定
HRESULT it35_PutBandWidth(IKsPropertySetPtr &props, WORD data)
{
	return props->Set(KSPROPSETID_IteExtension, KSPROPERTY_ITE_EX_BAND_WIDTH, NULL, 0, &data, sizeof(data));
}

// チューニング周波数取得
HRESULT it35_GetFreq(IKsPropertySetPtr &props, WORD data)
{
	DWORD bytes;
	BYTE buf[sizeof(data)];

	HRESULT hr = props->Get(KSPROPSETID_IteExtension, KSPROPERTY_ITE_EX_FREQ, NULL, 0, buf, sizeof(buf), &bytes);
	if (FAILED(hr))
		return hr;

	data = *(WORD*)buf;

	return hr;
}

// チューニング周波数設定
HRESULT it35_PutFreq(IKsPropertySetPtr &props, WORD data)
{
	return props->Set(KSPROPSETID_IteExtension, KSPROPERTY_ITE_EX_FREQ, NULL, 0, &data, sizeof(data));
}

//
// DVB-S IO コントロール プロパティセット用関数
//
// LNBPower の設定状態取得
HRESULT it35_GetLNBPower(IKsPropertySetPtr &props, BYTE &data)
{
	DWORD bytes;
	BYTE buf;

	HRESULT hr = props->Get(KSPROPSETID_DvbsIoCtl, KSPROPERTY_DVBS_IO_LNB_POWER, NULL, 0, &buf, sizeof(buf), &bytes);
	if (FAILED(hr))
		return hr;

	data = buf;
	return hr;
}

// LNBPower の設定
HRESULT it35_PutLNBPower(IKsPropertySetPtr &props, BYTE data)
{
	return props->Set(KSPROPSETID_DvbsIoCtl, KSPROPERTY_DVBS_IO_LNB_POWER, NULL, 0, &data, sizeof(data));
}

//
// 拡張 IO コントロール KSPROPERTY_EXT_IO_DRV_DATA 用関数
//
// KSPROPERTY_EXT_IO_DRV_DATA 共通関数
HRESULT it35_GetDrvData(IKsPropertySetPtr &props, DWORD code, DrvDataDataSet &data)
{
	DWORD bytes;
	DrvDataDataSet buf;
	HRESULT hr;
	
	hr = props->Set(KSPROPSETID_ExtIoCtl, KSPROPERTY_EXT_IO_DRV_DATA, NULL, 0, &code, sizeof(code));
	if (FAILED(hr))
		return hr;
	hr = props->Get(KSPROPSETID_ExtIoCtl, KSPROPERTY_EXT_IO_DRV_DATA, NULL, 0, &buf, sizeof(buf), &bytes);
	if (FAILED(hr))
		return hr;

	data = buf;
	return hr;
}

// ドライバーバージョン情報取得
HRESULT it35_GetDriverInfo(IKsPropertySetPtr &props, DrvDataDataSet &data)
{
	return it35_GetDrvData(props, DRV_DATA_FUNC_GET_DRIVER_INFO, data);
}

//
// 拡張 IO コントロール KSPROPERTY_EXT_IO_DEV_IO_CTL 用関数
//
// KSPROPERTY_EXT_IO_DEV_IO_CTL 共通関数
HRESULT it35_GetDevIoCtl(IKsPropertySetPtr &props, BOOL need_get, DWORD code, DWORD *retval, DevIoCtlDataSet &dataset)
{
	HRESULT hr;
	DWORD bytes;

#pragma pack(1)
	struct {
		DWORD data;
		DevIoCtlDataSet dataset;
	} put_get_data;
#pragma pack()

	put_get_data.data = code;
	put_get_data.dataset = dataset;
	hr = props->Set(KSPROPSETID_ExtIoCtl, KSPROPERTY_EXT_IO_DEV_IO_CTL, NULL, 0, &put_get_data, sizeof(put_get_data));
	if (FAILED(hr) || !need_get)
		return hr;
	hr = props->Get(KSPROPSETID_ExtIoCtl, KSPROPERTY_EXT_IO_DEV_IO_CTL, NULL, 0, &put_get_data, sizeof(put_get_data), &bytes);
	if (FAILED(hr))
		return hr;

	dataset = put_get_data.dataset;
	if(retval)
		*retval = put_get_data.data;
	return hr;
}

// OFDM Register 値取得
HRESULT it35_ReadOfdmReg(IKsPropertySetPtr &props, DWORD addr, BYTE &data)
{
	DWORD result;
	DevIoCtlDataSet dataset;

	dataset.Reg.Addr = addr;

	HRESULT hr = it35_GetDevIoCtl(props, TRUE, DEV_IO_CTL_FUNC_READ_OFDM_REG, &result, dataset);
	if (FAILED(hr))
		return hr;

	data = (BYTE)result;
	return hr;
}

// OFDM Register 値書込
HRESULT it35_WriteOfdmReg(IKsPropertySetPtr &props, DWORD addr, BYTE data)
{
	DevIoCtlDataSet dataset;

	dataset.Reg.Addr = addr;
	dataset.Reg.WriteData = data;
	return it35_GetDevIoCtl(props, FALSE, DEV_IO_CTL_FUNC_WRITE_OFDM_REG, NULL, dataset);
}

// Link Register 値取得
HRESULT it35_ReadLinkReg(IKsPropertySetPtr &props, DWORD addr, BYTE &data)
{
	DWORD result;
	DevIoCtlDataSet dataset;

	dataset.Reg.Addr = addr;
	HRESULT hr = it35_GetDevIoCtl(props, TRUE, DEV_IO_CTL_FUNC_READ_LINK_REG, &result, dataset);
	if (FAILED(hr))
		return hr;

	data = (BYTE)result;
	return hr;
}

// Link Register 値書込
HRESULT it35_WriteLinkReg(IKsPropertySetPtr &props, DWORD addr, BYTE data)
{
	DevIoCtlDataSet dataset;

	dataset.Reg.Addr = addr;
	dataset.Reg.WriteData = data;
	return it35_GetDevIoCtl(props, FALSE, DEV_IO_CTL_FUNC_WRITE_LINK_REG, NULL, dataset);
}

// Tuner パワーコントロール
HRESULT it35_ApCtrl(IKsPropertySetPtr &props, BOOL power_on)
{
	DevIoCtlDataSet dataset;

	dataset.DeviceId = (DWORD)power_on;

	return it35_GetDevIoCtl(props, FALSE, DEV_IO_CTL_FUNC_AP_CTRL, NULL, dataset);
}

// リモコン受信データ取得
HRESULT it35_ReadRawIR(IKsPropertySetPtr &props, DWORD &data)
{
	DWORD result;
	DevIoCtlDataSet dataset;

	HRESULT hr = it35_GetDevIoCtl(props, TRUE, DEV_IO_CTL_FUNC_READ_RAW_IR, &result, dataset);
	if (FAILED(hr))
		return hr;

	data = result;
	return hr;
}

// UART 受信データ取得
HRESULT it35_GetUartData(IKsPropertySetPtr &props, void *rcv_buff, DWORD &len)
{
	DWORD result;
	DevIoCtlDataSet dataset;

	dataset.UartData.Length = 255;		// ここに0を設定して呼び出すとBSoD BAD_POOL_HEADER が発生する(Ver. Beta ドライバ)

	HRESULT hr = it35_GetDevIoCtl(props, TRUE, DEV_IO_CTL_FUNC_GET_UART_DATA, &result, dataset);
	if (FAILED(hr)) {
		return hr;
	}
	if(rcv_buff)
		memcpy(rcv_buff, dataset.UartData.Buffer, std::min(std::min((size_t)dataset.UartData.Length, (size_t)255), (size_t)len));

	len = dataset.UartData.Length;

	return hr;
}

// UART 送信データ設定
HRESULT it35_SentUart(IKsPropertySetPtr &props, void *send_buff, DWORD len)
{
	DevIoCtlDataSet dataset;

	if (send_buff) {
		dataset.UartData.Length = (BYTE)std::min((DWORD)255, len);
		memcpy(dataset.UartData.Buffer, send_buff, dataset.UartData.Length);
	}

	return it35_GetDevIoCtl(props, FALSE, DEV_IO_CTL_FUNC_SENT_UART, NULL, dataset);
}

// UART ボーレート設定
HRESULT it35_SetUartBaudRate(IKsPropertySetPtr &props, WORD baud_rate)
{
	DevIoCtlDataSet dataset;

	dataset.UartBaudRate = baud_rate;
	return it35_GetDevIoCtl(props, FALSE, DEV_IO_CTL_FUNC_SET_UART_BAUDRATE, NULL, dataset);
}

// CARD 検出
HRESULT it35_CardDetect(IKsPropertySetPtr &props, BOOL &detect)
{
	DWORD result;
	DevIoCtlDataSet dataset;

	HRESULT hr = it35_GetDevIoCtl(props, TRUE, DEV_IO_CTL_FUNC_CARD_DETECT, &result, dataset);
	if (FAILED(hr))
		return hr;

	detect = (BOOL)dataset.CardDetected;
	return hr;
}

// CARD ATR データ取得(atr need 13byte)
HRESULT it35_GetATR(IKsPropertySetPtr &props, void *atr)
{
	DWORD result;
	DevIoCtlDataSet dataset;

	HRESULT hr = it35_GetDevIoCtl(props, TRUE, DEV_IO_CTL_FUNC_GET_ATR, &result, dataset);
	if (FAILED(hr))
		return hr;

	if (atr)
		memcpy(atr, dataset.ATR, sizeof(dataset.ATR));

	return hr;
}

// CARD リセット
HRESULT it35_ResetSmartCard(IKsPropertySetPtr &props)
{
	DevIoCtlDataSet dataset;

	return it35_GetDevIoCtl(props, FALSE, DEV_IO_CTL_FUNC_RESET_SMART_CARD, NULL, dataset);
}

// UART Ready 状態取得
HRESULT it35_IsUartReady(IKsPropertySetPtr &props, BOOL &ready)
{
	DWORD result;
	DevIoCtlDataSet dataset;

	HRESULT hr = it35_GetDevIoCtl(props, TRUE, DEV_IO_CTL_FUNC_IS_UART_READY, &result, dataset);
	if (FAILED(hr))
		return hr;

	ready = (BOOL)dataset.UartReady;

	return hr;
}

// 消費電流取得?
HRESULT it35_GetBoardInputPower(IKsPropertySetPtr &props, DWORD &power)
{
	DWORD result;
	DevIoCtlDataSet dataset;

	HRESULT hr = it35_GetDevIoCtl(props, TRUE, DEV_IO_CTL_FUNC_GET_BOARD_INPUT_POWER, &result, dataset);
	if (FAILED(hr))
		return hr;

	power = result;
	return hr;
}

/* TODO: what's value is this?? */
HRESULT it35_Unk99(IKsPropertySetPtr &props, BOOL *pbDetect)
{
	DWORD result;
	DevIoCtlDataSet dataset;

	HRESULT hr = it35_GetDevIoCtl(props, TRUE, DEV_IO_CTL_FUNC_UNKNOWN99, &result, dataset);
	if (FAILED(hr))
		return hr;

	if (pbDetect)
		*pbDetect = (BOOL)dataset.CardDetected;

	return hr;
}

// 
HRESULT it35_GetRxDeviceId(IKsPropertySetPtr &props, DWORD &dev_id)
{
	DWORD result;
	DevIoCtlDataSet dataset;

	HRESULT hr = it35_GetDevIoCtl(props, TRUE, DEV_IO_CTL_FUNC_GET_RX_DEVICE_ID, &result, dataset);
	if (FAILED(hr))
		return hr;

	dev_id = dataset.DeviceId;
	return hr;
}

// 
HRESULT it35_Unk101(IKsPropertySetPtr &props)
{
	DWORD result;
	DevIoCtlDataSet dataset;

	return it35_GetDevIoCtl(props, FALSE, DEV_IO_CTL_FUNC_UNKNOWN101, &result, dataset);
}

//
// 拡張 IO コントロール KSPROPERTY_EXT_IO_ISDBT_IO_CTL 用関数
//
// TSID をセット
HRESULT it35_PutISDBIoCtl(IKsPropertySetPtr &props, WORD data)
{
	return props->Set(KSPROPSETID_ExtIoCtl, KSPROPERTY_EXT_IO_ISDBT_IO_CTL, NULL, 0, &data, sizeof(data));
}

/* TODO: what's this?? */
//
// プライベート IO コントロール KSPROPERTY_PRIVATE_IO_DIGIBEST_TUNER 用関数
//
// DigiBest Tuner 用プライベートファンクション
HRESULT it35_DigibestPrivateIoControl(IKsPropertySetPtr &props, DWORD dwCode)
{
	return props->Set(KSPROPSETID_PrivateIoCtl, KSPROPERTY_PRIVATE_IO_DIGIBEST_TUNER, NULL, 0, &dwCode, sizeof(dwCode));
}
