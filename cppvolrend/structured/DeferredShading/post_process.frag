//post_process.frag
#version 430

in vec2 TexCoords;        // 从顶点着色器传进来的
out vec4 FragColor;       // 片元输出

// 从 C++ 中通过 glUniform1f / glUniform1i / glUniform3f 等传进来
uniform float exposure;
uniform float gamma;

// 片元着色器中最常见的采样方法就是 sampler2D
uniform sampler2D finalImage;
void main() {
    vec4 hdrColor = texture(finalImage, TexCoords);  // 读取完整的rgba
    
    vec3 result = hdrColor.rgb;
    
    // 如果使用HDR
    if(exposure > 0.0) {
        result = vec3(1.0) - exp(-result * exposure);
    }
    if(gamma > 0.0) {
        result = pow(result, vec3(1.0 / gamma));
    }
    
    FragColor = vec4(result, hdrColor.a);  // 保持原始alpha值
}