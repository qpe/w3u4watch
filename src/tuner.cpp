#include "stdafx.h"

#include <windows.h>

/* Com related library */
#include <comip.h>
#include <comdef.h>
#include <comutil.h>

#ifdef _DEBUG
#pragma comment(lib,"comsuppwd.lib")
#else
#pragma comment(lib,"comsuppw.lib")
#endif

#include <ks.h>
#include <ksmedia.h>
#include <bdatypes.h>
#include <bdamedia.h>
#include <strmif.h>

#include <dshow.h>
#pragma comment(lib, "strmiids.lib")

#include <algorithm>
#include <array>
#include <string>

#include "pmsg.h"
#include "tuner.h"

/* for null renderer and file writer */
extern "C" const CLSID CLSID_FileWriter;
extern "C" const CLSID CLSID_NullRenderer;

static const std::array<const std::wstring, 4> w3u4_tuner_friendly_name = {
	L"PXW3U4 Multi Tuner ISDB-T BDA Filter #0",
	L"PXW3U4 Multi Tuner ISDB-T BDA Filter #1",
	L"PXW3U4 Multi Tuner ISDB-S BDA Filter #0",
	L"PXW3U4 Multi Tuner ISDB-S BDA Filter #1"
};
static const std::array<const std::wstring, 4> w3u4_tuner_short_name = {
	L"PXW3U4 T0",
	L"PXW3U4 T1",
	L"PXW3U4 S0",
	L"PXW3U4 S1"
};

static std::wstring GetStringProperty(IPropertyBagPtr &prop_bag, std::wstring prop_name)
{
	VARIANT var;
	VariantInit(&var);
	HRESULT hr = prop_bag->Read(prop_name.c_str(), &var, NULL);
	std::wstring name;
	if (SUCCEEDED(hr)) {
		name = V_BSTR(&var);
	}
	VariantClear(&var);
	return name;
}

bool bda_tuner::get_filter() {

	HRESULT hr;

	ICreateDevEnumPtr dev_enum;
	hr = dev_enum.CreateInstance(CLSID_SystemDeviceEnum);
	if (FAILED(hr)) {
		pmsg(L"CreateInstance Failed.\n");
		return false;
	}

	IEnumMonikerPtr e;
	hr = dev_enum->CreateClassEnumerator(KSCATEGORY_BDA_NETWORK_TUNER, &e, CDEF_DEVMON_PNP_DEVICE);
	if (FAILED(hr)) {
		pmsg(L"CreateClassEnumerator Failed.\n");
		return false;
	}
	if (hr == S_FALSE) {
		pmsg(L"No Device.\n");
		return false;
	}

	int c = 0;
	for (;;) {
		IMonikerPtr m;
		hr = e->Next(1, &m, nullptr);
		if (FAILED(hr) || hr == S_FALSE || !m) {
			pmsg(L"Enum Tuner finished. %d device found.\n", c);
			break;
		}

		IPropertyBagPtr props;
		hr = m->BindToStorage(nullptr, nullptr, IID_IPropertyBag, reinterpret_cast<void**>(&props));
		if (FAILED(hr)) {
			pmsg(L"Cannot BindToStorage(IPropertyBag) Failed.\n");
			return false;
		}

		std::wstring name = GetStringProperty(props, L"FriendlyName");
		if (name.empty()) {
			pmsg(L"Oops. FriendlyName emptly...\n");
			return false;
		}
		
		/* check match PX-W3U4 devices */
		unsigned int matched = -1;
		for (unsigned int i = 0; i < w3u4_tuner_friendly_name.size(); i++) {
			if (w3u4_tuner_friendly_name.at(i) == name) matched = i;
		}
		if (matched < 0) continue;

		/* get filter */
		hr = m->BindToObject(NULL, NULL, IID_IBaseFilter, reinterpret_cast<void**>(&filter.at(matched)));
		if (FAILED(hr)) {
			pmsg(L"Cannot bind filter.\n");
			return false;
		}

		/* get guid */
		LPOLESTR pwszName;
		hr = m->GetDisplayName(NULL, NULL, &pwszName);
		if (FAILED(hr)) {
			pmsg(L"Cannot getDisplayName.\n");
			return false;
		}
		guid.at(matched) = pwszName;
		std::transform(guid.at(matched).begin(), guid.at(matched).end(), guid.at(matched).begin(), towlower);

		pmsg(L"guid(%u)=%s\n", matched, guid.at(matched).c_str());

		c++;
	}
	/* store short name */
	if (c != 4) {
		pmsg(L"target is not PX-W3U4, or connected multiple PX-W3U4.\n");
		return false;
	}
	return true;
}

