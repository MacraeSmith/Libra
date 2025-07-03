#pragma once
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Game/Entity.hpp"
#include "Game/GameCommon.hpp"
#include <vector>

class Game;
class Entity;
class Leo;
class Bullet;
struct Tile;
struct Vec2;
struct RaycastResult2D;
struct Ray2;
class SpriteSheet;
class TileHeatMap;
class TileHeatMap;
struct MapDefinition;
struct TileDefinition;

typedef std::vector<Entity*> EntityList;

struct TileTypeOverride
{
	TileDefinition const* m_oldTileDef;
	TileDefinition const* m_overrideTileDef;
	int m_tileIndex;
	float m_age;
	float m_overrideDuration;
};

class Map
{
public:
	explicit Map(Game* const& game, int mapIndex, MapDefinition* const m_mapDefinition);
	~Map();

	//Frame Flow
	void Update(float deltaSeconds);
	void Render() const;
	void EndFrame();

	//Entity Management
	Entity* SpawnNewEntity(EntityType entityType, EntityFaction faction);
	void SpawnExplosion(Vec2 const& pos, float size, float duration, Rgba8 const& tint = Rgba8::WHITE);
	void AddEntityToMap(Entity* entity);
	void RemoveEntityFromMap(Entity* entity);
	void SpawnInitialNpcs();
	void KillAllBulletsOnMap();

	//Player Management
	void ResetPlayer();
	void RespawnPlayer();

	//Tile Overrides
	void AddOverrideTileAtPos(Vec2 const& pos, std::string const& tileDefName, float duration);
	void UpdateAndCheckOverrideTilesAge(float deltaSeconds);

	//Tile index
	int GetTileIndexFromTileCoords(IntVec2 const& tileCoords) const;
	int GetTileIndexFromTileCoords(int tileCoordX, int tileCoordY) const;
	int GetTileIndexFromPosition(Vec2 const& position) const;

	//Tile coords
	IntVec2 GetTileCoordsFromPosition(Vec2 const& position) const;
	Vec2 GetTileCenterPosFromTileCoords(IntVec2 const& tileCoords) const;
	AABB2 GetTileBoundsFromTileCoords(IntVec2 const& tileCoords) const;

	//Tile solid checks
	bool WillBulletHitSolid(Bullet* const& bullet, Vec2 const& point) const;
	bool IsTileSolid(IntVec2 const& tileCoords, bool treatWaterAsSolid = false) const; // defaults false for sake of raycasts
	bool IsTileInBounds(IntVec2 const& tileCoords) const;

	//Raycast
	bool HasLineOfSight(Vec2 const& startPos, Vec2 const& endPos, float maxDist) const;
	bool HasLineOfSight(Vec2 const& startPos, Vec2 const& endPos, float maxDist, TileHeatMap const& solidMap) const; //if entity has specific solid map that blocks scorpios or other stationary entities
	RaycastResult2D const RaycastVsTiles(Ray2 const& ray, bool treatWaterAsSolid = false) const;

	//Heat Maps
	void PopulateDistanceMap(TileHeatMap& out_distanceMap, IntVec2 const& startCoords, float maxCost, bool treatWaterAsSolid = true);
	void PopulateDistanceMapWithStationaryEntities(TileHeatMap& out_distanceMap, IntVec2 const& startCoords, float maxCost, bool treatWaterAsSolid = true);
	void GenerateEntityPathToTargetPos(std::vector<Vec2>& out_path, TileHeatMap const& distanceMap, Vec2 const& startPos, int maxLength = 500);
	Vec2 GetRandomTraversablePosFromSolidMap(TileHeatMap* const& solidMap) const;
	Vec2 const GetNextClosestWayPointFromDistanceMap(TileHeatMap const& distanceMap, IntVec2 const& startCoords);
	void RegenerateSolidMapsForMobileEntities();
	void RotateThroughDebugHeatMaps();
	void UpdateTrackedLeo();

private:
	//Creation and Initialization
	void SpawnTiles();
	void SpawnTilesFromImage();
	void SpawnBunkerAreas();
	void SpawnWorms();
	void InitHeatMaps();
	
	//Update
	void UpdateEntities(float deltaSeconds);
	void UpdateGameCameraToFollowPlayer();
	void CheckIfPlayerDied();

	//Render
	void RenderTiles() const;
	void RenderEntities() const;
	void RenderEntityHealthBars() const;
	void RenderPauseOverlay() const;
	void RenderDebugTileInfo() const; //Debug for showing tile coords and tile index. WARNING! it is super taxing on performance
	void RenderDebugHeatMap() const;

	//Entity Management
	Entity* CreateNewEntity(EntityType entityType, EntityFaction faction);
	void AddEntityToList(Entity* entity, EntityList& entityList);
	void RemoveEntityFromList(Entity* entity, EntityList& entityList);
	void DeleteGarbageEntities();

	//Physics
	void PushEntityOutOfOverlappingEntities(Entity* entity);
	void PushEntityOutOfSurroundingTiles(Entity* entity);
	void CheckBulletCollision(Entity* bullet);

	//Helper Functions
	//-----------------------------------------------------------------------------------------------
	
	//Get random tiles
	std::vector<IntVec2> GetAllTraversableTileCoords(float treatWaterAsSolid = true) const;
	std::vector<IntVec2> GetAllSpawnableTileCoordsForNpcs() const;

	//Check tile conditions
	bool IsTileTraversable(int tileIndex, bool treatWaterAsSolid = true) const;
	bool IsTileTraversable(IntVec2 const& tileCoords, bool treatWaterAsSolid = true) const;
	bool IsTileSpawnableForNpcs(IntVec2 const& tileCoords) const;
	bool IsTileWithinBorderWalls(IntVec2 const& tileCoords) const;
	

public:
	Game* m_game = nullptr;
	bool m_isGarbage = false;

	//Tile Info
	std::vector<Tile> m_tiles;
	std::vector<TileTypeOverride> m_tileOverrides;

	IntVec2 m_dimensions;
	IntVec2 m_startCoord;
	IntVec2 m_endCoord;

	//Entity Info
	EntityList m_allEntities;
	EntityList m_entityListByType[NUM_ENTITY_TYPES];

	SpriteSheet* m_explosionSpriteSheet = nullptr;
	TileHeatMap* m_distanceMapToPlayer = nullptr;
	TileHeatMap* m_amphibianDistanceMapToPlayer = nullptr;

private:
	//Rendering
	SpriteSheet* m_terrainSpriteSheet = nullptr;
	bool m_renderDebugTileCoords = false;
	int m_mapIndex = -1;

	//Heat map management
	bool m_renderHeatMap = false;
	TileHeatMap* m_debugHeatMaps[NUM_DEBUG_HEAT_MAPS] = {};
	int m_currentHeatMapIndex = -1;

	TileHeatMap* m_startToEndDistanceMap = nullptr;
	TileHeatMap* m_solidTileMap = nullptr;
	TileHeatMap* m_amphibianSolidMap = nullptr;
	TileHeatMap* m_debugLeoRoamDistanceMap = nullptr;
	Leo* m_debugTrackedLeo = nullptr;

	//Map initialization
	MapDefinition* const m_mapDefinition = nullptr;
	int m_numSpawnAttempts = 0;
};

