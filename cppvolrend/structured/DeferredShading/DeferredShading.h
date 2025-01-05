/**
 * Isosurface raycaster with adaptive step size.
 *
 * @author Tino Weinkauf
**/
#pragma once

#include "../../volrenderbase.h"

class DeferredShading : public BaseVolumeRenderer
{
public:
    DeferredShading();
    virtual ~DeferredShading();
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
    void DeferredShading::LightingPass(vis::Camera* camera);
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
    void DeferredShading::InitQuad();
    // 私有渲染目标
    GLuint m_outputTexture;      // 最终输出纹理
    GLuint m_lightingBuffer;     // 光照计算缓冲
        // 全屏四边形的顶点数组对象和缓冲
    GLuint m_quad_vao;  // 顶点数组对象
    GLuint lightingBuffer;
    GLuint m_quad_vbo;  // 顶点缓冲对象
};


