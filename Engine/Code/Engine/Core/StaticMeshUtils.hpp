#pragma once
#include "Engine/Core/VertexUtils.hpp"
#include <string>

bool ParseOBJData(std::string& out_objFileContents, std::string const& objFilePath);
bool LoadOBJMeshFile(VertTBNs& verts, std::string const& objFilePath);
bool ParseOBJMeshTextBuffer(VertTBNs& verts, std::string const& objFileContents, char const* fileNameForErrorReporting = "DefaultOBJName");

