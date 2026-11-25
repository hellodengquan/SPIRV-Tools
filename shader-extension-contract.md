# 着色器扩展契约：从扩展进入核心版本的指令组在验证器中的接受边界

面向移植着色器的同事。本文以 SPIRV-Tools 验证器（`source/val/`）为准，分析一组"先以 KHR 扩展发布、后并入 SPIR-V 核心版本"的着色器能力，推导同一小段程序在两个有明确版本的目标环境中，模块必须声明什么才能被接受，并说明离线验证通过与运行时设备条件之间的责任分界。

文中所有实验均用本仓库构建的 `spirv-as` / `spirv-val` 实测复现。

## 1. 选取的指令组：SPV_KHR_16bit_storage 能力组

该扩展引入四个能力（capability），在 **SPIR-V 1.3** 并入核心；对照组 SPV_KHR_8bit_storage 在 **SPIR-V 1.5** 并入核心。语法表
`external/spirv-headers/include/spirv/unified1/spirv.core.grammar.json` 中每个能力都同时带有 `version`（进入核心的版本）和 `extensions`（此前启用它的扩展）两个字段：

| 能力 | 值 | 核心化版本 | 扩展 |
|---|---|---|---|
| `StorageBuffer16BitAccess`（别名 `StorageUniformBufferBlock16`） | 4433 | 1.3 | SPV_KHR_16bit_storage |
| `UniformAndStorageBuffer16BitAccess`（别名 `StorageUniform16`，依赖 `StorageBuffer16BitAccess`） | 4434 | 1.3 | SPV_KHR_16bit_storage |
| `StoragePushConstant16` | 4435 | 1.3 | SPV_KHR_16bit_storage |
| `StorageInputOutput16` | 4436 | 1.3 | SPV_KHR_16bit_storage |
| `StorageBuffer8BitAccess` / `UniformAndStorageBuffer8BitAccess` / `StoragePushConstant8` | 4448–4450 | 1.5 | SPV_KHR_8bit_storage |

这组能力本身不新增 opcode，而是解锁"在特定存储类里存放 8/16 位类型"这一用法——正好适合展示**操作数类型和存储用途如何逐级收紧接受边界**。

## 2. 验证器的四道闸门（代码路径）

一个模块要合法使用 16 位存储，需依次通过四道相互独立的检查：

1. **模块头版本门** — `source/val/validate.cpp:190`：模块头的 SPIR-V 版本不得超过目标环境允许的最高版本（`spvVersionForTargetEnv`，映射表见 `source/spirv_target_env.cpp:268` 附近，如 Vulkan 1.0 → SPIR-V 1.0，Vulkan 1.1 → SPIR-V 1.3，Vulkan 1.2 → SPIR-V 1.5）。超标直接报
   `Invalid SPIR-V binary version 1.3 for target environment SPIR-V 1.0 (under Vulkan 1.0 semantics)`。
2. **能力白名单门** — `source/val/validate_capability.cpp` 的 `CapabilityPass`：每个目标环境有 `IsSupportGuaranteedVulkan_X_Y` / `IsSupportOptionalVulkan_X_Y` 两张静态表；能力既不在表内、又不能由模块已声明的扩展启用（`IsEnabledByExtension`，按语法表的 `extensions` 字段匹配 `OpExtension`）则报错 `Capability ... is not allowed by Vulkan X.Y specification (or requires extension)`。
3. **操作数版本/扩展门** — `source/val/validate_instruction.cpp:87` 的 `OperandVersionExtensionCheck`：操作数（含能力枚举值本身）带 `minVersion`；模块版本已达标则放行，否则要求模块声明语法表所列扩展之一，缺则报 `requires one of these extensions: SPV_KHR_16bit_storage`。
4. **类型与存储类门** — `source/val/validate_type.cpp:56`（`ValidateTypeInt`/Float：16 位类型要求 `Int16`/`Float16` 或存储类能力置位的 `declare_int16_type`/`declare_float16_type` 特征位，见 `source/val/validation_state.cpp:433-467`）和 `source/val/validate_memory.cpp:1057` 的 `ValidateVariableShader`（按变量的存储类逐个要求对应能力）。

注意第 2、3 道是"或"关系之外的**双重检查**：在 Vulkan 1.0 下，`OpCapability StorageBuffer16BitAccess` 既要过白名单（靠 `OpExtension`），该能力操作数本身也要过版本/扩展门（同样靠 `OpExtension`）。一条 `OpExtension "SPV_KHR_16bit_storage"` 同时满足两者。

## 3. 同一小段程序在两个目标环境下的接受边界

测试程序（compute shader，SSBO 里放一个 `float16` 成员；SPIR-V 1.0 没有 `StorageBuffer` 存储类，故用 `Uniform` + `BufferBlock`，这在两个版本下都合法）：

