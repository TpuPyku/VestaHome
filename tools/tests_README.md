# Tests — файлы для проверки на ГУ

Собирается скриптом `python VestaHome\tools\make_tests.py` из папок сборки репозиториев (сначала
`build.bat` нужного проекта). Вручную здесь ничего не править — перезапишется. ПК-версии (`*PC.exe`)
сюда не копируются: они остаются в папках сборки репозиториев.

| Папка | Куда на ГУ | Что это |
|---|---|---|
| `SDCard\` | карта памяти: `\SDMemory` (внешняя), для OSMapsCE можно и `\SDMMC` | всё для проверок |
| `ResidentFlash\` | `\ResidentFlash` — встроенный раздел | `VHContent`: виджеты (`VHWidgets`) и дополнения (`VHPlugins`) VestaHome; `RadioLogos`: станции FM по городу и их логотипы (отдельный набор, не установщиком) |

Обычная папка программы копируется **целиком** (`SDCard\OSMapsCE\` → `\SDMemory\OSMapsCE\`).

- `VestaHomeInstall` — установщик VestaHome + proxy MCU и Bluetooth + пустышки вместо оболочки (кнопка 3);
- `LaunchInstall` — установщик VestaLaunch, сам кладёт его в `\SDMMC\VestaLaunch`;
- `OSMapsCE` — программа; карты ставятся из неё или загрузчиком для ПК;
- `TestMedia` — короткие аудио, видео и фото для плееров и «Диагностика → Звук и видео».

Папки с файлами для **корня внешней карты** — их **содержимое** кладётся в `\SDMemory\`:

- `SDCard\SafeMode\` — безопасный режим (флаг `…Mainflg` + `Cancle60s.exe`): включается на каждой загрузке,
  пока файлы в корне, — после разбора убрать;
- `SDCard\Updater\` — папка `UPDATE` для штатной оболочки: кнопка «Навигация» запускает `Updater.exe`
  (MortScript из пакета CityGuide) → наш `Updater.mscr` → по очереди `VestaHomeInstall.exe` и `LaunchInstall.exe`,
  что лежит рядом (ненужный — удалить из папки);
- `SDCard\SPIPower_rescue\` — аварийный пакет `SwDnl.upg` + `SwDnl.cfg`, только перед установкой proxy.
