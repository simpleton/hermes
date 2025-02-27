// Copyright (c) Facebook, Inc. and its affiliates.
//
// This source code is licensed under the MIT license found in the LICENSE
// file in the root directory of this source tree.
//
// RUN: (! %hermesc %s 2>&1 ) | %FileCheck --match-full-lines %s

let let = 3;
// CHECK: {{.*}}:8:5: error: 'let' is disallowed as a lexically bound name
// CHECK: let let = 3;
// CHECK:     ^~~
