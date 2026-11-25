// Copyright (c) 2020 Google LLC
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

#include "source/opt/debug_info_manager.h"

#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "source/opt/build_module.h"
#include "source/opt/instruction.h"
#include "spirv-tools/libspirv.hpp"
#include "spirv-tools/optimizer.hpp"

// Constants for OpenCL.DebugInfo.100 extension instructions.

static const uint32_t kDebugFunctionOperandFunctionIndex = 13;
static const uint32_t kDebugInlinedAtOperandLineIndex = 4;
static const uint32_t kDebugInlinedAtOperandScopeIndex = 5;
static const uint32_t kDebugInlinedAtOperandInlinedIndex = 6;
static const uint32_t kOpLineInOperandFileIndex = 0;
static const uint32_t kOpLineInOperandLineIndex = 1;
static const uint32_t kOpLineInOperandColumnIndex = 2;

namespace spvtools {
namespace opt {
namespace analysis {
namespace {

TEST(DebugInfoManager, GetDebugInlinedAt) {
  const std::string text = R"(
               OpCapability Shader
          %1 = OpExtInstImport "OpenCL.DebugInfo.100"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %in_var_COLOR
               OpExecutionMode %main OriginUpperLeft
          %5 = OpString "ps.hlsl"
         %14 = OpString "#line 1 \"ps.hlsl\"
void main(float in_var_color : COLOR) {
  float color = in_var_color;
}
"
         %17 = OpString "float"
         %21 = OpString "main"
         %24 = OpString "color"
               OpName %in_var_COLOR "in.var.COLOR"
               OpName %main "main"
               OpDecorate %in_var_COLOR Location 0
       %uint = OpTypeInt 32 0
    %uint_32 = OpConstant %uint 32
      %float = OpTypeFloat 32
%_ptr_Input_float = OpTypePointer Input %float
       %void = OpTypeVoid
         %27 = OpTypeFunction %void
%_ptr_Function_float = OpTypePointer Function %float
%in_var_COLOR = OpVariable %_ptr_Input_float Input
         %13 = OpExtInst %void %1 DebugExpression
         %15 = OpExtInst %void %1 DebugSource %5 %14
         %16 = OpExtInst %void %1 DebugCompilationUnit 1 4 %15 HLSL
         %18 = OpExtInst %void %1 DebugTypeBasic %17 %uint_32 Float
         %20 = OpExtInst %void %1 DebugTypeFunction FlagIsProtected|FlagIsPrivate %18 %18
         %22 = OpExtInst %void %1 DebugFunction %21 %20 %15 1 1 %16 %21 FlagIsProtected|FlagIsPrivate 1 %main
        %100 = OpExtInst %void %1 DebugInlinedAt 7 %22
       %main = OpFunction %void None %27
         %28 = OpLabel
         %31 = OpLoad %float %in_var_COLOR
               OpStore %100 %31
               OpReturn
               OpFunctionEnd
  )";

  std::unique_ptr<IRContext> context =
      BuildModule(SPV_ENV_UNIVERSAL_1_1, nullptr, text,
                  SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  DebugInfoManager manager(context.get());

  EXPECT_EQ(manager.GetDebugInlinedAt(150), nullptr);
  EXPECT_EQ(manager.GetDebugInlinedAt(31), nullptr);
  EXPECT_EQ(manager.GetDebugInlinedAt(22), nullptr);

  auto* inst = manager.GetDebugInlinedAt(100);
  EXPECT_EQ(inst->GetSingleWordOperand(kDebugInlinedAtOperandLineIndex), 7);
  EXPECT_EQ(inst->GetSingleWordOperand(kDebugInlinedAtOperandScopeIndex), 22);
}

TEST(DebugInfoManager, CreateDebugInlinedAt) {
  const std::string text = R"(
               OpCapability Shader
          %1 = OpExtInstImport "OpenCL.DebugInfo.100"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %in_var_COLOR
               OpExecutionMode %main OriginUpperLeft
          %5 = OpString "ps.hlsl"
         %14 = OpString "#line 1 \"ps.hlsl\"
void main(float in_var_color : COLOR) {
  float color = in_var_color;
}
"
         %17 = OpString "float"
         %21 = OpString "main"
         %24 = OpString "color"
               OpName %in_var_COLOR "in.var.COLOR"
               OpName %main "main"
               OpDecorate %in_var_COLOR Location 0
       %uint = OpTypeInt 32 0
    %uint_32 = OpConstant %uint 32
      %float = OpTypeFloat 32
%_ptr_Input_float = OpTypePointer Input %float
       %void = OpTypeVoid
         %27 = OpTypeFunction %void
%_ptr_Function_float = OpTypePointer Function %float
%in_var_COLOR = OpVariable %_ptr_Input_float Input
         %13 = OpExtInst %void %1 DebugExpression
         %15 = OpExtInst %void %1 DebugSource %5 %14
         %16 = OpExtInst %void %1 DebugCompilationUnit 1 4 %15 HLSL
         %18 = OpExtInst %void %1 DebugTypeBasic %17 %uint_32 Float
         %20 = OpExtInst %void %1 DebugTypeFunction FlagIsProtected|FlagIsPrivate %18 %18
         %22 = OpExtInst %void %1 DebugFunction %21 %20 %15 1 1 %16 %21 FlagIsProtected|FlagIsPrivate 1 %main
        %100 = OpExtInst %void %1 DebugInlinedAt 7 %22
       %main = OpFunction %void None %27
         %28 = OpLabel
         %31 = OpLoad %float %in_var_COLOR
               OpStore %100 %31
               OpReturn
               OpFunctionEnd
  )";

  DebugScope scope(22U, 0U);

