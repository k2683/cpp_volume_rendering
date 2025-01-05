#include "../../defines.h"
#include "rc1prenderer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

#include <vis_utils/camera.h>

#include <volvis_utils/utils.h>
#include <math_utils/utils.h>

#include "imgui.h"
#include "imgui_impl_glut.h"
#include "imgui_impl_opengl2.h"

RayCasting1Pass::RayCasting1Pass ()
  : m_glsl_transfer_function(nullptr)
  , cp_geometry_pass(nullptr)
  , m_u_step_size(0.5f)
  , m_apply_gradient_shading(false)
{
#ifdef MULTISAMPLE_AVAILABLE
  vr_pixel_multiscaling_support = true;
#endif
}

RayCasting1Pass::~RayCasting1Pass ()
{
  Clean();
}

void RayCasting1Pass::Clean ()
{
  if (m_glsl_transfer_function) delete m_glsl_transfer_function;
  m_glsl_transfer_function = nullptr;

  DestroyRenderingPass();

  BaseVolumeRenderer::Clean();
}

void RayCasting1Pass::ReloadShaders ()
{
  cp_geometry_pass->Reload();
  m_rdr_frame_to_screen.ClearShaders();
}

bool RayCasting1Pass::Init (int swidth, int sheight)
{
  if (IsBuilt()) Clean();

  if (m_ext_data_manager->GetCurrentVolumeTexture() == nullptr) return false;
  m_glsl_transfer_function = m_ext_data_manager->GetCurrentTransferFunction()->GenerateTexture_1D_RGBt();
  
  // Create Rendering Buffers and Shaders
  CreateRenderingPass();
  gl::ExitOnGLError("RayCasting1Pass: Error on Preparing Models and Shaders");
  
  // estimate initial integration step
  glm::dvec3 sv = m_ext_data_manager->GetCurrentStructuredVolume()->GetScale();
  m_u_step_size = float((0.5f / glm::sqrt(3.0f)) * glm::sqrt(sv.x * sv.x + sv.y * sv.y + sv.z * sv.z));

  Reshape(swidth, sheight);

  SetBuilt(true);
  SetOutdated();
  return true;
}

