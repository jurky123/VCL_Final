#version 430 core
in
vec3 vNormal;
in
vec3 vWorldPos;
in
vec2 vTexCoords;

out
vec4 FragColor;

uniform vec3 diffuse;
uniform vec3 specular;
uniform float shiness; // 建议 32~128
uniform vec3 emission;

uniform int u_light_count;
uniform vec3 u_light_positions[16];
uniform vec3 u_light_intensities[16];

uniform vec3 u_cam_pos;

uniform sampler2D u_diffuseTex;
uniform int hasDiffuseTex;
uniform vec3 u_ambient = vec3(0.1); // 调低环境光防止过亮

void main()
{
    vec3 N = normalize(vNormal);
    vec3 V = normalize(u_cam_pos - vWorldPos);

    // 纹理颜色或 diffuse
    vec3 baseColor = diffuse;
    if (hasDiffuseTex == 1)
        baseColor = texture(u_diffuseTex, vTexCoords).rgb;

    // 初始颜色：环境 + 自发光
    vec3 color = baseColor * u_ambient + emission;

    for (int i = 0; i < u_light_count; i++)
    {
        vec3 L = normalize(u_light_positions[i] - vWorldPos);
        float distance = length(u_light_positions[i] - vWorldPos);
        float attenuation = 1.0 / (1.0 + 0.5 * distance + 0.7 * distance * distance);

        // 漫反射
        float diff = max(dot(N, L), 0.0);
        vec3 diffuseTerm = diff * baseColor;

        // Blinn-Phong 高光
        vec3 H = normalize(L + V);
        float spec = pow(max(dot(N, H), 0.0), shiness);
        vec3 specularTerm = spec * specular;

        // 累加光照
        color += (diffuseTerm + specularTerm) * u_light_intensities[i] * attenuation;
    }

    FragColor = vec4(color, 1.0);
}
