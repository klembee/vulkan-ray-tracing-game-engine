//
// Created by cleme on 2020-02-03.
//

#include "Model.hpp"


#include <assimp/mesh.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/gtx/string_cast.inl>
#include <glm/gtc/type_ptr.hpp>

const aiNodeAnim* findNodeAnim(const aiAnimation *animation, std::string nodeName){
    for(uint32_t i = 0 ; i < animation->mNumChannels ; i++){
        const aiNodeAnim* pNodeAnim = animation->mChannels[i];
        if(std::string(pNodeAnim->mNodeName.data) == nodeName){
            return pNodeAnim;
        }
    }

    return nullptr;

//    throw std::runtime_error("Could not find animation node with specified name.");
}

aiMatrix4x4 calcInterpolatedScaling(float animationTime, const aiNodeAnim *nodeanim){
    aiMatrix4x4 matrix;
    aiVector3D scale = nodeanim->mScalingKeys[0].mValue;

    if(nodeanim->mNumScalingKeys == 1){
        aiMatrix4x4::Scaling(scale, matrix);
        return matrix;
    }

    uint32_t scaleIndex = 0;
    for(size_t i = 0 ; i < nodeanim->mNumScalingKeys - 1 ; i++){
        if(animationTime < (float)nodeanim->mScalingKeys[i + 1].mTime){
            scaleIndex = i;
        }
    }
    uint32_t nextScaleIndex = scaleIndex + 1;

    float deltaTime = nodeanim->mScalingKeys[nextScaleIndex].mTime - nodeanim->mScalingKeys[scaleIndex].mTime;
    float factor = (animationTime - (float)nodeanim->mScalingKeys[scaleIndex].mTime) / deltaTime;

    aiVector3D end = nodeanim->mScalingKeys[nextScaleIndex].mValue;
    aiVector3D start = nodeanim->mScalingKeys[scaleIndex].mValue;

    scale = (start + factor * (end - start));

    aiMatrix4x4::Scaling(scale, matrix);
    return matrix;
}

aiMatrix4x4 calcInterpolatedRotation(float animationTime, const aiNodeAnim *nodeanim){
    aiQuaternion rotationQ;

    if(nodeanim->mNumRotationKeys == 1){
        rotationQ = nodeanim->mRotationKeys[0].mValue;
    }else{
        //Find the rotation
        uint32_t rotationIndex = 0;
        for(size_t i = 0 ; i < nodeanim->mNumRotationKeys - 1 ; i++){
            if(animationTime < (float)nodeanim->mRotationKeys[i + 1].mTime){
                rotationIndex = i;
                break;
            }
        }
        uint32_t nextRotationIndex = (rotationIndex + 1) % nodeanim->mNumRotationKeys;

        aiQuatKey currentFrame = nodeanim->mRotationKeys[rotationIndex];
        aiQuatKey nextFrame = nodeanim->mRotationKeys[nextRotationIndex];

        float delta = (animationTime - (float)currentFrame.mTime / (float)(nextFrame.mTime - currentFrame.mTime));

        const aiQuaternion &startRotationQ = currentFrame.mValue;
        const aiQuaternion &endRotationQ = nextFrame.mValue;

        aiQuaternion::Interpolate(rotationQ, startRotationQ, endRotationQ, delta);
        rotationQ = rotationQ.Normalize();
    }

    return aiMatrix4x4(rotationQ.GetMatrix());
}

aiMatrix4x4 calcInterpolatedPosition(float animationTime, const aiNodeAnim *nodeanim){
    aiVector3D translation;

    if(nodeanim->mNumPositionKeys == 1){
        translation = nodeanim->mPositionKeys[0].mValue;
    }else{
        uint32_t translationIndex = 0;
        for(size_t i = 0 ; i < nodeanim->mNumPositionKeys - 1;i++){
            if(animationTime < (float)nodeanim->mPositionKeys[i + 1].mTime){
                translationIndex = i;
                break;
            }
        }
        uint32_t nextTranslationIndex = (translationIndex + 1) % nodeanim->mNumPositionKeys;

        aiVectorKey currentFrame = nodeanim->mPositionKeys[translationIndex];
        aiVectorKey nextFrame = nodeanim->mPositionKeys[nextTranslationIndex];

        float delta = (animationTime - (float)currentFrame.mTime) / (float)(nextFrame.mTime - currentFrame.mTime);

        const aiVector3D &startPosition = currentFrame.mValue;
        const aiVector3D &nextPosition = nextFrame.mValue;

        translation = startPosition + (nextPosition - startPosition) * delta;
    }


    aiMatrix4x4 mat;
    aiMatrix4x4::Translation(translation, mat);
    return mat;
}

