#include "Game/App.hpp"
#include "Game/Game.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Renderer/RendererDX11.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine//Window/Window.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Core/Clock.hpp"


App* g_app = nullptr;
RendererDX11* g_renderer = nullptr;
AudioSystem* g_audioSystem = nullptr;
Window* g_window = nullptr;
Game* g_game = nullptr;

//App Management
//-----------------------------------------------------------------------------------------------
void App::Startup()
{
	LoadGameConfig("Data/Definitions/GameConfig.xml");
	InputConfig inputConfig;
	g_inputSystem = new InputSystem(inputConfig);

	WindowConfig windowConfig;
	windowConfig.m_aspectRatio = 2.0f;
	windowConfig.m_inputSystem = g_inputSystem;
	windowConfig.m_windowTitle = "Libra";
	g_window = new Window(windowConfig);

	RendererConfig rendererConfig;
	rendererConfig.m_window = g_window;
	g_renderer = new RendererDX11(rendererConfig);

	EventSystemConfig eventSystemConfig;
	g_eventSystem = new EventSystem(eventSystemConfig);

	DevConsoleConfig devConsoleConfig;
	devConsoleConfig.m_defaultFontName = "Data/Font/SquirrelFixedFont";
	devConsoleConfig.m_defaultLinesOnScreen = 29.5f;
	devConsoleConfig.m_defaultRenderer = g_renderer;
	devConsoleConfig.m_defaultFontAspect = 1.f;
	g_devConsole = new DevConsole(devConsoleConfig);

	AudioConfig audioConfig;
	g_audioSystem = new AudioSystem(audioConfig);

	g_window->Startup();
	g_renderer->Startup();
	g_eventSystem->Startup();
	g_devConsole->Startup();
	g_inputSystem->Startup();
	g_audioSystem->Startup();

	g_game = new Game();
	FireEvent("Controls");

	SubscribeEventCallbackFunction("Quit", QuitEvent);
}

void App::Shutdown()
{
	delete g_game;
	g_game = nullptr;


	g_audioSystem->Shutdown();
	g_devConsole->ShutDown();
	g_eventSystem->ShutDown();
	g_inputSystem->Shutdown();
	g_renderer->Shutdown();
	g_window->Shutdown();

	delete g_audioSystem;
	g_audioSystem = nullptr;

	delete g_renderer;
	g_renderer = nullptr;

	delete g_window;
	g_window = nullptr;

	delete g_inputSystem;
	g_inputSystem = nullptr;
}

//Frame Flow
//-----------------------------------------------------------------------------------------------
void App::RunFrame()
{
	float timeNow = static_cast<float>(GetCurrentTimeSeconds());
	float deltaSeconds = GetClamped(timeNow - m_timeLastFrameStart, -1.f, 0.1f);
	m_timeLastFrameStart = timeNow;

	BeginFrame();
	Update(deltaSeconds);
	Render();
	EndFrame();
}

void App::BeginFrame()
{
	Clock::TickSystemClock();
	g_window->BeginFrame();
	g_inputSystem->BeginFrame();
	g_renderer->BeginFrame();
	g_eventSystem->BeginFrame();
	g_devConsole->BeginFrame();
	g_audioSystem->BeginFrame();
	g_game->BeginFrame();
}

void App::Update(float deltaSeconds)
{
	g_game->Update(deltaSeconds);
	//CheckDevConsoleKeyboardInputs(deltaSeconds);
}

void App::Render() const
{
	g_game->Render();
}

void App::EndFrame()
{
	g_game->EndFrame();
	g_audioSystem->EndFrame();
	g_devConsole->EndFrame();
	g_eventSystem->EndFrame();
	g_inputSystem->EndFrame();
	g_renderer->EndFrame();
}

void App::LoadGameConfig(char const* gameConfigFilePath)
{
	XmlDocument gameConfigXml;
	XmlResult result = gameConfigXml.LoadFile(gameConfigFilePath);
	if (result == tinyxml2::XML_SUCCESS)
	{
		XmlElement* rootElement = gameConfigXml.RootElement();
		if (rootElement != nullptr)
		{
			g_gameConfigBlackboard.PopulateFromXmlElementAttributes(*rootElement);
		}

		else
		{
			DebuggerPrintf("WARNING: game config from file \"%s\" was invalid (missing root element)\n", gameConfigFilePath);
		}
	}

	else
	{
		DebuggerPrintf("WARNING: failed to load game config from file \"%s\"\n", gameConfigFilePath);
	}
}


bool App::QuitEvent(EventArgs& args)
{
	UNUSED(args);
	if (g_app != nullptr)
	{
		g_app->HandleQuitRequested();
	}

	return true;
}


//Input
//-----------------------------------------------------------------------------------------------
void App::HandleQuitRequested()
{
	m_isQuitting = true;
}

//Game Management
//-----------------------------------------------------------------------------------------------
void App::RestartGame()
{
	delete g_game;
	g_game = nullptr;

	g_game = new Game();
}
