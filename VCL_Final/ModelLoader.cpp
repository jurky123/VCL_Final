#define STB_IMAGE_IMPLEMENTATION
#include "ModelLoader.h"
bool ModelLoader::LoadMTL(const std::string& path, std::unordered_map<std::string, Material>& outMaterials) {
    std::ifstream fin(path);
    if (!fin.is_open()) {
        std::cerr << "Failed to open MTL file: " << path << std::endl;
        return false;
    }
    std::string baseDir = std::filesystem::path(path).parent_path().string();
    std::string line;
    Material currentMat;
    std::string currentName;

    while (std::getline(fin, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "newmtl") {
            if (!currentName.empty())
                outMaterials[currentName] = currentMat;
            iss >> currentName;
            currentMat = Material(); // 重置材质
        }
        else if (cmd == "Ka") iss >> currentMat.ambient.r >> currentMat.ambient.g >> currentMat.ambient.b;
        else if (cmd == "Kd") iss >> currentMat.diffuse.r >> currentMat.diffuse.g >> currentMat.diffuse.b;
        else if (cmd == "Ks") iss >> currentMat.specular.r >> currentMat.specular.g >> currentMat.specular.b;
        else if (cmd == "Ns") iss >> currentMat.shiness;
        else if (cmd == "map_Kd") {
            iss >> currentMat.diffuseTexPath;               // 先读取文件名（相对路径或文件名）
            std::string texPath = baseDir + "/" + currentMat.diffuseTexPath; // 构造完整路径
            currentMat.diffuseTex = LoadTexture(texPath);  // 再加载
            // 把完整路径回写回 diffuseTexPath，便于后续调试/使用
            currentMat.diffuseTexPath = texPath;
        }
    }

    if (!currentName.empty())
        outMaterials[currentName] = currentMat;

    return true;
}
bool ModelLoader::LoadModel(const std::string& path,
    const glm::mat4& transform,
    uint32_t defaultMaterialID, bool forceMaterial)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_JoinIdenticalVertices
    );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "ASSIMP ERROR: "
            << importer.GetErrorString()
            << " for file: " << path << std::endl;
        return false;
    }

    std::string objDir = std::filesystem::path(path).parent_path().string();

    // --- 解析 Assimp 材质，抓取 Diffuse 颜色与 Diffuse 贴图（若有） ---
    std::unordered_map<std::string, Material> mtlMaterials;
    if (scene->mMaterials && scene->mNumMaterials > 0) {
        for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
            aiMaterial* aiMat = scene->mMaterials[i];
            std::string name = aiMat->GetName().C_Str();
            Material mat;
            // 读取漫反射颜色（若存在）
            aiColor3D color(1.0f, 1.0f, 1.0f);
            if (AI_SUCCESS == aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
                mat.diffuse = glm::vec3(color.r, color.g, color.b);
            }
            // 读取漫反射贴图（若存在）
            aiString texPath;
            if (AI_SUCCESS == aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath)) {
                mat.diffuseTexPath = objDir + "/" + texPath.C_Str();
                std::cout << "[ASSIMP] Mat '" << name << "' diffuseTexPath = " << mat.diffuseTexPath << "\n";
            }
            else {
                std::cout << "[ASSIMP] Mat '" << name << "' diffuse color = ("
                    << mat.diffuse.r << "," << mat.diffuse.g << "," << mat.diffuse.b << ")\n";
            }
            mtlMaterials[name] = mat;
        }
    }

    // 默认材质
    if (defaultMaterialID == UINT32_MAX || defaultMaterialID >= materials.size()) {
        Material defaultMat(glm::vec3(1.0f));
        materials.push_back(defaultMat);
        defaultMaterialID = static_cast<uint32_t>(materials.size() - 1);

        // 必须同步 nextMaterialID，保证后续新建材质的 id 与 materials 的索引一致
        nextMaterialID = static_cast<uint32_t>(materials.size());
    }

    glm::mat4 rootTransform = transform * aiMatrix4x4ToGlm(scene->mRootNode->mTransformation);
    ProcessNode(scene->mRootNode, scene, rootTransform, defaultMaterialID, mtlMaterials,forceMaterial);

    return true;
}

