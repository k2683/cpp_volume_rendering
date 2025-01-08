#include "../../defines.h"
#include "rc1custompisoadaptdfsrenderer.h"

#include <vis_utils/camera.h>
#include <volvis_utils/datamanager.h>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <math_utils/utils.h>



template <typename T>
T GetVoxelValue(void* data, size_t index) {
    return static_cast<T*>(data)[index];
}


void CustomRayCasting1PassIsodfsAdapt::ComputeBlocksFromVolume(
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





CustomRayCasting1PassIsodfsAdapt::CustomRayCasting1PassIsodfsAdapt()
    :cp_shader_rendering(nullptr)
    , m_u_isovalue(0.5f)
    , m_u_step_size_small(0.05f)
    , m_u_step_size_large(1.0f)
    , m_u_step_size_range(0.1f)
    , m_u_color(0.66f, 0.6f, 0.05f, 1.0f)
    , m_apply_gradient_shading(false)
{
}


CustomRayCasting1PassIsodfsAdapt::~CustomRayCasting1PassIsodfsAdapt()
{
}


const char* CustomRayCasting1PassIsodfsAdapt::GetName()
{
    return "Empty Sapce Skipping V2";
}


const char* CustomRayCasting1PassIsodfsAdapt::GetAbbreviationName()
{
    return "iso";
}


vis::GRID_VOLUME_DATA_TYPE CustomRayCasting1PassIsodfsAdapt::GetDataTypeSupport()
{
    return vis::GRID_VOLUME_DATA_TYPE::STRUCTURED;
}


void CustomRayCasting1PassIsodfsAdapt::Clean()
{
    if (cp_shader_rendering) delete cp_shader_rendering;
    cp_shader_rendering = nullptr;

    gl::ExitOnGLError("Could not destroy shaders");

    BaseVolumeRenderer::Clean();
}


bool CustomRayCasting1PassIsodfsAdapt::Init(int swidth, int sheight)
{
    if (IsBuilt()) Clean();

    if (m_ext_data_manager->GetCurrentVolumeTexture() == nullptr) return false;

    ////////////////////////////////////////////
    // 创建渲染缓冲区和着色器

    // - 定义统一网格和边界框
    glm::vec3 vol_resolution = glm::vec3(m_ext_data_manager->GetCurrentStructuredVolume()->GetWidth(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetHeight(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetDepth());

    glm::vec3 vol_voxelsize = glm::vec3(m_ext_data_manager->GetCurrentStructuredVolume()->GetScaleX(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetScaleY(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetScaleZ());

    glm::vec3 vol_aabb = vol_resolution * vol_voxelsize;
    std::cout << "vol_aabb.x= " << vol_aabb.x << "vol_aabb.y " << vol_aabb.y << "vol_aabb.z" << vol_aabb.z << std::endl;

    auto* volume = m_ext_data_manager->GetCurrentStructuredVolume();
    if (!volume) return false;

    // 初始化块分割
    glm::vec3 numBlocks(32, 32, 32);
    std::vector<float> minValues;
    std::vector<float> maxValues;
    ComputeBlocksFromVolume(volume, numBlocks, minValues, maxValues, vol_voxelsize);


    // - 加载着色器
    cp_shader_rendering = new gl::ComputeShader();
    cp_shader_rendering->AddShaderFile(CPPVOLREND_DIR"structured/_common_shaders/ray_bbox_intersection.comp");
    cp_shader_rendering->AddShaderFile(CPPVOLREND_DIR"structured/rc1pisodfscustom/custom_ray_marching_1p_iso_adapt.comp");
    cp_shader_rendering->LoadAndLink();
    cp_shader_rendering->Bind();


    cp_shader_rendering->SetUniform("blockTexture", 3);


    // - 要处理的数据集：标量场及其梯度
    if (m_ext_data_manager->GetCurrentVolumeTexture())
        cp_shader_rendering->SetUniformTexture3D("TexVolume", m_ext_data_manager->GetCurrentVolumeTexture()->GetTextureID(), 1);
    if (m_apply_gradient_shading && m_ext_data_manager->GetCurrentGradientTexture())
        cp_shader_rendering->SetUniformTexture3D("TexVolumeGradient", m_ext_data_manager->GetCurrentGradientTexture()->GetTextureID(), 2);

    // - 让着色器知道统一网格的信息
    cp_shader_rendering->SetUniform("VolumeGridResolution", vol_resolution);
    cp_shader_rendering->SetUniform("VolumeVoxelSize", vol_voxelsize);
    cp_shader_rendering->SetUniform("VolumeGridSize", vol_aabb);
    cp_shader_rendering->SetUniform("numBlocks", numBlocks);



    std::cout << "vol_resolution=" << vol_resolution.x << "  " << vol_resolution.y << "  " << vol_resolution.z << std::endl;
    std::cout << "vol_voxelsize=" << vol_voxelsize.x << "  " << vol_voxelsize.y << "  " << vol_voxelsize.z << std::endl;
    std::cout << "vol_aabb=" << vol_aabb.x << "  " << vol_aabb.y << "  " << vol_aabb.z << std::endl;


    // 1) 准备好OpenGL纹理ID
    GLuint texIDBlockMin, texIDBlockMax;
    glGenTextures(1, &texIDBlockMin);
    glGenTextures(1, &texIDBlockMax);

    // 2) 上传 blockMin 的数据
    glBindTexture(GL_TEXTURE_3D, texIDBlockMin);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F,
        (GLsizei)numBlocks.x, (GLsizei)numBlocks.y, (GLsizei)numBlocks.z,
        0, GL_RED, GL_FLOAT, minValues.data());
    // 设置过滤方式
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // 解除绑定
    glBindTexture(GL_TEXTURE_3D, 0);

    // 3) 上传 blockMax 的数据
    glBindTexture(GL_TEXTURE_3D, texIDBlockMax);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F,
        (GLsizei)numBlocks.x, (GLsizei)numBlocks.y, (GLsizei)numBlocks.z,
        0, GL_RED, GL_FLOAT, maxValues.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_3D, 0);



    // cp_shader_rendering->Bind(); // 如果需要的话
    cp_shader_rendering->SetUniformTexture3D("TexBlockMin", texIDBlockMin, 3);
    cp_shader_rendering->SetUniformTexture3D("TexBlockMax", texIDBlockMax, 4);



    cp_shader_rendering->Unbind();
    gl::ExitOnGLError("CustomRayCasting1PassIsodfsAdapt: Error on Preparing Models and Shaders");


    /////////////////////////////////
    // 完成初始化

    // 支持多重采样
    Reshape(swidth, sheight);

    SetBuilt(true);
    SetOutdated();
    return true;
}


void CustomRayCasting1PassIsodfsAdapt::ReloadShaders()
{
    cp_shader_rendering->Reload();
    m_rdr_frame_to_screen.ClearShaders();
}


bool CustomRayCasting1PassIsodfsAdapt::Update(vis::Camera* camera)
{
    cp_shader_rendering->Bind();

    /////////////////////////////
    // 多重采样
    if (IsPixelMultiScalingSupported() && GetCurrentMultiScalingMode() > 0)
    {
        cp_shader_rendering->RecomputeNumberOfGroups(m_rdr_frame_to_screen.GetWidth(),
            m_rdr_frame_to_screen.GetHeight(), 0);
    }
    else
    {
        cp_shader_rendering->RecomputeNumberOfGroups(m_ext_rendering_parameters->GetScreenWidth(),
            m_ext_rendering_parameters->GetScreenHeight(), 0);
    }

    /////////////////////////////
    // 相机
    cp_shader_rendering->SetUniform("CameraEye", camera->GetEye());
    cp_shader_rendering->SetUniform("u_CameraLookAt", camera->LookAt());
    cp_shader_rendering->SetUniform("ProjectionMatrix", camera->Projection());
    cp_shader_rendering->SetUniform("u_TanCameraFovY", (float)tan(DEGREE_TO_RADIANS(camera->GetFovY()) / 2.0));
    cp_shader_rendering->SetUniform("u_CameraAspectRatio", camera->GetAspectRatio());
    cp_shader_rendering->SetUniform("WorldEyePos", camera->GetEye());

    /////////////////////////////
    // 等值面参数
    cp_shader_rendering->SetUniform("Isovalue", m_u_isovalue);
    cp_shader_rendering->SetUniform("StepSizeSmall", m_u_step_size_small);
    cp_shader_rendering->SetUniform("StepSizeLarge", m_u_step_size_large);
    cp_shader_rendering->SetUniform("StepSizeRange", m_u_step_size_range);
    cp_shader_rendering->SetUniform("Color", m_u_color);
    cp_shader_rendering->SetUniform("ApplyGradientPhongShading", (m_apply_gradient_shading && m_ext_data_manager->GetCurrentGradientTexture()) ? 1 : 0);

    /////////////////////////////
    // 着色
    cp_shader_rendering->SetUniform("BlinnPhongKa", m_ext_rendering_parameters->GetBlinnPhongKambient());
    cp_shader_rendering->SetUniform("BlinnPhongKd", m_ext_rendering_parameters->GetBlinnPhongKdiffuse());
    cp_shader_rendering->SetUniform("BlinnPhongKs", m_ext_rendering_parameters->GetBlinnPhongKspecular());
    cp_shader_rendering->SetUniform("BlinnPhongShininess", m_ext_rendering_parameters->GetBlinnPhongNshininess());
    cp_shader_rendering->SetUniform("BlinnPhongIspecular", m_ext_rendering_parameters->GetLightSourceSpecular());
    cp_shader_rendering->SetUniform("LightSourcePosition", m_ext_rendering_parameters->GetBlinnPhongLightingPosition());

    // 绑定所有Uniform
    cp_shader_rendering->BindUniforms();

    gl::Shader::Unbind();
    gl::ExitOnGLError("CustomRayCasting1PassIsodfsAdapt: After Update.");
    return true;
}


void CustomRayCasting1PassIsodfsAdapt::Redraw()
{
    m_rdr_frame_to_screen.ClearTexture();

    cp_shader_rendering->Bind();
    m_rdr_frame_to_screen.BindImageTexture();

    cp_shader_rendering->Dispatch();
    gl::ComputeShader::Unbind();

    m_rdr_frame_to_screen.Draw();
}


void CustomRayCasting1PassIsodfsAdapt::FillParameterSpace(ParameterSpace& pspace)
{
    pspace.ClearParameterDimensions();
    pspace.AddParameterDimension(new ParameterRangeFloat("StepSizeSmall", &m_u_step_size_small, 0.01f, 0.25f, 0.05f));
    pspace.AddParameterDimension(new ParameterRangeFloat("StepSizeLarge", &m_u_step_size_large, 0.25f, 2.0f, 0.25f));
    pspace.AddParameterDimension(new ParameterRangeFloat("StepSizeRange", &m_u_step_size_range, 0.05f, 0.26f, 0.05f));
}


void CustomRayCasting1PassIsodfsAdapt::SetImGuiComponents()
{
    ImGui::Separator();

    ImGui::Text("Isovalue: ");
    if (ImGui::DragFloat("###CustomRayCasting1PassIsodfsAdaptUIIsovalue", &m_u_isovalue, 0.01f, 0.01f, 100.0f, "%.2f"))
    {
        m_u_isovalue = std::max(std::min(m_u_isovalue, 100.0f), 0.01f); // 当通过键盘输入时，ImGui 不会处理最小值/最大值。
        SetOutdated();
    }

    if (ImGui::ColorEdit4("Color", &m_u_color[0]))
    {
        SetOutdated();
    }

    ImGui::Text("Step Size Small: ");
    if (ImGui::DragFloat("###CustomRayCasting1PassIsodfsAdaptUIIntegrationStepSizeSmall", &m_u_step_size_small, 0.005f, 0.01f, 1.0f, "%.2f"))
    {
        m_u_step_size_small = std::max(std::min(m_u_step_size_small, 1.0f), 0.01f);
        SetOutdated();
    }

    ImGui::Text("Step Size Large: ");
    if (ImGui::DragFloat("###CustomRayCasting1PassIsodfsAdaptUIIntegrationStepSizeLarge", &m_u_step_size_large, 0.01f, 0.05f, 5.0f, "%.2f"))
    {
        m_u_step_size_large = std::max(std::min(m_u_step_size_large, 5.0f), 0.05f);
        SetOutdated();
    }

    ImGui::Text("Step Size Range: ");
    if (ImGui::DragFloat("###CustomRayCasting1PassIsodfsAdaptUIIntegrationStepSizeRange", &m_u_step_size_range, 0.01f, 0.05f, 0.5f, "%.2f"))
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
