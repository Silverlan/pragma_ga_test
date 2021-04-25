/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan
 */

#include "stdafx_shared.h"
#include "pragma/model/model.h"
#include "pragma/model/modelmesh.h"
#include "pragma/physics/collisionmesh.h"
#include "pragma/model/animation/fanim.h"
#include "pragma/physics/physsoftbodyinfo.hpp"
#include "pragma/model/animation/vertex_animation.hpp"
#include "pragma/model/animation/flex_animation.hpp"
#include "pragma/model/animation/skeleton.h"
#include "pragma/file_formats/wmd.h"
#include "pragma/asset/util_asset.hpp"
#include <fsys/filesystem.h>
#include <sharedutils/util_file.h>
#include <udm.hpp>

#define INDEX_OFFSET_INDEX_SIZE sizeof(uint64_t)
#define INDEX_OFFSET_MODEL_DATA 0
#define INDEX_OFFSET_MODEL_MESHES (INDEX_OFFSET_MODEL_DATA +1)
#define INDEX_OFFSET_LOD_DATA (INDEX_OFFSET_MODEL_MESHES +1)
#define INDEX_OFFSET_BODYGROUPS (INDEX_OFFSET_LOD_DATA +1)
#define INDEX_OFFSET_JOINTS (INDEX_OFFSET_BODYGROUPS +1)
#define INDEX_OFFSET_COLLISION_MESHES (INDEX_OFFSET_JOINTS +1)
#define INDEX_OFFSET_BONES (INDEX_OFFSET_COLLISION_MESHES +1)
#define INDEX_OFFSET_ANIMATIONS (INDEX_OFFSET_BONES +1)
#define INDEX_OFFSET_VERTEX_ANIMATIONS (INDEX_OFFSET_ANIMATIONS +1)
#define INDEX_OFFSET_FLEX_CONTROLLERS (INDEX_OFFSET_VERTEX_ANIMATIONS +1)
#define INDEX_OFFSET_FLEXES (INDEX_OFFSET_FLEX_CONTROLLERS +1)
#define INDEX_OFFSET_PHONEMES (INDEX_OFFSET_FLEXES +1)
#define INDEX_OFFSET_IK_CONTROLLERS (INDEX_OFFSET_PHONEMES +1)
#define INDEX_OFFSET_EYEBALLS (INDEX_OFFSET_IK_CONTROLLERS +1)
#define INDEX_OFFSET_FLEX_ANIMATIONS (INDEX_OFFSET_EYEBALLS +1)
#pragma optimize("",off)
static void write_offset(VFilePtrReal f,uint64_t offIndex)
{
	auto cur = f->Tell();
	f->Seek(offIndex);
	f->Write<uint64_t>(cur);
	f->Seek(cur);
}

static void to_vertex_list(ModelMesh &mesh,std::vector<Vertex> &vertices,std::unordered_map<ModelSubMesh*,std::vector<uint32_t>> &vertexIds)
{
	vertices.reserve(mesh.GetVertexCount());
	for(auto &subMesh : mesh.GetSubMeshes())
	{
		auto &verts = subMesh->GetVertices();
		auto itMesh = vertexIds.insert(std::remove_reference_t<decltype(vertexIds)>::value_type(subMesh.get(),{})).first;
		auto &vertIds = itMesh->second;
		vertIds.reserve(verts.size());
		for(auto &v : verts)
		{
			auto it = std::find(vertices.begin(),vertices.end(),v);
			if(it == vertices.end())
			{
				vertices.push_back({v.position,v.uv,v.normal,v.tangent});
				it = vertices.end() -1;
			}
			vertIds.push_back(static_cast<uint32_t>(it -vertices.begin()));
		}
	}
}

struct MeshBoneWeight
{
	MeshBoneWeight(uint64_t vId,float w)
		: vertId(vId),weight(w)
	{}
	uint64_t vertId;
	float weight;
};
static void to_vertex_weight_list(ModelMesh &mesh,std::unordered_map<uint32_t,std::vector<MeshBoneWeight>> &boneWeights,const std::unordered_map<ModelSubMesh*,std::vector<uint32_t>> &vertexIds)
{
	// TODO: Has to be the same order as 'to_vertex_list'!!
	for(auto &subMesh : mesh.GetSubMeshes())
	{
		auto &meshVerts = vertexIds.find(subMesh.get())->second;
		auto &weights = subMesh->GetVertexWeights();
		for(auto i=decltype(weights.size()){0};i<weights.size();++i)
		{
			auto vertId = meshVerts[i];
			auto &vertexWeight = weights[i];
			auto &weights = vertexWeight.weights;
			auto &boneId = vertexWeight.boneIds;
			for(uint8_t i=0;i<4;++i)
			{
				if(boneId[i] == -1)
					continue;
				auto it = boneWeights.find(boneId[i]);
				if(it == boneWeights.end())
					it = boneWeights.insert(std::remove_reference_t<decltype(boneWeights)>::value_type(boneId[i],{})).first;
				it->second.push_back({vertId,weights[i]});
			}
		}
	}
}

