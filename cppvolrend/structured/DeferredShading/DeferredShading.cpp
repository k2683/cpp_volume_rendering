#include "../../defines.h"
#include "DeferredShading.h"

#include <vis_utils/camera.h>
#include <volvis_utils/datamanager.h>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <math_utils/utils.h>



template <typename T>
T GetVoxelValue(void* data, size_t index) {
    return static_cast<T*>(data)[index];
}


void DeferredShading::ComputeBlocksFromVolume(
    vis::StructuredGridVolume* volume,
    glm::vec3 numBlocks,
    std::vector<float>& minValues,
    std::vector<float>& maxValues,
    glm::vec3 vol_voxelsize) {
    minValues.clear();
    maxValues.clear();
    // 获取体数据属性
    unsigned int volumeWidth = volume->GetWidth();
    unsigned int volumeHeight = volume->GetHeight();
    unsigned int volumeDepth = volume->GetDepth();

    void* volumeData = volume->GetArrayData();
    vis::DataStorageSize storageType = volume->m_data_storage_size;

    // 计算每个块的大小
    int blockSizeX = (volumeWidth + numBlocks.x - 1) / numBlocks.x;
    int blockSizeY = (volumeHeight + numBlocks.y - 1) / numBlocks.y;
    int blockSizeZ = (volumeDepth + numBlocks.z - 1) / numBlocks.z;

    // 遍历每个块
    for (int bz = 0; bz < numBlocks.z; ++bz) {
        for (int by = 0; by < numBlocks.y; ++by) {
            for (int bx = 0; bx < numBlocks.x; ++bx) {
                // 计算块的边界
                int startX = bx * blockSizeX;
                int startY = by * blockSizeY;
                int startZ = bz * blockSizeZ;

                int endX = std::min(startX + blockSizeX, static_cast<int>(volumeWidth));
                int endY = std::min(startY + blockSizeY, static_cast<int>(volumeHeight));
                int endZ = std::min(startZ + blockSizeZ, static_cast<int>(volumeDepth));
                // 初始化块的 min 和 max 值
                double minValue = std::numeric_limits<float>::max();
                double maxValue = std::numeric_limits<float>::lowest();

                // 获取数据类型的最大值，用于归一化
                double maxDataValue = 1.0;
                switch (storageType) {
                case vis::DataStorageSize::_8_BITS:
                    maxDataValue = 255.0;
                    break;
                case vis::DataStorageSize::_16_BITS:
                    maxDataValue = 65535.0;
                    break;
                case vis::DataStorageSize::_NORMALIZED_F:
                case vis::DataStorageSize::_NORMALIZED_D:
                    maxDataValue = 1.0;
                    break;
                default:
                    throw std::runtime_error("Unsupported data type");
                }

                // 遍历块中的所有体素
                for (int z = startZ; z < endZ; ++z) {
                    for (int y = startY; y < endY; ++y) {
                        for (int x = startX; x < endX; ++x) {
                            size_t index = z * (volumeWidth * volumeHeight) + y * volumeWidth + x;
                            double value;

                            // 根据存储类型读取并归一化体素值
                            switch (storageType) {
                            case vis::DataStorageSize::_8_BITS:
                                value = GetVoxelValue<unsigned char>(volumeData, index) / maxDataValue;
                                break;
                            case vis::DataStorageSize::_16_BITS:
                                value = GetVoxelValue<unsigned short>(volumeData, index) / maxDataValue;
                                break;
                            case vis::DataStorageSize::_NORMALIZED_F:
                                value = GetVoxelValue<float>(volumeData, index);
                                break;
                            case vis::DataStorageSize::_NORMALIZED_D:
                                value = GetVoxelValue<double>(volumeData, index);
                                break;
                            default:
                                throw std::runtime_error("Unsupported data type");
                            }
                            // 更新 min 和 max 值
                            minValue = std::min(minValue, value);
                            maxValue = std::max(maxValue, value);
                        }
                    }
                }


                minValues.push_back(minValue);
                maxValues.push_back(maxValue);

            }
        }
    }

}




DeferredShading::DeferredShading()
    : cp_geometry_pass(nullptr)
    , cp_lighting_pass(nullptr)
    , cp_post_process(nullptr)
    , cp_shader_rendering(nullptr)  // 保留原有的shader
    , m_u_isovalue(0.5f)
    , m_u_step_size_small(0.05f)
    , m_u_step_size_large(1.0f)
    , m_u_step_size_range(0.1f)
    , m_u_color(0.66f, 0.6f, 0.05f, 1.0f)
    , m_apply_gradient_shading(false)
{
    // 保持其他初始化不变
}


