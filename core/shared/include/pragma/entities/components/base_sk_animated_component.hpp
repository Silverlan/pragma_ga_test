/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan */

#ifndef __BASE_ANIMATED_COMPONENT_HPP__
#define __BASE_ANIMATED_COMPONENT_HPP__

#include "pragma/entities/components/base_entity_component.hpp"
#include "pragma/model/animation/play_animation_flags.hpp"
#include "pragma/model/animation/activities.h"
#include "pragma/model/animation/animation_event.h"
#include "pragma/model/animation/animated_pose.hpp"
#include "pragma/model/animation/anim_channel_desc.hpp"
#include "pragma/types.hpp"
#include <sharedutils/property/util_property.hpp>
#include <pragma/math/orientation.h>
#include <mathutil/transform.hpp>
#include <mathutil/uvec.h>

class Frame;
class ModelSubMesh;
struct AnimationEvent;
using BoneId = uint16_t;
enum class ALSoundType : int32_t;
namespace pragma
{
	namespace animation {class Animation; class AnimatedPose;};
	class DLLNETWORK BaseSkAnimatedComponent
		: public BaseEntityComponent
	{
	public:
		static ComponentEventId EVENT_ON_PLAY_LAYERED_ANIMATION;
		static ComponentEventId EVENT_ON_PLAY_LAYERED_ACTIVITY;
		static ComponentEventId EVENT_ON_LAYERED_ANIMATION_START;
		static ComponentEventId EVENT_ON_LAYERED_ANIMATION_COMPLETE;
		static ComponentEventId EVENT_TRANSLATE_LAYERED_ANIMATION;
		static ComponentEventId EVENT_TRANSLATE_ACTIVITY;
		static ComponentEventId EVENT_MAINTAIN_ANIMATION_MOVEMENT;
		static ComponentEventId EVENT_SHOULD_UPDATE_BONES;
		static ComponentEventId EVENT_ON_ANIMATION_RESET;

		static ComponentEventId EVENT_ON_PLAY_ACTIVITY;
		static ComponentEventId EVENT_ON_STOP_LAYERED_ANIMATION;
		static ComponentEventId EVENT_ON_BONE_TRANSFORM_CHANGED;
		static ComponentEventId EVENT_ON_BLEND_ANIMATION;

		static ComponentEventId EVENT_ON_PLAY_ANIMATION;
		static ComponentEventId EVENT_ON_ANIMATION_COMPLETE;
		static ComponentEventId EVENT_ON_ANIMATION_START;
		static ComponentEventId EVENT_MAINTAIN_ANIMATIONS;
		static ComponentEventId EVENT_ON_ANIMATIONS_UPDATED;
		static ComponentEventId EVENT_PLAY_ANIMATION;
		static ComponentEventId EVENT_TRANSLATE_ANIMATION;
		static constexpr auto *ROOT_POSE_BONE_NAME = "%rootPose%";
		static void RegisterEvents(pragma::EntityComponentManager &componentManager);

		enum class StateFlags : uint8_t
		{
			None = 0u,
			AbsolutePosesDirty = 1u,
			BaseAnimationDirty = AbsolutePosesDirty<<1u,
			RootPoseTransformEnabled = BaseAnimationDirty<<1u
		};

		virtual void Initialize() override;
		virtual void MaintainAnimationMovement(const Vector3 &disp);

		virtual void OnEntityComponentAdded(BaseEntityComponent &component) override;
		virtual void OnEntityComponentRemoved(BaseEntityComponent &component) override;

		void SetGlobalBonePosition(UInt32 boneId,const Vector3 &pos,const Quat &rot,const Vector3 &scale);
		void SetGlobalBonePosition(UInt32 boneId,const Vector3 &pos,const Quat &rot);
		void SetGlobalBonePosition(UInt32 boneId,const Vector3 &pos);
		void SetGlobalBoneRotation(UInt32 boneId,const Quat &rot);

		bool IsPlayingAnimation() const;
		bool CalcAnimationMovementSpeed(float *x,float *z,int32_t frameOffset=0) const;

		void SetBoneScale(uint32_t boneId,const Vector3 &scale);
		const Vector3 *GetBoneScale(uint32_t boneId) const;
		std::optional<Mat4> GetBoneMatrix(unsigned int boneID) const;

		FPlayAnim GetBaseAnimationFlags() const;
		void SetBaseAnimationFlags(FPlayAnim flags);

		std::optional<FPlayAnim> GetLayeredAnimationFlags(uint32_t layerIdx) const;
		void SetLayeredAnimationFlags(uint32_t layerIdx,FPlayAnim flags);

		// Returns the bone position / rotation in world space. Very expensive.
		Bool GetGlobalBonePosition(UInt32 boneId,Vector3 &pos,Quat &rot,Vector3 *scale=nullptr) const;
		Bool GetGlobalBonePosition(UInt32 boneId,Vector3 &pos) const;
		Bool GetGlobalBoneRotation(UInt32 boneId,Quat &rot) const;
		// Returns the bone position / rotation in entity space. Very expensive.
		Bool GetLocalBonePosition(UInt32 boneId,Vector3 &pos,Quat &rot,Vector3 *scale=nullptr) const;
		Bool GetLocalBonePosition(UInt32 boneId,Vector3 &pos) const;
		Bool GetLocalBoneRotation(UInt32 boneId,Quat &rot) const;

		// Returns the bone position / rotation in bone space.
		Bool GetBonePosition(UInt32 boneId,Vector3 &pos,Quat &rot,Vector3 &scale) const;
		Bool GetBonePosition(UInt32 boneId,Vector3 &pos,Quat &rot) const;
		Bool GetBonePosition(UInt32 boneId,Vector3 &pos) const;
		Bool GetBoneRotation(UInt32 boneId,Quat &rot) const;
		// Returns the bone position / angles in bone space.
		Bool GetBonePosition(UInt32 boneId,Vector3 &pos,EulerAngles &ang) const;
		Bool GetBoneAngles(UInt32 boneId,EulerAngles &ang) const;
		// Returns the bone position in bone space, or null if the bone doesn't exist.
		const Vector3 *GetBonePosition(UInt32 boneId) const;
		// Returns the bone rotation in bone space, or null if the bone doesn't exist.
		const Quat *GetBoneRotation(UInt32 boneId) const;
		void SetBonePosition(UInt32 boneId,const Vector3 &pos,const Quat &rot,const Vector3 &scale);
		void SetBonePosition(UInt32 boneId,const Vector3 &pos,const Quat &rot);
		void SetBonePosition(UInt32 boneId,const Vector3 &pos,const EulerAngles &ang);
		void SetBonePosition(UInt32 boneId,const Vector3 &pos);
		void SetBoneRotation(UInt32 boneId,const Quat &rot);
		void SetLocalBonePosition(UInt32 boneId,const Vector3 &pos,const Quat &rot,const Vector3 &scale);
		void SetLocalBonePosition(UInt32 boneId,const Vector3 &pos,const Quat &rot);
		void SetLocalBonePosition(UInt32 boneId,const Vector3 &pos);
		void SetLocalBoneRotation(UInt32 boneId,const Quat &rot);

		float GetCycle() const;
		void SetCycle(float cycle);
		animation::Animation *GetAnimationObject() const;
		animation::AnimationId GetAnimation() const;

		Activity GetActivity() const;
		animation::AnimationId GetLayeredAnimation(uint32_t slot) const;
		Activity GetLayeredActivity(uint32_t slot) const;

		virtual void PlayAnimation(animation::AnimationId animation,FPlayAnim flags=FPlayAnim::Default);
		virtual void PlayLayeredAnimation(animation::LayeredAnimationSlot slot,animation::AnimationId animation,FPlayAnim flags=FPlayAnim::Default);
		bool PlayActivity(Activity activity,FPlayAnim flags=FPlayAnim::Default);
		bool PlayLayeredActivity(animation::LayeredAnimationSlot slot,Activity activity,FPlayAnim flags=FPlayAnim::Default);
		bool PlayLayeredAnimation(animation::LayeredAnimationSlot slot,std::string animation,FPlayAnim flags=FPlayAnim::Default);
		virtual void StopLayeredAnimation(animation::LayeredAnimationSlot slot);
		bool PlayAnimation(const std::string &animation,FPlayAnim flags=FPlayAnim::Default);
		animation::AnimationId SelectTranslatedAnimation(Activity &inOutActivity) const;
		int SelectWeightedAnimation(Activity activity,animation::AnimationId animAvoid=-1) const;
		// Returns the time left until the current animation has finished playing
		float GetRemainingAnimationDuration() const;

		BoneId AddRootPoseBone();
		void SetRootPoseBoneId(BoneId boneId);
		BoneId GetRootPoseBoneId() const {return m_rootPoseBoneId;}

		void SetAnimatedRootPoseTransformEnabled(bool enabled);
		bool IsAnimatedRootPoseTransformEnabled() const;

		void SetBlendController(unsigned int controller,float val);
		void SetBlendController(const std::string &controller,float val);
		float GetBlendController(const std::string &controller) const;
		float GetBlendController(unsigned int controller) const;
		const std::unordered_map<unsigned int,float> &GetBlendControllers() const;

		void AddAnimationEvent(const std::string &name,uint32_t frameId,const AnimationEvent &ev);
		CallbackHandle AddAnimationEvent(const std::string &name,uint32_t frameId,const std::function<void(void)> &f);
		CallbackHandle AddAnimationEvent(const std::string &name,uint32_t frameId,const CallbackHandle &cb);

		void AddAnimationEvent(uint32_t animId,uint32_t frameId,const AnimationEvent &ev);
		CallbackHandle AddAnimationEvent(uint32_t animId,uint32_t frameId,const std::function<void(void)> &f);
		CallbackHandle AddAnimationEvent(uint32_t animId,uint32_t frameId,const CallbackHandle &cb);
		void ClearAnimationEvents();
		void ClearAnimationEvents(uint32_t animId);
		void ClearAnimationEvents(uint32_t animId,uint32_t frameId);
		void InjectAnimationEvent(const AnimationEvent &ev);

		void ClearAnimationEvents(const std::string &anim);
		void ClearAnimationEvents(const std::string &anim,uint32_t frameId);
		//void RemoveAnimationEvent(uint32_t animId,uint32_t frameId,uint32_t idx);

		const pragma::animation::AnimatedPose &GetProcessedBones() const;
		pragma::animation::AnimatedPose &GetProcessedBones();

		void SetBonePosition(UInt32 boneId,const Vector3 &pos,const Quat &rot,const Vector3 *scale,Bool updatePhysics);

		// Transforms all bone positions / rotations to entity space
		void UpdateSkeleton();

		bool ShouldUpdateBones() const;
		UInt32 GetBoneCount() const;
		const pragma::animation::AnimatedPose &GetBoneTransforms() const;
		pragma::animation::AnimatedPose &GetBoneTransforms();
		const pragma::animation::AnimatedPose &GetProcessedBoneTransforms() const;
		pragma::animation::AnimatedPose &GetProcessedBoneTransforms();

		Activity TranslateActivity(Activity act);
		void SetBaseAnimationDirty();

		void BlendBonePoses(
			const std::vector<umath::Transform> &srcBonePoses,const std::vector<Vector3> *optSrcBoneScales,
			const std::vector<umath::Transform> &dstBonePoses,const std::vector<Vector3> *optDstBoneScales,
			std::vector<umath::Transform> &outBonePoses,std::vector<Vector3> *optOutBoneScales,
			animation::Animation &anim,float interpFactor
		) const;
		void BlendBoneFrames(
			std::vector<umath::Transform> &tgt,std::vector<Vector3> *tgtScales,std::vector<umath::Transform> &add,std::vector<Vector3> *addScales,float blendScale
		) const;

		virtual bool MaintainAnimations(double dt);
		virtual void OnTick(double dt) override;

		virtual std::optional<Mat4> GetVertexTransformMatrix(const ModelSubMesh &subMesh,uint32_t vertexId) const;
		virtual bool GetLocalVertexPosition(const ModelSubMesh &subMesh,uint32_t vertexId,Vector3 &pos,const std::optional<Vector3> &vertexOffset={}) const;
		bool GetVertexPosition(uint32_t meshGroupId,uint32_t meshId,uint32_t subMeshId,uint32_t vertexId,Vector3 &pos) const;
		bool GetVertexPosition(const ModelSubMesh &subMesh,uint32_t vertexId,Vector3 &pos) const;

		void SetBindPose(const std::shared_ptr<pragma::animation::AnimatedPose> &bindPose);
		const pragma::animation::AnimatedPose *GetBindPose() const;

		animation::AnimationPlayer *GetBaseAnimationPlayer();
		const animation::AnimationPlayer *GetBaseAnimationPlayer() const {return const_cast<BaseSkAnimatedComponent*>(this)->GetBaseAnimationPlayer();}
		animation::AnimationPlayer *GetLayeredAnimationPlayer(animation::LayeredAnimationSlot slot);
		const animation::AnimationPlayer *GetLayeredAnimationPlayer(animation::LayeredAnimationSlot slot) const {return const_cast<BaseSkAnimatedComponent*>(this)->GetLayeredAnimationPlayer(slot);}

		CallbackHandle BindAnimationEvent(AnimationEvent::Type eventId,const std::function<void(std::reference_wrapper<const AnimationEvent>)> &fCallback);

		virtual void Save(udm::LinkedPropertyWrapper &udm) override;
		using BaseEntityComponent::Load;
	protected:
		BaseSkAnimatedComponent(BaseEntity &ent);
		virtual void OnModelChanged(const std::shared_ptr<Model> &mdl);
		virtual void Load(udm::LinkedPropertyWrapper &udm,uint32_t version) override;
		void ResetAnimation(const std::shared_ptr<Model> &mdl);
		
		struct DLLNETWORK AnimationBlendInfo
		{
			float scale = 0.f;
			animation::Animation *animation = nullptr;
			Frame *frameSrc = nullptr; // Frame to blend from
			Frame *frameDst = nullptr; // Frame to blend to
		};

		struct AnimationEventQueueItem
		{
			int32_t animId = -1;
			std::shared_ptr<animation::Animation> animation = nullptr;
			int32_t lastFrame = -1;
			uint32_t frameId = 0;
		};

		// Custom animation events
		struct CustomAnimationEvent
			: public AnimationEvent
		{
			std::pair<bool,CallbackHandle> callback = {false,{}};
			CustomAnimationEvent(const AnimationEvent &ev);
			CustomAnimationEvent(const std::function<void(void)> &f);
			CustomAnimationEvent(const CallbackHandle &cb);
			CustomAnimationEvent()=default;
		};
		struct TemplateAnimationEvent
		{
			TemplateAnimationEvent()=default;
			TemplateAnimationEvent(const TemplateAnimationEvent&)=default;
			int32_t animId = -1;
			uint32_t frameId = 0;
			std::string anim;
			CustomAnimationEvent ev = {};
		};
		//

		void HandleAnimationEvent(const AnimationEvent &ev);
		void PlayLayeredAnimation(int slot,int animation,FPlayAnim flags);

		// Animations
		void TransformBoneFrames(std::vector<umath::Transform> &bonePoses,std::vector<Vector3> *boneScales,animation::Animation &anim,Frame *frameBlend,bool bAdd=true);
		void TransformBoneFrames(std::vector<umath::Transform> &tgt,std::vector<Vector3> *boneScales,const std::shared_ptr<animation::Animation> &anim,std::vector<umath::Transform> &add,std::vector<Vector3> *addScales,bool bAdd=true);
		//

		Vector3 m_animDisplacement = {};
		pragma::animation::AnimatedPose m_currentPose = {};
		pragma::animation::AnimatedPose m_currentPoseEntitySpace = {};
	protected:
		// We have to collect the animation events for the current frame and execute them after ALL animations have been completed (In case some events need to access animation data)
		std::queue<AnimationEventQueueItem> m_animEventQueue = std::queue<AnimationEventQueueItem>{};

		// Custom animation events
		void ApplyAnimationEventTemplate(const TemplateAnimationEvent &t);
		void ApplyAnimationEventTemplates();
		std::unordered_map<uint32_t,std::unordered_map<uint32_t,std::vector<CustomAnimationEvent>>> &GetAnimationEvents();
		std::vector<CustomAnimationEvent> *GetAnimationEvents(uint32_t animId,uint32_t frameId);

		std::vector<TemplateAnimationEvent> m_animEventTemplates;
		std::unordered_map<animation::LayeredAnimationSlot,std::unordered_map<uint32_t,std::vector<CustomAnimationEvent>>> m_animEvents;

		animation::PAnimationPlayer m_baseAnimationPlayer = nullptr;
		std::unordered_map<uint32_t,animation::PAnimationPlayer> m_layeredAnimationPlayers {};
		
		BoneId m_rootPoseBoneId = std::numeric_limits<BoneId>::max();
		StateFlags m_stateFlags = StateFlags::AbsolutePosesDirty;
		std::shared_ptr<pragma::animation::AnimatedPose> m_bindPose = nullptr;
		std::unordered_map<unsigned int,float> m_blendControllers = {};

		std::unordered_map<AnimationEvent::Type,CallbackHandle> m_boundAnimEvents;
	};

