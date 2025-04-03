#version 450

layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoord;
layout(location = 3) in vec3 ViewDir;

layout(location = 0) out vec4 outColor;

// Material properties
struct Material {
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;
  float shininess;
};

// Light properties
struct Light {
  vec3 position;
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;
};

// Wrap Material and Light in uniform blocks
layout(std140, binding = 0) uniform MaterialBlock { Material material; };

layout(std140, binding = 1) uniform LightBlock { Light light; };

void main() {
  // Normalize the interpolated normal and view direction
  vec3 norm = normalize(Normal);
  vec3 viewDir = normalize(ViewDir);

  // Calculate light direction (from fragment to light)
  vec3 lightDir = normalize(light.position - FragPos);

  // Ambient component
  vec3 ambient = light.ambient * material.ambient;

  // Diffuse component
  float diff = max(dot(norm, lightDir), 0.0);
  vec3 diffuse = light.diffuse * (diff * material.diffuse);

  // Blinn-Phong specular component
  vec3 halfwayDir = normalize(lightDir + viewDir);
  float spec = pow(max(dot(norm, halfwayDir), 0.0), material.shininess);
  vec3 specular = light.specular * (spec * material.specular);

  // Combine all components
  vec3 result = ambient + diffuse + specular;
  outColor = vec4(result, 1.0);
}