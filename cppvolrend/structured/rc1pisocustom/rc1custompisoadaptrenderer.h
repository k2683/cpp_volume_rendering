/**
 * Isosurface raycaster with adaptive step size.
 *
 * @author Tino Weinkauf
**/
#pragma once

#include "../../volrenderbase.h"

class CustomRayCasting1PassIsoAdapt : public BaseVolumeRenderer
{
public:
    CustomRayCasting1PassIsoAdapt();
    virtual ~CustomRayCasting1PassIsoAdapt();

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


    GLuint m_texBlockMin = 0;  
    GLuint m_texBlockMax = 0;  

    glm::vec3 m_blockSize = glm::vec3(8, 8, 8); 
    glm::vec3 m_blockGridRes;                   
    bool CreateBlockMinMaxTextures();
    void DestroyBlockMinMaxTextures();
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

private:
    gl::ComputeShader* cp_geometry_pass;

};


