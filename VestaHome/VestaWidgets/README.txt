Свои виджеты VestaHome
======================

Папка \SDMemory\VestaWidgets\ (на ПК - VestaWidgets рядом с VestaHomePC.exe). Каждый файл *.ini - один виджет на
главном экране (до 8); включается, выключается и переставляется в «Настройки -> Общие -> Виджеты главной».

ОПИСАНИЕ ВИДЖЕТА (<имя>.ini, UTF-8 или cp1251)
    title=   заголовок
    icon=    music radio phone gear home car cloud nav map sd usb wifi bt info clock photo video apps warn eye
             power folder tasks star lock file
    wide=1   двойной (по умолчанию одинарный)
    data=    файл данных (по умолчанию <имя>.txt рядом)
    refresh= как часто VestaHome перечитывает данные, секунды (по умолчанию 10)
    start=   что запустить один раз при старте VestaHome (скрипт, который обновляет данные)
    tap=     что запустить по нажатию на виджет
    Пути без «\» в начале - от папки виджета. *.mscr запускаются через MortScript.exe: он ищется рядом с
    виджетом, в \SDMemory\MortScript\, в \SDMemory\, рядом с VestaHome, в \SDMMC\MortScript\.

HTTPS ЧЕРЕЗ VESTAHOME (в системе ГУ HTTPS нет - MortScript и wget его не умеют)
    url=     http:// или https:// адрес; VestaHome скачивает его сам
    every=   раз в сколько секунд (не чаще раза в минуту; по умолчанию 600)
    save=    куда положить ответ (по умолчанию <имя>.resp рядом) - для скрипта
    value.from= text.from= text2.from= unit.from= state.from=   «начало|конец»: что взять из ответа
             (первое совпадение; &amp; и т. п. раскрываются); тогда файл данных пишет сам VestaHome - без скрипта
    value= unit= text= text2= state=   постоянный текст поля; %s в нём - взятое из ответа
    state: green/yellow/red переводятся в ok/warn/alert. Ответ - до 256 КБ; при ошибке остаются прежние
    данные, причина - в журнале VestaHome. Пример - probki.ini (баллы Яндекс Пробок).

ДАННЫЕ (пишет скрипт; VestaHome только читает)
    value=   крупное значение     unit=  единица рядом с ним
    text=    строка под ним       text2= вторая строка
    state=   ok / warn / alert - зелёная, оранжевая, красная точка
    Без value - текст крупнее, в две строки. Нет файла - «Нет данных».

Пишите данные на SD-карту (как здесь), не во встроенную память ГУ: частая запись её изнашивает.
