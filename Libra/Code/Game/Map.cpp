#include "Game/Map.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Game.hpp"
#include "Game/TileDefinition.hpp"
#include "Game/MapDefinition.hpp"
#include "Game/Tile.hpp"
#include "Game/Entity.hpp"
#include "Game/Player.hpp"
#include "Game/Scorpio.hpp"
#include "Game/Leo.hpp"
#include "Game/Aries.hpp"
#include "Game/Capricorn.hpp"
#include "Game/Aquarius.hpp"
#include "Game/Gemini.hpp"
#include "Game/Bullet.hpp"
#include "Game/Explosion.hpp"

#include "Engine/Renderer/Texture.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/RendererDX11.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Renderer/SimpleTriangleFont.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/SpriteSheet.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Core/TileHeatMap.hpp"
#include "Engine/Math/FloatRange.hpp"
#include "Engine/Core/Image.hpp"
#include <queue>
#include <string>
#include <algorithm>

bool g_noClipMode = false;


Map::Map(Game* const& game, int mapIndex, MapDefinition* const m_mapDefinition)
	:m_game(game)
	,m_mapIndex(mapIndex)
	,m_dimensions(m_mapDefinition->m_dimensions)
	,m_mapDefinition(m_mapDefinition)
{
	Texture* terrainTexture = g_renderer->CreateOrGetTextureFromFile(g_gameConfigBlackboard.GetValue("terrainSpriteSheetTexture","").c_str());
	m_terrainSpriteSheet = new SpriteSheet(*terrainTexture, g_gameConfigBlackboard.GetValue("terrainSpriteSheetDimensions", IntVec2(8, 8)));

	Texture* explosionTexture = g_renderer->CreateOrGetTextureFromFile(g_gameConfigBlackboard.GetValue("explosionSpriteSheetTexture", "").c_str());
	m_explosionSpriteSheet = new SpriteSheet(*explosionTexture, g_gameConfigBlackboard.GetValue("explosionSpriteSheetDimensions", IntVec2(5, 5)));

	m_startToEndDistanceMap = new TileHeatMap(m_dimensions);
	m_debugHeatMaps[0] = m_startToEndDistanceMap;
	SpawnTiles();
}

Map::~Map()
{
	m_tiles.clear();

	for (int entityNum = 0; entityNum < static_cast<int>(m_allEntities.size()); ++entityNum)
	{
		Entity* entity = m_allEntities[entityNum];
		RemoveEntityFromMap(entity);
		delete entity;
	}

	m_entityListByType->clear();
	m_allEntities.clear();

	//Not deleting last map because that belongs to leo which has already been deleted
	for (int heatMapNum = 0; heatMapNum < NUM_DEBUG_HEAT_MAPS - 1; ++heatMapNum)
	{
		delete(m_debugHeatMaps[heatMapNum]);
		m_debugHeatMaps[heatMapNum] = nullptr;
	}
	
	m_startToEndDistanceMap = nullptr;
	m_solidTileMap = nullptr;
	m_amphibianSolidMap = nullptr;
	m_distanceMapToPlayer = nullptr;
	m_amphibianDistanceMapToPlayer = nullptr;

}

void Map::Update(float deltaSeconds)
{
	UpdateAndCheckOverrideTilesAge(deltaSeconds);
	UpdateEntities(deltaSeconds);

	if (g_inputSystem->WasKeyJustPressed(KEYCODE_F6))
	{
		RotateThroughDebugHeatMaps();
	}

	if (g_inputSystem->WasKeyJustPressed(KEYCODE_F7))
	{
		m_renderDebugTileCoords = !m_renderDebugTileCoords;
	}

	if (g_inputSystem->WasKeyJustPressed(KEYCODE_F3))
	{
		g_noClipMode = !g_noClipMode;
	}
	
	UpdateGameCameraToFollowPlayer();
}

void Map::Render() const
{
	RenderTiles();
	RenderDebugHeatMap();
	RenderEntities();
	RenderEntityHealthBars();
	RenderPauseOverlay();
	RenderDebugTileInfo();
}

void Map::EndFrame()
{
	DeleteGarbageEntities();
}


//Spawn Functions
//-----------------------------------------------------------------------------------------------

void Map::SpawnTiles()
{
	GUARANTEE_OR_DIE(m_mapDefinition != nullptr, Stringf("Map Definition was a null pointer for Map Num:  \"%s\"", m_mapIndex));

	m_tiles.clear();

	TileDefinition* tileDef = nullptr;
	TileDefinition* borderTileDef = TileDefinition::GetTileDefinitionAdressFromName(m_mapDefinition->m_borderTileType.c_str());
	TileDefinition* fillTileDef = TileDefinition::GetTileDefinitionAdressFromName(m_mapDefinition->m_fillTileType.c_str());
	m_tiles.reserve(m_dimensions.x * m_dimensions.y);

	for (int tileRowIndex = 0; tileRowIndex < m_dimensions.y; ++tileRowIndex)
	{
		for (int tileColumnIndex = 0; tileColumnIndex < m_dimensions.x; ++tileColumnIndex)
		{

			//Borders
			if (tileColumnIndex == 0 || tileColumnIndex == m_dimensions.x - 1)
			{
				tileDef = borderTileDef;
			}

			else if (tileRowIndex == 0 || tileRowIndex == m_dimensions.y - 1)
			{
				tileDef = borderTileDef;
			}

			//Interior blocks
			else
			{
				tileDef = fillTileDef;
			}

			Tile newTile(tileDef, IntVec2(tileColumnIndex, tileRowIndex));

			m_tiles.push_back(newTile);
		}
	}

	SpawnWorms();

	if (m_mapDefinition->m_mapImage != nullptr)
	{
		SpawnTilesFromImage();
		return;
	}

	SpawnBunkerAreas();
	InitHeatMaps();
}

void Map::SpawnTilesFromImage()
{
	
	Image* mapImage = m_mapDefinition->m_mapImage;
	TileDefinition* tileDef;
	Rgba8 tileColor;
	IntVec2 tileCoords;

	char const* mapEntryName = m_mapDefinition->m_mapEntryTileType.c_str();
	char const* mapExitName = m_mapDefinition->m_mapExitTileType.c_str();
	char const* tileDefName;
	for (int tileRowIndex = 0; tileRowIndex < m_dimensions.y; ++tileRowIndex)
	{
		for (int tileColumnIndex = 0; tileColumnIndex < m_dimensions.x; ++tileColumnIndex)
		{
			tileCoords.x = tileColumnIndex;
			tileCoords.y = tileRowIndex;
			tileColor = mapImage->GetTexelColor(tileCoords);
			unsigned char alphaByte = tileColor.a;
			if (alphaByte == 0) //ignore tile if it has zero alpha
				continue;

			float randomAlphaRoll = g_rng->RollRandomFloatInRange(0, 254);
			if (randomAlphaRoll > static_cast<float>(alphaByte)) //if alpha is not 255 roll to randomly choose whether to change or not, higher alpha values have better chance of changing
				continue;

			tileDef = TileDefinition::GetTileDefinitionAdressFromRGBColor(tileColor);
			if (tileDef == nullptr)
				continue;

			tileDefName = tileDef->m_name.c_str();

			if (!_stricmp(mapEntryName, tileDefName))
			{
				m_startCoord = tileCoords;
			}

			if (!_stricmp(mapExitName, tileDefName))
			{
				m_endCoord = tileCoords;
			}

			m_tiles[GetTileIndexFromTileCoords(tileCoords)].m_tileDef = tileDef;
		}
	}

	//Create tile heat maps for map
	PopulateDistanceMap(*m_startToEndDistanceMap, m_startCoord, DEFAULT_HEAT_MAP_SOLID_VALUE);
	int endTileIndex = GetTileIndexFromTileCoords(m_endCoord);
	{
		if (m_startToEndDistanceMap->GetValue(endTileIndex) >= DEFAULT_HEAT_MAP_SOLID_VALUE)
		{
			m_startToEndDistanceMap->SetAllValues(DEFAULT_HEAT_MAP_SOLID_VALUE);
			m_numSpawnAttempts++;
			if (m_numSpawnAttempts >= 100)
			{
				ERROR_AND_DIE("Map failed 100 attemps to generate a solveable map from the selected image");
			}
			SpawnTilesFromImage();
			return;
		}
	}

	m_solidTileMap = new TileHeatMap(m_dimensions, DEFAULT_HEAT_MAP_SOLID_VALUE);
	m_debugHeatMaps[1] = m_solidTileMap;
	m_amphibianSolidMap = new TileHeatMap(m_dimensions, DEFAULT_HEAT_MAP_SOLID_VALUE);
	m_debugHeatMaps[2] = m_amphibianSolidMap;

	for (int tileIndex = 0; tileIndex < static_cast<int>(m_tiles.size()); ++tileIndex)
	{
		//Heat map for land based entities
		if (IsTileTraversable(tileIndex))
		{
			m_solidTileMap->SetValue(tileIndex, 0.f);
		}

		//Heat map for amphibians
		if (IsTileTraversable(tileIndex, false))
		{
			m_amphibianSolidMap->SetValue(tileIndex, 0.f);
		}
	}
}