```
OpCapability Shader
OpCapability StorageBuffer16BitAccess
;; 变体开关：OpExtension "SPV_KHR_16bit_storage"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %buf DescriptorSet 0
OpDecorate %buf Binding 0
OpDecorate %S BufferBlock
OpMemberDecorate %S 0 Offset 0
%f16  = OpTypeFloat 16
%S    = OpTypeStruct %f16
%ptr  = OpTypePointer Uniform %S
%buf  = OpVariable %ptr Uniform
...
```

两个目标环境：**`vulkan1.0`**（模块头须 ≤ SPIR-V 1.0）与 **`vulkan1.1`**（模块头须 ≤ SPIR-V 1.3）。实测结果：

| # | 模块声明 | 目标环境 | 结果 | 命中的闸门 |
|---|---|---|---|---|
| A | 两个能力，**无** `OpExtension` | vulkan1.0 | ❌ `Capability StorageBuffer16BitAccess is not allowed by Vulkan 1.0 specification (or requires extension)` | 闸门 2 |
| B | A + `OpExtension "SPV_KHR_16bit_storage"` | vulkan1.0 | ✅ | — |
| C | 同 A（无 `OpExtension`），1.3 模块头 | vulkan1.1 | ✅ | — |
| D | C 去掉 `StorageBuffer16BitAccess` | vulkan1.1 | ❌ `Using a 16-bit floating point type requires the Float16 or Float16Buffer capability...` | 闸门 4（类型级） |
| E | 拿 C 的 1.3 二进制 | vulkan1.0 | ❌ `Invalid SPIR-V binary version 1.3 ...` | 闸门 1 |
| I | C + 保留冗余的 `OpExtension` | vulkan1.1 | ✅ | — |

**接受边界推导**：

- 在 **vulkan1.0**（SPIR-V 1.0）：`StorageBuffer16BitAccess` 不在 `IsSupportGuaranteedVulkan_1_0`/`IsSupportOptionalVulkan_1_0` 中（`validate_capability.cpp:30-46, 115-161`），且语法表 `version: "1.3"` > 模块版本 1.0，因此**必须**声明 `OpExtension "SPV_KHR_16bit_storage"`，由 `IsEnabledByExtension`（`validate_capability.cpp:298`）和操作数扩展门同时放行。
- 在 **vulkan1.1**（SPIR-V 1.3）：该能力经别名 `StorageUniformBufferBlock16`/`StorageUniform16` 进入 `IsSupportOptionalVulkan_1_1`（`validate_capability.cpp:176-181`），且语法表 `version: "1.3"` ≤ 模块版本 1.3，`OperandVersionExtensionCheck` 的 `version_satisfied` 直接成立——**不需要任何 `OpExtension`**。
- 反向不成立：1.3 模块头无法回到 vulkan1.0 环境（实验 E，闸门 1 先于一切能力检查）。
- 对照组 8 位存储：核心化版本是 1.5，但 `OpExtension "SPV_KHR_8bit_storage"` + `StorageBuffer8BitAccess` 在 **vulkan1.0** 下实测同样通过——扩展路径不受核心化版本约束，只要求"声明了对应扩展"。

**移植建议**：`OpExtension` 声明已核心化的扩展不会被拒绝（`ValidateExtension`，`validate_extensions.cpp:1368`，只对少数扩展设最低版本），实验 I 证实 1.3 模块保留该声明无害。因此同一份着色器源**保留 `OpExtension "SPV_KHR_16bit_storage"` 并分别按目标环境的最高版本汇编**，即可在 vulkan1.0 与 vulkan1.1 下都通过验证；想去掉声明则只能在 SPIR-V ≥ 1.3 的模块里去掉。

## 4. 操作数类型与存储用途如何收紧边界

声明了"能进核心"的能力只是拿到了入场券；具体用到哪种类型、放进哪个存储类，边界会进一步收紧：

**(a) 类型级**：`OpTypeFloat 16` / `OpTypeInt 16` 本身要求 `Float16`/`Int16` 能力，或任一 16 位存储类能力（它们会置位 `declare_float16_type`/`declare_int16_type` 特征位，`validation_state.cpp:459-466`）。实验 D：只删能力、保留类型，立即在类型定义处报错。

**(b) 存储类级**：`ValidateVariableShader`（`validate_memory.cpp:1057-1126`）对**不含**完整 `Int16`/`Float16` 能力的模块，按变量存储类逐个要求对应能力：

