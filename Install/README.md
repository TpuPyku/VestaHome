# Install — единый установщик Vesta

Один файл `VestaInstall.exe` (Windows CE, ARM) ставит на ГУ всё сразу:

- **VestaLaunch** — запускалка приложений → `\SDMMC\VestaLaunch\VestaLaunch.exe`;
- **патч оболочки** `G3NavHMI.exe` (loopback `127.0.0.1` → `127.0.0.2`);
- **`Config\ExternNavi.cfg`** — кнопка «Навигация» открывает VestaLaunch;
- **скин VESTA** — `Itelma/MMC27` → `Itelma/VESTA` + картинки из `skin-vesta`.

## Использование на ГУ
1. Положить `VestaInstall.exe` на SD, запустить.
2. **Установить** — делает всё выше одним нажатием (обратимо, создаёт `.bak`).
3. **Удалить** — полный откат (exe, cfg, папки VESTA и VestaLaunch).
4. **Выход**.

Папка оболочки по умолчанию — `\ResidentFlash\navigation\app`. Для скриптов:
`VestaInstall.exe "<APP_DIR>" [status|install|uninstall]`; лог пишется в
`VestaInstall.log` рядом с exe. Реестр не используется — состояние читается из файлов.

## Сборка
`G3patch\native\build.bat` (VS2005). Перед ним — `python make_payload.py`, который
пакует `VestaLaunch\VestaLaunch\VestaLaunch.exe` + `G3patch\skin-vesta` в `payload.bin`;
он встраивается в exe как RCDATA-ресурс.
