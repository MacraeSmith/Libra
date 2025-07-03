#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Core/EventSystem.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
//#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/RendererDX11.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Core/Timer.hpp"
#include "Engine/Core/Clock.hpp"
#include <algorithm>


Rgba8 const DevConsole::ERROR = Rgba8(255, 0, 0, 200);
Rgba8 const DevConsole::WARNING = Rgba8(255, 190, 0, 255);
Rgba8 const DevConsole::INFO_MAJOR = Rgba8(255, 255, 255);
Rgba8 const DevConsole::INFO_MINOR = Rgba8(175, 175, 175);
Rgba8 const DevConsole::SELECTED_LINE = Rgba8(175,255,255);
Rgba8 const DevConsole::VALID_COMMAND = Rgba8(75, 175, 255);
Rgba8 const DevConsole::VALID_LINE = Rgba8(150, 255, 150);
Rgba8 const DevConsole::HELP_INFO = Rgba8(150, 175, 220);
Rgba8 const DevConsole::CONSOLE_BACKGROUND = Rgba8(0, 0, 0, 175);
Rgba8 const DevConsole::INPUT_LINE_BACKGROUND = Rgba8(0, 0, 0, 250);

DevConsole::DevConsole(DevConsoleConfig const& config)
	:m_config(config)
{
	m_config.m_numLinesOnScreen = m_config.m_defaultLinesOnScreen;
}

void DevConsole::Startup()
{
	SubscribeEventCallbackFunction("KeyPressed", DevConsole::Event_KeyPressed);
	SubscribeEventCallbackFunction("CharPressed", DevConsole::Event_CharInput);
	Strings clearArguments;
	clearArguments.push_back("Errors");
	clearArguments.push_back("Screen");
	clearArguments.push_back("Messages");
	clearArguments.push_back("EventCalls=");
	clearArguments.push_back("NumLines=");
	SubscribeEventCallbackFunction("Clear", clearArguments, DevConsole::Event_CommandClear);
	SubscribeEventCallbackFunction("Help", DevConsole::Event_CommandHelp);
	SubscribeEventCallbackFunction("DevConsoleControls", DevConsole::Event_CommandDevConsoleNavigation);

	Strings showSuggestionsArguments;
	showSuggestionsArguments.push_back("True");
	showSuggestionsArguments.push_back("False");
	SubscribeEventCallbackFunction("ShowSuggestions", showSuggestionsArguments, DevConsole::Event_CommandShowSuggestions);

	Strings resizeConsoleArguments;
	resizeConsoleArguments.push_back("NumLines=");
	resizeConsoleArguments.push_back("Scale=");
	resizeConsoleArguments.push_back("Full");
	resizeConsoleArguments.push_back("Quarter");
	SubscribeEventCallbackFunction("ResizeConsole", resizeConsoleArguments, DevConsole::Event_CommandResizeDevConsole);

	AddLine(INFO_MINOR, "Type 'Help' for list of commands", 1.f, true);
	m_insertionPointTimer = new Timer(0.5f, &Clock::GetSystemClock());
	m_insertionPointTimer->Start();
}

void DevConsole::ShutDown()
{

}

void DevConsole::BeginFrame()
{
	m_frameNumber++;
	while (m_insertionPointTimer->DecrementPeriodIfElapsed())
	{
		m_insertionPointVisible = !m_insertionPointVisible;
	}

	float deltaSeconds = Clock::GetSystemClock().GetDeltaSeconds();

	float mouseWheelDelta = g_inputSystem->GetWheelDelta();
	if (mouseWheelDelta != 0.f)
	{
		if (mouseWheelDelta > 0.f)
		{
			m_scrollPercentage = GetClamped(m_scrollPercentage + (5.f * deltaSeconds), 0.f, 1.f);
		}

		else if (mouseWheelDelta < 0.f)
		{
			m_scrollPercentage = GetClamped(m_scrollPercentage - (5.f * deltaSeconds), 0.f, 1.f);
		}

		//#TODO have selected line update based on mouse scroll
// 		const int NUM_LINES_RENDERED = GetClampedInt((int)ceilf(m_config.m_numLinesOnScreen), 0, (int)m_lines.size());
// 		const int NUM_LINES_BELOW_WINDOW = GetClampedInt((int)floorf((float)m_lines.size() * m_scrollPercentage), 0, (int)m_lines.size() - NUM_LINES_RENDERED);
// 		int bottomLineNum = GetClampedInt((int)(m_lines.size() - NUM_LINES_RENDERED - 1 - NUM_LINES_BELOW_WINDOW), -1, (int)m_lines.size() - 1);;
//  		m_selectedLineNum = bottomLineNum;// - 1;
// 		DebugAddMessage(Stringf("Selected Line: %i", m_selectedLineNum), 5.f);
// 		DebugAddMessage(Stringf("Bottom Line: %i", bottomLineNum), 5.f, Rgba8::RED, Rgba8::RED);
// 		if (m_selectedLineNum > bottomLineNum)
// 		{
// 		}

	}
	
}

void DevConsole::EndFrame()
{
	
}

