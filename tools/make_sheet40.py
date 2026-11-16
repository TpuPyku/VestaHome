# The HU check sheet generator, sheet 40 (2026-11-16, after sheet 39): VestaHome 1.0 (the logs off by default and
# their switch, the installer updating the MCU proxy, the network's order, Wi-Fi off / on as a restart with its 30 s),
# VestaSmartHome 0.3 (the screen "Подключение", the password in dots, the ini's password sealed), OSMapsCE 2.1 with
# the new countries; carried from 39: the support splash, the 3G modem, the tunnels. Then the PDF. (Pattern: 39.)
import io, re, subprocess, os, time
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'docs') + os.sep
OUT = 'checklist-2026-11-16'
old = io.open(D + 'checklist-2026-11-15e.html', encoding='utf-8').read()
head = old[:old.index('</style>') + len('</style>')]
rows39 = {}
for m in re.finditer(r'<tr><td class="n">(\d+)</td><td>(.*?)</td><td>(.*?)</td>', old, re.S):
    rows39[int(m.group(1))] = (m.group(2), m.group(3))
SECTIONS = [
 ('Установка (сначала)', 'Новое: VestaHome 1.0 (журналы по умолчанию выключены, Wi-Fi без кнопки «Перезапустить», '
  'новый порядок «Сети»), «Умный дом» 0.3 (настройки вводятся на ГУ), OSMapsCE 2.1.', [
  ('VestaHome закрыть: «Настройки → Система → Выход». «Установить» в Tests\\SDCard\\VestaHomeInstall. '
   'Tests\\ResidentFlash\\VHContent — в \\ResidentFlash\\VHContent с заменой. Папку Tests\\SDCard\\OSMapsCE — в '
   '\\SDMMC\\OSMapsCE с заменой. Перезагрузка.',
   'Установщик без отказа (прокси MCU обновился); после перезагрузки VestaHome работает.'),
  ('«Настройки → О системе».', 'VestaHome 1.0.2611… (не 0.x).'),
 ]),
 ('Журналы (теперь выключены по умолчанию)', 'Раньше журналы писались, пока их не выключили. Теперь наоборот: после '
  'обновления они выключены, пока их не включить. Для проверок ниже их нужно включить.', [
  ('«Настройки → О системе → Диагностика»: строка «Журналы».', 'Переключатель выключен. Кнопки «Перезапуск Bluetooth» '
   'в списке нет.'),
  ('Через 2 минуты вынуть SD-карту, на ПК посмотреть папку VestaLogs на ней: время у VestaHome.log. Карту вернуть.', 'После перезагрузки новых записей '
   'нет (время — до установки).'),
  ('Включить «Журналы». Перезагрузить ГУ.', 'После перезагрузки в \\SDMemory\\VestaLogs снова пишутся VestaHome.log, '
   'SPIPower.log, BTProxy.log. Дальше — с журналами.'),
 ]),
 ('Сеть и Wi-Fi', None, [
  ('«Настройки → Сеть».', 'Строки по порядку: USB-модем, 3G-модем: оператор, APN (Настроить), Телефон (Bluetooth PAN) '
   '(Подключить), Автоподключение, Телефон по USB, Режим совместимости.'),
  ('«Настройки → Wi-Fi», Wi-Fi подключён.', 'Кнопки «Перезапустить» нет — только переключатель.'),
  ('Выключить Wi-Fi и сразу включить.', 'Строка «включение Wi-Fi: N с…», N убывает примерно от 30; затем Wi-Fi сам '
   'включается и подключается к сети.'),
  ('Выключить, сразу включить и во время отсчёта выключить снова.', 'Отсчёт пропал, Wi-Fi остаётся выключенным; '
   'VestaHome не завис.'),
 ]),
 ('Умный дом 0.3 — настройки на ГУ', 'Сервер, порт, логин и пароль теперь вводятся на экране; пароль хранится '
  'зашифрованным ключом этой ГУ. Старый VestaSmartHome.ini с паролем открытым текстом переделывается сам при первом '
  'запуске.', [
  ('«Настройки → Дополнения → Умный дом → Открыть» → «Подключение».', 'Сервер, порт 8883, логин car1 — как в ini; '
   'Пароль — точки, «хранится зашифрованным»; Связь — «подключено».'),
  ('«Настройки → О системе → Диагностика → Текстовые файлы»: \\ResidentFlash\\VHContent\\VHPlugins\\VestaSmartHome'
   '\\VestaSmartHome.ini.', 'В строке pass= — «v1:» и длинный набор цифр и букв, своего пароля открытым текстом нет.'),
  ('«Пароль» → набрать что-нибудь неверное → «Готово».', 'На клавиатуре вместо букв точки. Через несколько секунд '
   'Связь — «брокер отказал (5)».'),
  ('«Пароль» → набрать верный → «Готово».', 'Связь — «подключено».'),
  ('«Порт» → стереть, набрать «abc» → «Готово».', 'Надпись «Порт — число от 1 до 65535», порт остался 8883.'),
 ]),
 ('OSMapsCE 2.1', 'Релиз 2.1. Новые страны — после сборки карт 16.11: на своём сервере; на Яндекс Диске — с 1 декабря. '
  'Свой сервер по мобильной сети может не открываться (см. лист 31) — тогда этот пункт по Wi-Fi.', [
  ('Запустить OSMapsCE.', 'Открылась, карта и GPS как раньше; «Настройки» — версия 2.1.261116.'),
  ('«Настройки → КАРТЫ»: прокрутить список областей до конца.', 'После стран СНГ — Эстония, Латвия, Литва, '
   'Польша (воеводства), Германия (земли), Чехия, Словакия, Венгрия, с глобусом.'),
  ('(Если по Wi-Fi или открывается свой сервер) скачать Эстонию («СЕРВЕР»).', 'Скачалась; карта Таллина без '
   'интернета.'),
  rows39[19],
 ]),
 ('Заставка поддержки (перенесено из листа 39)', 'VestaAuto 1.2: заставка рисуется одним кадром — не должна '
  'моргать.', [rows39[k] for k in range(7, 13)] + [
  ('Смотреть на заставку 10 с.', 'Не моргает; меняется только строка отсчёта (в движении).'),
 ]),
 ('3G-модем (перенесено)', None, [rows39[13]]),
]
TH = '<table>\n<tr><th>№</th><th>Действие</th><th>Что должно быть</th><th></th><th>Заметки</th></tr>\n'
ROW = '<tr><td class="n">%d</td><td>%s</td><td>%s</td><td class="ok"></td><td class="memo"></td></tr>\n'
out = [head, '\n</head>\n<body>\n<h1>Проверки на ГУ — сороковой лист</h1>\n',
       '<p class="note">2026-11-16. Лист 39: звук Android Auto без MODE, Samsung, экран 3G, AUX — ок; бар VestaHome '
       'под Android Auto не появлялся.</p>\n',
       '<p class="note">☐ — «сделано», «Заметки» — что увидели. Журналы теперь включаются вручную (раздел 2). '
       'Перед проверкой удалите старые журналы из \\SDMemory\\VestaLogs.</p>\n']
