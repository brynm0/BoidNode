#version 450

layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoord;
layout(location = 3) in vec3 ViewDir;

layout(location = 0) out vec4 outColor;

// Material properties
struct Material {
  vec4 ambient;
  vec4 diffuse;
  vec4 specular;
  float shininess;
};

// Light properties
struct Light {
  vec4 position;
  vec4 ambient;
  vec4 diffuse;
  vec4 specular;
};

// Wrap Material and Light in uniform blocks
layout(std140, binding = 1) uniform MaterialBlock { Material material; };
layout(std140, binding = 2) uniform LightBlock { Light light; };

void main() {
  vec3 norm = normalize(Normal);
  vec3 viewDir = normalize(ViewDir);

  // Calculate light direction (from fragment to light)
  vec3 lightDir = normalize(light.position.xyz - FragPos);

  // Ambient component
  vec3 ambient = light.ambient.xyz * material.ambient.xyz;

  // Diffuse component
  float diff = max(dot(norm, lightDir.xyz), 0.0);
  vec3 diffuse = light.diffuse.xyz * (diff * material.diffuse.xyz);

  // Blinn-Phong specular component
  vec3 halfwayDir = normalize(lightDir + viewDir);
  float spec = pow(max(dot(norm, halfwayDir), 0.0), material.shininess);
  vec3 specular = light.specular.xyz * (spec * material.specular.xyz);

  // Combine all components
  vec3 result = ambient + diffuse + specular;
  float gamma = 2.2;
  result.rgb = pow(result.rgb, vec3(1.0 / gamma));
  outColor = vec4(result, 1.0);
}