  std::unique_ptr<IRContext> context =
      BuildModule(SPV_ENV_UNIVERSAL_1_1, nullptr, text,
                  SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  DebugInfoManager manager(context.get());

  uint32_t inlined_at_id = manager.CreateDebugInlinedAt(nullptr, scope);
  auto* inlined_at = manager.GetDebugInlinedAt(inlined_at_id);
  EXPECT_NE(inlined_at, nullptr);
  EXPECT_EQ(inlined_at->GetSingleWordOperand(kDebugInlinedAtOperandLineIndex),
            1);
  EXPECT_EQ(inlined_at->GetSingleWordOperand(kDebugInlinedAtOperandScopeIndex),
            22);
  EXPECT_EQ(inlined_at->NumOperands(), kDebugInlinedAtOperandScopeIndex + 1);

  const uint32_t line_number = 77U;
  Instruction line(context.get(), spv::Op::OpLine);
  line.SetInOperands({
      {spv_operand_type_t::SPV_OPERAND_TYPE_ID, {5U}},
      {spv_operand_type_t::SPV_OPERAND_TYPE_LITERAL_INTEGER, {line_number}},
      {spv_operand_type_t::SPV_OPERAND_TYPE_LITERAL_INTEGER, {0U}},
  });

  inlined_at_id = manager.CreateDebugInlinedAt(&line, scope);
  inlined_at = manager.GetDebugInlinedAt(inlined_at_id);
  EXPECT_NE(inlined_at, nullptr);
  EXPECT_EQ(inlined_at->GetSingleWordOperand(kDebugInlinedAtOperandLineIndex),
            line_number);
  EXPECT_EQ(inlined_at->GetSingleWordOperand(kDebugInlinedAtOperandScopeIndex),
            22);
  EXPECT_EQ(inlined_at->NumOperands(), kDebugInlinedAtOperandScopeIndex + 1);

  scope.SetInlinedAt(100U);
  inlined_at_id = manager.CreateDebugInlinedAt(&line, scope);
  inlined_at = manager.GetDebugInlinedAt(inlined_at_id);
  EXPECT_NE(inlined_at, nullptr);
  EXPECT_EQ(inlined_at->GetSingleWordOperand(kDebugInlinedAtOperandLineIndex),
            line_number);
  EXPECT_EQ(inlined_at->GetSingleWordOperand(kDebugInlinedAtOperandScopeIndex),
            22);
  EXPECT_EQ(inlined_at->NumOperands(), kDebugInlinedAtOperandInlinedIndex + 1);
  EXPECT_EQ(
      inlined_at->GetSingleWordOperand(kDebugInlinedAtOperandInlinedIndex),
      100U);
}

TEST(DebugInfoManager, CreateDebugInlinedAtWithConstantManager) {
  // Show that CreateDebugInlinedAt will use the Constant manager to generate
  // its line operand if the Constant and DefUse managers are valid. This is
  // proven by checking that the id for the line operand 7 is the same as the
  // existing constant 7.
  //
  // int function1() {
  //   return 1;
  // }
  //
  // void main() {
  //   function1();
  // }
  const std::string text = R"(OpCapability Shader
OpExtension "SPV_KHR_non_semantic_info"
%1 = OpExtInstImport "NonSemantic.Shader.DebugInfo.100"
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main"
OpExecutionMode %main OriginUpperLeft
%3 = OpString "parent3.hlsl"
%8 = OpString "int"
%19 = OpString "function1"
%20 = OpString ""
%26 = OpString "main"
OpName %main "main"
OpName %src_main "src.main"
OpName %bb_entry "bb.entry"
OpName %function1 "function1"
OpName %bb_entry_0 "bb.entry"
%int = OpTypeInt 32 1
%int_1 = OpConstant %int 1
%uint = OpTypeInt 32 0
%uint_32 = OpConstant %uint 32
%void = OpTypeVoid
%uint_4 = OpConstant %uint 4
%uint_0 = OpConstant %uint 0
%uint_3 = OpConstant %uint 3
%uint_1 = OpConstant %uint 1
%uint_5 = OpConstant %uint 5
%uint_2 = OpConstant %uint 2
%uint_17 = OpConstant %uint 17
%uint_6 = OpConstant %uint 6
%uint_13 = OpConstant %uint 13
%100 = OpConstant %uint 7
%31 = OpTypeFunction %void
%42 = OpTypeFunction %int
%10 = OpExtInst %void %1 DebugTypeBasic %8 %uint_32 %uint_4 %uint_0
%13 = OpExtInst %void %1 DebugTypeFunction %uint_3 %10
%15 = OpExtInst %void %1 DebugSource %3
%16 = OpExtInst %void %1 DebugCompilationUnit %uint_1 %uint_4 %15 %uint_5
%21 = OpExtInst %void %1 DebugFunction %19 %13 %15 %uint_2 %uint_1 %16 %20 %uint_3 %uint_2
%23 = OpExtInst %void %1 DebugLexicalBlock %15 %uint_2 %uint_17 %21
%25 = OpExtInst %void %1 DebugTypeFunction %uint_3 %void
%27 = OpExtInst %void %1 DebugFunction %26 %25 %15 %uint_6 %uint_1 %16 %20 %uint_3 %uint_6
%29 = OpExtInst %void %1 DebugLexicalBlock %15 %uint_6 %uint_13 %27
%main = OpFunction %void None %31
%32 = OpLabel
%33 = OpFunctionCall %void %src_main
OpLine %3 8 1
OpReturn
OpFunctionEnd
OpLine %3 6 1
%src_main = OpFunction %void None %31
OpNoLine
%bb_entry = OpLabel
%47 = OpExtInst %void %1 DebugScope %27
%37 = OpExtInst %void %1 DebugFunctionDefinition %27 %src_main
%48 = OpExtInst %void %1 DebugScope %29
OpLine %3 7 3
%39 = OpFunctionCall %int %function1
%49 = OpExtInst %void %1 DebugScope %27
OpLine %3 8 1
OpReturn
%50 = OpExtInst %void %1 DebugNoScope
OpFunctionEnd
OpLine %3 2 1
%function1 = OpFunction %int None %42
OpNoLine
%bb_entry_0 = OpLabel
%51 = OpExtInst %void %1 DebugScope %21
%45 = OpExtInst %void %1 DebugFunctionDefinition %21 %function1
%52 = OpExtInst %void %1 DebugScope %23
OpLine %3 3 3
OpReturnValue %int_1
%53 = OpExtInst %void %1 DebugNoScope
OpFunctionEnd
  )";

  std::unique_ptr<IRContext> context =
      BuildModule(SPV_ENV_UNIVERSAL_1_1, nullptr, text,
                  SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);

  const uint32_t line_number = 7U;
  Instruction line(context.get(), spv::Op::OpLine);
  line.SetInOperands({
      {spv_operand_type_t::SPV_OPERAND_TYPE_ID, {5U}},
      {spv_operand_type_t::SPV_OPERAND_TYPE_LITERAL_INTEGER, {line_number}},
      {spv_operand_type_t::SPV_OPERAND_TYPE_LITERAL_INTEGER, {0U}},
  });

  DebugScope scope(29U, 0U);

  auto db_manager = context.get()->get_debug_info_mgr();
  auto du_manager = context.get()->get_def_use_mgr();
  auto c_manager = context.get()->get_constant_mgr();

  (void)du_manager;
  (void)c_manager;

  uint32_t inlined_at_id = db_manager->CreateDebugInlinedAt(&line, scope);
  auto* inlined_at = db_manager->GetDebugInlinedAt(inlined_at_id);
  EXPECT_NE(inlined_at, nullptr);
  EXPECT_EQ(inlined_at->GetSingleWordOperand(kDebugInlinedAtOperandLineIndex),
            100);
}

TEST(DebugInfoManager, GetDebugInfoNone) {
  const std::string text = R"(
               OpCapability Shader
          %1 = OpExtInstImport "OpenCL.DebugInfo.100"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %in_var_COLOR
               OpExecutionMode %main OriginUpperLeft
          %5 = OpString "ps.hlsl"
         %14 = OpString "#line 1 \"ps.hlsl\"
void main(float in_var_color : COLOR) {
  float color = in_var_color;
}
"
         %17 = OpString "float"
         %21 = OpString "main"
         %24 = OpString "color"
               OpName %in_var_COLOR "in.var.COLOR"
               OpName %main "main"
               OpDecorate %in_var_COLOR Location 0
       %uint = OpTypeInt 32 0
    %uint_32 = OpConstant %uint 32
      %float = OpTypeFloat 32
%_ptr_Input_float = OpTypePointer Input %float
       %void = OpTypeVoid
         %27 = OpTypeFunction %void
%_ptr_Function_float = OpTypePointer Function %float
%in_var_COLOR = OpVariable %_ptr_Input_float Input
         %13 = OpExtInst %void %1 DebugExpression
         %15 = OpExtInst %void %1 DebugSource %5 %14
         %16 = OpExtInst %void %1 DebugCompilationUnit 1 4 %15 HLSL
         %18 = OpExtInst %void %1 DebugTypeBasic %17 %uint_32 Float
         %20 = OpExtInst %void %1 DebugTypeFunction FlagIsProtected|FlagIsPrivate %18 %18
         %22 = OpExtInst %void %1 DebugFunction %21 %20 %15 1 1 %16 %21 FlagIsProtected|FlagIsPrivate 1 %main
         %12 = OpExtInst %void %1 DebugInfoNone
         %25 = OpExtInst %void %1 DebugLocalVariable %24 %18 %15 1 20 %22 FlagIsLocal 0
       %main = OpFunction %void None %27
         %28 = OpLabel
        %100 = OpVariable %_ptr_Function_float Function
         %31 = OpLoad %float %in_var_COLOR
               OpStore %100 %31
         %36 = OpExtInst %void %1 DebugDeclare %25 %100 %13
               OpReturn
               OpFunctionEnd
  )";

