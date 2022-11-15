// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "maya_progress_bar_interface.hpp"

#include <maya/MGlobal.h>
#include <maya/MProgressWindow.h>

#include <frantic/logging/logging_level.hpp>
#include <sstream>

namespace {
std::string escape_mel_string( const std::string& inString ) {
    std::string resultString( inString );
    size_t currentPos = 0;

    while( currentPos < resultString.length() ) {
        currentPos = resultString.find_first_of( '\"', currentPos );

        if( currentPos == std::string::npos ) {
            break;
        } else {
            resultString.replace( currentPos, 1, "\\\"" );
            currentPos += 2;
        }
    }

    return "\"" + resultString + "\"";
}
} // namespace

maya_progress_bar_interface::maya_progress_bar_interface() { reset_state(); }

maya_progress_bar_interface::~maya_progress_bar_interface() {
    if( m_displayStarted ) {
        end_display();
    }
}

bool maya_progress_bar_interface::is_cancelled() {
    if( m_displayStarted ) {
        std::ostringstream os;
        os << "progressBar -query -isCancelled $gMainProgressBar;";
        int result;
        MGlobal::executeCommand( os.str().c_str(), result );
        return result ? true : false;
    }

    return false;
}

void maya_progress_bar_interface::reset_state() {
    m_numFrames = 1;
    m_currentFrame = 0;
    m_displayStarted = false;
}

void maya_progress_bar_interface::set_title( const char* title ) {
    if( m_displayStarted ) {
        std::ostringstream os;
        os << "progressBar -edit -status " << escape_mel_string( title ) << " $gMainProgressBar;";
        MGlobal::executeCommand( os.str().c_str() );
    }
}

void maya_progress_bar_interface::set_progress( float progress ) {
    if( m_displayStarted ) {
        std::ostringstream os;
        os << "progressBar -edit -progress " << int( ( m_currentFrame + progress ) * 100.0f ) << " $gMainProgressBar;";
        MGlobal::executeCommand( os.str().c_str() );
    }
}

void maya_progress_bar_interface::set_num_frames( int numFrames ) {
    if( numFrames > 0 && numFrames != m_numFrames ) {
        m_numFrames = numFrames;
        set_progress_min_max();

        if( m_currentFrame >= m_numFrames ) {
            set_current_frame( m_numFrames - 1 );
        }
    }
}

void maya_progress_bar_interface::set_current_frame( int currentFrame ) {
    if( currentFrame >= 0 && currentFrame < m_numFrames && currentFrame != m_currentFrame ) {
        m_currentFrame = currentFrame;
        // I'm assuming that when setting the current frame, you also want to set the progress to the start of that
        // frame
        set_progress( 0.0f );
    }
}

void maya_progress_bar_interface::begin_display() {
    if( !m_displayStarted ) {
        std::ostringstream os;
        os << "progressBar -edit -isInterruptable true -beginProgress $gMainProgressBar;";
        MGlobal::executeCommand( os.str().c_str() );
        m_displayStarted = true;
        set_progress( 0.0f );
    }
}

void maya_progress_bar_interface::end_display() {
    if( m_displayStarted ) {
        std::ostringstream os;
        os << "progressBar -edit -endProgress $gMainProgressBar;";
        MGlobal::executeCommand( os.str().c_str() );
        set_progress_min_max();
        m_displayStarted = false;
    }
}

void maya_progress_bar_interface::set_progress_min_max() {
    std::ostringstream os;
    os << "progressBar -edit -minValue " << 0 << " -maxValue " << m_numFrames * 100 << " $gMainProgressBar;";
    MGlobal::executeCommand( os.str().c_str() );
}
