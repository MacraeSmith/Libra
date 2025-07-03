#include "Game/Entity.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Map.hpp"
#include "Game/Player.hpp"
#include "Game/Game.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/RendererDX11.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/TileHeatMap.hpp"
#include "Engine/Math/LineSegment2.hpp"

Entity::Entity(Map* const& mapOwner, EntityType entityType, EntityFaction faction, Vec2 const& startingPosition, float orientationDeg)
 	: m_map(mapOwner)
	, m_entityType(entityType)
 	, m_entityFaction(faction)
 	, m_position(startingPosition)
 	, m_orientationDegrees(orientationDeg)
 {
 	m_debugLineThickness = g_gameConfigBlackboard.GetValue("debugDrawLineThickness", 0.03f);
 }

Entity::Entity(Map* const& mapOwner, EntityType entityType, EntityFaction faction)
	:m_map(mapOwner)
	,m_entityType(entityType)
	,m_entityFaction(faction)
{
	m_debugLineThickness = g_gameConfigBlackboard.GetValue("debugDrawLineThickness", 0.03f);
}

Entity::~Entity()
{
	delete(m_roamDistanceMap);
	m_roamDistanceMap = nullptr;

	delete(m_solidMap);
	m_solidMap = nullptr;
}

//Pathfinding
//----------------------------------------------------------------------
void Entity::InitPathFinding()
{
	if (m_solidMap && m_roamDistanceMap)
	{
		m_map->PopulateDistanceMapWithStationaryEntities(*m_solidMap, IntVec2(m_position), DEFAULT_HEAT_MAP_SOLID_VALUE, !m_canTraverseWater);
		m_targetPos = m_map->GetRandomTraversablePosFromSolidMap(m_solidMap);
		m_map->PopulateDistanceMapWithStationaryEntities(*m_roamDistanceMap, IntVec2(m_targetPos), DEFAULT_HEAT_MAP_SOLID_VALUE, !m_canTraverseWater);
		m_map->GenerateEntityPathToTargetPos(m_pathToTarget, *m_roamDistanceMap, m_position);
		m_nextWaypointPos = m_pathToTarget.back();
		m_targetPos = m_pathToTarget.front();
	}
}

Vec2 const Entity::UpdateEntityPathFinding(float deltaSeconds)
{
	TileHeatMap* distanceMapToPlayer = m_map->m_distanceMapToPlayer;
	if (m_canTraverseWater)
	{
		distanceMapToPlayer = m_map->m_amphibianDistanceMapToPlayer;
	}

	Vec2 playerPos = g_game->m_player->m_position;
	IntVec2 playerTileCoords = m_map->GetTileCoordsFromPosition(playerPos);
	if (m_map->HasLineOfSight(m_position, playerPos, m_sightRange) && IsTileAccessible(playerTileCoords))
	{
		m_chasingPlayerLocation = true;
		m_map->GenerateEntityPathToTargetPos(m_pathToTarget, *distanceMapToPlayer, m_position, static_cast<int>(m_sightRange));
		m_targetPos = m_pathToTarget.front();

		if (IsOnTileAdjacentToPlayer())
		{
			m_targetPos = playerPos;
			m_nextWaypointPos = m_targetPos;
		}
	}

	
	SetNextWaypoint(GetForwardNormal());
	
	Vec2 fwrdNormal = UpdatePositionAndOrientation(deltaSeconds);

	//Update waypoint position whenever entity arrives at the next one
	if (IsOnTargetTile(m_position, m_nextWaypointPos))
	{
		SetNextWaypoint(fwrdNormal);
	}

	//Update target position when arrived at target tile
	if (IsOnTargetTile(m_position, m_targetPos) || m_pathToTarget.size() < 1)
	{
		if (!m_chasingPlayerLocation)
		{
			m_targetPos = m_map->GetRandomTraversablePosFromSolidMap(m_solidMap);
			m_map->PopulateDistanceMapWithStationaryEntities(*m_roamDistanceMap, IntVec2(m_targetPos), DEFAULT_HEAT_MAP_SOLID_VALUE, !m_canTraverseWater);
			m_map->GenerateEntityPathToTargetPos(m_pathToTarget, *m_roamDistanceMap, m_position);
			m_nextWaypointPos = m_pathToTarget.back();
		}

		m_chasingPlayerLocation = false;
	}

	return fwrdNormal;
}