  std::unique_ptr<IRContext> context =
      BuildModule(SPV_ENV_UNIVERSAL_1_1, nullptr, text,
                  SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  DebugInfoManager manager(context.get());

  Instruction* debug_info_none_inst = manager.GetDebugInfoNone();
  EXPECT_NE(debug_info_none_inst, nullptr);
  EXPECT_EQ(debug_info_none_inst->GetOpenCL100DebugOpcode(),
            OpenCLDebugInfo100DebugInfoNone);
  EXPECT_EQ(debug_info_none_inst->PreviousNode(), nullptr);
}

TEST(DebugInfoManager, CreateDebugInfoNone) {
  const std::string text = R"(
               OpCapability Shader
          %1 = OpExtInstImport "OpenCL.DebugInfo.100"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %in_var_COLOR
               OpExecutionMode %main OriginUpperLeft
          %5 = OpString "ps.hlsl"
         %14 = OpString "#line 1 \"ps.hlsl\"
void main(float in_var_color : COLOR) {
  float color = in_var_color;
}
"
         %17 = OpString "float"
         %21 = OpString "main"
         %24 = OpString "color"
               OpName %in_var_COLOR "in.var.COLOR"
               OpName %main "main"
               OpDecorate %in_var_COLOR Location 0
       %uint = OpTypeInt 32 0
    %uint_32 = OpConstant %uint 32
      %float = OpTypeFloat 32
%_ptr_Input_float = OpTypePointer Input %float
       %void = OpTypeVoid
         %27 = OpTypeFunction %void
%_ptr_Function_float = OpTypePointer Function %float
%in_var_COLOR = OpVariable %_ptr_Input_float Input
         %13 = OpExtInst %void %1 DebugExpression
         %15 = OpExtInst %void %1 DebugSource %5 %14
         %16 = OpExtInst %void %1 DebugCompilationUnit 1 4 %15 HLSL
         %18 = OpExtInst %void %1 DebugTypeBasic %17 %uint_32 Float
         %20 = OpExtInst %void %1 DebugTypeFunction FlagIsProtected|FlagIsPrivate %18 %18
         %22 = OpExtInst %void %1 DebugFunction %21 %20 %15 1 1 %16 %21 FlagIsProtected|FlagIsPrivate 1 %main
         %25 = OpExtInst %void %1 DebugLocalVariable %24 %18 %15 1 20 %22 FlagIsLocal 0
       %main = OpFunction %void None %27
         %28 = OpLabel
        %100 = OpVariable %_ptr_Function_float Function
         %31 = OpLoad %float %in_var_COLOR
               OpStore %100 %31
         %36 = OpExtInst %void %1 DebugDeclare %25 %100 %13
               OpReturn
               OpFunctionEnd
  )";

  std::unique_ptr<IRContext> context =
      BuildModule(SPV_ENV_UNIVERSAL_1_1, nullptr, text,
                  SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  DebugInfoManager manager(context.get());

  Instruction* debug_info_none_inst = manager.GetDebugInfoNone();
  EXPECT_NE(debug_info_none_inst, nullptr);
  EXPECT_EQ(debug_info_none_inst->GetOpenCL100DebugOpcode(),
            OpenCLDebugInfo100DebugInfoNone);
  EXPECT_EQ(debug_info_none_inst->PreviousNode(), nullptr);
}

TEST(DebugInfoManager, GetDebugFunction) {
  const std::string text = R"(
               OpCapability Shader
          %1 = OpExtInstImport "OpenCL.DebugInfo.100"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %200 "200" %in_var_COLOR
               OpExecutionMode %200 OriginUpperLeft
          %5 = OpString "ps.hlsl"
         %14 = OpString "#line 1 \"ps.hlsl\"
void 200(float in_var_color : COLOR) {
  float color = in_var_color;
}
"
         %17 = OpString "float"
         %21 = OpString "200"
         %24 = OpString "color"
               OpName %in_var_COLOR "in.var.COLOR"
               OpName %200 "200"
               OpDecorate %in_var_COLOR Location 0
       %uint = OpTypeInt 32 0
    %uint_32 = OpConstant %uint 32
      %float = OpTypeFloat 32
%_ptr_Input_float = OpTypePointer Input %float
       %void = OpTypeVoid
         %27 = OpTypeFunction %void
%_ptr_Function_float = OpTypePointer Function %float
%in_var_COLOR = OpVariable %_ptr_Input_float Input
         %13 = OpExtInst %void %1 DebugExpression
         %15 = OpExtInst %void %1 DebugSource %5 %14
         %16 = OpExtInst %void %1 DebugCompilationUnit 1 4 %15 HLSL
         %18 = OpExtInst %void %1 DebugTypeBasic %17 %uint_32 Float
         %20 = OpExtInst %void %1 DebugTypeFunction FlagIsProtected|FlagIsPrivate %18 %18
         %22 = OpExtInst %void %1 DebugFunction %21 %20 %15 1 1 %16 %21 FlagIsProtected|FlagIsPrivate 1 %200
         %25 = OpExtInst %void %1 DebugLocalVariable %24 %18 %15 1 20 %22 FlagIsLocal 0
       %200 = OpFunction %void None %27
         %28 = OpLabel
        %100 = OpVariable %_ptr_Function_float Function
         %31 = OpLoad %float %in_var_COLOR
               OpStore %100 %31
         %36 = OpExtInst %void %1 DebugDeclare %25 %100 %13
               OpReturn
               OpFunctionEnd
  )";

