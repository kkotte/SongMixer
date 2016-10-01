//
// MainPage.xaml.cpp
// Implementation of the MainPage class
//

#include "pch.h"
#include "MainPage.xaml.h"
#include "Track.h"

using namespace SongMixer;

using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

// The Basic Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=234237

MainPage::MainPage()
{
	InitializeComponent();
	SetValue(_defaultViewModelProperty, ref new Map<String^,Object^>(std::less<String^>()));
	auto navigationHelper = ref new Common::NavigationHelper(this);
	SetValue(_navigationHelperProperty, navigationHelper);
	navigationHelper->LoadState += ref new Common::LoadStateEventHandler(this, &MainPage::LoadState);
	navigationHelper->SaveState += ref new Common::SaveStateEventHandler(this, &MainPage::SaveState);
}

DependencyProperty^ MainPage::_defaultViewModelProperty =
	DependencyProperty::Register("DefaultViewModel",
		TypeName(IObservableMap<String^,Object^>::typeid), TypeName(MainPage::typeid), nullptr);

/// <summary>
/// used as a trivial view model.
/// </summary>
IObservableMap<String^, Object^>^ MainPage::DefaultViewModel::get()
{
	return safe_cast<IObservableMap<String^, Object^>^>(GetValue(_defaultViewModelProperty));
}

DependencyProperty^ MainPage::_navigationHelperProperty =
	DependencyProperty::Register("NavigationHelper",
		TypeName(Common::NavigationHelper::typeid), TypeName(MainPage::typeid), nullptr);

/// <summary>
/// Gets an implementation of <see cref="NavigationHelper"/> designed to be
/// used as a trivial view model.
/// </summary>
Common::NavigationHelper^ MainPage::NavigationHelper::get()
{
	return safe_cast<Common::NavigationHelper^>(GetValue(_navigationHelperProperty));
}

#pragma region Navigation support

/// The methods provided in this section are simply used to allow
/// NavigationHelper to respond to the page's navigation methods.
/// 
/// Page specific logic should be placed in event handlers for the  
/// <see cref="NavigationHelper::LoadState"/>
/// and <see cref="NavigationHelper::SaveState"/>.
/// The navigation parameter is available in the LoadState method 
/// in addition to page state preserved during an earlier session.

void MainPage::OnNavigatedTo(NavigationEventArgs^ e)
{
	NavigationHelper->OnNavigatedTo(e);
}

void MainPage::OnNavigatedFrom(NavigationEventArgs^ e)
{
	NavigationHelper->OnNavigatedFrom(e);
}

#pragma endregion

/// <summary>
/// Populates the page with content passed during navigation. Any saved state is also
/// provided when recreating a page from a prior session.
/// </summary>
/// <param name="sender">
/// The source of the event; typically <see cref="NavigationHelper"/>
/// </param>
/// <param name="e">Event data that provides both the navigation parameter passed to
/// <see cref="Frame.Navigate(Type, Object)"/> when this page was initially requested and
/// a dictionary of state preserved by this page during an earlier
/// session. The state will be null the first time a page is visited.</param>
void MainPage::LoadState(Object^ sender, Common::LoadStateEventArgs^ e)
{
	(void) sender;	// Unused parameter
	(void) e;	// Unused parameter
}

/// <summary>
/// Preserves state associated with this page in case the application is suspended or the
/// page is discarded from the navigation cache.  Values must conform to the serialization
/// requirements of <see cref="SuspensionManager::SessionState"/>.
/// </summary>
/// <param name="sender">The source of the event; typically <see cref="NavigationHelper"/></param>
/// <param name="e">Event data that provides an empty dictionary to be populated with
/// serializable state.</param>
void MainPage::SaveState(Object^ sender, Common::SaveStateEventArgs^ e){
	(void) sender;	// Unused parameter
	(void) e; // Unused parameter
}