Model::Model(Application *application, VkDevice &device, glm::vec3 position){
    this->application = application;
    this->device = device;

    this->loadModel("../models/man/BaseMesh_Anim.fbx");
//    this->loadModel("../models/elf/Elf01_Stand.obj");

    this->modelMatrix = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(0.05f, 0.05f, 0.05f)), position);
    this->modelMatrix = glm::rotate(this->modelMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
}

void Model::loadModel(std::string path) {

    this->scene = importer.ReadFile(path,
            aiProcess_Triangulate|
                    aiProcess_FlipUVs);


    this->globalInverseTransform = this->scene->mRootNode->mTransformation;
    this->globalInverseTransform.Inverse();

    uint32_t numVertices = 0;
    for(size_t meshIndex = 0 ; meshIndex < scene->mNumMeshes ; meshIndex++){
        numVertices += scene->mMeshes[meshIndex]->mNumVertices;
    }

    this->vertices.resize(numVertices);
    this->bones.resize(numVertices);

    uint32_t vertexOffset = 0;
    for(size_t meshIndex = 0 ; meshIndex < scene->mNumMeshes; meshIndex++){
        const aiMesh *mesh = scene->mMeshes[meshIndex];

        //Load the bones
        if(mesh->HasBones()){
            for(size_t i = 0; i < mesh->mNumBones ; i++){
                aiBone *bone = mesh->mBones[i];
                uint32_t boneIndex = 0;
                std::string nodeName(bone->mName.data);

                if(this->boneMapping.find(nodeName) == this->boneMapping.end()){
                    boneIndex = this->numberOfBones;
                    this->numberOfBones++;

                    BoneInfo bi;
                    this->boneInfos.push_back(bi);

                    this->boneInfos[boneIndex].boneOffset = bone->mOffsetMatrix;
                    this->boneMapping[nodeName] = boneIndex;
                }else{
                    boneIndex = this->boneMapping[nodeName];
                }

                for(uint32_t j = 0 ; j < bone->mNumWeights ; j++){
                    uint32_t vertexId = vertexOffset + bone->mWeights[j].mVertexId;
                    float weight = bone->mWeights[j].mWeight;

                    this->bones[vertexId].addBoneData(boneIndex, weight);
                }

            }

            //Attach the bone information to the vertices
            for(size_t boneId = 0 ; boneId < this->bones.size(); boneId++){
                VertexBoneData boneData = this->bones[boneId];
                this->vertices[boneId].boneIds = glm::vec4(boneData.ids[0], boneData.ids[1], boneData.ids[2], boneData.ids[3]);
                this->vertices[boneId].boneWeights = glm::vec4(boneData.weights[0], boneData.weights[1], boneData.weights[2], boneData.weights[3]);
            }
        }

        const aiVector3D zero3D(0.0f, 0.0f, 0.0f);
        for(size_t vertexIndex = 0 ; vertexIndex < mesh->mNumVertices ; vertexIndex++){
            const aiVector3D *pPos = &mesh->mVertices[vertexIndex];
            const aiVector3D *pNormal = &mesh->mNormals[vertexIndex];
            const aiVector3D *pTexCoord = mesh->HasTextureCoords(0) ? &mesh->mTextureCoords[0][vertexIndex] : &zero3D;

            Vertex vertex = this->vertices[vertexIndex];
            vertex.pos = glm::vec3(pPos->x, pPos->y, pPos->z);
            vertex.normal = glm::vec3(pNormal->x, pNormal->y, pNormal->z);
            vertex.texCoord = glm::vec2(pTexCoord->x, pTexCoord->y);
            vertex.texId = mesh->mMaterialIndex;

            this->vertices[vertexIndex] = vertex;
        }

        //Retrieve face data
        for(size_t faceIndex = 0 ; faceIndex < mesh->mNumFaces ; faceIndex++){
            const aiFace &face = mesh->mFaces[faceIndex];
            assert(face.mNumIndices == 3);

            this->indices.push_back(face.mIndices[0]);
            this->indices.push_back(face.mIndices[1]);
            this->indices.push_back(face.mIndices[2]);
        }

        vertexOffset += mesh->mNumVertices;
    }

    //Load the materials
    std::string::size_type SlashIndex = path.find_last_of("/");
    std::string Dir;

    if (SlashIndex == std::string::npos) {
        Dir = ".";
    }
    else if (SlashIndex == 0) {
        Dir = "/";
    }
    else {
        Dir = path.substr(0, SlashIndex);
    }


    for(size_t i = 0 ; i < scene->mNumMaterials ; i++){
        aiMaterial *material = scene->mMaterials[i];

        if(material->GetTextureCount(aiTextureType_DIFFUSE) > 0){
            aiString path;

            if(material->GetTexture(aiTextureType_DIFFUSE, 0, &path, nullptr, nullptr, nullptr, nullptr, nullptr) == AI_SUCCESS){
                std::string fullPath = Dir + "/" + path.data;

                if(strcmp(path.data, ".") == -1) {
                    this->textures.push_back(Texture(this->application, this->device, fullPath));
                }else{
                    this->textures.push_back(Texture(this->application, this->device));
                }
            }else{
                this->textures.push_back(Texture(this->application, this->device));
            }
        }else{
            this->textures.push_back(Texture(this->application, this->device));
        }
    }
}