  std::unique_ptr<IRContext> context =
      BuildModule(SPV_ENV_UNIVERSAL_1_1, nullptr, text,
                  SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  DebugInfoManager manager(context.get());

  EXPECT_EQ(manager.GetDebugFunction(100), nullptr);
  EXPECT_EQ(manager.GetDebugFunction(150), nullptr);

  Instruction* dbg_fn = manager.GetDebugFunction(200);

  EXPECT_EQ(dbg_fn->GetOpenCL100DebugOpcode(), OpenCLDebugInfo100DebugFunction);
  EXPECT_EQ(dbg_fn->GetSingleWordOperand(kDebugFunctionOperandFunctionIndex),
            200);
}

TEST(DebugInfoManager, GetDebugFunction_InlinedAway) {
  // struct PS_INPUT
  // {
  //   float4 iColor : COLOR;
  // };
  //
  // struct PS_OUTPUT
  // {
  //   float4 oColor : SV_Target0;
  // };
  //
  // float4 foo(float4 ic)
  // {
  //   float4 c = ic / 2.0;
  //   return c;
  // }
  //
  // PS_OUTPUT MainPs(PS_INPUT i)
  // {
  //   PS_OUTPUT ps_output;
  //   float4 ic = i.iColor;
  //   ps_output.oColor = foo(ic);
  //   return ps_output;
  // }
  const std::string text = R"(
               OpCapability Shader
          %1 = OpExtInstImport "OpenCL.DebugInfo.100"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %MainPs "MainPs" %in_var_COLOR %out_var_SV_Target0
               OpExecutionMode %MainPs OriginUpperLeft
         %15 = OpString "foo2.frag"
         %19 = OpString "PS_OUTPUT"
         %23 = OpString "float"
         %26 = OpString "oColor"
         %28 = OpString "PS_INPUT"
         %31 = OpString "iColor"
         %33 = OpString "foo"
         %37 = OpString "c"
         %39 = OpString "ic"
         %42 = OpString "src.MainPs"
         %47 = OpString "ps_output"
         %50 = OpString "i"
               OpName %in_var_COLOR "in.var.COLOR"
               OpName %out_var_SV_Target0 "out.var.SV_Target0"
               OpName %MainPs "MainPs"
               OpDecorate %in_var_COLOR Location 0
               OpDecorate %out_var_SV_Target0 Location 0
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
       %uint = OpTypeInt 32 0
    %uint_32 = OpConstant %uint 32
%_ptr_Input_v4float = OpTypePointer Input %v4float
%_ptr_Output_v4float = OpTypePointer Output %v4float
       %void = OpTypeVoid
   %uint_128 = OpConstant %uint 128
     %uint_0 = OpConstant %uint 0
         %52 = OpTypeFunction %void
%in_var_COLOR = OpVariable %_ptr_Input_v4float Input
%out_var_SV_Target0 = OpVariable %_ptr_Output_v4float Output
  %float_0_5 = OpConstant %float 0.5
        %130 = OpConstantComposite %v4float %float_0_5 %float_0_5 %float_0_5 %float_0_5
        %115 = OpExtInst %void %1 DebugInfoNone
         %49 = OpExtInst %void %1 DebugExpression
         %17 = OpExtInst %void %1 DebugSource %15
         %18 = OpExtInst %void %1 DebugCompilationUnit 1 4 %17 HLSL
         %21 = OpExtInst %void %1 DebugTypeComposite %19 Structure %17 6 1 %18 %19 %uint_128 FlagIsProtected|FlagIsPrivate %22
         %24 = OpExtInst %void %1 DebugTypeBasic %23 %uint_32 Float
         %25 = OpExtInst %void %1 DebugTypeVector %24 4
         %22 = OpExtInst %void %1 DebugTypeMember %26 %25 %17 8 5 %21 %uint_0 %uint_128 FlagIsProtected|FlagIsPrivate
         %29 = OpExtInst %void %1 DebugTypeComposite %28 Structure %17 1 1 %18 %28 %uint_128 FlagIsProtected|FlagIsPrivate %30
         %30 = OpExtInst %void %1 DebugTypeMember %31 %25 %17 3 5 %29 %uint_0 %uint_128 FlagIsProtected|FlagIsPrivate
         %32 = OpExtInst %void %1 DebugTypeFunction FlagIsProtected|FlagIsPrivate %25 %25
         %34 = OpExtInst %void %1 DebugFunction %33 %32 %17 11 1 %18 %33 FlagIsProtected|FlagIsPrivate 12 %115
         %36 = OpExtInst %void %1 DebugLexicalBlock %17 12 1 %34
         %38 = OpExtInst %void %1 DebugLocalVariable %37 %25 %17 13 12 %36 FlagIsLocal
         %41 = OpExtInst %void %1 DebugTypeFunction FlagIsProtected|FlagIsPrivate %21 %29
         %43 = OpExtInst %void %1 DebugFunction %42 %41 %17 17 1 %18 %42 FlagIsProtected|FlagIsPrivate 18 %115
         %45 = OpExtInst %void %1 DebugLexicalBlock %17 18 1 %43
         %46 = OpExtInst %void %1 DebugLocalVariable %39 %25 %17 20 12 %45 FlagIsLocal
         %48 = OpExtInst %void %1 DebugLocalVariable %47 %21 %17 19 15 %45 FlagIsLocal
        %107 = OpExtInst %void %1 DebugInlinedAt 21 %45
     %MainPs = OpFunction %void None %52
         %53 = OpLabel
         %57 = OpLoad %v4float %in_var_COLOR
        %131 = OpExtInst %void %1 DebugScope %45
               OpLine %15 20 12
        %117 = OpExtInst %void %1 DebugValue %46 %57 %49
        %132 = OpExtInst %void %1 DebugScope %36 %107
               OpLine %15 13 19
        %112 = OpFMul %v4float %57 %130
               OpLine %15 13 12
        %116 = OpExtInst %void %1 DebugValue %38 %112 %49
        %133 = OpExtInst %void %1 DebugScope %45
        %128 = OpExtInst %void %1 DebugValue %48 %112 %49 %int_0
        %134 = OpExtInst %void %1 DebugNoScope
               OpStore %out_var_SV_Target0 %112
               OpReturn
               OpFunctionEnd
  )";

  std::unique_ptr<IRContext> context =
      BuildModule(SPV_ENV_UNIVERSAL_1_1, nullptr, text,
                  SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  DebugInfoManager manager(context.get());

  EXPECT_EQ(manager.GetDebugFunction(115), nullptr);
}

TEST(DebugInfoManager, CloneDebugInlinedAt) {
  const std::string text = R"(
               OpCapability Shader
          %1 = OpExtInstImport "OpenCL.DebugInfo.100"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %in_var_COLOR
               OpExecutionMode %main OriginUpperLeft
          %5 = OpString "ps.hlsl"
         %14 = OpString "#line 1 \"ps.hlsl\"
void main(float in_var_color : COLOR) {
  float color = in_var_color;
}
"
         %17 = OpString "float"
         %21 = OpString "main"
         %24 = OpString "color"
               OpName %in_var_COLOR "in.var.COLOR"
               OpName %main "main"
               OpDecorate %in_var_COLOR Location 0
       %uint = OpTypeInt 32 0
    %uint_32 = OpConstant %uint 32
      %float = OpTypeFloat 32
%_ptr_Input_float = OpTypePointer Input %float
       %void = OpTypeVoid
         %27 = OpTypeFunction %void
%_ptr_Function_float = OpTypePointer Function %float
%in_var_COLOR = OpVariable %_ptr_Input_float Input
         %13 = OpExtInst %void %1 DebugExpression
         %15 = OpExtInst %void %1 DebugSource %5 %14
         %16 = OpExtInst %void %1 DebugCompilationUnit 1 4 %15 HLSL
         %18 = OpExtInst %void %1 DebugTypeBasic %17 %uint_32 Float
         %20 = OpExtInst %void %1 DebugTypeFunction FlagIsProtected|FlagIsPrivate %18 %18
         %22 = OpExtInst %void %1 DebugFunction %21 %20 %15 1 1 %16 %21 FlagIsProtected|FlagIsPrivate 1 %main
        %100 = OpExtInst %void %1 DebugInlinedAt 7 %22
       %main = OpFunction %void None %27
         %28 = OpLabel
         %31 = OpLoad %float %in_var_COLOR
               OpStore %100 %31
               OpReturn
               OpFunctionEnd
  )";

