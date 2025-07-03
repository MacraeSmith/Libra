#pragma once
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include <vector>
#include <string>

class Image;

struct MapDefinition
{
	static std::vector<MapDefinition> s_mapDefinitions;

	std::string m_mapImageName = "";
	Image* m_mapImage = nullptr;
	IntVec2 m_mapImageOffset = IntVec2::ZERO;

	std::string m_name = "UNAMED MAP";
	IntVec2 m_dimensions;
	std::string m_fillTileType = "Grass";
	std::string m_borderTileType = "BrickWall";
	std::string m_wallFillTileType = "BrickWall";

	int m_numWormTypes = 0;

	std::string m_worm1TileType = "UNAMED WORM TYPE";
	int m_worm1Count = 0;
	int m_worm1MaxLength = 0;

	std::string m_worm2TileType = "UNAMED WORM TYPE";
	int m_worm2Count = 0;
	int m_worm2MaxLength = 0;

	std::string m_worm3TileType = "UNAMED WORM TYPE";
	int m_worm3Count = 0;
	int m_worm3MaxLength = 0;

	std::string m_worm4TileType = "UNAMED WORM TYPE";
	int m_worm4Count = 0;
	int m_worm4MaxLength = 0;

	std::string m_mapEntryTileType = "MapEntry";
	std::string m_mapExitTileType = "MapExit";
	std::string m_startBunkerWallTileType = "BunkerWall";
	std::string m_endBunkerWallTileType = "BunkerWall";
	std::string m_startBunkerFloorTileType = "BunkerFloor";
	std::string m_endBunkerFloorTileType = "BunkerFloor";

	int m_scorpioCount = 0;
	int m_leoCount = 0;
	int m_ariesCount = 0;
	int m_capricornCount = 0;
	int m_aquariusCount = 0;
	int m_geminiPairCount = 0;
	//#TODO add more npc types


public:
	explicit MapDefinition(XmlElement const& mapDefElement);
	~MapDefinition() {}

	static void InitMapDefinitions();

	static MapDefinition* GetMapDefinitionAdressFromName(char const* mapName);
	void GetMapWormInfo(std::vector<char const*>& wormNames, std::vector<int>& wormTypeCounts, std::vector<int>& wormTypeMaxLengths);

};

