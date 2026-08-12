# AX7020: Neutrino BSP snapshot

Snapshot содержит board-specific файлы, необходимые для формирования
Neutrino IFS проекта Burevestnik.

## 1. Состав snapshot

| Путь | Содержимое |
|---|---|
| `images/zynq7000-ax7020-ssh.build` | base image description |
| `install/` | startup, драйверы и runtime libraries BSP |
| `README.md` | назначение snapshot и правила сборки |

## 2. Использование

```sh
NEUTRINO_BSP_DIR=third_party/neutrino/bsp/ax7020 \
  ./build.sh neutrino-image
```

Скрипт `scripts/neutrino/build_image.sh` берёт `install/` и base `.build`,
добавляет `/usr/bin/bvstkctl`, `/usr/bin/bvstkd`, SSH-материалы и создаёт IFS.

## 3. SSH-материалы

| Материал | Где создаётся |
|---|---|
| SSH host key | `build/neutrino/ssh/` |
| `authorized_keys` | `build/neutrino/ssh/` |
| root shadow line | `build/neutrino/root.shadow` |

Сгенерированные ключи создаются локально при сборке. Пароль root начинается с
заблокированного состояния; для парольной авторизации используется
`scripts/neutrino/generate_root_shadow.py`.

## 4. Внешние зависимости

На host-машине остаются обязательными Neutrino SDK (`qcc`, `mkifs`) и generic
target files, на которые ссылается BSP image description. Подробный workflow
описан в [`scripts/neutrino/README.md`](../../../../scripts/neutrino/README.md).