void Map::SpawnWorms()
{
	IntVec2 directions[4] = { IntVec2::NORTH, IntVec2::EAST, IntVec2::SOUTH, IntVec2::WEST };
	IntVec2 currentCoords;
	int tileIndex;

	std::vector<char const*> wormTypeNames;
	std::vector<int> wormTypeCount;
	std::vector<int> wormTypeMaxLengths;

	m_mapDefinition->GetMapWormInfo(wormTypeNames, wormTypeCount, wormTypeMaxLengths);

	int numWormTypes = m_mapDefinition->m_numWormTypes;
	numWormTypes = GetClampedInt(numWormTypes, 0, MAX_NUM_WORM_TYPES);

	for (int wormTypeIndex = 0; wormTypeIndex < numWormTypes; ++wormTypeIndex)
	{
		TileDefinition* tileDef = TileDefinition::GetTileDefinitionAdressFromName(wormTypeNames[wormTypeIndex]);
		for (int wormNum = 0; wormNum < wormTypeCount[wormTypeIndex]; ++wormNum)
		{
			currentCoords.x = g_rng->RollRandomIntInRange(1, m_dimensions.x - 1);
			currentCoords.y = g_rng->RollRandomIntInRange(1, m_dimensions.y - 1);

			for (int tileNum = 0; tileNum < wormTypeMaxLengths[wormTypeIndex]; ++tileNum)
			{
				if (IsTileWithinBorderWalls(currentCoords))
				{
					tileIndex = GetTileIndexFromTileCoords(currentCoords);
					m_tiles[tileIndex].m_tileDef = tileDef;
				}

				currentCoords += directions[g_rng->RollRandomIntInRange(0, 3)];
			}
		}
	}

}

void Map::SpawnBunkerAreas()
{
	//Home Bunker
	const int HOME_BUNKER_WIDTH = g_gameConfigBlackboard.GetValue("startAreaSize", 5); // 5 until wall plus perimeter of grass
	const int HOME_BUNKER_HEIGHT = HOME_BUNKER_WIDTH;

	TileDefinition const* mapEntryTileDef = TileDefinition::GetTileDefinitionAdressFromName(m_mapDefinition->m_mapEntryTileType.c_str());
	TileDefinition const* startBunkerWallTileDef = TileDefinition::GetTileDefinitionAdressFromName(m_mapDefinition->m_startBunkerWallTileType.c_str());
	TileDefinition const* startBunkerFloorTileDef = TileDefinition::GetTileDefinitionAdressFromName(m_mapDefinition->m_startBunkerFloorTileType.c_str());

	//Create home grass area with bunker walls
	for (int tileColumnIndex = 1; tileColumnIndex <= HOME_BUNKER_HEIGHT; ++tileColumnIndex)
	{
		for (int tileRowIndex = 1; tileRowIndex <= HOME_BUNKER_WIDTH; ++tileRowIndex)
		{
			int tileIndex = GetTileIndexFromTileCoords(IntVec2(tileRowIndex, tileColumnIndex));

			//set walls one row / column inside home area
			if ((tileColumnIndex > 1 && tileColumnIndex < HOME_BUNKER_WIDTH)
				&& (tileRowIndex > HOME_BUNKER_HEIGHT - 2 && tileRowIndex < HOME_BUNKER_HEIGHT))
			{
				m_tiles[tileIndex].m_tileDef = startBunkerWallTileDef;
			}

			else if ((tileRowIndex > 1 && tileRowIndex < HOME_BUNKER_HEIGHT)
				&& (tileColumnIndex > HOME_BUNKER_WIDTH - 2 && tileColumnIndex < HOME_BUNKER_WIDTH))
			{
				m_tiles[tileIndex].m_tileDef = startBunkerWallTileDef;
			}

			//Game start tile
			else if (tileRowIndex == 1 && tileColumnIndex == 1)
			{
				m_tiles[tileIndex].m_tileDef = mapEntryTileDef;
				m_startCoord = IntVec2(tileColumnIndex, tileRowIndex);
			}

			else
			{
				m_tiles[tileIndex].m_tileDef = startBunkerFloorTileDef;
			}
		}
	}

	//Exit Bunker
	const int EXIT_AREA_WIDTH = g_gameConfigBlackboard.GetValue("endAreaSize", 7); // 7 including walls plus perimeter of grass
	const int EXIT_AREA_HEIGHT = EXIT_AREA_WIDTH;

	TileDefinition const* mapExitTileDef = TileDefinition::GetTileDefinitionAdressFromName(m_mapDefinition->m_mapExitTileType.c_str());
	TileDefinition const* endBunkerWallTileDef = TileDefinition::GetTileDefinitionAdressFromName(m_mapDefinition->m_endBunkerWallTileType.c_str());
	TileDefinition const* endBunkerFloorTileDef = TileDefinition::GetTileDefinitionAdressFromName(m_mapDefinition->m_endBunkerFloorTileType.c_str());

	//Create exit area with bunker walls
	for (int tileColumnIndex = m_dimensions.x - EXIT_AREA_WIDTH; tileColumnIndex < m_dimensions.x - 1; ++tileColumnIndex)
	{
		for (int tileRowIndex = m_dimensions.y - EXIT_AREA_HEIGHT; tileRowIndex < m_dimensions.y - 1; ++tileRowIndex)
		{
			int tileIndex = GetTileIndexFromTileCoords(IntVec2(tileColumnIndex, tileRowIndex));

			//set walls one row / column inside home area
			if ((tileColumnIndex > m_dimensions.x - EXIT_AREA_WIDTH && tileColumnIndex < m_dimensions.x - 2)
				&& (tileRowIndex < m_dimensions.y - EXIT_AREA_HEIGHT + 2 && tileRowIndex > m_dimensions.y - EXIT_AREA_HEIGHT))
			{
				m_tiles[tileIndex].m_tileDef = endBunkerWallTileDef;
			}

			else if ((tileRowIndex > m_dimensions.y - EXIT_AREA_HEIGHT && tileRowIndex < m_dimensions.y - 2)
				&& (tileColumnIndex < m_dimensions.x - EXIT_AREA_WIDTH + 2 && tileColumnIndex > m_dimensions.x - EXIT_AREA_WIDTH))
			{
				m_tiles[tileIndex].m_tileDef = endBunkerWallTileDef;
			}

			//Game end tile
			else if (tileRowIndex == m_dimensions.y - 4 && tileColumnIndex == m_dimensions.x - 4)
			{
				m_tiles[tileIndex].m_tileDef = mapExitTileDef;
				m_endCoord = IntVec2(tileColumnIndex, tileRowIndex);
			}

			else
			{
				m_tiles[tileIndex].m_tileDef = endBunkerFloorTileDef;
			}
		}
	}
}

