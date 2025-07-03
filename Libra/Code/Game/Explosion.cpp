#include "Game/Explosion.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Renderer/RendererDX11.hpp"
#include "Engine/Core/VertexUtils.hpp"

Explosion::Explosion(Map* const& mapOwner, Vec2 const& centerPos, AABB2 const& bounds, SpriteSheet const& spriteSheet, Rgba8 const& tint, SpriteAnimPlaybackType playbackType, int startIndex, int endIndex, float duration)
	:Entity(mapOwner, ENTITY_TYPE_NEUTRAL_EXPLOSION, FACTION_NEUTRAL, centerPos, g_rng->RollRandomFloatInRange(0.f, 360.f))
	,m_spriteAnimDef(SpriteAnimDefinition(spriteSheet, startIndex, endIndex, endIndex / duration, playbackType))
	,m_duration(duration)
	,m_tint(tint)
{
	m_entityBounds = bounds;
	UpdateGameConfigXmlData();
	CreateTexture();
}

void Explosion::Update(float deltaSeconds)
{
	m_age += deltaSeconds;
	if (m_age >= m_duration)
	{
		Die();
	}
}

void Explosion::Render() const
{
	std::vector<Vertex_PCU> explosionVerts;

	SpriteDefinition spriteDef = m_spriteAnimDef.GetSpriteDefAtTime(m_age);

	AddVertsForAABB2D(explosionVerts, m_entityBounds, m_tint, spriteDef.GetUVS());

	Vec2 fwrdNormal = GetForwardNormal();
	TransformVertexArrayXY3D(explosionVerts, fwrdNormal, fwrdNormal.GetRotated90Degrees(), m_position);

	g_renderer->SetBlendMode(BlendMode::ADDITIVE);
	g_renderer->BindTexture(m_texture);
	g_renderer->DrawVertexArray(explosionVerts);
}

void Explosion::DebugRender() const
{
	std::vector<Vertex_PCU> debugVerts;
	AddVertsForDisc2D(debugVerts, m_position, 0.025f, Rgba8::BLACK);

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->BindTexture(nullptr);
	g_renderer->DrawVertexArray(debugVerts);
}

void Explosion::Die()
{
	m_isGarbage = true;
	m_isDead = true;
}

void Explosion::UpdateGameConfigXmlData()
{
	m_isPushedByEntities = false;
	m_doesPushEntities = false;
	m_isHitByBullets = false;
}

void Explosion::CreateTexture()
{
	SpriteDefinition spriteDef = m_spriteAnimDef.GetSpriteDefAtTime(m_age);
	m_texture = &spriteDef.GetTexture();
}