bool ModelLoader::LoadSceneFromYAML(const std::string& yamlPath) {
    triangles.clear();
    materials.clear();
    materialNameToID.clear();
    nextMaterialID =0;
    lights.clear(); // 新增灯光容器

    YAML::Node config;
    try {
        config = YAML::LoadFile(yamlPath);
    }
    catch (const YAML::BadFile& e) {
        std::cerr << "YAML BadFile Exception: " << e.what() << " | Path: " << yamlPath << std::endl;
        return false;
    }
    catch (const YAML::ParserException& e) {
        std::cerr << "YAML Parser Exception: " << e.what() << " | Path: " << yamlPath << std::endl;
        return false;
    }
    catch (const std::exception& e) {
        std::cerr << "YAML Exception: " << e.what() << " | Path: " << yamlPath << std::endl;
        return false;
    }
    catch (...) {
        std::cerr << "Unknown exception while loading YAML: " << yamlPath << std::endl;
        return false;
    }

    /* ---------- Camera ---------- */
    if (config["Cameras"]) {
        auto cam = config["Cameras"][0];
        initialEye = glm::vec3(
            cam["Eye"][0].as<float>(),
            cam["Eye"][1].as<float>(),
            cam["Eye"][2].as<float>()
        );
        initialTarget = glm::vec3(
            cam["Target"][0].as<float>(),
            cam["Target"][1].as<float>(),
            cam["Target"][2].as<float>()
        );
        if (cam["Fovy"])
            initialFovy = cam["Fovy"].as<float>();
    }

    /* ---------- Lights ---------- */
    if (config["Lights"]) {
        for (auto const& l : config["Lights"]) {
            Light light; // ensure we initialize members below
            // Initialize to safe defaults to avoid using uninitialized memory
            light.type = Light::Point;
            light.position = glm::vec3(0.0f);
            light.direction = glm::vec3(0.0f, -1.0f,0.0f);
            light.intensity = glm::vec3(1.0f);

            std::string type = l["Type"].as<std::string>();
            if (type == "Point") {
                light.type = Light::Point;
                auto pos = l["Position"];
                light.position = glm::vec3(pos[0].as<float>(), pos[1].as<float>(), pos[2].as<float>());
            }
            else if (type == "Directional") {
                light.type = Light::Directional;
                auto dir = l["Direction"];
                light.direction = glm::vec3(dir[0].as<float>(), dir[1].as<float>(), dir[2].as<float>());
            }

            if (l["Intensity"]) {
                auto inten = l["Intensity"];
                light.intensity = glm::vec3(inten[0].as<float>(), inten[1].as<float>(), inten[2].as<float>());
            }
            else {
                light.intensity = glm::vec3(1.0f);
            }

            lights.push_back(light);
        }
    }

    /* ---------- Materials ---------- */
    if (config["Materials"]) {
        for (auto const& mat : config["Materials"]) {
            std::string name = mat["Name"].as<std::string>();

            Material m; // 默认全白
            m.diffuse = glm::vec3(1.0f);
            m.emission =0.0f;
            m.metallic =0.0f;
            m.shiness =0.5f;

            if (mat["Diffuse"]) {
                m.diffuse = glm::vec3(
                    mat["Diffuse"][0].as<float>(),
                    mat["Diffuse"][1].as<float>(),
                    mat["Diffuse"][2].as<float>()
                );
            }
            if (mat["Specular"]) {
                m.specular = glm::vec3(
                    mat["Specular"][0].as<float>(),
                    mat["Specular"][1].as<float>(),
                    mat["Specular"][2].as<float>()
                );
            }
            if (mat["Emission"])
                m.emission = mat["Emission"].as<float>();
            if (mat["Metallic"]) 
                m.metallic = mat["Metallic"].as<float>();
            if (mat["Shiness"]) 
                m.shiness = mat["Shiness"].as<float>();

            uint32_t id = nextMaterialID++;
            materialNameToID[name] = id;
            materials.push_back(m);
        }
    }

    /* ---------- Models / ComplexModels ---------- */
    YAML::Node modelsNode;
    if (config["Models"]) modelsNode = config["Models"];
    else if (config["ComplexModels"]) modelsNode = config["ComplexModels"];
    else {
        std::cerr << "No Models or ComplexModels key in YAML!" << std::endl;
        return false;
    }

    std::string baseDir = std::filesystem::path(yamlPath).parent_path().string();

    for (auto const& node : modelsNode) {
        if (!node["Mesh"]) continue;
        std::string meshPath = baseDir + "/" + node["Mesh"].as<std::string>();

        uint32_t materialID =0;
        bool forceThisModel = false; // 默认为 false，使用模型贴图

        if (node["Material"]) {
            // 如果 YAML 明确写了 Material 字段，我们认为用户想覆盖模型贴图
            std::string matName = node["Material"].as<std::string>();
            if (materialNameToID.count(matName)) {
                materialID = materialNameToID[matName];
                forceThisModel = true; // 开启强制模式
            }
        }

        // 调用 LoadModel 时传入 forceThisModel
        LoadModel(meshPath, glm::mat4(1.0f), materialID, forceThisModel);
    }

    return true;
}