void Entity::SetNextWaypoint(Vec2 const& fwrdNormal)
{
	if (m_pathToTarget.size() < 2)
		return;

	int nextIndex = static_cast<int>(m_pathToTarget.size() - 2);

	//Raycast from center
	if (!m_map->HasLineOfSight(m_position, m_pathToTarget[nextIndex], m_sightRange, *m_roamDistanceMap))
		return;

	Vec2 jBasis = fwrdNormal.GetRotated90Degrees();
	Vec2 offset = jBasis * m_physicsRadius;

	//Raycast from left wing forward
	if (!m_map->HasLineOfSight(m_position + offset, m_pathToTarget[nextIndex] + offset, m_sightRange, *m_roamDistanceMap))
		return;

	//Raycast from right wing forward
	if (!m_map->HasLineOfSight(m_position - offset, m_pathToTarget[nextIndex] - offset, m_sightRange, *m_roamDistanceMap))
		return;

	m_pathToTarget.pop_back();

	if (GetDistanceSquared2D(m_position, m_pathToTarget.back()) < (m_physicsRadius * m_physicsRadius) && m_pathToTarget.size() < 2)
	{
		m_pathToTarget.pop_back();
	}

	if (m_pathToTarget.size() > 0)
	{
		m_nextWaypointPos = m_pathToTarget.back();
	}
}

void Entity::RegenerateSolidMap()
{
	if (m_solidMap == nullptr)
		return;

	m_map->PopulateDistanceMapWithStationaryEntities(*m_solidMap, IntVec2(m_position), DEFAULT_HEAT_MAP_SOLID_VALUE, !m_canTraverseWater);
}

//Update flow
//----------------------------------------------------------------------
void Entity::UpdateTimers(float deltaSeconds)
{
	m_fireSFXAge += deltaSeconds;
	m_damageSFXAge += deltaSeconds;
	m_fireCountdownTime -= deltaSeconds;
}

Vec2 const Entity::UpdatePositionAndOrientation(float deltaSeconds)
{
	//Rotate towards waypoint
	Vec2 dispToNextWaypoint = m_nextWaypointPos - m_position;
	m_orientationDegrees = GetTurnedTowardDegrees(m_orientationDegrees, dispToNextWaypoint.GetOrientationDegrees(), m_turnSpeed * deltaSeconds);
	Vec2 fwrdNormal = GetForwardNormal(); //update forward normal after rotation

	float angleToWaypoint = GetAngleDegreesBetweenVectors2D(dispToNextWaypoint, fwrdNormal);

	//Update position if within drive apeture
	if (angleToWaypoint <= m_driveAperture && angleToWaypoint >= -m_driveAperture)
	{
		m_velocity = fwrdNormal * m_moveSpeed * deltaSeconds;
		m_position += m_velocity;
	}

	return fwrdNormal;
}

