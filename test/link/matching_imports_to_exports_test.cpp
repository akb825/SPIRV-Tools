// Copyright (c) 2017 Pierre Moreau
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

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/link/linker_fixture.h"

namespace spvtools {
namespace {

using ::testing::HasSubstr;
using MatchingImportsToExports = spvtest::LinkerTest;

TEST_F(MatchingImportsToExports, Default) {
  const std::string body1 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Import
%2 = OpTypeFloat 32
%1 = OpVariable %2 Uniform
%3 = OpVariable %2 Input
)";
  const std::string body2 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Export
%2 = OpTypeFloat 32
%3 = OpConstant %2 42
%1 = OpVariable %2 Uniform %3
)";

  spvtest::Binary linked_binary;
  ASSERT_EQ(SPV_SUCCESS, AssembleAndLink({body1, body2}, &linked_binary))
      << GetErrorMessage();

  const std::string expected_res =
      R"(OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpModuleProcessed "Linked by SPIR-V Tools Linker"
%1 = OpTypeFloat 32
%2 = OpVariable %1 Input
%3 = OpConstant %1 42
%4 = OpVariable %1 Uniform %3
)";
  std::string res_body;
  SetDisassembleOptions(SPV_BINARY_TO_TEXT_OPTION_NO_HEADER);
  ASSERT_EQ(SPV_SUCCESS, Disassemble(linked_binary, &res_body))
      << GetErrorMessage();
  EXPECT_EQ(expected_res, res_body);
}

TEST_F(MatchingImportsToExports, NotALibraryExtraExports) {
  const std::string body = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Export
%2 = OpTypeFloat 32
%1 = OpVariable %2 Uniform
)";

  spvtest::Binary linked_binary;
  ASSERT_EQ(SPV_SUCCESS, AssembleAndLink({body}, &linked_binary))
      << GetErrorMessage();

  const std::string expected_res =
      R"(OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpModuleProcessed "Linked by SPIR-V Tools Linker"
%1 = OpTypeFloat 32
%2 = OpVariable %1 Uniform
)";
  std::string res_body;
  SetDisassembleOptions(SPV_BINARY_TO_TEXT_OPTION_NO_HEADER);
  ASSERT_EQ(SPV_SUCCESS, Disassemble(linked_binary, &res_body))
      << GetErrorMessage();
  EXPECT_EQ(expected_res, res_body);
}

TEST_F(MatchingImportsToExports, LibraryExtraExports) {
  const std::string body = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Export
%2 = OpTypeFloat 32
%1 = OpVariable %2 Uniform
)";

  spvtest::Binary linked_binary;
  LinkerOptions options;
  options.SetCreateLibrary(true);
  ASSERT_EQ(SPV_SUCCESS, AssembleAndLink({body}, &linked_binary, options))
      << GetErrorMessage();

  const std::string expected_res = R"(OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpModuleProcessed "Linked by SPIR-V Tools Linker"
OpDecorate %1 LinkageAttributes "foo" Export
%2 = OpTypeFloat 32
%1 = OpVariable %2 Uniform
)";
  std::string res_body;
  SetDisassembleOptions(SPV_BINARY_TO_TEXT_OPTION_NO_HEADER);
  ASSERT_EQ(SPV_SUCCESS, Disassemble(linked_binary, &res_body))
      << GetErrorMessage();
  EXPECT_EQ(expected_res, res_body);
}

TEST_F(MatchingImportsToExports, UnresolvedImports) {
  const std::string body1 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Import
%2 = OpTypeFloat 32
%1 = OpVariable %2 Uniform
)";
  const std::string body2 = R"(
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
)";

  spvtest::Binary linked_binary;
  EXPECT_EQ(SPV_ERROR_INVALID_BINARY,
            AssembleAndLink({body1, body2}, &linked_binary));
  EXPECT_THAT(GetErrorMessage(),
              HasSubstr("Unresolved external reference to \"foo\"."));
}

TEST_F(MatchingImportsToExports, TypeMismatch) {
  const std::string body1 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Import
%2 = OpTypeFloat 32
%1 = OpVariable %2 Uniform
%3 = OpVariable %2 Input
)";
  const std::string body2 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Export
