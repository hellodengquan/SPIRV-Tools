// Copyright (c) 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "source/opt/rename_debug_source_pass.h"

#include "source/opt/debug_info_manager.h"
#include "source/opt/ir_context.h"

namespace spvtools {
namespace opt {

Pass::Status RenameDebugSourcePass::Process() {
  const bool changed =
      context()->get_debug_info_mgr()->RenameDebugSourceNames(
          source_name_map_);
  return changed ? Status::SuccessWithChange : Status::SuccessWithoutChange;
}

}  // namespace opt
}  // namespace spvtools
