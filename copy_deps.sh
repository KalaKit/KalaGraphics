#!/bin/sh

# Move file for use with mf, read more at https://github.com/greeenlaser/personal-stash/tree/main/mf

set -e

#
# References
#

EXTERNAL_DIR=external

KH_ORIGIN=../kalaheaders
KH_TARGET=${EXTERNAL_DIR}/kalaheaders

VK_ORIGIN=../../_forks/spirv-reflect/_vk
VK_TARGET=${EXTERNAL_DIR}

SPV_ORIGIN=../../_forks/spirv-reflect/_build
SPV_TARGET=${EXTERNAL_DIR}/spirv-reflect

HB_ORIGIN=../../_forks/harfbuzz/_build
HB_TARGET=${EXTERNAL_DIR}/harfbuzz

CG_ORIGIN=../../_forks/cgltf/_build
CG_TARGET=${EXTERNAL_DIR}/cgltf

LO_ORIGIN=../../_forks/lodepng/_build
LO_TARGET=${EXTERNAL_DIR}/lodepng

#
# Copy dependencies
#

# Always a fresh start
rm -rf "${EXTERNAL_DIR}"
mkdir "${EXTERNAL_DIR}"

# KalaHeaders
mkdir "${KH_TARGET}"

mf --f "${KH_ORIGIN}/README.md" --t "${KH_TARGET}/README.md"
mf --f "${KH_ORIGIN}/LICENSE.md" --t "${KH_TARGET}/LICENSE.md"

mf --f "${KH_ORIGIN}/include" --t "${KH_TARGET}"

# Vulkan
mf --f "${VK_ORIGIN}" --t "${VK_TARGET}"
mv "${VK_TARGET}/_vk" "${VK_TARGET}/vulkan"

# Spirv-Reflect
mkdir "${SPV_TARGET}"

if [ -d "${SPV_ORIGIN}/release-windows" ]; then
    mf --f "${SPV_ORIGIN}/release-windows" --t "${SPV_TARGET}"
fi
if [ -d "${SPV_ORIGIN}/release-windows-gnu" ]; then
    mf --f "${SPV_ORIGIN}/release-windows-gnu" --t "${SPV_TARGET}"
fi
if [ -d "${SPV_ORIGIN}/release-linux" ]; then
    mf --f "${SPV_ORIGIN}/release-linux" --t "${SPV_TARGET}"
fi

if [ -d "${SPV_ORIGIN}/debug-windows" ]; then
    mf --f "${SPV_ORIGIN}/debug-windows" --t "${SPV_TARGET}"
fi
if [ -d "${SPV_ORIGIN}/debug-windows-gnu" ]; then
    mf --f "${SPV_ORIGIN}/debug-windows-gnu" --t "${SPV_TARGET}"
fi
if [ -d "${SPV_ORIGIN}/debug-linux" ]; then
    mf --f "${SPV_ORIGIN}/debug-linux" --t "${SPV_TARGET}"
fi

# Harfbuzz
mkdir "${HB_TARGET}"

if [ -d "${HB_ORIGIN}/release-windows" ]; then
    mf --f "${HB_ORIGIN}/release-windows" --t "${HB_TARGET}"
fi
if [ -d "${HB_ORIGIN}/release-windows-gnu" ]; then
    mf --f "${HB_ORIGIN}/release-windows-gnu" --t "${HB_TARGET}"
fi
if [ -d "${HB_ORIGIN}/release-linux" ]; then
    mf --f "${HB_ORIGIN}/release-linux" --t "${HB_TARGET}"
fi

if [ -d "${HB_ORIGIN}/debug-windows" ]; then
    mf --f "${HB_ORIGIN}/debug-windows" --t "${HB_TARGET}"
fi
if [ -d "${HB_ORIGIN}/debug-windows-gnu" ]; then
    mf --f "${HB_ORIGIN}/debug-windows-gnu" --t "${HB_TARGET}"
fi
if [ -d "${HB_ORIGIN}/debug-linux" ]; then
    mf --f "${HB_ORIGIN}/debug-linux" --t "${HB_TARGET}"
fi

# cgltf
mkdir "${CG_TARGET}"

if [ -d "${CG_ORIGIN}/release-windows" ]; then
    mf --f "${CG_ORIGIN}/release-windows" --t "${CG_TARGET}"
fi
if [ -d "${CG_ORIGIN}/release-windows-gnu" ]; then
    mf --f "${CG_ORIGIN}/release-windows-gnu" --t "${CG_TARGET}"
fi
if [ -d "${CG_ORIGIN}/release-linux" ]; then
    mf --f "${CG_ORIGIN}/release-linux" --t "${CG_TARGET}"
fi

if [ -d "${CG_ORIGIN}/debug-windows" ]; then
    mf --f "${CG_ORIGIN}/debug-windows" --t "${CG_TARGET}"
fi
if [ -d "${CG_ORIGIN}/debug-windows-gnu" ]; then
    mf --f "${CG_ORIGIN}/debug-windows-gnu" --t "${CG_TARGET}"
fi
if [ -d "${CG_ORIGIN}/debug-linux" ]; then
    mf --f "${CG_ORIGIN}/debug-linux" --t "${CG_TARGET}"
fi

# lodepng
mkdir "${LO_TARGET}"

if [ -d "${LO_ORIGIN}/release-windows" ]; then
    mf --f "${LO_ORIGIN}/release-windows" --t "${LO_TARGET}"
fi
if [ -d "${LO_ORIGIN}/release-windows-gnu" ]; then
    mf --f "${LO_ORIGIN}/release-windows-gnu" --t "${LO_TARGET}"
fi
if [ -d "${LO_ORIGIN}/release-linux" ]; then
    mf --f "${LO_ORIGIN}/release-linux" --t "${LO_TARGET}"
fi

if [ -d "${LO_ORIGIN}/debug-windows" ]; then
    mf --f "${LO_ORIGIN}/debug-windows" --t "${LO_TARGET}"
fi
if [ -d "${LO_ORIGIN}/debug-windows-gnu" ]; then
    mf --f "${LO_ORIGIN}/debug-windows-gnu" --t "${LO_TARGET}"
fi
if [ -d "${LO_ORIGIN}/debug-linux" ]; then
    mf --f "${LO_ORIGIN}/debug-linux" --t "${LO_TARGET}"
fi
