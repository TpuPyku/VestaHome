# MCU головного устройства: справочник

Связь ГУ (Windows CE) с микроконтроллером (радио, звук, кнопки, питание, CAN). Собрано 2026-10-20: исходящее — из `SPIPowerMgr.dll` прошивки 4120 скриптом Ghidra `SPIPower/ghidra_scripts/SpmCatalog.java`, входящее — из журналов proxy MCU (`SPIPower.log`, 2026-10-17…20) скриптом `tools/mcu_inbound.py`; смысл — из проверок на ГУ (`PROJECT.md`). «?» — не выяснено.

## Как устроено

- Программа зовёт `SPApi_*` из `SPIPowerMgr.dll`; каждая — обёртка над внутренней функцией, которая укладывает аргументы по байту и шлёт сообщение с кодом (`0xGGnn`, `GG` — группа). Ответы MCU приходят в «скелеты» (обработчики), зарегистрированные оболочкой (`SPApi_SetSkelonton`), с кодом группы на 3 меньше.
- Группы (исходящая → входящая, скелет): тюнер `0x0C` → `0x09` (1); клавиши и списки `0x4C`/`0x49` → `0x49` (9); `0x14` → `0x11` (2: аппаратные кнопки `0x1102`); звук `0x34` → `0x31` (6); часы `0x3C` → `0x39` (7); система/навигация `0x44` → `0x41` (8); инженерное `0x7C` → `0x79` (15); Bluetooth `0x8C` → `0x89` (17); медиа `0x94` → `0x91` (18); iPod `0x1C`, CD/MP3 `0x14`/`0x24` (`0x14` — и позиция воспроизведения), AUX/DTV `0x6C`, CAN-сиденья `0x74`, сброс `0x54`.
- **Из какого процесса:** клавиши (`0x4C03`, `0x4C0F`) и `0x4901` доходят из любого процесса, где загружена `SPIPowerMgr.dll`; звук (`0x3402`, `0x3404`) — только из процесса оболочки G3NavHMI (из VestaHome MCU не отвечал). Поэтому proxy MCU в G3NavHMI делает вызовы VestaHome сам (общая память `VestaMCUCmd`, `SPIPower/native/mcucmd.h`). Какие ещё группы так себя ведут — проверять по ответам.

## Исходящие (SPApi → MCU)

Аргументы: `0xd` — байт. «—» в коде: функция сообщения не шлёт (инициализация, обратные вызовы, питание…) — разобрать отдельно. Номера `Ordinal_N` — те же функции по номеру, в таблице не повторены.

