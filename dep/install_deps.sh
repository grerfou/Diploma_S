#!/bin/bash
#==============================================================================
#   install_deps.sh — Installation des dépendances sur Linux Mint / Ubuntu
#
#   USAGE:
#       chmod +x install_deps.sh
#       ./install_deps.sh
#
#   Installe :
#       - Raylib 5.0+ (depuis les sources si pas dispo en paquet)
#       - FFmpeg (libavcodec, libavformat, libavutil, libswscale)
#       - OpenGL / Mesa
#       - Outils de compilation (gcc, make)
#==============================================================================

set -e

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  MAPPING APP — Installation des dépendances"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Vérification OS
if [ ! -f /etc/debian_version ]; then
    echo "⚠ Ce script est prévu pour Linux Mint / Ubuntu / Debian"
    echo "  Adapte les commandes pour ton système"
    exit 1
fi

echo "→ Mise à jour des paquets..."
sudo apt update

echo ""
echo "→ Installation des outils de compilation..."
sudo apt install -y \
    gcc \
    make \
    git \
    cmake \
    pkg-config

echo ""
echo "→ Installation des dépendances système..."
sudo apt install -y \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libx11-dev \
    libxrandr-dev \
    libxi-dev \
    libxinerama-dev \
    libxcursor-dev \
    libwayland-dev \
    libxkbcommon-dev

echo ""
echo "→ Installation de FFmpeg..."
sudo apt install -y \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libswresample-dev

echo ""
echo "→ Vérification de Raylib..."

# Tenter d'abord via apt
RAYLIB_VERSION=$(pkg-config --modversion raylib 2>/dev/null || echo "")

if [ -n "$RAYLIB_VERSION" ]; then
    MAJOR=$(echo "$RAYLIB_VERSION" | cut -d. -f1)
    if [ "$MAJOR" -ge 5 ]; then
        echo "✓ Raylib $RAYLIB_VERSION déjà installé"
    else
        echo "⚠ Raylib $RAYLIB_VERSION trop ancien — compilation depuis les sources"
        INSTALL_RAYLIB_FROM_SOURCE=1
    fi
else
    echo "→ Raylib non trouvé — tentative via apt..."
    sudo apt install -y libraylib-dev 2>/dev/null || INSTALL_RAYLIB_FROM_SOURCE=1

    # Re-vérifier après apt
    RAYLIB_VERSION=$(pkg-config --modversion raylib 2>/dev/null || echo "")
    if [ -n "$RAYLIB_VERSION" ]; then
        MAJOR=$(echo "$RAYLIB_VERSION" | cut -d. -f1)
        if [ "$MAJOR" -ge 5 ]; then
            echo "✓ Raylib $RAYLIB_VERSION installé via apt"
            INSTALL_RAYLIB_FROM_SOURCE=0
        else
            echo "⚠ Version trop ancienne — compilation depuis les sources"
            INSTALL_RAYLIB_FROM_SOURCE=1
        fi
    fi
fi

# Compilation Raylib depuis les sources si nécessaire
if [ "${INSTALL_RAYLIB_FROM_SOURCE}" = "1" ]; then
    echo ""
    echo "→ Compilation de Raylib 5.5 depuis les sources..."
    RAYLIB_DIR="/tmp/raylib_build"
    rm -rf "$RAYLIB_DIR"
    git clone --depth 1 --branch 5.5 https://github.com/raysan5/raylib.git "$RAYLIB_DIR"
    cd "$RAYLIB_DIR/src"
    make PLATFORM=PLATFORM_DESKTOP -j$(nproc)
    sudo make install
    sudo ldconfig
    cd -
    rm -rf "$RAYLIB_DIR"
    echo "✓ Raylib 5.5 compilé et installé"
fi

echo ""
echo "→ Vérification finale..."
echo ""

# Vérifications
check() {
    if pkg-config --exists "$1" 2>/dev/null; then
        echo "✓ $1 $(pkg-config --modversion $1)"
    else
        echo "✗ $1 non trouvé"
    fi
}

check raylib
check libavcodec
check libavformat
check libavutil
check libswscale

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "✓ Installation terminée"
echo ""
echo "Lance maintenant :"
echo "  cd mapping_app"
echo "  cd proj1 && make && cd .."
echo "  cd proj2 && make && cd .."
echo "  ./launch.sh"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
