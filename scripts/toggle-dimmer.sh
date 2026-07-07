#!/bin/bash

if pgrep -x screen-dimmer >/dev/null; then
    pkill -USR1 -x screen-dimmer
else
    ~/.local/bin/screen-dimmer &
fi