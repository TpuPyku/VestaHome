# The HU check sheet generator, sheet 37 (2026-11-15, after HU 2026-11-15-3): Android Auto again (the phone's reads
# without aborts, VestaAuto 0.7 logging what it leaves unanswered), AUX in MODE's round, the programs' top bar; the
# Navitel / Bluetooth and tunnel items carried. Then the PDF. (Pattern: make_sheet36.py.)
import io, re, subprocess, os, time
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'docs') + os.sep
OUT = 'checklist-2026-11-15c'
old = io.open(D + 'checklist-2026-11-15b.html', encoding='utf-8').read()
head = old[:old.index('</style>') + len('</style>')]
rows36 = {}
for m in re.finditer(r'<tr><td class="n">(\d+)</td><td>(.*?)</td><td>(.*?)</td>', old, re.S):
    rows36[int(m.group(1))] = (m.group(2), m.group(3))
SECTIONS = [
 ('Установка (сначала)', 'Новое: VestaHome (AUX, верхняя строка «Приложений»), VestaAuto 0.7, драйвер телефона '
  '(Android Auto без потери данных).', [
  ('VestaHome закрыть: «Настройки → Система → Выход». «Установить» в Tests\\SDCard\\VestaHomeInstall. '
   'Tests\\ResidentFlash\\VHContent — в \\ResidentFlash\\VHContent с заменой. Tests\\SDCard\\TetherProbe — на карту. '
   'Перезагрузка.', 'Установщик без отказа; после перезагрузки VestaHome работает.'),
 ]),
 ('Android Auto (VestaAuto 0.7)', 'Лист 36: чёрный экран — телефон доходил до запроса служб и замолкал. Причина '
  '(вероятная): драйвер обрывал чтение через 1 с без данных и терял байты. Теперь чтение не обрывается; VestaAuto '
  'пишет в журнал всё, на что не ответил. Звука Android Auto пока нет.', [
  ('После перезагрузки запустить TetherProbe.exe, OK. Подключить телефон кабелем. Открыть экран Android Auto, '
   'подождать до 20 с.', 'На ГУ — экран Android Auto (карты, музыка). Если чёрный — записать время; в VestaAuto.log '
   'после «телефон спросил службы» — строки «канал N открыт» / «канал N: сообщение …».'),
  ('Нажимать по экрану Android Auto: открыть карты, листать список.', 'Касания срабатывают там, где нажали.'),
  ('MUTE дважды, потом значок Android Auto в VestaHome.', 'VestaHome впереди без видео поверх; значок — экран '
   'телефона вернулся за 1–3 с.'),
 ]),
 ('AUX и MODE', 'Лист 36: AUX играет, но MODE его пропускал. Гнездо на этой прошивке не читается (так и у штатной), '
  'VestaHome теперь говорит MCU «штекер вставлен».', [
  ('Штекер AUX вставлен, играет радио. Нажимать MODE по кругу.', 'В круге есть AUX (звук с AUX, экран AUX).'),
  ('Вынуть штекер AUX. Нажимать MODE по кругу.', 'Записать: заходит ли MODE в AUX без штекера (тишина) или '
   'пропускает его.'),
 ]),
 ('Экран «Приложения»', None, [
  ('Включены Wi-Fi, Bluetooth, вставлены SD и флешка. Открыть «Приложения».', 'Вверху все значки состояния видны '
   'целиком; кнопки диспетчера и правки — сразу за словом «Приложения», обе работают.'),
 ]),
 ('Bluetooth и Навител (перенесено)', 'Лист 36 п. 13: при игравшем BT Навител забирает весь звук, BT падает. В работе; '
  'нужен ещё один повтор с журналами.', [
  ('Музыка телефона по BT играет. Запустить Навител, через минуту выйти из него.', 'Записать время запуска и выхода, '
   'был ли звук BT и Навитела, закрылся ли Навител.'),
 ]),
 ('Без ответа из листа 36', None, [rows36[16]]),
]
TH = '<table>\n<tr><th>№</th><th>Действие</th><th>Что должно быть</th><th></th><th>Заметки</th></tr>\n'
ROW = '<tr><td class="n">%d</td><td>%s</td><td>%s</td><td class="ok"></td><td class="memo"></td></tr>\n'
out = [head, '\n</head>\n<body>\n<h1>Проверки на ГУ — тридцать седьмой лист</h1>\n',
       '<p class="note">2026-11-15, вечер. Лист 36: ок — п. 1–3, 6–11 (USB-модем работает), 14, 15, 17–20; пауза музыки '
       'BT при втыкании кабеля — её ставит сам телефон.</p>\n',
       '<p class="note">☐ — «сделано», «Заметки» — что увидели. Перед проверкой удалите старые журналы из '
       '\\SDMemory\\VestaLogs. Журналы включены, SD-карта вставлена.</p>\n']
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
           'SPIPower.log, VestaStub.log, VestaTether.log, VestaAuto.log).</li>\n'
           '<li>Этот лист с заметками.</li>\n</ul>\n</body>\n</html>\n' % s)
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
