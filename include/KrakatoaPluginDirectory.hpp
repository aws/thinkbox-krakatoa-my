// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MArgList.h>
#include <maya/MPxCommand.h>
#include <maya/MString.h>

class KrakatoaPluginDirectory : public MPxCommand {
  public:
    static const MString commandName;
    static void* creator();

    static void setPluginPath( const MString& path );
    static const MString getPluginPath();

  private:
    static MString s_pluginPath;

  public:
    MStatus doIt( const MArgList& args );
};