void DevConsole::Render(AABB2 const& screenBounds, Renderer* overrideRenderer) const
{
	Renderer* renderer = overrideRenderer ? overrideRenderer : m_config.m_defaultRenderer;
	RendererDX11* rendererDX11 = dynamic_cast<RendererDX11*>(renderer);
	GUARANTEE_OR_DIE(rendererDX11, "trying to render dev console without DX11");

	BitmapFont* font = renderer->CreatOrGetBitMapFontFromFile(m_config.m_defaultFontName.c_str());
	if (font == nullptr)
 		return;

	if (rendererDX11)
	{
		switch (m_mode)
		{
		case HIDDEN:
			break;
		case FULL:
			rendererDX11->BeginRendererEvent("Draw - DevConsole");
			rendererDX11->SetBlendMode(BlendMode::ALPHA);
			rendererDX11->SetDepthMode(DepthMode::DISABLED);
			rendererDX11->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
			rendererDX11->SetSamplerMode(SamplerMode::POINT_CLAMP);
			Render_Open_DX11(screenBounds, *rendererDX11, *font, m_config.m_defaultFontAspect);
			rendererDX11->EndRendererEvent();
			break;
		}
	}

}

void DevConsole::Render(Camera* camera, Renderer* overrideRenderer) const
{
	Render(AABB2(camera->GetOrthoBottomLeft(), camera->GetOrthoTopRight()), overrideRenderer);
}

void DevConsole::Execute(std::string const& consoleCommandText)
{
	Strings linesToExecute = SplitStringOnDelimiter(consoleCommandText, '\n');
	
	for (int lineNum = 0; lineNum < (int)linesToExecute.size(); ++lineNum)
	{
		Strings words = SplitStringOnDelimiter(linesToExecute[lineNum], ' ', '\"');
		EventArgs args;

		//skip index 0 because words[0] is the name of the event, everything else is either key or value of argument
		for (int argNum = 1; argNum < (int)words.size(); ++argNum)
		{
			Strings keyValuePairs = SplitStringOnDelimiter(words[argNum], '=');

			//Avoid firing event on empty string
			if (keyValuePairs.size() < 1)
				continue;
			
			//Useful for cases like "True/False"
			if (keyValuePairs.size() <= 1)
			{
				args.SetValue(keyValuePairs[0], "", true);
				continue;
			}

			args.SetValue(keyValuePairs[0], keyValuePairs[1], true);
		}

		//If event was invalid, or fired but had no subscribers, message to dev console
		if (!FireEvent(words[0], args))
		{
			m_lines[(int)m_lines.size() - 1].m_color = WARNING;
			std::string failureMessage = "Unknown Command: ";
			Rgba8 failureColor = ERROR;

			if (g_eventSystem->IsValidEvent(words[0]))
			{
				failureMessage = "No subscribers to: ";
				failureColor = WARNING;
			}

			AddLine(failureColor, failureMessage.append(words[0]), 0.65f, true);
		}
	}
}

DevConsoleMode DevConsole::GetMode() const
{
	return m_mode;
}

void DevConsole::SetMode(DevConsoleMode mode)
{
	m_mode = mode;
}

void DevConsole::ToggleMode(DevConsoleMode mode)
{
	if (m_mode == mode)
	{
		m_mode = HIDDEN;
		return;
	}

	m_mode = mode;
	m_scrollPercentage = 0.f;
}

void DevConsole::AddLine(Rgba8 const& color, std::string const& text, float heightPercent, bool isMessage)
{
	DevConsoleLine newLine;
	newLine.m_color = color;
	newLine.m_text = text;
	newLine.m_timePrinted = GetCurrentTimeSeconds();
	newLine.m_frameNumberPrinted = m_frameNumber;
	newLine.m_textHeightScale = heightPercent;
	newLine.m_isMessage = isMessage;
	m_lines.push_back(newLine);
}

