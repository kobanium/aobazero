#!/bin/sh
find . -type d | xargs chmod 755
find . -type f | xargs chmod 644
find . -name '*.sh' | xargs chmod 744
find . -name '*.py' | xargs chmod 744
