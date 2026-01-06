#include "Vk/VK_MeshPipeline.h"
#include "Vk/VK_ShaderCompiler.h"
#include "Vk/VK_Context.h"
#include "Log.h"
#include <gtest/gtest.h>

namespace Nexus {

/**
 * @brief Mesh Shader 管线单元测试
 */
TEST(MeshShaderTest, PipelineCreation) {
    VK_Context context;
    ASSERT_TRUE(context.initializeHeadless().ok());

    vk::Device device = context.getDevice();

    std::string source = R"(
        struct VSOutput { float4 pos : SV_POSITION; };
        [numthreads(3, 1, 1)]
        [outputtopology("triangle")]
        void MSMain(out indices uint3 tris[1], out vertices VSOutput verts[3]) {
            SetMeshOutputs(3, 1);
            verts[0].pos = float4(0, 0, 0, 1);
            verts[1].pos = float4(1, 0, 0, 1);
            verts[2].pos = float4(0, 1, 0, 1);
            tris[0] = uint3(0, 1, 2);
        }
        float4 PSMain() : SV_Target { return float4(1, 1, 1, 1); }
    )";

    auto meshResult = VK_ShaderCompiler::compileLayer(device, source, "MSMain", shaderc_mesh_shader);
    auto pixelResult = VK_ShaderCompiler::compileLayer(device, source, "PSMain", shaderc_fragment_shader);

    ASSERT_TRUE(meshResult.ok());
    ASSERT_TRUE(pixelResult.ok());

    vk::PipelineLayoutCreateInfo layoutInfo;
    auto layout = device.createPipelineLayout(layoutInfo).value;

    VK_MeshPipeline pipeline(device);
    auto status = pipeline.initialize(layout, nullptr, meshResult.value(), pixelResult.value());

    EXPECT_TRUE(status.ok());

    device.destroyPipelineLayout(layout);
    device.destroyShaderModule(meshResult.value());
    device.destroyShaderModule(pixelResult.value());
}

} // namespace Nexus
