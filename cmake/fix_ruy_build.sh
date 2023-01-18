#!/usr/bin/env bash
# Turns out that QT's MOC generator with cmake adds extra inlcude path to some files with the RUY header
# this is normally not a problem, but ruy uses a generic time.h header which interferes with the system one...
# hence we do this hacky bit
SRC_PATH=$(realpath $1)
BUILD_DIR=$(realpath $2)

INCLUDE_PATH=I${SRC_PATH}/3rd_party/bergamot-translator/3rd_party/marian-dev/src/3rd_party/ruy/ruy

cd ${BUILD_DIR}

# Remove the extra include path
grep -R ${INCLUDE_PATH}  | cut -d ":" -f1 | xargs sed -i "s\\-${INCLUDE_PATH}\\\\g"
# There's a /profiler in one of them that is not captured, so remove it manually
grep -R " /profiler "  | cut -d ":" -f1 | xargs sed -i "s\\ /profiler \\\\g"
