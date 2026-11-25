// Copyright (c) 2016 Google Inc.
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

// Basic tests for the ValidationState_t datastructure.

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "source/spirv_validator_options.h"
#include "source/util/string_utils.h"
#include "test/unit_spirv.h"
#include "test/val/val_fixtures.h"

namespace spvtools {
namespace val {
namespace {

using ::testing::HasSubstr;

using ValidationStateTest = spvtest::ValidateBase<bool>;

const char kHeader[] =
    " OpCapability Shader"
    " OpCapability Linkage"
    " OpMemoryModel Logical GLSL450 ";

const char kVulkanMemoryHeader[] =
    " OpCapability Shader"
    " OpCapability VulkanMemoryModelKHR"
    " OpExtension \"SPV_KHR_vulkan_memory_model\""
    " OpMemoryModel Logical VulkanKHR ";

const char kVoidFVoid[] =
    " %void   = OpTypeVoid"
    " %void_f = OpTypeFunction %void"
    " %func   = OpFunction %void None %void_f"
    " %label  = OpLabel"
    "           OpReturn"
    "           OpFunctionEnd ";

// k*RecursiveBody examples originally from test/opt/function_test.cpp
const char* kNonRecursiveBody = R"(
OpEntryPoint Fragment %1 "main"
OpExecutionMode %1 OriginUpperLeft
%void = OpTypeVoid
%4 = OpTypeFunction %void
%float = OpTypeFloat 32
%_struct_6 = OpTypeStruct %float %float
%null = OpConstantNull %_struct_6
%7 = OpTypeFunction %_struct_6
%12 = OpFunction %_struct_6 None %7
%13 = OpLabel
OpReturnValue %null
OpFunctionEnd
%9 = OpFunction %_struct_6 None %7
%10 = OpLabel
%11 = OpFunctionCall %_struct_6 %12
OpReturnValue %null
OpFunctionEnd
%1 = OpFunction %void Pure|Const %4
%8 = OpLabel
%2 = OpFunctionCall %_struct_6 %9
OpKill
OpFunctionEnd
)";

const char* kDirectlyRecursiveBody = R"(
OpEntryPoint Fragment %1 "main"
OpExecutionMode %1 OriginUpperLeft
%void = OpTypeVoid
%4 = OpTypeFunction %void
%float = OpTypeFloat 32
%_struct_6 = OpTypeStruct %float %float
%7 = OpTypeFunction %_struct_6
%9 = OpFunction %_struct_6 None %7
%10 = OpLabel
%11 = OpFunctionCall %_struct_6 %9
OpKill
OpFunctionEnd
%1 = OpFunction %void Pure|Const %4
%8 = OpLabel
%2 = OpFunctionCall %_struct_6 %9
OpReturn
OpFunctionEnd
)";

const char* kIndirectlyRecursiveBody = R"(
OpEntryPoint Fragment %1 "main"
OpExecutionMode %1 OriginUpperLeft
%void = OpTypeVoid
%4 = OpTypeFunction %void
%float = OpTypeFloat 32
%_struct_6 = OpTypeStruct %float %float
%null = OpConstantNull %_struct_6
%7 = OpTypeFunction %_struct_6
%9 = OpFunction %_struct_6 None %7
%10 = OpLabel
%11 = OpFunctionCall %_struct_6 %12
OpReturnValue %null
OpFunctionEnd
%12 = OpFunction %_struct_6 None %7
%13 = OpLabel
%14 = OpFunctionCall %_struct_6 %9
OpReturnValue %null
OpFunctionEnd
%1 = OpFunction %void Pure|Const %4
%8 = OpLabel
%2 = OpFunctionCall %_struct_6 %9
OpKill
OpFunctionEnd
)";

// Tests that the instruction count in ValidationState is correct.
TEST_F(ValidationStateTest, CheckNumInstructions) {
  std::string spirv = std::string(kHeader) + "%int = OpTypeInt 32 0";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
  EXPECT_EQ(size_t(4), vstate_->ordered_instructions().size());
}

// Tests that the number of global variables in ValidationState is correct.
TEST_F(ValidationStateTest, CheckNumGlobalVars) {
  std::string spirv = std::string(kHeader) + R"(
     %int = OpTypeInt 32 0
%_ptr_int = OpTypePointer Input %int
   %var_1 = OpVariable %_ptr_int Input
   %var_2 = OpVariable %_ptr_int Input
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
  EXPECT_EQ(unsigned(2), vstate_->num_global_vars());
}

