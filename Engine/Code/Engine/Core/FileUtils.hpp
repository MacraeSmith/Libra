#pragma once
#include <vector>
#include <string>

int FileReadToBuffer(std::vector<uint8_t>& outBuffer, std::string const& fileName);
int FileReadToString(std::string& outString, std::string const& fileName);
