#!/usr/bin/env bash
# After copying recordings off the robot: check every run folder and say which ones are safe to
# delete on the robot. The decision uses only rosbagcheck's exit code; this script deletes nothing.
#
#   examples/after_copy.sh RULES.yaml RUN_FOLDER...
#
# Uses $ROSBAGCHECK if set, else rosbagcheck from PATH.
# Exit code: 0 every run healthy, 1 at least one run must be kept, 2 rosbagcheck could not check.
set -uo pipefail  # no -e: a non-zero exit code from rosbagcheck is an answer, not a crash

if [ $# -lt 2 ]; then
  echo "usage: $0 RULES.yaml RUN_FOLDER..." >&2
  exit 2
fi
rosbagcheck="${ROSBAGCHECK:-rosbagcheck}"
rules="$1"
shift

result=0
for run in "$@"; do
  "$rosbagcheck" --expect "$rules" "$run" > /dev/null
  case $? in
    0) echo "OK    $run  -> safe to delete on the robot" ;;
    1) echo "KEEP  $run  -> do not delete; details: rosbagcheck --expect $rules $run"
       result=1 ;;
    *) echo "ERROR $run  -> could not be checked (see the message above)" >&2
       exit 2 ;;
  esac
done
exit $result