// Tests that the number of local variables in ValidationState is correct.
TEST_F(ValidationStateTest, CheckNumLocalVars) {
  std::string spirv = std::string(kHeader) + R"(
 %int      = OpTypeInt 32 0
 %_ptr_int = OpTypePointer Function %int
 %voidt    = OpTypeVoid
 %funct    = OpTypeFunction %voidt
 %main     = OpFunction %voidt None %funct
 %entry    = OpLabel
 %var_1    = OpVariable %_ptr_int Function
 %var_2    = OpVariable %_ptr_int Function
 %var_3    = OpVariable %_ptr_int Function
 OpReturn
 OpFunctionEnd
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
  EXPECT_EQ(unsigned(3), vstate_->num_local_vars());
}

// Tests that the "id bound" in ValidationState is correct.
TEST_F(ValidationStateTest, CheckIdBound) {
  std::string spirv = std::string(kHeader) + R"(
 %int      = OpTypeInt 32 0
 %voidt    = OpTypeVoid
  )";
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
  EXPECT_EQ(unsigned(3), vstate_->getIdBound());
}

// Tests that the entry_points in ValidationState is correct.
TEST_F(ValidationStateTest, CheckEntryPoints) {
  std::string spirv = std::string(kHeader) +
                      " OpEntryPoint Vertex %func \"shader\"" +
                      std::string(kVoidFVoid);
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
  EXPECT_EQ(size_t(1), vstate_->entry_points().size());
  EXPECT_EQ(spv::Op::OpFunction,
            vstate_->FindDef(vstate_->entry_points()[0])->opcode());
}

TEST_F(ValidationStateTest, CheckStructMemberLimitOption) {
  spvValidatorOptionsSetUniversalLimit(
      options_, spv_validator_limit_max_struct_members, 32000u);
  EXPECT_EQ(32000u, options_->universal_limits_.max_struct_members);
}

TEST_F(ValidationStateTest, CheckNumGlobalVarsLimitOption) {
  spvValidatorOptionsSetUniversalLimit(
      options_, spv_validator_limit_max_global_variables, 100u);
  EXPECT_EQ(100u, options_->universal_limits_.max_global_variables);
}

TEST_F(ValidationStateTest, CheckNumLocalVarsLimitOption) {
  spvValidatorOptionsSetUniversalLimit(
      options_, spv_validator_limit_max_local_variables, 100u);
  EXPECT_EQ(100u, options_->universal_limits_.max_local_variables);
}

TEST_F(ValidationStateTest, CheckStructDepthLimitOption) {
  spvValidatorOptionsSetUniversalLimit(
      options_, spv_validator_limit_max_struct_depth, 100u);
  EXPECT_EQ(100u, options_->universal_limits_.max_struct_depth);
}

TEST_F(ValidationStateTest, CheckSwitchBranchesLimitOption) {
  spvValidatorOptionsSetUniversalLimit(
      options_, spv_validator_limit_max_switch_branches, 100u);
  EXPECT_EQ(100u, options_->universal_limits_.max_switch_branches);
}

TEST_F(ValidationStateTest, CheckFunctionArgsLimitOption) {
  spvValidatorOptionsSetUniversalLimit(
      options_, spv_validator_limit_max_function_args, 100u);
  EXPECT_EQ(100u, options_->universal_limits_.max_function_args);
}

TEST_F(ValidationStateTest, CheckCFGDepthLimitOption) {
  spvValidatorOptionsSetUniversalLimit(
      options_, spv_validator_limit_max_control_flow_nesting_depth, 100u);
  EXPECT_EQ(100u, options_->universal_limits_.max_control_flow_nesting_depth);
}

TEST_F(ValidationStateTest, CheckAccessChainIndexesLimitOption) {
  spvValidatorOptionsSetUniversalLimit(
      options_, spv_validator_limit_max_access_chain_indexes, 100u);
  EXPECT_EQ(100u, options_->universal_limits_.max_access_chain_indexes);
}

