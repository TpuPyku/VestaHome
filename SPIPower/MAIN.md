# `Main.exe` — оболочка ГУ: запуск `G3NavHMI` и защита от «кирпича»

Разобрано 2026-10-09 декомпиляцией (`Vesta/SPIPower/ghidra_scripts/DecompAll.java`,
полный листинг — `Vesta/SPIPower/ghidra/main_decomp.txt`, ~250 функций).

Вопрос, на который отвечает документ: **кто и когда перезагружает ГУ, если
`G3NavHMI.exe` не запускается, каково окно отката и можно ли флагом
`SwUpdate.flag` заставить ГУ уйти в авто‑откат.**

## Разобранные файлы

| Файл | Размер | MD5 | Примечание |
|---|---|---|---|
| `MAIN/ResidentFlash/Main.exe` | 57 344 | `F649210E3792134392D0B340E0311AA5` | эталон разбора (лежит рядом с флагами) |
| `MAIN/4120/Main.exe` | 57 344 | `F649210E…` | побайтно тот же |
| `MAIN/ResidentFlash/Main_H2.exe` | 57 856 | `FA885809EAAD12BBE388B381F6F11F53` | те же строки/логика |
| `MAIN/4120_unpacked/NK/fs/Windows/Main.exe` | 56 832 | `A26E72A1BF124051ED3C72A0C0491D8C` | сборка 4120 из `NK.bin`; **тот же набор строк** |

Проверка на всех трёх вариантах (`ascii`/`wide` вхождения): `G3NavHMI` 4/4,
`Cancle60s` 5/5, `SwDnl` 22, `IsIncompletedSwUpdate` 2, `SetUpdateTaskFlag` 3,
`SPApi_SendResetEvent` 1, `SPIPowerMgr` 1, `Explorer.exe` 2, `bACKuPdESAYs` 1 —
логика одинаковая, отличия только в сборке.

PDB‑путь внутри: `d:\Project\MMC27\Code\G3MMC27\Tools\Main\Desay_SDK_Prima_II (ARMV4I)\Release\Main.pdb`
(сам `.pdb` не поставляется, поэтому имена функций в листинге — `FUN_…`).

Импорт (статический): только `desay_sdk_api.dll` и `COREDLL.dll`.
`SPIPowerMgr.dll` **не** импортируется, а грузится динамически (см. ниже) — поэтому
в старых заметках «Main импортирует SPApi_*» неверно.

Английские строки в файле — «узкие» (`fopen`), пути запуска процессов — «широкие»
(`CreateFileW`/`CreateProcessW`). Поэтому обычный ASCII‑поиск по `SwDnl`/`G3NavHMI`
даёт 0: искать надо в UTF‑16.

## Разбор

### 0. `FUN_000115d8` — Main грузит `SPIPowerMgr.dll` в свой процесс

```
iVar1 = LoadLibraryW(L"\\ResidentFlash\\navigation\\app\\SPIPowerMgr.dll");
*obj            = iVar1;                                   // hModule
obj[8]  = GetProcAddress(h, "SPApi_LoadByMain");
obj[9]  = GetProcAddress(h, "SPApi_SetSkelonton");
obj[10] = GetProcAddress(h, "SPApi_NavStarted");
obj[0xd]= GetProcAddress(h, "SPApi_SendRadioReceivedRVCStatus");
obj[0xb]= GetProcAddress(h, "SPApi_LogoCompleteByMain");
obj[0xc]= GetProcAddress(h, "SPApi_EmptySkelonton");
obj[0xe]= GetProcAddress(h, "SPApi_RegisterAspectCallBack");
obj[0xf]= GetProcAddress(h, "SPApi_SetExitSPI");
obj[2]  = GetProcAddress(h, "SPApi_SendResetEvent");       // <-- m_pReset
...
(*obj[8])();                   // SPApi_LoadByMain()
(*obj[9])(8, cb);              // SPApi_SetSkelonton(8, ...)
(*obj[0xe])(0, FUN_000175dc);  // SPApi_RegisterAspectCallBack(0, ...)
(*obj[10])();                  // SPApi_NavStarted()
```

Если хоть один `GetProcAddress` не удался — пишется `[main][error code:…]` и
SPI‑часть просто выключается (Main продолжает работать).

**Следствие для нас:** наш proxy грузит **не только** `G3NavHMI`, но и сам
`Main.exe` в своём процессе.

### 1. Поток `FUN_00016a30` (`WaitResidentFlashThread`) — точка принятия решения