  std::unique_ptr<IRContext> context =
      BuildModule(SPV_ENV_UNIVERSAL_1_1, nullptr, text,
                  SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  DebugInfoManager manager(context.get());

  EXPECT_EQ(manager.CloneDebugInlinedAt(150), nullptr);
  EXPECT_EQ(manager.CloneDebugInlinedAt(22), nullptr);

  auto* inst = manager.CloneDebugInlinedAt(100);
  EXPECT_EQ(inst->GetSingleWordOperand(kDebugInlinedAtOperandLineIndex), 7);
  EXPECT_EQ(inst->GetSingleWordOperand(kDebugInlinedAtOperandScopeIndex), 22);
  EXPECT_EQ(inst->NumOperands(), kDebugInlinedAtOperandScopeIndex + 1);

  Instruction* before_100 = nullptr;
  for (auto it = context->module()->ext_inst_debuginfo_begin();
       it != context->module()->ext_inst_debuginfo_end(); ++it) {
    if (it->result_id() == 100) break;
    before_100 = &*it;
  }
  EXPECT_NE(inst, before_100);

  inst = manager.CloneDebugInlinedAt(100, manager.GetDebugInlinedAt(100));
  EXPECT_EQ(inst->GetSingleWordOperand(kDebugInlinedAtOperandLineIndex), 7);
  EXPECT_EQ(inst->GetSingleWordOperand(kDebugInlinedAtOperandScopeIndex), 22);
  EXPECT_EQ(inst->NumOperands(), kDebugInlinedAtOperandScopeIndex + 1);

  before_100 = nullptr;
  for (auto it = context->module()->ext_inst_debuginfo_begin();
       it != context->module()->ext_inst_debuginfo_end(); ++it) {
    if (it->result_id() == 100) break;
    before_100 = &*it;
  }
  EXPECT_EQ(inst, before_100);
}

TEST(DebugInfoManager, KillDebugDeclares) {
  const std::string text = R"(
               OpCapability Shader
          %1 = OpExtInstImport "OpenCL.DebugInfo.100"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %in_var_COLOR
               OpExecutionMode %main OriginUpperLeft
          %5 = OpString "ps.hlsl"
         %14 = OpString "#line 1 \"ps.hlsl\"
void main(float in_var_color : COLOR) {
  float color = in_var_color;
}
"
         %17 = OpString "float"
         %21 = OpString "main"
         %24 = OpString "color"
               OpName %in_var_COLOR "in.var.COLOR"
               OpName %main "main"
               OpDecorate %in_var_COLOR Location 0
       %uint = OpTypeInt 32 0
    %uint_32 = OpConstant %uint 32
      %float = OpTypeFloat 32
%_ptr_Input_float = OpTypePointer Input %float
       %void = OpTypeVoid
         %27 = OpTypeFunction %void
%_ptr_Function_float = OpTypePointer Function %float
%in_var_COLOR = OpVariable %_ptr_Input_float Input
         %13 = OpExtInst %void %1 DebugExpression
         %15 = OpExtInst %void %1 DebugSource %5 %14
         %16 = OpExtInst %void %1 DebugCompilationUnit 1 4 %15 HLSL
         %18 = OpExtInst %void %1 DebugTypeBasic %17 %uint_32 Float
         %20 = OpExtInst %void %1 DebugTypeFunction FlagIsProtected|FlagIsPrivate %18 %18
         %22 = OpExtInst %void %1 DebugFunction %21 %20 %15 1 1 %16 %21 FlagIsProtected|FlagIsPrivate 1 %main
         %12 = OpExtInst %void %1 DebugInfoNone
         %25 = OpExtInst %void %1 DebugLocalVariable %24 %18 %15 1 20 %22 FlagIsLocal 0
       %main = OpFunction %void None %27
         %28 = OpLabel
        %100 = OpVariable %_ptr_Function_float Function
         %31 = OpLoad %float %in_var_COLOR
               OpStore %100 %31
         %36 = OpExtInst %void %1 DebugDeclare %25 %100 %13
         %37 = OpExtInst %void %1 DebugDeclare %25 %100 %13
         %38 = OpExtInst %void %1 DebugDeclare %25 %100 %13
               OpReturn
               OpFunctionEnd
  )";

  std::unique_ptr<IRContext> context =
      BuildModule(SPV_ENV_UNIVERSAL_1_1, nullptr, text,
                  SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  auto* dbg_info_mgr = context->get_debug_info_mgr();
  auto* def_use_mgr = context->get_def_use_mgr();

  EXPECT_TRUE(dbg_info_mgr->IsVariableDebugDeclared(100));
  EXPECT_EQ(def_use_mgr->GetDef(36)->GetOpenCL100DebugOpcode(),
            OpenCLDebugInfo100DebugDeclare);
  EXPECT_EQ(def_use_mgr->GetDef(37)->GetOpenCL100DebugOpcode(),
            OpenCLDebugInfo100DebugDeclare);
  EXPECT_EQ(def_use_mgr->GetDef(38)->GetOpenCL100DebugOpcode(),
            OpenCLDebugInfo100DebugDeclare);

  dbg_info_mgr->KillDebugDeclares(100);
  EXPECT_EQ(def_use_mgr->GetDef(36), nullptr);
  EXPECT_EQ(def_use_mgr->GetDef(37), nullptr);
  EXPECT_EQ(def_use_mgr->GetDef(38), nullptr);
  EXPECT_FALSE(dbg_info_mgr->IsVariableDebugDeclared(100));
}

TEST(DebugInfoManager, AddDebugValueForDecl) {
  const std::string text = R"(
               OpCapability Shader
          %1 = OpExtInstImport "OpenCL.DebugInfo.100"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %in_var_COLOR
               OpExecutionMode %main OriginUpperLeft
          %5 = OpString "ps.hlsl"
         %14 = OpString "#line 1 \"ps.hlsl\"
void main(float in_var_color : COLOR) {
  float color = in_var_color;
}
"
         %17 = OpString "float"
         %21 = OpString "main"
         %24 = OpString "color"
               OpName %in_var_COLOR "in.var.COLOR"
               OpName %main "main"
               OpDecorate %in_var_COLOR Location 0
       %uint = OpTypeInt 32 0
    %uint_32 = OpConstant %uint 32
      %float = OpTypeFloat 32
%_ptr_Input_float = OpTypePointer Input %float
       %void = OpTypeVoid
         %27 = OpTypeFunction %void
%_ptr_Function_float = OpTypePointer Function %float
%in_var_COLOR = OpVariable %_ptr_Input_float Input
         %13 = OpExtInst %void %1 DebugExpression
         %15 = OpExtInst %void %1 DebugSource %5 %14
         %16 = OpExtInst %void %1 DebugCompilationUnit 1 4 %15 HLSL
         %18 = OpExtInst %void %1 DebugTypeBasic %17 %uint_32 Float
         %20 = OpExtInst %void %1 DebugTypeFunction FlagIsProtected|FlagIsPrivate %18 %18
         %22 = OpExtInst %void %1 DebugFunction %21 %20 %15 1 1 %16 %21 FlagIsProtected|FlagIsPrivate 1 %main
         %12 = OpExtInst %void %1 DebugInfoNone
         %25 = OpExtInst %void %1 DebugLocalVariable %24 %18 %15 1 20 %22 FlagIsLocal 0
       %main = OpFunction %void None %27
         %28 = OpLabel
        %100 = OpVariable %_ptr_Function_float Function
         %31 = OpLoad %float %in_var_COLOR
        %101 = OpExtInst %void %1 DebugScope %22
               OpLine %5 13 7
               OpStore %100 %31
         %36 = OpExtInst %void %1 DebugDeclare %25 %100 %13
               OpReturn
               OpFunctionEnd
  )";