void Map::InitHeatMaps()
{
	PopulateDistanceMap(*m_startToEndDistanceMap, m_startCoord, DEFAULT_HEAT_MAP_SOLID_VALUE);
	int endTileIndex = GetTileIndexFromTileCoords(m_endCoord);
	if (m_startToEndDistanceMap->GetValue(endTileIndex) >= DEFAULT_HEAT_MAP_SOLID_VALUE)
	{
		m_startToEndDistanceMap->SetAllValues(DEFAULT_HEAT_MAP_SOLID_VALUE);
		m_numSpawnAttempts++;
		if (m_numSpawnAttempts >= 1000)
		{
			ERROR_AND_DIE("Map failed 1000 attemps to generate a solveable map");
		}
		SpawnTiles();
		return;
	}

	m_solidTileMap = new TileHeatMap(m_dimensions, DEFAULT_HEAT_MAP_SOLID_VALUE);
	m_debugHeatMaps[1] = m_solidTileMap;
	m_amphibianSolidMap = new TileHeatMap(m_dimensions, DEFAULT_HEAT_MAP_SOLID_VALUE);
	m_debugHeatMaps[2] = m_amphibianSolidMap;


	TileDefinition const* wallFillTileDef = TileDefinition::GetTileDefinitionAdressFromName(m_mapDefinition->m_wallFillTileType.c_str());
	for (int tileIndex = 0; tileIndex < static_cast<int>(m_tiles.size()); ++tileIndex)
	{
		if (m_debugHeatMaps[0]->GetValue(tileIndex) >= DEFAULT_HEAT_MAP_SOLID_VALUE)
		{
			TileDefinition const* tileDef = m_tiles[tileIndex].m_tileDef;
			char const* tileName = tileDef->m_name.c_str();

			//if tile name is NOT already a wall or water, fill it with mapDef->m_wallFillType
			if (strcmp(tileName, m_mapDefinition->m_borderTileType.c_str()) && strcmp(tileName, m_mapDefinition->m_startBunkerWallTileType.c_str()) //#TODO: make it so that water that is enclosed in walls still gets filled
				&& strcmp(tileName, m_mapDefinition->m_endBunkerWallTileType.c_str()) && !tileDef->m_isWater)
			{
				m_tiles[tileIndex].m_tileDef = wallFillTileDef;
			}
		}

		//Heat map for land based entities
		if (IsTileTraversable(tileIndex))
		{
			m_solidTileMap->SetValue(tileIndex, 0.f);
		}

		//Heat map for amphibians
		if (IsTileTraversable(tileIndex, false))
		{
			m_amphibianSolidMap->SetValue(tileIndex, 0.f);
		}
	}
}

Entity* Map::SpawnNewEntity(EntityType entityType, EntityFaction faction)
{
	Entity* newEntity = CreateNewEntity(entityType, faction);
	AddEntityToMap(newEntity);
	return newEntity;

}

void Map::SpawnExplosion(Vec2 const& pos, float size, float duration, Rgba8 const& tint)
{
	float halfSize = size * 0.5f;
	AABB2 explosionBounds(-halfSize, -halfSize, halfSize, halfSize);
	int lastIndex = m_explosionSpriteSheet->GetNumSprites() - 1;
	Entity* newExplosion = new Explosion(this, pos, explosionBounds, *m_explosionSpriteSheet, tint, ONCE, 0, lastIndex, duration);
	AddEntityToMap(newExplosion);
}

void Map::SpawnInitialNpcs()
{
	const int scorpioCount = m_mapDefinition->m_scorpioCount;
	const int leoCount = m_mapDefinition->m_leoCount;
	const int ariesCount = m_mapDefinition->m_ariesCount;
	const int capricornCount = m_mapDefinition->m_capricornCount;
	const int aquariusCount = m_mapDefinition->m_aquariusCount;
	const int geminiPairCount = m_mapDefinition->m_geminiPairCount;

	std::vector<Entity*> initialNpcs;
	initialNpcs.reserve(scorpioCount + leoCount + ariesCount + aquariusCount + (geminiPairCount * 2));

	for (int scorpioNum = 0; scorpioNum < scorpioCount; ++scorpioNum)
	{
		initialNpcs.push_back(SpawnNewEntity(ENTITY_TYPE_EVIL_SCORPIO, FACTION_EVIL));
	}

	for (int leoNum = 0; leoNum < leoCount; ++leoNum)
	{
		initialNpcs.push_back(SpawnNewEntity(ENTITY_TYPE_EVIL_LEO, FACTION_EVIL));
	}

	for (int ariesNum = 0; ariesNum < ariesCount; ++ariesNum)
	{
		initialNpcs.push_back(SpawnNewEntity(ENTITY_TYPE_EVIL_ARIES, FACTION_EVIL));
	}

	for (int capricornNum = 0; capricornNum < capricornCount; ++capricornNum)
	{
		initialNpcs.push_back(SpawnNewEntity(ENTITY_TYPE_EVIL_CAPRICORN, FACTION_EVIL));
	}

	for (int aquariusNum = 0; aquariusNum < aquariusCount; ++aquariusNum)
	{
		initialNpcs.push_back(SpawnNewEntity(ENTITY_TYPE_EVIL_AQUARIUS, FACTION_EVIL));
	}

	for (int geminiNum = 0; geminiNum < geminiPairCount; ++geminiNum)
	{
		Entity* gemini = SpawnNewEntity(ENTITY_TYPE_EVIL_GEMINI_BROTHER, FACTION_EVIL);
		Gemini* geminiBrother = dynamic_cast<Gemini*>(gemini);
		Entity* geminiSister = geminiBrother->m_twinEntity;

		initialNpcs.push_back(gemini);
		initialNpcs.push_back(geminiSister);
	}

	std::vector<IntVec2> spawnableTileCoords = GetAllSpawnableTileCoordsForNpcs();

	IntVec2 tileCoords;
	int randomTileIndex;
	for (int npcNum = 0; npcNum < static_cast<int>(initialNpcs.size()); ++npcNum)
	{
		if (initialNpcs[npcNum])
		{
			randomTileIndex = g_rng->RollRandomIntInRange(0, static_cast<int>(spawnableTileCoords.size() - 1));
			tileCoords = spawnableTileCoords[randomTileIndex];
			initialNpcs[npcNum]->m_position = GetTileCenterPosFromTileCoords(tileCoords);
			initialNpcs[npcNum]->InitPathFinding();
			spawnableTileCoords.erase(spawnableTileCoords.begin() + randomTileIndex);
		}
	}


	m_distanceMapToPlayer = new TileHeatMap(m_dimensions, DEFAULT_HEAT_MAP_SOLID_VALUE);
	m_amphibianDistanceMapToPlayer = new TileHeatMap(m_dimensions, DEFAULT_HEAT_MAP_SOLID_VALUE);
	PopulateDistanceMapWithStationaryEntities(*m_distanceMapToPlayer, m_startCoord, DEFAULT_HEAT_MAP_SOLID_VALUE);
	PopulateDistanceMapWithStationaryEntities(*m_amphibianDistanceMapToPlayer, m_startCoord, DEFAULT_HEAT_MAP_SOLID_VALUE, false);
	m_debugHeatMaps[3] = m_distanceMapToPlayer;

	UpdateTrackedLeo();
}

//Update functions
//-----------------------------------------------------------------------------------------------
void Map::UpdateEntities(float deltaSeconds)
{
	for (int entityIndex = 0; entityIndex < static_cast<int>(m_allEntities.size()); ++entityIndex)
	{
		if (m_allEntities[entityIndex] != nullptr)
		{
			m_allEntities[entityIndex]->Update(deltaSeconds);
		}
	}

	//Physics with each other
	for (int entityIndex = 0; entityIndex < static_cast<int>(m_allEntities.size()); ++entityIndex)
	{
		if (m_allEntities[entityIndex] != nullptr)
		{
			PushEntityOutOfOverlappingEntities(m_allEntities[entityIndex]);
		}
	}

	//Wall physics
	for (int entityIndex = 0; entityIndex < static_cast<int>(m_allEntities.size()); ++entityIndex)
	{
		if (m_allEntities[entityIndex] != nullptr)
		{
			PushEntityOutOfSurroundingTiles(m_allEntities[entityIndex]);
		}
	}

	//Bullet collision
	for (int bulletListNum = ENTITY_TYPE_GOOD_BOLT; bulletListNum < NUM_ENTITY_TYPES - 1; ++bulletListNum)
	{
		EntityList bulletList = m_entityListByType[bulletListNum];
		for (int bulletNum = 0; bulletNum < static_cast<int>(bulletList.size()); ++bulletNum)
		{
			CheckBulletCollision(bulletList[bulletNum]);
		}
	}

}

void Map::UpdateGameCameraToFollowPlayer()
{
	Vec2 playerPos = m_game->m_player->m_position;
	IntVec2 playerCoord = GetTileCoordsFromPosition(playerPos);
	AABB2& cameraBounds = m_game->m_currentWorldCameraBounds;
	int numVTilesInView = m_game->m_numberTilesInViewVertically;

	int numHTilesInView = numVTilesInView * static_cast<int>(g_gameConfigBlackboard.GetValue("worldAspect", 2.f));
	numVTilesInView /= 2;
	numHTilesInView /= 2;

	Vec2 boundsCenter = cameraBounds.GetCenterPos();
	if (playerCoord.x > numHTilesInView - 1 && m_dimensions.x - numHTilesInView > playerCoord.x)
	{
		cameraBounds.TranslateX(playerPos.x - boundsCenter.x);
	}

	if (playerCoord.y > numVTilesInView - 1 && m_dimensions.y - numVTilesInView > playerCoord.y)
	{
		cameraBounds.TranslateY(playerPos.y - boundsCenter.y);
	}
}

