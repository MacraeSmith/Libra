#include "Game/Aries.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Game.hpp"
#include "Game/Map.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Renderer/RendererDX11.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include"Engine/Math/MathUtils.hpp"
#include "Engine/Core/TileHeatMap.hpp"

Aries::Aries(Map* const& mapOwner, EntityFaction faction)
	:Entity(mapOwner, ENTITY_TYPE_EVIL_ARIES, faction)
{
	UpdateGameConfigXmlData();
	CreateTexture();
	m_roamDistanceMap = new TileHeatMap(m_map->m_dimensions, DEFAULT_HEAT_MAP_SOLID_VALUE);
	m_solidMap = new TileHeatMap(m_map->m_dimensions, DEFAULT_HEAT_MAP_SOLID_VALUE);
}

void Aries::Update(float deltaSeconds)
{
	m_positionLastFrame = m_position;
	UpdateTimers(deltaSeconds);
	UpdateEntityPathFinding(deltaSeconds);
}

void Aries::Render() const
{
	Vec2 fwrdNormal = GetForwardNormal();
	std::vector<Vertex_PCU> worldVerts;
	AddVertsForAABB2D(worldVerts, m_entityBounds, Rgba8::WHITE);

	TransformVertexArrayXY3D(worldVerts, fwrdNormal, fwrdNormal.GetRotated90Degrees(), m_position);

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(m_texture);
	g_renderer->DrawVertexArray(worldVerts);
}

void Aries::DebugRender() const
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

void Aries::UpdateGameConfigXmlData()
{
	m_physicsRadius = g_gameConfigBlackboard.GetValue("ariesPhysicsRadius", 0.35f);
	m_moveSpeed = g_gameConfigBlackboard.GetValue("ariesDriveSpeed", 0.5f);
	m_turnSpeed = g_gameConfigBlackboard.GetValue("ariesTurnRate", 45.f);
	m_maxHealth = g_gameConfigBlackboard.GetValue("ariesStartingHealth", 10.f);
	m_health = m_maxHealth;
	m_sightRange = g_gameConfigBlackboard.GetValue("ariesSightRange", 7.f);
	m_shieldAperture = g_gameConfigBlackboard.GetValue("ariesShieldAperture", 115.f);
	m_driveAperture = g_gameConfigBlackboard.GetValue("ariesDriveAperture", 90.f);
	m_canTraverseWater = false;
}


bool Aries::DidBulletHitShield(Vec2 const& bulletPos) const
{
	return IsPointInsideDirectedSector2D(bulletPos, m_position, GetForwardNormal(), m_shieldAperture, m_physicsRadius);
}

void Aries::CreateTexture()
{
	m_entityBounds = AABB2(-0.5f, -0.5f, 0.5f, 0.5f);
	m_texture = g_renderer->CreateOrGetTextureFromFile(g_gameConfigBlackboard.GetValue("ariesTexturePath", "").c_str());
	AddVertsForAABB2D(m_shapeVerts, m_entityBounds, Rgba8::WHITE);
}






