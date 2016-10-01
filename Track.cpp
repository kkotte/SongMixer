#include "pch.h"
#include "Track.h"
#include "XEngine.h"

#define PREROLL_DURATION_IN_MS (100)
#define LOW_WATER_MARK_IN_MS (500)
#define HIGH_WATER_MARK_IN_MS (1000)

using namespace SongMixer;

Track::Track(String^ name, IRandomAccessStream^ ras)
: _Name(name)
, _SourceVoice(nullptr)
, _SourceVoiceCallback(this)
, _bEOS(false)
, _SamplesBuffered(0)
, _SourceReaderCallback(this)
, _bPrerolling(false)
, _bIsPlaying(false)
, _bLPFEnabled(false)
, _bHPFEnabled(false)
{

	ThrowIfFailed(ConfigureSourceReader(ras, _SourceReader.GetAddressOf()));

	_cSamplesForPreroll = PREROLL_DURATION_IN_MS * _SamplingRate / 1000;
	_cSamplesLowWaterMark = LOW_WATER_MARK_IN_MS * _SamplingRate / 1000;
	_cSamplesHighWaterMark = HIGH_WATER_MARK_IN_MS * _SamplingRate / 1000;

	ThrowIfFailed(AddSourceVoice());
}

IAsyncOperation<Track^>^ Track::OpenTrackAsync(StorageFile^ file, IRandomAccessStream^ ras)
{
	return create_async(
	[=]()
	{
		return Track::OpenTrack(file, ras);
	});
}

Track^ Track::OpenTrack(Windows::Storage::StorageFile^ file, IRandomAccessStream^ ras)
{
	return (file == nullptr) ? nullptr : ref new Track(file->DisplayName, ras);
}

HRESULT Track::ConfigureSourceReader(_In_ IRandomAccessStream^ ras, _Outptr_ IMFSourceReader** ppSourceReader)
{
	HRESULT hr;
	ComPtr<IMFSourceReader> spSourceReader;
	ComPtr<IMFMediaType> spMasterVoiceMediaType;
	ComPtr<IMFByteStream> spMFByteStream;
	ComPtr<IMFAttributes> spAttributes;
	std::shared_ptr<WAVEFORMATEXTENSIBLE> MasterVoiceFormat;

	IF_FAILED_JUMP(MFStartup(MF_VERSION), Exit);
	IF_FAILED_JUMP(MFCreateMFByteStreamOnStreamEx(reinterpret_cast<IUnknown*>(ras), spMFByteStream.GetAddressOf()), Exit);
	
	IF_FAILED_JUMP(MFCreateAttributes(spAttributes.GetAddressOf(), 1), Exit);
	IF_FAILED_JUMP(spAttributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, static_cast<IMFSourceReaderCallback*>(&_SourceReaderCallback)), Exit);

	IF_FAILED_JUMP(MFCreateSourceReaderFromByteStream(spMFByteStream.Get(), spAttributes.Get(), spSourceReader.GetAddressOf()), Exit); // MFCreateSourceReaderFromURL  only works for msappx:// and http:// URLs from Store apps

	// Deselect all other streams and select the first audio stream 
	IF_FAILED_JUMP(spSourceReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE), Exit);
	IF_FAILED_JUMP(spSourceReader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE), Exit);

	// Ask the source reader to convert to the MasteringVoice Format
	// Get the Mastering voice's format
	XEngine* engine = XEngine::TheXEngine();
	MasterVoiceFormat = engine->MasterVoiceFormat();

	IF_FAILED_JUMP(MFCreateMediaType(spMasterVoiceMediaType.GetAddressOf()), Exit);
	IF_FAILED_JUMP(MFInitMediaTypeFromWaveFormatEx(spMasterVoiceMediaType.Get(), reinterpret_cast<WAVEFORMATEX*>(MasterVoiceFormat.get()), sizeof(WAVEFORMATEXTENSIBLE)), Exit);

	IF_FAILED_JUMP(spSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, spMasterVoiceMediaType.Get()), Exit);

	_nBlockAlign = MasterVoiceFormat->Format.nBlockAlign;
	_SamplingRate = MasterVoiceFormat->Format.nSamplesPerSec;
	_NumChannels = MasterVoiceFormat->Format.nChannels;
	*ppSourceReader = spSourceReader.Detach();

Exit:
	return hr;
}

HRESULT Track::AddSourceVoice()
{
	HRESULT hr;
	XEngine* engine = XEngine::TheXEngine();
	IF_FAILED_JUMP(engine->AddSourceVoice(static_cast<IXAudio2VoiceCallback*>(&_SourceVoiceCallback), &_SourceVoice), Exit);
	
	IF_FAILED_JUMP(AddEffectChain(), Exit);

	IF_FAILED_JUMP(PrerollAndStart(), Exit);
Exit:
	return hr;
}

