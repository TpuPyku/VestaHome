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
| proxy `SPIPowerMgr.dll` | 22 528 | `0e0a1169…` |
| заводская `SPIPowerMgr.dll` | 111 104 | `438505b7…` |
| `SPIPowerInstall.exe` | 50 176 | `b220bc8b…` |
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
  check \SDMMC\SPIPower_test\proxy=yes (0e0a1169… 22528 B) orig=yes (438505b7… 111104 B)
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
5. `SPIPowerInstall.exe drill-off` (кнопка «СНЯТЬ ФЛАГ») — снять флаг и убрать drill-файлы с SD.

## C. Rescue-набор на SD — обязательная страховка ПЕРЕД установкой

1. Скопировать `SDMemory\SPIPower_rescue\SwDnl.upg` и `SwDnl.cfg` в **корень SD-карты** (`\SDMemory\`).
   Больше на карту ничего не класть.
2. **Не класть** на SD `update.pkg`, `RdDnl.upg`, `CANUpdate.s19`: по ним апдейтер захочет
   обновлять OS/Radio/CAN (для OS вызывается `SV_SwDnlUpdateImage` — это опасно).
3. Внимание: `drill-off` удаляет `\SDMemory\SwDnl.cfg` (он же cfg для drill). Поэтому rescue-пару
   копировать **после** `drill-off`; если drill запускался ещё раз — копировать заново.
4. Проверить, что файлов ровно два и что в `SwDnl.cfg` источник — `SWDNL_SRC_NV_FILE "SDMemory\SwDnl.upg"`,
   цель — `SWDNL_DST_DIR ResidentFlash`. Флаг `\ResidentFlash\SwUpdate.flag` должен **отсутствовать**.

## D0. Обновление proxy (2026-10-15)

Новый proxy (22 528 Б) складывает каждое сообщение MCU в общую память `VestaMCU` — её читает VestaHome
(«Настройки → Автомобиль → Данные MCU», климат, «О системе → Подробнее»). Старый (22 016 Б, `fa7b65e0…`) этого
не умеет. Обновление — так же, как установка: новый `ResidentFlash\SPIPower\` → `\ResidentFlash\SPIPower\`,
«СТАТУС» (в отчёте: «стоит прежний proxy — установка его обновит», `SPIPowerMgr_orig.dll` = заводская ok) →
«УСТ. + ЛОГ» → перезагрузка с SD (шаг D ниже). Удалять старый не нужно.

Журнал proxy — `\SDMMC\SPIPower.log` (рядом с `SPIPower.on`, пишется сам, ничего запускать не надо; «СТАТУС» печатает
его размер). Обёртка обработчиков MCU теперь работает всегда (общая память), файл — только при `SPIPower.on`.

## D. Установка proxy — при загрузке (boot)

Живая установка (`install` из CLI) не работает: запущенную `G3NavHMI` не убить (`TerminateProcess`
err=170), а ГУ сбрасывается посреди ожидания (2026-10-13). Заводской апдейтер с `.upg`-пакетом тоже не
годится: `SwDnl.exe` не останавливает `Main.exe`, а тот держит `SPIPowerMgr.dll` с самого начала
загрузки. Поэтому кнопки «УСТАНОВИТЬ» / «УСТ. + ЛОГ» / «УДАЛИТЬ» теперь только **заряжают** операцию,
а выполняет её сам установщик при следующей загрузке — `Main.exe` при заряженном флаге запускает
`\SDMemory\SwDnl.exe` **вместо** оболочки, и этим `SwDnl.exe` будет наш установщик.

1. SD-карта вставлена (нужен именно `\SDMemory`), в корне нет чужого `SwDnl.exe`, `update.pkg`,
   `RdDnl.upg`, `CANUpdate.s19`.
2. `SPIPowerInstall.exe status` — гейт версии зелёный: строка `fw:4120 … HARD … ok` и
   `самопроверка MD5: ok`. Отказ гейта — стоп; `force` не использовать.
3. Кнопка «УСТ. + ЛОГ» (CLI `boot install p2`; без лога proxy — «УСТАНОВИТЬ» / `boot install`).
   Программа кладёт на SD: `\SDMemory\SwDnl.exe` (копия самой себя), `\SDMemory\SPIPower_boot\SPIPowerMgr.dll`
   (proxy, md5 сверен) и `\SDMemory\SPIPower_boot\job.txt` (задание), пишет `\ResidentFlash\SwUpdate.flag` = `0x02`.
   В `status` видно `swflag:0x02 boot:armed`. Передумали — «СНЯТЬ ФЛАГ» (`boot-off`).
4. Перезагрузить ГУ, **не вынимая SD**. Вместо оболочки стартует наш `SwDnl.exe` (экран может
   несколько секунд показывать только логотип) и делает по порядку:
   - сразу **удаляет флаг и задание** — что бы ни случилось дальше, следующая загрузка обычная;
   - повторяет гейт версии и md5 proxy;
   - останавливает `Main.exe` (+ `AppLauncher.exe`; оболочки ещё нет), ждёт освобождения файла
     (при необходимости — и `HzWD.exe`/`dr_Service.exe`);
   - бэкап заводской DLL → `SPIPowerMgr_orig.dll` + `\SDMemory\SPIPower_backup\`, подмена, md5 файла на месте;
   - флаг лога `\SDMMC\SPIPower.on` (для «УСТ. + ЛОГ») и **сброс МКУ** → ГУ перезагружается уже с proxy.
     Если сброс не прошёл — сам запускает `Main.exe`, и оболочка поднимается с proxy без перезагрузки.
   Любой провал до подмены (файл занят, гейт, md5) — ничего не меняется, оболочка запускается как обычно.
5. Проверка: `status` → `dll` = proxy (22 528 Б, `0e0a1169…`), `swflag:none`, оболочка жива,
   в `\SDMMC\SPIPower.log` есть `PROXY OK`. Журнал загрузочного этапа — в двух местах:
   `\SDMMC\SPIPower_install.log` и `\SDMemory\SPIPower_boot\boot.log` (строки `=== boot: …`,
   `kill …`, `install: …`).
6. Откат: кнопка «УДАЛИТЬ» (`boot rollback`) — так же через загрузку: заряжает задание, после
   перезагрузки наш `SwDnl.exe` останавливает `Main.exe`, возвращает `SPIPowerMgr_orig.dll` на место
   и запускает `Main.exe` (оболочка поднимается без ещё одной перезагрузки). CLI `install` / `rollback`
   (живой своп) оставлены только для экспериментов.

`\SDMemory\SwDnl.exe` после установки **лучше оставить на SD** — это страховка: если оболочка с proxy
не запустится, `Main.exe` после трёх провалов сам зарядит флаг и запустит его, а он без задания увидит
proxy на месте и вернёт заводскую DLL. Если же флаг зарядил кто-то другой (proxy не стоит), он
передаёт управление заводскому `SwDnl.exe` (`\ResidentFlash`, затем `\Windows`), и rescue-пара из
шага C работает как раньше.

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
DLL действительно подменена (`status`: `dll` = proxy 22 528 Б).

1. Оставить вставленной SD с нашим `SwDnl.exe` (шаг D) и rescue-парой из шага C. При повторном провале
   запуска `G3NavHMI` `Main.exe` сам пишет байт `0x02` в `\ResidentFlash\SwUpdate.flag` и поднимает
   `SwDnl.exe` с SD: наш вернёт `SPIPowerMgr_orig.dll` сам (журнал — `\SDMemory\SPIPower_boot\boot.log`);
   без нашего заводской распакует rescue-пару в `\ResidentFlash\navigation\app\SPIPowerMgr.dll`.
2. Если появится диалог — нажать `Update Start!`/OK. Успех в отладочном логе: `SWDNL: Copy … completed!`,
   `SWDNL: Exract … Successed!`, флаг `0x02` снят.
3. SD затем извлечь, чтобы откат не срабатывал повторно. Детали — `VestaHome\SPIPower\rescue\README.md`.

## Чего не делать

- Не смешивать сборки: папка из разных версий (например, старый `SPIPowerInstall.exe` + новый
  proxy) ловится самопроверкой md5 — при отказе пересобрать набор (`python tools\make_deploy.py`)
  и скопировать заново.
- Не копировать `\SDMMC\` и `ResidentFlash\` на SD-карту как есть: это разные тома ГУ.
- Не заряжать установку, не выполнив A (Уровень 0) и B (drill).
- Не класть на SD чужой `SwDnl.exe` поверх нашего, пока proxy установлен.

## Источники (для деталей)

- `VestaHome\SPIPower\README.md` — CLI и кнопки, что печатает `status`, уровни логов.
- `VestaHome\SPIPower\rescue\README.md` — rescue-`.upg`, триггер `SwUpdate.flag`, ограничения.
- `VestaHome\SPIPower\SWDNL.md` — разбор заводского `SwDnl.exe`; `VestaHome\SPIPower\MAIN.md` — окно отката.
- `PLAN.md` / `PROJECT.md` — план и факты по прошивке 4120.
