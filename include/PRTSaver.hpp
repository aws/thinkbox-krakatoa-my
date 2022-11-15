// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <maya/MPxLocatorNode.h>
class PRTSaver : public MPxNode {
  public:
    static void* creator();
    static MStatus initialize();
    static MTypeId typeId;
    static MString typeName;

    PRTSaver( void );
    ~PRTSaver( void );
};
