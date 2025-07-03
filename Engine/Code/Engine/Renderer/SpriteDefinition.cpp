#include "Engine/Renderer/SpriteDefinition.hpp"

#include "Engine/Math/AABB2.hpp"
#include "Engine/Renderer/SpriteSheet.hpp"
#include "Engine/Renderer/Texture.hpp"

SpriteDefinition::SpriteDefinition(SpriteSheet const& spriteSheet, int spriteIndex, Vec2 const& uvAtMins, Vec2 const& uvAtMaxs)
    :m_spriteSheet(spriteSheet)
    ,m_spriteIndex(spriteIndex)
    ,m_uvAtMins(uvAtMins)
    ,m_uvAtMaxs(uvAtMaxs)
{
}

void SpriteDefinition::GetUVS(Vec2& out_uvAtMins, Vec2& out_uvAtMaxs) const
{
    out_uvAtMins = m_uvAtMins;
    out_uvAtMaxs = m_uvAtMaxs;
}

AABB2 SpriteDefinition::GetUVS() const
{
    return AABB2(m_uvAtMins, m_uvAtMaxs);
}

SpriteSheet const& SpriteDefinition::GetSpriteSheet() const
{
    return m_spriteSheet;
}

Texture& SpriteDefinition::GetTexture() const
{
    return m_spriteSheet.GetTexture();
}

float SpriteDefinition::GetAspect() const
{
    IntVec2 textureDimensions = GetTexture().GetDimensions();
    float uvDimensionsX = m_uvAtMaxs.x - m_uvAtMins.x;
    float uvDimensionsY = m_uvAtMaxs.y - m_uvAtMins.y;

    return uvDimensionsX / uvDimensionsY;
}
