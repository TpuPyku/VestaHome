// vhseen.h - whether VestaHome is what the user sees, so its live parts (the clock, the speed, the widgets) are
// drawn. Was "its window in front" alone: on the unit with the CAN panel (HU 2026-11-26) a window of the panel took
// the front while VestaHome stayed on the screen - nothing drawn for 7 minutes of driving, the speed widget at 0.
// Now also seen when what is in front is invisible or covers less than half the screen (a bar, a small popup).
#pragma once

inline bool VhSeenBy(bool shown, bool fgOurs, bool fgShown, long fgArea, long screenArea)
{
    return shown && (fgOurs || !fgShown || fgArea * 2 < screenArea);
}