TEST_F(ValidationStateTest, CheckDescriptorHeapLayoutOptions) {
  EXPECT_EQ(0u, options_->buffer_descriptor_layout.size);
  EXPECT_EQ(1u, options_->buffer_descriptor_layout.alignment);
  EXPECT_EQ(0u, options_->image_descriptor_layout.size);
  EXPECT_EQ(1u, options_->image_descriptor_layout.alignment);
  EXPECT_EQ(0u, options_->sampler_descriptor_layout.size);
  EXPECT_EQ(1u, options_->sampler_descriptor_layout.alignment);
  EXPECT_EQ(0u, options_->tensor_descriptor_layout.size);
  EXPECT_EQ(1u, options_->tensor_descriptor_layout.alignment);

  spvValidatorOptionsSetBufferDescriptorLayout(options_, 16u, 8u);
  spvValidatorOptionsSetImageDescriptorLayout(options_, 64u, 16u);
  spvValidatorOptionsSetSamplerDescriptorLayout(options_, 32u, 8u);
  spvValidatorOptionsSetTensorDescriptorLayout(options_, 128u, 32u);

  EXPECT_EQ(16u, options_->buffer_descriptor_layout.size);
  EXPECT_EQ(8u, options_->buffer_descriptor_layout.alignment);
  EXPECT_EQ(64u, options_->image_descriptor_layout.size);
  EXPECT_EQ(16u, options_->image_descriptor_layout.alignment);
  EXPECT_EQ(32u, options_->sampler_descriptor_layout.size);
  EXPECT_EQ(8u, options_->sampler_descriptor_layout.alignment);
  EXPECT_EQ(128u, options_->tensor_descriptor_layout.size);
  EXPECT_EQ(32u, options_->tensor_descriptor_layout.alignment);
}

TEST_F(ValidationStateTest, CheckNonRecursiveBodyGood) {
  std::string spirv = std::string(kHeader) + kNonRecursiveBody;
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidationStateTest, CheckVulkanNonRecursiveBodyGood) {
  std::string spirv = std::string(kVulkanMemoryHeader) + kNonRecursiveBody;
  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
}

TEST_F(ValidationStateTest, CheckDirectlyRecursiveBodyGood) {
  std::string spirv = std::string(kHeader) + kDirectlyRecursiveBody;
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidationStateTest, CheckVulkanDirectlyRecursiveBodyBad) {
  std::string spirv = std::string(kVulkanMemoryHeader) + kDirectlyRecursiveBody;
  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_BINARY,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-None-04634"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Entry points may not have a call graph with cycles.\n "
                        " %1 = OpFunction %void Pure|Const %3\n"));
}

TEST_F(ValidationStateTest, CheckIndirectlyRecursiveBodyGood) {
  std::string spirv = std::string(kHeader) + kIndirectlyRecursiveBody;
  CompileSuccessfully(spirv);
  EXPECT_EQ(SPV_SUCCESS, ValidateAndRetrieveValidationState());
}

TEST_F(ValidationStateTest, CheckVulkanIndirectlyRecursiveBodyBad) {
  std::string spirv =
      std::string(kVulkanMemoryHeader) + kIndirectlyRecursiveBody;
  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_BINARY,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-None-04634"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Entry points may not have a call graph with cycles.\n "
                        " %1 = OpFunction %void Pure|Const %3\n"));
}

// The following multi-entry call graph tests exercise a shared function
// library where every entry point reaches a different subgraph. Explicit ids
// are used so the computed function -> entry-point mapping can be asserted.

// Two entry points of different execution models may legally share the same
// acyclic helper. Sharing must not be mistaken for recursion.
TEST_F(ValidationStateTest,
       VulkanCallGraphSharedAcyclicHelperBetweenEntriesGood) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %10 "frag_main"
OpEntryPoint Vertex %11 "vert_main"
OpExecutionMode %10 OriginUpperLeft
%1 = OpTypeVoid
%2 = OpTypeFunction %1
%20 = OpFunction %1 None %2
%21 = OpLabel
OpReturn
OpFunctionEnd
%10 = OpFunction %1 None %2
%12 = OpLabel
%13 = OpFunctionCall %1 %20
OpReturn
OpFunctionEnd
%11 = OpFunction %1 None %2
%14 = OpLabel
%15 = OpFunctionCall %1 %20
OpReturn
OpFunctionEnd
)";

  SetAssembleOptions(SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  // The shared helper is reachable from both entry points.
  EXPECT_EQ((std::vector<uint32_t>{10u, 11u}),
            vstate_->FunctionEntryPoints(20u));
  EXPECT_TRUE(vstate_->recursive_entry_points().empty());
}

// A recursive cycle that is not reachable from any entry point is outside every
// entry point's static call graph and must be accepted in Vulkan.
TEST_F(ValidationStateTest,
       VulkanCallGraphUnreachableRecursiveLibraryFunctionsGood) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %10 "main"
