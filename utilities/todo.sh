#!/bin/bash
echo "Kiwi TODO"
echo "========="
echo
find source -type f -print0 | xargs -0r egrep -HIi --color=auto "(TODO|FIXME)"
