#!/usr/bin/env bash

# Set variables. Those may be set in this file or exported in the environment
MY_DEVELOPER_ID=""
MY_APPLE_ID=""
MY_APPLE_PST=""
PATH_TO_BUILD_DIR=$1

if [[ ! -d ${PATH_TO_BUILD_DIR}/translateLocally.app ]]
then
	echo "translateLocally.app not found in the path provided: ${PATH_TO_BUILD_DIR} . Please provide the path to the build directory."
	exit 1
fi

if [[ -z ${MY_DEVELOPER_ID} ]]
then
	MY_DEVELOPER_ID=${APPLE_DEVELOPER_ID}
fi

if [[ -z ${MY_APPLE_ID} ]]
then
	MY_APPLE_ID=${APPLE_ID}
fi

if [[ -z ${MY_APPLE_PST} ]]
then
	MY_APPLE_PST=${APPLE_PST}
fi

# Try to get custom distributed Qt, assuming it's installed in $HOME
MACDEPLOYQT_EXEC=$(find $HOME/Qt -path "*macos/bin/macdeployqt" | sort -r | head -n1)

# If we can't find it, use system default instead
if [[ -z "$MACDEPLOYQT_EXEC" ]]
then
	echo "Could not find custom Qt installation, trying to use macdeployqt from our path"
	MACDEPLOYQT_EXEC=$(which macdeployqt)
else
	echo "Using macdeployqt from Qt Installation at home:"
fi
echo "$MACDEPLOYQT_EXEC"

# First, try to get our .app signed, in case we have defined a certificate
if [[ -z "$MY_DEVELOPER_ID" ]]
then
	echo "Certificate not found, building unsigned dmg"
	$MACDEPLOYQT_EXEC ${PATH_TO_BUILD_DIR}/translateLocally.app -always-overwrite -dmg
else
	echo "Certicate found, building and signing dmg"
	$MACDEPLOYQT_EXEC ${PATH_TO_BUILD_DIR}/translateLocally.app -always-overwrite -sign-for-notarization="$MY_DEVELOPER_ID" -dmg -appstore-compliant
fi

# Then try to notarize the .dmg

if [[ -z "${MY_APPLE_ID}" ]]
then
	echo "Apple ID not defined, skipping notarization."
else
	echo "Notarizing .dmg..."
	response=$(xcrun altool -t osx -f ${PATH_TO_BUILD_DIR}/translateLocally.dmg --primary-bundle-id com.translatelocally.com --notarize-app -u ${MY_APPLE_ID} -p ${MY_APPLE_PST})
	requestUUID=$(echo "${response}" | tr ' ' '\n' | tail -1)

	while true;
	do
	  echo "--> Checking notarization status"

	  statusCheckResponse=$(xcrun altool --notarization-info ${requestUUID} -u ${MY_APPLE_ID} -p ${MY_APPLE_PST})

	  isSuccess=$(echo "${statusCheckResponse}" | grep "success")
	  isFailure=$(echo "${statusCheckResponse}" | grep "invalid")

	  if [[ "${isSuccess}" != "" ]]
	  then
	      echo "Notarization done!"
	      xcrun stapler staple -v ${PATH_TO_BUILD_DIR}/translateLocally.dmg
	      echo "Stapler done!"
	      break
	  fi
	  if [[ "${isFailure}" != "" ]]
	  then
	      echo "Notarization failed"
	      exit 1
	  fi
	  echo "Notarization not finished yet, sleep 2m then check again..."
	  sleep 120
	done
fi