%2 = OpTypeInt 32 0
%3 = OpConstant %2 42
%1 = OpVariable %2 Uniform %3
)";

  LinkerOptions options;
  for (int i = 0; i < 2; i++) {
    spvtest::Binary linked_binary;
    options.SetAllowPtrTypeMismatch(i == 1);
    EXPECT_EQ(SPV_ERROR_INVALID_BINARY,
              AssembleAndLink({body1, body2}, &linked_binary))
        << GetErrorMessage();
    EXPECT_THAT(
        GetErrorMessage(),
        HasSubstr("Type mismatch on symbol \"foo\" between imported "
                  "variable/function %1 and exported variable/function %4"));
  }
}

TEST_F(MatchingImportsToExports, MultipleDefinitions) {
  const std::string body1 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Import
%2 = OpTypeFloat 32
%1 = OpVariable %2 Uniform
%3 = OpVariable %2 Input
)";
  const std::string body2 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Export
%2 = OpTypeFloat 32
%3 = OpConstant %2 42
%1 = OpVariable %2 Uniform %3
)";
  const std::string body3 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Export
%2 = OpTypeFloat 32
%3 = OpConstant %2 -1
%1 = OpVariable %2 Uniform %3
)";

  LinkerOptions options;
  for (int i = 0; i < 2; i++) {
    spvtest::Binary linked_binary;
    options.SetAllowPtrTypeMismatch(i == 1);
    EXPECT_EQ(SPV_ERROR_INVALID_BINARY,
              AssembleAndLink({body1, body2, body3}, &linked_binary))
        << GetErrorMessage();
    EXPECT_THAT(GetErrorMessage(),
                HasSubstr("Too many external references, 2, were found "
                          "for \"foo\"."));
  }
}

TEST_F(MatchingImportsToExports, SameNameDifferentTypes) {
  const std::string body1 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Import
%2 = OpTypeFloat 32
%1 = OpVariable %2 Uniform
%3 = OpVariable %2 Input
)";
  const std::string body2 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Export
%2 = OpTypeInt 32 0
%3 = OpConstant %2 42
%1 = OpVariable %2 Uniform %3
)";
  const std::string body3 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Export
%2 = OpTypeFloat 32
%3 = OpConstant %2 12
%1 = OpVariable %2 Uniform %3
)";

  spvtest::Binary linked_binary;
  EXPECT_EQ(SPV_ERROR_INVALID_BINARY,
            AssembleAndLink({body1, body2, body3}, &linked_binary))
      << GetErrorMessage();
  EXPECT_THAT(GetErrorMessage(),
              HasSubstr("Too many external references, 2, were found "
                        "for \"foo\"."));
}

TEST_F(MatchingImportsToExports, DecorationMismatch) {
  const std::string body1 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Import
OpDecorate %2 Constant
%2 = OpTypeFloat 32
%1 = OpVariable %2 Uniform
%3 = OpVariable %2 Input
)";
  const std::string body2 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Export
%2 = OpTypeFloat 32
%3 = OpConstant %2 42
%1 = OpVariable %2 Uniform %3
)";

  LinkerOptions options;
  for (int i = 0; i < 2; i++) {
    spvtest::Binary linked_binary;
    options.SetAllowPtrTypeMismatch(i == 1);
    EXPECT_EQ(SPV_ERROR_INVALID_BINARY,
              AssembleAndLink({body1, body2}, &linked_binary))
        << GetErrorMessage();
    EXPECT_THAT(
        GetErrorMessage(),
        HasSubstr("Type mismatch on symbol \"foo\" between imported "
                  "variable/function %1 and exported variable/function %4"));
  }
}

TEST_F(MatchingImportsToExports,
       FuncParamAttrDifferButStillMatchExportToImport) {
  const std::string body1 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Import
OpDecorate %2 FuncParamAttr Zext
%3 = OpTypeVoid
%4 = OpTypeInt 32 0
%5 = OpTypeFunction %3 %4
%1 = OpFunction %3 None %5
%2 = OpFunctionParameter %4
OpFunctionEnd
)";
  const std::string body2 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Export
