﻿#pragma once

#include "pch.h"
#include "Common\DeviceResources.h"
#include "FireAndForget_v2Main.h"

namespace FireAndForget_v2
{
	// Main entry point for our app. Connects the app with the Windows shell and handles application lifecycle events.
	ref class App sealed : public Windows::ApplicationModel::Core::IFrameworkView
	{
	public:
		App();

		// IFrameworkView methods.
		virtual void Initialize(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView);
		virtual void SetWindow(Windows::UI::Core::CoreWindow^ window);
		virtual void Load(Platform::String^ entryPoint);
		virtual void Run();
		virtual void Uninitialize();

	protected:
		// Application lifecycle event handlers.
		void OnActivated(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView, Windows::ApplicationModel::Activation::IActivatedEventArgs^ args);
		void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ args);
		void OnResuming(Platform::Object^ sender, Platform::Object^ args);

		// Window event handlers.
		void OnWindowSizeChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::WindowSizeChangedEventArgs^ args);
		void OnVisibilityChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args);
		void OnWindowClosed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CoreWindowEventArgs^ args);

		// DisplayInformation event handlers.
		void OnDpiChanged(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);
		void OnOrientationChanged(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);
		void OnDisplayContentsInvalidated(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);

	private:
		Platform::Agile<Windows::UI::Core::CoreWindow> m_window;
		// Private accessor for m_deviceResources, protects against device removed errors.
		std::shared_ptr<DX::DeviceResources> GetDeviceResources();

		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::unique_ptr<FireAndForget_v2Main> m_main;
		bool m_windowClosed;
		bool m_windowVisible;

		int pointerdown = 0;
		bool holding = false;
		Windows::UI::Input::GestureRecognizer^ m_gestureRecognizer;
		void OnPointerMoved(Windows::UI::Core::CoreWindow ^sender, Windows::UI::Core::PointerEventArgs ^args);
		void OnPointerPressed(Windows::UI::Core::CoreWindow ^sender, Windows::UI::Core::PointerEventArgs ^args);
		void OnPointerReleased(Windows::UI::Core::CoreWindow ^sender, Windows::UI::Core::PointerEventArgs ^args);
		void OnKeyUp(Windows::UI::Core::CoreWindow ^sender, Windows::UI::Core::KeyEventArgs ^args);
		void OnKeyDown(Windows::UI::Core::CoreWindow ^sender, Windows::UI::Core::KeyEventArgs ^args);
		void ManipulationCompleted(Windows::UI::Input::GestureRecognizer^ gestureRecognizer, Windows::UI::Input::ManipulationCompletedEventArgs^ args);
		void ManipulationInertiaStarting(Windows::UI::Input::GestureRecognizer^ gestureRecognizer, Windows::UI::Input::ManipulationInertiaStartingEventArgs^ args);
		void ManipulationStarted(Windows::UI::Input::GestureRecognizer^ gestureRecognizer, Windows::UI::Input::ManipulationStartedEventArgs^ args);
		void ManipulationUpdated(Windows::UI::Input::GestureRecognizer^ gestureRecognizer, Windows::UI::Input::ManipulationUpdatedEventArgs^ args);
		void Holding(Windows::UI::Input::GestureRecognizer^ gestureRecognizer, Windows::UI::Input::HoldingEventArgs^ args);

	};
}

ref class Direct3DApplicationSource sealed : Windows::ApplicationModel::Core::IFrameworkViewSource
{
public:
	virtual Windows::ApplicationModel::Core::IFrameworkView^ CreateView();
};