void DevConsole::Render_Open_DX11(AABB2 const& bounds, RendererDX11& renderer, BitmapFont& font, float fontAspect) const
{
	renderer.SetSamplerMode(SamplerMode::POINT_CLAMP);
	renderer.SetBlendMode(BlendMode::ALPHA);
	renderer.SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	renderer.SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	renderer.BindShader(nullptr);

	//Adjust bounds for window scale
	Vec2 maxs = bounds.m_maxs * m_consoleWindowScale;
	AABB2 adjustedBounds(bounds.m_mins, maxs);

	const int NUM_LINES_RENDERED = GetClampedInt((int)ceilf(m_config.m_numLinesOnScreen), 0, (int)m_lines.size());
	const float INPUT_LINE_HEIGHT = (bounds.m_maxs.y - bounds.m_mins.y) / m_config.m_defaultLinesOnScreen;
	const float LINE_HEIGHT = (adjustedBounds.m_maxs.y - (adjustedBounds.m_mins.y + INPUT_LINE_HEIGHT)) / GetClamped(m_config.m_numLinesOnScreen, m_config.m_defaultLinesOnScreen, 100.f);
	const int NUM_LINES_BELOW_WINDOW = GetClampedInt((int)floorf((float)m_lines.size() * m_scrollPercentage), 0, (int)m_lines.size() - NUM_LINES_RENDERED);

	if (m_config.m_numLinesOnScreen < m_config.m_defaultLinesOnScreen)
	{
		adjustedBounds.m_maxs.y = adjustedBounds.m_mins.y + ((m_config.m_numLinesOnScreen * LINE_HEIGHT) + INPUT_LINE_HEIGHT);
	}

	//Render background
	std::vector<Vertex_PCU> devConsoleVerts;
	AddVertsForAABB2D(devConsoleVerts, adjustedBounds, CONSOLE_BACKGROUND);

	//Render Mouse Wheel bar
	const float BAR_WIDTH = 0.015f * adjustedBounds.GetWidth();
	const float WINDOW_HEIGHT = adjustedBounds.GetHeight() - INPUT_LINE_HEIGHT;
	const float FRAC_LINES_SEEN = (float)NUM_LINES_RENDERED / (float)m_lines.size();
	const float BAR_FRAC_FROM_BOTTOM = (NUM_LINES_BELOW_WINDOW / (float)m_lines.size());// * WINDOW_HEIGHT;
	const float BAR_MINS_Y = adjustedBounds.m_mins.y + (BAR_FRAC_FROM_BOTTOM * WINDOW_HEIGHT) + INPUT_LINE_HEIGHT;
	const float BAR_HEIGHT = GetClamped(FRAC_LINES_SEEN * WINDOW_HEIGHT, 0.1f * WINDOW_HEIGHT, WINDOW_HEIGHT);
	const float BAR_MAXS_Y = BAR_MINS_Y + BAR_HEIGHT;
	AABB2 barBounds(adjustedBounds.m_maxs.x - BAR_WIDTH, BAR_MINS_Y, adjustedBounds.m_maxs.x - (0.005f * adjustedBounds.GetWidth()), BAR_MAXS_Y);
	AddVertsForAABB2D(devConsoleVerts, barBounds, Rgba8(125,125,125,125));

	//Render black bar under input text
	AABB2 inputLineBounds(adjustedBounds.m_mins.x, adjustedBounds.m_mins.y, adjustedBounds.m_maxs.x, adjustedBounds.m_mins.y + INPUT_LINE_HEIGHT);
	AddVertsForAABB2D(devConsoleVerts, inputLineBounds, INPUT_LINE_BACKGROUND);

	renderer.BindTexture(nullptr);
	renderer.DrawVertexArray(devConsoleVerts);

	devConsoleVerts.clear();

	//Render line history
	for (int lineNum = 0; lineNum < NUM_LINES_RENDERED; ++lineNum)
	{
		int lineIndexFromEnd = GetClampedInt((int)(m_lines.size() - lineNum - 1 - NUM_LINES_BELOW_WINDOW), -1, (int)m_lines.size() - 1);
		if (lineIndexFromEnd < 0)
			continue;

		DevConsoleLine currentLine = m_lines[lineIndexFromEnd]; // starts with last index and moves backwards
		std::string lineText = currentLine.m_text;

		float lineYPos = adjustedBounds.m_mins.y + (LINE_HEIGHT * lineNum) + INPUT_LINE_HEIGHT;
		AABB2 lineBounds(adjustedBounds.m_mins.x, lineYPos, adjustedBounds.m_maxs.x, lineYPos + LINE_HEIGHT);

		if (!m_isNavigatingHelpBox && m_selectedLineNum == lineIndexFromEnd)
		{
			//Render higlighted line
			font.AddVertsForTextInBox2D(devConsoleVerts, lineText, lineBounds, LINE_HEIGHT * currentLine.m_textHeightScale * DEFAULT_TEXT_HEIGHT, SELECTED_LINE, fontAspect, Vec2(0.f, 0.5f), SHRINK_TO_FIT);
			continue;
		}

		font.AddVertsForTextInBox2D(devConsoleVerts, lineText, lineBounds, LINE_HEIGHT * currentLine.m_textHeightScale * DEFAULT_TEXT_HEIGHT, currentLine.m_color, fontAspect, Vec2(0.f, 0.5f), SHRINK_TO_FIT);
	}

	//Render input line
	const float CURSOR_WIDTH = INPUT_LINE_HEIGHT * 0.1f;
	float adjustedCellHeight = INPUT_LINE_HEIGHT;
	if (m_inputLine.m_text.size() > 0)
	{
		inputLineBounds.m_maxs.x -= CURSOR_WIDTH; //clamps text length so that cursor is always visible
		adjustedCellHeight = font.AddVertsForTextInBox2D(devConsoleVerts, m_inputLine.m_text, inputLineBounds, INPUT_LINE_HEIGHT * DEFAULT_TEXT_HEIGHT, m_inputLine.m_color, fontAspect, Vec2(0.f, 0.5f), SHRINK_TO_FIT);
	}

	renderer.BindTexture(&font.GetTexture());
	renderer.DrawVertexArray(devConsoleVerts);
	devConsoleVerts.clear();

	//Adds shapes for insertionCursor
	if (m_insertionPointVisible)
	{
		float insertionCursorStart = GetClamped((float)m_insertionPointPosition * adjustedCellHeight * fontAspect, bounds.m_mins.x, bounds.m_maxs.x - CURSOR_WIDTH);
		float insertionCursorEnd = GetClamped(insertionCursorStart + CURSOR_WIDTH, bounds.m_mins.x + CURSOR_WIDTH, bounds.m_maxs.x);
		AABB2 insertionCursorBounds(insertionCursorStart, bounds.m_mins.y, insertionCursorEnd, bounds.m_mins.y + INPUT_LINE_HEIGHT);
		AddVertsForAABB2D(devConsoleVerts, insertionCursorBounds, INFO_MAJOR);
	}

	//Adds frame to devConsole to cover the half line overlap of top text
	if (m_consoleWindowScale < 1.f || m_config.m_numLinesOnScreen < m_config.m_defaultLinesOnScreen)
	{
		AddVertsForAABB2DFrame(devConsoleVerts, adjustedBounds, LINE_HEIGHT * 0.5f, Rgba8::GREY);
	}

	if (devConsoleVerts.size() > 0)
	{
		renderer.BindTexture(nullptr);
		renderer.DrawVertexArray(devConsoleVerts);
	}

	//render help box
	if (m_showSuggestions && m_helpBox.m_shouldRender)
	{
		const int NUM_LINES = (int)m_helpBox.m_text.size();
		const float TEXT_HEIGHT = DEFAULT_TEXT_HEIGHT;
		const float HELP_BOX_WIDTH = BitmapFont::GetTextWidth(INPUT_LINE_HEIGHT * TEXT_HEIGHT, m_helpBox.m_longestLine, fontAspect);

		float boxStartPos = GetClamped((float)m_helpBox.m_startIndex * adjustedCellHeight * fontAspect, bounds.m_mins.x, bounds.m_maxs.x - HELP_BOX_WIDTH);
		AABB2 subBox(boxStartPos, bounds.m_mins.y + INPUT_LINE_HEIGHT, boxStartPos + HELP_BOX_WIDTH, bounds.m_mins.y + (INPUT_LINE_HEIGHT * (float)NUM_LINES) + INPUT_LINE_HEIGHT);
		AddVertsForAABB2D(devConsoleVerts, subBox, INPUT_LINE_BACKGROUND);

		renderer.BindTexture(nullptr);
		renderer.DrawVertexArray(devConsoleVerts);
		devConsoleVerts.clear();

		for (int lineNum = 0; lineNum < NUM_LINES; ++lineNum)
		{
			float lineYPos = subBox.m_mins.y + (INPUT_LINE_HEIGHT * lineNum);
			AABB2 lineBounds(subBox.m_mins.x, lineYPos, subBox.m_maxs.x, lineYPos + INPUT_LINE_HEIGHT);

			if (m_isNavigatingHelpBox && lineNum == m_helpBoxSelectedLinePosition)
			{
				//Render highlighted help text
				font.AddVertsForTextInBox2D(devConsoleVerts, m_helpBox.m_text[lineNum], lineBounds, INPUT_LINE_HEIGHT * TEXT_HEIGHT, SELECTED_LINE, fontAspect, Vec2(0.f, 0.5f), SHRINK_TO_FIT);
				continue;
			}

			font.AddVertsForTextInBox2D(devConsoleVerts, m_helpBox.m_text[lineNum], lineBounds, INPUT_LINE_HEIGHT * TEXT_HEIGHT, INFO_MINOR, fontAspect, Vec2(0.f, 0.5f), SHRINK_TO_FIT);
		}
		
		renderer.BindTexture(&font.GetTexture());
		renderer.DrawVertexArray(devConsoleVerts);
		devConsoleVerts.clear();
	}
}


