# Tests — файлы для проверки на ГУ

Собирается скриптом `python VestaHome\tools\make_tests.py` из папок сборки репозиториев (сначала
`build.bat` нужного проекта). Вручную здесь ничего не править — перезапишется. ПК-версии (`*PC.exe`)
сюда не копируются: они остаются в папках сборки репозиториев.

Первая папка = том ГУ, куда копировать:

| Папка | Том на ГУ | Что это |
|---|---|---|
| `SDMemory\` | `\SDMemory` — внешняя SD-карта | **все эксперименты** |
| `SDMMC\` | `\SDMMC` — встроенная карта | только то, что запускает сама оболочка или что жёстко прописано на `\SDMMC` |
| `ResidentFlash\` | `\ResidentFlash` — встроенный раздел (дом оболочки) | только набор SPIPower |
| `StorageFlash\` | `\StorageFlash` — встроенная память | пока ничего |
| `USBDevice\` | `\USBDevice` — флешка в USB | пока ничего |

Обычная папка программы копируется **целиком** (`SDMemory\VestaHome\` → `\SDMemory\VestaHome\`).
Исключения — папки с файлами для **корня** карты, их **содержимое** кладётся в `\SDMemory\`:

- `SDMemory\SafeMode\` — безопасный режим (флаг `…Mainflg` + `Cancle60s.exe`): включается на каждой
  загрузке, пока файлы в корне, — после разбора убрать;
- `SDMemory\SPIPower_rescue\` — аварийный пакет `SwDnl.upg` + `SwDnl.cfg`, только перед установкой proxy.

Где что:

- `SDMemory\VestaHome`, `GstProbe`, `VestaInstall`, `LaunchInstall`, `navit` (+ `GpsGate 2.0` для теста GPS);
- `SDMMC\VestaLaunch` (на него указывает `ExternNavi.cfg`: без SD оболочка его не потеряет), `YaMapsCE`, `OSMapsCE`;
- `SDMMC\SPIPower_test`, `ResidentFlash\SPIPower` — набор SPIPower, инструкция в `ResidentFlash\SPIPower\README.md`.