void Entity::TryShootBullet(EntityType bulletType, EntityFaction faction, Vec2 const& fwrdNormal, bool isPlayer)
{
	if (!IsEntityBullet(bulletType))
		return;
	
	if (!isPlayer)
	{
		if (!m_chasingPlayerLocation || m_fireCountdownTime > 0 || !CanSeePlayer(m_sightRange))
			return;

		//Quit if angle not close enough to player
		Vec2 playerPos = g_game->m_player->m_position;
		Vec2 dispToPlayer = playerPos - m_position;
		float angleToPlayer = GetAngleDegreesBetweenVectors2D(dispToPlayer, fwrdNormal);

		if (angleToPlayer > m_fireAperture || angleToPlayer < -m_fireAperture)
			return;
	}
	
	Vec2 bulletSpawnPos = m_position + fwrdNormal * m_bulletSpawnOffset;
	Entity* bullet = m_map->SpawnNewEntity(bulletType, faction);
	bullet->m_position = bulletSpawnPos;
	bullet->m_orientationDegrees = fwrdNormal.GetOrientationDegrees();

	if (m_fireSFXAge >= SFX_PLAY_RATE)
	{
		m_map->SpawnExplosion(bulletSpawnPos, BULLET_FIRE_EXPLOSION_SIZE, BULLET_FIRE_EXPLOSION_DURATION);
		m_fireSFXAge = 0.f;

		switch (bulletType)
		{
		case ENTITY_TYPE_GOOD_BOLT: g_game->PlayGameSFX(BOLT_FIRED, m_position);
			break;
		case ENTITY_TYPE_GOOD_BULLET: g_game->PlayGameSFX(BULLET_FIRED, m_position);
			break;
		case ENTITY_TYPE_EVIL_BOLT: g_game->PlayGameSFX(BOLT_FIRED, m_position);
			break;
		case ENTITY_TYPE_EVIL_BULLET: g_game->PlayGameSFX(BULLET_FIRED, m_position);
			break;
		case ENTITY_TYPE_EVIL_SHELL: g_game->PlayGameSFX(BULLET_FIRED, m_position);
			break;
		case ENTITY_TYPE_EVIL_BOUNCING_BOLT: g_game->PlayGameSFX(BOLT_FIRED, m_position);
			break;
		}
	}

	m_fireCountdownTime = m_fireRate;

	return;
}

//Health
//----------------------------------------------------------------------
void Entity::ReplenishHealth()
{
	m_health = m_maxHealth;
}

void Entity::LoseHealth(float amount)
{
	if (m_isDead)
		return;

	m_health -= amount;
	if (m_damageSFXAge >= SFX_PLAY_RATE)
	{
		g_game->PlayGameSFX(ENEMY_DAMAGED, m_position);
		m_damageSFXAge = 0.f;
	}

	if (m_health <= 0)
	{
		Die();
	}
}

void Entity::Die()
{
	m_isGarbage = true;
	m_isDead = true;
	m_map->SpawnExplosion(m_position, DEATH_EXPLOSION_SIZE, DEATH_EXPLOSION_DURATION);
	g_game->PlayGameSFX(ENEMY_KILLED, m_position);

	if (m_isDebugTrackedEntity)
	{
		FireEvent("ChangeTrackedLeo");
	}
}

//Helpers
//----------------------------------------------------------------------
void Entity::DebugRender() const
{

	std::vector<Vertex_PCU> debugVerts;

	//Physics Ring
	AddVertsForRing2D(debugVerts, m_position, m_physicsRadius, m_debugLineThickness, Rgba8(0, 255, 255));

	//Forward Vector
	Vec2 vecFwrd = GetForwardNormal() * m_cosmeticRadius;
	AddVertsForLineSegment2D(debugVerts, m_position, m_position + vecFwrd, m_debugLineThickness, Rgba8(255, 0, 0));

	//Left Vector
	Vec2 vecLeft = vecFwrd.GetRotated90Degrees();
	AddVertsForLineSegment2D(debugVerts, m_position, m_position + vecLeft, m_debugLineThickness, Rgba8(0, 255, 0));

	//Velocity Line
	AddVertsForLineSegment2D(debugVerts, m_position, m_position + m_velocity, m_debugLineThickness, Rgba8(255, 255, 0));

	g_renderer->BindTexture(nullptr);
	g_renderer->DrawVertexArray(debugVerts);

}

void Entity::TurnTowardsPosition(Vec2 const& targetPos, float maxTurnDegrees)
{
	Vec2 meToTargetDisp = targetPos - m_position;
	float orientToTarget = meToTargetDisp.GetOrientationDegrees();
	m_orientationDegrees = GetTurnedTowardDegrees(m_orientationDegrees, orientToTarget, maxTurnDegrees);
}

