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

mf --f "${SPV_ORIGIN}/LICENSE" --t "${SPV_TARGET}/LICENSE"

mf --f "${SPV_ORIGIN}/include" --t "${SPV_TARGET}"

mf --f "${SPV_ORIGIN}/release" --t "${SPV_TARGET}"
mf --f "${SPV_ORIGIN}/debug" --t "${SPV_TARGET}"

# Harfbuzz
mkdir "${HB_TARGET}"

mf --f "${HB_ORIGIN}/LICENSE" --t "${HB_TARGET}/LICENSE"

mf --f "${HB_ORIGIN}/include" --t "${HB_TARGET}"

mf --f "${HB_ORIGIN}/release" --t "${HB_TARGET}"
mf --f "${HB_ORIGIN}/debug" --t "${HB_TARGET}"

# cgltf
mkdir "${CG_TARGET}"

mf --f "${CG_ORIGIN}/LICENSE" --t "${CG_TARGET}/LICENSE"

mf --f "${CG_ORIGIN}/include" --t "${CG_TARGET}"

mf --f "${CG_ORIGIN}/release" --t "${CG_TARGET}"
mf --f "${CG_ORIGIN}/debug" --t "${CG_TARGET}"

# lodepng
mkdir "${LO_TARGET}"

mf --f "${LO_ORIGIN}/LICENSE" --t "${LO_TARGET}/LICENSE"

mf --f "${LO_ORIGIN}/include" --t "${LO_TARGET}"

mf --f "${LO_ORIGIN}/release" --t "${LO_TARGET}"
mf --f "${LO_ORIGIN}/debug" --t "${LO_TARGET}"