std::shared_ptr<Model> Model::Copy(Game *game,CopyFlags copyFlags) const
{
	auto fCreateModel = static_cast<std::shared_ptr<Model>(Game::*)(bool) const>(&Game::CreateModel);
	auto mdl = (game->*fCreateModel)(false);
	if(mdl == nullptr)
		return nullptr;
	mdl->m_metaInfo = m_metaInfo;
	mdl->m_bValid = m_bValid;
	mdl->m_mass = m_mass;
	mdl->m_meshCount = m_meshCount;
	mdl->m_subMeshCount = m_subMeshCount;
	mdl->m_vertexCount = m_vertexCount;
	mdl->m_triangleCount = m_triangleCount;
	mdl->m_maxEyeDeflection = m_maxEyeDeflection;
	mdl->m_eyeballs = m_eyeballs;
	mdl->m_flexAnimationNames = m_flexAnimationNames;
	mdl->m_blendControllers = m_blendControllers;
	mdl->m_meshGroups = m_meshGroups;
	mdl->m_bodyGroups = m_bodyGroups;
	mdl->m_hitboxes = m_hitboxes;
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	mdl->m_reference = Frame::Create(*m_reference);
#endif
	mdl->m_name = m_name;
	mdl->m_bAllMaterialsLoaded = true;
	mdl->m_animations = m_animations;
	mdl->m_flexAnimations = m_flexAnimations;
	mdl->m_animationIDs = m_animationIDs;
	mdl->m_skeleton = std::make_unique<Skeleton>(*m_skeleton);
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	mdl->m_bindPose = m_bindPose;
#endif
	mdl->m_eyeOffset = m_eyeOffset;
	mdl->m_collisionMin = m_collisionMin;
	mdl->m_collisionMax = m_collisionMax;
	mdl->m_renderMin = m_renderMin;
	mdl->m_renderMax = m_renderMax;
	mdl->m_collisionMeshes = m_collisionMeshes;
	mdl->m_joints = m_joints;
	mdl->m_baseMeshes = m_baseMeshes;
	mdl->m_lods = m_lods;
	mdl->m_attachments = m_attachments;
	mdl->m_objectAttachments = m_objectAttachments;
	mdl->m_materials = m_materials;
	mdl->m_textureGroups = m_textureGroups;
	mdl->m_phonemeMap = m_phonemeMap;
	mdl->m_flexControllers = m_flexControllers;
	mdl->m_flexes = m_flexes;
	for(auto &ikController : mdl->m_ikControllers)
		ikController = std::make_shared<IKController>(*ikController);
	std::unordered_map<ModelMesh*,ModelMesh*> oldMeshToNewMesh;
	std::unordered_map<ModelSubMesh*,ModelSubMesh*> oldSubMeshToNewSubMesh;
	if((copyFlags &CopyFlags::CopyMeshesBit) != CopyFlags::None)
	{
		for(auto &meshGroup : mdl->m_meshGroups)
		{
			auto newMeshGroup = ModelMeshGroup::Create(meshGroup->GetName());
			static_assert(sizeof(ModelMeshGroup) == 72,"Update this function when making changes to this class!");
			newMeshGroup->GetMeshes() = meshGroup->GetMeshes();
			for(auto &mesh : newMeshGroup->GetMeshes())
			{
				auto newMesh = mesh->Copy();
				oldMeshToNewMesh[mesh.get()] = newMesh.get();
				for(auto &subMesh : newMesh->GetSubMeshes())
				{
					auto newSubMesh = subMesh->Copy(true);
					oldSubMeshToNewSubMesh[subMesh.get()] = newSubMesh.get();
					subMesh = newSubMesh;
				}
				mesh = newMesh;
			}
			meshGroup = newMeshGroup;
		}
	}
	if((copyFlags &CopyFlags::CopyAnimationsBit) != CopyFlags::None)
	{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
		for(auto &anim : mdl->m_animations)
			anim = Animation::Create(*anim,Animation::ShareMode::None);
#endif
	}
	if((copyFlags &CopyFlags::CopyVertexAnimationsBit) != CopyFlags::None)
	{
		for(auto &vertexAnim : mdl->m_vertexAnimations)
		{
			vertexAnim = VertexAnimation::Create(*vertexAnim);
			for(auto &meshAnim : vertexAnim->GetMeshAnimations())
			{
				auto *mesh = meshAnim->GetMesh();
				auto *subMesh = meshAnim->GetSubMesh();
				if(mesh == nullptr || subMesh == nullptr)
					continue;
				auto itMesh = oldMeshToNewMesh.find(mesh);
				auto itSubMesh = oldSubMeshToNewSubMesh.find(subMesh);
				if(itMesh == oldMeshToNewMesh.end() || itSubMesh == oldSubMeshToNewSubMesh.end())
					continue;
				meshAnim->SetMesh(*itMesh->second,*itSubMesh->second);
			}
		}
	}
	if((copyFlags &CopyFlags::CopyCollisionMeshesBit) != CopyFlags::None)
	{
		for(auto &colMesh : mdl->m_collisionMeshes)
		{
			colMesh = CollisionMesh::Create(*colMesh);
			// TODO: Update shape?
		}
	}
	if((copyFlags &CopyFlags::CopyFlexAnimationsBit) != CopyFlags::None)
	{
		for(auto &flexAnim : mdl->m_flexAnimations)
			flexAnim = std::make_shared<FlexAnimation>(*flexAnim);
	}
	// TODO: Copy collision mesh soft body sub mesh reference
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	static_assert(sizeof(Model) == 1'000,"Update this function when making changes to this class!");
#endif
	return mdl;
}

bool Model::FindSubMeshIndex(const ModelMeshGroup *optMeshGroup,const ModelMesh *optMesh,const ModelSubMesh *optSubMesh,uint32_t &outGroupIdx,uint32_t &outMeshIdx,uint32_t &outSubMeshIdx) const
{
	auto &meshGroups = GetMeshGroups();
	uint32_t mgStart = 0;
	uint32_t mgEnd = meshGroups.size();
	if(optMeshGroup)
	{
		auto it = std::find_if(meshGroups.begin(),meshGroups.end(),[optMeshGroup](const std::shared_ptr<ModelMeshGroup> &mmg) {return mmg.get() == optMeshGroup;});
		if(it == meshGroups.end())
			return false;
		mgStart = (it -meshGroups.begin());
		mgEnd = mgStart +1;
		if(!optMesh && !optSubMesh)
		{
			outGroupIdx = mgStart;
			return true;
		}
	}

	for(auto i=mgStart;i<mgEnd;++i)
	{
		auto &mg = meshGroups[i];
		auto &meshes = mg->GetMeshes();
		uint32_t meshStart = 0;
		uint32_t meshEnd = meshes.size();
		if(optMesh)
		{
			auto it = std::find_if(meshes.begin(),meshes.end(),[optMesh](const std::shared_ptr<ModelMesh> &mesh) {return mesh.get() == optMesh;});
			if(it == meshes.end())
				continue;
			meshStart = (it -meshes.begin());
			meshEnd = meshStart +1;
			if(!optSubMesh)
			{
				outGroupIdx = i;
				outMeshIdx = meshStart;
				return true;
			}
		}
		if(optSubMesh == nullptr)
			continue;
		for(auto j=meshStart;j<meshEnd;++j)
		{
			auto &mesh = meshes[j];
			auto &subMeshes = mesh->GetSubMeshes();
			auto it = std::find_if(subMeshes.begin(),subMeshes.end(),[optSubMesh](const std::shared_ptr<ModelSubMesh> &subMesh) {return subMesh.get() == optSubMesh;});
			if(it == subMeshes.end())
				continue;
			outGroupIdx = i;
			outMeshIdx = j;
			outSubMeshIdx = (it -subMeshes.begin());
			return true;
		}
	}
	return false;
}

bool Model::LoadFromAssetData(Game &game,const udm::AssetData &data,std::string &outErr)
{
	if(data.GetAssetType() != PMDL_IDENTIFIER)
	{
		outErr = "Incorrect format!";
		return false;
	}

	const auto udm = *data;
	auto version = data.GetAssetVersion();
	if(version < 1)
	{
		outErr = "Invalid version!";
		return false;
	}
	// if(version > PMDL_VERSION)
	// 	return false;
	auto activity = udm["activity"];

	udm["materialPaths"].GetBlobData(GetTexturePaths());
	udm["materials"].GetBlobData(GetTextures());
	m_materials.resize(m_metaInfo.textures.size());
	udm["includeModels"].GetBlobData(GetMetaInfo().includes);
	udm["eyeOffset"](m_eyeOffset);
	udm["maxEyeDeflection"](m_maxEyeDeflection);
	udm["mass"](m_mass);
	
	udm["render"]["bounds"]["min"](m_renderMin);
	udm["render"]["bounds"]["max"](m_renderMax);

	auto readFlag = [this](auto udm,auto flag,const std::string &name,auto &outFlags) {
		auto udmFlags = udm["flags"];
		if(!udmFlags)
			return;
		umath::set_flag(outFlags,flag,udmFlags[name](false));
	};
	auto &flags = GetMetaInfo().flags;
	readFlag(udm,Model::Flags::Static,"static",flags);
	readFlag(udm,Model::Flags::Inanimate,"inanimate",flags);
	readFlag(udm,Model::Flags::DontPrecacheTextureGroups,"dontPrecacheSkins",flags);
	static_assert(umath::to_integral(Model::Flags::Count) == 8,"Update this list when new flags have been added!");

	auto isStatic = umath::is_flag_set(flags,Model::Flags::Static);
	if(!isStatic)
	{
		auto &ref = GetReference();
		auto udmSkeleton = udm["skeleton"];
#if ENABLE_LEGACY_ANIMATION_SYSTEM
		m_skeleton = Skeleton::Load(ref,udm::AssetData{udmSkeleton},outErr);
#endif
		if(m_skeleton == nullptr)
			return false;

		auto &attachments = GetAttachments();
		auto udmAttachments = udm["attachments"];
		auto numAttachments = udmAttachments.GetSize();
		attachments.resize(numAttachments);
		for(auto i=decltype(numAttachments){0u};i<numAttachments;++i)
		{
			auto &att = attachments[i];
			umath::Transform pose {};
			auto udmAttachment = udmAttachments[i];
			udmAttachment["name"](att.name);
			udmAttachment["bone"](att.bone);
			udmAttachment["pose"](pose);
			att.offset = pose.GetOrigin();
			att.angles = pose.GetRotation();
		}

		auto &objAttachments = GetObjectAttachments();
		auto udmObjAttachments = udm["objectAttachments"];
		auto numObjAttachments = udmObjAttachments.GetSize();
		objAttachments.resize(numObjAttachments);
		for(auto i=decltype(numObjAttachments){0u};i<numObjAttachments;++i)
		{
			auto &objAtt = objAttachments[i];
			auto udmObjAttachment = udmObjAttachments[i];
			udmObjAttachment["name"](objAtt.name);

			int32_t attId = -1;
			udmObjAttachment["attachment"](attId);
			if(attId >= 0 && attId < attachments.size())
				objAtt.attachment = attachments[attId].name;
			udm::to_enum_value<ObjectAttachment::Type>(udmObjAttachment["type"],objAtt.type);

			udmObjAttachment["keyValues"](objAtt.keyValues);
		}

		auto &hitboxes = GetHitboxes();
		auto udmHitboxes = udm["hitboxes"];
		auto numHitboxes = udmHitboxes.GetSize();
		hitboxes.reserve(numHitboxes);
		for(auto i=decltype(numHitboxes){0u};i<numHitboxes;++i)
		{
			auto udmHb = udmHitboxes[i];
			Hitbox hb {};

			udm::to_enum_value<HitGroup>(udmHb["hitGroup"],hb.group);
			udmHb["bounds"]["min"](hb.min);
			udmHb["bounds"]["max"](hb.max);

			auto boneId = std::numeric_limits<uint32_t>::max();
			udmHb["bone"](boneId);
			if(boneId != std::numeric_limits<uint32_t>::max())
				hitboxes[boneId] = hb;
		}
	}

	// Bodygroups
	auto &bodyGroups = GetBodyGroups();
	auto udmBodyGroups = udm["bodyGroups"];
	auto numBodyGroups = udmBodyGroups.GetChildCount();
	bodyGroups.resize(numBodyGroups);
	uint32_t idx = 0;
	for(auto udmBodyGroup : udmBodyGroups.ElIt())
	{
		auto &bg = bodyGroups[idx++];
		bg.name = udmBodyGroup.key;
		udmBodyGroup.property["meshGroups"](bg.meshGroups);
	}

	udm["baseMeshGroups"].GetBlobData(m_baseMeshes);

	// Material groups
	auto &texGroups = GetTextureGroups();
	auto udmTexGroups = udm["skins"];
	auto numTexGroups = udmTexGroups.GetSize();
	texGroups.resize(numTexGroups);
	for(auto i=decltype(texGroups.size()){0u};i<texGroups.size();++i)
	{
		auto &texGroup = texGroups[i];
		auto udmMaterials = udmTexGroups[i]["materials"];
		udmMaterials.GetBlobData(texGroup.textures);
	}

	// Collision meshes
	auto &colMeshes = GetCollisionMeshes();
	auto udmColMeshes = udm["collisionMeshes"];
	auto numColMeshes = udmColMeshes.GetSize();
	colMeshes.resize(numColMeshes);
	for(auto i=decltype(numColMeshes){0u};i<numColMeshes;++i)
	{
		auto &colMesh = colMeshes[i];
		colMesh = CollisionMesh::Load(game,*this,udm::AssetData{udmColMeshes[i]},outErr);
		if(colMesh == nullptr)
			return false;
	}

	// Joints
	auto &joints = GetJoints();
	auto udmJoints = udm["joints"];
	auto numJoints = udmJoints.GetSize();
	joints.resize(numJoints);
	for(auto i=decltype(numJoints){0u};i<numJoints;++i)
	{
		auto &joint = joints[i];
		auto udmJoint = udmJoints[i];

		udm::to_enum_value<JointType>(udmJoint["type"],joint.type);
		udmJoint["parentBone"](joint.parent);
		udmJoint["childBone"](joint.child);
		udmJoint["enableCollisions"](joint.collide);
		udmJoint["args"](joint.args);
	}

	auto &meshGroups = GetMeshGroups();
	auto udmMeshGroups = udm["meshGroups"];
	meshGroups.resize(udmMeshGroups.GetChildCount());
	for(auto udmMeshGroup : udmMeshGroups.ElIt())
	{
		auto meshGroup = ModelMeshGroup::Create(std::string{udmMeshGroup.key});
		uint32_t groupIdx = 0;
		udmMeshGroup.property["index"](groupIdx);
		meshGroups[groupIdx] = meshGroup;
		auto udmMeshes = udmMeshGroup.property["meshes"];
		auto numMeshes = udmMeshes.GetSize();
		auto &meshes = meshGroup->GetMeshes();
		meshes.resize(numMeshes);
		for(auto meshIdx=decltype(numMeshes){0u};meshIdx<numMeshes;++meshIdx)
		{
			auto &mesh = meshes[meshIdx];
			auto udmMesh = udmMeshes[meshIdx];
			mesh = std::make_shared<ModelMesh>();
			auto referenceId = std::numeric_limits<uint32_t>::max();
			udmMesh["referenceId"](referenceId);
			mesh->SetReferenceId(referenceId);
			auto udmSubMeshes = udmMesh["subMeshes"];
			auto numSubMeshes = udmSubMeshes.GetSize();
			auto &subMeshes = mesh->GetSubMeshes();
			subMeshes.resize(numSubMeshes);
			for(auto subMeshIdx=decltype(numSubMeshes){0u};subMeshIdx<numSubMeshes;++subMeshIdx)
			{
				auto &subMesh = subMeshes[subMeshIdx];
				auto udmSubMesh = udmSubMeshes[subMeshIdx];
				subMesh = CreateSubMesh();
				subMesh->LoadFromAssetData(udm::AssetData{udmSubMesh},outErr);
				if(subMesh == nullptr)
					return false;
				subMesh->Update(ModelUpdateFlags::UpdateBuffers);
			}
		}
	}
	
	if(!isStatic)
	{
		auto &skeleton = GetSkeleton();
		auto &reference = GetReference();
		auto &animations = GetAnimations();
		auto udmAnimations = udm["animations"];
		animations.resize(udmAnimations.GetChildCount());
		for(auto udmAnimation : udmAnimations.ElIt())
		{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
			auto anim = Animation::Load(udm::AssetData{udmAnimation.property},outErr,&skeleton,&reference);
			if(anim == nullptr)
				return false;
			uint32_t index = 0;
			udmAnimation.property["index"](index);
			m_animationIDs[std::string{udmAnimation.key}] = index;
			animations[index] = anim;
#endif
		}

		auto &blendControllers = GetBlendControllers();
		auto udmBlendControllers = udm["blendControllers"];
		auto numBlendControllers = udmBlendControllers.GetSize();
		blendControllers.resize(numBlendControllers);
		for(auto i=decltype(numBlendControllers){0u};i<numBlendControllers;++i)
		{
			auto &bc = blendControllers[i];
			auto udmBc = udmBlendControllers[i];
			udmBc["name"](bc.name);
			udmBc["min"](bc.min);
			udmBc["max"](bc.max);
			udmBc["loop"](bc.loop);
		}

		auto &ikControllers = GetIKControllers();
		auto udmIkControllers = udm["ikControllers"];
		auto numIkControllers = udmIkControllers.GetSize();
		ikControllers.resize(numIkControllers);
		for(auto i=decltype(numIkControllers){0u};i<numIkControllers;++i)
		{
			auto &ikc = ikControllers[i];
			auto udmIkController = udmIkControllers[i];

			std::string effectorName;
			std::string type;
			udmIkController["effectorName"](effectorName);
			udmIkController["type"](type);

			ikc->SetEffectorName(effectorName);
			ikc->SetType(type);
			uint32_t chainLength = 0;
			udmIkController["chainLength"](chainLength);
			ikc->SetChainLength(chainLength);
			auto method = ikc->GetMethod();
			udm::to_enum_value<util::ik::Method>(udmIkController["method"],method);
			ikc->SetMethod(method);

			udmIkController["keyValues"](ikc->GetKeyValues());
		}

		auto &morphAnims = GetVertexAnimations();
		auto udmMorphTargetAnims = udm["morphTargetAnimations"];
		auto numMorphTargetAnims = udmMorphTargetAnims.GetChildCount();
		morphAnims.resize(numMorphTargetAnims);
		for(auto udmMorphTargetAnim : udmMorphTargetAnims.ElIt())
		{
			udmMorphTargetAnim.property["index"](idx);
			morphAnims[idx] = VertexAnimation::Load(*this,udm::AssetData{udmMorphTargetAnim.property},outErr);
			if(morphAnims[idx] == nullptr)
				return false;
			++idx;
		}

		auto &flexes = GetFlexes();
		auto udmFlexes = udm["flexes"];
		flexes.resize(udmFlexes.GetChildCount());
		for(auto udmFlex : udmFlexes.ElIt())
		{
			udmFlex.property["index"](idx);
			auto &flex = flexes[idx];
			flex.GetName() = udmFlex.key;

			auto morphTargetAnimation = std::numeric_limits<uint32_t>::max();
			udmFlex.property["morphTargetAnimation"](morphTargetAnimation);
			if(morphTargetAnimation < morphAnims.size())
			{
				uint32_t frameIndex = 0;
				flex.SetVertexAnimation(*morphAnims[morphTargetAnimation],frameIndex);
			}
				
			auto &ops = flex.GetOperations();
			auto udmOps = udmFlex.property["operations"];
			auto numOps = udmOps.GetSize();
			ops.resize(numOps);
			for(auto i=decltype(numOps){0u};i<numOps;++i)
			{
				auto &op = ops[i];
				auto udmOp = udmOps[i];
				udm::to_enum_value<Flex::Operation::Type>(udmOp["type"],op.type);
				if(udmOp["value"])
					udmOp["value"](op.d.value);
				else if(udmOp["index"])
					udmOp["index"](op.d.index);
			}
		}

		auto &flexControllers = GetFlexControllers();
		auto udmFlexControllers = udm["flexControllers"];
		auto numFlexControllers = udmFlexControllers.GetChildCount();
		flexControllers.resize(numFlexControllers);
		for(auto udmFlexController : udmFlexControllers.ElIt())
		{
			udmFlexController.property["index"](idx);
			auto &flexC = flexControllers[idx];
			flexC.name = udmFlexController.key;
			udmFlexController.property["min"](flexC.min);
			udmFlexController.property["max"](flexC.max);
		}

		auto &eyeballs = GetEyeballs();
		auto udmEyeballs = udm["eyeballs"];
		auto numEyeballs = udmEyeballs.GetChildCount();
		eyeballs.resize(numEyeballs);
		for(auto udmEyeball : udmEyeballs.ElIt())
		{
			udmEyeball.property["index"](idx);
			auto &eyeball = eyeballs[idx];
			eyeball.name = udmEyeball.key;
			udmEyeball.property["bone"](eyeball.boneIndex);
			udmEyeball.property["origin"](eyeball.origin);
			udmEyeball.property["zOffset"](eyeball.zOffset);
			udmEyeball.property["radius"](eyeball.radius);
			udmEyeball.property["up"](eyeball.up);
			udmEyeball.property["forward"](eyeball.forward);
			udmEyeball.property["maxDilationFactor"](eyeball.maxDilationFactor);

			udmEyeball.property["iris"]["material"](eyeball.irisMaterialIndex);
			udmEyeball.property["iris"]["uvRadius"](eyeball.irisUvRadius);
			udmEyeball.property["iris"]["scale"](eyeball.irisScale);

			auto writeLid = [](udm::LinkedPropertyWrapper &prop,const Eyeball::LidFlexDesc &lid) {
				prop["raiser"]["lidFlexIndex"] = lid.lidFlexIndex;

				prop["raiser"]["raiserFlexIndex"] = lid.raiserFlexIndex;
				prop["raiser"]["targetAngle"] = umath::rad_to_deg(lid.raiserValue);

				prop["neutral"]["neutralFlexIndex"] = lid.neutralFlexIndex;
				prop["neutral"]["targetAngle"] = umath::rad_to_deg(lid.neutralValue);

				prop["lowerer"]["lowererFlexIndex"] = lid.lowererFlexIndex;
				prop["lowerer"]["targetAngle"] = umath::rad_to_deg(lid.lowererValue);
			};
			writeLid(udmEyeball.property["eyelids"]["upperLid"],eyeball.upperLid);
			writeLid(udmEyeball.property["eyelids"]["lowerLid"],eyeball.lowerLid);
		}
			
		auto &phonemeMap = GetPhonemeMap();
		auto udmPhonemes = udm["phonemes"];
		phonemeMap.phonemes.reserve(udmPhonemes.GetChildCount());
		for(auto udmPhoneme : udmPhonemes.ElIt())
		{
			auto &phonemeInfo = phonemeMap.phonemes[std::string{udmPhoneme.key}] = {};
			udmPhoneme.property(phonemeInfo.flexControllers);
		}
			
		auto &flexAnims = GetFlexAnimations();
		auto &flexAnimNames = GetFlexAnimationNames();
		auto udmFlexAnims = udm["flexAnimations"];
		auto numFlexAnims = udmFlexAnims.GetChildCount();
		flexAnims.resize(numFlexAnims);
		flexAnimNames.resize(numFlexAnims);
		for(auto udmFlexAnim : udmFlexAnims.ElIt())
		{
			udmFlexAnim.property["index"](idx);
			flexAnimNames[idx] = udmFlexAnim.key;
			flexAnims[idx] = FlexAnimation::Load(udm::AssetData{udmFlexAnim.property},outErr);
			if(flexAnims[idx] == nullptr)
				return false;
		}
	}
	return true;
}

bool Model::Save(Game &game,const std::string &fileName,std::string &outErr)
{
	auto udmData = udm::Data::Create();
	std::string err;
	auto result = Save(game,udmData->GetAssetData(),err);
	if(result == false)
		return false;
	FileManager::CreatePath(ufile::get_path_from_filename(fileName).c_str());
	auto writeFileName = fileName;
	ufile::remove_extension_from_filename(writeFileName,pragma::asset::get_supported_extensions(pragma::asset::Type::Model));
	writeFileName += '.' +std::string{pragma::asset::FORMAT_MODEL_BINARY};
	auto f = FileManager::OpenFile<VFilePtrReal>(writeFileName.c_str(),"wb");
	if(f == nullptr)
	{
		outErr = "Unable to open file '" +writeFileName +"'!";
		return false;
	}
	result = udmData->Save(f);
	if(result == false)
	{
		outErr = "Unable to save UDM data!";
		return false;
	}
	return true;
}
bool Model::Save(Game &game,std::string &outErr)
{
	auto mdlName = GetName();
	std::string absFileName;
	auto result = FileManager::FindAbsolutePath("models/" +mdlName,absFileName);
	if(result == false)
		absFileName = "models/" +mdlName;
	else
	{
		auto path = util::Path::CreateFile(absFileName);
		path.MakeRelative(util::get_program_path());
		absFileName = path.GetString();
	}
	return Save(game,absFileName,outErr);
}

bool Model::Save(Game &game,udm::AssetData &outData,std::string &outErr)
{
	outData.SetAssetType(PMDL_IDENTIFIER);
	outData.SetAssetVersion(PMDL_VERSION);
	auto udm = *outData;

	udm["materialPaths"] = GetTexturePaths();
	udm["materials"] = m_metaInfo.textures;
	udm["includeModels"] = GetMetaInfo().includes;
	udm["eyeOffset"] = GetEyeOffset();
	udm["maxEyeDeflection"] = m_maxEyeDeflection;
	udm["mass"] = m_mass;

	Vector3 min,max;
	GetRenderBounds(min,max);
	udm["render"]["bounds"]["min"] = min;
	udm["render"]["bounds"]["max"] = max;

	auto flags = GetMetaInfo().flags;
	auto writeFlag = [](auto udm,auto flag,const std::string &name,auto flags) {
		if(umath::is_flag_set(flags,flag) == false)
			return;
		udm["flags"][name] = true;
	};
	auto writeModelFlag = [&udm,flags,&writeFlag](Model::Flags flag,const std::string &name) {writeFlag(udm,flag,name,flags);};
	writeModelFlag(Model::Flags::Static,"static");
	writeModelFlag(Model::Flags::Inanimate,"inanimate");
	writeModelFlag(Model::Flags::DontPrecacheTextureGroups,"dontPrecacheSkins");
	static_assert(umath::to_integral(Model::Flags::Count) == 8,"Update this list when new flags have been added!");

	auto isStatic = umath::is_flag_set(flags,Model::Flags::Static);
	if(!isStatic)
	{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
		auto udmSkeleton = udm["skeleton"];
		auto &skeleton = GetSkeleton();
		auto &reference = GetReference();
		if(skeleton.Save(reference,udm::AssetData{udmSkeleton},outErr) == false)
			return false;

		auto &attachments = GetAttachments();
		auto udmAttachments = udm.AddArray("attachments",attachments.size());
		for(auto i=decltype(attachments.size()){0u};i<attachments.size();++i)
		{
			auto &att = attachments[i];
			auto udmAtt = udmAttachments[i];
			umath::Transform transform {att.offset,uquat::create(att.angles)};
			udmAtt["name"] = att.name;
			udmAtt["bone"] = att.bone;
			udmAtt["pose"] = transform;
		}
		
		auto &objAttachments = GetObjectAttachments();
		auto udmObjAttachments = udm.AddArray("objectAttachments",objAttachments.size());
		for(auto i=decltype(objAttachments.size()){0u};i<objAttachments.size();++i)
		{
			auto &objAtt = objAttachments[i];
			auto udmObjAtt = udmObjAttachments[i];
			udmObjAtt["name"] = objAtt.name;
			udmObjAtt["attachment"] = LookupAttachment(objAtt.attachment);
			udmObjAtt["type"] = udm::enum_to_string(objAtt.type);
			udmObjAtt["keyValues"] = objAtt.keyValues;
		}

		auto &hitboxes = GetHitboxes();
		auto udmHitboxes = udm.AddArray("hitboxes",hitboxes.size());
		uint32_t hbIdx = 0;
		for(auto &pair : hitboxes)
		{
			auto &hb = pair.second;
			auto udmHb = udmHitboxes[hbIdx++];
			udmHb["hitGroup"] = udm::enum_to_string(hb.group);
			udmHb["bounds"]["min"] = hb.min;
			udmHb["bounds"]["max"] = hb.max;
			udmHb["bone"] = pair.first;
		}
#endif
	}

	auto &bodyGroups = GetBodyGroups();
	auto udmBodyGroups = udm["bodyGroups"];
	for(auto &bg : bodyGroups)
	{
		auto udmBodyGroup = udmBodyGroups[bg.name];
		udmBodyGroup["meshGroups"] = bg.meshGroups;
	}

	udm["baseMeshGroups"] = m_baseMeshes;
	
	auto &texGroups = GetTextureGroups();
	auto udmTexGroups = udm.AddArray("skins",texGroups.size());
	for(auto i=decltype(texGroups.size()){0u};i<texGroups.size();++i)
	{
		auto &texGroup = texGroups[i];
		udmTexGroups[i]["materials"] = texGroup.textures;
	}
	
	auto &colMeshes = GetCollisionMeshes();
	auto udmColMeshes = udm.AddArray("collisionMeshes",colMeshes.size());
	auto &surfaceMaterials = game.GetSurfaceMaterials();
	for(auto i=decltype(colMeshes.size()){0u};i<colMeshes.size();++i)
	{
		auto &colMesh = colMeshes[i];
		if(colMesh->Save(game,*this,udm::AssetData{udmColMeshes[i]},outErr) == false)
			return false;
	}

	// Joints
	auto &joints = GetJoints();
	if(!joints.empty())
	{
		auto udmJoints = udm.AddArray("joints",joints.size());
		uint32_t jointIdx = 0;
		for(auto &joint : joints)
		{
			auto udmJoint = udmJoints[jointIdx++];
			udmJoint["type"] = udm::enum_to_string(joint.type);
			udmJoint["parentBone"] = joint.parent;
			udmJoint["childBone"] = joint.child;
			udmJoint["enableCollisions"] = joint.collide;
			udmJoint["args"] = joint.args;
		}
	}

	if(!isStatic)
	{
		auto &animations = GetAnimations();
		if(!animations.empty())
		{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
			auto &ref = GetReference();
			auto udmAnimations = udm["animations"];
			for(auto i=decltype(animations.size()){0u};i<animations.size();++i)
			{
				auto &anim = animations[i];
				auto animName = GetAnimationName(i);
				auto udmAnim = udmAnimations[animName];
				udmAnim["index"] = static_cast<uint32_t>(i);
				if(anim->Save(udm::AssetData{udmAnim},outErr,&ref) == false)
					return false;
			}
#endif
		}

		auto &blendControllers = GetBlendControllers();
		if(!blendControllers.empty())
		{
			auto udmBlendControllers = udm.AddArray("blendControllers",blendControllers.size());
			uint32_t bcIdx = 0;
			for(auto &bc : blendControllers)
			{
				auto udmBc = udmBlendControllers[bcIdx++];
				udmBc["name"] = bc.name;
				udmBc["min"] = bc.min;
				udmBc["max"] = bc.max;
				udmBc["loop"] = bc.loop;
			}
		}

		auto &ikControllers = GetIKControllers();
		if(!ikControllers.empty())
		{
			auto udmIkControllers = udm.AddArray("ikControllers",ikControllers.size());
			uint32_t ikControllerIdx = 0;
			for(auto &ikc : ikControllers)
			{
				auto udmIkController = udmIkControllers[ikControllerIdx++];
				udmIkController["effectorName"] = ikc->GetEffectorName();
				udmIkController["type"] = ikc->GetType();
				udmIkController["chainLength"] = ikc->GetChainLength();
				udmIkController["method"] = udm::enum_to_string(ikc->GetMethod());
				udmIkController["keyValues"] = ikc->GetKeyValues();
			}
		}

		auto &morphAnims = GetVertexAnimations();
		if(!morphAnims.empty())
		{
			auto udmMorphAnims = udm["morphTargetAnimations"];
			for(auto i=decltype(morphAnims.size()){0u};i<morphAnims.size();++i)
			{
				auto &va = morphAnims[i];
				auto udmMa = udmMorphAnims[va->GetName()];
				udmMa["index"] = static_cast<uint32_t>(i);
				if(va->Save(*this,udm::AssetData{udmMa},outErr) == false)
					return false;
			}

			auto &flexes = GetFlexes();
			auto udmFlexes = udm["flexes"];
			for(uint32_t flexIdx=0u;flexIdx<flexes.size();++flexIdx)
			{
				auto &flex = flexes[flexIdx];
				auto udmFlex = udmFlexes[flex.GetName()];
				udmFlex["index"] = flexIdx;

				auto *va = flex.GetVertexAnimation();
				if(va != nullptr)
				{
					auto &vertAnims = GetVertexAnimations();
					auto itVa = std::find(vertAnims.begin(),vertAnims.end(),va->shared_from_this());
					if(itVa != vertAnims.end())
						udmFlex["morphTargetAnimation"] = static_cast<uint32_t>(itVa -vertAnims.begin());
				}
				if(!udmFlex["morphTargetAnimation"])
					udmFlex.Add("morphTargetAnimation",udm::Type::Nil);
				udmFlex["frame"] = flex.GetFrameIndex();

				auto &ops = flex.GetOperations();
				auto udmOps = udmFlex.AddArray("operations",ops.size());
				uint32_t opIdx = 0u;
				for(auto &op : ops)
				{
					auto udmOp = udmOps[opIdx++];
					udmOp["type"] = udm::enum_to_string(op.type);
					auto valueType = Flex::Operation::GetOperationValueType(op.type);
					switch(valueType)
					{
					case Flex::Operation::ValueType::Index:
						udmOp["index"] = op.d.index;
						break;
					case Flex::Operation::ValueType::Value:
						udmOp["value"] = op.d.value;
						break;
					case Flex::Operation::ValueType::None:
						break;
					}
				}
			}

			auto &flexControllers = GetFlexControllers();
			auto udmFlexControllers = udm["flexControllers"];
			uint32_t flexCIdx = 0;
			for(auto &flexC : flexControllers)
			{
				auto udmFlexC = udmFlexControllers[flexC.name];
				udmFlexC["index"] = flexCIdx++;
				udmFlexC["min"] = flexC.min;
				udmFlexC["max"] = flexC.max;
			}

			auto &eyeballs = GetEyeballs();
			auto udmEyeballs = udm["eyeballs"];
			uint32_t eyeballIdx = 0;
			for(auto &eyeball : eyeballs)
			{
				std::string name = eyeball.name;
				if(name.empty())
				{
					name = "eyeball" +std::to_string(eyeballIdx);
					Con::cwar<<"WARNING: Eyeball with no name found, assigning name '"<<name<<"'"<<Con::endl;
				}
				auto udmEyeball = udmEyeballs[name];
				udmEyeball["index"] = eyeballIdx++;
				udmEyeball["bone"] = eyeball.boneIndex;
				udmEyeball["origin"] = eyeball.origin;
				udmEyeball["zOffset"] = eyeball.zOffset;
				udmEyeball["radius"] = eyeball.radius;
				udmEyeball["up"] = eyeball.up;
				udmEyeball["forward"] = eyeball.forward;
				udmEyeball["maxDilationFactor"] = eyeball.maxDilationFactor;

				udmEyeball["iris"]["material"] = eyeball.irisMaterialIndex;
				udmEyeball["iris"]["uvRadius"] = eyeball.irisUvRadius;
				udmEyeball["iris"]["scale"] = eyeball.irisScale;

				auto readLid = [](udm::LinkedPropertyWrapper &prop,Eyeball::LidFlexDesc &lid) {
					prop["raiser"]["lidFlexIndex"](lid.lidFlexIndex);

					prop["raiser"]["raiserFlexIndex"](lid.raiserFlexIndex);
					prop["raiser"]["targetAngle"](lid.raiserValue);
					lid.raiserValue = umath::deg_to_rad(lid.raiserValue);

					prop["neutral"]["neutralFlexIndex"](lid.neutralFlexIndex);
					prop["neutral"]["targetAngle"](lid.neutralValue);
					lid.neutralValue = umath::deg_to_rad(lid.neutralValue);

					prop["lowerer"]["lowererFlexIndex"](lid.lowererFlexIndex);
					prop["lowerer"]["targetAngle"](lid.lowererValue);
					lid.lowererValue = umath::deg_to_rad(lid.lowererValue);
				};
				readLid(udmEyeball["eyelids"]["upperLid"],eyeball.upperLid);
				readLid(udmEyeball["eyelids"]["lowerLid"],eyeball.lowerLid);
			}
			
			auto &phonemeMap = GetPhonemeMap();
			auto udmPhonemes = udm["phonemes"];
			for(auto &pairPhoneme : phonemeMap.phonemes)
				udmPhonemes[pairPhoneme.first] = pairPhoneme.second.flexControllers;
			
			auto &flexAnims = GetFlexAnimations();
			auto &flexAnimNames = GetFlexAnimationNames();
			auto udmFlexAnims = udm["flexAnimations"];
			for(uint32_t i=decltype(flexAnims.size()){0u};i<flexAnims.size();++i)
			{
				auto &flexAnim = flexAnims[i];
				auto udmFlexAnim = udmFlexAnims[flexAnimNames[i]];
				udmFlexAnim["index"] = i;
				if(flexAnim->Save(udm::AssetData{udmFlexAnim},outErr) == false)
					return false;
			}
		}
	}


	auto &meshGroups = GetMeshGroups();
	auto udmMeshGroups = udm.Add("meshGroups");
	for(auto i=decltype(meshGroups.size()){0u};i<meshGroups.size();++i)
	{
		auto &meshGroup = meshGroups[i];
		auto udmMeshGroup = udmMeshGroups[meshGroup->GetName()];
		udmMeshGroup["index"] = static_cast<uint32_t>(i);
		auto &meshes = meshGroup->GetMeshes();
		auto udmMeshes = udmMeshGroup.AddArray("meshes",meshes.size());
		uint32_t meshIdx = 0;
		for(auto &mesh : meshes)
		{
			auto udmMesh = udmMeshes[meshIdx++];
			udmMesh["referenceId"] = mesh->GetReferenceId();
			auto &subMeshes = mesh->GetSubMeshes();
			auto udmSubMeshes = udmMesh.AddArray("subMeshes",subMeshes.size());
			uint32_t subMeshIdx = 0;
			for(auto &subMesh : subMeshes)
			{
				if(subMesh->Save(udm::AssetData{udmSubMeshes[subMeshIdx++]},outErr) == false)
					return false;
			}
		}
	}
	return true;
}
#pragma optimize("",on)
