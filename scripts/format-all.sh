#!/bin/bash

usage ()
{
    cat << EOF
Usage: format-all.sh [options] [dir]
Format all files in specified directory using clang-format.
Default directory is src.

OPTIONS:
    -e, --exclude dir      exclude specified directory

OTHER OPTIONS:
    -h, --help             display this help
EOF
}


required_args() {
    if [ "$1" -lt $2 ]; then
        usage
        exit 1
    fi
}

POSITIONAL_ARGS=()
EXCLUDE=()

while [[ $# -gt 0 ]]; do
  case $1 in
    -e|--exclude)
      shift
      required_args $# 1
      EXCLUDE+=("$1")
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    -*|--*)
      echo "Unknown option $1"
      exit 1
      ;;
    *)
      POSITIONAL_ARGS+=("$1") # save positional arg
      shift # past argument
      ;;
  esac
done

set -- "${POSITIONAL_ARGS[@]}" # restore positional parameters

dir="${1:-src}"

executor=(xargs -I{} -n1)
if command -v parallel &> /dev/null
then
    executor=(parallel --bar)
fi

function join_by {
  local d=${1-} f=${2-}
  if shift 2; then
    printf %s "$f" "${@/#/$d}"
  fi
}


FIND_EXCLUDE=()
if [ "${#EXCLUDE[@]}" -gt 0 ] ; then
    FIND_EXCLUDE+=(-o '(' -path)
    for value in "${EXCLUDE[@]}"
    do
        FIND_EXCLUDE+=("$value" -prune -false -o -path)
    done

    unset FIND_EXCLUDE[-1]
    unset FIND_EXCLUDE[-1]
    FIND_EXCLUDE+=(')')
fi

find "$dir" -type f\
         \( -name '*.c' \
         -o -name '*.cc' \
         -o -name '*.cpp' \
         -o -name '*.h' \
         -o -name '*.hh' \
         -o -name '*.hpp' \) \
         "${FIND_EXCLUDE[@]}" | "${executor[@]}" "clang-format" -i '{}'