void Track::OnBufferEnd(PVOID pBufferContext)
{
	Concurrency::critical_section::scoped_lock lock(_SamplesBufferedLock);
	XAudioBuffer* xab = (XAudioBuffer*)pBufferContext;

	_SamplesBuffered -= xab->_cSamples;
	if (xab->_spBuffer)
	{
		xab->_spBuffer->Unlock();
	}
	delete xab;

	if (!_bEOS && _SamplesBuffered < _cSamplesLowWaterMark)
	{
		ThrowIfFailed(_SourceReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, nullptr, nullptr, nullptr));
	}
}

HRESULT Track::PrerollAndStart()
{
	HRESULT hr;
	bool bPrerolled = false;
	unsigned int SamplesRequiredToCompletePreroll = PREROLL_DURATION_IN_MS * _SamplingRate / 1000;
	unsigned int cSamplesPrerolled = 0;

	// Issue a sample request to initiate preroll
	_bPrerolling = true;
	IF_FAILED_JUMP(_SourceReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, nullptr, nullptr, nullptr), Exit);

Exit:
	return hr;
}

HRESULT Track::OnReadSample(_In_ HRESULT hrStatus, _In_ DWORD StreamIndex, _In_ DWORD dwStreamFlags, _In_ LONGLONG llTimestamp, _In_ IMFSample *pSample)
{
	unsigned int cSamplesInBuffer;
	ThrowIfFailed(hrStatus);

	if (dwStreamFlags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
	{
		ThrowIfFailed(MF_E_INVALIDMEDIATYPE);
	}

	_bEOS = dwStreamFlags & MF_SOURCE_READERF_ENDOFSTREAM ? true : false;
	
	if (pSample)
	{
		ComPtr<IMFMediaBuffer> spBuffer;
		DWORD cbBuffer;

		ThrowIfFailed(pSample->ConvertToContiguousBuffer(spBuffer.GetAddressOf()));
		ThrowIfFailed(spBuffer->GetCurrentLength(&cbBuffer));
		cSamplesInBuffer = cbBuffer / _nBlockAlign;

		ThrowIfFailed(SubmitBuffer(spBuffer.Get(), cSamplesInBuffer, _bEOS));
	}
	else
	{
		if (_bEOS)
		{
			ThrowIfFailed(SubmitBuffer(nullptr, 0, _bEOS));
		}
	}

	{
		Concurrency::critical_section::scoped_lock lock(_SamplesBufferedLock);
		
		// Handle prerolling check first
		if (_bPrerolling)
		{
			if (_bEOS || _SamplesBuffered >= _cSamplesForPreroll)
			{
				_bPrerolling = false;
				ThrowIfFailed(_SourceVoice->Start(0));
				SetPlayState(true);
			}
		}

		// Next, check against high water mark
		if (!_bEOS && _SamplesBuffered < _cSamplesHighWaterMark)
		{
			ThrowIfFailed(_SourceReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, nullptr, nullptr, nullptr));
		}
	}

	return S_OK;
}


HRESULT Track::SubmitBuffer(IMFMediaBuffer *pBuffer, unsigned int cSamples, bool bEOS)
{
	HRESULT hr;
	XAUDIO2_BUFFER xb = { 0 };
	std::unique_ptr<XAudioBuffer> xab = std::make_unique<XAudioBuffer>(pBuffer, cSamples);

	xb.Flags = bEOS ? XAUDIO2_END_OF_STREAM : 0;
	xb.pContext = xab.get();
	xb.AudioBytes = cSamples * _nBlockAlign;
	
	if (pBuffer)
	{
		IF_FAILED_JUMP(pBuffer->Lock((BYTE**)&xb.pAudioData, nullptr, nullptr), Exit);
		IF_FAILED_JUMP(_SourceVoice->SubmitSourceBuffer(&xb), Exit);
		xab.release();
	}
	else
	{
		IF_FAILED_JUMP(_SourceVoice->Discontinuity(), Exit);
	}

	{
		Concurrency::critical_section::scoped_lock lock(_SamplesBufferedLock);
		_SamplesBuffered += cSamples;
	}

Exit:
	return hr;
}

void Track::SetPlayState(bool isPlaying)
{
	_bIsPlaying = isPlaying;
	OnPlayStateChanged();
}