OpExecutionMode %10 OriginUpperLeft
%1 = OpTypeVoid
%2 = OpTypeFunction %1
%20 = OpFunction %1 None %2
%21 = OpLabel
OpReturn
OpFunctionEnd
%30 = OpFunction %1 None %2
%32 = OpLabel
%33 = OpFunctionCall %1 %31
OpReturn
OpFunctionEnd
%31 = OpFunction %1 None %2
%34 = OpLabel
%35 = OpFunctionCall %1 %30
OpReturn
OpFunctionEnd
%10 = OpFunction %1 None %2
%12 = OpLabel
%13 = OpFunctionCall %1 %20
OpReturn
OpFunctionEnd
)";

  SetAssembleOptions(SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_SUCCESS,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  EXPECT_TRUE(vstate_->recursive_entry_points().empty());
  // The dead recursive pair belongs to no entry point.
  EXPECT_TRUE(vstate_->FunctionEntryPoints(30u).empty());
  EXPECT_TRUE(vstate_->FunctionEntryPoints(31u).empty());
}

// Two entries share an acyclic helper, but only one of them additionally
// reaches a recursive cycle. Vulkan must reject the module and the diagnostic
// must be anchored on the affected entry point, not the clean one.
TEST_F(ValidationStateTest,
       VulkanCallGraphRecursionReachableFromOneEntryNamesThatEntry) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %10 "good_main"
OpEntryPoint Vertex %11 "bad_main"
OpExecutionMode %10 OriginUpperLeft
OpName %10 "good_entry"
OpName %11 "bad_entry"
%1 = OpTypeVoid
%2 = OpTypeFunction %1
%20 = OpFunction %1 None %2
%21 = OpLabel
OpReturn
OpFunctionEnd
%30 = OpFunction %1 None %2
%32 = OpLabel
%33 = OpFunctionCall %1 %30
OpReturn
OpFunctionEnd
%10 = OpFunction %1 None %2
%12 = OpLabel
%13 = OpFunctionCall %1 %20
OpReturn
OpFunctionEnd
%11 = OpFunction %1 None %2
%14 = OpLabel
%16 = OpFunctionCall %1 %20
%15 = OpFunctionCall %1 %30
OpReturn
OpFunctionEnd
)";

  SetAssembleOptions(SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_BINARY,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-None-04634"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("Entry points may not have a call graph with cycles."));
  // The error points at the recursive Vertex entry point.
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("%bad_entry = OpFunction"));
  EXPECT_THAT(getDiagnosticString(),
              ::testing::Not(HasSubstr("%good_entry = OpFunction")));

  // Only entry %11 reaches the self recursion.
  const auto& recursive = vstate_->recursive_entry_points();
  EXPECT_EQ(1u, recursive.size());
  EXPECT_NE(recursive.end(), recursive.find(11u));
  // The shared helper is still reachable from both entries, while the
  // recursive function is reachable only from the bad entry.
  EXPECT_EQ((std::vector<uint32_t>{10u, 11u}),
            vstate_->FunctionEntryPoints(20u));
  EXPECT_EQ((std::vector<uint32_t>{11u}),
            vstate_->FunctionEntryPoints(30u));
}

// When two entry points both reach the same cycle, both are recorded as
// affected and the emitted diagnostic is anchored on the first one.
TEST_F(ValidationStateTest,
       VulkanCallGraphRecursionReachableFromBothEntriesRecordsBoth) {
  const std::string spirv = R"(
OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %10 "first_main"
OpEntryPoint Fragment %11 "second_main"
OpExecutionMode %10 OriginUpperLeft
OpExecutionMode %11 OriginUpperLeft
OpName %10 "first_entry"
OpName %11 "second_entry"
%1 = OpTypeVoid
%2 = OpTypeFunction %1
%30 = OpFunction %1 None %2
%32 = OpLabel
%33 = OpFunctionCall %1 %30
OpReturn
OpFunctionEnd
%10 = OpFunction %1 None %2
%12 = OpLabel
%13 = OpFunctionCall %1 %30
OpReturn
OpFunctionEnd
%11 = OpFunction %1 None %2
%14 = OpLabel
%15 = OpFunctionCall %1 %30
OpReturn
OpFunctionEnd
)";

  SetAssembleOptions(SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_BINARY,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  EXPECT_THAT(getDiagnosticString(),
              AnyVUID("VUID-StandaloneSpirv-None-04634"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("%first_entry = OpFunction"));

  const auto& recursive = vstate_->recursive_entry_points();
  EXPECT_EQ(2u, recursive.size());
  EXPECT_NE(recursive.end(), recursive.find(10u));
  EXPECT_NE(recursive.end(), recursive.find(11u));
  EXPECT_EQ((std::vector<uint32_t>{10u, 11u}),
            vstate_->FunctionEntryPoints(30u));
}

// A stage-restricted instruction (OpDPdx is limited to Fragment/GLCompute/
// MeshEXT/TaskEXT models) accepted in a Fragment entry through a call chain.
TEST_F(ValidationStateTest,
       VulkanCallGraphStageLimitationThroughChainAcceptedForFragment) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability DerivativeControl
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %10 "frag_main"
OpExecutionMode %10 OriginUpperLeft
%1 = OpTypeVoid
%2 = OpTypeFunction %1
%3 = OpTypeFloat 32
%20 = OpFunction %1 None %2
%4 = OpLabel
%5 = OpUndef %3
%6 = OpDPdx %3 %5
OpReturn
OpFunctionEnd
%21 = OpFunction %1 None %2
%7 = OpLabel
%8 = OpFunctionCall %1 %20
OpReturn
OpFunctionEnd
%10 = OpFunction %1 None %2
%12 = OpLabel
%13 = OpFunctionCall %1 %21
OpReturn
OpFunctionEnd
)";

  SetAssembleOptions(SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_1));
}

