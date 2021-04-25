/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan
 */

#include "stdafx_shared.h"
#include "pragma/model/modelmanager.h"
#include "pragma/model/animation/animation.hpp"
#include "pragma/asset/util_asset.hpp"
#include <sharedutils/util_path.hpp>

extern DLLNETWORK Engine *engine;

static const std::vector<std::string> &get_model_extensions()
{
	static std::vector<std::string> extensions {};
	if(extensions.empty())
	{
		extensions = pragma::asset::get_supported_extensions(pragma::asset::Type::Model);
		extensions.push_back("mdl");
		extensions.push_back("nif");
		auto &assetManager = engine->GetAssetManager();
		auto numImporters = assetManager.GetImporterCount(pragma::asset::Type::Model);
		for(auto i=decltype(numImporters){0u};i<numImporters;++i)
		{
			auto *importerInfo = assetManager.GetImporterInfo(pragma::asset::Type::Model,i);
			if(importerInfo == nullptr)
				continue;
			extensions.reserve(extensions.size() +importerInfo->fileExtensions.size());
			for(auto &ext : importerInfo->fileExtensions)
				extensions.push_back(ext);
		}
	}
	return extensions;
}
std::string pragma::asset::ModelManager::GetNormalizedModelName(const std::string &mdlName)
{
	util::Path path {mdlName};
	path.Canonicalize();
	path.RemoveFileExtension(get_model_extensions());
	// path += ".wmd"; // TODO: Remove this extension!
	return path.GetString();
}
std::string pragma::asset::ModelManager::GetCacheName(const std::string &mdlName)
{
	auto normalizedName = GetNormalizedModelName(mdlName);
	util::Path path {normalizedName};
	path.RemoveFileExtension(get_model_extensions());
	auto strPath = path.GetString();
	ustring::to_lower(strPath);
	return strPath;
}

pragma::asset::ModelManager::ModelManager(NetworkState &nw)
	: m_nw{nw}
{}

uint32_t pragma::asset::ModelManager::ClearUnused()
{
	uint32_t n = 0;
	for(auto it=m_cache.begin();it!=m_cache.end();)
	{
		auto &mdl = it->second;
		if(mdl.use_count() == 1)
		{
			it = m_cache.erase(it);
			++n;
			continue;
		}
		++it;
	}
	return n;
}
uint32_t pragma::asset::ModelManager::ClearFlagged()
{
	uint32_t n = 0;
	for(auto &name : m_flaggedForDeletion)
	{
		auto itCache = m_cache.find(name);
		if(itCache == m_cache.end())
			continue;
		m_cache.erase(itCache);
		++n;
	}
	m_flaggedForDeletion.clear();
	return n;
}
uint32_t pragma::asset::ModelManager::Clear()
{
	auto n = m_cache.size();
	m_cache.clear();
	m_flaggedForDeletion.clear();
	return n;
}
void pragma::asset::ModelManager::FlagForRemoval(Model &mdl)
{
	auto name = GetCacheName(mdl.GetName());
	auto it = m_cache.find(name);
	if(it == m_cache.end())
		return;
	m_flaggedForDeletion.insert(name);
}

void pragma::asset::ModelManager::FlagAllForRemoval()
{
	m_flaggedForDeletion.reserve(m_cache.size());
	for(auto &pair : m_cache)
		m_flaggedForDeletion.insert(pair.first);
}

const std::unordered_map<std::string,std::shared_ptr<Model>> &pragma::asset::ModelManager::GetCache() const {return m_cache;}

const Model *pragma::asset::ModelManager::FindCachedModel(const std::string &mdlName) const
{
	return const_cast<ModelManager*>(this)->FindCachedModel(mdlName);
}
Model *pragma::asset::ModelManager::FindCachedModel(const std::string &mdlName)
{
	auto normalizedName = GetCacheName(mdlName);
	auto it = m_cache.find(normalizedName);
	return (it != m_cache.end()) ? it->second.get() : nullptr;
}

std::shared_ptr<Model> pragma::asset::ModelManager::CreateModel(uint32_t numBones,const std::string &mdlName)
{
	return Model::Create<Model>(&m_nw,numBones,mdlName);
}
std::shared_ptr<Model> pragma::asset::ModelManager::CreateModel(const std::string &name,bool bAddReference,bool addToCache)
{
	uint32_t boneCount = (bAddReference == true) ? 1 : 0;
	auto mdl = CreateModel(boneCount,name);
	auto &skeleton = mdl->GetSkeleton();
	auto reference = pragma::animation::Animation::Create();

	if(bAddReference == true)
	{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
		auto frame = Frame::Create(1);
		auto *root = new Bone;
		root->name = "root";
		auto rootID = skeleton.AddBone(root);
		mdl->SetBindPoseBoneMatrix(0,glm::inverse(umat::identity()));
		auto &rootBones = skeleton.GetRootBones();
		rootBones[0] = skeleton.GetBone(rootID).lock();
		reference->AddBoneId(0);

		frame->SetBonePosition(0,Vector3(0.f,0.f,0.f));
		frame->SetBoneOrientation(0,uquat::identity());

		auto refFrame = Frame::Create(*frame);
		frame->Localize(*reference,skeleton);
		reference->AddFrame(frame);
		mdl->AddAnimation("reference",reference);
		mdl->SetReference(refFrame);

		auto &baseMeshes = mdl->GetBaseMeshes();
		baseMeshes.push_back(0);
		mdl->AddMeshGroup("reference");

		mdl->CreateTextureGroup();
#endif
	}

	if(addToCache)
	{
		auto cacheName = GetCacheName(name);
		m_cache[cacheName] = mdl;
	}
	return mdl;
}
std::shared_ptr<Model> pragma::asset::ModelManager::LoadModel(FWMD &wmd,const std::string &mdlName) const
{
	auto *game = m_nw.GetGameState();
	assert(game);
	return std::shared_ptr<Model>{wmd.Load<Model,ModelMesh,ModelSubMesh>(game,mdlName,[this](const std::string &mat,bool reload) -> Material* {
		return m_nw.LoadMaterial(mat,reload);
	},[this](const std::string &mdlName) -> std::shared_ptr<Model> {
		return m_nw.GetGameState()->LoadModel(mdlName);
	})};
}
std::shared_ptr<Model> pragma::asset::ModelManager::LoadModel(const std::string &mdlName,bool bReload,bool *outIsNewModel)
{
	if(outIsNewModel)
		*outIsNewModel = false;
	auto normMdlName = GetNormalizedModelName(mdlName);
	auto cacheName = GetCacheName(normMdlName);
	if(bReload == false)
	{
		auto itFlagged = m_flaggedForDeletion.find(cacheName);
		if(itFlagged!= m_flaggedForDeletion.end())
			m_flaggedForDeletion.erase(itFlagged);

		auto it = m_cache.find(cacheName);
		if(it != m_cache.end())
		{
			auto &mdl = it->second;
			// mdl->PrecacheTextureGroup(0);
			return mdl;
		}
	}

	assert(m_nw.GetGameState());
	FWMD wmdLoader {m_nw.GetGameState()};
	auto mdl = LoadModel(wmdLoader,normMdlName);
	if(mdl == nullptr)
		return nullptr;
	mdl->Update();
	m_cache[cacheName] = mdl;
	if(outIsNewModel != nullptr)
		*outIsNewModel = true;

	return mdl;
}
std::shared_ptr<Model> pragma::asset::ModelManager::FindModel(const std::string &mdlName)
{
	auto it = m_cache.find(GetCacheName(mdlName));
	return (it != m_cache.end()) ? it->second : nullptr;
}


