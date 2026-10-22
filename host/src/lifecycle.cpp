#include "lifecycle.h"
#include "natives.h"

namespace lifecycle {
using gta::Call;

const char* Name(State s) {
    switch (s) {
    case State::Unprepared: return "Unprepared";
    case State::Preparing: return "Preparing";
    case State::Ready: return "Ready";
    case State::Waiting: return "Waiting";
    case State::Entering: return "Entering";
    case State::Skating: return "Skating";
    case State::Leaving: return "Leaving";
    case State::Delegated: return "Delegated";
    }
    return "?";
}

bool EnteringVehicle(Ped ped) {
    return Call<BOOL>(gta::IS_PED_IN_ANY_VEHICLE, ped, 1) || // 1: also while getting in
           Call<BOOL>(gta::IS_PED_GETTING_INTO_A_VEHICLE, ped) ||
           Call<Vehicle>(gta::GET_VEHICLE_PED_IS_TRYING_TO_ENTER, ped) != 0 ||
           Call<BOOL>(gta::IS_PED_HANGING_ON_TO_VEHICLE, ped);
}

const char* Blocked(Ped ped, bool strict, const Settings& settings) {
    (void)strict;
    // Story Mode only (AGENTS.md hard scope).
    if (Call<BOOL>(gta::NETWORK_IS_SESSION_STARTED) || Call<BOOL>(gta::NETWORK_IS_GAME_IN_PROGRESS))
        return "network session: SkateV is Story Mode only";
    if (!Call<BOOL>(gta::DOES_ENTITY_EXIST, ped)) return "no player";
    if (Call<BOOL>(gta::IS_ENTITY_DEAD, ped, 0) || Call<BOOL>(gta::IS_PED_DEAD_OR_DYING, ped, 1)) return "dead";
    if (EnteringVehicle(ped)) return "in or entering a vehicle";
    if (Call<BOOL>(gta::IS_PAUSE_MENU_ACTIVE)) return "paused";
    if (Call<BOOL>(gta::IS_CUTSCENE_ACTIVE)) return "cutscene";
    if (Call<BOOL>(gta::IS_PLAYER_SWITCH_IN_PROGRESS)) return "character switch";
    if (Call<BOOL>(gta::GET_IS_LOADING_SCREEN_ACTIVE) || !Call<BOOL>(gta::IS_SCREEN_FADED_IN)) return "loading / faded";
    if (Call<BOOL>(gta::IS_ENTITY_ATTACHED, ped)) return "attached";
    // Director Mode raises the mission flag for its whole session; it is free
    // play, so only real missions are refused. joaat("director_mode").
    if (!settings.inMissions && Call<BOOL>(gta::GET_MISSION_FLAG) &&
        Call<int>(gta::GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH, 0xCAC8014Fu) == 0)
        return "mission running (BoardActionInMissions=0)";
    // The hand-off captures GTA's live pose and velocity before clearing its
    // tasks. Transient ped states do not change player ownership or safety.
    return nullptr;
}

} // namespace lifecycle
