#pragma once
#include "ThirdParty/TinyXML2/tinyxml2.h"
#include "Engine/Core/XmlUtils.hpp"
#include <map>
#include <string>

struct Rgba8;
struct Vec2;
struct IntVec2;

class NamedStrings
{
public:
	void PopulateFromXmlElementAttributes(XmlElement const& element);
	void		SetValue(std::string const& keyName, std::string const& newValue, bool defaultLowerCase = false);

	std::string GetValue(std::string const& keyName, std::string const& defaultValue, bool defaultLowerCase = false) const;
	bool		GetValue(std::string const& keyName, bool defaultValue, bool defaultLowerCase = false) const;
	int			GetValue(std::string const& keyName, int defaultValue, bool defaultLowerCase = false) const;
	float		GetValue(std::string const& keyName, float defaultValue, bool defaultLowerCase = false) const;
	std::string GetValue(std::string const& keyName, char const* defaultValue, bool defaultLowerCase = false) const;
	Rgba8		GetValue(std::string const& keyName, Rgba8 const& defaultValue, bool defaultLowerCase = false) const;
	Vec2		GetValue(std::string const& keyName, Vec2 const& defaultValue, bool defaultLowerCase = false) const;
	IntVec2		GetValue(std::string const& keyName, IntVec2 const& defaultValue, bool defaultLowerCase = false) const;
	EulerAngles GetValue(std::string const& keyName, EulerAngles const& defaultValue, bool defaultLowerCase = false) const;
	bool		HasKey(std::string const& keyName, bool defaultLowerCase = false) const;

private:
	std::map<std::string, std::string> m_keyValuePairs;
};

