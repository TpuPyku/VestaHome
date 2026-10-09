# rescue/ — аварийный откат SPIPowerMgr.dll через загрузочный обновитель

Восстанавливает заводской `\ResidentFlash\navigation\app\SPIPowerMgr.dll`, если proxy
«окирпичил» ГУ и тот ушёл в циклическую перезагрузку (не даёт запустить `SPIPowerInstall.exe rollback`).

## Состав
- `SwDnl.upg` (111 КБ) — минимальный пакет: **один файл** `SPIPowerMgr.dll` (заводские байты,
  md5 `438505b76467ee393156f1aaacb9e3ab`) + его исходная контрольная сумма из заводской прошивки.
- `SwDnl.cfg` — минимальный конфиг: цель `ResidentFlash`, источник `SDMemory\SwDnl.upg`,
  **без** директив удаления (`SWDNL_NAV_DEL_*`) — только перезапись одного DLL, `navigation\app` не трогается.

## Сначала drill: проверить путь без риска для DLL
Тот же путь применяется безобидным маркером (`\navigation\app\VestaDrill.txt`) — так проверяются том
`\SDMemory`, парсинг cfg, флаг `0x02` и распаковка в `\ResidentFlash` до того, как на ГУ попадёт
хоть один байт вместо `SPIPowerMgr.dll`:

1. на PC: `python tools\make_drill.py rescue\drill` → `SwDnl_drill.upg` + `SwDnl.cfg` +
   `SPIPowerInstall.exe`;
2. скопировать папку на SD, запустить `SPIPowerInstall.exe drill` — файлы на SD и флаг `0x02` кладёт
   сама программа (вручную ничего создавать не нужно);
3. выключить/включить ГУ (SD оставить), затем `SPIPowerInstall.exe status` → PASS = маркер
   `\ResidentFlash\navigation\app\VestaDrill.txt` есть, `swflag:none`; то же отвечает на вопрос,
   стартует апдейтер сам или ждёт кнопку `Update Start!`;
4. `SPIPowerInstall.exe drill-off` — снять флаг и убрать drill-файлы с SD.

## Как пользоваться (выверено по коду `SwDnl.exe`, разбор — `../SWDNL.md`)

1. На SD-карту в корень (`\SDMemory\`) положить **только два файла** — `SwDnl.upg` и `SwDnl.cfg`.
   `update.pkg`, `RdDnl.upg`, `CANUpdate.s19` **не класть**: по ним апдейтер захочет обновлять
   OS/Radio/CAN (для OS вызывается `SV_SwDnlUpdateImage` — это опасно).
2. Триггер — `\ResidentFlash\SwUpdate.flag`, **один байт `0x02`** (бит Navi). Если ГУ циклически
   перезагружается из-за прокси, `Main.exe` сам пишет `0x02` после третьего провала запуска
   `G3NavHMI` и поднимает `SwDnl.exe` с SD (то есть откат может пройти уже с первой загрузки);
   при живой оболочке флаг создаётся вручную (`SPIPowerInstall.exe drill`, раздел выше).
3. Вставить SD, включить ГУ. `SwDnl.exe` стартует **до** оболочки, берёт пакет по ключу
   `SWDNL_SRC_NV_FILE` и распаковывает его в `SWDNL_DST_DIR` (`ResidentFlash`), т.е. в
   `\ResidentFlash\navigation\app\SPIPowerMgr.dll`. Если появится диалог — нажать
   `Update Start!`/OK.
4. Успех: бит `0x2` в `SwUpdate.flag` снят (файл удаляется, когда маска станет 0), в отладочном
   логе `NKDbgPrintfW` есть `SWDNL: Copy … completed!`, `SWDNL: Exract … Successed!`.
   Затем SD извлечь, чтобы не срабатывало повторно.

## Как собрано
```
python tools\make_rescue.py <SwDnl.upg> <outdir>
```
Парсер/сборщик: `tools\upg_parse.py` (список файлов), `tools\upg_build.py` (пересборка `.upg`).
Формат `.upg` разобран и **проверен побайтово**: пересборка заводского `SwDnl.upg` (53 МБ, 2023 файла)
совпадает с оригиналом `identical=True`.

## Ограничения / что проверено кодом, а что нет

**Проверено разбором `SwDnl.exe`** (см. `SWDNL.md`):
- триггер = байт `\ResidentFlash\SwUpdate.flag`, маска типов: `1` Radio, `2` Navi (`SwDnl.upg`),
  `4` CAN, `8` OS; `0x02` пишет сам `Main.exe` при провале запуска `G3NavHMI`, после чего
  поднимает `SwDnl.exe` с SD;
- источник пакета для Navi = ключ `SWDNL_SRC_NV_FILE` второго cfg, а не что-то ещё;
- второй cfg читается **жёстко** из `\SDMemory\SwDnl.cfg` (ключ `SWDNL_RD_DATA_LEN` парсер не
  читает; `SWDNL_NAV_DEL_*` необязательны — без них ничего не удаляется);
- валидность = `SRC_NV_FILE` или `SRC_RD_FILE` непустые **плюс** наличие файла на SD;
- распаковка идёт в `SWDNL_DST_DIR` (`ResidentFlash`), пути внутри `.upg` начинаются с `\`;
- том SD монтируется как `\SDMemory` (профиль StorageManager `SDMemory` в реестре ROM).

**Не проверено на железе:**
- стартует ли распаковка автоматически или после нажатия `Update Start!` в диалоге
  `CAutoUpdateDlg` (в UI есть строки `Press 'OK' to start upgrade.`, `Update Start!`);
- успевает ли `Main.exe` записать `0x02` до сброса по `SPApi_SendResetEvent(1)` (сброс
  запрашивается за 3 с до записи флага) — то есть сработает ли откат с **первой** загрузки;
- `SV_SwDnlUpdateImage` с однобайтовым `update.pkg` (в `MAIN/4120/update.pkg` лежит `0x00`) —
  **на SD `update.pkg` не класть**, только `SwDnl.upg` + `SwDnl.cfg`;
- пошаговый прогон на ГУ (см. `../PLAN.md`, шаг D).