DeferredShading::~DeferredShading()
{
}


const char* DeferredShading::GetName()
{
    return "Deferred Rendering";
}


const char* DeferredShading::GetAbbreviationName()
{
    return "iso";
}


vis::GRID_VOLUME_DATA_TYPE DeferredShading::GetDataTypeSupport()
{
    return vis::GRID_VOLUME_DATA_TYPE::STRUCTURED;
}

void DeferredShading::CreateGBuffer(int width, int height)
{
    // 创建G-Buffer帧缓冲
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

    // 位置缓冲 - 用于存储射线相交点
    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

    // 法线/梯度缓冲 - 用于存储梯度信息
    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

    // 配置绘制缓冲
    GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, drawBuffers);

    // 验证帧缓冲完整性
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "G-Buffer Framebuffer is not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DeferredShading::LightingPass(vis::Camera* camera) {
    if (!camera) return;

    // 计算工作组大小
    int width = m_ext_rendering_parameters->GetScreenWidth();
    int height = m_ext_rendering_parameters->GetScreenHeight();

    cp_lighting_pass->RecomputeNumberOfGroups(width, height, 1);  

    // 绑定纹理
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    cp_lighting_pass->SetUniform("gPosition", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    cp_lighting_pass->SetUniform("gNormal", 1);

    // 绑定输出图像
    glBindImageTexture(2, m_lighting_texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    // 设置光照参数
    cp_lighting_pass->SetUniform("Ka", 0.1f);  
    cp_lighting_pass->SetUniform("Kd", 0.7f);
    cp_lighting_pass->SetUniform("Ks", 0.2f);
    cp_lighting_pass->SetUniform("shininess", 32.0f);
    cp_lighting_pass->SetUniform("lightColor", glm::vec3(1.0f));

    cp_lighting_pass->Bind();
    cp_lighting_pass->Dispatch();

    // 确保计算完成
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    cp_lighting_pass->Unbind();
}
void DeferredShading::Clean() {
    // 清理着色器
    delete cp_geometry_pass;
    delete cp_lighting_pass;
    delete cp_post_process;
    cp_geometry_pass = nullptr;
    cp_lighting_pass = nullptr;
    cp_post_process = nullptr;

    // 清理G-Buffer
    if (gBuffer) {
        glDeleteFramebuffers(1, &gBuffer);
        glDeleteTextures(1, &gPosition);
        glDeleteTextures(1, &gNormal);
        gBuffer = 0;
        gPosition = 0;
        gNormal = 0;
    }

    // 清理四边形资源
    if (m_quad_vao) {
        glDeleteVertexArrays(1, &m_quad_vao);
        m_quad_vao = 0;
    }
    if (m_quad_vbo) {
        glDeleteBuffers(1, &m_quad_vbo);
        m_quad_vbo = 0;
    }

    // 清理分块纹理
    if (m_texBlockMin) {
        glDeleteTextures(1, &m_texBlockMin);
        m_texBlockMin = 0;
    }
    if (m_texBlockMax) {
        glDeleteTextures(1, &m_texBlockMax);
        m_texBlockMax = 0;
    }
    if (m_lighting_texture) {
        glDeleteTextures(1, &m_lighting_texture);
        m_lighting_texture = 0;
    }
    gl::ExitOnGLError("DeferredShading: Could not clean up resources");
}
void DeferredShading::InitQuad() {
    // 定义全屏四边形的顶点数据（位置和纹理坐标）
    float quadVertices[] = {
        // 位置(x,y)      // 纹理坐标(u,v)
        -1.0f,  1.0f,     0.0f, 1.0f,   // 左上角
        -1.0f, -1.0f,     0.0f, 0.0f,   // 左下角
         1.0f,  1.0f,     1.0f, 1.0f,   // 右上角
         1.0f, -1.0f,     1.0f, 0.0f    // 右下角
    };

    // 创建并绑定VAO
    glGenVertexArrays(1, &m_quad_vao);
    glBindVertexArray(m_quad_vao);

    // 创建并绑定VBO
    glGenBuffers(1, &m_quad_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

    // 设置顶点属性指针
    // 位置属性
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    // 纹理坐标属性
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    // 解绑VAO和VBO
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
void DeferredShading::RenderQuad() {
    // 绑定最终渲染的顶点数组
    glBindVertexArray(m_quad_vao);

    // 禁用深度测试，因为我们要渲染一个全屏四边形
    glDisable(GL_DEPTH_TEST);

    // 绑定光照结果纹理
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_lighting_texture);
    cp_post_process->SetUniform("finalImage", 0);

    // 使用三角形带绘制四边形（两个三角形）
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // 恢复深度测试
    glEnable(GL_DEPTH_TEST);

    // 解绑VAO
    glBindVertexArray(0);
}
bool DeferredShading::Init(int swidth, int sheight) {
    if (IsBuilt()) Clean();

    if (m_ext_data_manager->GetCurrentVolumeTexture() == nullptr) return false;

    // 创建G-Buffer
    CreateGBuffer(swidth, sheight);

    // 初始化全屏四边形
    InitQuad();

    // 获取体数据信息
    glm::vec3 vol_resolution = glm::vec3(m_ext_data_manager->GetCurrentStructuredVolume()->GetWidth(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetHeight(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetDepth());

    glm::vec3 vol_voxelsize = glm::vec3(m_ext_data_manager->GetCurrentStructuredVolume()->GetScaleX(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetScaleY(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetScaleZ());

    glm::vec3 vol_aabb = vol_resolution * vol_voxelsize;

    // 初始化分块结构
    auto* volume = m_ext_data_manager->GetCurrentStructuredVolume();
    if (!volume) return false;

    glm::vec3 numBlocks(4, 4, 4);
    std::vector<float> minValues;
    std::vector<float> maxValues;
    ComputeBlocksFromVolume(volume, numBlocks, minValues, maxValues, vol_voxelsize);

    // 创建几何Pass着色器
    cp_geometry_pass = new gl::ComputeShader();
    cp_geometry_pass->AddShaderFile(CPPVOLREND_DIR"structured/DeferredShading/geometry_pass.comp");
    cp_geometry_pass->AddShaderFile(CPPVOLREND_DIR"structured/_common_shaders/ray_bbox_intersection.comp");
    if (!cp_geometry_pass->LoadAndLink()) {
        std::cerr << "Failed to load geometry pass shader" << std::endl;
        return false;
    }
    // 创建光照缓冲
    glGenFramebuffers(1, &lightingBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, lightingBuffer);


    // 创建光照Pass着色器
    cp_lighting_pass = new gl::ComputeShader();
    cp_lighting_pass->AddShaderFile(CPPVOLREND_DIR"structured/DeferredShading/lighting_pass.comp");
    if (!cp_lighting_pass->LoadAndLink()) {
        std::cerr << "Failed to load lighting pass shader" << std::endl;
        return false;
    }

    // 创建后处理Pass着色器
    cp_post_process = new gl::PipelineShader();
    cp_post_process->AddShaderFile(gl::PipelineShader::TYPE::VERTEX, CPPVOLREND_DIR"structured/DeferredShading/post_process.vert");

    cp_post_process->AddShaderFile(gl::PipelineShader::TYPE::FRAGMENT, CPPVOLREND_DIR"structured/DeferredShading/post_process.frag");
    if (!cp_post_process->LoadAndLink()) {
        std::cerr << "Failed to load post process shader" << std::endl;
        return false;
    }

    // 设置几何Pass的uniform
    cp_geometry_pass->Bind();
    cp_geometry_pass->SetUniform("VolumeGridResolution", vol_resolution);
    cp_geometry_pass->SetUniform("VolumeVoxelSize", vol_voxelsize);
    cp_geometry_pass->SetUniform("VolumeGridSize", vol_aabb);
    cp_geometry_pass->SetUniform("numBlocks", numBlocks);

    // 绑定体数据纹理
    if (m_ext_data_manager->GetCurrentVolumeTexture()) {
        cp_geometry_pass->SetUniformTexture3D("TexVolume",
            m_ext_data_manager->GetCurrentVolumeTexture()->GetTextureID(), 2);
    }

    // 上传分块数据
    GLuint texIDBlockMin, texIDBlockMax;
    glGenTextures(1, &texIDBlockMin);
    glGenTextures(1, &texIDBlockMax);

    // 设置分块最小值纹理
    glBindTexture(GL_TEXTURE_3D, texIDBlockMin);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F,
        (GLsizei)numBlocks.x, (GLsizei)numBlocks.y, (GLsizei)numBlocks.z,
        0, GL_RED, GL_FLOAT, minValues.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_3D, 0);

    // 设置分块最大值纹理
    glBindTexture(GL_TEXTURE_3D, texIDBlockMax);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F,
        (GLsizei)numBlocks.x, (GLsizei)numBlocks.y, (GLsizei)numBlocks.z,
        0, GL_RED, GL_FLOAT, maxValues.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_3D, 0);

    cp_geometry_pass->SetUniformTexture3D("TexBlockMin", texIDBlockMin, 3);
    cp_geometry_pass->SetUniformTexture3D("TexBlockMax", texIDBlockMax, 4);

    glGenTextures(1, &m_lighting_texture);
    glBindTexture(GL_TEXTURE_2D, m_lighting_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, swidth, sheight, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_lighting_texture, 0);

    // 初始化完成
    cp_geometry_pass->Unbind();

    // 支持多重采样（保持原有代码）
    Reshape(swidth, sheight);

    SetBuilt(true);
    SetOutdated();
    return true;
}

void DeferredShading::ReloadShaders()
{
    cp_shader_rendering->Reload();
    m_rdr_frame_to_screen.ClearShaders();
}

void DeferredShading::UpdateGeometryPassUniforms(vis::Camera* camera) {
    // 设置相机参数
    cp_geometry_pass->SetUniform("CameraEye", camera->GetEye());
    cp_geometry_pass->SetUniform("u_CameraLookAt", camera->LookAt());
    cp_geometry_pass->SetUniform("ProjectionMatrix", camera->Projection());
    cp_geometry_pass->SetUniform("u_TanCameraFovY", (float)tan(DEGREE_TO_RADIANS(camera->GetFovY()) / 2.0));
    cp_geometry_pass->SetUniform("u_CameraAspectRatio", camera->GetAspectRatio());

    // 设置等值面参数
    cp_geometry_pass->SetUniform("Isovalue", m_u_isovalue);
    cp_geometry_pass->SetUniform("StepSizeSmall", m_u_step_size_small);
    cp_geometry_pass->SetUniform("StepSizeLarge", m_u_step_size_large);
    cp_geometry_pass->SetUniform("StepSizeRange", m_u_step_size_range);
    cp_geometry_pass->SetUniform("Color", m_u_color);

    // 重新计算工作组大小
    if (IsPixelMultiScalingSupported() && GetCurrentMultiScalingMode() > 0) {
        cp_geometry_pass->RecomputeNumberOfGroups(
            m_rdr_frame_to_screen.GetWidth(),
            m_rdr_frame_to_screen.GetHeight(), 0);
    }
    else {
        cp_geometry_pass->RecomputeNumberOfGroups(
            m_ext_rendering_parameters->GetScreenWidth(),
            m_ext_rendering_parameters->GetScreenHeight(), 0);
    }
}
void DeferredShading::UpdateLightingPassUniforms(vis::Camera* camera) {
    // 设置光照相关参数
    cp_lighting_pass->SetUniform("objectColor", m_u_color);  // 添加这一行
    cp_lighting_pass->SetUniform("Ka", m_ext_rendering_parameters->GetBlinnPhongKambient());
    cp_lighting_pass->SetUniform("Kd", m_ext_rendering_parameters->GetBlinnPhongKdiffuse());
    cp_lighting_pass->SetUniform("Ks", m_ext_rendering_parameters->GetBlinnPhongKspecular());
    cp_lighting_pass->SetUniform("shininess", m_ext_rendering_parameters->GetBlinnPhongNshininess());
    cp_lighting_pass->SetUniform("BlinnPhongIspecular", m_ext_rendering_parameters->GetLightSourceSpecular());
    cp_lighting_pass->SetUniform("LightSourcePosition", m_ext_rendering_parameters->GetBlinnPhongLightingPosition());
    cp_lighting_pass->SetUniform("CameraEye", camera->GetEye());
}
bool DeferredShading::Update(vis::Camera* camera)
{
    if (!camera) return false;
    mycamera = camera;
    // 更新几何Pass的uniform
    cp_geometry_pass->Bind();
    UpdateGeometryPassUniforms(camera);
    cp_geometry_pass->BindUniforms();
    cp_geometry_pass->Unbind();

    // 更新光照Pass的uniform
    cp_lighting_pass->Bind();
    UpdateLightingPassUniforms(camera);
    cp_lighting_pass->BindUniforms();
    cp_lighting_pass->Unbind();

    gl::ExitOnGLError("DeferredShading: After Update.");
    return true;
}
void DeferredShading::Redraw() {
    // 1. 几何Pass
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    cp_geometry_pass->Bind();
    glBindImageTexture(0, gPosition, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glBindImageTexture(1, gNormal, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    cp_geometry_pass->Dispatch();
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    gl::ComputeShader::Unbind();

    // 添加调试代码：读取G-Buffer的内容
    GLfloat pixel[4];
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixel);

    // 2. 光照Pass
    LightingPass(mycamera);

    // 添加调试代码：读取光照结果
    glBindFramebuffer(GL_FRAMEBUFFER, lightingBuffer);
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixel);

    // 3. 最终合成
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    cp_post_process->Bind();

    // 确保正确设置了采样器
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_lighting_texture);
    cp_post_process->SetUniform("finalImage", 0);
    cp_post_process->SetUniform("exposure", 1.0f);  // 添加合理的曝光值
    cp_post_process->SetUniform("gamma", 2.2f);     // 添加gamma校正

    RenderQuad();
    cp_post_process->Unbind();
}

void DeferredShading::FillParameterSpace(ParameterSpace& pspace)
{
    pspace.ClearParameterDimensions();
    pspace.AddParameterDimension(new ParameterRangeFloat("StepSizeSmall", &m_u_step_size_small, 0.01f, 0.25f, 0.05f));
    pspace.AddParameterDimension(new ParameterRangeFloat("StepSizeLarge", &m_u_step_size_large, 0.25f, 2.0f, 0.25f));
    pspace.AddParameterDimension(new ParameterRangeFloat("StepSizeRange", &m_u_step_size_range, 0.05f, 0.26f, 0.05f));
}


void DeferredShading::SetImGuiComponents()
{
    ImGui::Separator();

    ImGui::Text("Isovalue: ");
    if (ImGui::DragFloat("###DeferredShadingUIIsovalue", &m_u_isovalue, 0.01f, 0.01f, 100.0f, "%.2f"))
    {
        m_u_isovalue = std::max(std::min(m_u_isovalue, 100.0f), 0.01f); // 当通过键盘输入时，ImGui 不会处理最小值/最大值。
        SetOutdated();
    }

    if (ImGui::ColorEdit4("Color", &m_u_color[0]))
    {
        SetOutdated();
    }

    ImGui::Text("Step Size Small: ");
    if (ImGui::DragFloat("###DeferredShadingUIIntegrationStepSizeSmall", &m_u_step_size_small, 0.005f, 0.01f, 1.0f, "%.2f"))
    {
        m_u_step_size_small = std::max(std::min(m_u_step_size_small, 1.0f), 0.01f);
        SetOutdated();
    }

    ImGui::Text("Step Size Large: ");
    if (ImGui::DragFloat("###DeferredShadingUIIntegrationStepSizeLarge", &m_u_step_size_large, 0.01f, 0.05f, 5.0f, "%.2f"))
    {
        m_u_step_size_large = std::max(std::min(m_u_step_size_large, 5.0f), 0.05f);
        SetOutdated();
    }

    ImGui::Text("Step Size Range: ");
    if (ImGui::DragFloat("###DeferredShadingUIIntegrationStepSizeRange", &m_u_step_size_range, 0.01f, 0.05f, 0.5f, "%.2f"))
    {
        m_u_step_size_range = std::max(std::min(m_u_step_size_range, 0.5f), 0.05f);
        SetOutdated();
    }

    if (m_ext_data_manager->GetCurrentGradientTexture())
    {
        ImGui::Separator();
        if (ImGui::Checkbox("Apply Gradient Shading", &m_apply_gradient_shading))
        {
            // 删除当前的 Uniform
            cp_shader_rendering->ClearUniform("TexVolumeGradient");

            if (m_apply_gradient_shading && m_ext_data_manager->GetCurrentGradientTexture())
            {
                cp_shader_rendering->Bind();
                cp_shader_rendering->SetUniformTexture3D("TexVolumeGradient", m_ext_data_manager->GetCurrentGradientTexture()->GetTextureID(), 2);
                cp_shader_rendering->BindUniform("TexVolumeGradient");
                gl::ComputeShader::Unbind();
            }
            SetOutdated();
        }
        ImGui::Separator();
    }
}