OpDecorate %2 FuncParamAttr Sext
%3 = OpTypeVoid
%4 = OpTypeInt 32 0
%5 = OpTypeFunction %3 %4
%1 = OpFunction %3 None %5
%2 = OpFunctionParameter %4
%6 = OpLabel
OpReturn
OpFunctionEnd
)";

  spvtest::Binary linked_binary;
  ASSERT_EQ(SPV_SUCCESS, AssembleAndLink({body1, body2}, &linked_binary))
      << GetErrorMessage();

  const std::string expected_res = R"(OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpModuleProcessed "Linked by SPIR-V Tools Linker"
OpDecorate %1 FuncParamAttr Sext
%2 = OpTypeVoid
%3 = OpTypeInt 32 0
%4 = OpTypeFunction %2 %3
%5 = OpFunction %2 None %4
%1 = OpFunctionParameter %3
%6 = OpLabel
OpReturn
OpFunctionEnd
)";
  std::string res_body;
  SetDisassembleOptions(SPV_BINARY_TO_TEXT_OPTION_NO_HEADER);
  ASSERT_EQ(SPV_SUCCESS, Disassemble(linked_binary, &res_body))
      << GetErrorMessage();
  EXPECT_EQ(expected_res, res_body);
}

TEST_F(MatchingImportsToExports, FunctionCtrl) {
  const std::string body1 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Import
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%4 = OpTypeFloat 32
%5 = OpVariable %4 Uniform
%1 = OpFunction %2 None %3
OpFunctionEnd
)";
  const std::string body2 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Export
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%1 = OpFunction %2 Inline %3
%4 = OpLabel
OpReturn
OpFunctionEnd
)";

  spvtest::Binary linked_binary;
  ASSERT_EQ(SPV_SUCCESS, AssembleAndLink({body1, body2}, &linked_binary))
      << GetErrorMessage();

  const std::string expected_res =
      R"(OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpModuleProcessed "Linked by SPIR-V Tools Linker"
%1 = OpTypeVoid
%2 = OpTypeFunction %1
%3 = OpTypeFloat 32
%4 = OpVariable %3 Uniform
%5 = OpFunction %1 Inline %2
%6 = OpLabel
OpReturn
OpFunctionEnd
)";
  std::string res_body;
  SetDisassembleOptions(SPV_BINARY_TO_TEXT_OPTION_NO_HEADER);
  ASSERT_EQ(SPV_SUCCESS, Disassemble(linked_binary, &res_body))
      << GetErrorMessage();
  EXPECT_EQ(expected_res, res_body);
}

TEST_F(MatchingImportsToExports, UseExportedFuncParamAttr) {
  const std::string body1 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Import
OpDecorate %2 FuncParamAttr Zext
%2 = OpDecorationGroup
OpGroupDecorate %2 %3 %4
%5 = OpTypeVoid
%6 = OpTypeInt 32 0
%7 = OpTypeFunction %5 %6
%1 = OpFunction %5 None %7
%3 = OpFunctionParameter %6
OpFunctionEnd
%8 = OpFunction %5 None %7
%4 = OpFunctionParameter %6
OpFunctionEnd
)";
  const std::string body2 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Export
OpDecorate %2 FuncParamAttr Sext
%3 = OpTypeVoid
%4 = OpTypeInt 32 0
%5 = OpTypeFunction %3 %4
%1 = OpFunction %3 None %5
%2 = OpFunctionParameter %4
%6 = OpLabel
OpReturn
OpFunctionEnd
)";

  spvtest::Binary linked_binary;
  ASSERT_EQ(SPV_SUCCESS, AssembleAndLink({body1, body2}, &linked_binary))
      << GetErrorMessage();

  const std::string expected_res = R"(OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpModuleProcessed "Linked by SPIR-V Tools Linker"
