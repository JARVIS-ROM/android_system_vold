/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ObbVolume.h"
#include "Loop.h"
#include "Utils.h"
#include "VoldUtil.h"
#include "fs/Vfat.h"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <cutils/fs.h>
#include <private/android_filesystem_config.h>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>

using android::base::StringPrintf;

namespace android {
namespace vold {

ObbVolume::ObbVolume(int id, const std::string& sourcePath, gid_t ownerGid)
    : VolumeBase(Type::kObb) {
    setId(StringPrintf("obb:%d", id));
    mSourcePath = sourcePath;
    mOwnerGid = ownerGid;
}

ObbVolume::~ObbVolume() {}

status_t ObbVolume::doCreate() {
    if (Loop::create(mSourcePath, mLoopPath)) {
        PLOG(ERROR) << getId() << " failed to create loop";
        return -1;
    }
    return OK;
}

status_t ObbVolume::doDestroy() {
    if (!mLoopPath.empty() && Loop::destroyByDevice(mLoopPath.c_str())) {
        PLOG(WARNING) << getId() << " failed to destroy loop";
    }
    mLoopPath.clear();
    return OK;
}

status_t ObbVolume::doMount() {
    auto path = StringPrintf("/mnt/obb/%s", getId().c_str());
    setPath(path);

    if (fs_prepare_dir(path.c_str(), 0700, AID_ROOT, AID_ROOT)) {
        PLOG(ERROR) << getId() << " failed to create mount point";
        return -1;
    }
    // clang-format off
    if (android::vold::vfat::Mount(mLoopPath, path, true, false, true,
                                   0, mOwnerGid, 0227, false)) {
        // clang-format on
        PLOG(ERROR) << getId() << " failed to mount";
        return -1;
    }
    return OK;
}

status_t ObbVolume::doUnmount() {
    auto path = getPath();

    KillProcessesUsingPath(path);
    ForceUnmount(path);
    rmdir(path.c_str());

    return OK;
}

}  // namespace vold
}  // namespace android