```
…поиск \ResidentFlash, установка системного времени, загрузка шрифта…
bVar1 = IsIncompletedSwUpdate();
if (bVar1) {                                   // SwUpdate.flag «незавершён»
    log("MAIN: Incompleted sw update");
    ждать \SDMemory до 5 с;
    если SD нет  -> FUN_00016898() /* SwDnl из \Windows */; выход;
    иначе        -> FUN_000163c8() /* SwDnl с SD/ResidentFlash */; выход;
} else {
    iVar2 = FUN_00016604();                    // запуск G3NavHMI, 3 попытки
    if (iVar2 != 0) { log("MAIN: AutoRunCe(srcDir)  success"); выход; }  // <-- и всё
    ждать \SDMemory до 5 с;
    если SD нет -> SetUpdateTaskFlag(2); SwDnl из \Windows; выход;
    иначе       -> SwUpdate.flag = 0x02;       // отметка «незавершённое обновление»
    FUN_000163c8();                            // SwDnl с SD
}
```

### 2. `FUN_00016604` — запуск `G3NavHMI.exe`, 3 попытки

```
local_220 = 0;  memset(&si, 0, 0x206);
buildCmdLine(&startup, L"\\ResidentFlash\\navigation\\app\\G3NavHMI.exe");
log("MAIN: Run G3NavHMI ----------- GetTickCount[%ld].\n", GetTickCount());
try = 1; wrote_fail_flag = false;
do {
    ok = CreateProcess(&startup, 0, 0, 0);
    if (ok) {
        g_nav_started = 1;
        log("MAIN: CreateProcess G3NavHMI.exe success as %d time. …", try, GetTickCount());
        if (exists(L"\\ResidentFlash\\CreateProcessResetflg.flag")) {
            CloseHandle(); DeleteFile(L"\\ResidentFlash\\CreateProcessResetflg.flag");
        }
        return 1;                              // <-- процесс дальше НЕ отслеживается
    }
    err = GetLastError();
    log("MAIN: ERR CreateProcess G3NavHMI.exe !   err = %d  try time = %d \r\n", err, try);
    f = create(L"\\ResidentFlash\\CreateProcessFail.flag");
    if (!f) log("MAIN: CreateProcess CreateProcessflg.flag fopen error 111!!!  \r\n");
    else { write(f, &try, 4); write(f, &err, 4); CloseHandle(f); wrote_fail_flag = true; }
    Sleep(1000);
    try++;
} while (try < 4);                             // попытки 1, 2, 3

// все 3 неудачны:
if (!exists(L"\\ResidentFlash\\CreateProcessResetflg.flag")) {
    create(L"\\ResidentFlash\\CreateProcessResetflg.flag");     // пустая отметка
    reset = GetSpiObject();                    // FUN_00011000()
    m_pReset(1);                               // FUN_000115a8 -> SPApi_SendResetEvent(1)
    Sleep(3000);
    log("MAIN: CreateProcess elapse 3 sencond after reset request!!!  \r\n");
    if (wrote_fail_flag) DeleteFile(L"\\ResidentFlash\\CreateProcessFail.flag");
} else { CloseHandle(); }
DeleteFile(L"\\ResidentFlash\\CreateProcessResetflg.flag");     // «before update sw»
return 0;
```

`FUN_000115a8(obj)` — просто вызов указателя:

```
if (*(code **)(obj + 8) != 0) (**(code **)(obj + 8))(1);   // SPApi_SendResetEvent(1)
else log("[main][warning] The m_pReset is NULL !!!!");
```

### 3. Формат `\ResidentFlash\SwUpdate.flag` — **один байт**

`FUN_00016318` (`IsIncompletedSwUpdate`):

```
f = fopen("\\ResidentFlash\\SwUpdate.flag", mode);      // «узкий» путь
if (!f) return false;
n = fread(&b, 1, 1, f);                                 // 1 байт
log("MAIN: IsIncompletedSwUpdate() - iReadCount : %d  bSwDnlType : %d  \r\n", n, b);
return (n != 0) && ((b & 0x0f) != 0);
```

`FUN_0001699c` (`SetUpdateTaskFlag`):

```
f = fopen("\\ResidentFlash\\SwUpdate.flag", mode);
b = (BYTE)arg;  n = fwrite(&b, 1, 1, f);
log("MAIN: SetUpdateTaskFlag() - iWriteCount : %d  bSwDnlType : %d  \r\n", n, b);
```

То есть **никакого заголовка/счётчика нет**: файл = 1 байт (`bSwDnlType`), а
условие «идёт обновление» = файл существует **и** младший ниббл байта ≠ 0.
Своё значение при провале запуска `G3NavHMI` Main пишет сам: **`0x02`**.

### 4. `FUN_000163c8` — запуск `SwDnl.exe` (апдейтер)

```
есть \SDMemory\SwDnl.exe ?
нет -> \ResidentFlash\SwDnl.exe     (страховка: копия SwDnl_bk.exe, 4 попытки CopyFile)
нигде нет -> "SwDnl.exe" из \Windows («OS BackUp»);
CreateProcess(exe, L"1 0 0", 0, 0);
если не поднялся -> повтор со \SDMemory\SwDnl.exe;
```

