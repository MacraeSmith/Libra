#include "Game/Gemini.hpp"
#include "Game/Map.hpp"
#include "Game/Game.hpp"
#include "Engine/Renderer/RendererDX11.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/LineSegment2.hpp"
#include "Engine/Core/TileHeatMap.hpp"


Gemini::Gemini(Map* const& mapOwner, EntityType type, EntityFaction faction)
	:Entity(mapOwner, type, faction)
{
	UpdateGameConfigXmlData();
	CreateTexture();
	m_roamDistanceMap = new TileHeatMap(m_map->m_dimensions, DEFAULT_HEAT_MAP_SOLID_VALUE);
	m_solidMap = new TileHeatMap(m_map->m_dimensions, DEFAULT_HEAT_MAP_SOLID_VALUE);

	if (type == ENTITY_TYPE_EVIL_GEMINI_BROTHER)
	{
		Entity* newGemini = m_map->SpawnNewEntity(ENTITY_TYPE_EVIL_GEMINI_SISTER, faction);
		m_twinEntity = dynamic_cast<Gemini*>(newGemini);
		m_twinEntity->m_twinEntity = this;
	}

}

void Gemini::Update(float deltaSeconds)
{
	m_positionLastFrame = m_position;
	UpdateTimers(deltaSeconds);
	UpdateEntityPathFinding(deltaSeconds);
}

void Gemini::Render() const
{
	//body
	Vec2 fwrdNormal = GetForwardNormal();
	std::vector<Vertex_PCU> worldVerts;
	AddVertsForAABB2D(worldVerts, m_entityBounds, Rgba8::WHITE);

	TransformVertexArrayXY3D(worldVerts, fwrdNormal, fwrdNormal.GetRotated90Degrees(), m_position);

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(m_texture);
	g_renderer->DrawVertexArray(worldVerts);

	//Laser
	worldVerts.clear();
	AddVertsForLineSegment2D(worldVerts, m_laserStartPos, m_raycastResult.m_impactPos, 0.05f, Rgba8(200, 100, 255, 200));
	AddVertsForLineSegment2D(worldVerts, m_laserStartPos, m_raycastResult.m_impactPos, 0.1f, Rgba8(200, 0, 255, 100));

	g_renderer->SetBlendMode(BlendMode::ADDITIVE);
	g_renderer->BindTexture(nullptr);
	g_renderer->DrawVertexArray(worldVerts);
	
	//turret
	worldVerts.clear();
	fwrdNormal = Vec2::MakeFromPolarDegrees(m_turretOrientation);
	AddVertsForAABB2D(worldVerts, m_turretBounds, Rgba8::WHITE);
	TransformVertexArrayXY3D(worldVerts, fwrdNormal, fwrdNormal.GetRotated90Degrees(), m_position);

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(m_turretTexture);
	g_renderer->DrawVertexArray(worldVerts);
}

void Gemini::Die()
{
	m_isGarbage = true;
	m_isDead = true;
	g_game->PlayGameSFX(ENEMY_KILLED, m_position);
	m_map->SpawnExplosion(m_position, DEATH_EXPLOSION_SIZE, DEATH_EXPLOSION_DURATION);

	if (m_twinEntity && m_twinEntity->IsAlive())
	{
		m_twinEntity->Die();
	}
}

void Gemini::UpdateGameConfigXmlData()
{
	m_physicsRadius = g_gameConfigBlackboard.GetValue("geminiPhysicsRadius", 0.35f);
	m_turnSpeed = g_gameConfigBlackboard.GetValue("geminiTurnRate", 45.f);
	m_bulletSpawnOffset = g_gameConfigBlackboard.GetValue("geminiBulletSpawnOffset", 0.4f);
	m_driveAperture = g_gameConfigBlackboard.GetValue("geminiDriveAperture", 45.f);
	m_moveSpeed = g_gameConfigBlackboard.GetValue("geminiDriveSpeed", 0.5f);
	m_orientationDegrees = g_rng->RollRandomFloatInRange(0.f, 360.f);
	m_maxHealth = g_gameConfigBlackboard.GetValue("geminiStartingHealth", 20.f);
	m_health = m_maxHealth;
	m_damage = g_gameConfigBlackboard.GetValue("geminiLaserDamage", 1.f);
	m_sightRange = g_gameConfigBlackboard.GetValue("geminiSightRange", 10.f);
	m_canTraverseWater = true;
}