bool RayCasting1Pass::Update (vis::Camera* camera)
{
  cp_geometry_pass->Bind();

  // MULTISAMPLE
  if (IsPixelMultiScalingSupported() && GetCurrentMultiScalingMode() > 0)
  {
    cp_geometry_pass->RecomputeNumberOfGroups(m_rdr_frame_to_screen.GetWidth(),
                                                 m_rdr_frame_to_screen.GetHeight(), 0);
  }
  else
  {
    cp_geometry_pass->RecomputeNumberOfGroups(m_ext_rendering_parameters->GetScreenWidth(),
                                                 m_ext_rendering_parameters->GetScreenHeight(), 0);
  }

  cp_geometry_pass->SetUniform("CameraEye", camera->GetEye());
  cp_geometry_pass->BindUniform("CameraEye");

  cp_geometry_pass->SetUniform("u_CameraLookAt", camera->LookAt());
  cp_geometry_pass->BindUniform("u_CameraLookAt");

  cp_geometry_pass->SetUniform("ProjectionMatrix", camera->Projection());
  cp_geometry_pass->BindUniform("ProjectionMatrix");

  cp_geometry_pass->SetUniform("u_TanCameraFovY", (float)tan(DEGREE_TO_RADIANS(camera->GetFovY()) / 2.0));
  cp_geometry_pass->BindUniform("u_TanCameraFovY");

  cp_geometry_pass->SetUniform("u_CameraAspectRatio", camera->GetAspectRatio());
  cp_geometry_pass->BindUniform("u_CameraAspectRatio");

  cp_geometry_pass->SetUniform("StepSize", m_u_step_size);
  cp_geometry_pass->BindUniform("StepSize");

  cp_geometry_pass->SetUniform("ApplyOcclusion", 1);
  cp_geometry_pass->BindUniform("ApplyOcclusion");

  cp_geometry_pass->SetUniform("ApplyShadow", 1);
  cp_geometry_pass->BindUniform("ApplyShadow");

  cp_geometry_pass->SetUniform("ApplyGradientPhongShading", (m_apply_gradient_shading && m_ext_data_manager->GetCurrentGradientTexture()) ? 1 : 0);
  cp_geometry_pass->BindUniform("ApplyGradientPhongShading");

  cp_geometry_pass->SetUniform("BlinnPhongKa", m_ext_rendering_parameters->GetBlinnPhongKambient());
  cp_geometry_pass->BindUniform("BlinnPhongKa");
  cp_geometry_pass->SetUniform("BlinnPhongKd", m_ext_rendering_parameters->GetBlinnPhongKdiffuse());
  cp_geometry_pass->BindUniform("BlinnPhongKd");
  cp_geometry_pass->SetUniform("BlinnPhongKs", m_ext_rendering_parameters->GetBlinnPhongKspecular());
  cp_geometry_pass->BindUniform("BlinnPhongKs");
  cp_geometry_pass->SetUniform("BlinnPhongShininess", m_ext_rendering_parameters->GetBlinnPhongNshininess());
  cp_geometry_pass->BindUniform("BlinnPhongShininess");

  cp_geometry_pass->SetUniform("BlinnPhongIspecular", m_ext_rendering_parameters->GetLightSourceSpecular());
  cp_geometry_pass->BindUniform("BlinnPhongIspecular");

  cp_geometry_pass->SetUniform("WorldEyePos", camera->GetEye());
  cp_geometry_pass->BindUniform("WorldEyePos");

  cp_geometry_pass->SetUniform("LightSourcePosition", m_ext_rendering_parameters->GetBlinnPhongLightingPosition());
  cp_geometry_pass->BindUniform("LightSourcePosition");

  cp_geometry_pass->BindUniforms();

  gl::Shader::Unbind();
  gl::ExitOnGLError("RayCasting1Pass: After Update.");
  return true;
}

void RayCasting1Pass::Redraw ()
{
  m_rdr_frame_to_screen.ClearTexture();

  cp_geometry_pass->Bind();
  m_rdr_frame_to_screen.BindImageTexture();

  cp_geometry_pass->Dispatch();
  gl::ComputeShader::Unbind();
 
  m_rdr_frame_to_screen.Draw();
}

void RayCasting1Pass::MultiSampleRedraw ()
{
  m_rdr_frame_to_screen.ClearTexture();

  cp_geometry_pass->Bind();
  m_rdr_frame_to_screen.BindImageTexture();

  cp_geometry_pass->Dispatch();
  gl::ComputeShader::Unbind();

  m_rdr_frame_to_screen.DrawMultiSampleHigherResolutionMode();
}

void RayCasting1Pass::DownScalingRedraw ()
{
  m_rdr_frame_to_screen.ClearTexture();

  cp_geometry_pass->Bind();
  m_rdr_frame_to_screen.BindImageTexture();

  cp_geometry_pass->Dispatch();
  gl::ComputeShader::Unbind();

  m_rdr_frame_to_screen.DrawHigherResolutionWithDownScale();
}

void RayCasting1Pass::UpScalingRedraw ()
{
  m_rdr_frame_to_screen.ClearTexture();

  cp_geometry_pass->Bind();
  m_rdr_frame_to_screen.BindImageTexture();

  cp_geometry_pass->Dispatch();
  gl::ComputeShader::Unbind();

  m_rdr_frame_to_screen.DrawLowerResolutionWithUpScale();
}

