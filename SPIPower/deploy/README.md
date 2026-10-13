# SPIPower — набор для проверки и установки proxy SPIPowerMgr.dll на ГУ

Лежит в `Tests\` (папки по томам ГУ). Собран на PC скриптом `VestaHome\SPIPower\tools\make_deploy.py`
(его же вызывает `VestaHome\tools\make_tests.py`) из готовых артефактов (`VestaHome\SPIPower\out\`, заводских
DLL прошивки 4120, `VestaHome\SPIPower\rescue\`). Источник этого текста — `VestaHome\SPIPower\deploy\README.md`.
Пересобрать после любой пересборки proxy / теста / установщика:

```
python VestaHome\SPIPower\tools\make_deploy.py     :: по умолчанию пишет в <repo>\Tests
```

Размеры и md5 всех файлов — в `ResidentFlash\SPIPower\FILES.md` (генерируется вместе с набором и сверяется с
`install/fingerprints.h`; при расхождении сборка набора падает). Ключевые значения:

| Артефакт | Размер | MD5 (первые 8) |
|---|---:|---|
| proxy `SPIPowerMgr.dll` | 22 016 | `fa7b65e0…` |
| заводская `SPIPowerMgr.dll` | 111 104 | `438505b7…` |
| `SPIPowerInstall.exe` | 37 376 | `4e507e88…` |
| `SwDnl_drill.upg` | 357 | `8800cb0e…` |

## Иерархия = монтирования ГУ

Папки копируются **как есть**, структура повторяет тома ГУ:

```
Tests\
  SDMMC\SPIPower_test\       ->  \SDMMC\SPIPower_test\       Уровень 0: тест proxy до свопа
  ResidentFlash\SPIPower\    ->  \ResidentFlash\SPIPower\    установщик + proxy + drill-пакет + эта инструкция
  SDMemory\SPIPower_rescue\  ->  КОРЕНЬ SD-карты             rescue-набор (класть ПЕРЕД install)
  SDMemory\SafeMode\         ->  КОРЕНЬ SD-карты             безопасный режим (только при «кирпиче»)
```

`\SDMMC` (внутренняя eMMC) и `\ResidentFlash` (раздел прошивки, там живёт оболочка) — внутри ГУ;
`\SDMemory` — **сама SD-карта**, её надо вынуть и записать с PC. Заводская оболочка лежит в
`\ResidentFlash\navigation\app\`, там же подменяемый `SPIPowerMgr.dll`.

---

## A. Уровень 0 — проверка proxy до свопа (ничего не меняет, безопасно)

1. Скопировать папку `SDMMC\SPIPower_test\` на ГУ целиком, чтобы получилось
   `\SDMMC\SPIPower_test\` (5 файлов: proxy, `SPIPowerMgr_orig.dll`, `NCL_BaseCom.dll`,
   `desay_sdk_api.dll`, `SPIPowerTest.exe`).
2. Запустить `\SDMMC\SPIPower_test\SPIPowerTest.exe`.
3. Успех = `exit code 0` и в MessageBox / в `\SDMMC\SPIPower_test\SPIPower_test.log`:

```
  check \SDMMC\SPIPower_test\proxy=yes (fa7b65e0… 22016 B) orig=yes (438505b7… 111104 B)
  exports resolved 139/139, missing 0
  RESULT: OK
