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
    GLuint m_lighting_texture;  // �洢���ս��������

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
    // G-Buffer�ṹ�� - �����������֯��һ��
    struct GBuffer {
        GLuint gPosition;  // λ�û���
        GLuint gNormal;    // ���߻���
        GLuint gDiffuse;   // ��������ɫ����
        GLuint gDepth;     // ��Ȼ���
        GLuint frameBuffer; // G-Buffer֡����
    } m_gbuffer;

    // �ֿ�ṹ�� - �ռ���Ծ�Ż����
    struct BlockStructure {
        GLuint texMin;           // ����Сֵ����
        GLuint texMax;           // �����ֵ����
        glm::vec3 blockSize;     // ���С
        glm::vec3 blockGridRes;  // ������ֱ���
    } m_blocks;

    // ��Ⱦ������ɫ��
    struct Shaders {
        gl::ComputeShader* geometry;     // ����pass
        gl::ComputeShader* lighting;     // ����pass
        gl::ComputeShader* postProcess;  // ����pass
    } m_shaders;

    // ˽�и�������
    void CreateGBuffer(int width, int height);
    void LightingPass();
    void RenderQuad();
    void UpdateGeometryPassUniforms(vis::Camera* camera);
    void UpdateLightingPassUniforms(vis::Camera* camera);
    void DeferredShading::InitQuad();
    // ˽����ȾĿ��
    GLuint m_outputTexture;      // �����������
    GLuint m_lightingBuffer;     // ���ռ��㻺��
        // ȫ���ı��εĶ����������ͻ���
    GLuint m_quad_vao;  // �����������
    GLuint lightingBuffer;
    GLuint m_quad_vbo;  // ���㻺�����
};


