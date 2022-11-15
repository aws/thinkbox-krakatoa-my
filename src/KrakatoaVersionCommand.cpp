// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "KrakatoaVersionCommand.hpp"

#include "../KrakatoaVersion.h"

const MString KrakatoaVersionCommand::commandName = "krakatoaGetVersion";

KrakatoaVersionCommand::KrakatoaVersionCommand() {}

KrakatoaVersionCommand::~KrakatoaVersionCommand() {}

void* KrakatoaVersionCommand::creator() { return new KrakatoaVersionCommand(); }

MStatus KrakatoaVersionCommand::doIt( const MArgList& args ) {
    MString result = FRANTIC_VERSION;

    setResult( result );

    return MStatus::kSuccess;
}