void DevConsole::EditInputText(unsigned char keyCode)
{
	if (keyCode == KEYCODE_BACKSPACE && m_inputLine.m_text.size() > 0 && m_insertionPointPosition > 0)
	{
		m_inputLine.m_text.erase(m_insertionPointPosition - 1, 1);
		m_insertionPointPosition--;
		m_insertionPointPosition = GetClampedInt(m_insertionPointPosition, 0, (int)m_inputLine.m_text.size());
		m_isNavigatingHelpBox = false;
	}

	else if (keyCode >= 32 && keyCode <= 126 && keyCode != '~' && keyCode != '`')
	{
		std::string stringToInsert(1, (char)keyCode);
		m_inputLine.m_text.insert(m_insertionPointPosition, stringToInsert);
		m_insertionPointPosition++;
		m_isNavigatingHelpBox = false;
	}

	UpdateInputLineColor();
	ResetInsertionPointBlink();
}

void DevConsole::DeleteFromInputText(unsigned char keyCode)
{
	if (keyCode == KEYCODE_DELETE && m_inputLine.m_text.size() > 0)
	{
		m_scrollPercentage = 0.f;
		m_inputLine.m_text.erase(m_insertionPointPosition, 1);
		m_insertionPointPosition = GetClampedInt(m_insertionPointPosition, 0, (int)m_inputLine.m_text.size());
		ResetInsertionPointBlink();
		UpdateInputLineColor();
	}
}

