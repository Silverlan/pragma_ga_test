/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan */

#ifndef __WAD_H__
#define __WAD_H__
#include "pragma/networkdefinitions.h"
#include "pragma/file_formats/wdf.h"

namespace pragma {class Animation;};
class DLLNETWORK FWAD
	: FWDF
{
private:
	
public:
	std::shared_ptr<pragma::Animation> ReadData(unsigned short version,VFilePtr f);
	std::shared_ptr<pragma::Animation> Load(unsigned short version,const char *animation);
};

#endif