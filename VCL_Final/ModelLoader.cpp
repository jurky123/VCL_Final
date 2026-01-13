#include "ModelLoader.h"
bool ModelLoader::LoadModel(const std::string& path,
    const glm::mat4& transform,
    uint32_t materialID) {
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

    // --- 默认材质处理 ---
    if (materialID == UINT32_MAX || materialID >= materials.size()) {
        // 创建一个默认全白材质
        Material defaultMat(glm::vec3(1.0f), 0.0f, 0.0f, 0.5f);
        materials.push_back(defaultMat);
        materialID = static_cast<uint32_t>(materials.size() - 1);
    }

    glm::mat4 rootTransform =
        transform * aiMatrix4x4ToGlm(scene->mRootNode->mTransformation);

    ProcessNode(scene->mRootNode, scene, rootTransform, materialID);
    return true;
}
bool ModelLoader::LoadSceneFromYAML(const std::string& yamlPath) {
    triangles.clear();
    materials.clear();
    materialNameToID.clear();
    nextMaterialID = 0;

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

    /* ---------- Materials ---------- */
    if (config["Materials"]) {
        for (auto const& mat : config["Materials"]) {
            std::string name = mat["Name"].as<std::string>();

            Material m; // 默认全白
            m.baseColor = glm::vec3(1.0f);
            m.emission = 0.0f;
            m.metallic = 0.0f;
            m.roughness = 0.5f;

            if (mat["Diffuse"]) {
                m.baseColor = glm::vec3(
                    mat["Diffuse"][0].as<float>(),
                    mat["Diffuse"][1].as<float>(),
                    mat["Diffuse"][2].as<float>()
                );
            }

            if (mat["Emission"])
                m.emission = mat["Emission"].as<float>();

            if (mat["Metallic"])
                m.metallic = mat["Metallic"].as<float>();

            if (mat["Roughness"])
                m.roughness = mat["Roughness"].as<float>();

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
            vertices.push_back(mat.baseColor.r);
            vertices.push_back(mat.baseColor.g);
            vertices.push_back(mat.baseColor.b);
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
    // 按材质分组
    std::unordered_map<uint32_t, std::vector<Triangle>> matGroups;
    for (auto& tri : triangles) {
        matGroups[tri.material_id].push_back(tri);
    }

    for (auto& [matID, tris] : matGroups) {
        Mesh mesh;
        SetupRasterMesh(tris, materials[matID], mesh);
        meshes.push_back(mesh);
    }
}
Mesh ModelLoader::ConvertToMesh(const std::vector<Triangle>& tris, const Material& mat) {
    Mesh mesh;
    mesh.material = mat;

    for (const auto& t : tris) {
        // Triangle 的 v0/v1/v2 是 vec4，没有法线/uv，所以给默认值
        MeshVertex v0{ glm::vec3(t.v0), glm::vec3(0,1,0), glm::vec2(0,0) };
        MeshVertex v1{ glm::vec3(t.v1), glm::vec3(0,1,0), glm::vec2(0,0) };
        MeshVertex v2{ glm::vec3(t.v2), glm::vec3(0,1,0), glm::vec2(0,0) };

        uint32_t startIndex = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(v0);
        mesh.vertices.push_back(v1);
        mesh.vertices.push_back(v2);

        mesh.indices.push_back(startIndex);
        mesh.indices.push_back(startIndex + 1);
        mesh.indices.push_back(startIndex + 2);
    }

    mesh.indexCount = mesh.indices.size();
    mesh.setupGL();  // 上传 VAO/VBO/EBO

    return mesh;
}
void ModelLoader::ProcessNode(aiNode* node,
    const aiScene* scene,
    const glm::mat4& transform,
    uint32_t materialID) {

    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        ExtractTriangles(mesh, transform, materialID);
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        glm::mat4 childTransform =
            transform * aiMatrix4x4ToGlm(node->mChildren[i]->mTransformation);
        ProcessNode(node->mChildren[i], scene, childTransform, materialID);
    }
}
void ModelLoader::ExtractTriangles(aiMesh* mesh,
    const glm::mat4& transform,
    uint32_t materialID) {

    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        const aiFace& face = mesh->mFaces[i];
        if (face.mNumIndices != 3)
            continue;

        Triangle tri;

        auto apply = [&](unsigned int idx) {
            aiVector3D v = mesh->mVertices[face.mIndices[idx]];
            return transform * glm::vec4(v.x, v.y, v.z, 1.0f);
            };

        tri.v0 = apply(0);
        tri.v1 = apply(1);
        tri.v2 = apply(2);
        tri.material_id = materialID;

        triangles.push_back(tri);
    }
}