	// Events

	struct DLLNETWORK CEOnBoneTransformChanged
		: public ComponentEvent
	{
		CEOnBoneTransformChanged(UInt32 boneId,const Vector3 *pos,const Quat *rot,const Vector3 *scale);
		virtual void PushArguments(lua_State *l) override;
		UInt32 boneId;
		const Vector3 *pos;
		const Quat *rot;
		const Vector3 *scale;
	};
	struct DLLNETWORK CELayeredAnimationInfo
		: public ComponentEvent
	{
		CELayeredAnimationInfo(animation::LayeredAnimationSlot slot,animation::AnimationId animation,Activity activity);
		virtual void PushArguments(lua_State *l) override;
		animation::LayeredAnimationSlot slot;
		animation::AnimationId animation;
		Activity activity;
	};
	struct DLLNETWORK CEOnPlayActivity
		: public ComponentEvent
	{
		CEOnPlayActivity(Activity activity,FPlayAnim flags);
		virtual void PushArguments(lua_State *l) override;
		Activity activity;
		pragma::FPlayAnim flags;
	};
	struct DLLNETWORK CEOnPlayLayeredActivity
		: public ComponentEvent
	{
		CEOnPlayLayeredActivity(animation::LayeredAnimationSlot slot,Activity activity,FPlayAnim flags);
		virtual void PushArguments(lua_State *l) override;
		animation::LayeredAnimationSlot slot;
		Activity activity;
		FPlayAnim flags;
	};
	struct DLLNETWORK CESkelOnPlayAnimation
		: public ComponentEvent
	{
		CESkelOnPlayAnimation(animation::AnimationId prevAnim,animation::AnimationId animation,pragma::FPlayAnim flags);
		virtual void PushArguments(lua_State *l) override;
		animation::AnimationId previousAnimation = animation::INVALID_ANIMATION;
		animation::AnimationId animation;
		pragma::FPlayAnim flags;
	};
	struct DLLNETWORK CEOnPlayLayeredAnimation
		: public CESkelOnPlayAnimation
	{
		CEOnPlayLayeredAnimation(animation::LayeredAnimationSlot slot,animation::AnimationId previousAnimation,animation::AnimationId animation,pragma::FPlayAnim flags);
		virtual void PushArguments(lua_State *l) override;
		animation::LayeredAnimationSlot slot;
	};
	struct DLLNETWORK CEOnStopLayeredAnimation
		: public ComponentEvent
	{
		CEOnStopLayeredAnimation(animation::LayeredAnimationSlot slot);
		virtual void PushArguments(lua_State *l) override;
		animation::LayeredAnimationSlot slot;
	};
	struct DLLNETWORK CETranslateLayeredActivity
		: public ComponentEvent
	{
		CETranslateLayeredActivity(animation::LayeredAnimationSlot &slot,Activity &activity,pragma::FPlayAnim &flags);
		virtual void PushArguments(lua_State *l) override;
		virtual uint32_t GetReturnCount() override;
		virtual void HandleReturnValues(lua_State *l) override;
		animation::LayeredAnimationSlot &slot;
		Activity &activity;
		pragma::FPlayAnim &flags;
	};
	struct DLLNETWORK CETranslateLayeredAnimation
		: public ComponentEvent
	{
		CETranslateLayeredAnimation(animation::LayeredAnimationSlot &slot,animation::AnimationId &animation,pragma::FPlayAnim &flags);
		virtual void PushArguments(lua_State *l) override;
		virtual uint32_t GetReturnCount() override;
		virtual void HandleReturnValues(lua_State *l) override;
		animation::LayeredAnimationSlot &slot;
		animation::AnimationId &animation;
		pragma::FPlayAnim &flags;
	};
	struct DLLNETWORK CETranslateActivity
		: public ComponentEvent
	{
		CETranslateActivity(Activity &activity);
		virtual void PushArguments(lua_State *l) override;
		virtual uint32_t GetReturnCount() override;
		virtual void HandleReturnValues(lua_State *l) override;
		Activity &activity;
	};
	struct DLLNETWORK CEMaintainAnimationMovement
		: public ComponentEvent
	{
		CEMaintainAnimationMovement(const Vector3 &displacement);
		virtual void PushArguments(lua_State *l) override;
		const Vector3 &displacement;
	};
	struct DLLNETWORK CEShouldUpdateBones
		: public ComponentEvent
	{
		CEShouldUpdateBones();
		virtual void PushArguments(lua_State *l) override;
		bool shouldUpdate = true;
	};
};
REGISTER_BASIC_BITWISE_OPERATORS(pragma::BaseSkAnimatedComponent::StateFlags)

#endif