  std::unique_ptr<IRContext> context =
      BuildModule(SPV_ENV_UNIVERSAL_1_1, nullptr, text,
                  SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  auto* def_use_mgr = context->get_def_use_mgr();
  auto* dbg_decl = def_use_mgr->GetDef(36);
  EXPECT_EQ(dbg_decl->GetOpenCL100DebugOpcode(),
            OpenCLDebugInfo100DebugDeclare);

  auto* dbg_info_mgr = context->get_debug_info_mgr();
  Instruction* store = dbg_decl->PreviousNode();
  auto* dbg_value =
      dbg_info_mgr->AddDebugValueForDecl(dbg_decl, 100, dbg_decl, store);

  EXPECT_EQ(dbg_value->GetOpenCL100DebugOpcode(), OpenCLDebugInfo100DebugValue);
  EXPECT_EQ(dbg_value->dbg_line_inst()->GetSingleWordInOperand(
                kOpLineInOperandFileIndex),
            5);
  EXPECT_EQ(dbg_value->dbg_line_inst()->GetSingleWordInOperand(
                kOpLineInOperandLineIndex),
            13);
  EXPECT_EQ(dbg_value->dbg_line_inst()->GetSingleWordInOperand(
                kOpLineInOperandColumnIndex),
            7);
}

TEST(DebugInfoManager, ConvertGlobalToLocal) {
  const std::string text = R"(
               OpCapability Shader
          %1 = OpExtInstImport "NonSemantic.Shader.DebugInfo.100"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %2 "PSMain" %3
               OpExecutionMode %2 OriginUpperLeft
          %4 = OpString "C:\\local\\Temp\\2528091a-6811-4e62-9ed5-02f1547c2016.hlsl"
          %5 = OpString "float"
          %6 = OpString "Pi"
      %float = OpTypeFloat 32
%float_3_1415 = OpConstant %float 3.1415
       %uint = OpTypeInt 32 0
    %uint_32 = OpConstant %uint 32
%_ptr_Private_float = OpTypePointer Private %float
%_ptr_Function_float = OpTypePointer Function %float
       %void = OpTypeVoid
     %uint_3 = OpConstant %uint 3
     %uint_0 = OpConstant %uint 0
     %uint_4 = OpConstant %uint 4
     %uint_1 = OpConstant %uint 1
     %uint_5 = OpConstant %uint 5
     %uint_8 = OpConstant %uint 8
     %uint_6 = OpConstant %uint 6
    %uint_20 = OpConstant %uint 20
         %25 = OpTypeFunction %void
    %uint_11 = OpConstant %uint 11
          %3 = OpVariable %_ptr_Private_float Private
          %8 = OpExtInst %void %1 DebugTypeBasic %5 %uint_32 %uint_3 %uint_0
         %12 = OpExtInst %void %1 DebugSource %4
         %13 = OpExtInst %void %1 DebugCompilationUnit %uint_1 %uint_4 %12 %uint_5
         %17 = OpExtInst %void %1 DebugGlobalVariable %6 %8 %12 %uint_6 %uint_20 %13 %6 %3 %uint_8
          %2 = OpFunction %void None %25
         %27 = OpLabel
         %29 = OpVariable %_ptr_Function_float Function
               OpStore %3 %float_3_1415
         %28 = OpExtInst %void %1 DebugLine %12 %uint_11 %uint_11 %uint_1 %uint_1
               OpReturn
               OpFunctionEnd
  )";

