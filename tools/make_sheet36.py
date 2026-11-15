# The HU check sheet generator, sheet 36 (2026-11-15, after HU 2026-11-15-2): only what is open - Android Auto
# (the phone switched when the screen opens, MUTE twice), the USB modem as a network adapter, "Выход" without the
# stock shell; sheet 35's unchecked items (10, 17) and AUX carried. Then the PDF. (Pattern: make_sheet35.py.)
import io, re, subprocess, os, time
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'docs') + os.sep
OUT = 'checklist-2026-11-15b'
old = io.open(D + 'checklist-2026-11-15.html', encoding='utf-8').read()
head = old[:old.index('</style>') + len('</style>')]
rows35 = {}
for m in re.finditer(r'<tr><td class="n">(\d+)</td><td>(.*?)</td><td>(.*?)</td>', old, re.S):
    rows35[int(m.group(1))] = (m.group(2), m.group(3))
SECTIONS = [
 ('Установка (сначала)', 'Новое 15.11: VestaHome и заглушка («Выход» больше не включает штатную), VestaAuto 0.6, '
  'драйвер телефона (Android Auto + USB-модем как сеть).', [
  ('VestaHome закрыть: «Настройки → Система → Выход».', 'Штатная оболочка НЕ появляется (раньше появлялась — '
   'это и была «штатная без причины»). В VestaStub.log «VestaHome закрыт кнопкой «Выход»».'),
  ('«Установить» в Tests\\SDCard\\VestaHomeInstall. Tests\\ResidentFlash\\VHContent — в \\ResidentFlash\\VHContent '
   'с заменой. Tests\\SDCard\\TetherProbe — на карту. Перезагрузка.', 'Установщик без отказа; после перезагрузки '
   'VestaHome работает.'),
 ]),
 ('Android Auto (VestaAuto 0.6)', '15.11-2 сеанса не было: телефон подключили раньше, чем открыли экран Android '
  'Auto, а переводить уже подключённый драйвер не умел (видео было пробой из файла — эти кнопки убраны). Теперь '
  'переводит, как только экран открыт. Звука Android Auto пока нет.', [
  ('После перезагрузки запустить TetherProbe.exe, OK. Подключить телефон кабелем (в любой момент).',
   'Телефон заряжается; на экране Android Auto без телефона — «Подключите телефон кабелем…» и «Назад».'),
  ('Открыть экран Android Auto, ничего не нажимать, подождать до 15 с.', 'Телефон переподключается сам, на ГУ '
   '— экран Android Auto (карты, музыка, приложения). На телефоне при первом разе — разрешения: разрешить.'),
  ('Нажимать по экрану Android Auto: открыть карты, листать список.', 'Касания срабатывают там, где нажали.'),
  ('MUTE дважды.', 'VestaHome впереди, видео Android Auto не висит поверх.'),
  ('Значок Android Auto в VestaHome.', 'Экран телефона вернулся (через 1–3 с), касания работают.'),
  ('Вынуть кабель.', 'VestaHome на экран сам.'),
 ]),
 ('USB-модем телефона (сеть)', 'Новое 15.11: телефон с «USB-модемом» становится сетевым адаптером ГУ '
  '(интернет для погоды, радио, пробок). Проба на ГУ 15.11 утром дала адрес — теперь адаптер.', [
  ('Экран Android Auto закрыт. Wi-Fi и Bluetooth-модем выключены. На телефоне включить «USB-модем», подключить '
   'кабель (TetherProbe запущен после перезагрузки).', 'В VestaTether.log «ПРОБА: успех», «адаптер: '
   'зарегистрирован», «адаптер: NDIS принял».'),
  ('Через 30 с открыть погоду / интернет-радио.', 'Погода обновилась, радио играет. Если нет — записать, что в '
   'VestaTether.log после «адаптер:».'),
  ('Отключить кабель.', 'В журнале «адаптер: остановлен (отправлено N, принято M…)»; ГУ работает дальше.'),
 ]),
 ('Bluetooth (лист 35 п. 5 и 15.11-2)', 'По журналам 15.11-2 телефон подключался после каждой загрузки, стек '
  'BT играл нормально (состояние A2DP обычное, трек идёт, MCU на Bluetooth), но звука, по словам пользователя, '
  'не было — значит, теряется после стека. Новое: VestaHome пишет состояние звука BT раз в 10 с.', [
  ('Музыка телефона по Bluetooth 15+ мин (можно с Навителем). Как только звука нет, хотя трек на экране идёт, — '
   'записать время (часы ГУ) и что делали перед этим.', 'Время в заметке. В VestaHome.log раз в 10 с строки '
   '«BT-звук: источник …, телефон …, трек … (идёт/стоит), устройства …» — по ним видно, что изменилось.'),
  ('Если Bluetooth «не подключается» — записать время, что именно не работает (звонки / музыка / интернет) и '
   'горит ли значок BT вверху.', 'Заметка с временем.'),
 ]),
 ('Без ответа из листа 35', None, [rows35[10], rows35[17]]),
 ('AUX (нет кабеля — перенесено)', None, [rows35[n] for n in (21, 22, 23, 24)]),
]
TH = '<table>\n<tr><th>№</th><th>Действие</th><th>Что должно быть</th><th></th><th>Заметки</th></tr>\n'
ROW = '<tr><td class="n">%d</td><td>%s</td><td>%s</td><td class="ok"></td><td class="memo"></td></tr>\n'
out = [head, '\n</head>\n<body>\n<h1>Проверки на ГУ — тридцать шестой лист</h1>\n',
       '<p class="note">2026-11-15, вечер. Лист 35: ок — п. 1–4, 8, 9, 12–16, 18–20. Открытое без изменений '
       '(«Штатная» на логотипе, Bluetooth после выкл/вкл, Навител и звук BT) — в работе, в лист не входит.</p>\n',
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
