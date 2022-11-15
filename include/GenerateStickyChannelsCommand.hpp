// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MArgList.h>
#include <maya/MPxCommand.h>
#include <maya/MString.h>

/**
 * Maya Plugin class for the GenerateStickyChannels command
 */
class GenerateStickyChannels : public MPxCommand {
  public:
    static const MString commandName;
    static void* creator();

  public:
    GenerateStickyChannels();
    ~GenerateStickyChannels();
    MStatus doIt( const MArgList& args );
};
