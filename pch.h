//
// pch.h
// Header for standard system include files.
//

#pragma once

#include <collection.h>
#include <ppltasks.h>
#define XAUDIO2_HELPER_FUNCTIONS
#include <xaudio2.h>
#include <xaudio2fx.h>
#include <mfidl.h>
#include <mfapi.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <propvarutil.h>

#include "App.xaml.h"

using namespace Concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Microsoft::WRL;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace SongMixer;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml;

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw Platform::Exception::CreateException(hr);
	}
}

#define IF_FAILED_JUMP(action, label) \
{ \
	hr = (action); \
	if (FAILED(hr)) { goto label;  } \
}