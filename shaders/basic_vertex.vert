#version 450

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(std140, binding = 0) uniform UniformBuffer {
  mat4 MVP;
  mat4 Model;
  mat4 View;
  vec4 ViewPos; // Camera position in world space
};

layout(location = 0) out vec3 FragPos;
layout(location = 1) out vec3 Normal;
layout(location = 2) out vec2 TexCoord;
layout(location = 3) out vec3 ViewDir;

void main() {
  gl_Position = MVP * inPosition;

  // Calculate fragment position in world space
  FragPos = vec3(Model * inPosition);

  // Calculate normal in world space
  Normal = mat3(transpose(inverse(Model))) * inNormal;

  // Pass through texture coordinates
  TexCoord = inTexCoord;

  // Calculate view direction (from fragment to camera)
  ViewDir = ViewPos.xyz - FragPos;
  // }
}