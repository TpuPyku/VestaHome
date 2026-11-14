// stockwatch.h: the stock shell after "Штатная" - alive, wait, restart the unit once, or leave it.
#include "stockwatch.h"
#include <stdio.h>

static int s_checks, s_failed;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

int main()
{
    Check(StockVerdict(true, 9000, false) == STOCK_ALIVE, "its window there - alive");
    Check(StockVerdict(true, 30000, true) == STOCK_ALIVE, "alive after the restart too");
    Check(StockVerdict(false, 9000, false) == STOCK_WAIT, "no window yet, early - wait");
    Check(StockVerdict(false, kStockDeadlineMs - 1, false) == STOCK_WAIT, "just before the deadline - wait");
    Check(StockVerdict(false, kStockDeadlineMs, false) == STOCK_RESTART, "the deadline, first time - restart");
    Check(StockVerdict(false, kStockDeadlineMs, true) == STOCK_GIVEUP, "the deadline after a restart - leave it");
    Check(kStockDeadlineMs >= 20000 && kStockDeadlineMs <= 30000, "the deadline: the shell is up in under 20 s");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