void DevConsole::MoveInsertionCursor(unsigned char keyCode)
{
	if (keyCode == KEYCODE_LEFTARROW)
	{
		m_insertionPointPosition--;
		m_insertionPointPosition = GetClampedInt(m_insertionPointPosition, 0, (int)m_inputLine.m_text.size());
	}

	if (keyCode == KEYCODE_RIGHTARROW)
	{
		m_insertionPointPosition++;
		m_insertionPointPosition = GetClampedInt(m_insertionPointPosition, 0, (int)m_inputLine.m_text.size());
	}

	if (keyCode == KEYCODE_END)
	{
		m_insertionPointPosition = (int)m_inputLine.m_text.size();
	}

	if (keyCode == KEYCODE_HOME)
	{
		m_insertionPointPosition = 0;
	}

	ResetInsertionPointBlink();
	UpdateInputLineColor();
}

void DevConsole::NavigateLines(unsigned char keyCode)
{
	if (keyCode == KEYCODE_TAB && m_helpBox.m_shouldRender && m_showSuggestions)
	{
		Strings inputStrings = SplitStringOnDelimiter(m_inputLine.m_text, ' ', false);
		if (!m_isNavigatingHelpBox)
		{
			m_isNavigatingHelpBox = true;
			m_helpBoxSelectedLinePosition = 0;
			std::string argument = m_helpBox.m_text[0];
			argument.pop_back();

			int lastWhiteSpace = GetIndexOfLastChar(m_inputLine.m_text, ' ');
			if (lastWhiteSpace == (int)m_inputLine.m_text.length() - 1)
			{
				argument.erase(0,1);
				m_inputLine.m_text += argument;
			}

			else
			{
				m_inputLine.m_text = inputStrings[0];
				const int NUM_STRINGS = (int)inputStrings.size();

				if (NUM_STRINGS > 2)
				{
					m_inputLine.m_text += " ";
				}

				for (int i = 1; i < NUM_STRINGS - 1; ++i)
				{
					m_inputLine.m_text += inputStrings[i];
					if (i < NUM_STRINGS - 2)
					{
						m_inputLine.m_text += " ";
					}
				}

				m_inputLine.m_text += argument;
			}

			
			m_insertionPointPosition = (int)m_inputLine.m_text.size();
			ResetInsertionPointBlink();
			return;
		}

		m_inputLine.m_text.clear();
		for (int i = 0; i < (int)inputStrings.size() - 1; ++i)
		{
			m_inputLine.m_text += inputStrings[i];
			if (i < (int)inputStrings.size() - 2)
			{
				m_inputLine.m_text += " ";
			}
		}

		m_isNavigatingHelpBox = false;
		m_insertionPointPosition = (int)m_inputLine.m_text.size();
		ResetInsertionPointBlink();
		return;
	}

	if (keyCode == KEYCODE_UPARROW)
	{
		if (m_helpBox.m_shouldRender && m_isNavigatingHelpBox)
		{
			m_helpBoxSelectedLinePosition++;
			if (m_helpBoxSelectedLinePosition >= (int)m_helpBox.m_text.size())
			{
				m_helpBoxSelectedLinePosition = 0;
			}

			Strings inputStrings = SplitStringOnDelimiter(m_inputLine.m_text, ' ');
			m_inputLine.m_text.clear();
			std::string argument = m_helpBox.m_text[m_helpBoxSelectedLinePosition];
			argument.pop_back();
			for (int i = 0; i < (int)inputStrings.size() - 1; ++i)
			{
				m_inputLine.m_text += inputStrings[i];
				if (i < (int)inputStrings.size() - 2)
				{
					m_inputLine.m_text += " ";
				}
			}

			m_inputLine.m_text = m_inputLine.m_text + argument;
			m_insertionPointPosition = (int)m_inputLine.m_text.size();
			ResetInsertionPointBlink();
			return;
		}

		const int NUM_LINES = (int)m_lines.size();
		if (NUM_LINES <= 0)
			return;
		int numMessageLines = 0;
		m_selectedLineNum--;

		//Skip over error messages
		for (int lineNum = 0; lineNum < (int)m_lines.size(); ++lineNum)
		{
			if (m_selectedLineNum < 0)
			{
				m_selectedLineNum = NUM_LINES - 1;
			}

			if (m_selectedLineNum >= NUM_LINES)
			{
				m_selectedLineNum = 0;
			}

			DevConsoleLine highlighedLine = m_lines[m_selectedLineNum];
			if (highlighedLine.m_isMessage)
			{
				m_selectedLineNum--;
				numMessageLines++;
				if (numMessageLines >= NUM_LINES) //returns from function if there are no valid lines to copy
				{
					m_selectedLineNum = -1;
					return;
				}
				continue;
			}

			break;
		}

		m_inputLine.m_text = m_lines[m_selectedLineNum].m_text;
		m_insertionPointPosition = (int)m_inputLine.m_text.size();
	}

	if (keyCode == KEYCODE_DOWNARROW)
	{
		if (m_helpBox.m_shouldRender && m_isNavigatingHelpBox)
		{
			m_helpBoxSelectedLinePosition--;
			if (m_helpBoxSelectedLinePosition < 0)
			{
				m_helpBoxSelectedLinePosition = (int)m_helpBox.m_text.size() - 1;
			}

			Strings inputStrings = SplitStringOnDelimiter(m_inputLine.m_text, ' ');
			m_inputLine.m_text.clear();
			std::string argument = m_helpBox.m_text[m_helpBoxSelectedLinePosition];
			argument.pop_back();
			for (int i = 0; i < (int)inputStrings.size() - 1; ++i)
			{
				m_inputLine.m_text += inputStrings[i];
				if (i < (int)inputStrings.size() - 2)
				{
					m_inputLine.m_text += " ";
				}
			}

			m_inputLine.m_text = m_inputLine.m_text + argument;
			m_insertionPointPosition = (int)m_inputLine.m_text.size();
			ResetInsertionPointBlink();
			return;
		}

		const int NUM_LINES = (int)m_lines.size();
		if (NUM_LINES <= 0)
			return;

		int numMessageLines = 0;
		m_selectedLineNum++;

		//Skip over error messages
		for (int lineNum = 0; lineNum < (int)m_lines.size(); ++lineNum)
		{
			if (m_selectedLineNum < 0)
			{
				m_selectedLineNum = NUM_LINES - 1;
			}
			if (m_selectedLineNum >= NUM_LINES)
			{
				m_selectedLineNum = 0;
			}

			DevConsoleLine highlighedLine = m_lines[m_selectedLineNum];
			if (highlighedLine.m_isMessage)
			{
				m_selectedLineNum++;
				numMessageLines++;
				if (numMessageLines >= NUM_LINES) //returns from function if there are no valid lines to copy
				{
					m_selectedLineNum = -1;
					return;
				}
				continue;
			}

			break;
		}

		m_inputLine.m_text = m_lines[m_selectedLineNum].m_text;
		m_insertionPointPosition = (int)m_inputLine.m_text.size();
	}


	ResetInsertionPointBlink();
	UpdateInputLineColor();
}

