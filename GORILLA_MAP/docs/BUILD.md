# Сборка и проверка GORILLA MAP

Нужен Python 3.10+. Сторонние Python-пакеты не требуются. Команды выполняются из корня проекта:

```sh
python tools/test_packaging.py
python tools/verify.py
python tools/build.py
python tools/verify.py --pbo dist/@GorillaMap/addons/gorillamap.pbo
```

Результат:

- `dist/@GorillaMap/addons/gorillamap.pbo` — несжатый PBO с виртуальным префиксом `GorillaMap`.
- `dist/@GorillaMap/mod.cpp` — метаданные для лаунчера.
- `dist/GORILLA_MAP.zip` — мод, исходники, документация, инструменты и образец серверного профиля.
- `dist/build-manifest.json` — SHA-256 PBO и каждого исходного файла, статус неподписанной сборки.

Сборка воспроизводима: файлы упорядочены, даты записей фиксированы. PBO содержит штатный SHA-1 trailer. Схема заголовков сверена с открытой реализацией [armake](https://github.com/KoffeinFlummi/armake/blob/master/src/build.c); код armake в проект не включён.

`build.py` упаковывает скрипты и текстовый `config.cpp`. Он не компилирует Enforce Script, не биниризует ресурсы и не создаёт цифровую подпись. `verify.py` проверяет структуру текста, XML, ссылки на ресурсы и виджеты, а с `--pbo` — точное соответствие упаковки исходникам. Эти проверки не подтверждают работу в движке.

## Сборка штатными инструментами Windows

Альтернатива: DayZ Tools → Addon Builder. Source — `Source/GorillaMap`, destination — `@GorillaMap/addons`, prefix — `GorillaMap`. Сохраняйте `.c`, `.layout`, `.xml`, `.paa` в наборе файлов упаковки. Скрипты проверяются при запуске DayZDiag/DayZ, а не самим фактом создания PBO.

## Подпись для сервера

Используйте DSUtils из DayZ Tools и собственную пару ключей. Закрытый `.biprivatekey` хранится вне распространяемого архива. `DSSignFile` создаёт `.bisign` для финального PBO; разместите подпись рядом с PBO. Только публичный `.bikey` установите в серверный каталог `keys/`.

Пример PowerShell, где пути заменяются вашими:

```powershell
& 'C:\DayZ Tools\DSUtils\DSSignFile.exe' 'D:\PrivateKeys\GORILLA.biprivatekey' '.\dist\@GorillaMap\addons\gorillamap.pbo'
```

После любого изменения PBO подпись создаётся заново. В архиве намеренно нет фиктивной подписи или чужого ключа. Для публичного сервера с `verifySignatures=2` подпись обязательна; изменение этой настройки не является шагом установки.

## Проверка в DayZ

Мод загружается в `-mod` на клиенте и сервере. Сначала проверьте чистый ChernarusPlus без других модов, затем ваш набор модов. Для итогового теста используйте собранный PBO без `-filePatching`, проверьте клиентский и серверный RPT/script logs, личные метки после рестарта, групповые права, приглашения, закрытие UI, масштабирование и координаты карты. Инструменты в этом репозитории не заменяют эти испытания.
