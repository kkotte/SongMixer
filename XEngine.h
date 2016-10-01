#pragma once

class XEngine sealed // I'm not making XEngine a ref class, since it needs to access data types such as IXaudio2SourceVoice from its public methods
{
public:
	static XEngine* TheXEngine();
	HRESULT AddSourceVoice(_In_ IXAudio2VoiceCallback* pCallBack,
						   _Outptr_ IXAudio2SourceVoice** ppSourceVoice);
	std::shared_ptr<WAVEFORMATEXTENSIBLE> MasterVoiceFormat();

	unsigned int SamplingRate;
	unsigned int NumChannels;
	DWORD ChannelMask;

private:
	XEngine();
	static BOOL CALLBACK XEngine::InitXAudioEngine(PINIT_ONCE InitOnce, PVOID Param, PVOID *lpContext);

	Microsoft::WRL::ComPtr<IXAudio2> _AudioEngine;
	IXAudio2MasteringVoice* _MasteringVoice;
	WAVEFORMATEXTENSIBLE _MasteringVoiceFormat;
};