### 5. `FUN_00016e28` — аварийная оболочка с SD

```
if (exists("\\SDMemory\\bACKuPdESAYsV201200070012.Mainflg")) {
    CreateProcess("\\SDMemory\\Cancle60s.exe", 0,0,0);    // «отмена 60 с» (2 попытки)
    CreateProcess("\\windows\\Explorer.exe", 0,0,0);      // обычный рабочий стол
}
```

## Ответы (решение)

1. **Перезагружает ли `Main.exe`?** Да, но как **запрос по SPI**:
   `SPApi_SendResetEvent(1)` из `SPIPowerMgr.dll`. Сам SoC не сбрасывает — сброс
   выполняет MCU. `SPApi_SendResetEvent` = `FUN_1000dc00` в `SPIPowerMgr.dll`
   (RVA `0x12DF8`, image base `0x10000000`): формирует 13‑байтовое сообщение и
   отправляет его с **id `0x5403`**. То есть сброс идёт **через наш proxy**.
2. **Кто именно включает сброс?** Только провал `CreateProcess`
   (`CreateProcess` вернул 0) **три раза** подряд. **Уже запущенный** `G3NavHMI`
   `Main.exe` не мониторит вообще: получив успешный `CreateProcess`, он
   возвращает 1 и навсегда забывает про процесс (ни `WaitForSingleObject`, ни
   таймера). Поэтому «перезагрузка через ~2 минуты после закрытия/падения
   `G3NavHMI`» — это **другой** механизм (MCU перестаёт получать heartbeat; в
   `Main.exe` heartbeat нет).
3. **Окно отката.** При отказе запуска — **≈6 с**: 3 попытки с `Sleep(1000)`
   (≈3 с) + `Sleep(3000)` после `SPApi_SendResetEvent`. Дальше Main уже пишет
   `SwUpdate.flag=2` и поднимает `SwDnl`. Задержка собственно сброса — на стороне
   MCU (по сообщению `0x5403`), из кода Main не выводится.
4. **Можно ли `SwUpdate.flag` заставить авто‑откат?** **Да, и это самый надёжный
   триггер.** Достаточно положить в `\ResidentFlash\` файл `SwUpdate.flag` из
   одного байта со значением, у которого младший ниббл ≠ 0 (проще всего — `0x02`,
   ровно то, что пишет сам Main). Тогда на следующей загрузке Main **не станет
   запускать `G3NavHMI`** и сразу пойдёт в `SwDnl` (с SD → ResidentFlash → \Windows).
   Это работает поверх загрузочного триггера по `\SDMemory\SwDnl.upg`/`SwDnl.cfg`.
   Откат: удалить `\ResidentFlash\SwUpdate.flag`.
5. **`CreateProcessFail.flag` / `CreateProcessResetflg.flag`** Main **не читает**
   нигде — только создаёт/пишет/удаляет. Это диагностика для человека/сервиса.
   `CreateProcessFail.flag` = `{DWORD try=3, DWORD GetLastError()}`; на диске
   `03 00 00 00 02 00 00 00` (3 попытки, ошибка 2 = `ERROR_FILE_NOT_FOUND` —
   типично для «не нашлась одна из импортируемых DLL»).

## Что это значит для proxy (`SPIPowerMgr.dll`)

- Сценарий «proxy сломал `G3NavHMI`»:
  - `CreateProcess` **вернул 0** (битый статический импорт) → 3 попытки →
    `SwUpdate.flag=2` → `SwDnl` с SD. Откат автоматический, окно ≈6 с.
  - `CreateProcess` **вернул успех**, а `G3NavHMI` упал в загрузчике/на старте →
    Main **не** реагирует; ГУ сбросит MCU по своему таймауту (те самые ~2 мин).
  - Proxy ломает **`Main.exe`** (тот грузит ту же DLL) → сломается вся оболочка.
- `SPApi_SendResetEvent` в нашем `.def` есть, поэтому `m_pReset` резолвится.
  Побочный эффект: наш proxy может **логировать/гасить** этот запрос сброса —
  удобная точка для «не сбрасывать, а откатывать».
- После успешного старта `G3NavHMI` Main удаляет `CreateProcessResetflg.flag` —
  годится как признак «запуск состоялся».

## Артефакты

- `ghidra_scripts/DecompAll.java` — декомпиляция всех функций программы в файл.
- `ghidra/main_decomp.txt` — листинг `Main.exe` (генерируется, в git не хранится).
- `ghidra_scripts/DecompAt.java` — декомпиляция по адресам (использован для
  `SPApi_SendResetEvent`; копия из `G3patch/ghidra_scripts`).
- `tools/pe_exports.py` — RVA экспортов `SPIPowerMgr.dll` (для адресов выше).