void Track::OnStreamEnd()
{
	PROPVARIANT var = { 0 };
	SetPlayState(false);
	var.vt = VT_I8;
	ThrowIfFailed(_SourceReader->SetCurrentPosition(GUID_NULL, var));
}

void Track::Start()
{
	if (!_bIsPlaying)
	{
		if (!_bEOS)
		{
			ThrowIfFailed(_SourceVoice->Start(0));
			SetPlayState(true);
		}
		else
		{
			ThrowIfFailed(PrerollAndStart());
		}
	}
}

void Track::Stop()
{
	if (_bIsPlaying)
	{
		ThrowIfFailed(_SourceVoice->Stop(0));
		SetPlayState(false);
	}
}

HRESULT Track::AddEffectChain()
{
	// Add a reverb effect
	HRESULT hr;
	ComPtr<IUnknown> spReverb;
	XAUDIO2_EFFECT_DESCRIPTOR descriptors[1] = { 0 };
	XAUDIO2_EFFECT_CHAIN chain = { 0 };

	IF_FAILED_JUMP(XAudio2CreateReverb(spReverb.GetAddressOf(), 0), Exit);

	descriptors[0].InitialState = false;
	descriptors[0].OutputChannels = _NumChannels;
	descriptors[0].pEffect = spReverb.Get();

	chain.EffectCount = ARRAYSIZE(descriptors);
	chain.pEffectDescriptors = descriptors;

	IF_FAILED_JUMP(_SourceVoice->SetEffectChain(&chain), Exit);


	XAUDIO2FX_REVERB_PARAMETERS reverbParameters;
	reverbParameters.ReflectionsDelay = XAUDIO2FX_REVERB_DEFAULT_REFLECTIONS_DELAY;
	reverbParameters.ReverbDelay = XAUDIO2FX_REVERB_DEFAULT_REVERB_DELAY;
	reverbParameters.RearDelay = XAUDIO2FX_REVERB_DEFAULT_REAR_DELAY;
	reverbParameters.PositionLeft = XAUDIO2FX_REVERB_DEFAULT_POSITION;
	reverbParameters.PositionRight = XAUDIO2FX_REVERB_DEFAULT_POSITION;
	reverbParameters.PositionMatrixLeft = XAUDIO2FX_REVERB_DEFAULT_POSITION_MATRIX;
	reverbParameters.PositionMatrixRight = XAUDIO2FX_REVERB_DEFAULT_POSITION_MATRIX;
	reverbParameters.EarlyDiffusion = XAUDIO2FX_REVERB_DEFAULT_EARLY_DIFFUSION;
	reverbParameters.LateDiffusion = XAUDIO2FX_REVERB_DEFAULT_LATE_DIFFUSION;
	reverbParameters.LowEQGain = XAUDIO2FX_REVERB_DEFAULT_LOW_EQ_GAIN;
	reverbParameters.LowEQCutoff = XAUDIO2FX_REVERB_DEFAULT_LOW_EQ_CUTOFF;
	reverbParameters.HighEQGain = XAUDIO2FX_REVERB_DEFAULT_HIGH_EQ_GAIN;
	reverbParameters.HighEQCutoff = XAUDIO2FX_REVERB_DEFAULT_HIGH_EQ_CUTOFF;
	reverbParameters.RoomFilterFreq = XAUDIO2FX_REVERB_DEFAULT_ROOM_FILTER_FREQ;
	reverbParameters.RoomFilterMain = XAUDIO2FX_REVERB_DEFAULT_ROOM_FILTER_MAIN;
	reverbParameters.RoomFilterHF = XAUDIO2FX_REVERB_DEFAULT_ROOM_FILTER_HF;
	reverbParameters.ReflectionsGain = XAUDIO2FX_REVERB_DEFAULT_REFLECTIONS_GAIN;
	reverbParameters.ReverbGain = XAUDIO2FX_REVERB_DEFAULT_REVERB_GAIN;
	reverbParameters.DecayTime = XAUDIO2FX_REVERB_DEFAULT_DECAY_TIME;
	reverbParameters.Density = XAUDIO2FX_REVERB_DEFAULT_DENSITY;
	reverbParameters.RoomSize = XAUDIO2FX_REVERB_DEFAULT_ROOM_SIZE;
	reverbParameters.WetDryMix = XAUDIO2FX_REVERB_DEFAULT_WET_DRY_MIX;

	IF_FAILED_JUMP(_SourceVoice->SetEffectParameters(0, &reverbParameters, sizeof(reverbParameters)), Exit);
	IF_FAILED_JUMP(_SourceVoice->DisableEffect(0), Exit);
Exit:
	return hr;
}