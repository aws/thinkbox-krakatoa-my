// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include <maya/MArgList.h>
#include <maya/MPxCommand.h>
#include <maya/MString.h>

class krakatoaParticleJitter : public MPxCommand {
  public:
    static const MString commandName;
    static void* creator();
    static void setPluginPath( const MString& path );
    static const MString getPluginPath();
    static MSyntax newSyntax();

    /*static const char *randomFlag;
    static const char *randomLongFlag;
    static const char *radiusFlag;
    static const char *radiusLongFlag;
    static const char *nodeFlag;
    static const char *nodeLongFlag;*/

  private:
    static MString s_pluginPath;

  public:
    MStatus doIt( const MArgList& args );
};
