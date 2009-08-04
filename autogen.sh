#!/bin/sh
rm -f config.cache
if [ -d "m4" ]; then
	ACLOCAL_FLAGS="-I m4 $ACLOCAL_FLAGS"
fi

which gnome-autogen.sh || {
        echo "You need to install gnome-common from the GNOME CVS"
        exit 1
}
 
USE_GNOME2_MACROS=1 . gnome-autogen.sh $@