bool DevConsole::ExecuteInputLine()
{
	if (m_inputLine.m_text.size() > 0)
	{
		
		AddLine(VALID_COMMAND, m_inputLine.m_text);
		Execute(GetTextFromMostRecentLine());
		ClearInputLine();
		m_scrollPercentage = 0.f;
		return true;
	}
	
	ClearInputLine();
	return false;
}

void DevConsole::ClearCommands(EventArgs& args)
{
	bool completeClear = true;
	if (args.HasKey("Screen", true))
	{
		int numLinesOnScreen = (int)floorf(m_config.m_numLinesOnScreen);
		int totalLines = (int)m_lines.size() - 1;
		int indexOfFirstLineToDelete = GetClampedInt(totalLines - numLinesOnScreen, 0, totalLines);
		m_lines.erase(m_lines.begin() + GetClampedInt(indexOfFirstLineToDelete, 1, totalLines), m_lines.end() - 1);
		completeClear = false;

		//#TODO: handle when screen is different because of mouse scroll
	}

	if(args.HasKey("Messages", true))
	{
		completeClear = false;
		for (int lineNum = 1; lineNum < (int)m_lines.size(); ++lineNum)
		{
			if (m_lines[lineNum].m_isMessage)
			{
				m_lines.erase(m_lines.begin() + lineNum);
				lineNum--;
			}
		}
	}

	if (args.HasKey("Errors", true))
	{
		completeClear = false;
		for (int lineNum = 0; lineNum < (int)m_lines.size(); ++lineNum)
		{
			if ((!g_eventSystem->IsValidEvent(m_lines[lineNum].m_text) && !m_lines[lineNum].m_isMessage) || m_lines[lineNum].m_color == ERROR || m_lines[lineNum].m_color == WARNING)
			{
				m_lines.erase(m_lines.begin() + lineNum);
				lineNum--;
			}
		}
	}

	if (args.HasKey("EventCalls", true))
	{
		std::string eventName = args.GetValue("EventCalls", "Invalid", true);
		eventName = GetLowercase(eventName);
		if (eventName != "Invalid")
		{
			completeClear = false;
			int numLinesCleared = 0;
			for (int lineNum = 0; lineNum < (int)m_lines.size() - 1; ++lineNum)
			{
				DevConsoleLine currentLine = m_lines[lineNum];
				Strings splitText = SplitStringOnDelimiter(currentLine.m_text, ' ');

				if (currentLine.m_color == HELP_INFO)
					continue;

				if (splitText.size() > 0 && GetLowercase(splitText[0]) == eventName)
				{
					m_lines.erase(m_lines.begin() + lineNum);
					lineNum--;
					numLinesCleared++;
				}
			}

			if (!args.HasKey("HideClearMessage", true))
			{
				if (numLinesCleared < 1)
				{
					AddLine(WARNING, Stringf(" %i lines cleared - check spelling or capitalization", numLinesCleared), 0.75f, true);
					return;
				}

				AddLine(INFO_MINOR, Stringf(" %i lines cleared", numLinesCleared), 0.75f, true);
			}
		}
	}

	int numLines = args.GetValue("NumLines", -1, true);
	if (numLines >= 0)
	{
		completeClear = false;
		int totalLines = (int)m_lines.size() - 1;
		int indexOfFirstLineToDelete = GetClampedInt(totalLines - GetClampedInt(numLines, 0, totalLines), 1, totalLines);
		m_lines.erase(m_lines.begin() + indexOfFirstLineToDelete, m_lines.end() - 1);
	}

	else if (args.HasKey("NumLines", true))
	{
		completeClear = false;
		m_lines[(int)m_lines.size() - 1].m_color = WARNING;
		AddLine(ERROR, "Invalid Number", 0.75f, true);
	}

	else if (completeClear)
	{
		m_lines.erase(m_lines.begin() + 1, m_lines.end()); // does not clear first line which gives help instructions
	}

}

