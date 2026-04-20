/**
 * @file menu_handler.h
 * @brief Menu action handler for ss_viewer.
 */
#pragma once

#include "ss_viewer/model/viewer_state.h"
#include "tapiru/widgets/classic_app.h"

#include <string>

namespace ssv {

/// @brief Handle a menu item activation by label.
void handle_menu(const std::string &label, viewer_state &st, tapiru::classic_app &app);

} // namespace ssv