void Entity::AddVertsForHealthBar(std::vector<Vertex_PCU>& verts) const
{
	Vec2 healthBarCenter(m_position.x, m_position.y + 0.35f);
	LineSegment2 healthBarLine(healthBarCenter, Vec2::ONE_TO_ZERO, 0.5f);
	AddVertsForLineSegment2D(verts, healthBarLine, 0.07f, Rgba8::RED);

	float xPos = RangeMapClamped(static_cast<float>(m_health), 0.f, static_cast<float>(m_maxHealth), healthBarLine.m_start.x, healthBarLine.m_end.x);
	AddVertsForLineSegment2D(verts, healthBarLine.m_start, Vec2(xPos, healthBarLine.m_end.y), 0.07f, Rgba8::GREEN);
}

//Accessors
//----------------------------------------------------------------------
bool Entity::IsOnTileAdjacentToPlayer() const
{
	Vec2 playerPos = m_map->m_game->m_player->m_position;
	IntVec2 playerTileCoords = m_map->GetTileCoordsFromPosition(playerPos);
	IntVec2 currentTileCoords = m_map->GetTileCoordsFromPosition(m_position);
	if (GetDistanceSquared2D(m_map->GetTileCenterPosFromTileCoords(playerTileCoords), m_map->GetTileCenterPosFromTileCoords(currentTileCoords)) <= 1.25f * 1.25f)
	{
		return true;
	}

	return false;
}

bool Entity::CanTravelToPlayer() const
{
	if(m_solidMap == nullptr)
		return true;

	Vec2 playerPos = g_game->m_player->m_position;
	int tileIndex = m_map->GetTileIndexFromPosition(playerPos);
	float tileSolidMapValue = m_solidMap->GetValue(tileIndex);

	return tileSolidMapValue != DEFAULT_HEAT_MAP_SOLID_VALUE;
}

bool Entity::CanSeePlayer(float sightRange) const
{
	Entity* player = g_game->m_player;
	if (!player->IsAlive())
		return false;

	return m_map->HasLineOfSight(m_position, player->m_position, sightRange);
}

Vec2 const Entity::GetForwardNormal() const
{
	return Vec2::MakeFromPolarDegrees(m_orientationDegrees, 1.f);
}

Texture* Entity::GetTexture() const
{
	return m_texture;
}

bool Entity::IsOnTargetTile(Vec2 const& currentPos, Vec2 const& targetPos) const
{
	IntVec2 currentTileCoords = m_map->GetTileCoordsFromPosition(currentPos);
	IntVec2 targetTileCoords = m_map->GetTileCoordsFromPosition(targetPos);
	return currentTileCoords == targetTileCoords;
}

bool Entity::IsOnTargetTile(Vec2 const& currentPos, IntVec2 const& targetTileCoords) const
{
	IntVec2 currentTileCoords = m_map->GetTileCoordsFromPosition(currentPos);
	return currentTileCoords == targetTileCoords;
}

bool Entity::IsTileAccessible(IntVec2 const& tileCoords) const
{
	int tileIndex = m_map->GetTileIndexFromTileCoords(tileCoords);
	return m_solidMap->m_values[tileIndex] != DEFAULT_HEAT_MAP_SOLID_VALUE;
}

bool const Entity::DidChangeTile() const
{
	IntVec2 lastTileCoords = m_map->GetTileCoordsFromPosition(m_positionLastFrame);
	IntVec2 currentTileCoords = m_map->GetTileCoordsFromPosition(m_position);
	return lastTileCoords != currentTileCoords;
}

bool const Entity::IsEntityBullet(EntityType entityType) const
{
	switch (entityType)
	{
	case ENTITY_TYPE_GOOD_BULLET: return true;
	case ENTITY_TYPE_GOOD_BOLT: return true;
	case ENTITY_TYPE_EVIL_BULLET: return true;
	case ENTITY_TYPE_EVIL_BOLT: return true;
	case ENTITY_TYPE_EVIL_BOUNCING_BOLT: return true;
	case ENTITY_TYPE_EVIL_SHELL: return true;
	case ENTITY_TYPE_GOOD_FLAME_BULLET: return true;
	default: return false;
	}
	
}
