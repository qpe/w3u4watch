#pragma once

#include <comip.h>
#include <comdef.h>
#include <comutil.h>

#include <array>
#include <string>

#include <dshow.h>


_COM_SMARTPTR_TYPEDEF(IMoniker, __uuidof(IMoniker));
_COM_SMARTPTR_TYPEDEF(IBaseFilter, __uuidof(IBaseFilter));
_COM_SMARTPTR_TYPEDEF(IEnumMoniker, __uuidof(IEnumMoniker));
_COM_SMARTPTR_TYPEDEF(IPropertyBag, __uuidof(IPropertyBag));
_COM_SMARTPTR_TYPEDEF(ICreateDevEnum, __uuidof(ICreateDevEnum));
_COM_SMARTPTR_TYPEDEF(IKsPropertySet, __uuidof(IKsPropertySet));

#define TUNER_COUNT 4

class bda_tuner {
	enum bda_tuner_type : size_t {
		ISDB_T0,
		ISDB_T1,
		ISDB_S0,
		ISDB_S1
	};
	std::array<IBaseFilterPtr, TUNER_COUNT> filter;
	std::array<std::wstring, TUNER_COUNT> guid;
	std::array<IKsPropertySetPtr, TUNER_COUNT> ksprops;
	std::array<IBaseFilterPtr, TUNER_COUNT> null_filter;
	std::array<IBaseFilterPtr, TUNER_COUNT> write_filter;

public:
	bool get_filter();
	bool has_filter();

	bool get_misc_output();
	bool has_misc_output();

	std::wstring short_name(size_t i);
};