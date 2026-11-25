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

#ifndef SOURCE_OPT_RENAME_DEBUG_SOURCE_PASS_H_
#define SOURCE_OPT_RENAME_DEBUG_SOURCE_PASS_H_

#include <string>
#include <unordered_map>

#include "source/opt/pass.h"

namespace spvtools {
namespace opt {

// See optimizer.hpp for documentation.
class RenameDebugSourcePass : public Pass {
 public:
  // |source_name_map| maps names (paths) currently used as debug source names
  // to their replacement names. See
  // DebugInfoManager::RenameDebugSourceNames for the exact semantics.
  explicit RenameDebugSourcePass(
      std::unordered_map<std::string, std::string> source_name_map)
      : source_name_map_(std::move(source_name_map)) {}

  const char* name() const override { return "rename-debug-source"; }
  Status Process() override;

 private:
  std::unordered_map<std::string, std::string> source_name_map_;
};

}  // namespace opt
}  // namespace spvtools

#endif  // SOURCE_OPT_RENAME_DEBUG_SOURCE_PASS_H_
