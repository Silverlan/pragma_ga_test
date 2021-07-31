/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan
 */

#include "stdafx_shared.h"
#include "pragma/lua/base_lua_handle.hpp"

#pragma optimize("",off)
pragma::BaseLuaHandle::BaseLuaHandle()
	: m_handle{this,[](BaseLuaHandle*) {}}
{}
void pragma::BaseLuaHandle::InvalidateHandle()
{
	m_handle = {};
}
void pragma::BaseLuaHandle::SetLuaObject(const luabind::object &o)
{
	m_luaObj = o;
}
pragma::BaseLuaHandle::~BaseLuaHandle()
{
	InvalidateHandle();
}
#pragma optimize("",on)