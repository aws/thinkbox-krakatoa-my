// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MArgList.h>
#include <maya/MPxCommand.h>
#include <maya/MString.h>

class KrakatoaSettingsNodeName : public MPxCommand {
  public:
    static const MString commandName;
    static const MString nodeName;
    static void* creator();

  public:
    MStatus doIt( const MArgList& args );
};
