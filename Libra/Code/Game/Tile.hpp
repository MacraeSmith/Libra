#pragma once
#include "Engine/Math/IntVec2.hpp"
#include "Game/TileDefinition.hpp"

struct TileDefinition;

struct Tile
{
public:
	explicit Tile(TileDefinition* tileDef, IntVec2 const& tileCoords);

	IntVec2 m_tileCoords = IntVec2::ZERO;
	TileDefinition const* m_tileDef;
};

