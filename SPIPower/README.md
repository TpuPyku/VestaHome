# SPIPower — proxy для `SPIPowerMgr.dll` (Desay SDK, MMC Lada Vesta)

Свой перехватчик общения G3NavHMI ↔ MCU вместо платной прошивки «CAN панель».

`G3NavHMI.exe` грузит `\ResidentFlash\navigation\app\SPIPowerMgr.dll` (139 экспортов,
Desay SDK «Prima II», ARMV4I) и через неё шлёт/принимает сообщения MCU (яркость
`0x4107`, день/ночь `0x4105`, кнопки). Наш proxy заменяет эту DLL и прозрачно
пробрасывает все вызовы в оригинал (переименованный `SPIPowerMgr_orig.dll`).

## Устройство
- `tools/pe_exports.py` — дамп таблицы экспорта PE (имя/ординал/RVA).
- `tools/pe_strings.py` — строки (ASCII + UTF-16LE) с офсетами: в CE-бинару wide и narrow строки
  смешаны, поэтому обычный поиск по путям/ключам в ASCII ничего не находит.
- `tools/ghidra_post.bat` — запуск Ghidra-скриптов над программой проекта `ghidra/`
  (`PROGRAM=` выбирает программу, по умолчанию `SwDnl.exe`).
- `ghidra_scripts/` — скрипты разбора: `StrRefDecomp` (поиск строк в обеих кодировках + xrefs +
  декомпиляция), `Callers` (вызывающие, включая ссылки из таблиц), `DecompAt`, `DumpW`.
- `MAIN.md` — разбор `Main.exe` (триггер сброса/отката, окно ≈6 с); `SWDNL.md` — разбор заводского
  апдейтера `SwDnl.exe` (байт `SwUpdate.flag`, ключи `SwDnl.cfg`, распаковка `.upg`).
- `tools/make_test_dir.py` — собирает папку Уровня 0: `SPIPowerMgr.dll` (proxy) +
  `SPIPowerMgr_orig.dll` (заводская) + `NCL_BaseCom.dll` + `desay_sdk_api.dll` + `SPIPowerTest.exe`.
- `test/` — `SPIPowerTest.exe` (ARM, импорт COREDLL): без G3NavHMI грузит proxy, проверяет
  `DllMain` (ищет `_orig` рядом с собой) и все 139 экспортов; лог — `\SDMMC` или
  `\StaticStore\SPIPower_test.log`, иначе рядом с самим exe.
- `install/spipower_install.cpp` + `install/build.bat` — `SPIPowerInstall.exe` (ARM, импорт COREDLL):
  `status | install [p2|p3] [force] | rollback | drill [force] | drill-off`; без аргументов — окно
  с теми же действиями кнопками (раздел «Установщик» ниже).
  Исходник — UTF-8 **с BOM**: без BOM MSVC 2005 читает его как cp1251 и все русские строки в exe
  получаются двойной перекодировкой (`РћС‚РєР°Р·` вместо `Отказ`).
- `install/md5.h` — MD5 (RFC 1321) одним файлом, без `windows.h`; тот же код собирается и для ПК
  (`tools/md5_hosttest.bat` — векторы + хеши файлов, сверка с `hashlib`), и в ARM-сборку установщика.
- `install/fingerprints.h` — **генерируемая** таблица эталонов прошивки (HARD/SOFT/INFO) для гейта
  версии. `tools/gen_fingerprints.py` берёт байты из официального пакета `MAIN/4120/SwDnl.upg`
  (TOC `0x03276D3F`, то же значение, что в `make_rescue.py`) и из образа `MAIN/4120_unpacked/NK`,
  а также md5 наших артефактов (proxy рядом с exe, drill-пакет).
- `tools/make_drill.py` — drill-пакет для проверки загрузочного rescue-пути **до** подмены DLL:
  `rescue/drill/SwDnl_drill.upg` (один файл `\navigation\app\VestaDrill.txt`) + `SwDnl.cfg` +
  копия `SPIPowerInstall.exe` рядом с ними.
- `native/gen_thunks.py <real_dll>` — генерит из оригинала:
  - `thunks.s` — по одному ARM-trampoline на экспорт (`ldr ip,[pc,#off]; ldr ip,[ip]; mov pc,ip`),
    tail-jump в таблицу указателей `g_real[]`; сигнатуры функций знать не нужно;
  - `spipower.def` — список экспорта (139 имён);
  - `g_proxy.h` — `PROXY_COUNT` + `kProxyNames[]`.
- `native/spipower_proxy.cpp` — `DllMain` грузит `SPIPowerMgr_orig.dll` и заполняет `g_real[]`.
- `native/build.bat` — armasm + cl + link (VS2005 CE ARM) → `out/SPIPowerMgr.dll`.

