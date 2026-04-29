#include "GaussianSplattingLoader.h"
#include "../Bridge/ResourceLoader.h"
#include <sstream>
#include <cstring>
#include <iostream>
#include <algorithm>

namespace Nexus {
namespace Core {

StatusOr<std::vector<GaussianSplatData>> GaussianSplattingLoader::loadFromPLY(const std::string& path) {
    NX_ASSIGN_OR_RETURN(auto data, ResourceLoader::loadBinaryFile(path));

    std::string header;
    size_t dataOffset = 0;
    
    const char* endHeaderStr = "end_header\n";
    auto it = std::search(data.begin(), data.end(), endHeaderStr, endHeaderStr + 11);
    if (it == data.end()) {
        return InternalError("Invalid PLY file: no end_header found");
    }
    
    dataOffset = std::distance(data.begin(), it) + 11;
    header.assign(data.begin(), it);

    std::istringstream iss(header);
    std::string line;
    uint32_t vertexCount = 0;
    
    struct Property {
        std::string name;
        std::string type;
        size_t size;
        int offset;
    };
    std::vector<Property> properties;
    int currentOffset = 0;
    
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        
        std::istringstream lineStream(line);
        std::string token;
        lineStream >> token;
        
        if (token == "element") {
            std::string elementType;
            lineStream >> elementType;
            if (elementType == "vertex") {
                lineStream >> vertexCount;
            }
        } else if (token == "property") {
            std::string type, name;
            lineStream >> type >> name;
            size_t size = 4; 
            properties.push_back({name, type, size, currentOffset});
            currentOffset += size;
        }
    }
    
    if (vertexCount == 0) {
        return InternalError("PLY file has no vertices");
    }
    
    size_t vertexStride = currentOffset;
    if (dataOffset + vertexCount * vertexStride > data.size()) {
        return InternalError("PLY file data size is smaller than expected");
    }
    
    int offsetX = -1, offsetY = -1, offsetZ = -1;
    int offsetDC0 = -1, offsetDC1 = -1, offsetDC2 = -1;
    int offsetOpacity = -1;
    int offsetScale0 = -1, offsetScale1 = -1, offsetScale2 = -1;
    int offsetRot0 = -1, offsetRot1 = -1, offsetRot2 = -1, offsetRot3 = -1;
    
    for (const auto& prop : properties) {
        if (prop.name == "x") offsetX = prop.offset;
        else if (prop.name == "y") offsetY = prop.offset;
        else if (prop.name == "z") offsetZ = prop.offset;
        else if (prop.name == "f_dc_0") offsetDC0 = prop.offset;
        else if (prop.name == "f_dc_1") offsetDC1 = prop.offset;
        else if (prop.name == "f_dc_2") offsetDC2 = prop.offset;
        else if (prop.name == "opacity") offsetOpacity = prop.offset;
        else if (prop.name == "scale_0") offsetScale0 = prop.offset;
        else if (prop.name == "scale_1") offsetScale1 = prop.offset;
        else if (prop.name == "scale_2") offsetScale2 = prop.offset;
        else if (prop.name == "rot_0") offsetRot0 = prop.offset;
        else if (prop.name == "rot_1") offsetRot1 = prop.offset;
        else if (prop.name == "rot_2") offsetRot2 = prop.offset;
        else if (prop.name == "rot_3") offsetRot3 = prop.offset;
    }
    
    if (offsetX < 0 || offsetOpacity < 0 || offsetScale0 < 0 || offsetRot0 < 0 || offsetDC0 < 0) {
        return InternalError("PLY file missing required 3DGS properties");
    }
    
    std::vector<GaussianSplatData> splats(vertexCount);
    const uint8_t* rawData = data.data() + dataOffset;

    auto readFloat = [&](const uint8_t* ptr, int offset) -> float {
        float val = 0.0f;
        if (offset >= 0) {
            std::memcpy(&val, ptr + offset, sizeof(float));
        }
        return val;
    };
    
    for (uint32_t i = 0; i < vertexCount; ++i) {
        const uint8_t* vData = rawData + i * vertexStride;
        GaussianSplatData& splat = splats[i];
        
        splat.position_opacity.x = readFloat(vData, offsetX);
        splat.position_opacity.y = readFloat(vData, offsetY);
        splat.position_opacity.z = readFloat(vData, offsetZ);
        splat.position_opacity.w = readFloat(vData, offsetOpacity);
        
        splat.scale.x = readFloat(vData, offsetScale0);
        splat.scale.y = readFloat(vData, offsetScale1);
        splat.scale.z = readFloat(vData, offsetScale2);
        splat.scale.w = 0.0f;
        
        splat.rot.x = readFloat(vData, offsetRot0);
        splat.rot.y = readFloat(vData, offsetRot1);
        splat.rot.z = readFloat(vData, offsetRot2);
        splat.rot.w = readFloat(vData, offsetRot3);
        
        splat.color.x = readFloat(vData, offsetDC0);
        splat.color.y = readFloat(vData, offsetDC1);
        splat.color.z = readFloat(vData, offsetDC2);
        splat.color.w = 0.0f;
    }
    
    return splats;
}

} // namespace Core
} // namespace Nexus
