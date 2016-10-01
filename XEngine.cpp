#include "pch.h"
#include "XEngine.h"

INIT_ONCE XEngineInitOnce = INIT_ONCE_STATIC_INIT;

IInspectable* AsIInspectable(Platform::Object^ Object)
{
	return reinterpret_cast<IInspectable*>(Object);
}

// NOTE THAT THIS ENDS UP CREATING AN EXTRA REFERENCE ON THE XENGINE. 
// So once created, this instance will never be destroyed.
BOOL CALLBACK XEngine::InitXAudioEngine(PINIT_ONCE InitOnce, PVOID Param, PVOID *lpContext)
{
#if 0
	// Use this had XEngine been a ref class
	XEngine^ engine = ref new XEngine;
	Microsoft::WRL::ComPtr<IInspectable> spInspectable = AsIInspectable(engine); // Should result in an AddRef
	*lpContext = reinterpret_cast<PVOID>(spInspectable.Detach());
#else
	XEngine* engine = new XEngine();
	*lpContext = reinterpret_cast<PVOID>(engine);
#endif
	return (engine != nullptr);
}

XEngine* XEngine::TheXEngine()
{
	PVOID pContext;
	InitOnceExecuteOnce(&XEngineInitOnce, InitXAudioEngine, nullptr, &pContext);

	return reinterpret_cast<XEngine*>(pContext);
}

XEngine::XEngine()
{
	XAUDIO2_VOICE_DETAILS voiceDetails;

	ThrowIfFailed(XAudio2Create(_AudioEngine.GetAddressOf()));

	ThrowIfFailed(_AudioEngine->CreateMasteringVoice(&_MasteringVoice));

	_MasteringVoice->GetVoiceDetails(&voiceDetails);

	_MasteringVoiceFormat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	_MasteringVoiceFormat.Format.nChannels = voiceDetails.InputChannels;
	_MasteringVoiceFormat.Format.nSamplesPerSec = voiceDetails.InputSampleRate;
	_MasteringVoiceFormat.Format.wBitsPerSample = 32;
	_MasteringVoiceFormat.Format.nBlockAlign = _MasteringVoiceFormat.Format.nChannels * _MasteringVoiceFormat.Format.wBitsPerSample / 8;
	_MasteringVoiceFormat.Format.nAvgBytesPerSec = _MasteringVoiceFormat.Format.nBlockAlign * _MasteringVoiceFormat.Format.nSamplesPerSec;
	_MasteringVoiceFormat.Format.cbSize = sizeof(_MasteringVoiceFormat)-sizeof(WAVEFORMATEX);
	ThrowIfFailed(_MasteringVoice->GetChannelMask(&_MasteringVoiceFormat.dwChannelMask));
	_MasteringVoiceFormat.Samples.wValidBitsPerSample = 32;
	_MasteringVoiceFormat.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

	SamplingRate = voiceDetails.InputSampleRate;
	NumChannels = voiceDetails.InputChannels;

}

HRESULT XEngine::AddSourceVoice(_In_ IXAudio2VoiceCallback* pCallBack, _Outptr_ IXAudio2SourceVoice** ppSourceVoice)
{
	HRESULT hr;

	IF_FAILED_JUMP(_AudioEngine->CreateSourceVoice(ppSourceVoice, reinterpret_cast<WAVEFORMATEX*>(&_MasteringVoiceFormat), XAUDIO2_VOICE_USEFILTER, XAUDIO2_DEFAULT_FREQ_RATIO, pCallBack), Exit);

Exit:
	return hr;
}

std::shared_ptr<WAVEFORMATEXTENSIBLE> XEngine::MasterVoiceFormat()
{
#if 0
	std::shared_ptr<WAVEFORMATEX> p = std::shared_ptr<WAVEFORMATEX>(reinterpret_cast<WAVEFORMATEX*>(new char[sizeof(_MasteringVoiceFormat)]));
	memcpy(p.get(), &_MasteringVoiceFormat, sizeof(_MasteringVoiceFormat));
	return p;
#else
	return std::make_shared<WAVEFORMATEXTENSIBLE>(_MasteringVoiceFormat);
#endif
}
