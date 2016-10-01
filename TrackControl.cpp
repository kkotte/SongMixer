﻿//
// TrackControl.cpp
// Implementation of the TrackControl class.
//

#include "pch.h"
#include "TrackControl.h"

using namespace SongMixer;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Documents;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::UI::Xaml::Media;

// The Templated Control item template is documented at http://go.microsoft.com/fwlink/?LinkId=234235

TrackControl::TrackControl()
{
	DefaultStyleKey = "SongMixer.TrackControl";
}
