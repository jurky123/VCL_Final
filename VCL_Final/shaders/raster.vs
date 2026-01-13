#version 430 core
layout(location = 0) in
vec3 aPos;
layout(location = 1) in
vec3 aNormal;

uniform mat4 view;
uniform mat4 proj;

out
vec3 FragPos;
out
vec3 Normal;

void main()
{
    FragPos = aPos;
    Normal = aNormal;
    gl_Position = proj * view * vec4(aPos, 1.0);
}
