#!/bin/bash
# usage: shot.sh "<steps>" <name>
# Runs the PIE steps then a HighResShot, waits for the NEW screenshot (the editor writes it 1 to 3 s late; taking the
# newest file instead once mixed up a whole audit) and moves it to build/factory/shots/<name>.png.
source "$(dirname "$0")/common.sh"
BEFORE=$(ls "$SHOTS_DIR" | sort)
bash "$(dirname "$0")/pie_step.sh" "$1; shot" > /dev/null
for i in $(seq 1 60); do
	NEW=$(comm -13 <(echo "$BEFORE") <(ls "$SHOTS_DIR" | sort) | grep HighresScreenshot | head -1)
	if [ -n "$NEW" ]; then
		sleep 0.5
		mv "$SHOTS_DIR/$NEW" "$WORK_DIR/shots/$2.png"
		echo "$WORK_DIR/shots/$2.png"
		exit 0
	fi
	sleep 0.5
done
echo "no screenshot for $2"
exit 1
