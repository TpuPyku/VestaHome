# The HU check sheet generator, sheet 41 (2026-11-17, after sheet 40): the incoming call over any program, Android Auto
# "to the max" (its exit, the car's GPS and night, Bluetooth for calls, the turn in the top bar, what plays with its
# cover, its own 44.1 kHz sound, the microphone, nothing over it, OSMapsCE's starts to it), the music covers (the tag,
# the folder, the net), the tall "now playing", the boxes on the saver, VestaLaunch (the camera, the update keeping the
# look, its log by the switch, the probe's integrity), OSMapsCE's daily build; carried from 40: Wi-Fi off / on (fixed),
# the smart home's connection (no data then). Then the PDF. (Pattern: 40.)
import io, re, subprocess, os, time
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'docs') + os.sep
OUT = 'checklist-2026-11-17'
old = io.open(D + 'checklist-2026-11-16.html', encoding='utf-8').read()
head = old[:old.index('</style>') + len('</style>')]
rows40 = {}
for m in re.finditer(r'<tr><td class="n">(\d+)</td><td>(.*?)</td><td>(.*?)</td>', old, re.S):
    rows40[int(m.group(1))] = (m.group(2), m.group(3))
SECTIONS = [
 ('Установка (сначала)', 'VestaHome 1.0.261117 (звонки, обложки, Android Auto), VestaAuto (выход, GPS, Bluetooth, '
  'музыка, микрофон), VestaLaunch 1.05.261117, OSMapsCE 2.1.261117.', [
  ('VestaHome закрыть: «Настройки → О системе» → питание → «Выход» (дополнения закрываются вместе с ней, зависшие — '
   'через 3 с). Сначала, пока VestaHome закрыта: Tests\\ResidentFlash\\VHContent — в \\ResidentFlash\\VHContent с '
   'заменой; папку Tests\\SDCard\\OSMapsCE — в \\SDMMC\\OSMapsCE с заменой. Потом «УСТАНОВИТЬ / ОБНОВИТЬ» в '
   'Tests\\SDCard\\VestaHomeInstall. Перезагрузка.',
   'Копирование без ошибок «файл занят»; установщик без отказа; после перезагрузки VestaHome работает.'),
  ('«Настройки → О системе». В \\SDMemory\\VestaLogs\\VestaAuto.log — первая строка после запуска.',
   'VestaHome 1.0.26111…; «=== VestaAuto 1.4 запущен» (не 1.3).'),
  ('«О системе → Диагностика»: включить «Журналы» (если выключены).', 'Журналы пишутся в \\SDMemory\\VestaLogs.'),
 ]),
 ('Входящий звонок поверх всего', 'На прошлой поездке звонки при OSMapsCE впереди сбрасывались: окна VestaHome не '
  'было. Теперь звонок выводит VestaHome вперёд поверх любой программы.', [
  ('Открыть OSMapsCE на весь экран. Позвонить на телефон.', 'Поверх карты — окно входящего VestaHome; ответить на ГУ.'),
  ('Поговорить, положить трубку на ГУ.', 'Окно звонка закрылось, впереди снова OSMapsCE.'),
  ('Во время звонка нажать на руле «вверх / вниз».', 'Звонок и звук не мешаются, карта не меняет масштаб.'),
 ]),
 ('Android Auto', 'VestaAuto: выход, GPS и ночь машины, Bluetooth для звонков, манёвр в верхней строке, трек и обложка '
  'в «Сейчас играет», свой пересчёт звука, микрофон для помощника, ничто не перекрывает его экран. Всё в '
  'VestaAuto.log.', [
  ('Android Auto открыт. В нём нажать кнопку выхода (значок машины / «Выйти»).', 'Сразу VestaHome, не чёрный экран; '
   'значок Android Auto снова открывает экран телефона.'),
  ('Запустить навигацию в Android Auto (Яндекс Карты / Google Maps), ехать.', 'Метка движется по GPS машины без '
   'задержек; в верхней строке VestaHome (если открыть его) — манёвр и расстояние, касание открывает Android Auto.'),
  ('Вечером / «Настройки → Экран → День и ночь: Ночь».', 'Android Auto переходит в тёмную тему вместе с VestaHome.'),
  ('Посмотреть VestaAuto.log после подключения.', 'Строка «Bluetooth: … уже сопряжён с ГУ» (или «не тот, что '
   'подключён к ГУ»).'),
  ('При Android Auto позвонить на телефон, ответить.', 'Звонок не сбрасывается, голос через динамики машины.'),
  ('Включить музыку в Android Auto, выйти в VestaHome (MUTE дважды), главный экран.', 'В «Сейчас играет» — «Android '
   'Auto»: трек, исполнитель, приложение, время, обложка; кнопки ⏮ ⏯ ⏭ переключают треки телефона.'),
  ('Одну и ту же песню послушать через Android Auto и через Bluetooth.', 'Через Android Auto звук не хуже (не '
   '«пережатый»).'),
  ('«Окей, Google» в Android Auto, задать вопрос.', 'Помощник слышит и отвечает. Если нет — в VestaAuto.log строка '
   '«микрофон ГУ не открылся (устройств записи N …)»: записать N.'),
  ('Android Auto впереди. Из «Приложений» через MUTE и обратно запустить OSMapsCE.', 'Вместо OSMapsCE открывается '
   'Android Auto (сеанс идёт); в журнале VestaHome «вместо OSMapsCE его экран».'),
  ('Android Auto впереди, OSMapsCE уже была запущена раньше. Подождать 1 мин, поездить.', 'OSMapsCE не вылезает '
   'поверх Android Auto; звонок и камера заднего хода показываются поверх.'),
  ('В Android Auto кнопки руля «вверх / вниз» (с прошлого листа).', 'Переключают треки телефона.'),
 ]),
 ('Обложки и «Сейчас играет»', 'Обложка музыки с карты — из mp3 или из картинки в папке; для Bluetooth и '
  'интернет-радио — из интернета (Яндекс Музыка, затем iTunes), кэш на карте (VestaHome\\Covers).', [
  ('«Медиа → Музыка»: трек с обложкой в mp3; затем папка с cover.jpg / folder.jpg.', 'Вместо оранжевой заглушки — '
   'обложка, в обоих случаях.'),
  ('«Настройки → Общие → Виджеты главной»: включить «Сейчас играет · вертикальный», поставить сразу после «Карта OSM».',
   'На главной: карта слева, справа столбец с обложкой на фоне, текст читается.'),
  ('Музыка с телефона по Bluetooth, интернет есть.', 'Через несколько секунд — обложка песни на экране Bluetooth и в '
   'виджете.'),
  ('Интернет-радио, не трогать экран 15 с.', 'Обложка песни появляется сама (не после касания).'),
  ('Поставить радио на паузу.', 'Пока в строке та же песня — обложка остаётся.'),
 ]),
 ('Заставка и шрифт', 'Квадраты вместо «+» и «°» на заставке: исправлена утечка шрифтов, знак ▶ убран.', [
  ('Фон заставки «Мозаика», дать заставке поработать 1–2 часа (можно стоя).', 'Погода «+N°» без квадратов.'),
  ('Главный экран, музыка не играет: виджет «Сейчас играет».', '«последний трек — можно продолжить», без квадратов.'),
  ('Журнал VestaHome: найти «шрифты: больше 160».', 'Записать, есть ли строка.'),
 ]),
 ('Wi-Fi (исправлено, лист 40 п. 8–9)', None, [rows40[8], rows40[9]]),
 ('Умный дом: подключение (с листа 40)', 'Данные: сервер vestasmarthome.duckdns.org, порт 8883, логин car1; пароль — в файле '
  'accounts.txt на ПК (папка .vesta\\mqtt в профиле Windows).', [rows40[10], rows40[11], rows40[13]]),
 ('VestaLaunch', 'Ставить на машине со штатной оболочкой: LaunchInstall из Tests\\SDCard\\LaunchInstall.', [
  ('Запустить навигатор из VestaLaunch, включить задний ход (с прошлого листа).', 'Штатная камера с разметкой '
   'поверх навигатора; после заднего хода — снова навигатор.'),
  ('Положить свой значок в \\SDMMC\\VestaLaunch\\Icons, «УСТАНОВИТЬ» новую LaunchInstall.', 'Значок на месте; '
   'изменённые файлы Skin (если были) — в Skin.previous.'),
  ('Без файла \\SDMMC\\VestaLogs.on (журналы выключены).', 'VestaLaunch.log не пишется.'),
  ('(Машина с CAN-панелью) «ДИАГНОСТИКА».', 'В LaunchInstall_probe.log раздел «== целостность»: прислать.'),
 ]),
 ('OSMapsCE 2.1.261117', None, [
  ('Запустить OSMapsCE.', 'Открылась, карта и GPS как раньше.'),
  ('Место со множеством парковок и неровностей рядом.', 'Значки не налезают друг на друга; неровности ближе 40 м — '
   'один значок.'),
  ('Поиск «воронеж генерала лизюкова 2» (с картой Воронежской области или с интернетом).', 'Дом 2 находится; если '
   '«нет связи» — прислать OSMapsCE.log.'),
 ]),
]
TH = '<table>\n<tr><th>№</th><th>Действие</th><th>Что должно быть</th><th></th><th>Заметки</th></tr>\n'
ROW = '<tr><td class="n">%d</td><td>%s</td><td>%s</td><td class="ok"></td><td class="memo"></td></tr>\n'
out = [head, '\n</head>\n<body>\n<h1>Проверки на ГУ — сорок первый лист</h1>\n',
       '<p class="note">2026-11-17. Лист 40: установка, журналы, «Умный дом» (порт, неверный пароль), OSMapsCE, '
       'заставка Android Auto, MODE, 3G — ок; Wi-Fi выкл → вкл исправлен; подключения «Умного дома» не было данных.</p>\n',
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
           'BTProxy.log, SPIPower.log, VestaStub.log, VestaSmartHome.log, OSMapsCE.log, VestaLaunch.log).</li>\n'
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
