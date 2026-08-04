# SSH-консоль FreeRTOS

FreeRTOS-сборка `bvstk` может поднимать SSH-сервер на TCP-порту `22`. SSH использует тот же диспетчер команд, что и TCP-консоль на `8888`, поэтому команды `help`, `i2c`, `smi`, `spi`, `mem`, `fs`, `tar`, `ip`, `reboot` и файловые команды доступны через оба интерфейса.

SSH включается только при явном задании `BVSTK_SSH_ENABLE=1`. Обычная сборка FreeRTOS при этом остаётся без зависимости от wolfSSL/wolfSSH и без SSH-сервера.

## Зависимости

Для SSH нужны статические библиотеки wolfSSL и wolfSSH, собранные под ARM/FreeRTOS-платформу проекта. Репозиторий не встраивает эти сторонние исходники в `bvstk`; их можно получить из официальных репозиториев [wolfSSL](https://github.com/wolfSSL/wolfssl) и [wolfSSH](https://github.com/wolfSSL/wolfssh).

Минимальные требования к сборке библиотек:

- wolfSSL: wolfCrypt, SHA-256, RSA и необходимые алгоритмы обмена ключами;
- wolfSSL: `WOLFSSL_LWIP`, без зависимости от файловой системы и TLS;
- wolfSSL: `WC_RNG_SEED_CB`, чтобы приложение могло заменить POSIX seed path (`/dev/urandom` отсутствует в bare-metal FreeRTOS);
- wolfSSH: серверная часть, shell, без client/SFTP/SCP;
- wolfSSH: `WOLFSSH_TERM` и `NO_TERMIOS`, чтобы распознавать SSH `pty-req`, не требуя локального POSIX-терминала.

В переменную `BVSTK_WOLFSSL_ROOT` передаётся prefix с каталогами `include/wolfssl` и `lib/libwolfssl.a`. В `BVSTK_WOLFSSH_ROOT` передаётся корень wolfSSH с `wolfssh/ssh.h` и `src/.libs/libwolfssh.a` либо `lib/libwolfssh.a`.

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

Пароль не записывается в исходники: build-скрипт встраивает только SHA-256 digest в игнорируемый файл `src/ssh/bvstk_ssh_generated.h`. Этот файл и сгенерированный host key не следует добавлять в Git.

## Подключение

После загрузки ELF по JTAG или другим способом:

```sh
ssh -p 22 root@192.168.0.10
```

Для выполнения одной команды без интерактивной сессии:

```sh
ssh -p 22 root@192.168.0.10 'i2c list'
ssh -p 22 root@192.168.0.10 'ip addr show'
```

TCP-консоль на `8888` продолжает работать параллельно. В SSH поддержаны история команд по `Up`/`Down`, перемещение курсора стрелками, `Home`/`End`, переход по словам через `Ctrl+Left`/`Ctrl+Right`, а также `Ctrl+A`/`Ctrl+E`. Для редактирования строки доступны `Backspace`, `Delete`, `Ctrl+W`/`Ctrl+U`/`Ctrl+K` для вырезания и `Ctrl+Y` для вставки последнего вырезанного фрагмента. Автодополнение имён команд вызывается клавишей `Tab`; для `i2c` и `smi` также дополняются имена устройств, адресные селекторы и подкоманды. При нескольких совпадениях выводится список кандидатов. SSH в текущем варианте является инженерной консолью, а не полноценной Unix-оболочкой: SFTP/SCP и аргументы регистров через SSH пока не дополняются. Передача файлов остаётся доступна через существующие файловые и HTTP-интерфейсы.
