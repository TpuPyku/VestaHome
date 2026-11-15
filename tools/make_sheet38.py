# The HU check sheet generator, sheet 38 (2026-11-15, after HU 2026-11-15-4): Android Auto's sound (VestaAuto 0.8),
# its video at 30 a second, VestaHome's bar behind its icons; AUX in MODE's round and the tunnels carried; Navitel's
# sound output. Then the PDF. (Pattern: make_sheet37.py.)
import io, re, subprocess, os, time
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'docs') + os.sep
OUT = 'checklist-2026-11-15d'
old = io.open(D + 'checklist-2026-11-15c.html', encoding='utf-8').read()
head = old[:old.index('</style>') + len('</style>')]
rows37 = {}
for m in re.finditer(r'<tr><td class="n">(\d+)</td><td>(.*?)</td><td>(.*?)</td>', old, re.S):
    rows37[int(m.group(1))] = (m.group(2), m.group(3))
SECTIONS = [
 ('Установка (сначала)', 'Новое: VestaHome (команда «sound» для звука дополнений), VestaAuto 0.8 (звук Android Auto, '
  '30 кадров/с).', [
  ('VestaHome закрыть: «Настройки → Система → Выход». «Установить» в Tests\\SDCard\\VestaHomeInstall. '
   'Tests\\ResidentFlash\\VHContent — в \\ResidentFlash\\VHContent с заменой. Перезагрузка.',
   'Установщик без отказа; после перезагрузки VestaHome работает.'),
 ]),
 ('Android Auto: звук и плавность', 'Лист 37: экран и касания работают; звука не было, «на динамике подтормаживало» '
  '(телефон слал 60 кадров/с, декодер отставал до 0,6 с — теперь просим 30).', [
  ('Играет радио. TetherProbe.exe, OK. Кабель, экран Android Auto. Покрутить карту, полистать списки.',
   'Движение плавное, без рывков и отставания от пальца.'),
  ('В Android Auto включить музыку (любой плеер телефона).', 'Музыка из динамиков машины; радио смолкло; '
   'в VestaAuto.log «звук канала 4 — начался».'),
  ('Кнопки громкости на руле / ГУ во время музыки Android Auto.', 'Громкость меняется.'),
  ('В картах Android Auto проложить маршрут (любой), дождаться голосовой подсказки (или «Повторить подсказку»).',
   'Подсказка слышна; музыка в это время тише или прерывается.'),
  ('MUTE дважды (VestaHome впереди), подождать 10 с, значок Android Auto.', 'Музыка Android Auto не прерывалась '
   'или продолжилась; экран телефона вернулся.'),
  ('Всё время с Android Auto смотреть на низ экрана.', 'Нижний бар VestaHome под значками Android Auto не появляется. '
   'Если появился — записать время, сфотографировать.'),
 ]),
 ('AUX и MODE (перенесено из листа 37)', 'Гнездо на этой прошивке не читается, VestaHome говорит MCU «штекер '
  'вставлен».', [rows37[5], rows37[6]]),
 ('Навител и Bluetooth', 'Лист 37: после выхода из Навитела BT пропадал до перезагрузки, Навител зависал. Наши '
  'программы звук в этот момент не держали. Похоже, Навител сам выводит звук в Bluetooth.', [
  ('Навител: «Меню → Настройки → Звук» (или «Система»): какой выбран вывод звука (Bluetooth / Динамик / Audio '
   'Output / FM). Записать. Поставить «Динамик» или «Audio Output». Выйти из Навитела.', 'Записать, что было выбрано.'),
  ('Музыка телефона по BT играет. Запустить Навител, через минуту выйти.', 'BT-музыка играет дальше; Навител '
   'закрылся. Записать время запуска и выхода.'),
 ]),
 ('Без ответа из листа 37', None, [rows37[9]]),
]
TH = '<table>\n<tr><th>№</th><th>Действие</th><th>Что должно быть</th><th></th><th>Заметки</th></tr>\n'
ROW = '<tr><td class="n">%d</td><td>%s</td><td>%s</td><td class="ok"></td><td class="memo"></td></tr>\n'
out = [head, '\n</head>\n<body>\n<h1>Проверки на ГУ — тридцать восьмой лист</h1>\n',
       '<p class="note">2026-11-15, поздний вечер. Лист 37: ок — п. 1, 2–4 (Android Auto работает), 7 (верхняя строка '
       '«Приложений»).</p>\n',
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
           '<li>Этот лист с заметками; фото, если бар VestaHome был виден под Android Auto.</li>\n</ul>\n'
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