Vec2 const Gemini::UpdateEntityPathFinding(float deltaSeconds)
{
	Vec2 fwrdNormal = GetForwardNormal();
	SetNextWaypoint(fwrdNormal);

	//Rotate towards waypoint
	Vec2 dispToNextWaypoint = m_nextWaypointPos - m_position;
	m_orientationDegrees = GetTurnedTowardDegrees(m_orientationDegrees, dispToNextWaypoint.GetOrientationDegrees(), m_turnSpeed * deltaSeconds);
	fwrdNormal = GetForwardNormal(); //update forward normal after rotation

	float angleToWaypoint = GetAngleDegreesBetweenVectors2D(dispToNextWaypoint, fwrdNormal);

	//Update position if within drive apeture
	if (angleToWaypoint <= m_driveAperture && angleToWaypoint >= -m_driveAperture)
	{
		m_velocity = fwrdNormal * m_moveSpeed * deltaSeconds;
		m_position += m_velocity;
	}

	//turret orientation
	Vec2 dispToTwin = m_twinEntity->m_position - m_position;
	float angleToTwin = dispToTwin.GetOrientationDegrees();
	m_turretOrientation = GetTurnedTowardDegrees(m_turretOrientation, angleToTwin, 360.f);
	Vec2 turretFwrdNormal = Vec2::MakeFromPolarDegrees(m_turretOrientation);
	m_laserStartPos = m_position + (turretFwrdNormal * m_bulletSpawnOffset);

	//Raycast to twin
	Ray2 laserRay;
	laserRay.m_startPos = m_laserStartPos;
	laserRay.m_fwrdNormal = turretFwrdNormal;
	Vec2 dispToOtherBulletOffset = m_twinEntity->m_laserStartPos - m_laserStartPos;
	laserRay.m_maxLength = dispToOtherBulletOffset.GetLength();
	m_raycastResult = m_map->RaycastVsTiles(laserRay);

	//Raycast to player
	Entity* player = g_game->m_player;
	RaycastResult2D hitPlayerResult = RaycastVsDisc2D(m_position, turretFwrdNormal, m_raycastResult.m_impactDistance, player->m_position, player->m_physicsRadius);
	if (hitPlayerResult.m_didImpact)
	{
		player->LoseHealth(m_damage * deltaSeconds);
	}


	//Update waypoint position whenever entity arrives at the next one
	if (IsOnTargetTile(m_position, m_nextWaypointPos))
	{
		SetNextWaypoint(fwrdNormal);
	}

	if (IsOnTargetTile(m_position, m_targetPos) || m_pathToTarget.size() < 1)
	{
		m_targetPos = m_map->GetRandomTraversablePosFromSolidMap(m_solidMap);
		m_map->PopulateDistanceMapWithStationaryEntities(*m_roamDistanceMap, IntVec2(m_targetPos), DEFAULT_HEAT_MAP_SOLID_VALUE, !m_canTraverseWater);
		m_map->GenerateEntityPathToTargetPos(m_pathToTarget, *m_roamDistanceMap, m_position);
		m_nextWaypointPos = m_pathToTarget.back();
	}

	return fwrdNormal;
}

void Gemini::CreateTexture()
{
	m_entityBounds = AABB2(-0.5f, -0.5f, 0.5f, 0.5f);
	m_texture = g_renderer->CreateOrGetTextureFromFile(g_gameConfigBlackboard.GetValue("geminiTexturePath", "").c_str());
	AddVertsForAABB2D(m_shapeVerts, m_entityBounds, Rgba8::WHITE);

	m_turretBounds = m_entityBounds;
	m_turretTexture = g_renderer->CreateOrGetTextureFromFile(g_gameConfigBlackboard.GetValue("geminiTurretTexturePath", "").c_str());
}
