# The HU check sheet generator, sheet 42 (2026-11-19, after sheet 41): the apps' buttons in a strip under the title,
# Android Auto's exit ending the session (the cable free for the phone's tethering, the turn gone), its GPS with the
# point's time, the theme and the silent phone logged / tried again, the unit's Bluetooth address, the microphone from
# input 1, its music on the saver and the top bar, "Занято Android Auto" on the Bluetooth screen, the supporters;
# carried from 41: items 15-16 (OSMapsCE under Android Auto), Wi-Fi off / on, the smart home's password typed again,
# VestaLaunch; the CAN panel's RESET button. Then the PDF. (Pattern: 41.)
import io, re, subprocess, os, time
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'docs') + os.sep
OUT = 'checklist-2026-11-19'
old = io.open(D + 'checklist-2026-11-17.html', encoding='utf-8').read()
head = old[:old.index('</style>') + len('</style>')]
rows41 = {}
for m in re.finditer(r'<tr><td class="n">(\d+)</td><td>(.*?)</td><td>(.*?)</td>', old, re.S):
    rows41[int(m.group(1))] = (m.group(2), m.group(3))
SECTIONS = [
 ('Установка (сначала)', 'VestaHome 1.0.261119, VestaAuto 1.5 (выход, GPS, микрофон). OSMapsCE и VestaLaunch не '
  'менялись.', [
  ('VestaHome закрыть: «Настройки → О системе» → питание → «Выход». Пока она закрыта: Tests\\ResidentFlash\\VHContent '
   '— в \\ResidentFlash\\VHContent с заменой. Потом «УСТАНОВИТЬ / ОБНОВИТЬ» в Tests\\SDCard\\VestaHomeInstall. '
   'Перезагрузка.', 'Копирование без ошибок «файл занят»; установщик без отказа; после перезагрузки VestaHome '
   'работает.'),
  ('«Настройки → О системе». В \\SDMemory\\VestaLogs\\VestaAuto.log — первая строка после запуска.',
   'VestaHome 1.0.261119; «=== VestaAuto 1.5 запущен».'),
  ('«О системе → Диагностика»: включить «Журналы» (если выключены).', 'Журналы пишутся в \\SDMemory\\VestaLogs.'),
 ]),
 ('«Приложения»', 'Кнопки «Изменить» и «Диспетчер» перенесены из верхней строки в полосу под ней (вариант B).', [
  ('Открыть «Приложения», когда в верхней строке есть сообщение (манёвр Android Auto, состояние).',
   'Сообщение видно целиком; справа под строкой две кнопки с подписями «Изменить» и «Диспетчер».'),
  ('Ярлыков больше четырёх (или добавить): посмотреть оба ряда.', 'Два ряда плиток целиком, точки страниц под ними, '
   'ничего не налезает на нижнюю панель.'),
  ('Нажать «Изменить», затем «Диспетчер».', '«Изменить» подсвечен, плитки в рамке; «Диспетчер» открывает диспетчер '
   'задач.'),
 ]),
 ('Android Auto: выход и USB-модем', 'Выход в Android Auto теперь завершает сеанс: кабель свободен для USB-модема '
  'телефона, значок Android Auto в VestaHome запускает его снова.', [
  ('Android Auto открыт, идёт навигация. В нём нажать кнопку выхода.', 'Сразу VestaHome; манёвра в верхней строке '
   'нет. В VestaAuto.log: «выход из Android Auto - сеанс закрывается» и «телефон ответил на прощание» (или «не '
   'ответил»).'),
  ('После выхода включить на телефоне USB-модем.', 'Интернет по USB-модему появляется (без снятия VestaAuto в '
   'диспетчере).'),
  ('Нажать значок Android Auto в VestaHome.', 'Снова экран Android Auto (телефон переключается из модема в Android '
   'Auto).'),
 ]),
 ('Android Auto: GPS, тема, Bluetooth, помощник', 'Всё — в VestaAuto.log.', [
  ('Навигация в Android Auto (Яндекс Карты / Google Maps), ехать.', 'Метка движется по GPS машины. В журнале '
   '«телефон включил датчик 1» и «первая точка GPS - телефону» (если метка стоит — есть ли эти строки).'),
  ('Android Auto открыт: «Настройки → Экран → День и ночь» — сменить тему (ночь → день и обратно).', 'Android Auto '
   'меняет тему и не вылетает. Если вылетел — время записать (в журналах VestaHome «тема: …», VestaAuto «ночь/день - '
   'телефону»).'),
  ('Переподключить телефон (второе подключение после включения ГУ), VestaAuto.log.', 'Строки «адрес Bluetooth ГУ '
   'ещё не известен» нет; есть строки «Bluetooth: …».'),
  ('«Окей, Google» в Android Auto, задать вопрос.', 'Помощник слышит и отвечает. В журнале «микрофон ГУ открыт '
   '(устройство 1 из N)» и «первый кадр, уровень N» — записать уровень.'),
  rows41[15], rows41[16],
 ]),
 ('Музыка Android Auto в VestaHome', 'Музыка Android Auto теперь видна на заставке и значком в верхней строке; экран '
  'Bluetooth при Android Auto говорит, что занято.', [
  ('Музыка в Android Auto, выйти в VestaHome (MUTE дважды), дождаться заставки.', 'На заставке — трек · исполнитель '
   'Android Auto.'),
  ('Главный экран, верхняя строка.', 'Значок ноты; касание открывает Android Auto.'),
  ('«Медиа → Bluetooth» при играющем Android Auto.', '«Занято Android Auto», полосы трека нет, кнопки тусклые; '
   'музыка Android Auto не прерывается.'),
 ]),
 ('«Поддержали разработку»', None, [
  ('«О системе → Подробнее → Поддержали».', 'Дмитрий, Insaf S., Александр М., Алексей, Hobta Evgen, Roman R., '
   'Дмитрий П., outerdusk; список прокручивается.'),
 ]),
 ('Wi-Fi (с листа 41, ответа не было)', None, [rows41[26], rows41[27]]),
 ('Умный дом: пароль заново', 'Лист 41: в поле «Логин» однажды ушёл пароль, с логином car1 пароль был неверный. '
  'Логин — car1, пароль — только в поле «Пароль» (файл accounts.txt на ПК, папка .vesta\\mqtt в профиле Windows).', [
  ('«Настройки → Дополнения → Умный дом → Открыть» → «Подключение»: «Логин» — car1.', 'Логин car1, сервер, порт '
   '8883.'),
  ('«Пароль» → набрать пароль car1 → «Готово».', 'Связь — «подключено».'),
 ]),
 ('VestaLaunch (с листа 41, ответа не было)', 'Ставить на машине со штатной оболочкой: LaunchInstall из '
  'Tests\\SDCard\\LaunchInstall.', [rows41[31], rows41[32], rows41[33], rows41[34]]),
 ('Машина с CAN-панелью: кнопка RESET', 'Скрытая кнопка (скрепкой): узнать, что она делает. Ничего не '
  'перепрошивать.', [
  ('Нажать RESET коротко.', 'Записать: просто перезагрузка или что-то ещё; после неё на месте VestaLaunch («Программы»), '
   'CAN-панель и её настройки, пресеты радио, время.'),
  ('Нажать и держать RESET ~10 с.', 'То же записать.'),
 ]),
]
TH = '<table>\n<tr><th>№</th><th>Действие</th><th>Что должно быть</th><th></th><th>Заметки</th></tr>\n'
ROW = '<tr><td class="n">%d</td><td>%s</td><td>%s</td><td class="ok"></td><td class="memo"></td></tr>\n'
out = [head, '\n</head>\n<body>\n<h1>Проверки на ГУ — сорок второй лист</h1>\n',
       '<p class="note">2026-11-19. Лист 41: установка, звонок поверх OSMapsCE, Android Auto (звонки, «Сейчас '
       'играет», кнопки руля), обложки, заставка, OSMapsCE — ок; «вверх/вниз» при звонке = взять/положить — так и '
       'оставлено; масштаб с руля — отложен (MCU не передаёт удержание).</p>\n',
       '<p class="note">☐ — «сделано», «Заметки» — что увидели. Перед проверкой удалите старые журналы из '
       '\\SDMemory\\VestaLogs.</p>\n']
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
out.append('\n<h2>%d. Что прислать</h2>\n<ul>\n<li>\\SDMemory\\VestaLogs\\ целиком (VestaHome, VestaAuto.log, '
           'VestaTether.log, BTProxy.log, SPIPower.log, VestaStub.log, VestaSmartHome.log, VestaLaunch.log).</li>\n'
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
