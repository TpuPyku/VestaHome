// test_plugvol.cpp - which volumes VestaHome starts its add-ons from (plugvol.h), on the PC.
#include <windows.h>
#include <stdio.h>
#include "plugvol.h"

static int s_failed, s_checks;

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
    Check(PlugVolumeOk(L"\\ResidentFlash\\VHContent\\VHPlugins\\VestaAuto\\VestaAuto.exe"), "the built-in partition");
    Check(PlugVolumeOk(L"\\SDMMC\\VHContent\\VHPlugins\\VestaAuto\\VestaAuto.exe"), "the built-in card");
    Check(PlugVolumeOk(L"\\residentflash\\X\\X.exe"), "the volume's name in any case");
    Check(!PlugVolumeOk(L"\\SDMemory\\VHContent\\VHPlugins\\VestaAuto\\VestaAuto.exe"),
          "the SD card: taken out, the add-on running from it dies (the tester 2026-11-23)");
    Check(!PlugVolumeOk(L"\\USBDevice\\VHContent\\VHPlugins\\X\\X.exe"), "a USB disk");
    Check(!PlugVolumeOk(L"\\USBDisk\\X\\X.exe"), "a USB disk's other name");
    Check(!PlugVolumeOk(L"\\ResidentFlashX\\X\\X.exe"), "only the volume itself, not a name starting like it");
    Check(!PlugVolumeOk(L"\\SDMMC"), "the volume without a file");
    Check(!PlugVolumeOk(L""), "nothing");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}
