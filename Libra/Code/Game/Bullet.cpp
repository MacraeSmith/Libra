#include "Game/Bullet.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Map.hpp"
#include "Game/Game.hpp"
#include "Engine/Renderer/RendererDX11.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/SpriteAnimDefinition.hpp"



Bullet::Bullet(Map* const& mapOwner, EntityType entityType, EntityFaction faction)
	:Entity(mapOwner, entityType, faction)
{
	UpdateGameConfigXmlData();
	CreateTexture();
	m_isPushedByWalls = false;
	m_isPushedByEntities = false;
	m_doesPushEntities = false;
	m_isHitByBullets = false;
	m_health = 1;
	m_physicsRadius = 0.f;

	if (entityType == ENTITY_TYPE_EVIL_SHELL)
	{
		m_isTrackingBullet = true;
	}

	else if (entityType == ENTITY_TYPE_GOOD_FLAME_BULLET)
	{
		m_isFlameBullet = true;
		m_physicsRadius = 0.25f;
		m_numBounces = 0;
		m_flameOrientation = g_rng->RollRandomFloatInRange(0.f, 360.f);
	}
}

Bullet::~Bullet()
{
	delete(m_spriteAnimDef);
	m_spriteAnimDef = nullptr;
}

void Bullet::Update(float deltaSeconds)
{
	m_velocity = GetForwardNormal() * m_moveSpeed * deltaSeconds;
	Vec2 futurePos = m_position + m_velocity;

	if (m_isTrackingBullet)
	{
		TurnTowardsPlayer(m_turnSpeed * deltaSeconds);
	}

	else if (m_isFlameBullet)
	{
		LoseHealth(m_decayRate * deltaSeconds);
		m_flameOrientation += m_flameRotationSpeed * deltaSeconds;
	}

	if (m_map->WillBulletHitSolid(this, futurePos))
	{
		if (m_numBounces <= 0)
		{
			Die();
		}

		//Bounce off wall
		else
		{
			IntVec2 tileCoords = m_map->GetTileCoordsFromPosition(futurePos);
			AABB2 tileBounds = m_map->GetTileBoundsFromTileCoords(tileCoords);
			Vec2 nearestPoint = tileBounds.GetNearestPoint(m_position);
			Vec2 surfaceNormal = (m_position - nearestPoint).GetNormalized();
			BounceOffSurfaceNormal(surfaceNormal);
			g_game->PlayGameSFX(BULLET_BOUNCE, m_position);
		}
	}

	else
	{
		m_position = futurePos;
	}
}

void Bullet::Render() const
{
	Vec2 fwrdNormal;
	std::vector<Vertex_PCU> worldVerts;

	if (!m_isFlameBullet)
	{
		fwrdNormal = GetForwardNormal();
		AddVertsForAABB2D(worldVerts, m_entityBounds, Rgba8::WHITE);
		TransformVertexArrayXY3D(worldVerts, fwrdNormal, fwrdNormal.GetRotated90Degrees(), m_position);
		g_renderer->SetBlendMode(BlendMode::ALPHA);
	}

	else if (m_spriteAnimDef != nullptr)
	{
		fwrdNormal = Vec2::MakeFromPolarDegrees(m_flameOrientation);
		SpriteDefinition spriteDef = m_spriteAnimDef->GetSpriteDefAtTime(RangeMapClamped(m_health, 1.f, 0.f, 0.f, m_decayRate));
		AddVertsForAABB2D(worldVerts, m_entityBounds, Rgba8::WHITE, spriteDef.GetUVS());
		TransformVertexArrayXY3D(worldVerts, fwrdNormal, fwrdNormal.GetRotated90Degrees(), m_position);
		g_renderer->SetBlendMode(BlendMode::ADDITIVE);
	}

	g_renderer->BindTexture(m_texture);
	g_renderer->DrawVertexArray(worldVerts);
	
}

void Bullet::DebugRender() const
{
	std::vector<Vertex_PCU> debugVerts;
	if (m_isFlameBullet)
	{
		//Physics Ring
		AddVertsForRing2D(debugVerts, m_position, m_physicsRadius, m_debugLineThickness, Rgba8(0, 255, 255));
	}

	else
	{
		AddVertsForDisc2D(debugVerts, m_position, 0.05f, Rgba8::WHITE);
	}
	

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(nullptr);
	g_renderer->DrawVertexArray(debugVerts);
}

void Bullet::LoseHealth(float amount)
{
	m_health -= amount;
	if (m_health <= 0)
	{
		Die();
	}
}

void Bullet::Die()
{
	m_isGarbage = true;
	m_isDead = true;
	Vec2 fwrdNormal = GetForwardNormal();
	if (!m_isFlameBullet)
	{
		m_map->SpawnExplosion(m_position + (fwrdNormal * m_bulletLength * 0.45f) , BULLET_DEATH_EXPLOSION_SIZE, DEATH_EXPLOSION_DURATION);
	}
}

