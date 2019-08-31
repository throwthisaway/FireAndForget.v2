#include "pch.h"
#include "App.h"
#include "Content/UI.h"
#include <ppltasks.h>

using namespace FireAndForget_v2;

using namespace concurrency;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::System;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;

using Microsoft::WRL::ComPtr;

// The DirectX 12 Application template is documented at https://go.microsoft.com/fwlink/?LinkID=613670&clcid=0x409

// The main function is only used to initialize our IFrameworkView class.
[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^)
{
	auto direct3DApplicationSource = ref new Direct3DApplicationSource();
	CoreApplication::Run(direct3DApplicationSource);
	return 0;
}

IFrameworkView^ Direct3DApplicationSource::CreateView()
{
	return ref new App();
}

App::App() :
	m_windowClosed(false),
	m_windowVisible(true)
{
}

// The first method called when the IFrameworkView is being created.
void App::Initialize(CoreApplicationView^ applicationView)
{
	// Register event handlers for app lifecycle. This example includes Activated, so that we
	// can make the CoreWindow active and start rendering on the window.
	applicationView->Activated +=
		ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(this, &App::OnActivated);

	CoreApplication::Suspending +=
		ref new EventHandler<SuspendingEventArgs^>(this, &App::OnSuspending);

	CoreApplication::Resuming +=
		ref new EventHandler<Platform::Object^>(this, &App::OnResuming);
}

// Called when the CoreWindow object is created (or re-created).
void App::SetWindow(CoreWindow^ window)
{
	m_window = window;
	window->SizeChanged += 
		ref new TypedEventHandler<CoreWindow^, WindowSizeChangedEventArgs^>(this, &App::OnWindowSizeChanged);

	window->VisibilityChanged +=
		ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &App::OnVisibilityChanged);

	window->Closed += 
		ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(this, &App::OnWindowClosed);

	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	currentDisplayInformation->DpiChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnDpiChanged);

	currentDisplayInformation->OrientationChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnOrientationChanged);

	DisplayInformation::DisplayContentsInvalidated +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnDisplayContentsInvalidated);

	m_gestureRecognizer = ref new Windows::UI::Input::GestureRecognizer();
	m_gestureRecognizer->GestureSettings = Windows::UI::Input::GestureSettings::ManipulationScale | Windows::UI::Input::GestureSettings::ManipulationScaleInertia;
	m_gestureRecognizer->ManipulationCompleted += ref new Windows::Foundation::TypedEventHandler<GestureRecognizer^, ManipulationCompletedEventArgs^>(this, &App::ManipulationCompleted);
	m_gestureRecognizer->ManipulationInertiaStarting += ref new Windows::Foundation::TypedEventHandler<GestureRecognizer^, ManipulationInertiaStartingEventArgs^>(this, &App::ManipulationInertiaStarting);
	m_gestureRecognizer->ManipulationStarted += ref new Windows::Foundation::TypedEventHandler<GestureRecognizer^, ManipulationStartedEventArgs^>(this, &App::ManipulationStarted);
	m_gestureRecognizer->ManipulationUpdated += ref new Windows::Foundation::TypedEventHandler<GestureRecognizer^, ManipulationUpdatedEventArgs^>(this, &App::ManipulationUpdated);
	m_gestureRecognizer->Holding += ref new Windows::Foundation::TypedEventHandler<GestureRecognizer^, HoldingEventArgs^>(this, &App::Holding);

	window->PointerMoved += ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow ^, Windows::UI::Core::PointerEventArgs ^>(this, &App::OnPointerMoved);
	window->PointerPressed += ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow ^, Windows::UI::Core::PointerEventArgs ^>(this, &App::OnPointerPressed);
	window->PointerReleased += ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow ^, Windows::UI::Core::PointerEventArgs ^>(this, &App::OnPointerReleased);
	window->KeyUp += ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow ^, Windows::UI::Core::KeyEventArgs ^>(this, &App::OnKeyUp);
	window->KeyDown += ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow ^, Windows::UI::Core::KeyEventArgs ^>(this, &App::OnKeyDown);
	window->CharacterReceived += ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow^, Windows::UI::Core::CharacterReceivedEventArgs^>(this, &FireAndForget_v2::App::OnCharacterReceived);
	window->PointerWheelChanged += ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow^, Windows::UI::Core::PointerEventArgs^>(this, &FireAndForget_v2::App::OnPointerWheelChanged);
}

