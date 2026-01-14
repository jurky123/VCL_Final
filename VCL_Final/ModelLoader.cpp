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
            iss >> currentMat.diffuseTexPath;               // 先读取文件名
            std::string texPath = baseDir + "/" + currentMat.diffuseTexPath;
            currentMat.diffuseTex = LoadTexture(texPath);  // 再加载
        }
    }

    if (!currentName.empty())
        outMaterials[currentName] = currentMat;

    return true;
}
bool ModelLoader::LoadModel(const std::string& path,
    const glm::mat4& transform,
    uint32_t defaultMaterialID)
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

    // --- 解析 MTL 材质 ---
    std::unordered_map<std::string, Material> mtlMaterials;
    if (scene->mMaterials && scene->mNumMaterials > 0) {
        for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
            aiMaterial* aiMat = scene->mMaterials[i];
            aiString texPath;
            if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
                // 有贴图的情况
                Material mat;
                mat.diffuseTexPath = objDir + "/" + texPath.C_Str();
                mtlMaterials[aiMat->GetName().C_Str()] = mat;
            }
        }
    }

    // 默认材质
    if (defaultMaterialID == UINT32_MAX || defaultMaterialID >= materials.size()) {
        Material defaultMat(glm::vec3(1.0f));
        materials.push_back(defaultMat);
        defaultMaterialID = static_cast<uint32_t>(materials.size() - 1);
    }

    glm::mat4 rootTransform = transform * aiMatrix4x4ToGlm(scene->mRootNode->mTransformation);
    ProcessNode(scene->mRootNode, scene, rootTransform, defaultMaterialID, mtlMaterials);

    return true;
}

bool ModelLoader::LoadSceneFromYAML(const std::string& yamlPath) {
    triangles.clear();
    materials.clear();
    materialNameToID.clear();
    nextMaterialID = 0;
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
            Light light;
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
            m.emission = 0.0f;
            m.metallic = 0.0f;
            m.shiness = 0.5f;

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

        uint32_t materialID = 0; // 默认材质
        if (node["Material"]) {
            std::string matName = node["Material"].as<std::string>();
            if (materialNameToID.count(matName))
                materialID = materialNameToID[matName];
        }

        // 调用 LoadModel，会自动用默认材质 ID=0 如果没指定
        LoadModel(meshPath, glm::mat4(1.0f), materialID);
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
        std::cout << "Created mesh for material " << matID
            << " with " << mesh.vertices.size() << " vertices, "
            << mesh.indices.size() << " indices" << std::endl;

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
    const std::unordered_map<std::string, Material>& mtlMaterials)
{
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        uint32_t materialID = defaultMaterialID;

        if (mesh->mMaterialIndex < scene->mNumMaterials) {
            aiMaterial* aiMat = scene->mMaterials[mesh->mMaterialIndex];
            std::string matName = aiMat->GetName().C_Str();

            if (mtlMaterials.count(matName)) {
                Material mat = mtlMaterials.at(matName);

                // 确保 diffuseTex 赋值给 mat.diffuseTex
                if (!mat.diffuseTexPath.empty()) {
                    GLuint texID = LoadTexture(mat.diffuseTexPath);
                    mat.diffuseTex = texID;
                }

                // 分配新的材料 id 并把 id 写回到 materials 中
                materialID = nextMaterialID++;
                materials.push_back(mat);
                materials.back().id = static_cast<int>(materialID);
                materialNameToID[matName] = materialID;
            }
        }

        ExtractTriangles(mesh, transform, materialID);
        // 运行时调试断言（debug 有用）
        assert(materialID < materials.size());
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++)
        ProcessNode(node->mChildren[i], scene, transform, defaultMaterialID, mtlMaterials);
}
GLuint ModelLoader::LoadTexture(const std::string& path) {
    // 如果之前已经加载过，直接返回
    if (loadedTextures.count(path)) {
        return loadedTextures[path];
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