void Bullet::UpdateGameConfigXmlData()
{
	switch (m_entityType)
	{
	case ENTITY_TYPE_GOOD_BULLET:
		m_moveSpeed = g_gameConfigBlackboard.GetValue("defaultBulletSpeed", 4.f);
		m_damage = g_gameConfigBlackboard.GetValue("bulletDamage", 1.f);
		break;
	case ENTITY_TYPE_GOOD_BOLT:
		m_moveSpeed = g_gameConfigBlackboard.GetValue("defaultBoltSpeed", 6.f);
		m_damage = g_gameConfigBlackboard.GetValue("boltDamage", 1.f);
		m_numBounces = 2;
		break;
	case ENTITY_TYPE_EVIL_BULLET:
		m_moveSpeed = g_gameConfigBlackboard.GetValue("defaultBulletSpeed", 4.f);
		m_damage = g_gameConfigBlackboard.GetValue("bulletDamage", 1.f);
		break;
	case ENTITY_TYPE_EVIL_BOLT:
		m_moveSpeed = g_gameConfigBlackboard.GetValue("defaultBoltSpeed", 6.f);
		m_damage = g_gameConfigBlackboard.GetValue("boltDamage", 1.f);
		break;
	case ENTITY_TYPE_EVIL_BOUNCING_BOLT:
		m_moveSpeed = g_gameConfigBlackboard.GetValue("defaultBoltSpeed", 6.f);
		m_damage = g_gameConfigBlackboard.GetValue("boltDamage", 1.f);
		m_numBounces = 1;
		break;
	case ENTITY_TYPE_EVIL_SHELL:
		m_moveSpeed = g_gameConfigBlackboard.GetValue("defaultShellSpeed", 2.f);
		m_turnSpeed = g_gameConfigBlackboard.GetValue("trackingBulletTurnSpeed", 180.f);
		m_damage = g_gameConfigBlackboard.GetValue("shellDamage", 3.f);
		break;
	case ENTITY_TYPE_GOOD_FLAME_BULLET:
		m_moveSpeed = g_gameConfigBlackboard.GetValue("defaultFlameSpeed", 0.25f);
		m_damage = g_gameConfigBlackboard.GetValue("flameDamage", 0.25f);
		m_decayRate = g_gameConfigBlackboard.GetValue("flameDecayRate", 1.f);

		Vec2 turnSpeedRange = g_gameConfigBlackboard.GetValue("flameTurnSpeedRange", Vec2(100.f, 500.f));
		m_flameRotationSpeed = g_rng->RollRandomFloatInRange(turnSpeedRange.x, turnSpeedRange.y);
		if (static_cast<int>(m_flameRotationSpeed) % 2)
		{
			m_flameRotationSpeed *= -1.f;
		}

		break;
	}
}

void Bullet::BounceOffSurfaceNormal(Vec2 const& surfaceNormal)
{
	Vec2 reflectedDirection = m_velocity.GetReflected(surfaceNormal);
	m_orientationDegrees = reflectedDirection.GetOrientationDegrees();
	m_numBounces--;
}

void Bullet::CreateTexture()
{
	switch (m_entityType)
	{
	case ENTITY_TYPE_GOOD_BULLET:
		m_texture = g_renderer->CreateOrGetTextureFromFile("Data/Images/Bullets/FriendlyBullet.png");
		m_bulletLength = g_gameConfigBlackboard.GetValue("defaultBulletVertsSizeX", 0.25f);
		break;
	case ENTITY_TYPE_GOOD_BOLT:
		m_texture = g_renderer->CreateOrGetTextureFromFile("Data/Images/Bullets/FriendlyBolt.png");
		m_bulletLength = g_gameConfigBlackboard.GetValue("defaultBulletVertsSizeX", 0.25f);
		break;
	case ENTITY_TYPE_EVIL_BULLET:
		m_texture = g_renderer->CreateOrGetTextureFromFile("Data/Images/Bullets/EnemyBullet.png");
		m_bulletLength = g_gameConfigBlackboard.GetValue("defaultBulletVertsSizeX", 0.25f);
		break;
	case ENTITY_TYPE_EVIL_BOLT:
		m_texture = g_renderer->CreateOrGetTextureFromFile("Data/Images/Bullets/EnemyBolt.png");
		m_bulletLength = g_gameConfigBlackboard.GetValue("defaultBulletVertsSizeX", 0.25f);
		break;
	case ENTITY_TYPE_EVIL_BOUNCING_BOLT:
		m_texture = g_renderer->CreateOrGetTextureFromFile("Data/Images/Bullets/EnemyBolt.png");
		m_bulletLength = g_gameConfigBlackboard.GetValue("defaultBulletVertsSizeX", 0.25f);
		break;
	case ENTITY_TYPE_EVIL_SHELL:
		m_texture = g_renderer->CreateOrGetTextureFromFile("Data/Images/Bullets/EnemyShell.png");
		m_bulletLength = g_gameConfigBlackboard.GetValue("defaultBulletVertsSizeX", 0.25f);
		break;
	case ENTITY_TYPE_GOOD_FLAME_BULLET:
		m_texture = g_renderer->CreateOrGetTextureFromFile(g_gameConfigBlackboard.GetValue("explosionSpriteSheetTexture", "").c_str());
		m_bulletLength = 1.f;
		SpriteSheet* spriteSheet = m_map->m_explosionSpriteSheet;
		int endIndex = spriteSheet->GetNumSprites() - 1;
		m_spriteAnimDef = new SpriteAnimDefinition(*spriteSheet, 0, endIndex, endIndex / m_decayRate, ONCE);
		break;
	}

	IntVec2 textureDims = m_texture->GetDimensions();
	float xDims = static_cast<float>(textureDims.x);
	float yDims = static_cast<float>(textureDims.y);
	float xToYAspect = yDims / xDims;

	Vec2 convertedDims(m_bulletLength, m_bulletLength * xToYAspect);
	float xOffset = convertedDims.x * 0.5f;
	float yOffset = convertedDims.y * 0.5f;
	m_entityBounds = AABB2(-xOffset, -yOffset, xOffset, yOffset);
	AddVertsForAABB2D(m_shapeVerts, m_entityBounds, Rgba8::WHITE);
}

void Bullet::TurnTowardsPlayer(float maxTurnDegrees)
{
	Entity* player = g_game->m_player;
	float orientToPlayer = (player->m_position - m_position).GetOrientationDegrees();
	m_orientationDegrees = GetTurnedTowardDegrees(m_orientationDegrees, orientToPlayer, maxTurnDegrees);
}
