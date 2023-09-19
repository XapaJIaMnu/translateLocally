#!/usr/bin/env bash

# Find out the OS
unameOut="$(uname -s)"
case "${unameOut}" in
	Linux*)     machine=Linux;;
	Darwin*)    machine=Mac;;
	CYGWIN*)    machine=Cygwin;;
	MINGW*)     machine=MinGw;;
	*)          machine="UNKNOWN:${unameOut}"
esac

if [ "$machine" = "Linux" ]; then
	if grep -q avx512 /proc/cpuinfo; then
		echo "avx512"
	elif grep -q avx2 /proc/cpuinfo; then
		echo "avx2"
	elif grep -q avx /proc/cpuinfo; then
		echo "avx"
	elif grep -q ssse3 /proc/cpuinfo; then
		echo "ssse3"
	else
		echo `uname -m`
	fi
elif [ "$machine" = "Mac" ]; then
	if /usr/sbin/sysctl -n machdep.cpu.features machdep.cpu.leaf7_features | grep -q AVX512; then
		echo "avx512"
	elif /usr/sbin/sysctl -n machdep.cpu.features machdep.cpu.leaf7_features | grep -q AVX2; then
		echo "avx2"
	elif /usr/sbin/sysctl -n machdep.cpu.features machdep.cpu.leaf7_features | grep -q AVX; then
		echo "avx"
	elif /usr/sbin/sysctl -n machdep.cpu.features machdep.cpu.leaf7_features | grep -q SSSE3; then
		echo "ssse3"
	else
		echo `uname -m`
	fi
else
	echo "Unsupported platform"
fi
