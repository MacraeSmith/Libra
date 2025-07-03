#include "Game/Scorpio.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Map.hpp"
#include "Game/Game.hpp"
#include "Game/Player.hpp"
#include "Engine/Renderer/RendererDX11.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/LineSegment2.hpp"

Scorpio::Scorpio(Map* const& mapOwner, EntityFaction faction)
	:Entity(mapOwner, ENTITY_TYPE_EVIL_SCORPIO, faction)
{
	UpdateGameConfigXmlData();
	m_isPushedByEntities = false;
	m_isPushedByWalls = false;
	CreateTexture();
}

void Scorpio::Update(float deltaSeconds)
{
	UpdateTimers(deltaSeconds);

	if (CanSeePlayer(m_sightRange))
	{
		m_chasingPlayerLocation = true;
		Vec2 dispToPlayer = g_game->m_player->m_position - m_position;
		float orientToPlayer = (dispToPlayer).GetOrientationDegrees();
		m_orientationDegrees = GetTurnedTowardDegrees(m_orientationDegrees, orientToPlayer, m_turretTurnSpeed * deltaSeconds);

		Vec2 fwrdNormal = GetForwardNormal();
		TryShootBullet(ENTITY_TYPE_EVIL_BOLT, FACTION_EVIL, fwrdNormal);
	}

	else
	{
		m_orientationDegrees += m_turretTurnSpeed * deltaSeconds;
	}

	m_laserRay.m_startPos = m_position;
	m_laserRay.m_fwrdNormal = GetForwardNormal();
	m_laserRay.m_maxLength = m_laserMaxLength;

	RaycastResult2D laserRayResult = m_map->RaycastVsTiles(m_laserRay);
	if (laserRayResult.m_didImpact)
	{
		m_laserHitPos = laserRayResult.m_impactPos;
		m_laserLengthFraction = RangeMapClamped(laserRayResult.m_impactDistance, 0.f, m_laserMaxLength, 0.f, 1.f);
	}
	else
	{
		m_laserHitPos = m_position + (m_laserRay.m_fwrdNormal * m_laserMaxLength);
		m_laserLengthFraction = 1.f;
	}

}

void Scorpio::Render() const
{
	Verts worldVerts;
	AddVertsForAABB2D(worldVerts, m_entityBounds, Rgba8::WHITE);

	TransformVertexArrayXY3D(worldVerts, Vec2::ZERO_TO_ONE, Vec2::ONE_TO_ZERO, m_position);

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_renderer->BindTexture(m_texture);
	g_renderer->DrawVertexArray(worldVerts);

	Vec2 fwrdNormal = GetForwardNormal();

	worldVerts.clear();
	unsigned char alphaByte = static_cast<unsigned char>(Lerp(255.f, 0.f, m_laserLengthFraction));
	AddVertsForLineSegment2D(worldVerts, m_position, m_laserHitPos, 0.05f, Rgba8::RED, Rgba8(255, 0,0, alphaByte));

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(nullptr);
	g_renderer->DrawVertexArray(worldVerts);

	worldVerts.clear();
	AddVertsForAABB2D(worldVerts, m_turretBounds, Rgba8::WHITE);
	TransformVertexArrayXY3D(worldVerts, fwrdNormal, fwrdNormal.GetRotated90Degrees(), m_position);

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(m_turretTexture);
	g_renderer->DrawVertexArray(worldVerts);
}

void Scorpio::Die()
{
	m_isGarbage = true;
	m_isDead = true;
	g_game->PlayGameSFX(ENEMY_KILLED, m_position);
	m_map->SpawnExplosion(m_position, DEATH_EXPLOSION_SIZE, DEATH_EXPLOSION_DURATION);

	//Update entities solid map now that this scorpio is no longer blocking travel
	FireEvent("RegenerateSolidMapsForMobileEntities");
}

void Scorpio::UpdateGameConfigXmlData()
{
	m_physicsRadius = g_gameConfigBlackboard.GetValue("scorpioPhysicsRadius", 0.4f);
	m_turretTurnSpeed = g_gameConfigBlackboard.GetValue("scorpioTurretTurnRate", 30.f);
	m_sightRange = g_gameConfigBlackboard.GetValue("scorpioSightRange", 10.f);
	m_laserMaxLength = m_sightRange;
	m_fireRate = g_gameConfigBlackboard.GetValue("scorpioFireRate", 0.3f);
	m_bulletSpawnOffset = g_gameConfigBlackboard.GetValue("scorpioBulletSpawnOffset", 0.4f);
	m_fireAperture = g_gameConfigBlackboard.GetValue("scorpioFireAperture", 10.f);
	m_orientationDegrees = g_rng->RollRandomFloatInRange(0.f, 360.f);
	m_maxHealth = g_gameConfigBlackboard.GetValue("scorpioStartingHealth", 20.f);
	m_health = m_maxHealth;
}


void Scorpio::CreateTexture()
{
	m_entityBounds = AABB2(-0.5f, -0.5f, 0.5f, 0.5f);
	m_texture = g_renderer->CreateOrGetTextureFromFile(g_gameConfigBlackboard.GetValue("scorpioTexturePath", "").c_str());
	AddVertsForAABB2D(m_shapeVerts, m_entityBounds, Rgba8::WHITE);

	m_turretBounds = m_entityBounds;
	m_turretTexture = g_renderer->CreateOrGetTextureFromFile(g_gameConfigBlackboard.GetValue("scorpioTurretTexturePath", "").c_str());
}
