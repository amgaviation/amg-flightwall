// Scene-engine include aggregator for the hd_wf2_live target.
//
// Historical note: during the parallel platform build this header carried a
// stub mirror of the scene-engine API so the target could compile before the
// portable core landed. The core is now present and verified, so the stub
// branch was removed at integration (see docs/platform-design.md); only the
// real includes remain.
#pragma once

#include "amg/flightwall/amg_mission_board_scene.hpp"
#include "amg/flightwall/amg_ops_scene.hpp"
#include "amg/flightwall/clock_scene.hpp"
#include "amg/flightwall/color.hpp"
#include "amg/flightwall/countdown_scene.hpp"
#include "amg/flightwall/data_models.hpp"
#include "amg/flightwall/flight_radar_scene.hpp"
#include "amg/flightwall/message_scene.hpp"
#include "amg/flightwall/metar_scene.hpp"
#include "amg/flightwall/notification_scene.hpp"
#include "amg/flightwall/scene.hpp"
#include "amg/flightwall/scene_rotator.hpp"

#define AMG_LIVE_HAVE_SCENES 1
