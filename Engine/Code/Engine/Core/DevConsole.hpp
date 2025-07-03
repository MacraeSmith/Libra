#pragma once
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/EventSystem.hpp"
#include <vector>
#include <string>

#ifdef ERROR
#undef ERROR
#endif

class Renderer;
class RendererDX11;
struct AABB2;
class BitmapFont;
class Camera;
class Timer;

constexpr float DEFAULT_TEXT_HEIGHT = 0.75f;
constexpr float DEFAULT_LINES_ON_SCREEN = 29.5f;
enum DevConsoleMode : int
{
	HIDDEN,
	FULL,
	NUM_DEVCONSOLE_MODES,
};

struct DevConsoleConfig
{
	Renderer* m_defaultRenderer = nullptr;
	std::string m_defaultFontName;
	float m_defaultFontAspect = 1.f;
	float m_defaultLinesOnScreen = DEFAULT_LINES_ON_SCREEN;
	float m_numLinesOnScreen = m_defaultLinesOnScreen;
};

struct DevConsoleLine
{
	Rgba8 m_color = Rgba8::WHITE;
	std::string m_text;
	int m_frameNumberPrinted = 0;
	double m_timePrinted = 0.0;
	float m_textHeightScale = DEFAULT_TEXT_HEIGHT;
	bool m_isMessage = false; //flag to determine if line can be copied
};

struct HelpBox
{
	bool m_shouldRender = false;
	Strings m_text;
	std::string m_longestLine;
	int m_startIndex = 0;
};

struct DevConsoleInputLine
{
	Rgba8 m_color = Rgba8(175, 175, 175);
	std::string m_text;
	bool m_validCommand = false;
};

class DevConsole
{
public:
	explicit DevConsole(DevConsoleConfig const& config);
	~DevConsole() {}
	void Startup();
	void ShutDown();
	void BeginFrame();
	void EndFrame();
	void Render(AABB2 const& screenBounds, Renderer* overrideRenderer = nullptr) const;
	void Render(Camera* camera, Renderer* overrideRenderer = nullptr) const;

	void Execute(std::string const& consoleCommandText);
	void AddLine(Rgba8 const& color, std::string const& text, float heightScale = 1.f, bool isMessage = false);

	DevConsoleMode GetMode() const;
	void SetMode(DevConsoleMode mode);
	void ToggleMode(DevConsoleMode mode);

	std::string GetTextFromMostRecentLine();

	static bool Event_KeyPressed(EventArgs& args);
	static bool Event_CharInput(EventArgs& args);
	static bool Event_CommandClear(EventArgs& args);
	static bool Event_CommandHelp(EventArgs& args);
	static bool Event_CommandResizeDevConsole(EventArgs& args);
	static bool Event_CommandDevConsoleNavigation(EventArgs& args);
	static bool Event_CommandShowSuggestions(EventArgs& args);

public:
	static const Rgba8 ERROR;
	static const Rgba8 WARNING;
	static const Rgba8 INFO_MAJOR;
	static const Rgba8 INFO_MINOR;
	static const Rgba8 SELECTED_LINE;
	static const Rgba8 VALID_COMMAND;
	static const Rgba8 VALID_LINE;
	static const Rgba8 HELP_INFO;
	static const Rgba8 CONSOLE_BACKGROUND;
	static const Rgba8 INPUT_LINE_BACKGROUND;

protected:
	void Render_Open_DX11(AABB2 const& bounds, RendererDX11& renderer, BitmapFont& font, float fontAspect = 1.f) const;

	void EditInputText(unsigned char keyCode);
	void DeleteFromInputText(unsigned char keyCode);
	void MoveInsertionCursor(unsigned char keyCode);
	void NavigateLines(unsigned char keyCode);
	bool ExecuteInputLine();
	void ClearCommands(EventArgs& args);
	void HelpCommand();
	void ClearInputLine();
	void ResizeConsoleWindow(EventArgs& args);
	void ResetInsertionPointBlink();
	void UpdateInputLineColor();
	void SaveHelpTextForEventNames();
	void FormatHelpBoxForEventName(std::string const& eventName);
	void ShowSuggestions(bool show);


protected:
	DevConsoleConfig m_config;
	DevConsoleMode m_mode = HIDDEN;
	float m_consoleWindowScale = 1.f;
	std::vector<DevConsoleLine> m_lines;
	DevConsoleInputLine m_inputLine;
	int m_insertionPointPosition = 0;
	int m_selectedLineNum = -1;
	std::vector<std::string> m_commandHistory;

	bool m_insertionPointVisible = true;
	int m_frameNumber = 0;

	Timer* m_insertionPointTimer = nullptr;

	bool m_showSuggestions = true;
	bool m_isNavigatingHelpBox = false;
	HelpBox m_helpBox;
	int m_helpBoxSelectedLinePosition = -1;
	std::map<std::string, Strings> m_helpTextByEventName;
	float m_scrollPercentage = 0.f;
};
