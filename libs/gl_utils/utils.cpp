#include "utils.h"
#include <GL/glew.h>
#include <cerrno>
#include <string>
#include <codecvt>
#include <locale>
#include <fstream>

namespace gl
{
  void ExitOnGLError(const char* error_message)
  {
    const GLenum ErrorValue = glGetError();

    if (ErrorValue != GL_NO_ERROR)
    {
      const char* APPEND_DETAIL_STRING = ": %s\n";
      const size_t APPEND_LENGTH = strlen(APPEND_DETAIL_STRING) + 1;
      const size_t message_length = strlen(error_message);
      char* display_message = (char*)malloc(message_length + APPEND_LENGTH);

      memcpy(display_message, error_message, message_length);
      memcpy(&display_message[message_length], APPEND_DETAIL_STRING, APPEND_LENGTH);

      fprintf(stderr, display_message, glewGetErrorString(ErrorValue));
      
      free(display_message);
      exit(EXIT_FAILURE);
    }
  }

  void ShaderInfoLog(GLuint obj)
  {
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;

    glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &infologLength);

    if (infologLength > 0)
    {
      infoLog = (char *)malloc(infologLength);
      glGetShaderInfoLog(obj, infologLength, &charsWritten, infoLog);
      printf("%s\n",infoLog);
      free(infoLog);
    }else
    {
      printf("Shader Info Log: OK\n");
    }
  }

  void ProgramInfoLog(GLuint obj)
  {
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;

    glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &infologLength);

    if (infologLength > 0)
    {
      infoLog = (char *)malloc(infologLength);
      glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
      printf("%s\n",infoLog);
      free(infoLog);
    }else
    {
      printf("Program Info Log: OK\n");
    }
  }

  GLuint LoadShader(const char* file_name, GLenum shader_type)
  {
    GLuint shader_id = 0;
    FILE* file;
    errno_t err;

    long file_size = -1;
    char* glsl_source;

    if (((err = fopen_s(&file, file_name, "rb")) != 0) &&
      0 == fseek(file, 0, SEEK_END) &&
      -1 != (file_size = ftell(file)))
    {
      rewind(file);

      if (NULL != (glsl_source = (char*)malloc(file_size + 1)))
      {
        if (file_size == (long)fread(glsl_source, sizeof(char), file_size, file))
        {
          glsl_source[file_size] = '\0';

          if (0 != (shader_id = glCreateShader(shader_type)))
          {
            const char* const_glsl_source = glsl_source;
            glShaderSource(shader_id, 1, &const_glsl_source, NULL);
            glCompileShader(shader_id);

            const GLenum ErrorValue = glGetError();
            if (ErrorValue != GL_NO_ERROR)
            {
              const char* APPEND_DETAIL_STRING = ": %s\n";
              const size_t APPEND_LENGTH = strlen(APPEND_DETAIL_STRING) + 1;
              const size_t message_length = strlen("Could not compile a shader");
              char* display_message = (char*)malloc(message_length + APPEND_LENGTH);

              memcpy(display_message, "Could not compile a shader", message_length);
              memcpy(&display_message[message_length], APPEND_DETAIL_STRING, APPEND_LENGTH);

              //fprintf(stderr, display_message, gluErrorString(ErrorValue));

              free(display_message);
              exit(EXIT_FAILURE);
            }
          }
          else
            fprintf(stderr, "ERROR: Could not create a shader.\n");
        }
        else
          fprintf(stderr, "ERROR: Could not read file %s\n", file_name);

        free(glsl_source);
      }
      else
        fprintf(stderr, "ERROR: Could not allocate %i bytes.\n", file_size);

      fclose(file);
    }
    else
      fprintf(stderr, "ERROR: Could not open file %s\n", file_name);

    return shader_id;
  }

  char* TextFileRead(const char* file_name)
  {
      #ifdef _DEBUG
        printf("File Name TextFileRead \"%s\"\n", file_name);
      #endif

      std::ifstream in(file_name, std::ios::in | std::ios::binary);
      if (in)
      {
          std::string contents;
          in.seekg(0, std::ios::end);
          contents.resize(in.tellg());
          in.seekg(0, std::ios::beg);
          in.read(&contents[0], contents.size());
          in.close();

          // UTF-8 to wstring
          std::wstring wstr;
          try
          {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> wconv;
            wstr = wconv.from_bytes(contents.c_str());
          }
          // thrown by std::wstring_convert.to_bytes() for bad conversions
          catch (std::range_error& exception)
          {
            printf("\nFile \"%s\" could not be converted from UTF-8 to wstring (wide string)", file_name);
            getchar();
            exit(1);
          }

          // wstring to string
          char* result = (char*) malloc (sizeof (char)* (wstr.size() + 1));
          std::use_facet<std::ctype<wchar_t>>(std::locale(".1252")).narrow(wstr.data(), wstr.data() + wstr.size(), ' ', result);
          result[wstr.size()] = '\0';

          return result;
      }

    printf ("\nFile \"%s\" not found", file_name);
    getchar ();
    exit (1);
    //throw(errno);
  }

  glm::ivec3 ComputeShaderGetNumberOfGroups (int w, int h, int d)
  {
    glm::ivec3 num_groups;
    // x
    int disp_w = 2;
    while (disp_w < w) disp_w = disp_w * 2;
    num_groups.x = disp_w / 8;

    // y
    int disp_h = 2;
    while (disp_h < h) disp_h = disp_h * 2;
    num_groups.y = disp_h / 8;

    // z
    int disp_d = 2;
    while (disp_d < d) disp_d = disp_d * 2;
    num_groups.z = disp_d / 8;
  
    return num_groups;
  }

  void Vertex3f (glm::vec3 v)
  {
    glVertex3f(v.x, v.y, v.z);
  }

  void Rotatef (float angle, glm::vec3 v)
  {
    glRotatef(angle, v.x, v.y, v.z);
  }

  void Color3f (glm::vec3 color)
  {
    glColor3f(color.x, color.y, color.z);
  }

  void Translatef (glm::vec3 t)
  {
    glTranslatef(t.x, t.y, t.z);
  }
}