void Map::UpdateAndCheckOverrideTilesAge(float deltaSeconds)
{
	for (int tileNum = 0; tileNum < static_cast<int>(m_tileOverrides.size()); ++tileNum)
	{
		TileTypeOverride& tileOverride = m_tileOverrides[tileNum];
		tileOverride.m_age += deltaSeconds;
		if (tileOverride.m_age >= tileOverride.m_overrideDuration)
		{
			m_tiles[tileOverride.m_tileIndex].m_tileDef = tileOverride.m_oldTileDef;
			m_tileOverrides.erase(m_tileOverrides.begin() + tileNum);
			tileNum--;
		}
	}
}

//Render Functions
//-----------------------------------------------------------------------------------------------
void Map::RenderTiles() const
{
	std::vector<Vertex_PCU> mapTileVerts;
	const int NUM_TRIS = 2;
	const int NUM_VERTS = 3 * NUM_TRIS;
	mapTileVerts.reserve(NUM_VERTS * m_tiles.size());

	TileDefinition const* tileDef;
	float minX;
	float minY;

	for (int tileIndex = 0; tileIndex < static_cast<int>(m_tiles.size()); ++tileIndex)
	{
		Tile const& tile = m_tiles[tileIndex];
		tileDef = tile.m_tileDef;
		minX = static_cast<float>(tile.m_tileCoords.x);
		minY = static_cast<float>(tile.m_tileCoords.y);
		AddVertsForAABB2D(mapTileVerts, AABB2(minX, minY, minX + 1.f, minY + 1.f), tileDef->m_tintColor, tileDef->m_spriteSheetUVCoords.m_mins, tileDef->m_spriteSheetUVCoords.m_maxs);
	}

	g_renderer->BeginRendererEvent("Draw - Tiles");
	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(&m_terrainSpriteSheet->GetTexture());
	g_renderer->DrawVertexArray(mapTileVerts);
	g_renderer->EndRendererEvent();
}

void Map::RenderEntities() const
{
	//Render all entities except explosions
	g_renderer->BeginRendererEvent("Draw - Entities");
	for (int entityTypeIndex = 0; entityTypeIndex < ENTITY_TYPE_NEUTRAL_EXPLOSION; ++entityTypeIndex)
	{
		EntityList const& entitiesOfType = m_entityListByType[entityTypeIndex];

		for (int entityIndex = 0; entityIndex < static_cast<int>(entitiesOfType.size()); ++entityIndex)
		{
			Entity* entity = entitiesOfType[entityIndex];
			if (entity && entity->IsAlive())
			{
				entity->Render();
				if (g_debugMode)
				{
					entity->DebugRender();
				}
			}
		}
	}
	g_renderer->EndRendererEvent();
	
	g_renderer->BeginRendererEvent("Draw - Explosions");
	//render explosions
	EntityList const& explosionEntities = m_entityListByType[ENTITY_TYPE_NEUTRAL_EXPLOSION];
	for (int entityIndex = 0; entityIndex < static_cast<int>(explosionEntities.size()); ++entityIndex)
	{
		Entity* entity = explosionEntities[entityIndex];
		if (entity && entity->IsAlive())
		{
			entity->Render();
			if (g_debugMode)
			{
				entity->DebugRender();
			}
		}
	}
	g_renderer->EndRendererEvent();
}

void Map::RenderEntityHealthBars() const
{
	std::vector<Vertex_PCU> healthBarVerts;
	for (int entityListNum = 0; entityListNum < ENTITY_TYPE_GOOD_BOLT; ++entityListNum)
	{
		EntityList entityList = m_entityListByType[entityListNum];
		for (int entityNum = 0; entityNum < static_cast<int>(entityList.size()); ++entityNum)
		{
			Entity* entity = entityList[entityNum];
			if (entity && entity->IsAlive())
			{
				entity->AddVertsForHealthBar(healthBarVerts);
			}
		}
	}

	g_renderer->BeginRendererEvent("Draw - HealthBars");
	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(nullptr);
	g_renderer->DrawVertexArray(healthBarVerts);
	g_renderer->EndRendererEvent();
}

void Map::RenderPauseOverlay() const
{
	if (!m_game->m_isPaused || m_game->m_shouldIgnorePauseOverlay)
		return;

	std::vector<Vertex_PCU> overlayVerts;
	AABB2 mapBounds(0.f, 0.f, static_cast<float>(m_dimensions.x), static_cast<float>(m_dimensions.y));
	AddVertsForAABB2D(overlayVerts, mapBounds, Rgba8(0, 0, 0, 150));

	g_renderer->BindTexture(nullptr);
	g_renderer->DrawVertexArray(overlayVerts);

}

void Map::RenderDebugTileInfo() const
{
	if (!m_renderDebugTileCoords)
		return;

	BitmapFont* bitMapFont = g_renderer->CreatOrGetBitMapFontFromFile("Data/Font/SquirrelFixedFont");

	std::vector<Vertex_PCU> textVerts;
	std::vector<Vertex_PCU> gridVerts;
	std::string text;
	IntVec2 tileCoords;
	Vec2 position;
	AABB2 tileBounds;

	for (int tileIndex = 0; tileIndex < static_cast<int>(m_tiles.size()); tileIndex++)
	{
		//Tile coordinates text
		tileCoords = m_tiles[tileIndex].m_tileCoords;
		position = Vec2(static_cast<float>(tileCoords.x), static_cast<float>(tileCoords.y));
		text = std::to_string(tileCoords.x) + "," + std::to_string(tileCoords.y);
		bitMapFont->AddVertsForText2D(textVerts, position, 0.15f, text, Rgba8::WHITE);

		//Tile Index text
		float minX = static_cast<float>(m_tiles[tileIndex].m_tileCoords.x);
		float minY = static_cast<float>(m_tiles[tileIndex].m_tileCoords.y);
		tileBounds = AABB2(minX, minY, minX + 1.f, minY + 1.f);
		text = std::to_string(tileIndex);
		position = tileBounds.GetCenterPos();
		bitMapFont->AddVertsForCenteredText2D(textVerts, position, 0.15f, text, Rgba8::RED);

		//Grid overlay
		if (tileCoords.y % 2 == 0)
		{
			if (tileIndex % 2 == 0)
			{
				AABB2 overlaySquare(tileBounds);
				AddVertsForAABB2D(gridVerts, overlaySquare, Rgba8(150, 150, 150, 100));
			}
		}
		else if (tileCoords.x % 2 != 0)
		{
			AABB2 overlaySquare(tileBounds);
			AddVertsForAABB2D(gridVerts, overlaySquare, Rgba8(150, 150, 150, 100));
		}
	}

	g_renderer->BeginRendererEvent("Draw - TileDebug");
	g_renderer->BindTexture(nullptr);
	g_renderer->DrawVertexArray(gridVerts);

	g_renderer->BindTexture(&bitMapFont->GetTexture());
	g_renderer->DrawVertexArray(textVerts);
	g_renderer->EndRendererEvent();
}

void Map::RenderDebugHeatMap() const
{
	if (m_debugHeatMaps[m_currentHeatMapIndex] == nullptr || !m_renderHeatMap)
		return;

	std::vector<Vertex_PCU> heatMapVerts;
	std::string debugText;

	AABB2 mapBounds = AABB2(0.f, 0.f, (float)m_dimensions.x, (float)m_dimensions.y);
	switch(m_currentHeatMapIndex)
	{
	case 0:
		m_debugHeatMaps[m_currentHeatMapIndex]->AddVertsForDebugDraw(heatMapVerts, mapBounds, DEFAULT_HEAT_MAP_SOLID_VALUE);
		debugText = "Distance map from entry to exit";
		break;
	case 1:
		m_debugHeatMaps[m_currentHeatMapIndex]->AddVertsForDebugDraw(heatMapVerts, mapBounds, FloatRange(0.f, DEFAULT_HEAT_MAP_SOLID_VALUE));
		debugText = "Solid map";
		break;
	case 2:
		m_debugHeatMaps[m_currentHeatMapIndex]->AddVertsForDebugDraw(heatMapVerts, mapBounds, FloatRange(0.f, DEFAULT_HEAT_MAP_SOLID_VALUE));
		debugText = "Amphibian solid map";
		break;
	case 3:
		m_debugHeatMaps[m_currentHeatMapIndex]->AddVertsForDebugDraw(heatMapVerts, mapBounds, DEFAULT_HEAT_MAP_SOLID_VALUE);
		debugText = "Distance map to player";
		break;
	case 4:
		if (m_debugTrackedLeo == nullptr)
			break;

		m_debugHeatMaps[m_currentHeatMapIndex]->AddVertsForDebugDraw(heatMapVerts, mapBounds, DEFAULT_HEAT_MAP_SOLID_VALUE);
		debugText = "Entity distance map for roaming";
		break;
	}

	g_renderer->BindTexture(nullptr);
	g_renderer->DrawVertexArray(heatMapVerts);

	std::vector<Vertex_PCU> textVerts;
	BitmapFont* bitMapFont = g_renderer->CreatOrGetBitMapFontFromFile("Data/Font/SquirrelFixedFont");
	Vec2 textPos(2.f, m_dimensions.y - 2.f);
	bitMapFont->AddVertsForText2D(textVerts, textPos, 0.5f, debugText, Rgba8::RED);

	g_renderer->BindTexture(&bitMapFont->GetTexture());
	g_renderer->DrawVertexArray(textVerts);

	if (m_currentHeatMapIndex == 4 && m_debugTrackedLeo != nullptr)
	{
		m_debugTrackedLeo->DebugRenderPathFindingInfo();
	}
}

