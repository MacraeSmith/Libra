#include "Game/Leo.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Game.hpp"
#include "Game/Map.hpp"
#include "Game/Player.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Renderer/RendererDX11.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/TileHeatMap.hpp"


Leo::Leo(Map* const& mapOwner, EntityFaction faction)
	:Entity(mapOwner, ENTITY_TYPE_EVIL_LEO, faction)
{
	UpdateGameConfigXmlData();
	CreateTexture();
	m_roamDistanceMap = new TileHeatMap(m_map->m_dimensions, DEFAULT_HEAT_MAP_SOLID_VALUE);
	m_solidMap = new TileHeatMap(m_map->m_dimensions, DEFAULT_HEAT_MAP_SOLID_VALUE);
}

void Leo::Update(float deltaSeconds)
{
	m_positionLastFrame = m_position;
	UpdateTimers(deltaSeconds);
	Vec2 fwrdNormal = UpdateEntityPathFinding(deltaSeconds);
	TryShootBullet(ENTITY_TYPE_EVIL_BULLET, FACTION_EVIL, fwrdNormal);
}

void Leo::Render() const
{
	Vec2 fwrdNormal = GetForwardNormal();
	std::vector<Vertex_PCU> worldVerts;
	AddVertsForAABB2D(worldVerts, m_entityBounds, Rgba8::WHITE);

	TransformVertexArrayXY3D(worldVerts, fwrdNormal, fwrdNormal.GetRotated90Degrees(), m_position);

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(m_texture);
	g_renderer->DrawVertexArray(worldVerts);	
}

void Leo::DebugRender() const
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

void Leo::UpdateGameConfigXmlData()
{
	m_physicsRadius = g_gameConfigBlackboard.GetValue("leoPhysicsRadius", 0.35f);
	m_maxHealth = g_gameConfigBlackboard.GetValue("leoStartingHealth", 7.f);
	m_health = m_maxHealth;
	m_sightRange = g_gameConfigBlackboard.GetValue("leoSightRange", 5.f);
	m_turnSpeed = g_gameConfigBlackboard.GetValue("leoTurnRate", 45.f);
	m_moveSpeed = g_gameConfigBlackboard.GetValue("leoDriveSpeed", 0.25f);
	m_fireAperture = g_gameConfigBlackboard.GetValue("leoFireAperture", 5.f);
	m_driveAperture = g_gameConfigBlackboard.GetValue("leoDriveAperture", 45.f);
	m_bulletSpawnOffset = g_gameConfigBlackboard.GetValue("leoBulletSpawnOffset", 0.2f);
	m_fireRate = g_gameConfigBlackboard.GetValue("leoFireRate", 2.f);
	m_canTraverseWater = false;
}


void Leo::DebugRenderPathFindingInfo() const
{
	std::vector<Vertex_PCU> debugVerts;
	AddVertsForRing2D(debugVerts,m_position, m_physicsRadius + 0.25f, m_debugLineThickness * 2.f, Rgba8::YELLOW);
	//Next Waypoint	
	AddVertsForLineSegment2D(debugVerts, m_position, m_nextWaypointPos, m_debugLineThickness, Rgba8::BLUE);
	AddVertsForDisc2D(debugVerts, m_nextWaypointPos, 0.05f, Rgba8::GREEN);

	//TargetPos
	AddVertsForDisc2D(debugVerts, m_targetPos, 0.1f, Rgba8::YELLOW);
	
	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(nullptr);
	g_renderer->DrawVertexArray(debugVerts);
}

void Leo::CreateTexture()
{
	m_entityBounds = AABB2(-0.5f, -0.5f, 0.5f, 0.5f);
	m_texture = g_renderer->CreateOrGetTextureFromFile(g_gameConfigBlackboard.GetValue("leoTexturePath","").c_str());
	AddVertsForAABB2D(m_shapeVerts, m_entityBounds, Rgba8::WHITE);
}