std::wstring bda_tuner::short_name(size_t i) {
	if (i < w3u4_tuner_short_name.size())
		return w3u4_tuner_short_name.at(i);
	else
		return std::wstring();
}

bool bda_tuner::has_filter() {
	for (auto i = 0; i < guid.size(); i++) {
		if (guid.empty())
			return false;
	}
	return true;
}

bool bda_tuner::get_misc_output() {
	for (auto i = 0; i < guid.size(); i++) {
	}
}

#if 0

//
// Filter Graph �ϐ�
IFraphBuilder* pGraphBuilder;// ����̏������Ȃǂ͍���͏ȗ�
							 // File Writer Filter�̐ݒ�
IBaseFilter *pFileWriterFilter = NULL;
hr = CoCreateInstance(CLSID_FileWriter, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&pFileWrite);
if (FAILED(hr)) {/**** Error���������� ****/ }
// File Writer Filter��Graph�ɒǉ�����
hr = pGraphBuilder->AddFilter(pFileWrite, L"File write");
if (FAILED(hr)) {/**** Error���������� ****/ }
// �o�̓t�@�C������ݒ�
IFileSinkFilter *pFileSinkFilter = NULL; pFileWrite->QueryInterface(IID_IFileSinkFilter, (void**)&pFileSinkFilter);
// �����C:\\tmp.wav�Ƃ����t�@�C�����ɂ���
hr = pFileSinkFilter->SetFileName(L"C:\\tmp.wav", NULL);
if (FAILED(hr)) {/**** Error���������� ****/ }
pFileSinkFilter->Release();


static HRESULT test_func() {
	IEnumMoniker* enum_moniker = nullptr;
	ICreateDevEnum* dev_enum = nullptr;
	IMoniker* moniker = nullptr;
	HRESULT hr = ::CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC, IID_ICreateDevEnum, reinterpret_cast<void**>(&dev_enum));
	if (FAILED(hr)) {
		return 
	}
	hr = m_pICreateDevEnum->CreateClassEnumerator(clsid, &m_pIEnumMoniker, dwFlags);
}

if (FAILED(hr)) {
	wstring e(L"Error in CoCreateInstance.");
	throw e;
	return; /* not reached */
}

hr = m_pICreateDevEnum->CreateClassEnumerator(clsid, &m_pIEnumMoniker, dwFlags);

if ((FAILED(hr)) || (hr != S_OK)) {
	// CreateClassEnumerator �����Ȃ� || ������Ȃ�
	SAFE_RELEASE(m_pICreateDevEnum);
	wstring e(L"Error in CreateClassEnumerator.");
	throw e;
	return; /* not reached */
}