//Entity Management
//-----------------------------------------------------------------------------------------------
Entity* Map::CreateNewEntity(EntityType entityType, EntityFaction faction)
{

	switch (entityType)
	{
	case ENTITY_TYPE_GOOD_PLAYER: return new Player(this, faction, Vec2(1.5f, 1.5f));
	case ENTITY_TYPE_EVIL_SCORPIO: return new Scorpio(this, faction);
	case ENTITY_TYPE_EVIL_LEO: return new Leo(this, faction);
	case ENTITY_TYPE_EVIL_ARIES: return new Aries(this, faction);
	case ENTITY_TYPE_EVIL_CAPRICORN: return new Capricorn(this, faction);
	case ENTITY_TYPE_EVIL_AQUARIUS: return new Aquarius(this, faction);
	case ENTITY_TYPE_EVIL_GEMINI_BROTHER: return new Gemini(this, ENTITY_TYPE_EVIL_GEMINI_BROTHER, faction);
	case ENTITY_TYPE_EVIL_GEMINI_SISTER: return new Gemini(this, ENTITY_TYPE_EVIL_GEMINI_SISTER, faction);
	case ENTITY_TYPE_GOOD_BULLET: return new Bullet(this, ENTITY_TYPE_GOOD_BULLET, faction);
	case ENTITY_TYPE_GOOD_BOLT: return new Bullet(this, ENTITY_TYPE_GOOD_BOLT, faction);
	case ENTITY_TYPE_EVIL_BULLET: return new Bullet(this, ENTITY_TYPE_EVIL_BULLET, faction);
	case ENTITY_TYPE_EVIL_BOLT: return new Bullet(this, ENTITY_TYPE_EVIL_BOLT, faction);
	case ENTITY_TYPE_EVIL_BOUNCING_BOLT: return new Bullet(this, ENTITY_TYPE_EVIL_BOUNCING_BOLT, faction);
	case ENTITY_TYPE_EVIL_SHELL: return new Bullet(this, ENTITY_TYPE_EVIL_SHELL, faction);
	case ENTITY_TYPE_GOOD_FLAME_BULLET: return new Bullet(this, ENTITY_TYPE_GOOD_FLAME_BULLET, faction);
	default: return nullptr;
	}

}

void Map::AddEntityToMap(Entity* entity)
{
	AddEntityToList(entity, m_allEntities);
	AddEntityToList(entity, m_entityListByType[entity->m_entityType]);
	entity->m_map = this;
}

void Map::AddEntityToList(Entity* entity, EntityList& entityList)
{
	for (int listIndex = 0; listIndex < static_cast<int>(entityList.size()); ++listIndex)
	{
		if (entityList[listIndex] == nullptr)
		{
			entityList[listIndex] = entity;
			return;
		}
	}

	entityList.push_back(entity);
}

void Map::RemoveEntityFromMap(Entity* entity)
{
	RemoveEntityFromList(entity, m_allEntities);
	if (entity != nullptr)
	{
		RemoveEntityFromList(entity, m_entityListByType[entity->m_entityType]);
	}
}

void Map::RemoveEntityFromList(Entity* entity, EntityList& entityList)
{
	for (int listIndex = 0; listIndex < static_cast<int>(entityList.size()); ++listIndex)
	{
		if (entityList[listIndex] == entity)
		{
			entityList[listIndex] = nullptr;
			return;
		}
	}
}

void Map::DeleteGarbageEntities()
{
	for (int entityNum = 0; entityNum < static_cast<int>(m_allEntities.size()); ++entityNum)
	{
		Entity* entity = m_allEntities[entityNum];
		if (entity != nullptr && entity->IsGarbage())
		{
			RemoveEntityFromMap(entity);
			delete entity;
		}
	}
}

void Map::KillAllBulletsOnMap()
{
	for (int bulletListNum = ENTITY_TYPE_GOOD_BOLT; bulletListNum < NUM_ENTITY_TYPES; ++bulletListNum)
	{
		EntityList bulletList = m_entityListByType[bulletListNum];
		for (int bulletNum = 0; bulletNum < static_cast<int>(bulletList.size()); ++bulletNum)
		{
			if (bulletList[bulletNum] != nullptr)
			{
				bulletList[bulletNum]->m_isGarbage = true;
			}
		}
	}
}

//Player respawn
//-----------------------------------------------------------------------------------------------
void Map::CheckIfPlayerDied()
{
	if (!g_game->m_player->IsAlive())
	{
		g_game->m_inGameOverCountdownMode = true;
	}
}

void Map::ResetPlayer()
{
	Player* player = dynamic_cast<Player*>(g_game->m_player);
	if (player != nullptr)
	{
		player->m_position = GetTileCenterPosFromTileCoords(m_startCoord);
		g_game->m_player->m_isDead = false;
	}
	
}

void Map::RespawnPlayer()
{
	g_game->m_player->m_isDead = false;
	g_game->m_player->m_health = g_gameConfigBlackboard.GetValue("playerStartingHealth", 10.f);
	g_game->PlayGameSFX(RESPAWN);
}

//Physics
//-----------------------------------------------------------------------------------------------
void Map::PushEntityOutOfOverlappingEntities(Entity* entity)
{
	if (entity == nullptr || !entity->m_doesPushEntities)
		return;

	for (int entityNum = 0; entityNum < static_cast<int>(m_allEntities.size()); ++entityNum)
	{
		Entity* otherEntity = m_allEntities[entityNum];
		if (otherEntity == nullptr || entity == otherEntity)
			continue;

		if (DoDiscsOverlap(entity->m_position, entity->m_physicsRadius, otherEntity->m_position, otherEntity->m_physicsRadius))
		{
			if (entity->m_isPushedByEntities && otherEntity->m_doesPushEntities)
			{
				if (otherEntity->m_isPushedByEntities)
				{
					PushDiscsOutOfEachOther2D(entity->m_position, entity->m_physicsRadius, otherEntity->m_position, otherEntity->m_physicsRadius);
				}

				else
				{
					PushDiscOutOfFixedDisc2D(entity->m_position, entity->m_physicsRadius, otherEntity->m_position, otherEntity->m_physicsRadius);
				}
			}

			else if (otherEntity->m_isPushedByEntities)
			{
				PushDiscOutOfFixedDisc2D(otherEntity->m_position, otherEntity->m_physicsRadius, entity->m_position, entity->m_physicsRadius);
			}
			
		}
	}
}

void Map::PushEntityOutOfSurroundingTiles(Entity* entity)
{
	if (entity == nullptr || !entity->m_isPushedByWalls)
		return;

	Vec2& position = entity->m_position;
	IntVec2 posCoords = IntVec2(static_cast<int>(floorf(position.x)), static_cast<int>(floorf(position.y)));
	
	std::vector<IntVec2> surroundingTilesCoords;
	surroundingTilesCoords.reserve(8);
	surroundingTilesCoords.push_back(posCoords + IntVec2::NORTH); //north Tile
	surroundingTilesCoords.push_back(posCoords + IntVec2::EAST); //east Tile
	surroundingTilesCoords.push_back(posCoords + IntVec2::SOUTH); //south Tile
	surroundingTilesCoords.push_back(posCoords + IntVec2::WEST); //west Tile
	surroundingTilesCoords.push_back(IntVec2(posCoords.x - 1, posCoords.y + 1)); //northwest Tile
	surroundingTilesCoords.push_back(IntVec2(posCoords.x + 1, posCoords.y + 1)); //northeast Tile
	surroundingTilesCoords.push_back(IntVec2(posCoords.x + 1, posCoords.y - 1)); //southeast Tile
	surroundingTilesCoords.push_back(IntVec2(posCoords.x - 1, posCoords.y - 1)); //southwest Tile

	for (int tileIndex = 0; tileIndex < static_cast<int>(surroundingTilesCoords.size()); ++tileIndex)
	{
		IntVec2 tileCoords = surroundingTilesCoords[tileIndex];
		
		if (!IsTileInBounds(tileCoords))
			continue;

		if (!IsTileSolid(tileCoords, !entity->m_canTraverseWater))
			continue;

		AABB2 tileBounds = GetTileBoundsFromTileCoords(tileCoords);
		PushDiscOutOfFixedAABB2D(position, entity->m_physicsRadius, tileBounds);
	}
}

