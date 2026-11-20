# The HU check sheet generator, sheet 43 (2026-11-20, after sheet 42 - no answer to it yet but the logs of 20.11 with
# the microphone): VestaHome 1.0.261120 without the proxies (the loader does the MCU and Bluetooth), the installer's
# check of the unit, the clock without GPS, the MCU's versions, the music list's folders, the loader's minute,
# VestaAuto 1.7 (the microphone's record), VHCheck 1.3, the MCU's power-off message; carried from 42: the apps' strip,
# Android Auto (exit, GPS, theme, Bluetooth, its music), Wi-Fi, the smart home's password, VestaLaunch, the CAN panel's
# RESET; OSMapsCE in one section of its own, its first item: it opened. Then the PDF. (Pattern: 42.)
import io, re, subprocess, os, time
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'docs') + os.sep
OUT = 'checklist-2026-11-20'
old = io.open(D + 'checklist-2026-11-19.html', encoding='utf-8').read()
head = old[:old.index('</style>') + len('</style>')]
r42 = {}
for m in re.finditer(r'<tr><td class="n">(\d+)</td><td>(.*?)</td><td>(.*?)</td>', old, re.S):
    r42[int(m.group(1))] = (m.group(2), m.group(3))
SECTIONS = [
 ('Установка (сначала)', 'VestaHome 1.0.261120 — <b>без proxy</b>: заводские SPIPowerMgr.dll и FLC_BLUETOOTH_SDK.dll '
  'больше не подменяются, MCU и Bluetooth для VestaHome ведёт загрузчик. VestaAuto 1.7. OSMapsCE и VestaLaunch не '
  'менялись.', [
  ('VestaHome закрыть: «Настройки → О системе» → питание → «Выход». Пока она закрыта: Tests\\ResidentFlash\\VHContent '
   '— в \\ResidentFlash\\VHContent с заменой. Запустить Tests\\SDCard\\VestaHomeInstall, нажать «СТАТУС».',
   'В отчёте — строка проверки ГУ: установка разрешена (4120). Сфотографировать отчёт.'),
  ('«УСТАНОВИТЬ / ОБНОВИТЬ» с отметками VestaHome и «Убрать прежние proxy». Перезагрузка.', 'Установщик без отказа; '
   'после перезагрузки VestaHome работает. В VestaHomeInstall.log — «rollback» (прежние proxy убраны).'),
  ('Ещё одна перезагрузка. «СТАТУС» снова.', 'Proxy нет (заводские DLL на месте), загрузчик стоит, неудачных запусков '
   '0 из 3.'),
  ('«Настройки → О системе». В \\SDMemory\\VestaLogs\\VestaAuto.log — первая строка после запуска.',
   'VestaHome 1.0.261120; «=== VestaAuto 1.7 запущен».'),
  r42[3],
 ]),
 ('Без proxy: всё ли работает', 'MCU и Bluetooth теперь через загрузчик. Проверить то, что раньше шло через proxy.', [
  ('Кнопки руля: громкость, следующий / предыдущий, MUTE; кнопки панели ГУ.', 'Работают как раньше.'),
  ('«Радио»: станции, переключение, RDS (название станции).', 'Звук есть, станции переключаются, RDS виден.'),
  ('Задний ход.', 'Камера заднего хода показывается; после — снова VestaHome.'),
  ('Bluetooth: музыка с телефона, пауза / следующий трек.', 'Играет, кнопки работают, трек и время видны.'),
  ('Позвонить и принять звонок.', 'Звонок идёт, собеседника слышно, вас слышно; окно звонка поверх.'),
  ('«Телефон → Контакты».', 'Книга телефона загружается.'),
  ('Интернет через телефон по Bluetooth (PAN).', 'Интернет есть (погода, интернет-радио).'),
 ]),
 ('Часы, версии, GPS', None, [
  ('Включить ГУ (до того, как появится GPS или интернет). Посмотреть время.', 'Время верное (не 00:00); дата может '
   'быть старой. В журнале VestaStub «часы MCU без даты - взято время суток» или похожая строка.'),
  ('Подключить интернет (Wi-Fi или телефон).', 'В течение минуты дата верная; в журнале VestaHome «часы: дата из '
   'сети».'),
  ('«О системе → Подробнее» → «Запросить».', 'Версии MCU SW и HW заполнены (не нули).'),
  ('Машина в гараже или GPS ещё не нашёл спутники: «Подробнее» / виджет времени.', '«время по GPS недоступно» вместо '
   '«нет фикса».'),
 ]),
 ('Музыка с SD-карты: папки', 'Список терял папки, если в папке больше 400 файлов.', [
  ('«Медиа → Музыка → Список», корень SD-карты (той, где 400+ файлов).', 'Сначала папки (оранжевые), потом файлы; '
   'вверху «показаны не все файлы папки», если файлов больше 400.'),
  ('Открыть папку с музыкой, нажать трек.', 'Играет трек этой папки.'),
  ('Вернуться в корень, «Играть всё».', 'Играет музыка со всей карты (её первые 400 треков).'),
 ]),
 ('Загрузчик: минута вместо 3', 'Запуск VestaHome считается удачным после 1 минуты работы.', [
  ('Если VestaHome не запускается после прошлых коротких поездок: удалить '
   '\\ResidentFlash\\navigation\\app\\VestaStub.cnt, перезагрузить.', 'VestaHome снова запускается.'),
  ('Включить ГУ, через 1–2 минуты выключить зажигание; повторить 3 раза. Потом включить.', 'VestaHome запускается '
   '(не штатная). «СТАТУС» в установщике — неудачных запусков 0 из 3.'),
  ('Выключить зажигание при включённых журналах (для будущей доработки).', 'Прислать VestaStub.log и SPIPower.log '
   'этого выключения: по последним сообщениям MCU найдём предупреждение о выключении.'),
 ]),
 ('OSMapsCE', None, [
  ('Открыть OSMapsCE.', 'OSMapsCE открылась, карта видна.'),
  r42[14], r42[15],
 ]),
 ('Android Auto: микрофон', 'Gemini в Казахстане не работает — смотрим не на ответ помощника, а на запись: VestaAuto '
  '1.7 пишет, что ушло телефону, в \\SDMemory\\VestaLogs\\AAmic.wav.', [
  ('Android Auto открыт, нажать кнопку микрофона, сказать фразу 5–10 с.', 'Прислать AAmic.wav и VestaAuto.log: '
   'в журнале «устройство записи N: «…»» (все устройства), «микрофон записан в AAmic.wav», «уровень от … до …».'),
  ('Пока помощник слушал: смотреть на экран Android Auto.', 'Записать: двигались ли полоски помощника от голоса.'),
  ('На телефоне: «Настройки → Приложения → Приложения по умолчанию → Цифровой помощник» → Яндекс; снова кнопка '
   'микрофона в Android Auto.', 'Записать: запустилась ли Алиса или что сказал Android Auto.'),
 ]),
 ('Android Auto: выход и USB-модем (с листа 42)', None, [r42[7], r42[8], r42[9]]),
 ('Android Auto: GPS, тема, Bluetooth (с листа 42)', None, [r42[10], r42[11], r42[12]]),
 ('Музыка Android Auto в VestaHome (с листа 42)', None, [r42[16], r42[17], r42[18]]),
 ('«Приложения» (с листа 42)', None, [r42[4], r42[5], r42[6]]),
 ('«Поддержали разработку»', 'Список обновлён 20.11.', [
  ('«О системе → Подробнее → Поддержали».', 'Алексей, Дмитрий З., Дмитрий П., Dima E., Roman R., Александр М., '
   'Владислав Б., Гена Б., Денис П., Иван Н., Максим К., Михаил А., Роман Е., Hobta Evgen, Insaf S., outerdusk и '
   'другие; список прокручивается.'),
 ]),
 ('Wi-Fi (с листа 42)', None, [r42[20], r42[21]]),
 ('Умный дом: пароль заново (с листа 42)', 'Логин — car1, пароль — только в поле «Пароль».', [r42[22], r42[23]]),
 ('VHCheck 1.3 на этой ГУ', 'Проверка ГУ, ничего не меняет. Журнал без координат и номеров ГУ.', [
  ('Папку Tests\\SDCard\\VHCheck — на SD-карту, запустить VHCheck.exe, ждать «конец» (~1 мин).', 'Прислать '
   'VestaLogs\\VHCheck.log; в нём «MCU TR8005-…» (версия MCU пришла).'),
 ]),
 ('VestaLaunch (с листа 42)', 'Ставить на машине со штатной оболочкой: LaunchInstall из Tests\\SDCard\\LaunchInstall.',
  [r42[24], r42[25], r42[26], r42[27]]),
 ('Машина с CAN-панелью: кнопка RESET (с листа 42)', 'Скрытая кнопка (скрепкой): узнать, что она делает. Ничего не '
  'перепрошивать.', [r42[28], r42[29]]),
]
TH = '<table>\n<tr><th>№</th><th>Действие</th><th>Что должно быть</th><th></th><th>Заметки</th></tr>\n'
ROW = '<tr><td class="n">%d</td><td>%s</td><td>%s</td><td class="ok"></td><td class="memo"></td></tr>\n'
out = [head, '\n</head>\n<body>\n<h1>Проверки на ГУ — сорок третий лист</h1>\n',
       '<p class="note">2026-11-20. Лист 42 — ответа не было (кроме микрофона Android Auto: помощник не понял; '
       'Gemini в Казахстане не работает) — его пункты перенесены. Новое: VestaHome без proxy, проверка ГУ в '
       'установщике, часы без GPS, версии MCU, папки в списке музыки, порог загрузчика 1 мин, запись микрофона.</p>\n',
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
           'AAmic.wav, VestaTether.log, SPIPower.log, VestaStub.log, VestaHomeInstall.log, VestaSmartHome.log, '
           'VestaLaunch.log, VHCheck.log).</li>\n<li>Фото отчёта «СТАТУС».</li>\n'
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