| Функция | Код | Аргументы | Что известно |
|---|---|---|---|
| `SPApi_SendRadioPlayPosition` | `0x1401` | 3 |  |
| `SPApi_SendRadioCDListDisplayRange` | `0x1402` | 4 |  |
| `SPApi_SendRadioProgressPress` | `0x1403` |  |  |
| `SPApi_SetIPodConnectStatus` | `0x1c01` | 2 |  |
| `SPApi_SetIPodMeidaStatus` | `0x1c02` | 1 |  |
| `SPApi_SendRadioIpodListOpenStatus` | `0x1c03` | 1 |  |
| `SPApi_SendRadioIpodSongChangeStatus` | `0x1c04` |  |  |
| `SPApi_SendRadioMp3ListDisplayRange` | `0x240c` | 4 |  |
| `SPApi_SendRadioNaviVol` | `0x3401` | 1 | громкость навигации |
| `SPApi_SendRadioAudioHMLData` | `0x3402` | 2 | эквалайзер: (полоса 0 высокие / 1 средние / 2 низкие, значение −6..6 + 6); ответ 0x3102–0x3104 — **только из процесса оболочки** (VH — через proxy, VestaMCUCmd) |
| `SPApi_SendRadioAudioVolData` | `0x3403` | 2 | (вид, громкость): вид 0 — громкость «по умолчанию»/при включении (CAudioVolumeDlg::OnTimerDefaultSlot), 3 — ?, 5 — TA |
| `SPApi_SendRadioAudioBalanceFaderData` | `0x3404` | 2 | баланс/фейдер (x + 8, y + 8), центр (8, 8); ответ 0x3105 — **только из оболочки** |
| `SPApi_SendRadioDeMuteCurrentSource` | `0x3405` | 1 | снять «без звука» с текущего источника |
| `SPApi_SendSRSDefinitionData` | `0x3406` | 1 |  |
| `SPApi_SendRadioSRS3DData` | `0x3407` | 1 |  |
| `SPApi_SendRadioSRSTrubassData` | `0x3408` | 1 |  |
| `SPApi_SendRadioSRSFocusData` | `0x3409` | 1 |  |
| `SPApi_SendRadioKeyVolData` | `0x340e` | 1 | громкость с расш. клавишей 0x9E (окно громкости оболочки) |
| `SPApi_SendRadioTTSVolData` | `0x340f` | 1 | громкость голосовых подсказок (TTS) |
| `SPApi_SetClockTime` | `0x3c01` | 6 | (час, мин, сек, год % 100, месяц, день) — часы MCU; VH: время по GPS |
| `SPApi_GetClockTime` | `0x3c02` |  | запрос часов MCU (→ 0x39xx?) |
| `SPApi_SendReSetFactory` | `0x4103` | 1 |  |
| `SPApi_SendRadioNaviVoiceGuidance` | `0x4402` | 1 | 1 — голос навигации начался (радио тише, waveOut слышен), 0 — кончился; оболочка шлёт (0) при своём старте |
| `SPApi_SendRadioNaviTFTOpenClose` | `0x4403` | 1 |  |
| `SPApi_SendRadioAutoManualModel` | `0x4404` | 1 |  |
| `SPApi_SendRadioSKDeMute` | `0x4405` |  |  |
| `SPApi_SendRadioReceivedRVCStatus` | `0x4406` | 1 | (0) — оболочка подтверждает конец заднего хода |
| `SPApi_SendRadioRequestProductInfo` | `0x4409` |  | запрос версий/ID → 0x7902, 0x4111, 0x4112, 0x4118 |
| `SPApi_SysSendRadioBackLight` | `0x4410` | 1 | яркость подсветки 1..10 текущего режима день/ночь (у экрана на деле два уровня) |
| `SPApi_SendRadioNaviMediaMuteStatus` | `0x4414` | 1 | приглушение медиа под навигацию — оболочка |
| `SPApi_SendRadioNaviLanguage` | `0x4416` | 1 |  |
| `SPApi_SendRadioNaviEpromSection` | `0x441f` | 1 |  |
| `SPApi_SendRadioNaviRecoverEpromisDone` | `0x4420` |  |  |
| `SPApi_SendRadioNaviRequestMapActivationSN` | `0x4422` |  |  |
| `SPApi_SendListItemControl` | `0x4901` | 3 | (1, n, 0) — пресет n; (1, n, 1) — сохранить станцию в пресет n (ответ 0x0909); (0, строка, 0) — станция из списка. **Из VH доходит** |
| `SPApi_SendListItemControlEx` | `0x4901` | 4 |  |
| `SPApi_SendNewConfigCode` | `0x4907` | 1 |  |
| `SPApi_SendLoadSpeekerTest` | `0x4908` | 2 |  |
| `SPApi_SendKeyEvent` | `0x4c03` | 3 | клавиша: (код, 0, 0) нажата + (код, 1, 1) отпущена; коды — ниже. **Из процесса VH доходит.** VH: McuKey |
| `SPApi_SendRotaryEvent` | `0x4c04` | 3 | крутилка — не пробовалось |
| `SPApi_SendVRControl` | `0x4c09` | 3 |  |
| `SPApi_SendVROver` | `0x4c0a` |  |  |
| `SPApi_SendVRSourceControlCommand` | `0x4c0b` | 1 |  |
| `SPApi_sendHarkeyStatus` | `0x4c0c` | 1 |  |
| `SPApi_SendRadioChangeSource` | `0x4c0d` |  | смена источника (как MODE?) — не пробовалось |
| `SPApi_SendRadioSendCurrentSourceNow` | `0x4c0e` |  | попросить MCU прислать текущий источник (0x4904?) — не пробовалось |
| `SPApi_SendExtendKeyEvent` | `0x4c0f` | 3 | расширенная клавиша, как выше. **Из VH доходит.** |
| `SPApi_SendRadioValue` | `0x4c13` | 3 | ? — не пробовалось |
| `SPApi_SendRadioIsNaviSourceCouldIn` | `0x4c14` | 1 | оболочка шлёт (0) при старте — сразу после него MCU отдаёт весь тюнер; VH так же будит радио |
| `SPApi_SendNaviBoxStatus` | `0x4c16` | 2 |  |
| `SPApi_SendResetEvent` | `0x5403` | 1 | сброс — Main.exe |
| `SPApi_SendRadioCCPHaveSave` | `0x5404` |  |  |
| `SPApi_SendRadioDTVInitStatus` | `0x6c01` | 1 |  |
| `SPApi_SendRadioAuxInOutStatus` | `0x6c02` | 1 |  |
| `SPApi_SendRadioCanAutoSeatSetData` | `0x7401` | 2 |  |
| `EngSendBTMp3PlayStatus` | `0x7c02` | 1 |  |
| `EngSendBTMatchStatus` | `0x7c03` | 1 |  |
| `EngSendBTOverStatus` | `0x7c04` | 1 |  |
| `EngSendGPSAntennaStatus` | `0x7c05` | 1 |  |
| `EngSendSatelliteID` | `0x7c06` | 2 |  |
| `EngSendGPSIntensity` | `0x7c07` | 2 |  |
| `EngSendGPSSearchTime` | `0x7c08` | 2 |  |
| `EngSendSWVersionB` | `0x7c09` | 3 |  |
| `EngSendTFTColor` | `0x7c0a` | 1 |  |
| `EngSendTFTBrightnessData` | `0x7c0b` | 1 |  |
| `EngSendTFTCalibrationStatus` | `0x7c0c` | 1 |  |
| `EngSendMediaTestStatus` | `0x7c0d` | 2 |  |
| `EngSendFactorySettingStatus` | `0x7c11` | 1 |  |
| `EngSendRaioOpenCloseTFT` | `0x7c13` | 1 |  |
| `EngSendGPSNum` | `0x7c15` | 2 |  |
| `EngSendGPSStatus` | `0x7c16` | 1 |  |
| `EngSendGPSTime` | `0x7c17` | 3 |  |
| `EngSendGPSFirstPosTime` | `0x7c18` | 4 |  |
| `EngSendCarSpeed` | `0x7c19` | 2 |  |
| `SPApi_SysSendRadioDecoctTime` | `0x7c1c` | 1 |  |
| `SPApi_SendRadioRequestBTAddress` | `0x7c1d` |  | запрос BT-адреса → 0x7922 |
| `SPApi_SendBTPhoneStatus` | `0x8c01` | 2 | статус телефона для MCU (приглушение радио при звонке?) — оболочка |
| `SPApi_SendBTOpenStatus` | `0x8c02` | 1 | BT включён/выключен — оболочка |
| `SPApi_SendBTMatchStatus` | `0x8c03` | 1 | сопряжение — оболочка |
| `SPApi_SendBTEnterPhoneStatus` | `0x8c04` | 1 | вход в экран телефона — оболочка |
| `SPApi_SendRadioBTAudioChannelChoiceStatus` | `0x8c05` | 1 | звук телефона в ГУ/в телефон — оболочка |
| `SPApi_SendRadioBTRouteChange` | `0x8c12` |  |  |
| `SPApi_SendRadioBTTipStatus` | `0x8c13` | 1 |  |
| `SPApi_SendRadioBTPrivacyStatus` | `0x8c14` | 1 |  |
| `SPApi_SendMediaAvailability` | `0x9401` | 3 |  |
| `SPApi_SendMediaSourceSelect` | `0x9402` | 2 | (5, 3) — «Музыка USB», (5, 4) — «Видео» оболочки → источник 0x4904 = 06; VH: наш плеер (MediaSource) |
| `SPApi_SendMediaCardStatus` | `0x9403` | 3 |  |
| `SPApi_SendMediaPlayerResetStatue` | `0x940b` | 1 |  |
| `SPApi_SendMediaCardIsNewStatus` | `0x940c` | 2 |  |
| `SPApi_SendSelectedDevice` | `0x940d` | 1 |  |
| `SPApi_SendCurrentAdjustItem` | `0xc01` | 2 |  |
| `SPApi_SendCurrentAdjustBand` | `0xc02` | 1 |  |
| `SPApi_SendCurrentAdjustPty` | `0xc03` | 1 |  |
| `SPApi_SendUpdateMemoryList` | `0xc04` | 2 |  |
| `EngSendBTOpenStatus` | — |  |  |
| `EngSendGYROACCData` | — |  |  |
| `EngSendSWVersionA` | — |  |  |
| `EngSendSWVersionW` | — |  |  |
| `SPApi_DisableTFT` | — |  |  |
| `SPApi_EmptySkelonton` | — |  |  |
| `SPApi_EnableTFT` | — |  |  |
| `SPApi_EngSendCanTestResult` | — |  |  |
| `SPApi_EngSendWifiRegResult` | — |  |  |
| `SPApi_EngSendWifiRegStatus` | — |  |  |
| `SPApi_EngSendWifiSwCtrlResult` | — |  |  |
| `SPApi_EngSendWifiTestResult` | — |  |  |
| `SPApi_GetCurrentPowerState` | — |  |  |
| `SPApi_InitializeRadioEngine` | — |  |  |
| `SPApi_IsInitialized` | — |  |  |
| `SPApi_IsLogoComplete` | — |  |  |
| `SPApi_IsNeedToUpdateRadio` | — |  |  |
| `SPApi_LoadByMain` | — |  |  |
| `SPApi_LoadRadioEngine` | — |  |  |
| `SPApi_LogoCompleteByMain` | — |  |  |
| `SPApi_NavStarted` | — |  |  |
| `SPApi_NotifyCurrentSpeed` | — |  |  |
| `SPApi_OffBacklight` | — |  |  |
| `SPApi_RegisterAspectCallBack` | — |  |  |
| `SPApi_RequestEpromDataFromRadio` | — |  |  |
| `SPApi_RequestMediaPlayTimeFromRadio` | — |  |  |
| `SPApi_SendDiagMessage` | — |  |  |
| `SPApi_SendMediaPlayTimeToRadio` | — |  |  |
| `SPApi_SendRadioNaviEnterTipDlg` | — |  |  |
| `SPApi_SendRadioSaveDataToEprom` | — |  |  |
| `SPApi_SendVoiceGuidanceStatus` | — |  |  |
| `SPApi_SetBackLightOn` | — |  |  |
| `SPApi_SetExitSPI` | — |  |  |
| `SPApi_SetSkelonton` | — |  |  |
| `SPApi_SpeakerTest` | — |  |  |
| `SPApi_TellMainToExit` | — |  |  |
| `SPApi_TellRadioToSendDialog` | — |  |  |
| `SPApi_TurnOnBacklight` | — |  |  |
| `SPApi_UnLoadRadioEngine` | — |  |  |
| `SPApi_UnRegisterAspectCallBack` | — |  |  |
| `SPApi_UninitializeRadioEngine` | — |  |  |
| `SPApi_queryProductionDate` | — |  |  |
| `SPApi_queryProductionSN` | — |  |  |
| `TerminalModelEnterExit` | — |  |  |
| `TerminalModelStatus` | — |  |  |