void Map::CheckBulletCollision(Entity* bullet)
{
	if (bullet == nullptr || !bullet->IsAlive())
		return;

	Vec2& bulletPos = bullet->m_position;

	//reverse for loop starting from index below where bullets start and working its way through all non-bullet entities. ends with player
	for (int entityListNum = ENTITY_TYPE_GOOD_BOLT - 1; entityListNum >= 0; --entityListNum) 
	{
		EntityList entityList = m_entityListByType[entityListNum];
		for (int entityNum = 0; entityNum < static_cast<int>(entityList.size()); ++entityNum)
		{
			Entity* entity = entityList[entityNum];
			if (entity == nullptr || !entity->m_isHitByBullets || !entity->IsAlive())
				continue;

			if (entity->m_entityFaction == bullet->m_entityFaction)
				continue;

			if (entityListNum == ENTITY_TYPE_EVIL_ARIES)
			{
				Aries* aries = dynamic_cast<Aries*>(entity);
				if (aries != nullptr && aries->DidBulletHitShield(bulletPos))
				{
					Bullet* bulletCast = dynamic_cast<Bullet*>(bullet);
					if (bulletCast != nullptr)
					{
						Vec2 shieldNormal = (bulletPos - aries->m_position).GetNormalized();
						PushDiscOutOfFixedDisc2D(bulletPos, aries->m_velocity.GetLength(), aries->m_position, aries->m_physicsRadius);
						bulletCast->BounceOffSurfaceNormal(shieldNormal);
						m_game->PlayGameSFX(BULLET_BOUNCE, bulletPos);
					}
					
					continue;
				}
			}

			if (DoDiscsOverlap(bullet->m_position, bullet->m_physicsRadius, entity->m_position, entity->m_physicsRadius))
			{
				entity->LoseHealth(bullet->m_damage);

				if (bullet->m_entityType == ENTITY_TYPE_GOOD_FLAME_BULLET)
				{
					bullet->m_damage = 0.f;
					continue;
				}

				bullet->Die();
			}
		}
	}
}

//Tile heat maps
//-----------------------------------------------------------------------------------------------
void Map::RotateThroughDebugHeatMaps()
{
	m_renderHeatMap = true;
	m_currentHeatMapIndex++;

	if (m_currentHeatMapIndex >= NUM_DEBUG_HEAT_MAPS)
	{
		m_currentHeatMapIndex = -1;
		m_renderHeatMap = false;
	}
}

void Map::UpdateTrackedLeo()
{
	m_debugTrackedLeo = nullptr;
	m_debugLeoRoamDistanceMap = nullptr;
	m_debugHeatMaps[4] = nullptr;

	EntityList leoList = m_entityListByType[ENTITY_TYPE_EVIL_LEO];
	for (int leoNum = 0; leoNum < static_cast<int>(leoList.size()); ++leoNum)
	{
		if (leoList[leoNum] != nullptr && leoList[leoNum]->IsAlive())
		{
			m_debugTrackedLeo = dynamic_cast<Leo*>(leoList[leoNum]);
			m_debugTrackedLeo->m_isDebugTrackedEntity = true;
			m_debugLeoRoamDistanceMap = m_debugTrackedLeo->m_roamDistanceMap;
			m_debugHeatMaps[4] = m_debugLeoRoamDistanceMap;
			return;
		}
	}
}

void Map::PopulateDistanceMap(TileHeatMap& out_distanceMap, IntVec2 const& startCoords, float maxCost, bool treatWaterAsSolid)
{
	int startIndex = GetTileIndexFromTileCoords(startCoords);
	out_distanceMap.SetAllValues(maxCost);
	out_distanceMap.SetValue(startIndex, 0.f);

	float currentValue = 0.f;
	int currentIndex;
	std::queue<IntVec2> tileQueue;
	tileQueue.push(startCoords);

	IntVec2 coords = startCoords;
	IntVec2 northCoords;
	IntVec2 eastCoords;
	IntVec2 southCoords;
	IntVec2 westCoords;

	while (!tileQueue.empty())
	{
		coords = tileQueue.front();
		currentIndex = GetTileIndexFromTileCoords(coords);
		currentValue = out_distanceMap.GetValue(currentIndex);
		northCoords = coords + IntVec2::NORTH;
		eastCoords = coords + IntVec2::EAST;
		southCoords = coords + IntVec2::SOUTH;
		westCoords = coords + IntVec2::WEST;

		if (IsTileTraversable(northCoords, treatWaterAsSolid))
		{
			int tileIndex = GetTileIndexFromTileCoords(northCoords);
			if (out_distanceMap.GetValue(tileIndex) > currentValue + 1)
			{
				out_distanceMap.SetValue(tileIndex, currentValue + 1);
				tileQueue.push(northCoords);
			}
		}

		if (IsTileTraversable(eastCoords, treatWaterAsSolid))
		{
			int tileIndex = GetTileIndexFromTileCoords(eastCoords);
			if (out_distanceMap.GetValue(tileIndex) > currentValue + 1)
			{
				out_distanceMap.SetValue(tileIndex, currentValue + 1);
				tileQueue.push(eastCoords);
			}
		}

		if (IsTileTraversable(southCoords, treatWaterAsSolid))
		{
			int tileIndex = GetTileIndexFromTileCoords(southCoords);
			if (out_distanceMap.GetValue(tileIndex) > currentValue + 1)
			{
				out_distanceMap.SetValue(tileIndex, currentValue + 1);
				tileQueue.push(southCoords);
			}
		}

		if (IsTileTraversable(westCoords, treatWaterAsSolid))
		{
			int tileIndex = GetTileIndexFromTileCoords(westCoords);
			if (out_distanceMap.GetValue(tileIndex) > currentValue + 1)
			{
				out_distanceMap.SetValue(tileIndex, currentValue + 1);
				tileQueue.push(westCoords);
			}
		}

		tileQueue.pop();
	}

}

void Map::PopulateDistanceMapWithStationaryEntities(TileHeatMap& out_distanceMap, IntVec2 const& startCoords, float maxCost, bool treatWaterAsSolid)
{
	EntityList scorpioList = m_entityListByType[ENTITY_TYPE_EVIL_SCORPIO];
	const int NUM_SCORPIOS = static_cast<int>(scorpioList.size());
	std::vector<IntVec2> scorpioCoords;
	scorpioCoords.reserve(NUM_SCORPIOS);

	for (int scorpioNum = 0; scorpioNum < NUM_SCORPIOS; ++scorpioNum)
	{
		Entity* scorpio = scorpioList[scorpioNum];
		if (scorpio == nullptr || !scorpio->IsAlive())
			continue;

		IntVec2 coords = GetTileCoordsFromPosition(scorpio->m_position);
		scorpioCoords.push_back(coords);
	}

	int startIndex = GetTileIndexFromTileCoords(startCoords);
	out_distanceMap.SetAllValues(maxCost);
	out_distanceMap.SetValue(startIndex, 0.f);

	float currentValue = 0.f;
	int currentIndex;
	std::queue<IntVec2> tileQueue;
	tileQueue.push(startCoords);

	IntVec2 coords = startCoords;
	IntVec2 northCoords;
	IntVec2 eastCoords;
	IntVec2 southCoords;
	IntVec2 westCoords;

	while (!tileQueue.empty())
	{
		coords = tileQueue.front();
		currentIndex = GetTileIndexFromTileCoords(coords);
		currentValue = out_distanceMap.GetValue(currentIndex);
		northCoords = coords + IntVec2::NORTH;
		eastCoords = coords + IntVec2::EAST;
		southCoords = coords + IntVec2::SOUTH;
		westCoords = coords + IntVec2::WEST;

		if (IsTileTraversable(northCoords, treatWaterAsSolid) && std::find(scorpioCoords.begin(), scorpioCoords.end(), northCoords) == scorpioCoords.end())
		{
			int tileIndex = GetTileIndexFromTileCoords(northCoords);
			if (out_distanceMap.GetValue(tileIndex) > currentValue + 1)
			{
				out_distanceMap.SetValue(tileIndex, currentValue + 1);
				tileQueue.push(northCoords);
			}
		}

		if (IsTileTraversable(eastCoords, treatWaterAsSolid) && std::find(scorpioCoords.begin(), scorpioCoords.end(), eastCoords) == scorpioCoords.end())
		{
			int tileIndex = GetTileIndexFromTileCoords(eastCoords);
			if (out_distanceMap.GetValue(tileIndex) > currentValue + 1)
			{
				out_distanceMap.SetValue(tileIndex, currentValue + 1);
				tileQueue.push(eastCoords);
			}
		}

		if (IsTileTraversable(southCoords, treatWaterAsSolid) && std::find(scorpioCoords.begin(), scorpioCoords.end(), southCoords) == scorpioCoords.end())
		{
			int tileIndex = GetTileIndexFromTileCoords(southCoords);
			if (out_distanceMap.GetValue(tileIndex) > currentValue + 1)
			{
				out_distanceMap.SetValue(tileIndex, currentValue + 1);
				tileQueue.push(southCoords);
			}
		}

		if (IsTileTraversable(westCoords, treatWaterAsSolid) && std::find(scorpioCoords.begin(), scorpioCoords.end(), westCoords) == scorpioCoords.end())
		{
			int tileIndex = GetTileIndexFromTileCoords(westCoords);
			if (out_distanceMap.GetValue(tileIndex) > currentValue + 1)
			{
				out_distanceMap.SetValue(tileIndex, currentValue + 1);
				tileQueue.push(westCoords);
			}
		}

		tileQueue.pop();
	}
}

