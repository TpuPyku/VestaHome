# `SwDnl.exe` — заводской загрузочный апдейтер: триггер, конфиг, распаковка

Разобрано 2026-10-09 декомпиляцией в Ghidra (проект `Vesta/SPIPower/ghidra/Main`,
программа `SwDnl.exe`; скрипты `ghidra_scripts/{StrRefDecomp,Callers,DecompAt,DumpW}.java`,
запуск — `tools/ghidra_post.bat`). PDB нет (`…\SD SwDnl V1.3\…\Release\SwDnl.pdb` — только
путь в бинаре), поэтому функции — `FUN_…`; имена классов восстановлены по строкам логов
(`CSwDnlApp`, `CSwDnlMgr`, `CAppConfig`, `CAutoUpdateDlg`).

Вопрос, на который отвечает документ: **что именно запускает загрузочное обновление, что
обязано лежать в `SwDnl.cfg` и как заставить апдейтер восстановить один файл на ГУ.**

## Разобранные файлы

| Файл | Где | Размер | MD5 |
|---|---|---|---|
| `SwDnl.exe` | `MAIN/4120/`, `MAIN/ResidentFlash/`, `…\NK\fs\Windows\` (одни и те же байты) | 895 488 | `6C5F7368B9B553A70D8C948CA17A8B9D` |
| `SwDnl_bk.exe` | `MAIN/ResidentFlash/` | 895 488 | резервная копия (`CreateProcess` из Main: «OS BackUp») |
| `SwDnl.cfg` | `MAIN/ResidentFlash/` (1302 Б) | эталон семантики ключей | — |
| `SwDnlSrc.cfg` | `MAIN/ResidentFlash/` (34 Б) | `SWDNL_SRC_CFG "SDMemory\SwDnl.cfg"` | — |
| `update.pkg` | `MAIN/4120/` | 1 | один байт `0x00` — пустышка (см. OS-путь) |

Импорт: `desay_sdk_api.dll`, `toolhelp.dll`, `COREDLL.dll`. Машина `0x1C2` (thumb), секции
обычные (`.text` RVA `0x1000`, `.rdata` RVA `0x30000`), база образа `0x10000` →
`VA = file_off + 0x10E00` для `.rdata`. Строки — **UTF-16** (ASCII-поиск по путям/ключам даёт 0).

Импорт COREDLL идёт **по ординалам**, поэтому имена взяты из экспорта `coredll.dll`
(`MAIN/4120_unpacked/TINYNK/fs/Windows/coredll.dll`, `tools/pe_exports.py`):
`545 = NKDbgPrintfW` (все логи, читаются debug-терминалом), `168 CreateFileW`,
`167 FindFirstFileW`, `169 SetFileAttributesW`, `172 GetFileSize`, `184 GetDiskFreeSpaceExW`,
`496 Sleep`, `509/544/553 OpenProcess/TerminateProcess/CloseHandle`, `865 PostMessageW`,
`875/876 SetTimer/KillTimer`, `735 fwscanf`, `56 wsprintfW`, `63 wcslen`, `230 _wcsicmp`.
Отсюда `FUN_00034xx`=MFC `CFile` (Open/Read/Write/Close/Flush), `FUN_00022e5c`=удаление файла,
`FUN_00022e9c`=проверка существования, `FUN_00011ea4/11dd4/11f80/114b8/115a8`=CString-операции.

## 1. Кто и когда запускает `SwDnl.exe`

Запускает **`Main.exe`** (разбор — `MAIN.md`), аргументы: `CreateProcess(exe, L"1 0 0", …)`,
путь — `\SDMemory\SwDnl.exe` → `\ResidentFlash\SwDnl.exe` → («OS BackUp») `\ResidentFlash\SwDnl_bk.exe`
→ `\Windows\SwDnl.exe`. Решение принимает `IsIncompletedSwUpdate()`:

```
\ResidentFlash\SwUpdate.flag существует И (младший ниббл != 0)   →  запустить SwDnl
иначе                                                           →  запустить G3NavHMI
после 3 провалов CreateProcess(G3NavHMI) Main сам пишет 0x02 в флаг и запускает SwDnl
```

## 2. `\ResidentFlash\SwUpdate.flag` — 1 байт, **битовая маска типов**

Читается/перезаписывается в `OSUpdateThread` (`FUN_00016074`) и в
`CompleteUpdate(type)` (`FUN_00020b38`): каждый завершённый этап делает `byte &= ~type`,
пишет байт обратно и **удаляет файл, когда маска стала 0** (`CompleteUpdate(%d): uReadCount…`).

| Бит | Что обновляется | Файл-источник на SD | Доказательство |
|---|---|---|---|
| `0x1` | Radio (RdDnl) | `\SDMemory\RdDnl.upg` | `CAppConfig::ReadSwDnlConfig` → `m_strSrcRadFile` |
| `0x2` | Navi (распаковка `SwDnl.upg`) | `\SDMemory\SwDnl.upg` | `DownloadNaviStart` → `ExtractFiles` → `CompleteUpdate(this,2)` |
| `0x4` | CAN | `\SDMemory\CANUpdate.s19` | после успеха CAN-ветки `CompleteUpdate(…,4)` |
| `0x8` | OS (NK/образ) | `\SDMemory\update.pkg` | `OSUpdateThread` снимает именно бит `8` |

`0x02` пишет сам `Main.exe` при провале запуска `G3NavHMI` — то есть **штатный откат
навигации и есть «сработавший» триггер**: файлы берутся с SD и распаковываются обратно в
`\ResidentFlash`.

## 3. Первый конфиг: `\ResidentFlash\SwDnlSrc.cfg`

Единственная строка вида

```
SWDNL_SRC_CFG	"SDMemory\SwDnl.cfg"
```

её читают `InitInstance`/`FUN_00012588` (ссылки на литерал `SwDnlSrc.cfg`), т.е. это
**указатель, откуда брать настоящий cfg**. Путь ко второму cfg в коде апдейтера при этом
**захардкожен**: `\SDMemory\SwDnl.cfg` (литерал `0x479B8`, используется в
`FUN_0002284c`) — совпадает со значением из `SwDnlSrc.cfg`, поэтому на ГУ достаточно
положить `SwDnl.cfg` в корень SD.

## 4. Второй конфиг (`\SDMemory\SwDnl.cfg`) — все ключи

Парсер `CAppConfig::ReadSwDnlCfgFile` (`FUN_0002284c`): файл открывается
`_wfopen(path, L"r")` и читается построчно через **`fwscanf(f, L"%s", buf)`** — то есть
«токен ключ» + «токен значение», значения — в кавычках, как в заводском файле.

| Ключ | Поле объекта | Смысл |
|---|---|---|
| `SWDNL_DST_DIR` | `+0x80` | база для распаковки и удалений: `ResidentFlash` |
| `SWDNL_SRC_NV_FILE` | `+0x84` | **источник Navi-обновления** (`SDMemory\SwDnl.upg`) |
| `SWDNL_SRC_RD_FILE` | `+0x88` | источник Radio-обновления (`SDMemory\RdDnl.upg`) |
| `SWDNL_RAD_FILE` | `+0x8C` | файл радио (для `m_strSrcRadFile`) |
| `SWDNL_NAV_DEL_DIR` | список `+0x90` | каталоги на удаление, ключ **повторяемый** |
| `SWDNL_NAV_DEL_FILE` | список `+0xA4` | файлы на удаление, ключ **повторяемый** |
| `SWDNL_CAN_UPDATE_COM` | `+0x40` | номер COM для CAN-обновления (строка-цифра, по умолчанию `"4"`) |

Важное:

* **`SWDNL_RD_DATA_LEN` парсер не читает вообще** (в заводском cfg он есть — это остаток
  от радио-пути; в нашем rescue-cfg строка безвредна, но и не нужна).
* Валидность («есть что обновлять») = `len(SRC_RD_FILE) != 0 || len(SRC_NV_FILE) != 0`;
  при этом пишется флаг `+0xB8` и возвращается `1`. Если оба пустые — возврат `0`
  и лог `SWDNL: ERR Parse Cfg file`.
* Отсутствие любого ключа (включая `SWDNL_DST_DIR` и оба `NAV_DEL_*`) **не является
  ошибкой** — парсер просто пропускает строку.
* Лог результата: `SWDNL: Parse result = %s %s %s %s` (DST / SRC_NV / SRC_RD / RAD)
  и `SWDNL: Parse result CANUpdateCom = %s` — по нему видно, что именно прочиталось.

## 5. Проверки перед стартом

`CAppConfig::ReadSwDnlConfig` (`FUN_000120b8`, лог `CAppConfig : ReadSwDnlConfig()!`)
и `FUN_00022c10` проверяют **наличие файлов на SD**, а не только ключи cfg:

| Проверка | Что значит | Лог |
|---|---|---|
| `\SDMemory` (том) | SD вставлена | `SWDNL: ERR no memory device` |
| `\SDMemory\RdDnl.upg` | radio valid (`+0xC`) | `m_strSrcRadFile is Empty!` / `Radio Update Valid!` |
| `\SDMemory\SwDnl.upg` | navi valid (`+0x10`) | `m_strSrcSwFile is Empty!` / `Navi Update Valid!` |
| `\SDMemory\update.pkg` | OS valid (`+0x14`) | `OS Update Valid!tq` |
| `\SDMemory\CANUpdate.s19` | CAN valid (`+0x1C`) | `m_strUpdateCanFile is Empty!` / `CAN Update Valid!` |

`FUN_00022c10` дополнительно делает `FindFirstFileW("SDMemory")` и вызывает парсер cfg;
всё вместе возвращает «обновление возможно» и запускает UI-список (`CAutoUpdateDlg`).

## 6. Navi-путь — то, чем восстанавливается наш файл

`CSwDnlMgr::DownloadNaviStart` (`FUN_00022cec`):

```
лог --CSwDnlMgr::DownloadNaviStart  ExtractFiles <SRC_NV_FILE>
IsFreeDiscSpaceEnough(size)            // GetDiskFreeSpaceExW("ResidentFlash"),
                                       // удаляет старый ResidentFlash\SwDnlTemp.upg