OpDecorate %1 FuncParamAttr Zext
%1 = OpDecorationGroup
OpGroupDecorate %1 %2
OpDecorate %3 FuncParamAttr Sext
%4 = OpTypeVoid
%5 = OpTypeInt 32 0
%6 = OpTypeFunction %4 %5
%7 = OpFunction %4 None %6
%2 = OpFunctionParameter %5
OpFunctionEnd
%8 = OpFunction %4 None %6
%3 = OpFunctionParameter %5
%9 = OpLabel
OpReturn
OpFunctionEnd
)";
  std::string res_body;
  SetDisassembleOptions(SPV_BINARY_TO_TEXT_OPTION_NO_HEADER);
  ASSERT_EQ(SPV_SUCCESS, Disassemble(linked_binary, &res_body))
      << GetErrorMessage();
  EXPECT_EQ(expected_res, res_body);
}

TEST_F(MatchingImportsToExports, NamesAndDecorations) {
  const std::string body1 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpName %1 "foo"
OpName %3 "param"
OpDecorate %1 LinkageAttributes "foo" Import
OpDecorate %2 Restrict
OpDecorate %4 NonWritable
%2 = OpDecorationGroup
OpGroupDecorate %2 %3 %4
%5 = OpTypeVoid
%6 = OpTypeInt 32 0
%9 = OpTypePointer Function %6
%7 = OpTypeFunction %5 %9
%1 = OpFunction %5 None %7
%3 = OpFunctionParameter %9
OpFunctionEnd
%8 = OpFunction %5 None %7
%4 = OpFunctionParameter %9
OpFunctionEnd
)";
  const std::string body2 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpName %1 "foo"
OpName %2 "param"
OpDecorate %1 LinkageAttributes "foo" Export
OpDecorate %2 Restrict
%3 = OpTypeVoid
%4 = OpTypeInt 32 0
%7 = OpTypePointer Function %4
%5 = OpTypeFunction %3 %7
%1 = OpFunction %3 None %5
%2 = OpFunctionParameter %7
%6 = OpLabel
OpReturn
OpFunctionEnd
)";

  spvtest::Binary linked_binary;
  ASSERT_EQ(SPV_SUCCESS, AssembleAndLink({body1, body2}, &linked_binary))
      << GetErrorMessage();

  const std::string expected_res = R"(OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpName %1 "foo"
OpName %2 "param"
OpModuleProcessed "Linked by SPIR-V Tools Linker"
OpDecorate %3 Restrict
OpDecorate %4 NonWritable
%3 = OpDecorationGroup
OpGroupDecorate %3 %4
OpDecorate %2 Restrict
%5 = OpTypeVoid
%6 = OpTypeInt 32 0
%7 = OpTypePointer Function %6
%8 = OpTypeFunction %5 %7
%9 = OpFunction %5 None %8
%4 = OpFunctionParameter %7
OpFunctionEnd
%1 = OpFunction %5 None %8
%2 = OpFunctionParameter %7
%10 = OpLabel
OpReturn
OpFunctionEnd
)";
  std::string res_body;
  SetDisassembleOptions(SPV_BINARY_TO_TEXT_OPTION_NO_HEADER);
  ASSERT_EQ(SPV_SUCCESS, Disassemble(linked_binary, &res_body))
      << GetErrorMessage();
  EXPECT_EQ(expected_res, res_body);
}

TEST_F(MatchingImportsToExports, FunctionCall) {
  const std::string body1 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpName %1 "foo"
OpName %3 "param"
OpDecorate %1 LinkageAttributes "foo" Import
 %5 = OpTypeVoid
 %6 = OpTypeInt 32 0
 %9 = OpTypePointer Function %6
 %7 = OpTypeFunction %5 %9
 %1 = OpFunction %5 None %7
 %3 = OpFunctionParameter %9
OpFunctionEnd
 %8 = OpFunction %5 None %7
 %4 = OpFunctionParameter %9
%10 = OpLabel
%11 = OpFunctionCall %5 %1 %4
OpReturn
OpFunctionEnd
)";
  const std::string body2 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpName %1 "foo"