  std::unique_ptr<IRContext> context =
      BuildModule(SPV_ENV_UNIVERSAL_1_1, nullptr, text,
                  SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  auto* def_use_mgr = context->get_def_use_mgr();
  auto* dbg_var = def_use_mgr->GetDef(17);
  EXPECT_EQ(dbg_var->GetCommonDebugOpcode(),
            OpenCLDebugInfo100DebugGlobalVariable);
  EXPECT_EQ(dbg_var->NumInOperands(), 11);

  std::vector<Operand> originalOperands;
  for (uint32_t i = 0; i < dbg_var->NumInOperands(); ++i) {
    originalOperands.emplace_back(dbg_var->GetInOperand((i)));
  }

  auto* local_var = def_use_mgr->GetDef(29);
  auto* dbg_info_mgr = context->get_debug_info_mgr();
  dbg_info_mgr->ConvertDebugGlobalToLocalVariable(dbg_var, local_var);

  EXPECT_EQ(dbg_var->NumInOperands(), 9);

  // This checks that the first two inoperands are correct.
  EXPECT_EQ(dbg_var->GetCommonDebugOpcode(),
            OpenCLDebugInfo100DebugLocalVariable);

  // Then next 6 operands should be the same as the original instruction.
  EXPECT_EQ(dbg_var->GetInOperand(2), originalOperands[2]);
  EXPECT_EQ(dbg_var->GetInOperand(3), originalOperands[3]);
  EXPECT_EQ(dbg_var->GetInOperand(4), originalOperands[4]);
  EXPECT_EQ(dbg_var->GetInOperand(5), originalOperands[5]);
  EXPECT_EQ(dbg_var->GetInOperand(6), originalOperands[6]);
  EXPECT_EQ(dbg_var->GetInOperand(7), originalOperands[7]);

  // The flags operand should have shifted because operand 8 and 9 in the global
  // instruction are not relevant.
  EXPECT_EQ(dbg_var->GetInOperand(8), originalOperands[10]);
}

TEST(DebugInfoManagerTest, DebugInstructionWithForwardRefsKHR) {
  const std::string text = R"(
               OpCapability Shader
               OpExtension "SPV_KHR_non_semantic_info"
               OpExtension "SPV_KHR_relaxed_extended_instruction"
          %1 = OpExtInstImport "NonSemantic.Shader.DebugInfo.100"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main"
               OpExecutionMode %main OriginUpperLeft
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
     %uint_0 = OpConstant %uint 0
     %uint_3 = OpConstant %uint 3
         %10 = OpExtInstWithForwardRefsKHR %void %1 DebugTypeFunction %uint_3 %11
         %11 = OpExtInst %void %1 DebugInfoNone
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  std::unique_ptr<IRContext> context =
      BuildModule(SPV_ENV_UNIVERSAL_1_1, nullptr, text,
                  SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  ASSERT_NE(nullptr, context);

  Instruction* forward_ref_inst = context->get_def_use_mgr()->GetDef(10);
  ASSERT_NE(nullptr, forward_ref_inst);
  EXPECT_EQ(forward_ref_inst->opcode(), spv::Op::OpExtInstWithForwardRefsKHR);

  EXPECT_EQ(forward_ref_inst->GetShaderDebugOpcode(),
            NonSemanticShaderDebugInfoDebugTypeFunction);

  EXPECT_EQ(forward_ref_inst->GetCommonDebugOpcode(),
            CommonDebugInfoDebugTypeFunction);
}

// Finds the single OpString instruction whose decoded text is |text|.
Instruction* FindStringByText(IRContext* context, const std::string& text) {
  Instruction* result = nullptr;
  for (Instruction& inst : context->module()->debugs1()) {
    if (inst.opcode() != spv::Op::OpString) continue;
    if (inst.GetInOperand(0).AsString() != text) continue;
    EXPECT_EQ(nullptr, result) << "more than one OpString with text " << text;
    result = &inst;
  }
  return result;
}

// Serializes |context|, reparses the binary into a fresh context and checks
// it with the validator. Returns the reparsed context.
std::unique_ptr<IRContext> RoundTripAndValidate(
    IRContext* context, spv_target_env env = SPV_ENV_UNIVERSAL_1_3) {
  std::vector<uint32_t> binary;
  context->module()->ToBinary(&binary, /*skip_nop=*/false);

  spvtools::SpirvTools tools(env);
  tools.SetMessageConsumer([](spv_message_level_t, const char*,
                              const spv_position_t&, const char* m) {
    ADD_FAILURE() << m;
  });
  EXPECT_TRUE(tools.Validate(binary));

  auto reparsed = BuildModule(env, nullptr, binary.data(), binary.size());
  EXPECT_NE(nullptr, reparsed);
  return reparsed;
}

// Checks the raw binary encoding of the OpString instruction defining |id|:
// its word count must be 1 (wordcount/opcode) + 1 (result id) plus the exact
// number of words needed for |expected_text| plus its NUL terminator, and
// the instruction immediately following it must have opcode |next_opcode|.
void ExpectStringBinaryEncoding(const std::vector<uint32_t>& binary,
                                uint32_t id, const std::string& expected_text,
                                uint32_t next_opcode) {
  ASSERT_GE(binary.size(), 5u);
  size_t pos = 5;  // Skip the 5-word SPIR-V header.
  bool found = false;
  while (pos < binary.size()) {
    const uint32_t first_word = binary[pos];
    const uint32_t word_count = first_word >> 16;
    const uint32_t opcode = first_word & 0xFFFFu;
    ASSERT_GE(word_count, 1u);
    ASSERT_LE(pos + word_count, binary.size());
    if (opcode == static_cast<uint32_t>(spv::Op::OpString) &&
        word_count >= 2 && binary[pos + 1] == id) {
      const size_t string_word_count =
          (expected_text.size() + 1 + 3) / 4;  // bytes plus NUL, rounded up.
      EXPECT_EQ(word_count, 2u + string_word_count);
      std::string decoded;
      for (size_t w = 2; w < word_count; ++w) {
        uint32_t word = binary[pos + w];
        for (int b = 0; b < 4; ++b) {
          char c = static_cast<char>((word >> (8 * b)) & 0xFFu);
          if (c == '\0') break;
          decoded.push_back(c);
        }
      }
      EXPECT_EQ(decoded, expected_text);
      // The next encoded instruction must start exactly here and carry the
      // expected opcode, proving the changed instruction length did not
      // shift subsequent instructions.
      ASSERT_LT(pos + word_count, binary.size());
      EXPECT_EQ(binary[pos + word_count] & 0xFFFFu, next_opcode);
      found = true;
    }
    pos += word_count;
  }
  EXPECT_TRUE(found) << "OpString id " << id << " not found in binary";
  EXPECT_EQ(pos, binary.size()) << "instruction stream not consumed exactly";
}

const char* kLegacySourceText = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "PSMain"
               OpExecutionMode %main OriginUpperLeft
          %5 = OpString "a.hlsl"
               OpSource HLSL 600 %5
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
       %main = OpFunction %void None %3
          %6 = OpLabel
               OpLine %5 42 7
               OpReturn
               OpFunctionEnd
)";

TEST(DebugInfoManager, RenameDebugSourceNamesLegacyInPlace) {
  auto context = BuildModule(SPV_ENV_UNIVERSAL_1_3, nullptr, kLegacySourceText,
                             SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  ASSERT_NE(nullptr, context);
  DebugInfoManager manager(context.get());

  const std::string long_name =
      "/Volumes/work/build-agent-07/long-sandbox/materials/ps_main.slang";
  DebugSourceNameMap map = {{"a.hlsl", long_name}};
  EXPECT_TRUE(manager.RenameDebugSourceNames(map));

  // The same OpString id must now carry the long path (in-place update).
  Instruction* str = context->get_def_use_mgr()->GetDef(5);
  ASSERT_NE(nullptr, str);
  EXPECT_EQ(str->opcode(), spv::Op::OpString);
  EXPECT_EQ(str->GetInOperand(0).AsString(), long_name);

  // OpSource File operand still references id 5.
  Instruction* op_source = nullptr;
  for (Instruction& inst : context->module()->debugs1()) {
    if (inst.opcode() == spv::Op::OpSource) op_source = &inst;
  }
  ASSERT_NE(nullptr, op_source);
  EXPECT_EQ(op_source->GetSingleWordOperand(2), 5u);

  // OpLine keeps the same file id together with its line and column.
  bool found_line = false;
  context->module()->ForEachInst(
      [&](Instruction* inst) {
        if (inst->opcode() != spv::Op::OpLine) return;
        found_line = true;
        EXPECT_EQ(inst->GetSingleWordOperand(0), 5u);
        EXPECT_EQ(inst->GetSingleWordOperand(1), 42u);
        EXPECT_EQ(inst->GetSingleWordOperand(2), 7u);
      },
      /*run_on_debug_line_insts=*/true);
  EXPECT_TRUE(found_line);

  // The entry point name is an inline literal and must be unchanged.
  for (Instruction& ep : context->module()->entry_points()) {
    // Operands: execution model, function id, then the name literal.
    EXPECT_EQ(ep.GetInOperand(2).AsString(), std::string("PSMain"));
  }

  std::vector<uint32_t> binary;
  context->module()->ToBinary(&binary, false);
  ExpectStringBinaryEncoding(binary, 5, long_name,
                             static_cast<uint32_t>(spv::Op::OpSource));

  auto reparsed = RoundTripAndValidate(context.get());
  ASSERT_NE(nullptr, reparsed);
  Instruction* reparsed_str = reparsed->get_def_use_mgr()->GetDef(5);
  ASSERT_NE(nullptr, reparsed_str);
  EXPECT_EQ(reparsed_str->GetInOperand(0).AsString(), long_name);
}

// Names whose length crosses the 4-byte word boundary must re-encode to the
// exact computed number of words without disturbing the next instruction.
TEST(DebugInfoManager, RenameDebugSourceNamesWordBoundaries) {
  const std::vector<std::pair<std::string, std::string>> cases = {
      {"abc", "abcd"},    // 3 -> 4 chars: 1 -> 2 string words.
      {"abcd", "abc"},    // 4 -> 3 chars: 2 -> 1 string words.
      {"ab", "abcde"},    // 2 -> 5 chars.
      {"a", "abcdefgh"},  // 1 -> 8 chars (NUL exactly fills a word).
  };
  for (const auto& pair : cases) {
    const std::string& old_name = pair.first;
    const std::string& new_name = pair.second;
    std::string text = kLegacySourceText;
    text.replace(text.find("a.hlsl"), std::string("a.hlsl").size(), old_name);

    auto context = BuildModule(SPV_ENV_UNIVERSAL_1_3, nullptr, text,
                               SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
    ASSERT_NE(nullptr, context) << old_name;
    DebugInfoManager manager(context.get());
    DebugSourceNameMap map = {{old_name, new_name}};
    EXPECT_TRUE(manager.RenameDebugSourceNames(map));

    std::vector<uint32_t> binary;
    context->module()->ToBinary(&binary, false);
    ExpectStringBinaryEncoding(
        binary, 5, new_name, static_cast<uint32_t>(spv::Op::OpSource));
    EXPECT_NE(nullptr, RoundTripAndValidate(context.get()));
  }
}

TEST(DebugInfoManager, RenameDebugSourceNamesNoMatchIsNoChange) {
  auto context = BuildModule(SPV_ENV_UNIVERSAL_1_3, nullptr, kLegacySourceText,
                             SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  ASSERT_NE(nullptr, context);
  DebugInfoManager manager(context.get());
  DebugSourceNameMap map = {{"different/path.spv", "another/path.spv"}};
  EXPECT_FALSE(manager.RenameDebugSourceNames(map));
  Instruction* str = context->get_def_use_mgr()->GetDef(5);
  ASSERT_NE(nullptr, str);
  EXPECT_EQ(str->GetInOperand(0).AsString(), "a.hlsl");
}

const char* kSharedStringDebugInfoText = R"(
               OpCapability Shader
          %1 = OpExtInstImport "OpenCL.DebugInfo.100"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "PSMain" %in_var_COLOR
               OpExecutionMode %main OriginUpperLeft
          %5 = OpString "ps.hlsl"
         %14 = OpString "#line text"
         %21 = OpString "main"
               OpName %in_var_COLOR "in_var_COLOR"
               OpName %main "main"
               OpDecorate %in_var_COLOR Location 0
       %uint = OpTypeInt 32 0
    %uint_32 = OpConstant %uint 32
      %float = OpTypeFloat 32
%_ptr_Input_float = OpTypePointer Input %float
       %void = OpTypeVoid
         %27 = OpTypeFunction %void
%in_var_COLOR = OpVariable %_ptr_Input_float Input
         %13 = OpExtInst %void %1 DebugExpression
         %15 = OpExtInst %void %1 DebugSource %5 %14
         %16 = OpExtInst %void %1 DebugCompilationUnit 1 4 %15 HLSL
         %18 = OpExtInst %void %1 DebugTypeBasic %5 %uint_32 Float
         %20 = OpExtInst %void %1 DebugTypeFunction FlagIsProtected|FlagIsPrivate %18 %18
         %22 = OpExtInst %void %1 DebugFunction %21 %20 %15 1 1 %16 %21 FlagIsProtected|FlagIsPrivate 1 %main
        %100 = OpExtInst %void %1 DebugInlinedAt 7 %22
       %main = OpFunction %void None %27
         %28 = OpLabel
               OpLine %5 123 4
         %31 = OpLoad %float %in_var_COLOR
               OpReturn
               OpFunctionEnd
)";

TEST(DebugInfoManager, RenameDebugSourceNamesSplitsSharedString) {
  auto context =
      BuildModule(SPV_ENV_UNIVERSAL_1_3, nullptr, kSharedStringDebugInfoText,
                  SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  ASSERT_NE(nullptr, context);
  DebugInfoManager manager(context.get());

  const std::string long_name =
      "/Volumes/work/build-agent-07/deep/sandbox/materials/ps_generated.hlsl";
  EXPECT_TRUE(manager.RenameDebugSourceNames(
      DebugSourceNameMap{{"ps.hlsl", long_name}}));

  // The original OpString survives unchanged for the non-path use.
  Instruction* old_str = context->get_def_use_mgr()->GetDef(5);
  ASSERT_NE(nullptr, old_str);
  EXPECT_EQ(old_str->GetInOperand(0).AsString(), "ps.hlsl");

  // A new OpString carries the long path.
  Instruction* new_str = FindStringByText(context.get(), long_name);
  ASSERT_NE(nullptr, new_str);
  const uint32_t new_id = new_str->result_id();
  EXPECT_NE(new_id, 5u);

  // DebugSource File (absolute operand 4) uses the new id.
  Instruction* debug_source = context->get_def_use_mgr()->GetDef(15);
  ASSERT_NE(nullptr, debug_source);
  EXPECT_EQ(debug_source->GetSingleWordOperand(4), new_id);
  // Its Text operand is untouched.
  EXPECT_EQ(debug_source->GetSingleWordOperand(5), 14u);

  // The non-path use (DebugTypeBasic Name, also absolute operand 4) keeps 5.
  Instruction* debug_basic = context->get_def_use_mgr()->GetDef(18);
  ASSERT_NE(nullptr, debug_basic);
  EXPECT_EQ(debug_basic->GetSingleWordOperand(4), 5u);

  // OpLine File moved to the new id; line/column are preserved.
  bool found_line = false;
  context->module()->ForEachInst(
      [&](Instruction* inst) {
        if (inst->opcode() != spv::Op::OpLine) return;
        found_line = true;
        EXPECT_EQ(inst->GetSingleWordOperand(0), new_id);
        EXPECT_EQ(inst->GetSingleWordOperand(1), 123u);
        EXPECT_EQ(inst->GetSingleWordOperand(2), 4u);
      },
      true);
  EXPECT_TRUE(found_line);

  // The inlining chain is untouched: DebugInlinedAt still uses line 7 and
  // scope 22.
  Instruction* inlined_at = context->get_def_use_mgr()->GetDef(100);
  ASSERT_NE(nullptr, inlined_at);
  EXPECT_EQ(inlined_at->GetSingleWordOperand(4), 7u);
  EXPECT_EQ(inlined_at->GetSingleWordOperand(5), 22u);

  // Entry point name and OpName strings keep their original meaning.
  for (Instruction& ep : context->module()->entry_points()) {
    EXPECT_EQ(ep.GetInOperand(2).AsString(), std::string("PSMain"));
  }
  EXPECT_NE(FindStringByText(context.get(), "main"), nullptr);

  // DefUse must still be consistent after retargeting operands.
  EXPECT_TRUE(context->IsConsistent());

  auto reparsed = RoundTripAndValidate(context.get());
  ASSERT_NE(nullptr, reparsed);
  // After reparse both strings exist and the DebugSource still references
  // the long one.
  EXPECT_NE(FindStringByText(reparsed.get(), "ps.hlsl"), nullptr);
  EXPECT_NE(FindStringByText(reparsed.get(), long_name), nullptr);
}

const char* kNonPathOnlyStringText = R"(
               OpCapability Shader
          %1 = OpExtInstImport "OpenCL.DebugInfo.100"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "PSMain"
               OpExecutionMode %main OriginUpperLeft
          %5 = OpString "type_name_that_looks_like_a_path.hlsl"
          %7 = OpString "actual_source.hlsl"
       %uint = OpTypeInt 32 0
    %uint_32 = OpConstant %uint 32
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
         %15 = OpExtInst %void %1 DebugSource %7
         %18 = OpExtInst %void %1 DebugTypeBasic %5 %uint_32 Float
       %main = OpFunction %void None %3
          %6 = OpLabel
               OpReturn
               OpFunctionEnd
)";

TEST(DebugInfoManager, RenameDebugSourceNamesLeavesNonPathOnlyString) {
  auto context =
      BuildModule(SPV_ENV_UNIVERSAL_1_3, nullptr, kNonPathOnlyStringText,
                  SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  ASSERT_NE(nullptr, context);
  DebugInfoManager manager(context.get());

  // The key matches the OpString used only as a DebugTypeBasic Name, never as
  // a source file. It must not be renamed or split.
  EXPECT_FALSE(manager.RenameDebugSourceNames(DebugSourceNameMap{
      {"type_name_that_looks_like_a_path.hlsl", "/new/name.hlsl"}}));

  Instruction* name_str = context->get_def_use_mgr()->GetDef(5);
  ASSERT_NE(nullptr, name_str);
  EXPECT_EQ(name_str->GetInOperand(0).AsString(),
            "type_name_that_looks_like_a_path.hlsl");
  EXPECT_EQ(FindStringByText(context.get(), "/new/name.hlsl"), nullptr);
  EXPECT_NE(context->get_def_use_mgr()->GetDef(7), nullptr);
}

TEST(DebugInfoManager, RenameDebugSourcePassThroughOptimizer) {
  spvtools::SpirvTools tools(SPV_ENV_UNIVERSAL_1_3);
  std::vector<uint32_t> binary;
  ASSERT_TRUE(tools.Assemble(kLegacySourceText, &binary,
                             SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS));

  spvtools::Optimizer optimizer(SPV_ENV_UNIVERSAL_1_3);
  optimizer.RegisterPass(spvtools::CreateRenameDebugSourcePass(
      {{"a.hlsl", "/tmp/very/long/relocated/path/material.hlsl"}}));
  optimizer.SetMessageConsumer([](spv_message_level_t, const char*,
                                   const spv_position_t&, const char* m) {
    ADD_FAILURE() << m;
  });

  std::vector<uint32_t> optimized;
  ASSERT_TRUE(optimizer.Run(binary.data(), binary.size(), &optimized));
  EXPECT_TRUE(tools.Validate(optimized));

  auto context = BuildModule(SPV_ENV_UNIVERSAL_1_3, nullptr, optimized.data(),
                             optimized.size());
  ASSERT_NE(nullptr, context);
  EXPECT_NE(FindStringByText(context.get(),
                             "/tmp/very/long/relocated/path/material.hlsl"),
            nullptr);
  EXPECT_EQ(FindStringByText(context.get(), "a.hlsl"), nullptr);
}

}  // namespace
}  // namespace analysis
}  // namespace opt
}  // namespace spvtools