// The same call chain under a Vertex entry propagates the derivative stage
// restriction back to the entry point and names both the entry and the
// function that contains the restricted instruction.
TEST_F(ValidationStateTest,
       VulkanCallGraphStageLimitationThroughChainRejectedForVertex) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability DerivativeControl
OpMemoryModel Logical GLSL450
OpEntryPoint Vertex %11 "vert_main"
OpName %11 "vert_entry"
OpName %20 "derivative_helper"
%1 = OpTypeVoid
%2 = OpTypeFunction %1
%3 = OpTypeFloat 32
%20 = OpFunction %1 None %2
%4 = OpLabel
%5 = OpUndef %3
%6 = OpDPdx %3 %5
OpReturn
OpFunctionEnd
%21 = OpFunction %1 None %2
%7 = OpLabel
%8 = OpFunctionCall %1 %20
OpReturn
OpFunctionEnd
%11 = OpFunction %1 None %2
%14 = OpLabel
%15 = OpFunctionCall %1 %21
OpReturn
OpFunctionEnd
)";

  SetAssembleOptions(SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  const std::string diag = getDiagnosticString();
  EXPECT_THAT(diag, HasSubstr("OpEntryPoint Entry Point <id> "));
  EXPECT_THAT(diag,
              HasSubstr("[%vert_entry]'s callgraph contains function <id>"));
  EXPECT_THAT(
      diag, HasSubstr("[%derivative_helper]', which cannot be used with the "
                      "current execution model"));
  EXPECT_THAT(diag,
              HasSubstr("Derivative instructions require Fragment, GLCompute, "
                        "MeshEXT or TaskEXT execution model: DPdx"));
  // The propagation mapping reaches the helper through the intermediate
  // caller.
  EXPECT_EQ((std::vector<uint32_t>{11u}),
            vstate_->FunctionEntryPoints(20u));
}

// One library chain is shared by a legal Fragment entry and an illegal Vertex
// entry. The diagnostic must single out the Vertex entry as the affected one
// even though the Fragment entry reaches the exact same helper.
TEST_F(ValidationStateTest,
       VulkanCallGraphStageLimitationNamesAffectedEntryAmongSharedEntries) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability DerivativeControl
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %10 "frag_main"
OpEntryPoint Vertex %11 "vert_main"
OpExecutionMode %10 OriginUpperLeft
OpName %10 "frag_entry"
OpName %11 "vert_entry"
OpName %20 "derivative_helper"
%1 = OpTypeVoid
%2 = OpTypeFunction %1
%3 = OpTypeFloat 32
%20 = OpFunction %1 None %2
%4 = OpLabel
%5 = OpUndef %3
%6 = OpDPdx %3 %5
OpReturn
OpFunctionEnd
%21 = OpFunction %1 None %2
%7 = OpLabel
%8 = OpFunctionCall %1 %20
OpReturn
OpFunctionEnd
%10 = OpFunction %1 None %2
%12 = OpLabel
%13 = OpFunctionCall %1 %21
OpReturn
OpFunctionEnd
%11 = OpFunction %1 None %2
%14 = OpLabel
%15 = OpFunctionCall %1 %21
OpReturn
OpFunctionEnd
)";

  SetAssembleOptions(SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_ERROR_INVALID_ID,
            ValidateAndRetrieveValidationState(SPV_ENV_VULKAN_1_1));
  const std::string diag = getDiagnosticString();
  EXPECT_THAT(diag,
              HasSubstr("[%vert_entry]'s callgraph contains function <id>"));
  EXPECT_THAT(
      diag, HasSubstr("[%derivative_helper]', which cannot be used with the "
                      "current execution model"));
  EXPECT_THAT(diag, ::testing::Not(HasSubstr("[%frag_entry]")));
  // Both entries do reach the shared helper; the model check distinguishes
  // them.
  EXPECT_EQ((std::vector<uint32_t>{10u, 11u}),
            vstate_->FunctionEntryPoints(20u));
  EXPECT_EQ((std::vector<uint32_t>{10u, 11u}),
            vstate_->FunctionEntryPoints(21u));
}

