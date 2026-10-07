# Clip Pocket 0.3.0

Клиппер с шестью алгоритмами для плотной музыки. Две большие ручки IN и Ceiling, рядом Output и Low Protect; Delta и три индикатора IN / OUT / GR. Дизайн, Inter, три темы и blur при bypass сохранены из Pocket.

## Управление

| Контроль | Действие |
|---|---|
| IN, −24…+36 dB | Уровень до клиппинга. |
| Ceiling, −30…0 dB | Порог нелинейности в oversampled domain, по умолчанию 0.00 dB. |
| Output, −24…+12 dB | Финальный уровень. |
| Low Protect, 0…100% | Гибридная плавная защита перегруженного низа около 100 Hz; может уменьшать уровень бочки. По умолчанию 0%. |
| Mode, сверху | Clean / Cancel / Analog / Multiband / Fold / Orbit. |
| Delta | Прослушивание удалённой части сигнала. |
| Quality, в меню | 2x–64x; по умолчанию 16x. |
| Maximum 64x on offline render | 64x при сообщении хостом non-realtime rendering. Включено по умолчанию. |
| Bypass | Исходный сигнал с компенсацией задержки. |

Пресеты, Mix, Auto Gain, Release, DC correction и ISP Guard удалены. Старые сохранённые состояния сохраняют доступные параметры; удалённые параметры отбрасываются. Автоматизация удалённых параметров больше не обслуживается.

## Обработка

Дифференциальная топология: исходный сигнал задерживается, а oversampling-фильтры обрабатывают только удалённую часть. Тихий сигнал вне области клиппинга проходит без изменения амплитуды и фазы. Спектральный анализ управляет узким плавным коленом около порога, без широкого компрессионного колена и без общей gain-огибающей L/R.

Верхний список выбирает Clean (зафиксированный алгоритм 0.2), Cancel (компенсация НЧ-искажений), Analog (плавная сатурация), Multiband (три полосы), Fold (wavefolding), Orbit (радиальное стереоограничение). Fold и Orbit — экспериментальные цветные режимы.

Low Protect вместо восстановления уже обрезанного низа применяет плавное регулирование усиления. На НЧ-доминантных фрагментах работает ближе к лимитеру, на смешанном сигнале преимущественно защищает низкую полосу. Не отделяет дорожку бочки; одновременные инструменты могут затрагиваться. Подробности и патентные основы — в [DSP](Docs/DSP.md).

**ISP Guard и финального ограничителя нет. Пики после реконструкции могут превысить Ceiling, особенно в Cancel.** Положительный Output дополнительно повышает выход.

Задержка — **768 сэмплов**, 16 ms при 48 kHz; одинакова для всех качеств, Low Protect, Delta и bypass. Хост получает это значение. Внутренние вычисления double, поддерживаются float/double processing и mono/stereo.

## Сборка

JUCE 8.0.4, CMake ≥3.22, C++17, Xcode / Visual Studio 2022. По умолчанию только **VST3 + AAX**. JUCE содержит bundled AAX SDK; `AAX_SDK_PATH` нужен только для собственного SDK.

```bash
cmake -S . -B build -DJUCE_DIR=/absolute/path/to/JUCE-8.0.4 -DCLIP_BUILD_UI_TESTS=ON
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

CI выдаёт отдельные VST3/AAX архивы для Windows x64 и macOS Universal. Схема Apple signing/notarization и pluginval адаптирована из Duck Pocket. AAX Developer-сборки требуют соответствующей PACE-подписи для обычного Pro Tools. Standalone и AU не собираются.

DSP-тесты без JUCE:

```bash
cmake -S . -B build-dsp -DCLIP_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --parallel 2
ctest --test-dir build-dsp --output-on-failure
```

См. [DSP](Docs/DSP.md) и [проверки 0.3.0](Docs/VALIDATION.md). Существующие Binaries/Linux-x64, старые изображения Docs/Previews и логи v0.2 относятся к прошлым версиям.
