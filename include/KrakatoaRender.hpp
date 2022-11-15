// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MArgList.h>
#include <maya/MPxCommand.h>
#include <maya/MString.h>

class KrakatoaRender : public MPxCommand {
  public:
    static const MString commandName;
    static void* creator();

  public:
    MStatus doIt( const MArgList& args );
};

class KrakatoaBatchRender : public MPxCommand {
  public:
    static const MString commandName;
    static void* creator();

  public:
    MStatus doIt( const MArgList& args );
};

class KrakatoaLocalBatchRenderOptionsString : public MPxCommand {
  public:
    static const MString commandName;
    static void* creator();

  public:
    MStatus doIt( const MArgList& args );
};

class KrakatoaLocalBatchRender : public MPxCommand {
  public:
    static const MString commandName;
    static void* creator();
    static bool isLocalBatchRender();

  public:
    MStatus doIt( const MArgList& args );

  private:
    static bool s_flaggedAsLocalBatchRender;
};

class RenderCommandListener : public MPxCommand {
  public:
    static const MString commandName;
    static void* creator();

  public:
    MStatus doIt( const MArgList& args );
};
