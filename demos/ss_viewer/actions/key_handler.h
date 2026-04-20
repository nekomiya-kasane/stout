/**
 * @file key_handler.h
 * @brief Keyboard event handler for ss_viewer.
 */
#pragma once

#include "ss_viewer/model/viewer_state.h"
#include "tapiru/core/input.h"
#include "tapiru/widgets/classic_app.h"

namespace ssv {

/// @brief Handle a key event. Returns true if the event was consumed.
[[nodiscard]] bool handle_key(const tapiru::key_event &ke, viewer_state &st, tapiru::classic_app &app);

} // namespace ssv
