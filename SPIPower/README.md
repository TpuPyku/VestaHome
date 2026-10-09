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