## Входящие (MCU → программа)

Все, что proxy видел 2026-10-17…20: код, скелет, длина, сколько раз, частые первые 4 байта.

| Код | Скелет | Длина | Раз | Частые начала | Что известно |
|---|---|---|---|---|---|
| `0x0901` | 1 | 7 | 3518 | 00 00 28 82 \| 00 00 28 B4 \| 00 00 22 56 \| 00 00 27 C4 | частота: 00 <диапазон 0 FM, 1 УКВ, 2 СВ> <частота BE: ×10 кГц; СВ — кГц> |
| `0x0904` | 1 | 3 | 2043 | 00 04 01 00 \| 00 02 00 00 \| 00 04 00 00 \| 00 01 00 00 | флаги тюнера: 00 <флаг> <0/1>; 01 — идёт поиск, 02 — поиск вверх, 03 — вниз |
| `0x0907` | 1 | 3 | 683 | 00 01 FF 00 \| 00 01 03 00 \| 00 01 00 00 \| 00 01 04 00 | играющий пресет: 00 01 <n> (FF — нет) |
| `0x0908` | 1 | 21 | 864 | 00 01 00 07 \| 00 01 10 17 \| 00 01 08 0F \| 00 00 00 07 | списки: 00 <0 станции / 1 пресеты> <с> <по> <всего> + частоты |
| `0x0909` | 1 | 5 | 35 | 00 01 02 27 \| 00 01 00 22 \| 00 01 04 28 \| 00 01 1C 00 | пресет сохранён: 00 01 <пресет 0..23> <частота BE> (нового 0x0908 нет) |
| `0x090B` | 1 | 11 | 1632 | 00 00 00 00 \| 00 00 00 20 \| 00 00 00 4C \| 00 00 00 4E | RDS PS (8 знаков с байта 3) |
| `0x090C` | 1 | 4 | 1871 | 00 00 00 00 \| 00 00 00 0A \| 00 00 00 0B \| 00 00 00 01 | ? |
| `0x090D` | 1 | 4 | 1771 | 00 00 00 00 \| 00 00 00 01 | ? |
| `0x091E` | 1 | 3 | 338 | 00 00 00 00 \| 00 28 00 00 \| 00 01 00 00 \| 00 03 00 00 | ? (00 00 / 01 / 02 / 28 — меняется с диапазоном) |
| `0x0920` | 1 | 3 | 302 | 00 01 00 00 \| 00 01 01 00 \| 00 00 00 00 | настройки: RDS, TA |
| `0x0921` | 1 | 3 | 285 | 00 01 00 00 \| 00 01 01 00 | AF |
| `0x0922` | 1 | 3 | 66 | 00 01 02 00 | ? |
| `0x0923` | 1 | 3 | 66 | 00 01 00 00 | ? |
| `0x0924` | 1 | 3 | 92 | 00 01 00 00 \| 00 01 01 00 | CT (часы по RDS) |
| `0x0927` | 1 | 3 | 66 | 00 01 00 00 | ? |
| `0x0929` | 1 | 4 | 1737 | 00 00 00 00 | ? |
| `0x092B` | 1 | 3 | 62 | 00 01 01 00 \| 00 01 00 00 | ? |
| `0x092C` | 1 | 2 | 13 | 00 19 00 00 \| 00 17 00 00 | ? |
| `0x092D` | 1 | 3 | 285 | 00 01 00 00 \| 00 01 01 00 | MS |
| `0x092E` | 1 | 3 | 305 | 00 01 00 00 \| 00 01 01 00 \| 00 00 00 00 | шумоподавление, СВ (включён ли диапазон) |
| `0x092F` | 1 | 4 | 1679 | 00 00 00 00 \| 00 00 00 01 | ? |
| `0x0930` | 1 | 20,36 | 108 | 00 00 03 1E \| 00 04 07 1E \| 00 08 0B 1E \| 00 0C 0F 1E | имена станций списка: 4 по 8 знаков |
| `0x0931` | 1 | 4,36 | 17087 | 00 00 00 00 \| 00 00 00 01 \| 00 00 00 02 | RadioText: часть 0/1 по 32 знака |
| `0x1102` | 2 | 3 | 223 | 00 16 00 00 \| 00 18 00 00 \| 00 0E 00 00 | аппаратные кнопки: 16 — MODE?, 18 — крутилка 5 с (сброс через 5 с), 0E — ? |
| `0x3101` | 6 | 3 | 2176 | 00 00 06 00 \| 00 00 05 00 \| 00 00 04 00 \| 00 00 07 00 | громкость: 00 00 <n> текущего источника (свои у радио/медиа/BT); 00 09/0A <n> — других видов |
| `0x3102` | 6 | 2 | 44 | 00 06 00 00 \| 00 07 00 00 \| 00 0C 00 00 | эквалайзер низкие 0..12 |
| `0x3103` | 6 | 2 | 27 | 00 07 00 00 \| 00 06 00 00 | средние 0..12 |
| `0x3104` | 6 | 2 | 27 | 00 08 00 00 \| 00 06 00 00 | высокие 0..12 |
| `0x3105` | 6 | 3 | 34 | 00 00 07 00 \| 00 08 07 00 \| 00 08 08 00 \| 00 00 08 00 | баланс, фейдер (центр 8) |
| `0x3107` | 6 | 2 | 126 | 00 05 00 00 | ? (05 при смене источника) |
| `0x3108` | 6 | 3 | 15 | 00 04 00 00 | ? |
| `0x3109` | 6 | 2 | 296 | 00 00 00 00 \| 00 01 00 00 | без звука: 00 <0/1> |
| `0x310A` | 6 | 2 | 24 | 00 00 00 00 | ? |
| `0x3117` | 6 | 2 | 24 | 00 00 00 00 | ? |
| `0x3901` | 7 | 7 | 6 | 00 0D 20 04 \| 00 07 1F 12 | ? |
| `0x3903` | 7 | 2 | 12 | 00 00 00 00 | ? |
| `0x4101` | 8 | 3 | 34 | 00 00 00 00 \| 00 01 00 00 | ? (00 01 00 → 00 00 00 с переинициализацией тюнера в поездке) |
| `0x4102` | 8 | 3 | 111 | 00 05 00 00 \| 00 10 00 00 \| 00 0C 00 00 \| 00 32 00 00 | страница/режим? (00 05 00 после клавиши 0x9B оболочки при старте) |
| `0x4105` | 8 | 2 | 33 | 00 00 00 00 \| 00 01 00 00 | ночь 1 / день 0 |
| `0x4107` | 8 | 2 | 33 | 00 0A 00 00 \| 00 01 00 00 \| 00 00 00 00 | подсветка 1..10 |
| `0x4111` | 8 | 5 | 65 | 00 20 18 06 | ID изделия (часть) |
| `0x4112` | 8 | 15 | 65 | 00 30 39 33 | ID изделия (текст) |
| `0x4118` | 8 | 6 | 65 | 00 41 80 05 | ID изделия (часть) |
| `0x411B` | 8 | 2 | 33 | 00 00 00 00 | ? |
| `0x411E` | 8 | 34 | 8 | 00 64 01 00 \| 00 47 00 00 | ? |
| `0x411F` | 8 | 3 | 624 | 00 02 01 00 \| 00 00 01 00 \| 00 15 01 00 \| 00 37 01 00 | ? раз в ~3,7 с при USB (02 01 / 38 01 / 37 01) |
| `0x4121` | 8 | 3 | 12 | 00 07 00 00 \| 00 05 00 00 \| 00 03 00 00 \| 00 0A 00 00 | ? |
| `0x412A` | 8 | 3 | 716 | 00 00 05 00 \| 00 00 04 00 \| 00 00 14 00 \| 00 00 09 00 | скорость машины, км/ч: 00 00 <n> (только в движении) |
| `0x4140` | 8 | 2 | 38 | 00 01 00 00 | ? |
| `0x4904` | 9 | 2 | 143 | 00 00 00 00 \| 00 06 00 00 \| 00 07 00 00 \| 00 09 00 00 | источник звука: 00 радио, 06/07 медиа, 09 Bluetooth |
| `0x4908` | 9 | 2 | 30 | 00 00 00 00 | ? |
| `0x4C01` | 9 | 4 | 85 | 00 0B 01 03 \| 00 0A 01 03 \| 00 06 01 01 | ? |
| `0x7901` | 15 | 3 | 126 | 00 00 00 00 \| 00 04 00 00 | страница инженерного меню |
| `0x7902` | 15 | 11,41 | 96 | 00 03 00 00 \| 00 00 00 00 | версии ПО/железа MCU |
| `0x7905` | 15 | 4,5 | 21393 | 00 00 42 68 \| 00 0D 00 00 \| 00 03 CE 4C \| 00 0E CE 4C | System status: sub 0A плата °C, 06 усилитель °C, 07 поле тюнера дБ, 03/0E кнопки руля (мВ), 0D экран °C; раз в 2 с |
| `0x791E` | 15 | 3 | 4280 | 00 01 01 00 \| 00 00 04 00 \| 00 00 03 00 \| 00 00 02 00 | ? |
| `0x7922` | 15 | 8 | 23 | 00 06 04 F8 | BT-адрес ГУ |
| `0x7931` | 15 | 2 | 2 | 00 31 00 00 | ? |
| `0x8902` | 17 | 2 | 16 | 00 00 00 00 | BT-музыка (с 0x8903) |
| `0x8903` | 17 | 3 | 77 | 00 00 00 00 \| 00 00 06 00 \| 00 01 09 00 \| 00 00 09 00 | BT-музыка источник: 00 01 09; выход — 00 00 00 (и перед сбросом) |
| `0x8905` | 17 | 3 | 39 | 00 00 00 00 \| 00 01 00 00 \| 00 0A 00 00 | ответ на клавишу «Телефон» 0x84 |
| `0x9101` | 18 | 4 | 173 | 00 05 06 00 \| 00 01 02 00 \| 00 00 02 00 \| 00 05 09 00 | медиа: состояние при смене источника |
| `0x9103` | 18 | 4 | 173 | 00 01 01 03 \| 00 01 00 03 \| 00 00 01 03 \| 00 00 00 03 | медиа: состояние при смене источника |