s = k = 0
for t, note, rows in SECTIONS:
    s += 1
    out.append('\n<h2>%d. %s</h2>\n' % (s, t))
    if note:
        out.append('<p class="note">%s</p>\n' % note)
    out.append(TH)
    for a, b in rows:
        k += 1
        out.append(ROW % (k, a, b))
    out.append('</table>\n')
s += 1
out.append('\n<h2>%d. Что прислать</h2>\n<ul>\n<li>\\SDMemory\\VestaLogs\\ целиком (VestaHome, BTProxy.log, '
           'SPIPower.log, VestaStub.log, VestaAuto.log, VestaSmartHome.log, OSMapsCE.log).</li>\n'
           '<li>Этот лист с заметками.</li>\n</ul>\n'
           '</body>\n</html>\n' % s)
io.open(D + OUT + '.html', 'w', encoding='utf-8', newline='\n').write(''.join(out))
print(k, 'items,', s, 'sections')
EDGE = r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'
pdf = os.path.abspath(D + OUT + '.pdf')
tmp = pdf + '.new.pdf'
if os.path.exists(tmp):
    os.remove(tmp)
subprocess.run([EDGE, '--headless', '--disable-gpu', '--no-pdf-header-footer', '--print-to-pdf=' + tmp,
                'file:///' + os.path.abspath(D + OUT + '.html').replace('\\', '/')], check=True)
for _ in range(30):
    if os.path.exists(tmp) and os.path.getsize(tmp) > 0:
        break
    time.sleep(1)
time.sleep(1)
try:
    os.replace(tmp, pdf)
    print('pdf', os.path.getsize(pdf))
except OSError as e:
    print('pdf open elsewhere, left as', tmp, e)
