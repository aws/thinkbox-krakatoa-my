// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MArgList.h>
#include <maya/MPxCommand.h>
#include <maya/MString.h>

/**
 * Maya Plugin class for the KrakatoaSR exporter command
 */
class prt_exporter : public MPxCommand {
  public:
    static const MString commandName;
    static void* creator();

  public:
    prt_exporter();
    ~prt_exporter();
    MStatus doIt( const MArgList& Args );
};
