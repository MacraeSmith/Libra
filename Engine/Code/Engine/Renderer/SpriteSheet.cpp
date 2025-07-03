#include "Engine/Renderer/SpriteSheet.hpp"

#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Renderer/Texture.hpp"

SpriteSheet::SpriteSheet(Texture& texture, IntVec2 const& simpleGridLayout)
    :m_texture(texture)
{
    int numSpriteDefs = simpleGridLayout.x * simpleGridLayout.y;
    m_spriteDefs.reserve(numSpriteDefs);

    float spriteDefUVDimensionsX = 1.f / 
        (simpleGridLayout.x);
    float spriteDefUVDimensionY = 1.f / (float)(simpleGridLayout.y);
    Vec2 uvMins(0.f, 1.f - spriteDefUVDimensionY);
    Vec2 uvMaxs(spriteDefUVDimensionsX, 1.f);

    //nudge texture uvs to avoid bleeding
    Vec2 textureDimensions = Vec2(m_texture.GetDimensions());
    float xNudge = 1.f / (textureDimensions.x * 128.f); //#TODO may need to adjust these spritesheet nudge values
    float yNudge = 1.f / (textureDimensions.y * 128.f);

    int rowNum = 0;
    int columnNum = 0;

    for (int spriteNum = 0; spriteNum < numSpriteDefs; ++spriteNum)
    {
        if (columnNum >= simpleGridLayout.x)
        {
            rowNum++;
            columnNum = 0;
        }

        uvMins = Vec2((columnNum * spriteDefUVDimensionsX) + xNudge, (1 - ((rowNum + 1) * spriteDefUVDimensionY)) + yNudge);
        uvMaxs = Vec2(((columnNum + 1) * spriteDefUVDimensionsX) - xNudge, (1 - (rowNum * spriteDefUVDimensionY)) - yNudge);
        m_spriteDefs.push_back(SpriteDefinition(*this, spriteNum, uvMins, uvMaxs));

        columnNum++;
    }
}

Texture& SpriteSheet::GetTexture() const
{
    return m_texture;
}

int SpriteSheet::GetNumSprites() const
{
    return (int)(m_spriteDefs.size());
}

SpriteDefinition const& SpriteSheet::GetSpriteDef(int spriteIndex) const
{
    return m_spriteDefs[spriteIndex];
}

void SpriteSheet::GetSpriteUVs(Vec2& out_uvAtMins, Vec2& out_uvAtMax, int spriteIndex) const
{
    m_spriteDefs[spriteIndex].GetUVS(out_uvAtMins, out_uvAtMax);
}

AABB2 SpriteSheet::GetSpriteUVs(int spriteIndex) const
{
    return m_spriteDefs[spriteIndex].GetUVS();
}
