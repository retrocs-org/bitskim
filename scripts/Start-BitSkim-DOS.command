#!/bin/sh
cd "$(dirname "$0")/.." || exit 1
python3 scripts/run-dos.py "$@"
result=$?
if [ "$result" -ne 0 ]; then
  echo "DOS runtime could not start. See the message above or work/dos-play/runner.log."
  printf 'Press Return to close: '
  read -r reply
fi
exit "$result"
