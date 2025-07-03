#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"


int FileReadToBuffer(std::vector<uint8_t>& outBuffer, std::string const& fileName)
{
    FILE* newFile = new FILE;
    if (fopen_s(&newFile, fileName.c_str(), "rb") != 0)
    {
        ERROR_RECOVERABLE(Stringf("Could not open file: \"%s\"", fileName.c_str()));
        return 0;
    }

    if (fseek(newFile, 0, SEEK_END) != 0)
    {
        ERROR_RECOVERABLE(Stringf("File position indicator was not moved on file: \"%s\"", fileName.c_str()));
        return 0;
    }

    size_t size = ftell(newFile);
    outBuffer.resize(size);

    fseek(newFile, 0, SEEK_SET);

    if (fread(outBuffer.data(), sizeof(uint8_t), size, newFile) != size)
    {
        ERROR_RECOVERABLE(Stringf("Could not read file: \"%s\"", fileName.c_str()));
        return 0;
    }

    fclose(newFile);
    
    return (int)size;
}

int FileReadToString(std::string& outString, std::string const& fileName)
{
    std::vector <uint8_t> outBuffer;
    int success = FileReadToBuffer(outBuffer, fileName);
    outBuffer.push_back(0); //add null terminator

    outString = std::string(outBuffer.begin(), outBuffer.end());
    return success;
}