OpName %2 "param"
OpDecorate %1 LinkageAttributes "foo" Export
%3 = OpTypeVoid
%4 = OpTypeInt 32 0
%7 = OpTypePointer Function %4
%5 = OpTypeFunction %3 %7
%1 = OpFunction %3 None %5
%2 = OpFunctionParameter %7
%6 = OpLabel
OpReturn
OpFunctionEnd
)";

  LinkerOptions options;
  for (int i = 0; i < 2; i++) {
    spvtest::Binary linked_binary;
    options.SetAllowPtrTypeMismatch(i == 1);
    ASSERT_EQ(SPV_SUCCESS,
              AssembleAndLink({body1, body2}, &linked_binary, options))
        << GetErrorMessage();

    const std::string expected_res = R"(OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpName %1 "foo"
OpName %2 "param"
OpModuleProcessed "Linked by SPIR-V Tools Linker"
%3 = OpTypeVoid
%4 = OpTypeInt 32 0
%5 = OpTypePointer Function %4
%6 = OpTypeFunction %3 %5
%7 = OpFunction %3 None %6
%8 = OpFunctionParameter %5
%9 = OpLabel
%10 = OpFunctionCall %3 %1 %8
OpReturn
OpFunctionEnd
%1 = OpFunction %3 None %6
%2 = OpFunctionParameter %5
%11 = OpLabel
OpReturn
OpFunctionEnd
)";

    std::string res_body;
    SetDisassembleOptions(SPV_BINARY_TO_TEXT_OPTION_NO_HEADER);
    ASSERT_EQ(SPV_SUCCESS, Disassemble(linked_binary, &res_body))
        << GetErrorMessage();
    EXPECT_EQ(expected_res, res_body);
  }
}

TEST_F(MatchingImportsToExports, FunctionSignatureMismatchPointer) {
  const std::string body1 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpName %1 "foo"
OpName %3 "param"
OpDecorate %1 LinkageAttributes "foo" Import
 %5 = OpTypeVoid
 %6 = OpTypeInt 8 0
 %9 = OpTypePointer Function %6
 %7 = OpTypeFunction %5 %9
 %1 = OpFunction %5 None %7
 %3 = OpFunctionParameter %9
OpFunctionEnd
 %8 = OpFunction %5 None %7
 %4 = OpFunctionParameter %9
%10 = OpLabel
%11 = OpFunctionCall %5 %1 %4
OpReturn
OpFunctionEnd
)";
  const std::string body2 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpName %1 "foo"
OpName %2 "param"
OpDecorate %1 LinkageAttributes "foo" Export
%3 = OpTypeVoid
%4 = OpTypeInt 32 0
%7 = OpTypePointer Function %4
%5 = OpTypeFunction %3 %7
%1 = OpFunction %3 None %5
%2 = OpFunctionParameter %7
%6 = OpLabel
OpReturn
OpFunctionEnd
)";

  spvtest::Binary linked_binary;
  ASSERT_EQ(SPV_ERROR_INVALID_BINARY,
            AssembleAndLink({body1, body2}, &linked_binary))
      << GetErrorMessage();
  EXPECT_THAT(
      GetErrorMessage(),
      HasSubstr("Type mismatch on symbol \"foo\" between imported "
                "variable/function %1 and exported variable/function %11"));

  LinkerOptions options;
  options.SetAllowPtrTypeMismatch(true);
  ASSERT_EQ(SPV_SUCCESS,
            AssembleAndLink({body1, body2}, &linked_binary, options))
      << GetErrorMessage();

  const std::string expected_res = R"(OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpName %1 "foo"
OpName %2 "param"
OpModuleProcessed "Linked by SPIR-V Tools Linker"
%3 = OpTypeVoid
%4 = OpTypeInt 8 0
%5 = OpTypePointer Function %4
%6 = OpTypeFunction %3 %5
%7 = OpTypeInt 32 0
%8 = OpTypePointer Function %7
%9 = OpTypeFunction %3 %8
%10 = OpFunction %3 None %6
%11 = OpFunctionParameter %5
%12 = OpLabel
%13 = OpBitcast %8 %11
%14 = OpFunctionCall %3 %1 %13
OpReturn
OpFunctionEnd
%1 = OpFunction %3 None %9
%2 = OpFunctionParameter %8
%15 = OpLabel
OpReturn
OpFunctionEnd
)";
  std::string res_body;
  SetDisassembleOptions(SPV_BINARY_TO_TEXT_OPTION_NO_HEADER);
  ASSERT_EQ(SPV_SUCCESS, Disassemble(linked_binary, &res_body))
      << GetErrorMessage();
  EXPECT_EQ(expected_res, res_body);
}

