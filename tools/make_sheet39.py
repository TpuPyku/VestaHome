# The HU check sheet generator, sheet 39 (2026-11-15, after HU 2026-11-15-5): the phone's USB driver built into
# VestaHome (no TetherProbe; its switch in "Сеть"), the 3G settings' own screen, Android Auto's sound without MODE;
# VestaHome's bar behind Android Auto, AUX in MODE's round and the tunnels carried. Then the PDF. (Pattern: sheet 38.)
import io, re, subprocess, os, time
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'docs') + os.sep
OUT = 'checklist-2026-11-15e'
old = io.open(D + 'checklist-2026-11-15d.html', encoding='utf-8').read()
head = old[:old.index('</style>') + len('</style>')]
rows38 = {}
for m in re.finditer(r'<tr><td class="n">(\d+)</td><td>(.*?)</td><td>(.*?)</td>', old, re.S):
    rows38[int(m.group(1))] = (m.group(2), m.group(3))
SECTIONS = [
 ('Установка (сначала)', 'Новое: VestaHome сам ставит драйвер телефона по USB (TetherProbe больше не нужен и из Tests '
  'убран), настройки 3G-модема — на отдельном экране.', [
  ('VestaHome закрыть: «Настройки → Система → Выход». «Установить» в Tests\\SDCard\\VestaHomeInstall. Папку '
   'TetherProbe с карты можно удалить. Перезагрузка.', 'Установщик без отказа; после перезагрузки VestaHome работает.'),
 ]),
 ('Телефон по USB без TetherProbe', 'На телефоне «Отладка по USB» не нужна — можно выключить.', [
  ('После перезагрузки TetherProbe НЕ запускать. Открыть экран Android Auto, подключить телефон кабелем.',
   'Экран телефона в Android Auto, как с TetherProbe.'),
  ('Играет радио. В Android Auto включить музыку. Кнопку MODE НЕ нажимать, подождать 5 с.',
   'Музыка из динамиков сама, радио смолкло (VestaHome сам переключает на «медиа»).'),
  ('Отключить кабель. Выйти из экрана Android Auto («Назад»). На телефоне включить «USB-модем», подключить кабель.',
   'Через 10–20 с в VestaHome есть интернет (погода, радио из сети).'),
  ('«Настройки → Сеть»: выключить «Телефон по USB». Отключить и снова подключить кабель (модем на телефоне включён).',
   'Интернета по кабелю нет, телефон только заряжается.'),
  ('Включить «Телефон по USB» снова, переподключить кабель.', 'Интернет по кабелю вернулся.'),
 ]),
 ('Экран 3G-модема', None, [
  ('«Настройки → Сеть»: строка «3G-модем: оператор, APN» → «Настроить». Нажать «APN», ввести что-нибудь, '
   'подтвердить; затем стереть.', 'Отдельный экран с оператором, APN, логином и паролем; после клавиатуры — обратно '
   'на него; «Назад» — в «Сеть».'),
 ]),
 ('Перенесено из листа 38', None, [rows38[7], rows38[8], rows38[9], rows38[11]]),
]
TH = '<table>\n<tr><th>№</th><th>Действие</th><th>Что должно быть</th><th></th><th>Заметки</th></tr>\n'
ROW = '<tr><td class="n">%d</td><td>%s</td><td>%s</td><td class="ok"></td><td class="memo"></td></tr>\n'
out = [head, '\n</head>\n<body>\n<h1>Проверки на ГУ — тридцать девятый лист</h1>\n',
       '<p class="note">2026-11-15, ночь. Лист 38: Android Auto и модем по кабелю работают — драйвер встроен в '
       'VestaHome; видео 30 кадров/с, задержка ~60 мс. Навител — позже, отдельно.</p>\n',
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
