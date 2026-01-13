#version 430 core
out
vec4 FragColor;
in
vec2 vUV;

uniform sampler2D u_texOutput;

void main()
{
    FragColor = texture(u_texOutput, vUV);
}