TEST_F(MatchingImportsToExports, FunctionSignatureMismatchValue) {
  const std::string body1 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpName %1 "foo"
OpName %3 "param"
OpDecorate %1 LinkageAttributes "foo" Import
 %5 = OpTypeVoid
 %6 = OpTypeInt 8 0
 %7 = OpTypeFunction %5 %6
 %1 = OpFunction %5 None %7
 %3 = OpFunctionParameter %6
OpFunctionEnd
 %8 = OpFunction %5 None %7
 %4 = OpFunctionParameter %6
%10 = OpLabel
%11 = OpFunctionCall %5 %1 %4
OpReturn
OpFunctionEnd
)";
  const std::string body2 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpName %1 "foo"
OpName %2 "param"
OpDecorate %1 LinkageAttributes "foo" Export
%3 = OpTypeVoid
%4 = OpTypeInt 32 0
%5 = OpTypeFunction %3 %4
%1 = OpFunction %3 None %5
%2 = OpFunctionParameter %4
%6 = OpLabel
OpReturn
OpFunctionEnd
)";

  LinkerOptions options;
  for (int i = 0; i < 2; i++) {
    spvtest::Binary linked_binary;
    options.SetAllowPtrTypeMismatch(i == 1);
    ASSERT_EQ(SPV_ERROR_INVALID_BINARY,
              AssembleAndLink({body1, body2}, &linked_binary))
        << GetErrorMessage();
    EXPECT_THAT(
        GetErrorMessage(),
        HasSubstr("Type mismatch on symbol \"foo\" between imported "
                  "variable/function %1 and exported variable/function %10"));
  }
}

TEST_F(MatchingImportsToExports, FunctionSignatureMismatchTypePointerInt) {
  const std::string body1 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpName %1 "foo"
OpName %3 "param"
OpDecorate %1 LinkageAttributes "foo" Import
 %5 = OpTypeVoid
 %6 = OpTypeInt 64 0
 %7 = OpTypeFunction %5 %6
 %1 = OpFunction %5 None %7
 %3 = OpFunctionParameter %6
OpFunctionEnd
 %8 = OpFunction %5 None %7
 %4 = OpFunctionParameter %6
%10 = OpLabel
%11 = OpFunctionCall %5 %1 %4
OpReturn
OpFunctionEnd
)";
  const std::string body2 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpName %1 "foo"
OpName %2 "param"
OpDecorate %1 LinkageAttributes "foo" Export
%3 = OpTypeVoid
%4 = OpTypeInt 64 0
%7 = OpTypePointer Function %4
%5 = OpTypeFunction %3 %7
%1 = OpFunction %3 None %5
%2 = OpFunctionParameter %7
%6 = OpLabel
OpReturn
OpFunctionEnd
)";

  LinkerOptions options;
  for (int i = 0; i < 2; i++) {
    spvtest::Binary linked_binary;
    options.SetAllowPtrTypeMismatch(i == 1);
    ASSERT_EQ(SPV_ERROR_INVALID_BINARY,
              AssembleAndLink({body1, body2}, &linked_binary))
        << GetErrorMessage();
    EXPECT_THAT(
        GetErrorMessage(),
        HasSubstr("Type mismatch on symbol \"foo\" between imported "
                  "variable/function %1 and exported variable/function %10"));
  }
}

TEST_F(MatchingImportsToExports, FunctionSignatureMismatchTypeIntPointer) {
  const std::string body1 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpName %1 "foo"
OpName %3 "param"
OpDecorate %1 LinkageAttributes "foo" Import
 %5 = OpTypeVoid
 %6 = OpTypeInt 64 0
 %9 = OpTypePointer Function %6
 %7 = OpTypeFunction %5 %9
 %1 = OpFunction %5 None %7
 %3 = OpFunctionParameter %9
OpFunctionEnd
 %8 = OpFunction %5 None %7
 %4 = OpFunctionParameter %9
%10 = OpLabel
%11 = OpFunctionCall %5 %1 %4
OpReturn
OpFunctionEnd
)";
  const std::string body2 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpMemoryModel Physical64 OpenCL