void Model::createDescriptorSets() {
    uint32_t nbFrameBuffers = application->getSwapChainImagesCount();

    //Create the descriptor pool
    std::array<VkDescriptorPoolSize, 4> poolSizes = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    poolSizes[0].descriptorCount = nbFrameBuffers;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = nbFrameBuffers;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[2].descriptorCount = nbFrameBuffers * 100;
    poolSizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[3].descriptorCount = nbFrameBuffers * 8;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(nbFrameBuffers);
    poolInfo.flags = 0;

    if(vkCreateDescriptorPool(this->device, &poolInfo, nullptr, &this->descriptorPool) != VK_SUCCESS){
        throw std::runtime_error("Failed to create descriptor pool.");
    }

    //Create the descriptor sets
    std::vector<VkDescriptorSetLayout> layouts(nbFrameBuffers, application->getDescriptorSetLayout());
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = this->descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();

    //swapchainImages
    this->descriptorSets.resize(nbFrameBuffers);
    if(vkAllocateDescriptorSets(this->device, &allocInfo, this->descriptorSets.data()) != VK_SUCCESS){
        throw std::runtime_error("Failed to allocate descriptor sets");
    }

    for(size_t frameBufferIndex = 0 ; frameBufferIndex < nbFrameBuffers; frameBufferIndex++){
        VkDescriptorBufferInfo modelBufferInfo = {};
        modelBufferInfo.buffer = application->getModelUniformBuffer(frameBufferIndex);
        modelBufferInfo.offset = 0;
        modelBufferInfo.range = sizeof(glm::mat4);

        VkDescriptorBufferInfo viewBufferInfo = {};
        viewBufferInfo.buffer = application->getCameraUniformBuffer(frameBufferIndex);
        viewBufferInfo.offset = 0;
        viewBufferInfo.range = sizeof(CameraMatrices);

        std::vector<VkDescriptorImageInfo> imageInfos;
        for(size_t imageInfoIndex = 0 ; imageInfoIndex < 8 ; imageInfoIndex++){
            VkDescriptorImageInfo info = {};
            if(imageInfoIndex < this->textures.size()) {
                Texture &texture = this->textures[imageInfoIndex];

                info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                info.imageView = texture.getImageView();
                info.sampler = texture.getTextureSampler();

            }else{
                Texture &texture = this->textures[0];
                info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                info.imageView = texture.getImageView();
                info.sampler = texture.getTextureSampler();
            }
            imageInfos.push_back(info);
        }

//        std::vector<VkDescriptorBufferInfo> boneMatricesInfos;
//        for(size_t boneIndex = 0; boneIndex < 100; boneIndex++){
//            VkDescriptorBufferInfo boneBufferInfo = {};
//            boneBufferInfo.buffer = application->getBoneUniformBuffer(frameBufferIndex);
//            boneBufferInfo.offset = 0;
//            boneBufferInfo.range = sizeof(glm::mat4);
//            boneMatricesInfos.push_back(boneBufferInfo);
//        }
        VkDescriptorBufferInfo boneBufferInfo = {};
        boneBufferInfo.buffer = application->getBoneUniformBuffer(frameBufferIndex);
        boneBufferInfo.offset = 0;
        boneBufferInfo.range = sizeof(glm::mat4) * 100;

        std::array<VkWriteDescriptorSet, 4> descriptorWrites = {};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = this->descriptorSets[frameBufferIndex];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &modelBufferInfo;
        descriptorWrites[0].pImageInfo = nullptr;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = this->descriptorSets[frameBufferIndex];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &viewBufferInfo;
        descriptorWrites[1].pImageInfo = nullptr;

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = this->descriptorSets[frameBufferIndex];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &boneBufferInfo;
        descriptorWrites[2].pImageInfo = nullptr;

        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = this->descriptorSets[frameBufferIndex];
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[3].descriptorCount = 8;
        descriptorWrites[3].pImageInfo = &imageInfos[0];

        vkUpdateDescriptorSets(this->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

    }
}

void Model::getBoneTransforms(float timeInSeconds, std::vector<glm::mat4> &transforms){
    aiMatrix4x4 identity;

    float ticksPerSecond = this->scene->mAnimations[1]->mTicksPerSecond != 0 ?
                 this->scene->mAnimations[1]->mTicksPerSecond : 25.0f;
    float timeInTicks = timeInSeconds * ticksPerSecond;
    float animationTime = fmod(timeInTicks, this->scene->mAnimations[1]->mDuration);

    this->readNodeHierarchy(animationTime, this->scene->mRootNode, identity);

    transforms.resize(this->numberOfBones);

    for (uint32_t i = 0; i < this->numberOfBones; i++) {
        transforms[i] = glm::transpose(glm::make_mat4(&this->boneInfos[i].finalTransformation.a1));
    }

}

void Model::readNodeHierarchy(float animationTime, const aiNode* pNode, const aiMatrix4x4& parentTransform){
    std::string nodeName(pNode->mName.data);

    const aiAnimation *animation = this->scene->mAnimations[1];

    aiMatrix4x4 nodeTransformation(pNode->mTransformation);

    const aiNodeAnim *nodeanim = findNodeAnim(animation, nodeName);

    if(nodeanim){
        aiMatrix4x4 scalingM = calcInterpolatedScaling(animationTime, nodeanim);
        aiMatrix4x4 rotationM = calcInterpolatedRotation(animationTime, nodeanim);
        aiMatrix4x4 translationM = calcInterpolatedPosition(animationTime, nodeanim);

        nodeTransformation = translationM * rotationM * scalingM;
    }

    aiMatrix4x4 globalTransformation = parentTransform * nodeTransformation;

    if(this->boneMapping.find(nodeName) != this->boneMapping.end()){
        uint32_t boneIndex = this->boneMapping[nodeName];
        this->boneInfos[boneIndex].finalTransformation =
                globalTransformation *
                this->boneInfos[boneIndex].boneOffset;
    }

    for(uint32_t i = 0 ; i < pNode->mNumChildren ; i++){
        this->readNodeHierarchy(animationTime, pNode->mChildren[i], globalTransformation);
    }

}

void Model::init(){
    this->createDescriptorSets();
}

VkDescriptorSet* Model::getDescriptorSet(uint32_t i){
    return &this->descriptorSets[i];
}

glm::mat4 Model::getModelMatrix() {
    return this->modelMatrix;
}

std::vector<Vertex> Model::getVertices(){
    return this->vertices;
}

std::vector<uint32_t> Model::getIndices(){
    return this->indices;
}

void Model::cleanup() {
    for(size_t i = 0 ; i < this->textures.size() ; i++){
        this->textures[i].cleanup();
    }

    vkDestroyDescriptorPool(this->device, this->descriptorPool, nullptr);

}

