// The stock shell after "Штатная" (HU 2026-11-14-2): after the warm restart from VestaHome it may wait 23 s for the
// MCU's "logo complete" and hang; a restart from the stock shell's state brings it up. Ours: its window must be there by
// the deadline (time since the boot; it is up in under 20 s), else the unit is restarted - once.
#ifndef STOCKWATCH_H
#define STOCKWATCH_H

enum { STOCK_WAIT, STOCK_ALIVE, STOCK_RESTART, STOCK_GIVEUP };
static const unsigned kStockDeadlineMs = 25000;

// alive - its window is there; upMs - since the boot; restarted - this boot is our restart already
static int StockVerdict(bool alive, unsigned upMs, bool restarted)
{
    if (alive)
        return STOCK_ALIVE;
    if (upMs < kStockDeadlineMs)
        return STOCK_WAIT;
    return restarted ? STOCK_GIVEUP : STOCK_RESTART;
}

#endif