```

Папка **обязана** содержать `SPIPowerMgr_orig.dll`: proxy без него намеренно возвращает FALSE из
`DllMain` (защита от «кирпича»), и тест откажет — именно это случилось 2026-10-10, когда папку
на ГУ собрали «на глаз». Строка `proxy=yes` без md5 не отличает заводскую DLL от нашей, поэтому
в v3 печатаются размер и md5; при провале в логе будут `LoadLibrary(proxy): err=N`,
`orig: MISSING (DllMain will fail)` или список `MISSING: <имя>`.

## B. Drill — тест аварийного восстановления, без риска для DLL

Проверяет весь путь rescue (том `\SDMemory`, парсинг cfg, флаг `0x02`, распаковка в
`\ResidentFlash`) безобидным маркером `VestaDrill.txt`, пока на ГУ не попадёт ни один байт
вместо `SPIPowerMgr.dll`.

1. Скопировать `ResidentFlash\SPIPower\` на ГУ целиком → `\ResidentFlash\SPIPower\`
   (`SPIPowerInstall.exe` + `SPIPowerMgr.dll` + `SwDnl_drill.upg`). Пакет должен лежать
   **рядом с exe**: программа берёт его и proxy из своей папки, а не с SD.
2. Запустить `SPIPowerInstall.exe drill` (или кнопка «ТЕСТ ВОССТ.», с подтверждением).
   Файлы на SD и флаг кладёт сама программа — вручную ничего создавать не нужно: в корне
   SD-карты появятся `SwDnl_drill.upg` + `SwDnl.cfg`, в `\ResidentFlash\SwUpdate.flag` — байт `0x02`.
3. Перезагрузить ГУ, **оставив SD-карту вставленной**. Апдейтер может стартовать сам или
   спросить диалогом — если появится `Update Start!`/OK, нажать.
4. `SPIPowerInstall.exe status` (кнопка «СТАТУС»). PASS = появился
   `\ResidentFlash\navigation\app\VestaDrill.txt` и в `status` видно `swflag:none`
   (флаг снят самим апдейтером).
5. `SPIPowerInstall.exe drill-off` (кнопка «СНЯТЬ ТЕСТ») — снять флаг и убрать drill-файлы с SD.

## C. Rescue-набор на SD — обязательная страховка ПЕРЕД установкой

1. Скопировать `SDMemory\SPIPower_rescue\SwDnl.upg` и `SwDnl.cfg` в **корень SD-карты** (`\SDMemory\`).
   Больше на карту ничего не класть.
2. **Не класть** на SD `update.pkg`, `RdDnl.upg`, `CANUpdate.s19`: по ним апдейтер захочет
   обновлять OS/Radio/CAN (для OS вызывается `SV_SwDnlUpdateImage` — это опасно).
3. Внимание: `drill-off` удаляет `\SDMemory\SwDnl.cfg` (он же cfg для drill). Поэтому rescue-пару
   копировать **после** `drill-off`; если drill запускался ещё раз — копировать заново.
4. Проверить, что файлов ровно два и что в `SwDnl.cfg` источник — `SWDNL_SRC_NV_FILE "SDMemory\SwDnl.upg"`,
   цель — `SWDNL_DST_DIR ResidentFlash`. Флаг `\ResidentFlash\SwUpdate.flag` должен **отсутствовать**.

## D. Установка proxy

1. `SPIPowerInstall.exe status` — убедиться, что гейт версии зелёный: строка `fw:4120 … HARD … ok`
   и `самопроверка MD5: ok`. Отказ гейта («не из прошивки 4120») — стоп, разбираться по строкам
   отчёта; `force` не использовать.
2. `SPIPowerInstall.exe install` (кнопка «УСТАНОВИТЬ», с подтверждением). Что делает: бэкап
   заводской DLL в 2 места → `\ResidentFlash\navigation\app\SPIPowerMgr_orig.dll` и
   `\SDMemory\SPIPower_backup\SPIPowerMgr.dll` (на SD, если она смонтирована, иначе `\SDMMC\`),
   `WM_CLOSE` окну оболочки «RadioCommWindow» (ждёт 5 с), затем
   kill **всех** держателей DLL (оболочка + `\ResidentFlash\Main.exe` + `AppLauncher.exe` —
   `Main.exe` тоже `LoadLibrary`-ит `SPIPowerMgr.dll`, без его остановки файл не освобождается),
   ожидание реальной смерти процессов и повторные попытки записи, подмена, сверка md5 **файла на
   месте** и **запрос сброса МКУ** (`SPApi_SendResetEvent`) — ГУ перезагружается, оболочка
   поднимается уже с proxy. Любой провал **до** подмены поднимает оболочку обратно.
   При «всё ещё занят (err=32)» прислать `\SDMMC\SPIPower_install.log`: строки
   `close: …` и `kill <exe> pid=… threads=…: open …, terminate … err=…` покажут, вышла ли оболочка
   сама, сработал ли `TerminateProcess` и не поднялся ли процесс заново (новый pid).
   Вариант с логом proxy: `install p2` / `install p3` (кнопка «УСТ. + ЛОГ») — включают флаг
   `\SDMMC\SPIPower.on` и лог `\SDMMC\SPIPower.log`.
3. Признак успеха — ГУ **сам перезагрузился** сразу после `install` (в отчёте `сброс: запрошен`);
   если сброс не прошёл, в отчёте будет `сброс: не подтверждён` и ГУ перезагрузится по таймауту
   МКУ (~1 мин). После загрузки: `SPIPowerInstall.exe status` → `dll` = proxy (22 016 Б,
   `fa7b65e0…`), оболочка жива, в `\SDMMC\SPIPower.log` есть `PROXY OK`.
4. Ручной откат в любой момент: `SPIPowerInstall.exe rollback` (кнопка «УДАЛИТЬ») — kill тех же
   держателей, возврат заводской DLL, старт оболочки (обязательно **без** перезагрузки).

## Если ГУ «окирпичился» (оболочка не стартует, циклическая перезагрузка)

**Сначала — безопасный режим (папка `SDMemory\SafeMode\`).** Оба файла из неё положить в **корень внешней
SD-карты** (на ГУ это `\SDMemory\`; не `\SDMMC` — это встроенная карта, и не флешка `\USBDevice`:
`Main.exe` ищет файлы только по жёсткому пути `\SDMemory\…`): `bACKuPdESAYsV201200070012.Mainflg` и `Cancle60s.exe`. Заводской `Main.exe` после
запуска оболочки, увидев этот файл-флаг, запускает `\SDMemory\Cancle60s.exe` и `\windows\Explorer.exe`
(рабочий стол CE). Наш `Cancle60s.exe` (VestaRescue) заводских файлов не трогает: пишет
`\SDMemory\VestaRescue\rescue.log` (процессы, флаги загрузки, `ExternNavi.cfg`, md5 оболочки и DLL, часы,
память, самые новые файлы на `\ResidentFlash`/`\StorageFlash`/`\SDMMC` + копии мелких в `files\`),
при запуске из загрузки (первые 3 мин) останавливает и отключает наши программы (VestaHome, VestaLaunch, Navit, GstProbe: `.exe` →
`.exe.vrescue`; вернуть — переименовать обратно) и показывает итог. Журнал пишется построчно, так что
переживёт и сброс ГУ. После разбора оба файла с SD убрать, иначе это повторяется на каждой загрузке.

Заводской откат DLL (ниже) срабатывает **только** если `Main.exe` не смог запустить `G3NavHMI.exe`
(файла нет / не грузится); если оболочка запускается и виснет — не сработает. И он нужен, только если
DLL действительно подменена (`status`: `dll` = proxy 22 016 Б).

1. Оставить вставленной SD с rescue-парой из шага C. При повторном провале запуска `G3NavHMI`
   `Main.exe` сам пишет байт `0x02` в `\ResidentFlash\SwUpdate.flag` и поднимает `SwDnl.exe` с SD,
   который вернёт заводскую DLL в `\ResidentFlash\navigation\app\SPIPowerMgr.dll`.
2. Если появится диалог — нажать `Update Start!`/OK. Успех в отладочном логе: `SWDNL: Copy … completed!`,
   `SWDNL: Exract … Successed!`, флаг `0x02` снят.
3. SD затем извлечь, чтобы откат не срабатывал повторно. Детали — `VestaHome\SPIPower\rescue\README.md`.

## Чего не делать

- Не смешивать сборки: папка из разных версий (например, старый `SPIPowerInstall.exe` + новый
  proxy) ловится самопроверкой md5 — при отказе пересобрать набор (`python tools\make_deploy.py`)
  и скопировать заново.
- Не копировать `\SDMMC\` и `ResidentFlash\` на SD-карту как есть: это разные тома ГУ.
- Не запускать `install`, не выполнив A (Уровень 0) и B (drill).

## Источники (для деталей)

- `VestaHome\SPIPower\README.md` — CLI и кнопки, что печатает `status`, уровни логов.
- `VestaHome\SPIPower\rescue\README.md` — rescue-`.upg`, триггер `SwUpdate.flag`, ограничения.
- `VestaHome\SPIPower\SWDNL.md` — разбор заводского `SwDnl.exe`; `VestaHome\SPIPower\MAIN.md` — окно отката.
- `PLAN.md` / `PROJECT.md` — план и факты по прошивке 4120.
