#version 430 core
in
vec3 FragPos;
in
vec3 Normal;

uniform vec3 baseColor;
uniform float emission;

out
vec4 FragColor;

void main()
{
    vec3 color = baseColor;
    color += emission; // 简单发光
    // 简单 Lambert 光照
    vec3 lightDir = normalize(vec3(1, 1, 1));
    float diff = max(dot(Normal, lightDir), 0.0);
    color *= diff + 0.1; // 环境光 0.1
    FragColor = vec4(color, 1.0);
}