void DevConsole::HelpCommand()
{
	AddLine(INFO_MINOR, "---Registered Commands---", 1.f, true);
	std::vector<std::string> eventNames = g_eventSystem->GetAllRegisteredEventNames(true);
	std::sort(eventNames.begin(), eventNames.end()); //sorts event names alphabetically
	for (int eventNum = 0; eventNum < (int)eventNames.size(); ++eventNum)
	{
		std::string eventName = eventNames[eventNum];
		AddLine(HELP_INFO, eventName, 0.75f);
	}
	AddLine(INFO_MINOR, "-------------------------", 1.f, true);
}

void DevConsole::ClearInputLine()
{
	if (m_inputLine.m_text.size() <= 0)
	{
		SetMode(HIDDEN);
		return;
	}

	m_inputLine.m_text.clear();
	m_insertionPointPosition = 0;
	m_isNavigatingHelpBox = false;
	m_helpBox.m_shouldRender = false;
}

void DevConsole::ResizeConsoleWindow(EventArgs& args)
{
	if (args.HasKey("NumLines", true))
	{
		float numLines = GetClamped(args.GetValue("NumLines", m_config.m_defaultLinesOnScreen, true), 1.f, 100.f);
		numLines = floorf(numLines) + 0.5f;
		m_config.m_numLinesOnScreen = numLines;
		return;
	}

	if (args.HasKey("Scale", true))
	{
		float windowScale = args.GetValue("Scale", 1.f, true);
		if (windowScale < 0.25f)
		{
			AddLine(WARNING, "Warning: Too small value to scale console window", 0.75f, true);
			return;
		}

		m_consoleWindowScale = GetClamped(windowScale, 0.25f, 1.f);
		return;
	}

	if (args.HasKey("Quarter", true))
	{
		m_consoleWindowScale = 0.5f;
		m_config.m_numLinesOnScreen = m_config.m_defaultLinesOnScreen;
		return;
	}

	if (args.HasKey("Full", true))
	{
		m_consoleWindowScale = 1.f;
		m_config.m_numLinesOnScreen = m_config.m_defaultLinesOnScreen;
		return;
	}

	m_consoleWindowScale = 1.f;
	m_config.m_numLinesOnScreen = m_config.m_defaultLinesOnScreen;
	return;
}

void DevConsole::ResetInsertionPointBlink()
{
	m_insertionPointTimer->Start();
	m_insertionPointVisible = true;
}

void DevConsole::UpdateInputLineColor()
{
	Strings formattedtext = SplitStringOnDelimiter(m_inputLine.m_text.c_str(), ' ');

	if (formattedtext.size() > 0 && g_eventSystem->IsValidEvent(formattedtext[0]))
	{
		m_inputLine.m_color = VALID_LINE;
		FormatHelpBoxForEventName(formattedtext[0]);
	}

	else
	{
		m_inputLine.m_color = INFO_MINOR;
		m_helpBox.m_shouldRender = false;
	}
}

void DevConsole::SaveHelpTextForEventNames()
{
	Strings eventNames = g_eventSystem->GetAllRegisteredEventNames();

	for (int eventNum = 0; eventNum < (int)eventNames.size(); ++eventNum)
	{
		std::string eventName = eventNames[eventNum];
		Strings helpTexts;
		if (g_eventSystem->GetArgumentFormatsForEventName(eventName, helpTexts))
		{
			auto found = m_helpTextByEventName.find(eventName);
			if (found == m_helpTextByEventName.end())
			{
				m_helpTextByEventName.insert({ eventName, helpTexts });
				continue;
			}
			
			found->second = helpTexts;
		}
	}
}

void DevConsole::FormatHelpBoxForEventName(std::string const& eventName)
{
	auto found = m_helpTextByEventName.find(GetLowercase(eventName));
	if (found == m_helpTextByEventName.end())
	{
		m_helpBox.m_shouldRender = false;
		return;
	}

	m_helpBox.m_shouldRender = true;
	m_helpBox.m_startIndex = GetClampedInt(GetIndexOfLastChar(m_inputLine.m_text, ' '), (int)eventName.length(), 99999);
	m_helpBox.m_text = found->second;
	m_helpBox.m_longestLine.clear();
	for (int stringNum = 0; stringNum < (int)m_helpBox.m_text.size(); ++stringNum)
	{
		if (m_helpBox.m_text[stringNum].length() > m_helpBox.m_longestLine.length())
		{
			m_helpBox.m_longestLine = m_helpBox.m_text[stringNum];
		}
	}
}

