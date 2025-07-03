#pragma once
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include <string>
#include <vector>

struct Vec2;

struct TileDefinition
{
	static std::vector<TileDefinition> s_tileDefinitions;

	std::string m_name = "UNAMED TILE TYPE";
	bool m_isSolid = false;
	bool m_isWater = false;
	AABB2 m_spriteSheetUVCoords = AABB2(0.f, 0.f, 1.f, 1.f);
	Rgba8 m_tintColor = Rgba8::WHITE;
	Rgba8 m_mapImageColor = Rgba8::BLACK;

public:
	explicit TileDefinition(XmlElement const& tileDefElement);
	~TileDefinition() {}


	static void InitTileDefinitions();

	static TileDefinition* GetTileDefinitionAdressFromName(char const* tileTypeName);
	static TileDefinition* GetTileDefinitionAdressFromRGBColor(Rgba8 const& mapColor);
};

