#include "Game/TileDefinition.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Renderer/RendererDX11.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/SpriteSheet.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/Vec4.hpp"
#include "ThirdParty/TinyXML2/tinyxml2.h"

std::vector<TileDefinition> TileDefinition::s_tileDefinitions;

TileDefinition::TileDefinition(XmlElement const& tileDefElement)
{
	m_name = ParseXmlAttribute(tileDefElement, "name", m_name);
	m_tintColor = ParseXmlAttribute(tileDefElement, "tint", m_tintColor);
	m_isSolid = ParseXmlAttribute(tileDefElement, "isSolid", m_isSolid);
	m_isWater = ParseXmlAttribute(tileDefElement, "isWater", false);
	m_mapImageColor = ParseXmlAttribute(tileDefElement, "mapColor", Rgba8::BLACK);
	
	IntVec2 spriteCoords = ParseXmlAttribute(tileDefElement, "spriteCoords", IntVec2(-1, -1));
	int spriteIndex = spriteCoords.x + (spriteCoords.y * TERRAIN_SPRITES_WIDTH);
	m_spriteSheetUVCoords = g_terrainSprites->GetSpriteUVs(spriteIndex);
}


void TileDefinition::InitTileDefinitions()
{
	XmlDocument tileDefXml;
	char const* filePath = ("Data/Definitions/TileDefinitions.xml");
	XmlResult result = tileDefXml.LoadFile(filePath);
	GUARANTEE_OR_DIE(result == tinyxml2::XML_SUCCESS, Stringf("Failed to open required tile defs file \"%s\"", filePath));

	XmlElement* rootElement = tileDefXml.RootElement();
	GUARANTEE_OR_DIE(rootElement, "failed to find root element of Tile Defs xml");

	XmlElement* tileDefElement = rootElement->FirstChildElement();
	while (tileDefElement != nullptr)
	{
		std::string elementName = tileDefElement->Name();
		GUARANTEE_OR_DIE(elementName == "TileDefinition", Stringf("Root child element in %s was <%s>, must be <TileDefinition>!", filePath, elementName.c_str()));
		TileDefinition* newTileDef = new TileDefinition(*tileDefElement);
		s_tileDefinitions.push_back(*newTileDef);
		tileDefElement = tileDefElement->NextSiblingElement();
	}

}

TileDefinition* TileDefinition::GetTileDefinitionAdressFromName(char const* tileTypeName)
{
	for (int tileDefNum = 0; tileDefNum < static_cast<int>(s_tileDefinitions.size()); ++tileDefNum)
	{
		if (strcmp(s_tileDefinitions[tileDefNum].m_name.c_str(), tileTypeName) == 0) //if m_name == tileTypeName
		{
			return &s_tileDefinitions[tileDefNum];
		}
	}

	TileDefinition* failedDefinition = nullptr;
	GUARANTEE_OR_DIE(failedDefinition != nullptr, Stringf("Failed to find tileDefinition with name: \"%s\"", tileTypeName));
	return failedDefinition;
}

TileDefinition* TileDefinition::GetTileDefinitionAdressFromRGBColor(Rgba8 const& mapColor)
{
	for (int tileDefNum = 0; tileDefNum < static_cast<int>(s_tileDefinitions.size()); ++tileDefNum)
	{
		if (s_tileDefinitions[tileDefNum].m_mapImageColor.IsEqualIgnoringAlpha(mapColor))
		{
			return &s_tileDefinitions[tileDefNum];
		}
	}

	return nullptr;
}