// ini �t�@�C���Ŏw�肳�ꂽ�`���[�i�E�L���v�`���̑g����List���쐬
HRESULT CBonTuner::InitDSFilterEnum(void)
{
	HRESULT hr;

	// �V�X�e���ɑ��݂���`���[�i�E�L���v�`���̃��X�g
	vector<DSListData> TunerList;
	vector<DSListData> CaptureList;

	ULONG order;

	SAFE_DELETE(m_pDSFilterEnumTuner);
	SAFE_DELETE(m_pDSFilterEnumCapture);

	try {
		m_pDSFilterEnumTuner = new CDSFilterEnum(KSCATEGORY_BDA_NETWORK_TUNER, CDEF_DEVMON_PNP_DEVICE);
	}
	catch (...) {
		OutputDebug(L"[InitDSFilterEnum] Fail to construct CDSFilterEnum(KSCATEGORY_BDA_NETWORK_TUNER).\n");
		return E_FAIL;
	}

	order = 0;
	while (SUCCEEDED(hr = m_pDSFilterEnumTuner->next()) && hr == S_OK) {
		wstring sDisplayName;
		wstring sFriendlyName;

		// �`���[�i�� DisplayName, FriendlyName �𓾂�
		m_pDSFilterEnumTuner->getDisplayName(&sDisplayName);
		m_pDSFilterEnumTuner->getFriendlyName(&sFriendlyName);

		// �ꗗ�ɒǉ�
		TunerList.emplace_back(sDisplayName, sFriendlyName, order);

		order++;
	}

	try {
		m_pDSFilterEnumCapture = new CDSFilterEnum(KSCATEGORY_BDA_RECEIVER_COMPONENT, CDEF_DEVMON_PNP_DEVICE);
	}
	catch (...) {
		OutputDebug(L"[InitDSFilterEnum] Fail to construct CDSFilterEnum(KSCATEGORY_BDA_RECEIVER_COMPONENT).\n");
		return E_FAIL;
	}

	order = 0;
	while (SUCCEEDED(hr = m_pDSFilterEnumCapture->next()) && hr == S_OK) {
		wstring sDisplayName;
		wstring sFriendlyName;

		// �`���[�i�� DisplayName, FriendlyName �𓾂�
		m_pDSFilterEnumCapture->getDisplayName(&sDisplayName);
		m_pDSFilterEnumCapture->getFriendlyName(&sFriendlyName);

		// �ꗗ�ɒǉ�

		CaptureList.emplace_back(sDisplayName, sFriendlyName, order);

		order++;
	}

	unsigned int total = 0;
	m_UsableTunerCaptureList.clear();

	for (unsigned int i = 0; i < m_aTunerParam.Tuner.size(); i++) {
		for (vector<DSListData>::iterator it = TunerList.begin(); it != TunerList.end(); it++) {
			// DisplayName �� GUID ���܂܂�邩�������āANO�������玟�̃`���[�i��
			if (m_aTunerParam.Tuner[i]->TunerGUID.compare(L"") != 0 && it->GUID.find(m_aTunerParam.Tuner[i]->TunerGUID) == wstring::npos) {
				continue;
			}

			// FriendlyName ���܂܂�邩�������āANO�������玟�̃`���[�i��
			if (m_aTunerParam.Tuner[i]->TunerFriendlyName.compare(L"") != 0 && it->FriendlyName.find(m_aTunerParam.Tuner[i]->TunerFriendlyName) == wstring::npos) {
				continue;
			}

			// �Ώۂ̃`���[�i�f�o�C�X������
			OutputDebug(L"[InitDSFilterEnum] Found tuner device=FriendlyName:%s,  GUID:%s\n", it->FriendlyName.c_str(), it->GUID.c_str());
			if (!m_aTunerParam.bNotExistCaptureDevice) {
				// Capture�f�o�C�X���g�p����
				vector<DSListData> TempCaptureList;
				for (vector<DSListData>::iterator it2 = CaptureList.begin(); it2 != CaptureList.end(); it2++) {
					// DisplayName �� GUID ���܂܂�邩�������āANO�������玟�̃L���v�`����
					if (m_aTunerParam.Tuner[i]->CaptureGUID.compare(L"") != 0 && it2->GUID.find(m_aTunerParam.Tuner[i]->CaptureGUID) == wstring::npos) {
						continue;
					}

					// FriendlyName ���܂܂�邩�������āANO�������玟�̃L���v�`����
					if (m_aTunerParam.Tuner[i]->CaptureFriendlyName.compare(L"") != 0 && it2->FriendlyName.find(m_aTunerParam.Tuner[i]->CaptureFriendlyName) == wstring::npos) {
						continue;
					}

					// �Ώۂ̃L���v�`���f�o�C�X������
					OutputDebug(L"[InitDSFilterEnum]   Found capture device=FriendlyName:%s,  GUID:%s\n", it2->FriendlyName.c_str(), it2->GUID.c_str());
					TempCaptureList.emplace_back(*it2);
				}

				if (TempCaptureList.empty()) {
					// �L���v�`���f�o�C�X��������Ȃ������̂Ŏ��̃`���[�i��
					OutputDebug(L"[InitDSFilterEnum]   No combined capture devices.\n");
					continue;
				}

				// �`���[�i��List�ɒǉ�
				m_UsableTunerCaptureList.emplace_back(*it);

				unsigned int count = 0;
				if (m_aTunerParam.bCheckDeviceInstancePath) {
					// �`���[�i�f�o�C�X�ƃL���v�`���f�o�C�X�̃f�o�C�X�C���X�^���X�p�X����v���Ă��邩�m�F
					OutputDebug(L"[InitDSFilterEnum]   Checking device instance path.\n");
					wstring::size_type n, last;
					n = last = 0;
					while ((n = it->GUID.find(L'#', n)) != wstring::npos) {
						last = n;
						n++;
					}
					if (last != 0) {
						wstring path = it->GUID.substr(0, last);
						for (vector<DSListData>::iterator it2 = TempCaptureList.begin(); it2 != TempCaptureList.end(); it2++) {
							if (it2->GUID.find(path) != wstring::npos) {
								// �f�o�C�X�p�X����v������̂�List�ɒǉ�
								OutputDebug(L"[InitDSFilterEnum]     Adding matched tuner and capture device.\n");
								OutputDebug(L"[InitDSFilterEnum]       tuner=FriendlyName:%s,  GUID:%s\n", it->FriendlyName.c_str(), it->GUID.c_str());
								OutputDebug(L"[InitDSFilterEnum]       capture=FriendlyName:%s,  GUID:%s\n", it2->FriendlyName.c_str(), it2->GUID.c_str());
								m_UsableTunerCaptureList.back().CaptureList.emplace_back(*it2);
								count++;
							}
						}
					}
				}

				if (count == 0) {
					// �f�o�C�X�p�X����v������̂��Ȃ����� or �m�F���Ȃ�
					if (m_aTunerParam.bCheckDeviceInstancePath) {
						OutputDebug(L"[InitDSFilterEnum]     No matched devices.\n");
					}
					for (vector<DSListData>::iterator it2 = TempCaptureList.begin(); it2 != TempCaptureList.end(); it2++) {
						// ���ׂ�List�ɒǉ�
						OutputDebug(L"[InitDSFilterEnum]   Adding tuner and capture device.\n");
						OutputDebug(L"[InitDSFilterEnum]     tuner=FriendlyName:%s,  GUID:%s\n", it->FriendlyName.c_str(), it->GUID.c_str());
						OutputDebug(L"[InitDSFilterEnum]     capture=FriendlyName:%s,  GUID:%s\n", it2->FriendlyName.c_str(), it2->GUID.c_str());
						m_UsableTunerCaptureList.back().CaptureList.emplace_back(*it2);
						count++;
					}
				}

				OutputDebug(L"[InitDSFilterEnum]   %d of combination was added.\n", count);
				total += count;
			}
			else
			{
				// Capture�f�o�C�X���g�p���Ȃ�
				OutputDebug(L"[InitDSFilterEnum]   Adding tuner device only.\n");
				OutputDebug(L"[InitDSFilterEnum]     tuner=FriendlyName:%s,  GUID:%s\n", it->FriendlyName.c_str(), it->GUID.c_str());
				m_UsableTunerCaptureList.emplace_back(*it);
			}
		}
	}
	if (m_UsableTunerCaptureList.empty()) {
		OutputDebug(L"[InitDSFilterEnum] No devices found.\n");
		return E_FAIL;
	}

	OutputDebug(L"[InitDSFilterEnum] Total %d of combination was added.\n", total);
	return S_OK;
}

#endif