#include "Engine/Core/StaticMeshUtils.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Math/Vec3.hpp"
#include <vector>

bool ParseOBJMeshTextBuffer(VertTBNs& verts, std::string const& objFileContents, char const* fileNameForErrorReporting)
{
	Strings lineArgs;
	
	std::vector<Vec3> positions;
	std::vector<Vec3> normals;
	std::vector<Vec2> uvs;
	return false;
}