void ModelLoader::SetupRasterMesh(const std::vector<Triangle>& tris, const Material& mat, Mesh& mesh) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    unsigned int idx = 0;

    for (auto& tri : tris) {
        // 每个顶点: position + baseColor
        auto addVertex = [&](const glm::vec4& v) {
            vertices.push_back(v.x);
            vertices.push_back(v.y);
            vertices.push_back(v.z);
            vertices.push_back(mat.diffuse.r);
            vertices.push_back(mat.diffuse.g);
            vertices.push_back(mat.diffuse.b);
            };
        addVertex(tri.v0);
        addVertex(tri.v1);
        addVertex(tri.v2);

        indices.push_back(idx++);
        indices.push_back(idx++);
        indices.push_back(idx++);
    }

    mesh.indexCount = (int)indices.size();
    mesh.material = mat;

    glGenVertexArrays(1, &mesh.VAO);
    glGenBuffers(1, &mesh.VBO);
    glGenBuffers(1, &mesh.EBO);

    glBindVertexArray(mesh.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); // baseColor
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
}
void ModelLoader::CreateRasterMeshes(std::vector<Mesh>& meshes) {
    meshes.clear();

    // 按材质分组（并做边界检查）
    std::unordered_map<uint32_t, std::vector<Triangle>> matGroups;
    for (auto& tri : triangles) {
        uint32_t mid = tri.material_id;
        if (mid >= materials.size()) {
            std::cerr << "[Warning] Triangle has invalid material_id " << mid << ", fallback to 0\n";
            mid = 0;
        }
        matGroups[mid].push_back(tri);
    }

    // 为保证可预测顺序，提取并排序 material id keys
    std::vector<uint32_t> keys;
    keys.reserve(matGroups.size());
    for (auto const& kv : matGroups) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());

    for (auto matID : keys) {
        auto& tris = matGroups[matID];

        // ConvertToMesh 已经将 mesh.material_index = material_index，但这里明确设置以防止不一致
        Mesh mesh = ConvertToMesh(tris, matID);
        mesh.material_index = static_cast<int>(matID);

        // debug log
        

        meshes.push_back(mesh);
    }
}

Mesh ModelLoader::ConvertToMesh(
    const std::vector<Triangle>& tris,
    int material_index
) {
    Mesh mesh;

    // ⭐ 核心：同时保存 material 和 index
    mesh.material_index = material_index;
    mesh.material = materials[material_index];

    for (const auto& t : tris) {
        MeshVertex v0{
            glm::vec3(t.v0),
            glm::vec3(t.n0),
            glm::vec2(t.uv0)
        };

        MeshVertex v1{
            glm::vec3(t.v1),
            glm::vec3(t.n1),
            glm::vec2(t.uv1)
        };

        MeshVertex v2{
            glm::vec3(t.v2),
            glm::vec3(t.n2),
            glm::vec2(t.uv2)
        };

        uint32_t startIndex = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(v0);
        mesh.vertices.push_back(v1);
        mesh.vertices.push_back(v2);

        mesh.indices.push_back(startIndex);
        mesh.indices.push_back(startIndex + 1);
        mesh.indices.push_back(startIndex + 2);
    }

    mesh.indexCount = mesh.indices.size();
    mesh.setupGL();

    return mesh;
}