void Map::GenerateEntityPathToTargetPos(std::vector<Vec2>& out_path, TileHeatMap const& distanceMap, Vec2 const& startPos, int maxLength)
{
	IntVec2 startCoords = GetTileCoordsFromPosition(startPos);
	IntVec2 endCoords = startCoords;
	IntVec2 waypointCoords = startCoords;
	Vec2 waypointPos = startPos;
	std::vector<Vec2> pathCurrentToEnd;

	//Find start value on distance map (end coords)
	int numTiles = distanceMap.m_dimensions.x * distanceMap.m_dimensions.y;
	for (int tileNum = 0; tileNum < numTiles; ++tileNum)
	{
		if (distanceMap.m_values[tileNum] <= 0.f)
		{
			endCoords = m_tiles[tileNum].m_tileCoords;
			break;
		}
	}

	//Liberally estimate how long the path will be for super windy scenaruos
	IntVec2 pathDimensions = endCoords - startCoords;
	int pathLength = pathDimensions.GetTaxicabLength();
	pathLength *= 3;
	if (maxLength < pathLength)
	{
		pathCurrentToEnd.reserve(maxLength);
	}

	else
	{
		pathCurrentToEnd.reserve(pathLength); 
	}

	float tileValue = DEFAULT_HEAT_MAP_SOLID_VALUE;

	//Populate path
	while (tileValue > 0.f && static_cast<int>(pathCurrentToEnd.size()) < maxLength)
	{
		int tileIndex = GetTileIndexFromPosition(waypointPos);
		tileValue = distanceMap.m_values[tileIndex];
		waypointCoords = GetTileCoordsFromPosition(waypointPos);
		waypointPos = GetNextClosestWayPointFromDistanceMap(distanceMap,waypointCoords);
		pathCurrentToEnd.push_back(waypointPos);
	}

	//populate out_path with pathCurrentToEnd in reverse order
	pathLength = static_cast<int>(pathCurrentToEnd.size());
	out_path.clear();
	out_path.reserve(pathLength);
	for (int pathPosNum = 0; pathPosNum < pathLength; ++pathPosNum)
	{
		waypointPos = pathCurrentToEnd[pathLength - pathPosNum - 1];
		out_path.push_back(waypointPos);
	}
}

Vec2 const Map::GetNextClosestWayPointFromDistanceMap(TileHeatMap const& distanceMap, IntVec2 const& startCoords)
{
	float startValue = distanceMap.m_values[GetTileIndexFromTileCoords(startCoords)];
	IntVec2 outCoords;

	IntVec2 northCoords = startCoords + IntVec2::NORTH;
	IntVec2 eastCoords = startCoords + IntVec2::EAST;
	IntVec2 southCoords = startCoords + IntVec2::SOUTH;
	IntVec2 westCoords = startCoords + IntVec2::WEST;

	const int NUM_DIRECTIONS = 4;
	IntVec2 surroundingTileCoords[NUM_DIRECTIONS] = { northCoords, eastCoords, southCoords, westCoords };
	std::map<float, IntVec2> valueCoordPairs; //map of coordinates with their distance value as the key
	std::vector<float> distanceValues; //list of heat map values on the tiles in each direction
	distanceValues.reserve(NUM_DIRECTIONS);

	//Add all coords to map with value key
	//Also add values to distanceValues list in prep to be sorted
	for (int surroudingTileIndex = 0; surroudingTileIndex < NUM_DIRECTIONS; ++surroudingTileIndex)
	{
		IntVec2 tileCoords = surroundingTileCoords[surroudingTileIndex];
		float value = distanceMap.m_values[GetTileIndexFromTileCoords(tileCoords)];
		distanceValues.push_back(value);
		valueCoordPairs.insert({ value, tileCoords });
	}

	//sort values from smallest to greatest
	std::sort(distanceValues.begin(), distanceValues.end(), std::less<float>());

	//look for valid coords in order of sorted list
	for (int valueIndex = 0; valueIndex < NUM_DIRECTIONS; ++valueIndex)
	{
		float value = distanceValues[valueIndex];
		if (value < startValue)
		{
			auto found = valueCoordPairs.find(value);
			if (found == valueCoordPairs.end())
				continue;

			outCoords = found->second;
			return(GetTileCenterPosFromTileCoords(outCoords));
		}
	}

	return GetTileCenterPosFromTileCoords(startCoords);
}

void Map::RegenerateSolidMapsForMobileEntities()
{
	const int NUM_MOBILE_ENTITY_LISTS = 6;
	std::vector<EntityList> mobileEntityList;
	mobileEntityList.reserve(NUM_MOBILE_ENTITY_LISTS);

	mobileEntityList.push_back(m_entityListByType[ENTITY_TYPE_EVIL_LEO]);
	mobileEntityList.push_back(m_entityListByType[ENTITY_TYPE_EVIL_ARIES]);
	mobileEntityList.push_back(m_entityListByType[ENTITY_TYPE_EVIL_CAPRICORN]);
	mobileEntityList.push_back(m_entityListByType[ENTITY_TYPE_EVIL_AQUARIUS]);
	mobileEntityList.push_back(m_entityListByType[ENTITY_TYPE_EVIL_GEMINI_BROTHER]);
	mobileEntityList.push_back(m_entityListByType[ENTITY_TYPE_EVIL_GEMINI_SISTER]);

	for (int entityListNum = 0; entityListNum < NUM_MOBILE_ENTITY_LISTS; ++entityListNum)
	{
  		EntityList entityList = mobileEntityList[entityListNum];
		for (int entityNum = 0; entityNum < static_cast<int>(entityList.size()); ++entityNum)
		{
 			Entity* entity = entityList[entityNum];
 			if (entity && entity->IsAlive())
 			{
 				entity->RegenerateSolidMap();
 			}
		}
	}

 	IntVec2 playerTileCoords = GetTileCoordsFromPosition(m_game->m_player->m_position);
 	PopulateDistanceMapWithStationaryEntities(*m_distanceMapToPlayer, playerTileCoords, DEFAULT_HEAT_MAP_SOLID_VALUE);
 	PopulateDistanceMapWithStationaryEntities(*m_amphibianDistanceMapToPlayer, playerTileCoords, DEFAULT_HEAT_MAP_SOLID_VALUE, false);
}

//Tile Overrides
//-----------------------------------------------------------------------------------------------
void Map::AddOverrideTileAtPos(Vec2 const& pos, std::string const& tileDefName, float duration)
{
	int tileIndex = GetTileIndexFromPosition(pos);
	if (!IsTileInBounds(GetTileCoordsFromPosition(pos)))
		return;

	for (int tileNum = 0; tileNum < static_cast<int>(m_tileOverrides.size()); ++tileNum)
	{
		if (m_tileOverrides[tileNum].m_tileIndex == tileIndex)
		{
			m_tileOverrides[tileNum].m_age = 0.f;
			return;
		}
	}

	TileDefinition const* overrideTileDef = TileDefinition::GetTileDefinitionAdressFromName(tileDefName.c_str());
	TileTypeOverride newTileOverride;
	newTileOverride.m_tileIndex = tileIndex;
	newTileOverride.m_oldTileDef = m_tiles[tileIndex].m_tileDef;
	newTileOverride.m_overrideTileDef = overrideTileDef;
	newTileOverride.m_age = 0.f;
	newTileOverride.m_overrideDuration = duration;

	m_tiles[tileIndex].m_tileDef = overrideTileDef;
	m_tileOverrides.push_back(newTileOverride);
}