OpName %1 "foo"
OpName %2 "param"
OpDecorate %1 LinkageAttributes "foo" Export
%3 = OpTypeVoid
%4 = OpTypeInt 64 0
%5 = OpTypeFunction %3 %4
%1 = OpFunction %3 None %5
%2 = OpFunctionParameter %4
%6 = OpLabel
OpReturn
OpFunctionEnd
)";

  LinkerOptions options;
  for (int i = 0; i < 2; i++) {
    spvtest::Binary linked_binary;
    options.SetAllowPtrTypeMismatch(i == 1);
    ASSERT_EQ(SPV_ERROR_INVALID_BINARY,
              AssembleAndLink({body1, body2}, &linked_binary))
        << GetErrorMessage();
    EXPECT_THAT(
        GetErrorMessage(),
        HasSubstr("Type mismatch on symbol \"foo\" between imported "
                  "variable/function %1 and exported variable/function %11"));
  }
}

TEST_F(MatchingImportsToExports, LinkOnceODRLinkageVarSingle) {
  const std::string body1 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpExtension "SPV_KHR_linkonce_odr"
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" LinkOnceODR
%2 = OpTypeFloat 32
%3 = OpConstant %2 3.1415
%1 = OpVariable %2 Uniform %3
)";

  const std::string body2 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpExtension "SPV_KHR_linkonce_odr"
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Import
%2 = OpTypeFloat 32
%1 = OpVariable %2 Uniform
)";

  const std::string matchTemplate = R"(
; CHECK-NOT: OpDecorate {{.*}} Import
; CHECK-NOT: OpDecorate {{.*}} LinkOnceODR
)";

  spvtest::Binary linked_binary;
  EXPECT_EQ(SPV_SUCCESS, AssembleAndLink({body1, body2}, &linked_binary))
      << GetErrorMessage();
  Match(matchTemplate, linked_binary);
}

TEST_F(MatchingImportsToExports, LinkOnceODRLinkageFunMultiple) {
  const std::string body1 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpExtension "SPV_KHR_linkonce_odr"
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" LinkOnceODR
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%1 = OpFunction %2 Inline %3
%4 = OpLabel
OpReturn
OpFunctionEnd
)";

  const std::string body2 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpExtension "SPV_KHR_linkonce_odr"
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Import
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%1 = OpFunction %2 None %3
OpFunctionEnd
)";

  const std::string matchTemplate = R"(
; CHECK-NOT: OpDecorate {{.*}} Import
; CHECK-NOT: OpDecorate {{.*}} LinkOnceODR
)";

  spvtest::Binary linked_binary;
  EXPECT_EQ(SPV_SUCCESS, AssembleAndLink({body1, body1, body2}, &linked_binary))
      << GetErrorMessage();
  Match(matchTemplate, linked_binary);
}

TEST_F(MatchingImportsToExports, LinkOnceODRAndExport) {
  const std::string body1 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpExtension "SPV_KHR_linkonce_odr"
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" LinkOnceODR
%2 = OpTypeFloat 32
%3 = OpConstant %2 3.1415
%1 = OpVariable %2 Uniform %3
)";

  const std::string body2 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpExtension "SPV_KHR_linkonce_odr"
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Export
%2 = OpTypeFloat 32
%3 = OpConstant %2 2.7183
%1 = OpVariable %2 Uniform %3
)";

  const std::string body3 = R"(
OpCapability Linkage
OpCapability Addresses
OpCapability Kernel
OpExtension "SPV_KHR_linkonce_odr"
OpMemoryModel Physical64 OpenCL
OpDecorate %1 LinkageAttributes "foo" Import
%2 = OpTypeFloat 32
%1 = OpVariable %2 Uniform
)";
  spvtest::Binary linked_binary;
  ASSERT_EQ(SPV_ERROR_INVALID_BINARY,
            AssembleAndLink({body1, body2, body3}, &linked_binary))
      << GetErrorMessage();
  EXPECT_THAT(
      GetErrorMessage(),
      HasSubstr("Combination of Export and LinkOnceODR is not allowed, found "
                "for \"foo\""));
}

}  // namespace
}  // namespace spvtools