## Клавиши (`SendKeyEvent` / `SendExtendKeyEvent`)

| Код | Вид | Что делает |
|---|---|---|
| 0x30 | обычная | прислать состояние тюнера; оболочка шлёт её перед каждой сменой источника |
| 0x7F | обычная | источник «Радио» (главное меню оболочки) |
| 0x3B | обычная | диапазон: FM1 → FM2 → FM3 → УКВ (из СВ — FM1) |
| 0x3A | обычная | СВ (AM) |
| 0x3C | обычная | список станций (только в нём работает 0x3F) |
| 0x3F | обычная | автопоиск (скан), повтор — стоп |
| 0x3B / 0x3C | расш. | поиск вниз / вверх (СВ — шаг 9 кГц) |
| 0xA4 | расш. | PTY |
| 0x90 | расш. | все настройки радио разом (при открытии экрана настроек) |
| 0x97 | обычная | перед каждой клавишей настроек радио (щелчок?) |
| 0xA7 … | расш. | настройки радио парами вкл/выкл (шумоподавление, СВ, RDS, TA, AF, CT, MS) — PROJECT.md |
| 0x84 | обычная | экран «Телефон» (MCU отвечает 0x8905) |
| 0x54 | обычная | BT-музыка — источник 09 (после 0x84) |
| 0x64 / 0x65 | обычная | подсветка день / ночь |
| 0xE4 | обычная | экран баланса оболочки |
| 0xE6 | обычная | экран эквалайзера оболочки |
| 0x5A | обычная | «Отмена» перезагрузки по долгой крутилке |
| 0x52 / 0x53, 0x95 / 0x96, 0x2D / 0x2B | расш. | строки окна громкости оболочки |
| 0x9E | расш. | вместе с SendRadioKeyVolData |
| 0x9B | обычная | оболочка при старте (ответ 0x4102 00 05) |
| 0x7D | расш. | оболочка при старте (ответ 0x7901) |
| 0xB1 | обычная | оболочка при старте (ответ 0x7901 00 04) |

## Не сделано (PLAN.md)

- Функции без кода (—): что делают (инициализация, `RegisterAspectCallBack`, питание, подсветка, `SpeakerTest`…).
- Входящие, которых в журналах не было: обработчики скелетов в G3NavHMI (что оболочка делает с каждым кодом).
- Полная таблица клавиш: все вызовы `SendKeyEvent` в G3NavHMI с постоянным кодом и экран/кнопка, откуда.
- Для каждой группы — доходит ли из процесса VestaHome.