void DevConsole::ShowSuggestions(bool show)
{
	m_showSuggestions = show;
	if (!m_showSuggestions)
	{
		m_helpBox.m_shouldRender = false;
		m_helpBoxSelectedLinePosition = 0;
		m_isNavigatingHelpBox = false;
	}
}

std::string DevConsole::GetTextFromMostRecentLine()
{
	int numLines = (int)(m_lines.size());
	if (numLines <= 0)
	{
		return "";
	}

	int lastIndex = numLines - 1;
	return m_lines[lastIndex].m_text;
}

bool DevConsole::Event_KeyPressed(EventArgs& args)
{
	if (g_devConsole == nullptr)
		return false;

	unsigned char keyCode = (unsigned char)args.GetValue("KeyCode", -1, true);
	if (keyCode == KEYCODE_TILDE)
	{
		g_devConsole->ToggleMode(FULL);
		g_devConsole->m_selectedLineNum = -1;

		if (g_devConsole->m_mode == FULL)
		{
			g_devConsole->SaveHelpTextForEventNames();
		}

		if (g_inputSystem && g_inputSystem->GetCursorMode() != CursorMode::POINTER)
		{
			g_inputSystem->SetCursorMode(CursorMode::POINTER);
		}

		return true;
	}

	if (g_devConsole->m_mode == HIDDEN)
		return false;

	if (keyCode == KEYCODE_LEFTARROW || keyCode == KEYCODE_RIGHTARROW || keyCode == KEYCODE_HOME || keyCode == KEYCODE_END)
	{
		g_devConsole->MoveInsertionCursor(keyCode);
		return true;
	}

	if (keyCode == KEYCODE_TAB || keyCode == KEYCODE_UPARROW || keyCode == KEYCODE_DOWNARROW)
	{
		g_devConsole->NavigateLines(keyCode);
		return true;
	}

	if (!g_devConsole->m_isNavigatingHelpBox)
	{
		g_devConsole->m_selectedLineNum = -1; //reset selected line
	}

	if (keyCode == KEYCODE_DELETE)
	{
		g_devConsole->DeleteFromInputText(keyCode);
		return true;
	}

	if (keyCode == KEYCODE_RETURN)
	{
		g_devConsole->ExecuteInputLine();
		return true;
	}

	if (keyCode == KEYCODE_ESC)
	{
		g_devConsole->ClearInputLine();
		return true;
	}

	return true;
}

bool DevConsole::Event_CharInput(EventArgs& args)
{
	if (g_devConsole == nullptr || g_devConsole->GetMode() == HIDDEN)
		return false;

	unsigned char keyCode = (unsigned char)args.GetValue("KeyCode", -1, true);

	g_devConsole->EditInputText(keyCode);

	if (keyCode != '\t' && !g_devConsole->m_isNavigatingHelpBox)
	{
		g_devConsole->m_selectedLineNum = -1;
	}

	return true;
}

bool DevConsole::Event_CommandClear(EventArgs& args)
{
	if (g_devConsole == nullptr || g_devConsole->GetMode() == HIDDEN)
		return false;

	g_devConsole->ClearCommands(args);
	return true;
}

bool DevConsole::Event_CommandHelp(EventArgs& args)
{
	UNUSED(args);
	if (g_devConsole == nullptr || g_devConsole->GetMode() == HIDDEN)
		return false;

	g_devConsole->HelpCommand();
	return true;
}

bool DevConsole::Event_CommandResizeDevConsole(EventArgs& args)
{
	if (g_devConsole == nullptr || g_devConsole->GetMode() == HIDDEN)
		return false;

	 g_devConsole->ResizeConsoleWindow(args);
	 return true;
}

bool DevConsole::Event_CommandDevConsoleNavigation(EventArgs& args)
{
	UNUSED(args);
	if (g_devConsole == nullptr || g_devConsole->GetMode() == HIDDEN)
		return false;

	g_devConsole->AddLine(DevConsole::INFO_MAJOR, "-------Dev Console Navigation-------", 1.f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "~                  Open/Close Console", .75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "ENTER              Execute Line", .75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "ARROW KEYS (U/D)   Copy Lines", .75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "ARROW KEYS (L/R)   Navigate Cursor", .75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "TAB                Select Suggestion", .75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "ESC                Clear Line", .75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MAJOR, "-----------------------------------", 1.f, true);
	return true;
}

bool DevConsole::Event_CommandShowSuggestions(EventArgs& args)
{
	if (g_devConsole == nullptr || g_devConsole->GetMode() == HIDDEN)
		return false;

	bool show = args.HasKey("True", true);
	bool hide = args.HasKey("False", true);

	if(show && !hide)
		g_devConsole->ShowSuggestions(true);
	
	else if (hide && !show)
		g_devConsole->ShowSuggestions(false);

	else 
		g_devConsole->ShowSuggestions(true);

	return true;
}





