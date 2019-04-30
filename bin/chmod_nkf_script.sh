#!/bin/sh
find . -type d | xargs chmod 755
find . -type f | xargs chmod 644
find . -name '*.sh' | xargs chmod 744
find . -name '*.py' | xargs chmod 744
find . \( -name '*.md' -name '*.[ch]pp' -o -name '*.[ch]' -o -name '*.sh' -o -name '*.py' -o -name '*.txt' -o -name '*.cfg' -o -name '*.def' -o -name 'Makefile*' -o -name '*.bat' -o -name '*.prototxt' \) | xargs nkf --overwrite --oc=UTF-8 -Lu
find learn \(  -name '*.md' -name '*.[ch]pp' -o -name '*.[ch]' -o -name '*.sh' -o -name '*.py' -o -name '*.txt' -o -name '*.cfg' -o -name '*.def' -o -name 'Makefile*' -o -name '*.bat' -o -name '*.prototxt' \) | xargs nkf --overwrite --oc=EUC-JP -Lu
find src/usi_engine \(  -name '*.md' -name '*.[ch]pp' -o -name '*.[ch]' -o -name '*.sh' -o -name '*.py' -o -name '*.txt' -o -name '*.cfg' -o -name '*.def' -o -name 'Makefile*' -o -name '*.bat' -o -name '*.prototxt' \) | xargs nkf --overwrite --oc=UTF-8-BOM -Lw
find src/win \(  -name '*.md' -name '*.[ch]pp' -o -name '*.[ch]' -o -name '*.sh' -o -name '*.py' -o -name '*.txt' -o -name '*.cfg' -o -name '*.def' -o -name 'Makefile*' -o -name '*.bat' -o -name '*.prototxt' \) | xargs nkf --overwrite --oc=UTF-8-BOM -Lw
