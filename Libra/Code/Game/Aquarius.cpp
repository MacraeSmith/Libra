#include "Game/Aquarius.hpp"
#include"Game/GameCommon.hpp"
#include "Game/Game.hpp"
#include "Game/Map.hpp"
#include "Game/Tile.hpp"
#include "Game/TileDefinition.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Renderer/RendererDX11.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include"Engine/Math/MathUtils.hpp"
#include "Engine/Core/TileHeatMap.hpp"
#include "Engine/Core/Time.hpp"

Aquarius::Aquarius(Map* const& mapOwner, EntityFaction faction)
	:Entity(mapOwner, ENTITY_TYPE_EVIL_AQUARIUS, faction)
{
	UpdateGameConfigXmlData();
	CreateTexture();
	m_roamDistanceMap = new TileHeatMap(m_map->m_dimensions, DEFAULT_HEAT_MAP_SOLID_VALUE);
	m_solidMap = new TileHeatMap(m_map->m_dimensions, DEFAULT_HEAT_MAP_SOLID_VALUE);
}

void Aquarius::Update(float deltaSeconds)
{
	UpdateTimers(deltaSeconds);
	m_positionLastFrame = m_position;

	Vec2 fwrdNormal = UpdateEntityPathFinding(deltaSeconds);

	TryShootBullet(ENTITY_TYPE_EVIL_BOUNCING_BOLT, FACTION_EVIL, fwrdNormal);

	if (DidChangeTile())
	{
		m_map->AddOverrideTileAtPos(m_position, "Water", m_trailAge);
	}
}

Vec2 const Aquarius::UpdateEntityPathFinding(float deltaSeconds)
{
	//Change target based on sight to player
	Vec2 playerPos = g_game->m_player->m_position;
	if (m_map->HasLineOfSight(m_position, playerPos, m_sightRange, *m_solidMap))
	{
		m_chasingPlayerLocation = true;
		m_map->GenerateEntityPathToTargetPos(m_pathToTarget, *m_map->m_amphibianDistanceMapToPlayer, m_position, static_cast<int>(m_sightRange));
		m_targetPos = m_pathToTarget.front();
	}


	SetNextWaypoint(GetForwardNormal());

	Vec2 fwrdNormal;

	if (IsOnTileAdjacentToPlayer())
	{
		m_targetPos = playerPos;
		m_nextWaypointPos = m_targetPos;
		m_isPushedByEntities = false;

		//Does not move if next to player. This is to keep him from turning the tile underneath the player into water
		Vec2 dispToNextWaypoint = m_nextWaypointPos - m_position;
		m_orientationDegrees = GetTurnedTowardDegrees(m_orientationDegrees, dispToNextWaypoint.GetOrientationDegrees(), m_turnSpeed * deltaSeconds);
		fwrdNormal = GetForwardNormal(); //update forward normal after rotation
	}

	else
	{
		m_isPushedByEntities = true;
		fwrdNormal = UpdatePositionAndOrientation(deltaSeconds);
	}

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

void Aquarius::Render() const
{
	Vec2 fwrdNormal = GetForwardNormal();
	std::vector<Vertex_PCU> worldVerts;
	AddVertsForAABB2D(worldVerts, m_entityBounds, Rgba8::WHITE);

	TransformVertexArrayXY3D(worldVerts, fwrdNormal, fwrdNormal.GetRotated90Degrees(), m_position);

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(m_texture);
	g_renderer->DrawVertexArray(worldVerts);
}

void Aquarius::DebugRender() const
{
	std::vector<Vertex_PCU> debugVerts;
	//Physics Ring
	DebugDrawRing(m_position, m_physicsRadius, m_debugLineThickness, Rgba8(0, 255, 255));

	//Forward Vector
	Vec2 vecFwrd = GetForwardNormal() * m_cosmeticRadius;
	AddVertsForLineSegment2D(debugVerts, m_position, m_position + vecFwrd, m_debugLineThickness, Rgba8(255, 0, 0));

	//Left Vector
	Vec2 vecLeft = vecFwrd.GetRotated90Degrees();
	AddVertsForLineSegment2D(debugVerts, m_position, m_position + vecLeft, m_debugLineThickness, Rgba8(0, 255, 0));

	//Velocity Line
	AddVertsForLineSegment2D(debugVerts, m_position, m_position + m_velocity, m_debugLineThickness, Rgba8(255, 255, 0));

	if (m_chasingPlayerLocation)
	{
		//Line to Target Pos
		AddVertsForLineSegment2D(debugVerts, m_position, m_targetPos, m_debugLineThickness, Rgba8(0, 0, 0, 100));
		AddVertsForDisc2D(debugVerts, m_targetPos, 0.05f, Rgba8::BLACK);
	}

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(nullptr);
	g_renderer->DrawVertexArray(debugVerts);
}

void Aquarius::UpdateGameConfigXmlData()
{
	m_physicsRadius = g_gameConfigBlackboard.GetValue("aquariusPhysicsRadius", 0.35f);
	m_moveSpeed = g_gameConfigBlackboard.GetValue("aquariusDriveSpeed", 0.5f);
	m_turnSpeed = g_gameConfigBlackboard.GetValue("aquariusTurnRate", 45.f);
	m_maxHealth = g_gameConfigBlackboard.GetValue("aquariusStartingHealth", 10.f);
	m_health = m_maxHealth;
	m_sightRange = g_gameConfigBlackboard.GetValue("aquariusSightRange", 7.f);
	m_driveAperture = g_gameConfigBlackboard.GetValue("aquariusDriveAperture", 90.f);
	m_trailAge = g_gameConfigBlackboard.GetValue("aquariusTrailAge", 10.f);
	m_fireAperture = g_gameConfigBlackboard.GetValue("aquariusFireAperture", 5.f);
	m_fireRate = g_gameConfigBlackboard.GetValue("aquariusFireRate", 2.f);
	m_bulletSpawnOffset = g_gameConfigBlackboard.GetValue("aquariusBulletSpawnOffset", 0.6f);
	m_canTraverseWater = true;
}

void Aquarius::CreateTexture()
{
	m_entityBounds = AABB2(-0.5f, -0.5f, 0.5f, 0.5f);
	m_texture = g_renderer->CreateOrGetTextureFromFile(g_gameConfigBlackboard.GetValue("aquariusTexturePath", "").c_str());
	AddVertsForAABB2D(m_shapeVerts, m_entityBounds, Rgba8::WHITE);
}