// Initializes scene resources, or loads a previously saved app state.
void App::Load(Platform::String^ entryPoint)
{
	if (m_main == nullptr)
	{
		m_main = std::unique_ptr<FireAndForget_v2Main>(new FireAndForget_v2Main());
	}
}

// This method is called after the window becomes active.
static int i = 0;
void App::Run()
{
	while (!m_windowClosed)
	{
		if (m_windowVisible)
		{
			CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

			// TODO:: nasty hack to not allow to timeout the app and close on first start and be able to attach graphics diagnostic tools
			if (i > 500) {
				auto commandQueue = GetDeviceResources()->GetCommandQueue();
				PIXBeginEvent(commandQueue, 0, L"Update");
				{
					m_main->Update();
				}
				PIXEndEvent(commandQueue);
				UI::UpdateMouseCursor(m_window.Get());
				PIXBeginEvent(commandQueue, 0, L"Render");
				{
					if (m_main->Render())
					{
						GetDeviceResources()->Present();
					}
				}
				PIXEndEvent(commandQueue);
			}
			else ++i;
		}
		else
		{
			CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
		}
	}
}

// Required for IFrameworkView.
// Terminate events do not cause Uninitialize to be called. It will be called if your IFrameworkView
// class is torn down while the app is in the foreground.
void App::Uninitialize()
{
}

// Application lifecycle event handlers.

void App::OnActivated(CoreApplicationView^ applicationView, IActivatedEventArgs^ args)
{
	// Run() won't start until the CoreWindow is activated.
	CoreWindow::GetForCurrentThread()->Activate();
}

void App::OnSuspending(Platform::Object^ sender, SuspendingEventArgs^ args)
{
	// Save app state asynchronously after requesting a deferral. Holding a deferral
	// indicates that the application is busy performing suspending operations. Be
	// aware that a deferral may not be held indefinitely. After about five seconds,
	// the app will be forced to exit.
	SuspendingDeferral^ deferral = args->SuspendingOperation->GetDeferral();

	create_task([this, deferral]()
	{
		m_main->OnSuspending();
		deferral->Complete();
	});
}

void App::OnResuming(Platform::Object^ sender, Platform::Object^ args)
{
	// Restore any data or state that was unloaded on suspend. By default, data
	// and state are persisted when resuming from suspend. Note that this event
	// does not occur if the app was previously terminated.

	m_main->OnResuming();
}

// Window event handlers.

void App::OnWindowSizeChanged(CoreWindow^ sender, WindowSizeChangedEventArgs^ args)
{
	GetDeviceResources()->WaitForGpu();
	UI::BeforeResize();
	GetDeviceResources()->SetLogicalSize(Size(sender->Bounds.Width, sender->Bounds.Height));
	m_main->OnWindowSizeChanged();
}

void App::OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args)
{
	m_windowVisible = args->Visible;
}

void App::OnWindowClosed(CoreWindow^ sender, CoreWindowEventArgs^ args)
{
	m_windowClosed = true;
}

// DisplayInformation event handlers.

void App::OnDpiChanged(DisplayInformation^ sender, Object^ args)
{
	// Note: The value for LogicalDpi retrieved here may not match the effective DPI of the app
	// if it is being scaled for high resolution devices. Once the DPI is set on DeviceResources,
	// you should always retrieve it using the GetDpi method.
	// See DeviceResources.cpp for more details.
	GetDeviceResources()->SetDpi(sender->LogicalDpi);
	m_main->OnWindowSizeChanged();
}

void App::OnOrientationChanged(DisplayInformation^ sender, Object^ args)
{
	GetDeviceResources()->SetCurrentOrientation(sender->CurrentOrientation);
	m_main->OnWindowSizeChanged();
}

void App::OnDisplayContentsInvalidated(DisplayInformation^ sender, Object^ args)
{
	GetDeviceResources()->ValidateDevice();
}