ExtractFiles(<SRC_NV_FILE>, delDirs, delFiles)      // FUN_00021d44
     ├ источник не найден  -> лог "SWDNL: %s not found", статус 8, выход
     ├ копия пакета в <SWDNL_DST_DIR>\SwDnlTemp.upg  (лог "Copy %s to %s completed!")
     ├ удаления: по спискам NAV_DEL_DIR/FILE (у нас списки пустые -> пропускается)
     ├ лог "SWDNL: Start extract <tmp>"
     ├ распаковка FUN_00020844(upg, <SWDNL_DST_DIR>, cb)   // пути внутри .upg начинаются
     │                                                     // с '\', поэтому получается
     │                                                     // \ResidentFlash\navigation\app\...
     └ удаление temp (лог "SWDNL: Delete %s completed!")
     при успехе -> CompleteUpdate(this, 2)   // снимает бит 0x2 в SwUpdate.flag
     иначе      -> статус 6
```

То есть для восстановления `\ResidentFlash\navigation\app\SPIPowerMgr.dll` достаточно, чтобы
пакет `SwDnl.upg` содержал запись с путём `\navigation\app\SPIPowerMgr.dll` (ровно так собран
`rescue/SwDnl.upg`: `upg_parse.py` показывает `dir=\navigation\app\`, `name=SPIPowerMgr.dll`,
`S2 cks=130710` — заводские байты и заводская контрольная сумма).

## 7. Остальные пути (чтобы случайно не задеть)

* **OS** (`OSUpdateThread`, `FUN_00016074`): сначала `FUN_00018b0c` — `CreateFileW(\SDMemory\update.pkg, …)`
  (лог `CreateFileForMapping …`), `GetFileSize` (лог `File size = 0x%X … update os begain !!!!!!!`),
  затем `SV_SwDnlUpdateImage(L"\SDMemory\update.pkg")` (экспорт `desay_sdk_api.dll` — прошивка образа)
  и `SV_TouchDisable()`; при неудаче открытия — `Image open error! \nPlease check again!`,
  статус `0x12` и `SPApi_SendResetToRadio()`. В конце снимается бит `0x8`. Поэтому `update.pkg` на SD
  класть нельзя: это путь прошивки, а не «навигации».
* **CAN**: `\SDMemory\CANUpdate.s19` + `SWDNL_CAN_UPDATE_COM` (номер COM строкой из cfg),
  `LoadLibrary(SDMemory\UppWriterDll.dll)`, `SPApi_SendUpdateConfigBitToRadio` /
  `SPApi_SendUpdateDataToRadio` / `SPApi_SendUpdateStatusToRadio`, таймеры (`SetTimer(…,2000)`,
  `KillTimer`), статусы `SWDNL_CAN Update---------------*`; по успеху `CompleteUpdate(type=4)`.
* **Radio**: `ReadFileThred` + `RdDnl.upg` (`SWDNL_SRC_RD_FILE`), `Update radio progress: %d%%`,
  `SWDNL: Send Read File End To Radio`.
* Общее для всех путей: апдейтер сначала убивает оболочку и сервисы (`main.exe`, `HzWD.exe`,
  `dr_Service.exe` через `CreateToolhelp32Snapshot`/`TerminateProcess`), в конце —
  `SWDNL Update Finished---------------Auto Restart` и `SPApi_SendResetToRadio()`.

## 8. Что это даёт rescue

**Подтверждено кодом** (можно опираться):

1. флаг `\ResidentFlash\SwUpdate.flag = 0x02` → `Main.exe` не запускает оболочку, а поднимает
   `SwDnl.exe` с SD (аргумент `"1 0 0"` разбирает сам `SwDnl`);
2. источник пакета — ровно ключ `SWDNL_SRC_NV_FILE` в `\SDMemory\SwDnl.cfg`;
3. распаковка идёт в `SWDNL_DST_DIR` (`ResidentFlash`), пути внутри `.upg` начинаются с `\`;
4. при отсутствии `SWDNL_NAV_DEL_DIR`/`SWDNL_NAV_DEL_FILE` **никаких удалений** не выполняется
   (списки пустые — ветки удалений пропускаются);
5. успех снимает бит `0x2`, неуспех оставляет (лог `SWDNL: Exract … Failed!` / `… Successed!`).

**Ещё не подтверждено на железе:** автоматический ли это старт или после `Update Start!` в диалоге
`CAutoUpdateDlg`; успевает ли `Main.exe` записать `0x02` до сброса МКУ; безопасность
`SV_SwDnlUpdateImage` с однобайтовым `update.pkg` (поэтому его на SD не кладём). Порядок проверки —
дрилл с безвредным маркерным файлом (`PLAN.md`, шаг D3).

## 9. Артефакты разбора

* `ghidra_scripts/StrRefDecomp.java` — поиск подстрок в **двух** кодировках, откат к началу строки,
  xrefs (в т.ч. из данных) и декомпиляция найденных функций.
* `ghidra_scripts/Callers.java` — вызывающие функции + ссылки на точку входа из таблиц/данных
  (MFC-обработчики приходят через vtable и карты сообщений, не через прямые `call`).
* `ghidra_scripts/DecompAt.java` — декомпиляция функции по адресу; `DumpW.java` — вычитать литерал
  по адресу (когда декомпилятор пишет `&DAT_…`).
* `tools/ghidra_post.bat` — запуск скриптов над программой проекта (`PROGRAM=SwDnl.exe` по умолчанию).
* `tools/pe_strings.py` — строки ASCII+UTF-16LE с офсетами (для сверки «файл ↔ адрес Ghidra»;
  в этом бинаре `VA = file_off + 0x10E00` для `.rdata`).
* Выгрузки (`ghidra/swdnl_*.txt`, `ghidra/import_swdnl.log`) — генерируются, в git не хранятся.
* `tools/pe_exports.py` по `coredll.dll` — таблица «ординал → имя», без неё декомпиляция нечитаема.

## 10. Проверка этого пути на ГУ (drill)

Практический прогон цепочки «том `\SDMemory` → `SwDnl.cfg` → `SwUpdate.flag = 0x02` → распаковка в
`\ResidentFlash`» без подмены DLL делает `SPIPowerInstall.exe drill` (`install/spipower_install.cpp`,
пакет — `tools/make_drill.py` → `rescue/drill/SwDnl_drill.upg`: один файл `\navigation\app\VestaDrill.txt`):

1. `drill` кладёт на SD (том резолвится в рантайме: `\SDMemory` → `\SDMMC`) `SwDnl_drill.upg` +
   сгенерированный `SwDnl.cfg` (`SWDNL_DST_DIR` + `SWDNL_SRC_NV_FILE`, ANSI-токены в кавычках — как в
   заводском файле) и пишет в `\ResidentFlash\SwUpdate.flag` один байт `0x02`. Отказы: нет SD; на SD
   лежит `update.pkg`/`RdDnl.upg`/`CANUpdate.s19` (пути OS/Radio/CAN); в пакете нет маркера `VestaDrill`.
   Маркер ищется **побайтово** (`memcmp`), а не `strstr`: `.upg` бинарный, первый `0x00` — в заголовке
   записи `S2` (смещение 257), а `VestaDrill` — в записи `S4` (смещение 314), так что поиск подстроки в
   стиле C-строки ложно отвергал корректный пакет (исправлено 2026-10-11).
2. Ребут: `Main.exe` (флаг есть и `(b & 0x0F) != 0`) пропускает `G3NavHMI` и поднимает `SwDnl.exe` —
   тот снимает бит `0x2` (`CompleteUpdate(2)`) и удаляет файл, когда маска обнулится.
3. Вердикт: `SPIPowerInstall.exe status` → `swflag:none drill:staged marker:y` = PASS (маркер появился
   в `\ResidentFlash\navigation\app\`); `drill-off` снимает флаг, убирает drill-файлы с SD и печатает
   итог. Если флаг остался `0x02` — апдейтер не стартовал: это и есть ответ на открытый вопрос
   «автостарт или кнопка `Update Start!` в `CAutoUpdateDlg`».