// Two legal call branches sharing one stage-restricted helper must still be
// accepted: shared use does not create a cycle or a stage violation.
TEST_F(ValidationStateTest,
       VulkanCallGraphTwoLegalBranchesShareStageLimitedHelperGood) {
  const std::string spirv = R"(
OpCapability Shader
OpCapability DerivativeControl
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %10 "main"
OpExecutionMode %10 OriginUpperLeft
%1 = OpTypeVoid
%2 = OpTypeFunction %1
%3 = OpTypeFloat 32
%20 = OpFunction %1 None %2
%4 = OpLabel
%5 = OpUndef %3
%6 = OpDPdx %3 %5
OpReturn
OpFunctionEnd
%21 = OpFunction %1 None %2
%22 = OpLabel
%23 = OpFunctionCall %1 %20
OpReturn
OpFunctionEnd
%24 = OpFunction %1 None %2
%25 = OpLabel
%26 = OpFunctionCall %1 %20
OpReturn
OpFunctionEnd
%10 = OpFunction %1 None %2
%12 = OpLabel
%13 = OpFunctionCall %1 %21
%14 = OpFunctionCall %1 %24
OpReturn
OpFunctionEnd
)";

  SetAssembleOptions(SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  CompileSuccessfully(spirv, SPV_ENV_VULKAN_1_1);
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions(SPV_ENV_VULKAN_1_1));
}

TEST_F(ValidationStateTest, EvalConstantValWiderThan64BitsIsNotEvaluable) {
  const uint32_t kInt128Id = 1;
  const uint32_t kConstId = 2;

  std::vector<uint32_t> words = spvtest::Concatenate({
      {spv::MagicNumber, 0x10000, 0u, 3u /* id bound */, 0u},
      spvtest::MakeInstruction(spv::Op::OpCapability,
                               {uint32_t(spv::Capability::Shader)}),
      spvtest::MakeInstruction(
          spv::Op::OpCapability,
          {uint32_t(spv::Capability::ArbitraryPrecisionIntegersINTEL)}),
      spvtest::MakeInstruction(spv::Op::OpCapability,
                               {uint32_t(spv::Capability::Linkage)}),
      spvtest::MakeInstruction(
          spv::Op::OpExtension,
          utils::MakeVector("SPV_INTEL_arbitrary_precision_integers")),
      spvtest::MakeInstruction(spv::Op::OpMemoryModel,
                               {uint32_t(spv::AddressingModel::Logical),
                                uint32_t(spv::MemoryModel::GLSL450)}),
      spvtest::MakeInstruction(spv::Op::OpTypeInt, {kInt128Id, 128u, 0u}),
      spvtest::MakeInstruction(spv::Op::OpConstant,
                               {kInt128Id, kConstId, 4u, 0u, 0u, 0u}),
  });

  spv_diagnostic diagnostic = nullptr;
  spvtest::ScopedContext context;
  EXPECT_EQ(SPV_SUCCESS,
            spvtools::val::ValidateBinaryAndKeepValidationState(
                context.context, getValidatorOptions(), words.data(),
                words.size(), &diagnostic, &vstate_));
  spvDiagnosticDestroy(diagnostic);

  uint64_t uval = 0xdeadbeef;
  EXPECT_FALSE(vstate_->EvalConstantValUint64(kConstId, &uval));

  int64_t ival = 0xdeadbeef;
  EXPECT_FALSE(vstate_->EvalConstantValInt64(kConstId, &ival));
}

}  // namespace
}  // namespace val
}  // namespace spvtools