void SongMixer::MainPage::AddNewTrack(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	// Use file picker to pick a file
	auto openPicker = ref new Windows::Storage::Pickers::FileOpenPicker();
	openPicker->SuggestedStartLocation = Windows::Storage::Pickers::PickerLocationId::MusicLibrary;
	openPicker->ViewMode = Windows::Storage::Pickers::PickerViewMode::Thumbnail;
	openPicker->FileTypeFilter->Append("*");

#if 0
	Concurrency::create_task(openPicker->PickSingleFileAsync())
	.then([this](Windows::Storage::StorageFile^ file) 
	{
		// return SongMixer::Track::OpenTrack(file);      // This will block UI if you don't specify task_continuation_context::use_arbitrary
		
		return SongMixer::Track::OpenTrackAsync(file); // Expose and use an async method if this method is meant to be projected, or called from the UI thread.
		                                               // In this case, this task continuation is already running on a non-ui thread, so it is ok to call synchronous OpenTrack()
		                                               //
		                                               // If you didn't specify task_continuation_context::use_arbitrary() for this continuation, it would've executed on the UI thread, and then 
		                                               // use of the Async version becomes mandatory
	}, Concurrency::task_continuation_context::use_arbitrary()) // So that the track is created on another thread
		.then([sender, this](SongMixer::Track^ track)
	{
		if (track != nullptr)
		{
			// Add a row for the newly added track
			unsigned int index = TrackList->RowDefinitions->Size;

			RowDefinition^ rd = ref new RowDefinition();
			TrackList->RowDefinitions->InsertAt(index - 1, rd);

			TextBlock^ tb = ref new TextBlock();

			tb->Style = safe_cast<Windows::UI::Xaml::Style^>(Application::Current->Resources->Lookup("SubtitleTextBlockStyle"));
			tb->Text = track->Name;

			Grid::SetRow(tb, index - 1);
			Grid::SetColumn(tb, 0);
			TrackList->Children->Append(tb);

			// Push the 'Add Track...' button down by a row
			Grid::SetRow(safe_cast<Button^>(sender), index);
		}
	}, Concurrency::task_continuation_context::use_current()); // SO that the UI is updated on this very thread
#endif

#if 1
	create_task(openPicker->PickSingleFileAsync())
	.then([this, sender](StorageFile^ file)
	{
		if (file != nullptr)
		{
			create_task(file->OpenAsync(FileAccessMode::Read))
			.then([file](IRandomAccessStream^ ras) -> Track^
			{
				return Track::OpenTrack(file, ras); // We know we are running on a non UI thread, so its OK to call the sync method as opposed to the async one
			}, task_continuation_context::use_arbitrary())
			.then([sender, this](Track^ track)
			{
				if (track != nullptr)
				{
					// Add a row for the newly added track
					unsigned int index = TrackList->RowDefinitions->Size;

					RowDefinition^ rd = ref new RowDefinition();
					TrackList->RowDefinitions->InsertAt(index - 1, rd);

					// <TextBlock Grid.Row = "0" VerticalAlignment = "Center" MaxLines="1" Grid.Column = "0" FontSize = "20" Width = "250" TextTrimming = "CharacterEllipsis">Nothing Else Matters< / TextBlock>
					TextBlock^ tb = ref new TextBlock();
					tb->Style = safe_cast<Windows::UI::Xaml::Style^>(Application::Current->Resources->Lookup("SubtitleTextBlockStyle"));
					tb->Text = track->Name;
					tb->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Center;
					tb->FontSize = 20;
					tb->Width = 250;
					tb->TextTrimming = TextTrimming::CharacterEllipsis;
					tb->MaxLines = 1;

					Grid::SetRow(tb, index - 1);
					Grid::SetColumn(tb, 0);
					TrackList->Children->Append(tb);

					// <SymbolIcon VerticalAlignment = "Center" Grid.Row = "0" Grid.Column = "1" Symbol = "Play" Margin = "10,0,0,0" / >
					SymbolIcon^ s = ref new SymbolIcon();
					s->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Center;
					s->Margin = Thickness(10, 0, 0, 0);
					Grid::SetRow(s, index - 1);
					Grid::SetColumn(s, 1);
					
					s->Tapped += ref new Windows::UI::Xaml::Input::TappedEventHandler([s, track](Object^ sender, TappedRoutedEventArgs^ e){
						if (s->Symbol == Symbol::Play)
						{
							track->Start();
						}
						else
						{
							track->Stop();
						}
					});
					TrackList->Children->Append(s);
					track->OnPlayStateChanged += ref new SongMixer::PlayStateChangedEventHandler([this, s, track](){
						Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new DispatchedHandler(
							[s, track]()
						{
							s->Symbol = track->Playing ? Symbol::Pause : Symbol::Play;
						}));
					});

					// <Slider VerticalAlignment = "Bottom" Grid.Row = "0" Grid.Column = "2" Width = "100" Margin = "10,15,0,0" / >
					Slider^ vol = ref new Slider();
					vol->Width = 100;
					vol->Value = track->Volume * 100.0;
					vol->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Bottom;
					vol->Margin = Thickness(10, 15, 0, 0);
					Grid::SetRow(vol, index - 1);
					Grid::SetColumn(vol, 2);
					
					vol->ValueChanged += ref new Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventHandler([track](Object^ sender, RangeBaseValueChangedEventArgs^ e){
						track->Volume = e->NewValue / 100.0;
					});
				
					TrackList->Children->Append(vol);

					// <CheckBox Grid.Row="0" Grid.Column="3" Margin="15,0,0,0" VerticalAlignment="Center">Reverb</CheckBox>
					CheckBox^ c = ref new CheckBox();
					c->Margin = Thickness(15, 0, 0, 0);
					c->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Center;
					c->Content = "Reverb";
					c->Checked += ref new Windows::UI::Xaml::RoutedEventHandler([track](Object^ sender, RoutedEventArgs^ e){
						track->Reverb = true;
					});
					c->Unchecked += ref new Windows::UI::Xaml::RoutedEventHandler([track](Object^ sender, RoutedEventArgs^ e){
						track->Reverb = false;
					});
					c->IsChecked = track->Reverb;
					Grid::SetRow(c, index - 1);
					Grid::SetColumn(c, 3);
					TrackList->Children->Append(c);

					CheckBox^ lpf = ref new CheckBox();
					lpf->Margin = Thickness(15, 0, 0, 0);
					lpf->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Center;
					lpf->Content = "Low Pass Filter";
					lpf->Checked += ref new Windows::UI::Xaml::RoutedEventHandler([track](Object^ sender, RoutedEventArgs^ e){
						track->LowPassFilter = true;
					});
					lpf->Unchecked += ref new Windows::UI::Xaml::RoutedEventHandler([track](Object^ sender, RoutedEventArgs^ e){
						track->LowPassFilter = false;
					});
					lpf->IsChecked = track->LowPassFilter;
					Grid::SetRow(lpf, index - 1);
					Grid::SetColumn(lpf, 4);
					TrackList->Children->Append(lpf);

					CheckBox^ hpf = ref new CheckBox();
					hpf->Margin = Thickness(15, 0, 0, 0);
					hpf->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Center;
					hpf->Content = "High Pass Filter";
					hpf->Checked += ref new Windows::UI::Xaml::RoutedEventHandler([track](Object^ sender, RoutedEventArgs^ e){
						track->HighPassFilter = true;
					});
					hpf->Unchecked += ref new Windows::UI::Xaml::RoutedEventHandler([track](Object^ sender, RoutedEventArgs^ e){
						track->HighPassFilter = false;
					});
					hpf->IsChecked = track->HighPassFilter;
					Grid::SetRow(hpf, index - 1);
					Grid::SetColumn(hpf, 5);
					TrackList->Children->Append(hpf);

					s->Symbol = track->Playing ? Symbol::Pause : Symbol::Play;

					// Push the 'Add Track...' button down by a row
					Grid::SetRow(safe_cast<Button^>(sender), index);
				}
			}, task_continuation_context::use_current()) // SO that the UI is updated on this very thread
			.then([file](task<void> t)
			{
				try
				{
					t.get(); // If this throws, it means something went wrong in the task chain above
				}
				catch (Platform::Exception^ e)
				{
					auto message = ref new Windows::UI::Popups::MessageDialog(e->Message);
					message->ShowAsync();
				}
			});
		}
	});
#endif
}