std::shared_ptr<DX::DeviceResources> App::GetDeviceResources()
{
	if (m_deviceResources != nullptr && m_deviceResources->IsDeviceRemoved())
	{
		// All references to the existing D3D device must be released before a new device
		// can be created.

		m_deviceResources = nullptr;
		m_main->OnDeviceRemoved();

#if defined(_DEBUG)
		ComPtr<IDXGIDebug1> dxgiDebug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
		{
			dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
		}
#endif
	}

	if (m_deviceResources == nullptr)
	{
		m_deviceResources = std::make_shared<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM);
		m_deviceResources->SetWindow(CoreWindow::GetForCurrentThread());
		m_main->CreateRenderers(m_deviceResources);
	}
	return m_deviceResources;
}

void App::OnPointerMoved(Windows::UI::Core::CoreWindow ^sender, Windows::UI::Core::PointerEventArgs ^args)
{
	UI::UpdateKeyboardModifiers(args->KeyModifiers == VirtualKeyModifiers::Control, args->KeyModifiers == VirtualKeyModifiers::Menu, args->KeyModifiers == VirtualKeyModifiers::Shift);
	float x = DX::ConvertDipsToPixels(args->CurrentPoint->Position.X, GetDeviceResources()->GetDpi());
	float y = DX::ConvertDipsToPixels(args->CurrentPoint->Position.Y, GetDeviceResources()->GetDpi());
	if (!UI::UpdateMousePos((int)x, (int)y)) {
		if (pointerdown == 1) {
			m_main->PointerMoved(args->CurrentPoint->Position.X, args->CurrentPoint->Position.Y, args->CurrentPoint->Properties->IsLeftButtonPressed, args->CurrentPoint->Properties->IsMiddleButtonPressed, args->CurrentPoint->Properties->IsRightButtonPressed, (size_t)args->KeyModifiers);
		}
		unsigned int pointerId = args->CurrentPoint->PointerId;
		Windows::Foundation::Collections::IVector<Windows::UI::Input::PointerPoint^>^ pointerPoints = Windows::UI::Input::PointerPoint::GetIntermediatePoints(pointerId);
		/*if (m_gestureRecognizer->IsActive)*/ m_gestureRecognizer->ProcessMoveEvents(pointerPoints);
	}
}

void App::OnPointerPressed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
{
	sender->SetPointerCapture();
	UI::UpdateKeyboardModifiers((args->KeyModifiers & VirtualKeyModifiers::Control) == VirtualKeyModifiers::Control,
		(args->KeyModifiers & VirtualKeyModifiers::Menu) == VirtualKeyModifiers::Menu,
		(args->KeyModifiers & VirtualKeyModifiers::Shift) == VirtualKeyModifiers::Shift);
	if (!UI::UpdateMouseButton(args->CurrentPoint->Properties->IsLeftButtonPressed, args->CurrentPoint->Properties->IsRightButtonPressed, args->CurrentPoint->Properties->IsMiddleButtonPressed)) {
		pointerdown++;
		m_main->PointerPressed(args->CurrentPoint->Position.X, args->CurrentPoint->Position.Y, args->CurrentPoint->Properties->IsLeftButtonPressed, args->CurrentPoint->Properties->IsMiddleButtonPressed, args->CurrentPoint->Properties->IsRightButtonPressed);
		unsigned int pointerId = args->CurrentPoint->PointerId;
		Windows::UI::Input::PointerPoint^ pointerPoint = Windows::UI::Input::PointerPoint::GetCurrentPoint(pointerId);
		/*if (m_gestureRecognizer->IsActive)*/ m_gestureRecognizer->ProcessDownEvent(pointerPoint);
	}
}

