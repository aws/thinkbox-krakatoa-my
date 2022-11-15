// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <krakatoasr_renderer.hpp>

#include <frantic/strings/tstring.hpp>

/**
 * Krakatoa SR progress bar callback for maya
 * Unfortunately, this is actually 90% calls to mel's 'progressBar' procedure, since there
 * seems to be no C++ api for the maya progress bar.  Someone please correct me if I'm wrong.
 */
class maya_progress_bar_interface : public krakatoasr::progress_logger_interface,
                                    public krakatoasr::cancel_render_interface {
  public:
    maya_progress_bar_interface();
    ~maya_progress_bar_interface();

    virtual bool is_cancelled();

    virtual void set_title( const char* title );
    virtual void set_progress( float progress );

    void set_num_frames( int numFrames );
    void set_current_frame( int currentFrame );

    void begin_display();
    void end_display();

    void reset_state();

  private:
    void set_progress_min_max();

    int m_numFrames;
    int m_currentFrame;
    frantic::tstring m_title;
    bool m_displayStarted;
};
