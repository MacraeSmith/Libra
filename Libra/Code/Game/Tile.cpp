#include "Game/Tile.hpp"

Tile::Tile(TileDefinition* tileDef, IntVec2 const& tileCoords)
	:m_tileDef(tileDef)
	,m_tileCoords(tileCoords)
{
}