void App::OnPointerReleased(Windows::UI::Core::CoreWindow ^sender, Windows::UI::Core::PointerEventArgs ^args)
{
	if (!UI::UpdateMouseButton(args->CurrentPoint->Properties->IsLeftButtonPressed, args->CurrentPoint->Properties->IsRightButtonPressed, args->CurrentPoint->Properties->IsMiddleButtonPressed)) {
		pointerdown--;
		// TODO:: UI::UpdateKeyboardModifiers();
		m_main->PointerReleased(args->CurrentPoint->Position.X, args->CurrentPoint->Position.Y, args->CurrentPoint->Properties->IsLeftButtonPressed, args->CurrentPoint->Properties->IsMiddleButtonPressed, args->CurrentPoint->Properties->IsRightButtonPressed);
		unsigned int pointerId = args->CurrentPoint->PointerId;
		Windows::UI::Input::PointerPoint^ pointerPoint = Windows::UI::Input::PointerPoint::GetCurrentPoint(pointerId);
		/*if (m_gestureRecognizer->IsActive)*/ m_gestureRecognizer->ProcessUpEvent(pointerPoint);
	}
	sender->ReleasePointerCapture();
}

void App::OnKeyUp(Windows::UI::Core::CoreWindow ^sender, Windows::UI::Core::KeyEventArgs ^args)
{
	if (!UI::UpdateKeyboard((int)args->VirtualKey, false)) {
		m_main->KeyUp(args->VirtualKey);
	}
}

void App::OnKeyDown(Windows::UI::Core::CoreWindow ^sender, Windows::UI::Core::KeyEventArgs ^args)
{
	if (!UI::UpdateKeyboard((int)args->VirtualKey, true)) {
		m_main->KeyDown(args->VirtualKey);
	}
}

void App::ManipulationCompleted(Windows::UI::Input::GestureRecognizer^ gestureRecognizer, Windows::UI::Input::ManipulationCompletedEventArgs^ args) {
	//ManipulationDelta m = args->Cumulative;
	//if (m.Expansion > 0)
	//{
	//	m_main->Zoom(m.Expansion);
	//	//textBoxGesture.Text = "Zoom gesture detected";
	//}
	//else if (m.Expansion < 0)
	//{
	//	m_main->Zoom(m.Expansion);
	//	//textBoxGesture.Text = "Pinch gesture detected";
	//}

	//if (m.Rotation != 0.0)
	//{
	//	//textBoxGesture.Text = "Rotation detected";
	//}
}

void App::ManipulationInertiaStarting(Windows::UI::Input::GestureRecognizer^ gestureRecognizer, Windows::UI::Input::ManipulationInertiaStartingEventArgs^ args) {

}

void App::ManipulationStarted(Windows::UI::Input::GestureRecognizer^ gestureRecognizer, Windows::UI::Input::ManipulationStartedEventArgs^ args) {

}

void App::ManipulationUpdated(Windows::UI::Input::GestureRecognizer^ gestureRecognizer, Windows::UI::Input::ManipulationUpdatedEventArgs^ args) {
	if (holding) return;
	ManipulationDelta m = args->Cumulative;
	if (m.Expansion > 0)
	{
		m_main->Zoom(m.Expansion);
		//textBoxGesture.Text = "Zoom gesture detected";
	}
	else if (m.Expansion < 0)
	{
		m_main->Zoom(m.Expansion);
		//textBoxGesture.Text = "Pinch gesture detected";
	}

	if (m.Rotation != 0.0)
	{
		//textBoxGesture.Text = "Rotation detected";
	}
}


void App::Holding(Windows::UI::Input::GestureRecognizer^ gestureRecognizer, Windows::UI::Input::HoldingEventArgs^ args) {
	//string holdingState = "";
	switch (args->HoldingState)
	{
	case HoldingState::Canceled:
		//holdingState = "Cancelled";
		holding = false;
		break;
	case HoldingState::Completed:
		//holdingState = "Completed";
		holding = false;
		break;
	case HoldingState::Started:
		//holdingState = "Started";
		holding = true;
		break;
	}
	//textBoxGesture.Text = "Holding state = " + holdingState;
}


void FireAndForget_v2::App::OnCharacterReceived(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CharacterReceivedEventArgs^ args)
{
	UI::UpdateKeyboardInput(args->KeyCode);
}


void FireAndForget_v2::App::OnPointerWheelChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
{
	UI::UpdateMouseWheel(args->CurrentPoint->Properties->MouseWheelDelta, args->CurrentPoint->Properties->IsHorizontalMouseWheel);
}