void RayCasting1Pass::SetImGuiComponents ()
{
  ImGui::Separator();
  ImGui::Text("Step Size: ");
  if (ImGui::DragFloat("###RayCasting1PassUIIntegrationStepSize", &m_u_step_size, 0.01f, 0.01f, 100.0f, "%.2f"))
  {
    m_u_step_size = std::max(std::min(m_u_step_size, 100.0f), 0.01f); //When entering with keyboard, ImGui does not take care of this.
    SetOutdated();
  }
  
  AddImGuiMultiSampleOptions();
  
  if (m_ext_data_manager->GetCurrentGradientTexture())
  {
    ImGui::Separator();
    if (ImGui::Checkbox("Apply Gradient Shading", &m_apply_gradient_shading))
    {
      // Delete current uniform
      cp_geometry_pass->ClearUniform("TexVolumeGradient");

      if (m_apply_gradient_shading && m_ext_data_manager->GetCurrentGradientTexture())
      {
        cp_geometry_pass->Bind();
        cp_geometry_pass->SetUniformTexture3D("TexVolumeGradient", m_ext_data_manager->GetCurrentGradientTexture()->GetTextureID(), 3);
        cp_geometry_pass->BindUniform("TexVolumeGradient");
        gl::ComputeShader::Unbind();
      }
      SetOutdated();
    }
    ImGui::Separator();
  }
}

void RayCasting1Pass::FillParameterSpace(ParameterSpace& pspace)
{
  pspace.ClearParameterDimensions();
  pspace.AddParameterDimension(new ParameterRangeFloat("StepSize", &m_u_step_size, 0.2, 2.0, 0.1));
}

void RayCasting1Pass::CreateRenderingPass ()
{
  glm::vec3 vol_resolution = glm::vec3(m_ext_data_manager->GetCurrentStructuredVolume()->GetWidth() ,
                                       m_ext_data_manager->GetCurrentStructuredVolume()->GetHeight(),
                                       m_ext_data_manager->GetCurrentStructuredVolume()->GetDepth() );

  glm::vec3 vol_voxelsize = glm::vec3(m_ext_data_manager->GetCurrentStructuredVolume()->GetScaleX(),
                                      m_ext_data_manager->GetCurrentStructuredVolume()->GetScaleY(),
                                      m_ext_data_manager->GetCurrentStructuredVolume()->GetScaleZ());

  glm::vec3 vol_aabb = vol_resolution * vol_voxelsize;
 
  cp_geometry_pass = new gl::ComputeShader();
  cp_geometry_pass->AddShaderFile(CPPVOLREND_DIR"structured/_common_shaders/ray_bbox_intersection.comp");
  cp_geometry_pass->AddShaderFile(CPPVOLREND_DIR"structured/rc1pass/ray_marching_1p.comp");
  cp_geometry_pass->LoadAndLink();
  cp_geometry_pass->Bind();

  if (m_ext_data_manager->GetCurrentVolumeTexture())
    cp_geometry_pass->SetUniformTexture3D("TexVolume", m_ext_data_manager->GetCurrentVolumeTexture()->GetTextureID(), 1);
  if (m_glsl_transfer_function)
    cp_geometry_pass->SetUniformTexture1D("TexTransferFunc", m_glsl_transfer_function->GetTextureID(), 2);
  if (m_apply_gradient_shading && m_ext_data_manager->GetCurrentGradientTexture())
    cp_geometry_pass->SetUniformTexture3D("TexVolumeGradient", m_ext_data_manager->GetCurrentGradientTexture()->GetTextureID(), 3);

  cp_geometry_pass->SetUniform("VolumeGridResolution", vol_resolution);
  cp_geometry_pass->SetUniform("VolumeVoxelSize", vol_voxelsize);
  cp_geometry_pass->SetUniform("VolumeGridSize", vol_aabb);

  cp_geometry_pass->BindUniforms();
  cp_geometry_pass->Unbind();
}

void RayCasting1Pass::DestroyRenderingPass ()
{
  if (cp_geometry_pass) delete cp_geometry_pass;
  cp_geometry_pass = nullptr;

  gl::ExitOnGLError("Could not destroy shaders");
}

void RayCasting1Pass::RecreateRenderingPass ()
{
  DestroyRenderingPass();
  CreateRenderingPass();

  gl::ExitOnGLError("Could not recreate rendering pass");
}
