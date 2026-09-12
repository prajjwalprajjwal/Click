#ifndef MILESTONE_TYPES_H
#define MILESTONE_TYPES_H

#include <stdint.h>

static const uint32_t MILESTONE_FLAG_WORDS = 8;  // 256 one-time milestone bits

enum class MilestoneEvent : uint8_t {
    NONE = 0,
    HELLO_10,
    KEEP_GOING_25,
    ANIMATION_50,
    NICE_69,
    FIRST_SIGNAL,
    GLITCH_111,
    STILL_HERE_250,
    SIGNAL_02,
    PROBABLY_FINE_666,
    WE_HEARD_YOU_1000,
    I_REMEMBER_2000,
    PATTERN_FOUND,
    AWAKE,
    COUNTED_12345,
    WHY_DO_YOU_CLICK,
    MYSTERIOUS_BAR_25000,
    TRANSITION_30000,
    ALMOST_40000,
    HALFWAY,
    STILL_GOING_60000,
    NICE_69420,
    READY_75000,
    REMAIN_80000,
    DONT_STOP_90000,
    FINAL_APPROACH,
    FINAL_99900,
    FINAL_99990,
    FINAL_99999,
    CYCLE_COMPLETE,
    YOU_CAME_BACK_100001,
    CYCLE_02_110000,
    AGAIN_125000,
    HALFWAY_AGAIN_150000,
    SOMETHING_DIFFERENT_175000,
    FINAL_199999,
    CYCLE_02_COMPLETE,
};

enum class MilestoneId : uint8_t {
    M10 = 0,
    M25,
    M50,
    M69,
    M100,
    M111,
    M250,
    M500,
    M666,
    M1000,
    M2000,
    M5000,
    M10000,
    M12345,
    M20000,
    M25000,
    M30000,
    M40000,
    M50000,
    M60000,
    M69420,
    M75000,
    M80000,
    M90000,
    M99000,
    M99900,
    M99990,
    M99999,
    M100000,
    M100001,
    M110000,
    M125000,
    M150000,
    M175000,
    M199999,
    M200000,
    COUNT,
};

struct MilestoneTrigger {
    uint64_t lifetimeClicks;
    MilestoneId id;
    MilestoneEvent event;
};

struct MilestoneEventInfo {
    MilestoneEvent event = MilestoneEvent::NONE;
    MilestoneId id = MilestoneId::M10;
    uint64_t lifetimeClicks = 0;
    uint32_t cycleNumber = 1;
    uint32_t cycleClicks = 0;
};

#endif // MILESTONE_TYPES_H