void ModelLoader::ProcessNode(aiNode* node,
    const aiScene* scene,
    const glm::mat4& transform,
    uint32_t defaultMaterialID,
    const std::unordered_map<std::string, Material>& mtlMaterials,
    bool forceMaterial) // 必须传入此参数
{
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

        // 初始设为 YAML 传入的默认材质 ID
        uint32_t materialID = defaultMaterialID;

        // --- 核心逻辑：只有在不强制要求使用 YAML 材质时，才解析模型自带材质 ---
        if (!forceMaterial && mesh->mMaterialIndex < scene->mNumMaterials) {
            aiMaterial* aiMat = scene->mMaterials[mesh->mMaterialIndex];
            std::string matName = aiMat->GetName().C_Str();

            // 1. 尝试复用已经加载过的材质 ID
            if (materialNameToID.count(matName)) {
                materialID = materialNameToID[matName];
            }
            // 2. 否则，如果该材质存在于之前 LoadMTL 预加载的结果中，则创建新材质
            else if (mtlMaterials.count(matName)) {
                Material mat = mtlMaterials.at(matName);

                // 处理贴图加载
                if (!mat.diffuseTexPath.empty()) {
                    mat.diffuseTex = LoadTexture(mat.diffuseTexPath);
                }

                materialID = nextMaterialID++;
                mat.id = static_cast<int>(materialID);
                materials.push_back(mat);
                materialNameToID[matName] = materialID;

                std::cout << "[Compatible] Created model material: " << matName << " ID: " << materialID << "\n";
            }
        }
        else if (forceMaterial) {
            // 如果是强制材质模式，不进入上述逻辑，直接保持使用 defaultMaterialID
            // 这通常发生在 YAML 中明确写了 Material: "XXX" 的时候
        }

        // 边界安全检查
        if (materials.empty() || materialID >= (uint32_t)materials.size()) {
            materialID = 0;
        }

        // 提取三角形数据
        ExtractTriangles(mesh, transform, materialID);
    }

    // 递归子节点，注意透传 forceMaterial 参数
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        ProcessNode(node->mChildren[i], scene, transform, defaultMaterialID, mtlMaterials, forceMaterial);
    }
}
GLuint ModelLoader::LoadTexture(const std::string& path) {
    // 如果之前已经加载过，直接返回
    if (loadedTextures.count(path)) {
        GLuint cached = loadedTextures[path];
        std::cout << "[Texture] Reuse cached: " << path << " -> texID=" << cached << "\n";
        return cached;
    }

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true); // 根据需要翻转图片
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return 0;
    }

    GLenum format;
    if (nrChannels == 1) format = GL_RED;
    else if (nrChannels == 3) format = GL_RGB;
    else if (nrChannels == 4) format = GL_RGBA;
    else {
        stbi_image_free(data);
        std::cerr << "Unsupported number of channels (" << nrChannels << ") in texture: " << path << std::endl;
        return 0;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // 设置默认采样参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    loadedTextures[path] = textureID;

    
    return textureID;
}
void ModelLoader::ExtractTriangles(aiMesh* mesh,
    const glm::mat4& transform,
    uint32_t materialID) {

    bool hasUV = mesh->HasTextureCoords(0);
    if (hasUV)
        std::cout << "Mesh " << mesh->mName.C_Str() << " has UVs" << std::endl;
    else
        std::cout << "Mesh " << mesh->mName.C_Str() << " has NO UVs" << std::endl;

    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        const aiFace& face = mesh->mFaces[i];
        if (face.mNumIndices != 3)
            continue;

        Triangle tri;

        auto getVertex = [&](unsigned int idx) {
            unsigned int vertIdx = face.mIndices[idx];

            // --- 顶点位置 ---
            glm::vec4 pos = transform * glm::vec4(
                mesh->mVertices[vertIdx].x,
                mesh->mVertices[vertIdx].y,
                mesh->mVertices[vertIdx].z,
                1.0f
            );

            // --- 法线 ---
            glm::vec4 normal(0, 1, 0, 0);
            if (mesh->HasNormals()) {
                aiVector3D n = mesh->mNormals[vertIdx];
                normal = glm::vec4(glm::normalize(glm::vec3(n.x, n.y, n.z)), 0.0f);
            }

            // --- UV ---
            glm::vec4 uv(0.0f, 0.0f, 0.0f, 0.0f);
            if (hasUV && vertIdx < mesh->mNumVertices) {
                aiVector3D t = mesh->mTextureCoords[0][vertIdx];
                uv.x = t.x;
                uv.y = t.y;
                // 打印调试 UV
                // std::cout << "Vertex " << vertIdx << " UV: " << uv.x << ", " << uv.y << std::endl;
            }

            return std::make_tuple(pos, normal, uv);
        };

        std::tie(tri.v0, tri.n0, tri.uv0) = getVertex(0);
        std::tie(tri.v1, tri.n1, tri.uv1) = getVertex(1);
        std::tie(tri.v2, tri.n2, tri.uv2) = getVertex(2);

        // 材质 ID
        tri.material_id = materialID;
        tri.pad0 = tri.pad1 = tri.pad2 = 0; // padding

        triangles.push_back(tri);
    }
}
