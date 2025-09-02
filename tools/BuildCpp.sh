#!/usr/bin/env bash
set -e -o pipefail -u

cd "$(dirname "$0")"/..

CPP_COMPILER="${CPP_COMPILER:-g++}"
CPP_TARGET="${CPP_TARGET:-release}"
CPP_MODEL="${CPP_MODEL:-native}"

# capture option variables and ensure arrays exist
cpp_opts_env="${CPP_OPTIONS-}"
c_opts_env="${C_OPTIONS-}"
declare -a CPP_OPTIONS=()
declare -a C_OPTIONS=()
cpp_std=""
c_std=""

# split CPP_OPTIONS into array and separate out -std flag
if [[ -n "$cpp_opts_env" ]]; then
	read -r -a _cpp_opts <<< "$cpp_opts_env" || true
	for opt in "${_cpp_opts[@]}"; do
		if [[ $opt == -std=* ]]; then
			cpp_std=$opt
		else
			CPP_OPTIONS+=("$opt")
		fi
	done
fi

# split C_OPTIONS into array and separate out -std flag
if [[ -n "$c_opts_env" ]]; then
	read -r -a _c_opts <<< "$c_opts_env" || true
	for opt in "${_c_opts[@]}"; do
		if [[ $opt == -std=* ]]; then
			c_std=$opt
		else
			C_OPTIONS+=("$opt")
		fi
	done
fi

# Parsing build target and model
if [[ ${1:-} =~ ^(debug|beta|release)$ ]]; then
	CPP_TARGET="$1"
	shift
fi

if [[ ${1:-} =~ ^(x64|x86|arm64|native|fat)$ ]]; then
	CPP_MODEL="$1"
	shift
fi

# Setting compilation options based on the target
case "$CPP_TARGET" in
	debug)
		C_OPTIONS=(-O0 -DDEBUG -g ${C_OPTIONS+"${C_OPTIONS[@]}"})
		CPP_OPTIONS=(-O0 -DDEBUG -g ${CPP_OPTIONS+"${CPP_OPTIONS[@]}"})
		;;
	beta)
		C_OPTIONS=(-Os -DDEBUG -g ${C_OPTIONS+"${C_OPTIONS[@]}"})
		CPP_OPTIONS=(-Os -DDEBUG -g ${CPP_OPTIONS+"${CPP_OPTIONS[@]}"})
		;;
	release)
		C_OPTIONS=(-Os -DNDEBUG ${C_OPTIONS+"${C_OPTIONS[@]}"})
		CPP_OPTIONS=(-Os -DNDEBUG ${CPP_OPTIONS+"${CPP_OPTIONS[@]}"})
		;;
	*)
		echo "Unrecognized CPP_TARGET: $CPP_TARGET"
		exit 1
		;;
esac

# Setting compilation options based on the model and platform
unameOut="$(uname -s)"
macFlags() {
	case "$1" in
		x64) echo "-m64 -arch x86_64 -target x86_64-apple-macos" ;;
		x86) echo "-m32 -arch i386 -target i386-apple-macos" ;;
		arm64) echo "-m64 -arch arm64 -target arm64-apple-macos" ;;
		fat) echo "-m64 -arch x86_64 -arch arm64" ;;
	esac
}

case "$CPP_MODEL" in
	x64|x86|arm64|fat)
		if [[ "$unameOut" == "Darwin" ]]; then
			read -r -a flags <<< "$(macFlags "$CPP_MODEL")"
		else
			[[ "$CPP_MODEL" == x86 ]] && flags=(-m32) || flags=(-m64)
		fi
		C_OPTIONS=("${flags[@]}" ${C_OPTIONS+"${C_OPTIONS[@]}"})
		CPP_OPTIONS=("${flags[@]}" ${CPP_OPTIONS+"${CPP_OPTIONS[@]}"})
		;;
	native) ;;
	*)
		echo "Unrecognized CPP_MODEL: $CPP_MODEL"
		exit 1
		;;
esac

common_flags=(-fvisibility=hidden -fvisibility-inlines-hidden -Wno-trigraphs -Wreturn-type -Wunused-variable)
C_OPTIONS=("${common_flags[@]}" ${C_OPTIONS+"${C_OPTIONS[@]}"})
CPP_OPTIONS=("${common_flags[@]}" ${CPP_OPTIONS+"${CPP_OPTIONS[@]}"})

if [ $# -lt 2 ]; then
	echo "BuildCpp.sh [debug|beta|release*] [x86|x64|arm64|native*|fat] <output> <source files and other compiler arguments>"
	echo "You can also use the environment variables: CPP_COMPILER, CPP_TARGET, CPP_MODEL, CPP_OPTIONS and C_OPTIONS"
	exit 1
fi

output="$1"
shift

args=()
need_cpp_std=1
for arg in "$@"; do
	if [[ "$arg" == *.c ]]; then
		args+=(-x c ${C_OPTIONS[@]})
		if [[ -n $c_std ]]; then
			if [[ $CPP_COMPILER == *clang++* ]]; then
				args+=(-Xclang "$c_std")
			else
				args+=("$c_std")
			fi
		fi
		args+=("$arg" -x none)
		need_cpp_std=1
	else
		if ((need_cpp_std)) && [[ -n $cpp_std ]]; then
			args+=("$cpp_std")
		fi
		args+=("$arg")
		need_cpp_std=0
	fi
done

len=${#args[@]}
if (( len >= 2 )); then
	last=$((len - 1))
	prev=$((len - 2))
	if [[ ${args[$prev]} == -x && ${args[$last]} == none ]]; then
		unset "args[$last]"
		unset "args[$prev]"
	fi
fi

echo "Compiling $output $CPP_TARGET $CPP_MODEL using $CPP_COMPILER"
echo "${CPP_OPTIONS[*]} -o $output ${args[*]}"

if ! "$CPP_COMPILER" -pipe "${CPP_OPTIONS[@]}" -o "$output" "${args[@]}" 2>&1; then
	echo "Compilation of $output failed"
	exit 1
else
	echo "Compiled $output successfully"
	exit 0
fi
