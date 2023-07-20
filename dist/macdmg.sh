#!/usr/bin/env bash

# Set variables. Those may be set in this file or exported in the environment
MY_DEVELOPER_ID=""
MY_APPLE_ID=""
MY_APPLE_PST=""
MY_APPLE_TEAM_ID=""
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

if [[ -z ${MY_APPLE_TEAM_ID} ]]
then
	MY_APPLE_TEAM_ID=${APPLE_TEAM_ID}
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
	response=$(xcrun notarytool submit ${PATH_TO_BUILD_DIR}/translateLocally.dmg --apple-id ${MY_APPLE_ID} --team-id ${MY_APPLE_TEAM_ID} --password ${MY_APPLE_PST} --wait)
	requestUUID=$(echo "${response}" | grep id | head -n1 | tr ' ' '\n' | tail -n1)
	isAccepted=$(echo "${response}" | grep status | tail -n1 | tr ' ' '\n' | tail -n1)

    echo "Summary of signing..."
	xcrun notarytool info --apple-id ${MY_APPLE_ID} --team-id ${MY_APPLE_TEAM_ID} --password ${MY_APPLE_PST} ${requestUUID}

	echo "Log of the operation..."
	xcrun notarytool log --apple-id ${MY_APPLE_ID} --team-id ${MY_APPLE_TEAM_ID} --password ${MY_APPLE_PST} ${requestUUID}

	if [[ "${isSuccess}" != "Accepted" ]]
	  then
	      echo "Notarization done!"
	      xcrun stapler staple -v ${PATH_TO_BUILD_DIR}/translateLocally.dmg
	      echo "Stapler done!"
	  else
	      echo "Notarization failed"
	      exit 1
	  fi
fi
