#include "Engine/Core/NamedStrings.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include <sstream>

void NamedStrings::PopulateFromXmlElementAttributes(XmlElement const& element)
{
	XmlAttribute const* attribute = element.FirstAttribute();
	while (attribute)
	{
		SetValue(attribute->Name(), attribute->Value());
		attribute = attribute->Next();
	}
}

void NamedStrings::SetValue(std::string const& keyName, std::string const& newValue, bool defaultLowerCase)
{
	std::string keyNameAdjusted = keyName;
	if (defaultLowerCase)
	{
		keyNameAdjusted = GetLowercase(keyName);
	}

	auto found = m_keyValuePairs.find(keyNameAdjusted);
	if (found != m_keyValuePairs.end())
	{
		found->second = newValue;
		return;
	}

	m_keyValuePairs.insert({ keyNameAdjusted, newValue });
}


std::string NamedStrings::GetValue(std::string const& keyName, std::string const& defaultValue, bool defaultLowerCase) const
{
	std::string keyNameAdjusted = keyName;
	if (defaultLowerCase)
	{
		keyNameAdjusted = GetLowercase(keyName);
	}

	auto found = m_keyValuePairs.find(keyNameAdjusted);
	if (found == m_keyValuePairs.end())
	{
		DebuggerPrintf("WARNING: keyName \"%s\" was not found)\n", keyNameAdjusted.c_str());
		return defaultValue;
	}

	return found->second;
}

bool NamedStrings::GetValue(std::string const& keyName, bool defaultValue, bool defaultLowerCase) const
{
	std::string keyNameAdjusted = keyName;
	if (defaultLowerCase)
	{
		keyNameAdjusted = GetLowercase(keyName);
	}

	auto found = m_keyValuePairs.find(keyNameAdjusted);
	if (found == m_keyValuePairs.end())
	{
		DebuggerPrintf("WARNING: keyName \"%s\" was not found)\n", keyNameAdjusted.c_str());
		return defaultValue;
	}

	if (_stricmp(found->second.c_str(), "false") == 0)
	{
		return false;
	}

	if (_stricmp(found->second.c_str(), "true") == 0)
	{
		return true;
	}

	return defaultValue;
}

int NamedStrings::GetValue(std::string const& keyName, int defaultValue, bool defaultLowerCase) const
{
	std::string keyNameAdjusted = keyName;
	if (defaultLowerCase)
	{
		keyNameAdjusted = GetLowercase(keyName);
	}

	auto found = m_keyValuePairs.find(keyNameAdjusted);
	if (found == m_keyValuePairs.end())
	{
		DebuggerPrintf("WARNING: keyName \"%s\" was not found)\n", keyNameAdjusted.c_str());
		return defaultValue;
	}

	return atoi(found->second.c_str());
}

float NamedStrings::GetValue(std::string const& keyName, float defaultValue, bool defaultLowerCase) const
{
	std::string keyNameAdjusted = keyName;
	if (defaultLowerCase)
	{
		keyNameAdjusted = GetLowercase(keyName);
	}
	auto found = m_keyValuePairs.find(keyNameAdjusted);
	if (found == m_keyValuePairs.end())
	{
		DebuggerPrintf("WARNING: keyName \"%s\" was not found)\n", keyNameAdjusted.c_str());
		return defaultValue;
	}


	return (float)atof(found->second.c_str());
}

std::string NamedStrings::GetValue(std::string const& keyName, char const* defaultValue, bool defaultLowerCase) const
{
	std::string keyNameAdjusted = keyName;
	if (defaultLowerCase)
	{
		keyNameAdjusted = GetLowercase(keyName);
	}
	auto found = m_keyValuePairs.find(keyNameAdjusted);
	if (found == m_keyValuePairs.end())
	{
		DebuggerPrintf("WARNING: keyName \"%s\" was not found)\n", keyNameAdjusted.c_str());
		return defaultValue;
	}

	return found->second;
}

Rgba8 NamedStrings::GetValue(std::string const& keyName, Rgba8 const& defaultValue, bool defaultLowerCase) const
{
	std::string keyNameAdjusted = keyName;
	if (defaultLowerCase)
	{
		keyNameAdjusted = GetLowercase(keyName);
	}
	auto found = m_keyValuePairs.find(keyNameAdjusted);
	if (found == m_keyValuePairs.end())
	{
		DebuggerPrintf("WARNING: keyName \"%s\" was not found)\n", keyNameAdjusted.c_str());
		return defaultValue;
	}

	Rgba8 newColor;
	newColor.SetFromText(found->second.c_str());
	return newColor;
}

Vec2 NamedStrings::GetValue(std::string const& keyName, Vec2 const& defaultValue, bool defaultLowerCase) const
{
	std::string keyNameAdjusted = keyName;
	if (defaultLowerCase)
	{
		keyNameAdjusted = GetLowercase(keyName);
	}
	auto found = m_keyValuePairs.find(keyNameAdjusted);
	if (found == m_keyValuePairs.end())
	{
		DebuggerPrintf("WARNING: keyName \"%s\" was not found)\n", keyNameAdjusted.c_str());
		return defaultValue;
	}

	Vec2 newVec2;
	newVec2.SetFromText(found->second.c_str());
	return newVec2;
}

IntVec2 NamedStrings::GetValue(std::string const& keyName, IntVec2 const& defaultValue, bool defaultLowerCase) const
{
	std::string keyNameAdjusted = keyName;
	if (defaultLowerCase)
	{
		keyNameAdjusted = GetLowercase(keyName);
	}
	auto found = m_keyValuePairs.find(keyNameAdjusted);
	if (found == m_keyValuePairs.end())
	{
		DebuggerPrintf("WARNING: keyName \"%s\" was not found)\n", keyNameAdjusted.c_str());
		return defaultValue;
	}

	IntVec2 newIntVec2;
	newIntVec2.SetFromText(found->second.c_str());
	return newIntVec2;
}

EulerAngles NamedStrings::GetValue(std::string const& keyName, EulerAngles const& defaultValue, bool defaultLowerCase) const
{
	std::string keyNameAdjusted = keyName;
	if (defaultLowerCase)
	{
		keyNameAdjusted = GetLowercase(keyName);
	}
	auto found = m_keyValuePairs.find(keyNameAdjusted);
	if (found == m_keyValuePairs.end())
	{
		DebuggerPrintf("WARNING: keyName \"%s\" was not found)\n", keyNameAdjusted.c_str());
		return defaultValue;
	}

	Strings angleStrings = SplitStringOnDelimiter(found->second, ',');
	if(angleStrings.size() < 3)
		return defaultValue;

	EulerAngles newEulerAngles;
	newEulerAngles.SetFromText(found->second.c_str());
	return newEulerAngles;
}

bool NamedStrings::HasKey(std::string const& keyName, bool defaultLowerCase) const
{
	std::string keyNameAdjusted = keyName;
	if (defaultLowerCase)
	{
		keyNameAdjusted = GetLowercase(keyName);
	}

	auto found = m_keyValuePairs.find(keyNameAdjusted);
	if (found == m_keyValuePairs.end())
	{
		return false;
	}
	return true;
}

