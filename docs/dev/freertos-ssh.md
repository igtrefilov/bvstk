# SSH-консоль FreeRTOS

FreeRTOS-сборка `bvstk` может поднимать SSH-сервер на TCP-порту `22`. SSH использует тот же диспетчер команд, что и TCP-консоль на `8888`, поэтому команды `help`, `i2c`, `smi`, `spi`, `mem`, `fs`, `tar`, `ip`, `reboot` и файловые команды доступны через оба интерфейса.

SSH включается только при явном задании `BVSTK_SSH_ENABLE=1`. Обычная сборка FreeRTOS при этом остаётся без зависимости от wolfSSL/wolfSSH и без SSH-сервера.

## Зависимости

Для SSH нужны статические библиотеки wolfSSL и wolfSSH, собранные под ARM/FreeRTOS-платформу проекта. Репозиторий не встраивает эти сторонние исходники в `bvstk`; их можно получить из официальных репозиториев [wolfSSL](https://github.com/wolfSSL/wolfssl) и [wolfSSH](https://github.com/wolfSSL/wolfssh).

Минимальные требования к сборке библиотек:

- wolfSSL: wolfCrypt, SHA-256, RSA и необходимые алгоритмы обмена ключами;
- wolfSSL: `WOLFSSL_LWIP`, без зависимости от файловой системы и TLS;
- wolfSSL: `WC_RNG_SEED_CB`, чтобы приложение могло заменить POSIX seed path (`/dev/urandom` отсутствует в bare-metal FreeRTOS);
- wolfSSH: серверная часть, shell и SCP с пользовательскими callbacks; client/SFTP не нужны;
- wolfSSH: `WOLFSSH_TERM` и `NO_TERMIOS`, чтобы распознавать SSH `pty-req`, не требуя локального POSIX-терминала.

В переменную `BVSTK_WOLFSSL_ROOT` передаётся prefix с каталогами `include/wolfssl` и `lib/libwolfssl.a`. В `BVSTK_WOLFSSH_ROOT` передаётся корень wolfSSH с `wolfssh/ssh.h` и `src/.libs/libwolfssh.a` либо `lib/libwolfssh.a`.

Для SCP wolfSSH нужно конфигурировать с `--enable-scp` и собрать с
`WOLFSSH_SCP_USER_CALLBACKS`. Без этих опций `scripts/vitis/build.tcl`
остановит сборку с понятной ошибкой о недостающих SCP-символах.

В wolfSSH также примените патч
[`scripts/vitis/wolfssh-scp-single-file.patch`](../../scripts/vitis/wolfssh-scp-single-file.patch)
и пересоберите библиотеку. Это compatibility-патч для завершения
однофайловых и рекурсивных SCP-сессий: без него OpenSSH `scp` может получить
ошибку при закрытии последнего файла или каталога.

## Сборка

Пример для уже собранных зависимостей:

```sh
cd /path/to/bvstk
export BVSTK_SSH_ENABLE=1
export BVSTK_WOLFSSL_ROOT=/path/to/wolfssl-install
export BVSTK_WOLFSSH_ROOT=/path/to/wolfssh
export BVSTK_SSH_USER=root
export BVSTK_SSH_PASSWORD='your-password'
export BVSTK_SSH_PORT=22
CLEAN=1 ./build.sh freertos
```

`BVSTK_SSH_USER`, `BVSTK_SSH_PORT` и `BVSTK_SSH_HOST_KEY` являются необязательными. По умолчанию используются пользователь `root` и порт `22`. Если `BVSTK_SSH_HOST_KEY` не задан, build-скрипт генерирует RSA host key на время сборки. Для постоянного fingerprint передайте путь к PEM-ключу через `BVSTK_SSH_HOST_KEY`.

Пароль не записывается в исходники: build-скрипт встраивает только SHA-256 digest в игнорируемый файл `src/apps/freertos/services/ssh/bvstk_ssh_generated.h`. Этот файл и сгенерированный host key не следует добавлять в Git.

## Подключение

После загрузки ELF по JTAG или другим способом:

```sh
ssh root@192.168.0.10
```

Ключ `-tt` не требуется: сервер принимает `pty-req`, поэтому обычная команда `ssh root@<ip>` сразу открывает интерактивную консоль.

Для выполнения одной команды без интерактивной сессии:

```sh
ssh -p 22 root@192.168.0.10 'i2c list'
ssh -p 22 root@192.168.0.10 'ip addr show'
```

TCP-консоль на `8888` продолжает работать параллельно. В SSH поддержаны история команд по `Up`/`Down`, перемещение курсора стрелками, `Home`/`End`, переход по словам через `Ctrl+Left`/`Ctrl+Right`, а также `Ctrl+A`/`Ctrl+E`. Для редактирования строки доступны `Backspace`, `Delete`, `Ctrl+W`/`Ctrl+U`/`Ctrl+K` для вырезания и `Ctrl+Y` для вставки последнего вырезанного фрагмента. Автодополнение имён команд вызывается клавишей `Tab`; для `i2c` и `smi` также дополняются имена устройств, адресные селекторы и подкоманды. При нескольких совпадениях выводится список кандидатов. SSH в текущем варианте является инженерной консолью, а не полноценной Unix-оболочкой. SCP поддерживает передачу отдельных файлов и каталогов между хостом и томами `sd:/`/`flash:/`; SFTP и сохранение POSIX-прав/временных меток пока не поддерживаются.

На системах с OpenSSH, где `scp` по умолчанию использует SFTP, выбирайте классический SCP ключом `-O`:

```sh
scp -O ./hello.bin root@192.168.0.10:/sd:/hello.bin
scp -O root@192.168.0.10:/sd:/hello.bin ./hello-from-board.bin
scp -O -r ./config root@192.168.0.10:/flash:/
scp -O -r root@192.168.0.10:/flash:/config ./config-from-board
```

Если в качестве назначения указать существующий каталог (`/sd:/` или `/flash:/`), файл или каталог будет сохранён в нём под своим исходным именем. Для каталога нужен ключ `-r`. Пути с `..`, неизвестные тома и вложенность более 16 уровней отклоняются.

## Повторная сборка и host key

Если `BVSTK_SSH_HOST_KEY` не задан, при каждой SSH-сборке генерируется новый серверный ключ. Тогда после загрузки нового ELF локальный SSH-клиент справедливо сообщит `REMOTE HOST IDENTIFICATION HAS CHANGED`. Для отладочной платы старую запись можно удалить и подключиться заново:

```sh
ssh-keygen -f "$HOME/.ssh/known_hosts" -R 192.168.0.10
ssh root@192.168.0.10
```

Чтобы fingerprint не менялся между сборками, создайте постоянный PEM-ключ вне Git и передавайте его через `BVSTK_SSH_HOST_KEY`.