//Tile Accessors
//-----------------------------------------------------------------------------------------------

int Map::GetTileIndexFromTileCoords(IntVec2 const& tileCoords) const
{
	return (tileCoords.y * m_dimensions.x) + tileCoords.x;
}

int Map::GetTileIndexFromTileCoords(int tileCoordX, int tileCoordY) const
{
	return (tileCoordY * m_dimensions.x) + tileCoordX;
}

int Map::GetTileIndexFromPosition(Vec2 const& position) const
{
	IntVec2 tileCoords = GetTileCoordsFromPosition(position);
	return GetTileIndexFromTileCoords(tileCoords);
}

IntVec2 Map::GetTileCoordsFromPosition(Vec2 const& position) const
{
	return IntVec2(static_cast<int>(floorf(position.x)), static_cast<int>(floorf(position.y)));
}

Vec2 Map::GetTileCenterPosFromTileCoords(IntVec2 const& tileCoords) const
{
	float xPos = static_cast<float>(tileCoords.x) + 0.5f;
	float yPos = static_cast<float>(tileCoords.y) + 0.5f;
	return Vec2(xPos, yPos);
}

bool Map::WillBulletHitSolid(Bullet* const& bullet, Vec2 const& point) const
{
	IntVec2 tileCoords = GetTileCoordsFromPosition(point);
	if (!IsTileInBounds(tileCoords) && bullet != nullptr)
	{
		bullet->Die(); // kill bullet if it exits the map
		return false;
	}

	int tileIndex = GetTileIndexFromPosition(point);
	TileDefinition const* tileDef = m_tiles[tileIndex].m_tileDef;
	return tileDef->m_isSolid;
}

bool Map::IsTileSolid(IntVec2 const& tileCoords, bool treatWaterAsSolid) const
{
	if (!IsTileInBounds(tileCoords))
		return true;

	int tileIndex = GetTileIndexFromTileCoords(tileCoords);
	TileDefinition const* tileDef = m_tiles[tileIndex].m_tileDef;
	if (tileDef->m_isWater)
	{
		return treatWaterAsSolid;
	}

	return tileDef->m_isSolid;
}

bool Map::IsTileInBounds(IntVec2 const& tileCoords) const
{
	return tileCoords.x >= 0.f && tileCoords.y >= 0.f
		&& tileCoords.x < m_dimensions.x && tileCoords.y < m_dimensions.y;
}

bool Map::HasLineOfSight(Vec2 const& startPos, Vec2 const& endPos, float maxDist) const
{
	float distSqrd = GetDistanceSquared2D(startPos, endPos);

	if (distSqrd > (maxDist * maxDist))
		return false;

	Ray2 ray(startPos, endPos);
	RaycastResult2D raycastResult = RaycastVsTiles(ray);
	return !raycastResult.m_didImpact;
}

bool Map::HasLineOfSight(Vec2 const& startPos, Vec2 const& endPos, float maxDist, TileHeatMap const& solidMap) const
{
	float distSqrd = GetDistanceSquared2D(startPos, endPos);
	if (distSqrd > (maxDist * maxDist))
		return false;

	Ray2 ray(startPos, endPos);
	RaycastResult2D result = RaycastVsTileHeatMap(ray, solidMap, DEFAULT_HEAT_MAP_SOLID_VALUE);
	return !result.m_didImpact;
}

RaycastResult2D const Map::RaycastVsTiles(Ray2 const& ray, bool treatWaterAsSolid) const
{
	TileHeatMap* solidMap = m_solidTileMap;
	if (!treatWaterAsSolid)
	{
		solidMap = m_amphibianSolidMap;
	}

	return RaycastVsTileHeatMap(ray, *solidMap, DEFAULT_HEAT_MAP_SOLID_VALUE);
}

std::vector<IntVec2> Map::GetAllTraversableTileCoords(float treatWaterAsSolid) const
{
	std::vector<IntVec2> travelableTiles;
	for (int tileIndex = 0; tileIndex < static_cast<int>(m_tiles.size()); ++tileIndex)
	{
		if (IsTileTraversable(tileIndex, treatWaterAsSolid))
		{
			travelableTiles.push_back(m_tiles[tileIndex].m_tileCoords);
		}
	}
	return travelableTiles;
}

Vec2 Map::GetRandomTraversablePosFromSolidMap(TileHeatMap* const& solidMap) const
{
	std::vector<IntVec2> traversableCoords;
	IntVec2 tileCoords;
	int numTiles = solidMap->m_dimensions.x * solidMap->m_dimensions.y;
	for (int tileIndex = 0; tileIndex < numTiles; ++tileIndex)
	{
		if (solidMap->GetValue(tileIndex) != DEFAULT_HEAT_MAP_SOLID_VALUE)
		{
			tileCoords = m_tiles[tileIndex].m_tileCoords;
			if (IsTileWithinBorderWalls(tileCoords))
			{
				traversableCoords.push_back(tileCoords);
			}
		}
	}

	int randomIndex = g_rng->RollRandomIntInRange(0, static_cast<int>(traversableCoords.size()) - 1);
	IntVec2 randTileCoords = traversableCoords[randomIndex];

	return GetTileCenterPosFromTileCoords(randTileCoords);
}

std::vector<IntVec2> Map::GetAllSpawnableTileCoordsForNpcs() const
{
	std::vector<IntVec2> travelableTiles;
	for (int tileIndex = 0; tileIndex < static_cast<int>(m_tiles.size()); ++tileIndex)
	{
		IntVec2 tileCoords = m_tiles[tileIndex].m_tileCoords;
		if (IsTileInBounds(tileCoords) && IsTileSpawnableForNpcs(tileCoords))
		{
			travelableTiles.push_back(m_tiles[tileIndex].m_tileCoords);
		}
	}
	return travelableTiles;
}

bool Map::IsTileWithinBorderWalls(IntVec2 const& tileCoords) const
{
	return tileCoords.x >= 1 && tileCoords.y >= 1
		&& tileCoords.x < m_dimensions.x - 1 && tileCoords.y < m_dimensions.y - 1;
}

bool Map::IsTileTraversable(int tileIndex, bool treatWaterAsSolid) const
{
	IntVec2 tileCoords = m_tiles[tileIndex].m_tileCoords;
	if (!IsTileInBounds(tileCoords))
		return false;

	TileDefinition const* tileDef = m_tiles[tileIndex].m_tileDef;
	if (treatWaterAsSolid && tileDef->m_isWater)
	{
		return false;
	}

	return !tileDef->m_isSolid;
}

bool Map::IsTileTraversable(IntVec2 const& tileCoords, bool treatWaterAsSolid) const
{
	if(!IsTileInBounds(tileCoords))
		return false;

	int index = GetTileIndexFromTileCoords(tileCoords);
	TileDefinition const* tileDef = m_tiles[index].m_tileDef;

	if (tileDef->m_isWater)
	{
		return !treatWaterAsSolid;
	}

	return !tileDef->m_isSolid;
}

bool Map::IsTileSpawnableForNpcs(IntVec2 const& tileCoords) const
{
	if (!IsTileInBounds(tileCoords))
		return false;

	int index = GetTileIndexFromTileCoords(tileCoords);

	TileDefinition const* tileDef = m_tiles[index].m_tileDef;
	char const* tileName = tileDef->m_name.c_str();

	// exclude bunker floors, start, and exit tiles from spawnable area
	if (!strcmp(tileName, m_mapDefinition->m_startBunkerFloorTileType.c_str()) || !strcmp(tileName, m_mapDefinition->m_endBunkerFloorTileType.c_str())
		|| !strcmp(tileName, m_mapDefinition->m_mapEntryTileType.c_str()) || !strcmp(tileName, m_mapDefinition->m_mapExitTileType.c_str()))
	{
		return false;
	}

	if (tileDef->m_isWater)
	{
		return false;
	}

	return !tileDef->m_isSolid;
}

AABB2 Map::GetTileBoundsFromTileCoords(IntVec2 const& tileCoords) const
{
	float minX = static_cast<float>(tileCoords.x);
	float minY = static_cast<float>(tileCoords.y);
	return AABB2(minX, minY, minX + 1.f, minY + 1.f);
}
