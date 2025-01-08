/**
 * Isosurface raycaster with adaptive step size.
 *
 * @author Tino Weinkauf
**/
#pragma once

#include "../../volrenderbase.h"

class AdvancedDeferredShading : public BaseVolumeRenderer
{
public:
    AdvancedDeferredShading();
    virtual ~AdvancedDeferredShading();
    vis::Camera* mycamera;
    virtual const char* GetName();
    virtual const char* GetAbbreviationName();
    virtual vis::GRID_VOLUME_DATA_TYPE GetDataTypeSupport();

    virtual void Clean();
    virtual bool Init(int shader_width, int shader_height);
    virtual void ReloadShaders();

    virtual bool Update(vis::Camera* camera);
    virtual void Redraw();

    virtual void FillParameterSpace(ParameterSpace& pspace) override;

    virtual void SetImGuiComponents();
    void ComputeBlocksFromVolume(vis::StructuredGridVolume* volume,
        glm::vec3 numBlocks,
        std::vector<float>& minValues,
        std::vector<float>& maxValues,
        glm::vec3 vol_voxelsize);
    void AdvancedDeferredShading::LightingPass(vis::Camera* camera);
    GLuint m_texBlockMin = 0;  
    GLuint m_texBlockMax = 0;  

    glm::vec3 m_blockSize = glm::vec3(8, 8, 8); 
    glm::vec3 m_blockGridRes;                   

    gl::ComputeShader* cp_geometry_pass;
    gl::ComputeShader* cp_lighting_pass;
    gl::PipelineShader* cp_post_process;
    GLuint outputTexture;

    // G-buffer textures
    GLuint gPosition;
    GLuint gNormal;
    GLuint gDiffuse;
    GLuint gDepth;
    GLuint m_lighting_texture;  // 存储光照结果的纹理

    // Framebuffers
    GLuint gBuffer;
    gl::ComputeShader* cp_shader_rendering;

protected:
    float m_u_isovalue;

    /// Step size near the isovalue.
    float m_u_step_size_small;

    /// Step size when the data value is not near the isovalue.
    float m_u_step_size_large;

    /// Withing this range around the isovalue, the small step size will be chosen.
    /// Outside of this range, the large step size will be used.
    float m_u_step_size_range;

    glm::vec4 m_u_color;
    bool m_apply_gradient_shading;

public:
    // G-Buffer结构体 - 将相关纹理组织在一起
    struct GBuffer {
        GLuint gPosition;  // 位置缓冲
        GLuint gNormal;    // 法线缓冲
        GLuint gDiffuse;   // 漫反射颜色缓冲
        GLuint gDepth;     // 深度缓冲
        GLuint frameBuffer; // G-Buffer帧缓冲
    } m_gbuffer;

    // 分块结构体 - 空间跳跃优化相关
    struct BlockStructure {
        GLuint texMin;           // 块最小值纹理
        GLuint texMax;           // 块最大值纹理
        glm::vec3 blockSize;     // 块大小
        glm::vec3 blockGridRes;  // 块网格分辨率
    } m_blocks;

    // 渲染管线着色器
    struct Shaders {
        gl::ComputeShader* geometry;     // 几何pass
        gl::ComputeShader* lighting;     // 光照pass
        gl::ComputeShader* postProcess;  // 后处理pass
    } m_shaders;

    // 私有辅助函数
    void CreateGBuffer(int width, int height);
    void LightingPass();
    void RenderQuad();
    void UpdateGeometryPassUniforms(vis::Camera* camera);
    void UpdateLightingPassUniforms(vis::Camera* camera);
    void AdvancedDeferredShading::InitQuad();
    // 私有渲染目标
    GLuint m_outputTexture;      // 最终输出纹理
    GLuint m_lightingBuffer;     // 光照计算缓冲
        // 全屏四边形的顶点数组对象和缓冲
    GLuint m_quad_vao;  // 顶点数组对象
    GLuint lightingBuffer;
    GLuint m_quad_vbo;  // 顶点缓冲对象


    // 曲率相关纹理
    GLuint gCurvatureMax;      // 最大主曲率 κ1
    GLuint gCurvatureMin;      // 最小主曲率 κ2
    GLuint gCurvatureDir;      // 主曲率方向

    // 传输函数纹理
    GLuint curvatureColorMap;  // 曲率颜色映射
    GLuint ridgeValleyMap;     // 脊线和谷线检测
    GLuint noiseTexture;       // 用于流场可视化的噪声纹理

    // 新增着色器
    gl::ComputeShader* cp_curvature_pass;  // 计算曲率的pass

    // 着色参数
    float m_flow_speed = 1.0f;
    float m_flow_scale = 10.0f;
    bool m_show_ridges = true;
    bool m_show_valleys = true;
    float m_accessibility_strength = 0.5f;
    void AdvancedDeferredShading::InitCurvaturePass();
    void AdvancedDeferredShading::CreateTransferFunctions();

    void AdvancedDeferredShading::CurvaturePass();

    // HSV color control
    float m_hsv_h = 0.0f;    // Hue [0-360]
    float m_hsv_s = 0.6f;    // Saturation [0-1]
    float m_hsv_v = 0.8f;    // Value [0-1]

    // Surface appearance control
    float m_surface_roughness = 0.5f;    // Surface roughness [0-1]
    float m_surface_metallic = 0.0f;     // Surface metallic property [0-1]
    float m_surface_opacity = 1.0f;      // Surface opacity [0-1]
    float m_edge_strength = 1.0f;        // Edge highlighting strength [0-1]
    float m_ambient_occlusion = 0.5f;    // Ambient occlusion strength [0-1]

    // Material effects
    bool m_enable_fresnel = false;       // Enable Fresnel effect
    bool m_enable_rim_light = false;     // Enable rim lighting
    float m_rim_power = 2.0f;           // Rim light power
    float m_rim_intensity = 0.5f;       // Rim light intensity

    // Texture controls
    float m_texture_scale = 1.0f;        // Texture scaling factor
    float m_texture_rotation = 0.0f;     // Texture rotation angle
    bool m_enable_procedural_texture = false;  // Enable procedural texturing
    float s;    //饱和度
    float v;    //亮度
    GLuint gCurvatureDebug;
    float m_current_time;  // 跟踪时间
    float m_start_time;    // 记录开始时间

};


