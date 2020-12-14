/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2020 Florian Weischer
 */

#ifndef __FLEX_ANIMATION_HPP__
#define __FLEX_ANIMATION_HPP__

#include "pragma/networkdefinitions.h"

using FlexControllerId = uint32_t;
class DLLNETWORK FlexAnimationFrame
	: public std::enable_shared_from_this<FlexAnimationFrame>
{
public:
	FlexAnimationFrame()=default;
	std::vector<float> &GetValues() {return m_flexControllerValues;}
	const std::vector<float> &GetValues() const {return const_cast<FlexAnimationFrame*>(this)->GetValues();}
private:
	std::vector<float> m_flexControllerValues;
};

class DLLNETWORK FlexAnimation
	: public std::enable_shared_from_this<FlexAnimation>
{
public:
	static constexpr uint32_t FORMAT_VERSION = 1u;
	static std::shared_ptr<FlexAnimation> Load(std::shared_ptr<VFilePtrInternal> &f);
	FlexAnimation()=default;
	std::vector<std::shared_ptr<FlexAnimationFrame>> &GetFrames() {return m_frames;}
	const std::vector<std::shared_ptr<FlexAnimationFrame>> &GetFrames() const {return const_cast<FlexAnimation*>(this)->GetFrames();}
	std::vector<FlexControllerId> &GetFlexControllerIds() {return m_flexControllerIds;}
	const std::vector<FlexControllerId> &GetFlexControllerIds() const {return const_cast<FlexAnimation*>(this)->GetFlexControllerIds();}
	uint32_t AddFlexControllerId(FlexControllerId id);
	FlexAnimationFrame &AddFrame();
	void SetFlexControllerIds(std::vector<FlexControllerId> &&ids);
	void SetFps(float fps) {m_fps = fps;}
	float GetFps() const {return m_fps;}

	bool Save(std::shared_ptr<VFilePtrInternalReal> &f);
private:
	std::vector<std::shared_ptr<FlexAnimationFrame>> m_frames;
	std::vector<FlexControllerId> m_flexControllerIds;
	float m_fps = 24.f;
};

#endif