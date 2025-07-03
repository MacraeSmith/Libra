#include "Engine/Core/Rgba8.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Game/EngineBuildPreferences.hpp"

#include <vector>
#include <string>

struct Vertex_PCU;
class Window;
struct IntVec2;
class Texture;
class BitmapFont;
class Image;

#define DX_SAFE_RELEASE(dxObject)	\
{									\
	if ((dxObject) != nullptr)		\
		{							\
			(dxObject)->Release();	\
			(dxObject) = nullptr;	\
		}							\
}

class Shader;
class VertexBuffer;
class ConstantBuffer;
class IndexBuffer;

enum class RendererType : int
{
	DIRECTX_11,
	DIRECTX_12,
	NUM_RENDERING_TYPES
};
struct RendererConfig 
{
	Window* m_window = nullptr;
	RendererType m_renderingType = RendererType::DIRECTX_11;
};


enum class VertexType : int
{
	VERTEX_PCU,
	VERTEX_PCUTBN,
	COUNT
};

#if defined(OPAQUE)
#undef OPAQUE
#endif
enum class BlendMode : int
{
	OPAQUE,
	ALPHA,
	ADDITIVE,
	LIGHTEN,
	COUNT,
	//add to GetNameForBlendMode() for future additions
};

enum class SamplerMode : int
{
	POINT_CLAMP,
	BILINEAR_WRAP,
	COUNT
};

enum class RasterizerMode : int
{
	SOLID_CULL_NONE,
	SOLID_CULL_BACK,
	WIREFRAME_CULL_NONE,
	WIREFRAME_CULL_BACK,
	WIREFRAME_CULL_FRONT,
	COUNT
	//add to GetNameForRasterizerMode() for future additions
};

enum class DepthMode : int
{
	DISABLED,
	READ_ONLY_ALWAYS,
	READ_ONLY_LESS_EQUAL,
	READ_WRITE_LESS_EQUAL,
	COUNT
	//add to GetNameForDepthMode() for future additions
};

constexpr int MAX_NUM_LIGHTS = 64;
struct LightData
{
	LightData() {};
	LightData(Rgba8 const& color, Vec3 const& position, float innerRadius, float outerRadius,
		Vec3 const& spotForward = Vec3::ZERO, float penumbraInnerDotThreshold = -1.f, float penumbraOuterDotThreshold = -2.f); //Default variables if unset will make a point light
	bool IsPointLight() const;

	float m_color[4] = {1.f, 1.f, 1.f, 1.f}; // alpha is intensity [0,1]
	Vec3 m_position;
	float m_innerRadius = 1.f;
	float m_outerRadius = 2.f;
	Vec3 m_spotForward;
	float m_penumbraInnerDotThreshold = -1.f;
	float m_penumbraOuterDotThreshold = -2.f;
	Vec2 Padding;
};

struct PerFrameConstants
{
	float m_time = 0.f;
	int m_debugInt = 0;
	float m_debugFloat = 0.f;
	float Padding = 0.f;
};

static const int k_perFrameConstantsSlot = 1;

struct CameraConstants
{
	Mat44 m_worldToCameraTransform;
	Mat44 m_cameraToRenderTransform;
	Mat44 m_renderToClipTransform;
	Vec3 m_cameraPosition;
	float Padding = 0.f;
};

static const int k_cameraConstantsSlot = 2;

struct ModelConstants
{
	Mat44 m_modelToWorldTransform;
	float m_modelColor[4];
};

static const int k_modelConstantsSlot = 3;

struct LightConstants
{
	LightConstants() {};
	explicit LightConstants(Vec3 const& sunDirection, float sunIntensity, float ambientIntensity, Rgba8 const& sunColorRGB);
	explicit LightConstants(Vec3 const& sunDirection, float sunIntensity, float ambientIntensity, std::vector<LightData> lights, Rgba8 const& sunColorRGB);

	Vec3 m_sunDirection;
	float m_sunIntensity = 1.f;
	float m_sunColorRGB[3] = {1.f, 1.f, 1.f};
	float m_ambientIntensity = 0.5f;
	int m_numLights = 0;
	float Padding[3] = {};
	LightData m_lights[MAX_NUM_LIGHTS];
};

static const int k_lightConstantsSlot = 4;

struct ColorAdjustmentConstants
{
	float m_saturation = 1.f;
	float m_brightness = 0.f;
	float m_inversion = 0.f;
	float m_hueStrength = 0.f;
	float m_hueColor[4] = {1.f, 1.f, 1.f, 1.f};

};

static const int k_colorAdjustmentConstantsSlot = 8;

constexpr int NUM_TEXTURE_DATA = 16;
static const int k_defaultDiffuseSlot = 0;
static const int k_defaultNormalsSlot = 1;

class Renderer
{
public:
	Renderer(RendererConfig const& config);
	~Renderer() {};

	virtual void Startup() = 0;
	virtual void BeginFrame() = 0;
	virtual void EndFrame() = 0;
	virtual void Shutdown() = 0;

	virtual void ClearScreen(const Rgba8& clearColor) = 0;
	virtual void BeginCamera(const Camera& camera) = 0;
	virtual void EndCamera(const Camera& camera) = 0;

	virtual void BeginRendererEvent(char const* eventName) = 0;
	virtual void EndRendererEvent() = 0;

	//Creation
	virtual Texture* CreateOrGetTextureFromFile(char const* imageFilePath) = 0;
	virtual BitmapFont* CreatOrGetBitMapFontFromFile(char const* bitmapFontFilePathWithNoExtension) = 0;


	//Binds
	void BindTexture(Texture* texture, int slot = 0);

	virtual void SetModelConstants(Mat44 const& modelToWorldTransform = Mat44(), Rgba8 const& modelColor = Rgba8::WHITE) = 0;
	virtual void SetLightConstants(Vec3 const& sunDirection, float sunIntensity, float ambientIntensity, Rgba8 const& sunColor = Rgba8::WHITE) = 0;
	virtual void SetLightConstants(LightConstants const& lightConstants) = 0;
	virtual void SetColorAdjustmentConstants(ColorAdjustmentConstants const& colorAdjustmentConstants) = 0;
	virtual void SetPerFrameConstants(PerFrameConstants const& perFrameConstants) = 0;

	std::string GetNameForBlendMode(BlendMode const& blendMode) const;
	std::string GetNameForDepthMode(DepthMode const& depthMode) const;
	std::string GetNameForRasterizerMode(RasterizerMode const& rasterizerMode) const;

private:


private:
	//std::vector<Texture*> m_loadedTextures;
	//std::vector<BitmapFont*> m_loadedFonts;

protected:
	RendererConfig m_config;


#if defined(ENGINE_DEBUG_RENDERER)
	void* m_dxgiDebug = nullptr;
	void* m_dxgiDebugModule = nullptr;
#endif

};