| 存储类 | 需要的能力 |
|---|---|
| `StorageBuffer` / `PhysicalStorageBuffer` | `StorageBuffer16BitAccess` |
| `Uniform` | `UniformAndStorageBuffer16BitAccess`；或 `StorageBuffer16BitAccess` **且** 结构体带 `BufferBlock` 装饰（`validate_memory.cpp:1093-1095`） |
| `PushConstant` | `StoragePushConstant16` |
| `Input` / `Output` | `StorageInputOutput16` |
| `Workgroup` | `WorkgroupMemoryExplicitLayout16BitAccessKHR`（另一扩展 SPV_KHR_workgroup_memory_explicit_layout） |
| 其余（`Function`、`Private`…） | 直接报错 `Cannot allocate a variable containing a 16-bit type in ... storage class` |

实测（vulkan1.1 目标）：

| # | 声明 | 变量位置 | 结果 |
|---|---|---|---|
| F | `StorageBuffer16BitAccess` | PushConstant | ❌ `Allocating a variable containing a 16-bit element in PushConstant storage class requires an additional capability` |
| G | `Float16`（完整类型能力） | PushConstant | ✅ |
| G2 | `StoragePushConstant16` | PushConstant | ✅ |

即**能力不可跨存储类通用**：`StorageBuffer16BitAccess` 管不了 PushConstant（实验 F）。合法化 16 位数据有两条路线——声明完整的 `Float16`/`Int16` 类型能力（此时 `ValidateVariableShader` 的存储类检查整块被跳过，实验 G），或按实际用到的每个存储类声明对应的受限能力（实验 G2）。前者在设备端对应更重的特性要求（见第 5 节）。

**(c) 访问粒度级**：即使变量合法，`OpCopyMemorySized` 拷贝非 4 字节对齐的常量 Size 时，`validate_memory.cpp:1979-2014` 还会按源/目标存储类分别核对 8/16 位能力组合（如 `ssbo_int16 = StorageBuffer16BitAccess || ubo_int16 || ssbo_int8`，能力之间有蕴含链）。存储用途（源是 UBO 还是 SSBO）不同，接受边界就不同。

## 5. 验证器的保证 vs 渲染设备的特性责任

**验证器保证的是"模块对目标环境规范契约的静态合规"，不是"设备能跑"。** `IsSupportOptionalVulkan_X_Y` 这类表编码的是"该 API 版本**允许**模块声明此能力"，验证器没有任何设备 profile 输入——`spvValidate` 的入参只有目标环境和选项，不知道 `VkPhysicalDeviceFeatures` 里开了什么。

具体实例（本文实验 B）：模块声明 `OpExtension "SPV_KHR_16bit_storage"` + `OpCapability StorageBuffer16BitAccess`，`spirv-val --target-env vulkan1.0` **通过**。但同一个二进制交给 Vulkan 1.0 设备时，运行时还要求：

1. 创建 `VkDevice` 时启用了 `VK_KHR_16bit_storage` 设备扩展；
2. 通过 `VkPhysicalDevice16BitStorageFeatures` 启用了 `storageBuffer16BitAccess` 特性（且该特性在物理设备上确实受支持）。

任一缺失，`vkCreateShaderModule`/管线创建阶段会被 validation layer 以 VUID 拦下或直接失败——**离线验证全绿，运行时缺条件**。同理：

- 实验 G 用 `Float16` 能力过验证，对应设备特性是 `shaderFloat16`（VK_KHR_shader_float16_int8），与 `storageBuffer16BitAccess` 是两个独立 feature bit；选了第 4 节 (b) 中"完整类型能力"路线，就把设备端门槛从存储特性抬高到了完整 16 位浮点特性。
- 8 位存储组（实验 H）在 vulkan1.0 过验证，设备端需 `VK_KHR_8bit_storage` 扩展 + `storageBuffer8BitAccess` 特性。
- 反方向的版本演进也写在这张表里：Vulkan 1.4 把 `shaderInt8`/`shaderInt16`/`storageBuffer16BitAccess` 等列为强制特性，因此 `IsSupportGuaranteedVulkan_1_4`（`validate_capability.cpp:89-113`）把 `Int8`、`Int16`、`StorageBuffer16BitAccess` 等从"可选"挪进"保证"——"guaranteed"的含义是**该 API 版本的核心规范保证设备必有此特性**，验证器只是把这层规范承诺静态化了。1.0–1.3 下这些能力永远停在 optional 表，设备责任始终由应用侧（枚举物理设备特性、按特性挑选/编译着色器变体）承担。

**给移植流程的结论**：`spirv-val` 通过只说明模块声明（版本头 + `OpCapability` + 必要的 `OpExtension`）与目标环境的规范契约自洽；上线前还必须用 `vkGetPhysicalDeviceFeatures2` 等核对设备特性位，并为"特性缺失"准备降级变体（如把 16 位存储展开为 32 位加载+位运算的版本）。验证器与运行时的责任分界，就是"模块声明了什么"与"设备启用了什么"的分界。
