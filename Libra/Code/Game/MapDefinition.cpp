#include "Game/MapDefinition.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Image.hpp"
#include "ThirdParty/TinyXML2/tinyxml2.h"

std::vector<MapDefinition> MapDefinition::s_mapDefinitions;

MapDefinition::MapDefinition(XmlElement const& mapDefElement)
{
	m_name = ParseXmlAttribute(mapDefElement, "name", m_name);

	m_mapImageName = ParseXmlAttribute(mapDefElement, "imageFilePath", ""); // default to blank
	if (m_mapImageName != "")
	{
		m_mapImage = new Image(m_mapImageName.c_str());
		m_dimensions = m_mapImage->GetDimensions();
		m_mapImageOffset = ParseXmlAttribute(mapDefElement, "mapImageOffset", IntVec2::ZERO);
	}

	else
	{
		m_dimensions = ParseXmlAttribute(mapDefElement, "dimensions", IntVec2(25, 25));
	}

	m_fillTileType = ParseXmlAttribute(mapDefElement, "fillTileType", m_fillTileType);
	m_borderTileType = ParseXmlAttribute(mapDefElement, "edgeTileType", m_borderTileType);
	m_wallFillTileType = ParseXmlAttribute(mapDefElement, "wallFillTileType", m_borderTileType); //defaults to same type as the map border

	m_numWormTypes = ParseXmlAttribute(mapDefElement, "numWormsTypes", 0); // default to zero
	m_worm1TileType = ParseXmlAttribute(mapDefElement, "worm1TileType", m_worm1TileType);
	m_worm2TileType = ParseXmlAttribute(mapDefElement, "worm2TileType", m_worm2TileType);
	m_worm3TileType = ParseXmlAttribute(mapDefElement, "worm3TileType", m_worm3TileType);
	m_worm4TileType = ParseXmlAttribute(mapDefElement, "worm4TileType", m_worm4TileType);

	m_worm1Count = ParseXmlAttribute(mapDefElement, "worm1Count", m_worm1Count);
	m_worm2Count = ParseXmlAttribute(mapDefElement, "worm2Count", m_worm2Count);
	m_worm3Count = ParseXmlAttribute(mapDefElement, "worm3Count", m_worm3Count);
	m_worm4Count = ParseXmlAttribute(mapDefElement, "worm4Count", m_worm4Count);

	m_worm1MaxLength = ParseXmlAttribute(mapDefElement, "worm1MaxLength", m_worm1MaxLength);
	m_worm2MaxLength = ParseXmlAttribute(mapDefElement, "worm2MaxLength", m_worm2MaxLength);
	m_worm3MaxLength = ParseXmlAttribute(mapDefElement, "worm3MaxLength", m_worm3MaxLength);
	m_worm4MaxLength = ParseXmlAttribute(mapDefElement, "worm4MaxLength", m_worm4MaxLength);

	m_mapEntryTileType = ParseXmlAttribute(mapDefElement, "mapEntryTileType", m_mapEntryTileType);
	m_mapExitTileType = ParseXmlAttribute(mapDefElement, "mapExitTileType", m_mapExitTileType);
	m_startBunkerWallTileType = ParseXmlAttribute(mapDefElement, "startBunkerWallTileType", m_startBunkerWallTileType);
	m_endBunkerWallTileType = ParseXmlAttribute(mapDefElement, "endBunkerWallTileType", m_startBunkerWallTileType);
	m_startBunkerFloorTileType = ParseXmlAttribute(mapDefElement, "startBunkerFloorTileType", m_startBunkerFloorTileType);
	m_endBunkerFloorTileType = ParseXmlAttribute(mapDefElement, "endBunkerFloorTileType", m_startBunkerFloorTileType);

	m_scorpioCount = ParseXmlAttribute(mapDefElement, "scorpioCount", m_scorpioCount);
	m_leoCount = ParseXmlAttribute(mapDefElement, "leoCount", m_leoCount);
	m_ariesCount = ParseXmlAttribute(mapDefElement, "ariesCount", m_ariesCount);
	m_capricornCount = ParseXmlAttribute(mapDefElement, "capricornCount", m_capricornCount);
	m_aquariusCount = ParseXmlAttribute(mapDefElement, "aquariusCount", m_aquariusCount);
	m_geminiPairCount = ParseXmlAttribute(mapDefElement, "geminiPairCount", m_geminiPairCount);
	//#TODO: add more npc types
}

void MapDefinition::InitMapDefinitions()
{
	XmlDocument mapDefXml;
	char const* filePath = ("Data/Definitions/MapDefinitions.xml");
	XmlResult result = mapDefXml.LoadFile(filePath);
	GUARANTEE_OR_DIE(result == tinyxml2::XML_SUCCESS, Stringf("Failed to open required map defs file \"%s\"", filePath));

	XmlElement* rootElement = mapDefXml.RootElement();
	GUARANTEE_OR_DIE(rootElement, "failed to find root element of Map Defs xml");

	XmlElement* mapDefElement = rootElement->FirstChildElement();
	while (mapDefElement != nullptr)
	{
		std::string elementName = mapDefElement->Name();
		GUARANTEE_OR_DIE(elementName == "MapDefinition", Stringf("Root child element in %s was <%s>, must be <MapDefinition>!", filePath, elementName.c_str()));
		MapDefinition* newMapDef = new MapDefinition(*mapDefElement);
		s_mapDefinitions.push_back(*newMapDef);
		mapDefElement = mapDefElement->NextSiblingElement();
	}
}

MapDefinition* MapDefinition::GetMapDefinitionAdressFromName(char const* mapName)
{
	for (int mapDefNum = 0; mapDefNum < static_cast<int>(s_mapDefinitions.size()); ++mapDefNum)
	{
		if (strcmp(s_mapDefinitions[mapDefNum].m_name.c_str(), mapName) == 0) //if m_name == mapName
		{
			return &s_mapDefinitions[mapDefNum];
		}
	}

	MapDefinition* failedDefinition = nullptr;
	GUARANTEE_OR_DIE(failedDefinition != nullptr, Stringf("Failed to find MapDefinition with name: \"%s\"", mapName));
	return failedDefinition;
}

void MapDefinition::GetMapWormInfo(std::vector<char const*>& wormNames, std::vector<int>& wormTypeCounts, std::vector<int>& wormTypeMaxLengths)
{
	wormNames.push_back(m_worm1TileType.c_str());
	wormNames.push_back(m_worm2TileType.c_str());
	wormNames.push_back(m_worm3TileType.c_str());
	wormNames.push_back(m_worm4TileType.c_str());

	wormTypeCounts.push_back(m_worm1Count);
	wormTypeCounts.push_back(m_worm2Count);
	wormTypeCounts.push_back(m_worm3Count);
	wormTypeCounts.push_back(m_worm4Count);

	wormTypeMaxLengths.push_back(m_worm1MaxLength);
	wormTypeMaxLengths.push_back(m_worm2MaxLength);
	wormTypeMaxLengths.push_back(m_worm3MaxLength);
	wormTypeMaxLengths.push_back(m_worm4MaxLength);
}
