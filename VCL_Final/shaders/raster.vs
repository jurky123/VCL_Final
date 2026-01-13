#version 430 core
layout(location = 0) in
vec3 aPos;
layout(location = 1) in
vec3 aNormal;
layout(location = 2) in
vec2 aTexCoords;

out
vec3 vNormal;
out
vec3 vWorldPos;
out
vec2 vTexCoords;

uniform mat4 view;
uniform mat4 proj;

void main()
{
    vWorldPos = aPos;
    vNormal = aNormal; // 或 normalize(aNormal)
    vTexCoords = aTexCoords;

    gl_Position = proj * view * vec4(vWorldPos, 1.0);
}
