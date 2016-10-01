#pragma once

namespace SongMixer
{
	delegate void PlayStateChangedEventHandler();

	ref class Track sealed
	{
	public:
		Track(String^ name, IRandomAccessStream^ ras);  // wstring not OK in public methods of WinRT classes. Ok in POCO 

		static IAsyncOperation<Track^>^ OpenTrackAsync(StorageFile^ file, IRandomAccessStream^ ras);
		static Track^ OpenTrack(StorageFile^ file, IRandomAccessStream^ ras);

		property String^ Name // NOTE: This has to be String^ and not std::wstring as this is a public method on a WinRT object (i.e. may need to be projected)
		{
			String^ get()
			{
				return _Name;
			}
		}

		event PlayStateChangedEventHandler^ OnPlayStateChanged;

		property bool Playing
		{
			bool get()
			{
				return _bIsPlaying;
			}
		}

		property double Volume
		{
			double get()
			{
				float vol;
				_SourceVoice->GetVolume(&vol);
				return vol;
			}

			void set(double newvol)
			{
				ThrowIfFailed(_SourceVoice->SetVolume((float)newvol));
			}
		}

		property bool Reverb
		{
			bool get()
			{
				BOOL bEnabled;
				_SourceVoice->GetEffectState(0, &bEnabled);
				return (bEnabled == TRUE);
			}

			void set(bool bEnabled)
			{
				if (bEnabled)
				{
					ThrowIfFailed(_SourceVoice->EnableEffect(0));
				}
				else
				{
					ThrowIfFailed(_SourceVoice->DisableEffect(0));
				}
			}
		}

		property bool LowPassFilter
		{
			bool get()
			{
				return _bLPFEnabled;
			}

			void set(bool bEnabled)
			{
				XAUDIO2_FILTER_PARAMETERS params;
				ZeroMemory(&params, sizeof(params));
				if (bEnabled)
				{
					params.Type = XAUDIO2_FILTER_TYPE::LowPassOnePoleFilter;
					params.Frequency = XAudio2CutoffFrequencyToOnePoleCoefficient(2000, _SamplingRate);
				}
				else
				{
					params.Type = XAUDIO2_FILTER_TYPE::LowPassFilter;
					params.Frequency = 1.0F;
					params.OneOverQ = 1.0F;
				}
				ThrowIfFailed(_SourceVoice->SetFilterParameters(&params));
				_bLPFEnabled = bEnabled;
			}
		}

		property bool HighPassFilter
		{
			bool get()
			{
				return _bHPFEnabled;
			}

			void set(bool bEnabled)
			{
				XAUDIO2_FILTER_PARAMETERS params;
				ZeroMemory(&params, sizeof(params));
				if (bEnabled)
				{
					params.Type = XAUDIO2_FILTER_TYPE::HighPassOnePoleFilter;
					params.Frequency = XAudio2CutoffFrequencyToOnePoleCoefficient(2000, _SamplingRate);
				}
				else
				{
					params.Type = XAUDIO2_FILTER_TYPE::HighPassFilter;
					params.Frequency = 1.0F;
					params.OneOverQ = 1.0F;
				}
				ThrowIfFailed(_SourceVoice->SetFilterParameters(&params));
				_bHPFEnabled = bEnabled;
			}
		}

		void Start();
		void Stop();

	private:

		HRESULT ConfigureSourceReader(_In_ IRandomAccessStream^ ras, _Outptr_ IMFSourceReader** ppSourceReader);
		HRESULT AddSourceVoice();
		void OnBufferEnd(PVOID pBufferContext);
		HRESULT PrerollAndStart();
		HRESULT SubmitBuffer(IMFMediaBuffer* pBuffer, unsigned int cSamples, bool bEOS);
		HRESULT OnReadSample(_In_ HRESULT hrStatus, _In_ DWORD StreamIndex, _In_ DWORD dwStreamFlags, _In_ LONGLONG llTimestamp, _In_ IMFSample *pSample);
		void SetPlayState(bool isPlaying);
		void OnStreamEnd();
		HRESULT AddEffectChain();

		String^ _Name;
		ComPtr<IMFSourceReader> _SourceReader;
		IXAudio2SourceVoice* _SourceVoice;
		unsigned int _nBlockAlign;
		unsigned int _SamplingRate;
		unsigned int _NumChannels;
		bool _bEOS;
		unsigned int _SamplesBuffered;
		Concurrency::critical_section _SamplesBufferedLock;
		unsigned int _cSamplesForPreroll;
		unsigned int _cSamplesLowWaterMark;
		unsigned int _cSamplesHighWaterMark;
		bool _bPrerolling;
		bool _bIsPlaying;
		bool _bLPFEnabled;
		bool _bHPFEnabled;

		class SourceVoiceCallback
		: public IXAudio2VoiceCallback
		{
		public:
			SourceVoiceCallback(Track^ track)
			: _track(track)
			{}

			// IXAudio2VoiceCallback
			STDMETHODIMP_(void) OnBufferEnd(PVOID pBufferContext) { _track->OnBufferEnd(pBufferContext); }
			STDMETHODIMP_(void) OnBufferStart(PVOID pBufferContext) {  }
			STDMETHODIMP_(void) OnLoopEnd(PVOID pBufferContext) {  }
			STDMETHODIMP_(void) OnStreamEnd() { _track->OnStreamEnd(); }
			STDMETHODIMP_(void) OnVoiceError(PVOID pBufferContext, HRESULT error) { }
			STDMETHODIMP_(void) OnVoiceProcessingPassEnd() { }
			STDMETHODIMP_(void) OnVoiceProcessingPassStart(UINT32 BytesRequired) { }

		private:
			Track^ _track;
		} _SourceVoiceCallback;

		class SourceReaderCallback
		: public IMFSourceReaderCallback
		{
		public:
			SourceReaderCallback(Track^ track)
			: _track(track)
			{}

			// IMFSourceReaderCallback
			STDMETHODIMP OnEvent(_In_ DWORD StreamIndex, _In_ IMFMediaEvent* pEvent) { return S_OK; }
			STDMETHODIMP OnFlush(_In_ DWORD StreamIndex) { return S_OK; }
			STDMETHODIMP OnReadSample(_In_ HRESULT hrStatus, _In_ DWORD StreamIndex, _In_ DWORD dwStreamFlags, _In_ LONGLONG llTimestamp, _In_ IMFSample *pSample)
			{ 
				return _track->OnReadSample(hrStatus, StreamIndex, dwStreamFlags, llTimestamp, pSample);
			}

			// IUnknown
			STDMETHOD(QueryInterface)(REFIID iid, PVOID *ppv)
			{
				*ppv = nullptr;
				if (iid == IID_IUnknown || iid == __uuidof(IMFSourceReaderCallback))
				{
					*ppv = static_cast<IMFSourceReaderCallback*>(this);
					return S_OK;
				}
				return E_NOINTERFACE;
			}

			// Lifetime of this interface is not controlled by its refcount...
			STDMETHOD_(ULONG, AddRef)() { return 1; }
			STDMETHOD_(ULONG, Release)() { return 1; }

		private:
			Track^ _track;
		} _SourceReaderCallback;
		
		class XAudioBuffer
		{
		public:
			XAudioBuffer(IMFMediaBuffer* pBuffer, unsigned int cSamples)
				: _spBuffer(pBuffer)
				, _cSamples(cSamples)
			{}

			ComPtr<IMFMediaBuffer> _spBuffer;
			unsigned int _cSamples;
		};

	};

	// Plain old C++ object - POCO
	class Track_POCO sealed
	{
	public:
		Track_POCO(std::wstring name); // wstring OK in public constructor

		// static IAsyncOperation<Track_POCO>^ OpenTrackAsync(Windows::Storage::StorageFile^ file); This doesn't compile -> Template argument to IAsyncOperation not liked

		/* Properties not allowed in POCO
		property String^ Name
		{
		String^ get()
		{
		return ref new String(_Name.c_str());
		}
		}
		*/

	private:
		std::wstring _Name;
	};


}

