#!/bin/bash
set -e

ARCH=${1:-x86_64}
BUILD_DIR="build_${ARCH}"

echo "=== Сборка для ${ARCH} ==="

# 1. Установка Conan (если нужно)
if ! command -v conan &> /dev/null; then
    echo "Установка Conan..."
    pip3 install --user conan
    export PATH="$HOME/.local/bin:$PATH"
fi

# 2. Создание профиля Conan
conan profile new default --detect --force 2>/dev/null || true
conan profile update settings.compiler=gcc default
conan profile update settings.build_type=Release default

# 3. Установка зависимостей
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}
conan install .. --build=missing

# 4. CMake конфигурация
cmake .. -DCMAKE_BUILD_TYPE=Release

# 5. Сборка
cmake --build . -j$(nproc)

echo "✓ Готово! Бинарник: ${BUILD_DIR}/ru.tk.FruktAImeter"
