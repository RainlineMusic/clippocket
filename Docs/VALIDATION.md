# Проверки Clip Pocket 0.1.0

Проверялась Linux x86_64 сборка на GCC 13 / JUCE 8.0.4. Протоколы лежат в `Docs/Checks`, изображения — в `Docs/Previews`. Windows/macOS CI подготовлены, но ещё не запускались в этой среде. AAX SDK здесь отсутствует.

## DSP

`ClipDSPTest` проверяет:

- Exact delayed null на тестовом сигнале ниже порога при 44.1 / 48 / 88.2 / 96 / 176.4 / 192 kHz; максимальная разница 0 в double.
- Ограничение сэмплового ceiling при сильном drive, двухтоновых сигналах и случайном входе, во всех трёх моделях и шести качествах.
- Противофазную стереопару, задержку bypass, NaN/Inf sanitization.
- Переключение модели, качества, Asymmetry и Mix в идущем потоке; sample bound и finite output.
- Null при смене oversampling на неактивном клиппере.
- Счётчик `operator new`: 0 heap allocations в проверенном цикле Engine::process.
- Изменение результата при каждом дополнительном музыкальном контроле на тестовом музыкальном сигнале, включая тихий второй канал для Stereo Link.

ASan+UBSan: тесты проходят с `ASAN_OPTIONS=detect_leaks=0`. LeakSanitizer не запускался успешно из-за ограничений доступа к `/proc/.../task` в этой среде; это не результат leak-check. В CI остаются sanitizer-задачи с обычными настройками для hosted Linux runner.

Проверки finite/ceiling не являются субъективным прослушиванием реального мастера. Возможность высокого IN означает, что extreme drive будет слышимо искажать сигнал.

## Processor / UI

`ClipIntegrationTest` проверяет mono/stereo, float/double, сохранение и восстановление всех параметров, фабричные пресеты, default Ceiling=0, latency=624 при 48 kHz, автоматический offline 64x против ручного 64x, четыре темы и размеры 600/800/1200 px, expanded/compact, отсутствие пересечений контролов, enabled state Perceptual-ручек, blur при bypass и 20 create/destroy циклов редактора.

Изображения получены реальным `createComponentSnapshot` JUCE, а не отрисованы макетом. Внешний pluginval дополнительно открывал native VST3 editor на X11 virtual display. Экспериментальный OpenGL runtime в этой проверке выключен.

## VST3

Проверяется внешним **pluginval 1.0.4**. Протокол содержит реальные тесты audio processing, parameter automation, state, editor, editor automation и bus layouts. **Level 10 завершён с SUCCESS**: добавляются fuzz и дополнительные проверки. Параметры запуска записаны в логе. Встроенный Steinberg validator отдельно не подключён; auval на Linux не выполняет macOS AU validation.

```bash
pluginval --strictness-level 10 --random-seed 73 --validate '/absolute/path/Clip Pocket.vst3'
```

Для headless систем без дисплея возможен `--skip-gui-tests`; это уже более узкая проверка. macOS/Windows DAW и Pro Tools interoperability нужно проверять на соответствующих native сборках.

## Спектральные измерения

`ClipMeasurements` экспортирует 65536 steady-state сэмплов после warmup 8192. Fs=48 kHz, IN=0, Ceiling=0, Shape=0, Cleanliness=100, Focus=50, Punch=0, Stereo Link=0, **DC/ISP отключены**, sample guard включён, Output=0. Синус amplitude=1.45; отдельно есть двухтон 73+7013 Hz. Это настройки измерения, а не default preset.

`Tools/analyse_measurements.py` использует Blackman-Harris окно и ищет максимальный bin вне ±20 Hz от DC, основной частоты и нечётных гармоник, которые лежат ниже Nyquist. Результат в dBc относительно fundamental. Он включает alias spurs, guard modulation и остатки спектральной утечки, поэтому не называется точным «aliasing floor».

```bash
mkdir measurements
./build/ClipMeasurements measurements
python3 -m pip install numpy scipy
python3 Tools/analyse_measurements.py measurements --csv Docs/Checks/spectral.csv
```

Результаты представлены в `spectral.csv`. Пример максимального spur в dBc:

| Модель / качество | 997 Hz | 7000 Hz | 15000 Hz |
|---|---:|---:|---:|
| Naive hard clamp 1x | −56.4 | −32.9 | −16.8 |
| Clean 8x | −82.5 | −70.2 | −57.0 |
| Clean 16x | −82.7 | −80.2 | −67.0 |
| Clean 64x | −82.8 | −104.9 | −91.9 |
| Perceptual 64x | −84.6 | −121.3 | −117.8 |
 Сравнение с naive hard clamp — контрольная точка, не сравнение с другими продуктами. Perceptual использует более мягкое адаптивное колено и потому отличается от Clean по гармоникам, уровню и форме среза; сравнение двух режимов не доказывает превосходство антиалиасинга одного над другим.

Для звания «топ 1» ещё нужны level-matched слепые прослушивания на музыке, независимые TP/IMD/aliasing измерения, CPU comparisons, DAW-тесты на Windows/macOS и сравнение с конкурентами. В этой версии такого доказательства нет.
