#pragma  once
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Math/IntVec2.hpp"
#include <string>


class InputSystem;

struct WindowConfig
{
	float m_aspectRatio = (16.f / 9.f);
	InputSystem* m_inputSystem = nullptr;
	std::string m_windowTitle = "Unnamed SD Application";
};

class Window
{
public:
	Window(WindowConfig const& config);
	~Window() {};

	//App Flow
	void Startup();
	void BeginFrame();
	void EndFrame();
	void Shutdown();

	WindowConfig const& GetConfig() const;
	void* const& GetDisplayContext() const;

	Vec2 GetNormalizedMouseUV() const;
	void* GetHwnd() const;
	IntVec2 GetClientDimensions() const;
	bool IsWindowActive() const;

private:
	void RunMessagePump();
	void CreateOSWindow();

public:
	static Window*	s_mainWindow;

private:
	WindowConfig	m_config;
	void* m_displayContext = nullptr; //Actually Windows HDC on Windows platform
	void* m_windowHandle = nullptr; //Actually Windows HWND on Windows platform
	IntVec2 m_clientDimensions;


	

};