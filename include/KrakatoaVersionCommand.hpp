// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MPxCommand.h>

class KrakatoaVersionCommand : public MPxCommand {
  public:
    static const MString commandName;
    KrakatoaVersionCommand();
    virtual ~KrakatoaVersionCommand();
    static void* creator();
    virtual MStatus doIt( const MArgList& args );
};