## Сборка
```
native\build.bat        # нужно, чтобы рядом был исходный DLL (см. путь в build.bat)
```
Результат: `out/SPIPowerMgr.dll` (импорт только COREDLL).

## Установка на ГУ (вручную, для теста)
1. В `\ResidentFlash\navigation\app\` переименовать `SPIPowerMgr.dll` → `SPIPowerMgr_orig.dll`.
2. Скопировать `out/SPIPowerMgr.dll` как `\ResidentFlash\navigation\app\SPIPowerMgr.dll`.
3. Откат: скопировать `SPIPowerMgr_orig.dll` обратно.

## Статус
- P1 (прозрачный proxy) собран и проверен статически: 139 экспортов идентичны оригиналу,
  импорт только COREDLL.
- P2 (лог исходящих) готов: 7 функций (`EngSendTFTBrightnessData`, `EngSendCarSpeed`,
  `SPApi_SendKeyEvent/ExtendKeyEvent/RotaryEvent`, `SPApi_SetSkelonton`, `SPApi_LoadByMain`)
  логируют аргументы в `\SDMMC\SPIPower.log`, если есть флаг `\SDMMC\SPIPower.on`.
- P3 (перехват входящих) готов: `SPApi_SetSkelonton` подменяет vtable каждого скелетона
  (slot 1 → хук, логирует `msg`/`len`/payload и форвардит), только при флаге.
- На ГУ **не запускался**. Уровень 0 на эмуляторе **пройден** (2026-10-09): `RESULT: OK`,
  `exports resolved 139/139`, `orig: present`, обе зависимости подгружены.
- Заводская `SPIPowerMgr.dll` импортирует `desay_sdk_api.dll` (ПЗУ, `\Windows`) и `NCL_BaseCom.dll`
  (`navigation\app`); proxy — только COREDLL. Поэтому для Уровня 0 обе DLL кладутся рядом с
  `SPIPowerMgr_orig.dll` (их копирует `tools/make_test_dir.py`).
- Эмулятор (2026-10-09): `dlldiag.txt` снят **в эмуляторе** и показывает `SPIPowerMgr.dll OK err=0`,
  `desay_sdk_api.dll OK`, `NCL_BaseCom.dll OK` → Уровень 0 там выполним (G3NavHMI там падает).

## Уровень 0 — проверка proxy до свопа (без Г3NavHMI)
```
test\build.bat                  # пересобрать SPIPowerTest.exe (ARM)
python tools\make_test_dir.py   # папка: proxy + orig + зависимости + тест
```
Запустить `SPIPowerTest.exe` из этой папки: на ГУ — `\SDMMC\SPIPower_test\`, в эмуляторе —
`\StaticStore\SPIPower_test\`. Тест сам ищет proxy (`\SDMMC` → `\StaticStore` → своя папка → app),
пришпиливает зависимости заводской DLL полным путём и пишет лог в первое доступное место.

Ожидаемо: `RESULT: OK`, `exports resolved 139/139, missing 0`. При провале в логе будет
`LoadLibrary(proxy): err=N`, `orig: MISSING (DllMain will fail)` или список `MISSING: <имя>`.
Выход 0 = OK, 1 = FAIL (то же в MessageBox).

## Установщик (ГУ): `SPIPowerInstall.exe`
```
install\build.bat                       # → out\SPIPowerInstall.exe
```
Запускать можно из любой папки: proxy берётся **рядом с exe**, поэтому всю папку можно положить и на
SD, и на `\SDMMC`. Подкоманды:

| Команда | Действие |
|---|---|
| `status` | состояние: `dll/orig/sd-bak/flag/shell` + `sd/swflag/drill/marker` |
| `install` | бэкап заводской DLL в 2 места, подмена на proxy, старт оболочки, watchdog 30 с, автооткат |
| `install p2` | то же + флаг `\SDMMC\SPIPower.on` (лог исходящих) |
| `install p3` | то же с флагом (лог входящих через vtable скелетона) |
| `install force` | то же, но **обойти** гейт версии прошивки (только осознанно, см. ниже) |
| `rollback` | kill оболочки, возврат заводской DLL, снять флаг, старт |
| `drill` | зарядить заводской апдейтер `SwDnl.exe` безобидным маркером (см. ниже) |
| `drill-off` | снять `\ResidentFlash\SwUpdate.flag`, убрать drill-файлы с SD, выдать вердикт |

`install*` отказывается работать, пока заряжен `\ResidentFlash\SwUpdate.flag` (иначе после следующей
загрузки над работающим proxy отработал бы загрузочный апдейтер из того, что лежит на SD).

### Окно (запуск без аргументов)
`SPIPowerInstall.exe` без параметров открывает полноэкранное окно 800×480 (чистый GDI: без `.rc`,
`comctl32` и MFC, импорт остаётся только COREDLL) — те же действия кнопками, удобно на ГУ:

| Кнопка | Действие |
|---|---|
| `СТАТУС` | обновить отчёт в окне и в логе (то же, что `status`) |
| `УСТАНОВИТЬ` | `install` (pass-through), с подтверждением |
| `УСТ. + ЛОГ` | `install` с флагом `\SDMMC\SPIPower.on` (лог proxy: исходящие + входящие) |
| `ПРОГОН` | `drill`, с подтверждением (потребуется перезагрузка ГУ) |
| `СНЯТЬ ПРОГОН` | `drill-off` (снять флаг, убрать drill-файлы с SD, вердикт) |
| `ОТКАТ` | `rollback` (вернуть заводскую DLL, снять флаг, перезапустить оболочку) |
| `FORCE: ВЫКЛ/ВКЛ` | тумблер обхода гейта версии (красный = включён), передаётся в `install`/`drill` |
| `ВЫХОД` | закрыть окно (или `Esc`) |

Сверху окно показывает строку состояния и полный отчёт гейта версии (то же, что печатает `status`);
этот же текст всегда пишется в `\SDMMC\SPIPower_install.log`. При открытии окно сразу выполняет
`status`. Разрушающие кнопки (`УСТАНОВИТЬ`, `УСТ. + ЛОГ`, `ПРОГОН`) спрашивают подтверждение, а на
время операции ввод блокируется.

CLI-режим **сохранён** для тестов и автоматизации: известная подкоманда в командной строке →
прежний однократный `MessageBox`; аргументов нет → окно; неизвестное слово → подсказка (`Способ: …`).

### Проверка версии прошивки (гейт 4120)
Наш proxy собран под конкретные байты заводской DLL: в **4120** это `SPIPowerMgr.dll` =
111 104 Б / `438505b7…`. На других версиях файл **того же размера, но с другими байтами**
(например, копия в дереве CE-эмулятора `MAIN/WinCE_Emulator_StaticStore+SDMMC/.../SPIPowerMgr.dll`
= `2e1351…`; там же другой `NCL_BaseCom.dll`) — подмена такой DLL ломает оболочку. Поэтому
`install*` перед свопом считает MD5 файлов и сверяет их с таблицей `install/fingerprints.h`:

| Уровень | Файл | Поведение |
|---|---|---|
| `HARD` | `\ResidentFlash\navigation\app\SPIPowerMgr.dll` | не совпал/не читается → **install отказывает** |
| `SOFT` | `\ResidentFlash\Main.exe`, `…\app\NCL_BaseCom.dll`, `\Windows\SPIPowerMgr_OS.dll`, `\Windows\desay_sdk_api.dll` | не совпал → отказ, обойти можно только `install force` |
| `INFO` | `\ResidentFlash\SwDnl.exe`, `…\app\G3NavHMI.exe` | только показывается в `status` (наш `G3NavHMI` пропатчен `G3patch`, поэтому он и не может быть HARD) |

Отдельно проверяется **самопроверка MD5 на месте**: хешируются proxy рядом с exe и `SwDnl_drill.upg`
и сверяются с md5, записанными при генерации таблицы. Это одновременно (а) доказывает, что MD5-код
работает на этом ГУ, и (б) ловит папку установщика, собранную из разных сборок (proxy не от этого
`SPIPowerInstall.exe`) — в этом случае тоже отказ (код `4`).

Если `SPIPowerMgr.dll` уже равен нашему proxy — `install` скажет «сначала `rollback`» (отдельный
код `3`), а не «чужая прошивка».

```text
python tools\gen_fingerprints.py            # перегенерировать таблицу + отчёт (какие файлы где не совпали)
python tools\gen_fingerprints.py --check    # проверить, что fingerprints.h актуален
python tools\gen_fingerprints.py --simulate \ResidentFlash=..\..\MAIN\ResidentFlash   # вердикт гейта на ПК
tools\md5_hosttest.bat                      # MD5-ядро: векторы RFC 1321 (+ файлы для сверки с hashlib)
```
`--simulate` читает **сгенерированный** `fingerprints.h` и повторяет вердикт установщика на деревьях
файлов ПК: на дампе ГУ (`MAIN/ResidentFlash`, `MAIN/4120_unpacked/NK/fs/Windows`) выходит
`install would proceed`, на дереве эмулятора — `REFUSE` (HARD `SPIPowerMgr.dll`).

### Точки монтирования (ROM-реестр `MAIN/4120_unpacked/TINYNK/Registry/default.reg`)
| Путь | Что это |
|---|---|
| `\SDMemory` | SD-карта (`Profiles\SDMemory`, «SD Memory Card») — **сюда смотрят `SwDnl.exe` и `Main.exe`** |
| `\SDMMC` | внутренний eMMC (`Profiles\MMC`/`MMC2`, «EMMC Disk Drive») |
| `\SDMemory2` | второй том eMMC |

SD-тома в эмуляторе нет (общая папка — `\StaticStore`), поэтому `\SDMMC` там пуст. Пути proxy/теста
(`\SDMMC\SPIPower.on`, `\SDMMC\SPIPower.log`) пока остаются на eMMC — это контракт установщика и
proxy; перенос на `\SDMemory` делать только после `status` на живом ГУ (увидим, какие тома есть).

### Прогон (drill): проверка rescue-пути до подмены DLL

«Прогон» — это **учебный запуск аварийного пути восстановления**: боевую DLL мы не трогаем, а заряжаем
заводской загрузочный апдейтер `SwDnl.exe` **безобидным** пакетом (внутри один файл-маркер
`\navigation\app\VestaDrill.txt`) и убеждаемся, что rescue-цепочка действительно срабатывает с SD.
В окне это кнопки `ПРОГОН` и `СНЯТЬ ПРОГОН` (в командной строке — `drill` / `drill-off`).

```
python tools\make_drill.py rescue\drill   # SwDnl_drill.upg + SwDnl.cfg + SPIPowerInstall.exe
```
1. Скопировать папку `rescue\drill\` на SD и запустить `SPIPowerInstall.exe drill`: на SD
   раскладываются `SwDnl_drill.upg` + `SwDnl.cfg`, в `\ResidentFlash\SwUpdate.flag` пишется байт `0x02`.
   `drill` отказывается, если: нет SD, рядом с exe нет пакета, md5 пакета не равен эталону
   (`8800cb0e…`, 357 Б — таблица в `fingerprints.h`; обойти: `drill force`), в пакете нет маркера
   `VestaDrill`, на SD лежит `update.pkg`/`RdDnl.upg`/`CANUpdate.s19` (это пути прошивки OS/Radio/CAN).
   После изменения `make_drill.py` пакет нужно пересобрать **и** обновить таблицу:
   `python tools\make_drill.py rescue\drill && python tools\gen_fingerprints.py`.
2. Перезагрузить ГУ, **оставив SD**: `Main.exe` пропустит `G3NavHMI` и поднимет `SwDnl.exe`, тот
   распакует пакет в `\ResidentFlash` и снимет бит `0x2`.
3. `SPIPowerInstall.exe status` (или `drill-off`) даёт вердикт: PASS = маркер
   `\ResidentFlash\navigation\app\VestaDrill.txt` появился и `swflag:none`. Тот же прогон отвечает на
   вопрос «автостарт или диалог `Update Start!`» (если маркера нет, а флаг остался — апдейтер не
   стартовал сам).
4. `drill-off`: снимает флаг и удаляет drill-файлы с SD (пакет `rescue\drill\` на PC не трогается).

## Риск
Ошибка proxy → G3NavHMI падает → перезагрузки по кругу (см. `PROJECT.md`).
Перед пробами на ГУ держать рядом оригинал и проверить на эмуляторе.

Поведение ГУ при провале запуска G3NavHMI разобрано в `MAIN.md`: `Main.exe` делает
3 попытки, затем `SPApi_SendResetEvent(1)` (id MCU `0x5403`) и `SwUpdate.flag=0x02`
→ `SwDnl.exe`. `SPApi_SendResetEvent` вызывается **и в процессе Main.exe** (он тоже
`LoadLibrary`'ит эту DLL), так что её можно логировать/гасить. Аварийный откат:
файл `\ResidentFlash\SwUpdate.flag` из одного байта `0x02` заставляет Main пропустить
G3NavHMI и запустить `SwDnl`; снять откат — удалить файл.

Процедура rescue выверена по коду `SwDnl.exe` (разбор — `SWDNL.md`, инструкция —
`rescue/README.md`): на SD в `\SDMemory\` кладутся **только** `SwDnl.upg` + `SwDnl.cfg`
(пакет берётся по ключу `SWDNL_SRC_NV_FILE`, распаковка идёт в `SWDNL_DST_DIR` =
`\ResidentFlash\navigation\app\`), флаг `0x02` = бит Navi, при отсутствии `SWDNL_NAV_DEL_*`
ничего не удаляется. `update.pkg`, `RdDnl.upg`, `CANUpdate.s19` на SD не класть — это пути
прошивки OS/радио/CAN.

