#pragma once
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include <vector>

class Game;
class Texture;
struct IntVec2;
class TileHeatMap;
class Map;


enum EntityFaction : int
{
	FACTION_UNKNOWN = -1,
	FACTION_GOOD,
	FACTION_NEUTRAL,
	FACTION_EVIL,
	NUM_FACTIONS,
};

enum EntityType : int 
{
	ENTITY_TYPE_UNKNOWN	= -1,
	ENTITY_TYPE_GOOD_PLAYER,
	ENTITY_TYPE_EVIL_SCORPIO,
	ENTITY_TYPE_EVIL_LEO,
	ENTITY_TYPE_EVIL_ARIES,
	ENTITY_TYPE_EVIL_CAPRICORN,
	ENTITY_TYPE_EVIL_AQUARIUS,
	ENTITY_TYPE_EVIL_GEMINI_BROTHER,
	ENTITY_TYPE_EVIL_GEMINI_SISTER,
	ENTITY_TYPE_GOOD_BOLT,
	ENTITY_TYPE_GOOD_BULLET,
	ENTITY_TYPE_EVIL_BOLT,
	ENTITY_TYPE_EVIL_BOUNCING_BOLT,
	ENTITY_TYPE_EVIL_BULLET,
	ENTITY_TYPE_EVIL_SHELL,
	ENTITY_TYPE_GOOD_FLAME_BULLET,
	ENTITY_TYPE_NEUTRAL_EXPLOSION,
	NUM_ENTITY_TYPES
};

class Entity
{
	friend class Map;
public:
	void ReplenishHealth();
	virtual void LoseHealth(float amount = 1.f);

protected:
	explicit Entity(Map* const& mapOwner, EntityType entityType, EntityFaction faction, Vec2 const& startingPosition, float orientationDeg); 
	explicit Entity(Map* const& mapOwner, EntityType entityType, EntityFaction faction);
	virtual ~Entity();

	virtual void Update(float deltaSeconds) = 0; 
	virtual void Render() const = 0;
	virtual void DebugRender() const;

	//Mutators
	//----------------------------------------------------------------------

	virtual void Die();
	virtual void UpdateGameConfigXmlData() = 0;

	//Pathfinding
	virtual Vec2 const UpdateEntityPathFinding(float deltaSeconds); 
	void RegenerateSolidMap();
	void InitPathFinding();
	void SetNextWaypoint(Vec2 const& fwrdNormal);

	//Update functions
	void UpdateTimers(float deltaSeconds);
	virtual Vec2 const UpdatePositionAndOrientation(float deltaSeconds); //returns fwrd Vector
	void TryShootBullet(EntityType bulletType, EntityFaction faction, Vec2 const& fwrdNormal, bool isPlayer = false);

	//for rendering health bars
	void AddVertsForHealthBar(std::vector<Vertex_PCU>& verts) const;

	//Accessors
	//----------------------------------------------------------------------
	virtual bool IsOnTargetTile(Vec2 const& currentPos, Vec2 const& targetPos) const;
	virtual bool IsOnTargetTile(Vec2 const& currentPos, IntVec2 const& targetTileCoords) const;
	bool IsTileAccessible(IntVec2 const& tileCoords) const;

	Vec2 const GetForwardNormal() const;
	Texture* GetTexture() const;
	virtual bool CanSeePlayer(float sightRange) const;
	bool CanTravelToPlayer() const;
	bool IsOnTileAdjacentToPlayer() const;
	bool const IsAlive() const { return !m_isDead; };
	bool const IsGarbage() const { return m_isGarbage; };
	bool const DidChangeTile() const;

private:
	virtual void TurnTowardsPosition(Vec2 const& targetPos, float maxTurnDegrees);
	virtual void CreateTexture() = 0;
	bool const IsEntityBullet(EntityType entityType) const;

public:
	Vec2 m_position;
	float m_physicsRadius = 0.3f;

protected:
	Map* m_map = nullptr;
	EntityFaction m_entityFaction = FACTION_UNKNOWN;
	EntityType m_entityType = ENTITY_TYPE_UNKNOWN;

	//Appearance
	Texture* m_texture = nullptr;
	AABB2 m_entityBounds;
	float m_cosmeticRadius = .5f;
	std::vector<Vertex_PCU> m_shapeVerts;

	//Health
	float m_health = 1.f;
	float m_maxHealth = 0;
	bool m_isDead = false;
	bool m_isGarbage = false;

	float m_damage = 1.f;
	float m_fireCountdownTime = 0.f;

	//Movement and orientation
	Vec2 m_velocity;
	Vec2 m_positionLastFrame;
	float m_orientationDegrees = 0.f;
	float m_moveSpeed;
	float m_turnSpeed;
	float m_sightRange;
	float m_driveAperture;

	//Physics
	bool m_doesPushEntities = true;
	bool m_isPushedByEntities = true;
	bool m_isPushedByWalls = true;
	bool m_isHitByBullets = true;
	bool m_canTraverseWater = false;

	//Bullet firing
	float m_fireAperture;
	float m_bulletSpawnOffset = 0.f;
	float m_fireRate = 1.f;

	//Limit on SFX and VFX
	float m_fireSFXAge = 0.f;
	float m_damageSFXAge = 0.f;

	//Pathfinding
	TileHeatMap* m_roamDistanceMap = nullptr;
	TileHeatMap* m_solidMap = nullptr;
	Vec2 m_targetPos;
	Vec2 m_nextWaypointPos;
	std::vector<Vec2> m_pathToTarget;
	bool m_chasingPlayerLocation = false;

	//Debug
	float m_debugLineThickness;
	bool m_isDebugTrackedEntity = false;
};

